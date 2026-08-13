#pragma once
#include <string>

struct Store;

namespace WebUtils {
    std::string formatMoney(float value);
    bool esMesActual(const std::string& fechaStr);
    void ActualizarPreciosEnRemitosPendientes(Store* tienda);
}
