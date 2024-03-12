#include <SensirionI2CScd4x.h>

// Define the maximum length of the MAC address
const int MAX_MAC_LENGTH = 18;  // For example, a MAC address is usually 17 characters long (including colons) plus 1 for null terminator

// Struc for transmitting sensor data
typedef struct SensorData {
  // uint8_t rootNodeAddress;
  uint8_t requestType;
  char MACaddr[MAX_MAC_LENGTH];  // Use a char array to store the MAC address
  float c02Data;
  float temperatureData;
  float humidityData;
} SensorData;

SensorData sensorData = {
  .requestType = 2
};

uint16_t error;
char errorMessage[256];
uint16_t co2 = 0;
float temperature = 0.0f;
float humidity = 0.0f;
bool isDataReady = false;

float getRandomFloat(float min, float max) {
  // Generate a random floating-point number
  float randomFloat = min + random() / ((float)RAND_MAX / (max - min));

  // Convert the float to a string
  // char buffer[10];                     // Adjust the size as needed
  // dtostrf(randomFloat, 6, 2, buffer);  // Format the float with 6 total characters and 2 decimal places
  // String floatString = String(buffer);

  return randomFloat;
}

// Function to parse a String MAC address to uint8_t arra

String getRandomFloatAsString(float min, float max) {
  // Generate a random floating-point number
  float randomFloat = min + random() / ((float)RAND_MAX / (max - min));

  // Convert the float to a string
  char buffer[10];                     // Adjust the size as needed
  dtostrf(randomFloat, 6, 2, buffer);  // Format the float with 6 total characters and 2 decimal places
  String floatString = String(buffer);

  return floatString;
}


SensirionI2CScd4x scd4x;

void printUint16Hex(uint16_t value) {
  Serial.print(value < 4096 ? "0" : "");
  Serial.print(value < 256 ? "0" : "");
  Serial.print(value < 16 ? "0" : "");
  Serial.print(value, HEX);
}

void printSerialNumber(uint16_t serial0, uint16_t serial1, uint16_t serial2) {
  Serial.print("Serial: 0x");
  printUint16Hex(serial0);
  printUint16Hex(serial1);
  printUint16Hex(serial2);
  Serial.println();
}

void setupSensor() {
  Wire.begin(I2C_SDA, I2C_SCL);

  uint16_t error;
  char errorMessage[256];

  scd4x.begin(Wire);

  // stop potentially previously started measurement
  error = scd4x.stopPeriodicMeasurement();
  if (error) {
    Serial.print("Error trying to execute stopPeriodicMeasurement(): ");
    errorToString(error, errorMessage, 256);
    Serial.println(errorMessage);
  }

  uint16_t serial0;
  uint16_t serial1;
  uint16_t serial2;
  error = scd4x.getSerialNumber(serial0, serial1, serial2);
  if (error) {
    Serial.print("Error trying to execute getSerialNumber(): ");
    errorToString(error, errorMessage, 256);
    Serial.println(errorMessage);
  } else {
    printSerialNumber(serial0, serial1, serial2);
  }

  // Start Measurement
  error = scd4x.startPeriodicMeasurement();
  if (error) {
    Serial.print("Error trying to execute startPeriodicMeasurement(): ");
    errorToString(error, errorMessage, 256);
    Serial.println(errorMessage);
  }
}

bool getSensorReading() {
  error = scd4x.getDataReadyFlag(isDataReady);
  if (error) {
    Serial.print("Error trying to execute getDataReadyFlag(): ");
    errorToString(error, errorMessage, 256);
    Serial.println(errorMessage);
    // If error, use random data
    sensorData.c02Data = getRandomFloat(100.0, 1000.0);
    sensorData.temperatureData = getRandomFloat(0.0, 40.0);
    sensorData.humidityData = getRandomFloat(90.0, 1030.0);
  } else if (!isDataReady) {
    // If data not ready, return
    return false;
  } else {
    // Read sensor measurement
    error = scd4x.readMeasurement(co2, temperature, humidity);
    if (error) {
      Serial.print("Error trying to execute readMeasurement(): ");
      errorToString(error, errorMessage, 256);
      Serial.println(errorMessage);
      // If error, use random data
      sensorData.c02Data = getRandomFloat(100.0, 1000.0);
      sensorData.temperatureData = getRandomFloat(0.0, 40.0);
      sensorData.humidityData = getRandomFloat(90.0, 1030.0);
    } else if (co2 == 0) {
      Serial.println("Invalid sample detected, skipping.");
      // If invalid sample, use random data
      sensorData.c02Data = getRandomFloat(100.0, 1000.0);
      sensorData.temperatureData = getRandomFloat(0.0, 40.0);
      sensorData.humidityData = getRandomFloat(90.0, 1030.0);
    } else {
      // Assign sensor data to sensorData structure
      sensorData.c02Data = co2;
      sensorData.temperatureData = temperature;
      sensorData.humidityData = humidity;
    }
  }
  return true;
}