#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLECharacteristic.h>
#include <BLE2902.h>

#define SERVICE_UUID        "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

#define RX_PIN 16
#define TX_PIN 17
#define DE_PIN 5
#define RE_PIN 4

BLECharacteristic *pCharacteristic;
bool deviceConnected = false;

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    Serial.println("Cliente conectado!");
  }
  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    Serial.println("Cliente desconectado!");
    BLEDevice::startAdvertising();
  }
};

void setup() {
  Serial.begin(115200);
  Serial2.begin(4800, SERIAL_8N1, RX_PIN, TX_PIN);

  pinMode(DE_PIN, OUTPUT);
  pinMode(RE_PIN, OUTPUT);
  digitalWrite(DE_PIN, LOW);
  digitalWrite(RE_PIN, LOW);

  BLEDevice::init("ESP32-NPK");
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_NOTIFY
  );
  pCharacteristic->addDescriptor(new BLE2902());
  pService->start();
  BLEDevice::startAdvertising();
  Serial.println("BLE iniciado!");
}

void loop() {
  digitalWrite(DE_PIN, HIGH);
  digitalWrite(RE_PIN, HIGH);
  delay(1);

  byte request[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x07, 0x04, 0x08};
  Serial2.write(request, sizeof(request));
  Serial2.flush();
  delay(1);

  digitalWrite(DE_PIN, LOW);
  digitalWrite(RE_PIN, LOW);

  delay(200);

  if (Serial2.available() >= 19) {
    byte r[19];
    Serial2.readBytes(r, 19);

    float moisture = ((r[3] << 8) | r[4]) * 0.1;
    float temp     = ((r[5] << 8) | r[6]) * 0.1;
    int   ec       = (r[7] << 8) | r[8];
    float ph       = ((r[9] << 8) | r[10]) * 0.1;
    int   n        = (r[11] << 8) | r[12];
    int   p        = (r[13] << 8) | r[14];
    int   k        = (r[15] << 8) | r[16];

    String payload = "{\"N\":" + String(n) +
                     ",\"P\":" + String(p) +
                     ",\"K\":" + String(k) +
                     ",\"pH\":" + String(ph) +
                     ",\"T\":" + String(temp) +
                     ",\"H\":" + String(moisture) +
                     ",\"EC\":" + String(ec) + "}";

    Serial.println(payload);

    if (deviceConnected) {
      pCharacteristic->setValue(payload.c_str());
      pCharacteristic->notify();
    }
  } else {
    Serial.println("sem resposta do sensor...");
  }

  delay(3000);
}
