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

int publish_delta_DP = 100; 
int threshold_vent_DP = 1000; 
int auto_mode_DP = 0; 
int vent_relay_DP = 0; 

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

//-----------------------------------------------------
// Callback MQTT → Se ejecuta automáticamente al recibir un mensaje suscrito
//-----------------------------------------------------
void callback(char* topic, byte* payload, unsigned int length) {
  String mensaje = "";
  for(int i = 0; i < length; i++) {
    mensaje += (char)payload[i];
  }
  
  Serial.println(DEBUG_STRING + "======= MENSAJE RECIBIDO =======");
  Serial.println(DEBUG_STRING + "Payload: " + mensaje);

  // --- GESTIÓN DE PROPIEDADES DESEADAS (DESIRED) ---
  if (String(topic) == topic_DESIRED) {
    StaticJsonDocument<512> doc;
    deserializeJson(doc, mensaje);
    bool hayCambios = false;

    // ¿El usuario ha cambiado el modo (AUTO/MANUAL)?
    if (doc.containsKey("auto_mode_DP")) {
      auto_mode_DP = doc["auto_mode_DP"]["value"].as<int>();
      camposPublicacion |= PUB_AUTO_MODE;
      hayCambios = true;
    }
    
    // ¿El usuario ha cambiado el umbral de ventilación (ppm)?
    if (doc.containsKey("threshold_vent_DP")) {
      threshold_vent_DP = doc["threshold_vent_DP"]["value"].as<int>();
      camposPublicacion |= PUB_THRESHOLD;
      hayCambios = true;
    }
    if (doc.containsKey("publish_delta_DP")) {
      publish_delta_DP = doc["publish_delta_DP"]["value"].as<int>();
      camposPublicacion |= PUB_PUB_DELTA;
      hayCambios = true;
    }

    // Si estamos en MODO MANUAL, permitimos encender/apagar el relé desde la nube
    if (doc.containsKey("vent_relay_DP")) {
      int nuevo_vr = doc["vent_relay_DP"]["value"].as<int>();
      if (auto_mode_DP == 0) { 
        vent_relay_DP = nuevo_vr;
        digitalWrite(RELAYPIN, vent_relay_DP ? HIGH : LOW);
        digitalWrite(LEDPIN, vent_relay_DP ? HIGH : LOW); // LED indicador físico
        camposPublicacion |= PUB_RELAY;
        hayCambios = true;
      }
    }

    // Si hubo cambios, avisamos al publicador para que informe a Ditto del nuevo estado real
    if (hayCambios) {
      xSemaphoreGive(semPublish);
    }
  }
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
  mqtt_client.setCallback(callback);
  conecta_mqtt();
  pull_on_boot();
  xSemaphoreGive(semMqttReady);

  while(true) {
    if (!mqtt_client.connected()) conecta_mqtt();
    mqtt_client.loop(); 
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void taskReader(void *pvParameters) {
  info_tarea_actual();
  dht.setup(DHTPIN, DHTesp::DHT22);
  strip.begin();
  strip.setBrightness(100);
  strip.show();
  
  xSemaphoreTake(semMqttReady, portMAX_DELAY);
  
  while(true) {
    int rawADC = analogRead(POTPIN);
    int current_ppm = map(rawADC, 0, 4095, 400, 5000);
    TempAndHumidity newValues = dht.getTempAndHumidity();
    global_ppm = current_ppm;
    mostrarCalidadAire(global_ppm);
    
    if (dht.getStatus() == 0) {
      global_temp = newValues.temperature;
      global_hum = newValues.humidity;
    }
    
    Serial.println(DEBUG_STRING + "--- LECTURA DE SENSORES ---");
    Serial.println(DEBUG_STRING+"Temp: "+String(global_temp)+"ºC | Hum: "+String(global_hum)+" % | CO2: "+String(global_ppm)+" ppm\n");

    bool time_elapsed = (millis() - last_publish_time >= PERIODO_PUBLICACION);
    if (time_elapsed) {
      camposPublicacion |= (PUB_TEMP | PUB_HUM | PUB_AIR); 
      last_publish_time = millis(); 
      Serial.println(DEBUG_STRING + "Periodo (30s) cumplido. Despertando a Publicador...");
      xSemaphoreGive(semPublish); 
    }
    
    vTaskDelay(pdMS_TO_TICKS(2000));
    } // <--- Fin del while(true) de taskReader
}

void taskPublisher(void *pvParameters) {
  info_tarea_actual();
  while(true) {
    xSemaphoreTake(semPublish, portMAX_DELAY);
    uint8_t currentBits = camposPublicacion;
    camposPublicacion = 0; 
    StaticJsonDocument<256> telemetria;
    if (currentBits & PUB_TEMP)      telemetria["temperature"]    = global_temp;
    if (currentBits & PUB_HUM)       telemetria["humidity"]       = global_hum;
    if (currentBits & PUB_AIR)       telemetria["air_quality"]    = global_ppm;
    if (currentBits & PUB_RELAY)     telemetria["vent_relay_DP"]     = vent_relay_DP;
    if (currentBits & PUB_AUTO_MODE) telemetria["auto_mode_DP"]      = auto_mode_DP;
    if (currentBits & PUB_THRESHOLD) telemetria["threshold_vent_DP"] = threshold_vent_DP;
    if (currentBits & PUB_PUB_DELTA) telemetria["publish_delta_DP"]  = publish_delta_DP;
    if (telemetria.size() == 0) continue;
    String jsonStr;
    serializeJson(telemetria, jsonStr);
    Serial.println(DEBUG_STRING + ">>> [MQTT UPLINK] : " + jsonStr);
    mqtt_client.publish(topic_TELEMETRIA.c_str(), jsonStr.c_str());
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LEDPIN, OUTPUT);
  pinMode(RELAYPIN, OUTPUT);
  digitalWrite(LEDPIN, LOW);
  digitalWrite(RELAYPIN, LOW);
  semMqttReady = xSemaphoreCreateBinary();
  semPublish = xSemaphoreCreateBinary();
  
  xTaskCreate(taskMQTTService, "MQTT_Srv", 8192, NULL, 3, NULL);
  xTaskCreate(taskReader, "Reader", 4096, NULL, 2, NULL);
  xTaskCreate(taskPublisher, "Publisher", 4096, NULL, 1, NULL);
  vTaskDelete(NULL);
}

void loop() {}
