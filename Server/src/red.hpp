#pragma once
#include "class.hpp"
#include <mutex>

extern std::recursive_mutex mutexTienda;

namespace Red
{
    void IniciarServidor(Store &tienda);
    void ApagarServidorAmablemente();
}