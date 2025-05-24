#include <ArduinoMqttClient.h>
#include "Firebase_Arduino_WiFiNINA.h"
#include <WiFiNINA.h>
#include "secrets.h"
#include <MFRC522.h>
#include <Servo.h>

///////please enter your sensitive data in the Secret tab/arduino_secrets.h
char ssid[] = SECRET_SSID;  // your network SSID (name)
char pass[] = SECRET_PASS;  // your network password (use for WPA, or use as key for WEP)

#define LED 7
#define SS_PIN 10
#define RST_PIN 9
#define Buzzer 8

String getPath = "/Gate";
bool GateOpened = false;
bool GateClosed = false;

MFRC522 rfid(SS_PIN, RST_PIN);  // Instance of the class

// Init array that will store new NUID
byte nuidPICC[4];

unsigned long tagID = 0;
int buzzer = Buzzer;
String IDStatus = "";
String IDStatus1 = "";
int pos = 0;

Servo myservo;

WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

const char broker[] = "broker.emqx.io";
int port = 1883;
const char Topic[] = "SIT210/wave11";
const char Topic2[] = "SIT210/wave6";

unsigned long lastCheck = 0;
const unsigned long CheckInterval = 4000;

int count = 0;

FirebaseData fbdo;

void setup() {
  //Initialize serial and wait for port to open:
  Serial.begin(9600);

  pinMode(LED, OUTPUT);
  pinMode(buzzer, OUTPUT);
  myservo.attach(3);

  SPI.begin();      // Init SPI bus
  rfid.PCD_Init();  // Init MFRC522

  Serial.println(F("This code scan the MIFARE Classsic NUID."));

  CheckWIFI();

  //Provide the autntication data
  Firebase.begin(DATABASE_URL, DATABASE_SECRET, SECRET_SSID, SECRET_PASS);
  Firebase.reconnectWiFi(true);

  Serial.print("Attempting to connect to the MQTT broker: ");
  Serial.println(broker);

  while (!mqttClient.connect(broker, port)) {
    Serial.print("MQTT connection failed! Error code = ");
    Serial.println(mqttClient.connectError());
    delay(2000);
    Serial.println("Retrying MQTT...");
  }

  mqttClient.subscribe(Topic2);

  Serial.println("You're connected to the MQTT broker!");
  Serial.println();

  if (WDT->CTRL.reg & WDT_CTRL_ENABLE) {
    WDT->CTRL.reg &= ~WDT_CTRL_ENABLE;
    Serial.println("Watchdog Timer is disabled.");
  }
}

void loop() {
  mqttClient.poll();

  CheckRapberryPi();

  // Reset the loop if no new card present on the sensor/reader. This saves the entire process when idle.
  if (!rfid.PICC_IsNewCardPresent())
    return;

  // Verify if the NUID has been readed
  if (!rfid.PICC_ReadCardSerial())
    return;

  MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);

  // Check is the PICC of Classic MIFARE type
  if (piccType != MFRC522::PICC_TYPE_MIFARE_MINI && piccType != MFRC522::PICC_TYPE_MIFARE_1K && piccType != MFRC522::PICC_TYPE_MIFARE_4K) {
    Serial.println(F("Your tag is not of type MIFARE Classic."));
    return;
  }

  if (rfid.uid.uidByte[0] != nuidPICC[0] || rfid.uid.uidByte[1] != nuidPICC[1] || rfid.uid.uidByte[2] != nuidPICC[2] || rfid.uid.uidByte[3] != nuidPICC[3]) {
    Serial.println(F("New ID detected."));

    // Store NUID into nuidPICC array
    for (byte i = 0; i < 4; i++) {
      nuidPICC[i] = rfid.uid.uidByte[i];
    }

    Serial.println(F("The NUID tag is:"));
    tagID = printDec(rfid.uid.uidByte, rfid.uid.size);
    Serial.println(tagID);

    Serial.print("Sending message to topic: ");
    Serial.println(Topic);
    Serial.println(tagID);

    SendMsg();
    Serial.println();
    VerifiedMsg();
    GateOpen();

  } else {
    Serial.println(F("Card already read"));
    tone(buzzer, 1200, 50);
    delay(60);
    tone(buzzer, 900, 60);
    delay(70);
    noTone(buzzer);
  }
  // Halt PICC
  rfid.PICC_HaltA();

  // Stop encryption on PCD
  rfid.PCD_StopCrypto1();
}

unsigned long printDec(byte buffer[], byte bufferSize) {
  unsigned long result = 0;
  for (byte i = 0; i < bufferSize; i++) {
    result = (result << 8) | buffer[i];
  }
  return result;
}

void SendMsg() {
  // send message, the Print interface can be used to set the message contents
  mqttClient.beginMessage(Topic);
  mqttClient.print(tagID);
  mqttClient.endMessage();

  tone(buzzer, 800);
  delay(80);
  tone(buzzer, 1000);
  delay(70);
  tone(buzzer, 1300);
  delay(60);
  tone(buzzer, 1700);
  delay(50);

  noTone(buzzer);
  delay(500);
}

void VerifiedMsg() {
  Serial.println("Wait for response...");

  bool received_status = false;
  unsigned long start = millis();

  while (received_status == false) {

    mqttClient.poll();

    int messageSize = 0;

    while (true) {
      messageSize = mqttClient.parseMessage();

      if (messageSize != 0) {
        break;
      }

      if (millis() - start > 12000) {
        Serial.println("Server not found");
        return;
      }
    }

    if (messageSize) {
      // we received a message, print out the topic and contents
      Serial.print("Status: ");

      // use the Stream interface to print the contents
      while (mqttClient.available()) {
        char message = mqttClient.read();
        IDStatus += message;
        Serial.print(message);
      }
      Serial.println();

      received_status = true;

      return;
    }
  }
}

void GateOpen() {
  Serial.println(IDStatus);
  if (IDStatus == "Device verified!" && pos == 0) {
    for (pos = 0; pos <= 90; pos += 1) {  // goes from 0 degrees to 180 degrees
      // in steps of 1 degree
      myservo.write(pos);  // tell servo to go to position in variable 'pos'
      delay(15);           // waits 15 ms for the servo to reach the position
    }
    delay(3000);
    for (pos = 90; pos >= 0; pos -= 1) {  // goes from 180 degrees to 0 degrees
      myservo.write(pos);                 // tell servo to go to position in variable 'pos'
      delay(15);                          // waits 15 ms for the servo to reach the position
    }
    if (Firebase.getString(fbdo, getPath) && fbdo.stringData() == "Opened") {
      Firebase.setString(fbdo, getPath, "Closed");
    }

  } else if (IDStatus == "Device verified!" && pos != 0) {
    delay(3000);
    for (pos = 90; pos >= 0; pos -= 1) {
      myservo.write(pos);
      delay(15);
    }
    if (Firebase.getString(fbdo, getPath) && fbdo.stringData() == "Opened") {
      Firebase.setString(fbdo, getPath, "Closed");
    }
  }

  IDStatus = "";
}

void CheckRapberryPi() {
  digitalWrite(LED, HIGH);

  if (Firebase.getString(fbdo, getPath)) {
    Serial.println("Gate" + fbdo.stringData());
    if (fbdo.stringData() == "Opened" && GateOpened == false) {
      GateClosed = false;
      GateOpenNow();

    } else if (fbdo.stringData() == "Closed" && GateClosed == false) {
      GateOpened = false;
      GateCloseNow();
    }
  }
}

void GateOpenNow() {
  Serial.println(IDStatus);

  for (pos = 0; pos <= 90; pos += 1) {  // goes from 0 degrees to 180 degrees
    // in steps of 1 degree
    myservo.write(pos);  // tell servo to go to position in variable 'pos'
    delay(15);           // waits 15 ms for the servo to reach the position
  }

  IDStatus = "";
  GateOpened = true;
}

void GateCloseNow() {
  Serial.println(IDStatus);

  for (pos = 90; pos >= 0; pos -= 1) {
    myservo.write(pos);
    delay(15);
  }

  IDStatus = "";
  GateClosed = true;
}

void CheckWIFI() {
  Serial.print("Attempting to connect to WPA SSID: ");
  Serial.println(ssid);

  while (WiFi.begin(ssid, pass) != WL_CONNECTED) {
    // failed, retry
    Serial.print(".");
    digitalWrite(LED, HIGH);
    delay(700);
    digitalWrite(LED, LOW);
    delay(700);
    digitalWrite(LED, HIGH);
    delay(700);
    digitalWrite(LED, LOW);
    delay(2900);
  }

  Serial.println("You're connected to the network");
  Serial.println();
}
