#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "config.h"

// ============================================
// PROTOCOLO DE COMUNICACAO JSON PARA LORA
// ============================================
//
// Formato do pacote enviado pelos nos sensores:
// {
//   "id": "NODE001",        // ID unico do no
//   "type": "sensor",       // Tipo: sensor, actuator, etc
//   "seq": 123,             // Numero de sequencia
//   "data": {               // Dados do sensor
//     "temp": 25.5,
//     "hum": 60.0,
//     "bat": 3.7
//   }
// }
//
// Formato do pacote enviado pelo gateway para o servidor:
// {
//   "gateway_id": "GW001",
//   "timestamp": 1699999999,
//   "node": {
//     "id": "NODE001",
//     "type": "sensor",
//     "seq": 123,
//     "data": {...}
//   },
//   "rf": {
//     "rssi": -65,
//     "snr": 9.5
//   }
// }

// Tipos de mensagem
enum MessageType {
    MSG_TYPE_UNKNOWN = 0,
    MSG_TYPE_SENSOR_DATA,
    MSG_TYPE_ACTUATOR_CMD,
    MSG_TYPE_ACK,
    MSG_TYPE_STATUS,
    MSG_TYPE_CONFIG
};

// Estrutura de dados do sensor
struct SensorData {
    String nodeId;
    String nodeType;
    uint32_t sequence;
    JsonDocument data;
    bool valid;
};

// Estrutura de pacote para o servidor
struct ServerPacket {
    String gatewayId;
    unsigned long timestamp;
    SensorData node;
    int rssi;
    float snr;
};

class Protocol {
public:
    Protocol();

    // Parsing de dados recebidos via LoRa
    SensorData parseLoRaPacket(const String& payload);

    // Criacao de pacote para enviar ao servidor
    String createServerPayload(const SensorData& sensorData, int rssi, float snr);

    // Criacao de ACK para enviar ao no
    String createAck(const String& nodeId, uint32_t sequence, bool success);

    // Criacao de mensagem de status do gateway
    String createGatewayStatus(int wifiRssi, uint32_t packetsReceived,
                               uint32_t packetsForwarded, unsigned long uptime);

    // Validacao de pacote
    bool validatePacket(const String& payload);

    // Utilitarios
    MessageType getMessageType(const String& payload);

private:
    static const size_t JSON_DOC_SIZE = 1024;
};

#endif // PROTOCOL_H
