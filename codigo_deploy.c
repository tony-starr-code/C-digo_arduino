#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLECharacteristic.h>
#include <BLE2902.h>

#define SERVICE_UUID        "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

// Pinos RS485
#define RX_PIN 16
#define TX_PIN 17
#define DE_PIN 5
#define RE_PIN 4

BLECharacteristic *pCharacteristic;
bool deviceConnected = false;

// Callbacks para conexão/desconexão
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    Serial.println("✅ Cliente conectado!");
  }
  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    Serial.println("❌ Cliente desconectado! Reiniciando advertising...");
    pServer->startAdvertising(); // Reanuncia para novas conexões
  }
};

void setup() {
  Serial.begin(115200);
  Serial2.begin(4800, SERIAL_8N1, RX_PIN, TX_PIN);

  pinMode(DE_PIN, OUTPUT);
  pinMode(RE_PIN, OUTPUT);
  digitalWrite(DE_PIN, LOW);
  digitalWrite(RE_PIN, LOW);

  // Inicializa BLE
  BLEDevice::init("ESP32-NPK");
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Cria serviço e característica com propriedade NOTIFY
  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_NOTIFY
  );
  pCharacteristic->addDescriptor(new BLE2902()); // Necessário para notificações
  pService->start();

  // Configura o advertising para ser visível
  BLEAdvertising *pAdvertising = pServer->getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMaxPreferred(0x12);
  pServer->startAdvertising();

  Serial.println("🚀 BLE iniciado - ESP32-NPK pronto para enviar dados!");
}

void loop() {
  // Leitura do sensor via RS485
  digitalWrite(DE_PIN, HIGH);
  digitalWrite(RE_PIN, HIGH);
  delay(1);

  byte request[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x07, 0x04, 0x08};
  Serial2.write(request, sizeof(request));
  Serial2.flush();
  delay(1);

  digitalWrite(DE_PIN, LOW);
  digitalWrite(RE_PIN, LOW);

  // Aguarda resposta com timeout de 500ms
  unsigned long start = millis();
  while (Serial2.available() < 19 && millis() - start < 500) {
    delay(10);
  }

  if (Serial2.available() >= 19) {
    byte r[19];
    Serial2.readBytes(r, 19);

    // Extrai os valores úteis (bytes 3 a 16)
    uint16_t moistureRaw = (r[3] << 8) | r[4];
    uint16_t tempRaw     = (r[5] << 8) | r[6];
    uint16_t ecRaw       = (r[7] << 8) | r[8];
    uint16_t phRaw       = (r[9] << 8) | r[10];
    uint16_t nRaw        = (r[11] << 8) | r[12];
    uint16_t pRaw        = (r[13] << 8) | r[14];
    uint16_t kRaw        = (r[15] << 8) | r[16];

    float moisture = moistureRaw * 0.1;
    float temp     = tempRaw * 0.1;
    int   ec       = ecRaw;
    float ph       = phRaw * 0.1;
    int   n        = nRaw;
    int   p        = pRaw;
    int   k        = kRaw;

    // Log para depuração (opcional)
    Serial.printf("N:%d P:%d K:%d pH:%.1f T:%.1f H:%.1f EC:%d\n",
                  n, p, k, ph, temp, moisture, ec);

    // Prepara o buffer de 14 bytes na ordem esperada pelo app:
    // [H (2), T (2), EC (2), pH (2), N (2), P (2), K (2)]
    byte dataToSend[14];
    int idx = 0;
    uint16_t hInt = (uint16_t)(moisture * 10);
    dataToSend[idx++] = (hInt >> 8) & 0xFF;
    dataToSend[idx++] = hInt & 0xFF;
    uint16_t tInt = (uint16_t)(temp * 10);
    dataToSend[idx++] = (tInt >> 8) & 0xFF;
    dataToSend[idx++] = tInt & 0xFF;
    dataToSend[idx++] = (ec >> 8) & 0xFF;
    dataToSend[idx++] = ec & 0xFF;
    uint16_t phInt = (uint16_t)(ph * 10);
    dataToSend[idx++] = (phInt >> 8) & 0xFF;
    dataToSend[idx++] = phInt & 0xFF;
    dataToSend[idx++] = (n >> 8) & 0xFF;
    dataToSend[idx++] = n & 0xFF;
    dataToSend[idx++] = (p >> 8) & 0xFF;
    dataToSend[idx++] = p & 0xFF;
    dataToSend[idx++] = (k >> 8) & 0xFF;
    dataToSend[idx++] = k & 0xFF;

    // Envia via BLE se houver cliente conectado
    if (deviceConnected) {
      pCharacteristic->setValue(dataToSend, 14);
      pCharacteristic->notify();
      Serial.println("📤 Dados enviados via BLE (14 bytes)");
    }
  } else {
    // Sem resposta do sensor (exibe a cada 5 segundos para não poluir)
    static unsigned long lastLog = 0;
    if (millis() - lastLog > 5000) {
      Serial.println("⚠️ Sem resposta do sensor RS485");
      lastLog = millis();
    }
  }

  delay(3000); // Aguarda 3 segundos antes da próxima leitura
}