/**
 * No Sensor LoRa - Exemplo para Heltec WiFi LoRa 32 V2
 *
 * Envia dados de sensores para o Gateway LoRa JVtech
 * usando protocolo JSON.
 *
 * Este exemplo simula leituras de temperatura, umidade e bateria.
 * Substitua pelas leituras reais dos seus sensores.
 *
 * Hardware: Heltec WiFi LoRa 32 V2
 * - ESP32 + SX1276 (LoRa)
 * - Display OLED SSD1306 128x64
 * - LED onboard GPIO25
 */

#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include "SSD1306Wire.h"

// ============================================
// CONFIGURACOES DE PINOS - HELTEC LORA 32 V2
// ============================================

// Pinos LoRa SPI
#ifndef LORA_SCK
#define LORA_SCK 5
#endif
#ifndef LORA_MISO
#define LORA_MISO 19
#endif
#ifndef LORA_MOSI
#define LORA_MOSI 27
#endif
#ifndef LORA_CS
#define LORA_CS 18
#endif
#ifndef LORA_RST
#define LORA_RST 14
#endif
#ifndef LORA_DIO0
#define LORA_DIO0 26
#endif

// Pinos OLED
#ifndef OLED_SDA
#define OLED_SDA 4
#endif
#ifndef OLED_SCL
#define OLED_SCL 15
#endif
#ifndef OLED_RST
#define OLED_RST 16
#endif

// LED onboard (ja definido pela board Heltec = GPIO25)

// Controle de energia Vext (para perifericos externos)
#ifndef VEXT_PIN
#define VEXT_PIN 21
#endif

// Pino ADC para leitura de bateria
#ifndef BATTERY_PIN
#define BATTERY_PIN 37
#endif

// ============================================
// CONFIGURACOES DE RADIO LORA
// ============================================

#ifndef LORA_FREQUENCY
#define LORA_FREQUENCY 915E6  // 915 MHz (Brasil - AU915)
#endif
#ifndef LORA_TX_POWER
#define LORA_TX_POWER 20
#endif
#ifndef LORA_SF
#define LORA_SF 7
#endif
#ifndef LORA_BW
#define LORA_BW 125E3
#endif
#ifndef LORA_CR
#define LORA_CR 5
#endif

// ID do no
#ifndef NODE_ID
#define NODE_ID "NODE001"
#endif

// Sync Word (deve ser igual ao gateway)
#define LORA_SYNC_WORD 0x20

// Intervalo de transmissao (ms)
#define TX_INTERVAL 3000  // 3 segundos

// ============================================
// OBJETOS GLOBAIS
// ============================================

// Display OLED (endereco 0x3C, SDA, SCL)
SSD1306Wire display(0x3c, OLED_SDA, OLED_SCL);

// Contador de sequencia
uint32_t packetSequence = 0;

// Estatisticas
uint32_t packetsSent = 0;
uint32_t packetsAcked = 0;
int lastRssi = 0;

// ============================================
// PROTOTIPOS DE FUNCOES
// ============================================

void initVext();
void initOLED();
bool initLoRa();
void sendSensorData();
String createPacket(float temp, float hum, float battery);
void checkForAck();
float readTemperature();
float readHumidity();
float readBattery();
void updateDisplay(float temp, float hum, float bat, const char* status);
void blinkLED(int times, int delayMs);

// ============================================
// SETUP
// ============================================

void setup() {
    Serial.begin(115200);
    delay(1000);

    // Inicializa LED
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    // Inicializa Vext (alimentacao para perifericos)
    initVext();

    Serial.println("\n");
    Serial.println("╔════════════════════════════════════════╗");
    Serial.println("║   NO SENSOR LORA - HELTEC V2           ║");
    Serial.println("╠════════════════════════════════════════╣");
    Serial.printf("║ Node ID: %-29s ║\n", NODE_ID);
    Serial.printf("║ Frequencia: %.0f MHz                   ║\n", LORA_FREQUENCY / 1E6);
    Serial.printf("║ Intervalo TX: %d s                     ║\n", TX_INTERVAL / 1000);
    Serial.println("╚════════════════════════════════════════╝\n");

    // Inicializa OLED
    initOLED();
    display.clear();
    display.setFont(ArialMT_Plain_16);
    display.drawString(0, 0, "LoRa Node");
    display.setFont(ArialMT_Plain_10);
    display.drawString(0, 20, "Inicializando...");
    display.drawString(0, 35, NODE_ID);
    display.display();

    // Inicializa LoRa
    if (!initLoRa()) {
        Serial.println("ERRO FATAL: Falha ao inicializar LoRa!");
        display.clear();
        display.setFont(ArialMT_Plain_16);
        display.drawString(0, 20, "ERRO LoRa!");
        display.display();
        while (true) {
            blinkLED(5, 100);
            delay(1000);
        }
    }

    Serial.println("No sensor pronto!\n");
    blinkLED(3, 100);

    // Tela inicial
    display.clear();
    display.setFont(ArialMT_Plain_16);
    display.drawString(0, 0, "LoRa Pronto!");
    display.setFont(ArialMT_Plain_10);
    display.drawString(0, 20, "Aguardando...");
    display.display();
    delay(1000);
}

// ============================================
// LOOP PRINCIPAL
// ============================================

void loop() {
    // Le sensores
    float temperature = readTemperature();
    float humidity = readHumidity();
    float battery = readBattery();

    // Atualiza display antes de enviar
    updateDisplay(temperature, humidity, battery, "Enviando...");

    // Envia dados dos sensores
    sendSensorData();

    // Aguarda possivel ACK do gateway
    unsigned long startWait = millis();
    bool ackReceived = false;
    while (millis() - startWait < 2000) {
        checkForAck();
        delay(10);
    }

    // Atualiza display com status
    char status[32];
    snprintf(status, sizeof(status), "TX:%d ACK:%d", packetsSent, packetsAcked);
    updateDisplay(temperature, humidity, battery, status);

    // Modo sleep ou espera ate proximo envio
    Serial.printf("Aguardando %d segundos...\n\n", TX_INTERVAL / 1000);

    // Para economia de energia, pode usar deep sleep:
    // esp_sleep_enable_timer_wakeup(TX_INTERVAL * 1000ULL);
    // esp_deep_sleep_start();

    // Ou simplesmente delay
    delay(TX_INTERVAL);
}

// ============================================
// FUNCOES DE INICIALIZACAO
// ============================================

void initVext() {
    // Habilita alimentacao Vext (LOW = ligado, HIGH = desligado)
    pinMode(VEXT_PIN, OUTPUT);
    digitalWrite(VEXT_PIN, LOW);
    delay(100);
}

void initOLED() {
    // Reset do OLED
    pinMode(OLED_RST, OUTPUT);
    digitalWrite(OLED_RST, LOW);
    delay(50);
    digitalWrite(OLED_RST, HIGH);
    delay(50);

    // Inicializa display
    display.init();
    display.flipScreenVertically();
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_LEFT);
}

bool initLoRa() {
    Serial.println("[LoRa] Inicializando...");

    // Configura SPI para LoRa (Heltec usa pinos especificos)
    SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);

    // Configura pinos do LoRa
    LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);

    // Inicializa
    if (!LoRa.begin(LORA_FREQUENCY)) {
        Serial.println("[LoRa] ERRO: Falha na inicializacao!");
        return false;
    }

    // Configura parametros (devem ser iguais ao gateway!)
    LoRa.setSpreadingFactor(LORA_SF);
    LoRa.setSignalBandwidth(LORA_BW);
    LoRa.setCodingRate4(LORA_CR);
    LoRa.setTxPower(LORA_TX_POWER);
    LoRa.setSyncWord(LORA_SYNC_WORD);
    LoRa.enableCrc();

    Serial.println("[LoRa] Inicializado com sucesso!");
    Serial.printf("[LoRa] SF=%d, BW=%.0fkHz, CR=4/%d, TXPwr=%ddBm\n",
                  LORA_SF, LORA_BW / 1E3, LORA_CR, LORA_TX_POWER);

    return true;
}

// ============================================
// FUNCOES DE COMUNICACAO LORA
// ============================================

void sendSensorData() {
    // Le sensores (substitua por leituras reais)
    float temperature = readTemperature();
    float humidity = readHumidity();
    float battery = readBattery();

    // Cria pacote JSON
    String packet = createPacket(temperature, humidity, battery);

    Serial.println("--- Enviando Dados ---");
    Serial.printf("Temp: %.1f C\n", temperature);
    Serial.printf("Umid: %.1f %%\n", humidity);
    Serial.printf("Bat: %.2f V\n", battery);
    Serial.printf("Seq: %d\n", packetSequence);
    Serial.printf("Pacote: %s\n", packet.c_str());

    // Liga LED durante transmissao
    digitalWrite(LED_BUILTIN, HIGH);

    // Envia via LoRa
    LoRa.beginPacket();
    LoRa.print(packet);
    int result = LoRa.endPacket();

    digitalWrite(LED_BUILTIN, LOW);

    if (result) {
        Serial.println("Pacote enviado!");
        packetSequence++;
        packetsSent++;
    } else {
        Serial.println("ERRO no envio!");
    }

    // Volta para modo de recepcao (para ACK)
    LoRa.receive();
}

String createPacket(float temp, float hum, float battery) {
    JsonDocument doc;

    doc["id"] = NODE_ID;
    doc["type"] = "sensor";
    doc["seq"] = packetSequence;

    JsonObject data = doc["data"].to<JsonObject>();
    data["temp"] = round(temp * 10) / 10.0;  // 1 casa decimal
    data["hum"] = round(hum * 10) / 10.0;
    data["bat"] = round(battery * 100) / 100.0;  // 2 casas decimais

    String output;
    serializeJson(doc, output);

    return output;
}

void checkForAck() {
    int packetSize = LoRa.parsePacket();
    if (packetSize > 0) {
        String received = "";
        while (LoRa.available()) {
            received += (char)LoRa.read();
        }

        lastRssi = LoRa.packetRssi();

        Serial.printf("[ACK] Recebido: %s\n", received.c_str());
        Serial.printf("[ACK] RSSI: %d dBm, SNR: %.2f dB\n",
                      lastRssi, LoRa.packetSnr());

        // Verifica se e um ACK para este no
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, received);

        if (!error) {
            String type = doc["type"] | "";
            String to = doc["to"] | "";

            if (type == "ack" && to == NODE_ID) {
                bool ok = doc["ok"] | false;
                uint32_t seq = doc["seq"] | 0;

                if (ok) {
                    Serial.printf("[ACK] Confirmacao recebida para seq %d\n", seq);
                    packetsAcked++;
                    blinkLED(1, 50);
                } else {
                    Serial.printf("[ACK] Gateway reportou erro para seq %d\n", seq);
                }
            }
        }
    }
}

// ============================================
// FUNCOES DE LEITURA DE SENSORES
// ============================================
// Substitua estas funcoes pelas leituras reais
// dos seus sensores (DHT22, BME280, etc.)

float readTemperature() {
    // Simula temperatura entre 20 e 30 graus
    return 25.0 + (random(-50, 50) / 10.0);
}

float readHumidity() {
    // Simula umidade entre 40 e 80%
    return 60.0 + (random(-200, 200) / 10.0);
}

float readBattery() {
    // Le tensao real da bateria usando ADC
    // A Heltec V2 tem divisor de tensao no GPIO37
    // Fator de conversao: ADC * 2 * 3.3 / 4096

    uint16_t adcValue = analogRead(BATTERY_PIN);

    // Conversao para tensao (ajuste conforme calibracao)
    // Divisor de tensao 1:2, referencia 3.3V, ADC 12 bits
    float voltage = (adcValue / 4096.0) * 3.3 * 2.0;

    // Adiciona offset de calibracao se necessario
    // voltage += 0.1;

    // Se a leitura for muito baixa, pode ser que nao tem bateria
    // Nesse caso, simula valor
    if (voltage < 2.5) {
        return 3.7 + (random(-40, 50) / 100.0);
    }

    return voltage;
}

// ============================================
// FUNCOES DE INTERFACE
// ============================================

void updateDisplay(float temp, float hum, float bat, const char* status) {
    display.clear();

    // Titulo
    display.setFont(ArialMT_Plain_10);
    display.drawString(0, 0, "LoRa Node: ");
    display.drawString(65, 0, NODE_ID);

    // Linha separadora
    display.drawHorizontalLine(0, 12, 128);

    // Dados dos sensores
    display.setFont(ArialMT_Plain_10);

    char line[32];

    snprintf(line, sizeof(line), "Temp: %.1f C", temp);
    display.drawString(0, 16, line);

    snprintf(line, sizeof(line), "Umid: %.1f %%", hum);
    display.drawString(0, 28, line);

    snprintf(line, sizeof(line), "Bat: %.2fV", bat);
    display.drawString(0, 40, line);

    // Status na ultima linha
    display.drawHorizontalLine(0, 52, 128);
    display.drawString(0, 54, status);

    display.display();
}

void blinkLED(int times, int delayMs) {
    for (int i = 0; i < times; i++) {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(delayMs);
        digitalWrite(LED_BUILTIN, LOW);
        delay(delayMs);
    }
}
