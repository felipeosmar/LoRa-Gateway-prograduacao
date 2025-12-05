# Sensor Node - Monitor de Maquina Industrial

Node LoRa para monitoramento de maquinas industriais, compativel com Heltec WiFi LoRa 32 V2.

## Funcionalidades

- **4 Entradas Digitais** (DI1-DI4): Monitoramento de estados on/off
- **2 Entradas Analogicas** (AI1-AI2): Leitura de valores 0-4095 (12 bits)
- **Temperatura Interna**: Temperatura do chip ESP32
- **Transmissao por Evento**: Envia imediatamente quando uma entrada digital muda
- **Transmissao Periodica**: Envia a cada 30 segundos (configuravel)
- **Display OLED**: Exibe status em tempo real

## Hardware

### Placa Suportada

- Heltec WiFi LoRa 32 V2

### Pinagem

| Funcao | GPIO | Descricao |
|--------|------|-----------|
| DI1 | 12 | Entrada Digital 1 |
| DI2 | 13 | Entrada Digital 2 |
| DI3 | 32 | Entrada Digital 3 |
| DI4 | 33 | Entrada Digital 4 |
| AI1 | 36 (SVP) | Entrada Analogica 1 |
| AI2 | 39 (SVN) | Entrada Analogica 2 |

> **Nota**: As entradas digitais usam pull-up interno. Conecte ao GND para ativar (logica invertida).

## Formato dos Dados

O node transmite dados no seguinte formato JSON:

```json
{
  "id": "M001",
  "type": "machine",
  "seq": 42,
  "data": {
    "macAddress": "AA:BB:CC:DD:EE:FF",
    "machineId": "M001",
    "timestamp": 1234567,
    "digitalInputs": {
      "di1": true,
      "di2": false,
      "di3": true,
      "di4": false
    },
    "analogInputs": {
      "ai1": 2048,
      "ai2": 1536
    },
    "temperature": 25.5,
    "trigger": "event"
  }
}
```

### Campos

| Campo | Tipo | Descricao |
|-------|------|-----------|
| `macAddress` | string | Endereco MAC do ESP32 |
| `machineId` | string | ID configurado da maquina |
| `timestamp` | number | Segundos desde o boot |
| `digitalInputs` | object | Estado das 4 entradas digitais |
| `analogInputs` | object | Valores das 2 entradas analogicas (0-4095) |
| `temperature` | number | Temperatura interna do ESP32 em Celsius |
| `trigger` | string | `"event"` se houve mudanca, `"periodic"` se periodico |

## Configuracao

Edite as definicoes no inicio do arquivo `src/main.cpp`:

```cpp
// ID da maquina (unico para cada dispositivo)
#define MACHINE_ID "M001"

// Intervalo de transmissao periodica (ms)
#define TX_INTERVAL 30000  // 30 segundos

// Frequencia LoRa (deve ser igual ao gateway)
#define LORA_FREQUENCY 915E6  // 915 MHz (Brasil)

// Parametros de radio (devem ser iguais ao gateway)
#define LORA_SF 7          // Spreading Factor
#define LORA_BW 125E3      // Bandwidth
#define LORA_TX_POWER 20   // Potencia de transmissao (dBm)
```

### Configurando Multiplos Nodes

Para cada maquina, altere o `MACHINE_ID`:

```cpp
#define MACHINE_ID "M001"  // Maquina 1
#define MACHINE_ID "M002"  // Maquina 2
#define MACHINE_ID "M003"  // Maquina 3
```

## Compilacao

### Usando PlatformIO

```bash
cd examples/sensor_node
pio run
```

### Upload para o ESP32

```bash
pio run -t upload
```

### Monitor Serial

```bash
pio run -t monitor
```

Ou tudo junto:

```bash
pio run -t upload -t monitor
```

## Modos de Transmissao

### 1. Transmissao por Evento (`trigger: "event"`)

Quando qualquer entrada digital muda de estado, o node transmite imediatamente.

- Tempo de debounce: 50ms
- Util para detectar eventos como:
  - Maquina ligada/desligada
  - Porta aberta/fechada
  - Sensor de presenca ativado

### 2. Transmissao Periodica (`trigger: "periodic"`)

O node transmite automaticamente a cada `TX_INTERVAL` milissegundos.

- Default: 30 segundos
- Util para:
  - Monitoramento continuo de entradas analogicas
  - Verificacao de conectividade (heartbeat)
  - Registro historico de temperatura

## Display OLED

O display mostra em tempo real:

```
M001            TX:42
----------------
DI: 1 0 1 0
AI1:2048 AI2:1536
Temp: 25.5 C
----------------
33:75:EF:05
```

- **Linha 1**: ID da maquina e contador de transmissoes
- **Linha 2**: Estado das entradas digitais
- **Linha 3**: Valores das entradas analogicas
- **Linha 4**: Temperatura interna
- **Linha 5**: Ultimos 4 bytes do MAC Address

## LED Indicador

- **3 piscadas rapidas**: Inicializacao OK
- **1 piscada**: ACK recebido do gateway
- **5 piscadas continuas**: Erro na inicializacao do LoRa

## Integracao com o Sistema

### Fluxo de Dados

```
[Sensor Node] --LoRa--> [Gateway] --HTTP--> [Servidor] --Web--> [Dashboard]
```

1. O node le as entradas e monta o JSON
2. Transmite via LoRa para o Gateway
3. O Gateway encaminha via HTTP para o servidor
4. O servidor armazena e disponibiliza na interface web
5. A pagina "Maquinas" exibe os dados em tempo real

### Visualizacao no Dashboard

Acesse a pagina **Maquinas** no dashboard web para ver:

- Status online/offline de cada maquina
- LEDs indicadores para entradas digitais
- Barras de progresso para entradas analogicas
- Temperatura com indicador de cor
- Tipo de trigger (evento ou periodico)
- Tempo desde a ultima atualizacao

## Solucao de Problemas

### Node nao transmite

1. Verifique se a frequencia LoRa e igual ao gateway
2. Verifique o Sync Word (deve ser `0x20`)
3. Verifique os parametros SF, BW e CR

### Entradas digitais sempre em 0

1. Verifique a conexao fisica dos sensores
2. Lembre que a logica e invertida (pull-up interno)
3. Conecte ao GND para ativar a entrada

### Temperatura muito alta

A temperatura interna do ESP32 e naturalmente mais alta que a ambiente. O codigo aplica um offset de -20C, ajuste conforme necessario:

```cpp
// Em readInternalTemperature()
tempC -= 20.0;  // Ajuste este valor
```

### Display nao liga

1. Verifique se o Vext esta habilitado (GPIO 21 = LOW)
2. Verifique as conexoes I2C (SDA=4, SCL=15)
3. Verifique o reset do OLED (GPIO 16)

## Exemplo de Aplicacao

### Monitoramento de Maquina CNC

- **DI1**: Maquina ligada/desligada
- **DI2**: Ciclo em execucao
- **DI3**: Alarme ativo
- **DI4**: Porta de seguranca
- **AI1**: Velocidade do spindle (0-10V -> 0-3.3V com divisor)
- **AI2**: Corrente do motor (sensor de corrente)

### Monitoramento de Compressor

- **DI1**: Compressor ligado
- **DI2**: Pressao OK
- **DI3**: Nivel de oleo OK
- **DI4**: Temperatura OK
- **AI1**: Pressao do reservatorio (transdutor 4-20mA)
- **AI2**: Temperatura do oleo (sensor NTC)
