#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "config.h"

// ============================================
// SERVIDOR WEB PARA DASHBOARD DO GATEWAY LORA
// ============================================

// Numero maximo de dispositivos rastreados
#define MAX_DEVICES 20

// Numero maximo de pacotes no historico
#define MAX_PACKET_HISTORY 30

// Tempo para considerar dispositivo offline (ms)
#define DEVICE_TIMEOUT_MS 300000  // 5 minutos

// Estrutura para informacoes de dispositivo LoRa
struct DeviceInfo {
    String id;
    String type;
    int rssi;
    float snr;
    uint32_t packets;
    unsigned long lastSeen;  // millis() do ultimo contato
    bool active;
};

// Estrutura para historico de pacotes
struct PacketLogEntry {
    String nodeId;
    JsonDocument data;
    int rssi;
    float snr;
    unsigned long timestamp;
};

class WebServer {
public:
    WebServer(uint16_t port = 80);

    // Inicializacao
    bool begin();

    // Atualiza estatisticas (chamar do main.cpp)
    void updateStats(uint32_t packetsRx, uint32_t packetsFwd, uint32_t packetsErr,
                     int wifiRssi, unsigned long uptimeMs);

    // Sincronizacao de tempo
    bool isTimeSynced() const { return timeSynced; }
    time_t getBootTime() const { return bootTime; }

    // Registra pacote recebido
    void logPacket(const String& nodeId, const String& nodeType,
                   const JsonDocument& data, int rssi, float snr);

    // Getters para estatisticas
    uint32_t getDeviceCount() const;
    const DeviceInfo* getDevices() const { return devices; }

private:
    AsyncWebServer server;
    uint16_t serverPort;

    // Estatisticas do gateway
    uint32_t packetsReceived;
    uint32_t packetsForwarded;
    uint32_t packetsError;
    int wifiRssi;
    unsigned long uptimeMs;

    // Lista de dispositivos
    DeviceInfo devices[MAX_DEVICES];
    uint8_t deviceCount;

    // Historico de pacotes
    PacketLogEntry packetHistory[MAX_PACKET_HISTORY];
    uint8_t packetHistoryIndex;
    uint8_t packetHistoryCount;

    // Configuracao de rotas
    void setupRoutes();

    // Handlers de rotas
    void handleRoot(AsyncWebServerRequest* request);
    void handleStats(AsyncWebServerRequest* request);
    void handleDevices(AsyncWebServerRequest* request);
    void handleTimeSync(AsyncWebServerRequest* request);
    void handleNotFound(AsyncWebServerRequest* request);

    // Sincronizacao de tempo
    bool timeSynced;
    time_t bootTime;  // Timestamp Unix do momento do boot

    // Utilitarios
    int findDeviceIndex(const String& nodeId);
    void updateDevice(const String& nodeId, const String& nodeType, int rssi, float snr);
    void addPacketToHistory(const String& nodeId, const JsonDocument& data, int rssi, float snr);
    void cleanupInactiveDevices();
};

#endif // WEB_SERVER_H
