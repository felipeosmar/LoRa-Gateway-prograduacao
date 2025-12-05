#!/usr/bin/env python3
"""
Script para consultar dados do servidor LoRa
Compativel com Windows e Linux
"""

from tinydb import TinyDB, Query
from datetime import datetime
import argparse
import json
import os
import sys

# Diretorio base
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
DATABASE_FILE = os.path.join(BASE_DIR, 'data', 'lora_data.json')


def get_db():
    if not os.path.exists(DATABASE_FILE):
        print(f"[ERRO] Banco de dados nao encontrado: {DATABASE_FILE}")
        print("Execute o servidor primeiro para criar o banco.")
        sys.exit(1)
    return TinyDB(DATABASE_FILE)


def list_devices():
    """Lista todos os dispositivos conhecidos"""
    db = get_db()
    devices = db.table('devices').all()

    print("\n" + "=" * 70)
    print(" DISPOSITIVOS REGISTRADOS")
    print("=" * 70)

    if not devices:
        print(" Nenhum dispositivo encontrado")
        return

    print(f"{'Node ID':<15} {'Tipo':<10} {'Gateway':<10} {'Pacotes':<10} {'Ultimo Contato'}")
    print("-" * 70)

    devices.sort(key=lambda x: x.get('last_seen', ''), reverse=True)
    for d in devices:
        print(f"{d.get('node_id', '-'):<15} {d.get('node_type', '-'):<10} "
              f"{d.get('gateway_id', '-'):<10} {d.get('total_packets', 0):<10} "
              f"{d.get('last_seen', '-')[:19]}")

    db.close()


def list_readings(node_id=None, limit=20):
    """Lista leituras recentes"""
    db = get_db()
    readings = db.table('sensor_readings').all()

    if node_id:
        readings = [r for r in readings if r.get('node_id') == node_id]

    readings.sort(key=lambda x: x.get('received_at', ''), reverse=True)
    readings = readings[:limit]

    print("\n" + "=" * 90)
    print(f" ULTIMAS {limit} LEITURAS" + (f" (Node: {node_id})" if node_id else ""))
    print("=" * 90)

    if not readings:
        print(" Nenhuma leitura encontrada")
        return

    print(f"{'Hora':<20} {'Node':<12} {'RSSI':<8} {'SNR':<8} {'Dados'}")
    print("-" * 90)

    for r in readings:
        data_str = json.dumps(r.get('data', {}))
        if len(data_str) > 40:
            data_str = data_str[:37] + "..."
        time_str = r.get('received_at', '-')[:19]
        print(f"{time_str:<20} {r.get('node_id', '-'):<12} "
              f"{r.get('rssi', 0):<8} {r.get('snr', 0):<8.1f} {data_str}")

    db.close()


def show_stats():
    """Mostra estatisticas gerais"""
    db = get_db()
    readings = db.table('sensor_readings').all()
    devices = db.table('devices').all()

    # Leituras hoje
    today = datetime.now().date().isoformat()
    readings_today = len([r for r in readings if r.get('received_at', '').startswith(today)])

    # Media RSSI
    rssi_values = [r.get('rssi', 0) for r in readings if r.get('rssi')]
    avg_rssi = sum(rssi_values) / len(rssi_values) if rssi_values else 0

    print("\n" + "=" * 40)
    print(" ESTATISTICAS")
    print("=" * 40)
    print(f" Total de leituras:    {len(readings)}")
    print(f" Dispositivos:         {len(devices)}")
    print(f" Leituras hoje:        {readings_today}")
    print(f" RSSI medio:           {avg_rssi:.1f} dBm")
    print("=" * 40)

    db.close()


def export_csv(node_id=None, output_file='export.csv'):
    """Exporta dados para CSV"""
    db = get_db()
    readings = db.table('sensor_readings').all()

    if node_id:
        readings = [r for r in readings if r.get('node_id') == node_id]

    readings.sort(key=lambda x: x.get('received_at', ''), reverse=True)

    with open(output_file, 'w', encoding='utf-8') as f:
        f.write('received_at,gateway_id,node_id,node_type,sequence,data,rssi,snr\n')
        for r in readings:
            data_str = json.dumps(r.get('data', {})).replace('"', '""')
            f.write(f"{r.get('received_at', '')},{r.get('gateway_id', '')},"
                    f"{r.get('node_id', '')},{r.get('node_type', '')},"
                    f"{r.get('sequence', 0)},\"{data_str}\","
                    f"{r.get('rssi', 0)},{r.get('snr', 0)}\n")

    print(f"\n[OK] {len(readings)} registros exportados para '{output_file}'")
    db.close()


def show_node_data(node_id, limit=10):
    """Mostra dados detalhados de um no"""
    db = get_db()
    readings = db.table('sensor_readings').all()
    readings = [r for r in readings if r.get('node_id') == node_id]
    readings.sort(key=lambda x: x.get('received_at', ''), reverse=True)
    readings = readings[:limit]

    print("\n" + "=" * 60)
    print(f" DADOS DO NODE: {node_id}")
    print("=" * 60)

    if not readings:
        print(" Nenhuma leitura encontrada")
        return

    for r in readings:
        print(f"\n[{r.get('received_at', '-')[:19]}]")
        print(f"  RSSI: {r.get('rssi', 0)} dBm | SNR: {r.get('snr', 0)} dB | Seq: {r.get('sequence', 0)}")
        data = r.get('data', {})
        for key, value in data.items():
            print(f"  {key}: {value}")

    db.close()


def clear_data(confirm=False):
    """Limpa todos os dados do banco"""
    if not confirm:
        print("\n[AVISO] Esta acao ira apagar TODOS os dados!")
        print("Use --confirm para confirmar a operacao.")
        return

    db = get_db()
    db.drop_tables()
    print("\n[OK] Todos os dados foram apagados.")
    db.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description='Consulta dados do servidor LoRa',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
Exemplos:
  python query_data.py stats              # Ver estatisticas
  python query_data.py devices            # Listar dispositivos
  python query_data.py readings           # Ver ultimas leituras
  python query_data.py readings -n NODE1  # Leituras de um node
  python query_data.py export             # Exportar para CSV
  python query_data.py node -n NODE1      # Detalhes de um node
  python query_data.py clear --confirm    # Apagar todos os dados
        '''
    )

    parser.add_argument('command',
                        choices=['devices', 'readings', 'stats', 'export', 'node', 'clear'],
                        help='Comando a executar')
    parser.add_argument('--node', '-n', help='ID do node para filtrar')
    parser.add_argument('--limit', '-l', type=int, default=20, help='Limite de registros')
    parser.add_argument('--output', '-o', default='export.csv', help='Arquivo de saida para export')
    parser.add_argument('--confirm', action='store_true', help='Confirma operacao destrutiva')

    args = parser.parse_args()

    if args.command == 'devices':
        list_devices()
    elif args.command == 'readings':
        list_readings(args.node, args.limit)
    elif args.command == 'stats':
        show_stats()
    elif args.command == 'export':
        export_csv(args.node, args.output)
    elif args.command == 'node':
        if not args.node:
            print("[ERRO] Especifique o node com --node ou -n")
        else:
            show_node_data(args.node, args.limit)
    elif args.command == 'clear':
        clear_data(args.confirm)
