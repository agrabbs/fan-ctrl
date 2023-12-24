/**
 * FAN-CTRL v1.5
 * 
 */
#include "BLEDevice.h"

#define RELAY_NO true // Set to true to define Relay as Normally Open (NO)
#define NUM_RELAYS 3 // Set number of relays
#define LOW_RELAY_PIN 25
#define MED_RELAY_PIN 26
#define HIGH_RELAY_PIN 27

// Assign each GPIO to a relay
// +--------------------------+
// | OFF |  LOW |  MED | HIGH |
// +--------------------------+
// |  0  |  Z1  |  Z2  |  Z3+ |
// +--------------------------+
#define FAN_OFF 0    // OFF 
#define FAN_LOW 60   // Z1
#define FAN_MED 65   // Z2
#define FAN_HIGH 70  // Z3+


uint8_t relayGPIOs[NUM_RELAYS] = {LOW_RELAY_PIN, MED_RELAY_PIN, HIGH_RELAY_PIN}; // Assign each GPIO to a relay

static BLEUUID serviceUUID(BLEUUID((uint16_t)0x180d));  // BLE Heart Rate Service
static BLEUUID charUUID(BLEUUID((uint16_t)0x2A37));     // BLE Heart Rate Measure Characteristic

static boolean doScan = true;
static boolean doConnect = false;
static boolean connected = false;
static BLEAdvertisedDevice* hrmDevice;
static BLERemoteCharacteristic* pRemoteCharacteristic;
static uint8_t fanState = FAN_OFF;
static void relayManager(uint8_t heartrate);

// Classes
class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
    Serial.printf("%lu [info] Connected: %s\n", millis(), pclient->getPeerAddress().toString().c_str());
  }

  void onDisconnect(BLEClient* pclient) {
    relayManager(0);
    connected = false;
    doScan = true;
    Serial.printf("%lu [info] Disconnected: %s\n", millis(), pclient->getPeerAddress().toString().c_str());
  }
};

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID)) {
      Serial.printf("%lu [info] %s\n", millis(), advertisedDevice.toString().c_str());
      BLEDevice::getScan()->stop();
      hrmDevice = new BLEAdvertisedDevice(advertisedDevice);
      doConnect = true;
      doScan = false;
    } 
  }
}; 

// Functions
// Relay Control Logic
static void relayManager(uint8_t heartrate) {
  uint8_t newFanState;
  switch (heartrate) {
    case FAN_OFF ... (FAN_LOW - 1):   // OFF
      newFanState = FAN_OFF;
      break;
    case FAN_LOW ... (FAN_MED - 1):   // LOW
      newFanState = FAN_LOW;
      break;  
    case FAN_MED ... (FAN_HIGH - 1): // MEDIUM
      newFanState = FAN_MED;
      break;
    case FAN_HIGH ... 9999:      // HIGH
      newFanState = FAN_HIGH;
      break;
    default:                 // OFF
      newFanState = FAN_OFF;
      break;
  }
  Serial.printf("%lu [info] %ubpm\n", millis(), heartrate);
  if (newFanState != fanState) {
    fanState = newFanState;
    bool relay[] = {
      (fanState == FAN_LOW) ? LOW : HIGH,
      (fanState == FAN_MED) ? LOW : HIGH,
      (fanState == FAN_HIGH) ? LOW : HIGH
    };
    //Serial.printf("[fan-state] change %u, [%u:%u:%u]\n", fanState, relay[0], relay[1], relay[2]);
    // Update Relays
    // Set all off first 
    for(int i=1; i<=NUM_RELAYS; i++){
      digitalWrite(relayGPIOs[i-1], HIGH);
    }
    // Now turn on if needed
    for(int i=1; i<=NUM_RELAYS; i++){
      if (relay[i-1] == LOW) {
        Serial.printf("%lu [info] Turning on relay %u\n", millis(), relayGPIOs[i-1]);
        digitalWrite(relayGPIOs[i-1], LOW);
      }
    }
  }
}

// BLE Heart Rate Measure Callback
static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {
    relayManager(pData[1]);
}

bool connectToServer() {
    BLEClient* pClient = BLEDevice::createClient();
    pClient->setClientCallbacks(new MyClientCallback());
    pClient->connect(hrmDevice);

    BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
    if (pRemoteService == nullptr) {
      pClient->disconnect();
      return false;
    }

    pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
    if (pRemoteCharacteristic == nullptr) {
      pClient->disconnect();
      return false;
    }

    if (pRemoteCharacteristic->canNotify()) {
      pRemoteCharacteristic->registerForNotify(notifyCallback);
    } else {
      pClient->disconnect();
      return false;
    }

    connected = true;
    return true;
}

void setup() {
  Serial.begin(115200);

  // Setup Relay Outputs
  for(int i=1; i<=NUM_RELAYS; i++){
    pinMode(relayGPIOs[i-1], OUTPUT);
    digitalWrite(relayGPIOs[i-1], (RELAY_NO) ? HIGH : LOW);
  }

  BLEDevice::init("");
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);
} 

void loop() {
  // Perform Connection
  if (doConnect) {
    if (!connectToServer()) {
      Serial.printf("%lu [error] Failed to connect to HRM.\n", millis());
    }
    doConnect = false;
  }

  // Perform Scan
  if (doScan) {
    Serial.printf("%lu [info] Performing Scan...", millis());
    BLEDevice::getScan()->start(5, false); 
    delay(5000);
  }
}
