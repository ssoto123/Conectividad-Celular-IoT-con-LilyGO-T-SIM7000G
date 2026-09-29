# Guía Técnica de Prácticas: Conectividad Celular IoT con LilyGO T-SIM7000G

¡Bienvenidas y bienvenidos al repositorio de la práctica! Este documento sirve como guía integral y teórica para comprender el funcionamiento de nuestra placa **LilyGO T-SIM7000G** y la implementación del firmware en C++ usando la librería `TinyGSM` para la transmisión de datos IoT mediante el protocolo **MQTT**.

## 📸 1. Estructura del Hardware: La Integración ESP32 y SIM7000G

Aunque vemos una sola tarjeta de circuito impreso, la **LilyGO T-SIM7000G** en realidad es como tener dos computadoras independientes que viven en la misma casa y se hablan a través de cables internos.

1. **ESP32 (Microcontrolador Principal):** Es el cerebro de nuestro código. Ejecuta la lógica, lee sensores, y decide qué hacer.

2. **SIM7000G (Módem Celular):** Es el chip de radio dedicado exclusivamente a manejar la conexión con las antenas celulares y el GPS.

### Diagrama de Conexión Interna

El ESP32 y el SIM7000G no son lo mismo. Están separados pero unidos en la placa mediante los siguientes pines:

```
+-----------------------------+                           +-------------------------------+
|      ESP32 (Cerebro)        |                           |  SIM7000G (Radio Módem)       |
|                             |                           |                               |
|               (GPIO 27) TX  |-------- Serial1 --------->| RX (UART_RXD)                 |
|               (GPIO 26) RX  |<------- Serial1 ----------| TX (UART_TXD)                 |
|                             |                           |                               |
|                (GPIO 4) D4  |---- Pulso de Encendido -->| PWRKEY (Botón de encendido)   |
|                             |                           |                               |
|                    3.3V/5V  |====== Alimentación =======| VCC                           |
|                        GND  |====== Tierra =============| GND                           |
+-----------------------------+                           +-------------------------------+
                                                                         |
                                                                    (Señal RF)
                                                                         v
                                                                 Antena Celular Externa

```

### 💡 Analogía de la Placa: *El Ingeniero y el Chofer*

* **El ESP32 es el "Ingeniero":** Escribe las cartas (datos MQTT) pero no sabe conducir por la red celular.
* **El SIM7000G es el "Chofer":** No le importa qué dice la carta, pero conoce perfectamente las rutas, sabe cómo hablar con las torres celulares y transporta el paquete de forma segura.
* **El Cable Serial (TX/RX):** Es la ventanilla donde el Ingeniero le pasa las cartas y las instrucciones al Chofer.

## 📡 2. Tecnologías Celulares: GSM, GPRS, LTE-M y NB-IoT

El módem SIM7000G es un módulo híbrido que soporta múltiples tecnologías (LPWAN y 2G). Aquí explicamos cada una:

| Tecnología | Generación | Características Principales | Ventajas | Desventajas | Casos de Uso Comunes | 
| ----- | ----- | ----- | ----- | ----- | ----- | 
| **GSM** | 2G | Orientada a llamadas de voz y SMS. | Conexión muy estable para voz. | No sirve para Internet continuo (reserva todo el canal). | Llamadas clásicas, SMS. | 
| **GPRS** | 2.5G | Datos por paquetes sobre redes 2G. | Cobertura casi global; sirve como respaldo confiable. | Muy lento; alto consumo de batería; redes 2G apagándose en muchos países. | Telemetría básica antigua, cajeros automáticos (ATMs). | 
| **LTE-M** (Cat-M1) | 4G (IoT) | Diseñada para IoT móvil. Soporta transición entre antenas sin cortar conexión (Handover). | Mayor velocidad que NB-IoT; soporta voz (VoLTE) y movilidad (vehículos). | Menor penetración en interiores profundos comparado con NB-IoT. | Rastreo de flotas de camiones, wearables, alarmas médicas. | 
| **NB-IoT** | 4G (IoT) | Banda estrecha (NarrowBand), pulsos intermitentes. | Consumo de batería extremadamente bajo (años); penetración profunda en concreto/sótanos. | Muy lento; **no soporta movilidad** (si te mueves de antena, la conexión se cae y debe reiniciarse). | Medidores de agua/luz subterráneos, sensores agrícolas fijos. | 

### ¿Cuál estamos usando en este ejemplo y por qué?

En el código definimos `#define TINY_GSM_USE_GPRS true`.
Aunque la tarjeta *físicamente* puede conectarse a LTE-M o NB-IoT (si la SIM y el operador lo soportan), en términos de **código y protocolo (Librería TinyGSM)**, estamos utilizando los comandos estándar de **GPRS (Packet Data Protocol)**.

* **¿Por qué?** Porque es el estándar universal de abstracción. Al pedirle al módem una conexión "GPRS", le estamos diciendo *"Abre un canal de datos de Internet"*. El módem se encargará automáticamente de negociar con la torre celular (Telcel en este caso) para usar la mejor red disponible (LTE-M si está configurado, o GPRS clásico como respaldo). Es más fácil de programar y asegura retrocompatibilidad.

## 🗣️ 3. Comunicación: Los Comandos AT

Como vimos en el diagrama, el ESP32 se comunica con el módem por los pines `TX/RX` mediante texto plano. Este lenguaje se llama **Comandos AT** (de *Attention*).

La librería `TinyGSM` hace todo este trabajo sucio por nosotros, pero por detrás, el ESP32 está enviando mensajes como estos al SIM7000G:

1. **Prueba de vida:**
   * ESP32 envía: `AT`
   * Módem responde: `OK`

2. **Consultar calidad de señal (Signal Quality):**
   * ESP32 envía: `AT+CSQ`
   * Módem responde: `+CSQ: 18,99` *(El 18 indica buena señal)*

3. **Consultar si la tarjeta SIM está desbloqueada:**
   * ESP32 envía: `AT+CPIN?`
   * Módem responde: `+CPIN: READY`

4. **Conectarse a la red (GPRS Attach):**
   * ESP32 envía: `AT+CGATT=1`
   * Módem responde: `OK` *(¡Conectado a la antena!)*

## 🌐 4. Conceptos Teóricos y Analogías de Red MQTT

Para entender cómo viaja un dato desde nuestro LED hasta la nube:

```
[ESP32] --AT Cmds--> [Módem SIM7000] --LTE/GPRS--> [Antena Telcel] --> [APN Caseta] --> [Internet] --> [Broker MQTT]
```

### A. El APN (Access Point Name)

El APN (en el código: `"internet.itelcel.com"`) es la puerta de enlace configurada por el operador celular (Telcel).
* **Analogía:** Es la **caseta de cobro y aduana** para salir de la autopista privada de la compañía telefónica y entrar a la red global de Internet. Sin el APN correcto, la torre celular reconoce tu chip, pero te prohíbe salir a Internet.

### B. Protocolo MQTT (Publish / Subscribe)

MQTT es un protocolo de mensajería extremadamente ligero, ideal para redes celulares donde pagamos por cada Kilobyte.
* **El Broker (`broker.hivemq.com`):** Es la oficina central de correos.
* **Topics (Tópicos):** Son los apartados postales (`GsmClientTest/led`).
* **Publish (Publicar):** Enviar un mensaje a un apartado postal.
* **Subscribe (Suscribir):** Decirle al correo: *"Avísame en tiempo real si llega una carta a este apartado"*.

## 🛠️ 5. Desglose del Código y Control de Flujo

### A. Inicialización del Módem (Control de Alimentación)
El módem requiere un "empujón" físico para despertar de su estado de bajo consumo:
```cpp
pinMode(4, OUTPUT);
digitalWrite(4, HIGH);
delay(1000); // Mantenemos el "botón" presionado 1 segundo
digitalWrite(4, LOW);
```
El GPIO 4 del ESP32 está conectado físicamente al pin `PWRKEY` del módem.

### B. La Secuencia de Conexión en `setup()`
1. **`SerialAT.begin(...)`**: Abre el canal de comunicación a 115200 baudios.
2. **`modem.waitForNetwork()`**: El módem busca señal de las antenas.
3. **`modem.gprsConnect(apn, ...)`**: Entrega el pasaporte a la caseta.
4. **`mqttConnect()`**: Abre un canal directo a la oficina de correos de HiveMQ y se suscribe al tópico.

### C. Recepción en `loop()` y `mqttCallback()`
* **`mqttCallback()`**: Es la función que se ejecuta como un "reflejo" cuando el Broker nos avisa que llegó un mensaje.
* **`loop()`**: Verifica continuamente que no nos hayamos quedado sin señal celular. Si un túnel o interferencia corta la conexión, ejecuta rutinas de reconexión automática.

## 🚀 6. Guía de Despliegue y Pruebas

### Requisitos Previos
Instala las siguientes librerías en tu IDE (Arduino o PlatformIO):
1. `TinyGSM` (Volodymyr Shymanskyy).
2. `PubSubClient` (Nick O'Leary).

### Pasos de Ejecución
1. Inserta una Nano SIM con datos (sin código PIN) en la placa.
2. Ajusta las variables `apn`, `gprsUser` y `gprsPass` si usas un proveedor distinto a Telcel.
3. Carga el código al ESP32.
4. Abre un cliente web como [HiveMQ WebSocket Client](http://www.hivemq.com/demos/websocket-client/).
5. Suscríbete al tópico: `GsmClientTest/ledStatus`.
6. Publica el mensaje `"toggle"` en el tópico: `GsmClientTest/led`.
7. **Resultado:** ¡Tu placa LilyGO recibirá el comando celularmente, cambiará la luz de su LED y te responderá en la web!

## ⚠️ 7. Troubleshooting: 5 Escenarios de Fallo Comunes

El trabajo con radiofrecuencia e IoT celular puede ser caprichoso. Aquí están los errores más comunes y cómo resolverlos:

### Fallo 1: Reinicios constantes o el módem no responde (Brownout / Caída de Voltaje)
* **Síntoma:** El ESP32 se reinicia solo, o el monitor serie dice "Initializing modem... fail".
* **Causa (La Analogía):** El "Chofer" intentó encender un motor V8 pero la batería era de motocicleta. El módem SIM7000G puede consumir picos de hasta 2 Amperios cuando intenta registrarse en la red celular. Un puerto USB de computadora estándar (500mA) a menudo no es suficiente.
* **Solución:** Conecta una batería LiPo de 3.7V al conector JST de la placa, o utiliza un cargador USB de pared de alta calidad (min 2A) con un cable grueso.

### Fallo 2: Falla en "Waiting for network..." (No hay señal)
* **Síntoma:** El código se queda atascado esperando la red celular y luego imprime "fail".
* **Causa:** El módem está encendido y hablando con el ESP32, pero no puede "ver" las antenas celulares.
* **Solución:** 
  1. Verifica que conectaste la antena LTE incluida al puerto U.FL correcto (el SIM7000 tiene dos conectores: uno para LTE y otro para GPS).
  2. Asegúrate de que la tarjeta SIM está correctamente insertada y activada con saldo/datos.
  3. Verifica que la SIM no tenga un PIN de bloqueo (o configúralo en el código en `#define GSM_PIN ""`).

### Fallo 3: Falla en "Connecting to [APN]" (Rechazo de red)
* **Síntoma:** Pasa la prueba de red, pero al intentar conectarse al GPRS marca error.
* **Causa:** Las torres de Telcel ven tu dispositivo, pero la "caseta de cobro" (APN) rechaza tu entrada.
* **Solución:** Revisa que el string `apn` esté escrito exactamente como lo dicta tu proveedor de telefonía. Por ejemplo, en México para Telcel a veces es `internet.itelcel.com`, pero para chips de IoT específicos (M2M) puede ser diferente (ej. `m2m.itelcel.com`).

### Fallo 4: Fallo de comunicación serial (Modem not responding)
* **Síntoma:** El código ni siquiera puede leer la información del módem (`modem.getModemInfo()` devuelve basura o nada).
* **Causa:** El "Ingeniero" y el "Chofer" no están hablando en el mismo idioma (baudios) o los cables están cruzados.
* **Solución:** Revisa la línea `SerialAT.begin(115200, SERIAL_8N1, 26, 27);`. Si estás usando otra versión de la placa, los pines TX/RX podrían ser diferentes. Asegúrate de que la rutina de encendido del pin 4 (`PWRKEY`) se ejecute correctamente.

### Fallo 5: Conecta a GPRS pero "=== MQTT NOT CONNECTED ==="
* **Síntoma:** Tienes Internet celular, pero no puedes publicar ni recibir mensajes.
* **Causa:** Problemas de resolución DNS o rechazo por parte del servidor de correos (HiveMQ).
* **Solución:** 
  1. Cambia el Client ID de MQTT. El código dice `mqtt.connect("GsmClientTest")`. Si alguien más en el mundo está usando exactamente el mismo nombre "GsmClientTest", el broker los desconectará a ambos repetidamente. Añade números aleatorios al final (ej. `GsmClientTest_9981`).
  2. Asegúrate de que el puerto sea el correcto (1883 para conexiones sin encriptar).