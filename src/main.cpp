/**
 * Gateway LoRa - Placa JVtech MIJ
 *
 * Funcao: Receber dados de nos sensores via LoRa e encaminhar
 * para servidor backend via WiFi/HTTP.
 *
 * Hardware: ESP32 + SX1276/SX1278 (Modulo MIJ JVtech)
 * Frequencia: 915 MHz (AU915 - Brasil)
 *
 * Autor: [Seu Nome]
 * Data: Dezembro 2024
 */

#include <Arduino.h>
#include "config.h"
#include "lora_handler.h"
#include "wifi_handler.h"
#include "protocol.h"
#include "web_server.h"

// Instancias globais
LoRaHandler lora;
WiFiHandler wifi;
Protocol protocol;
WebServer webServer(80);

// Estatisticas
uint32_t packetsReceived = 0;
uint32_t packetsForwarded = 0;
uint32_t packetsError = 0;
unsigned long lastStatusReport = 0;

// LED de status
bool ledState = false;
unsigned long lastLedBlink = 0;
const int LED_BLINK_INTERVAL = 1000;

// Prototipos
void setupLED();
void updateLED();
void blinkLED(int times, int delayMs);
void processLoRaPacket(const LoRaPacket& packet);
void sendStatusReport();
void printStartupInfo();

void setup() {
    // Inicializa Serial
    Serial.begin(DEBUG_BAUD);
    delay(1000);

    printStartupInfo();

    // Inicializa LED
    setupLED();
    blinkLED(3, 100);

    // Inicializa WiFi
    DEBUG_PRINTLN("\n=== Inicializando WiFi ===");
    if (!wifi.begin()) {
        DEBUG_PRINTLN("ERRO: Falha ao conectar WiFi!");
        DEBUG_PRINTLN("Continuando sem WiFi...");
    }

    // Inicializa LoRa
    DEBUG_PRINTLN("\n=== Inicializando LoRa ===");
    if (!lora.begin()) {
        DEBUG_PRINTLN("ERRO FATAL: Falha ao inicializar LoRa!");
        DEBUG_PRINTLN("Verifique as conexoes do modulo.");

        // Pisca LED rapidamente para indicar erro
        while (true) {
            blinkLED(5, 50);
            delay(1000);
        }
    }

    // Inicializa servidor web (se WiFi conectado)
    if (wifi.isConnected()) {
        DEBUG_PRINTLN("\n=== Inicializando Servidor Web ===");
        if (!webServer.begin()) {
            DEBUG_PRINTLN("AVISO: Falha ao inicializar servidor web");
        } else {
            DEBUG_PRINTF("Dashboard disponivel em: http://%s/\n", WiFi.localIP().toString().c_str());
        }
    }

    DEBUG_PRINTLN("\n=== Gateway Pronto ===");
    DEBUG_PRINTLN("Aguardando pacotes LoRa...\n");

    // Indica que esta pronto
    blinkLED(2, 200);
}

void loop() {
    // Verifica conexao WiFi periodicamente
    wifi.checkConnection();

    // Verifica se ha pacotes LoRa disponiveis
    // Usa lora.available() que internamente chama parsePacket()
    if (lora.available()) {
        LoRaPacket packet = lora.receivePacket();

        if (packet.valid) {
            packetsReceived++;
            processLoRaPacket(packet);

            // Pisca LED ao receber pacote
            blinkLED(1, 50);
        }
    }

    // Envia relatorio de status periodicamente
    if (millis() - lastStatusReport > STATUS_REPORT_INTERVAL_MS) {
        sendStatusReport();
        lastStatusReport = millis();
    }

    // Atualiza estatisticas do servidor web
    webServer.updateStats(packetsReceived, packetsForwarded, packetsError,
                          wifi.getRSSI(), millis());

    // Atualiza LED de status
    updateLED();

    // Pequeno delay para nao sobrecarregar
    delay(10);
}

void setupLED() {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
}

void updateLED() {
    // Pisca LED lentamente para indicar que esta funcionando
    if (millis() - lastLedBlink > LED_BLINK_INTERVAL) {
        ledState = !ledState;
        digitalWrite(LED_PIN, ledState ? HIGH : LOW);
        lastLedBlink = millis();
    }
}

void blinkLED(int times, int delayMs) {
    for (int i = 0; i < times; i++) {
        digitalWrite(LED_PIN, HIGH);
        delay(delayMs);
        digitalWrite(LED_PIN, LOW);
        delay(delayMs);
    }
}

void processLoRaPacket(const LoRaPacket& packet) {
    DEBUG_PRINTLN("\n--- Pacote LoRa Recebido ---");
    DEBUG_PRINTF("Payload: %s\n", packet.payload.c_str());
    DEBUG_PRINTF("RSSI: %d dBm\n", packet.rssi);
    DEBUG_PRINTF("SNR: %.2f dB\n", packet.snr);

    // Valida o pacote
    if (!protocol.validatePacket(packet.payload)) {
        DEBUG_PRINTLN("ERRO: Pacote invalido!");
        packetsError++;
        return;
    }

    // Faz parsing do pacote
    SensorData sensorData = protocol.parseLoRaPacket(packet.payload);

    if (!sensorData.valid) {
        DEBUG_PRINTLN("ERRO: Falha no parsing!");
        packetsError++;
        return;
    }

    // Registra pacote no servidor web para dashboard
    webServer.logPacket(sensorData.nodeId, sensorData.nodeType,
                        sensorData.data, packet.rssi, packet.snr);

    // Cria payload para o servidor
    String serverPayload = protocol.createServerPayload(sensorData, packet.rssi, packet.snr);

    // Envia para o servidor via HTTP
    if (wifi.isConnected()) {
        DEBUG_PRINTLN("Enviando para servidor...");

        if (wifi.sendHTTPPost(SERVER_ENDPOINT, serverPayload)) {
            DEBUG_PRINTLN("Dados enviados com sucesso!");
            packetsForwarded++;

            // Envia ACK para o no (opcional)
            String ack = protocol.createAck(sensorData.nodeId, sensorData.sequence, true);
            lora.send(ack);
        } else {
            DEBUG_PRINTLN("ERRO: Falha ao enviar para servidor!");
            packetsError++;
        }
    } else {
        DEBUG_PRINTLN("AVISO: WiFi desconectado, dados nao enviados");
        packetsError++;
    }

    DEBUG_PRINTLN("----------------------------\n");
}

void sendStatusReport() {
    DEBUG_PRINTLN("\n=== Status do Gateway ===");
    DEBUG_PRINTF("Uptime: %lu s\n", millis() / 1000);
    DEBUG_PRINTF("Pacotes recebidos: %d\n", packetsReceived);
    DEBUG_PRINTF("Pacotes encaminhados: %d\n", packetsForwarded);
    DEBUG_PRINTF("Pacotes com erro: %d\n", packetsError);
    DEBUG_PRINTF("WiFi: %s (RSSI: %d dBm)\n",
                 wifi.isConnected() ? "Conectado" : "Desconectado",
                 wifi.getRSSI());
    DEBUG_PRINTF("Heap livre: %d bytes\n", ESP.getFreeHeap());
    DEBUG_PRINTLN("=========================\n");

    // Envia status para o servidor
    if (wifi.isConnected()) {
        String statusPayload = protocol.createGatewayStatus(
            wifi.getRSSI(),
            packetsReceived,
            packetsForwarded,
            millis()
        );

        wifi.sendHTTPPost("/api/gateway-status", statusPayload);
    }
}

void printStartupInfo() {
    DEBUG_PRINTLN("\n");
    DEBUG_PRINTLN("╔════════════════════════════════════════╗");
    DEBUG_PRINTLN("║     GATEWAY LORA - JVTECH MIJ          ║");
    DEBUG_PRINTLN("╠════════════════════════════════════════╣");
    DEBUG_PRINTF("║ Gateway ID: %-26s ║\n", GATEWAY_ID);
    DEBUG_PRINTF("║ Frequencia: %.0f MHz                   ║\n", LORA_FREQUENCY / 1E6);
    DEBUG_PRINTF("║ SF: %d  BW: %.0f kHz  CR: 4/%d            ║\n",
                 LORA_SF, LORA_BW / 1E3, LORA_CR);
    DEBUG_PRINTF("║ Potencia TX: %d dBm                     ║\n", LORA_TX_POWER);
    DEBUG_PRINTLN("╚════════════════════════════════════════╝");
}
