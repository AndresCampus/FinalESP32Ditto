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

## 4.1. Punto de Partida: La Plantilla Inicial
Para focalizar el aprendizaje en la arquitectura de red y el modelo de concurrencia avanzado, se proporciona a los alumnos el proyecto Wokwi con el firmware inicial 👉 **[Proyecto inicial - Wokwi](https://wokwi.com/projects/459309993466875905)**

Este fichero ya viene configurado con el paradigma de **FreeRTOS** (eliminando el uso de la función secuencial `loop()`) e incluye dos tareas concurrentes pre-programadas:

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
