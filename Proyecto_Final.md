# Proyecto Final

## 1. Introducción
El objetivo de este proyecto final es diseñar, desarrollar y desplegar una arquitectura IoT industrial completa basándonos en el paradigma de **Gemelos Digitales (Digital Twins)**. 

Partiendo de un microcontrolador ESP32 equipado con sensores ambientales y actuadores físicos, construiremos un flujo de datos bidireccional usando el protocolo **MQTT**. Los sensores informarán del estado físico en tiempo real a la plataforma **Eclipse Ditto** (que mantendrá una representación virtual del dispositivo), almacenando el histórico en **InfluxDB** y permitiendo a un operador remoto observar y gobernar el sistema mediante un panel de control profesional diseñado en **Node-RED Dashboard 2.0** y **Grafana**.

---

## 2. Objetivos de Aprendizaje
Al finalizar con éxito este proyecto, el estudiante será capaz de:
* **Integración Hardware:** Adquirir y procesar datos físicos (Temperatura, Humedad, CO2) usando DHT22 y conversores ADC, controlando hardware reactivo (NeoPixel, Relés).
* **Concurrencia con FreeRTOS:** Abandonar el paradigma *Super-Loop* (`loop()` de Arduino) para diseñar un sistema robusto multitarea con colas y semáforos, separando las comunicaciones de red del procesamiento de los sensores físicos.
* **Protocolos IoT y Gemelos Digitales:** Comprender y aplicar el *Ditto Protocol* sobre conexiones usando el protocoloMQTT para mantener actualizado el estado del Gemelo desde el dispositivo físico. Mantener sicronizado en el dispositivo el estado deseado (`desiredProperties`) frente al estado real (`properties`) reflejado en el Gemelo. Publicar en MQTT las actualizaciones del gemelo para poder encaminarlas hacia InfluxDB, Grafana y Node-RED.
* **Diseño e Interfaces (HMI):** Desarrollar un "Centro de Comando" (Dashboard) web reactivo para la monitorización de telemetría y el telecontrol (envío de comandos, actuación de relés, cambio de configuraciones en caliente).
* **API REST Ditto:** Usar la API REST de Ditto para interactuar con el Gemelo Digital y obtener información del estado del Gemelo Digital desde el dispositivo o el dashboard (por ejemplo al iniciar, *pull-on-boot*). También para mandarle mensajes al dispositivo físico a través de Ditto.

---

## 3. Material Necesario y Arquitectura Base
* **Hardware:** Placa de desarrollo ESP32, Sensor DHT22, Potenciómetro (simulador de sensor de calidad de aire/CO2), Módulo Relé, Anillo LED NeoPixel y un pulsador físico.
* **Software Local:** Arduino IDE (optimizado con librerías FreeRTOS, `ArduinoJson`, `PubSubClient`, y `Button2`).
* Al ser un *laboratorio virtual*, no es necesario disponer del hardware físico, se utilizará Wokwi para simular el entorno físico.

### 3.2. Servicios Disponibles (Cloud)
Todos los servicios del ecosistema están accesibles desde el exterior de forma segura a través del puerto estándar HTTPS (443), por lo que no es necesario especificar ningún puerto en la URL. Para acceder a ellos, debes utilizar las credenciales de conexión que se indican en la tarea correspondiente del Campus Virtual.

| Servicio | URL de Acceso (Hostname) | Descripción |
| :--- | :--- | :--- |
| **Node-RED (Editor)** | `micro#.iot-uma.es` | Tu instancia individual para el diseño de flujos en Node-RED. *(Reemplaza el # por tu número asignado)*. |
| **Node-RED (Dashboard)** | `micro#.iot-uma.es/dashboard` | Panel de control e interfaz de usuario de tu instancia de Node-RED. |
| **Bróker MQTT** | `mqtt.iot-uma.es` | El servidor central para el envío y recepción de mensajes de los dispositivos. |
| **Eclipse Ditto** | `ditto.iot-uma.es` <br> `10.10.10.201:8080` | Punto de acceso a la API REST para interactuar con los Gemelos Digitales. Desde la instancia Node-RED usar la IP local. |
| **InfluxDB** | `influxdb.iot-uma.es` | Interfaz web de la base de datos temporal para explorar datos y configurar tareas. |
| **Grafana** | `grafana.iot-uma.es` | Plataforma analítica para consultar datos y crear los cuadros de mando visuales. |

### 3.1. Simulación del Entorno Físico (Wokwi)
El montaje hardware se ejecutará en **Wokwi** (usando la placa **[ESP32-C3-DevKitM-1](https://docs.espressif.com/projects/esp-idf/en/v5.2/esp32c3/hw-reference/esp32c3/user-guide-devkitm-1.html#esp32-c3-devkitm-1)**). 
![image](https://hackmd.io/_uploads/BkZQG305bl.png)
El cableado electrónico de la placa final usa el siguiente mapa de pines:
| Componente | Pin (GPIO) ESP32-C3 | Función en el Gemelo |
| :--- | :---: | :--- |
| **Sensor DHT22** | `GPIO 1` | Telemetría base: Temperatura y Humedad |
| **Potenciómetro** | `GPIO 2` | *(Entrada Analógica ADC).* Simula un sensor analógico de MQs/CO2 (Rango 400 - 5000 ppm) |
| **Botón (Pushbutton)** | `GPIO 3` | Permite alternar la filosofía del Gemelo (Auto/Manual) o forzar la ventilación. Usa resistencia *PULLUP* habilitada por software |
| **Módulo de Relé** | `GPIO 4` | Actuador final: Simula la puesta en marcha de los motores de extracción del aire (Ventilación) |
| **LED RGB NeoPixel** | `GPIO 8` | Anillo *WS2812B*. Output visual en tiempo real de la simulación del aire sucio (Verde, Naranja, Rojo) |
| **LED Testigo Normal** | `GPIO 5` | Chivato de estado de operación y flasheo de acuse de recibo de tramas MQTT |

### Entorno de pruebas HW Preparado en Wokwi
Puedes acceder a todo el entorno hardware ya emsamblado de fábrica, junto con un pequeño firmware local de prueba (sin las librerías de red todavía), haciendo clic en el siguiente enlace:
👉 **[Proyecto Hardware Fase 0 - Wokwi](https://wokwi.com/projects/459079537457837057)**

Este proyecto te permitirá entender la lógica transaccional de los pines antes de pasar a la nube. Incluye una máquina de estados local que gobierna la circuitería:
* Al presionar el botón de forma prolongada (durante más de 2 segundos), se alterna lógicamente entre el **Modo Automático** y **Manual** del actuador (el LED físico destellará para confirmarte el cambio de filosofía).
* Estando exclusivamente en modo Manual, dar una **pulsación corta** sobre el botón conmutará instantáneamente el estado del Relé simulado.
* El **LED estándar** permanece encendido siempre que el relé esté activo, sirviendo como chivato visual principal de la extracción.
* El **anillo de LEDs RGB NeoPixel** funciona como un semáforo dinámico de la calidad del aire (Verde, Naranja, Rojo) que reacciona de forma inmediata al alterar la lectura del simulador girando el **potenciómetro**.
* Finalmente, si prestas atención a la pestaña inferior de la **Consola Serie (Serial Monitor)**, podrás monitorear un *log* en tiempo real que imprime los valores medidos por el DHT22, los ppm de CO2 calculados y el reporte de estados de las variables internas.

---

## 4. Comienzo del Trabajo
El proyecto se dividirá en las siguientes fases incrementales (que iremos detallando en los próximos apartados):
1. **Fase 1:** Construcción de Tareas FreeRTOS (Lectura vs Comunicaciones).
2. **Fase 2:** Emparejamiento MQTT bidireccional y modelado en Eclipse Ditto.
3. **Fase 3:** Sistema guiado por eventos (Manejo del pulsador con librerías y semáforos).
4. **Fase 4:** Diseño del Panel de Mando reactivo a través de Node-RED.

## 4.1. Creación del Gemelo en Eclipse Ditto
Antes de abrir el simulador Wokwi y escribir una sola línea de código en C++, nuestra máxima prioridad es **aprovisionar el modelo de datos virtual** en la nube. Necesitamos indicarle a la plataforma Eclipse Ditto qué atributos físicos y funcionales (Features) va a tener nuestra máquina y qué política de acceso lo gobernará.

Abre un terminal o usa una consola local (Git Bash, WSL, Terminal de macOS/Linux) y ejecuta el siguiente comando *cURL* para registrar estructuralmente tu gemelo digital. 

> [!CAUTION]
> **⚠️ MUY IMPORTANTE:** El siguiente *script* pertenece al usuario de pruebas `micro1`. Debes **reemplazar cuidadosamente TODAS las apariciones** de la palabra `micro1` por tu **nombre de usuario asignado** en el Moodle, así como reemplazar la contraseña correspondiente. 

```bash
curl -X PUT 'https://ditto.iot-uma.es/api/2/things/micro1:ESP32-final' \
  -u 'micro1:PASSWORD' \
  -H 'Content-Type: application/json' \
  -d '{
    "policyId": "micro1:policy",
    "attributes": {
      "location": "Laboratorio de Electronica",
      "manufacturer": "micro1",
      "model": "ESP32-C3"
    },
    "features": {
      "temperature": { "properties": { "value": 25, "unit": "celsius" } },
      "humidity": { "properties": { "value": 50, "unit": "percentage" } },
      "air_quality": { "properties": { "value": 400, "unit": "ppm", "range": [400,5000] } },
      "online": { "properties": { "value": 0, "states": { "0": "Offline", "1": "Online" } } },
      "vent_relay":{ "properties": { "value": 0, "states": { "0": "OFF", "1": "ON" }}, "desiredProperties": {"value": 0 } },
      "auto_mode": { "properties": { "value": 0, "states": { "0": "Manual", "1": "Auto" }}, "desiredProperties": {"value": 0 } },
      "threshold_vent": { "properties": { "value": 1000, "unit": "ppm"}, "desiredProperties": {"value": 1000 } },
      "publish_delta": { "properties": { "value": 100, "unit": "ppm"}, "desiredProperties": {"value": 100 } }
    }
  }'
```

**Verificación Visual:** Una vez que la consola te devuelva un acuse de recibo de éxito, dirígete con tu navegador web a la UI visual de Ditto (👉 **[https://ditto.iot-uma.es/ui/](https://ditto.iot-uma.es/ui/)**). Inicia sesión con tus credenciales, localiza tu *Thing* (ej. `micro1:ESP32-final`) y comprueba gráficamente que todos los sub-árboles JSON de `features` han aparecido correctamente.

---

## 4.2. Punto de Partida: La Plantilla Inicial
Para focalizar el aprendizaje en la arquitectura de red y el modelo de concurrencia avanzado, se proporciona a los alumnos el proyecto Wokwi con el firmware inicial 👉 **[Proyecto inicial - Wokwi](https://wokwi.com/projects/459309993466875905)**

Este proyecto ya viene programado con el modelo de programación concurrente de **FreeRTOS** (eliminando el uso de la función secuencial `loop()`) e incluye dos tareas concurrentes programadas:

1. **`taskMQTTService` (Prioridad Alta):**
   - Se encarga de levantar y mantener viva la conexión **WiFi** y establecer el circuito **MQTT** contra el *broker*.
   - Se suscribe automáticamente a los topics de Eclipse Ditto (`mensajes` y `desiredproperties`).
   - Una vez la red es estable, usa un *Semáforo Binario* (`semMqttReady`) para avisar al resto del sistema de que puede empezar a funcionar.
2. **`taskReader` (Prioridad Media):**
   - Tarea en reposo, bloqueada hasta que la tarea MQTT suelta el semáforo `semMqttReady`.
   - Una vez activa, escanea cada **2 segundos** la temperatura, humedad y el nivel de CO2 simulado por el potenciómetro.
   - Traduce instantáneamente ese nivel de CO2 en un código de colores sobre el **Anillo LED NeoPixel** (Verde = Óptimo, Naranja = Regular, Rojo = Crítico) para dar *feedback* visual físico.
   - Vuelca en la **Consola Serie** el valor exacto de los sensores para que puedas observar gráficamente en Wokwi que el ESP32 está vivo e interactuando con el hardware.

Con esta base 100% funcional aseguramos la capa física de adquisición de datos y la capa de transporte. **A partir de aquí, el documento irá describiendo los retos (Fases) que tendrás que ir programando tú mismo paso a paso.**

---

## 5. Fase 1: Añadiendo la Tarea Publicadora (Event-Driven)

### 5.1 Contexto y Objetivos
La *Plantilla Inicial* que estás utilizando tiene un problema a nivel IoT: es aislada. Es capaz de leer el sensor de temperatura y activar las luces NeoPixel, pero toda esa información se queda dentro del propio ESP32, sin llegar jamás a la nube.

Para enviar la información a través de la conexión WiFi (que ya se está estableciendo en la primera tarea `taskMQTTService`) necesitamos publicar mensajes en el *broker* MQTT. Sin embargo, no sería eficiente poner a la tarea `taskReader` a enviar indiscriminadamente mensajes a Internet cada vez que lee un dato de un periférico, porque la latencia de la red colapsaría y bloquearía la rápida ejecución de los botones o de las propias luces locales.

**El Objetivo:** Crear una tercera tarea independiente (`taskPublisher`) que esté en "hibernación profunda" (consumiendo un 0% de CPU). La tarea lectora la despertará (usando un *Semáforo FreeRTOS*) únicamente cuando hayan transcurrido 30 segundos de silencio de red y sea obligatorio publicar la telemetría, ahorrando ancho de banda.

### 5.2 El Código (Solución a implementar)
Copia esta función íntegra. Es la nueva Tarea que empaquetará las variables globales de los sensores en formato JSON y las publicará a Ditto por MQTT:

```cpp
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
```

### 5.3 Instrucciones de Inserción
Para hacer que la nueva tarea del publicador funcione, tienes que hacer **tres modificaciones** en tu proyecto de Wokwi:

1. **Pegar la Tarea:** Copia el bloque superior y pégalo justo antes del comentario `//   SETUP Y LOOP PRINCIPAL`.
2. **Despertar al publicador cada 30 segundos:** Localiza en el cuerpo de la función `taskReader` la zona final inferior del bucle `while(true)`. Reemplaza las últimas líneas (donde imprime al log y hace el retardo) por esta nueva lógica temporal basada en memoria `millis()` y Semáforos:
   ```cpp
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
    ```
3. **Instanciar la Tarea en el Sistema Operativo:** Ve a la parte final del archivo, concretamente a la función core de configuración `setup()`. Localiza el lugar donde se inicializa la `taskReader` (`xTaskCreate(...)`) e inyecta debajo la orden para que FreeRTOS reserve memoria tu nueva tercera Tarea bajo una prioridad mínima (1):
   ```cpp
   // 3. Tarea de Publicación (Prioridad Baja: 1) Guiada por eventos
   xTaskCreate(taskPublisher, "Publisher", 4096, NULL, 1, NULL);
   ```

### 5.4 Comprobación Visual
1. Dale al botón **Play** del simulador Wokwi.
2. Abre la pestaña inferior de la **Consola Serie**.
3. Verás que cada 2 segundos se escanean localmente en amarillo la temperatura y las ppm del potenciómetro. 
4. **Alcanzados los primeros 30 segundos de reloj**,  observarás de pronto algo distinto: el hilo de lectura imprime `Periodo (30s) cumplido...`, lo cual desencadena inmediatamente la ejecución de nuestra nueva y durmiente tarea, lanzando el log `>>> [MQTT UPLINK] : {"temperature":24,"humidity":40,"air_quality":400}`.
5. ¡Felicidades! Tú mismo acabas de transformar un Arduino monohilo en una placa Multitarea de clase industrial operando por eventos y semáforos.
