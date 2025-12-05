# Gateway LoRa - Placa JVtech MIJ

Gateway LoRa para coleta de dados de sensores remotos, usando a placa JVtech MIJ (ESP32 + SX1276/SX1278).

## Arquitetura

```
[Nó Sensor 1] ──┐
[Nó Sensor 2] ──┼── LoRa 915MHz ──> [Gateway JVtech] ── WiFi ──> [Servidor Backend]
[Nó Sensor N] ──┘
```

## Hardware

### Placa JVtech MIJ

- **MCU**: ESP32
- **LoRa**: SX1276/SX1278
- **Frequência**: 902-928 MHz (AU915 Brasil)
- **Potência TX**: até +20 dBm (100mW)
- **Sensibilidade**: -148 dBm
- **Interfaces**: WiFi, Bluetooth/BLE, SPI, I2C, UART

### Pinagem LoRa (SPI)

| Função | GPIO |
|--------|------|
| SCK    | 18   |
| MISO   | 19   |
| MOSI   | 23   |
| CS     | 5    |
| RST    | 14   |
| DIO0   | 26   |
| DIO1   | 13   |

## Instalação

### Requisitos

- [PlatformIO](https://platformio.org/) (VS Code extension ou CLI)
- Cabo USB para programação
- Antena LoRa 915MHz

### Passos

1. Clone o repositório:
```bash
git clone <repo-url>
cd LoRa-Gateway-prograduacao
```

2. Configure o WiFi em `include/config.h`:
```cpp
#define WIFI_SSID "SUA_REDE_WIFI"
#define WIFI_PASSWORD "SUA_SENHA_WIFI"
```

3. Configure o servidor backend:
```cpp
#define SERVER_HOST "192.168.1.100"
#define SERVER_PORT 8080
#define SERVER_ENDPOINT "/api/sensor-data"
```

4. Compile e faça upload:
```bash
pio run -t upload
```

5. Monitore a saída serial:
```bash
pio device monitor
```

## Protocolo de Comunicação

### Pacote do Nó Sensor (LoRa)

```json
{
  "id": "NODE001",
  "type": "sensor",
  "seq": 123,
  "data": {
    "temp": 25.5,
    "hum": 60.0,
    "bat": 3.7
  }
}
```

### Pacote do Gateway para Servidor (HTTP POST)

```json
{
  "gateway_id": "GW001",
  "timestamp": 1699999999,
  "node": {
    "id": "NODE001",
    "type": "sensor",
    "seq": 123,
    "data": {
      "temp": 25.5,
      "hum": 60.0,
      "bat": 3.7
    }
  },
  "rf": {
    "rssi": -65,
    "snr": 9.5
  }
}
```

### ACK do Gateway para Nó

```json
{
  "type": "ack",
  "to": "NODE001",
  "seq": 123,
  "ok": true,
  "gw": "GW001"
}
```

## Parâmetros LoRa

| Parâmetro | Valor Padrão | Descrição |
|-----------|--------------|-----------|
| Frequência | 915 MHz | Banda AU915 (Brasil) |
| Spreading Factor | 7 | 7-12 (maior = mais alcance) |
| Bandwidth | 125 kHz | 125/250/500 kHz |
| Coding Rate | 4/5 | 4/5 a 4/8 |
| Sync Word | 0x12 | Diferente de LoRaWAN (0x34) |
| TX Power | 20 dBm | Máximo permitido |

## Estrutura do Projeto

```
LoRa-Gateway-prograduacao/
├── docs/                    # Documentação da placa JVtech
├── include/
│   ├── config.h            # Configurações gerais
│   ├── lora_handler.h      # Handler LoRa
│   ├── wifi_handler.h      # Handler WiFi
│   └── protocol.h          # Protocolo JSON
├── src/
│   ├── main.cpp            # Programa principal
│   ├── lora_handler.cpp    # Implementação LoRa
│   ├── wifi_handler.cpp    # Implementação WiFi
│   └── protocol.cpp        # Implementação protocolo
├── examples/
│   └── sensor_node/        # Exemplo de nó sensor
├── platformio.ini          # Configuração PlatformIO
└── README.md
```

## Exemplo de Nó Sensor

O diretório `examples/sensor_node/` contém um exemplo completo de nó sensor.

Para usar:
```bash
cd examples/sensor_node
pio run -t upload
```

## Servidor Backend (Exemplo)

Exemplo simples de servidor em Python para receber os dados:

```python
from flask import Flask, request, jsonify

app = Flask(__name__)

@app.route('/api/sensor-data', methods=['POST'])
def receive_sensor_data():
    data = request.json
    print(f"Dados recebidos: {data}")
    return jsonify({"status": "ok"}), 200

@app.route('/api/gateway-status', methods=['POST'])
def receive_gateway_status():
    data = request.json
    print(f"Status gateway: {data}")
    return jsonify({"status": "ok"}), 200

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=8080)
```

## Troubleshooting

### LoRa não inicializa
- Verifique se a chave de alimentação LoRa está ligada (SW na placa)
- Confira as conexões SPI
- Verifique se a antena está conectada

### WiFi não conecta
- Verifique SSID e senha em `config.h`
- Confira se a rede está em 2.4GHz (ESP32 não suporta 5GHz)

### Pacotes não chegam ao gateway
- Verifique se os parâmetros LoRa são idênticos (SF, BW, CR, Sync Word)
- Confirme que ambos estão na mesma frequência
- Teste com antenas próximas primeiro

## Licença

MIT License
