#include "web_routes.hpp"
#include "web_utils.hpp"
#include "web_auth.hpp"
#include <mutex>
#include <algorithm>

extern std::recursive_mutex mutexTienda;

namespace WebRoutes
{

    std::string toLowerString(const std::string &str)
    {
        std::string lowerStr = str;
        std::transform(lowerStr.begin(), lowerStr.end(), lowerStr.begin(), ::tolower);
        return lowerStr;
    }

    void handleApiLiquidarRemito(const httplib::Request &req, httplib::Response &res, Store *tienda)
    {
        std::string userEmail = req.get_header_value("Cf-Access-Authenticated-User-Email");
        std::string miNombre = WebAuth::obtenerNombreVendedora(userEmail);
        if (miNombre.empty())
            miNombre = "Administrador";

        try
        {
            json payload = json::parse(req.body);
            std::string idRemito = payload["id"].get<std::string>();

            time_t now = time(0);
            tm *ltm = localtime(&now);
            char buffer[20];
            strftime(buffer, sizeof(buffer), "%d/%m/%Y", ltm);
            std::string fechaHoy(buffer);

            bool encontrado = false;
            bool yaLiquidado = false;

            {
                std::lock_guard<std::recursive_mutex> lock(mutexTienda);

                for (auto &cons : tienda->consignments)
                {
                    if (cons.m_idRemito == idRemito)
                    {
                        encontrado = true;
                        if (cons.m_estado == "Pagado" || cons.m_estado == "Cerrado")
                        {
                            yaLiquidado = true;
                            break;
                        }

                        float nuevoTotalAPagar = 0.0f;

                        for (const auto &dev : payload["devoluciones"])
                        {
                            std::string sku = dev["sku"].get<std::string>();
                            std::string talle = dev["talle"].get<std::string>();
                            std::string color = dev["color"].get<std::string>();
                            int cantDevuelta = dev["devuelto"].get<int>();

                            for (auto &item : cons.m_items)
                            {
                                if (item.m_sku == sku && item.m_size == talle && item.m_color == color)
                                {
                                    item.m_returned = cantDevuelta;
                                    nuevoTotalAPagar += (item.m_quantity - item.m_returned) * item.m_price;

                                    if (cantDevuelta > 0)
                                    {
                                        for (auto &fam : tienda->inventory)
                                        {
                                            if (fam.m_sku == sku)
                                            {
                                                for (auto &var : fam.m_variantes)
                                                {
                                                    if (var.m_talle == talle && var.m_color == color)
                                                    {
                                                        var.m_stock += cantDevuelta;
                                                        std::string desc = fam.m_nombre + " [" + talle + "/" + color + "]";
                                                        Movement mov(fechaHoy, miNombre, sku, desc, cantDevuelta, "Devolución Remito (" + idRemito + ")");
                                                        tienda->movements.push_back(mov);
                                                        break;
                                                    }
                                                }
                                                break;
                                            }
                                        }
                                    }
                                    break;
                                }
                            }
                        }

                        cons.m_totalAPagar = nuevoTotalAPagar;
                        cons.m_estado = "Pagado";
                        cons.m_fechaLimite = fechaHoy;

                        int articulosVendidos = 0;
                        Sale nuevaVenta(miNombre, fechaHoy);
                        nuevaVenta.m_cliente = cons.m_cliente;
                        nuevaVenta.m_idVenta = "VTR-" + idRemito;

                        for (const auto &item : cons.m_items)
                        {
                            int vendidos = item.m_quantity - item.m_returned;
                            if (vendidos > 0)
                            {
                                SaleItem si(item.m_sku, item.m_description, item.m_size, item.m_color, vendidos, item.m_price);
                                nuevaVenta.m_items.push_back(si);
                                articulosVendidos += vendidos;
                            }
                        }

                        if (articulosVendidos > 0)
                        {
                            nuevaVenta.m_totalArticulos = articulosVendidos;
                            nuevaVenta.m_totalAbonado = nuevoTotalAPagar;
                            tienda->sales.push_back(nuevaVenta);
                        }
                        break;
                    }
                }
            }

            if (!encontrado)
            {
                res.set_content("{\"status\":\"error\", \"msg\":\"No encontrado\"}", "application/json");
            }
            else if (yaLiquidado)
            {
                res.set_content("{\"status\":\"error\", \"msg\":\"El remito ya está liquidado\"}", "application/json");
            }
            else
            {
                tienda->saveToFileAsync();
                res.set_content("{\"status\":\"ok\"}", "application/json");
            }
        }
        catch (...)
        {
            res.status = 500;
            res.set_content("{\"status\":\"error\", \"msg\":\"Error interno\"}", "application/json");
        }
    }

    void procesarCarrito(Store *tienda, const json &carrito, const std::string &vendedora, const std::string &tipo, const std::string &idTx)
    {
        time_t now = time(0);
        tm *ltm = localtime(&now);
        char buffer[20];
        strftime(buffer, sizeof(buffer), "%d/%m/%Y", ltm);
        std::string fechaHoy(buffer);

        for (const auto &item : carrito)
        {
            std::string sku = item["sku"].get<std::string>();
            std::string talle = item["talle"].get<std::string>();
            std::string color = item["color"].get<std::string>();
            int cantidad = item["cantidad"].get<int>();

            for (auto &fam : tienda->inventory)
            {
                if (fam.m_sku == sku)
                {
                    for (auto &var : fam.m_variantes)
                    {
                        if (var.m_talle == talle && var.m_color == color)
                        {
                            var.m_stock -= cantidad;
                            if (var.m_stock < 0)
                                var.m_stock = 0;

                            std::string desc = fam.m_nombre + " [" + talle + "/" + color + "]";
                            Movement mov(fechaHoy, vendedora, sku, desc, -cantidad, tipo + " (" + idTx + ")");
                            tienda->movements.push_back(mov);
                            break;
                        }
                    }
                    break;
                }
            }
        }
    }

    void handleApiGuardarVenta(const httplib::Request &req, httplib::Response &res, Store *tienda)
    {
        std::string userEmail = req.get_header_value("Cf-Access-Authenticated-User-Email");
        std::string miNombre = WebAuth::obtenerNombreVendedora(userEmail);
        if (miNombre.empty())
            miNombre = "Administrador";

        try
        {
            json payload = json::parse(req.body);
            time_t now = time(0);
            tm *ltm = localtime(&now);
            char buffer[20];
            strftime(buffer, sizeof(buffer), "%d/%m/%Y", ltm);

            Sale nuevaVenta(miNombre, std::string(buffer));

            for (const auto &item : payload["carrito"])
            {
                SaleItem si(
                    item["sku"].get<std::string>(),
                    item["desc"].get<std::string>(),
                    item["talle"].get<std::string>(),
                    item["color"].get<std::string>(),
                    item["cantidad"].get<int>(),
                    item["precio"].get<float>());
                nuevaVenta.m_items.push_back(si);
                nuevaVenta.m_totalArticulos += si.m_quantity;
                nuevaVenta.m_totalAbonado += (si.m_price * si.m_quantity);
            }

            {
                std::lock_guard<std::recursive_mutex> lock(mutexTienda);
                tienda->sales.push_back(nuevaVenta);
                procesarCarrito(tienda, payload["carrito"], miNombre, "Venta Directa", nuevaVenta.m_idVenta);
            }

            tienda->saveToFileAsync();
            res.set_content("{\"status\":\"ok\"}", "application/json");
        }
        catch (...)
        {
            res.status = 500;
            res.set_content("{\"status\":\"error\"}", "application/json");
        }
    }

    void handleApiGuardarRemito(const httplib::Request &req, httplib::Response &res, Store *tienda)
    {
        std::string userEmail = req.get_header_value("Cf-Access-Authenticated-User-Email");
        std::string miNombre = WebAuth::obtenerNombreVendedora(userEmail);
        if (miNombre.empty())
            miNombre = "Administrador";

        try
        {
            json payload = json::parse(req.body);
            time_t now = time(0);
            tm *ltm = localtime(&now);
            char buffer[20];
            strftime(buffer, sizeof(buffer), "%d/%m/%Y", ltm);
            std::string fechaHoy(buffer);

            time_t t_limite = now + (15 * 24 * 60 * 60);
            tm *ltm_limite = localtime(&t_limite);
            char buffer_limite[20];
            strftime(buffer_limite, sizeof(buffer_limite), "%d/%m/%Y", ltm_limite);
            std::string fechaLimite(buffer_limite);

            std::string nombreRevendedora = payload.value("revendedora", "Revendedora Desconocida");

            Consignment nuevoRemito(miNombre, fechaHoy);
            nuevoRemito.m_cliente = nombreRevendedora;
            nuevoRemito.m_fechaLimite = fechaLimite;

            for (const auto &item : payload["carrito"])
            {
                ConsignedItem ci(
                    item["sku"].get<std::string>(),
                    item["desc"].get<std::string>(),
                    item["talle"].get<std::string>(),
                    item["color"].get<std::string>(),
                    item["cantidad"].get<int>(),
                    item["precio"].get<float>());
                nuevoRemito.m_items.push_back(ci);
                nuevoRemito.m_totalArticulos += ci.m_quantity;
                nuevoRemito.m_totalAPagar += (ci.m_price * ci.m_quantity);
            }

            {
                std::lock_guard<std::recursive_mutex> lock(mutexTienda);
                tienda->consignments.push_back(nuevoRemito);
                procesarCarrito(tienda, payload["carrito"], miNombre, "Remito Armado", nuevoRemito.m_idRemito);
            }

            tienda->saveToFileAsync();
            res.set_content("{\"status\":\"ok\"}", "application/json");
        }
        catch (...)
        {
            res.status = 500;
            res.set_content("{\"status\":\"error\"}", "application/json");
        }
    }
}