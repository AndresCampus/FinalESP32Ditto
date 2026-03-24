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
  Serial.println(DEBUG_STRING + "Iniciando Pull-on-Boot (Sincronización Avanzada)...");
  
  WiFiClientSecure client;
  client.setInsecure(); // Saltamos validación para agilizar en laboratorios
  
  HTTPClient http;
  String url = "https://ditto.iot-uma.es/api/2/things/" + NAMESPACE + ":" + THING_NAME + "/features";
  
  http.begin(client, url);
  http.setAuthorization(mqtt_user.c_str(), mqtt_pass.c_str());
  
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error) {
      bool hayCambiosPendientes = false;
      Serial.println(DEBUG_STRING + "¡Features descargadas! Analizando discrepancias...");
      
      // Estructura para procesar cada feature de forma atómica
      auto sincronizarFeature = [&](const char* featureName, int &variableLocal, uint8_t bitPublicacion) {
        bool hasDesired = doc[featureName]["desiredProperties"].containsKey("value");
        bool hasReported = doc[featureName]["properties"].containsKey("value");
        
        int valDesired = hasDesired ? doc[featureName]["desiredProperties"]["value"].as<int>() : -1;
        int valReported = hasReported ? doc[featureName]["properties"]["value"].as<int>() : -1;

        // 1. Alineamos la variable local con lo más nuevo del gemelo
        if (hasDesired) {
          variableLocal = valDesired;
        } else if (hasReported) {
          variableLocal = valReported;
        }

        // 2. ¿Hay una orden pendiente en el gemelo? (Desired != Reported)
        if (hasDesired && (!hasReported || valDesired != valReported)) {
          camposPublicacion |= bitPublicacion;
          hayCambiosPendientes = true;
        }
      };

      // Sincronizamos cada parámetro ambiental y de control
      sincronizarFeature("auto_mode_DP", auto_mode_DP, PUB_AUTO_MODE);
      sincronizarFeature("threshold_vent_DP", threshold_vent_DP, PUB_THRESHOLD);
      sincronizarFeature("publish_delta_DP", publish_delta_DP, PUB_PUB_DELTA);

      // Tratamiento especial para el relé (debe obedecer al cloud si estamos en Manual)
      bool hasDesiredRelay = doc["vent_relay_DP"]["desiredProperties"].containsKey("value");
      bool hasReportedRelay = doc["vent_relay_DP"]["properties"].containsKey("value");
      int valDesiredRelay = hasDesiredRelay ? doc["vent_relay_DP"]["desiredProperties"]["value"].as<int>() : -1;
      int valReportedRelay = hasReportedRelay ? doc["vent_relay_DP"]["properties"]["value"].as<int>() : -1;

      if (hasDesiredRelay) {
        if (auto_mode_DP == 0) vent_relay_DP = valDesiredRelay;
      } else if (hasReportedRelay) {
        if (auto_mode_DP == 0) vent_relay_DP = valReportedRelay;
      }

      // Aplicar estado físico tras el arranque
      digitalWrite(RELAYPIN, vent_relay_DP ? HIGH : LOW);
      digitalWrite(LEDPIN, vent_relay_DP ? HIGH : LOW);

      if (hasDesiredRelay && (!hasReportedRelay || valDesiredRelay != valReportedRelay)) {
        camposPublicacion |= PUB_RELAY;
        hayCambiosPendientes = true;
      }

      if (hayCambiosPendientes) {
        Serial.println(DEBUG_STRING + "Detectados cambios pendientes en la nube. Enviando confirmación...");
        xSemaphoreGive(semPublish);
      } else {
        Serial.println(DEBUG_STRING + "Sistema sincronizado. Sin cambios pendientes.");
      }
      
    } else {
      Serial.println(DEBUG_STRING + "Error parseando JSON: " + String(error.c_str()));
    }
  } else {
    Serial.println(DEBUG_STRING + "Error en Pull-on-Boot (HTTP " + String(httpCode) + ")");
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
    // 1. Leer el entorno (Potenciómetro y DHT22)
    int rawADC = analogRead(POTPIN);
    int current_ppm = map(rawADC, 0, 4095, 400, 5000);
    TempAndHumidity newValues = dht.getTempAndHumidity();
    
    // 2. Actualizar variables globales
    global_ppm = current_ppm;
    if (dht.getStatus() == 0) {
      global_temp = newValues.temperature;
      global_hum = newValues.humidity;
    }
    
    // 3. Representamos visualmente el estado del aire instantáneamente
    mostrarCalidadAire(current_ppm);
    
    // 4. MOTOR DE REGLAS 1: Control de Ventilación Automática
    bool state_changed = false;
    if (auto_mode_DP == 1) {
      int deseado = (current_ppm >= threshold_vent_DP) ? 1 : 0;
      
      if (deseado != vent_relay_DP) {
        vent_relay_DP = deseado;
        digitalWrite(RELAYPIN, vent_relay_DP ? HIGH : LOW);
        digitalWrite(LEDPIN, vent_relay_DP ? HIGH : LOW);
        
        Serial.println(DEBUG_STRING + ">>> AUTÓMATA: Cambio de estado a: " + String(vent_relay_DP));
        state_changed = true; // Forzamos publicación para avisar a Ditto
      }
    }
    
    // 5. MOTOR DE REGLAS 2: Criterios de Publicación por Red
    bool time_elapsed = (millis() - last_publish_time >= PERIODO_PUBLICACION);
    bool delta_exceeded = (abs(current_ppm - last_published_ppm) >= publish_delta_DP);
    
    // Si se cumple CUALQUIERA de las condiciones, disparamos el publicador
    if (time_elapsed || delta_exceeded || state_changed) {
      if (time_elapsed) camposPublicacion |= (PUB_TEMP | PUB_HUM | PUB_AIR); 
      if (delta_exceeded) camposPublicacion |= PUB_AIR; 
      if (state_changed) camposPublicacion |= PUB_RELAY; 

      last_publish_time = millis();
      last_published_ppm = current_ppm;
      
      Serial.println(DEBUG_STRING + "Evento detectado. Despertando publicador...");
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
    if (currentBits & PUB_RELAY)     telemetria["vent_relay_DP"]     = vent_relay_DP;
    if (currentBits & PUB_AUTO_MODE) telemetria["auto_mode_DP"]      = auto_mode_DP;
    if (currentBits & PUB_THRESHOLD) telemetria["threshold_vent_DP"] = threshold_vent_DP;
    if (currentBits & PUB_PUB_DELTA) telemetria["publish_delta_DP"]  = publish_delta_DP;
    if (telemetria.size() == 0) continue;
    String jsonStr;
    serializeJson(telemetria, jsonStr);
    mqtt_client.publish(topic_TELEMETRIA.c_str(), jsonStr.c_str());
  }
}

void clickCorto(Button2& btn) {
  if (auto_mode_DP == 0) {
    vent_relay_DP = (vent_relay_DP == 1) ? 0 : 1; 
    digitalWrite(RELAYPIN, vent_relay_DP ? HIGH : LOW);
    digitalWrite(LEDPIN, vent_relay_DP ? HIGH : LOW);
    Serial.println(DEBUG_STRING + ">>> [BOTÓN] Relé conmutado a: " + String(vent_relay_DP));
    camposPublicacion |= PUB_RELAY;
    xSemaphoreGive(semPublish);
  } else {
    Serial.println(DEBUG_STRING + ">>> [BOTÓN] Acción ignorada: El Modo Automático está activo.");
  }
}

void clickLargo(Button2& btn) {
  auto_mode_DP = (auto_mode_DP == 1) ? 0 : 1; // Alternamos el modo global
  if (auto_mode_DP == 1) Serial.println(DEBUG_STRING + ">>> [BOTÓN] MODO AUTOMÁTICO ACTIVADO");
  else Serial.println(DEBUG_STRING + ">>> [BOTÓN] MODO MANUAL ACTIVADO");
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
