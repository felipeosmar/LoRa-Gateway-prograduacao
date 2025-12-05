#include "wifi_handler.h"

WiFiHandler::WiFiHandler()
    : _state(WIFI_STATE_DISCONNECTED),
      _lastReconnectAttempt(0),
      _ssid(WIFI_SSID),
      _password(WIFI_PASSWORD),
      _connectedCallback(nullptr),
      _disconnectedCallback(nullptr) {
}

bool WiFiHandler::begin() {
    DEBUG_PRINTLN("[WiFi] Inicializando...");

    // Configura modo Station
    WiFi.mode(WIFI_STA);

    // Desabilita sleep do WiFi para melhor responsividade
    WiFi.setSleep(false);

    // Registra hostname
    WiFi.setHostname(GATEWAY_ID);

    DEBUG_PRINTF("[WiFi] MAC: %s\n", WiFi.macAddress().c_str());

    return connect();
}

bool WiFiHandler::connect() {
    if (_state == WIFI_STATE_CONNECTED) {
        return true;
    }

    updateState(WIFI_STATE_CONNECTING);

    DEBUG_PRINTF("[WiFi] Conectando a: %s\n", _ssid.c_str());

    WiFi.begin(_ssid.c_str(), _password.c_str());

    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - startTime > WIFI_CONNECT_TIMEOUT_MS) {
            DEBUG_PRINTLN("[WiFi] ERRO: Timeout de conexao!");
            updateState(WIFI_STATE_ERROR);
            return false;
        }
        delay(100);
        DEBUG_PRINT(".");
    }

    DEBUG_PRINTLN("");
    updateState(WIFI_STATE_CONNECTED);

    DEBUG_PRINTLN("[WiFi] Conectado!");
    DEBUG_PRINTF("[WiFi] IP: %s\n", WiFi.localIP().toString().c_str());
    DEBUG_PRINTF("[WiFi] RSSI: %d dBm\n", WiFi.RSSI());
    DEBUG_PRINTF("[WiFi] Gateway: %s\n", WiFi.gatewayIP().toString().c_str());

    if (_connectedCallback) {
        _connectedCallback();
    }

    return true;
}

void WiFiHandler::disconnect() {
    WiFi.disconnect(true);
    updateState(WIFI_STATE_DISCONNECTED);
    DEBUG_PRINTLN("[WiFi] Desconectado");

    if (_disconnectedCallback) {
        _disconnectedCallback();
    }
}

bool WiFiHandler::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

WiFiState WiFiHandler::getState() {
    // Atualiza estado baseado no status real
    if (WiFi.status() == WL_CONNECTED && _state != WIFI_STATE_CONNECTED) {
        updateState(WIFI_STATE_CONNECTED);
    } else if (WiFi.status() != WL_CONNECTED && _state == WIFI_STATE_CONNECTED) {
        updateState(WIFI_STATE_DISCONNECTED);
    }
    return _state;
}

String WiFiHandler::getIP() {
    if (isConnected()) {
        return WiFi.localIP().toString();
    }
    return "0.0.0.0";
}

int WiFiHandler::getRSSI() {
    if (isConnected()) {
        return WiFi.RSSI();
    }
    return 0;
}

String WiFiHandler::getMAC() {
    return WiFi.macAddress();
}

void WiFiHandler::checkConnection() {
    if (!isConnected()) {
        unsigned long now = millis();
        if (now - _lastReconnectAttempt > WIFI_RECONNECT_INTERVAL_MS) {
            DEBUG_PRINTLN("[WiFi] Conexao perdida, reconectando...");
            _lastReconnectAttempt = now;
            reconnect();
        }
    }
}

void WiFiHandler::reconnect() {
    disconnect();
    delay(100);
    connect();
}

bool WiFiHandler::sendHTTPPost(const String& endpoint, const String& jsonPayload) {
    if (!isConnected()) {
        DEBUG_PRINTLN("[HTTP] ERRO: WiFi nao conectado!");
        return false;
    }

    HTTPClient http;
    String url = String("http://") + SERVER_HOST + ":" + SERVER_PORT + endpoint;

    DEBUG_PRINTF("[HTTP] POST para: %s\n", url.c_str());
    DEBUG_PRINTF("[HTTP] Payload: %s\n", jsonPayload.c_str());

    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(HTTP_TIMEOUT_MS);

    int httpCode = http.POST(jsonPayload);

    if (httpCode > 0) {
        DEBUG_PRINTF("[HTTP] Resposta: %d\n", httpCode);

        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED) {
            String response = http.getString();
            DEBUG_PRINTF("[HTTP] Body: %s\n", response.c_str());
            http.end();
            return true;
        }
    } else {
        DEBUG_PRINTF("[HTTP] ERRO: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();
    return false;
}

bool WiFiHandler::sendHTTPGet(const String& endpoint, String& response) {
    if (!isConnected()) {
        DEBUG_PRINTLN("[HTTP] ERRO: WiFi nao conectado!");
        return false;
    }

    HTTPClient http;
    String url = String("http://") + SERVER_HOST + ":" + SERVER_PORT + endpoint;

    DEBUG_PRINTF("[HTTP] GET: %s\n", url.c_str());

    http.begin(url);
    http.setTimeout(HTTP_TIMEOUT_MS);

    int httpCode = http.GET();

    if (httpCode > 0) {
        DEBUG_PRINTF("[HTTP] Resposta: %d\n", httpCode);

        if (httpCode == HTTP_CODE_OK) {
            response = http.getString();
            DEBUG_PRINTF("[HTTP] Body: %s\n", response.c_str());
            http.end();
            return true;
        }
    } else {
        DEBUG_PRINTF("[HTTP] ERRO: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();
    return false;
}

void WiFiHandler::setConnectedCallback(void (*callback)()) {
    _connectedCallback = callback;
}

void WiFiHandler::setDisconnectedCallback(void (*callback)()) {
    _disconnectedCallback = callback;
}

void WiFiHandler::updateState(WiFiState newState) {
    if (_state != newState) {
        _state = newState;

        switch (_state) {
            case WIFI_STATE_DISCONNECTED:
                DEBUG_PRINTLN("[WiFi] Estado: DESCONECTADO");
                break;
            case WIFI_STATE_CONNECTING:
                DEBUG_PRINTLN("[WiFi] Estado: CONECTANDO");
                break;
            case WIFI_STATE_CONNECTED:
                DEBUG_PRINTLN("[WiFi] Estado: CONECTADO");
                break;
            case WIFI_STATE_ERROR:
                DEBUG_PRINTLN("[WiFi] Estado: ERRO");
                break;
        }
    }
}
