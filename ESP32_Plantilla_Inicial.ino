#include <WiFi.h>
#include <PubSubClient.h> // Librería para MQTT
#include <DHTesp.h>      // Librería para el sensor DHT22 (instalar DHT sensor library for ESPx)
#include <Adafruit_NeoPixel.h> // Librería para el anillo NeoPixel
#include <Button2.h> // Librería Button2 optimizada
#include <ArduinoJson.h> // Librería para parsear y crear JSON fácilmente
#include <HTTPClient.h>  // Para hacer la petición pull-on-boot
#include <WiFiClientSecure.h> // Para hacer la petición por la capa HTTPS

// --- MACRO PARA DEPURACIÓN ---
// Muestra nombre de tarea, función y línea en los logs por puerto serie
#define DEBUG_STRING "["+String(pcTaskGetName(NULL))+" - "+String(__FUNCTION__)+"():"+String(__LINE__)+"]   "

// ================================================================================
// CONSTANTES DE CONFIGURACIÓN (¡Modificar usuario y contraseña!)
// ================================================================================

// --- WiFi --- (En Wokwi se suele usar Wokwi-GUEST)
const String ssid = "Wokwi-GUEST";
const String password = "";

// --- MQTT Broker ---
const String mqtt_server = "mqtt.iot-uma.es";
const int mqtt_port      = 1883;
const String mqtt_user   = "micro1";    // USUARIO MQTT ASIGNADO (coincide con el namespace)
const String mqtt_pass   = "iQvXjmy7";  // CONTRASEÑA MQTT ASIGNADA

// --- Identificadores del Gemelo Digital (Ditto) ---
const String NAMESPACE = mqtt_user;
const String THING_NAME = "ESP32-final";

// --- Topics según la configuración de Eclipse Ditto ---
// Telemetría de entrada (Conexión 1: mqtt-micro-in)
const String topic_TELEMETRIA = "iot/devices/" + NAMESPACE + "/" + THING_NAME;

// Recepción de comandos nativos (Conexión 3: mqtt-micro-cmd, downlink)
const String topic_COMANDOS = NAMESPACE + "/" + THING_NAME + "/mensajes";

// Respuestas a los comandos nativos (Conexión 3: mqtt-micro-cmd, uplink)
const String topic_RESPUESTAS = NAMESPACE + "/" + THING_NAME + "/respuestas";

// Recepción de Desired Properties (Conexión 6: mqtt-desired-out)
const String topic_DESIRED = NAMESPACE + "/" + THING_NAME + "/desiredproperties";

// --- Tiempos ---
#define PERIODO_PUBLICACION 30000  // Publicar cada 30 segundos (30000 ms)
//-----------------------------------------------------
// CONFIGURACIÓN DE PINES (ESP32-C3)
//-----------------------------------------------------
#define DHTPIN     1   // Sensor de temperatura/humedad DHT22
#define POTPIN     2   // Potenciómetro simulando sensor de calidad del aire (ADC)
#define BOTONPIN   3   // Botón pulsador interactivo
#define RELAYPIN   4   // Placa de relé
#define LEDPIN     5   // LED estándar
#define NEOPIN     8   // WS2812 NeoPixel Ring (u onboard LED del ESP32-C3)


// Configuración del anillo NeoPixel
#define NUMPIXELS 16   // Cambia este valor al número de LEDs de tu anillo (12, 16, 24...)

//-----------------------------------------------------
// --- Variables Globales ---
//-----------------------------------------------------
WiFiClient wClient;
PubSubClient mqtt_client(wClient);
DHTesp dht;

// Semáforos para coordinar las tareas FreeRTOS
SemaphoreHandle_t semMqttReady; // Semáforo para coordinar el arranque de tareas
SemaphoreHandle_t semPublish; // Semáforo para disparar al publicador

// --- Variables Globales de Sensores y UI (Compartidas entre tareas) ---
Adafruit_NeoPixel strip(NUMPIXELS, NEOPIN, NEO_GRB + NEO_KHZ800);
Button2 boton;

volatile float global_temp = 0.0;
volatile float global_hum = 0.0;
volatile int   global_ppm = 400;

// Variables de Control de Estado y Reglas (Sincronizadas con Ditto)
int publish_delta = 100; // Parametro de salto configurable
int threshold_vent = 1000; // Umbral para arrancar ventilación
int auto_mode = 0; // 0 = Manual, 1 = Automático
int vent_relay = 0; // Estado real de los ventiladores (0 = OFF, 1 = ON)

// Variables internas de memoria
int last_published_ppm = -1000; // Valor absurdo para forzar la 1ª publicación
unsigned long last_publish_time = 0;

// --- BITMAP PARA PUBLICACIÓN SELECTIVA ---
#define PUB_TEMP         0b00000001
#define PUB_HUM          0b00000010
#define PUB_AIR          0b00000100
#define PUB_RELAY        0b00001000
#define PUB_AUTO_MODE    0b00010000
#define PUB_THRESHOLD    0b00100000
#define PUB_PUB_DELTA    0b01000000
#define PUB_ALL          0b01111111 // Fuerza todos los campos a la vez
#define PUB_NONE         0b00000000 // Ningún campo se publica


// Por defecto arrancamos el ESP32 pidiendo lanzar todo para emparejar
volatile uint8_t camposPublicacion = PUB_NONE;

//-----------------------------------------------------
// Utilidad: Muestra información de la prioridad de la tarea actual
//-----------------------------------------------------
inline void info_tarea_actual() { 
  Serial.println(DEBUG_STRING + "Prioridad de tarea " + String(pcTaskGetName(NULL)) + ": " + String(uxTaskPriorityGet(NULL)));
}

//-----------------------------------------------------
// FUNCIÓN: Muestra la calidad del aire en el anillo NeoPixel
//-----------------------------------------------------
void mostrarCalidadAire(int ppm) {
  uint32_t color;
  int ledsActivos = 1;

  int tercio1 = NUMPIXELS / 3;
  int tercio2 = (NUMPIXELS * 2) / 3;

  if (ppm < 1000) {
    color = strip.Color(0, 255, 0); // Verde
    ledsActivos = map(ppm, 400, 999, 1, tercio1);
  } else if (ppm >= 1000 && ppm < 2000) {
    color = strip.Color(255, 128, 0);  // Naranja
    ledsActivos = map(ppm, 1000, 1999, tercio1 + 1, tercio2);
  } else {
    color = strip.Color(255, 0, 0); // Rojo
    ledsActivos = map(ppm, 2000, 5000, tercio2 + 1, NUMPIXELS);
  }

  ledsActivos = constrain(ledsActivos, 1, NUMPIXELS);

  for(int i = 0; i < NUMPIXELS; i++) {
    if (i < ledsActivos) {
      strip.setPixelColor(i, color); 
    } else {
      strip.setPixelColor(i, strip.Color(0, 0, 0)); 
    }
  }
  
  strip.show();
}

//-----------------------------------------------------
// Callback MQTT → Se ejecuta automáticamente al recibir un mensaje suscrito
//-----------------------------------------------------
void procesa_mensaje(char* topic, byte* payload, unsigned int length) { 
  String mensaje = "";
  for(int i = 0; i < length; i++) {
    mensaje += (char)payload[i];
  }
  
  Serial.println(DEBUG_STRING + "======= MENSAJE RECIBIDO =======");
  Serial.println(DEBUG_STRING + "Topic: [" + String(topic) + "]");
  Serial.println(DEBUG_STRING + "Payload: " + mensaje);

}

//-----------------------------------------------------
// Conexión con WiFi
//-----------------------------------------------------
void conecta_wifi() {
  Serial.println(DEBUG_STRING + "Conectando a " + ssid);
 
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());

  while (WiFi.status() != WL_CONNECTED) {
    vTaskDelay(pdMS_TO_TICKS(500));
    Serial.print(".");
  }
  Serial.println();
  Serial.println(DEBUG_STRING + "WiFi conectado. IP: " + WiFi.localIP().toString());
}

//-----------------------------------------------------
// Conexión con MQTT
//-----------------------------------------------------
void conecta_mqtt() {
  // Bucle hasta que logremos conectar
  while (!mqtt_client.connected()) {
    Serial.println(DEBUG_STRING + "Intentando conexión MQTT...");
    
    // El Client ID debe ser único, la MAC no es unica en Wokwi, usamos el namespace y el thing name
    String clientId = "MQTT-" + NAMESPACE + ":" + THING_NAME;
    
    if (mqtt_client.connect(clientId.c_str(), mqtt_user.c_str(), mqtt_pass.c_str(),topic_TELEMETRIA.c_str(),1,true,"{\"online\":0}")) {
      Serial.println(DEBUG_STRING + "Conectado al broker: " + mqtt_server);
      
      // Nos suscribimos al topic para recibir los mensajes/órdenes de Ditto
      mqtt_client.subscribe(topic_COMANDOS.c_str());
      Serial.println(DEBUG_STRING + "Suscrito a: " + topic_COMANDOS);
      
      // Nos suscribimos al topic para monitorizar los cambios a desiredProperties
      mqtt_client.subscribe(topic_DESIRED.c_str());
      Serial.println(DEBUG_STRING + "Suscrito a: " + topic_DESIRED);
      // Publicamos en el topic iot/devices/<namespace>/<name>
      mqtt_client.publish(topic_TELEMETRIA.c_str(), "{\"online\":1}", true);
    } else {
      Serial.print(DEBUG_STRING + "Error MQTT state: ");
      Serial.print(mqtt_client.state());
      Serial.println(". Reintentando en 5 segundos...");
      vTaskDelay(pdMS_TO_TICKS(5000));
    }
  }
}
//-----------------------------------------------------
// Pull on Boot: Recupera el estado actual del Gemelo Digital
//-----------------------------------------------------
void pull_on_boot() {
  Serial.println(DEBUG_STRING + "Iniciando Pull-on-Boot (Descargando configuración del gemelo)...");
}

//================================================================================
//   TAREAS FreeRTOS
//================================================================================

//-----------------------------------------------------
// TAREA 1: Mantener conexión WiFi/MQTT y atender mensajes entrantes
//-----------------------------------------------------
void taskMQTTService(void *pvParameters) {
  info_tarea_actual();
  
  // 1. Iniciar WiFi
  conecta_wifi();

  // 2. Configurar cliente MQTT
  mqtt_client.setServer(mqtt_server.c_str(), mqtt_port);
  mqtt_client.setBufferSize(1024); // Ampliado para soportar JSON pesados del Ditto Protocol
  mqtt_client.setCallback(procesa_mensaje);

  // 3. Conectar al Broker MQTT
  conecta_mqtt();

  // 4. Sincronización Pull-on-Boot (Descarga el estado del Gemelo y corrige discrepancias)
  pull_on_boot();

  Serial.println(DEBUG_STRING + "Conexiones establecidas. Abriendo semáforo para publicar...");

  // Señalizar a la tarea de sensores que ya puede empezar a trabajar
  xSemaphoreGive(semMqttReady);

  // Bucle infinito de la tarea MQTT
  while(true) {
    if (!mqtt_client.connected()) {
      conecta_mqtt(); // Reconectar si se cae
    }
    mqtt_client.loop(); // Atiende llamadas y suscripciones (Triggera el callback si hay mensajes)
    
    // Pequeño delay para ceder tiempo de CPU al sistema
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

//-----------------------------------------------------
// TAREA 2: Leer Sensores y Evaluar Reglas (Bucle cada 2s)
//-----------------------------------------------------
void taskReader(void *pvParameters) {
  info_tarea_actual();
  
  // Inicializamos hardware físico
  dht.setup(DHTPIN, DHTesp::DHT22);
  strip.begin();
  strip.setBrightness(100);
  strip.show();
  
  Serial.println(DEBUG_STRING + "Tarea sensora esperando a que MQTT esté listo...");
  // Esperar a tener red (tomamos el semáforo)
  xSemaphoreTake(semMqttReady, portMAX_DELAY);
  
  Serial.println(DEBUG_STRING + "Red lista. Iniciando escaneo de sensores cada 2 segundos...");
  
  while(true) {
    // 1. Leer el entorno (Potenciómetro y DHT22)
    int rawADC = analogRead(POTPIN);
    int current_ppm = map(rawADC, 0, 4095, 400, 5000);
    TempAndHumidity newValues = dht.getTempAndHumidity();
    // 2. Actualizar variables globales (estado actual del Gemelo Físico)
    global_ppm = current_ppm;
    mostrarCalidadAire(global_ppm);
    
    if (dht.getStatus() == 0) {
      global_temp = newValues.temperature;
      global_hum = newValues.humidity;
    }
    
    // --- Mostrar por consola las mediciones ---
    Serial.println(DEBUG_STRING + "--- LECTURA DE SENSORES ---");
    Serial.println(DEBUG_STRING+"Temp: "+String(global_temp)+"ºC | Hum: "+String(global_hum)+" %% | CO2: "+String(global_ppm)+" ppm\n");
    
    // A dormir 2 segundos cediendo la CPU
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

//================================================================================
//   SETUP Y LOOP PRINCIPAL
//================================================================================
void setup() {
  Serial.begin(115200);
  Serial.println();
  
  // Pines de actuadores físicos
  pinMode(LEDPIN, OUTPUT);
  pinMode(RELAYPIN, OUTPUT);
  digitalWrite(LEDPIN, LOW);
  digitalWrite(RELAYPIN, LOW);
  
  // Creamos los semáforos binarios
  semMqttReady = xSemaphoreCreateBinary();
  semPublish = xSemaphoreCreateBinary();
  
  info_tarea_actual();

  // 1. Tarea de Red (Prioridad Alta: 3)
  xTaskCreate(taskMQTTService, "MQTT_Srv", 8192, NULL, 3, NULL);

  // 2. Tarea de Sensores (Prioridad Media: 2) Despierta cada 2s a leer y evaluar
  xTaskCreate(taskReader, "Reader", 4096, NULL, 2, NULL);

  Serial.println(DEBUG_STRING + "Setup (core) terminado, el sistema queda en manos de FreeRTOS.");
  
  // Eliminamos la tarea loop() ya que en FreeRTOS no necesitamos este bucle
  vTaskDelete(NULL);
}

void loop() {
  // El loop de Arduino queda inutilizado y liberado gracias a las tareas FreeRTOS
}
