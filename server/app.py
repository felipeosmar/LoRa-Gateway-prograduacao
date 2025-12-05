"""
Servidor Backend para Gateway LoRa
Recebe dados dos sensores via HTTP POST e armazena em TinyDB (JSON)
Compativel com Windows e Linux - Self Contained
"""

from flask import Flask, request, jsonify, render_template
from flask_cors import CORS
from datetime import datetime
from tinydb import TinyDB, Query
from tinydb.table import Document
import json
import os
import sys

# Obtem diretorio base da aplicacao
if getattr(sys, 'frozen', False):
    BASE_DIR = os.path.dirname(sys.executable)
else:
    BASE_DIR = os.path.dirname(os.path.abspath(__file__))

app = Flask(__name__,
            static_folder=os.path.join(BASE_DIR, 'static'),
            template_folder=os.path.join(BASE_DIR, 'templates'))
CORS(app)

# Configuracao
DATABASE_DIR = os.path.join(BASE_DIR, 'data')
DATABASE_FILE = os.path.join(DATABASE_DIR, 'lora_data.json')
HOST = '0.0.0.0'
PORT = 8081

# Banco de dados TinyDB
db = None
readings_table = None
devices_table = None
gateway_status_table = None


def init_db():
    """Inicializa o banco de dados TinyDB"""
    global db, readings_table, devices_table, gateway_status_table

    # Cria diretorio de dados se nao existir
    if not os.path.exists(DATABASE_DIR):
        os.makedirs(DATABASE_DIR)

    db = TinyDB(DATABASE_FILE, indent=2)
    readings_table = db.table('sensor_readings')
    devices_table = db.table('devices')
    gateway_status_table = db.table('gateway_status')

    print(f"[DB] Banco de dados inicializado: {DATABASE_FILE}")


@app.route('/api/sensor-data', methods=['POST'])
def receive_sensor_data():
    """
    Endpoint principal para receber dados dos sensores
    Formato esperado:
    {
        "gateway_id": "GW001",
        "timestamp": 12345,
        "node": {
            "id": "SENSOR01",
            "type": "sensor",
            "seq": 42,
            "data": { ... }
        },
        "rf": {
            "rssi": -65,
            "snr": 9.5
        }
    }
    """
    try:
        data = request.json

        if not data:
            return jsonify({"error": "JSON invalido"}), 400

        gateway_id = data.get('gateway_id', 'unknown')
        node = data.get('node', {})
        rf = data.get('rf', {})

        node_id = node.get('id', 'unknown')
        node_type = node.get('type', 'sensor')
        sequence = node.get('seq', 0)
        sensor_data = node.get('data', {})
        rssi = rf.get('rssi', 0)
        snr = rf.get('snr', 0.0)

        now = datetime.now().isoformat()

        # Insere leitura
        readings_table.insert({
            'gateway_id': gateway_id,
            'node_id': node_id,
            'node_type': node_type,
            'sequence': sequence,
            'data': sensor_data,
            'rssi': rssi,
            'snr': snr,
            'received_at': now
        })

        # Atualiza ou insere dispositivo
        Device = Query()
        existing = devices_table.get(Device.node_id == node_id)

        if existing:
            devices_table.update({
                'node_type': node_type,
                'gateway_id': gateway_id,
                'last_seen': now,
                'total_packets': existing.get('total_packets', 0) + 1
            }, Device.node_id == node_id)
        else:
            devices_table.insert({
                'node_id': node_id,
                'node_type': node_type,
                'gateway_id': gateway_id,
                'first_seen': now,
                'last_seen': now,
                'total_packets': 1
            })

        # Log no console
        print(f"[{datetime.now().strftime('%H:%M:%S')}] "
              f"Gateway: {gateway_id} | Node: {node_id} | "
              f"RSSI: {rssi} dBm | SNR: {snr} dB")
        print(f"    Data: {json.dumps(sensor_data)}")

        return jsonify({"status": "ok", "message": "Dados recebidos"}), 200

    except Exception as e:
        print(f"[ERRO] {str(e)}")
        return jsonify({"error": str(e)}), 500


@app.route('/api/gateway-status', methods=['POST'])
def receive_gateway_status():
    """Recebe status periodico do gateway"""
    try:
        data = request.json

        if not data:
            return jsonify({"error": "JSON invalido"}), 400

        gateway_id = data.get('gateway_id', 'unknown')
        stats = data.get('stats', {})

        gateway_status_table.insert({
            'gateway_id': gateway_id,
            'uptime_s': stats.get('uptime_s', 0),
            'packets_rx': stats.get('packets_rx', 0),
            'packets_fwd': stats.get('packets_fwd', 0),
            'wifi_rssi': stats.get('wifi_rssi', 0),
            'free_heap': stats.get('free_heap', 0),
            'received_at': datetime.now().isoformat()
        })

        print(f"[{datetime.now().strftime('%H:%M:%S')}] "
              f"Status Gateway: {gateway_id} | "
              f"Uptime: {stats.get('uptime_s', 0)}s | "
              f"Packets: {stats.get('packets_rx', 0)}")

        return jsonify({"status": "ok"}), 200

    except Exception as e:
        print(f"[ERRO] {str(e)}")
        return jsonify({"error": str(e)}), 500


@app.route('/api/readings', methods=['GET'])
def get_readings():
    """
    Retorna leituras dos sensores
    Parametros opcionais:
        - node_id: filtrar por no
        - gateway_id: filtrar por gateway
        - limit: numero maximo de registros (default: 100)
    """
    node_id = request.args.get('node_id')
    gateway_id = request.args.get('gateway_id')
    limit = request.args.get('limit', 100, type=int)

    # Obtem todas as leituras
    all_readings = readings_table.all()

    # Filtra
    if node_id:
        all_readings = [r for r in all_readings if r.get('node_id') == node_id]
    if gateway_id:
        all_readings = [r for r in all_readings if r.get('gateway_id') == gateway_id]

    # Ordena por data (mais recente primeiro)
    all_readings.sort(key=lambda x: x.get('received_at', ''), reverse=True)

    # Limita
    readings = all_readings[:limit]

    # Adiciona ID do documento
    result = []
    for r in readings:
        item = dict(r)
        item['id'] = r.doc_id if hasattr(r, 'doc_id') else 0
        result.append(item)

    return jsonify({
        'count': len(result),
        'readings': result
    })


@app.route('/api/devices', methods=['GET'])
def get_devices():
    """Retorna lista de dispositivos conhecidos"""
    all_devices = devices_table.all()

    # Ordena por ultimo contato
    all_devices.sort(key=lambda x: x.get('last_seen', ''), reverse=True)

    return jsonify({
        'count': len(all_devices),
        'devices': all_devices
    })


@app.route('/api/stats', methods=['GET'])
def get_stats():
    """Retorna estatisticas gerais do sistema"""
    all_readings = readings_table.all()
    all_devices = devices_table.all()

    # Total de leituras
    total_readings = len(all_readings)

    # Total de dispositivos
    total_devices = len(all_devices)

    # Leituras nas ultimas 24 horas
    now = datetime.now()
    readings_24h = 0
    for r in all_readings:
        try:
            received = datetime.fromisoformat(r.get('received_at', ''))
            diff = (now - received).total_seconds()
            if diff <= 86400:  # 24 horas em segundos
                readings_24h += 1
        except:
            pass

    # Ultima leitura
    last_reading = None
    if all_readings:
        all_readings.sort(key=lambda x: x.get('received_at', ''), reverse=True)
        last = all_readings[0]
        last_reading = {
            'node_id': last.get('node_id'),
            'time': last.get('received_at')
        }

    return jsonify({
        'total_readings': total_readings,
        'total_devices': total_devices,
        'readings_last_24h': readings_24h,
        'last_reading': last_reading
    })


@app.route('/api/node/<node_id>/readings', methods=['GET'])
def get_node_readings(node_id):
    """Retorna leituras de um no especifico"""
    limit = request.args.get('limit', 50, type=int)

    Device = Query()
    all_readings = readings_table.search(Device.node_id == node_id)

    # Ordena por data
    all_readings.sort(key=lambda x: x.get('received_at', ''), reverse=True)

    # Limita
    readings = all_readings[:limit]

    result = []
    for r in readings:
        result.append({
            'id': r.doc_id if hasattr(r, 'doc_id') else 0,
            'data': r.get('data', {}),
            'rssi': r.get('rssi', 0),
            'snr': r.get('snr', 0),
            'received_at': r.get('received_at')
        })

    return jsonify({
        'node_id': node_id,
        'count': len(result),
        'readings': result
    })


@app.route('/', methods=['GET'])
def index():
    """Pagina inicial - Interface Web"""
    return render_template('index.html')


@app.route('/api', methods=['GET'])
def api_info():
    """Informacoes da API"""
    return jsonify({
        'service': 'LoRa Gateway Backend',
        'version': '1.0.0',
        'database': 'TinyDB (JSON)',
        'endpoints': {
            'POST /api/sensor-data': 'Recebe dados dos sensores',
            'POST /api/gateway-status': 'Recebe status do gateway',
            'GET /api/readings': 'Lista leituras (params: node_id, gateway_id, limit)',
            'GET /api/devices': 'Lista dispositivos conhecidos',
            'GET /api/stats': 'Estatisticas gerais',
            'GET /api/node/<node_id>/readings': 'Leituras de um no especifico'
        }
    })


@app.route('/health', methods=['GET'])
def health():
    """Endpoint de health check"""
    return jsonify({'status': 'healthy', 'timestamp': datetime.now().isoformat()})


def main():
    print("=" * 50)
    print("  Servidor Backend LoRa Gateway")
    print("  Compativel com Windows e Linux")
    print("=" * 50)

    # Inicializa banco de dados
    init_db()

    print(f"\n[SERVER] Iniciando em http://{HOST}:{PORT}")
    print(f"[SERVER] Interface Web: http://localhost:{PORT}")
    print(f"[SERVER] API endpoint: POST /api/sensor-data")
    print(f"[SERVER] Dados salvos em: {DATABASE_FILE}")
    print(f"[SERVER] Pressione Ctrl+C para parar\n")

    app.run(host=HOST, port=PORT, debug=False, threaded=True)


if __name__ == '__main__':
    main()
