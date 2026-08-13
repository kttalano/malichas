#include "red.hpp"
#include "class.hpp"
#include <thread>
#include <vector>
#include <iostream>
#include <memory>
#include <SFML/System/Clock.hpp>
#include <SFML/Network.hpp>
#include <fstream>
#include <atomic>

std::recursive_mutex mutexTienda;
std::recursive_mutex diskMutex;
std::recursive_mutex mutexClientes;

const std::string SECRET_TOKEN = "Token_Secreto";
extern std::atomic<bool> servidorCorriendo;

struct ClienteNet
{
    std::unique_ptr<sf::TcpSocket> socket;
    sf::Packet paquetePendiente;
    bool autenticado = false;
    sf::Clock timerConexion;
    sf::Clock timerRateLimit;
    sf::Clock timerUltimoMensaje;
    int mensajesPorSegundo = 0;

    ClienteNet() { socket = std::make_unique<sf::TcpSocket>(); }
};

std::vector<std::unique_ptr<ClienteNet>> clientesConectados;
ClienteNet *clienteActualProcesando = nullptr;

void SincronizarDatosRed(Store &tienda, const std::string &deltaDump)
{
    sf::Packet aviso;
    aviso << "APPLY_DELTA" << deltaDump;
    std::lock_guard<std::recursive_mutex> lock(mutexClientes);
    for (auto &c : clientesConectados)
    {
        if (c->autenticado && c.get() != clienteActualProcesando)
        {
            c->socket->setBlocking(true);
            c->socket->send(aviso);
            c->socket->setBlocking(false);
        }
    }
}

void DifundirSyncRest(Store &tienda)
{
    json jCons, jSales, jMov;
    {
        std::lock_guard<std::recursive_mutex> lock(mutexTienda);
        jCons = tienda.consignments;
        jSales = tienda.sales;
        jMov = tienda.movements;
    }

    sf::Packet pRest;
    pRest << "SYNC_REST" << jCons.dump() << jSales.dump() << jMov.dump();

    std::lock_guard<std::recursive_mutex> lock(mutexClientes);
    for (auto &c : clientesConectados)
    {
        if (c->autenticado && c.get() != clienteActualProcesando)
        {
            c->socket->setBlocking(true);
            c->socket->send(pRest);
            c->socket->setBlocking(false);
        }
    }
}

namespace Red
{

    std::unique_ptr<sf::TcpListener> listenerGlobal;

    void ApagarServidorAmablemente()
    {
        sf::Packet aviso;
        aviso << "HOST_SHUTDOWN";

        mutexClientes.lock();
        for (auto &c : clientesConectados)
        {
            c->socket->setBlocking(false);
            c->socket->send(aviso);
            c->socket->disconnect();
        }
        clientesConectados.clear();
        mutexClientes.unlock();

        if (listenerGlobal)
        {
            listenerGlobal->close();
            listenerGlobal.reset();
            Logger::Log("Listener SFML cerrado exitosamente. Puerto 53000 liberado.");
        }

        sf::sleep(sf::milliseconds(200));
    }

    void HiloServidor(Store *tienda)
    {
        listenerGlobal = std::make_unique<sf::TcpListener>();

        while (listenerGlobal->listen(tienda->puertoRed) != sf::Socket::Done && servidorCorriendo)
        {
            sf::sleep(sf::seconds(1));
        }

        if (listenerGlobal)
            listenerGlobal->setBlocking(false);

        while (servidorCorriendo)
        {
            auto nuevoCliente = std::make_unique<ClienteNet>();

            if (listenerGlobal && listenerGlobal->accept(*(nuevoCliente->socket)) == sf::Socket::Done)
            {
                nuevoCliente->socket->setBlocking(false);
                nuevoCliente->timerUltimoMensaje.restart();
                std::lock_guard<std::recursive_mutex> lock(mutexClientes);
                clientesConectados.push_back(std::move(nuevoCliente));
            }

            mutexClientes.lock();
            int cajasVerificadas = 0;

            for (auto it = clientesConectados.begin(); it != clientesConectados.end();)
            {
                ClienteNet *cliente = it->get();

                if (!cliente->autenticado && cliente->timerConexion.getElapsedTime().asSeconds() > 5.0f)
                {
                    it = clientesConectados.erase(it);
                    continue;
                }

                if (cliente->autenticado && cliente->timerUltimoMensaje.getElapsedTime().asSeconds() > 15.0f)
                {
                    it = clientesConectados.erase(it);
                    continue;
                }

                if (cliente->timerRateLimit.getElapsedTime().asSeconds() >= 1.0f)
                {
                    cliente->mensajesPorSegundo = 0;
                    cliente->timerRateLimit.restart();
                }

                sf::Socket::Status status = cliente->socket->receive(cliente->paquetePendiente);

                if (status == sf::Socket::Done)
                {
                    cliente->timerUltimoMensaje.restart();
                    cliente->mensajesPorSegundo++;

                    clienteActualProcesando = cliente;

                    if (cliente->mensajesPorSegundo > 30)
                    {
                        it = clientesConectados.erase(it);
                        clienteActualProcesando = nullptr;
                        continue;
                    }
                    else
                    {
                        std::string comando;
                        cliente->paquetePendiente >> comando;

                        if (!cliente->autenticado)
                        {
                            if (comando == "AUTH")
                            {
                                std::string tokenRecibido;
                                cliente->paquetePendiente >> tokenRecibido;
                                if (tokenRecibido == SECRET_TOKEN)
                                {
                                    cliente->autenticado = true;
                                    sf::Packet authOk;
                                    authOk << "AUTH_OK";
                                    cliente->socket->setBlocking(true);
                                    cliente->socket->send(authOk);
                                    cliente->socket->setBlocking(false);
                                }
                                else
                                {
                                    it = clientesConectados.erase(it);
                                    clienteActualProcesando = nullptr;
                                    continue;
                                }
                            }
                        }
                        else
                        {
                            if (comando == "REQ_USER_LOGIN")
                            {
                                std::string user, pass;
                                cliente->paquetePendiente >> user >> pass;

                                bool loginExitoso = false;
                                int rolAsignado = 0;

                                std::ifstream ifs("usuarios.json");
                                if (ifs.is_open())
                                {
                                    std::string contenido((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
                                    try
                                    {
                                        json jUsers = json::parse(contenido);
                                        if (jUsers.contains(user))
                                        {
                                            std::string passReal = jUsers[user]["password"].get<std::string>();
                                            std::string passRealLower = passReal;
                                            for (char &c : passRealLower)
                                                c = std::tolower(c);

                                            if (passRealLower == pass)
                                            {
                                                loginExitoso = true;
                                                rolAsignado = jUsers[user]["rol"].get<int>();
                                            }
                                        }
                                    }
                                    catch (...)
                                    {
                                        Logger::Log("Error parseando usuarios.json en el servidor.");
                                    }
                                    ifs.close();
                                }

                                sf::Packet pResp;
                                if (loginExitoso)
                                    pResp << "RESP_LOGIN_OK" << rolAsignado;
                                else
                                    pResp << "RESP_LOGIN_FAIL";

                                cliente->socket->setBlocking(true);
                                cliente->socket->send(pResp);
                                cliente->socket->setBlocking(false);
                            }
                            else if (comando == "REQ_SYNC_INV")
                            {
                                json jInv;
                                {
                                    std::lock_guard<std::recursive_mutex> lock(mutexTienda);
                                    jInv = tienda->inventory;
                                }
                                sf::Packet pInv;
                                pInv << "SYNC_INV" << jInv.dump();
                                cliente->socket->setBlocking(true);
                                cliente->socket->send(pInv);
                                cliente->socket->setBlocking(false);
                            }
                            else if (comando == "REQ_ALL_USERS")
                            {
                                std::ifstream ifs("usuarios.json");
                                if (ifs.is_open())
                                {
                                    std::string contenido((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
                                    sf::Packet respuesta;
                                    respuesta << "RESP_ALL_USERS" << contenido;
                                    cliente->socket->setBlocking(true);
                                    cliente->socket->send(respuesta);
                                    cliente->socket->setBlocking(false);
                                    ifs.close();
                                }
                            }
                            else if (comando == "REQ_USER_DB")
                            {
                                std::ifstream ifs("usuarios.json");
                                if (ifs.is_open())
                                {
                                    std::string contenido((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
                                    std::string datosEncriptados = CifrarDescifrarXOR(contenido);
                                    sf::Packet respuesta;
                                    respuesta << "SET_USER_DB" << datosEncriptados;
                                    cliente->socket->setBlocking(true);
                                    cliente->socket->send(respuesta);
                                    cliente->socket->setBlocking(false);
                                    ifs.close();
                                }
                            }
                            else if (comando == "REQ_SYNC_REST")
                            {
                                json jCons, jSales, jMov;
                                {
                                    std::lock_guard<std::recursive_mutex> lock(mutexTienda);
                                    jCons = tienda->consignments;
                                    jSales = tienda->sales;
                                    jMov = tienda->movements;
                                }
                                sf::Packet pRest;
                                pRest << "SYNC_REST" << jCons.dump() << jSales.dump() << jMov.dump();
                                cliente->socket->setBlocking(true);
                                cliente->socket->send(pRest);
                                cliente->socket->setBlocking(false);
                            }
                            else if (comando == "DELTA_UPDATE")
                            {
                                std::string payloadStr;
                                cliente->paquetePendiente >> payloadStr;
                                try
                                {
                                    json delta = json::parse(payloadStr);
                                    {
                                        std::lock_guard<std::recursive_mutex> lock(mutexTienda);
                                        tienda->aplicarDelta(delta);
                                    }
                                    tienda->saveToFileAsync();

                                    SincronizarDatosRed(*tienda, payloadStr);
                                }
                                catch (...)
                                {
                                    Logger::Log("⚠️ JSON corrupto recibido en DELTA_UPDATE.");
                                }
                            }
                            else if (comando == "OUTBOX_SYNC")
                            {
                                std::string payloadStr;
                                cliente->paquetePendiente >> payloadStr;
                                try
                                {
                                    json payloadJson = json::parse(payloadStr);
                                    {
                                        std::lock_guard<std::recursive_mutex> lock(mutexTienda);
                                        tienda->procesarAccionesOffline(payloadJson);
                                    }
                                    tienda->saveToFileAsync();
                                    DifundirSyncRest(*tienda);
                                }
                                catch (...)
                                {
                                    Logger::Log("⚠️ JSON corrupto recibido en OUTBOX_SYNC.");
                                }

                                sf::Packet ack;
                                ack << "OUTBOX_OK";
                                cliente->socket->setBlocking(true);
                                cliente->socket->send(ack);
                                cliente->socket->setBlocking(false);
                            }
                            else if (comando == "PING")
                            {
                                sf::Packet pong;
                                pong << "PONG";
                                cliente->socket->setBlocking(true);
                                cliente->socket->send(pong);
                                cliente->socket->setBlocking(false);
                            }
                        }
                    }
                    cliente->paquetePendiente.clear();
                    clienteActualProcesando = nullptr;
                    ++it;
                }
                else if (status == sf::Socket::Disconnected || status == sf::Socket::Error)
                {
                    it = clientesConectados.erase(it);
                }
                else
                {
                    ++it;
                }

                if (it != clientesConectados.end() && (*it)->autenticado)
                    cajasVerificadas++;
            }
            tienda->cantidadClientes = cajasVerificadas;
            mutexClientes.unlock();
            sf::sleep(sf::milliseconds(10));
        }
    }

    void IniciarServidor(Store &tienda)
    {
        std::thread t(HiloServidor, &tienda);
        t.detach();
    }

}