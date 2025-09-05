#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_Fingerprint.h>
#include <HardwareSerial.h>
#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <WebServer.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Fingerprint Sensor
HardwareSerial mySerial(1); // GPIO16, GPIO17
Adafruit_Fingerprint finger(&mySerial);
#define WAKE_PIN 25

// RFID
#define SS_PIN 5
#define RST_PIN 4
MFRC522 rfid(SS_PIN, RST_PIN);

// WiFi
const char* ssid = "1234";
const char* password = "123456789";
WebServer server(80);

// NTP for IST
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 19800, 60000); // IST = UTC+5:30 (19800 sec)

// UIDs
byte ADITYA_UID[] = {0x76, 0x59, 0x6B, 0x26};
byte DILAN_UID[]  = {0x66, 0xCF, 0xA9, 0x26};
byte VARUN_UID[]  = {0x16, 0x40, 0x96, 0x26};
byte SACHIN_UID[] = {0xF3, 0xBD, 0x8F, 0x2A};

const char* subjects[] = {"CONTROL SYSTEM", "MACHINES", "POWER SYSTEM", "MATHS"};
const char* examDates[] = {"20-03-2025", "21-03-2025", "22-03-2025", "23-03-2025"};
int currentExamIndex = 0;
int scannedCount = 0;

struct Student {
  const char* name;
  const char* usn;
  bool eligibility[4];
};
Student students[] = {
  {"ADITYA", "1DA22EE002", {true, false, true, false}},  // ID 1
  {"DILAN",  "1DA22EE010", {false, false, true, true}},  // ID 2
  {"VARUN",  "1DA22EE058", {false, true, false, true}},  // ID 3
  {"SACHIN", "1DA22EE039", {true, true, false, false}}   // ID 4
};

struct Entry {
  String name, usn, subject, eligible, date, time;
};
Entry logs[50];
int logIndex = 0;

// ===== Web Page HTML =====
String generateHTML() {
  String html = "<html><head><title>EXAM ENTRY SYSTEM</title>";
  html += "<meta http-equiv='refresh' content='10'>";
  html += "<style>body{font-family:Arial;background:white;}table{border-collapse:collapse;width:100%;}td,th{border:1px solid #000;padding:8px;text-align:center;}th{background:#f0f0f0;}</style>";
  html += "</head><body><h2>EXAM ENTRY SYSTEM</h2>";
  html += "<table><tr><th>S.No.</th><th>Name</th><th>USN</th><th>Subject</th><th>Eligibility</th><th>Date</th><th>Time</th></tr>";

  for (int i = 0; i < logIndex; i++) {
    html += "<tr><td>" + String(i+1) + "</td><td>" + logs[i].name + "</td><td>" + logs[i].usn + "</td><td>" + logs[i].subject + "</td><td>" + logs[i].eligible + "</td><td>" + logs[i].date + "</td><td>" + logs[i].time + "</td></tr>";
  }

  html += "</table><br><button onclick='window.print()'>Export to PDF</button></body></html>";
  return html;
}

// ===== Setup =====
void setup() {
  Serial.begin(115200);
  pinMode(WAKE_PIN, OUTPUT);
  digitalWrite(WAKE_PIN, HIGH);

  lcd.begin();
  lcd.backlight();
  lcd.clear();
  lcd.print("Initializing...");

  SPI.begin();
  rfid.PCD_Init();
  mySerial.begin(57600, SERIAL_8N1, 16, 17);
  finger.begin(57600);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected. IP: " + WiFi.localIP().toString());

  timeClient.begin();
  timeClient.update();

  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Date:");
  lcd.setCursor(6, 0); lcd.print(examDates[currentExamIndex]);
  lcd.setCursor(0, 1); lcd.print("Subject:");
  lcd.setCursor(9, 1); lcd.print(subjects[currentExamIndex]);
  delay(3000);
  lcd.clear(); lcd.print("Scan your card");

  server.on("/", []() {
    server.send(200, "text/html", generateHTML());
  });
  server.begin();
}

// ===== Main Loop =====
void loop() {
  server.handleClient();
  timeClient.update();

  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) return;

  int studentIndex = matchStudent(rfid.uid.uidByte);
  if (studentIndex != -1) {
    lcd.clear(); lcd.print("Place Finger");

    unsigned long timeout = millis() + 8000;
    int fingerID = -1;
    while (millis() < timeout) {
      fingerID = getFingerprintID();
      if (fingerID > 0) break;
      delay(200);
    }

    if (fingerID == studentIndex + 1) {
      showResult(studentIndex);
    } else {
      lcd.clear(); lcd.print("FP Not Match");
      delay(1500);
    }
  } else {
    lcd.clear(); lcd.print("Access Denied");
    delay(1500);
  }

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();

  scannedCount++;
  if (scannedCount == 4) {
    currentExamIndex = (currentExamIndex + 1) % 4;
    scannedCount = 0;
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("Date:");
    lcd.setCursor(6, 0); lcd.print(examDates[currentExamIndex]);
    lcd.setCursor(0, 1); lcd.print("Subject:");
    lcd.setCursor(9, 1); lcd.print(subjects[currentExamIndex]);
    delay(3000);
  }

  lcd.clear(); lcd.print("Scan your card");
}

// ===== Helper Functions =====
int matchStudent(byte *uid) {
  if (compareUID(uid, ADITYA_UID)) return 0;
  if (compareUID(uid, DILAN_UID))  return 1;
  if (compareUID(uid, VARUN_UID))  return 5;
  if (compareUID(uid, SACHIN_UID)) return 3;
  return -1;
}

bool compareUID(byte* uid1, byte* uid2) {
  for (byte i = 0; i < 4; i++) {
    if (uid1[i] != uid2[i]) return false;
  }
  return true;
}

int getFingerprintID() {
  digitalWrite(WAKE_PIN, HIGH);
  if (finger.getImage() != FINGERPRINT_OK) return -1;
  if (finger.image2Tz() != FINGERPRINT_OK) return -1;
  if (finger.fingerSearch() == FINGERPRINT_OK) {
    return finger.fingerID;
  }
  return 0;
}

void showResult(int idx) {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Name:");
  lcd.setCursor(6, 0); lcd.print(students[idx].name);
  lcd.setCursor(0, 1); lcd.print("USN:");
  lcd.setCursor(5, 1); lcd.print(students[idx].usn);
  delay(2000);

  lcd.clear();
  lcd.setCursor(0, 0); lcd.print(subjects[currentExamIndex]);
  lcd.setCursor(0, 1);
  if (students[idx].eligibility[currentExamIndex]) {
    lcd.print("Eligible");
  } else {
    lcd.print("Not Eligible");
  }
  delay(2000);

  if (logIndex < 50) {
    logs[logIndex++] = {
      students[idx].name,
      students[idx].usn,
      subjects[currentExamIndex],
      students[idx].eligibility[currentExamIndex] ? "Eligible" : "Not Eligible",
      examDates[currentExamIndex],
      timeClient.getFormattedTime()
    };
  }
}
