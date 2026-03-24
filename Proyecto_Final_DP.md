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

Antes de empezar a programar, es vital conocer claramente qué estamos a punto de construir. Al finalizar todas las fases, el dispositivo físico y su gemelo digital contarán con el siguiente abanico de **funcionalidades y lógica combinada (Edge + Cloud)**:

1. **Adquisición Automática:** El dispositivo leerá localmente sus sensores (Temperatura y Humedad vía DHT22, y Calidad de Aire simulada vía Potenciómetro ADC) cada 2 segundos ininterrumpidamente.
2. **Telemetría Inteligente (Heartbeat):** Publicará regularmente todo su estado de forma programada por MQTT cada 30 segundos (Periodo de refresco).
3. **Telemetría Basada en Eventos (Deltas):** Si el ESP32 detecta cambios bruscos en la calidad del aire superiores a una diferencia programable (`publish_delta_DP`), cortocircuitará el tiempo de espera y publicará instantáneamente la alerta de red, ahorrando ancho de banda el resto del tiempo.
4. **Auto-Gobernador Dinámico:** Monitorizará continuamente el aire para actuar directamente, y sin latencia de red, sobre un actuador duro (Relé). Abriendo así un sistema de ventilación de emergencia en el instante en el que supere un umbral de peligro (`threshold_vent_DP`) programable.
5. **UI Física Sensorial:** El usuario *in-situ* siempre estará informado del estado actual gracias a un anillo LED NeoPixel multicolor (que traduce el aire en colores) y un LED clásico que se enciende en paralelo al Relé de ventilación.
6. **Manejo Manual (HMI Integrado):** Usando un interruptor de control de placa (Botón), el usuario podrá hacer una **Pulsación Larga** para deshabilitar el Modo Automático y tomar control manual del sistema, así como usar **Pulsaciones Cortas** para forzar encendidos y apagados del ventilador a conveniencia, cruzando inmediatamente esa información a la nube.
7. **Control Remoto vía Gemelo:** Mediante el sistema de *Desired Properties* de Ditto, el administrador de la red será capaz no solo de preconfigurar los parámetros de sensibilidad del dispositivo (`publish_delta_DP` y `threshold_vent_DP`) desde la nube, sino que podrá actuar remotamente sobre el ventilador manipulando sus propiedades.
8. **Comandos RPC:** El ESP32 será capaz de subscribirse a *Messages* nativos de Eclipse Ditto, habilitando el envío directo del comando `"refresh"` para obligar a la placa a realizar un volcado absoluto de datos bajo demanda, fuera de su ciclo de 30 segundos. El dispositivo será capaz no solo de ejecutar la orden, sino de devolver una confirmación (Ack) al Gemelo Digital para cerrar la transacción.

Para lograr todo ese hito IoT, el desarrollo del proyecto se estructurará en las siguientes **fases incrementales** (que iremos resolviendo paso a paso en los próximos apartados):

1. **Fase 1:** Construcción de Tareas FreeRTOS (Capa Lógica vs Capa de Red).
2. **Fase 2:** Emparejamiento MQTT bidireccional y modelado en Eclipse Ditto.
3. **Fase 3:** Sistema asíncrono guiado por eventos (Manejo local de interrupciones del pulsador).
4. **Fase 4:** Diseño del Panel de Mando reactivo del usuario a través de Node-RED.

## 4.1. Creación del Gemelo en Eclipse Ditto
Antes de abrir el simulador Wokwi y escribir una sola línea de código en C++, nuestra máxima prioridad es **aprovisionar el modelo de datos virtual** en la nube. Necesitamos indicarle a la plataforma Eclipse Ditto qué atributos físicos y funcionales (Features) va a tener nuestra máquina y qué política de acceso lo gobernará.

Abre un terminal o usa una consola local (Git Bash, WSL, Terminal de macOS/Linux) y ejecuta el siguiente comando *cURL* para registrar estructuralmente tu gemelo digital. 

> [!CAUTION]
> **⚠️ MUY IMPORTANTE:** El siguiente *script* pertenece al usuario de pruebas `micro1`. Debes **reemplazar cuidadosamente TODAS las apariciones** de la palabra `micro1` por tu **nombre de usuario asignado** en el Moodle, así como reemplazar la contraseña correspondiente. El nombre del **gemelo digital** debería ser `ESP32-final`.

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
      "vent_relay_DP":{ "properties": { "value": 0, "states": { "0": "OFF", "1": "ON" }}, "desiredProperties": {"value": 0 } },
      "auto_mode_DP": { "properties": { "value": 0, "states": { "0": "Manual", "1": "Auto" }}, "desiredProperties": {"value": 0 } },
      "threshold_vent_DP": { "properties": { "value": 1000, "unit": "ppm"}, "desiredProperties": {"value": 1000 } },
      "publish_delta_DP": { "properties": { "value": 100, "unit": "ppm"}, "desiredProperties": {"value": 100 } }
    }
  }'
```

**Verificación Visual:** Una vez que la consola te devuelva un acuse de recibo de éxito, dirígete con tu navegador web a la UI visual de Ditto (👉 **[https://ditto.iot-uma.es/ui/](https://ditto.iot-uma.es/ui/)**). Inicia sesión con tus credenciales, localiza tu *Thing* (ej. `micro1:ESP32-final`) y comprueba gráficamente que todos los sub-árboles JSON de `features` han aparecido correctamente.
![image](https://hackmd.io/_uploads/HkQIQrkoZx.png)

---

## 4.2. Punto de partida ESP32 (Wokwi)
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

## 4.3. Flujo Inicial de Node-RED
Como complemento a la simulación física, también contarás desde el primer momento con un **Panel de Mando (Dashboard)** pre-construido en Node-RED que te permitirá visualizar en tiempo real el comportamiento de tu Gemelo Digital.

Este flujo inicial ya viene configurado para suscribirse automáticamente al topic de telemetría de Ditto y generar una interfaz gráfica amigable (Dashboard 2.0).
> [!NOTE]
> Puesto que tu *Plantilla Inicial* en C++ todavía no tiene programada la Tarea Publicadora, en este primer estadio tu Dashboard visual únicamente te confirmará si la placa ESP32 figura como **ONLINE** de cara a Ditto al arrancar Wokwi, pero la información de telemetría se mostrará cuando programes la Tarea Publicadora en la sección 5.

Para incorporar este proyecto visual a tu instancia, no necesitas descargar ningún archivo de la asignatura, solo sigue estos facilísimos pasos nativos de Node-RED:

1. **Copia (Copy code)** íntegramente todo este bloque de código JSON inferior.
2. Abre la URL de tu entorno privado (ej. `micro1.iot-uma.es`) e inicia sesión en Node-RED con tus credenciales.
3. Ve a la esquina superior derecha y haz clic en el botón de Menú Principal (`☰`) > **Import**.
4. Pega el JSON con (Ctrl+V) en el gran recuadro central que se despliega y pulsa el botón rosa de confirmar la Importación. ¡Aparecerá el flujo completo con todos sus nodos UI ensamblados!
5. Recuerda configurar el usuario y contraseña en el nodo MQTT in.
6. Haz clic en el botón **Deploy** (Arriba a la derecha) para subirlo a tu propio servidor y luego dirígete a la URl terminada en dashboard (ej. `micro1.iot-uma.es/dashboard`) para navegar por tu nuevo panel interactivo vacío.

```json
[{"id":"61cea54cc7d7714d","type":"tab","label":"Flujo Final de partida","disabled":false,"info":"","env":[]},{"id":"b69f386697442f9a","type":"config","z":"61cea54cc7d7714d","name":"Nombre de usuario","properties":[{"p":"usuario","pt":"flow","to":"micro1","tot":"str"}],"active":true,"x":190,"y":140,"wires":[]},{"id":"d820dc31dc3ff352","type":"comment","z":"61cea54cc7d7714d","name":"Configuración","info":"","x":150,"y":80,"wires":[]},{"id":"5ed9d4be806dd480","type":"config","z":"61cea54cc7d7714d","name":"Clave de usuario","properties":[{"p":"claveusuario","pt":"flow","to":"PASSWORD","tot":"str"}],"active":true,"x":190,"y":200,"wires":[]},{"id":"d2def1a4db98c885","type":"config","z":"61cea54cc7d7714d","name":"Dispositivo","properties":[{"p":"dispositivo","pt":"flow","to":"ESP32-final","tot":"str"}],"active":true,"x":170,"y":260,"wires":[]},{"id":"34e86b09963ff309","type":"mqtt in","z":"61cea54cc7d7714d","name":"iot/telemetry/<usuario>/<dispositivo>","topic":"","qos":"2","datatype":"auto-detect","broker":"8f28a739dcf74881","nl":false,"rap":true,"rh":0,"inputs":1,"x":700,"y":260,"wires":[["d5d3ea1d5bc5a0d8","15cf10f9af26f7b2","2cf086c4d636ad20","8bbce118e7f6389a","5f52c5c21333cfdc","ec5ef4ba78c59b5d"]]},{"id":"5891a0e8fe7e6f00","type":"inject","z":"61cea54cc7d7714d","name":"","props":[{"p":"topic","v":"\"iot/telemetry/\" & $flowContext(\"usuario\") &\"/\"&  $flowContext(\"dispositivo\")","vt":"jsonata"},{"p":"action","v":"subscribe","vt":"str"},{"p":"payload"}],"repeat":"","crontab":"","once":true,"onceDelay":"0.5","topic":"","payload":"","payloadType":"str","x":440,"y":260,"wires":[["34e86b09963ff309","76f04cf284948332"]]},{"id":"76f04cf284948332","type":"debug","z":"61cea54cc7d7714d","name":"debug 60","active":true,"tosidebar":true,"console":false,"tostatus":false,"complete":"true","targetType":"full","statusVal":"","statusType":"auto","x":627,"y":324,"wires":[]},{"id":"d5d3ea1d5bc5a0d8","type":"switch","z":"61cea54cc7d7714d","name":"campo online?","property":"payload.online","propertyType":"msg","rules":[{"t":"nnull"}],"checkall":"true","repair":false,"outputs":1,"x":1000,"y":200,"wires":[["ea3597fbcddd201f"]]},{"id":"ea3597fbcddd201f","type":"ui-template","z":"61cea54cc7d7714d","group":"36a310f050d3e09e","page":"","ui":"","name":"Estado conexión","order":3,"width":0,"height":0,"head":"","format":"<div style=\"display:flex; justify-content:center; align-items:center; height:100%;\">\n\n  <div style=\"display:flex; align-items:center; gap:12px;\">\n\n    \n    <!-- Indicador -->\n    <div :style=\"{\n      width: '30px',\n      height: '30px',\n      borderRadius: '50%',\n      backgroundColor: msg.payload?.online ? '#2ecc71' : '#e74c3c',\n      boxShadow: msg.payload?.online \n        ? '0 0 10px rgba(46, 204, 113, 0.8)' \n        : '0 0 10px rgba(231, 76, 60, 0.8)',\n      border: '2px solid rgba(0,0,0,0.2)'\n    }\"></div>\n\n    <!-- Estado -->\n    <span :style=\"{\n      fontWeight: 'bold',\n      fontSize: '20px',\n      color: msg.payload?.online ? '#2ecc71' : '#e74c3c',\n      lineHeight: '30px'\n    }\">\n      {{ msg.payload?.online ? \"ONLINE\" : \"OFFLINE\" }}\n    </span>\n\n     <!-- Nombre -->\n  <span\n        style=\"\n          min-width:160px;\n          font-weight:bold;\n          font-size:18px;\n          color:#5e35b1;\n          text-align:center;\n        \">\n        {{ msg.topic?.split('/')[2] + ':' + msg.topic?.split('/')[3] }}\n      </span>\n\n\n  </div>\n \n</div>","storeOutMessages":true,"passthru":true,"resendOnRefresh":true,"templateScope":"local","className":"","x":1250,"y":200,"wires":[[]]},{"id":"15cf10f9af26f7b2","type":"ui-text","z":"61cea54cc7d7714d","group":"36a310f050d3e09e","order":5,"width":0,"height":0,"name":"","label":"Topic MQTT recepción desde Ditto :  ","format":"{{msg.payload}}","layout":"row-spread","style":false,"font":"","fontSize":16,"color":"#717171","wrapText":false,"className":"","value":"topic","valueType":"msg","x":1200,"y":260,"wires":[]},{"id":"2cf086c4d636ad20","type":"debug","z":"61cea54cc7d7714d","name":"debug 61","active":true,"tosidebar":true,"console":false,"tostatus":false,"complete":"false","statusVal":"","statusType":"auto","x":980,"y":140,"wires":[]},{"id":"8bbce118e7f6389a","type":"function","z":"61cea54cc7d7714d","name":"function telemetría","func":"// 1️⃣  Recuperamos el último estado completo\nlet lastState = flow.get('lastTelemetry') || {};\n\n// 2️⃣  Normalizamos la carga entrante (puede ser parcial)\nlet inc = msg.payload || {};\n\n// 3️⃣  Si el mensaje *solo* lleva `online`, lo guardamos y salimos\nif (Object.keys(inc).length === 1 && inc.hasOwnProperty('online')) {\n    flow.set('lastOnline', inc.online);   // actualizamos el flag\n    return null;                         // no necesitamos renderizar aún\n}\n\n// 4️⃣  Mezclamos con el último flag guardado\nif (inc.online !== undefined) \n context.set('lastOnline',inc.online);\nelse \n inc.online = context.get('lastOnline') || 0;\n\n\n// 5️⃣  Merge con el resto del estado (igual que antes)\nlet merged = {\n    namespace: inc.namespace   || lastState.namespace   || \"Desconocido\",\n    thingId:   inc.thingId     || lastState.thingId     || \"Dispositivo\",\n    online:    inc.online,\n    temperature:    inc.temperature    !== undefined ? inc.temperature    : lastState.temperature,\n    humidity:       inc.humidity       !== undefined ? inc.humidity       : lastState.humidity,\n    air_quality:    inc.air_quality    !== undefined ? inc.air_quality    : lastState.air_quality,\n    vent_relay_DP:     inc.vent_relay_DP     !== undefined ? inc.vent_relay_DP     : lastState.vent_relay_DP,\n    auto_mode_DP:      inc.auto_mode_DP      !== undefined ? inc.auto_mode_DP      : lastState.auto_mode_DP,\n    threshold_vent_DP: inc.threshold_vent_DP !== undefined ? inc.threshold_vent_DP : lastState.threshold_vent_DP,\n    publish_delta_DP:  inc.publish_delta_DP  !== undefined ? inc.publish_delta_DP  : lastState.publish_delta_DP\n};\n\n// 6️⃣  Campos semánticos y colores (igual que antes)\nmerged.relayText = (merged.vent_relay_DP === 1) ? \"EXTRACCIÓN ON\" : \"REPOSO OFF\";\nmerged.modeText  = (merged.auto_mode_DP  === 1) ? \"AUTOMÁTICO\"   : \"MANUAL\";\n\nmerged.airColor   = (merged.air_quality >= (merged.threshold_vent_DP || 1000))\n                    ? \"#ef4444\"\n                    : (merged.air_quality >= 1000 ? \"#f59e0b\" : \"#10b981\");\nmerged.relayColor = (merged.vent_relay_DP === 1) ? \"#3b82f6\" : \"#334155\";\nmerged.modeColor  = (merged.auto_mode_DP  === 1) ? \"#8b5cf6\" : \"#6366f1\";\n\n// 7️⃣  Guardamos el estado completo para la próxima actualización\nflow.set('lastTelemetry', merged);\n\n// 8️⃣  Enviamos al UI\nmsg.payload = merged;\nreturn msg;\n","outputs":1,"timeout":0,"noerr":0,"initialize":"","finalize":"","libs":[],"x":1010,"y":320,"wires":[["3b97c137b90696c8","dd7b4a236519353c"]]},{"id":"3b97c137b90696c8","type":"ui-template","z":"61cea54cc7d7714d","group":"36a310f050d3e09e","page":"","ui":"","name":"Telemetría Dispositivo","order":4,"width":0,"height":0,"head":"","format":"<style>\n  @import url('https://fonts.googleapis.com/css2?family=Inter:wght@400;600;800&display=swap');\n\n  /* ------------- Card base ------------- */\n  .telemetry-glass-card {\n    font-family: 'Inter', sans-serif;\n    background: linear-gradient(135deg, rgba(30, 41, 59, 0.9) 0%, rgba(15, 23, 42, 0.95) 100%);\n    border: 1px solid rgba(255, 255, 255, 0.1);\n    border-radius: 16px;\n    padding: 24px;\n    color: #f8fafc;\n    box-shadow: 0 10px 30px rgba(0, 0, 0, 0.3);\n    max-width: 500px;\n    width: 100%;\n    margin: auto;\n    transition: transform 0.2s ease-in-out, opacity 0.4s ease;\n  }\n\n  /* Cuando el dispositivo está offline → opacidad reducida */\n  .offline {\n    opacity: 0.28;          /* ≈ 30 % visible */\n    filter: grayscale(0.6); /* opcional: le da un tono grisáceo */\n  }\n\n  /* Cuando está online → opacidad total */\n  .online {\n    opacity: 1;\n  }\n\n  /* ----------- Resto del estilo (sin cambios) ----------- */\n  .tel-header { display:flex; justify-content:space-between; align-items:center;\n                border-bottom:1px solid rgba(255,255,255,0.05); padding-bottom:12px;\n                margin-bottom:20px; }\n  .tel-header h2 { margin:0; font-size:1.1rem; font-weight:800; letter-spacing:0.5px;\n                   color:#e2e8f0; }\n  .tel-badge { background:rgba(255,255,255,0.1); padding:6px 12px; border-radius:20px;\n               font-size:0.70rem; font-weight:800; text-transform:uppercase;\n               letter-spacing:1px; color:#cbd5e1; }\n  .tel-grid { display:grid; grid-template-columns:1fr 1fr; gap:16px; }\n  .tel-stat { background:rgba(0,0,0,0.25); padding:16px; border-radius:12px;\n              display:flex; flex-direction:column; justify-content:center;\n              border:1px solid rgba(255,255,255,0.02); }\n  .tel-stat.accent { grid-column:span 2; background:rgba(0,0,0,0.35); }\n  .tel-label { font-size:0.8rem; color:#94a3b8; margin-bottom:6px;\n               font-weight:600; text-transform:uppercase; }\n  .tel-value { font-size:1.6rem; font-weight:800; color:#ffffff; }\n  .tel-value-large { font-size:2.8rem; text-shadow:0px 0px 20px rgba(0,0,0,0.5); }\n  .tel-unit { font-size:0.9rem; color:#64748b; font-weight:600; margin-left:4px; }\n  .tel-status-pill { display:inline-block; padding:8px 12px; border-radius:8px;\n                     font-size:0.85rem; font-weight:800; text-align:center;\n                     box-shadow:inset 0 2px 4px rgba(0,0,0,0.1); }\n</style>\n\n<!-- El card recibe dinámicamente la clase \"online\" / \"offline\" -->\n<div v-if=\"msg.payload\"\n     :class=\"['telemetry-glass-card',\n              msg.payload.online === 1 ? 'online' : 'offline']\">\n\n  <div class=\"tel-header\">\n    <div class=\"tel-badge\">IoT Node</div>\n    <h2>{{ msg.payload.namespace }} : {{ msg.payload.thingId }}</h2>\n  </div>\n\n  <div class=\"tel-grid\">\n    <!-- Calidad del Aire (ancho total) -->\n    <div class=\"tel-stat accent\">\n      <span class=\"tel-label\">Calidad del Aire (CO2)</span>\n      <div>\n        <span class=\"tel-value tel-value-large\"\n              :style=\"{ color: msg.payload.airColor }\">\n          {{ msg.payload.air_quality }}\n        </span>\n        <span class=\"tel-unit\">ppm</span>\n      </div>\n    </div>\n\n    <!-- Temperatura -->\n    <div class=\"tel-stat\">\n      <span class=\"tel-label\">Temperatura</span>\n      <div>\n        <span class=\"tel-value\">{{ msg.payload.temperature }}</span>\n        <span class=\"tel-unit\">°C</span>\n      </div>\n    </div>\n\n    <!-- Humedad -->\n    <div class=\"tel-stat\">\n      <span class=\"tel-label\">Humedad</span>\n      <div>\n        <span class=\"tel-value\">{{ msg.payload.humidity }}</span>\n        <span class=\"tel-unit\">%</span>\n      </div>\n    </div>\n\n    <!-- Estado Relé -->\n    <div class=\"tel-stat\" style=\"align-items:center;\">\n      <span class=\"tel-label\">Relé Ventilación</span>\n      <span class=\"tel-status-pill\"\n            :style=\"{ background: msg.payload.relayColor, color: 'white', width: '100%' }\">\n        {{ msg.payload.relayText }}\n      </span>\n    </div>\n\n    <!-- Modo Automático / Manual -->\n    <div class=\"tel-stat\" style=\"align-items:center;\">\n      <span class=\"tel-label\">Gobernador</span>\n      <span class=\"tel-status-pill\"\n            :style=\"{ background: msg.payload.modeColor, color: 'white', width: '100%' }\">\n        {{ msg.payload.modeText }}\n      </span>\n    </div>\n\n    <!-- Umbral Disparo -->\n    <div class=\"tel-stat\">\n      <span class=\"tel-label\">Umbral Disparo</span>\n      <div>\n        <span class=\"tel-value\">{{ msg.payload.threshold_vent_DP }}</span>\n      </div>\n    </div>\n\n    <!-- Refresh Δ -->\n    <div class=\"tel-stat\">\n      <span class=\"tel-label\">Refresh Δ</span>\n      <div>\n        <span class=\"tel-value\">{{ msg.payload.publish_delta_DP }}</span>\n      </div>\n    </div>\n  </div>\n</div>\n","storeOutMessages":true,"passthru":true,"resendOnRefresh":true,"templateScope":"local","className":"","x":1240,"y":320,"wires":[[]]},{"id":"0c5352af35cbc69c","type":"ui-text","z":"61cea54cc7d7714d","group":"36a310f050d3e09e","order":6,"width":0,"height":0,"name":"","label":"Última actualización sensores : ","format":"{{msg.payload}}","layout":"row-left","style":false,"font":"","fontSize":16,"color":"#717171","wrapText":false,"className":"","value":"payload","valueType":"msg","x":1210,"y":400,"wires":[]},{"id":"5f52c5c21333cfdc","type":"function","z":"61cea54cc7d7714d","name":"fecha/hora","func":"let semana = [\"Domingo\", \"Lunes\", \"Martes\", \"Miércoles\", \"Jueves\", \"Viernes\", \"Sábado\"];\nlet ahora = new Date();\nmsg.payload = semana[ahora.getDay()] + \", \"\n    + ahora.toLocaleString('es-ES', { timeZone: 'Europe/Madrid', hour12: false });\nreturn msg;\n","outputs":1,"timeout":0,"noerr":0,"initialize":"","finalize":"","libs":[],"x":970,"y":400,"wires":[["0c5352af35cbc69c"]]},{"id":"ba536abadc421306","type":"ui-slider","z":"61cea54cc7d7714d","group":"7c527cd7dda522e3","name":"","label":"Umbral disparo","tooltip":"","order":6,"width":0,"height":0,"passthru":false,"outs":"end","topic":"threshold_vent_DP","topicType":"str","thumbLabel":"always","showTicks":"true","min":"400","max":"5000","step":"100","className":"","iconPrepend":"","iconAppend":"","color":"","colorTrack":"","colorThumb":"","showTextField":false,"x":480,"y":560,"wires":[["5a9caa191309a4a7"]]},{"id":"fc476b1b996aa486","type":"ui-slider","z":"61cea54cc7d7714d","group":"7c527cd7dda522e3","name":"","label":"Delta refresco","tooltip":"","order":7,"width":0,"height":0,"passthru":false,"outs":"end","topic":"publish_delta_DP","topicType":"str","thumbLabel":"always","showTicks":"true","min":"10","max":"1000","step":"10","className":"","iconPrepend":"","iconAppend":"","color":"","colorTrack":"","colorThumb":"","showTextField":false,"x":480,"y":500,"wires":[["5a9caa191309a4a7"]]},{"id":"6bb04be3c4a615d3","type":"ui-radio-group","z":"61cea54cc7d7714d","group":"7c527cd7dda522e3","name":"","label":"Modo de funcionamiento:","order":5,"width":0,"height":0,"columns":"2","passthru":false,"options":[{"label":"MANUAL","value":0,"type":"num"},{"label":"AUTOMATICO","value":1,"type":"num"}],"payload":"","topic":"auto_mode_DP","topicType":"str","className":"","x":450,"y":620,"wires":[["5a9caa191309a4a7","30247a6031ed2d71"]]},{"id":"8b7f1b6a8a2c6425","type":"ui-button","z":"61cea54cc7d7714d","group":"7c527cd7dda522e3","name":"","label":"Encender ventilación","order":3,"width":"2","height":"1","emulateClick":false,"tooltip":"","color":"","bgcolor":"","className":"","icon":"","iconPosition":"left","payload":"1","payloadType":"num","topic":"vent_relay_DP","topicType":"str","buttonColor":"","textColor":"","iconColor":"","enableClick":true,"enablePointerdown":false,"pointerdownPayload":"","pointerdownPayloadType":"str","enablePointerup":false,"pointerupPayload":"","pointerupPayloadType":"str","x":460,"y":700,"wires":[["5a9caa191309a4a7"]]},{"id":"a27debffc4ff045d","type":"ui-button","z":"61cea54cc7d7714d","group":"7c527cd7dda522e3","name":"","label":"Apagar ventilación","order":4,"width":"2","height":"1","emulateClick":false,"tooltip":"","color":"","bgcolor":"","className":"","icon":"","iconPosition":"left","payload":"0","payloadType":"num","topic":"vent_relay_DP","topicType":"str","buttonColor":"","textColor":"","iconColor":"","enableClick":true,"enablePointerdown":false,"pointerdownPayload":"","pointerdownPayloadType":"str","enablePointerup":false,"pointerupPayload":"","pointerupPayloadType":"str","x":470,"y":760,"wires":[["5a9caa191309a4a7"]]},{"id":"5a9caa191309a4a7","type":"debug","z":"61cea54cc7d7714d","name":"Ordenes a Ditto","active":true,"tosidebar":true,"console":false,"tostatus":false,"complete":"payload","targetType":"msg","statusVal":"","statusType":"auto","x":760,"y":560,"wires":[]},{"id":"30247a6031ed2d71","type":"function","z":"61cea54cc7d7714d","name":"Sólo en manual","func":"// Comprobamos si el gobernador está en Automático (1)\nlet esAutomatico = (msg.payload === 1);\n// Configuramos la propiedad especial de Dashboard 2.0\nmsg.enabled= ! esAutomatico // Si es true, el botón se bloquea en gris y no hace nada\nreturn msg;","outputs":1,"timeout":0,"noerr":0,"initialize":"","finalize":"","libs":[],"x":200,"y":720,"wires":[["8b7f1b6a8a2c6425","a27debffc4ff045d"]]},{"id":"ec5ef4ba78c59b5d","type":"function","z":"61cea54cc7d7714d","name":"Actualiza automático","func":"if (msg.payload && msg.payload.auto_mode_DP !== undefined) {\n  msg.payload = msg.payload.auto_mode_DP;\n  return msg;\n}","outputs":1,"timeout":0,"noerr":0,"initialize":"","finalize":"","libs":[],"x":200,"y":620,"wires":[["6bb04be3c4a615d3","30247a6031ed2d71"]]},{"id":"dd7b4a236519353c","type":"debug","z":"61cea54cc7d7714d","name":"debug 67","active":true,"tosidebar":true,"console":false,"tostatus":false,"complete":"false","statusVal":"","statusType":"auto","x":1200,"y":360,"wires":[]},{"id":"8f28a739dcf74881","type":"mqtt-broker","name":"","broker":"mqtt.iot-uma.es","port":"1883","clientid":"","autoConnect":true,"usetls":false,"protocolVersion":"4","keepalive":"60","cleansession":true,"autoUnsubscribe":true,"birthTopic":"","birthQos":"0","birthRetain":"false","birthPayload":"","birthMsg":{},"closeTopic":"","closeQos":"0","closeRetain":"false","closePayload":"","closeMsg":{},"willTopic":"","willQos":"0","willRetain":"false","willPayload":"","willMsg":{},"userProps":"","sessionExpiry":""},{"id":"36a310f050d3e09e","type":"ui-group","name":"Datos dispositivo final ESP32","page":"5754964613c06fde","width":"4","height":"1","order":1,"showTitle":true,"className":"","visible":"true","disabled":"false","groupType":"default"},{"id":"7c527cd7dda522e3","type":"ui-group","name":"Configuración dispositivo","page":"5754964613c06fde","width":"4","height":"1","order":2,"showTitle":true,"className":"","visible":"true","disabled":"false","groupType":"default"},{"id":"5754964613c06fde","type":"ui-page","name":"App final Ditto","ui":"9aa19c35c0896e2a","path":"/finalDitto","icon":"home","layout":"grid","theme":"6f5d93716d274d38","breakpoints":[{"name":"Default","px":"0","cols":"3"},{"name":"Tablet","px":"576","cols":"6"},{"name":"Small Desktop","px":"768","cols":"9"},{"name":"Desktop","px":"1024","cols":"12"}],"order":1,"className":"","visible":"true","disabled":"false"},{"id":"9aa19c35c0896e2a","type":"ui-base","name":"My Dashboard","path":"/dashboard","appIcon":"","includeClientData":true,"acceptsClientConfig":["ui-notification","ui-control"],"showPathInSidebar":false,"headerContent":"page","navigationStyle":"default","titleBarStyle":"default","showReconnectNotification":true,"notificationDisplayTime":1,"showDisconnectNotification":true,"allowInstall":false},{"id":"6f5d93716d274d38","type":"ui-theme","name":"Default Theme","colors":{"surface":"#ffffff","primary":"#0094CE","bgPage":"#eeeeee","groupBg":"#ffffff","groupOutline":"#cccccc"},"sizes":{"density":"default","pagePadding":"12px","groupGap":"12px","groupBorderRadius":"4px","widgetGap":"12px"}}]
```
![image](https://hackmd.io/_uploads/SymiQrkjWg.png)
---
## 4.4. Verificación Inicial de Conectividad
Antes de proceder a la programación de las fases, es fundamental validar que el "puente" entre nuestro hardware simulado y nuestra interfaz de usuario funciona correctamente.

**Sigue estos pasos para la "prueba de humo":**

1. **En Wokwi:** Pulsa el botón **Play**. Observa la consola serie; tras unos segundos de negociación WiFi, deberías ver el mensaje `[context: taskMQTTService] MQTT Connected`.
2. **En Node-RED:** Asegúrate de haber pulsado **Deploy**.
3. **En el Dashboard:** Abre tu interfaz visual (`/dashboard`). 
   - El indicador circular debe cambiar de rojo (**OFFLINE**) a verde (**ONLINE**).
   - El texto debe mostrar el nombre de tu dispositivo (ej. `micro1:ESP32-final`).

> [!IMPORTANT]
> Si el indicador permanece en rojo, revisa que hayas configurado correctamente tu **usuario** y **contraseña** en el nodo de configuración MQTT de Node-RED (dentro del flujo que acabas de importar).
![image](https://hackmd.io/_uploads/rkjo4pkiZl.png)
Una vez confirmes el estado **ONLINE**, ¡estás listo para empezar a programar las funcionalidades reales!

![image](https://hackmd.io/_uploads/rJ94H6ysWe.png)


---

## 4.5. Sincronización Inicial del Dashboard (Node-RED)
Aunque ya veas el estado **ONLINE**, notarás que los valores de temperatura, humedad o el estado del relé no aparecen reflejados todavía. Esto se debe a que el Dashboard está esperando recibir los datos por MQTT, y la placa aún no envía nada.

Para mejorar la experiencia de usuario, vamos a añadir un pequeño flujo de "descarga inicial" que, nada más abrir el Dashboard, consulte a Ditto vía API REST y rellene todos los huecos automáticamente.

![image](https://hackmd.io/_uploads/HkjrSSkoWe.png)

1. **Importa estos nodos:**
```json
[{"id":"8fa5da13e176568f","type":"function","z":"37899cd0b78ff4c0","name":"pull-on-boot de todas las features Ditto","func":"const usuario = flow.get(\"usuario\");\nconst claveusuario = flow.get(\"claveusuario\");\nconst dispositivo = flow.get(\"dispositivo\");\nconst credenciales = usuario + \":\" + claveusuario;\nconst credencialesBase64 = Buffer.from(credenciales).toString('base64');\n\n// Apuntamos al endpoint raíz general que devuelve TODAS las features (GET)\nmsg.method = \"GET\";\nmsg.url = \"http://10.10.10.201:8080/api/2/things/\"+usuario+\":\"+dispositivo+\"/features/\";\n\nmsg.headers = {\n    \"Accept\": \"application/json\",\n    \"Authorization\": \"Basic \" + credencialesBase64\n};\n\nreturn msg;\n","outputs":1,"timeout":0,"noerr":0,"initialize":"","finalize":"","libs":[],"x":550,"y":40,"wires":[["6cf910729146d1ef"]]},{"id":"6cf910729146d1ef","type":"http request","z":"37899cd0b78ff4c0","name":"","method":"use","ret":"obj","paytoqs":"ignore","url":"","tls":"","persist":false,"proxy":"","insecureHTTPParser":false,"authType":"","senderr":false,"headers":[],"x":790,"y":40,"wires":[["e05188b05ee21964","67213998d480ddd2"]]},{"id":"e05188b05ee21964","type":"function","z":"37899cd0b78ff4c0","name":"respuesta como telemetría","func":"let p = msg.payload || {};\n// Cogemos \"iot/telemetry/micro1/ESP32-final\" y lo partimos en un Array:\n// partes[0] = \"iot\"\n// partes[1] = \"telemetry\"\n// partes[2] = \"micro1\"\n// partes[3] = \"ESP32-final\"\nlet ns = \"Desconocido\";\nlet thId = \"Dispositivo\";\nif (msg.topic) {\n    let partes = msg.topic.split('/');\n    if (partes.length >= 4) {\n        ns = partes[2];\n        thId = partes[3];\n    }\n}\n// 1. Verificamos que Ditto respondió un 200 OK\nif (msg.statusCode !== 200) {\n    node.error(\"Fallo Pull-On-Boot en Node-RED: \" + msg.statusCode);\n    return null; // Detiene el flujo\n}\n\nlet features = msg.payload;\n\n// 2. Pequeño helper para extraer el valor reportado evitando crasheos\nlet extrae = (featureName) => {\n    try {\n        return features[featureName].properties.value;\n    } catch (e) {\n        return null;\n    }\n};\n\n\n\n// 3. Reconstruimos y simulamos el payload MQTT plano que espera la pantalla\nlet telemetria = {\n    namespace: ns,\n    thingId: thId,\n    temperature: extrae(\"temperature\"),\n    humidity: extrae(\"humidity\"),\n    air_quality: extrae(\"air_quality\"),\n    vent_relay_DP: extrae(\"vent_relay_DP\"),\n    auto_mode_DP: extrae(\"auto_mode_DP\"),\n    threshold_vent_DP: extrae(\"threshold_vent_DP\"),\n    publish_delta_DP: extrae(\"publish_delta_DP\"),\n    online: extrae(\"online\")\n};\n\n// Sustituimos el inmenso JSON del HTTP por nuestro JSON aplanado\nmsg.payload = telemetria;\nreturn msg;\n\nreturn msg;","outputs":1,"timeout":0,"noerr":0,"initialize":"","finalize":"","libs":[],"x":1020,"y":40,"wires":[["26af635b0579b621","dec9f2fe17eedb10"]]},{"id":"dec9f2fe17eedb10","type":"debug","z":"37899cd0b78ff4c0","name":"debug 57","active":true,"tosidebar":true,"console":false,"tostatus":false,"complete":"false","statusVal":"","statusType":"auto","x":1300,"y":40,"wires":[]}]
```

2. **Cableado:**
   - Busca el nodo **Inject** llamado `inyectar¹` (el que tiene el símbolo de ejecución al inicio). Conéctalo a la entrada del nuevo nodo `pull-on-boot de todas las features Ditto`.
   - Conecta la salida del nodo `respuesta como telemetría` directamente a la entrada del nodo llamado `function telemetría` que ya tenías en el flujo anterior.

Con este cambio, cada vez que hagas un *Deploy* o recargues el flujo, el Dashboard se auto-completará con los valores guardados en el Gemelo Digital, ofreciendo una sensación de respuesta instantánea.
![image](https://hackmd.io/_uploads/S1yqSHks-g.png)

---

## 5. Fase 1: Añadiendo la Tarea Publicadora (Event-Driven)

### 5.1 Contexto y Objetivos
La *Plantilla Inicial* que estás utilizando tiene una limitación importante a nivel IoT: está aislada. Es capaz de leer el sensor de temperatura y activar las luces NeoPixel, pero toda esa información se queda dentro del propio ESP32, sin llegar jamás a la nube.

Para enviar la información a través de la conexión WiFi (que ya se está estableciendo en la primera tarea `taskMQTTService`) necesitamos publicar mensajes en el *broker* MQTT. Sin embargo, no sería eficiente poner a la tarea `taskReader` a enviar indiscriminadamente mensajes a Internet cada vez que lee un dato de un periférico, porque la latencia de la red colapsaría y bloquearía la rápida ejecución de los botones o de las propias luces locales.

**El Objetivo:** Crear una tercera tarea independiente (`taskPublisher`) que esté "bloqueada" (consumiendo un 0% de CPU). La tarea lectora la despertará (usando un *Semáforo FreeRTOS*) únicamente cuando hayan transcurrido 30 segundos de silencio de red y sea obligatorio publicar la telemetría, ahorrando ancho de banda.

### 5.2 El Código (Solución a implementar)
Copia esta función íntegra. Es la nueva Tarea que empaquetará las variables globales de los sensores en formato JSON y las publicará a Ditto por MQTT. Hemos previsto un esquema de publicación selectiva en la que sólo se envían los datos que han cambiado y que están activos en el mapa de bits `camposPublicacion`:

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
    if (currentBits & PUB_RELAY)     telemetria["vent_relay_DP"]     = vent_relay_DP;
    if (currentBits & PUB_AUTO_MODE) telemetria["auto_mode_DP"]      = auto_mode_DP;
    if (currentBits & PUB_THRESHOLD) telemetria["threshold_vent_DP"] = threshold_vent_DP;
    if (currentBits & PUB_PUB_DELTA) telemetria["publish_delta_DP"]  = publish_delta_DP;
    
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
3. **Instanciar la Tarea en el Sistema Operativo:** Ve a la parte final del archivo, concretamente a la función core de configuración `setup()`. Localiza el lugar donde se inicializa la `taskReader` (`xTaskCreate(...)`) e inyecta debajo la orden para que FreeRTOS ponga en marcha tu nueva tercera Tarea bajo una prioridad mínima (1):
   ```cpp
   // 3. Tarea de Publicación (Prioridad Baja: 1) Guiada por eventos
   xTaskCreate(taskPublisher, "Publisher", 4096, NULL, 1, NULL);
   ```

### 5.4 Comprobación Visual
1. Dale al botón **Play** del simulador Wokwi.
2. Abre la pestaña inferior de la **Consola Serie**.
3. Verás que cada 2 segundos se escanean localmente la temperatura, la humedad y las ppm del potenciómetro. 
4. **Alcanzados los primeros 30 segundos de reloj**,  observarás de pronto algo distinto: el hilo de lectura imprime `Periodo (30s) cumplido...`, lo cual desencadena inmediatamente la ejecución de nuestra nueva y durmiente tarea, lanzando el log `>>> [MQTT UPLINK] : {"temperature":24,"humidity":40,"air_quality":400}`. Podrás comprobar en el panel de control de Node-RED que el dispositivo actualiza la telemetría cada 30 segundos. Mueve los valores del sensor DHT22 (el blanco) en Wokwi y mueve el potenciometro lineal para comprobar que se actualizan los datos.
5. ¡Felicidades! Acabas de programar una placa ESP32 con sistema operativo FreeRTOS, multitarea de clase industrial operando por eventos y semáforos.

---

## 6. Fase 2: Control Remoto y Configuración (Downlink)

### 6.1 Contexto y Objetivos
Hasta ahora, la comunicación ha sido unidireccional: del ESP32 a la nube. Sin embargo, el verdadero potencial de un **Gemelo Digital** reside en la capacidad de actuar sobre el dispositivo físico a través de su representación virtual.

En esta fase, aprenderemos a manejar el **Downlink** (mensajes que bajan de la nube al dispositivo). Esto nos permitirá **Sincronizar Estados (Desired Properties)**: Cambiar parámetros de funcionamiento (como el umbral de ventilación o el modo de operación) desde el Panel de Control.

**El Objetivo:** Actualizar la función `callback` (el receptor de mensajes MQTT en el ESP32) para que sepa parsear el JSON de tipo *desired* entrante de Eclipse Ditto y actualizar nuestras variables globales de control.

### 6.2 El Código (Solución a implementar)
Reemplaza la función `callback` vacía que tienes actualmente por esta versión. Fíjate cómo utiliza la librería `ArduinoJson` para extraer los valores de las propiedades deseadas y actualiza las variables globales de control, además anota los cambios en el mapa de bits de publicación para llamar al publicador cuando sea necesario con las actualizaciones necesarias. Se controlan los cambios en el modo de operación, el umbral de ventilación, el delta de publicación y el estado del relé. Es importante destacar que el relé solo se puede controlar en modo manual.

```cpp
void procesa_mensaje(char* topic, byte* payload, unsigned int length) { 
  String mensaje = "";
  for(int i = 0; i < length; i++) {
    mensaje += (char)payload[i];
  }
  
  Serial.println(DEBUG_STRING + "======= MENSAJE RECIBIDO =======");
  Serial.println(DEBUG_STRING + "Topic: [" + String(topic) + "]");
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

    // ¿El usuario ha cambiado el delta de publicación (ppm)?
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
```

### 6.3 Instrucciones de Inserción
1. Localiza tu función `procesa_mensaje()` actual en Wokwi.
2. Selecciónala entera y sustituye por el código anterior encima.
3. **Punto clave:** Fíjate que al final de los cambios hacemos un `xSemaphoreGive(semPublish)`. Esto es vital: cuando cambias algo desde la nube, el ESP32 obedece y automáticamente **le devuelve un mensaje de confirmación** a Ditto para que el Gemelo Digital sepa que la orden se ha ejecutado físicamente traslando el cambio de las propiedades deseadas (*desiredPropierties*) a las reportadas (*propierties*).
4. Para probar su funcionamiento habrá que añadir el cambio de propiedades deseadas en el Gemelo desde nuestra aplicación NodeRED.

---

## 6.4. Control desde Node-RED (Capa de Aplicación)
Para que el Dashboard no sea solo un visor, sino un mando a distancia, debemos añadir una lógica que capture tus interacciones (mover un slider, pulsar un botón) y las envíe a la API REST de Eclipse Ditto. Los 5 nodos que producen cambios en el gemelo digital envían en payload el valor que se desea establecer en la propiedad deseada y en topic el nombre de la propiedad deseada. Por ejemplo, si movemos el slider de umbral de ventilación, se enviará un mensaje en payload con el valor del umbral y en topic "threshold_vent_DP". 
![image](https://hackmd.io/_uploads/SkeBLrkiWx.png)

### Instrucciones de Integración:
1. **Importa los nuevos nodos:** Copia el JSON inferior e impórtalo en tu flujo actual de Node-RED.
2. **Conexión de Cables:** Localiza en tu flujo los nodos de entrada (Sliders de umbral/delta y Botones de relé/modo) que actualmente van conectados al nodo de debug llamado `"Ordenes Ditto"`. **Conéctalos a la entrada de la nueva función `"Envío desired property..."`**.
3. **Despliegue:** Pulsa **Deploy**. Ahora, cuando muevas el slider en el Dashboard, se generará una petición HTTP hacia Ditto, que a su vez enviará un mensaje MQTT de tipo *desired* a tu ESP32.
4. Comprueba que el gemelo digital se actualiza con los nuevos valores que envíes desde el dashboard y que el dispositivo responde a los cambios.

```json
[{"id":"6ab9d286db100cd2","type":"debug","z":"37899cd0b78ff4c0","name":"debug 52","active":true,"tosidebar":true,"console":false,"tostatus":false,"complete":"true","targetType":"full","statusVal":"","statusType":"auto","x":1200,"y":660,"wires":[]},{"id":"3e36033e693905ad","type":"function","z":"37899cd0b78ff4c0","name":"Envío desired property desde  APP a Ditto para el dispositivo","func":"const usuario = flow.get(\"usuario\");\nconst claveusuario = flow.get(\"claveusuario\");\nconst dispositivo = flow.get(\"dispositivo\");\nconst credenciales = usuario+\":\"+claveusuario;\nconst credencialesBase64 = Buffer.from(credenciales).toString('base64');\n\n\n// 1. Especificamos que vamos a hacer un PUT (para forzar la actualización o creación)\nmsg.method = \"PUT\";\n\n\n// 2. Definimos la URL completa (incluyendo el parámetro timeout)\nmsg.url = \"http://10.10.10.201:8080/api/2/things/\" + \n           usuario + \":\" + dispositivo + \"/features/\"+msg.topic+\"/desiredProperties\";\n\n// 3. Indicamos que vamos a mandar un JSON\nmsg.headers = {\n    \"Content-Type\": \"application/json\",\n    \"Authorization\": \"Basic \"+ credencialesBase64\n};\n// 4. El \"Estado Deseado\" de la válvula (Payload limpio, sin comandos extraños)\nmsg.payload = {\n    \"value\": msg.payload\n};\n\nreturn msg;\n\nreturn msg;","outputs":1,"timeout":0,"noerr":0,"initialize":"","finalize":"","libs":[],"x":900,"y":600,"wires":[["f9948ae7d03da872","6ab9d286db100cd2"]]},{"id":"f9948ae7d03da872","type":"http request","z":"37899cd0b78ff4c0","name":"","method":"use","ret":"obj","paytoqs":"ignore","url":"","tls":"","persist":false,"proxy":"","insecureHTTPParser":false,"authType":"","senderr":false,"headers":[],"x":1230,"y":600,"wires":[["b454cc44078df9b5"]]},{"id":"b454cc44078df9b5","type":"debug","z":"37899cd0b78ff4c0","name":"debug 53","active":true,"tosidebar":true,"console":false,"tostatus":false,"complete":"payload","targetType":"msg","statusVal":"","statusType":"auto","x":1400,"y":600,"wires":[]}]
```

> [!NOTE]
> La función utiliza `flow.get("usuario")` y `flow.get("claveusuario")`, por lo que es vital que los nodos de **Configuración** del principio del flujo tengan tus datos reales para que la API de Ditto te autorice el cambio.

---

## 7. Fase 3: Interacción Física (El Pulsador)

### 7.1 Contexto y Objetivos
Un dispositivo IoT no debe depender exclusivamente de la nube para ser funcional. El usuario que está frente a la máquina física también puede optar por actuar sobre ella de forma inmediata. Sin embargo, en un sistema de **Gemelo Digital**, cualquier cambio físico (pulsar un botón) debe verse reflejado instantáneamente en el panel de control remoto.

**El Objetivo:** Implementar un sistema de control híbrido. El botón físico permitirá:
1. **Pulsación Corta:** Encender/Apagar el ventilador (solo si el modo es Manual).
2. **Pulsación Larga (2 segundos):** Alternar entre Modo Automático (el sensor manda) y Modo Manual (la persona manda).

### 7.2 El Código (Solución a implementar)
Copia este bloque que contiene los "manejadores" (callbacks) del botón y su tarea dedicada de escaneo. Usaremos la librería Button2 para facilitar la implementación. Esta librería se encarga de detectar los diferentes tipos de pulsación (corta, larga, doble, etc.) y nos permite definir qué acción realizar en cada caso. En la tarea que vigila el botón se inicializa la librería y se definen los callbacks que se ejecutarán en cada caso y se inicia el bucle de escaneo que se repite indefinidamente.

```cpp
// --- Callbacks de la librería Button2 ---
void clickCorto(Button2& btn) {
  // Sólo conmutamos a mano si estamos en el modo Manual
  if (auto_mode_DP == 0) {
    vent_relay_DP = (vent_relay_DP == 1) ? 0 : 1; 
    digitalWrite(RELAYPIN, vent_relay_DP ? HIGH : LOW);
    digitalWrite(LEDPIN, vent_relay_DP ? HIGH : LOW);
    
    Serial.println(DEBUG_STRING + ">>> [BOTÓN] Relé conmutado a: " + String(vent_relay_DP));
    camposPublicacion |= PUB_RELAY; // Marcamos para informar a la nube
    xSemaphoreGive(semPublish);     // Despertamos al publicador para sincronizar Ditto
  } else {
    Serial.println(DEBUG_STRING + ">>> [BOTÓN] Acción ignorada: El Modo Automático está activo.");
  }
}

void clickLargo(Button2& btn) {
  auto_mode_DP = (auto_mode_DP == 1) ? 0 : 1; // Alternamos el modo global
  
  if (auto_mode_DP == 1) {
    Serial.println(DEBUG_STRING + ">>> [BOTÓN] MODO AUTOMÁTICO ACTIVADO");
  } else {
    Serial.println(DEBUG_STRING + ">>> [BOTÓN] MODO MANUAL ACTIVADO");
  }
  
  camposPublicacion |= PUB_AUTO_MODE; // Marcamos el cambio de modo
  xSemaphoreGive(semPublish);         // Sincronizamos con el Gemelo Digital
}

// --- TAREA 4: Gestión del Pulsador ---
void taskBotones(void *pvParameters) {
  info_tarea_actual();
  
  boton.begin(BOTONPIN);
  boton.setLongClickTime(2000); // 2 segundos para el modo manual/auto
  boton.setTapHandler(clickCorto);
  boton.setLongClickDetectedHandler(clickLargo);
  
  Serial.println(DEBUG_STRING + "Lógica de botones lista.");

  while(true) {
    boton.loop(); // Escanea el estado del pin físico
    vTaskDelay(pdMS_TO_TICKS(15)); // Respira 15ms (60Hz de refresco)
  }
}
```

### 7.3 Instrucciones de Inserción
1. **Pega el código:** Inserta el bloque anterior justo antes de la función `setup()`.
2. **Lanza la Tarea:** En la función `setup()`, busca donde creaste la tarea del publicador e inyecta debajo esta cuarta tarea:
   ```cpp
   // 4. Tarea de Interfaz de Usuario (Prioridad Media: 2)
   xTaskCreate(taskBotones, "UI_Buttons", 3072, NULL, 2, NULL);
   ```

### 7.4 Comprobación Visual
1. Pulsa el botón del simulador Wokwi **brevemente**. Si estás en Modo Manual, verás que el relé y el LED se encienden, y en Node-RED se actualiza el estado del relé también.
2. Mantén pulsado el botón **2 segundos**. Verás en la consola el mensaje de cambio de modo. El led parpaderá para indicar que se ha producido el cambio de modo. Si has cambiado al modo automático y ahora pulsas brevemente, el sistema ignorará la orden manual porque "el sistema" tiene el control.
3. ¡Enhorabuena! Has cerrado el círculo: control desde la nube (Fase 2) y control desde el hardware (Fase 3), ambos sincronizados en tiempo real.
---

## 8. Fase 4: El Motor de Reglas (Inteligencia Local)

### 8.1 Contexto y Objetivos
Hasta ahora, el ESP32 es un esclavo: lee sensores y obedece botones. Pero un dispositivo IoT avanzado debe ser capaz de **tomar decisiones por sí mismo** basándose en la configuración que le hayamos dado desde la nube.

**El Objetivo:** Implementar la lógica de "Smart Device":
1. **Control Automático:** Si el aire supera el `threshold_vent_DP`, el ventilador debe encenderse solo (y avisar a la nube).
2. **Publicación por Delta:** Si la calidad del aire cambia bruscamente (por ejemplo, alguien echa humo cerca del sensor), el ESP32 no debe esperar a los 30 segundos, sino que debe **publicar el dato de inmediato** para que el Gemelo Digital reaccione en tiempo real.

### 8.2 El Código (Solución a implementar)
Debemos actualizar la tarea `taskReader`. Localiza tu función actual y **reemplaza todo su bucle `while(true)`** por esta versión vitaminada:

```cpp
  while(true) {
    // 1. Leer el entorno (Potenciómetro y DHT22)
    int rawADC = analogRead(POTPIN);
    int current_ppm = map(rawADC, 0, 4095, 400, 5000);
    TempAndHumidity newValues = dht.getTempAndHumidity();
    
    // 2. Actualizar variables globales
    global_ppm = current_ppm;
    mostrarCalidadAire(global_ppm);

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
      
      if (time_elapsed) {
        camposPublicacion |= (PUB_TEMP | PUB_HUM | PUB_AIR); // Refresco general
      }
      if (delta_exceeded) {
        camposPublicacion |= PUB_AIR; // Solo notificamos el salto crítico de CO2
      }
      if (state_changed) {
        camposPublicacion |= PUB_RELAY; // Notificamos el cambio del relé
      }

      last_publish_time = millis();
      last_published_ppm = current_ppm;
      
      Serial.println(DEBUG_STRING + "Evento detectado. Despertando publicador...");
      xSemaphoreGive(semPublish);
    }

    vTaskDelay(pdMS_TO_TICKS(2000));
  }
```
### 8.3 Comprobación Visual
1. **Prueba del Delta:** Si mueves el potenciómetro de Wokwi rápidamente. Verás que el ESP32 publica mensajes MQTT muy seguidos (en menos de 2 segundos) porque detecta el "salto" de PPM superior al delta configurado. No esperará los 30 segundos de periodo de publicación establecido.
2. **Prueba del Umbral:** Pon el ESP32 en **Modo Automático** (vía Dashboard o pulsación larga en el botón físico). Sube el potenciómetro por encima del umbral (1000 ppm por defecto). El relé saltará solo.
3. ¡Felicidades! Tienes un sistema de control de lazo cerrado totalmente funcional y sincronizado con la nube.
---

## 9. Fase 5: Sincronización de Arranque (Pull-on-Boot)

### 9.1 Contexto y Objetivos
¿Qué pasa si se va la luz y el ESP32 se reinicia? Por defecto, las variables volverían a sus valores iniciales (Modo Manual, Relé OFF, Umbral 1000). Sin embargo, es posible que en la nube hubiéramos configurado un umbral distinto o que el ventilador estuviera encendido. Además no recibiríamos nos perderíamos las ordenes de cambio de estado de los desired properties mientras estemos desconectados. Al reconectar solucionamos todas las discrepancias entre el estado real del dispositivo y el estado deseado por el gemelo digital.

**El Objetivo:** Al encenderse, antes de empezar a medir, el ESP32 debe hacer una consulta "de cortesía" a la API REST de Eclipse Ditto para descargar su última configuración conocida.

### 9.2 El Código (Solución a implementar)
Localiza la función `pull_on_boot()` que tenías vacía y **reemplázala por esta versión completa**. Fíjate que utiliza la librería `HTTPClient` para realizar una petición segura (HTTPS) al servidor:

```cpp
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
```

### 9.3 Comprobación Visual
1. Para el simulador de Wokwi
2. En el dashboard de Node-RED, cambia el umbral, el modo automático u otro parámetro.
3. Reinicia la ejecución del simulador de Wokwi.
4. Observa la consola serie: verás cómo tras conectar al WiFi, el dispositivo descarga los datos desde el Gemelo Digital y verás el mensaje `Sincronización completada`. En el Dashboard de Node-RED verás que el estado reportado por el Gemelo Digital ha cambiado al valor que has introducido en el paso 2.

---

---

## 10. Fase 6: Comandos RPC (Mensaje Refresh)

### 10.1 Contexto y Objetivos
Hasta ahora hemos visto cómo sincronizar el estado del dispositivo mediante propiedades (`Properties` y `Desired Properties`). Sin embargo, a veces necesitamos enviar **órdenes directas** (comandos) que no son un estado, sino una acción puntual. En el protocolo de Eclipse Ditto, esto se gestiona mediante **Messages**.

**El Objetivo:** Implementar el comando `/refresh`. Cuando el administrador pulse un botón especial en el Dashboard, el ESP32 recibirá un mensaje RPC. Su obligación será:
1. Interrumpir su espera y realizar una **publicación total inmediata** de todos sus sensores (`PUB_ALL`).
2. Devolver un mensaje de **confirmación (Response)** a la nube para que Ditto sepa que el ESP32 ha recibido y ejecutado la orden.

### 10.2 El Código (Solución a implementar)
Debemos actualizar de nuevo la función *callback* `procesa_mensaje`. Ahora deberá distinguir si el mensaje llega por el topic de propiedades o por el de comandos. Reemplaza tu función `procesa_mensaje` por esta versión final:

```cpp
void procesa_mensaje(char* topic, byte* payload, unsigned int length) { 
  String mensaje = "";
  for(int i = 0; i < length; i++) {
    mensaje += (char)payload[i];
  }
  
  Serial.println(DEBUG_STRING + "======= MENSAJE RECIBIDO =======");
  Serial.println(DEBUG_STRING + "Topic: [" + String(topic) + "]");
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

    if (doc.containsKey("auto_mode_DP")) {
      auto_mode_DP = doc["auto_mode_DP"]["value"].as<int>();
      camposPublicacion |= PUB_AUTO_MODE;
      hayCambios = true;
    }
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
    if (doc.containsKey("vent_relay_DP")) {
      int nuevo_vr = doc["vent_relay_DP"]["value"].as<int>();
      if (auto_mode_DP == 0) { 
        vent_relay_DP = nuevo_vr;
        digitalWrite(RELAYPIN, vent_relay_DP ? HIGH : LOW);
        digitalWrite(LEDPIN, vent_relay_DP ? HIGH : LOW);
        camposPublicacion |= PUB_RELAY;
        hayCambios = true;
      }
    }
    if (hayCambios) xSemaphoreGive(semPublish);
  }
}
```

### 10.3 Interfaz en Node-RED (Envío de Comandos)
Para poder enviar este comando desde nuestro Dashboard, necesitamos añadir una nueva rama en el flujo de Node-RED. Esta rama utilizará un botón para disparar una petición **POST** a la API de Ditto (concretamente al endpoint `/inbox/messages/`).
![image](https://hackmd.io/_uploads/Sk-Vwrki-e.png)

Copia e importa este JSON en tu Node-RED:

```json
[{"id":"e2961185c7da18dc","type":"http request","z":"37899cd0b78ff4c0","name":"POST mensaje a Ditto","method":"use","ret":"obj","paytoqs":"ignore","url":"","tls":"","persist":false,"proxy":"","insecureHTTPParser":false,"authType":"","senderr":false,"headers":[],"x":880,"y":1060,"wires":[["82395dd089958b2b","ec8fc7d3c5fe5283","611ba1f44debbecd"]]},{"id":"82395dd089958b2b","type":"debug","z":"37899cd0b78ff4c0","name":"debug 54","active":true,"tosidebar":true,"console":false,"tostatus":false,"complete":"true","targetType":"full","statusVal":"","statusType":"auto","x":1120,"y":1100,"wires":[]},{"id":"570f9e1543cf1b7a","type":"function","z":"37899cd0b78ff4c0","name":"Envío mensaje desde  APP a Ditto para el dispositivo","func":"const usuario = flow.get(\"usuario\");\nconst claveusuario = flow.get(\"claveusuario\");\nconst dispositivo = flow.get(\"dispositivo\");\nconst mensaje = msg.payload;\nconst credenciales = usuario+\":\"+claveusuario;\nconst credencialesBase64 = Buffer.from(credenciales).toString('base64');\nconst retraso = flow.get(\"retraso\");\n\n// 1. Definimos el método HTTP\nmsg.method = \"POST\";\n\n// 2. Definimos la URL completa (incluyendo el parámetro timeout)\nmsg.url = \"http://10.10.10.201:8080/api/2/things/\" + \n           usuario + \":\" + dispositivo + \"/inbox/messages/\"+ mensaje + \"?timeout=5\";\n// 3. Configuramos las cabeceras\nmsg.headers = {\n    \"Content-Type\": \"application/json\",\n    \"Authorization\": \"Basic \"+ credencialesBase64\n};\n\n// 4. Preparamos el cuerpo del mensaje (payload)\nif(mensaje === \"refresh\")\n{\n    msg.payload = {\"mensaje\": mensaje };\n}\nelse \n    msg.payload = {\"mensaje\":\"Desconocido\"};\n\nreturn msg;\n","outputs":1,"timeout":0,"noerr":0,"initialize":"","finalize":"","libs":[],"x":520,"y":1060,"wires":[["e2961185c7da18dc","5fc97caa9e60fc5f","34f3265dd3383a32"]]},{"id":"5fc97caa9e60fc5f","type":"debug","z":"37899cd0b78ff4c0","name":"debug 55","active":true,"tosidebar":true,"console":false,"tostatus":false,"complete":"true","targetType":"full","statusVal":"","statusType":"auto","x":840,"y":1100,"wires":[]},{"id":"9e603676033f393e","type":"ui-button","z":"37899cd0b78ff4c0","group":"7c527cd7dda522e3","name":"","label":"Envía mensaje refresco inmediato","order":7,"width":0,"height":0,"emulateClick":false,"tooltip":"","color":"","bgcolor":"","className":"","icon":"","iconPosition":"left","payload":"","payloadType":"str","topic":"topic","topicType":"msg","buttonColor":"","textColor":"","iconColor":"","enableClick":true,"enablePointerdown":false,"pointerdownPayload":"","pointerdownPayloadType":"str","enablePointerup":false,"pointerupPayload":"","pointerupPayloadType":"str","x":200,"y":940,"wires":[["51a750d74641decc","e1ea95a4ac8af46a","a56f156533da359e","7a7d97d150cf6902"]]},{"id":"51a750d74641decc","type":"function","z":"37899cd0b78ff4c0","name":"fecha/hora","func":"let semana = [\"Domingo\", \"Lunes\", \"Martes\", \"Miércoles\", \"Jueves\", \"Viernes\", \"Sábado\"];\nlet ahora = new Date();\nmsg.payload = semana[ahora.getDay()] + \", \"\n    + ahora.toLocaleString('es-ES', { timeZone: 'Europe/Madrid', hour12: false });\nreturn msg;\n","outputs":1,"timeout":0,"noerr":0,"initialize":"","finalize":"","libs":[],"x":630,"y":940,"wires":[["51656367ebfd11d4"]]},{"id":"51656367ebfd11d4","type":"ui-text","z":"37899cd0b78ff4c0","group":"7c527cd7dda522e3","order":8,"width":0,"height":0,"name":"","label":"Último envío mensaje","format":"{{msg.payload}}","layout":"row-spread","style":false,"font":"","fontSize":16,"color":"#717171","wrapText":false,"className":"","value":"payload","valueType":"msg","x":880,"y":940,"wires":[]},{"id":"b03a400b632d2df5","type":"ui-text","z":"37899cd0b78ff4c0","group":"7c527cd7dda522e3","order":9,"width":0,"height":0,"name":"","label":"Última respuesta mensaje","format":"{{msg.payload}}","layout":"row-spread","style":false,"font":"","fontSize":16,"color":"#717171","wrapText":false,"className":"","value":"payload","valueType":"msg","x":1390,"y":880,"wires":[]},{"id":"ec8fc7d3c5fe5283","type":"function","z":"37899cd0b78ff4c0","name":"fecha/hora","func":"let semana = [\"Domingo\", \"Lunes\", \"Martes\", \"Miércoles\", \"Jueves\", \"Viernes\", \"Sábado\"];\nlet ahora = new Date();\nmsg.payload = semana[ahora.getDay()] + \", \"\n    + ahora.toLocaleString('es-ES', { timeZone: 'Europe/Madrid', hour12: false });\nreturn msg;\n","outputs":1,"timeout":0,"noerr":0,"initialize":"","finalize":"","libs":[],"x":1130,"y":940,"wires":[["b03a400b632d2df5"]]},{"id":"a56f156533da359e","type":"change","z":"37899cd0b78ff4c0","name":"","rules":[{"t":"set","p":"payload","pt":"msg","to":"Esperando respuesta...","tot":"str"}],"action":"","property":"","from":"","to":"","reg":false,"x":810,"y":880,"wires":[["b03a400b632d2df5"]]},{"id":"611ba1f44debbecd","type":"function","z":"37899cd0b78ff4c0","name":"Procesa respuesta","func":"let status = msg.payload.status;\nlet error = msg.payload.error;\n\nmsg.ui = {\n    loading: false,\n    ok: status >= 200 && status < 300,\n    status: status,\n    message: msg.payload.message,\n    description: msg.payload.description,\n    error: error\n};\n\nreturn msg;","outputs":1,"timeout":0,"noerr":0,"initialize":"","finalize":"","libs":[],"x":1150,"y":1060,"wires":[["9fe4337216e0a6e4"]]},{"id":"9fe4337216e0a6e4","type":"ui-template","z":"37899cd0b78ff4c0","group":"7c527cd7dda522e3","page":"","ui":"","name":"Respuesta mensaje","order":10,"width":0,"height":0,"head":"","format":"<div style=\"padding:16px; border-radius:10px;\"\n     :style=\"{\n        backgroundColor: msg.ui.loading ? '#eeeeee' : (msg.ui.ok ? '#e8f5e9' : '#ffebee'),\n        border: '1px solid ' + (msg.ui.loading ? '#9e9e9e' : (msg.ui.ok ? '#4caf50' : '#f44336'))\n     }\">\n\n    <!-- ESPERANDO -->\n    <div v-if=\"msg.ui.loading\">\n        <h3 style=\"margin:0; color:#616161;\">\n            ⏳ Esperando respuesta...\n        </h3>\n        <p>{{ msg.ui.description }}</p>\n    </div>\n\n    <!-- OK -->\n    <div v-else-if=\"msg.ui.ok\">\n        <h3 style=\"margin:0; color:#2e7d32;\">\n            ✔ Operación correcta\n        </h3>\n\n        <p><b>Status:</b> {{ msg.ui.status }}</p>\n\n        <p v-if=\"msg.ui.message\">\n            <b>Mensaje:</b> {{ msg.ui.message }}\n        </p>\n\n        <p v-if=\"msg.ui.description\">\n            <b>Descripción:</b> {{ msg.ui.description }}\n        </p>\n    </div>\n\n    <!-- ERROR -->\n    <div v-else>\n        <h3 style=\"margin:0; color:#c62828;\">\n            ✖ Error\n        </h3>\n\n        <p><b>Status:</b> {{ msg.ui.status }}</p>\n\n        <p v-if=\"msg.ui.error\">\n            <b>Error:</b> {{ msg.ui.error }}\n        </p>\n         <p v-if=\"msg.ui.message\">\n            <b>Mensaje:</b> {{ msg.ui.message }}\n        </p>\n        <p v-if=\"msg.ui.description\">\n            <b>Descripción:</b> {{ msg.ui.description }}\n        </p>\n    </div>\n\n</div>","storeOutMessages":true,"passthru":true,"resendOnRefresh":true,"templateScope":"local","className":"","x":1390,"y":1060,"wires":[[]]},{"id":"7a7d97d150cf6902","type":"function","z":"37899cd0b78ff4c0","name":"En espera","func":"msg.ui = {\n    loading: true,\n    ok: null,\n    status: null,\n    message: null,\n    description: \"Esperando respuesta del servidor...\",\n    error: null\n};\n\nreturn msg;","outputs":1,"timeout":0,"noerr":0,"initialize":"","finalize":"","libs":[],"x":1120,"y":1000,"wires":[["9fe4337216e0a6e4"]]},{"id":"e1ea95a4ac8af46a","type":"delay","z":"37899cd0b78ff4c0","name":"Retraso 1s para ver progreso","pauseType":"delay","timeout":"1","timeoutUnits":"seconds","rate":"1","nbRateUnits":"1","rateUnits":"second","randomFirst":"1","randomLast":"5","randomUnits":"seconds","drop":false,"allowrate":false,"outputs":1,"x":200,"y":1000,"wires":[["01b5fb7cfc18db7c"]]},{"id":"9591e649c8db4246","type":"comment","z":"37899cd0b78ff4c0","name":"Envía mensajes","info":"","x":140,"y":880,"wires":[]},{"id":"34f3265dd3383a32","type":"function","z":"37899cd0b78ff4c0","name":"muestra canales de comunicación","func":"const usuario = flow.get(\"usuario\");\nconst dispositivo = flow.get(\"dispositivo\");\nconst mensaje = msg.payload;\n\n// Construcciones útiles\nconst thingId = usuario + \":\" + dispositivo;\n\nconst url = msg.url;\n\n// Topics MQTT\nconst topicEnvio = usuario + \"/\" + dispositivo + \"/mensajes\";\nconst topicRespuesta = usuario + \"/\" + dispositivo + \"/respuestas\";\n\n// Objeto para UI\nmsg.ui = {\n    thingId: thingId,\n    url: url,\n    mensaje: mensaje,\n    mqtt: {\n        envio: topicEnvio,\n        respuesta: topicRespuesta\n    }\n};\n\nreturn msg;","outputs":1,"timeout":0,"noerr":0,"initialize":"","finalize":"","libs":[],"x":940,"y":1160,"wires":[["239cb13637b32c7c"]]},{"id":"239cb13637b32c7c","type":"ui-template","z":"37899cd0b78ff4c0","group":"7c527cd7dda522e3","page":"","ui":"","name":"Canales de comunicación","order":11,"width":0,"height":0,"head":"","format":"<div style=\"padding:16px; border-radius:10px; border:1px solid #ccc;\">\n\n    <h3 style=\"margin:0; color:#1976d2;\">\n        🔗 Comunicación con dispositivo\n    </h3>\n\n    <hr>\n\n    <!-- Thing ID -->\n    <p>\n        <b>Thing ID:</b><br>\n        <span style=\"font-family:monospace;\">\n            {{ msg.ui.thingId }}\n        </span>\n    </p>\n\n    <!-- Mensaje -->\n    <p>\n        <b>Payload del mensaje enviado:</b><br>\n        <span style=\"font-family:monospace;\">\n            {{ msg.ui.mensaje }}\n        </span>\n    </p>\n\n    <!-- URL HTTP -->\n    <p>\n        <b>URL (Ditto HTTP API):</b><br>\n        <span style=\"font-family:monospace; word-break: break-all;\">\n            {{ msg.ui.url }}\n        </span>\n    </p>\n\n    <hr>\n\n    <!-- MQTT -->\n    <h4 style=\"margin-bottom:5px;\">📡 Topics MQTT configuradas en las conexiónes Ditto para estos mensajes</h4>\n\n    <p>\n        <b>Envío:</b><br>\n        <span style=\"font-family:monospace;\">\n            {{ msg.ui.mqtt.envio }}\n        </span>\n    </p>\n\n    <p>\n        <b>Respuesta:</b><br>\n        <span style=\"font-family:monospace;\">\n            {{ msg.ui.mqtt.respuesta }}\n        </span>\n    </p>\n\n</div>","storeOutMessages":true,"passthru":true,"resendOnRefresh":true,"templateScope":"local","className":"","x":1370,"y":1160,"wires":[[]]},{"id":"f43d7362d29bddd2","type":"inject","z":"37899cd0b78ff4c0","name":"Envío manual","props":[{"p":"payload"},{"p":"topic","vt":"str"}],"repeat":"","crontab":"","once":false,"onceDelay":0.1,"topic":"","payload":"","payloadType":"str","x":150,"y":1060,"wires":[["e1ea95a4ac8af46a"]]},{"id":"01b5fb7cfc18db7c","type":"function","z":"37899cd0b78ff4c0","name":"payload = \"refresh\"","func":"msg.payload=\"refresh\";\nreturn msg;","outputs":1,"timeout":0,"noerr":0,"initialize":"","finalize":"","libs":[],"x":490,"y":1000,"wires":[["570f9e1543cf1b7a"]]},{"id":"7c527cd7dda522e3","type":"ui-group","name":"Configuración dispositivo","page":"5754964613c06fde","width":"4","height":"1","order":2,"showTitle":true,"className":"","visible":"true","disabled":"false","groupType":"default"},{"id":"5754964613c06fde","type":"ui-page","name":"App final Ditto","ui":"9aa19c35c0896e2a","path":"/finalDitto","icon":"home","layout":"grid","theme":"6f5d93716d274d38","breakpoints":[{"name":"Default","px":"0","cols":"3"},{"name":"Tablet","px":"576","cols":"6"},{"name":"Small Desktop","px":"768","cols":"9"},{"name":"Desktop","px":"1024","cols":"12"}],"order":1,"className":"","visible":"true","disabled":"false"},{"id":"9aa19c35c0896e2a","type":"ui-base","name":"My Dashboard","path":"/dashboard","appIcon":"","includeClientData":true,"acceptsClientConfig":["ui-notification","ui-control"],"showPathInSidebar":false,"headerContent":"page","navigationStyle":"default","titleBarStyle":"default","showReconnectNotification":true,"notificationDisplayTime":1,"showDisconnectNotification":true,"allowInstall":false},{"id":"6f5d93716d274d38","type":"ui-theme","name":"Default Theme","colors":{"surface":"#ffffff","primary":"#0094CE","bgPage":"#eeeeee","groupBg":"#ffffff","groupOutline":"#cccccc"},"sizes":{"density":"default","pagePadding":"12px","groupGap":"12px","groupBorderRadius":"4px","widgetGap":"12px"}}]
```

### 10.4 Comprobación Visual
1. En tu Dashboard de Node-RED, localiza el botón **"Envía mensaje refresco inmediato"**.
2. Al pulsarle, observa la consola serie del ESP32. Verás el mensaje `¡Orden REFRESH!`.
3. Inmediatamente después, verás que el ESP32 lanza un `[MQTT UPLINK]` con todos los datos, sin esperar a que pasen los 30 segundos.
4. En el Dashboard verás aparecer un cuadro de estado verde indicando "Operación correcta" junto con la descripción del éxito devuelta por el dispositivo.

---

## 11. Fase Final: Integración de Grafana en el Dashboard NodeRED
Como broche de oro a nuestro sistema IoT, vamos a integrar una visualización de datos históricos utilizando **Grafana**. En lugar de saltar entre pestañas, incrustaremos el panel directamente en nuestra App de Node-RED mediante un **Iframe**.
![image](https://hackmd.io/_uploads/H1VTwS1jZg.png)


1. **Importa estos nodos:**
```json
[{"id":"f8d945fa33172ee6","type":"ui-template","z":"37899cd0b78ff4c0","group":"007924922983f999","page":"","ui":"","name":"Grafana","order":1,"width":"4","height":"14","head":"","format":"<div style=\"display:flex; flex-direction:column; height:100%;\">\n  \n  <div style=\"margin-bottom:10px;\">\n    <a :href=\"msg.payload\" target=\"_blank\">\n      Abrir dashboard de Grafana en otra pestaña\n    </a>\n  </div>\n\n  <iframe\n    :src=\"msg.payload\"\n    style=\"flex:1; width:100%; border:none;\">\n  </iframe>\n\n</div>","storeOutMessages":true,"passthru":true,"resendOnRefresh":true,"templateScope":"local","className":"","x":1280,"y":400,"wires":[[]]},{"id":"ead84aad491a32eb","type":"inject","z":"37899cd0b78ff4c0","name":"URL BASE GRAFANA","props":[{"p":"payload"},{"p":"topic","vt":"str"}],"repeat":"","crontab":"","once":true,"onceDelay":0.1,"topic":"","payload":"https://grafana.iot-uma.es/d/minxssn/panel-esp32-final-micro1","payloadType":"str","x":840,"y":400,"wires":[["16433ad97c43e582"]]},{"id":"16433ad97c43e582","type":"function","z":"37899cd0b78ff4c0","name":"parámetros del dashboard","func":"msg.payload +=\"?orgId=1&from=now-15m&to=now&timezone=browser&refresh=5s&kiosk\"\nreturn msg;","outputs":1,"timeout":0,"noerr":0,"initialize":"","finalize":"","libs":[],"x":1090,"y":400,"wires":[["f8d945fa33172ee6"]]},{"id":"007924922983f999","type":"ui-group","name":"Grafana","page":"5754964613c06fde","width":"4","height":"1","order":3,"showTitle":true,"className":"","visible":"true","disabled":"false","groupType":"default"}]
```

2. **Configuración:**
   - Abre el nodo **URL BASE GRAFANA** (el de color naranja tipo Inject).
   - En el campo `payload`, asegúrate de que la URL apunta a **tu propio dashboard** de Grafana. Si todavía no tienes diseñado tu propio Dashboard puedes usar éste con esa misma URL, para ver tus datos podrás introducir tu nombre de usuario en la entrada "UsuarioBucket" arriba a la izquierda del dashboard una vez desplegado el flujo.

Al desplegar (`Deploy`), verás que tu Dashboard de Node-RED incluye una pestaña o sección dedicada con las gráficas de evolución temporal de la calidad de aire y el límite establecido, de la temperatura y humedad o del estado de configuración del dispositivo y los actuadores, a parte de otra información interesante.

### 11.1. Diseño del Dashboard (Grafana JSON)
Para que no tengas que diseñar el panel desde cero, aquí tienes la definición completa del Dashboard funcional utilizado en el ejemplo. Este panel incluye indicadores de estado, una aguja para el CO2, y gráficas históricas comparativas.
Copia este JSON y guárdalo en un archivo de texto (ej. `dashboard.json`):

```json
{"__inputs": [{"name": "DS_INFLUXDB", "label": "InfluxDB", "description": "", "type": "datasource", "pluginId": "influxdb", "pluginName": "InfluxDB"}], "__elements": {}, "__requires": [{"type": "panel", "id": "gauge", "name": "Gauge", "version": ""}, {"type": "grafana", "id": "grafana", "name": "Grafana", "version": "12.3.3"}, {"type": "datasource", "id": "influxdb", "name": "InfluxDB", "version": "1.0.0"}, {"type": "panel", "id": "stat", "name": "Stat", "version": ""}, {"type": "panel", "id": "state-timeline", "name": "State timeline", "version": ""}, {"type": "panel", "id": "timeseries", "name": "Time series", "version": ""}], "annotations": {"list": [{"builtIn": 1, "datasource": {"type": "grafana", "uid": "-- Grafana --"}, "enable": true, "hide": true, "iconColor": "rgba(0, 211, 255, 1)", "name": "Annotations & Alerts", "type": "dashboard"}]}, "editable": true, "fiscalYearStartMonth": 0, "graphTooltip": 0, "links": [], "panels": [{"collapsed": false, "gridPos": {"h": 1, "w": 24, "x": 0, "y": 0}, "id": 4, "panels": [], "title": "Situaci\u00f3n actual", "type": "row"}, {"datasource": {"type": "influxdb", "uid": "${DS_INFLUXDB}"}, "fieldConfig": {"defaults": {"color": {"mode": "thresholds"}, "mappings": [{"options": {"0": {"color": "red", "index": 0, "text": "OFFLINE"}, "1": {"color": "green", "index": 1, "text": "ONLINE"}}, "type": "value"}], "thresholds": {"mode": "absolute", "steps": [{"color": "green", "value": 0}, {"color": "red", "value": 80}]}}, "overrides": [{"matcher": {"id": "byName", "options": "auto_mode_DP"}, "properties": [{"id": "mappings", "value": [{"options": {"0": {"color": "orange", "index": 1, "text": "MANUAL"}, "1": {"color": "blue", "index": 0, "text": "AUTO"}}, "type": "value"}]}, {"id": "displayName", "value": "MODO"}]}, {"matcher": {"id": "byName", "options": "vent_relay_DP"}, "properties": [{"id": "mappings", "value": [{"options": {"0": {"color": "text", "index": 1, "text": "VENT.APAGADA"}, "1": {"color": "purple", "index": 0, "text": "VENT.ACTIVA"}}, "type": "value"}]}, {"id": "displayName", "value": "VENTILACI\u00d3N"}]}, {"matcher": {"id": "byName", "options": "online"}, "properties": [{"id": "displayName", "value": "CONEXI\u00d3N"}]}]}, "gridPos": {"h": 5, "w": 10, "x": 0, "y": 1}, "id": 1, "options": {"colorMode": "value", "graphMode": "area", "justifyMode": "auto", "orientation": "auto", "percentChangeColorMode": "standard", "reduceOptions": {"calcs": ["lastNotNull"], "fields": "", "values": false}, "showPercentChange": false, "textMode": "auto", "wideLayout": true}, "pluginVersion": "12.3.3", "targets": [{"datasource": {"type": "influxdb", "uid": "${DS_INFLUXDB}"}, "query": "from(bucket: \"${UsuarioBucket}\")\r\n  |> range(start: v.timeRangeStart, stop: v.timeRangeStop)\r\n  |> filter(fn: (r) => r[\"_measurement\"] == \"alumnos_telemetry\")\r\n  |> filter(fn: (r) => r[\"thingId\"] == \"ESP32-final\")\r\n  |> filter(fn: (r) => r[\"_field\"] == \"online\" or r[\"_field\"] == \"auto_mode_DP\" or r[\"_field\"] == \"vent_relay_DP\")\r\n  // Agrupamos en ventanas (p.ej. 1\u202fmin). Cambia el intervalo a tu resoluci\u00f3n deseada.\r\n  |> aggregateWindow(every: 1m, fn: last, createEmpty: true)\r\n  // Rellenamos los huecos con el \u00faltimo valor no\u2011nulo anterior.\r\n  |> fill(usePrevious: true)", "refId": "A"}], "title": "Situaci\u00f3n actual", "type": "stat"}, {"datasource": {"type": "influxdb", "uid": "${DS_INFLUXDB}"}, "fieldConfig": {"defaults": {"color": {"mode": "thresholds"}, "mappings": [], "max": 5000, "min": 400, "thresholds": {"mode": "absolute", "steps": [{"color": "green", "value": 0}, {"color": "#EAB839", "value": 1000}, {"color": "red", "value": 2000}]}}, "overrides": []}, "gridPos": {"h": 5, "w": 5, "x": 10, "y": 1}, "id": 5, "options": {"minVizHeight": 75, "minVizWidth": 75, "orientation": "auto", "reduceOptions": {"calcs": ["lastNotNull"], "fields": "", "values": false}, "showThresholdLabels": true, "showThresholdMarkers": true, "sizing": "auto"}, "pluginVersion": "12.3.3", "targets": [{"query": "from(bucket: \"${UsuarioBucket}\")\r\n  |> range(start: v.timeRangeStart, stop: v.timeRangeStop)\r\n  |> filter(fn: (r) => r[\"_measurement\"] == \"alumnos_telemetry\")\r\n  |> filter(fn: (r) => r[\"thingId\"] == \"ESP32-final\")\r\n  |> filter(fn: (r) => r[\"_field\"] == \"air_quality\")\r\n  |> last()", "refId": "A", "datasource": {"type": "influxdb", "uid": "${DS_INFLUXDB}"}}], "title": "Calidad del aire", "type": "gauge"}, {"collapsed": false, "gridPos": {"h": 1, "w": 24, "x": 0, "y": 6}, "id": 3, "panels": [], "title": "Hist\u00f3rico", "type": "row"}, {"datasource": {"type": "influxdb", "uid": "${DS_INFLUXDB}"}, "fieldConfig": {"defaults": {"color": {"mode": "palette-classic"}, "custom": {"axisBorderShow": false, "axisCenteredZero": false, "axisColorMode": "text", "axisLabel": "", "axisPlacement": "auto", "barAlignment": 0, "barWidthFactor": 0.6, "drawStyle": "line", "fillOpacity": 0, "gradientMode": "none", "hideFrom": {"legend": false, "tooltip": false, "viz": false}, "insertNulls": false, "lineInterpolation": "linear", "lineWidth": 1, "pointSize": 5, "scaleDistribution": {"type": "linear"}, "showPoints": "auto", "showValues": false, "spanNulls": false, "stacking": {"group": "A", "mode": "none"}, "thresholdsStyle": {"mode": "off"}}, "mappings": [], "max": 5000, "min": 400, "thresholds": {"mode": "absolute", "steps": [{"color": "green", "value": 0}, {"color": "#EAB839", "value": 1000}, {"color": "red", "value": 2000}]}, "unit": "ppm"}, "overrides": [{"matcher": {"id": "byName", "options": "air_quality"}, "properties": [{"id": "displayName", "value": "Calidad del aire"}]}, {"matcher": {"id": "byName", "options": "threshold_vent_DP"}, "properties": [{"id": "displayName", "value": "Umbral"}]}]}, "gridPos": {"h": 7, "w": 15, "x": 0, "y": 7}, "id": 2, "options": {"legend": {"calcs": ["lastNotNull"], "displayMode": "list", "placement": "bottom", "showLegend": true}, "tooltip": {"hideZeros": false, "mode": "single", "sort": "none"}}, "pluginVersion": "12.3.3", "targets": [{"query": "from(bucket: \"${UsuarioBucket}\")\r\n  |> range(start: v.timeRangeStart, stop: v.timeRangeStop)\r\n  |> filter(fn: (r) => r[\"_measurement\"] == \"alumnos_telemetry\" and r[\"thingId\"] == \"ESP32-final\")\r\n  |> filter(fn: (r) => r[\"_field\"] == \"air_quality\" or r[\"_field\"] == \"threshold_vent_DP\")\r\n  |> aggregateWindow(every: v.windowPeriod, fn: mean, createEmpty: false) // Interpola para suavizar la l\u00ednea\r\n", "refId": "A", "datasource": {"type": "influxdb", "uid": "${DS_INFLUXDB}"}}], "title": "CO2 vs umbral", "type": "timeseries"}, {"datasource": {"type": "influxdb", "uid": "${DS_INFLUXDB}"}, "fieldConfig": {"defaults": {"color": {"mode": "palette-classic"}, "custom": {"axisBorderShow": false, "axisCenteredZero": false, "axisColorMode": "text", "axisLabel": "", "axisPlacement": "auto", "barAlignment": 0, "barWidthFactor": 0.6, "drawStyle": "line", "fillOpacity": 0, "gradientMode": "none", "hideFrom": {"legend": false, "tooltip": false, "viz": false}, "insertNulls": false, "lineInterpolation": "linear", "lineWidth": 1, "pointSize": 5, "scaleDistribution": {"type": "linear"}, "showPoints": "auto", "showValues": false, "spanNulls": false, "stacking": {"group": "A", "mode": "none"}, "thresholdsStyle": {"mode": "off"}}, "mappings": [], "thresholds": {"mode": "absolute", "steps": [{"color": "green", "value": 0}, {"color": "red", "value": 80}]}}, "overrides": [{"matcher": {"id": "byName", "options": "humidity"}, "properties": [{"id": "unit", "value": "percent"}, {"id": "min", "value": 0}, {"id": "max", "value": 100}, {"id": "displayName", "value": "Humedad"}]}, {"matcher": {"id": "byName", "options": "temperature"}, "properties": [{"id": "unit", "value": "celsius"}, {"id": "min", "value": -5}, {"id": "max", "value": 50}, {"id": "displayName", "value": "Temperatura"}]}]}, "gridPos": {"h": 6, "w": 15, "x": 0, "y": 14}, "id": 6, "options": {"legend": {"calcs": ["lastNotNull"], "displayMode": "list", "placement": "bottom", "showLegend": true}, "tooltip": {"hideZeros": false, "mode": "single", "sort": "none"}}, "pluginVersion": "12.3.3", "targets": [{"query": "from(bucket: \"${UsuarioBucket}\")\r\n  |> range(start: v.timeRangeStart, stop: v.timeRangeStop)\r\n  |> filter(fn: (r) => r[\"_measurement\"] == \"alumnos_telemetry\")\r\n  |> filter(fn: (r) => r[\"thingId\"] == \"ESP32-final\")\r\n  |> filter(fn: (r) => r[\"_field\"] == \"temperature\" or r[\"_field\"] == \"humidity\")\r\n  |> aggregateWindow(every: v.windowPeriod, fn: mean, createEmpty: false)\r\n  |> yield(name: \"mean\")", "refId": "A", "datasource": {"type": "influxdb", "uid": "${DS_INFLUXDB}"}}], "title": "Temperatura y Humedad", "type": "timeseries"}, {"datasource": {"type": "influxdb", "uid": "${DS_INFLUXDB}"}, "fieldConfig": {"defaults": {"color": {"mode": "fixed"}, "custom": {"axisPlacement": "auto", "fillOpacity": 70, "hideFrom": {"legend": false, "tooltip": false, "viz": false}, "insertNulls": false, "lineWidth": 0, "spanNulls": false}, "mappings": [{"options": {"0": {"color": "text", "index": 1, "text": "OFF"}, "1": {"color": "blue", "index": 0, "text": "ON"}}, "type": "value"}], "thresholds": {"mode": "absolute", "steps": [{"color": "green", "value": 0}, {"color": "red", "value": 80}]}}, "overrides": [{"matcher": {"id": "byName", "options": "auto_mode_DP"}, "properties": [{"id": "displayName", "value": "MODO AUTO"}]}, {"matcher": {"id": "byName", "options": "vent_relay_DP"}, "properties": [{"id": "displayName", "value": "VENTILACI\u00d3N"}]}, {"matcher": {"id": "byName", "options": "online"}, "properties": [{"id": "displayName", "value": "CONEXI\u00d3N"}]}]}, "gridPos": {"h": 5, "w": 15, "x": 0, "y": 20}, "id": 7, "options": {"alignValue": "center", "legend": {"displayMode": "list", "placement": "bottom", "showLegend": true}, "mergeValues": true, "rowHeight": 0.9, "showValue": "auto", "tooltip": {"hideZeros": false, "mode": "single", "sort": "none"}}, "pluginVersion": "12.3.3", "targets": [{"query": "from(bucket: \"${UsuarioBucket}\")\r\n  |> range(start: v.timeRangeStart, stop: v.timeRangeStop)\r\n  |> filter(fn: (r) => r[\"_measurement\"] == \"alumnos_telemetry\")\r\n  |> filter(fn: (r) => r[\"thingId\"] == \"ESP32-final\")\r\n  |> filter(fn: (r) => r[\"_field\"] == \"vent_relay_DP\" or r[\"_field\"] == \"auto_mode_DP\"or r[\"_field\"] == \"online\")\r\n  |> aggregateWindow(every: v.windowPeriod, fn: last, createEmpty: false)\r\n  |> yield(name: \"last\")", "refId": "A", "datasource": {"type": "influxdb", "uid": "${DS_INFLUXDB}"}}], "title": "Histor\u00edco de estados", "type": "state-timeline"}], "preload": false, "schemaVersion": 42, "tags": [], "templating": {"list": [{"current": {"text": "micro1", "value": "micro1"}, "description": "Tu nombre de usuario que coincide con el bucket", "name": "UsuarioBucket", "options": [{"selected": true, "text": "micro1", "value": "micro1"}], "query": "micro1", "type": "textbox"}]}, "time": {"from": "now-12h", "to": "now"}, "timepicker": {}, "timezone": "browser", "title": "Panel-ESP32-final micro1", "uid": "minxssn", "version": 5, "weekStart": "", "id": null}
```

### 11.2. Publicación y Despliegue
Este panel tiene como variable el bucket del que hay que extraer la info (`micro*`) y se podría importar por el usuario en su carpeta de grafana y probarlo, pero para hacerlo accesible públicamente e insertarlo en Node-RED hay que copiarlo con un nombre único en la carpeta **"Dashboards externos"** que tiene acceso "Viewer" externo sin autenticación. Una vez en esa carpeta, copia el URL de acceso al dashboard para usarlo en Node-RED.

Con esto, habrás completado la visualización profesional de datos de tu sistema.

---

### ¡FIN DEL PROYECTO!
Has construido un sistema IoT robusto, basado en un sistema operativo en tiempo real (FreeRTOS) y con una sincronización bidireccional perfecta con su Gemelo Digital. 

**Resumen de hitos alcanzados:**
- [x] Multitarea FreeRTOS (Scheduler y Prioridades).
- [x] Comunicación MQTT asíncrona por eventos y semáforos.
- [x] Gestión de telemetría inteligente (Criterios de Delta y Tiempo).
- [x] Control remoto avanzado vía Commandos RPC y Propiedades Deseadas.
- [x] Sincronización bidireccional de arranque (ESP32 y Dashboard).
- [x] Interfaz HMI física (NeoPixel/Botón) y digital (Node-RED/Grafana).




