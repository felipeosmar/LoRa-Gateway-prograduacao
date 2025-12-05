#include "lora_handler.h"

LoRaHandler::LoRaHandler()
    : _initialized(false), _lastRSSI(0), _lastSNR(0.0) {
}

bool LoRaHandler::begin() {
    DEBUG_PRINTLN("[LoRa] Inicializando...");

    // Configura pinos SPI para LoRa
    SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);

    // Configura pinos do modulo LoRa
    LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);

    // Tenta inicializar na frequencia configurada
    if (!LoRa.begin(LORA_FREQUENCY)) {
        DEBUG_PRINTLN("[LoRa] ERRO: Falha na inicializacao!");
        DEBUG_PRINTLN("[LoRa] Verifique conexoes e alimentacao do modulo.");
        return false;
    }

    // Configura parametros do radio
    configureRadio();

    _initialized = true;
    DEBUG_PRINTLN("[LoRa] Inicializado com sucesso!");
    DEBUG_PRINTF("[LoRa] Frequencia: %.2f MHz\n", LORA_FREQUENCY / 1E6);
    DEBUG_PRINTF("[LoRa] SF: %d, BW: %.0f kHz, CR: 4/%d\n",
                 LORA_SF, LORA_BW / 1E3, LORA_CR);
    DEBUG_PRINTF("[LoRa] Potencia TX: %d dBm\n", LORA_TX_POWER);

    return true;
}

void LoRaHandler::configureRadio() {
    // Spreading Factor (7-12)
    // Maior SF = maior alcance, menor taxa de dados
    LoRa.setSpreadingFactor(LORA_SF);

    // Bandwidth (7.8E3 a 500E3)
    // Maior BW = maior taxa de dados, menor sensibilidade
    LoRa.setSignalBandwidth(LORA_BW);

    // Coding Rate (5-8 para 4/5 ate 4/8)
    // Maior CR = mais redundancia, menor taxa efetiva
    LoRa.setCodingRate4(LORA_CR);

    // Potencia de transmissao (2-20 dBm)
    LoRa.setTxPower(LORA_TX_POWER);

    // Preambulo
    LoRa.setPreambleLength(LORA_PREAMBLE_LENGTH);

    // Sync Word - usar valor diferente de 0x34 (LoRaWAN)
    LoRa.setSyncWord(LORA_SYNC_WORD);

    // Habilita CRC para verificacao de integridade
    LoRa.enableCrc();

    // Modo de recepcao continua
    LoRa.receive();
}

bool LoRaHandler::available() {
    return LoRa.parsePacket() > 0;
}

LoRaPacket LoRaHandler::receive() {
    LoRaPacket packet;
    packet.valid = false;
    packet.timestamp = millis();

    int packetSize = LoRa.parsePacket();
    if (packetSize == 0) {
        return packet;
    }

    // Le o payload
    packet.payload = "";
    while (LoRa.available()) {
        packet.payload += (char)LoRa.read();
    }

    // Captura metricas do pacote
    packet.rssi = LoRa.packetRssi();
    packet.snr = LoRa.packetSnr();

    _lastRSSI = packet.rssi;
    _lastSNR = packet.snr;

    // Valida se o RSSI esta acima do threshold
    if (packet.rssi >= RSSI_THRESHOLD && packet.payload.length() > 0) {
        packet.valid = true;
    }

    DEBUG_PRINTF("[LoRa] Pacote recebido: %d bytes, RSSI: %d dBm, SNR: %.2f dB\n",
                 packet.payload.length(), packet.rssi, packet.snr);

    return packet;
}

LoRaPacket LoRaHandler::receivePacket() {
    // Versao que NAO chama parsePacket() - usar apos available()
    LoRaPacket packet;
    packet.valid = false;
    packet.timestamp = millis();

    // Le o payload diretamente (parsePacket ja foi chamado em available())
    packet.payload = "";
    while (LoRa.available()) {
        packet.payload += (char)LoRa.read();
    }

    // Captura metricas do pacote
    packet.rssi = LoRa.packetRssi();
    packet.snr = LoRa.packetSnr();

    _lastRSSI = packet.rssi;
    _lastSNR = packet.snr;

    // Valida se o RSSI esta acima do threshold
    if (packet.rssi >= RSSI_THRESHOLD && packet.payload.length() > 0) {
        packet.valid = true;
    }

    DEBUG_PRINTF("[LoRa] Pacote recebido: %d bytes, RSSI: %d dBm, SNR: %.2f dB\n",
                 packet.payload.length(), packet.rssi, packet.snr);

    return packet;
}

bool LoRaHandler::send(const String& data) {
    if (!_initialized) {
        DEBUG_PRINTLN("[LoRa] ERRO: Modulo nao inicializado!");
        return false;
    }

    if (data.length() > MAX_PACKET_SIZE) {
        DEBUG_PRINTLN("[LoRa] ERRO: Pacote muito grande!");
        return false;
    }

    DEBUG_PRINTF("[LoRa] Enviando %d bytes...\n", data.length());

    LoRa.beginPacket();
    LoRa.print(data);
    int result = LoRa.endPacket();

    // Volta para modo de recepcao
    LoRa.receive();

    if (result) {
        DEBUG_PRINTLN("[LoRa] Envio OK");
        return true;
    } else {
        DEBUG_PRINTLN("[LoRa] ERRO no envio!");
        return false;
    }
}

bool LoRaHandler::sendWithRetry(const String& data, int maxRetries) {
    for (int i = 0; i < maxRetries; i++) {
        if (send(data)) {
            return true;
        }
        DEBUG_PRINTF("[LoRa] Tentativa %d/%d falhou, retentando...\n", i + 1, maxRetries);
        delay(100 * (i + 1));  // Backoff exponencial simples
    }
    return false;
}

void LoRaHandler::setFrequency(long frequency) {
    LoRa.setFrequency(frequency);
    DEBUG_PRINTF("[LoRa] Frequencia alterada para %.2f MHz\n", frequency / 1E6);
}

void LoRaHandler::setSpreadingFactor(int sf) {
    if (sf >= 7 && sf <= 12) {
        LoRa.setSpreadingFactor(sf);
        DEBUG_PRINTF("[LoRa] SF alterado para %d\n", sf);
    }
}

void LoRaHandler::setBandwidth(long bw) {
    LoRa.setSignalBandwidth(bw);
    DEBUG_PRINTF("[LoRa] BW alterado para %.0f kHz\n", bw / 1E3);
}

void LoRaHandler::setTxPower(int power) {
    if (power >= 2 && power <= 20) {
        LoRa.setTxPower(power);
        DEBUG_PRINTF("[LoRa] Potencia TX alterada para %d dBm\n", power);
    }
}

void LoRaHandler::setSyncWord(int sw) {
    LoRa.setSyncWord(sw);
    DEBUG_PRINTF("[LoRa] Sync Word alterado para 0x%02X\n", sw);
}

int LoRaHandler::getLastRSSI() {
    return _lastRSSI;
}

float LoRaHandler::getLastSNR() {
    return _lastSNR;
}

bool LoRaHandler::isInitialized() {
    return _initialized;
}

void LoRaHandler::enableReceiveMode() {
    LoRa.receive();
}

void LoRaHandler::sleep() {
    LoRa.sleep();
    DEBUG_PRINTLN("[LoRa] Modo sleep ativado");
}

void LoRaHandler::idle() {
    LoRa.idle();
    DEBUG_PRINTLN("[LoRa] Modo idle ativado");
}
