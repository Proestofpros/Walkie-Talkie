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
#define BUTTON_ANSWER 14  
#define BUTTON_HANGUP 15  


// Display setup
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Modem
TinyGsm modem(SerialAT);

// Timing
unsigned long lastUpdate = 0;
const unsigned long updateInterval = 5000;

// Power on SIM800L
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

// Manual SIM status check
bool isSimReady() {
  SerialAT.println("AT+CPIN?");
  delay(500);
  String resp = "";
  while (SerialAT.available()) {
    resp += (char)SerialAT.read();
  }
  SerialMon.print("AT+CPIN? Response: ");
  SerialMon.println(resp);
  return resp.indexOf("READY") >= 0;
}

// Manual network registration check
bool isRegistered() {
  SerialAT.println("AT+CREG?");
  delay(500);
  String resp = "";
  while (SerialAT.available()) {
    resp += (char)SerialAT.read();
  }
  SerialMon.print("AT+CREG? Response: ");
  SerialMon.println(resp);
  return (resp.indexOf("0,1") >= 0 || resp.indexOf("0,5") >= 0);
}

// Get network mode (reuse AT+CNSMOD? parser)
String getNetworkModeText() {
  bool reg = isRegistered();
  if (reg) return "2G";
  else return "No Service";
}

// Get battery level (reuse AT+CBC parser)
int getBatteryLevel() {
  SerialAT.println("AT+CBC");
  delay(500);
  String resp = "";
  while (SerialAT.available()) {
    resp += (char)SerialAT.read();
  }
  SerialMon.print("AT+CBC Response: ");
  SerialMon.println(resp);
  if (resp.indexOf("+CBC:") >= 0) {
    int idx1 = resp.indexOf(",");
    int idx2 = resp.indexOf(",", idx1 + 1);
    if (idx1 > 0 && idx2 > idx1) {
      return resp.substring(idx1 + 1, idx2).toInt();
    }
  }
  return -1;
}

void setup() {
  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);
  SerialMon.begin(115200);
  delay(3000);

  Wire.begin(21, 22);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    SerialMon.println(F("OLED init failed"));
    while (true);
  }
  pinMode(BUTTON_ANSWER, INPUT_PULLUP);
  pinMode(BUTTON_HANGUP, INPUT_PULLUP);


  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("Booting...");
  display.display();

  powerOnModem();
  delay(3000);

  SerialMon.println("Starting SerialAT...");
  SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(3000);

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
  // Check for button presses
  if (digitalRead(BUTTON_ANSWER) == LOW) {  // Answer button pressed (active low)
    delay(200);  // debounce
    SerialMon.println("Answering call...");
    SerialAT.println("ATA");          // Answer the call
    SerialAT.println("AT+CHFA=1");   // Route audio to speakerphone
    SerialAT.println("AT+CMIC=0,15"); // Set microphone gain max
  }

  if (digitalRead(BUTTON_HANGUP) == LOW) {  // Hangup button pressed (active low)
    delay(200);  // debounce
    SerialMon.println("Hanging up...");
    SerialAT.println("ATH");  // Hang up the call
  }


  // OLED update every 5 sec
  if (millis() - lastUpdate > updateInterval) {
    lastUpdate = millis();

    int rssi = modem.getSignalQuality();
    String simStatus = isSimReady() ? "READY" : "NOT READY";
    String operatorName = modem.getOperator();
    String netMode = getNetworkModeText();
    bool reg = isRegistered();
    int batt = getBatteryLevel();

    int bars = map(rssi, 0, 31, 0, 5);
    bars = constrain(bars, 0, 5);

    display.clearDisplay();

    // Signal bars (top-left)
    for (int i = 0; i < 5; i++) {
      int h = (i < bars) ? (2 + i * 2) : 1;
      int y = 10 - h;
      display.fillRect(2 + i * 6, y, 4, h, WHITE);
    }

    // Battery icon (top-right)
    int battX = 100, battY = 0, battH = 10, battW = 20;
    display.drawRect(battX, battY, battW, battH, WHITE);
    display.fillRect(battX + battW, battY + 3, 2, 4, WHITE);
    int battBars = map(batt, 0, 100, 0, battW - 2);
    display.fillRect(battX + 1, battY + 1, battBars, battH - 2, WHITE);

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

    display.display();
  }
}
