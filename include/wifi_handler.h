#ifndef WIFI_HANDLER_H
#define WIFI_HANDLER_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "config.h"

// Estados da conexao WiFi
enum WiFiState {
    WIFI_STATE_DISCONNECTED,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_ERROR
};

class WiFiHandler {
public:
    WiFiHandler();

    // Inicializacao e conexao
    bool begin();
    bool connect();
    void disconnect();

    // Status
    bool isConnected();
    WiFiState getState();
    String getIP();
    int getRSSI();
    String getMAC();

    // Gerenciamento de conexao
    void checkConnection();
    void reconnect();

    // Envio de dados HTTP
    bool sendHTTPPost(const String& endpoint, const String& jsonPayload);
    bool sendHTTPGet(const String& endpoint, String& response);

    // Callback para eventos (opcional)
    void setConnectedCallback(void (*callback)());
    void setDisconnectedCallback(void (*callback)());

private:
    WiFiState _state;
    unsigned long _lastReconnectAttempt;
    String _ssid;
    String _password;

    void (*_connectedCallback)();
    void (*_disconnectedCallback)();

    void updateState(WiFiState newState);
};

#endif // WIFI_HANDLER_H
