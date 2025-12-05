/**
 * No Sensor LoRa - Monitoramento de Maquina Industrial
 *
 * Envia dados de uma maquina para o Gateway LoRa JVtech
 * usando protocolo JSON.
 *
 * Dados transmitidos:
 * - macAddress: Endereco MAC do ESP32
 * - machineId: ID da maquina (configuravel)
 * - timestamp: Tempo em segundos desde boot
 * - digitalInputs: 4 entradas digitais (di1-di4)
 * - analogInputs: 2 entradas analogicas (ai1-ai2)
 * - temperature: Temperatura interna do ESP32
 * - trigger: "event" se houve mudanca ou "periodic" se transmissao periodica
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

// Para temperatura interna do ESP32
#ifdef __cplusplus
extern "C" {
#endif
uint8_t temprature_sens_read();
#ifdef __cplusplus
}
#endif

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

// Controle de energia Vext (para perifericos externos)
#ifndef VEXT_PIN
#define VEXT_PIN 21
#endif

// ============================================
// CONFIGURACOES DE ENTRADAS
// ============================================

// Pinos das entradas digitais (ajuste conforme seu hardware)
#define DI1_PIN 12   // Entrada Digital 1
#define DI2_PIN 13   // Entrada Digital 2
#define DI3_PIN 32   // Entrada Digital 3
#define DI4_PIN 33   // Entrada Digital 4

// Pinos das entradas analogicas
#define AI1_PIN 36   // Entrada Analogica 1 (SVP)
#define AI2_PIN 39   // Entrada Analogica 2 (SVN)

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

// ID da maquina (ajuste para cada dispositivo)
#ifndef MACHINE_ID
#define MACHINE_ID "M001"
#endif

// Sync Word (deve ser igual ao gateway)
#define LORA_SYNC_WORD 0x20

// Intervalo de transmissao periodica (ms)
#define TX_INTERVAL 30000  // 30 segundos

// Debounce para deteccao de eventos (ms)
#define DEBOUNCE_TIME 50

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

// Estado das entradas digitais
bool lastDI1 = false;
bool lastDI2 = false;
bool lastDI3 = false;
bool lastDI4 = false;

// Ultimo envio e estados anteriores para deteccao de eventos
unsigned long lastTxTime = 0;
bool inputChanged = false;

// MAC Address
String macAddress = "";

// ============================================
// PROTOTIPOS DE FUNCOES
// ============================================

void initVext();
void initOLED();
bool initLoRa();
void initInputs();
String getMacAddress();
void sendMachineData(const char* trigger);
String createPacket(const char* trigger);
void checkForAck();
float readInternalTemperature();
bool readDigitalInputs(bool &di1, bool &di2, bool &di3, bool &di4);
void readAnalogInputs(uint16_t &ai1, uint16_t &ai2);
bool checkInputChanges();
void updateDisplay(bool di1, bool di2, bool di3, bool di4,
                   uint16_t ai1, uint16_t ai2, float temp, const char* status);
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

    // Obtem MAC Address
    macAddress = getMacAddress();

    Serial.println("\n");
    Serial.println("╔════════════════════════════════════════╗");
    Serial.println("║   MONITOR DE MAQUINA INDUSTRIAL        ║");
    Serial.println("╠════════════════════════════════════════╣");
    Serial.printf("║ Machine ID: %-26s ║\n", MACHINE_ID);
    Serial.printf("║ MAC: %-33s ║\n", macAddress.c_str());
    Serial.printf("║ Frequencia: %.0f MHz                   ║\n", LORA_FREQUENCY / 1E6);
    Serial.printf("║ TX Periodico: %d s                    ║\n", TX_INTERVAL / 1000);
    Serial.println("╚════════════════════════════════════════╝\n");

    // Inicializa OLED
    initOLED();
    display.clear();
    display.setFont(ArialMT_Plain_16);
    display.drawString(0, 0, "Maquina");
    display.setFont(ArialMT_Plain_10);
    display.drawString(0, 20, "Inicializando...");
    display.drawString(0, 35, MACHINE_ID);
    display.display();

    // Inicializa entradas
    initInputs();

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

    // Le estado inicial das entradas digitais
    readDigitalInputs(lastDI1, lastDI2, lastDI3, lastDI4);

    Serial.println("Monitor de maquina pronto!\n");
    blinkLED(3, 100);

    // Envia primeiro pacote como periodico
    sendMachineData("periodic");
    lastTxTime = millis();

    // Tela inicial
    display.clear();
    display.setFont(ArialMT_Plain_16);
    display.drawString(0, 0, "Pronto!");
    display.setFont(ArialMT_Plain_10);
    display.drawString(0, 20, MACHINE_ID);
    display.display();
    delay(1000);
}

// ============================================
// LOOP PRINCIPAL
// ============================================

void loop() {
    // Verifica se houve mudanca nas entradas digitais
    bool eventTriggered = checkInputChanges();

    // Verifica se e hora da transmissao periodica
    bool periodicTrigger = (millis() - lastTxTime >= TX_INTERVAL);

    // Le estado atual das entradas
    bool di1, di2, di3, di4;
    uint16_t ai1, ai2;
    readDigitalInputs(di1, di2, di3, di4);
    readAnalogInputs(ai1, ai2);
    float temperature = readInternalTemperature();

    // Atualiza display
    char status[32];
    snprintf(status, sizeof(status), "TX:%d", packetsSent);
    updateDisplay(di1, di2, di3, di4, ai1, ai2, temperature, status);

    // Envia dados se houve evento ou e hora da transmissao periodica
    if (eventTriggered) {
        Serial.println(">>> EVENTO: Mudanca detectada nas entradas!");
        sendMachineData("event");
        lastTxTime = millis();
    } else if (periodicTrigger) {
        Serial.println(">>> TX Periodico");
        sendMachineData("periodic");
        lastTxTime = millis();
    }

    // Verifica ACK do gateway
    checkForAck();

    // Pequeno delay para nao sobrecarregar
    delay(50);
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

void initInputs() {
    // Configura entradas digitais com pull-up interno
    pinMode(DI1_PIN, INPUT_PULLUP);
    pinMode(DI2_PIN, INPUT_PULLUP);
    pinMode(DI3_PIN, INPUT_PULLUP);
    pinMode(DI4_PIN, INPUT_PULLUP);

    // Entradas analogicas nao precisam de configuracao especial
    // mas vamos garantir que estao no modo correto
    analogReadResolution(12);  // 12 bits (0-4095)
    analogSetAttenuation(ADC_11db);  // 0-3.3V range

    Serial.println("[IO] Entradas configuradas:");
    Serial.printf("  DI1: GPIO%d\n", DI1_PIN);
    Serial.printf("  DI2: GPIO%d\n", DI2_PIN);
    Serial.printf("  DI3: GPIO%d\n", DI3_PIN);
    Serial.printf("  DI4: GPIO%d\n", DI4_PIN);
    Serial.printf("  AI1: GPIO%d\n", AI1_PIN);
    Serial.printf("  AI2: GPIO%d\n", AI2_PIN);
}

String getMacAddress() {
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);

    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    return String(macStr);
}

// ============================================
// FUNCOES DE LEITURA DE ENTRADAS
// ============================================

bool readDigitalInputs(bool &di1, bool &di2, bool &di3, bool &di4) {
    // Le entradas (invertido porque usamos pull-up)
    di1 = !digitalRead(DI1_PIN);
    di2 = !digitalRead(DI2_PIN);
    di3 = !digitalRead(DI3_PIN);
    di4 = !digitalRead(DI4_PIN);

    return true;
}

void readAnalogInputs(uint16_t &ai1, uint16_t &ai2) {
    // Le valores analogicos (0-4095)
    ai1 = analogRead(AI1_PIN);
    ai2 = analogRead(AI2_PIN);
}

float readInternalTemperature() {
    // Le temperatura interna do ESP32
    // A funcao retorna em Fahrenheit, convertemos para Celsius
    uint8_t tempF = temprature_sens_read();
    float tempC = (tempF - 32) / 1.8;

    // Aplica offset de calibracao (ajuste conforme necessario)
    // A temperatura interna do ESP32 costuma ser maior que a ambiente
    tempC -= 20.0;  // Offset tipico

    return tempC;
}

bool checkInputChanges() {
    static unsigned long lastChangeTime = 0;

    bool di1, di2, di3, di4;
    readDigitalInputs(di1, di2, di3, di4);

    // Verifica se alguma entrada mudou
    bool changed = (di1 != lastDI1) || (di2 != lastDI2) ||
                   (di3 != lastDI3) || (di4 != lastDI4);

    if (changed && (millis() - lastChangeTime > DEBOUNCE_TIME)) {
        // Atualiza estados anteriores
        lastDI1 = di1;
        lastDI2 = di2;
        lastDI3 = di3;
        lastDI4 = di4;
        lastChangeTime = millis();

        Serial.printf("[IO] Mudanca: DI1=%d DI2=%d DI3=%d DI4=%d\n",
                      di1, di2, di3, di4);

        return true;
    }

    return false;
}

// ============================================
// FUNCOES DE COMUNICACAO LORA
// ============================================

void sendMachineData(const char* trigger) {
    // Cria pacote JSON
    String packet = createPacket(trigger);

    Serial.println("--- Enviando Dados da Maquina ---");
    Serial.printf("Machine ID: %s\n", MACHINE_ID);
    Serial.printf("MAC: %s\n", macAddress.c_str());
    Serial.printf("Trigger: %s\n", trigger);
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

String createPacket(const char* trigger) {
    JsonDocument doc;

    // Identificacao
    doc["id"] = MACHINE_ID;  // ID para o gateway
    doc["type"] = "machine";
    doc["seq"] = packetSequence;

    // Dados da maquina
    JsonObject data = doc["data"].to<JsonObject>();

    data["macAddress"] = macAddress;
    data["machineId"] = MACHINE_ID;
    data["timestamp"] = millis() / 1000;  // Segundos desde boot

    // Entradas digitais
    bool di1, di2, di3, di4;
    readDigitalInputs(di1, di2, di3, di4);

    JsonObject digitalInputs = data["digitalInputs"].to<JsonObject>();
    digitalInputs["di1"] = di1;
    digitalInputs["di2"] = di2;
    digitalInputs["di3"] = di3;
    digitalInputs["di4"] = di4;

    // Entradas analogicas
    uint16_t ai1, ai2;
    readAnalogInputs(ai1, ai2);

    JsonObject analogInputs = data["analogInputs"].to<JsonObject>();
    analogInputs["ai1"] = ai1;
    analogInputs["ai2"] = ai2;

    // Temperatura interna
    data["temperature"] = round(readInternalTemperature() * 10) / 10.0;

    // Trigger (event ou periodic)
    data["trigger"] = trigger;

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

        // Verifica se e um ACK para esta maquina
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, received);

        if (!error) {
            String type = doc["type"] | "";
            String to = doc["to"] | "";

            if (type == "ack" && to == MACHINE_ID) {
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
// FUNCOES DE INTERFACE
// ============================================

void updateDisplay(bool di1, bool di2, bool di3, bool di4,
                   uint16_t ai1, uint16_t ai2, float temp, const char* status) {
    display.clear();

    // Titulo
    display.setFont(ArialMT_Plain_10);
    display.drawString(0, 0, MACHINE_ID);
    display.drawString(70, 0, status);

    // Linha separadora
    display.drawHorizontalLine(0, 12, 128);

    // Entradas digitais
    char line[32];
    snprintf(line, sizeof(line), "DI: %d %d %d %d", di1, di2, di3, di4);
    display.drawString(0, 15, line);

    // Entradas analogicas
    snprintf(line, sizeof(line), "AI1:%4d AI2:%4d", ai1, ai2);
    display.drawString(0, 27, line);

    // Temperatura
    snprintf(line, sizeof(line), "Temp: %.1f C", temp);
    display.drawString(0, 39, line);

    // Linha separadora
    display.drawHorizontalLine(0, 52, 128);

    // MAC resumido
    display.setFont(ArialMT_Plain_10);
    display.drawString(0, 54, macAddress.substring(9));  // Ultimos 8 chars

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
