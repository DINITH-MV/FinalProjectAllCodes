import os
import paho.mqtt.client as mqtt
from gpiozero import PWMOutputDevice
import RPi.GPIO as GPIO

import firebase_admin
from firebase_admin import credentials
from firebase_admin import db

import time

BROKER = "broker.emqx.io"
TOPIC = "SIT210/wave11"
TOPIC2 = "SIT210/wave6"
LED_PIN = 19
BUZZER = 18
Relay = 3

GPIO.setmode(GPIO.BCM)
GPIO.setup(LED_PIN, GPIO.OUT)
BUZZER_TONE = PWMOutputDevice(BUZZER)

GPIO.setup(Relay, GPIO.OUT)

cred = credentials.Certificate("/home/dinith/Desktop/Project/MQTT/smart-farm-defense-system-firebase-adminsdk-fbsvc-745c505a8f.json")

firebase_admin.initialize_app(cred, {
    'databaseURL': 'https://smart-farm-defense-system-default-rtdb.firebaseio.com'
})

fence_state_check = False

GPIO.output(LED_PIN, GPIO.HIGH)

def on_message(client, userdata, msg):
    message = msg.payload.decode("utf-8")
    print("Received:", message)
    
    if message == "1779927128":
        ID = "TAG01"
    elif message == "980315838":
        ID = "TAG02"
    elif message == "1275606116":
        ID = "TAG03"
    else:
        return  # unknown tag, bail out

    ID_status = IDControl(ID)

    if ID_status == "Verified":
        confirmation = "Device verified!"
        client.publish(TOPIC2, confirmation)
        speak("Device Verified")
        print(f"Sent confirmation: {confirmation}")
        
        play_tone(1700, 0.06)
        play_tone(1300, 0.07)
        play_tone(1000, 0.08)
        play_tone(800, 0.09)
    else:
        status = "Verification failed!"
        client.publish(TOPIC2, status)
        speak("Verification failed")
        print(status)
        
        play_tone(600, 0.1)
        play_tone(800, 0.1)
        play_tone(600, 0.1)


def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("Connected to MQTT Broker!")
        client.subscribe(TOPIC)
    else:
        print(f"Failed to connect, return code {rc}")
        sys.exit(1)
        
def play_tone(freq, duration):
    BUZZER_TONE.frequency = freq
    BUZZER_TONE.value = 0.2 
    time.sleep(duration)
    BUZZER_TONE.off()
    
def speak(text):
    os.system(f'espeak "{text}"')

def IDControl(ID):
    ref = db.reference(f'/ID/{ID}')
    data = ref.get()
    return data    

def FenceVoltageControl():
        global fence_state_check
        ref = db.reference('/')  # Adjust path
        data = ref.get()
        print("Data from Firebase:", data)

        fence_state = data.get('Fence') if data else None

        # If gate just opened
        if fence_state == 'Enhance' and fence_state_check == False:
            GPIO.output(Relay, GPIO.HIGH)  # Turn on relay           
                        
        elif fence_state == 'Normal':
            GPIO.output(Relay, GPIO.LOW)  # Turn off relay
            fence_state_check = False

        time.sleep(2)
    
client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message

try:
    client.connect(BROKER, 1883, 60)
    client.loop_start()  # This will block until interrupted 
    while True:
        FenceVoltageControl()  
except KeyboardInterrupt:
     print("Program interrupted by user")
except Exception as e:
      print(f"Error occurred: {str(e)}")
finally:
     GPIO.cleanup()
     client.disconnect()
     print("GPIO cleaned up and MQTT client disconnected")
