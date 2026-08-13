#include "web_routes.hpp"
#include "web_utils.hpp"
#include "web_templates.hpp"
#include "web_auth.hpp"
#include <mutex>
#include <map>

extern std::recursive_mutex mutexTienda;

namespace WebRoutes
{

    void handleCrearRemito(const httplib::Request &req, httplib::Response &res, Store *tienda)
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

        std::string html = WebTemplates::getHeadAndNav("Nuevo Remito");
        html += R"(
            <style>
                @media (max-width: 768px) {
                    .ticket-col {
                        position: fixed; bottom: 15px; left: 10px; right: 10px; width: calc(100% - 20px); z-index: 1010; margin-bottom: 0 !important;
                    }
                    .ticket-col .card-glass {
                        border: 2px solid #c95c6f !important; border-radius: 20px !important; padding: 12px !important;
                        box-shadow: 0 -8px 25px rgba(0,0,0,0.2) !important; background: var(--mali-card-bg) !important;
                    }
                    .mobile-ticket-header { display: flex !important; cursor: pointer; user-select: none; }
                    #cartItems { max-height: 160px; overflow-y: auto; display: none; margin-top: 10px; border-top: 1px dashed var(--mali-border); }
                    .ticket-expanded #cartItems { display: block !important; }
                    
                    #btnConfirmar { display: none !important; }
                    .ticket-expanded #btnConfirmar { display: block !important; margin-top: 12px !important; font-size: 0.95rem !important; }
                }

                .sku-accordion { border: 1px solid var(--mali-border); border-radius: 16px; overflow: hidden; background: var(--mali-card-bg); margin-bottom: 8px; transform: translateZ(0); will-change: transform;}
                .sku-accordion .accordion-button { padding: 16px; font-weight: 800; font-size: 1.15rem; color: #c95c6f; }
                .sku-accordion .accordion-button:not(.collapsed) { background: rgba(201, 92, 111, 0.08); box-shadow: none; color: var(--mali-secondary); }
                .variant-row { border-top: 1px solid rgba(0,0,0,0.05); padding: 12px 16px; display: flex; justify-content: space-between; align-items: center; }
                .variant-row:first-child { border-top: none; }
                .sku-active { border-left: 5px solid #c95c6f; background: rgba(201, 92, 111, 0.05); }
            </style>

            <div class='d-flex justify-content-between align-items-center mb-3'>
                <h2 class='page-title mb-0'><i class='fas fa-box-open me-2' style='color: #c95c6f;'></i>Nuevo Remito</h2>
                <div>
                    <span class='badge bg-light text-dark border px-3 py-2 d-none d-md-inline-block me-2'>)" +
                miNombre + R"(</span>
                    <a href='/historial_liquidaciones' class='btn btn-sm rounded-pill fw-bold px-3 py-1 shadow-sm' style='border: 1px solid #c95c6f; color: #c95c6f;'><i class='fas fa-history me-1'></i>Historial</a>
                </div>
            </div>

            <div id='toastBox' class='position-fixed bottom-0 start-50 translate-middle-x p-3' style='z-index: 9999; display:none; transition: opacity 0.3s ease;'>
                <div id='toastMsg' class='badge rounded-pill fs-6 px-4 py-2 shadow-lg' style='background: #c95c6f; color: white; font-weight: 600;'></div>
            </div>

            <!-- BANDA COMPACTA REVENDEDORA -->
            <div class='card-glass px-3 py-2 mb-3 d-flex justify-content-between align-items-center shadow-sm' style='cursor: pointer; border-left: 4px solid #c95c6f;' onclick='abrirModalRevendedora()'>
                <div>
                    <small class='text-muted fw-bold d-block' style='font-size: 0.7rem; letter-spacing: 0.5px;'><i class='fas fa-user-tag me-1'></i>REVENDEDORA</small>
                    <span id='revendedoraDisplay' class='fw-bold fs-6'>Tocar para asignar...</span>
                </div>
                <i class='fas fa-pen text-muted opacity-50'></i>
            </div>

            <!-- MODAL FLOTANTE PARA INGRESAR EL NOMBRE -->
            <div class="modal fade" id="modalRevendedora" tabindex="-1" aria-hidden="true">
                <div class="modal-dialog modal-dialog-centered modal-sm">
                    <div class="modal-content card-glass" style="border-radius: 20px; border: 2px solid #c95c6f;">
                        <div class="modal-body p-4 text-center">
                            <h5 class="fw-bold mb-3" style="color: #c95c6f;"><i class="fas fa-user-tag me-2"></i>Asignar Revendedora</h5>
                            <input type='text' id='revendedoraInput' class='form-control form-control-lg text-center fw-bold shadow-sm mb-3' placeholder='Nombre o Apellido' style='border: 1px solid rgba(201, 92, 111, 0.3); color: var(--mali-primary);' onkeyup='manejarInputRevendedora(event)'>
                            <button class="btn w-100 rounded-pill fw-bold text-white shadow-sm py-2" style="background-color: #c95c6f;" data-bs-dismiss="modal">Guardar</button>
                        </div>
                    </div>
                </div>
            </div>

            <div class='row g-3'>
                <!-- PANEL IZQUIERDO (BUSCADOR PRINCIPAL) -->
                <div class='col-12 col-lg-6'>
                    <div class='card-glass p-3 mb-2'>
                        <div class='input-group shadow-sm' style='border-radius: 16px; overflow: hidden; border: 2px solid #c95c6f;'>
                            <span class='input-group-text bg-transparent border-0'><i class='fas fa-barcode' style='color: #c95c6f;'></i></span>
                            <input type='text' inputmode='numeric' pattern='[0-9]*' id='skuFilter' class='form-control border-0 bg-transparent shadow-none fs-5 fw-bold text-danger' placeholder='SKU' onkeyup='manejarBuscador(event)'>
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

                <!-- PANEL DERECHO (BOLSO) -->
                <div class='col-12 col-lg-6 ticket-col' id='ticketColumn'>
                    <div class='card-glass p-3' style='border-top: 4px solid #c95c6f;'>
                        <div class='mobile-ticket-header justify-content-between align-items-center d-none pb-1' onclick='toggleMobileTicket()'>
                            <span class='fw-bold' style='color: #c95c6f;'><i class='fas fa-shopping-bag me-2'></i> Mi Bolso (<b id='mobileQty'>0</b>)</span>
                            <span class='fw-bold text-dark me-auto ms-3' id='mobileTotal'>$0</span>
                            <i class='fas fa-chevron-up text-muted transition-all' id='ticketChevron'></i>
                        </div>

                        <h6 class='fw-bold mb-3 d-none d-lg-block' style='color: #c95c6f;'>Bolso Armado</h6>
                        <div id='cartItems' class='list-group mb-3'>
                            <div class='text-center text-muted p-4 small' id='emptyCartMsg'>El bolso está vacío.</div>
                        </div>
                        
                        <div class='d-flex justify-content-between align-items-center mt-3 pt-3 border-top d-none d-lg-flex'>
                            <span class='text-muted'>Total Prendas: <b id='totalQty'>0</b></span>
                            <h4 class='fw-bold mb-0' style='color: #c95c6f;' id='totalPrice'>$0</h4>
                        </div>
                        
                        <div class="d-flex gap-2 mt-3">
                            <button id='btnLimpiar' onclick='limpiarRemitoGuardado()' class='btn btn-light border rounded-pill shadow-sm px-3 d-none d-lg-block' title='Vaciar Bolso'><i class='fas fa-trash text-danger'></i></button>
                            <button id='btnConfirmar' onclick='confirmarRemito()' class='btn w-100 rounded-pill fw-bold text-white shadow-sm py-2' style='background-color: #c95c6f;' disabled>
                                CONFIRMAR CONSIGNACIÓN
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

                
                function abrirModalRevendedora() {
                    const modalEl = document.getElementById('modalRevendedora');
                    let modal = bootstrap.Modal.getInstance(modalEl);
                    if (!modal) modal = new bootstrap.Modal(modalEl);
                    modal.show();
                    setTimeout(() => document.getElementById('revendedoraInput').focus(), 500);
                }

                function manejarInputRevendedora(e) {
                    if (e.key === 'Enter') {
                        e.target.blur(); 
                    }
                    guardarRevendedoraLocal();
                }

                function guardarRevendedoraLocal() {
                    const nombre = document.getElementById('revendedoraInput').value;
                    localStorage.setItem('mali_draft_revendedora', nombre);
                    actualizarDisplayRevendedora(nombre);
                }

                function actualizarDisplayRevendedora(nombre) {
                    const display = document.getElementById('revendedoraDisplay');
                    if (nombre && nombre.trim() !== '') {
                        display.innerText = nombre;
                        display.style.color = 'var(--mali-primary)';
                    } else {
                        display.innerText = 'Tocar para asignar...';
                        display.style.color = '#c95c6f';
                    }
                }

                
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

                    let savedRev = localStorage.getItem('mali_draft_revendedora');
                    if (savedRev) {
                        document.getElementById('revendedoraInput').value = savedRev;
                        actualizarDisplayRevendedora(savedRev);
                    } else {
                        actualizarDisplayRevendedora('');
                    }

                    let savedCart = localStorage.getItem('mali_draft_remito');
                    if (savedCart) {
                        try {
                            carrito = JSON.parse(savedCart);
                            if (carrito.length > 0) {
                                showToast("<i class='fas fa-undo me-2'></i> Bolso recuperado");
                                renderCarrito();
                            }
                        } catch(e) {}
                    }
                };

                function guardarProgresoLocal() {
                    localStorage.setItem('mali_draft_remito', JSON.stringify(carrito));
                }

                function limpiarRemitoGuardado() {
                    if(confirm('¿Vaciar todo el bolso?')) {
                        carrito = [];
                        document.getElementById('revendedoraInput').value = '';
                        localStorage.removeItem('mali_draft_remito');
                        localStorage.removeItem('mali_draft_revendedora');
                        actualizarDisplayRevendedora('');
                        renderCarrito();
                        filtrarSKU(document.getElementById('skuFilter').value);
                    }
                }

                function showToast(msg, isError = false) {
                    const box = document.getElementById('toastBox');
                    const badge = document.getElementById('toastMsg');
                    badge.innerHTML = msg;
                    badge.style.background = isError ? '#dc3545' : '#c95c6f';
                    box.style.display = 'block'; box.style.opacity = '1';
                    setTimeout(() => { box.style.opacity = '0'; setTimeout(() => box.style.display = 'none', 300); }, 2000);
                }

                function toggleMobileTicket() {
                    const col = document.getElementById('ticketColumn');
                    const icon = document.getElementById('ticketChevron');
                    col.classList.toggle('ticket-expanded');
                    if(col.classList.contains('ticket-expanded')) {
                        icon.classList.replace('fa-chevron-up', 'fa-chevron-down');
                    } else {
                        icon.classList.replace('fa-chevron-down', 'fa-chevron-up');
                    }
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
                        let badgeActivo = itemEnCarrito ? "<span class='badge bg-danger ms-2' style='font-size:0.6rem;'>EN BOLSO</span>" : "";
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
                                : `<button onclick='agregarAlCarrito(${itemData}, ${v.stock})' class='btn btn-sm rounded-pill px-3 py-1 fw-bold' style='border: 1px solid #c95c6f; color: #c95c6f;'>+ Bolso</button>`;
                            
                            let indicatorHtml = cantEnCarrito > 0 
                                ? `<span class='badge bg-primary rounded-pill me-2'>${cantEnCarrito} carg.</span>` 
                                : ``;

                            varHtml += `
                                <div class='variant-row'>
                                    <div>
                                        <div class='mb-1'><small style='color: var(--mali-text);'>Talle: <b>${v.talle}</b> | Color: <b>${v.color}</b></small></div>
                                        <span class='badge' style='background: rgba(201, 92, 111, 0.1); color: #c95c6f;'>Stock Disp: ${v.stock - cantEnCarrito}</span>
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
                            showToast("<i class='fas fa-check-circle me-2'></i> Sumaste otra prenda");
                        } else {
                            showToast("<i class='fas fa-exclamation-triangle me-2'></i> Stock físico agotado", true);
                        }
                    } else {
                        item.cantidad = 1; carrito.push(item);
                        showToast("<i class='fas fa-check-circle me-2'></i> Prenda añadida al bolso");
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
                        container.innerHTML = "<div class='text-center text-muted p-4 small' id='emptyCartMsg'>El bolso está vacío.</div>";
                        document.getElementById('btnConfirmar').disabled = true;
                    } else {
                        let html = "";
                        carrito.forEach((item, index) => {
                            totalItems += item.cantidad; totalPrice += (item.precio * item.cantidad);
                            let marcaBadge = item.marca ? `<span class='badge align-middle ms-1' style='background: rgba(201, 92, 111, 0.1); color: #c95c6f; border: 1px solid rgba(201, 92, 111, 0.2); font-size: 0.65rem;'>${item.marca}</span>` : "";

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

                function confirmarRemito() {
                    const revNameRaw = document.getElementById('revendedoraInput').value.trim();
                    if (revNameRaw === "") {
                        showToast("<i class='fas fa-exclamation-triangle me-2'></i> Falta la revendedora", true);
                        abrirModalRevendedora(); 
                        return;
                    }

                    const revName = revNameRaw.toLowerCase().split(/\s+/).map(word => 
                        word.charAt(0).toUpperCase() + word.slice(1)
                    ).join(' ');

                    if (carrito.length === 0) return;
                    
                    const btn = document.getElementById('btnConfirmar');
                    btn.innerHTML = "<i class='fas fa-spinner fa-spin'></i> Procesando...";
                    btn.disabled = true;

                    fetch('/api/remitos', {
                        method: 'POST',
                        headers: {'Content-Type': 'application/json'},
                        body: JSON.stringify({ revendedora: revName, carrito: carrito })
                    })
                    .then(res => res.json())
                    .then(data => {
                        if (data.status === 'ok') {
                            localStorage.removeItem('mali_draft_remito'); 
                            localStorage.removeItem('mali_draft_revendedora'); 
                            window.location.href = '/historial_liquidaciones';
                        }
                        else { showToast('Error al guardar', true); btn.innerHTML = "CONFIRMAR CONSIGNACIÓN"; btn.disabled = false; }
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
        html += WebTemplates::getFooter("liquidaciones");
        res.set_content(html, "text/html");
    }

    void handleLiquidarRemito(const httplib::Request &req, httplib::Response &res, Store *tienda)
    {
        std::string idRemito = req.has_param("id") ? req.get_param_value("id") : "";
        if (idRemito.empty())
        {
            res.set_redirect("/historial_liquidaciones");
            return;
        }

        Consignment *consTarget = nullptr;
        std::map<std::string, std::string> mapMarcas;
        {
            std::lock_guard<std::recursive_mutex> lock(mutexTienda);
            for (const auto &fam : tienda->inventory)
            {
                mapMarcas[fam.m_sku] = fam.m_marca.m_name;
            }
            for (auto &c : tienda->consignments)
            {
                if (c.m_idRemito == idRemito)
                {
                    consTarget = &c;
                    break;
                }
            }
        }

        if (!consTarget || consTarget->m_estado != "Pendiente")
        {
            res.set_redirect("/historial_liquidaciones");
            return;
        }

        std::string html = WebTemplates::getHeadAndNav("Liquidar");

        std::string jsItems = "[";
        for (const auto &item : consTarget->m_items)
        {
            std::string marcaItem = mapMarcas.count(item.m_sku) ? mapMarcas[item.m_sku] : "Genérica";
            std::string descSegura = item.m_description;
            size_t pos = 0;
            while ((pos = descSegura.find('"', pos)) != std::string::npos)
            {
                descSegura.replace(pos, 1, "\\\"");
                pos += 2;
            }
            jsItems += "{\"sku\":\"" + item.m_sku + "\",\"marca\":\"" + marcaItem + "\",\"desc\":\"" + descSegura + "\",\"talle\":\"" + item.m_size + "\",\"color\":\"" + item.m_color + "\",\"llevo\":" + std::to_string(item.m_quantity) + ",\"precio\":" + std::to_string(item.m_price) + "},";
        }
        if (!consTarget->m_items.empty())
            jsItems.pop_back();
        jsItems += "]";

        std::string nombreRevendedora = consTarget->m_cliente.empty() ? consTarget->m_vendedora : consTarget->m_cliente;

        html += R"(
            <div class='d-flex justify-content-between align-items-center mb-3'>
                <h2 class='page-title mb-0'><i class='fas fa-hand-holding-usd me-2' style='color: #c95c6f;'></i>Liquidar</h2>
                </div>

            <div class='card-glass p-3 mb-4' style='border-top: 4px solid #c95c6f;'>
                <h5 class='fw-bold mb-1' style='color: var(--mali-primary);'>)" +
                nombreRevendedora + R"(</h5>
                <small class='text-muted'>Fecha de Bolso: )" +
                consTarget->m_fechaSalida + R"(</small>
            </div>

            <div id='toastBox' class='position-fixed bottom-0 start-50 translate-middle-x p-3' style='z-index: 9999; display:none; transition: opacity 0.3s ease;'>
                <div id='toastMsg' class='badge rounded-pill fs-6 px-4 py-2 shadow-lg' style='background: #c95c6f; color: white; font-weight: 600;'></div>
            </div>

            <div class='card-glass p-0 overflow-hidden mb-4'>
                <div id='itemsList'></div>
                
                <div class='p-3' style='background-color: rgba(201, 92, 111, 0.05);'>
                    <div class='d-flex justify-content-between align-items-center mb-2'>
                        <span class='text-muted fw-bold'>Prendas Devueltas:</span>
                        <h5 class='mb-0 fw-bold text-danger' id='totDevueltas'>0</h5>
                    </div>
                    <div class='d-flex justify-content-between align-items-center border-top pt-2'>
                        <span class='text-muted fw-bold fs-5'>Monto a Pagar:</span>
                        <h3 class='mb-0 fw-bold' style='color: #28a745;' id='totPlata'>$0</h3>
                    </div>
                </div>
            </div>

            <button id='btnConfirmar' onclick='enviarLiquidacion()' class='btn w-100 rounded-pill fw-bold text-white shadow-sm py-3 fs-5 mb-5' style='background-color: #28a745;'>
                <i class='fas fa-check-circle me-2'></i> CONFIRMAR CIERRE
            </button>

            <script>
                const idRemito = ')" +
                idRemito + R"(';
                let items = )" +
                jsItems + R"(;
                
                items.forEach(i => i.devuelto = 0);

                function showToast(msg, isError = false) {
                    const box = document.getElementById('toastBox');
                    const badge = document.getElementById('toastMsg');
                    badge.innerHTML = msg;
                    badge.style.background = isError ? '#dc3545' : '#28a745';
                    box.style.display = 'block'; box.style.opacity = '1';
                    setTimeout(() => { box.style.opacity = '0'; setTimeout(() => box.style.display = 'none', 300); }, 2000);
                }

                function cambiarDevolucion(idx, delta) {
                    let max = items[idx].llevo;
                    let nuevo = items[idx].devuelto + delta;
                    if(nuevo >= 0 && nuevo <= max) {
                        items[idx].devuelto = nuevo;
                        render();
                    }
                }

                function render() {
                    let html = ""; let totCobrar = 0; let totDevueltas = 0;

                    items.forEach((item, idx) => {
                        let vendidos = item.llevo - item.devuelto;
                        totCobrar += (vendidos * item.precio);
                        totDevueltas += item.devuelto;

                        html += `
                            <div class='p-3 border-bottom'>
                                <div class='d-flex justify-content-between align-items-center mb-2'>
                                    <div>
                                        <span class='fw-bold' style='color: var(--mali-primary);'>${item.sku}</span>
                                        <span class='badge align-middle ms-1' style='background: rgba(201, 92, 111, 0.1); color: #c95c6f; border: 1px solid rgba(201, 92, 111, 0.2); font-size: 0.65rem;'>${item.marca}</span>
                                        <small class='d-block text-muted' style='line-height:1.2; margin-top: 4px;'>${item.desc}</small>
                                        <small style='color: var(--mali-text);'>T: <b>${item.talle}</b> | C: <b>${item.color}</b></small>
                                    </div>
                                    <div class='text-end'>
                                        <span class='badge bg-warning text-dark mb-1'>$${item.precio.toLocaleString('es-AR')}</span>
                                        <div class='small text-muted'>Llevó: <b>${item.llevo}</b> un.</div>
                                    </div>
                                </div>
                                
                                <div class='d-flex justify-content-between align-items-center mt-2 p-2 rounded' style='background: rgba(0,0,0,0.03);'>
                                    <span class='fw-bold text-danger small'><i class='fas fa-undo me-1'></i>Devuelve:</span>
                                    <div class='input-group input-group-sm' style='width: 120px;'>
                                        <button class='btn btn-outline-danger' onclick='cambiarDevolucion(${idx}, -1)'>-</button>
                                        <input type='text' class='form-control text-center fw-bold text-dark' value='${item.devuelto}' readonly>
                                        <button class='btn btn-outline-danger' onclick='cambiarDevolucion(${idx}, 1)'>+</button>
                                    </div>
                                </div>
                            </div>
                        `;
                    });

                    document.getElementById('itemsList').innerHTML = html;
                    document.getElementById('totDevueltas').innerText = totDevueltas;
                    document.getElementById('totPlata').innerText = "$" + totCobrar.toLocaleString('es-AR');
                }

                function enviarLiquidacion() {
                    const btn = document.getElementById('btnConfirmar');
                    btn.innerHTML = "<i class='fas fa-spinner fa-spin'></i> Procesando...";
                    btn.disabled = true;

                    fetch('/api/liquidar_remito', {
                        method: 'POST',
                        headers: {'Content-Type': 'application/json'},
                        body: JSON.stringify({ id: idRemito, devoluciones: items })
                    })
                    .then(res => res.json())
                    .then(data => {
                        if (data.status === 'ok') window.location.href = '/historial_liquidaciones';
                        else { showToast(data.msg || 'Error al guardar', true); btn.innerHTML = "<i class='fas fa-check-circle me-2'></i> CONFIRMAR CIERRE"; btn.disabled = false; }
                    }).catch(e => { showToast('Error de conexión', true); btn.disabled = false; });
                }

                render();
            </script>
        )";
        html += WebTemplates::getFooter("liquidaciones");
        res.set_content(html, "text/html");
    }

    void handleApiGetLiquidaciones(const httplib::Request &req, httplib::Response &res, Store *tienda)
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

            for (auto it = tienda->consignments.rbegin(); it != tienda->consignments.rend(); ++it)
            {
                const auto &cons = *it;
                if (usuarioRol == Rol::Vendedora && cons.m_vendedora != miNombre)
                    continue;

                std::string nombreRevendedora = cons.m_cliente.empty() ? cons.m_vendedora : cons.m_cliente;
                std::string nombreLow = nombreRevendedora;
                std::transform(nombreLow.begin(), nombreLow.end(), nombreLow.begin(), ::tolower);

                std::string idRemitoLow = cons.m_idRemito;
                std::transform(idRemitoLow.begin(), idRemitoLow.end(), idRemitoLow.begin(), ::tolower);

                if (!query.empty() && nombreLow.find(query) == std::string::npos && idRemitoLow.find(query) == std::string::npos)
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

                float totalReal = cons.m_totalAPagar;
                if (totalReal <= 0.01f && !cons.m_items.empty())
                {
                    for (const auto &item : cons.m_items)
                    {
                        if (cons.m_estado == "Pendiente")
                        {
                            totalReal += (item.m_quantity * item.m_price);
                        }
                        else
                        {
                            int vendidos = item.m_quantity - item.m_returned;
                            if (vendidos > 0)
                                totalReal += (vendidos * item.m_price);
                        }
                    }
                }

                json jCons;
                jCons["idRemito"] = cons.m_idRemito;
                jCons["nombreRevendedora"] = nombreRevendedora;
                jCons["estado"] = cons.m_estado;
                jCons["fechaSalida"] = cons.m_fechaSalida;
                jCons["fechaLimite"] = cons.m_fechaLimite;
                jCons["totalReal"] = totalReal;
                jCons["totalArticulos"] = cons.m_totalArticulos;

                json jItems = json::array();
                for (const auto &item : cons.m_items)
                {
                    json jItem;
                    jItem["sku"] = item.m_sku;
                    jItem["marca"] = mapMarcas.count(item.m_sku) ? mapMarcas[item.m_sku] : "Genérica";
                    jItem["description"] = item.m_description;
                    jItem["quantity"] = item.m_quantity;
                    jItem["returned"] = item.m_returned;
                    jItem["price"] = item.m_price;
                    jItems.push_back(jItem);
                }
                jCons["items"] = jItems;

                resultado.push_back(jCons);
                matchesAgregados++;
                matchesEncontrados++;
            }
        }
        res.set_content(resultado.dump(), "application/json");
    }

    void handleLiquidaciones(const httplib::Request &req, httplib::Response &res, Store *tienda)
    {
        WebUtils::ActualizarPreciosEnRemitosPendientes(tienda);

        std::string html = WebTemplates::getHeadAndNav("Remitos");
        html += "<h2 class='page-title mb-4'>Gestión de Remitos</h2>";

        html += "<div class='mb-4'><input type='text' id='searchBox' class='form-control text-center' placeholder='🔍 Buscar vendedora o ID... (Servidor)'></div>";

        html += "<div class='accordion' id='accordionRemitos'></div>";

        html += R"(
            <div id="loading-spinner" class="text-center py-4" style="display:none;">
                <i class="fas fa-spinner fa-spin fa-2x" style="color: #c95c6f;"></i>
                <p class="text-muted mt-2 small fw-bold">Cargando remitos...</p>
            </div>
        )";

        html += R"(
            <script>
                let currentOffset = 0;
                const limit = 20;
                let isLoading = false;
                let hasMoreData = true;
                let searchQuery = '';
                let timeoutFiltroNuevoRemito = null;

                async function cargarMasLiquidaciones(reset = false) {
                    if (isLoading) return;
                    if (reset) {
                        currentOffset = 0;
                        hasMoreData = true;
                        document.getElementById('accordionRemitos').innerHTML = '';
                    }
                    if (!hasMoreData) return;

                    isLoading = true;
                    document.getElementById('loading-spinner').style.display = 'block';

                    try {
                        const url = `/api/get_liquidaciones?offset=${currentOffset}&limit=${limit}&q=${encodeURIComponent(searchQuery)}`;
                        const res = await fetch(url);
                        const data = await res.json();

                        if (data.length < limit) {
                            hasMoreData = false;
                            document.getElementById('loading-spinner').style.display = 'none';
                        }

                        if (currentOffset === 0 && data.length === 0) {
                            document.getElementById('accordionRemitos').innerHTML = "<div class='alert alert-light text-center'>No se encontraron remitos.</div>";
                        }

                        renderizarBloqueRemitos(data);
                        currentOffset += data.length;
                    } catch (e) {
                        console.error("Error cargando remitos:", e);
                    } finally {
                        isLoading = false;
                    }
                }

                function renderizarBloqueRemitos(remitos) {
                    const container = document.getElementById('accordionRemitos');
                    let html = '';
                    
                    remitos.forEach((cons, i) => {
                        let idxStr = 'r_' + (currentOffset + i);
                        let nombreRevendedora = cons.nombreRevendedora;
                        
                        let bgStatus = "bg-secondary";
                        if (cons.estado === "Pendiente") bgStatus = "bg-warning text-dark";
                        else if (cons.estado === "Pagado" || cons.estado === "Cerrado" || cons.estado === "Liquidado") bgStatus = "bg-success"; 
                       
                        let itemsHtml = '';
                        let textWa = "Hola *" + nombreRevendedora + "* 🌸,\nTe comparto tu liquidación:\n📅 *Fecha:* " + cons.fechaSalida + "\n👗 *Total Prendas:* " + cons.totalArticulos + "\n💰 *Total:* $" + cons.totalReal.toLocaleString('es-AR') + "\n\n*Detalle:*\n";

                        cons.items.forEach(item => {
                            let devolucionBadge = '';
                            if (cons.estado === "Pagado" || cons.estado === "Cerrado") {
                                devolucionBadge = `<span class='badge bg-light text-danger border ms-1'>Devolvió: ${item.returned}</span>`;
                            }

                            itemsHtml += `
                                <li class='list-group-item px-0 d-flex justify-content-between align-items-center border-bottom border-light py-2'>
                                    <div style='width: 70%;'>
                                        <span class='d-block fw-bold' style='color: var(--mali-primary); font-size:1rem;'>${item.sku} 
                                            <span class='badge align-middle ms-1' style='background: rgba(201, 92, 111, 0.1); color: #c95c6f; border: 1px solid rgba(201, 92, 111, 0.2); font-size: 0.65rem;'>${item.marca}</span>
                                        </span>
                                        <small class='text-muted d-block mt-1 mb-2'>${item.description}</small>
                                        <span class='badge bg-light text-dark border'>Llevó: ${item.quantity}</span>
                                        ${devolucionBadge}
                                    </div>
                                    <div class='text-end'>
                                        <span class='d-block text-dark fw-bold'>$${item.price.toLocaleString('es-AR')}</span>
                                    </div>
                                </li>
                            `;
                            textWa += "- *" + item.sku + "* (" + item.marca + ")\n  _" + item.description + "_\n  " + item.quantity + " un. x $" + item.price.toLocaleString('es-AR') + "\n\n";
                        });

                        textWa += "Gracias! ✨";
                        
                        let encodedWa = encodeURIComponent(textWa);

                        let botonLiquidar = '';
                        if (cons.estado === "Pendiente") {
                            botonLiquidar = `
                                <a href='/liquidar_remito?id=${cons.idRemito}' class='btn w-100 rounded-pill fw-bold text-white shadow-sm mb-2' style='background-color: #0d6efd;'>
                                    <i class='fas fa-hand-holding-usd me-2 fs-5 align-middle'></i> Liquidar Deuda
                                </a>
                            `;
                        }

                        let textoFechas = `Emisión: ${cons.fechaSalida}`;
                        if (cons.estado === "Pendiente") {
                            textoFechas += `<br>Vence: ${cons.fechaLimite || '—'}`;
                        } else {
                            textoFechas += `<br>Cierre: ${cons.fechaLimite || '—'}`;
                        }

                        html += `
                            <div class='card-glass mb-3' style='transform: translateZ(0);'>
                                <div class='accordion-item'>
                                    <h2 class='accordion-header' id='headingR${idxStr}'>
                                        <button class='accordion-button collapsed px-3 py-3' type='button' data-bs-toggle='collapse' data-bs-target='#collapseR${idxStr}'>
                                            <div class='d-flex flex-column w-100 me-2'>
                                                <div class='d-flex justify-content-between align-items-center mb-2'>
                                                    <span class='fw-bold text-dark fs-6'>${nombreRevendedora}</span>
                                                    <span class='badge ${bgStatus} rounded-pill'>${cons.estado}</span>
                                                </div>
                                                <div class='d-flex justify-content-between align-items-center text-muted' style='font-size:0.85rem;'>
                                                    <span>${textoFechas}</span>
                                                    <span class='fw-bold text-primary'>$${cons.totalReal.toLocaleString('es-AR')}</span>
                                                </div>
                                            </div>
                                        </button>
                                    </h2>
                                    <div id='collapseR${idxStr}' class='accordion-collapse collapse' data-bs-parent='#accordionRemitos'>
                                        <div class='accordion-body bg-white pt-2'>
                                            <h6 class='text-muted text-uppercase mb-3 mt-2 fs-7' style='letter-spacing: 1px;'>Detalle de Artículos</h6>
                                            <ul class='list-group list-group-flush'>
                                                ${itemsHtml}
                                            </ul>
                                            <div class='mt-4 pt-3 border-top'>
                                                ${botonLiquidar}
                                                <button onclick="window.location.href='https:
                                                    <i class='fab fa-whatsapp me-2 fs-5 align-middle'></i> Compartir por WhatsApp
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
                        cargarMasLiquidaciones();
                    }
                }, { rootMargin: '150px' });

                document.addEventListener('DOMContentLoaded', () => {
                    observer.observe(document.getElementById('loading-spinner'));
                    
                    cargarMasLiquidaciones(true);
                    const sb = document.getElementById('searchBox');
                    if (sb) {
                        sb.addEventListener('keyup', () => {
                            clearTimeout(timeoutFiltroNuevoRemito);
                            timeoutFiltroNuevoRemito = setTimeout(() => {
                                searchQuery = sb.value.trim();
                                cargarMasLiquidaciones(true);
                            }, 350);
                        });
                    }
                });
            </script>
        )";

        html += "</div>" + WebTemplates::getFooter("liquidaciones");
        res.set_content(html, "text/html");
    }
}