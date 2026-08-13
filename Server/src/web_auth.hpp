#pragma once
#include <string>
#include <fstream>
#include <algorithm>
#include <cctype>
#include "class.hpp"

namespace WebAuth
{
    inline json cargarUsuariosJSON()
    {
        std::ifstream file("usuarios.json");
        if (file.is_open())
        {
            json j;
            try
            {
                file >> j;
                return j;
            }
            catch (...)
            {
                Logger::Log("Error al parsear usuarios.json");
            }
        }
        return json::object();
    }

    inline std::string obtenerNombreVendedora(const std::string &email)
    {
        if (email.empty())
            return "";
        std::string mailLower = email;
        std::transform(mailLower.begin(), mailLower.end(), mailLower.begin(), ::tolower);
        json usuarios = cargarUsuariosJSON();
        for (auto &[nombre, datos] : usuarios.items())
        {
            if (datos.contains("email"))
            {
                std::string mailDb = datos["email"].get<std::string>();
                std::transform(mailDb.begin(), mailDb.end(), mailDb.begin(), ::tolower);
                if (mailDb == mailLower)
                    return nombre;
            }
        }
        return "";
    }

    inline Rol obtenerRolPorEmail(const std::string &email)
    {
        if (email.empty())
            return Rol::Ninguno;
        std::string mailLower = email;
        std::transform(mailLower.begin(), mailLower.end(), mailLower.begin(), ::tolower);
        json usuarios = cargarUsuariosJSON();
        for (auto &[nombre, datos] : usuarios.items())
        {
            if (datos.contains("email"))
            {
                std::string mailDb = datos["email"].get<std::string>();
                std::transform(mailDb.begin(), mailDb.end(), mailDb.begin(), ::tolower);
                if (mailDb == mailLower)
                    return static_cast<Rol>(datos["rol"].get<int>());
            }
        }
        return Rol::WebCliente;
    }
}