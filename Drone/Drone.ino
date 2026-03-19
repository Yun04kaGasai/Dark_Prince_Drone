#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// ESP32-C3 Mini motor outputs (as requested)
static const int MOTOR_PINS[4] = {4, 5, 2, 3};
static const int MOTOR_CHANNELS[4] = {0, 1, 2, 3};
static const int PWM_FREQ = 20000;
static const int PWM_RESOLUTION_BITS = 8;
static const unsigned long COMMAND_TIMEOUT_MS = 1200;

static const char* BLE_DEVICE_NAME = "DarkPrinceDrone";
static const char* SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
static const char* COMMAND_CHAR_UUID = "6e400002-b5a3-f393-e0a9-e50e24dcca9e";

BLEServer* pServer = nullptr;
BLECharacteristic* pCommandCharacteristic = nullptr;

bool isConnected = false;
bool isArmed = false;
unsigned long lastCommandMs = 0;

int channelByPin(int pin) {
  for (int i = 0; i < 4; i++) {
    if (MOTOR_PINS[i] == pin) {
      return MOTOR_CHANNELS[i];
    }
  }
  return -1;
}

void setMotorSpeed(int pin, int speed) {
  int channel = channelByPin(pin);
  if (channel < 0) {
    Serial.printf("Unknown pin: %d\n", pin);
    return;
  }

  int safeSpeed = constrain(speed, 0, 255);
  ledcWrite(pin, safeSpeed);
  Serial.printf("Motor pin %d speed %d\n", pin, safeSpeed);
}

void stopAllMotors() {
  for (int i = 0; i < 4; i++) {
    ledcWrite(MOTOR_PINS[i], 0);
  }
  Serial.println("All motors stopped");
}

void setAllMotors(int speed) {
  int safeSpeed = constrain(speed, 0, 255);
  for (int i = 0; i < 4; i++) {
    ledcWrite(MOTOR_PINS[i], safeSpeed);
  }
}

void applyCommand(const String& commandRaw) {
  String command = commandRaw;
  command.trim();
  if (command.length() == 0) {
    return;
  }

  lastCommandMs = millis();

  if (command.equalsIgnoreCase("ARM")) {
    isArmed = true;
    setAllMotors(0);
    Serial.println("ARMED");
    return;
  }

  if (command.equalsIgnoreCase("DISARM")) {
    isArmed = false;
    stopAllMotors();
    Serial.println("DISARMED");
    return;
  }

  if (command.equalsIgnoreCase("STOP")) {
    stopAllMotors();
    return;
  }

  if (!isArmed) {
    Serial.println("Command ignored: drone is not armed");
    return;
  }

  if (command.startsWith("M,")) {
    int firstComma = command.indexOf(',');
    int secondComma = command.indexOf(',', firstComma + 1);
    if (firstComma < 0 || secondComma < 0) {
      Serial.println("Bad command format");
      return;
    }

    int pin = command.substring(firstComma + 1, secondComma).toInt();
    int speed = command.substring(secondComma + 1).toInt();
    setMotorSpeed(pin, speed);
    return;
  }

  Serial.printf("Unknown command: %s\n", command.c_str());
}

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* server) override {
    isConnected = true;
    lastCommandMs = millis();
    Serial.println("BLE client connected");
  }

  void onDisconnect(BLEServer* server) override {
    isConnected = false;
    isArmed = false;
    stopAllMotors();
    Serial.println("BLE client disconnected");
    BLEDevice::startAdvertising();
  }
};

class CommandCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* characteristic) override {
    String value = characteristic->getValue();
    if (value.length() == 0) {
      return;
    }

    applyCommand(value);
  }
};

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("Booting BLE drone controller");

  for (int i = 0; i < 4; i++) {
    ledcAttachChannel(MOTOR_PINS[i], PWM_FREQ, PWM_RESOLUTION_BITS, MOTOR_CHANNELS[i]);
    ledcWrite(MOTOR_PINS[i], 0);
  }

  BLEDevice::init(BLE_DEVICE_NAME);
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  BLEService* pService = pServer->createService(SERVICE_UUID);
  pCommandCharacteristic = pService->createCharacteristic(
      COMMAND_CHAR_UUID,
      BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  pCommandCharacteristic->setCallbacks(new CommandCallbacks());
  pCommandCharacteristic->addDescriptor(new BLE2902());

  pService->start();
  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();

  Serial.println("BLE ready. Waiting for client...");
}

void loop() {
  if (isArmed && (millis() - lastCommandMs > COMMAND_TIMEOUT_MS)) {
    // Failsafe: stop motors if app stops sending commands.
    stopAllMotors();
    isArmed = false;
    Serial.println("FAILSAFE: command timeout, DISARMED");
  }

  delay(10);
}