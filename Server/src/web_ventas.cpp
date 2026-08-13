#include "web_routes.hpp"
#include "web_utils.hpp"
#include "web_templates.hpp"
#include "web_auth.hpp"
#include <mutex>
#include <map>

extern std::recursive_mutex mutexTienda;

namespace WebRoutes
{

    void handleCrearVenta(const httplib::Request &req, httplib::Response &res, Store *tienda)
    {
        std::string userEmail = req.get_header_value("Cf-Access-Authenticated-User-Email");
        std::string miNombre = WebAuth::obtenerNombreVendedora(userEmail);
        if (miNombre.empty())
            miNombre = "Administrador";

        std::string jsInventory = "{";
        {
            std::lock_guard<std::recursive_mutex> lock(mutexTienda);
            std::map<std::string, std::string> brandMap;

            for (const auto &fam : tienda->inventory)
            {
                bool hasStock = false;
                std::string varJson = "[";
                for (const auto &var : fam.m_variantes)
                {
                    if (var.m_stock > 0)
                    {
                        hasStock = true;
                        varJson += "{\"talle\":\"" + var.m_talle + "\",\"color\":\"" + var.m_color + "\",\"stock\":" + std::to_string(var.m_stock) + ",\"precio\":" + std::to_string(fam.obtenerPrecioFinal(var)) + "},";
                    }
                }

                if (hasStock)
                {
                    varJson.pop_back();
                    varJson += "]";

                    std::string safeName = fam.m_nombre;
                    size_t pos = 0;
                    while ((pos = safeName.find('"', pos)) != std::string::npos)
                    {
                        safeName.replace(pos, 1, "\\\"");
                        pos += 2;
                    }

                    std::string famJson = "{\"sku\":\"" + fam.m_sku + "\",\"desc\":\"" + safeName + "\",\"variantes\":" + varJson + "}";
                    brandMap[fam.m_marca.m_name] += famJson + ",";
                }
            }

            for (auto &pair : brandMap)
            {
                if (!pair.second.empty())
                    pair.second.pop_back();
                jsInventory += "\"" + pair.first + "\":[" + pair.second + "],";
            }
            if (jsInventory.length() > 1)
                jsInventory.pop_back();
            jsInventory += "}";
        }

        std::string html = WebTemplates::getHeadAndNav("Nueva Venta");
        html += R"(
            <style>
                @media (max-width: 768px) {
                    .ticket-col {
                        position: fixed; bottom: 15px; left: 10px; right: 10px; width: calc(100% - 20px); z-index: 1010; margin-bottom: 0 !important;
                    }
                    .ticket-col .card-glass {
                        border: 2px solid #28a745 !important; border-radius: 20px !important; padding: 12px !important;
                        box-shadow: 0 -8px 25px rgba(0,0,0,0.2) !important; background: var(--mali-card-bg) !important;
                    }
                    .mobile-ticket-header { display: flex !important; cursor: pointer; user-select: none; }
                    #cartItems { max-height: 180px; overflow-y: auto; display: none; margin-top: 10px; border-top: 1px dashed var(--mali-border); }
                    .ticket-expanded #cartItems { display: block !important; }
                    #btnConfirmar { display: none !important; }
                    .ticket-expanded #btnConfirmar { display: block !important; margin-top: 12px !important; font-size: 0.95rem !important; }
                }

                .sku-accordion { border: 1px solid var(--mali-border); border-radius: 16px; overflow: hidden; background: var(--mali-card-bg); margin-bottom: 8px;}
                .sku-accordion .accordion-button { padding: 16px; font-weight: 800; font-size: 1.15rem; color: var(--mali-primary); }
                .sku-accordion .accordion-button:not(.collapsed) { background: rgba(92, 51, 190, 0.08); box-shadow: none; color: var(--mali-secondary); }
                .variant-row { border-top: 1px solid rgba(0,0,0,0.05); padding: 12px 16px; display: flex; justify-content: space-between; align-items: center; }
                .variant-row:first-child { border-top: none; }
                .sku-active { border-left: 5px solid #28a745; background: rgba(40, 167, 69, 0.05); }
            </style>

            <div class='d-flex justify-content-between align-items-center mb-3'>
                <h2 class='page-title mb-0'><i class='fas fa-shopping-cart text-success me-2'></i>Nueva Venta</h2>
                <div>
                    <span class='badge bg-light text-dark border px-3 py-2 d-none d-md-inline-block me-2'>)" +
                miNombre + R"(</span>
                    <a href='/historial_ventas' class='btn btn-sm btn-outline-success rounded-pill fw-bold px-3 py-1 shadow-sm'><i class='fas fa-history me-1'></i>Historial</a>
                </div>
            </div>

            <div id='toastBox' class='position-fixed bottom-0 start-50 translate-middle-x p-3' style='z-index: 9999; display:none; transition: opacity 0.3s ease;'>
                <div id='toastMsg' class='badge rounded-pill fs-6 px-4 py-2 shadow-lg' style='background: var(--mali-primary); color: white; font-weight: 600;'></div>
            </div>

            <div class='row g-3'>
                <!-- PANEL IZQUIERDO (BUSCADOR PRINCIPAL) -->
                <div class='col-12 col-lg-6'>
                    <div class='card-glass p-3 mb-2'>
                        <div class='input-group shadow-sm' style='border-radius: 16px; overflow: hidden; border: 2px solid var(--mali-primary);'>
                            <span class='input-group-text bg-transparent border-0'><i class='fas fa-barcode' style='color: var(--mali-primary);'></i></span>
                            <input type='text' inputmode='numeric' pattern='[0-9]*' id='skuFilter' class='form-control border-0 bg-transparent shadow-none fs-5 fw-bold text-primary' placeholder='SKU' onkeyup='manejarBuscador(event)'>
                            <select id='brandSelect' class='form-select border-0 bg-light text-muted' style='max-width: 110px; font-weight: 600; cursor: pointer; box-shadow: none; border-left: 1px solid rgba(0,0,0,0.1) !important;' onchange='seleccionarMarca(this.value)'>
                                <option value=''>Marcas</option>
                            </select>
                        </div>
                    </div>

                    <div id='skuListContainer' style='padding-bottom: 120px;'>
                        <div class='accordion' id='accordionSKUs'>
                            <div class='text-center p-4 text-muted fw-bold'>
                                <i class='fas fa-search mb-2 fs-1 opacity-25 d-block'></i>Buscá un SKU
                            </div>
                        </div>
                    </div>
                </div>

                <!-- PANEL DERECHO (TICKET) -->
                <div class='col-12 col-lg-6 ticket-col' id='ticketColumn'>
                    <div class='card-glass p-3' style='border-top: 4px solid #28a745;'>
                        
                        <div class='mobile-ticket-header justify-content-between align-items-center d-none pb-1' onclick='toggleMobileTicket()'>
                            <span class='fw-bold text-success'><i class='fas fa-shopping-bag me-2'></i> Mi Ticket (<b id='mobileQty'>0</b>)</span>
                            <span class='fw-bold text-dark me-auto ms-3' id='mobileTotal'>$0</span>
                            <i class='fas fa-chevron-up text-muted transition-all' id='ticketChevron'></i>
                        </div>

                        <h6 class='fw-bold text-success mb-3 d-none d-lg-block'>Ticket de Venta</h6>
                        
                        <div id='cartItems' class='list-group mb-3'>
                            <div class='text-center text-muted p-4 small' id='emptyCartMsg'>El ticket está vacío.</div>
                        </div>
                        
                        <div class='d-flex justify-content-between align-items-center mt-3 pt-3 border-top d-none d-lg-flex'>
                            <span class='text-muted'>Total Prendas: <b id='totalQty'>0</b></span>
                            <h4 class='fw-bold text-success mb-0' id='totalPrice'>$0</h4>
                        </div>
                        
                        <div class="d-flex gap-2 mt-3">
                            <button id='btnLimpiar' onclick='limpiarVentaGuardada()' class='btn btn-light border rounded-pill shadow-sm px-3 d-none d-lg-block' title='Vaciar Ticket'><i class='fas fa-trash text-danger'></i></button>
                            <button id='btnConfirmar' onclick='confirmarVenta()' class='btn btn-success w-100 rounded-pill fw-bold shadow-sm py-2' disabled>
                                CONFIRMAR VENTA
                            </button>
                        </div>
                    </div>
                </div>
            </div>

            <script>
                const db = )" +
                jsInventory + R"(;
                let carrito = [];
                let marcaActual = '';
                let skuExpandido = '';

                
                function manejarBuscador(e) {
                    if (e.key === 'Enter') {
                        e.target.blur(); 
                    }
                    filtrarSKU(e.target.value);
                }

                
                window.onload = () => {
                    const select = document.getElementById('brandSelect');
                    Object.keys(db).sort().forEach(marca => {
                        select.add(new Option(marca, marca));
                    });

                    let savedCart = localStorage.getItem('mali_draft_venta');
                    if (savedCart) {
                        try {
                            carrito = JSON.parse(savedCart);
                            if (carrito.length > 0) {
                                showToast("<i class='fas fa-undo me-2'></i> Ticket recuperado");
                                renderCarrito();
                            }
                        } catch(e) {}
                    }
                };

                function guardarProgresoLocal() {
                    localStorage.setItem('mali_draft_venta', JSON.stringify(carrito));
                }

                function limpiarVentaGuardada() {
                    if(confirm('¿Vaciar todo el ticket?')) {
                        carrito = [];
                        localStorage.removeItem('mali_draft_venta');
                        renderCarrito();
                        filtrarSKU(document.getElementById('skuFilter').value);
                    }
                }

                function showToast(msg, isError = false) {
                    const box = document.getElementById('toastBox');
                    const badge = document.getElementById('toastMsg');
                    badge.innerHTML = msg;
                    badge.style.background = isError ? '#dc3545' : '#28a745';
                    box.style.display = 'block'; box.style.opacity = '1';
                    setTimeout(() => { box.style.opacity = '0'; setTimeout(() => box.style.display = 'none', 300); }, 2000);
                }

                function toggleMobileTicket() {
                    const col = document.getElementById('ticketColumn');
                    const icon = document.getElementById('ticketChevron');
                    col.classList.toggle('ticket-expanded');
                    if(col.classList.contains('ticket-expanded')) icon.classList.replace('fa-chevron-up', 'fa-chevron-down');
                    else icon.classList.replace('fa-chevron-down', 'fa-chevron-up');
                }

                function seleccionarMarca(marca) {
                    marcaActual = marca;
                    filtrarSKU(document.getElementById('skuFilter').value);
                }

                
                let timeoutBuscador = null;
                function filtrarSKU(q) {
                    clearTimeout(timeoutBuscador);
                    timeoutBuscador = setTimeout(() => {
                        q = q.toLowerCase().trim();
                        let filtrados = [];
                        
                        if (q === '' && marcaActual === '') {
                            document.getElementById('accordionSKUs').innerHTML = "<div class='text-center p-4 text-muted fw-bold'><i class='fas fa-search mb-2 fs-1 opacity-25 d-block'></i>Buscá un SKU</div>";
                            return;
                        }

                        if (q === '' && marcaActual !== '') {
                            filtrados = db[marcaActual].map(fam => ({...fam, marcaReal: marcaActual}));
                        } 
                        else if (q !== '') {
                            if (marcaActual !== '') {
                                filtrados = db[marcaActual]
                                    .filter(fam => fam.sku.toLowerCase().includes(q))
                                    .map(fam => ({...fam, marcaReal: marcaActual}));
                            } else {
                                Object.keys(db).forEach(marca => {
                                    let matches = db[marca].filter(fam => fam.sku.toLowerCase().includes(q));
                                    matches.forEach(m => filtrados.push({...m, marcaReal: marca}));
                                });
                            }
                        }
                        renderSKUs(filtrados);
                    }, 200);
                }

                function renderSKUs(familias) {
                    const container = document.getElementById('accordionSKUs');
                    if(familias.length === 0) {
                        container.innerHTML = "<div class='alert alert-light text-center border mt-3'>No se encontraron SKUs.</div>";
                        return;
                    }

                    let html = "";
                    familias.forEach((fam, idx) => {
                        let itemEnCarrito = carrito.some(c => c.sku === fam.sku);
                        let claseActiva = itemEnCarrito ? "sku-active" : "";
                        let badgeActivo = itemEnCarrito ? "<span class='badge bg-success ms-2' style='font-size:0.6rem;'>EN TICKET</span>" : "";
                        let badgeMarca = `<span class='badge bg-light text-secondary border ms-2' style='font-size:0.65rem;'>${fam.marcaReal}</span>`;

                        let varHtml = "";
                        fam.variantes.forEach(v => {
                            let itemData = JSON.stringify({
                                sku: fam.sku, marca: fam.marcaReal, desc: fam.desc, talle: v.talle, color: v.color, precio: v.precio
                            });
                            
                            let cantEnCarrito = 0;
                            let hallado = carrito.find(x => x.sku === fam.sku && x.talle === v.talle && x.color === v.color);
                            if(hallado) cantEnCarrito = hallado.cantidad;

                            let btnHtml = cantEnCarrito >= v.stock 
                                ? `<button class='btn btn-sm btn-secondary rounded-pill px-3 py-1' disabled>Agotado</button>`
                                : `<button onclick='agregarAlCarrito(${itemData}, ${v.stock})' class='btn btn-sm btn-outline-success rounded-pill px-3 py-1 fw-bold'>+ Añadir</button>`;
                            
                            let indicatorHtml = cantEnCarrito > 0 
                                ? `<span class='badge bg-primary rounded-pill me-2'>${cantEnCarrito} carg.</span>` 
                                : ``;

                            varHtml += `
                                <div class='variant-row'>
                                    <div>
                                        <div class='mb-1'><small style='color: var(--mali-text);'>Talle: <b>${v.talle}</b> | Color: <b>${v.color}</b></small></div>
                                        <span class='badge' style='background: rgba(40, 167, 69, 0.1); color: #28a745;'>Stock Disp: ${v.stock - cantEnCarrito}</span>
                                    </div>
                                    <div class='text-end'>
                                        ${indicatorHtml}
                                        <span class='badge bg-warning text-dark me-2 fs-6'>$${v.precio.toLocaleString('es-AR')}</span>
                                        <div class='mt-2'>${btnHtml}</div>
                                    </div>
                                </div>
                            `;
                        });

                        let isOpen = (fam.sku === skuExpandido);
                        let btnClass = isOpen ? "" : "collapsed";
                        let colClass = isOpen ? "show" : "";

                        html += `
                        <div class="accordion-item sku-accordion ${claseActiva}">
                            <h2 class="accordion-header" id="heading-${idx}">
                                <button class="accordion-button ${btnClass}" type="button" data-bs-toggle="collapse" data-bs-target="#col-${idx}" onclick="skuExpandido = (skuExpandido === '${fam.sku}') ? '' : '${fam.sku}'">
                                    ${fam.sku} ${badgeMarca} ${badgeActivo}
                                </button>
                            </h2>
                            <div id="col-${idx}" class="accordion-collapse collapse ${colClass}" data-bs-parent="#accordionSKUs">
                                <div class="accordion-body p-0 pb-1">
                                    <div class='px-3 pt-2 pb-1'><small class='text-muted'>${fam.desc}</small></div>
                                    ${varHtml}
                                </div>
                            </div>
                        </div>`;
                    });
                    container.innerHTML = html;
                }

                function agregarAlCarrito(item, stockMaximo) {
                    let existente = carrito.find(x => x.sku === item.sku && x.talle === item.talle && x.color === item.color);
                    if (existente) {
                        if (existente.cantidad < stockMaximo) {
                            existente.cantidad++;
                            showToast("<i class='fas fa-check-circle me-2'></i> Sumaste otra unidad");
                        } else {
                            showToast("<i class='fas fa-exclamation-triangle me-2'></i> Stock agotado", true);
                        }
                    } else {
                        item.cantidad = 1; carrito.push(item);
                        showToast("<i class='fas fa-check-circle me-2'></i> Artículo añadido");
                    }
                    
                    skuExpandido = item.sku; 
                    guardarProgresoLocal(); 
                    renderCarrito();
                    filtrarSKU(document.getElementById('skuFilter').value);
                }

                function eliminarDelCarrito(index) {
                    carrito.splice(index, 1);
                    guardarProgresoLocal(); 
                    renderCarrito();
                    filtrarSKU(document.getElementById('skuFilter').value);
                }

                function renderCarrito() {
                    const container = document.getElementById('cartItems');
                    let totalItems = 0; let totalPrice = 0;

                    if (carrito.length === 0) {
                        container.innerHTML = "<div class='text-center text-muted p-4 small' id='emptyCartMsg'>El ticket está vacío.</div>";
                        document.getElementById('btnConfirmar').disabled = true;
                    } else {
                        let html = "";
                        carrito.forEach((item, index) => {
                            totalItems += item.cantidad; totalPrice += (item.precio * item.cantidad);
                            let marcaBadge = item.marca ? `<span class='badge align-middle ms-1' style='background: rgba(92, 51, 190, 0.1); color: var(--mali-primary); border: 1px solid rgba(92, 51, 190, 0.2); font-size: 0.65rem;'>${item.marca}</span>` : "";
                            
                            html += `
                                <div class='list-group-item d-flex justify-content-between align-items-center border-start-0 border-end-0 px-0 bg-transparent py-2'>
                                    <div>
                                        <div class='fw-bold' style='color: var(--mali-text); font-size: 0.9rem;'>${item.sku} ${marcaBadge}</div>
                                        <small class='text-muted'>${item.talle} / ${item.color} (${item.cantidad} un.)</small>
                                    </div>
                                    <div class='text-end d-flex align-items-center'>
                                        <span class='fw-bold me-2' style='color: var(--mali-text);'>$${(item.precio * item.cantidad).toLocaleString('es-AR')}</span>
                                        <button onclick='eliminarDelCarrito(${index})' class='btn btn-link text-danger p-0'><i class='fas fa-times-circle fs-5'></i></button>
                                    </div>
                                </div>
                            `;
                        });
                        container.innerHTML = html;
                        document.getElementById('btnConfirmar').disabled = false;
                    }
                    
                    const formatPrice = "$" + totalPrice.toLocaleString('es-AR');
                    document.getElementById('totalQty').innerText = totalItems;
                    document.getElementById('totalPrice').innerText = formatPrice;
                    document.getElementById('mobileQty').innerText = totalItems;
                    document.getElementById('mobileTotal').innerText = formatPrice;
                }

                function confirmarVenta() {
                    if (carrito.length === 0) return;
                    const btn = document.getElementById('btnConfirmar');
                    btn.innerHTML = "<i class='fas fa-spinner fa-spin'></i> Procesando...";
                    btn.disabled = true;

                    fetch('/api/ventas', {
                        method: 'POST',
                        headers: {'Content-Type': 'application/json'},
                        body: JSON.stringify({ carrito: carrito })
                    })
                    .then(res => res.json())
                    .then(data => {
                        if (data.status === 'ok') {
                            localStorage.removeItem('mali_draft_venta'); 
                            window.location.href = '/historial_ventas';
                        } else { showToast('Error al guardar', true); btn.innerHTML = "CONFIRMAR VENTA"; btn.disabled = false; }
                    }).catch(e => { showToast('Error de conexión', true); btn.disabled = false; });
                }
                
                document.addEventListener("DOMContentLoaded", () => {
                    if (localStorage.getItem('mali_export_trigger') === 'true') {
                        localStorage.removeItem('mali_export_trigger');
                        let scanned = JSON.parse(localStorage.getItem('mali_scanned_cart')) || [];
                        if (scanned.length > 0) {
                            scanned.forEach(item => {
                                let existente = carrito.find(x => x.sku === item.sku && x.talle === item.talle && x.color === item.color);
                                if (existente) {
                                    if (existente.cantidad + item.cantidad <= item.stock) existente.cantidad += item.cantidad;
                                } else { carrito.push(item); }
                            });
                            guardarProgresoLocal(); 
                            renderCarrito();
                            showToast("<i class='fas fa-magic me-2'></i> Artículos importados del escáner");
                            localStorage.removeItem('mali_scanned_cart'); 
                        }
                    }
                });
            </script>
        )";
        html += WebTemplates::getFooter("ventas");
        res.set_content(html, "text/html");
    }

    void handleApiGetVentas(const httplib::Request &req, httplib::Response &res, Store *tienda)
    {
        int offset = req.has_param("offset") ? std::stoi(req.get_param_value("offset")) : 0;
        int limit = req.has_param("limit") ? std::stoi(req.get_param_value("limit")) : 20;
        std::string query = req.has_param("q") ? req.get_param_value("q") : "";
        std::transform(query.begin(), query.end(), query.begin(), ::tolower);

        std::string userEmail = req.get_header_value("Cf-Access-Authenticated-User-Email");
        Rol usuarioRol = WebAuth::obtenerRolPorEmail(userEmail);
        std::string miNombre = WebAuth::obtenerNombreVendedora(userEmail);

        json resultado = json::array();
        {
            std::lock_guard<std::recursive_mutex> lock(mutexTienda);

            std::map<std::string, std::string> mapMarcas;
            for (const auto &fam : tienda->inventory)
            {
                mapMarcas[fam.m_sku] = fam.m_marca.m_name;
            }

            int matchesEncontrados = 0;
            int matchesAgregados = 0;

            for (auto it = tienda->sales.rbegin(); it != tienda->sales.rend(); ++it)
            {
                const auto &venta = *it;
                if (usuarioRol == Rol::Vendedora && venta.m_cliente != miNombre)
                    continue;

                std::string cliente = venta.m_cliente.empty() ? "Consumidor Final" : venta.m_cliente;
                std::string clienteLow = cliente;
                std::transform(clienteLow.begin(), clienteLow.end(), clienteLow.begin(), ::tolower);

                if (!query.empty() && clienteLow.find(query) == std::string::npos)
                {
                    continue;
                }

                if (matchesEncontrados < offset)
                {
                    matchesEncontrados++;
                    continue;
                }

                if (matchesAgregados >= limit)
                    break;

                json jVenta;
                jVenta["cliente"] = cliente;
                jVenta["totalAbonado"] = venta.m_totalAbonado;
                jVenta["fecha"] = venta.m_fecha;

                json jItems = json::array();
                int prendasTotales = 0;
                for (const auto &item : venta.m_items)
                {
                    prendasTotales += item.m_quantity;
                    json jItem;
                    jItem["sku"] = item.m_sku;
                    jItem["marca"] = mapMarcas.count(item.m_sku) ? mapMarcas[item.m_sku] : "Genérica";
                    jItem["description"] = item.m_description;
                    jItem["size"] = item.m_size;
                    jItem["color"] = item.m_color;
                    jItem["price"] = item.m_price;
                    jItem["quantity"] = item.m_quantity;
                    jItems.push_back(jItem);
                }
                jVenta["items"] = jItems;
                jVenta["prendasTotales"] = prendasTotales;

                resultado.push_back(jVenta);
                matchesAgregados++;
                matchesEncontrados++;
            }
        }
        res.set_content(resultado.dump(), "application/json");
    }

    void handleVentas(const httplib::Request &req, httplib::Response &res, Store *tienda)
    {
        std::string html = WebTemplates::getHeadAndNav("Ventas");
        html += "<h2 class='page-title mb-4'>Historial de Ventas</h2>";

        html += "<div class='mb-4'><input type='text' id='searchBox' class='form-control text-center' placeholder='🔍 Buscar cliente... (Completo en Servidor)'></div>";

        html += "<div class='accordion' id='accordionVentas'></div>";

        html += R"(
            <div id="loading-spinner" class="text-center py-4">
                <i class="fas fa-spinner fa-spin fa-2x" style="color: var(--mali-primary);"></i>
                <p class="text-muted mt-2 small fw-bold">Cargando ventas...</p>
            </div>
        )";

        html += R"(
            <script>
                let currentOffset = 0;
                const limit = 20;
                let isLoading = false;
                let hasMoreData = true;
                let searchQuery = '';
                let timeoutBuscadorVentas = null;

                async function cargarMasVentas(reset = false) {
                    if (isLoading) return;
                    if (reset) {
                        currentOffset = 0;
                        hasMoreData = true;
                        document.getElementById('accordionVentas').innerHTML = '';
                    }
                    if (!hasMoreData) return;

                    isLoading = true;
                    document.getElementById('loading-spinner').style.display = 'block';

                    try {
                        const url = `/api/get_ventas?offset=${currentOffset}&limit=${limit}&q=${encodeURIComponent(searchQuery)}`;
                        const res = await fetch(url);
                        const data = await res.json();

                        if (data.length < limit) {
                            hasMoreData = false;
                            document.getElementById('loading-spinner').style.display = 'none';
                        }

                        if (currentOffset === 0 && data.length === 0) {
                            document.getElementById('accordionVentas').innerHTML = "<div class='alert alert-light text-center'>No se encontraron registros.</div>";
                        }

                        renderizarBloqueVentas(data);
                        currentOffset += data.length;
                    } catch (e) {
                        console.error("Error al paginar:", e);
                    } finally {
                        isLoading = false;
                        if(!hasMoreData && currentOffset > 0) {
                            document.getElementById('loading-spinner').style.display = 'none';
                        }
                    }
                }

                function renderizarBloqueVentas(ventas) {
                    const container = document.getElementById('accordionVentas');
                    let html = '';
                    
                    ventas.forEach((venta, i) => {
                        let idxStr = 'v_' + (currentOffset + i);
                        let cliente = venta.cliente;
                        
                        let itemsHtml = '';
                        let textWa = "Venta a nombre de: *" + cliente + "* 🌸\n📅 *Fecha:* " + venta.fecha + "\n💰 *Total Abonado:* $" + venta.totalAbonado.toLocaleString('es-AR') + "\n\n*Detalle:*\n";

                        venta.items.forEach(item => {
                            itemsHtml += `
                                <li class='list-group-item px-0 d-flex justify-content-between align-items-center border-0 py-2'>
                                    <div style='width: 70%;'>
                                        <span class='d-block fw-bold' style='color: var(--mali-primary); font-size:1rem;'>${item.sku} 
                                            <span class='badge align-middle ms-1' style='background: rgba(92, 51, 190, 0.1); color: var(--mali-primary); border: 1px solid rgba(92, 51, 190, 0.2); font-size: 0.65rem;'>${item.marca}</span>
                                        </span>
                                        <small class='text-muted d-block mt-1 mb-2'>${item.description}</small>
                                        <span class='badge bg-light text-dark border'>Talle: ${item.size} | Color: ${item.color}</span>
                                    </div>
                                    <div class='text-end'>
                                        <span class='d-block text-dark fw-bold'>$${item.price.toLocaleString('es-AR')}</span>
                                        <small class='text-muted'>Cant: ${item.quantity}</small>
                                    </div>
                                </li>
                            `;
                            textWa += "- *" + item.sku + "* (" + item.marca + ")\n  _" + item.description + "_\n  " + item.quantity + " un. x $" + item.price.toLocaleString('es-AR') + "\n\n";
                        });

                        textWa += "👗 *Total de prendas:* " + venta.prendasTotales + "\n\n¡Gracias por tu compra! ✨";
                        
                        let encodedWa = encodeURIComponent(textWa);

                        html += `
                            <div class='card-glass mb-3'>
                                <div class='accordion-item'>
                                    <h2 class='accordion-header' id='headingV${idxStr}'>
                                        <button class='accordion-button collapsed px-3 py-3' type='button' data-bs-toggle='collapse' data-bs-target='#collapseV${idxStr}'>
                                            <div class='d-flex flex-column w-100 me-2'>
                                                <div class='d-flex justify-content-between align-items-center mb-1'>
                                                    <span class='fw-bold text-dark'><i class='fas fa-user-circle text-secondary me-2'></i>${cliente}</span>
                                                    <span class='badge-price rounded-pill'>$${venta.totalAbonado.toLocaleString('es-AR')}</span>
                                                </div>
                                                <div class='d-flex justify-content-between align-items-center'>
                                                    <small class='text-muted'>${venta.fecha}</small>
                                                    <small class='text-muted'>${venta.items.length} art.</small>
                                                </div>
                                            </div>
                                        </button>
                                    </h2>
                                    <div id='collapseV${idxStr}' class='accordion-collapse collapse' data-bs-parent='#accordionVentas'>
                                        <div class='accordion-body bg-white pt-2'>
                                            <ul class='list-group list-group-flush'>
                                                ${itemsHtml}
                                            </ul>
                                            <div class='mt-4 pt-3 border-top'>
                                                <button onclick="window.location.href='https:
                                                    <i class='fab fa-whatsapp me-2 fs-5 align-middle'></i> Compartir Ticket
                                                </button>
                                            </div>
                                        </div>
                                    </div>
                                </div>
                            </div>
                        `;
                    });
                    container.insertAdjacentHTML('beforeend', html);
                }

                const observer = new IntersectionObserver((entries) => {
                    if (entries[0].isIntersecting && !isLoading && hasMoreData) {
                        cargarMasVentas();
                    }
                }, { rootMargin: '150px' });

                document.addEventListener('DOMContentLoaded', () => {
                    observer.observe(document.getElementById('loading-spinner'));
                    
                    cargarMasVentas(true);
                    
                    const sb = document.getElementById('searchBox');
                    if (sb) {
                        sb.addEventListener('keyup', () => {
                            clearTimeout(timeoutBuscadorVentas);
                            timeoutBuscadorVentas = setTimeout(() => {
                                searchQuery = sb.value.trim();
                                cargarMasVentas(true);
                            }, 350);
                        });
                    }
                });
            </script>
        )";

        html += WebTemplates::getFooter("ventas");
        res.set_content(html, "text/html");
    }
}