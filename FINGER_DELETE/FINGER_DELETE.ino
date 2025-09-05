#include <Adafruit_Fingerprint.h>
#include <HardwareSerial.h>

// UART2 on ESP32: GPIO16 (RX2), GPIO17 (TX2)
HardwareSerial mySerial(1);
Adafruit_Fingerprint finger(&mySerial);

// WAKE pin on R307S connected to GPIO25
#define WAKE_PIN 25

void setup() {
  Serial.begin(115200);
  delay(1000);

  // WAKE pin setup
  pinMode(WAKE_PIN, OUTPUT);
  digitalWrite(WAKE_PIN, HIGH);

  Serial.println("Initializing R307S Fingerprint Sensor...");

  // Start UART for sensor
  mySerial.begin(57600, SERIAL_8N1, 16, 17);
  finger.begin(57600);

  if (finger.verifyPassword()) {
    Serial.println("✅ Sensor connected successfully.");
  } else {
    Serial.println("❌ ERROR: Cannot communicate with sensor.");
    while (1) delay(1);
  }

  // Show initial template count
  finger.getTemplateCount();
  Serial.print("Sensor contains ");
  Serial.print(finger.templateCount);
  Serial.println(" fingerprint(s).");

  Serial.println("\nType 'd' to delete a fingerprint");
  Serial.println("Type 'c' to count stored fingerprints");
}

void loop() {
  // Serial command interface
  if (Serial.available()) {
    char command = Serial.read();

    if (command == 'd') {
      Serial.println("Enter ID to delete (1–127): ");
      while (!Serial.available());  // wait for input
      int id = Serial.parseInt();   // read the number
      if (id < 1 || id > 127) {
        Serial.println("❌ Invalid ID. Must be between 1 and 127.");
      } else {
        deleteFingerprint(id);
      }
    }

    else if (command == 'c') {
      finger.getTemplateCount();
      Serial.print("Sensor contains ");
      Serial.print(finger.templateCount);
      Serial.println(" fingerprint(s).");
    }

    // Clear input buffer
    while (Serial.available()) Serial.read();
  }

  delay(200);  // Prevent flooding
}

void deleteFingerprint(uint8_t id) {
  int p = finger.deleteModel(id);

  switch (p) {
    case FINGERPRINT_OK:
      Serial.print("✅ Fingerprint ID #");
      Serial.print(id);
      Serial.println(" deleted successfully.");
      break;

    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("❌ Communication error.");
      break;

    case FINGERPRINT_BADLOCATION:
      Serial.println("❌ Invalid ID or fingerprint not found at that location.");
      break;

    case FINGERPRINT_FLASHERR:
      Serial.println("❌ Flash write error while deleting.");
      break;

    default:
      Serial.print("❌ Unknown error code: ");
      Serial.println(p);
      break;
  }
}
