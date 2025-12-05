#include "protocol.h"

Protocol::Protocol() {
}

SensorData Protocol::parseLoRaPacket(const String& payload) {
    SensorData result;
    result.valid = false;

    if (payload.length() == 0) {
        DEBUG_PRINTLN("[Protocol] ERRO: Payload vazio");
        return result;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (error) {
        DEBUG_PRINTF("[Protocol] ERRO JSON: %s\n", error.c_str());
        return result;
    }

    // Valida campos obrigatorios
    if (!doc.containsKey("id") || !doc.containsKey("type")) {
        DEBUG_PRINTLN("[Protocol] ERRO: Campos obrigatorios ausentes (id, type)");
        return result;
    }

    result.nodeId = doc["id"].as<String>();
    result.nodeType = doc["type"].as<String>();
    result.sequence = doc["seq"] | 0;

    // Copia os dados do sensor
    if (doc.containsKey("data")) {
        result.data = doc["data"];
    }

    result.valid = true;

    DEBUG_PRINTF("[Protocol] Pacote parseado: Node=%s, Type=%s, Seq=%d\n",
                 result.nodeId.c_str(), result.nodeType.c_str(), result.sequence);

    return result;
}

String Protocol::createServerPayload(const SensorData& sensorData, int rssi, float snr) {
    JsonDocument doc;

    // Informacoes do gateway
    doc["gateway_id"] = GATEWAY_ID;
    doc["timestamp"] = millis() / 1000;  // Segundos desde boot (idealmente usar NTP)

    // Informacoes do no
    JsonObject node = doc["node"].to<JsonObject>();
    node["id"] = sensorData.nodeId;
    node["type"] = sensorData.nodeType;
    node["seq"] = sensorData.sequence;

    // Dados do sensor
    if (!sensorData.data.isNull()) {
        node["data"] = sensorData.data;
    }

    // Informacoes de RF
    JsonObject rf = doc["rf"].to<JsonObject>();
    rf["rssi"] = rssi;
    rf["snr"] = snr;

    String output;
    serializeJson(doc, output);

    DEBUG_PRINTF("[Protocol] Payload servidor: %s\n", output.c_str());

    return output;
}

String Protocol::createAck(const String& nodeId, uint32_t sequence, bool success) {
    JsonDocument doc;

    doc["type"] = "ack";
    doc["to"] = nodeId;
    doc["seq"] = sequence;
    doc["ok"] = success;
    doc["gw"] = GATEWAY_ID;

    String output;
    serializeJson(doc, output);

    return output;
}

String Protocol::createGatewayStatus(int wifiRssi, uint32_t packetsReceived,
                                     uint32_t packetsForwarded, unsigned long uptime) {
    JsonDocument doc;

    doc["gateway_id"] = GATEWAY_ID;
    doc["type"] = "status";
    doc["timestamp"] = millis() / 1000;

    JsonObject stats = doc["stats"].to<JsonObject>();
    stats["uptime_s"] = uptime / 1000;
    stats["packets_rx"] = packetsReceived;
    stats["packets_fwd"] = packetsForwarded;
    stats["wifi_rssi"] = wifiRssi;
    stats["free_heap"] = ESP.getFreeHeap();

    String output;
    serializeJson(doc, output);

    return output;
}

bool Protocol::validatePacket(const String& payload) {
    if (payload.length() == 0 || payload.length() > MAX_PACKET_SIZE) {
        return false;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (error) {
        return false;
    }

    // Verifica campos minimos
    return doc.containsKey("id") && doc.containsKey("type");
}

MessageType Protocol::getMessageType(const String& payload) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (error) {
        return MSG_TYPE_UNKNOWN;
    }

    String type = doc["type"] | "";

    if (type == "sensor") return MSG_TYPE_SENSOR_DATA;
    if (type == "actuator") return MSG_TYPE_ACTUATOR_CMD;
    if (type == "ack") return MSG_TYPE_ACK;
    if (type == "status") return MSG_TYPE_STATUS;
    if (type == "config") return MSG_TYPE_CONFIG;

    return MSG_TYPE_UNKNOWN;
}
