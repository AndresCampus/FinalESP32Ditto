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

void callback(char* topic, byte* payload, unsigned int length) {
  String mensaje = "";
  for(int i = 0; i < length; i++) mensaje += (char)payload[i];
  
  Serial.println(DEBUG_STRING + "======= MENSAJE RECIBIDO =======");
  Serial.println(DEBUG_STRING + "Payload: " + mensaje);

  // --- 1. GESTIÓN DE COMANDOS DIRECTOS (RPC / MESSAGES) ---
  if(String(topic) == topic_COMANDOS) {
    StaticJsonDocument<512> doc;
    deserializeJson(doc, mensaje);
    String path = doc["path"].as<String>();
    
    if (path.endsWith("/refresh")) {
      Serial.println(DEBUG_STRING + "¡Orden REFRESH! Forzando telemetría total...");
      camposPublicacion = PUB_ALL; 
      xSemaphoreGive(semPublish);  

      // RESPUESTA DE CONFIRMACIÓN (Ditto Protocol Uplink)
      StaticJsonDocument<512> resp;
      resp["topic"] = doc["topic"]; 
      resp["path"] = doc["path"];   
      resp["status"] = 200;         
      
      JsonObject headers = resp.createNestedObject("headers");
      headers["correlation-id"] = doc["headers"]["correlation-id"];
      headers["content-type"] = "application/json";
      
      JsonObject value = resp.createNestedObject("value");
      value["status"] = 200;
      value["message"] = "Telemetría forzada con éxito.";
      
      String respuestaStr;
      serializeJson(resp, respuestaStr);
      mqtt_client.publish(topic_RESPUESTAS.c_str(), respuestaStr.c_str());
    }
  }
  
  // --- 2. GESTIÓN DE PROPIEDADES DESEADAS (DESIRED) ---
  else if (String(topic) == topic_DESIRED) {
    StaticJsonDocument<512> doc;
    deserializeJson(doc, mensaje);
    bool hayCambios = false;

    if (doc.containsKey("auto_mode")) {
      auto_mode = doc["auto_mode"]["value"].as<int>();
      camposPublicacion |= PUB_AUTO_MODE;
      hayCambios = true;
    }
    if (doc.containsKey("threshold_vent")) {
      threshold_vent = doc["threshold_vent"]["value"].as<int>();
      camposPublicacion |= PUB_THRESHOLD;
      hayCambios = true;
    }
    if (doc.containsKey("publish_delta")) {
      publish_delta = doc["publish_delta"]["value"].as<int>();
      camposPublicacion |= PUB_PUB_DELTA;
      hayCambios = true;
    }
    if (doc.containsKey("vent_relay")) {
      int nuevo_vr = doc["vent_relay"]["value"].as<int>();
      if (auto_mode == 0) { 
        vent_relay = nuevo_vr;
        digitalWrite(RELAYPIN, vent_relay ? HIGH : LOW);
        digitalWrite(LEDPIN, vent_relay ? HIGH : LOW);
        camposPublicacion |= PUB_RELAY;
        hayCambios = true;
      }
    }
    if (hayCambios) xSemaphoreGive(semPublish);
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
  Serial.println(DEBUG_STRING + "Iniciando Pull-on-Boot (Descargando configuración del gemelo)...");
  
  WiFiClientSecure client;
  client.setInsecure(); // No validar el certificado para agilizar
  
  HTTPClient http;
  // Descargamos las features enteras del Thing
  String url = "https://ditto.iot-uma.es/api/2/things/" + NAMESPACE + ":" + THING_NAME + "/features";
  
  http.begin(client, url);
  http.setAuthorization(mqtt_user.c_str(), mqtt_pass.c_str());
  
  int httpCode = http.GET();
  
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    
    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error) {
      bool hayCambios = false;
      Serial.println(DEBUG_STRING + "¡Features descargadas con éxito! Chequeando parámetros locales...");
      
      // Pequeña lambda para simplificar la extracción y priorizar 'desired' frente a 'reported'
      auto extraeValor = [&](const char* feature, int currentVal) -> int {
        if (doc[feature]["desiredProperties"].containsKey("value")) {
          return doc[feature]["desiredProperties"]["value"].as<int>();
        } else if (doc[feature]["properties"].containsKey("value")) {
          return doc[feature]["properties"]["value"].as<int>();
        }
        return currentVal;
      };

      // 1. Extracción pura del JSON de Ditto
      int pull_auto_mode = extraeValor("auto_mode", auto_mode);
      int pull_vent_relay = extraeValor("vent_relay", vent_relay);
      int pull_threshold = extraeValor("threshold_vent", threshold_vent);
      int pull_delta = extraeValor("publish_delta", publish_delta);

      // 2. Transfusión segura de estados a RAM Global
      if (pull_auto_mode != auto_mode) {
        auto_mode = pull_auto_mode;
        hayCambios = true;
        camposPublicacion |= PUB_AUTO_MODE; // Activamos bandera del modo automático
      }
      
      if (pull_threshold != threshold_vent) {
        threshold_vent = pull_threshold;
        hayCambios = true;
        camposPublicacion |= PUB_THRESHOLD; // Activamos bandera del umbral
      }
      
      if (pull_delta != publish_delta) {
        publish_delta = pull_delta;
        hayCambios = true;
        camposPublicacion |= PUB_PUB_DELTA; // Activamos bandera del delta
      }

      // En el arranque, ¿obedecemos el relé del cloud? Solo si estamos en manual.
      // (Si caemos en auto, ya lo arreglará el sensor en el primer ciclo del taskReader)
      if (pull_vent_relay != vent_relay && auto_mode == 0) {
        vent_relay = pull_vent_relay;
        // Asignamos el pin físico del arranque
        digitalWrite(RELAYPIN, vent_relay ? HIGH : LOW);
        digitalWrite(LEDPIN, vent_relay ? HIGH : LOW);
        hayCambios = true;
        camposPublicacion |= PUB_RELAY; // Activamos bandera del relé
      }
      
      Serial.println(DEBUG_STRING + "Sincronización completada con éxito.");

      // 3. Forzar a TaskPublisher a subir el nuevo testamento (Ack al Servidor)
      if (hayCambios) {
        Serial.println(DEBUG_STRING + "Discrepancias corregidas. Forzando uplink para nivelar telemetría...");
        xSemaphoreGive(semPublish);
      }
      
    } else {
      Serial.println(DEBUG_STRING + "Error desencriptando el JSON del Pull-On-Boot: " + String(error.c_str()));
    }
  } else {
    Serial.println(DEBUG_STRING + "Fallo o timeout en HTTP GET durante Pull-on-boot (" + String(httpCode) + ")");
  }
  
  http.end();
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
    if (dht.getStatus() == 0) {
      global_temp = newValues.temperature;
      global_hum = newValues.humidity;
    }
    mostrarCalidadAire(current_ppm);
    bool state_changed = false;
    if (auto_mode == 1) {
      int deseado = (current_ppm >= threshold_vent) ? 1 : 0;
      if (deseado != vent_relay) {
        vent_relay = deseado;
        digitalWrite(RELAYPIN, vent_relay ? HIGH : LOW);
        digitalWrite(LEDPIN, vent_relay ? HIGH : LOW);
        state_changed = true; 
      }
    }
    bool time_elapsed = (millis() - last_publish_time >= PERIODO_PUBLICACION);
    bool delta_exceeded = (abs(current_ppm - last_published_ppm) >= publish_delta);
    if (time_elapsed || delta_exceeded || state_changed) {
      if (time_elapsed) camposPublicacion |= (PUB_TEMP | PUB_HUM | PUB_AIR); 
      if (delta_exceeded) camposPublicacion |= PUB_AIR; 
      if (state_changed) camposPublicacion |= PUB_RELAY; 
      last_publish_time = millis();
      last_published_ppm = current_ppm;
      xSemaphoreGive(semPublish);
    }
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
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
    if (currentBits & PUB_RELAY)     telemetria["vent_relay"]     = vent_relay;
    if (currentBits & PUB_AUTO_MODE) telemetria["auto_mode"]      = auto_mode;
    if (currentBits & PUB_THRESHOLD) telemetria["threshold_vent"] = threshold_vent;
    if (currentBits & PUB_PUB_DELTA) telemetria["publish_delta"]  = publish_delta;
    if (telemetria.size() == 0) continue;
    String jsonStr;
    serializeJson(telemetria, jsonStr);
    mqtt_client.publish(topic_TELEMETRIA.c_str(), jsonStr.c_str());
  }
}

void clickCorto(Button2& btn) {
  if (auto_mode == 0) {
    vent_relay = (vent_relay == 1) ? 0 : 1; 
    digitalWrite(RELAYPIN, vent_relay ? HIGH : LOW);
    digitalWrite(LEDPIN, vent_relay ? HIGH : LOW);
    camposPublicacion |= PUB_RELAY;
    xSemaphoreGive(semPublish);
  }
}

void clickLargo(Button2& btn) {
  auto_mode = (auto_mode == 1) ? 0 : 1;
  camposPublicacion |= PUB_AUTO_MODE; 
  xSemaphoreGive(semPublish);         
}

void taskBotones(void *pvParameters) {
  info_tarea_actual();
  boton.begin(BOTONPIN);
  boton.setLongClickTime(2000); 
  boton.setTapHandler(clickCorto);
  boton.setLongClickDetectedHandler(clickLargo);
  while(true) {
    boton.loop(); 
    vTaskDelay(pdMS_TO_TICKS(15));
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
  xTaskCreate(taskBotones, "UI_Buttons", 3072, NULL, 2, NULL);
  vTaskDelete(NULL);
}

void loop() {}
