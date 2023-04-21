#include <WiFi.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ESP32Servo.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>

#define RST_PIN         22                 // Configurable, see typical pin layout
#define SS_PIN          5                  // Configurable, see typical pin layout
#define SERVO_PIN       13                 // Servo pin

const int greenLedPin = 14;
const int redLedPin = 26;
const int BUZZER_PIN = 12;

#define NEW_UID {0x03, 0x31, 0x1c, 0x1c}          // New print UID for access when masterkey is scanned
#define WIFI_SSID "Bard_iPhone_14_Pro_Max_Rich"   
#define WIFI_PASSWORD "sopphaug"                  
#define MQTT_BROKER "172.20.10.3"                 // IP address of the MQTT broker (Raspberry Pi 4 ip adress in this case)
#define MQTT_PORT 1883                            // Standard port
#define MQTT_TOPIC "rfid/read"                    // Subscribe to this topic

WiFiClient espClient;
PubSubClient client(espClient);
Servo myServo;                      // Define the servo
LiquidCrystal_I2C lcd (0x27, 16,2); // Create the lcd object address 0x3F and 16 col
MFRC522 mfrc522(SS_PIN, RST_PIN);   // Create MFRC522 instance

void setupWiFi() {
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");              // Prints "." until wifi connected
  }
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void setupMQTT() {
  client.setServer(MQTT_BROKER, MQTT_PORT);
  Serial.print("Attempting MQTT connection...");
}

bool keyScanned = false; // initialize flag to false

void setup() { 
  // LCD screen
  lcd.begin ();
  lcd.backlight ();
  lcd.print ( "Place chip here" );
  Serial.begin(115200);
  while (!Serial);
  SPI.begin();
  // Rfid Module
  mfrc522.PCD_Init();
  delay(4);
  mfrc522.PCD_DumpVersionToSerial();
  Serial.println(F("Scan PICC to see UID, SAK, type, and data blocks..."));
  setupWiFi();
  setupMQTT();
  // Servo
  myServo.setPeriodHertz(50);             // Standard 50 Hz servo
  myServo.attach(SERVO_PIN, 1000, 2000);  // Attaches the servo on pin 13 to the servo object
  // Leds
  pinMode(greenLedPin, OUTPUT);
  pinMode(redLedPin, OUTPUT);
}

void loop() {
    if (WiFi.status() != WL_CONNECTED) {
    setupWiFi();
  }
  client.loop();
     if (!client.connected()) {
      Serial.print("Attempting MQTT connection...");
        if (client.connect("ESP32Client")) {
        Serial.println("connected");
    } else {
      Serial.print("failed");
      Serial.print(client.state());
      Serial.println("try again in 5 seconds");
      delay(3000);
    }
}
  // Reset the loop if no new card present on the sensor/reader. This saves the entire process when idle.
  if (!mfrc522.PICC_IsNewCardPresent()) {
    return;
  }
  // Select one of the cards
  if (!mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  String payload = "";      // Convert the UID to a string
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    payload += String(mfrc522.uid.uidByte[i] < 0x10 ? "0" : "");
    payload += String(mfrc522.uid.uidByte[i], HEX);
  }
 if (!client.connected()) {
    setupMQTT();
  }
    client.publish(MQTT_TOPIC, payload.c_str(), true);
    delay(100);
    Serial.print("Scan complete and published: ");
  // Add this line to print the UID to the serial monitor
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    Serial.println("RFID UID: " + payload);
  
  }
 // Declare writeModeStartTime variable
  unsigned long writeModeStartTime;
// Check if the scanned UID is 03311c1c
  if (payload == "03311c1c") {
    lcd.clear();
    lcd.print ("Access permitted");
    digitalWrite(greenLedPin, HIGH);    // Rest of your existing code for access permitted
    myServo.write(0);  // Set the servo to 90 degrees
    delay(1000);
    myServo.write(180);// Set the servo back to original position
    delay(1000);
    lcd.clear();
    lcd.print("Place chip here");

    }else if (payload == "7df42704") {
      Serial.println(F("Master key detected. Entering overwrite mode..."));
        // Blink green and red LEDs 3 times
    for (int i = 0; i < 3; i++) {
      digitalWrite(greenLedPin, HIGH);
      digitalWrite(redLedPin, HIGH);
      delay(250);
      digitalWrite(greenLedPin, LOW);
      digitalWrite(redLedPin, LOW);
      delay(250);
      }    
      lcd.clear();
      lcd.print( "Write mode" );
      delay(2000);
      for (int i = 5; i > 0; i--) {
      lcd.clear();
      lcd.print("Countdown! - ");
      lcd.print(i);
      delay(1000);
    }
      writeModeStartTime = millis();
      for (int i= 0; i<1; i++) {
        // Overwrite the UID
        delay(2000);
        byte newUid[] = NEW_UID;
        if ( mfrc522.MIFARE_SetUid(newUid, (byte)4, true) ) {
          Serial.println(F("Wrote new UID to card."));
          lcd.clear();
          lcd.print( "Write success");
          keyScanned = true; // set flag to true to indicate the master key has been scanned and the UID has been rewritten
          break;             // exit the while loop
        }
        lcd.clear();
        lcd.print( "Place chip here");
        delay(3000);
          // Reset the loop if no new card present on the sensor/reader. This saves the entire process when idle.
        if (keyScanned)
          break;
        }
        keyScanned = false;
    }
    else{
    lcd.clear();
    lcd.print ( "Access denied" );
    digitalWrite(redLedPin, HIGH);      // Rest of your existing code for access denied
    delay(2000);
    lcd.clear();
    lcd.print( "Place chip here" );
    digitalWrite(redLedPin, LOW);
    }
  Serial.println("RFID UID: " + payload);
  digitalWrite(greenLedPin, LOW);
  digitalWrite(redLedPin, LOW);
  delay (1000);
}
