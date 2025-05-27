#include <Arduino.h>
#include <HardwareSerial.h>
#include <Wire.h>
#include <TinyGsmClient.h>


#define MODEM_RST        5
#define MODEM_PWRKEY     4
#define MODEM_POWER_ON   23
#define MODEM_TX         27
#define MODEM_RX         26
#define SerialMon Serial
#define SerialAT  Serial2
#define LED              13

TinyGsm modem(SerialAT);

void powerOnModem() {
  pinMode(MODEM_PWRKEY, OUTPUT);
  pinMode(MODEM_POWER_ON, OUTPUT);
  pinMode(MODEM_RST, OUTPUT);
  digitalWrite(MODEM_RST, HIGH);
  digitalWrite(MODEM_POWER_ON, HIGH);
  delay(100);
  digitalWrite(MODEM_PWRKEY, LOW);
  delay(1100);
  digitalWrite(MODEM_PWRKEY, HIGH);

}

void setup() {
  pinMode(LED,OUTPUT);
  digitalWrite(LED,HIGH);

  SerialMon.begin(115200);
  delay(3000);

  SerialMon.println("Powering on SIM800L...");
  powerOnModem();
  delay(3000);

  SerialMon.println(" Starting SerialAT...");
  SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(3000);

}

void loop() {
  // Display responses
  while (SerialAT.available()) {
    SerialMon.write(SerialAT.read());
  }

  // Manual passthrough
  while (SerialMon.available()) {
    SerialAT.write(SerialMon.read());
  }
}
