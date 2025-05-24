import sys
import time
import paho.mqtt.client as mqtt
import RPi.GPIO as GPIO

import firebase_admin
from firebase_admin import credentials
from firebase_admin import db

BROKER = "broker.emqx.io"
TOPIC  = "test/topic3"

Relay = 3

client = mqtt.Client()
GPIO.setmode(GPIO.BCM)
GPIO.setup(Relay, GPIO.OUT)

cred = credentials.Certificate("/home/dinith/Desktop/Project/MQTT/smart-farm-defense-system-firebase-adminsdk-fbsvc-745c505a8f.json")

firebase_admin.initialize_app(cred, {
    'databaseURL': 'https://smart-farm-defense-system-default-rtdb.firebaseio.com'
})

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("Connected to MQTT Broker!")
        client.subscribe(TOPIC)            
    else:
        print(f"Failed to connect")
        sys.exit(1)                         

def on_message(client, userdata, msg):
    message = msg.payload.decode("utf-8")
    print("Received:", message)
    
    ref = db.reference('/Fence')  
    
    if message == "Kangaroo Detected":
        GPIO.output(Relay, GPIO.HIGH)
        ref.set("Enhanced")
        time.sleep(20)        
        count += 1
        
    if count > 2 and message != "Kangaroo Detected":
        GPIO.output(Relay, GPIO.LOW)  
        ref.set("Normal")
        count = 0

client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message

try:
    client.connect(BROKER, 1883, 60)
    client.loop_forever()              
except KeyboardInterrupt:
    print("Interrupted by user")
except Exception as e:
    print("Error occurred:", e)
finally:
    client.disconnect()
    print("Disconnected from MQTT Broker")
