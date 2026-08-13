#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>
#include "src/class.hpp" // O class_2.hpp, asegúrate de usar el nombre correcto en tu include
#include "src/red.hpp"
#include "src/web_server.h" 

// Variable atómica para controlar el ciclo de vida del servidor
std::atomic<bool> servidorCorriendo(true);

// Función que Linux llamará cuando intente apagar el servicio
void manejarSenal(int signum) {
    Logger::Log("Senal de interrupcion (" + std::to_string(signum) + ") recibida. Iniciando APAGADO SEGURO...");
    servidorCorriendo = false; 
}

void HiloAutosave(Store* tienda) {
    while (servidorCorriendo) {
        // Dormimos en intervalos de 1 segundo para poder salir rápido si se apaga el servidor
        // 3600 segundos = 1 hora
        for(int i = 0; i < 3600 && servidorCorriendo; ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        if (servidorCorriendo) {
            std::lock_guard<std::recursive_mutex> lock(mutexTienda);
            tienda->saveToFile(); // Guarda todo el inventario/ventas al disco
            Logger::Log("✅ Autosave automatico completado con exito.");
        }
    }
}

int main() {
    // Interceptar el apagado (Ctrl+C o systemctl stop)
    signal(SIGINT, manejarSenal);
    signal(SIGTERM, manejarSenal);

    Logger::Log("==================================================");
    Logger::Log("          INICIANDO SERVIDOR MALICHAS             ");
    Logger::Log("==================================================");

    Store miTienda;
    miTienda.loadFromFile();
    
    // Ejecutar el ciclo de backups al arrancar el día
    miTienda.crearRespaldos();

    // 1. Iniciar el Servidor de Sockets (Para los clientes de escritorio)
    Red::IniciarServidor(miTienda);
    Logger::Log("Servidor de sockets escuchando exitosamente...");

    // 2. Iniciar el Hilo de autoguardado
    std::thread tAutosave(HiloAutosave, &miTienda);
    tAutosave.detach();

    // 3. Iniciar el Servidor Web (Panel de control web)
    std::thread tWeb(WebServer::Iniciar, &miTienda);
    tWeb.detach();

    // Bucle principal atado a la variable atómica
    while (servidorCorriendo) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

// 🌟 RUTINA DE APAGADO SEGURO 🌟
    Logger::Log("Guardando estado en disco antes de salir...");
    miTienda.saveToFile();
    
    Logger::Log("Deteniendo Servidor Web...");
    WebServer::Detener(); 
    
    Logger::Log("Desconectando clientes de escritorio...");
    Red::ApagarServidorAmablemente();   
    
    // 🔥 FIX: Esperar a que todos los hilos de disco terminen antes de matar el proceso
    Logger::Log("Esperando que finalicen las escrituras en disco en segundo plano...");
    while(escriturasPendientes > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    Logger::Log("Servidor apagado correctamente. ¡Hasta luego!");
    return 0;
}   