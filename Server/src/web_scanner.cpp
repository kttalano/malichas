#include "web_scanner.hpp"
#include "web_templates.hpp"
#include "web_auth.hpp"
#include "web_utils.hpp"
#include <mutex>
#include <iostream>
#include <algorithm>

extern std::recursive_mutex mutexTienda;

namespace WebRoutes
{

    void handleApiScanBarcode(const httplib::Request &req, httplib::Response &res, Store *tienda)
    {
        std::string code = req.has_param("code") ? req.get_param_value("code") : "";
        if (code.empty())
        {
            res.set_content("{\"status\":\"error\"}", "application/json");
            return;
        }

        std::lock_guard<std::recursive_mutex> lock(mutexTienda);
        for (const auto &fam : tienda->inventory)
        {
            for (const auto &var : fam.m_variantes)
            {
                if (std::find(var.m_codigoBarras.begin(), var.m_codigoBarras.end(), code) != var.m_codigoBarras.end())
                {
                    json j;
                    j["status"] = "ok";
                    j["sku"] = fam.m_sku;
                    j["marca"] = fam.m_marca.m_name;
                    j["desc"] = fam.m_nombre;
                    j["talle"] = var.m_talle;
                    j["color"] = var.m_color;
                    j["precio"] = fam.obtenerPrecioFinal(var);
                    j["stock"] = var.m_stock;
                    res.set_content(j.dump(), "application/json");
                    return;
                }
            }
        }
        res.set_content("{\"status\":\"error\", \"msg\":\"Código no registrado en el sistema\"}", "application/json");
    }

    void handleApiLinkBarcode(const httplib::Request &req, httplib::Response &res, Store *tienda)
    {
        try
        {
            std::string userEmail = req.get_header_value("Cf-Access-Authenticated-User-Email");
            Rol usuarioRol = WebAuth::obtenerRolPorEmail(userEmail);

            if (usuarioRol != Rol::Admin && usuarioRol != Rol::Owner && usuarioRol != Rol::Supervisor)
            {
                res.set_content("{\"status\":\"error\", \"msg\":\"No tienes permisos\"}", "application/json");
                return;
            }

            if (req.body.empty())
            {
                res.set_content("{\"status\":\"error\", \"msg\":\"Datos vacios\"}", "application/json");
                return;
            }

            json payload = json::parse(req.body);
            std::string barcode = payload.value("barcode", "");
            std::string sku = payload.value("sku", "");
            std::string talle = payload.value("talle", "");
            std::string color = payload.value("color", "");
            bool forceMove = payload.value("force_move", false);
            bool replaceTarget = payload.value("replace_target", false);

            if (barcode.empty() || sku.empty())
            {
                res.set_content("{\"status\":\"error\", \"msg\":\"Faltan datos obligatorios\"}", "application/json");
                return;
            }

            bool vinculado = false;
            bool returnConfirm = false;
            std::string articuloDueño = "";

            {
                std::lock_guard<std::recursive_mutex> lock(mutexTienda);
                bool codigoYaExisteEnOtroLado = false;

                for (const auto &fam : tienda->inventory)
                {
                    for (const auto &var : fam.m_variantes)
                    {
                        if (std::find(var.m_codigoBarras.begin(), var.m_codigoBarras.end(), barcode) != var.m_codigoBarras.end())
                        {
                            if (!(fam.m_sku == sku && var.m_talle == talle && var.m_color == color))
                            {
                                codigoYaExisteEnOtroLado = true;
                                articuloDueño = fam.m_sku + " (T:" + var.m_talle + " C:" + var.m_color + ")";
                                break;
                            }
                        }
                    }
                    if (codigoYaExisteEnOtroLado)
                        break;
                }

                if (codigoYaExisteEnOtroLado && !forceMove)
                {
                    returnConfirm = true;
                }
                else
                {
                    if (codigoYaExisteEnOtroLado && forceMove)
                    {
                        for (auto &fam : tienda->inventory)
                        {
                            for (auto &var : fam.m_variantes)
                            {
                                auto it = std::remove(var.m_codigoBarras.begin(), var.m_codigoBarras.end(), barcode);
                                if (it != var.m_codigoBarras.end())
                                {
                                    var.m_codigoBarras.erase(it, var.m_codigoBarras.end());
                                }
                            }
                        }
                    }

                    for (auto &fam : tienda->inventory)
                    {
                        if (fam.m_sku == sku)
                        {
                            for (auto &var : fam.m_variantes)
                            {
                                if (var.m_talle == talle && var.m_color == color)
                                {
                                    if (replaceTarget)
                                    {
                                        var.m_codigoBarras.clear();
                                    }
                                    if (std::find(var.m_codigoBarras.begin(), var.m_codigoBarras.end(), barcode) == var.m_codigoBarras.end())
                                    {
                                        var.m_codigoBarras.push_back(barcode);
                                    }
                                    vinculado = true;
                                    break;
                                }
                            }
                        }
                        if (vinculado)
                            break;
                    }
                }
            }

            if (returnConfirm)
            {
                json err;
                err["status"] = "confirm_move";
                err["msg"] = "Este código ya pertenece a: " + articuloDueño + ".\n\n¿Querés quitarlo de ahí y asignarlo a este producto nuevo?";
                res.set_content(err.dump(), "application/json");
                return;
            }

            if (vinculado)
            {
                tienda->saveToFileAsync();
                res.set_content("{\"status\":\"ok\"}", "application/json");
            }
            else
            {
                res.set_content("{\"status\":\"error\", \"msg\":\"Variante no encontrada en la base de datos\"}", "application/json");
            }
        }
        catch (...)
        {
            res.set_content("{\"status\":\"error\", \"msg\":\"Error crítico en el servidor\"}", "application/json");
        }
    }

    void handleScanner(const httplib::Request &req, httplib::Response &res, Store *tienda)
    {
        std::string userEmail = req.get_header_value("Cf-Access-Authenticated-User-Email");
        Rol usuarioRol = WebAuth::obtenerRolPorEmail(userEmail);
        bool esAdmin = (usuarioRol == Rol::Admin || usuarioRol == Rol::Owner || usuarioRol == Rol::Supervisor);

        std::string html = WebTemplates::getHeadAndNav("Escáner");

        html += R"HTML(
            <style>
                #reader { width: 100%; border: 2px solid var(--mali-primary); background: black; min-height: 250px;}
                #reader video { object-fit: cover; }
                .scanner-cart { max-height: 250px; overflow-y: auto; }
                .variant-selector { transition: all 0.2s ease; color: var(--mali-text); background: transparent; border: 1px solid rgba(0,0,0,0.2); }
                .variant-selector:hover { background-color: rgba(0,0,0,0.05); }
                .variant-selected { background-color: var(--mali-primary) !important; color: white !important; border-color: var(--mali-primary) !important; box-shadow: 0 4px 10px rgba(92, 51, 190, 0.3) !important; }
            </style>
            
            <script src="https:

            <div class='d-flex justify-content-between align-items-center mb-3'>
                <h2 class='page-title mb-0'><i class='fas fa-barcode me-2' style='color: var(--mali-primary);'></i>Escáner</h2>
            </div>

            <div id='toastBox' class='position-fixed top-0 start-50 translate-middle-x p-3 mt-5' style='z-index: 9999; display:none; transition: opacity 0.3s ease;'>
                <div id='toastMsg' class='badge rounded-pill fs-6 px-4 py-3 shadow-lg' style='background: var(--mali-primary); color: white; font-weight: 600;'></div>
            </div>

            <div class='row g-3 pb-5 mb-5'>
                <div class='col-12 col-lg-6'>
                    <div class='card-glass p-3'>
                        <div class="d-flex justify-content-between align-items-center mb-3 border-bottom pb-2">
                            <span class="fw-bold text-muted"><i class="fas fa-sliders-h text-secondary me-1"></i> Estado:</span>
                            <div class="d-flex gap-3">
                                <div class="form-check form-switch fs-5 mb-0">
                                    <input class="form-check-input shadow-none border-success" type="checkbox" id="modoCarrito" onchange="if(this.checked && document.getElementById('modoVincular')) document.getElementById('modoVincular').checked = false;">
                                    <label class="form-check-label fw-bold text-success ms-1 fs-6" for="modoCarrito" style="margin-top: 2px;">Modo Venta</label>
                                </div>
        )HTML";

        if (esAdmin)
        {
            html += R"HTML(
                                <div class="form-check form-switch fs-5 mb-0">
                                    <input class="form-check-input shadow-none border-danger" type="checkbox" id="modoVincular" onchange="if(this.checked) document.getElementById('modoCarrito').checked = false;">
                                    <label class="form-check-label fw-bold text-danger ms-1 fs-6" for="modoVincular" style="margin-top: 2px;">Vincular</label>
                                </div>
            )HTML";
        }

        html += R"HTML(
                            </div>
                        </div>
                        
                        <div id="reader"></div>
                        
                        <div id="controlesCamara" class="mt-2 d-flex justify-content-center align-items-center gap-2" style="display: none !important;">
                            <select id="listaCamaras" class="form-select form-select-sm text-center fw-bold shadow-sm" style="border-radius: 8px; border: 1px solid rgba(0,0,0,0.1); background-color: rgba(128,128,128,0.05); color: var(--mali-text);" onchange="cambiarCamaraSeleccionada(this.value)"></select>
                            <button class="btn btn-sm btn-light shadow-sm border" onclick="cambiarCamara()" style="border-radius: 8px;"><i class="fas fa-sync-alt text-secondary"></i></button>
                        </div>
                        <button id="btnCamara" class="btn btn-outline-primary w-100 rounded-pill mt-3 fw-bold py-2" style="display: none;" onclick="iniciarCamaras()"><i class="fas fa-camera me-2"></i> Reintentar Cámara</button>
                    </div>
                </div>

                <div class='col-12 col-lg-6'>
                    <div class='card-glass p-3'>
                        <div class='d-flex justify-content-between align-items-center mb-3'>
                            <h5 class='fw-bold mb-0' style='color: var(--mali-secondary);'>Lote Escaneado</h5>
                            <button class='btn btn-sm btn-outline-danger rounded-pill px-3 fw-bold' onclick='limpiarCarritoLocal()'>Limpiar</button>
                        </div>
                        <div id='scanCart' class='list-group scanner-cart mb-3'></div>
                        <div class='d-flex gap-2 border-top pt-3'>
                            <button class='btn btn-success w-50 rounded-pill fw-bold py-2' onclick='exportarYRedirigir("/ventas")'><i class='fas fa-shopping-cart me-2'></i>A Ventas</button>
                            <button class='btn w-50 rounded-pill fw-bold text-white py-2' style='background-color: #c95c6f;' onclick='exportarYRedirigir("/liquidaciones")'><i class='fas fa-box-open me-2'></i>A Remitos</button>
                        </div>
                    </div>
                </div>
            </div>

            <div class="modal fade" id="modalConsultaPrecio" tabindex="-1" aria-hidden="true">
                <div class="modal-dialog modal-dialog-centered">
                    <div class="modal-content shadow-lg" data-bs-dismiss="modal" style="background: var(--mali-card-bg); border-radius: 20px; border: 2px solid #0dcaf0; cursor: pointer;">
                        <div class="modal-body p-4 text-center">
                            <h5 class="fw-bold text-info mb-1"><i class="fas fa-tag me-2"></i>Precio</h5>
                            <hr class="my-2 opacity-25">
                            <div id="consultaBrandSkuTxt" class="text-muted small fw-bold text-uppercase mb-1" style="letter-spacing: 1px;">MARCA • SKU</div>
                            <h1 id="consultaPrecioTxt" class="display-3 fw-bolder text-success mb-2" style="font-size: 4rem;">$0</h1>
                            <h5 id="consultaDescTxt" class="text-muted fw-bold mb-3 px-2">Nombre del Artículo</h5>
                            <div class="d-inline-block bg-light rounded-pill px-4 py-2 border shadow-sm mb-3">
                                <span id="consultaVarTxt" class="fw-bold fs-5" style="color: var(--mali-primary);">Talle / Color</span>
                            </div>
                        </div>
                    </div>
                </div>
            </div>

            <div class="modal fade" id="modalVincular" tabindex="-1" data-bs-backdrop="static">
                <div class="modal-dialog modal-dialog-centered">
                    <div class="modal-content" style="background: var(--mali-card-bg); backdrop-filter: blur(20px); border-radius: 20px;">
                        <div class="modal-body p-4 text-center">
                            <h5 class="fw-bold text-danger mb-3"><i class="fas fa-link me-2"></i>Asignar Código</h5>
                            <p class="text-muted mb-1 small">Código capturado:</p>
                            <h4 id="modalBarcodeDisplay" class="fw-bold mb-3 text-dark" style="letter-spacing: 1px;"></h4>
                            
                            <div class="input-group mb-3 shadow-sm" style="border-radius: 12px; overflow: hidden; border: 1px solid rgba(0,0,0,0.1);">
                                <span class="input-group-text bg-transparent border-0"><i class="fas fa-search text-muted"></i></span>
                                <input type="text" id="inputBuscarSKU" class="form-control border-0 bg-transparent shadow-none fw-bold" placeholder="Escribí un SKU..." onkeyup="manejarEnterTeclado(event)">
                            </div>

                            <div id="resultadosBusquedaVincular" class="text-start mb-4" style="max-height: 250px; overflow-y: auto;"></div>
                            
                            <div class="d-flex gap-2 border-top pt-3">
                                <button class="btn btn-light w-50 rounded-pill fw-bold border" onclick="cerrarModal()">Cancelar</button>
                                <button id="btnGuardarVinculo" class="btn btn-danger w-50 rounded-pill fw-bold" onclick="guardarVinculo()" disabled>Vincular</button>
                            </div>
                        </div>
                    </div>
                </div>
            </div>

            <div class="modal fade" id="modalOpcionesVinculo" tabindex="-1" style="z-index: 1060;">
                <div class="modal-dialog modal-dialog-centered modal-sm">
                    <div class="modal-content shadow-lg" style="background: var(--mali-card-bg); border-radius: 20px;">
                        <div class="modal-body p-4 text-center">
                            <i class="fas fa-list-ul mb-3 text-warning" style="font-size: 3.5rem;"></i>
                            <h6 class="fw-bold mb-3" style="color: var(--mali-text);">Este artículo ya tiene códigos cargados. ¿Qué hacemos?</h6>
                            <div class="d-flex flex-column gap-2 mt-4">
                                <button class="btn btn-warning rounded-pill fw-bold text-dark py-2 border" onclick="seleccionarOpcion(false)"><i class="fas fa-plus me-2"></i> Añadir a la lista</button>
                                <button class="btn btn-danger rounded-pill fw-bold text-white py-2 shadow-sm" onclick="seleccionarOpcion(true)"><i class="fas fa-exchange-alt me-2"></i> Reemplazar los viejos</button>
                                <button class="btn btn-light border rounded-pill fw-bold mt-2" data-bs-dismiss="modal">Cancelar</button>
                            </div>
                        </div>
                    </div>
                </div>
            </div>

            <script>
                let html5QrcodeScanner = null;
                let scanCart = JSON.parse(localStorage.getItem('mali_scanned_cart')) || [];
                let pauseScan = false;
                let pendingBarcode = "";
                let varianteSeleccionada = null;
                let camarasDisponibles = [];
                let indiceCamaraActual = 0;
                let timeoutConsulta = null; 
                let scannerSearchTimeout = null;
                let tieneCodigosPrevios = false;

                function playBeep() {
                    try {
                        const ctx = new (window.AudioContext || window.webkitAudioContext)();
                        const osc = ctx.createOscillator();
                        osc.type = 'square'; osc.frequency.setValueAtTime(1000, ctx.currentTime);
                        osc.connect(ctx.destination); osc.start(); osc.stop(ctx.currentTime + 0.1);
                        if (navigator.vibrate) navigator.vibrate([100, 50, 100]); 
                    } catch(e){}
                }

                function showToast(msg, isError = false) {
                    const box = document.getElementById('toastBox');
                    const badge = document.getElementById('toastMsg');
                    badge.innerHTML = msg;
                    badge.style.background = isError ? '#dc3545' : '#28a745';
                    box.style.display = 'block'; box.style.opacity = '1';
                    setTimeout(() => { box.style.opacity = '0'; setTimeout(() => box.style.display = 'none', 300); }, 3500);
                }

                function renderCart() {
                    localStorage.setItem('mali_scanned_cart', JSON.stringify(scanCart));
                    const container = document.getElementById('scanCart');
                    if (scanCart.length === 0) { container.innerHTML = "<div class='text-center text-muted p-4 small'>No hay artículos escaneados.</div>"; return; }
                    
                    let html = "";
                    scanCart.forEach((item, index) => {
                        html += `
                            <div class='list-group-item d-flex justify-content-between align-items-center bg-transparent border-0 border-bottom py-2 px-1'>
                                <div style="line-height: 1.2;">
                                    <div class='fw-bold' style='color: var(--mali-primary);'>${item.sku} <span class="badge bg-light text-dark ms-1">${item.cantidad} un.</span></div>
                                    <small class='text-muted' style="font-size: 0.75rem;">${item.desc}</small><br>
                                    <small class='text-muted fw-bold' style="font-size: 0.75rem;">${item.talle} / ${item.color}</small>
                                </div>
                                <div class='text-end'>
                                    <span class='fw-bold d-block text-success mb-1'>$${(item.precio * item.cantidad).toLocaleString('es-AR')}</span>
                                    <button class='btn btn-link text-danger p-0' onclick='removeItem(${index})'><i class='fas fa-times-circle fs-5'></i></button>
                                </div>
                            </div>`;
                    });
                    container.innerHTML = html;
                }

                function removeItem(idx) { scanCart.splice(idx, 1); renderCart(); }
                function limpiarCarritoLocal() { scanCart = []; renderCart(); }
                
                function exportarYRedirigir(url) {
                    if (scanCart.length === 0) { showToast("El lote está vacío", true); return; }
                    localStorage.setItem('mali_export_trigger', 'true');
                    window.location.href = url;
                }

                function onScanSuccess(decodedText) {
                    if (pauseScan) return;
                    pauseScan = true;
                    playBeep();

                    const isLinkMode = document.getElementById('modoVincular') && document.getElementById('modoVincular').checked;
                    const isCarritoMode = document.getElementById('modoCarrito') && document.getElementById('modoCarrito').checked;

                    if (isLinkMode) {
                        pendingBarcode = decodedText;
                        varianteSeleccionada = null;
                        document.getElementById('modalBarcodeDisplay').innerText = decodedText;
                        document.getElementById('inputBuscarSKU').value = "";
                        document.getElementById('resultadosBusquedaVincular').innerHTML = '<div class="text-center text-muted p-3 small">Buscá un SKU para ver sus variantes...</div>';
                        document.getElementById('btnGuardarVinculo').disabled = true;
                        
                        new bootstrap.Modal(document.getElementById('modalVincular')).show();
                        setTimeout(() => document.getElementById('inputBuscarSKU').focus(), 500);
                    } else {
                        
                        fetch('/api/scan_barcode?code=' + encodeURIComponent(decodedText))
                        .then(res => res.json())
                        .then(data => {
                            if (data.status === 'ok') {
                                if (isCarritoMode) {
                                    let existente = scanCart.find(x => x.sku === data.sku && x.talle === data.talle && x.color === data.color);
                                    if(existente) {
                                        if(existente.cantidad < data.stock) { existente.cantidad++; showToast("<i class='fas fa-check-circle me-2'></i> Sumaste otra unidad"); } 
                                        else { showToast("<i class='fas fa-exclamation-triangle me-2'></i> Sin stock físico", true); }
                                    } else {
                                        data.cantidad = 1; scanCart.push(data);
                                        showToast("<i class='fas fa-check-circle me-2'></i> <b>$" + data.precio.toLocaleString('es-AR') + "</b><br><small>" + data.sku + " agregado</small>");
                                    }
                                    renderCart();
                                } else {
                                    document.getElementById('consultaPrecioTxt').innerText = '$' + data.precio.toLocaleString('es-AR');
                                    document.getElementById('consultaDescTxt').innerText = data.desc;
                                    document.getElementById('consultaVarTxt').innerText = 'T: ' + data.talle + ' | C: ' + data.color;
                                    document.getElementById('consultaBrandSkuTxt').innerText = (data.marca || "Sin marca") + ' • ' + data.sku;

                                    const modalEl = document.getElementById('modalConsultaPrecio');
                                    const modal = bootstrap.Modal.getInstance(modalEl) || new bootstrap.Modal(modalEl);
                                    modal.show();
                                    
                                    clearTimeout(timeoutConsulta);
                                    timeoutConsulta = setTimeout(() => { modal.hide(); }, 10000);
                                }
                            } else {
                                showToast("<i class='fas fa-exclamation-triangle me-2'></i> Código desconocido", true);
                            }
                        })
                        .catch(err => { showToast("Error de conexión", true); })
                        .finally(() => { setTimeout(() => { pauseScan = false; }, 1500); });
                    }
                }

                function iniciarCamaras() {
                    document.getElementById('btnCamara').style.display = 'none'; 
                    if (!html5QrcodeScanner) html5QrcodeScanner = new Html5Qrcode("reader");
                    
                    Html5Qrcode.getCameras().then(devices => {
                        if (devices && devices.length > 0) {
                            camarasDisponibles = devices;
                            const select = document.getElementById('listaCamaras');
                            select.innerHTML = '';
                            let camaraGuardada = localStorage.getItem('mali_saved_camera');
                            let camaraElegidaId = devices[0].id;
                            
                            devices.forEach((cam, index) => {
                                select.add(new Option(cam.label || `Lente ${index + 1}`, cam.id));
                                if (camaraGuardada && cam.id === camaraGuardada) { camaraElegidaId = cam.id; indiceCamaraActual = index; }
                            });
                            
                            select.value = camaraElegidaId;
                            if (devices.length > 1) document.getElementById('controlesCamara').style.setProperty('display', 'flex', 'important');
                            encenderLente(camaraElegidaId);
                        } else encenderLente({ facingMode: "environment" });
                    }).catch(err => encenderLente({ facingMode: "environment" }));
                }

                function cambiarCamaraSeleccionada(camId) {
                    localStorage.setItem('mali_saved_camera', camId);
                    indiceCamaraActual = camarasDisponibles.findIndex(cam => cam.id === camId);
                    if (html5QrcodeScanner && html5QrcodeScanner.isScanning) { 
                        html5QrcodeScanner.stop().then(() => encenderLente(camId)); 
                    } else { encenderLente(camId); }
                }
                
                function cambiarCamara() {
                    if (camarasDisponibles.length > 1) {
                        indiceCamaraActual = (indiceCamaraActual + 1) % camarasDisponibles.length;
                        const nuevoLenteId = camarasDisponibles[indiceCamaraActual].id;
                        document.getElementById('listaCamaras').value = nuevoLenteId;
                        cambiarCamaraSeleccionada(nuevoLenteId);
                    }
                }

                function encenderLente(configuracion) {
                    
                    
                    
                    html5QrcodeScanner.start( 
                        configuracion, 
                        { fps: 10, qrbox: { width: 250, height: 120 } }, 
                        onScanSuccess, 
                        (e) => {} 
                    )
                    .catch(err => { 
                        document.getElementById('btnCamara').style.display = 'block'; 
                        console.error("Error prendiendo cámara:", err);
                    });
                }

                function manejarEnterTeclado(e) { if (e.key === 'Enter') e.target.blur(); buscarVariantes(e.target.value); }

                function buscarVariantes(sku) {
                    clearTimeout(scannerSearchTimeout);
                    const container = document.getElementById('resultadosBusquedaVincular');
                    const btnGuardar = document.getElementById('btnGuardarVinculo');
                    
                    if(sku.length < 2) { 
                        container.innerHTML = '<div class="text-center text-muted p-3 small">Buscá un SKU para ver sus variantes...</div>'; 
                        varianteSeleccionada = null; btnGuardar.disabled = true; return; 
                    }
                    container.innerHTML = '<div class="text-center text-muted p-3 small"><i class="fas fa-spinner fa-spin me-2"></i>Buscando en inventario...</div>';

                    scannerSearchTimeout = setTimeout(() => {
                        fetch('/api/buscar_articulo?q=' + encodeURIComponent(sku)).then(res => res.json()).then(data => {
                            varianteSeleccionada = null; btnGuardar.disabled = true;
                            if(data.length === 0) { container.innerHTML = `<div class="alert alert-light border border-danger text-center p-3 mt-2"><i class="fas fa-exclamation-triangle text-danger fs-4 mb-2 d-block"></i><strong class="text-danger">Artículo no encontrado</strong></div>`; return; }
                            
                            let html = '';
                            data.forEach((fam, index) => {
                                let varHtml = '';
                                fam.variantes.forEach(v => {
                                    let talleSeguro = v.talle || v.m_talle || "";
                                    let colorSeguro = v.color || v.m_color || "";
                                    let skuSeguro = fam.sku || fam.m_sku || "";
                                    
                                    const codigosArr = v.codigos || v.m_codigoBarras || v.barras || []; 
                                    const bCod = codigosArr.length > 0 ? `<span class="badge bg-warning text-dark mt-1" style="font-size: 0.7em;">Tiene ${codigosArr.length} código(s)</span>` : '';
                                    const codigosStr = encodeURIComponent(JSON.stringify(codigosArr));
                                    
                                    varHtml += `
                                        <button class="btn btn-outline-primary variant-selector w-100 text-start mb-2 p-2" onclick="seleccionarPildora(this, '${skuSeguro}', '${talleSeguro}', '${colorSeguro}', '${codigosStr}')" style="border-radius: 10px;">
                                            <div class="d-flex justify-content-between align-items-center">
                                                <div><div class="fw-bold fs-6">Talle: ${talleSeguro} | Color: ${colorSeguro}</div><div class="small text-muted">Stock: ${v.stock} un.</div>${bCod}</div>
                                                <i class="fas fa-chevron-right text-muted" style="font-size: 0.8rem;"></i>
                                            </div>
                                        </button>`;
                                });
                                html += `
                                <div class="p-3 mb-3 rounded shadow-sm border" style="background-color: var(--mali-card-bg);">
                                    <div class="d-flex justify-content-between align-items-center" data-bs-toggle="collapse" data-bs-target="#collapseFam${index}" style="cursor: pointer;">
                                        <div><span class="fw-bold d-block text-primary fs-5">${fam.sku || fam.m_sku}</span><small class="text-muted">${fam.desc || fam.m_nombre}</small></div>
                                        <div class="text-end"><span class="badge bg-secondary px-2 py-1 fs-6 mb-1 d-block">${fam.marca || fam.m_marca}</span><i class="fas fa-chevron-down text-muted small"></i></div>
                                    </div>
                                    <div class="collapse" id="collapseFam${index}"><div class="d-flex flex-column border-top pt-3 mt-3">${varHtml}</div></div>
                                </div>`;
                            });
                            container.innerHTML = html;
                        });
                    }, 400);
                }

                function seleccionarPildora(btnElement, sku, talle, color, codigosEncoded) {
                    let inputBuscador = document.getElementById('inputBuscarSKU');
                    if (inputBuscador) {
                        inputBuscador.blur();
                        inputBuscador.readOnly = true; 
                        setTimeout(() => { inputBuscador.readOnly = false; }, 500);
                    }
                    if (document.activeElement) document.activeElement.blur();

                    document.querySelectorAll('.variant-selector').forEach(btn => { btn.classList.remove('variant-selected', 'btn-primary', 'text-white'); btn.classList.add('btn-outline-primary'); const icon = btn.querySelector('.fa-check-circle'); if (icon) icon.classList.replace('fa-check-circle', 'fa-chevron-right'); });
                    btnElement.classList.remove('btn-outline-primary'); btnElement.classList.add('variant-selected', 'btn-primary', 'text-white');
                    const icon = btnElement.querySelector('.fa-chevron-right'); if(icon) icon.classList.replace('fa-chevron-right', 'fa-check-circle');

                    const codigosArraySafe = JSON.parse(decodeURIComponent(codigosEncoded));
                    varianteSeleccionada = { sku: sku, talle: talle, color: color, codigos: codigosArraySafe };
                    
                    tieneCodigosPrevios = codigosArraySafe.length > 0;
                    window.decidioOpcion = false;

                    const btnGuardar = document.getElementById('btnGuardarVinculo'); 
                    btnGuardar.disabled = false;
                    btnGuardar.innerHTML = 'Vincular a este Artículo'; 
                    btnGuardar.className = 'btn btn-danger w-50 rounded-pill fw-bold';
                }

                function cerrarModal() { 
                    const modalEl = document.getElementById('modalVincular');
                    const modalInst = bootstrap.Modal.getInstance(modalEl);
                    if (modalInst) modalInst.hide();
                    setTimeout(() => { pauseScan = false; }, 1000); 
                }

                function guardarVinculo() {
                    let inputBuscador = document.getElementById('inputBuscarSKU');
                    if (inputBuscador) {
                        inputBuscador.blur();
                        inputBuscador.readOnly = true; 
                        setTimeout(() => { inputBuscador.readOnly = false; }, 800);
                    }
                    if (document.activeElement) document.activeElement.blur();

                    if (!varianteSeleccionada) { showToast("Elegí una variante", true); return; }
                    
                    const arr = (varianteSeleccionada.codigos || []).map(String);
                    const pendingStr = String(pendingBarcode);

                    if (arr.includes(pendingStr)) {
                        alert("¡ATENCIÓN!\n\nEste código exacto ya está vinculado a esta misma variante.\nNo podés ni necesitás añadirlo de nuevo.");
                        return;
                    }

                    if (tieneCodigosPrevios && !window.decidioOpcion) {
                        const modalEl = document.getElementById('modalOpcionesVinculo');
                        let modal = bootstrap.Modal.getInstance(modalEl) || new bootstrap.Modal(modalEl);
                        modal.show();
                        return; 
                    }

                    ejecutarVinculoAPI(false, false);
                }

                function seleccionarOpcion(reemplazar) {
                    window.decidioOpcion = true; 
                    const modalInst = bootstrap.Modal.getInstance(document.getElementById('modalOpcionesVinculo'));
                    if (modalInst) modalInst.hide();
                    
                    setTimeout(() => { ejecutarVinculoAPI(false, reemplazar); }, 400);
                }

                function ejecutarVinculoAPI(forceMove, replaceTarget) {
                    const btn = document.getElementById('btnGuardarVinculo');
                    const textoOriginal = btn.innerHTML; const claseOriginal = btn.className;
                    btn.innerHTML = "<i class='fas fa-spinner fa-spin'></i> Guardando..."; btn.disabled = true;

                    fetch('/api/link_barcode', {
                        method: 'POST',
                        headers: {'Content-Type': 'application/json'},
                        body: JSON.stringify({ 
                            barcode: pendingBarcode, 
                            sku: varianteSeleccionada.sku, 
                            talle: varianteSeleccionada.talle, 
                            color: varianteSeleccionada.color,
                            force_move: forceMove,
                            replace_target: replaceTarget
                        })
                    })
                    .then(async res => {
                        if (!res.ok) throw new Error(res.status.toString()); 
                        return res.json();
                    })
                    .then(data => {
                        if (data.status === 'ok') {
                            btn.className = 'btn btn-success w-50 rounded-pill fw-bold text-white';
                            btn.innerHTML = "<i class='fas fa-check'></i> ¡Guardado!";
                            showToast("<i class='fas fa-check-circle me-2'></i> Vínculo actualizado con éxito");
                            setTimeout(() => { cerrarModal(); }, 1200); 
                        } 
                        else if (data.status === 'confirm_move') {
                            btn.innerHTML = textoOriginal; btn.className = claseOriginal; btn.disabled = false;
                            maliConfirmLocal(data.msg, () => { ejecutarVinculoAPI(true, replaceTarget); });
                        } 
                        else {
                            btn.innerHTML = textoOriginal; btn.className = claseOriginal; btn.disabled = false;
                            showToast("<i class='fas fa-times-circle me-2'></i> " + (data.msg || "Error interno"), true);
                        }
                    })
                    .catch(err => {
                        btn.innerHTML = textoOriginal; btn.className = claseOriginal; btn.disabled = false;
                        showToast("<i class='fas fa-wifi me-2'></i> Error de conexión", true);
                    });
                }
                
                window.onload = () => { renderCart(); iniciarCamaras(); };
            </script>
        )HTML";

        html += WebTemplates::getFooter("scanner");
        res.set_content(html, "text/html");
    }
}