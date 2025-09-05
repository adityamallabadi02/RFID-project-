#include <Adafruit_Fingerprint.h>
#include <HardwareSerial.h>

// UART2: GPIO16 = RX, GPIO17 = TX
HardwareSerial mySerial(1);
Adafruit_Fingerprint finger(&mySerial);

// WAKE pin connected to R307S pin 5
#define WAKE_PIN 25

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(WAKE_PIN, OUTPUT);
  digitalWrite(WAKE_PIN, HIGH);  // Keep sensor awake

  Serial.println("Initializing R307S Fingerprint Sensor...");

  // Start UART on pins 16 (RX) and 17 (TX)
  mySerial.begin(57600, SERIAL_8N1, 16, 17);

  // Start fingerprint sensor
  finger.begin(57600);

  // Verify communication
  if (finger.verifyPassword()) {
    Serial.println("✅ Sensor communication established.");
  } else {
    Serial.println("❌ ERROR: Could not connect to fingerprint sensor.");
    while (1) delay(1);
  }

  // Show number of stored fingerprints
  finger.getTemplateCount();
  Serial.print("Sensor contains "); Serial.print(finger.templateCount); Serial.println(" fingerprint(s).");

  // Enroll fingerprints 1 to 5
  for (int id = 1; id <= 5; id++) {
    Serial.print("\n--- Enrolling Fingerprint ID "); Serial.print(id); Serial.println(" ---");
    enrollFingerprint(id);
    delay(2000);
  }

  Serial.println("\nAll 5 fingerprints enrolled. Now entering verification mode...");
}

void loop() {
  // Check for fingerprint match
  int id = getFingerprintID();
  if (id > 0) {
    Serial.print("✅ Fingerprint matched with ID #"); Serial.println(id);
  } else if (id == 0) {
    Serial.println("❌ No match found.");
  }

  delay(2000);  // Small delay before scanning again
}

void enrollFingerprint(uint8_t id) {
  int p = -1;

  // First scan
  Serial.println("Place your finger on the sensor...");
  while ((p = finger.getImage()) != FINGERPRINT_OK) {
    delay(100);
  }

  p = finger.image2Tz(1);
  if (p != FINGERPRINT_OK) {
    Serial.println("❌ Couldn't convert first image.");
    return;
  }

  Serial.println("Remove finger...");
  delay(2000);
  while (finger.getImage() != FINGERPRINT_NOFINGER) {
    delay(100);
  }

  // Second scan
  Serial.println("Place the same finger again...");
  while ((p = finger.getImage()) != FINGERPRINT_OK) {
    delay(100);
  }

  p = finger.image2Tz(2);
  if (p != FINGERPRINT_OK) {
    Serial.println("❌ Couldn't convert second image.");
    return;
  }

  // Create model and store
  p = finger.createModel();
  if (p != FINGERPRINT_OK) {
    Serial.println("❌ Fingerprints do not match.");
    return;
  }

  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    Serial.println("✅ Fingerprint stored successfully!");
  } else {
    Serial.println("❌ Failed to store fingerprint.");
  }
}

// Function to check fingerprint and return ID
int getFingerprintID() {
  digitalWrite(WAKE_PIN, HIGH);  // Ensure sensor is awake

  int p = finger.getImage();
  if (p != FINGERPRINT_OK) return -1;

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) return -1;

  p = finger.fingerSearch();
  if (p == FINGERPRINT_OK) {
    return finger.fingerID;  // Matched ID
  } else {
    return 0;  // No match
  }
}
