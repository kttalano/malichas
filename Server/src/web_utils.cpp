#include "web_utils.hpp"
#include "class.hpp"
#include <mutex>
#include <sstream>
#include <iomanip>
#include <ctime>

namespace WebUtils
{
    std::string formatMoney(float value)
    {
        std::stringstream ss;
        ss << "$" << std::fixed << std::setprecision(0) << value;
        return ss.str();
    }

    bool esMesActual(const std::string &fechaStr)
    {
        if (fechaStr.length() < 10)
            return false;
        std::time_t t = std::time(nullptr);
        std::tm *now = std::localtime(&t);
        char mesActual[32];
        snprintf(mesActual, sizeof(mesActual), "%02d/%04d", now->tm_mon + 1, now->tm_year + 1900);
        return fechaStr.find(mesActual) != std::string::npos;
    }

    void ActualizarPreciosEnRemitosPendientes(Store *tienda)
    {
        bool tiendaModificada = false;

        {
            std::lock_guard<std::recursive_mutex> lock(mutexTienda);

            for (auto &cons : tienda->consignments)
            {
                if (cons.m_estado == "Pendiente")
                {
                    bool huboCambios = false;

                    for (auto &item : cons.m_items)
                    {
                        for (const auto &fam : tienda->inventory)
                        {
                            if (fam.m_sku == item.m_sku)
                            {
                                for (const auto &var : fam.m_variantes)
                                {
                                    if (var.m_talle == item.m_size && var.m_color == item.m_color)
                                    {
                                        float precioActual = fam.obtenerPrecioFinal(var);
                                        if (item.m_price != precioActual)
                                        {
                                            item.m_price = precioActual;
                                            huboCambios = true;
                                            tiendaModificada = true;
                                        }
                                    }
                                }
                            }
                        }
                    }

                    if (huboCambios)
                    {
                        cons.RecalcularTotales();
                    }
                }
            }
        }

        if (tiendaModificada)
        {
            tienda->saveToFileAsync();
        }
    }
}