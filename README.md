# 📡 Guía de Inicio: Comunicación IoT con LilyGO T-SIM7000G

¡Bienvenidos al grupo de maestría! Este documento está diseñado para entender a fondo cómo funciona nuestro código de conexión a internet usando la placa **LilyGO T-SIM7000G**, la cual combina un microcontrolador ESP32 y un módulo de comunicación celular SIM7000G. 

Para hacerlo más digerible, utilizaremos analogías cotidianas que nos ayudarán a entender conceptos abstractos de redes e IoT.

---

## 1. Arquitectura de Hardware: El Ingeniero y el Chofer

Es vital entender que la LilyGO T-SIM7000G **no es un solo cerebro**, sino dos componentes distintos viviendo en la misma placa de circuito impreso (PCB).

*   **ESP32 (El Ingeniero):** Es el cerebro principal. Ejecuta nuestro código, lee sensores, toma decisiones y enciende LEDs. Sin embargo, *no sabe cómo conectarse a una red celular*.
*   **SIM7000G (El Chofer / Mensajero):** Es un radio-módem especializado. Sabe todo sobre antenas, chips SIM, torres celulares y señal, pero *no tiene iniciativa propia*; solo hace lo que el ESP32 le ordena.

### 🔌 ¿Cómo se conectan entre ellos? (Esquema de Pines)

El Ingeniero y el Chofer están sentados en habitaciones separadas y se comunican a través de un "teléfono interno" de dos cables (Comunicación Serial o UART). Además, el Ingeniero tiene un "botón" para despertar o apagar al Chofer.

```text
  [ ESP32 (Microcontrolador) ]                       [ SIM7000G (Módem Celular) ]
          (El Ingeniero)                                    (El Chofer)
                 |                                               |
  Pin 26 (TX)    |------- Transmite Datos / Comandos AT ------>  | (RX)
                 |                                               |
  Pin 27 (RX)    |<------ Recibe Respuestas / Datos -----------  | (TX)
                 |                                               |
  Pin 4 (PWRKEY) |------- Señal de Encendido (Pulso de 1s) --->  | (Botón Power)
                 |                                               |
  Pin 12 (LED)   |------- (Indicador Visual del ESP32)           |
