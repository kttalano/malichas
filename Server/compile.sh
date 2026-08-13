#!/bin/bash

# Salir inmediatamente si algún comando falla
set -e

echo "=========================================="
echo "   Iniciando proceso de actualización     "
echo "=========================================="

# 1. Compilar PRIMERO en un archivo temporal (Zero Downtime)
echo "1. Compilando código fuente..."

ccache g++ -O3 -I. -I./src \
    main_server.cpp \
    src/red.cpp \
    src/web_server.cpp \
    src/web_routes.cpp \
    src/web_dashboard.cpp \
    src/web_ventas.cpp \
    src/web_liquidaciones.cpp \
    src/web_api.cpp \
    src/gemini_pdf.cpp \
    src/web_utils.cpp \
    src/web_templates.cpp \
    src/web_scanner.cpp \
    src/web_empretienda.cpp \
    -o malichas_server_new \
    -lsfml-network -lsfml-system -lpthread -lssl -lcrypto -std=c++17 -D_SERVER_ONLY

echo "2. Compilación exitosa. Aplicando cambios..."

# 2. Detener SOLO el servicio de Malichas
sudo systemctl stop malichas

# 3. Reemplazar el programa viejo por el nuevo y darle permisos
mv malichas_server_new malichas_server
chmod +x malichas_server

# 4. Volver a encender
sudo systemctl start malichas

echo "=========================================="
echo "   ¡Servidor actualizado con éxito!       "
echo "=========================================="