#include <Arduino.h>
#include <HardwareSerial.h>
#include <Wire.h>
#include <TinyGsmClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Pin definitions
#define MODEM_RST        5
#define MODEM_PWRKEY     4
#define MODEM_POWER_ON   23
#define MODEM_TX         27
#define MODEM_RX         26
#define SerialMon Serial
#define SerialAT  Serial2
#define LED              13

// Display setup
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Modem
TinyGsm modem(SerialAT);

// Timing
unsigned long lastUpdate = 0;
const unsigned long updateInterval = 500;

// Power on sequence for SIM800L
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

// --- AT fallback function: get network mode ---
String getNetworkModeText() {
  SerialAT.println("AT+CNSMOD?");
  delay(100);
  while (SerialAT.available()) {
    String res = SerialAT.readStringUntil('\n');
    if (res.indexOf("+CNSMOD:") >= 0) {
      int modeVal = res.charAt(res.length() - 2) - '0';
      if (modeVal == 0) return "NoSrv";
      else if (modeVal == 1) return "2G";
      else if (modeVal == 3) return "3G";
      else if (modeVal == 7) return "4G";
    }
  }
  return "Unknown";
}

// --- AT fallback function: get battery level ---
int getBatteryLevel() {
  SerialAT.println("AT+CBC");
  delay(100);
  while (SerialAT.available()) {
    String res = SerialAT.readStringUntil('\n');
    if (res.indexOf("+CBC:") >= 0) {
      int idx1 = res.indexOf(",");
      int idx2 = res.indexOf(",", idx1 + 1);
      if (idx1 > 0 && idx2 > idx1) {
        return res.substring(idx1 + 1, idx2).toInt();
      }
    }
  }
  return -1;
}

void setup() {
  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);
  SerialMon.begin(115200);
  delay(3000);

  Wire.begin(21, 22);  // SDA, SCL
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    SerialMon.println(F("OLED init failed"));
    while (true);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("Booting...");
  display.display();

  powerOnModem();
  delay(300);

  SerialMon.println("Starting SerialAT...");
  SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(300);

  modem.restart();
  SerialMon.println("Modem restarted");
}

void loop() {
  // Serial passthrough
  while (SerialAT.available()) {
    SerialMon.write(SerialAT.read());
  }
  while (SerialMon.available()) {
    SerialAT.write(SerialMon.read());
  }

  // OLED update
  if (millis() - lastUpdate > updateInterval) {
    lastUpdate = millis();

    int rssi = modem.getSignalQuality(); // 0–31
    String simStatus = modem.getSimStatus() == 3 ? "READY" : "NOT READY";
    String operatorName = modem.getOperator();
    String netMode = getNetworkModeText();
    bool reg = modem.isNetworkConnected();
    int batt = getBatteryLevel();

    int bars = map(rssi, 0, 31, 0, 5);
    bars = constrain(bars, 0, 5);
display.clearDisplay();

// ────── Signal Bars (top-left)
for (int i = 0; i < 5; i++) {
  int h = (i < bars) ? (2 + i * 2) : 1;
  int y = 10 - h;
  display.fillRect(2 + i * 6, y, 4, h, WHITE);
}

// ────── Battery Icon (top-right)
int battX = 100, battY = 0, battH = 10, battW = 20;
display.drawRect(battX, battY, battW, battH, WHITE);
display.fillRect(battX + battW, battY + 3, 2, 4, WHITE);
int battBars = map(batt, 0, 100, 0, battW - 2);
display.fillRect(battX + 1, battY + 1, battBars, battH - 2, WHITE);

// ────── Text Info (below)
display.setTextSize(1);
display.setTextColor(WHITE);
display.setCursor(0, 12);
display.print("RSSI: ");
display.print(rssi);
display.println("/31");

display.setCursor(0, 22);
display.print("SIM: ");
display.println(simStatus);

display.setCursor(0, 32);
display.print("Operator: ");
display.println(operatorName);

display.setCursor(0, 42);
display.print("Net: ");
display.print(netMode);
display.print(" | ");
display.print(reg ? "OK" : "Fail");

display.setCursor(0, 55);
display.print("Idle");

display.display();
  }
}
