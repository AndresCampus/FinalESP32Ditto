#include <WiFi.h>
#include <PubSubClient.h> 
#include <DHTesp.h>      
#include <Adafruit_NeoPixel.h> 
#include <Button2.h> 
#include <ArduinoJson.h> 
#include <HTTPClient.h>  
#include <WiFiClientSecure.h> 

#define DEBUG_STRING "["+String(pcTaskGetName(NULL))+" - "+String(__FUNCTION__)+"():"+String(__LINE__)+"]   "

// ================================================================================
// CONSTANTES DE CONFIGURACIÓN
// ================================================================================
const String ssid = "Wokwi-GUEST";
const String password = "";
const String mqtt_server = "mqtt.iot-uma.es";
const int mqtt_port      = 1883;
const String mqtt_user   = "micro1";    
const String mqtt_pass   = "iQvXjmy7";  

const String NAMESPACE = mqtt_user;
const String THING_NAME = "ESP32-final";

const String topic_TELEMETRIA = "iot/devices/" + NAMESPACE + "/" + THING_NAME;
const String topic_COMANDOS = NAMESPACE + "/" + THING_NAME + "/mensajes";
const String topic_RESPUESTAS = NAMESPACE + "/" + THING_NAME + "/respuestas";
const String topic_DESIRED = NAMESPACE + "/" + THING_NAME + "/desiredproperties";

#define PERIODO_PUBLICACION 30000  

#define DHTPIN     1   
#define POTPIN     2   
#define BOTONPIN   3   
#define RELAYPIN   4   
#define LEDPIN     5   
#define NEOPIN     8   
#define NUMPIXELS 16   

WiFiClient wClient;
PubSubClient mqtt_client(wClient);
DHTesp dht;
SemaphoreHandle_t semMqttReady; 
SemaphoreHandle_t semPublish; 
Adafruit_NeoPixel strip(NUMPIXELS, NEOPIN, NEO_GRB + NEO_KHZ800);
Button2 boton;

volatile float global_temp = 0.0;
volatile float global_hum = 0.0;
volatile int   global_ppm = 400;

int publish_delta = 100; 
int threshold_vent = 1000; 
int auto_mode = 0; 
int vent_relay = 0; 

int last_published_ppm = -1000; 
unsigned long last_publish_time = 0;

#define PUB_TEMP         0b00000001
#define PUB_HUM          0b00000010
#define PUB_AIR          0b00000100
#define PUB_RELAY        0b00001000
#define PUB_AUTO_MODE    0b00010000
#define PUB_THRESHOLD    0b00100000
#define PUB_PUB_DELTA    0b01000000
#define PUB_ALL          0b01111111 
#define PUB_NONE         0b00000000 

volatile uint8_t camposPublicacion = PUB_NONE;

inline void info_tarea_actual() { 
  Serial.println(DEBUG_STRING + "Prioridad de tarea " + String(pcTaskGetName(NULL)) + ": " + String(uxTaskPriorityGet(NULL)));
}

void mostrarCalidadAire(int ppm) {
  uint32_t color;
  int ledsActivos = 1;
  int tercio1 = NUMPIXELS / 3;
  int tercio2 = (NUMPIXELS * 2) / 3;

  if (ppm < 1000) {
    color = strip.Color(0, 255, 0); 
    ledsActivos = map(ppm, 400, 999, 1, tercio1);
  } else if (ppm >= 1000 && ppm < 2000) {
    color = strip.Color(255, 128, 0);  
    ledsActivos = map(ppm, 1000, 1999, tercio1 + 1, tercio2);
  } else {
    color = strip.Color(255, 0, 0); 
    ledsActivos = map(ppm, 2000, 5000, tercio2 + 1, NUMPIXELS);
  }

  ledsActivos = constrain(ledsActivos, 1, NUMPIXELS);
  for(int i = 0; i < NUMPIXELS; i++) {
    if (i < ledsActivos) strip.setPixelColor(i, color); 
    else strip.setPixelColor(i, strip.Color(0, 0, 0)); 
  }
  strip.show();
}

void procesa_mensaje(char* topic, byte* payload, unsigned int length) { 
  String mensaje = "";
  for(int i = 0; i < length; i++) mensaje += (char)payload[i];
  Serial.println(DEBUG_STRING + "======= MENSAJE RECIBIDO =======");
  Serial.println(DEBUG_STRING + "Topic: [" + String(topic) + "]");
  Serial.println(DEBUG_STRING + "Payload: " + mensaje);
}

void conecta_wifi() {
  Serial.println(DEBUG_STRING + "Conectando a " + ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  while (WiFi.status() != WL_CONNECTED) {
    vTaskDelay(pdMS_TO_TICKS(500));
    Serial.print(".");
  }
  Serial.println("\n" + DEBUG_STRING + "WiFi conectado. IP: " + WiFi.localIP().toString());
}

void conecta_mqtt() {
  while (!mqtt_client.connected()) {
    Serial.println(DEBUG_STRING + "Intentando conexión MQTT...");
    String clientId = "MQTT-" + NAMESPACE + ":" + THING_NAME;
    if (mqtt_client.connect(clientId.c_str(), mqtt_user.c_str(), mqtt_pass.c_str(),topic_TELEMETRIA.c_str(),1,true,"{\"online\":0}")) {
      Serial.println(DEBUG_STRING + "Conectado al broker: " + mqtt_server);
      mqtt_client.subscribe(topic_COMANDOS.c_str());
      mqtt_client.subscribe(topic_DESIRED.c_str());
      mqtt_client.publish(topic_TELEMETRIA.c_str(), "{\"online\":1}", true);
    } else {
      Serial.print(DEBUG_STRING + "Error MQTT state: ");
      Serial.println(mqtt_client.state());
      vTaskDelay(pdMS_TO_TICKS(5000));
    }
  }
}

void pull_on_boot() {
  Serial.println(DEBUG_STRING + "Iniciando Pull-on-Boot...");
}

void taskMQTTService(void *pvParameters) {
  info_tarea_actual();
  conecta_wifi();
  mqtt_client.setServer(mqtt_server.c_str(), mqtt_port);
  mqtt_client.setBufferSize(1024); 
  mqtt_client.setCallback(procesa_mensaje);
  conecta_mqtt();
  pull_on_boot();
  xSemaphoreGive(semMqttReady);

  while(true) {
    if (!mqtt_client.connected()) conecta_mqtt();
    mqtt_client.loop(); 
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
    Serial.println(DEBUG_STRING+"Temp: "+String(global_temp)+"ºC | Hum: "+String(global_hum)+" % | CO2: "+String(global_ppm)+" ppm\n");

    // 3. Evaluar el Motor de Reglas temporales de publicación
    bool time_elapsed = (millis() - last_publish_time >= PERIODO_PUBLICACION);
    
    // Si han pasado 30 segundos, enviamos todo el paquete ambiental
    if (time_elapsed) {
      camposPublicacion |= (PUB_TEMP | PUB_HUM | PUB_AIR); // Activamos los flags a '1'
      last_publish_time = millis(); // Reseteamos el reloj
      
      Serial.println(DEBUG_STRING + "Periodo (30s) cumplido. Despertando a Publicador...");
      xSemaphoreGive(semPublish); // Pasamos el Semáforo a Tarea 3
    }
    
    // A dormir 2 segundos cediendo la CPU de vuelta al OS
    vTaskDelay(pdMS_TO_TICKS(2000));
    } // <--- Fin del while(true) de taskReader
}

//-----------------------------------------------------
// TAREA 3: Publicador MQTT (Event-Driven)
//-----------------------------------------------------
void taskPublisher(void *pvParameters) {
  info_tarea_actual();
  
  while(true) {
    // Esta tarea duerme infinitamente (0% CPU) hasta que alguien lance xSemaphoreGive(semPublish)
    xSemaphoreTake(semPublish, portMAX_DELAY);
    
    // Copiamos la máscara de bits y la purgamos para la próxima vez
    uint8_t currentBits = camposPublicacion;
    camposPublicacion = 0; 
    
    StaticJsonDocument<256> telemetria;
    
    // Empaqueta SOLO lo necesario usando operaciones binarias AND a nivel de bit
    if (currentBits & PUB_TEMP)      telemetria["temperature"]    = global_temp;
    if (currentBits & PUB_HUM)       telemetria["humidity"]       = global_hum;
    if (currentBits & PUB_AIR)       telemetria["air_quality"]    = global_ppm;
    if (currentBits & PUB_RELAY)     telemetria["vent_relay"]     = vent_relay;
    if (currentBits & PUB_AUTO_MODE) telemetria["auto_mode"]      = auto_mode;
    if (currentBits & PUB_THRESHOLD) telemetria["threshold_vent"] = threshold_vent;
    if (currentBits & PUB_PUB_DELTA) telemetria["publish_delta"]  = publish_delta;
    
    // Cancelamos un eventual envío vacío
    if (telemetria.size() == 0) continue;
    
    String jsonStr;
    serializeJson(telemetria, jsonStr);

    Serial.println(DEBUG_STRING + ">>> [MQTT UPLINK] : " + jsonStr);
    mqtt_client.publish(topic_TELEMETRIA.c_str(), jsonStr.c_str());
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

   // 3. Tarea de Publicación (Prioridad Baja: 1) Guiada por eventos
   xTaskCreate(taskPublisher, "Publisher", 4096, NULL, 1, NULL);

  Serial.println(DEBUG_STRING + "Setup terminado.");
  vTaskDelete(NULL);
}

void loop() {}
