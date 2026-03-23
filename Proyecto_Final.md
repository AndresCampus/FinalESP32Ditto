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
3. **Telemetría Basada en Eventos (Deltas):** Si el ESP32 detecta cambios bruscos en la calidad del aire superiores a una diferencia programable (`publish_delta`), cortocircuitará el tiempo de espera y publicará instantáneamente la alerta de red, ahorrando ancho de banda el resto del tiempo.
4. **Auto-Gobernador Dinámico:** Monitorizará continuamente el aire para actuar directamente, y sin latencia de red, sobre un actuador duro (Relé). Abriendo así un sistema de ventilación de emergencia en el instante en el que supere un umbral de peligro (`threshold_vent`) programable.
5. **UI Física Sensorial:** El usuario *in-situ* siempre estará informado del estado actual gracias a un anillo LED NeoPixel multicolor (que traduce el aire en colores) y un LED clásico que se enciende en paralelo al Relé de ventilación.
6. **Manejo Manual (HMI Integrado):** Usando un interruptor de control de placa (Botón), el usuario podrá hacer una **Pulsación Larga** para deshabilitar el Modo Automático y tomar control manual del sistema, así como usar **Pulsaciones Cortas** para forzar encendidos y apagados del ventilador a conveniencia, cruzando inmediatamente esa información a la nube.
7. **Control Remoto vía Gemelo:** Mediante el sistema de *Desired Properties* de Ditto, el administrador de la red será capaz no solo de preconfigurar los parámetros de sensibilidad del dispositivo (`publish_delta` y `threshold_vent`) desde la nube, sino que podrá actuar remotamente sobre el ventilador manipulando sus propiedades.
8. **Comandos RPC:** El ESP32 será capaz de subscribirse a *Messages* nativos de Eclipse Ditto, habilitando el envío directo del comando `"refresh"` que obligará a la placa a interrumpir todo y realizar un volcado absoluto de datos bajo demanda, fuera de su ciclo de 30 segundos.

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
[{"id":"62b60d155fe9e520","type":"tab","label":"Flujo Final de partida","disabled":false,"info":"","env":[]},{"id":"de38854a09883df1","type":"config","z":"62b60d155fe9e520","name":"Nombre de usuario","properties":[{"p":"usuario","pt":"flow","to":"micro1","tot":"str"}],"active":true,"x":190,"y":140,"wires":[]},{"id":"b637f84c6d488fa3","type":"comment","z":"62b60d155fe9e520","name":"Configuración","info":"","x":150,"y":80,"wires":[]},{"id":"0eae88d0f4798f85","type":"config","z":"62b60d155fe9e520","name":"Clave de usuario","properties":[{"p":"claveusuario","pt":"flow","to":"iQvXjmy7","tot":"str"}],"active":true,"x":190,"y":200,"wires":[]},{"id":"af98d97fe7be1dc0","type":"config","z":"62b60d155fe9e520","name":"Dispositivo","properties":[{"p":"dispositivo","pt":"flow","to":"ESP32-final","tot":"str"}],"active":true,"x":170,"y":260,"wires":[]},{"id":"85515ab777a65116","type":"mqtt in","z":"62b60d155fe9e520","name":"iot/telemetry/<usuario>/<dispositivo>","topic":"","qos":"2","datatype":"auto-detect","broker":"8f28a739dcf74881","nl":false,"rap":true,"rh":0,"inputs":1,"x":700,"y":260,"wires":[["5a3dccc85c8f3f89","7fbfd145a7f2191b","f2d88fb060649408","e380ddb2abd31119","6efd06b37532e98e","f871c77d28fed2e8"]]},{"id":"d2fd8d5f210e092b","type":"inject","z":"62b60d155fe9e520","name":"","props":[{"p":"topic","v":"\"iot/telemetry/\" & $flowContext(\"usuario\") &\"/\"&  $flowContext(\"dispositivo\")","vt":"jsonata"},{"p":"action","v":"subscribe","vt":"str"},{"p":"payload"}],"repeat":"","crontab":"","once":true,"onceDelay":"0.5","topic":"","payload":"","payloadType":"str","x":440,"y":260,"wires":[["85515ab777a65116","b50bb4b400f563ae"]]},{"id":"b50bb4b400f563ae","type":"debug","z":"62b60d155fe9e520","name":"debug 60","active":true,"tosidebar":true,"console":false,"tostatus":false,"complete":"true","targetType":"full","statusVal":"","statusType":"auto","x":627,"y":324,"wires":[]},{"id":"5a3dccc85c8f3f89","type":"switch","z":"62b60d155fe9e520","name":"campo online?","property":"payload.online","propertyType":"msg","rules":[{"t":"nnull"}],"checkall":"true","repair":false,"outputs":1,"x":1000,"y":200,"wires":[["cb4083ad3a125f34"]]},{"id":"cb4083ad3a125f34","type":"ui-template","z":"62b60d155fe9e520","group":"36a310f050d3e09e","page":"","ui":"","name":"Estado conexión","order":2,"width":0,"height":0,"head":"","format":"<div style=\"display:flex; justify-content:center; align-items:center; height:100%;\">\n\n  <div style=\"display:flex; align-items:center; gap:12px;\">\n\n    \n    <!-- Indicador -->\n    <div :style=\"{\n      width: '30px',\n      height: '30px',\n      borderRadius: '50%',\n      backgroundColor: msg.payload?.online ? '#2ecc71' : '#e74c3c',\n      boxShadow: msg.payload?.online \n        ? '0 0 10px rgba(46, 204, 113, 0.8)' \n        : '0 0 10px rgba(231, 76, 60, 0.8)',\n      border: '2px solid rgba(0,0,0,0.2)'\n    }\"></div>\n\n    <!-- Estado -->\n    <span :style=\"{\n      fontWeight: 'bold',\n      fontSize: '20px',\n      color: msg.payload?.online ? '#2ecc71' : '#e74c3c',\n      lineHeight: '30px'\n    }\">\n      {{ msg.payload?.online ? \"ONLINE\" : \"OFFLINE\" }}\n    </span>\n\n     <!-- Nombre -->\n  <span\n        style=\"\n          min-width:160px;\n          font-weight:bold;\n          font-size:18px;\n          color:#5e35b1;\n          text-align:center;\n        \">\n        {{ msg.topic?.split('/')[2] + ':' + msg.topic?.split('/')[3] }}\n      </span>\n\n\n  </div>\n \n</div>","storeOutMessages":true,"passthru":true,"resendOnRefresh":true,"templateScope":"local","className":"","x":1250,"y":200,"wires":[[]]},{"id":"7fbfd145a7f2191b","type":"ui-text","z":"62b60d155fe9e520","group":"36a310f050d3e09e","order":4,"width":0,"height":0,"name":"","label":"Topic MQTT recepción desde Ditto :  ","format":"{{msg.payload}}","layout":"row-spread","style":false,"font":"","fontSize":16,"color":"#717171","wrapText":false,"className":"","value":"topic","valueType":"msg","x":1200,"y":260,"wires":[]},{"id":"f2d88fb060649408","type":"debug","z":"62b60d155fe9e520","name":"debug 61","active":true,"tosidebar":true,"console":false,"tostatus":false,"complete":"false","statusVal":"","statusType":"auto","x":980,"y":140,"wires":[]},{"id":"e380ddb2abd31119","type":"function","z":"62b60d155fe9e520","name":"function telemetría","func":"// 1️⃣  Recuperamos el último estado completo\nlet lastState = flow.get('lastTelemetry') || {};\n\n// 2️⃣  Normalizamos la carga entrante (puede ser parcial)\nlet inc = msg.payload || {};\n\n// 3️⃣  Si el mensaje *solo* lleva `online`, lo guardamos y salimos\nif (Object.keys(inc).length === 1 && inc.hasOwnProperty('online')) {\n    flow.set('lastOnline', inc.online);   // actualizamos el flag\n    return null;                         // no necesitamos renderizar aún\n}\n\n// 4️⃣  Mezclamos con el último flag guardado\nif (inc.online !== undefined) \n context.set('lastOnline',inc.online);\nelse \n inc.online = context.get('lastOnline') || 0;\n\n\n// 5️⃣  Merge con el resto del estado (igual que antes)\nlet merged = {\n    namespace: inc.namespace   || lastState.namespace   || \"Desconocido\",\n    thingId:   inc.thingId     || lastState.thingId     || \"Dispositivo\",\n    online:    inc.online,\n    temperature:    inc.temperature    !== undefined ? inc.temperature    : lastState.temperature,\n    humidity:       inc.humidity       !== undefined ? inc.humidity       : lastState.humidity,\n    air_quality:    inc.air_quality    !== undefined ? inc.air_quality    : lastState.air_quality,\n    vent_relay:     inc.vent_relay     !== undefined ? inc.vent_relay     : lastState.vent_relay,\n    auto_mode:      inc.auto_mode      !== undefined ? inc.auto_mode      : lastState.auto_mode,\n    threshold_vent: inc.threshold_vent !== undefined ? inc.threshold_vent : lastState.threshold_vent,\n    publish_delta:  inc.publish_delta  !== undefined ? inc.publish_delta  : lastState.publish_delta\n};\n\n// 6️⃣  Campos semánticos y colores (igual que antes)\nmerged.relayText = (merged.vent_relay === 1) ? \"EXTRACCIÓN ON\" : \"REPOSO OFF\";\nmerged.modeText  = (merged.auto_mode  === 1) ? \"AUTOMÁTICO\"   : \"MANUAL\";\n\nmerged.airColor   = (merged.air_quality >= (merged.threshold_vent || 1000))\n                    ? \"#ef4444\"\n                    : (merged.air_quality >= 1000 ? \"#f59e0b\" : \"#10b981\");\nmerged.relayColor = (merged.vent_relay === 1) ? \"#3b82f6\" : \"#334155\";\nmerged.modeColor  = (merged.auto_mode  === 1) ? \"#8b5cf6\" : \"#6366f1\";\n\n// 7️⃣  Guardamos el estado completo para la próxima actualización\nflow.set('lastTelemetry', merged);\n\n// 8️⃣  Enviamos al UI\nmsg.payload = merged;\nreturn msg;\n","outputs":1,"timeout":0,"noerr":0,"initialize":"","finalize":"","libs":[],"x":1010,"y":320,"wires":[["dd5169226161ae2d","5d92d4db032b2848"]]},{"id":"dd5169226161ae2d","type":"ui-template","z":"62b60d155fe9e520","group":"36a310f050d3e09e","page":"","ui":"","name":"Telemetría Dispositivo","order":3,"width":0,"height":0,"head":"","format":"<style>\n  @import url('https://fonts.googleapis.com/css2?family=Inter:wght@400;600;800&display=swap');\n\n  /* ------------- Card base ------------- */\n  .telemetry-glass-card {\n    font-family: 'Inter', sans-serif;\n    background: linear-gradient(135deg, rgba(30, 41, 59, 0.9) 0%, rgba(15, 23, 42, 0.95) 100%);\n    border: 1px solid rgba(255, 255, 255, 0.1);\n    border-radius: 16px;\n    padding: 24px;\n    color: #f8fafc;\n    box-shadow: 0 10px 30px rgba(0, 0, 0, 0.3);\n    max-width: 500px;\n    width: 100%;\n    margin: auto;\n    transition: transform 0.2s ease-in-out, opacity 0.4s ease;\n  }\n\n  /* Cuando el dispositivo está offline → opacidad reducida */\n  .offline {\n    opacity: 0.28;          /* ≈ 30 % visible */\n    filter: grayscale(0.6); /* opcional: le da un tono grisáceo */\n  }\n\n  /* Cuando está online → opacidad total */\n  .online {\n    opacity: 1;\n  }\n\n  /* ----------- Resto del estilo (sin cambios) ----------- */\n  .tel-header { display:flex; justify-content:space-between; align-items:center;\n                border-bottom:1px solid rgba(255,255,255,0.05); padding-bottom:12px;\n                margin-bottom:20px; }\n  .tel-header h2 { margin:0; font-size:1.1rem; font-weight:800; letter-spacing:0.5px;\n                   color:#e2e8f0; }\n  .tel-badge { background:rgba(255,255,255,0.1); padding:6px 12px; border-radius:20px;\n               font-size:0.70rem; font-weight:800; text-transform:uppercase;\n               letter-spacing:1px; color:#cbd5e1; }\n  .tel-grid { display:grid; grid-template-columns:1fr 1fr; gap:16px; }\n  .tel-stat { background:rgba(0,0,0,0.25); padding:16px; border-radius:12px;\n              display:flex; flex-direction:column; justify-content:center;\n              border:1px solid rgba(255,255,255,0.02); }\n  .tel-stat.accent { grid-column:span 2; background:rgba(0,0,0,0.35); }\n  .tel-label { font-size:0.8rem; color:#94a3b8; margin-bottom:6px;\n               font-weight:600; text-transform:uppercase; }\n  .tel-value { font-size:1.6rem; font-weight:800; color:#ffffff; }\n  .tel-value-large { font-size:2.8rem; text-shadow:0px 0px 20px rgba(0,0,0,0.5); }\n  .tel-unit { font-size:0.9rem; color:#64748b; font-weight:600; margin-left:4px; }\n  .tel-status-pill { display:inline-block; padding:8px 12px; border-radius:8px;\n                     font-size:0.85rem; font-weight:800; text-align:center;\n                     box-shadow:inset 0 2px 4px rgba(0,0,0,0.1); }\n</style>\n\n<!-- El card recibe dinámicamente la clase \"online\" / \"offline\" -->\n<div v-if=\"msg.payload\"\n     :class=\"['telemetry-glass-card',\n              msg.payload.online === 1 ? 'online' : 'offline']\">\n\n  <div class=\"tel-header\">\n    <div class=\"tel-badge\">IoT Node</div>\n    <h2>{{ msg.payload.namespace }} : {{ msg.payload.thingId }}</h2>\n  </div>\n\n  <div class=\"tel-grid\">\n    <!-- Calidad del Aire (ancho total) -->\n    <div class=\"tel-stat accent\">\n      <span class=\"tel-label\">Calidad del Aire (CO2)</span>\n      <div>\n        <span class=\"tel-value tel-value-large\"\n              :style=\"{ color: msg.payload.airColor }\">\n          {{ msg.payload.air_quality }}\n        </span>\n        <span class=\"tel-unit\">ppm</span>\n      </div>\n    </div>\n\n    <!-- Temperatura -->\n    <div class=\"tel-stat\">\n      <span class=\"tel-label\">Temperatura</span>\n      <div>\n        <span class=\"tel-value\">{{ msg.payload.temperature }}</span>\n        <span class=\"tel-unit\">°C</span>\n      </div>\n    </div>\n\n    <!-- Humedad -->\n    <div class=\"tel-stat\">\n      <span class=\"tel-label\">Humedad</span>\n      <div>\n        <span class=\"tel-value\">{{ msg.payload.humidity }}</span>\n        <span class=\"tel-unit\">%</span>\n      </div>\n    </div>\n\n    <!-- Estado Relé -->\n    <div class=\"tel-stat\" style=\"align-items:center;\">\n      <span class=\"tel-label\">Relé Ventilación</span>\n      <span class=\"tel-status-pill\"\n            :style=\"{ background: msg.payload.relayColor, color: 'white', width: '100%' }\">\n        {{ msg.payload.relayText }}\n      </span>\n    </div>\n\n    <!-- Modo Automático / Manual -->\n    <div class=\"tel-stat\" style=\"align-items:center;\">\n      <span class=\"tel-label\">Gobernador</span>\n      <span class=\"tel-status-pill\"\n            :style=\"{ background: msg.payload.modeColor, color: 'white', width: '100%' }\">\n        {{ msg.payload.modeText }}\n      </span>\n    </div>\n\n    <!-- Umbral Disparo -->\n    <div class=\"tel-stat\">\n      <span class=\"tel-label\">Umbral Disparo</span>\n      <div>\n        <span class=\"tel-value\">{{ msg.payload.threshold_vent }}</span>\n      </div>\n    </div>\n\n    <!-- Refresh Δ -->\n    <div class=\"tel-stat\">\n      <span class=\"tel-label\">Refresh Δ</span>\n      <div>\n        <span class=\"tel-value\">{{ msg.payload.publish_delta }}</span>\n      </div>\n    </div>\n  </div>\n</div>\n","storeOutMessages":true,"passthru":true,"resendOnRefresh":true,"templateScope":"local","className":"","x":1240,"y":320,"wires":[[]]},{"id":"84a449cfc51298f6","type":"ui-text","z":"62b60d155fe9e520","group":"36a310f050d3e09e","order":5,"width":0,"height":0,"name":"","label":"Última actualización sensores : ","format":"{{msg.payload}}","layout":"row-left","style":false,"font":"","fontSize":16,"color":"#717171","wrapText":false,"className":"","value":"payload","valueType":"msg","x":1210,"y":400,"wires":[]},{"id":"6efd06b37532e98e","type":"function","z":"62b60d155fe9e520","name":"fecha/hora","func":"let semana = [\"Domingo\", \"Lunes\", \"Martes\", \"Miércoles\", \"Jueves\", \"Viernes\", \"Sábado\"];\nlet ahora = new Date();\nmsg.payload = semana[ahora.getDay()] + \", \"\n    + ahora.toLocaleString('es-ES', { timeZone: 'Europe/Madrid', hour12: false });\nreturn msg;\n","outputs":1,"timeout":0,"noerr":0,"initialize":"","finalize":"","libs":[],"x":970,"y":400,"wires":[["84a449cfc51298f6"]]},{"id":"15c69df4f1af720f","type":"ui-slider","z":"62b60d155fe9e520","group":"7c527cd7dda522e3","name":"","label":"Umbral disparo","tooltip":"","order":5,"width":0,"height":0,"passthru":false,"outs":"end","topic":"threshold_vent","topicType":"str","thumbLabel":"always","showTicks":"true","min":"400","max":"5000","step":"100","className":"","iconPrepend":"","iconAppend":"","color":"","colorTrack":"","colorThumb":"","showTextField":false,"x":480,"y":560,"wires":[["2f622c2a2c68abb0"]]},{"id":"f0afdb3c2801acaa","type":"ui-slider","z":"62b60d155fe9e520","group":"7c527cd7dda522e3","name":"","label":"Delta refresco","tooltip":"","order":6,"width":0,"height":0,"passthru":false,"outs":"end","topic":"publish_delta","topicType":"str","thumbLabel":"always","showTicks":"true","min":"10","max":"1000","step":"10","className":"","iconPrepend":"","iconAppend":"","color":"","colorTrack":"","colorThumb":"","showTextField":false,"x":480,"y":500,"wires":[["2f622c2a2c68abb0"]]},{"id":"eacfbde4eeccaae4","type":"ui-radio-group","z":"62b60d155fe9e520","group":"7c527cd7dda522e3","name":"","label":"Modo de funcionamiento:","order":4,"width":0,"height":0,"columns":"2","passthru":false,"options":[{"label":"MANUAL","value":0,"type":"num"},{"label":"AUTOMATICO","value":1,"type":"num"}],"payload":"","topic":"auto_mode","topicType":"str","className":"","x":450,"y":620,"wires":[["2f622c2a2c68abb0","a26a37b2bdfc6891"]]},{"id":"c910cc8693fbed3e","type":"ui-button","z":"62b60d155fe9e520","group":"7c527cd7dda522e3","name":"","label":"Encender ventilación","order":2,"width":"2","height":"1","emulateClick":false,"tooltip":"","color":"","bgcolor":"","className":"","icon":"","iconPosition":"left","payload":"1","payloadType":"num","topic":"vent_relay","topicType":"str","buttonColor":"","textColor":"","iconColor":"","enableClick":true,"enablePointerdown":false,"pointerdownPayload":"","pointerdownPayloadType":"str","enablePointerup":false,"pointerupPayload":"","pointerupPayloadType":"str","x":460,"y":700,"wires":[["2f622c2a2c68abb0"]]},{"id":"a36cb6a532f74f6d","type":"ui-button","z":"62b60d155fe9e520","group":"7c527cd7dda522e3","name":"","label":"Apagar ventilación","order":3,"width":"2","height":"1","emulateClick":false,"tooltip":"","color":"","bgcolor":"","className":"","icon":"","iconPosition":"left","payload":"0","payloadType":"num","topic":"vent_relay","topicType":"str","buttonColor":"","textColor":"","iconColor":"","enableClick":true,"enablePointerdown":false,"pointerdownPayload":"","pointerdownPayloadType":"str","enablePointerup":false,"pointerupPayload":"","pointerupPayloadType":"str","x":470,"y":760,"wires":[["2f622c2a2c68abb0"]]},{"id":"2f622c2a2c68abb0","type":"debug","z":"62b60d155fe9e520","name":"Ordenes a Ditto","active":true,"tosidebar":true,"console":false,"tostatus":false,"complete":"payload","targetType":"msg","statusVal":"","statusType":"auto","x":760,"y":560,"wires":[]},{"id":"a26a37b2bdfc6891","type":"function","z":"62b60d155fe9e520","name":"Sólo en manual","func":"// Comprobamos si el gobernador está en Automático (1)\nlet esAutomatico = (msg.payload === 1);\n// Configuramos la propiedad especial de Dashboard 2.0\nmsg.enabled= ! esAutomatico // Si es true, el botón se bloquea en gris y no hace nada\nreturn msg;","outputs":1,"timeout":0,"noerr":0,"initialize":"","finalize":"","libs":[],"x":200,"y":720,"wires":[["c910cc8693fbed3e","a36cb6a532f74f6d"]]},{"id":"f871c77d28fed2e8","type":"function","z":"62b60d155fe9e520","name":"Actualiza automático","func":"if (msg.payload && msg.payload.auto_mode !== undefined) {\n  msg.payload = msg.payload.auto_mode;\n  return msg;\n}","outputs":1,"timeout":0,"noerr":0,"initialize":"","finalize":"","libs":[],"x":200,"y":620,"wires":[["eacfbde4eeccaae4"]]},{"id":"5d92d4db032b2848","type":"debug","z":"62b60d155fe9e520","name":"debug 67","active":true,"tosidebar":true,"console":false,"tostatus":false,"complete":"false","statusVal":"","statusType":"auto","x":1200,"y":360,"wires":[]},{"id":"8f28a739dcf74881","type":"mqtt-broker","name":"","broker":"mqtt.iot-uma.es","port":"1883","clientid":"","autoConnect":true,"usetls":false,"protocolVersion":"4","keepalive":"60","cleansession":true,"autoUnsubscribe":true,"birthTopic":"","birthQos":"0","birthRetain":"false","birthPayload":"","birthMsg":{},"closeTopic":"","closeQos":"0","closeRetain":"false","closePayload":"","closeMsg":{},"willTopic":"","willQos":"0","willRetain":"false","willPayload":"","willMsg":{},"userProps":"","sessionExpiry":""},{"id":"36a310f050d3e09e","type":"ui-group","name":"Datos dispositivo final ESP32","page":"5754964613c06fde","width":"4","height":"1","order":1,"showTitle":true,"className":"","visible":"true","disabled":"false","groupType":"default"},{"id":"7c527cd7dda522e3","type":"ui-group","name":"Configuración dispositivo","page":"5754964613c06fde","width":"4","height":"1","order":2,"showTitle":true,"className":"","visible":"true","disabled":"false","groupType":"default"},{"id":"5754964613c06fde","type":"ui-page","name":"App final Ditto","ui":"9aa19c35c0896e2a","path":"/finalDitto","icon":"home","layout":"grid","theme":"6f5d93716d274d38","breakpoints":[{"name":"Default","px":"0","cols":"3"},{"name":"Tablet","px":"576","cols":"6"},{"name":"Small Desktop","px":"768","cols":"9"},{"name":"Desktop","px":"1024","cols":"12"}],"order":1,"className":"","visible":"true","disabled":"false"},{"id":"9aa19c35c0896e2a","type":"ui-base","name":"My Dashboard","path":"/dashboard","appIcon":"","includeClientData":true,"acceptsClientConfig":["ui-notification","ui-control"],"showPathInSidebar":false,"headerContent":"page","navigationStyle":"default","titleBarStyle":"default","showReconnectNotification":true,"notificationDisplayTime":1,"showDisconnectNotification":true,"allowInstall":false},{"id":"6f5d93716d274d38","type":"ui-theme","name":"Default Theme","colors":{"surface":"#ffffff","primary":"#0094CE","bgPage":"#eeeeee","groupBg":"#ffffff","groupOutline":"#cccccc"},"sizes":{"density":"default","pagePadding":"12px","groupGap":"12px","groupBorderRadius":"4px","widgetGap":"12px"}}]
```

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
