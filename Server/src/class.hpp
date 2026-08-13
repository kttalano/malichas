#pragma once

#include <map>
#include <unordered_map>
#include <unordered_set>
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <algorithm>
#include <mutex>
#include <ctime>
#include <cstdlib>
#include <random>
#include <cctype>
#include <cmath>
#include <atomic>
#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#define _mkdir(path) mkdir(path, 0777)
#endif
#include <thread>
#include <filesystem>
#include <chrono>
#include <cstdio>
#include "../json.hpp"

using json = nlohmann::json;
namespace fs = std::filesystem;

extern std::recursive_mutex mutexTienda;
extern std::recursive_mutex diskMutex;

inline std::atomic<int> escriturasPendientes{0};

inline int CompareNatural(const std::string &a, const std::string &b)
{
    int i = 0, j = 0;
    while (i < a.length() && j < b.length())
    {
        if (std::isdigit(static_cast<unsigned char>(a[i])) && std::isdigit(static_cast<unsigned char>(b[j])))
        {
            int startA = i, startB = j;
            while (i < a.length() && std::isdigit(static_cast<unsigned char>(a[i])))
                i++;
            while (j < b.length() && std::isdigit(static_cast<unsigned char>(b[j])))
                j++;
            std::string numA = a.substr(startA, i - startA);
            std::string numB = b.substr(startB, j - startB);

            size_t trimA = 0;
            while (trimA < numA.length() - 1 && numA[trimA] == '0')
                trimA++;
            size_t trimB = 0;
            while (trimB < numB.length() - 1 && numB[trimB] == '0')
                trimB++;
            std::string valA = numA.substr(trimA);
            std::string valB = numB.substr(trimB);

            if (valA.length() != valB.length())
                return valA.length() < valB.length() ? -1 : 1;
            if (valA != valB)
                return valA < valB ? -1 : 1;
        }
        else
        {
            if (a[i] != b[j])
                return a[i] < b[j] ? -1 : 1;
            i++;
            j++;
        }
    }
    if (a.length() == b.length())
        return 0;
    return a.length() < b.length() ? -1 : 1;
}

inline bool NaturalLess(const std::string &a, const std::string &b)
{
    return CompareNatural(a, b) < 0;
}

inline int GetSizeRank(const std::string &sizeStr)
{
    std::string size = sizeStr;
    for (char &c : size)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (size == "xxs")
        return 1;
    if (size == "xs")
        return 2;
    if (size == "s")
        return 3;
    if (size == "m")
        return 4;
    if (size == "l")
        return 5;
    if (size == "xl")
        return 6;
    if (size == "xxl")
        return 7;
    if (size == "xxxl")
        return 8;
    return 99;
}

enum class Rol
{
    Ninguno = 0,
    WebCliente = 1,
    Vendedora = 2,
    Supervisor = 3,
    Owner = 4,
    Admin = 5
};

namespace Logger
{
    inline std::recursive_mutex logMutex;
    inline void Log(const std::string &mensaje)
    {
        std::lock_guard<std::recursive_mutex> lock(logMutex);
        std::ofstream archivo("log_red.txt", std::ios_base::app);
        if (archivo.is_open())
        {
            std::time_t t = std::time(nullptr);
            char buffer[32];
            std::strftime(buffer, sizeof(buffer), "%H:%M:%S ", std::localtime(&t));
            archivo << buffer << mensaje << std::endl;
        }
    }
}

inline std::string GenerarUUID(const std::string &prefijo)
{
    static const char alphanum[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    thread_local std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<> dis(0, sizeof(alphanum) - 2);
    std::string uuid;
    uuid.reserve(10);
    for (int i = 0; i < 10; ++i)
        uuid += alphanum[dis(gen)];
    return prefijo + "-" + std::to_string(std::time(nullptr)) + "-" + uuid;
}

class Category
{
public:
    std::string m_name;
    Category() = default;
    Category(const std::string &name) : m_name(name) {}
    bool operator==(const Category &o) const { return m_name == o.m_name; }
    bool operator!=(const Category &o) const { return !(*this == o); }
};
class Brand
{
public:
    std::string m_name;
    Brand() = default;
    Brand(const std::string &name) : m_name(name) {}
    bool operator==(const Brand &o) const { return m_name == o.m_name; }
    bool operator!=(const Brand &o) const { return !(*this == o); }
};

struct Variante
{
    std::vector<std::string> m_codigoBarras;
    std::string m_talle;
    std::string m_color;
    int m_stock = 0;
    float m_precioEspecifico = 0.0f;
    Variante() = default;
    Variante(const std::vector<std::string> &barras, const std::string &talle, const std::string &color, int stock, float precioEsp = 0.0f)
        : m_codigoBarras(barras), m_talle(talle), m_color(color), m_stock(stock), m_precioEspecifico(precioEsp) {}
    bool operator==(const Variante &o) const { return m_stock == o.m_stock && m_precioEspecifico == o.m_precioEspecifico && m_codigoBarras == o.m_codigoBarras && m_talle == o.m_talle && m_color == o.m_color; }
    bool operator!=(const Variante &o) const { return !(*this == o); }
};

struct ArticuloFamilia
{
    std::string m_sku;
    std::string m_nombre;
    Category m_categoria;
    Brand m_marca;
    float m_precioBase = 0.0f;
    std::vector<Variante> m_variantes;
    ArticuloFamilia() = default;
    ArticuloFamilia(const std::string &sku, const std::string &nombre, const Category &cat, const Brand &marca, float precioBase) : m_sku(sku), m_nombre(nombre), m_categoria(cat), m_marca(marca), m_precioBase(precioBase) {}
    float obtenerPrecioFinal(const Variante &v) const { return (v.m_precioEspecifico > 0.0f) ? v.m_precioEspecifico : m_precioBase; }
    bool operator==(const ArticuloFamilia &o) const { return m_sku == o.m_sku && m_nombre == o.m_nombre && m_precioBase == o.m_precioBase && m_categoria == o.m_categoria && m_marca == o.m_marca && m_variantes == o.m_variantes; }
    bool operator!=(const ArticuloFamilia &o) const { return !(*this == o); }
};

struct ConsignedItem
{
    std::string m_sku;
    std::string m_description;
    std::string m_size;
    std::string m_color;
    int m_quantity;
    int m_returned = 0;
    float m_price;
    ConsignedItem() = default;
    ConsignedItem(const std::string &sku, const std::string &desc, const std::string &size, const std::string &color, int qty, float price)
        : m_sku(sku), m_description(desc), m_size(size), m_color(color), m_quantity(qty), m_price(price) {}
};

struct Consignment
{
    std::string m_idRemito;
    std::string m_vendedora;
    std::string m_cliente;
    std::string m_fechaSalida;
    std::string m_fechaLimite;
    std::vector<ConsignedItem> m_items;
    int m_totalArticulos = 0;
    float m_totalAPagar = 0.0f;
    std::string m_estado = "Pendiente";
    Consignment() = default;
    Consignment(const std::string &vend, const std::string &fecha)
        : m_idRemito(GenerarUUID("REM")), m_vendedora(vend), m_fechaSalida(fecha), m_estado("Pendiente") {}
    void RecalcularTotales()
    {
        m_totalAPagar = 0.0f;
        m_totalArticulos = 0;
        for (const auto &item : m_items)
        {
            int vendidas = item.m_quantity - item.m_returned;
            m_totalAPagar += (vendidas * item.m_price);
            m_totalArticulos += item.m_quantity;
        }
    }
};

struct SaleItem
{
    std::string m_sku;
    std::string m_description;
    std::string m_size;
    std::string m_color;
    int m_quantity = 0;
    float m_price = 0.0f;
    SaleItem() = default;
    SaleItem(const std::string &sku, const std::string &desc, const std::string &size, const std::string &color, int qty, float price)
        : m_sku(sku), m_description(desc), m_size(size), m_color(color), m_quantity(qty), m_price(price) {}
};

struct Sale
{
    std::string m_idVenta;
    std::string m_cliente;
    std::string m_fecha;
    std::vector<SaleItem> m_items;
    int m_totalArticulos = 0;
    float m_totalAbonado = 0.0f;
    Sale() = default;
    Sale(const std::string &cli, const std::string &fecha)
        : m_idVenta(GenerarUUID("VT")), m_cliente(cli), m_fecha(fecha) {}
};

struct Movement
{
    std::string m_idMovimiento;
    std::string m_fecha;
    std::string m_usuario;
    std::string m_sku;
    std::string m_description;
    int m_cantidad = 0;
    std::string m_motivo;

    Movement() = default;
    Movement(const std::string &fecha, const std::string &usuario, const std::string &sku, const std::string &desc, int cantidad, const std::string &motivo)
        : m_idMovimiento(GenerarUUID("MOV")), m_fecha(fecha), m_usuario(usuario), m_sku(sku), m_description(desc), m_cantidad(cantidad), m_motivo(motivo) {}
};

struct AccionPendiente
{
    std::string m_idAccion;
    std::string m_tipoOperacion;
    std::string m_fecha;
    json m_payload;
    AccionPendiente() = default;
};

inline std::string CifrarDescifrarXOR(std::string data)
{
    std::string key = "TU_CLAVE_SECRETA_AQUI";
    for (size_t i = 0; i < data.size(); ++i)
    {
        data[i] ^= key[i % key.size()];
    }
    return data;
}

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Category, m_name)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Brand, m_name)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Variante, m_codigoBarras, m_talle, m_color, m_stock, m_precioEspecifico)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ArticuloFamilia, m_sku, m_nombre, m_categoria, m_marca, m_precioBase, m_variantes)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ConsignedItem, m_sku, m_description, m_size, m_color, m_quantity, m_returned, m_price)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Consignment, m_idRemito, m_vendedora, m_cliente, m_fechaSalida, m_fechaLimite, m_items, m_totalArticulos, m_totalAPagar, m_estado)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SaleItem, m_sku, m_description, m_size, m_color, m_quantity, m_price)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Sale, m_idVenta, m_cliente, m_fecha, m_items, m_totalArticulos, m_totalAbonado)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Movement, m_idMovimiento, m_fecha, m_usuario, m_sku, m_description, m_cantidad, m_motivo)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(AccionPendiente, m_idAccion, m_tipoOperacion, m_fecha, m_payload)

struct Store;
void SincronizarDatosRed(Store &tienda, const std::string &deltaDump);
void DifundirSyncRest(Store &tienda);

struct Store
{
    std::vector<ArticuloFamilia> inventory;
    std::vector<Consignment> consignments;
    std::vector<Sale> sales;
    std::vector<Movement> movements;

    unsigned short puertoRed = 53000;
    int cantidadClientes = 0;
    unsigned int dataVersion = 0;
    bool safeToSave = true;

    std::vector<ArticuloFamilia> snap_inventory;
    std::vector<Consignment> snap_consignments;
    std::vector<Sale> snap_sales;
    size_t snap_movs_count = 0;

    void updateSnapshots()
    {
        snap_inventory = inventory;
        snap_consignments = consignments;
        snap_sales = sales;
        snap_movs_count = movements.size();
    }

    void saveSnapshotsToDisk()
    {
        json j;
        j["inventory"] = snap_inventory;
        j["consignments"] = snap_consignments;
        j["sales"] = snap_sales;
        j["movs_count"] = snap_movs_count;
        guardarAtomico(j, "snapshots_sync.json");
    }

    void loadSnapshotsFromDisk()
    {
        std::ifstream f("snapshots_sync.json");
        if (f.is_open())
        {
            try
            {
                json j;
                f >> j;
                snap_inventory = j["inventory"].get<std::vector<ArticuloFamilia>>();
                snap_consignments = j["consignments"].get<std::vector<Consignment>>();
                snap_sales = j["sales"].get<std::vector<Sale>>();
                snap_movs_count = j["movs_count"].get<size_t>();
            }
            catch (...)
            {
                Logger::Log("Creando nuevos snapshots.");
                updateSnapshots();
            }
            f.close();
        }
        else
        {
            updateSnapshots();
            saveSnapshotsToDisk();
        }
    }

    void SanitizarPrecios()
    {
        std::lock_guard<std::recursive_mutex> lock(mutexTienda);
        bool huboCambios = false;
        for (auto &fam : inventory)
        {
            for (auto &var : fam.m_variantes)
            {
                if (var.m_precioEspecifico > 0.0f && std::abs(var.m_precioEspecifico - fam.m_precioBase) < 0.01f)
                {
                    var.m_precioEspecifico = 0.0f;
                    huboCambios = true;
                }
            }
        }
        if (huboCambios)
            dataVersion++;
    }

    void aplicarDelta(const json &delta)
    {
        std::lock_guard<std::recursive_mutex> lock(mutexTienda);

        if (delta.contains("mod_inv") && delta["mod_inv"].is_array())
        {
            auto mi = delta["mod_inv"].get<std::vector<ArticuloFamilia>>();
            std::unordered_map<std::string, size_t> inv_idx;

            for (size_t i = 0; i < inventory.size(); ++i)
            {
                std::string clave = inventory[i].m_sku + "|" + inventory[i].m_marca.m_name;
                inv_idx[clave] = i;
            }

            for (const auto &mod_f : mi)
            {
                std::string claveBusqueda = mod_f.m_sku + "|" + mod_f.m_marca.m_name;
                auto it = inv_idx.find(claveBusqueda);

                if (it != inv_idx.end())
                {
                    inventory[it->second] = mod_f;
                }
                else
                {
                    inventory.push_back(mod_f);
                    inv_idx[claveBusqueda] = inventory.size() - 1;
                }
            }
        }

        if (delta.contains("new_sales") && delta["new_sales"].is_array())
        {
            auto ns = delta["new_sales"].get<std::vector<Sale>>();
            for (const auto &nuevaVenta : ns)
            {
                auto it = std::find_if(sales.begin(), sales.end(), [&](const Sale &s)
                                       { return s.m_idVenta == nuevaVenta.m_idVenta; });
                if (it == sales.end())
                {
                    sales.push_back(nuevaVenta);
                }
            }
        }

        if (delta.contains("new_consignments") && delta["new_consignments"].is_array())
        {
            auto nc = delta["new_consignments"].get<std::vector<Consignment>>();
            for (const auto &nuevoRemito : nc)
            {
                auto it = std::find_if(consignments.begin(), consignments.end(), [&](const Consignment &c)
                                       { return c.m_idRemito == nuevoRemito.m_idRemito; });
                if (it == consignments.end())
                {
                    consignments.push_back(nuevoRemito);
                }
            }
        }

        if (delta.contains("new_movs") && delta["new_movs"].is_array())
        {
            auto nm = delta["new_movs"].get<std::vector<Movement>>();
            for (const auto &nuevoMov : nm)
            {
                auto it = std::find_if(movements.begin(), movements.end(), [&](const Movement &m)
                                       { return m.m_idMovimiento == nuevoMov.m_idMovimiento; });
                if (it == movements.end())
                {
                    movements.push_back(nuevoMov);
                }
            }
        }

        dataVersion++;
    }

    json generarDelta()
    {
        json delta;

        if (movements.size() > snap_movs_count)
        {
            std::vector<Movement> nm(movements.begin() + snap_movs_count, movements.end());
            delta["new_movs"] = nm;
        }

        if (sales.size() > snap_sales.size())
        {
            std::vector<Sale> ns(sales.begin() + snap_sales.size(), sales.end());
            delta["new_sales"] = ns;
        }

        if (consignments.size() > snap_consignments.size())
        {
            std::vector<Consignment> nc(consignments.begin() + snap_consignments.size(), consignments.end());
            delta["new_consignments"] = nc;
        }

        std::unordered_map<std::string, const ArticuloFamilia *> map_s_inv;
        for (const auto &snap : snap_inventory)
            map_s_inv[snap.m_sku + "|" + snap.m_marca.m_name] = &snap;
        std::vector<ArticuloFamilia> mod_inv;
        for (const auto &fam : inventory)
        {
            auto it = map_s_inv.find(fam.m_sku + "|" + fam.m_marca.m_name);
            if (it == map_s_inv.end() || fam != *(it->second))
                mod_inv.push_back(fam);
        }
        if (!mod_inv.empty())
            delta["mod_inv"] = mod_inv;

        return delta;
    }

    void guardarAtomico(const json &j, const std::string &path)
    {
        std::string tempPath = path + ".tmp";
        std::ofstream f(tempPath);
        if (f.is_open())
        {
            f << j.dump();
            if (f.bad() || f.fail())
            {
                Logger::Log("ERROR ESCRITURA: " + tempPath);
                f.close();
                fs::remove(tempPath);
                return;
            }
            f.close();
            std::remove(path.c_str());
            std::rename(tempPath.c_str(), path.c_str());
        }
    }

    void saveToFileAsync()
    {
        std::thread([this]()
                    { 
            escriturasPendientes++;
            saveToFile(); 
            escriturasPendientes--; })
            .detach();
    }

    void saveToFile()
    {
        if (!safeToSave)
            return;
        static std::recursive_mutex diskMutex;
        std::lock_guard<std::recursive_mutex> diskLock(diskMutex);

        mutexTienda.lock();
        dataVersion++;
        json deltaLocal = generarDelta();

        bool huboEdicion = false;
        if (consignments.size() == snap_consignments.size())
        {
            for (size_t i = 0; i < consignments.size(); ++i)
            {
                if (consignments[i].m_estado != snap_consignments[i].m_estado ||
                    consignments[i].m_totalAPagar != snap_consignments[i].m_totalAPagar)
                {
                    huboEdicion = true;
                    break;
                }
            }
        }
        if (!huboEdicion && sales.size() == snap_sales.size())
        {
            for (size_t i = 0; i < sales.size(); ++i)
            {
                if (sales[i].m_totalAbonado != snap_sales[i].m_totalAbonado ||
                    sales[i].m_totalArticulos != snap_sales[i].m_totalArticulos)
                {
                    huboEdicion = true;
                    break;
                }
            }
        }

        json jInv = inventory, jCons = consignments, jSales = sales, jMov = movements;
        mutexTienda.unlock();

        guardarAtomico(jInv, "inventario.json");
        guardarAtomico(jCons, "consignaciones.json");
        guardarAtomico(jSales, "ventas.json");
        guardarAtomico(jMov, "movimientos.json");

        if (!deltaLocal.empty())
        {
            SincronizarDatosRed(*this, deltaLocal.dump());
        }

        if (huboEdicion)
        {
            DifundirSyncRest(*this);
        }

        if (!deltaLocal.empty() || huboEdicion)
        {
            mutexTienda.lock();
            updateSnapshots();
            mutexTienda.unlock();
            saveSnapshotsToDisk();
        }
    }

    void loadFromFile()
    {
        auto leerSeguro = [&](const std::string &path, auto &destino)
        {
            if (!fs::exists(path))
                return;
            std::ifstream file(path);
            if (file.is_open())
            {
                try
                {
                    json j;
                    file >> j;
                    if (j.is_array())
                        destino = j.get<std::decay_t<decltype(destino)>>();
                }
                catch (const std::exception &e)
                {
                    Logger::Log("ERROR FATAL: " + std::string(e.what()));
                    safeToSave = false;
                }
                file.close();
            }
        };

        leerSeguro("inventario.json", inventory);
        leerSeguro("consignaciones.json", consignments);
        leerSeguro("ventas.json", sales);
        leerSeguro("movimientos.json", movements);
        loadSnapshotsFromDisk();

        OrdenarInventarioRaiz();
        SanitizarPrecios();
    }

    void crearRespaldos()
    {
        try
        {
            if (!fs::exists("backups"))
                fs::create_directory("backups");
            std::time_t t = std::time(nullptr);
            char dateBuf[32];
            std::strftime(dateBuf, sizeof(dateBuf), "%Y-%m-%d", std::localtime(&t));
            auto backupFile = [&](const std::string &fn)
            {
                if (fs::exists(fn) && !fs::exists("backups/" + std::string(dateBuf) + "_" + fn))
                {
                    fs::copy(fn, "backups/" + std::string(dateBuf) + "_" + fn);
                }
            };
            backupFile("inventario.json");
            backupFile("ventas.json");
            backupFile("consignaciones.json");
            backupFile("movimientos.json");
            limpiarBackups();
        }
        catch (...)
        {
        }
    }

    void limpiarBackups()
    {
        try
        {
            if (!fs::exists("backups"))
                return;
            auto now = fs::file_time_type::clock::now();
            for (auto &p : fs::directory_iterator("backups"))
            {
                if (now - fs::last_write_time(p) > std::chrono::hours(24 * 7))
                    fs::remove(p);
            }
        }
        catch (...)
        {
        }
    }

    void OrdenarInventarioRaiz()
    {
        std::lock_guard<std::recursive_mutex> lock(mutexTienda);

        std::sort(inventory.begin(), inventory.end(), [](const ArticuloFamilia &a, const ArticuloFamilia &b)
                  {
            if (a.m_sku == b.m_sku) return a.m_marca.m_name < b.m_marca.m_name; 
            return NaturalLess(a.m_sku, b.m_sku); });

        for (auto &fam : inventory)
        {
            std::sort(fam.m_variantes.begin(), fam.m_variantes.end(), [](const Variante &a, const Variante &b)
                      {
                if (a.m_color != b.m_color) return a.m_color < b.m_color;
                
                int rankA = GetSizeRank(a.m_talle);
                int rankB = GetSizeRank(b.m_talle);
                if (rankA != 99 && rankB != 99 && rankA != rankB) return rankA < rankB;
                
                return NaturalLess(a.m_talle, b.m_talle); });
        }
    }

    void procesarAccionesOffline(const json &accionesArray)
    {
        if (!accionesArray.is_array())
            return;

        auto buscarFamilia = [&](const std::string &skuBuscado, const std::string &marcaBuscada, const std::string &talle = "", const std::string &color = "")
        {
            auto it = std::lower_bound(inventory.begin(), inventory.end(), skuBuscado,
                                       [](const ArticuloFamilia &fam, const std::string &s)
                                       {
                                           return NaturalLess(fam.m_sku, s);
                                       });

            auto fallback = inventory.end();
            while (it != inventory.end() && it->m_sku == skuBuscado)
            {
                if (marcaBuscada.empty() || it->m_marca.m_name == marcaBuscada)
                {
                    if (fallback == inventory.end())
                        fallback = it;

                    if (!talle.empty() || !color.empty())
                    {
                        for (const auto &var : it->m_variantes)
                        {
                            if (var.m_talle == talle && var.m_color == color)
                            {
                                return it;
                            }
                        }
                    }
                    else
                    {
                        return it;
                    }
                }
                ++it;
            }
            return fallback;
        };

        auto limpiarCodigosDeBarras = [&](const std::vector<std::string> &codigosNuevos, const std::string &skuIgnorar, const std::string &marcaIgnorar, const std::string &talleIgnorar, const std::string &colorIgnorar)
        {
            if (codigosNuevos.empty())
                return;
            for (auto &fam : inventory)
            {
                for (auto &var : fam.m_variantes)
                {

                    if (fam.m_sku == skuIgnorar && fam.m_marca.m_name == marcaIgnorar && var.m_talle == talleIgnorar && var.m_color == colorIgnorar)
                        continue;

                    var.m_codigoBarras.erase(
                        std::remove_if(var.m_codigoBarras.begin(), var.m_codigoBarras.end(),
                                       [&](const std::string &cb)
                                       {
                                           return std::find(codigosNuevos.begin(), codigosNuevos.end(), cb) != codigosNuevos.end();
                                       }),
                        var.m_codigoBarras.end());
                }
            }
        };

        const json empty_payload = json::object();

        for (const auto &accion : accionesArray)
        {
            try
            {
                std::string tipo = accion.value("m_tipoOperacion", "");
                const json &payload = accion.contains("m_payload") ? accion["m_payload"] : empty_payload;
                std::string fechaTask = accion.value("m_fecha", "");

                auto extraerYAgregarMovimientos = [&](const json &p)
                {
                    if (p.contains("movimientos") && p["movimientos"].is_array())
                    {
                        try
                        {
                            auto movs = p["movimientos"].get<std::vector<Movement>>();
                            for (const auto &mov : movs)
                            {
                                auto it = std::find_if(movements.begin(), movements.end(), [&](const Movement &m)
                                                       { return m.m_idMovimiento == mov.m_idMovimiento; });
                                if (it == movements.end())
                                    movements.push_back(mov);
                            }
                        }
                        catch (...)
                        {
                            Logger::Log("Advertencia: No se pudo parsear la lista de movimientos adjuntos.");
                        }
                    }
                    else if (p.contains("movimiento"))
                    {
                        try
                        {
                            Movement mov = p["movimiento"].get<Movement>();
                            auto it = std::find_if(movements.begin(), movements.end(), [&](const Movement &m)
                                                   { return m.m_idMovimiento == mov.m_idMovimiento; });
                            if (it == movements.end())
                                movements.push_back(mov);
                        }
                        catch (...)
                        {
                            Logger::Log("Advertencia: No se pudo parsear el movimiento adjunto.");
                        }
                    }
                };

                if (tipo == "ELIMINAR_FAMILIA")
                {
                    std::string sku = payload.value("sku", "");
                    std::string marca = payload.value("marca", "");
                    auto it = buscarFamilia(sku, marca);
                    if (it != inventory.end())
                    {
                        inventory.erase(it);
                    }
                    extraerYAgregarMovimientos(payload);
                }

                else if (tipo == "ELIMINAR_VARIANTE")
                {
                    std::string sku = payload.value("sku", "");
                    std::string marca = payload.value("marca", "");
                    std::string talle = payload.value("talle", "");
                    std::string color = payload.value("color", "");

                    auto it = buscarFamilia(sku, marca, talle, color);
                    if (it != inventory.end())
                    {
                        auto itV = std::remove_if(it->m_variantes.begin(), it->m_variantes.end(), [&](const Variante &v)
                                                  { return v.m_talle == talle && v.m_color == color; });
                        if (itV != it->m_variantes.end())
                            it->m_variantes.erase(itV, it->m_variantes.end());
                    }
                    extraerYAgregarMovimientos(payload);
                }

                else if (tipo == "EDITAR_FAMILIA")
                {
                    std::string skuAnterior = payload.value("sku_anterior", "");
                    std::string marcaAnterior = payload.value("marca_anterior", payload.value("marca", ""));
                    auto it = buscarFamilia(skuAnterior, marcaAnterior);
                    if (it != inventory.end())
                    {
                        it->m_sku = payload.value("sku_nuevo", it->m_sku);
                        it->m_nombre = payload.value("nombre", it->m_nombre);
                        it->m_marca.m_name = payload.value("marca", it->m_marca.m_name);
                        it->m_categoria.m_name = payload.value("categoria", it->m_categoria.m_name);
                        it->m_precioBase = payload.value("precio_base", it->m_precioBase);
                    }
                    extraerYAgregarMovimientos(payload);
                }

                else if (tipo == "EDITAR_VARIANTE")
                {
                    std::string sku = payload.value("sku", "");
                    std::string marca = payload.value("marca", "");
                    std::string talleAnterior = payload.value("talle_anterior", "");
                    std::string colorAnterior = payload.value("color_anterior", "");

                    auto it = buscarFamilia(sku, marca, talleAnterior, colorAnterior);
                    if (it != inventory.end())
                    {
                        for (auto &var : it->m_variantes)
                        {
                            if (var.m_talle == talleAnterior && var.m_color == colorAnterior)
                            {
                                if (payload.contains("barras") && payload["barras"].is_array())
                                {
                                    auto nuevosCodigos = payload["barras"].get<std::vector<std::string>>();

                                    limpiarCodigosDeBarras(nuevosCodigos, sku, marca, payload.value("talle_nuevo", var.m_talle), payload.value("color_nuevo", var.m_color));
                                    var.m_codigoBarras = nuevosCodigos;
                                }
                                var.m_talle = payload.value("talle_nuevo", var.m_talle);
                                var.m_color = payload.value("color_nuevo", var.m_color);
                                var.m_stock = payload.value("stock", var.m_stock);
                                var.m_precioEspecifico = payload.value("precio_esp", var.m_precioEspecifico);
                                break;
                            }
                        }
                    }
                    extraerYAgregarMovimientos(payload);
                }

                else if (tipo == "AJUSTE_MASIVO")
                {
                    int filtroOpcion = payload.value("filtro_opcion", 0);
                    std::string filtroValor = payload.value("filtro_valor", "");
                    int tipoCalculo = payload.value("tipo_calculo", 0);
                    float valorInput = payload.value("valor_ajuste", 0.0f);

                    for (auto &fam : inventory)
                    {
                        bool coincide = false;
                        if (filtroOpcion == 0)
                            coincide = true;
                        else if (filtroOpcion == 1 && fam.m_marca.m_name == filtroValor)
                            coincide = true;
                        else if (filtroOpcion == 2 && fam.m_categoria.m_name == filtroValor)
                            coincide = true;

                        if (coincide)
                        {
                            if (tipoCalculo == 0)
                                fam.m_precioBase += fam.m_precioBase * (valorInput / 100.0f);
                            else
                                fam.m_precioBase += valorInput;
                            if (fam.m_precioBase < 0.0f)
                                fam.m_precioBase = 0.0f;
                            fam.m_precioBase = std::ceil(fam.m_precioBase / 100.0f) * 100.0f;
                        }
                    }
                    extraerYAgregarMovimientos(payload);
                }

                else if (tipo == "NUEVO_ARTICULO")
                {
                    std::string sku = payload.value("sku", "");
                    std::string nombre = payload.value("nombre", "");
                    std::string categoria = payload.value("categoria", "");
                    std::string marca = payload.value("marca", "");
                    std::vector<std::string> barras;
                    if (payload.contains("barras") && payload["barras"].is_array())
                        barras = payload["barras"].get<std::vector<std::string>>();
                    std::string talle = payload.value("talle", "");
                    std::string color = payload.value("color", "");
                    int stock = payload.value("stock", 1);
                    float precio = payload.value("precio", 0.0f);
                    float precio_esp = payload.value("precio_esp", 0.0f);

                    if (!barras.empty())
                    {
                        limpiarCodigosDeBarras(barras, sku, marca, talle, color);
                    }

                    auto itL = std::lower_bound(inventory.begin(), inventory.end(), sku,
                                                [](const ArticuloFamilia &fam, const std::string &s)
                                                { return NaturalLess(fam.m_sku, s); });

                    auto it = itL;
                    bool familiaEncontrada = false;

                    while (it != inventory.end() && it->m_sku == sku)
                    {
                        if (it->m_marca.m_name == marca)
                        {
                            familiaEncontrada = true;
                            break;
                        }
                        ++it;
                    }

                    if (familiaEncontrada)
                    {
                        bool varianteEncontrada = false;
                        for (auto &var : it->m_variantes)
                        {
                            if (var.m_talle == talle && var.m_color == color)
                            {
                                varianteEncontrada = true;
                                var.m_stock += stock;
                                var.m_precioEspecifico = precio_esp;
                                for (const auto &b : barras)
                                {
                                    if (std::find(var.m_codigoBarras.begin(), var.m_codigoBarras.end(), b) == var.m_codigoBarras.end())
                                    {
                                        var.m_codigoBarras.push_back(b);
                                    }
                                }
                                break;
                            }
                        }
                        if (!varianteEncontrada)
                            it->m_variantes.push_back(Variante(barras, talle, color, stock, precio_esp));
                    }
                    else
                    {
                        ArticuloFamilia nuevaFam(sku, nombre, Category(categoria), Brand(marca), precio);
                        nuevaFam.m_variantes.push_back(Variante(barras, talle, color, stock, precio_esp));
                        inventory.insert(itL, nuevaFam);
                    }
                    extraerYAgregarMovimientos(payload);
                }

                else if (tipo == "EDITAR_ARTICULO_COMPLETO")
                {
                    std::string skuAnt = payload.value("sku_anterior", "");
                    std::string marcaAnt = payload.value("marca_anterior", payload.value("marca", ""));
                    std::string talleAnt = payload.value("talle_anterior", "");
                    std::string colorAnt = payload.value("color_anterior", "");

                    auto it = buscarFamilia(skuAnt, marcaAnt, talleAnt, colorAnt);
                    if (it != inventory.end())
                    {
                        for (auto &var : it->m_variantes)
                        {
                            if (var.m_talle == talleAnt && var.m_color == colorAnt)
                            {
                                it->m_sku = payload.value("sku_nuevo", it->m_sku);
                                it->m_nombre = payload.value("nombre", it->m_nombre);
                                it->m_categoria.m_name = payload.value("categoria", it->m_categoria.m_name);
                                it->m_marca.m_name = payload.value("marca", it->m_marca.m_name);

                                if (payload.contains("barras_nuevo") && payload["barras_nuevo"].is_array())
                                {
                                    auto nuevosCodigos = payload["barras_nuevo"].get<std::vector<std::string>>();

                                    limpiarCodigosDeBarras(nuevosCodigos, it->m_sku, it->m_marca.m_name, payload.value("talle_nuevo", var.m_talle), payload.value("color_nuevo", var.m_color));
                                    var.m_codigoBarras = nuevosCodigos;
                                }

                                var.m_talle = payload.value("talle_nuevo", var.m_talle);
                                var.m_color = payload.value("color_nuevo", var.m_color);
                                var.m_stock = payload.value("stock", var.m_stock);
                                var.m_precioEspecifico = payload.value("precio_esp", var.m_precioEspecifico);
                                break;
                            }
                        }
                    }
                    extraerYAgregarMovimientos(payload);
                }

                else if (tipo == "NUEVA_VENTA")
                {
                    std::string fecha = payload.value("fecha", "");
                    std::string usuario = payload.value("usuario", "");
                    const json &items = payload.contains("items") ? payload["items"] : empty_payload;

                    Sale nuevaVenta(usuario, fecha);
                    std::time_t timeId = std::time(nullptr);
                    nuevaVenta.m_idVenta = "VT-" + std::to_string(timeId) + "-" + std::to_string(rand() % 10000);

                    float totalAbonado = 0.0f;
                    int totalArticulos = 0;

                    if (items.is_array())
                    {
                        for (const auto &jItem : items)
                        {
                            std::string sku = jItem.value("sku", "");
                            std::string marca = jItem.value("marca", "");
                            std::string desc = jItem.value("descripcion", "");
                            std::string talle = jItem.value("talle", "");
                            std::string color = jItem.value("color", "");
                            int qty = jItem.value("cantidad", 0);
                            float precio = jItem.value("precio", 0.0f);

                            nuevaVenta.m_items.push_back(SaleItem(sku, desc, talle, color, qty, precio));
                            totalArticulos += qty;
                            totalAbonado += (precio * qty);

                            auto it = buscarFamilia(sku, marca, talle, color);
                            if (it != inventory.end())
                            {
                                for (auto &var : it->m_variantes)
                                {
                                    if (var.m_talle == talle && var.m_color == color)
                                    {
                                        var.m_stock -= qty;
                                        break;
                                    }
                                }
                            }
                            movements.push_back(Movement(fecha, usuario, sku, desc, -qty, "Venta Directa"));
                        }
                    }
                    nuevaVenta.m_totalArticulos = totalArticulos;
                    nuevaVenta.m_totalAbonado = totalAbonado;
                    sales.push_back(nuevaVenta);
                }

                else if (tipo == "NUEVA_CONSIGNACION")
                {
                    std::string vendedora = payload.value("vendedora", "");
                    std::string cliente = payload.value("cliente", "");
                    std::string fechaSalida = payload.value("fecha_salida", "");
                    std::string fechaLimite = payload.value("fecha_limite", "");
                    std::string usuario = payload.value("usuario", "");
                    const json &items = payload.contains("items") ? payload["items"] : empty_payload;

                    Consignment nuevaCons(vendedora, fechaSalida);
                    nuevaCons.m_cliente = cliente;
                    nuevaCons.m_fechaLimite = fechaLimite;
                    std::time_t timeId = std::time(nullptr);
                    nuevaCons.m_idRemito = "REM-" + std::to_string(timeId) + "-" + std::to_string(rand() % 10000);

                    int totalArticulos = 0;
                    if (items.is_array())
                    {
                        for (const auto &jItem : items)
                        {
                            std::string sku = jItem.value("sku", "");
                            std::string marca = jItem.value("marca", "");
                            std::string desc = jItem.value("descripcion", "");
                            std::string talle = jItem.value("talle", "");
                            std::string color = jItem.value("color", "");
                            int qty = jItem.value("cantidad", 0);
                            float precio = jItem.value("precio", 0.0f);

                            nuevaCons.m_items.push_back(ConsignedItem(sku, desc, talle, color, qty, precio));
                            totalArticulos += qty;

                            auto it = buscarFamilia(sku, marca, talle, color);
                            if (it != inventory.end())
                            {
                                for (auto &var : it->m_variantes)
                                {
                                    if (var.m_talle == talle && var.m_color == color)
                                    {
                                        var.m_stock -= qty;
                                        break;
                                    }
                                }
                            }
                            movements.push_back(Movement(fechaSalida, usuario, sku, desc, -qty, "Entrega a Revendedora (" + cliente + ")"));
                        }
                    }
                    nuevaCons.m_totalArticulos = totalArticulos;
                    consignments.push_back(nuevaCons);
                }
                else if (tipo == "LIQUIDAR_CONSIGNACION")
                {
                    std::string idRemito = payload.value("id_remito", "");
                    std::string usuario = payload.value("usuario", "");
                    const json &devoluciones = payload.contains("devoluciones") ? payload["devoluciones"] : empty_payload;

                    std::time_t t = std::time(nullptr);
                    std::tm *now = std::localtime(&t);
                    char bufferFechaCorta[32];
                    snprintf(bufferFechaCorta, sizeof(bufferFechaCorta), "%02d/%02d/%04d", now->tm_mday, now->tm_mon + 1, now->tm_year + 1900);

                    for (auto &cons : consignments)
                    {
                        if (cons.m_idRemito == idRemito)
                        {
                            float totalACobrar = 0.0f;
                            for (size_t k = 0; k < cons.m_items.size(); ++k)
                            {
                                int qtyDevuelta = 0;
                                std::string marcaDevolucion = "";
                                if (devoluciones.is_array())
                                {
                                    for (const auto &dev : devoluciones)
                                    {
                                        if (dev["sku"] == cons.m_items[k].m_sku && dev["talle"] == cons.m_items[k].m_size && dev["color"] == cons.m_items[k].m_color)
                                        {
                                            qtyDevuelta = dev.value("devuelto", 0);
                                            marcaDevolucion = dev.value("marca", "");
                                            break;
                                        }
                                    }
                                }

                                cons.m_items[k].m_returned = qtyDevuelta;

                                if (qtyDevuelta > 0)
                                {
                                    auto it = buscarFamilia(cons.m_items[k].m_sku, marcaDevolucion, cons.m_items[k].m_size, cons.m_items[k].m_color);
                                    if (it != inventory.end())
                                    {
                                        for (auto &var : it->m_variantes)
                                        {
                                            if (var.m_talle == cons.m_items[k].m_size && var.m_color == cons.m_items[k].m_color)
                                            {
                                                var.m_stock += qtyDevuelta;
                                                break;
                                            }
                                        }
                                    }
                                    movements.push_back(Movement(fechaTask, usuario, cons.m_items[k].m_sku, cons.m_items[k].m_description, qtyDevuelta, "Devolución Remito (" + cons.m_cliente + ")"));
                                }
                                int vendidos = cons.m_items[k].m_quantity - qtyDevuelta;
                                totalACobrar += (vendidos * cons.m_items[k].m_price);
                            }
                            cons.m_estado = "Pagado";
                            cons.m_totalAPagar = totalACobrar;
                            cons.m_fechaLimite = bufferFechaCorta;
                            break;
                        }
                    }
                }
                else if (tipo == "EDITAR_CONSIGNACION")
                {
                    std::string idRemito = payload.value("id_remito", "");
                    std::string usuario = payload.value("usuario", "");
                    const json &correcciones = payload.contains("correcciones") ? payload["correcciones"] : empty_payload;

                    for (auto &cons : consignments)
                    {
                        if (cons.m_idRemito == idRemito)
                        {
                            float nuevoTotalACobrar = 0.0f;
                            for (size_t k = 0; k < cons.m_items.size(); ++k)
                            {
                                int qtyDevueltaNueva = cons.m_items[k].m_returned;
                                std::string marcaEdicion = "";
                                if (correcciones.is_array())
                                {
                                    for (const auto &corr : correcciones)
                                    {
                                        if (corr["sku"] == cons.m_items[k].m_sku && corr["talle"] == cons.m_items[k].m_size && corr["color"] == cons.m_items[k].m_color)
                                        {
                                            qtyDevueltaNueva = corr.value("devuelto_nuevo", cons.m_items[k].m_returned);
                                            marcaEdicion = corr.value("marca", "");
                                            break;
                                        }
                                    }
                                }

                                int qtyDevueltaVieja = cons.m_items[k].m_returned;
                                int diferenciaStock = qtyDevueltaNueva - qtyDevueltaVieja;

                                if (diferenciaStock != 0)
                                {
                                    auto it = buscarFamilia(cons.m_items[k].m_sku, marcaEdicion, cons.m_items[k].m_size, cons.m_items[k].m_color);
                                    if (it != inventory.end())
                                    {
                                        for (auto &var : it->m_variantes)
                                        {
                                            if (var.m_talle == cons.m_items[k].m_size && var.m_color == cons.m_items[k].m_color)
                                            {
                                                var.m_stock += diferenciaStock;
                                                break;
                                            }
                                        }
                                    }
                                    movements.push_back(Movement(fechaTask, usuario, cons.m_items[k].m_sku, cons.m_items[k].m_description, diferenciaStock, "Edicion (Correccion de Remito)"));
                                }
                                cons.m_items[k].m_returned = qtyDevueltaNueva;
                                int vendidos = cons.m_items[k].m_quantity - qtyDevueltaNueva;
                                nuevoTotalACobrar += (vendidos * cons.m_items[k].m_price);
                            }
                            cons.m_totalAPagar = nuevoTotalACobrar;
                            break;
                        }
                    }
                }
                else if (tipo == "EDITAR_VENTA")
                {
                    std::string idVenta = payload.value("id_venta", "");
                    std::string usuario = payload.value("usuario", "");
                    const json &correcciones = payload.contains("correcciones") ? payload["correcciones"] : empty_payload;

                    for (auto &v_opt : sales)
                    {
                        if (v_opt.m_idVenta == idVenta)
                        {
                            float nuevoTotalACobrar = 0.0f;
                            int nuevoTotalPrendas = 0;
                            for (size_t k = 0; k < v_opt.m_items.size(); ++k)
                            {
                                int cantNueva = v_opt.m_items[k].m_quantity;
                                std::string marcaEdicion = "";
                                if (correcciones.is_array())
                                {
                                    for (const auto &corr : correcciones)
                                    {
                                        if (corr["sku"] == v_opt.m_items[k].m_sku && corr["talle"] == v_opt.m_items[k].m_size && corr["color"] == v_opt.m_items[k].m_color)
                                        {
                                            cantNueva = corr.value("cantidad_nueva", v_opt.m_items[k].m_quantity);
                                            marcaEdicion = corr.value("marca", "");
                                            break;
                                        }
                                    }
                                }

                                int cantOriginal = v_opt.m_items[k].m_quantity;
                                int difVendida = cantNueva - cantOriginal;

                                if (difVendida != 0)
                                {
                                    auto it = buscarFamilia(v_opt.m_items[k].m_sku, marcaEdicion, v_opt.m_items[k].m_size, v_opt.m_items[k].m_color);
                                    if (it != inventory.end())
                                    {
                                        for (auto &var : it->m_variantes)
                                        {
                                            if (var.m_talle == v_opt.m_items[k].m_size && var.m_color == v_opt.m_items[k].m_color)
                                            {
                                                var.m_stock -= difVendida;
                                                break;
                                            }
                                        }
                                    }
                                    movements.push_back(Movement(fechaTask, usuario, v_opt.m_items[k].m_sku, v_opt.m_items[k].m_description, -difVendida, "Edicion (Correccion de Venta)"));
                                }
                                v_opt.m_items[k].m_quantity = cantNueva;
                                nuevoTotalACobrar += (cantNueva * v_opt.m_items[k].m_price);
                                nuevoTotalPrendas += cantNueva;
                            }
                            v_opt.m_totalAbonado = nuevoTotalACobrar;
                            v_opt.m_totalArticulos = nuevoTotalPrendas;
                            break;
                        }
                    }
                }
                else if (tipo == "AGREGAR_VARIANTE")
                {
                    std::string sku = payload.value("sku", "");
                    std::string marca = payload.value("marca", "");

                    auto it = buscarFamilia(sku, marca);
                    if (it != inventory.end())
                    {
                        std::vector<std::string> barras = payload["barras"].get<std::vector<std::string>>();
                        std::string talle = payload["talle_nuevo"];
                        std::string color = payload["color_nuevo"];
                        int stock = payload["stock"];
                        float precioEsp = payload.value("precio_esp", 0.0f);

                        limpiarCodigosDeBarras(barras, sku, marca, talle, color);

                        Variante nuevaVar(barras, talle, color, stock, precioEsp);
                        it->m_variantes.push_back(nuevaVar);
                    }
                }
            }
            catch (const std::exception &e)
            {
                Logger::Log("⚠️ Error procesando tarea offline: " + std::string(e.what()));
            }
        }

        if (!accionesArray.empty())
        {
            SanitizarPrecios();
            OrdenarInventarioRaiz();
        }
    }
};