#ifndef CONFIG_H
#define CONFIG_H

// ============================================
// CONFIGURACAO DO GATEWAY LORA - JVTECH MIJ
// ============================================

// --- Configuracao WiFi ---
#define WIFI_SSID "Lidomar"
#define WIFI_PASSWORD "Lidomar123"
#define WIFI_CONNECT_TIMEOUT_MS 10000
#define WIFI_RECONNECT_INTERVAL_MS 5000

// --- Configuracao do Servidor Backend ---
#define SERVER_HOST "192.168.0.3"
#define SERVER_PORT 8081
#define SERVER_ENDPOINT "/api/sensor-data"
#define HTTP_TIMEOUT_MS 5000

// --- Configuracao LoRa (pinos JVtech MIJ) ---
// Conforme documentacao: SPI para comunicacao com chip LoRa
#ifndef LORA_SCK
#define LORA_SCK 18      // GPIO18 - SCK
#endif
#ifndef LORA_MISO
#define LORA_MISO 19     // GPIO19 - MISO
#endif
#ifndef LORA_MOSI
#define LORA_MOSI 23     // GPIO23 - MOSI
#endif
#ifndef LORA_CS
#define LORA_CS 5        // GPIO05 - CS (Chip Select)
#endif
#ifndef LORA_RST
#define LORA_RST 14      // GPIO14 - RST (Reset)
#endif
#ifndef LORA_DIO0
#define LORA_DIO0 26     // GPIO26 - DIO0 (Interrupt)
#endif

// DIO1 disponivel se necessario
#define LORA_DIO1 13     // GPIO13 - DIO1

// --- Parametros de Radio LoRa ---
#ifndef LORA_FREQUENCY
#define LORA_FREQUENCY 915E6  // 915 MHz (Brasil - AU915)
#endif
#ifndef LORA_TX_POWER
#define LORA_TX_POWER 20      // Potencia TX em dBm (max 20)
#endif
#ifndef LORA_SF
#define LORA_SF 7             // Spreading Factor (7-12)
#endif
#ifndef LORA_BW
#define LORA_BW 125E3         // Bandwidth: 125kHz
#endif
#ifndef LORA_CR
#define LORA_CR 5             // Coding Rate: 4/5
#endif
#define LORA_PREAMBLE_LENGTH 8
#define LORA_SYNC_WORD 0x20   // Sync word privado (evita LoRaWAN)

// --- Configuracao do Gateway ---
#define GATEWAY_ID "GW001"
#define MAX_PACKET_SIZE 255
#define PACKET_QUEUE_SIZE 10
#define LED_PIN 25            // LED RGB na placa base (GPIO25)
#define BUTTON_PIN 0          // Botao de uso geral (GPIO0)

// --- Intervalo de Status ---
#define STATUS_REPORT_INTERVAL_MS 60000  // Reportar status a cada 1 min
#define RSSI_THRESHOLD -120              // Limite minimo de RSSI aceitavel

// --- Debug ---
#define DEBUG_SERIAL Serial
#define DEBUG_BAUD 115200
#define ENABLE_DEBUG 1

#if ENABLE_DEBUG
#define DEBUG_PRINT(x) DEBUG_SERIAL.print(x)
#define DEBUG_PRINTLN(x) DEBUG_SERIAL.println(x)
#define DEBUG_PRINTF(fmt, ...) DEBUG_SERIAL.printf(fmt, ##__VA_ARGS__)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#define DEBUG_PRINTF(fmt, ...)
#endif

#endif // CONFIG_H
