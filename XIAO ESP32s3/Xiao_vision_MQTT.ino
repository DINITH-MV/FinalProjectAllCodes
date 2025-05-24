#include <Seeed_Arduino_SSCMA.h>
#include <WiFi.h>
#include <PubSubClient.h>

// Replace with your network credentials
const char* ssid = "Dinith's Galaxy A55";
const char* password = "12345678.";

// MQTT broker IP address
const char* mqtt_server = "broker.emqx.io";

// Initialize the WiFi and MQTT client objects
WiFiClient espClient;
PubSubClient client(espClient);

SSCMA AI;

void setup() {
  AI.begin();
  Serial.begin(9600);

  // Connect to WiFi network
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  // Configure MQTT broker
  client.setServer(mqtt_server, 1883);

  // Connect to MQTT broker
  while (!client.connected()) {
    if (client.connect("")) {
      Serial.println("Connected to MQTT broker");
    } else {
      Serial.print("Failed to connect to MQTT broker");
      Serial.print(client.state());
      Serial.println("retrying in 5 seconds");
      delay(5000);
    }
  }
}

void loop() {

  if (!client.connected()) {
    if (client.connect("")) {
      Serial.println("Re-connected to MQTT broker");
    }
  }

  client.loop();

  if (!AI.invoke()) {
    Serial.println("invoke success");
    Serial.print("perf: prepocess=");
    Serial.print(AI.perf().prepocess);
    Serial.print(", inference=");
    Serial.print(AI.perf().inference);
    Serial.print(", postpocess=");
    Serial.println(AI.perf().postprocess);

    for (int i = 0; i < AI.boxes().size(); i++) {
      Serial.print("Box[");
      Serial.print(i);
      Serial.print("] target=");
      Serial.print(AI.boxes()[i].target);
      Serial.print(", score=");
      Serial.print(AI.boxes()[i].score);
      Serial.print(", x=");
      Serial.print(AI.boxes()[i].x);
      Serial.print(", y=");
      Serial.print(AI.boxes()[i].y);
      Serial.print(", w=");
      Serial.print(AI.boxes()[i].w);
      Serial.print(", h=");
      Serial.println(AI.boxes()[i].h);
    }
    for (int i = 0; i < AI.classes().size(); i++) {
      Serial.print("Class[");
      Serial.print(i);
      Serial.print("] target=");
      Serial.print(AI.classes()[i].target);
      Serial.print(", score=");
      Serial.println(AI.classes()[i].score);

      Detection(AI.classes()[i].score, AI.classes()[i].target);
    }
  }

  client.loop();
}

void Detection(int score, int target) {
  if (score >= 80) {
    if (target == 0) {
      client.publish("test/topic3", "Lion Detected");
      Serial.println("Lion Detected");
    } else if (target == 1) {
      client.publish("test/topic3", "Wild boar Detected");
      Serial.println("Wild boar Detected");
    } else if (target == 2) {
      client.publish("test/topic3", "Kangaroo Detected");
      Serial.println("Kangaroo Detected");
    } else if (target == 3) {
      client.publish("test/topic3", "No Object Detected");
      Serial.println("No Object Detected");
  
  } else if (score >= 20) {
    client.publish("test/topic3", "Animal Detected");
    Serial.println("Animal Detected");
  }
}
}
