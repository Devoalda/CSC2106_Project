#!/usr/bin/env python3

import serial
import argparse
import paho.mqtt.client as mqtt
import json

# MQTT Broker Settings
DEFAULT_BROKER_HOST = "localhost"
DEFAULT_BROKER_PORT = 1883

# Serial Port Settings
SERIAL_PORT = '/dev/ttyACM0'  # Adjust this to match your serial port
BAUD_RATE = 9600  # Adjust this to match your baud rate

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("Connected to MQTT broker")
    else:
        print(f"Connection to MQTT broker failed with result code {rc}")
        
def publish_data(client, topic, payload):
    client.publish(topic, payload)

def main(broker_host, broker_port):
    # Initialize MQTT client
    client = mqtt.Client(client_id="sensor_data_publisher")
    client.on_connect = on_connect

    # Connect to MQTT broker
    client.connect(broker_host, broker_port)

    # Open serial port
    ser = serial.Serial(SERIAL_PORT, BAUD_RATE)

    try:
        while True:
            # Read data from serial port
            data = ser.readline().decode().strip()

            # Split data into fields
            fields = data.split(',')

            # Assuming fields are in the order: macStr, rootNodeAddress, c02Data, temperatureData, thirdValue
            macStr, rootNodeAddress, c02Data, temperatureData, thirdValue = fields

            # Create a dictionary to represent the data
            sensor_data = {
                "macStr": macStr,
                "rootNodeAddress": int(rootNodeAddress),
                "c02Data": float(c02Data),
                "temperatureData": float(temperatureData),
                "thirdValue": float(thirdValue)
            }

            # Convert dictionary to JSON string
            payload = json.dumps(sensor_data)

            # Publish data to MQTT broker
            topic = "sensor_data"
            publish_data(client, topic, payload)
            print("Data published:", payload)

    except KeyboardInterrupt:
        print("Exiting...")
        client.disconnect()
        ser.close()

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="MQTT Client for publishing sensor data from serial port to MQTT broker")
    parser.add_argument("--host", default=DEFAULT_BROKER_HOST, help="MQTT broker host")
    parser.add_argument("--port", default=DEFAULT_BROKER_PORT, type=int, help="MQTT broker port")
    args = parser.parse_args()

    main(args.host, args.port)

