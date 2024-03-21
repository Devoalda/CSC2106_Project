#!/usr/bin/env python3

import serial
import argparse
import paho.mqtt.client as mqtt
import json
import random
import configparser

# Load configuration from config file
config = configparser.ConfigParser()
config.read('config.ini')

# MQTT Broker Settings
DEFAULT_BROKER_HOST = config.get('mqtt', 'broker_host')
DEFAULT_BROKER_PORT = config.getint('mqtt', 'broker_port')
username = config.get('mqtt', 'username')
password = config.get('mqtt', 'password')

# Serial Port Settings
SERIAL_PORT = '/dev/ttyACM0'  # Adjust this to match your serial port
BAUD_RATE = 115200  # Adjust this to match your baud rate

# Predefined list of latitude and longitude coordinates
locations = [
    (1.3774855113539792, 103.84879287896177),  # SIT
    (1.3789584912541433, 103.84865094534874),  # S Block
    (1.3800868743225736, 103.84894833006176),  # A Block
    (1.3820260703444032, 103.84942144208942),  # North Canteen
    (1.3827760725534397, 103.84940792460249)  # LRez
]

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("Connected to MQTT broker")
    else:
        print(f"Connection to MQTT broker failed with result code {rc}")
        
def publish_data(client, topic, payload):
    client.publish(topic, payload)

def generate_location_data():
    # Randomly choose a pair of latitude and longitude coordinates from the predefined list
    latitude, longitude = random.choice(locations)
    
    return latitude, longitude

def main(broker_host, broker_port):
    # Initialize MQTT client
    client = mqtt.Client(client_id="", userdata=None, protocol=mqtt.MQTTv5)
    client.on_connect = on_connect
    client.tls_set(tls_version=mqtt.ssl.PROTOCOL_TLS)
    client.username_pw_set(username, password)

    # Connect to MQTT broker
    client.connect(broker_host, broker_port)

    # Open serial port
    ser = serial.Serial(SERIAL_PORT, BAUD_RATE)

    try:
        while True:
            # Read data from serial port
            try:
                data = ser.readline().decode().strip()
            except UnicodeDecodeError:
                print("Error decoding data from serial port.")
                continue

            # Split data into fields
            fields = data.split(',')

            # Check if the data has the expected number of fields
            if len(fields) != 5:
                print("Unexpected number of fields:", len(fields))
                continue

            # Assuming fields are in the order: macStr, c02Data, temperatureData, humidityData
            try:
                macStr, c02Data, temperatureData, humidityData, protocol = fields
            except ValueError:
                print("Error parsing data fields.")
                continue

            # Generate location data
            latitude, longitude = generate_location_data()

            # Create a dictionary to represent the data
            sensor_data = {
                "macStr": macStr,
                "c02Data": float(c02Data),
                "temperatureData": float(temperatureData),
                "humidityData": float(humidityData),
                "latitude": latitude,
                "longitude": longitude,
                "protocol": protocol
            }

            # Convert dictionary to JSON string
            payload = json.dumps(sensor_data)

            # Publish data to MQTT broker
            topic = "sensor_data"
            try:
                publish_data(client, topic, payload)
                print("Sensor data published:", payload)
            except Exception as e:
                print("Error publishing sensor data:", e)

    except KeyboardInterrupt:
        print("Exiting...")
        client.disconnect()
        ser.close()

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="MQTT Client for publishing sensor data with location data to MQTT broker")
    parser.add_argument("--host", default=DEFAULT_BROKER_HOST, help="MQTT broker host")
    parser.add_argument("--port", default=DEFAULT_BROKER_PORT, type=int, help="MQTT broker port")
    args = parser.parse_args()

    main(args.host, args.port)

