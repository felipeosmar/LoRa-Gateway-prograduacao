#include "web_server.h"
#include <time.h>

WebServer::WebServer(uint16_t port) : server(port), serverPort(port) {
    packetsReceived = 0;
    packetsForwarded = 0;
    packetsError = 0;
    wifiRssi = 0;
    uptimeMs = 0;
    deviceCount = 0;
    packetHistoryIndex = 0;
    packetHistoryCount = 0;
    timeSynced = false;
    bootTime = 0;

    // Inicializa array de dispositivos
    for (int i = 0; i < MAX_DEVICES; i++) {
        devices[i].active = false;
        devices[i].packets = 0;
    }
}

bool WebServer::begin() {
    DEBUG_PRINTLN("\n=== Inicializando Servidor Web ===");

    // Inicializa LittleFS
    if (!LittleFS.begin(true)) {
        DEBUG_PRINTLN("ERRO: Falha ao montar LittleFS!");
        return false;
    }

    DEBUG_PRINTLN("LittleFS montado com sucesso");

    // Lista arquivos no LittleFS para debug
    DEBUG_PRINTLN("Arquivos em LittleFS:");
    File root = LittleFS.open("/");
    File file = root.openNextFile();
    while (file) {
        DEBUG_PRINTF("  %s (%d bytes)\n", file.name(), file.size());
        file = root.openNextFile();
    }

    // Configura rotas
    setupRoutes();

    // Inicia servidor
    server.begin();
    DEBUG_PRINTF("Servidor web iniciado na porta %d\n", serverPort);
    DEBUG_PRINTLN("Acesse: http://<IP_DO_GATEWAY>/");

    return true;
}

void WebServer::setupRoutes() {
    // IMPORTANTE: Registrar rotas da API ANTES do serveStatic
    // O AsyncWebServer processa rotas na ordem de registro

    // API REST para estatisticas
    server.on("/api/stats", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->handleStats(request);
    });

    // API REST para dispositivos
    server.on("/api/devices", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->handleDevices(request);
    });

    // API para sincronizacao de tempo (POST)
    server.on("/api/time", HTTP_POST, [this](AsyncWebServerRequest* request) {
        this->handleTimeSync(request);
    });

    // API para verificar status do tempo (GET)
    server.on("/api/time", HTTP_GET, [this](AsyncWebServerRequest* request) {
        JsonDocument doc;
        doc["synced"] = timeSynced;
        doc["boot_time"] = bootTime;
        doc["current_time"] = bootTime > 0 ? bootTime + (millis() / 1000) : 0;
        doc["uptime_s"] = millis() / 1000;

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // Serve arquivos estaticos do LittleFS (DEPOIS das APIs)
    server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

    // Handler para 404
    server.onNotFound([this](AsyncWebServerRequest* request) {
        this->handleNotFound(request);
    });
}

void WebServer::handleStats(AsyncWebServerRequest* request) {
    JsonDocument doc;

    doc["gateway_id"] = GATEWAY_ID;
    doc["uptime_s"] = uptimeMs / 1000;
    doc["packets_rx"] = packetsReceived;
    doc["packets_fwd"] = packetsForwarded;
    doc["packets_err"] = packetsError;
    doc["wifi_rssi"] = wifiRssi;
    doc["free_heap"] = ESP.getFreeHeap();

    // Info de tempo
    doc["time_synced"] = timeSynced;
    if (timeSynced) {
        doc["current_time"] = bootTime + (millis() / 1000);
    }

    // Configuracao LoRa
    JsonObject lora = doc["lora"].to<JsonObject>();
    lora["freq"] = LORA_FREQUENCY;
    lora["sf"] = LORA_SF;
    lora["bw"] = LORA_BW;
    lora["cr"] = LORA_CR;
    lora["tx_power"] = LORA_TX_POWER;
    lora["sync_word"] = LORA_SYNC_WORD;

    String response;
    serializeJson(doc, response);

    request->send(200, "application/json", response);
}

void WebServer::handleDevices(AsyncWebServerRequest* request) {
    // Limpa dispositivos inativos antes de responder
    cleanupInactiveDevices();

    JsonDocument doc;

    // Envia uptime atual para calculo de "tempo atras" no frontend
    unsigned long currentMillis = millis();
    doc["uptime_ms"] = currentMillis;

    // Array de dispositivos
    JsonArray devicesArray = doc["devices"].to<JsonArray>();
    for (int i = 0; i < MAX_DEVICES; i++) {
        if (devices[i].active) {
            JsonObject dev = devicesArray.add<JsonObject>();
            dev["id"] = devices[i].id;
            dev["type"] = devices[i].type;
            dev["rssi"] = devices[i].rssi;
            dev["snr"] = devices[i].snr;
            dev["packets"] = devices[i].packets;
            // Envia millis do ultimo contato (para calcular diferenca)
            dev["last_seen_ms"] = devices[i].lastSeen;
        }
    }

    // Info de tempo para calcular horario dos pacotes
    doc["time_synced"] = timeSynced;
    doc["boot_time"] = bootTime;

    // Array de ultimos pacotes
    JsonArray packetsArray = doc["lastPackets"].to<JsonArray>();

    // Percorre o historico do mais recente ao mais antigo
    for (int i = 0; i < packetHistoryCount; i++) {
        int idx = (packetHistoryIndex - 1 - i + MAX_PACKET_HISTORY) % MAX_PACKET_HISTORY;

        JsonObject pkt = packetsArray.add<JsonObject>();
        pkt["node_id"] = packetHistory[idx].nodeId;
        pkt["rssi"] = packetHistory[idx].rssi;
        pkt["snr"] = packetHistory[idx].snr;
        pkt["timestamp_ms"] = packetHistory[idx].timestamp;  // Em milissegundos desde boot

        // Copia dados do pacote
        JsonObject data = pkt["data"].to<JsonObject>();
        for (JsonPair kv : packetHistory[idx].data.as<JsonObject>()) {
            data[kv.key()] = kv.value();
        }
    }

    String response;
    serializeJson(doc, response);

    request->send(200, "application/json", response);
}

void WebServer::handleTimeSync(AsyncWebServerRequest* request) {
    // Verifica se tem o parametro timestamp
    if (!request->hasParam("timestamp", true)) {
        request->send(400, "application/json", "{\"error\":\"timestamp required\"}");
        return;
    }

    // Obtem o timestamp do navegador (em segundos Unix)
    String timestampStr = request->getParam("timestamp", true)->value();
    time_t browserTime = (time_t)timestampStr.toInt();

    if (browserTime > 1700000000) {  // Sanity check: apos Nov 2023
        // Calcula o tempo de boot baseado no timestamp atual e uptime
        unsigned long uptimeSec = millis() / 1000;
        bootTime = browserTime - uptimeSec;
        timeSynced = true;

        // Configura o relogio do sistema
        struct timeval tv;
        tv.tv_sec = browserTime;
        tv.tv_usec = 0;
        settimeofday(&tv, NULL);

        DEBUG_PRINTF("[WebServer] Tempo sincronizado! Browser: %lu, Boot: %lu\n",
                     (unsigned long)browserTime, (unsigned long)bootTime);

        // Mostra hora formatada
        struct tm timeinfo;
        localtime_r(&browserTime, &timeinfo);
        DEBUG_PRINTF("[WebServer] Hora atual: %02d:%02d:%02d %02d/%02d/%04d\n",
                     timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
                     timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);

        JsonDocument doc;
        doc["success"] = true;
        doc["synced_time"] = browserTime;
        doc["boot_time"] = bootTime;

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    } else {
        request->send(400, "application/json", "{\"error\":\"invalid timestamp\"}");
    }
}

void WebServer::handleNotFound(AsyncWebServerRequest* request) {
    request->send(404, "text/plain", "Pagina nao encontrada");
}

void WebServer::updateStats(uint32_t packetsRx, uint32_t packetsFwd, uint32_t packetsErr,
                             int wifiRssiVal, unsigned long uptimeMsVal) {
    packetsReceived = packetsRx;
    packetsForwarded = packetsFwd;
    packetsError = packetsErr;
    wifiRssi = wifiRssiVal;
    uptimeMs = uptimeMsVal;
}

void WebServer::logPacket(const String& nodeId, const String& nodeType,
                           const JsonDocument& data, int rssi, float snr) {
    // Atualiza informacoes do dispositivo
    updateDevice(nodeId, nodeType, rssi, snr);

    // Adiciona ao historico de pacotes
    addPacketToHistory(nodeId, data, rssi, snr);
}

int WebServer::findDeviceIndex(const String& nodeId) {
    for (int i = 0; i < MAX_DEVICES; i++) {
        if (devices[i].active && devices[i].id == nodeId) {
            return i;
        }
    }
    return -1;
}

void WebServer::updateDevice(const String& nodeId, const String& nodeType, int rssi, float snr) {
    int idx = findDeviceIndex(nodeId);

    if (idx >= 0) {
        // Dispositivo ja existe, atualiza
        devices[idx].rssi = rssi;
        devices[idx].snr = snr;
        devices[idx].packets++;
        devices[idx].lastSeen = millis();
    } else {
        // Novo dispositivo, procura slot livre
        for (int i = 0; i < MAX_DEVICES; i++) {
            if (!devices[i].active) {
                devices[i].id = nodeId;
                devices[i].type = nodeType;
                devices[i].rssi = rssi;
                devices[i].snr = snr;
                devices[i].packets = 1;
                devices[i].lastSeen = millis();
                devices[i].active = true;
                deviceCount++;
                DEBUG_PRINTF("Novo dispositivo registrado: %s\n", nodeId.c_str());
                break;
            }
        }
    }
}

void WebServer::addPacketToHistory(const String& nodeId, const JsonDocument& data, int rssi, float snr) {
    // Buffer circular
    packetHistory[packetHistoryIndex].nodeId = nodeId;
    packetHistory[packetHistoryIndex].data.clear();

    // Copia dados
    for (JsonPairConst kv : data.as<JsonObjectConst>()) {
        packetHistory[packetHistoryIndex].data[kv.key()] = kv.value();
    }

    packetHistory[packetHistoryIndex].rssi = rssi;
    packetHistory[packetHistoryIndex].snr = snr;
    packetHistory[packetHistoryIndex].timestamp = millis();

    packetHistoryIndex = (packetHistoryIndex + 1) % MAX_PACKET_HISTORY;
    if (packetHistoryCount < MAX_PACKET_HISTORY) {
        packetHistoryCount++;
    }
}

void WebServer::cleanupInactiveDevices() {
    unsigned long now = millis();
    for (int i = 0; i < MAX_DEVICES; i++) {
        if (devices[i].active) {
            if (now - devices[i].lastSeen > DEVICE_TIMEOUT_MS) {
                DEBUG_PRINTF("Dispositivo %s marcado como inativo\n", devices[i].id.c_str());
                devices[i].active = false;
                deviceCount--;
            }
        }
    }
}

uint32_t WebServer::getDeviceCount() const {
    uint32_t count = 0;
    for (int i = 0; i < MAX_DEVICES; i++) {
        if (devices[i].active) count++;
    }
    return count;
}
