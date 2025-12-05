# Servidor Backend LoRa Gateway

Servidor web para receber e armazenar dados do Gateway LoRa.
**Compativel com Windows e Linux - Self Contained**

## Requisitos

- Python 3.8 ou superior
- Conexao de rede com o Gateway

## Instalacao Rapida

### Windows

1. Instale Python de https://www.python.org/downloads/
   - **IMPORTANTE:** Marque "Add Python to PATH" durante a instalacao
2. Clique duas vezes em `run.bat`

### Linux

```bash
chmod +x run.sh
./run.sh
```

## Uso

Apos iniciar o servidor:

- **Interface Web:** http://localhost:8080
- **API Endpoint:** POST http://localhost:8080/api/sensor-data

## Configuracao do Gateway

No arquivo `include/config.h` do Gateway, configure:

```cpp
#define SERVER_HOST "192.168.1.100"  // IP do computador rodando o servidor
#define SERVER_PORT 8080
#define SERVER_ENDPOINT "/api/sensor-data"
```

## Estrutura de Arquivos

```
server/
├── app.py              # Servidor Flask principal
├── requirements.txt    # Dependencias Python
├── query_data.py       # CLI para consultas
├── run.bat             # Iniciar no Windows
├── run.sh              # Iniciar no Linux
├── README.md           # Este arquivo
├── templates/
│   └── index.html      # Interface web
├── static/
│   ├── style.css       # Estilos
│   └── app.js          # JavaScript
└── data/
    └── lora_data.json  # Banco de dados (criado automaticamente)
```

## Interface Web

A interface possui 4 paginas:

- **Dashboard:** Visao geral com estatisticas e atividade recente
- **Dispositivos:** Lista de todos os nodes registrados
- **Leituras:** Historico de dados com filtros
- **Exportar:** Download em CSV ou JSON

## API REST

| Metodo | Endpoint | Descricao |
|--------|----------|-----------|
| POST | `/api/sensor-data` | Recebe dados dos sensores |
| POST | `/api/gateway-status` | Recebe status do gateway |
| GET | `/api/readings` | Lista leituras |
| GET | `/api/devices` | Lista dispositivos |
| GET | `/api/stats` | Estatisticas gerais |
| GET | `/api/node/<id>/readings` | Leituras de um node |
| GET | `/health` | Health check |

### Parametros de Consulta

`/api/readings` aceita:
- `node_id` - Filtrar por dispositivo
- `gateway_id` - Filtrar por gateway
- `limit` - Numero maximo de registros (default: 100)

## Consultas via CLI

```bash
# Ativar ambiente virtual primeiro
# Windows: venv\Scripts\activate
# Linux: source venv/bin/activate

# Ver estatisticas
python query_data.py stats

# Listar dispositivos
python query_data.py devices

# Ver ultimas leituras
python query_data.py readings

# Filtrar por node
python query_data.py readings -n SENSOR01

# Limitar quantidade
python query_data.py readings -l 50

# Exportar para CSV
python query_data.py export
python query_data.py export -n SENSOR01 -o sensor01.csv

# Ver detalhes de um node
python query_data.py node -n SENSOR01

# Limpar todos os dados
python query_data.py clear --confirm
```

## Formato dos Dados

### Dados recebidos do Gateway

```json
{
  "gateway_id": "GW001",
  "timestamp": 12345,
  "node": {
    "id": "SENSOR01",
    "type": "sensor",
    "seq": 42,
    "data": {
      "temp": 25.5,
      "hum": 60.2
    }
  },
  "rf": {
    "rssi": -65,
    "snr": 9.5
  }
}
```

### Banco de Dados

Os dados sao armazenados em `data/lora_data.json` usando TinyDB.
Este arquivo pode ser copiado para backup ou transferido entre sistemas.

## Solucao de Problemas

### Windows: "python nao e reconhecido"
- Reinstale Python marcando "Add Python to PATH"
- Ou adicione manualmente ao PATH do sistema

### Linux: Permissao negada
```bash
chmod +x run.sh
```

### Porta 8080 em uso
Edite `app.py` e altere a variavel `PORT`:
```python
PORT = 8081  # ou outra porta disponivel
```

### Firewall bloqueando conexoes
- Windows: Permita Python no Windows Firewall
- Linux: `sudo ufw allow 8080`
