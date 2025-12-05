@echo off
title LoRa Gateway Server

echo ================================================
echo   Servidor Backend LoRa Gateway - Windows
echo ================================================
echo.

:: Verifica se Python esta instalado
python --version >nul 2>&1
if errorlevel 1 (
    echo [ERRO] Python nao encontrado!
    echo Instale Python 3.8+ de https://www.python.org/downloads/
    echo Marque "Add Python to PATH" durante a instalacao.
    pause
    exit /b 1
)

:: Verifica se o ambiente virtual existe
if not exist "venv" (
    echo [SETUP] Criando ambiente virtual...
    python -m venv venv
    if errorlevel 1 (
        echo [ERRO] Falha ao criar ambiente virtual!
        pause
        exit /b 1
    )
)

:: Ativa ambiente virtual
echo [SETUP] Ativando ambiente virtual...
call venv\Scripts\activate.bat

:: Instala dependencias
echo [SETUP] Verificando dependencias...
pip install -q -r requirements.txt
if errorlevel 1 (
    echo [ERRO] Falha ao instalar dependencias!
    pause
    exit /b 1
)

:: Inicia servidor
echo.
echo [START] Iniciando servidor...
echo.
python app.py

:: Pausa ao final
pause
