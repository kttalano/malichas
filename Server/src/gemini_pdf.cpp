#include "web_routes.hpp"
#include "web_utils.hpp"
#include "web_templates.hpp"
#include "web_auth.hpp"
#include <mutex>
#include <fstream>
#include <ctime>
#include <atomic>
#include <thread>
#include <sstream>
#include <iostream>

extern std::recursive_mutex mutexTienda;

std::atomic<bool> isProcessingPDF(false);
std::atomic<bool> pdfReady(false);
std::atomic<bool> pdfError(false);

namespace WebRoutes
{

    void handleSubirPDF(const httplib::Request &req, httplib::Response &res, Store *tienda)
    {
        std::string html = WebTemplates::getHeadAndNav("Importar Presupuesto");

        html += R"(
            <h2 class='page-title'>Importar Remito</h2>
            <div class='card-glass p-4 text-center'>
                <i class='fas fa-file-pdf fs-1 text-danger mb-3 me-2'></i>
                <i class='fas fa-image fs-1 text-primary mb-3'></i>
                <h5 class='fw-bold mb-3'>Subir Presupuesto Laila</h5>
                
                <input type='file' id='pdfInput' accept='.pdf, image/jpeg, image/png, image/webp' required class='form-control mb-4'>
                
                <button id='btnSubmit' onclick='uploadPDF()' class='btn w-100 rounded-pill fw-bold text-white shadow-sm' style='background-color: var(--mali-primary);'>
                    <i class='fas fa-robot me-2'></i> Procesar con Inteligencia Artificial
                </button>

                <div id='pdf-error-log' class='d-none bg-dark text-success p-3 rounded mt-3 text-start shadow-inner' style='font-family: monospace; font-size: 0.85rem; max-height: 250px; overflow-y: auto; border: 1px solid #111;'></div>
                
                <script>
                function uploadPDF() {
                    const fileInput = document.getElementById('pdfInput');
                    if (!fileInput.files.length) { 
                        alert('Por favor selecciona un archivo PDF o Imagen.'); 
                        return; 
                    }
                    
                    const btn = document.getElementById('btnSubmit');
                    btn.innerHTML = "<i class='fas fa-spinner fa-spin me-2'></i> La IA está analizando...";
                    btn.disabled = true;

                    
                    document.getElementById('pdf-error-log').classList.add('d-none');

                    fetch('/procesar_pdf', {
                        method: 'POST',
                        body: fileInput.files[0]
                    })
                    .then(response => response.json())
                    .then(data => {
                        if(data.status === 'started' || data.status === 'busy') {
                            checkStatus(); 
                        } else {
                            alert("Error al iniciar el proceso.");
                            btn.innerHTML = "<i class='fas fa-robot me-2'></i> Procesar con Inteligencia Artificial";
                            btn.disabled = false;
                        }
                    })
                    .catch(err => {
                        alert("Hubo un error de comunicación.");
                        btn.innerHTML = "<i class='fas fa-robot me-2'></i> Procesar con Inteligencia Artificial";
                        btn.disabled = false;
                    });
                }

                function checkStatus() {
                    setTimeout(() => {
                        fetch('/status_pdf')
                        .then(res => res.json())
                        .then(data => {
                            if(data.status === 'processing') {
                                checkStatus(); 
                            } else if(data.status === 'ready') {
                                window.location.href = '/editor_pdf'; 
                            } else if(data.status === 'error') {
                                const btn = document.getElementById('btnSubmit');
                                btn.innerHTML = "<i class='fas fa-exclamation-triangle me-2'></i> Error en IA";
                                btn.classList.replace('btn-primary', 'btn-danger');
                                btn.disabled = false;
                                
                                const consola = document.getElementById('pdf-error-log');
                                consola.classList.remove('d-none');
                                consola.innerText = "root@mali-server:~# analizando archivo...\n> ERROR ENCONTRADO:\n" + data.message;
                            }
                        });
                    }, 3000); 
                }
                </script>
            </div>
        )";

        html += WebTemplates::getFooter("");
        res.set_content(html, "text/html");
    }

    void handleProcesarPDF(const httplib::Request &req, httplib::Response &res, Store *tienda)
    {
        if (isProcessingPDF)
        {
            res.set_content("{\"status\":\"busy\"}", "application/json");
            return;
        }
        if (req.body.empty())
        {
            res.set_content("{\"status\":\"error\"}", "application/json");
            return;
        }

        std::ofstream ofs("temp_presupuesto.pdf", std::ios::binary);
        ofs << req.body;
        ofs.close();

        std::remove("log_ia.txt");
        std::remove("error_ia_msg.json");
        std::remove("temp_ia_output.json");

        isProcessingPDF = true;
        pdfReady = false;
        pdfError = false;

        std::thread([]()
                    {
            int result = system("python3 lector_pdf_ia.py temp_presupuesto.pdf > log_ia.txt 2>&1");
            if (result != 0) {
                pdfError = true; 
            } else {
                pdfReady = true; 
            }
            isProcessingPDF = false; })
            .detach();

        res.set_content("{\"status\":\"started\"}", "application/json");
    }

    void handleStatusPDF(const httplib::Request &req, httplib::Response &res, Store *tienda)
    {
        if (isProcessingPDF)
        {
            res.set_content("{\"status\":\"processing\"}", "application/json");
        }
        else if (pdfError)
        {
            std::string errorMsg = "Error fatal en el script de Python.";
            std::ifstream fError("error_ia_msg.json");
            if (fError.is_open())
            {
                json j;
                try
                {
                    fError >> j;
                    errorMsg = j.value("error_detallado", errorMsg);
                }
                catch (...)
                {
                }
                fError.close();
            }
            else
            {
                std::ifstream fLog("log_ia.txt");
                if (fLog.is_open())
                {
                    std::string logContent((std::istreambuf_iterator<char>(fLog)), std::istreambuf_iterator<char>());
                    errorMsg = logContent;
                    fLog.close();
                }
            }
            json resp;
            resp["status"] = "error";
            resp["message"] = errorMsg;
            res.set_content(resp.dump(), "application/json");
        }
        else if (pdfReady)
        {
            res.set_content("{\"status\":\"ready\"}", "application/json");
        }
        else
        {
            res.set_content("{\"status\":\"idle\"}", "application/json");
        }
    }

    void handleEditorPDF(const httplib::Request &req, httplib::Response &res, Store *tienda)
    {
        json j;
        bool jsonLoaded = false;

        std::ifstream f("temp_ia_output.json");
        if (f.is_open())
        {
            try
            {
                f >> j;
                jsonLoaded = true;
            }
            catch (...)
            {
            }
            f.close();
        }

        if (!jsonLoaded)
        {
            res.set_content("Error Crítico: El archivo JSON no se pudo cargar. Revisa que el script de Python lo haya creado correctamente con sintaxis válida.", "text/plain");
            return;
        }

        pdfReady = false;
        std::string jsonString = j.dump();

        std::string html = WebTemplates::getHeadAndNav("Revisión de Remito");
        html += R"(
        <h2 class='page-title text-warning mb-4'><i class='fas fa-clipboard-check me-2'></i>Control de Ingreso</h2>
        <div id='editor-container'></div>
        <div class='card-glass p-3 text-center mt-4'>
            <h5 class='fw-bold mb-3' id='total-badge'>Total Confirmado: 0 Prendas</h5>
            <button onclick='enviarConfirmacion()' class='btn btn-success w-100 rounded-pill fw-bold shadow-sm py-2' id='btnConfirmar'>
                <i class='fas fa-check-circle me-2'></i> Confirmar e Inyectar Stock
            </button>
            <a href='/inventario' class='btn btn-light text-danger w-100 rounded-pill fw-bold mt-2 border'>Cancelar y Borrar</a>
        </div>

        <script>
            let invData = )" +
                jsonString + R"(;
            invData.forEach(f => f.isEditing = false);

            function renderEditor() {
                let html = "";
                let total = 0;

                if (invData.length === 0) {
                    html = "<div class='alert alert-light text-center border-0 shadow-sm'>No hay artículos para ingresar.</div>";
                }

                invData.forEach((fam, fIdx) => {
                    if (!fam.isEditing) {
                        html += `<div class='card-glass mb-3 p-0 overflow-hidden shadow-sm'>
                            <div class='d-flex justify-content-between align-items-center p-3' style='background-color: rgba(92, 51, 190, 0.05); border-bottom: 1px solid var(--mali-border);'>
                                <div>
                                    <span class='fw-bold' style='color: var(--mali-primary); font-size: 1.1rem; letter-spacing: 0.5px;'>${fam.m_sku}</span>
                                    <span class='text-dark fw-bold'> | ${fam.m_marca.m_name}</span>
                                    <span class='badge ms-2 fw-bold' style='background-color: var(--mali-accent); color: #1e0b4f !important;'>$${parseFloat(fam.m_precioBase).toLocaleString('es-AR')}</span>
                                    <small class='d-block text-muted mt-1'>${fam.m_nombre}</small>
                                </div>
                                <div class='d-flex gap-2'>
                                    <button onclick='toggleEdit(${fIdx})' class='btn btn-light border text-primary rounded-circle shadow-sm' style='width: 38px; height: 38px;' title='Editar'><i class='fas fa-pen'></i></button>
                                    <button onclick='deleteFam(${fIdx})' class='btn btn-light border text-danger rounded-circle shadow-sm' style='width: 38px; height: 38px;' title='Eliminar Familia'><i class='fas fa-trash'></i></button>
                                </div>
                            </div>
                            <ul class='list-group list-group-flush'>`;
                        
                        fam.m_variantes.forEach((varItem) => {
                            let stock = parseInt(varItem.m_stock) || 0;
                            let precioEsp = parseFloat(varItem.m_precioEspecifico) || 0;
                            total += stock;
                            
                            let badgePrecioEsp = "";
                            if (precioEsp > 0) {
                                badgePrecioEsp = `<span class='badge bg-warning text-dark me-2'>$${precioEsp.toLocaleString('es-AR')}</span>`;
                            }

                            html += `
                                <li class='list-group-item bg-transparent d-flex justify-content-between align-items-center border-light py-2 px-3'>
                                    <small class='text-muted'>Talle: <b class='text-dark'>${varItem.m_talle}</b> - Color: <b class='text-dark'>${varItem.m_color}</b></small>
                                    <div>
                                        ${badgePrecioEsp}
                                        <span class='badge bg-success rounded-pill'>+${stock} un.</span>
                                    </div>
                                </li>
                            `;
                        });
                        html += `</ul></div>`;
                    } else {
                        html += `<div class='card-glass mb-3 p-3 border-primary shadow'>
                            <div class='d-flex justify-content-between align-items-center mb-3 pb-2 border-bottom'>
                                <h6 class='fw-bold text-primary mb-0'><i class='fas fa-edit me-2'></i>Editando Artículo</h6>
                                <button onclick='deleteFam(${fIdx})' class='btn btn-sm btn-danger rounded-pill px-3'><i class='fas fa-trash me-1'></i>Eliminar</button>
                            </div>
                            <div class='row g-2 mb-3'>
                                <div class='col-3 col-md-2'>
                                    <label class='small text-muted fw-bold'>SKU</label>
                                    <input type='text' class='form-control form-control-sm fw-bold text-primary' value='${fam.m_sku}' onchange='invData[${fIdx}].m_sku=this.value'>
                                </div>
                                <div class='col-5 col-md-3'>
                                    <label class='small text-muted fw-bold'>Marca</label>
                                    <input type='text' class='form-control form-control-sm text-dark' value='${fam.m_marca.m_name}' onchange='invData[${fIdx}].m_marca.m_name=this.value'>
                                </div>
                                <div class='col-4 col-md-3'>
                                    <label class='small text-muted fw-bold'>Precio Base</label>
                                    <input type='number' class='form-control form-control-sm text-success fw-bold text-center' value='${fam.m_precioBase}' onchange='invData[${fIdx}].m_precioBase=parseFloat(this.value)||0'>
                                </div>
                                <div class='col-12 col-md-4'>
                                    <label class='small text-muted fw-bold'>Descripción</label>
                                    <input type='text' class='form-control form-control-sm text-dark' value='${fam.m_nombre}' onchange='invData[${fIdx}].m_nombre=this.value'>
                                </div>
                            </div>
                            <div class='p-2 rounded border' style='background-color: var(--mali-bg);'>
                                <div class='row g-2 mb-1 px-1 d-none d-md-flex text-muted small fw-bold text-center'>
                                    <div class='col-3'>Talle</div>
                                    <div class='col-3'>Color</div>
                                    <div class='col-3'>Precio Esp.</div>
                                    <div class='col-2'>Cant</div>
                                </div>`;
                        
                        fam.m_variantes.forEach((varItem, vIdx) => {
                            let stock = parseInt(varItem.m_stock) || 0;
                            let precioEsp = parseFloat(varItem.m_precioEspecifico) || 0;
                            total += stock;
                            html += `
                                <div class='row g-2 mb-2 align-items-center'>
                                    <div class='col-3 col-md-3'>
                                        <input type='text' class='form-control form-control-sm' value='${varItem.m_talle}' onchange='invData[${fIdx}].m_variantes[${vIdx}].m_talle=this.value'>
                                    </div>
                                    <div class='col-3 col-md-3'>
                                        <input type='text' class='form-control form-control-sm' value='${varItem.m_color}' onchange='invData[${fIdx}].m_variantes[${vIdx}].m_color=this.value'>
                                    </div>
                                    <div class='col-3 col-md-3'>
                                        <input type='number' class='form-control form-control-sm text-warning fw-bold text-center' value='${precioEsp}' onchange='actualizarPrecioEsp(${fIdx}, ${vIdx}, this.value)'>
                                    </div>
                                    <div class='col-2 col-md-2'>
                                        <input type='number' class='form-control form-control-sm text-success fw-bold text-center' value='${stock}' onchange='actualizarCant(${fIdx}, ${vIdx}, this.value)'>
                                    </div>
                                    <div class='col-1 col-md-1 text-end'>
                                        <button onclick='deleteVar(${fIdx}, ${vIdx})' class='btn btn-sm btn-outline-danger border-0 rounded-circle'><i class='fas fa-times'></i></button>
                                    </div>
                                </div>
                            `;
                        });
                        
                        html += `
                            <div class='text-center mt-3 mb-2'>
                                <button onclick='addVar(${fIdx})' class='btn btn-sm btn-outline-success rounded-pill fw-bold px-3'><i class='fas fa-plus me-1'></i>Añadir Variante</button>
                            </div>
                            </div>
                            <div class='text-end mt-3'>
                                <button onclick='toggleEdit(${fIdx})' class='btn btn-primary rounded-pill px-4 fw-bold shadow-sm'><i class='fas fa-save me-2'></i>Guardar Edición</button>
                            </div>
                        </div>`;
                    }
                });

                document.getElementById('editor-container').innerHTML = html;
                document.getElementById('total-badge').innerText = `Total Confirmado: ${total} Prendas`;
            }

            function addVar(fIdx) {
                invData[fIdx].m_variantes.push({
                    m_talle: "",
                    m_color: "",
                    m_stock: 1,
                    m_precioEspecifico: 0.0,
                    m_codigoBarras: []
                });
                renderEditor();
            }

            function actualizarCant(fIdx, vIdx, val) {
                invData[fIdx].m_variantes[vIdx].m_stock = parseInt(val) || 0;
                renderEditor(); 
            }

            function actualizarPrecioEsp(fIdx, vIdx, val) {
                invData[fIdx].m_variantes[vIdx].m_precioEspecifico = parseFloat(val) || 0;
                renderEditor(); 
            }

            function toggleEdit(fIdx) {
                invData[fIdx].isEditing = !invData[fIdx].isEditing;
                renderEditor();
            }

            function deleteFam(fIdx) {
                if (confirm("¿Eliminar toda esta familia?")) {
                    invData.splice(fIdx, 1);
                    renderEditor();
                }
            }

            function deleteVar(fIdx, vIdx) {
                if (confirm("¿Eliminar variante?")) {
                    invData[fIdx].m_variantes.splice(vIdx, 1);
                    if (invData[fIdx].m_variantes.length === 0) invData.splice(fIdx, 1);
                    renderEditor();
                }
            }

            function enviarConfirmacion() {
                if (invData.length === 0) return;
                const btn = document.getElementById('btnConfirmar');
                btn.innerHTML = "<i class='fas fa-spinner fa-spin me-2'></i> Inyectando...";
                btn.disabled = true;

                
                let datosLimpios = invData.map(fam => {
                    let clon = Object.assign({}, fam);
                    delete clon.isEditing; 
                    clon.m_precioBase = parseFloat(clon.m_precioBase) || 0;
                    clon.m_variantes = clon.m_variantes.map(v => {
                        v.m_stock = parseInt(v.m_stock) || 0;
                        v.m_precioEspecifico = parseFloat(v.m_precioEspecifico) || 0;
                        return v;
                    });
                    return clon;
                });

                fetch('/confirmar_pdf', {
                    method: 'POST',
                    body: JSON.stringify(datosLimpios)
                }).then(async response => {
                    
                    if (!response.ok) {
                        const errorText = await response.text();
                        throw new Error(errorText);
                    }
                    window.location.href = '/inventario';
                }).catch(e => {
                    alert('🔥 Error crítico inyectando en la base de datos:\n\n' + e.message);
                    btn.innerHTML = "<i class='fas fa-check-circle me-2'></i> Confirmar e Inyectar Stock";
                    btn.disabled = false;
                });
            }

            renderEditor();
        </script>
        )";

        html += WebTemplates::getFooter("");
        res.set_content(html, "text/html");
    }

    void handleConfirmarPDF(const httplib::Request &req, httplib::Response &res, Store *tienda)
    {
        if (req.body.empty())
        {
            res.set_redirect("/inventario");
            return;
        }

        try
        {

            json j = json::parse(req.body);
            auto nuevasFamilias = j.get<std::vector<ArticuloFamilia>>();

            time_t now = time(0);
            tm *ltm = localtime(&now);
            char buffer[20];
            strftime(buffer, sizeof(buffer), "%d/%m/%Y", ltm);
            std::string fechaHoy(buffer);

            {
                std::lock_guard<std::recursive_mutex> lock(mutexTienda);
                for (const auto &famNueva : nuevasFamilias)
                {
                    bool famExiste = false;

                    for (auto &famExistente : tienda->inventory)
                    {
                        if (famExistente.m_sku == famNueva.m_sku)
                        {
                            famExiste = true;

                            if (famNueva.m_precioBase > famExistente.m_precioBase)
                            {
                                famExistente.m_precioBase = famNueva.m_precioBase;
                            }

                            for (const auto &varNueva : famNueva.m_variantes)
                            {
                                if (varNueva.m_stock <= 0)
                                    continue;

                                bool varExiste = false;
                                for (auto &varExistente : famExistente.m_variantes)
                                {
                                    if (varExistente.m_talle == varNueva.m_talle && varExistente.m_color == varNueva.m_color)
                                    {
                                        varExistente.m_stock += varNueva.m_stock;

                                        if (varNueva.m_precioEspecifico > varExistente.m_precioEspecifico)
                                        {
                                            varExistente.m_precioEspecifico = varNueva.m_precioEspecifico;
                                        }

                                        varExiste = true;
                                        break;
                                    }
                                }
                                if (!varExiste)
                                    famExistente.m_variantes.push_back(varNueva);
                            }
                            break;
                        }
                    }

                    if (!famExiste)
                    {
                        ArticuloFamilia famLimpia = famNueva;
                        famLimpia.m_variantes.clear();
                        for (const auto &v : famNueva.m_variantes)
                        {
                            if (v.m_stock > 0)
                                famLimpia.m_variantes.push_back(v);
                        }
                        if (!famLimpia.m_variantes.empty())
                            tienda->inventory.push_back(famLimpia);
                    }

                    for (const auto &varNueva : famNueva.m_variantes)
                    {
                        if (varNueva.m_stock <= 0)
                            continue;
                        std::string desc = famNueva.m_nombre + " (" + varNueva.m_talle + " - " + varNueva.m_color + ")";
                        Movement mov(fechaHoy, "Sistema Web", famNueva.m_sku, desc, varNueva.m_stock, "Ingreso Remito PDF");
                        tienda->movements.push_back(mov);
                    }
                }

                tienda->saveToFile();
            }

            std::remove("temp_presupuesto.pdf");
            std::remove("temp_ia_output.json");
            std::remove("log_ia.txt");

            res.set_content("OK", "text/plain");
        }
        catch (const json::parse_error &e)
        {
            res.status = 400;
            res.set_content(std::string("Error parseando el JSON: ") + e.what(), "text/plain");
        }
        catch (const json::type_error &e)
        {
            res.status = 400;
            res.set_content(std::string("Error de tipos (el formato web no coincide con C++): ") + e.what(), "text/plain");
        }
        catch (const std::exception &e)
        {
            res.status = 500;
            res.set_content(std::string("Error grave en el servidor: ") + e.what(), "text/plain");
        }
        catch (...)
        {
            res.status = 500;
            res.set_content("Error desconocido al inyectar el stock.", "text/plain");
        }
    }
}