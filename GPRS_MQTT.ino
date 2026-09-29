/**************************************************************
 * Ejemplo de Conexión MQTT por GPRS con LilyGO T-SIM7000G
 * 
 * Requisitos:
 * - Instalar librería PubSubClient (de Nick O'Leary)
 * - Instalar librería TinyGSM (de Volodymyr Shymanskyy)
 * 
 * Este código conecta el ESP32 a la red celular, abre el túnel 
 * de datos (GPRS/LTE) y se conecta a un "Broker" público (HiveMQ).
 **************************************************************/

// 1. DEFINICIÓN DEL HARDWARE ----------------------------------
// Le decimos a la librería qué "Chofer" (módem) estamos usando.
#define TINY_GSM_MODEM_SIM7000

// Definimos nombres amigables para los puertos seriales:
// SerialMon es la consola de la computadora (el monitor serie)
#define SerialMon Serial
// SerialAT es la línea de comunicación interna entre el ESP32 y el SIM7000G
#define SerialAT Serial1

// Activa los mensajes de depuración en la consola
#define TINY_GSM_DEBUG SerialMon

// 2. CONFIGURACIÓN DE RED Y CREDENCIALES ----------------------
// Le indicamos a la librería que usaremos la red celular (GPRS) y no WiFi
#define TINY_GSM_USE_GPRS true
#define TINY_GSM_USE_WIFI false

// PIN de la tarjeta SIM (dejar vacío si no tiene PIN de seguridad)
#define GSM_PIN ""

// CREDENCIALES APN (Access Point Name): La "caseta de cobro" de tu operadora.
// ¡IMPORTANTE! Cambia esto por los de tu compañía celular (ej. Telcel, AT&T).
const char apn[]      = "internet.itelcel.com";     
const char gprsUser[] = "itelcel";
const char gprsPass[] = "itelcel";

// 3. CONFIGURACIÓN DEL BROKER MQTT (Oficina de correos) -------
const char* broker = "broker.hivemq.com";

// Tópicos (Buzones) a los que nos vamos a suscribir o publicar
// Se recomienda cambiar "GsmClientTest" por un nombre único para evitar colisiones en el grupo.
const char* topicLed       = "GsmClientTest/led";       // Aquí escuchamos órdenes
const char* topicInit      = "GsmClientTest/init";      // Aquí avisamos que encendimos
const char* topicLedStatus = "GsmClientTest/ledStatus"; // Aquí avisamos el estado actual

// 4. LIBRERÍAS Y OBJETOS --------------------------------------
#include <TinyGsmClient.h>
#include <PubSubClient.h>

// Creamos los objetos (instancias) que harán el trabajo pesado:
TinyGsm        modem(SerialAT); // El objeto que controla el módem con comandos AT
TinyGsmClient  client(modem);   // El cliente de red que usa el módem
PubSubClient   mqtt(client);    // El cliente MQTT que usa la red

// 5. PINES DE CONTROL -----------------------------------------
#define LED_PIN 12      // Pin donde está conectado el LED azul en la placa LilyGO
#define PWR_PIN 4       // Pin de encendido (PWRKEY) del módem SIM7000G

int ledStatus = LOW;
uint32_t lastReconnectAttempt = 0; // Temporizador para intentar reconectar al broker

// =============================================================
// FUNCIÓN CALLBACK: Se ejecuta automáticamente al recibir un mensaje
// =============================================================
void mqttCallback(char* topic, byte* payload, unsigned int len) {
  SerialMon.print("Mensaje recibido [");
  SerialMon.print(topic);
  SerialMon.print("]: ");
  
  // Imprimir el mensaje recibido letra por letra en la consola
  for (int i = 0; i < len; i++) {
    SerialMon.print((char)payload[i]);
  }
  SerialMon.println();

  // Si el mensaje llegó al "buzón" (tópico) de control del LED...
  if (String(topic) == topicLed) {
    ledStatus = !ledStatus;            // Invertimos el estado (si estaba LOW pasa a HIGH y viceversa)
    digitalWrite(LED_PIN, ledStatus);  // Aplicamos el cambio físico al LED
    
    // Publicamos de regreso para confirmar que hicimos el cambio
    mqtt.publish(topicLedStatus, ledStatus ? "1" : "0");
  }
}

// =============================================================
// FUNCIÓN DE CONEXIÓN MQTT: Intenta hablar con el broker
// =============================================================
boolean mqttConnect() {
  SerialMon.print("Conectando al broker MQTT: ");
  SerialMon.print(broker);

  // Intentamos conectarnos. ¡El nombre "GsmClientTest" debe ser ÚNICO!
  // Si alguien más del grupo usa el mismo nombre, se desconectarán entre sí.
  boolean status = mqtt.connect("GsmClientTest_Unico");

  if (status == false) {
    SerialMon.println(" -> Falló");
    return false;
  }
  
  SerialMon.println(" -> ¡Éxito!");
  
  // Avisamos al mundo que acabamos de encender
  mqtt.publish(topicInit, "Dispositivo SIM7000G Iniciado");
  
  // Nos suscribimos al tópico para empezar a escuchar órdenes
  mqtt.subscribe(topicLed);
  return mqtt.connected();
}

// =============================================================
// SETUP: Configuración inicial (se ejecuta una sola vez)
// =============================================================
void setup() {
  // 1. Iniciamos la consola para la computadora
  SerialMon.begin(115200);
  delay(10);

  // 2. Configuramos el LED y lo encendemos (para saber que hay energía)
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  // 3. SECUENCIA DE ENCENDIDO DEL MÓDEM SIM7000G (Muy importante)
  // El módem necesita un pulso eléctrico como si presionáramos un botón de "Power"
  pinMode(PWR_PIN, OUTPUT);
  digitalWrite(PWR_PIN, HIGH); 
  delay(1000);                 // Mantenemos el "botón" presionado por 1 segundo
  digitalWrite(PWR_PIN, LOW);  // Soltamos el botón

  SerialMon.println("Esperando a que el módem inicie...");

  // 4. Iniciamos la comunicación serial con el módem
  // En la LilyGO T-SIM7000G, el RX está en el pin 26 y el TX en el 27
  SerialAT.begin(115200, SERIAL_8N1, 26, 27);
  delay(6000); // Damos tiempo para que el sistema operativo interno del módem arranque

  // 5. Reiniciamos y leemos la información del módem
  SerialMon.println("Inicializando módem celular...");
  modem.restart();
  
  String modemInfo = modem.getModemInfo();
  SerialMon.print("Info del Módem: ");
  SerialMon.println(modemInfo);

  // 6. BUSCANDO SEÑAL CELULAR (Antena)
  SerialMon.print("Buscando red celular...");
  if (!modem.waitForNetwork()) {
    SerialMon.println(" -> Falló. ¿Está conectada la antena LTE y hay chip insertado?");
    delay(10000);
    return;
  }
  SerialMon.println(" -> Red encontrada con éxito");

  // 7. CONECTANDO A DATOS MÓVILES (GPRS / APN)
  SerialMon.print(F("Conectando al APN: "));
  SerialMon.print(apn);
  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
    SerialMon.println(" -> Falló la conexión de datos. Revisa saldo y nombre del APN.");
    delay(10000);
    return;
  }
  SerialMon.println(" -> ¡GPRS Conectado! Ya tenemos internet.");

  // 8. Configuramos el servidor MQTT y la función que atenderá los mensajes
  mqtt.setServer(broker, 1883);
  mqtt.setCallback(mqttCallback);
}

// =============================================================
// LOOP: Ciclo infinito (se ejecuta para siempre)
// =============================================================
void loop() {
  
  // 1. VERIFICACIÓN DE RED: ¿Perdimos la señal de la antena?
  if (!modem.isNetworkConnected()) {
    SerialMon.println("Red celular desconectada. Intentando recuperar...");
    if (!modem.waitForNetwork(180000L, true)) { // Esperar hasta 3 minutos
      SerialMon.println(" -> No se pudo recuperar la red celular.");
      delay(10000);
      return;
    }
    SerialMon.println(" -> Red celular recuperada.");

    // Si recuperamos la red, debemos asegurarnos de que los datos (GPRS) sigan activos
    if (!modem.isGprsConnected()) {
      SerialMon.println("GPRS desconectado. Reconectando al APN...");
      if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
        SerialMon.println(" -> Falló la reconexión de datos.");
        delay(10000);
        return;
      }
      SerialMon.println(" -> GPRS reconectado.");
    }
  }

  // 2. VERIFICACIÓN MQTT: ¿Estamos conectados a la oficina postal?
  if (!mqtt.connected()) {
    SerialMon.println("=== MQTT NO CONECTADO ===");
    
    // Intentamos reconectar cada 10 segundos, no en cada milisegundo para no saturar
    uint32_t t = millis();
    if (t - lastReconnectAttempt > 10000L) {
      lastReconnectAttempt = t;
      if (mqttConnect()) {
        lastReconnectAttempt = 0; // Reiniciamos el contador si hay éxito
      }
    }
    delay(100);
    return; // Salimos del loop y volvemos a empezar
  }

  // 3. ATENDER TAREAS MQTT
  // Esta línea es crucial: le dice al ESP32 que revise si han llegado paquetes 
  // nuevos por internet y mantenga viva la conexión con el broker.
  mqtt.loop();
}
