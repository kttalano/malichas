#include "web_routes.hpp"
#include "web_utils.hpp"
#include "web_templates.hpp"
#include "web_auth.hpp"
#include <mutex>
#include <string>
#include <algorithm>
#include <fstream>
#include <cstdio>
#include <ctime>
#include <cstdlib>

extern std::recursive_mutex mutexTienda;

namespace WebRoutes
{

    static auto normalizarAtributo = [](std::string s)
    {
        std::string out = s;
        std::transform(out.begin(), out.end(), out.begin(), ::tolower);
        size_t start = out.find_first_not_of(" \t\r\n");
        if (start == std::string::npos)
            return std::string("");
        out.erase(0, start);
        out.erase(out.find_last_not_of(" \t\r\n") + 1);
        if (out == "undefined" || out == "null" || out == "nan" || out == "unico" || out == "único" || out == "-")
            return std::string("");
        return out;
    };

    void handleApiAplicarCambiosEmpretienda(const httplib::Request &req, httplib::Response &res, Store *tienda)
    {
        std::string userEmail = req.get_header_value("Cf-Access-Authenticated-User-Email");
        Rol usuarioRol = WebAuth::obtenerRolPorEmail(userEmail);
        std::string miNombre = WebAuth::obtenerNombreVendedora(userEmail);
        if (miNombre.empty())
            miNombre = "Administrador";

        if (usuarioRol != Rol::Owner && usuarioRol != Rol::Admin && usuarioRol != Rol::Supervisor)
        {
            res.set_content("{\"status\":\"error\", \"msg\":\"No tenés permisos para sincronizar.\"}", "application/json");
            return;
        }

        try
        {
            json payload = json::parse(req.body);

            if (payload.is_object() && payload.contains("action") && payload["action"] == "confirm")
            {
                tienda->dataVersion++;
                tienda->saveToFileAsync();
                res.set_content("{\"status\":\"ok\"}", "application/json");
                return;
            }

            if (payload.is_array())
            {
                time_t now = time(0);
                tm *ltm = localtime(&now);
                char buffer[20];
                strftime(buffer, sizeof(buffer), "%d/%m/%Y", ltm);
                std::string fechaHoy(buffer);

                std::lock_guard<std::recursive_mutex> lock(mutexTienda);
                for (auto &item : payload)
                {
                    std::string sku = item["sku"].get<std::string>();
                    std::string marca = item.contains("marca") ? item["marca"].get<std::string>() : "";

                    for (auto &fam : tienda->inventory)
                    {
                        if (fam.m_sku == sku && (marca.empty() || fam.m_marca.m_name == marca))
                        {
                            if (item.contains("nuevo_nombre"))
                                fam.m_nombre = item["nuevo_nombre"].get<std::string>();
                            if (item.contains("nuevo_categoria"))
                                fam.m_categoria.m_name = item["nuevo_categoria"].get<std::string>();

                            if (item.contains("variantes"))
                            {
                                for (auto &varPayload : item["variantes"])
                                {
                                    std::string tOld = normalizarAtributo(varPayload["t_old"].get<std::string>());
                                    std::string cOld = normalizarAtributo(varPayload["c_old"].get<std::string>());

                                    for (auto &var : fam.m_variantes)
                                    {
                                        std::string vT = normalizarAtributo(var.m_talle);
                                        std::string vC = normalizarAtributo(var.m_color);

                                        if (vT == tOld && vC == cOld)
                                        {
                                            std::string tClean = varPayload["t_new"].get<std::string>();
                                            std::string cClean = varPayload["c_new"].get<std::string>();
                                            int sNew = varPayload["s_new"].get<int>();
                                            int diff = sNew - var.m_stock;

                                            if (vT != normalizarAtributo(tClean) || vC != normalizarAtributo(cClean) || var.m_stock != sNew)
                                            {
                                                var.m_stock = sNew;
                                                if (diff != 0)
                                                {
                                                    std::string desc = fam.m_nombre + " [" + tClean + "/" + cClean + "]";
                                                    Movement mov(fechaHoy, miNombre, sku, desc, diff, "Ajuste Sincro Web");
                                                    tienda->movements.push_back(mov);
                                                }
                                            }
                                            break;
                                        }
                                    }
                                }
                            }
                            break;
                        }
                    }
                }
            }
            res.set_content("{\"status\":\"ok\"}", "application/json");
        }
        catch (...)
        {
            res.status = 400;
            res.set_content("{\"status\":\"error\"}", "application/json");
        }
    }

    void handleApiRevertirCambiosEmpretienda(const httplib::Request &req, httplib::Response &res, Store *tienda)
    {
        std::string userEmail = req.get_header_value("Cf-Access-Authenticated-User-Email");
        Rol usuarioRol = WebAuth::obtenerRolPorEmail(userEmail);
        std::string miNombre = WebAuth::obtenerNombreVendedora(userEmail);
        if (miNombre.empty())
            miNombre = "Administrador";

        if (usuarioRol != Rol::Owner && usuarioRol != Rol::Admin && usuarioRol != Rol::Supervisor)
        {
            res.set_content("{\"status\":\"error\", \"msg\":\"No tenés permisos para revertir.\"}", "application/json");
            return;
        }

        try
        {
            json payload = json::parse(req.body);
            bool modificado = false;

            time_t now = time(0);
            tm *ltm = localtime(&now);
            char buffer[20];
            strftime(buffer, sizeof(buffer), "%d/%m/%Y", ltm);
            std::string fechaHoy(buffer);

            std::lock_guard<std::recursive_mutex> lock(mutexTienda);
            for (auto &item : payload)
            {
                std::string sku = item["sku"].get<std::string>();
                std::string marca = item.contains("marca") ? item["marca"].get<std::string>() : "";

                for (auto &fam : tienda->inventory)
                {
                    if (fam.m_sku == sku && (marca.empty() || fam.m_marca.m_name == marca))
                    {
                        if (item.contains("viejo_nombre"))
                        {
                            fam.m_nombre = item["viejo_nombre"].get<std::string>();
                            modificado = true;
                        }
                        if (item.contains("vieja_categoria"))
                        {
                            fam.m_categoria.m_name = item["vieja_categoria"].get<std::string>();
                            modificado = true;
                        }

                        if (item.contains("variantes"))
                        {
                            for (auto &varPayload : item["variantes"])
                            {
                                std::string tNew = normalizarAtributo(varPayload["t_new"].get<std::string>());
                                std::string cNew = normalizarAtributo(varPayload["c_new"].get<std::string>());

                                for (auto &var : fam.m_variantes)
                                {
                                    std::string vT = normalizarAtributo(var.m_talle);
                                    std::string vC = normalizarAtributo(var.m_color);

                                    if (vT == tNew && vC == cNew)
                                    {
                                        int sOld = varPayload["s_old"].get<int>();
                                        int diff = sOld - var.m_stock;

                                        if (var.m_stock != sOld)
                                        {
                                            var.m_stock = sOld;
                                            if (diff != 0)
                                            {
                                                std::string desc = fam.m_nombre + " [" + tNew + "/" + cNew + "]";
                                                Movement mov(fechaHoy, miNombre, sku, desc, diff, "Reversión Sincro Web");
                                                tienda->movements.push_back(mov);
                                            }
                                            modificado = true;
                                        }
                                        break;
                                    }
                                }
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
            }
            res.set_content("{\"status\":\"ok\"}", "application/json");
        }
        catch (...)
        {
            res.status = 400;
            res.set_content("{\"status\":\"error\"}", "application/json");
        }
    }

    void handleSincronizarEmpretienda(const httplib::Request &req, httplib::Response &res, Store *tienda)
    {
        std::string html = WebTemplates::getHeadAndNav("Sync Empretienda");

        html += R"HTML(
            <script src="https:

            <style>
                .stage-container { display: none; transition: opacity 0.3s ease; }
                .stage-active { display: block; opacity: 1; }
                .table-edit input { border: 1px solid rgba(128, 128, 128, 0.3) !important; padding: 6px; border-radius: 6px; width: 100%; font-size: 0.85rem; background-color: transparent !important; color: inherit !important; }
                .table-edit input:focus { border-color: var(--mali-primary) !important; outline: none; box-shadow: 0 0 5px rgba(0,0,0,0.2);}
                .table-edit input.inp-locked { background-color: rgba(128,128,128,0.1) !important; opacity: 0.7; pointer-events: none; font-weight: bold; border:none !important; }
                .chk-variant { transform: scale(1.3); cursor: pointer; }
                
               
                #missingItemsList .card {
                    background-color: rgba(255, 255, 255, 0.03) !important;
                    border: 1px solid rgba(255, 255, 255, 0.1) !important;
                }
                #missingItemsList .card-header {
                    background-color: rgba(255, 255, 255, 0.06) !important;
                    color: var(--mali-text) !important;
                    border-bottom: 1px solid rgba(255, 255, 255, 0.1) !important;
                }
                #missingItemsList .list-group-item {
                    background-color: transparent !important;
                    color: var(--mali-text) !important;
                    border-color: rgba(255, 255, 255, 0.1) !important;
                }
            </style>

            <h2 class='page-title mb-4'>Sincronización Inteligente Empretienda</h2>

            <!-- ETAPA 0: RECUPERACIÓN -->
            <div id="stage0" class="stage-container card-glass p-5 text-center">
                <i class='fas fa-history fs-1 text-warning mb-3'></i>
                <h4 class='fw-bold'>Sincronización Pendiente</h4>
                <p class="text-muted mb-4">Guardaste cambios en tu BD pero no confirmaste si el Excel funcionó bien en Empretienda.</p>
                <button onclick='confirmarExito()' class='btn btn-success fw-bold px-4 me-2 mb-2'><i class="fas fa-check-double me-2"></i>Todo fue un éxito en Empretienda</button>
                <button onclick='revertirCambios()' class='btn btn-outline-danger fw-bold px-4 mb-2'><i class="fas fa-undo me-2"></i>Hubo errores, deshacer cambios en mi BD</button>
            </div>

            <!-- ETAPA 1: SUBIDA -->
            <div id="stage1" class="stage-container card-glass p-5 text-center">
                <i class='fas fa-file-excel fs-1 text-success mb-3'></i>
                <h4 class='fw-bold'>1. Configuración y Archivo</h4>
                
                <div class="row justify-content-center mb-4 mt-3">
                    <div class="col-md-4">
                        <label class="fw-bold text-muted mb-2">Porcentaje de Remarcación (%)</label>
                        <div class="input-group">
                            <span class="input-group-text"><i class="fas fa-percentage"></i></span>
                            <input type="number" id="margenGanancia" class="form-control fw-bold text-center fs-5" value="15">
                        </div>
                    </div>
                </div>

                <input type='file' id='fileInput' accept='.csv, .xlsx' class='form-control form-control-lg mb-4' style="max-width: 500px; margin: 0 auto;">
                
                <div class="d-flex justify-content-center gap-3">
                    <button id='btnAnalizar' onclick='iniciarAnalisis()' class='btn rounded-pill fw-bold text-white shadow-sm px-4 py-2' style='background-color: var(--mali-primary);'>
                        <i class='fas fa-list-check me-2'></i> Analizar Faltantes (Paso a Paso)
                    </button>
                    <button id='btnRapido' onclick='iniciarSincroRapida()' class='btn btn-outline-success rounded-pill fw-bold px-4 py-2'>
                        <i class='fas fa-bolt me-2'></i> Sincronización Rápida (Ignorar faltantes)
                    </button>
                </div>
            </div>

            <!-- ETAPA 2: SELECCIÓN -->
            <div id="stage2" class="stage-container card-glass p-4">
                <h4 class='fw-bold mb-1 text-primary'><i class='fas fa-tasks me-2'></i>2. Artículos Faltantes en Tienda</h4>
                <p class="text-muted small mb-4">Estos artículos existen en tu servidor pero no en el Excel.</p>
                <div class="d-flex justify-content-between mb-3">
                    <div>
                        <button class="btn btn-sm btn-outline-secondary me-2" onclick="toggleAllChecks(true)">Seleccionar Todo</button>
                        <button class="btn btn-sm btn-outline-secondary" onclick="toggleAllChecks(false)">Ninguno</button>
                    </div>
                    <button class='btn btn-success fw-bold' onclick='avanzarAEtapa3()'>Continuar <i class="fas fa-arrow-right ms-2"></i></button>
                </div>
                <div id="missingItemsList" class="mb-3"></div>
            </div>

            <!-- ETAPA 3: EDICIÓN -->
            <div id="stage3" class="stage-container card-glass p-4">
                <h4 class='fw-bold mb-1 text-primary'><i class='fas fa-edit me-2'></i>3. Edición de Faltantes</h4>
                <p class="text-muted small mb-4">Si algún artículo de la lista tiene un error y no querés subirlo, tocá la X roja para borrarlo de esta lista.</p>
                
                <div class="table-responsive">
                    <table class="table table-bordered align-middle table-edit" id="editTable">
                        <thead class="text-center small">
                            <tr>
                                <th>Nombre (Max 80)</th>
                                <th>SKU</th>
                                <th>Color</th>
                                <th>Talle</th>
                                <th>Stock</th>
                                <th>Categoría</th>
                                <th>Eliminar</th>
                            </tr>
                        </thead>
                        <tbody id="editTableBody"></tbody>
                    </table>
                </div>

                <div class="text-end mt-4">
                    <button class='btn btn-outline-danger fw-bold me-2' onclick='volverAEtapa2()'>Volver</button>
                    <button id='btnGuardarBd' class='btn btn-primary fw-bold px-5' onclick='avanzarAEtapa4()'><i class="fas fa-save me-2"></i>Guardar BD e Iniciar</button>
                </div>
            </div>

            <!-- ETAPA 4: DESCARGAS -->
            <div id="stage4" class="stage-container card-glass p-5 text-center">
                <div id="loadingZone">
                    <i class="fas fa-spinner fa-spin fs-1 text-primary mb-3"></i>
                    <h4 class='fw-bold'>Procesando Excel e inyectando datos...</h4>
                    <p class="text-muted">Por favor, espera unos segundos.</p>
                </div>
                
                <div id="successZone" style="display:none;">
                    <i class='fas fa-check-circle fs-1 text-success mb-3'></i>
                    <h4 class='fw-bold'>¡Proceso Terminado!</h4>
                    <p class="text-muted mb-4">Descargá tus archivos. Subí el Excel a Empretienda y revisá el Reporte para ver qué cambió.</p>
                    
                    <div class="d-flex justify-content-center gap-3 mb-4">
                        <a id="linkExcel" href="#" class='btn btn-lg rounded-pill fw-bold shadow-sm px-4 py-2' style='background-color: var(--mali-primary); color: white;'>
                            <i class='fas fa-file-excel me-2'></i> Descargar Excel
                        </a>
                        <a id="linkHtml" href="#" target="_blank" class='btn btn-lg btn-outline-secondary rounded-pill fw-bold shadow-sm px-4 py-2'>
                            <i class='fas fa-file-code me-2'></i> Ver Reporte
                        </a>
                    </div>
                    <hr>
                    
                    <button onclick='confirmarExito()' class='btn btn-success fw-bold px-4 me-2 mb-2'>
                        <i class='fas fa-check-double me-2'></i> Finalizar Proceso Correctamente
                    </button>
                    <button id="btnRevertirStage4" onclick='revertirCambios()' class='btn btn-outline-danger fw-bold px-4 mb-2'>
                        <i class='fas fa-undo me-1'></i> Revertir BD (Empretienda falló)
                    </button>
                </div>
                <div id='errorLog' class='alert alert-danger mt-3 text-start' style='display: none;'></div>
            </div>

            <script>
                let globalWorkbook = null;
                let globalWorksheetName = "";
                let globalExcelRows = [];
                let globalExactHeaders = []; 
                let globalCatalogo = [];
                let missingVariantsData = []; 
                let finalRowsToInject = [];

                (function initEmpretienda() {
                    if(localStorage.getItem('mali_empretienda_sync')) showStage(0);
                    else showStage(1);
                })();

                function showStage(stageNum) {
                    document.querySelectorAll('.stage-container').forEach(el => el.classList.remove('stage-active'));
                    document.getElementById('stage' + stageNum).classList.add('stage-active');
                }

                function getNormalizedFamilyId(str1, str2 = "") {
                    let combined = (str1 + " " + str2).toLowerCase().replace(/[^a-z0-9\s]/g, "");
                    let words = combined.split(/\s+/).filter(w => w.length > 0);
                    words.sort();
                    return words.join("_");
                }

                function getSafeString(val) {
                    if(val === null || val === undefined) return "";
                    return String(val).trim();
                }

                function sanitizeAttrJS(val) {
                    if(!val) return "";
                    let v = String(val).trim().toLowerCase();
                    if(['nan','none','null','undefined','unico','único', '-'].includes(v)) return "";
                    return v.normalize("NFD").replace(/[\u0300-\u036f]/g, "");
                }

                async function iniciarSincroRapida() {
                    const fileInput = document.getElementById('fileInput');
                    if (!fileInput.files.length) { alert('Selecciona el Excel primero.'); return; }
                    
                    const btn = document.getElementById('btnRapido');
                    btn.innerHTML = "<i class='fas fa-spinner fa-spin me-2'></i> Iniciando...";
                    btn.disabled = true;

                    finalRowsToInject = []; 
                    localStorage.setItem('mali_empretienda_sync_fast', 'true');
                    
                    showStage(4);
                    ejecutarPythonYDescargar(fileInput.files[0]);
                }

                async function iniciarAnalisis() {
                    const fileInput = document.getElementById('fileInput');
                    if (!fileInput.files.length) { alert('Selecciona el Excel primero.'); return; }
                    
                    const btn = document.getElementById('btnAnalizar');
                    btn.innerHTML = "<i class='fas fa-spinner fa-spin me-2'></i> Leyendo...";
                    btn.disabled = true;

                    try {
                        const resCat = await fetch('/api/catalogo_completo');
                        globalCatalogo = await resCat.json();

                        const file = fileInput.files[0];
                        const reader = new FileReader();
                        reader.onload = function(e) {
                            const data = new Uint8Array(e.target.result);
                            globalWorkbook = XLSX.read(data, {type: 'array'});
                            globalWorksheetName = globalWorkbook.SheetNames[0];
                            
                            const ws = globalWorkbook.Sheets[globalWorksheetName];
                            const range = XLSX.utils.decode_range(ws['!ref']);
                            globalExactHeaders = [];
                            for(let C = range.s.c; C <= range.e.c; ++C) {
                                let cell = ws[XLSX.utils.encode_cell({c:C, r:range.s.r})];
                                globalExactHeaders.push(cell ? cell.v : "");
                            }

                            globalExcelRows = XLSX.utils.sheet_to_json(ws, {defval: ""});
                            procesarFaltantes();
                            
                            btn.innerHTML = "<i class='fas fa-list-check me-2'></i> Analizar Faltantes (Paso a Paso)";
                            btn.disabled = false;
                        };
                        reader.readAsArrayBuffer(file);
                    } catch(err) {
                        alert("Error: " + err);
                        btn.disabled = false;
                    }
                }

                function procesarFaltantes() {
                    let excelSkusMap = new Map();
                    let excelSkusVariadosMap = new Map();

                    
                    globalExcelRows.forEach(row => {
                        let rawSku = getSafeString(row["SKU"]).toLowerCase();
                        let familyId = "";
                        if(rawSku) {
                            familyId = getNormalizedFamilyId(rawSku);
                        } else {
                            let rName = row["Nombre"] || "";
                            if (rName.startsWith("[")) {
                                let closeIdx = rName.indexOf("]");
                                if (closeIdx !== -1) familyId = getNormalizedFamilyId(rName.substring(1, closeIdx));
                            }
                        }
                        
                        let nom1 = getSafeString(row["Nombre atributo 1"]).toLowerCase();
                        let val1 = sanitizeAttrJS(row["Valor atributo 1"]);
                        let nom2 = getSafeString(row["Nombre atributo 2"]).toLowerCase();
                        let val2 = sanitizeAttrJS(row["Valor atributo 2"]);
                        
                        let c = "", t = "";
                        if(nom1.includes("color")) c = val1;
                        if(nom2.includes("color")) c = val2;
                        if(nom1.includes("talle")) t = val1;
                        if(nom2.includes("talle")) t = val2;
                        if(!nom1.includes("color") && !nom1.includes("talle") && nom1) {
                            if(!t) t = val1; else if(!c) c = val1;
                        }

                        if(familyId) {
                            if(!excelSkusMap.has(familyId)) excelSkusMap.set(familyId, new Set());
                            excelSkusMap.get(familyId).add(`${t}|${c}`);
                        }
                        
                        
                        if(!excelSkusVariadosMap.has(rawSku)) excelSkusVariadosMap.set(rawSku, new Set());
                        excelSkusVariadosMap.get(rawSku).add(`${t}|${c}`);
                    });

                    missingVariantsData = [];
                    globalCatalogo.forEach(fam => {
                        let skuLower = getSafeString(fam.sku).toLowerCase();
                        let marcaLower = getSafeString(fam.marca).toLowerCase();
                        
                        let familyId = getNormalizedFamilyId(fam.sku, fam.marca);
                        let existsInExcel = excelSkusMap.has(familyId) || excelSkusVariadosMap.has(skuLower) || excelSkusVariadosMap.has(marcaLower + " " + skuLower);
                        
                        let variantsSet = new Set();
                        if (excelSkusMap.has(familyId)) variantsSet = new Set([...variantsSet, ...excelSkusMap.get(familyId)]);
                        if (excelSkusVariadosMap.has(skuLower)) variantsSet = new Set([...variantsSet, ...excelSkusVariadosMap.get(skuLower)]);
                        if (excelSkusVariadosMap.has(marcaLower + " " + skuLower)) variantsSet = new Set([...variantsSet, ...excelSkusVariadosMap.get(marcaLower + " " + skuLower)]);

                        fam.variantes.forEach(varObj => {
                            let vt = sanitizeAttrJS(varObj.talle);
                            let vc = sanitizeAttrJS(varObj.color);
                            
                            if(!variantsSet.has(`${vt}|${vc}`)) {
                                missingVariantsData.push({ fam: fam, variante: varObj, uid: familyId + "_" + vt + "_" + vc, familyId: familyId, isNewProduct: !existsInExcel });
                            }
                        });
                    });

                    renderStage2();
                    showStage(2);
                }

                function renderStage2() {
                    const container = document.getElementById("missingItemsList");
                    container.innerHTML = "";
                    if(missingVariantsData.length === 0) {
                        container.innerHTML = "<div class='alert alert-success text-center fw-bold'>¡Tu Excel está completo! No hay faltantes.</div>";
                        return;
                    }

                    let groups = {};
                    missingVariantsData.forEach(item => {
                        if(!groups[item.familyId]) groups[item.familyId] = { fam: item.fam, items: [] };
                        groups[item.familyId].items.push(item);
                    });

                    let html = "";
                    for(let fId in groups) {
                        let g = groups[fId];
                        let isNew = g.items[0].isNewProduct;
                        let badge = isNew ? "<span class='badge bg-primary ms-2'>Producto Nuevo</span>" : "<span class='badge bg-info ms-2 text-dark'>Variante Nueva</span>";
                        
                        html += `
                        <div class="card mb-3 shadow-sm border-0 bg-transparent">
                            <div class="card-header fw-bold sku-header d-flex justify-content-between align-items-center" style="color: var(--mali-text);">
                                <div>[${g.fam.marca} ${g.fam.sku}] ${g.fam.nombre} ${badge}</div>
                            </div>
                            <ul class="list-group list-group-flush list-group-glass">`;
                        
                        g.items.forEach(item => {
                            let vt = getSafeString(item.variante.talle) || '-';
                            let vc = getSafeString(item.variante.color) || '-';
                            html += `
                                <li class="list-group-item d-flex justify-content-between align-items-center variant-row" style="color: var(--mali-text);">
                                    <div>
                                        <input type="checkbox" class="form-check-input me-3 chk-variant" value="${item.uid}" checked>
                                        <strong style="color: var(--mali-primary);">T:</strong> ${vt} | <strong style="color: var(--mali-primary);">C:</strong> ${vc} 
                                        <span class="text-muted ms-3">Stock: <b class="text-success">${item.variante.stock}</b></span>
                                    </div>
                                </li>`;
                        });
                        html += `</ul></div>`;
                    }
                    container.innerHTML = html;
                }

                function toggleAllChecks(state) { document.querySelectorAll('.chk-variant').forEach(cb => cb.checked = state); }

                function avanzarAEtapa3() {
                    const checkedUids = Array.from(document.querySelectorAll('.chk-variant:checked')).map(cb => cb.value);
                    if(checkedUids.length === 0) { 
                        finalRowsToInject = [];
                        localStorage.setItem('mali_empretienda_sync_fast', 'true'); 
                        showStage(4);
                        ejecutarPythonYDescargar(document.getElementById('fileInput').files[0]);
                        return; 
                    }
                    
                    const tbody = document.getElementById("editTableBody");
                    tbody.innerHTML = "";
                    
                    let selectedItems = missingVariantsData.filter(item => checkedUids.includes(item.uid));
                    
                    selectedItems.forEach(item => {
                        let f = item.fam;
                        let v = item.variante;
                        let prefijo = `[${f.marca} ${f.sku}] `;
                        let maxLen = 80 - prefijo.length;
                        let nomVal = getSafeString(f.nombre).substring(0, maxLen);
                        let skuVal = `${f.sku} ${f.marca}`;

                        let origC = getSafeString(v.color);
                        let origT = getSafeString(v.talle);

                        tbody.innerHTML += `
                            <tr data-uid="${item.uid}" data-stock="${v.stock}" data-skureal="${f.sku}" data-marca="${f.marca}" data-familyid="${item.familyId}" data-told="${origT}" data-cold="${origC}" data-catold="${f.categoria || ''}">
                                <td>
                                    <div class="input-group input-group-sm">
                                        <span class="input-group-text bg-dark text-light border-secondary fw-bold" style="font-size:0.75rem;">${prefijo}</span>
                                        <input type="text" class="form-control inp-nombre-limpio" value="${nomVal}" maxlength="${maxLen}">
                                    </div>
                                </td>
                                <td><input type="text" class="form-control form-control-sm inp-locked text-center" value="${skuVal}" readonly tabindex="-1"></td>
                                <td><input type="text" class="form-control form-control-sm inp-locked inp-color text-center" value="${origC}" readonly></td>
                                <td><input type="text" class="form-control form-control-sm inp-locked inp-talle text-center" value="${origT}" readonly></td>
                                <td><input type="number" class="form-control form-control-sm inp-stock text-center fw-bold text-primary" value="${v.stock}"></td>
                                <td><input type="text" class="form-control form-control-sm inp-cat text-center" value="${f.categoria || ''}"></td>
                                <td class="text-center"><button class="btn btn-sm btn-outline-danger" onclick="this.closest('tr').remove()"><i class="fas fa-times"></i></button></td>
                            </tr>
                        `;
                    });
                    showStage(3);
                }

                function volverAEtapa2() { showStage(2); }

                async function avanzarAEtapa4() {
                    const btn = document.getElementById('btnGuardarBd');
                    btn.innerHTML = "<i class='fas fa-spinner fa-spin me-2'></i> Guardando...";
                    btn.disabled = true;

                    const rows = document.querySelectorAll("#editTableBody tr");
                    finalRowsToInject = [];
                    let cambiosMap = {}; 

                    for(let tr of rows) {
                        let skuReal = tr.getAttribute('data-skureal');
                        let fMarca = tr.getAttribute('data-marca');
                        let familyId = tr.getAttribute('data-familyid');
                        
                        let colorViejo = tr.getAttribute('data-cold');
                        let talleViejo = tr.getAttribute('data-told');
                        let stockViejo = parseInt(tr.getAttribute('data-stock')) || 0;

                        let stockCargado = parseInt(tr.querySelector('.inp-stock').value) || 0;
                        let catCargada = tr.querySelector('.inp-cat').value.trim();
                        let nombreLimpio = tr.querySelector('.inp-nombre-limpio').value.trim();
                        
                        let nombreFinal = `[${fMarca} ${skuReal}] ${nombreLimpio}`;
                        
                        if (!cambiosMap[familyId]) {
                            cambiosMap[familyId] = { 
                                sku: skuReal, marca: fMarca, variantes: [],
                                viejo_nombre: missingVariantsData.find(x => x.familyId === familyId).fam.nombre,
                                vieja_categoria: tr.getAttribute('data-catold'),
                                nuevo_nombre: nombreLimpio, nuevo_categoria: catCargada
                            };
                        }
                        
                        cambiosMap[familyId].variantes.push({
                            t_old: talleViejo, c_old: colorViejo, s_old: stockViejo,
                            t_new: talleViejo, c_new: colorViejo, s_new: stockCargado
                        });

                        let tempDict = {
                            "Nombre": nombreFinal, "Stock": stockCargado,
                            "SKU": tr.querySelectorAll('td')[1].querySelector('input').value,
                            "Categorías": catCargada,
                            "Mostrar en tienda": stockCargado > 0 ? "Si" : "No",
                            "__NEW__": "True"
                        };

                        let colIdx = 1;
                        if(colorViejo) { tempDict[`Nombre atributo ${colIdx}`] = "Color"; tempDict[`Valor atributo ${colIdx}`] = colorViejo; colIdx++; }
                        if(talleViejo) { tempDict[`Nombre atributo ${colIdx}`] = "Talle"; tempDict[`Valor atributo ${colIdx}`] = talleViejo; }

                        let rowDict = {};
                        globalExactHeaders.forEach(h => { if(h) rowDict[h] = tempDict[h] !== undefined ? tempDict[h] : ""; });
                        if(!rowDict["__NEW__"]) rowDict["__NEW__"] = "True";
                        
                        finalRowsToInject.push(rowDict);
                    }
                    
                    let cambiosParaServidor = Object.values(cambiosMap);
                    
                    try {
                        const res = await fetch('/api/aplicar_cambios_empretienda', {
                            method: 'POST',
                            headers: {'Content-Type': 'application/json'},
                            body: JSON.stringify(cambiosParaServidor)
                        });
                        const data = await res.json();
                        
                        if(data.status === 'ok') {
                            localStorage.setItem('mali_empretienda_sync', JSON.stringify(cambiosParaServidor));
                            localStorage.removeItem('mali_empretienda_sync_fast');
                            showStage(4);
                            ejecutarPythonYDescargar(document.getElementById('fileInput').files[0]);
                        } else {
                            alert("Error al aplicar cambios en el servidor local.");
                            btn.disabled = false;
                            btn.innerHTML = "<i class='fas fa-save me-2'></i>Guardar BD e Iniciar";
                        }
                    } catch(e) {
                        alert("Error de red: " + e);
                        btn.disabled = false;
                        btn.innerHTML = "<i class='fas fa-save me-2'></i>Guardar BD e Iniciar";
                    }
                }

                function ejecutarPythonYDescargar(fileOriginal) {
                    const errDiv = document.getElementById('errorLog');
                    document.getElementById('loadingZone').style.display = 'block';
                    document.getElementById('successZone').style.display = 'none';
                    
                    if(localStorage.getItem('mali_empretienda_sync_fast')) {
                        document.getElementById('btnRevertirStage4').style.display = 'none';
                    }

                    try {
                        let blobToSend = fileOriginal; 

                        if (!localStorage.getItem('mali_empretienda_sync_fast') && globalWorkbook) {
                            const ws = globalWorkbook.Sheets[globalWorksheetName];
                            if(finalRowsToInject.length > 0) {
                                let headersWithNew = [...globalExactHeaders, "__NEW__"];
                                XLSX.utils.sheet_add_json(ws, finalRowsToInject, {skipHeader: true, origin: -1, header: headersWithNew});
                            }
                            const excelBuffer = XLSX.write(globalWorkbook, { bookType: 'xlsx', type: 'array' });
                            blobToSend = new Blob([excelBuffer], {type: "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"});
                        }
                        
                        const margen = document.getElementById('margenGanancia').value || "15";

                        fetch('/api/procesar_empretienda?margin=' + margen, { method: 'POST', body: blobToSend })
                        .then(async response => {
                            const data = await response.json();
                            if (response.ok && data.status === 'ok') {
                                document.getElementById('linkExcel').href = '/api/download_sync?id=' + data.id + '&type=excel';
                                document.getElementById('linkHtml').href = '/api/download_sync?id=' + data.id + '&type=html';
                                
                                document.getElementById('loadingZone').style.display = 'none';
                                document.getElementById('successZone').style.display = 'block';
                            } else {
                                throw new Error(data.msg || 'Error desconocido.');
                            }
                        })
                        .catch(err => {
                            errDiv.style.display = 'block';
                            errDiv.innerHTML = "<strong>Error en servidor:</strong><br>" + err.message;
                            document.getElementById('loadingZone').style.display = 'none';
                        });

                    } catch(e) {
                        errDiv.style.display = 'block';
                        errDiv.innerHTML = "<strong>Error interno:</strong><br>" + e.message;
                        document.getElementById('loadingZone').style.display = 'none';
                    }
                }

                async function confirmarExito() {
                    const btn = event.currentTarget;
                    btn.innerHTML = "<i class='fas fa-spinner fa-spin me-2'></i> Finalizando...";
                    btn.disabled = true;
                    if(!localStorage.getItem('mali_empretienda_sync_fast')) {
                        try {
                            await fetch('/api/aplicar_cambios_empretienda', { method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify({ action: "confirm" }) });
                        } catch(e) {}
                    }
                    localStorage.removeItem('mali_empretienda_sync');
                    localStorage.removeItem('mali_empretienda_sync_fast');
                    location.reload(); 
                }

                async function revertirCambios() {
                    let pendientes = localStorage.getItem('mali_empretienda_sync');
                    if (!pendientes) return;
                    if(!confirm("¿Revertir todos los cambios de stock aplicados en tu servidor local?")) return;
                    try {
                        const res = await fetch('/api/revertir_cambios_empretienda', { method: 'POST', headers: {'Content-Type': 'application/json'}, body: pendientes });
                        const data = await res.json();
                        if(data.status === 'ok') {
                            alert("Reversión completada.");
                            localStorage.removeItem('mali_empretienda_sync');
                            location.reload();
                        } else alert("Error al revertir.");
                    } catch(e) { alert("Error: " + e); }
                }
            </script>
        )HTML";

        html += WebTemplates::getFooter("");
        res.set_content(html, "text/html");
    }

    void handleApiProcesarEmpretienda(const httplib::Request &req, httplib::Response &res, Store *tienda)
    {
        if (req.body.empty())
        {
            res.status = 400;
            res.set_content("{\"status\":\"error\", \"msg\":\"No se recibió archivo.\"}", "application/json");
            return;
        }

        if (system("find . -name 'temp_*' -mmin +60 -delete") == -1)
        {
        }
        if (system("find . -name 'Reporte_Modificaciones_*' -mmin +60 -delete") == -1)
        {
        }

        std::string marginStr = req.has_param("margin") ? req.get_param_value("margin") : "15";

        long long timestamp = std::time(nullptr);
        std::string ts = std::to_string(timestamp) + "_" + std::to_string(rand() % 1000);

        std::string fIn = "temp_in_" + ts + ".xlsx";
        std::string fJson = "temp_inv_" + ts + ".json";
        std::string fOut = "temp_out_" + ts + ".xlsx";
        std::string fLog = "temp_err_" + ts + ".log";
        std::string fRep = "Reporte_Modificaciones_" + ts + ".html";

        std::ofstream ofs(fIn, std::ios::binary);
        ofs << req.body;
        ofs.close();

        json jInv;
        {
            std::lock_guard<std::recursive_mutex> lock(mutexTienda);
            jInv = tienda->inventory;
        }
        std::ofstream ofsJson(fJson);
        ofsJson << jInv.dump();
        ofsJson.close();

        std::string scriptCmd = "python3 sync_empretienda.py " + fIn + " " + fJson + " " + fOut + " " + fRep + " " + marginStr + " 2> " + fLog;
        int result = system(scriptCmd.c_str());

        if (result == 0)
        {
            std::remove(fIn.c_str());
            std::remove(fJson.c_str());
            std::remove(fLog.c_str());
            res.set_content("{\"status\":\"ok\", \"id\":\"" + ts + "\"}", "application/json");
        }
        else
        {
            std::ifstream errorFile(fLog);
            std::string errorMsg = "Error desconocido de Python.";
            if (errorFile.is_open())
            {
                std::string details((std::istreambuf_iterator<char>(errorFile)), (std::istreambuf_iterator<char>()));
                errorFile.close();
                if (!details.empty())
                    errorMsg = details;
            }
            std::remove(fIn.c_str());
            std::remove(fJson.c_str());
            std::remove(fLog.c_str());

            size_t pos = 0;
            while ((pos = errorMsg.find('"', pos)) != std::string::npos)
            {
                errorMsg.replace(pos, 1, "\\\"");
                pos += 2;
            }
            while ((pos = errorMsg.find('\n', pos)) != std::string::npos)
            {
                errorMsg.replace(pos, 1, "\\n");
                pos += 2;
            }

            res.status = 500;
            res.set_content("{\"status\":\"error\", \"msg\":\"" + errorMsg + "\"}", "application/json");
        }
    }

    void handleApiDownloadSync(const httplib::Request &req, httplib::Response &res, Store *tienda)
    {
        std::string id = req.has_param("id") ? req.get_param_value("id") : "";
        std::string type = req.has_param("type") ? req.get_param_value("type") : "";

        if (id.empty() || type.empty())
            return;

        std::string filename = (type == "excel") ? "temp_out_" + id + ".xlsx" : "Reporte_Modificaciones_" + id + ".html";
        std::string contentType = (type == "excel") ? "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet" : "text/html";
        std::string downloadName = (type == "excel") ? "Inventario_Actualizado.xlsx" : "Reporte_Auditoria.html";

        std::ifstream ifs(filename, std::ios::binary);
        if (ifs.is_open())
        {
            std::string content((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
            ifs.close();
            res.set_header("Content-Disposition", "attachment; filename=\"" + downloadName + "\"");
            res.set_content(content, contentType);

            std::remove(filename.c_str());
        }
        else
        {
            res.status = 404;
            res.set_content("Archivo no encontrado o ya descargado.", "text/plain");
        }
    }
}