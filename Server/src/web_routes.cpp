#include "web_routes.hpp"
#include "web_utils.hpp"
#include "web_templates.hpp"
#include "web_auth.hpp"
#include <map>
#include <mutex>
#include <string>
#include <algorithm>

extern std::recursive_mutex mutexTienda;

namespace WebRoutes
{

    void handleInventario(const httplib::Request &req, httplib::Response &res, Store *tienda)
    {
        std::string userEmail = req.get_header_value("Cf-Access-Authenticated-User-Email");
        Rol usuarioRol = WebAuth::obtenerRolPorEmail(userEmail);

        std::string html;
        html.reserve(8192);

        html += WebTemplates::getHeadAndNav("Inventario");
        html += "<script> window.MALI_USER_ROLE = " + std::to_string(static_cast<int>(usuarioRol)) + "; </script>";
        html += "<h2 class='page-title mb-4'>Gestión de Stock</h2>";

        if (usuarioRol == Rol::Owner || usuarioRol == Rol::Admin)
        {
            html += R"HTML(
                <div id='editSwitchContainer' class='card-glass p-2 mb-3 justify-content-between align-items-center' style='display: none !important; border: 2px dashed var(--mali-primary);'>
                    <span class='fw-bold ms-2 text-primary'><i class='fas fa-tools me-2'></i>Modo Edición</span>
                    <div class="form-check form-switch fs-4 mb-0 me-2">
                        <input class="form-check-input shadow-none" type="checkbox" id="switchModoEdicion" onchange="toggleModoEdicion(this.checked)">
                    </div>
                </div>
            )HTML";
        }

        html += R"HTML(
            <div class='card-glass p-3 mb-4'>
                <div class='input-group shadow-sm' style='border-radius: 16px; overflow: hidden; border: 2px solid var(--mali-primary);'>
                    <span class='input-group-text bg-transparent border-0'><i class='fas fa-barcode' style='color: var(--mali-primary);'></i></span>
                    <input type='text' inputmode='numeric' pattern='[0-9]*' id='searchBox' class='form-control border-0 bg-transparent shadow-none fs-5 fw-bold text-primary' placeholder='Buscar SKU...' onkeyup='if(event.key === "Enter") this.blur();'>
                </div>
            </div>
        )HTML";

        std::map<std::string, int> stockPorMarca;

        {
            std::lock_guard<std::recursive_mutex> lock(mutexTienda);
            for (const auto &fam : tienda->inventory)
            {
                if (fam.m_marca.m_name.empty())
                    continue;
                int stockFamilia = 0;
                for (const auto &var : fam.m_variantes)
                {
                    if (var.m_stock > 0)
                        stockFamilia += var.m_stock;
                }
                if (stockFamilia > 0)
                {
                    stockPorMarca[fam.m_marca.m_name] += stockFamilia;
                }
            }
        }

        std::string htmlMarcas = "<div class='row g-3' id='brand-grid'>";
        if (stockPorMarca.empty())
        {
            htmlMarcas += "<div class='col-12'><div class='alert alert-light text-center'>No hay marcas con stock disponible.</div></div>";
        }
        else
        {
            for (const auto &par : stockPorMarca)
            {
                std::string inicial(1, par.first.front());
                htmlMarcas += R"HTML(
                    <div class='col-6'>
                        <div class='card-glass brand-card' onclick="showBrand(')HTML" +
                              par.first + R"HTML(')">
                            <div class='brand-letter-icon'>)HTML" +
                              inicial + R"HTML(</div>
                            <h5 class='fw-bold text-dark mb-1'>)HTML" +
                              par.first + R"HTML(</h5>
                            <span class='badge bg-light text-secondary border'>)HTML" +
                              std::to_string(par.second) + R"HTML( prendas</span>
                        </div>
                    </div>
                )HTML";
            }
        }
        htmlMarcas += "</div>";

        std::string htmlItems = R"HTML(
            <div id='item-list' style='display: none;'>
                <div class='d-flex justify-content-between align-items-center mb-4 mt-2'>
                    <h5 id='list-title' class='fw-bold mb-0 text-secondary ms-2'>Catálogo</h5>
                    <button id='btn-volver' class='btn btn-light border rounded-pill fw-bold shadow-sm' onclick='showAllBrands()' style='color: var(--mali-primary);'>
                        <i class='fas fa-arrow-left me-2'></i>Volver
                    </button>
                </div>
                <div id="item-list-container"></div>
            </div>
        )HTML";

        html += htmlMarcas + htmlItems + WebTemplates::getFooter("inventario");
        res.set_content(html, "text/html");
    }

    void handleApiInventarioMarca(const httplib::Request &req, httplib::Response &res, Store *tienda)
    {
        std::string marcaReq = req.has_param("marca") ? req.get_param_value("marca") : "";
        bool isEdit = req.has_param("edit") && req.get_param_value("edit") == "true";

        if (marcaReq.empty())
        {
            res.set_content("[]", "application/json");
            return;
        }

        json resultado = json::array();
        {
            std::lock_guard<std::recursive_mutex> lock(mutexTienda);
            for (const auto &fam : tienda->inventory)
            {
                if (fam.m_marca.m_name == marcaReq)
                {
                    bool tieneStock = false;
                    json jFam;
                    jFam["sku"] = fam.m_sku;
                    jFam["nombre"] = fam.m_nombre;
                    jFam["marca"] = fam.m_marca.m_name;
                    jFam["variantes"] = json::array();

                    float precioBase = -1.0f;
                    bool multiplesPrecios = false;

                    for (const auto &var : fam.m_variantes)
                    {
                        if (var.m_stock > 0 || isEdit)
                        {
                            tieneStock = true;
                            json jVar;
                            jVar["talle"] = var.m_talle;
                            jVar["color"] = var.m_color;
                            jVar["stock"] = var.m_stock;
                            jVar["codigos"] = var.m_codigoBarras;

                            float precioVar = fam.obtenerPrecioFinal(var);
                            jVar["precio"] = precioVar;

                            if (precioBase < 0)
                            {
                                precioBase = precioVar;
                            }
                            else if (precioBase != precioVar)
                            {
                                multiplesPrecios = true;
                                if (precioVar < precioBase)
                                {
                                    precioBase = precioVar;
                                }
                            }
                            jFam["variantes"].push_back(jVar);
                        }
                    }
                    if (tieneStock)
                    {
                        jFam["precio"] = precioBase;
                        jFam["multiples_precios"] = multiplesPrecios;
                        resultado.push_back(jFam);
                    }
                }
            }
        }
        res.set_content(resultado.dump(), "application/json");
    }

    void handleApiBuscarArticulo(const httplib::Request &req, httplib::Response &res, Store *tienda)
    {
        std::string query = req.has_param("q") ? req.get_param_value("q") : "";
        std::transform(query.begin(), query.end(), query.begin(), ::tolower);
        bool isEdit = req.has_param("edit") && req.get_param_value("edit") == "true";

        json resultado = json::array();
        if (query.empty())
        {
            res.set_content(resultado.dump(), "application/json");
            return;
        }

        {
            std::lock_guard<std::recursive_mutex> lock(mutexTienda);
            for (const auto &fam : tienda->inventory)
            {
                std::string skuLow = fam.m_sku;
                std::string nomLow = fam.m_nombre;
                std::transform(skuLow.begin(), skuLow.end(), skuLow.begin(), ::tolower);
                std::transform(nomLow.begin(), nomLow.end(), nomLow.begin(), ::tolower);

                bool coincideSKU = (skuLow.find(query) != std::string::npos);
                bool coincideNom = (nomLow.find(query) != std::string::npos);
                bool coincideBarras = false;

                for (const auto &var : fam.m_variantes)
                {
                    if (std::find(var.m_codigoBarras.begin(), var.m_codigoBarras.end(), query) != var.m_codigoBarras.end())
                    {
                        coincideBarras = true;
                        break;
                    }
                }

                if (coincideSKU || coincideNom || coincideBarras)
                {
                    bool tieneStock = false;
                    json jFam;
                    jFam["sku"] = fam.m_sku;
                    jFam["nombre"] = fam.m_nombre;
                    jFam["desc"] = fam.m_nombre;
                    jFam["marca"] = fam.m_marca.m_name;
                    jFam["variantes"] = json::array();

                    float precioBase = -1.0f;
                    bool multiplesPrecios = false;

                    for (const auto &var : fam.m_variantes)
                    {
                        if (var.m_stock > 0 || isEdit)
                        {
                            tieneStock = true;
                            json jVar;
                            jVar["talle"] = var.m_talle;
                            jVar["color"] = var.m_color;
                            jVar["stock"] = var.m_stock;
                            jVar["codigos"] = var.m_codigoBarras;

                            float precioVar = fam.obtenerPrecioFinal(var);
                            jVar["precio"] = precioVar;

                            if (precioBase < 0)
                            {
                                precioBase = precioVar;
                            }
                            else if (precioBase != precioVar)
                            {
                                multiplesPrecios = true;
                                if (precioVar < precioBase)
                                {
                                    precioBase = precioVar;
                                }
                            }

                            jFam["variantes"].push_back(jVar);
                        }
                    }
                    if (tieneStock)
                    {
                        jFam["precio"] = precioBase;
                        jFam["multiples_precios"] = multiplesPrecios;
                        resultado.push_back(jFam);
                    }
                }
            }
        }
        res.set_content(resultado.dump(), "application/json");
    }

    void handleSeguimiento(const httplib::Request &req, httplib::Response &res, Store *tienda)
    {
        std::string html = WebTemplates::getHeadAndNav("Seguimiento");

        std::set<std::string> marcas;
        {
            std::lock_guard<std::recursive_mutex> lock(mutexTienda);
            for (const auto &fam : tienda->inventory)
            {
                if (!fam.m_marca.m_name.empty())
                    marcas.insert(fam.m_marca.m_name);
            }
        }

        std::string options = "<option value=''>1. Elegir marca...</option>";
        for (const auto &m : marcas)
        {
            options += "<option value='" + m + "'>" + m + "</option>";
        }

        html += R"HTML(
            <div class='d-flex justify-content-between align-items-center mb-3'>
                <h2 class='page-title mb-0'><i class='fas fa-route text-primary me-2'></i>Rastreo</h2>
            </div>
            <div class='card-glass p-3 mb-4'>
                <label class='fw-bold mb-2 text-muted'>Filtrar por Marca</label>
                <select id='segMarca' class='form-select form-select-lg mb-3 shadow-sm border-0 bg-light fw-bold text-primary' onchange='cargarSKUsSeguimiento()'>
                    )HTML" +
                options + R"HTML(
                </select>
                
                <label class='fw-bold mb-2 text-muted'>Seleccionar Artículo (SKU)</label>
                <select id='segSku' class='form-select form-select-lg mb-3 shadow-sm border-0 bg-light fw-bold text-secondary' disabled onchange='verHistorialSelect()'>
                    <option value=''>Primero elige una marca...</option>
                </select>
            </div>
            <div id='resultadoRastreo'></div>

            <script>
                function cargarSKUsSeguimiento() {
                    const marca = document.getElementById('segMarca').value;
                    const selectSku = document.getElementById('segSku');
                    const container = document.getElementById('resultadoRastreo');
                    container.innerHTML = ''; 
                    
                    if(!marca) {
                        selectSku.innerHTML = '<option value="">Primero elige una marca...</option>';
                        selectSku.disabled = true;
                        return;
                    }

                    selectSku.disabled = false;
                    selectSku.innerHTML = '<option value="">Cargando artículos...</option>';

                    fetch('/api/inventario_marca?marca=' + encodeURIComponent(marca) + '&edit=true')
                    .then(r => r.json())
                    .then(data => {
                        if(data.length === 0) {
                            selectSku.innerHTML = '<option value="">No hay artículos en esta marca</option>';
                            selectSku.disabled = true;
                            return;
                        }
                        let opts = '<option value="">2. Seleccionar SKU...</option>';
                        data.forEach(fam => {
                            opts += `<option value="${fam.sku || fam.m_sku}">${fam.sku || fam.m_sku}</option>`;
                        });
                        selectSku.innerHTML = opts;
                    });
                }

                function verHistorialSelect() {
                    const sku = document.getElementById('segSku').value;
                    if(!sku) return;
                    
                    const container = document.getElementById('resultadoRastreo');
                    container.innerHTML = '<div class="text-center p-4"><i class="fas fa-spinner fa-spin fa-2x text-primary"></i></div>';
                    
                    fetch('/api/historial_sku?sku=' + encodeURIComponent(sku))
                    .then(r => r.json())
                    .then(data => {
                        if(data.length === 0) {
                            container.innerHTML = '<div class="alert alert-light text-center border shadow-sm">No hay movimientos registrados para este SKU.</div>';
                            return;
                        }
                        let html = '<div class="timeline mt-2">';
                        data.forEach(mov => {
                            let icon = 'fa-box'; let color = 'text-secondary';
                            if(mov.motivo.includes('Venta')) { icon = 'fa-shopping-cart'; color = 'text-success'; }
                            else if(mov.motivo.includes('Remito') || mov.motivo.includes('Entrega')) { icon = 'fa-truck'; color = 'text-warning'; }
                            else if(mov.motivo.includes('Devolución')) { icon = 'fa-undo'; color = 'text-danger'; }
                            else if(mov.motivo.includes('Ingreso') || mov.motivo.includes('Alta')) { icon = 'fa-plus-circle'; color = 'text-primary'; }
                            else if(mov.motivo.includes('Edición') || mov.motivo.includes('Ajuste')) { icon = 'fa-edit'; color = 'text-info'; }

                            let signo = mov.cantidad > 0 ? '+' : '';
                            let cantColor = mov.cantidad > 0 ? 'text-success' : 'text-danger';

                            html += `
                            <div class="card-glass p-3 mb-2 d-flex align-items-center" style="border-left: 4px solid var(--mali-primary);">
                                <div class="me-3 ${color}"><i class="fas ${icon} fs-2"></i></div>
                                <div class="flex-grow-1">
                                    <div class="fw-bold" style="color: var(--mali-secondary);">${mov.motivo}</div>
                                    <small class="text-muted"><i class="far fa-calendar-alt me-1"></i>${mov.fecha} | <i class="far fa-user me-1"></i>${mov.usuario}</small>
                                    <div class="small mt-1 text-secondary">${mov.desc}</div>
                                </div>
                                <div class="text-end ms-2">
                                    <h4 class="mb-0 fw-bold ${cantColor}">${signo}${mov.cantidad}</h4>
                                </div>
                            </div>`;
                        });
                        html += '</div>';
                        container.innerHTML = html;
                    });
                }
            </script>
        )HTML";
        html += WebTemplates::getFooter("seguimiento");
        res.set_content(html, "text/html");
    }

    void handleApiHistorialSku(const httplib::Request &req, httplib::Response &res, Store *tienda)
    {
        std::string skuReq = req.has_param("sku") ? req.get_param_value("sku") : "";
        std::transform(skuReq.begin(), skuReq.end(), skuReq.begin(), ::tolower);

        json resultado = json::array();
        if (skuReq.empty())
        {
            res.set_content(resultado.dump(), "application/json");
            return;
        }

        {
            std::lock_guard<std::recursive_mutex> lock(mutexTienda);
            for (auto it = tienda->movements.rbegin(); it != tienda->movements.rend(); ++it)
            {
                std::string skuMov = it->m_sku;
                std::transform(skuMov.begin(), skuMov.end(), skuMov.begin(), ::tolower);
                if (skuMov == skuReq)
                {
                    json j;
                    j["fecha"] = it->m_fecha;
                    j["usuario"] = it->m_usuario;
                    j["desc"] = it->m_description;
                    j["cantidad"] = it->m_cantidad;
                    j["motivo"] = it->m_motivo;
                    resultado.push_back(j);
                }
            }
        }
        res.set_content(resultado.dump(), "application/json");
    }

    void handleApiEditarInventario(const httplib::Request &req, httplib::Response &res, Store *tienda)
    {
        std::string userEmail = req.get_header_value("Cf-Access-Authenticated-User-Email");
        Rol usuarioRol = WebAuth::obtenerRolPorEmail(userEmail);
        std::string miNombre = WebAuth::obtenerNombreVendedora(userEmail);
        if (miNombre.empty())
            miNombre = "Administrador";

        if (usuarioRol != Rol::Owner && usuarioRol != Rol::Admin)
        {
            res.set_content("{\"status\":\"error\", \"msg\":\"No tenés permisos para editar.\"}", "application/json");
            return;
        }

        try
        {
            json payload = json::parse(req.body);
            std::string sku = payload["sku"].get<std::string>();
            std::string talle = payload["talle"].get<std::string>();
            std::string color = payload["color"].get<std::string>();
            int nuevoStock = payload["stock"].get<int>();

            time_t now = time(0);
            tm *ltm = localtime(&now);
            char buffer[20];
            strftime(buffer, sizeof(buffer), "%d/%m/%Y", ltm);
            std::string fechaHoy(buffer);

            bool modificado = false;
            {
                std::lock_guard<std::recursive_mutex> lock(mutexTienda);
                for (auto &fam : tienda->inventory)
                {
                    if (fam.m_sku == sku)
                    {
                        for (auto &var : fam.m_variantes)
                        {
                            if (var.m_talle == talle && var.m_color == color)
                            {
                                int diff = nuevoStock - var.m_stock;
                                var.m_stock = nuevoStock;

                                if (diff != 0)
                                {
                                    std::string desc = fam.m_nombre + " [" + talle + "/" + color + "]";
                                    Movement mov(fechaHoy, miNombre, sku, desc, diff, "Ajuste Web");
                                    tienda->movements.push_back(mov);
                                }
                                modificado = true;
                                break;
                            }
                        }
                        break;
                    }
                }
            }

            if (modificado)
            {
                tienda->dataVersion++;
                tienda->saveToFileAsync();
                res.set_content("{\"status\":\"ok\"}", "application/json");
            }
            else
            {
                res.set_content("{\"status\":\"error\", \"msg\":\"Error al guardar.\"}", "application/json");
            }
        }
        catch (...)
        {
            res.status = 400;
            res.set_content("{\"status\":\"error\", \"msg\":\"Error de lectura.\"}", "application/json");
        }
    }

    void handleApiAgregarVariante(const httplib::Request &req, httplib::Response &res, Store *tienda)
    {
        std::string userEmail = req.get_header_value("Cf-Access-Authenticated-User-Email");
        Rol usuarioRol = WebAuth::obtenerRolPorEmail(userEmail);
        std::string miNombre = WebAuth::obtenerNombreVendedora(userEmail);
        if (miNombre.empty())
            miNombre = "Administrador";

        if (usuarioRol != Rol::Owner && usuarioRol != Rol::Admin)
        {
            res.set_content("{\"status\":\"error\", \"msg\":\"No tenés permisos para agregar.\"}", "application/json");
            return;
        }

        try
        {
            json payload = json::parse(req.body);
            std::string sku = payload["sku"].get<std::string>();
            std::string talle = payload["talle"].get<std::string>();
            std::string color = payload["color"].get<std::string>();
            int cantidad = payload["cantidad"].get<int>();

            time_t now = time(0);
            tm *ltm = localtime(&now);
            char buffer[20];
            strftime(buffer, sizeof(buffer), "%d/%m/%Y", ltm);
            std::string fechaHoy(buffer);

            bool agregado = false;
            std::string mensajeError = "Familia no encontrada.";

            {
                std::lock_guard<std::recursive_mutex> lock(mutexTienda);
                for (auto &fam : tienda->inventory)
                {
                    if (fam.m_sku == sku)
                    {
                        bool existe = false;
                        for (auto &var : fam.m_variantes)
                        {
                            if (var.m_talle == talle && var.m_color == color)
                            {
                                existe = true;
                                break;
                            }
                        }

                        if (!existe)
                        {
                            fam.m_variantes.push_back(Variante(std::vector<std::string>(), talle, color, cantidad, 0.0f));

                            if (cantidad > 0)
                            {
                                std::string desc = fam.m_nombre + " [" + talle + "/" + color + "]";
                                Movement mov(fechaHoy, miNombre, sku, desc, cantidad, "Alta Variante Web");
                                tienda->movements.push_back(mov);
                            }
                            agregado = true;
                        }
                        else
                        {
                            mensajeError = "La variante ya existe.";
                        }
                        break;
                    }
                }
            }

            if (agregado)
            {
                tienda->dataVersion++;
                tienda->saveToFileAsync();
                res.set_content("{\"status\":\"ok\"}", "application/json");
            }
            else
            {
                res.set_content("{\"status\":\"error\", \"msg\":\"" + mensajeError + "\"}", "application/json");
            }
        }
        catch (...)
        {
            res.status = 400;
            res.set_content("{\"status\":\"error\", \"msg\":\"Error de lectura de datos.\"}", "application/json");
        }
    }

    void handleApiCatalogoCompleto(const httplib::Request &req, httplib::Response &res, Store *tienda)
    {
        json resultado = json::array();
        std::lock_guard<std::recursive_mutex> lock(mutexTienda);
        for (const auto &fam : tienda->inventory)
        {
            json jFam;
            jFam["sku"] = fam.m_sku;
            jFam["nombre"] = fam.m_nombre;
            jFam["marca"] = fam.m_marca.m_name;

            std::string cat = "";
            if (!fam.m_categoria.m_name.empty())
            {
                cat = fam.m_categoria.m_name;
            }
            jFam["categoria"] = cat;

            jFam["variantes"] = json::array();
            for (const auto &var : fam.m_variantes)
            {
                json jVar;
                jVar["talle"] = var.m_talle;
                jVar["color"] = var.m_color;
                jVar["stock"] = var.m_stock;
                jVar["precio"] = fam.obtenerPrecioFinal(var);
                jFam["variantes"].push_back(jVar);
            }
            resultado.push_back(jFam);
        }
        res.set_content(resultado.dump(), "application/json");
    }
}