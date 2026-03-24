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

  // Verificamos si el mensaje viene por el topic de comandos (Downlink)
  if(String(topic) == topic_COMANDOS) {
    
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, mensaje);
    
    if (error) {
      Serial.print(DEBUG_STRING + "Error parseando JSON del comando: ");
      Serial.println(error.c_str());
      return;
    }

    String path = doc["path"].as<String>();
    
    if (path.endsWith("/refresh")) {
      Serial.println(DEBUG_STRING + "¡Orden de REFRESH recibida! Despertando publicador de telemetría...");
      
      // MAGIA RTOS: Disparamos la señal marcando todo el mapa de bits
      camposPublicacion = PUB_ALL;
      xSemaphoreGive(semPublish);
      
      // Construir respuesta de confirmación en formato Ditto Protocol (Uplink)
      StaticJsonDocument<256> respDoc;
      respDoc["topic"] = doc["topic"]; // Copiamos topic
      respDoc["path"] = doc["path"];   // Copiamos path
      respDoc["status"] = 200;         // 200 = Éxito total
      
      // Devuelvemos el correlator ID crucial para que Ditto cierre esa petición específica 
      JsonObject headers = respDoc.createNestedObject("headers");
      headers["correlation-id"] = doc["headers"]["correlation-id"];
      headers["content-type"] = "application/json";
      
      JsonObject value = respDoc.createNestedObject("value");
      value["status"] = 200;
      value["message"] = "Telemetría forzada con éxito hacia la plataforma central.";
      
      String respuestaStr;
      serializeJson(respDoc, respuestaStr);
      
      // Confirmamos el OK
      mqtt_client.publish(topic_RESPUESTAS.c_str(), respuestaStr.c_str());
      Serial.println(DEBUG_STRING + "Respuesta devuelta a Eclipse Ditto.");
    }
    else {
      Serial.println(DEBUG_STRING + "Comando no reconocido: " + path);
    }
  }
  
  // Verificamos si es un mensaje de cambio de Estado / Parámetros (Desired Properties)
  else if (String(topic) == topic_DESIRED) {
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, mensaje);
    
    if (error) {
      Serial.println(DEBUG_STRING + "Error parseando JSON de desiredProperties");
      return;
    }
    
    bool hayCambios = false;

    // ----- 1. VENT_RELAY -----
    if (doc.containsKey("vent_relay")) {
      // Intentamos extraerlo anidado (estándar de Ditto) o aplanado por seguridad
      int nuevo_vr =  doc["vent_relay"]["value"].as<int>();
                     
      if (auto_mode == 0) { // Solo si estamos en MODO MANUAL actuamos físicamente
        vent_relay = nuevo_vr;
        digitalWrite(RELAYPIN, vent_relay ? HIGH : LOW);
        digitalWrite(LEDPIN, vent_relay ? HIGH : LOW);
        Serial.println(DEBUG_STRING + ">>> [DESIRED] Relé forzado a " + String(vent_relay));
        camposPublicacion |= PUB_RELAY; // Activamos bandera del relé
        hayCambios = true;
      } else {
        Serial.println(DEBUG_STRING + ">>> [DESIRED] Petición sobre RELÉ ignorada (Automático Activo)");
      }
    }
    
    // ----- 2. AUTO_MODE -----
    if (doc.containsKey("auto_mode")) {
      auto_mode = doc["auto_mode"]["value"].as<int>();
      Serial.println(DEBUG_STRING + ">>> [DESIRED] Auto_mode asignado a " + String(auto_mode));
      camposPublicacion |= PUB_AUTO_MODE;
      hayCambios = true;
    }
    
    // ----- 3. THRESHOLD_VENT -----
    if (doc.containsKey("threshold_vent")) {
      threshold_vent = doc["threshold_vent"]["value"].as<int>();
      Serial.println(DEBUG_STRING + ">>> [DESIRED] Umbral de Alarma fijado en " + String(threshold_vent) + " ppm");
      camposPublicacion |= PUB_THRESHOLD;
      hayCambios = true;
    }

    // ----- 4. PUBLISH_DELTA -----
    if (doc.containsKey("publish_delta")) {
      publish_delta = doc["publish_delta"]["value"].as<int>();
      Serial.println(DEBUG_STRING + ">>> [DESIRED] Delta rápido fijado en " + String(publish_delta) + " ppm");
      camposPublicacion |= PUB_PUB_DELTA;
      hayCambios = true;
    }

    if (hayCambios) {
      xSemaphoreGive(semPublish);
      Serial.println(DEBUG_STRING + "Cambios locales aplicados. Despertando a Publicador para confirmar update...");
    }
  }
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
      sincronizarFeature("auto_mode", auto_mode, PUB_AUTO_MODE);
      sincronizarFeature("threshold_vent", threshold_vent, PUB_THRESHOLD);
      sincronizarFeature("publish_delta", publish_delta, PUB_PUB_DELTA);

      // Tratamiento especial para el relé (debe obedecer al cloud si estamos en Manual)
      bool hasDesiredRelay = doc["vent_relay"]["desiredProperties"].containsKey("value");
      bool hasReportedRelay = doc["vent_relay"]["properties"].containsKey("value");
      int valDesiredRelay = hasDesiredRelay ? doc["vent_relay"]["desiredProperties"]["value"].as<int>() : -1;
      int valReportedRelay = hasReportedRelay ? doc["vent_relay"]["properties"]["value"].as<int>() : -1;

      if (hasDesiredRelay) {
        if (auto_mode == 0) vent_relay = valDesiredRelay;
      } else if (hasReportedRelay) {
        if (auto_mode == 0) vent_relay = valReportedRelay;
      }

      // Aplicar estado físico tras el arranque
      digitalWrite(RELAYPIN, vent_relay ? HIGH : LOW);
      digitalWrite(LEDPIN, vent_relay ? HIGH : LOW);

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
    if (dht.getStatus() == 0) {
      global_temp = newValues.temperature;
      global_hum = newValues.humidity;
    }
    
    // 3. Representamos visualmente el estado del aire instantáneamente
    mostrarCalidadAire(current_ppm);
    
    // 4. Evaluar Actuadores Duros (Ventilación) si estamos en Modo Automático
    bool state_changed = false;
    if (auto_mode == 1) {
      // Tomamos decisión algorítmica basada en el umbral
      int deseado = (current_ppm >= threshold_vent) ? 1 : 0;
      
      // Si el sensor decide cambiar de estado al ventilador...
      if (deseado != vent_relay) {
        vent_relay = deseado;
        digitalWrite(RELAYPIN, vent_relay ? HIGH : LOW);
        digitalWrite(LEDPIN, vent_relay ? HIGH : LOW); // LED chivato secundario
        
        Serial.println(DEBUG_STRING + ">>> AUTÓMATA: Cambio de estado de ventilación a: " + String(vent_relay));
        state_changed = true; // Forzamos una publicación inmediata hacia la nube
      }
    }
    
    // 5. Evaluar el Motor de Reglas de publicación por red
    // - Ha pasado el tiempo estipulado (30 seg)
    bool time_elapsed = (millis() - last_publish_time >= PERIODO_PUBLICACION);
    // - Ocurrió un cambio abrupto de aire superior a nuestro umbral
    bool delta_exceeded = (abs(current_ppm - last_published_ppm) >= publish_delta);
    
    // Si ha pasado el tiempo, o ha cambiado mucho el aire, o el relé acaba de saltar solo
    if (time_elapsed || delta_exceeded || state_changed) {
      // Configuramos el mapa de bits estricto en función del trigger
      if (time_elapsed) {
        camposPublicacion |= (PUB_TEMP | PUB_HUM | PUB_AIR); // Refresco general ambiental
      }
      if (delta_exceeded) {
        camposPublicacion |= PUB_AIR; // Solo notificamos el salto de CO2 crítico
      }
      if (state_changed) {
        camposPublicacion |= PUB_RELAY; // Notificamos la reacción en vivo del autómata
      }

      // Actualizamos registros
      last_publish_time = millis();
      last_published_ppm = current_ppm;
      
      Serial.println(DEBUG_STRING + "Criterio de publicación alcanzado. Levantando bandera...");
      // Magia: Elevamos la bandera del semáforo para despertar a la Tarea 3
      xSemaphoreGive(semPublish);
    }

    // A dormir 2 segundos cediendo la CPU
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

//-----------------------------------------------------
// TAREA 3: Publicador MQTT (Event-Driven)
//-----------------------------------------------------
void taskPublisher(void *pvParameters) {
  info_tarea_actual();
  
  while(true) {
    // Esta tarea duerme infinitamente (0% CPU) hasta que alguien lance xSemaphoreGive()
    xSemaphoreTake(semPublish, portMAX_DELAY);
    
    // Copiamos el bitmap atómicamente por seguridad y lo purgamos para la próxima vez
    uint8_t currentBits = camposPublicacion;
    camposPublicacion = 0; 
    
    StaticJsonDocument<256> telemetria;
    
    // Si ha despertado, empaqueta SOLO lo marcado con bits a 1 usando operaciones unitarias AND
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

//-----------------------------------------------------
// TAREA 4 (NUEVA): Gestión Asíncrona del Botón Físico (UI)
//-----------------------------------------------------

// --- Callbacks de Button2 ---
void clickCorto(Button2& btn) {
  // Sólo conmutamos a mano si estamos en el modo Manual
  if (auto_mode == 0) {
    vent_relay = (vent_relay == 1) ? 0 : 1; // Alternamos la placa física
    digitalWrite(RELAYPIN, vent_relay ? HIGH : LOW);
    digitalWrite(LEDPIN, vent_relay ? HIGH : LOW);
    
    Serial.println(DEBUG_STRING + ">>> RELÉ CONMUTADO MANUALMENTE A: " + String(vent_relay));
    camposPublicacion |= PUB_RELAY; // Marcamos el pin de relé en el json
    xSemaphoreGive(semPublish); 
  } else {
    Serial.println(DEBUG_STRING + ">>> Acción ignorada (Modo Automático Activo)");
  }
}

void clickLargo(Button2& btn) {
  auto_mode = (auto_mode == 1) ? 0 : 1; // Alternamos el modo de control global
  
  // Retroalimentación LED asíncrona (usamos vTaskDelay para no bloquear totalmente el RTOS)
  digitalWrite(LEDPIN, LOW); 
  vTaskDelay(pdMS_TO_TICKS(150));
  
  if (auto_mode == 1) {
    Serial.println(DEBUG_STRING + ">>> MODO AUTOMÁTICO ACTIVADO");
    digitalWrite(LEDPIN, HIGH); vTaskDelay(pdMS_TO_TICKS(150));
    digitalWrite(LEDPIN, LOW);  vTaskDelay(pdMS_TO_TICKS(150));
  } else {
    Serial.println(DEBUG_STRING + ">>> MODO MANUAL ACTIVADO");
    digitalWrite(LEDPIN, HIGH); vTaskDelay(pdMS_TO_TICKS(150));
    digitalWrite(LEDPIN, LOW);  vTaskDelay(pdMS_TO_TICKS(150));
    digitalWrite(LEDPIN, HIGH); vTaskDelay(pdMS_TO_TICKS(150));
    digitalWrite(LEDPIN, LOW);  vTaskDelay(pdMS_TO_TICKS(150));
  }
  
  // Devolvemos el estado físico como lo mande la variable global actual
  digitalWrite(LEDPIN, vent_relay ? HIGH : LOW);
  
  camposPublicacion |= (PUB_AUTO_MODE | PUB_RELAY); // Hubo un switch de filosofía
  xSemaphoreGive(semPublish); 
}

// --- Cuerpo de la Tarea FreeRTOS ---
void taskBotones(void *pvParameters) {
  info_tarea_actual();
  
  boton.begin(BOTONPIN);
  boton.setLongClickTime(2000);
  boton.setTapHandler(clickCorto);
  boton.setLongClickDetectedHandler(clickLargo);
  
  Serial.println(DEBUG_STRING + "Sistema de botones UI activado.");

  while(true) {
    boton.loop();
    vTaskDelay(pdMS_TO_TICKS(15)); // Respira para ceder CPU al resto de tareas
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

  // 3. Tarea de Publicación (Prioridad Baja: 1) Duerme esperando semPublish
  xTaskCreate(taskPublisher, "Publisher", 4096, NULL, 1, NULL);

  // 4. Tarea de la Interfaz de Usuario/Botones (Prioridad Media: 2) Escanea en tiempo real
  xTaskCreate(taskBotones, "UI_Buttons", 3072, NULL, 2, NULL);

  Serial.println(DEBUG_STRING + "Setup (core) terminado, el sistema queda en manos de FreeRTOS.");
  
  // Eliminamos la tarea loop() ya que en FreeRTOS no necesitamos este bucle
  vTaskDelete(NULL);
}

void loop() {
  // El loop de Arduino queda inutilizado y liberado gracias a las tareas FreeRTOS
}
