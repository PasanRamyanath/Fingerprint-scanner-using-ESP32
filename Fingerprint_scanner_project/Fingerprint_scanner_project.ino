#include <Adafruit_Fingerprint.h>
#include <SPI.h>
#include <SD.h>

#define SD_CS 5
HardwareSerial mySerial(2);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

uint8_t id;

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  // Initialize SD card
  if (!SD.begin(SD_CS)) {
    Serial.println("SD card initialization failed!");
    while (1);
  }
  Serial.println("SD card initialized successfully!");

  // Initialize fingerprint sensor
  mySerial.begin(57600, SERIAL_8N1, 16, 17);
  finger.begin(57600);
  if (finger.verifyPassword()) {
    Serial.println("Found fingerprint sensor!");
  } else {
    Serial.println("Did not find fingerprint sensor :(");
    while (1);
  }

  // Ensure users.csv file exists
  if (!SD.exists("/users.csv")) {
    File csvFile = SD.open("/users.csv", FILE_WRITE);
    if (csvFile) {
      csvFile.println("ID,Name,DateTime");
      csvFile.close();
    }
  }
}

void loop() {
  Serial.println("Place your finger on the sensor to check...");
  while (!fingerprintMatch()) {
    delay(100);
  }

  if (isFingerprintSaved()) {
    Serial.println("Fingerprint already exists in the system.");
  } else {
    String name;
    Serial.println("Fingerprint not found. Enter a name for the new user:");
    while (name.isEmpty()) {
      if (Serial.available()) {
        name = Serial.readStringUntil('\n');
        name.trim();
      }
    }

    id = getNextID();
    Serial.println("Enrolling fingerprint...");
    if (!getFingerprintEnroll()) {
      Serial.println("Fingerprint enrollment failed!");
      return;
    }
    saveToCSV(id, name);
  }
}

bool fingerprintMatch() {
  int p = finger.getImage();
  if (p == FINGERPRINT_NOFINGER) {
    return false;
  }
  if (p == FINGERPRINT_OK) {
    return true;
  }
  Serial.println("Error in fingerprint matching!");
  return false;
}

bool isFingerprintSaved() {
  int p = finger.fingerSearch();
  if (p == FINGERPRINT_OK) {
    File csvFile = SD.open("/users.csv", FILE_READ);
    if (csvFile) {
      while (csvFile.available()) {
        String line = csvFile.readStringUntil('\n');
        int savedID = line.substring(0, line.indexOf(',')).toInt();
        if (savedID == finger.fingerID) {
          String name = line.substring(line.indexOf(',') + 1, line.lastIndexOf(','));
          Serial.println("Fingerprint belongs to: " + name);
          csvFile.close();
          return true;
        }
      }
      csvFile.close();
    }
  }
  return false;
}

uint8_t getNextID() {
  File csvFile = SD.open("/users.csv", FILE_READ);
  if (!csvFile) {
    return 1;
  }

  uint8_t lastID = 0;
  while (csvFile.available()) {
    String line = csvFile.readStringUntil('\n');
    if (line.indexOf(',') != -1) {
      lastID = line.substring(0, line.indexOf(',')).toInt();
    }
  }

  csvFile.close();
  return lastID + 1;
}

bool getFingerprintEnroll() {
  int p = -1;
  int retries = 3; // Allow 3 attempts for each step in the enrollment process

  // Capture Fingerprint Image
  while (retries--) {
    Serial.println("Place your finger on the sensor...");
    while ((p = finger.getImage()) != FINGERPRINT_OK) {
      if (p == FINGERPRINT_NOFINGER) {
        delay(100);
      } else if (p == FINGERPRINT_IMAGEFAIL) {
        Serial.println("Error capturing fingerprint. Please try again.");
        delay(1000);
      } else {
        Serial.println("Unknown error capturing fingerprint.");
        return false;
      }
    }
    Serial.println("Fingerprint image captured!");

    // Convert Image to Template
    p = finger.image2Tz(1);
    if (p == FINGERPRINT_OK) {
      Serial.println("Fingerprint image converted to template!");
      break;
    } else {
      Serial.println("Error converting fingerprint image to template. Retrying...");
      delay(1000);
    }
  }

  if (p != FINGERPRINT_OK) {
    Serial.println("Failed to capture and convert fingerprint image.");
    return false;
  }

  // Ask for a second scan
  retries = 5; // Increase retries for better chances
  Serial.println("Remove your finger and place it again for confirmation.");
  delay(2000); // Wait for the user to remove the finger

  while (retries--) {
    while ((p = finger.getImage()) != FINGERPRINT_OK) {
      if (p == FINGERPRINT_NOFINGER) {
        delay(100);
      } else if (p == FINGERPRINT_IMAGEFAIL) {
        Serial.println("Error capturing fingerprint. Please try again.");
        delay(1000);
      } else {
        Serial.println("Unknown error capturing fingerprint.");
        return false;
      }
    }
    Serial.println("Second fingerprint image captured!");

    p = finger.image2Tz(2);
    if (p == FINGERPRINT_OK) {
      Serial.println("Second fingerprint image converted to template!");
      break;
    } else if (p == FINGERPRINT_IMAGEMESS) {
      Serial.println("Second image is messy. Clean your finger and retry.");
    } else if (p == FINGERPRINT_IMAGEFAIL) {
      Serial.println("Error converting second fingerprint. Try again.");
    } else {
      Serial.println("Unknown error during second template creation.");
    }

    delay(1000);
  }

  if (p != FINGERPRINT_OK) {
    Serial.println("Failed to capture and convert the second fingerprint image.");
    return false;
  }

  // Create the fingerprint model
  p = finger.createModel();
  if (p == FINGERPRINT_OK) {
    Serial.println("Fingerprint model created successfully!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Error receiving data during model creation.");
    return false;
  } else if (p == FINGERPRINT_ENROLLMISMATCH) {
    Serial.println("Fingerprint scans do not match. Enrollment failed.");
    return false;
  } else {
    Serial.println("Unknown error during model creation.");
    return false;
  }

  // Store the fingerprint model
  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    Serial.println("Fingerprint stored successfully!");
    return true;
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Error receiving data during storage.");
  } else if (p == FINGERPRINT_BADLOCATION) {
    Serial.println("Invalid storage location.");
  } else if (p == FINGERPRINT_FLASHERR) {
    Serial.println("Error writing to flash memory.");
  } else {
    Serial.println("Unknown error during fingerprint storage.");
  }

  return false;
}

void saveToCSV(uint8_t id, String name) {
  File csvFile = SD.open("/users.csv", FILE_WRITE);
  if (csvFile) {
    csvFile.seek(csvFile.size());
    String dateTime = "2025-01-21 10:00:00"; 
    csvFile.print(id);
    csvFile.print(",");
    csvFile.print(name);
    csvFile.print(",");
    csvFile.println(dateTime);
    csvFile.close();
    Serial.println("User profile saved to users.csv!");
  } else {
    Serial.println("Failed to open users.csv for writing!");
  }
}
