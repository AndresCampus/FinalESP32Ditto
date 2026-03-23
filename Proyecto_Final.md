# Proyecto Final

## 1. Introducción
El objetivo de este proyecto final es diseñar, desarrollar y desplegar una arquitectura IoT industrial completa basándonos en el paradigma de **Gemelos Digitales (Digital Twins)**. 

Partiendo de un microcontrolador ESP32 equipado con sensores ambientales y actuadores físicos, construiremos un flujo de datos bidireccional usando el protocolo **MQTT**. Los sensores informarán del estado físico en tiempo real a la plataforma **Eclipse Ditto** (que mantendrá una representación virtual del dispositivo), almacenando el histórico en **InfluxDB** y permitiendo a un operador remoto observar y gobernar el sistema mediante un panel de control profesional diseñado en **Node-RED Dashboard 2.0** y **Grafana**.

---

## 2. Objetivos de Aprendizaje
Al finalizar con éxito este proyecto, el estudiante será capaz de:
* **Integración Hardware:** Adquirir y procesar datos físicos (Temperatura, Humedad, CO2) usando DHT22 y conversores ADC, controlando hardware reactivo (NeoPixel, Relés).
* **Concurrencia con FreeRTOS:** Abandonar el paradigma *Super-Loop* (`loop()` de Arduino) para diseñar un sistema robusto multitarea con colas y semáforos, separando las comunicaciones de red del procesamiento de los sensores físicos.
* **Protocolos IoT y Gemelos Digitales:** Comprender y aplicar el *Ditto Protocol* sobre MQTT para mantener sincronizado el estado deseado (`desiredProperties`) frente al estado real (`properties`) de un dispositivo.
* **Diseño e Interfaces (HMI):** Desarrollar un "Centro de Comando" (Dashboard) web reactivo para la monitorización de telemetría y el telecontrol (envío de comandos, actuación de relés, cambio de configuraciones en caliente).

---

## 3. Material Necesario y Arquitectura Base
* **Hardware:** Placa de desarrollo ESP32, Sensor DHT22, Potenciómetro (simulador de sensor de calidad de aire/CO2), Módulo Relé, Anillo LED NeoPixel y un pulsador físico.
* **Software Local:** Arduino IDE (optimizado con librerías FreeRTOS, `ArduinoJson`, `PubSubClient`, y `Button2`).
* **Cloud/Servicios:**
  * Broker MQTT (Mosquitto).
  * Gestor de Gemelos Digitales (Eclipse Ditto).
  * Base de Datos de Series Temporales (InfluxDB v2).
  * Orquestador de Flujos y Dashboard (Node-RED + Dashboard 2.0).

---

## 4. Estructura del Trabajo
El proyecto se dividirá en las siguientes fases incrementales (que iremos detallando en los próximos apartados):
1. **Fase 1:** Construcción de Tareas FreeRTOS (Lectura vs Comunicaciones).
2. **Fase 2:** Emparejamiento MQTT bidireccional y modelado en Eclipse Ditto.
3. **Fase 3:** Sistema guiado por eventos (Manejo del pulsador con librerías y semáforos).
4. **Fase 4:** Diseño del Panel de Mando reactivo a través de Node-RED.
