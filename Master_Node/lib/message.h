
// LoRa Defines
#define DISCOVERY_MESSAGE 0
#define DISCOVERY_REPLY_MESSAGE 1
#define DATA_MESSAGE 2
#define DATA_REPLY_MESSAGE 3

// Self MAC Address
const int MAX_MAC_LENGTH = 18;
char selfAddr[MAX_MAC_LENGTH];

/*---------------------------ESPNOW Defines-----------------------*/

// Struc for transmitting sensor data
typedef struct SensorData {
  // uint8_t rootNodeAddress;
  char MACaddr[MAX_MAC_LENGTH];  // Use a char array to store the MAC address
  float c02Data;
  float temperatureData;
  float humidityData;
} SensorData;


// Connection Request Struct
typedef struct Handshake {
  // 0 for request, 1 for reply
  uint8_t requestType;
  // 1 if connected to master, 0 otherwise
  uint8_t isConnectedToMaster;
  // number of hops to master
  uint8_t numberOfHopsToMaster;
} Handshake;

/*------------------------------------------------------------------*/

/*---------------------------LoRa Defines-----------------------*/
// Discovery Message
typedef struct DiscoveryMessage {
  uint8_t requestType;
} DiscoveryMessage;

DiscoveryMessage dm = { DISCOVERY_MESSAGE };

// Struc for transmitting sensor data
typedef struct SensorDataLoRa {
  // uint8_t rootNodeAddress;
  uint8_t requestType;
  char MACaddr[MAX_MAC_LENGTH];  // Use a char array to store the MAC address
  char SMACaddr[MAX_MAC_LENGTH]; // self mac
  float c02Data;
  float temperatureData;
  float humidityData;
  uint8_t randomNumber;
} SensorDataLoRa;

SensorDataLoRa sensorDataLoRa = {
  .requestType = 2
};

typedef struct SensorDataReply {
  uint8_t requestType;
  int randomNumber;  // from the data packet
} SensorDataReply;

SensorDataReply sdr = {
  .requestType = DATA_REPLY_MESSAGE
};
