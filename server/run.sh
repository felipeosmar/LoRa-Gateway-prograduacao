#!/bin/bash

echo "================================================"
echo "  Servidor Backend LoRa Gateway - Linux"
echo "================================================"
echo

# Vai para o diretorio do script
cd "$(dirname "$0")"

# Verifica se Python esta instalado
if ! command -v python3 &> /dev/null; then
    echo "[ERRO] Python3 nao encontrado!"
    echo "Instale com: sudo apt-get install python3 python3-venv python3-pip"
    exit 1
fi

# Verifica se o ambiente virtual existe
if [ ! -d "venv" ]; then
    echo "[SETUP] Criando ambiente virtual..."
    python3 -m venv venv
    if [ $? -ne 0 ]; then
        echo "[ERRO] Falha ao criar ambiente virtual!"
        exit 1
    fi
fi

# Ativa ambiente virtual
echo "[SETUP] Ativando ambiente virtual..."
source venv/bin/activate

# Instala dependencias
echo "[SETUP] Verificando dependencias..."
pip install -q -r requirements.txt
if [ $? -ne 0 ]; then
    echo "[ERRO] Falha ao instalar dependencias!"
    exit 1
fi

# Inicia servidor
echo
echo "[START] Iniciando servidor..."
echo
python app.py
