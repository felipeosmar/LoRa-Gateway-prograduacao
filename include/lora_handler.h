#ifndef LORA_HANDLER_H
#define LORA_HANDLER_H

#include <Arduino.h>
#include <LoRa.h>
#include "config.h"

// Estrutura para pacote LoRa recebido
struct LoRaPacket {
    String payload;
    int rssi;
    float snr;
    unsigned long timestamp;
    bool valid;
};

class LoRaHandler {
public:
    LoRaHandler();

    // Inicializacao
    bool begin();

    // Recepcao
    bool available();
    LoRaPacket receive();
    LoRaPacket receivePacket();  // Le pacote sem chamar parsePacket novamente

    // Transmissao (para ACK ou comandos)
    bool send(const String& data);
    bool sendWithRetry(const String& data, int maxRetries = 3);

    // Configuracao em tempo de execucao
    void setFrequency(long frequency);
    void setSpreadingFactor(int sf);
    void setBandwidth(long bw);
    void setTxPower(int power);
    void setSyncWord(int sw);

    // Status
    int getLastRSSI();
    float getLastSNR();
    bool isInitialized();

    // Modo de operacao
    void enableReceiveMode();
    void sleep();
    void idle();

private:
    bool _initialized;
    int _lastRSSI;
    float _lastSNR;

    void configureRadio();
};

#endif // LORA_HANDLER_H
