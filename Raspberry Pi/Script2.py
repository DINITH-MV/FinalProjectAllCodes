import sys
import time
import threading
import logging
import paho.mqtt.client as mqtt
import RPi.GPIO as GPIO
import firebase_admin
from firebase_admin import credentials, db
from queue import Queue

# Configuration
BROKER = "broker.emqx.io"
TOPIC = "test/topic3"
RELAY_PIN = 3
ACTIVATION_DURATION = 20  # seconds
MAX_RETRIES = 3
RETRY_DELAY = 5  # seconds

# Initialize logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler('fence_system.log'),
        logging.StreamHandler()
    ]
)

# Global variables
message_queue = Queue()
system_active = True
relay_lock = threading.Lock()

class FenceSystem:
    def __init__(self):
        # Initialize GPIO
        GPIO.setmode(GPIO.BCM)
        GPIO.setup(RELAY_PIN, GPIO.OUT)
        GPIO.output(RELAY_PIN, GPIO.LOW)
        
        # Initialize Firebase
        try:
            cred = credentials.Certificate(
                "/home/dinith/Desktop/Project/MQTT/smart-farm-defense-system-firebase-adminsdk-fbsvc-745c505a8f.json"
            )
            firebase_admin.initialize_app(cred, {
                'databaseURL': 'https://smart-farm-defense-system-default-rtdb.firebaseio.com'
            })
            self.firebase_ref = db.reference('/Fence')
            logging.info("Firebase initialized successfully")
        except Exception as e:
            logging.error(f"Firebase initialization failed: {str(e)}")
            raise

    def activate_relay(self, duration):
        with relay_lock:
            try:
                GPIO.output(RELAY_PIN, GPIO.HIGH)
                self.firebase_ref.set("Enhanced")
                logging.info(f"Relay activated for {duration} seconds")
                time.sleep(duration)
                GPIO.output(RELAY_PIN, GPIO.LOW)
                self.firebase_ref.set("Normal")
                logging.info("Relay deactivated")
            except Exception as e:
                logging.error(f"Error in relay operation: {str(e)}")
                GPIO.output(RELAY_PIN, GPIO.LOW)  # Ensure relay is off on error

def mqtt_client_worker(system):
    retry_count = 0
    client = mqtt.Client()
    
    def on_connect(client, userdata, flags, rc):
        nonlocal retry_count
        if rc == 0:
            logging.info("Connected to MQTT Broker")
            client.subscribe(TOPIC)
            retry_count = 0  # Reset retry counter on successful connection
        else:
            logging.error(f"Connection failed with code {rc}")
            if retry_count < MAX_RETRIES:
                retry_count += 1
                time.sleep(RETRY_DELAY)
                client.reconnect()

    def on_message(client, userdata, msg):
        try:
            message = msg.payload.decode("utf-8")
            logging.info(f"Received: {message}")
            message_queue.put(message)
        except Exception as e:
            logging.error(f"Error processing message: {str(e)}")

    client.on_connect = on_connect
    client.on_message = on_message

    while system_active and retry_count < MAX_RETRIES:
        try:
            client.connect(BROKER, 1883, 60)
            client.loop_forever()
        except Exception as e:
            logging.error(f"MQTT error: {str(e)}")
            if retry_count < MAX_RETRIES:
                retry_count += 1
                time.sleep(RETRY_DELAY)
            else:
                logging.critical("Max MQTT connection retries reached")
                break

def message_processor(system):
    last_activation_time = 0
    cooldown_period = 30  # Minimum seconds between activations
    
    while system_active:
        try:
            if not message_queue.empty():
                message = message_queue.get()
                current_time = time.time()
                
                # Only process if enough time has passed since last activation
                if current_time - last_activation_time >= cooldown_period:
                    if message == "Kangaroo Detected":
                        system.activate_relay(ACTIVATION_DURATION)
                        last_activation_time = current_time
                    elif message == "No Object Detected":
                        # Clear the queue when no objects detected
                        while not message_queue.empty():
                            try:
                                message_queue.get_nowait()
                            except:
                                pass
                
                # Always mark task as done
                message_queue.task_done()
                
            time.sleep(0.1)
            
        except Exception as e:
            logging.error(f"Message processing error: {str(e)}")
            time.sleep(1)

def main():
    global system_active
    
    try:
        # Initialize system
        system = FenceSystem()
        system.firebase_ref.set("Normal")
        
        # Start threads
        mqtt_thread = threading.Thread(
            target=mqtt_client_worker,
            args=(system,),
            daemon=True
        )
        processor_thread = threading.Thread(
            target=message_processor,
            args=(system,),
            daemon=True
        )
        
        mqtt_thread.start()
        processor_thread.start()
        
        logging.info("Fence defense system started")
        
        # Main thread just waits for shutdown
        while True:
            time.sleep(1)
            
    except KeyboardInterrupt:
        logging.info("Shutdown signal received")
    except Exception as e:
        logging.critical(f"System error: {str(e)}")
    finally:
        system_active = False
        GPIO.output(RELAY_PIN, GPIO.LOW)
        logging.info("System shutdown complete")

if __name__ == "__main__":
    main()