#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>
#include "src/class.hpp" 
#include "src/red.hpp"
#include "src/web_server.h" 

std::atomic<bool> servidorCorriendo(true);

void manejarSenal(int signum) {
    Logger::Log("Senal de interrupcion (" + std::to_string(signum) + ") recibida. Iniciando APAGADO SEGURO...");
    servidorCorriendo = false; 
}

void HiloAutosave(Store* tienda) {
    while (servidorCorriendo) {
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
    signal(SIGINT, manejarSenal);
    signal(SIGTERM, manejarSenal);

    Logger::Log("==================================================");
    Logger::Log("          INICIANDO SERVIDOR MALICHAS             ");
    Logger::Log("==================================================");

    Store miTienda;
    miTienda.loadFromFile();
    
    miTienda.crearRespaldos();

    Red::IniciarServidor(miTienda);
    Logger::Log("Servidor de sockets escuchando exitosamente...");

    std::thread tAutosave(HiloAutosave, &miTienda);
    tAutosave.detach();

    std::thread tWeb(WebServer::Iniciar, &miTienda);
    tWeb.detach();

    while (servidorCorriendo) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    Logger::Log("Guardando estado en disco antes de salir...");
    miTienda.saveToFile();
    
    Logger::Log("Deteniendo Servidor Web...");
    WebServer::Detener(); 
    
    Logger::Log("Desconectando clientes de escritorio...");
    Red::ApagarServidorAmablemente();   
    
    Logger::Log("Esperando que finalicen las escrituras en disco en segundo plano...");
    while(escriturasPendientes > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    Logger::Log("Servidor apagado correctamente. ¡Hasta luego!");
    return 0;
}   
