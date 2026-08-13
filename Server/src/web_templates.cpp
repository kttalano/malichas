#include "web_templates.hpp"

namespace WebTemplates
{
    std::string getHeadAndNav(const std::string &titulo)
    {
        std::string btnIzq;
        if (titulo == "Inicio")
        {

            btnIzq = R"HTML(<div style='width: 30px;'></div>)HTML";
        }
        else
        {
            btnIzq = R"HTML(<button onclick='maliGoBack()' class='btn btn-link text-decoration-none p-0' style='color: var(--mali-primary); font-size: 1.4rem; width: 30px;' title='Volver'><i class='fas fa-arrow-left'></i></button>)HTML";
        }

        return R"HTML(
        <!DOCTYPE html>
        <html lang='es'>
        <head>
            <meta charset='UTF-8'>
            <meta name='viewport' content='width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no'>
            <title>Malichas Web - )HTML" +
               titulo + R"HTML(</title>
            
            <link rel="manifest" href="/manifest.json">
            <meta name="theme-color" content="#5c33be">
            <meta name="color-scheme" content="light dark">
            <link rel="apple-touch-icon" href="/logo_512.png">
            <meta name="mobile-web-app-capable" content="yes">
            <meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">
            
            <link href='https:
            <link href='https:
            <link rel='stylesheet' href='https:
            
            <style>
                :root {
                    --mali-bg: #f5f4f9; 
                    --mali-primary: #5c33be;
                    --mali-secondary: #1e0b4f;
                    --mali-accent: #fbc02d;
                    --mali-text: #2d2d3a;
                    --mali-card-bg: #ffffff; 
                    --mali-border: #eae8f0;
                    --mali-nav-bg: rgba(255, 255, 255, 0.95);
                }

                html.dark-theme {
                    --mali-bg: #0f0c1b; 
                    --mali-primary: #9d7cff; 
                    --mali-secondary: #ffffff; 
                    --mali-text: #a19fad;
                    --mali-card-bg: #1c152e;
                    --mali-border: #2a2040;
                    --mali-nav-bg: rgba(15, 12, 27, 0.95);
                }

                html.dark-theme .bg-white, html.dark-theme .bg-light { background-color: var(--mali-card-bg) !important; }
                html.dark-theme .text-dark:not(.bg-warning) { color: #ffffff !important; }
                html.dark-theme .text-muted { color: #7f7c93 !important; }
                html.dark-theme .border, html.dark-theme .border-bottom { border-color: #2a2040 !important; }
                html.dark-theme .list-group-item { background: transparent; border-color: #2a2040; }
                html.dark-theme .form-control { background: #1c152e; color: white; border-color: #3b2e59; }
                html.dark-theme .form-control::placeholder { color: #7f7c93; }
                html.dark-theme .bg-warning { background-color: #ffd859 !important; color: #1c152e !important; font-weight: 800; }
                html.dark-theme .bg-warning.text-dark { color: #1c152e !important; }
                
                body { 
                    background-color: var(--mali-bg); 
                    font-family: 'Outfit', sans-serif; 
                    color: var(--mali-text);
                    padding-bottom: 30px; 
                    -webkit-font-smoothing: antialiased;
                    transition: background-color 0.3s ease, color 0.3s ease;
                }
                
                .navbar-custom { background: var(--mali-nav-bg); backdrop-filter: blur(10px); border-bottom: 1px solid var(--mali-border); padding: 12px 20px; }
                .navbar-brand { font-weight: 800; letter-spacing: 1.5px; font-size: 1.3rem; color: var(--mali-secondary) !important; }
                .navbar-brand span { color: var(--mali-primary); font-weight: 400; }
                
                .page-title { font-weight: 800; color: var(--mali-secondary); margin-bottom: 1.5rem; font-size: 1.5rem; letter-spacing: -0.3px;}

                .card-glass { border: 1px solid var(--mali-border); border-radius: 20px; background: var(--mali-card-bg); box-shadow: 0 4px 12px rgba(0, 0, 0, 0.04); margin-bottom: 20px; overflow: hidden; transform: translateZ(0); will-change: transform;}
                
                .brand-card { cursor: pointer; text-align: center; padding: 25px 15px; display: flex; flex-direction: column; justify-content: center; align-items: center; height: 100%; }
                .brand-card:active { transform: scale(0.96); }
                .brand-letter-icon { width: 50px; height: 50px; background: var(--mali-primary); color: white; border-radius: 16px; display: flex; justify-content: center; align-items: center; font-size: 1.4rem; font-weight: 600; margin-bottom: 15px; }

                .badge-price { background: rgba(27, 122, 67, 0.1); color: #2e9e5b; padding: 6px 14px; font-weight: 700; }
                .form-control { border-radius: 16px; padding: 12px 18px; border: 1px solid var(--mali-border); background: var(--mali-card-bg); font-size: 0.95rem; }
                .form-control:focus { border-color: var(--mali-primary); box-shadow: none; outline: 2px solid rgba(92, 51, 190, 0.2); }

                .accordion-button { font-weight: 600; border-radius: 20px !important; color: var(--mali-secondary); background: transparent; padding: 16px;}
                .accordion-button:not(.collapsed) { background-color: rgba(92, 51, 190, 0.05); color: var(--mali-primary); box-shadow: none; }
                .accordion-item { border: none; background: transparent; }

                .app-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 15px; margin-bottom: 25px; }
                .app-btn { background: var(--mali-card-bg); border: 1px solid var(--mali-border); border-radius: 20px; padding: 25px 15px; text-align: center; color: var(--mali-secondary); text-decoration: none; box-shadow: 0 4px 12px rgba(0, 0, 0, 0.04); display: flex; flex-direction: column; align-items: center; justify-content: center; transition: transform 0.2s; }
                .app-btn:active { transform: scale(0.95); }
                .app-btn i { font-size: 2.3rem; margin-bottom: 12px; color: var(--mali-primary); }
                .app-btn.scan i { color: #28a745; }
                .app-btn span { font-weight: 700; font-size: 1.05rem; }
                
               
                .accordion-button.collapsed .marca-expandida { 
                    display: none !important; 
                }

               
                .accordion-button:not(.collapsed) .marca-expandida { 
                    display: inline-block !important; 
                    color: var(--mali-text); 
                    font-size: 0.95rem; 
                    font-weight: 600; 
                    opacity: 0.8;
                }
                
                @media (max-width: 768px) {
                    body { padding-bottom: 20px; } 
                    .navbar-custom { padding: 8px 15px; } 
                    .navbar-brand { font-size: 1.15rem; }
                    h2.page-title { font-size: 1.25rem !important; margin-bottom: 1rem; } 
                    .card-glass { border-radius: 16px; margin-bottom: 12px; } 
                    .btn { padding: 8px 15px !important; font-size: 0.9rem; } 
                    .form-control { padding: 10px 15px; font-size: 0.9rem; border-radius: 14px; } 
                    .brand-card { padding: 15px 10px; border-radius: 14px; } 
                    .brand-letter-icon { width: 40px; height: 40px; font-size: 1.2rem; margin-bottom: 10px; }
                    .brand-card h5 { font-size: 1rem; }
                }
            </style>
        </head>
        <body>
            <script>
                (function() {
                    const theme = localStorage.getItem('mali_theme');
                    if (theme === 'dark') {
                        document.documentElement.classList.add('dark-theme');
                    }
                })();
            </script>

            <nav class='navbar navbar-custom sticky-top'>
                <div class='container-fluid d-flex justify-content-between align-items-center'>
                    )HTML" +
               btnIzq + R"HTML(
                    <span class='navbar-brand mb-0 mx-auto'>MALICHAS <span>WEB</span></span>
                    <button id='btn-theme' onclick='toggleTheme()' class='btn btn-link text-decoration-none p-0' style='color: var(--mali-secondary); font-size: 1.2rem; width: 30px;'>
                        <i id='theme-icon' class='fas fa-moon'></i>
                    </button>
                </div>
            </nav>
            <div class='container mt-4'>
        )HTML";
    }

    std::string getFooter(const std::string &activeTab)
    {
        return R"HTML(
            </div> 

            <!-- MODAL DE CONFIRMACIÓN MALI -->
            <div class="modal fade" id="modalConfirmacionMali" tabindex="-1" aria-hidden="true" style="z-index: 10000;">
                <div class="modal-dialog modal-dialog-centered modal-sm">
                    <div class="modal-content shadow-lg" style="background: var(--mali-card-bg); border-radius: 20px; border: 2px solid var(--mali-primary);">
                        <div class="modal-body p-4 text-center">
                            <i class="fas fa-question-circle mb-3" style="font-size: 3.5rem; color: var(--mali-primary);"></i>
                            <h6 id="maliConfirmText" class="fw-bold text-dark mb-4" style="line-height: 1.4; font-size: 1.1rem;">¿Estás seguro?</h6>
                            <div class="d-flex gap-2 justify-content-center">
                                <button type="button" class="btn btn-light rounded-pill fw-bold border w-50" data-bs-dismiss="modal">Cancelar</button>
                                <button type="button" id="maliConfirmBtn" class="btn rounded-pill fw-bold text-white w-50" style="background-color: var(--mali-primary);">Confirmar</button>
                            </div>
                        </div>
                    </div>
                </div>
            </div>

            <!-- MODAL AÑADIR VARIANTE -->
            <div class="modal fade" id="modalAddVariante" tabindex="-1" aria-hidden="true">
                <div class="modal-dialog modal-dialog-centered modal-sm">
                    <div class="modal-content card-glass" style="border-radius: 20px; border: 2px solid var(--mali-primary);">
                        <div class="modal-header border-0 pb-0">
                            <h5 class="fw-bold text-primary mb-0"><i class="fas fa-plus-circle me-2"></i>Añadir Variante</h5>
                            <button type="button" class="btn-close shadow-none" data-bs-dismiss="modal"></button>
                        </div>
                        <div class="modal-body">
                            <input type="hidden" id="addVarSku">
                            <div class="mb-3">
                                <label class="form-label text-muted fw-bold small mb-1">Talle / Tamaño</label>
                                <input type="text" id="addVarTalle" class="form-control form-control-sm text-center fw-bold" placeholder="Ej: 90, M, Único">
                            </div>
                            <div class="mb-3">
                                <label class="form-label text-muted fw-bold small mb-1">Color / Motivo</label>
                                <input type="text" id="addVarColor" class="form-control form-control-sm text-center fw-bold" placeholder="Ej: Negro, Blanco">
                            </div>
                            <div class="mb-4">
                                <label class="form-label text-muted fw-bold small mb-1">Cantidad Física (Stock)</label>
                                <input type="number" id="addVarStock" class="form-control form-control-sm text-center fw-bold text-primary fs-5" placeholder="0">
                            </div>
                            <button class="btn btn-primary w-100 rounded-pill fw-bold py-2 shadow-sm" onclick="confirmarAddVariante()">Guardar en Inventario</button>
                        </div>
                    </div>
                </div>
            </div>

            <!-- MODAL RASTREO / HISTORIAL -->
            <div class="modal fade" id="modalHistorial" tabindex="-1" aria-hidden="true">
                <div class="modal-dialog modal-dialog-centered modal-dialog-scrollable">
                    <div class="modal-content card-glass" style="border-radius: 20px; border: 2px solid var(--mali-primary);">
                        <div class="modal-header border-0 pb-0 d-flex justify-content-between align-items-center">
                            <h5 class="fw-bold text-primary mb-0"><i class="fas fa-search me-2"></i>Historial</h5>
                            <button type="button" class="btn-close shadow-none" data-bs-dismiss="modal"></button>
                        </div>
                        <div class="modal-body" id="historialContent" style="max-height: 60vh; overflow-y: auto;"></div>
                    </div>
                </div>
            </div>

            <script src='https:
            <script>
                
                function maliConfirmLocal(mensaje, callback) {
                    document.getElementById('maliConfirmText').innerText = mensaje;
                    const modalEl = document.getElementById('modalConfirmacionMali');
                    
                    let modal = bootstrap.Modal.getInstance(modalEl);
                    if (!modal) modal = new bootstrap.Modal(modalEl);
                    
                    const btnConfirm = document.getElementById('maliConfirmBtn');
                    btnConfirm.onclick = function() {
                        modal.hide();
                        callback();
                    };
                    modal.show();
                }

                
                const modalAddEl = document.getElementById('modalAddVariante');
                const modalAddObj = new bootstrap.Modal(modalAddEl);

                
                function promptAgregarVariante(sku) {
                    document.getElementById('addVarSku').value = sku;
                    document.getElementById('addVarTalle').value = '';
                    document.getElementById('addVarColor').value = '';
                    document.getElementById('addVarStock').value = ''; 
                    
                    history.pushState({modal: 'open'}, null, ''); 
                    modalAddObj.show();
                }

                
                window.addEventListener('popstate', (event) => {
                    if (modalAddEl.classList.contains('show')) {
                        modalAddObj.hide();
                    }
                });

                
                modalAddEl.addEventListener('hidden.bs.modal', () => {
                    if (history.state && history.state.modal === 'open') {
                        history.back();
                    }
                });

                function maliGoBack() {
                    if (window.location.hash) {
                        history.back(); 
                    } else {
                        window.location.replace('/'); 
                    }
                }

                window.addEventListener('hashchange', () => {
                    if (window.location.pathname === '/inventario') {
                        if (!window.location.hash) {
                            showAllBrands(true); 
                        }
                    }
                });

                function cerrarSesionSeguro() {
                    sessionStorage.clear();
                    window.location.replace('/cdn-cgi/access/logout');
                }
                
                function toggleTheme() {
                    const doc = document.documentElement;
                    doc.classList.toggle('dark-theme');
                    const themeIcon = document.getElementById('theme-icon');
                    
                    if (doc.classList.contains('dark-theme')) {
                        localStorage.setItem('mali_theme', 'dark');
                        if(themeIcon) themeIcon.classList.replace('fa-moon', 'fa-sun');
                        document.querySelector('meta[name="theme-color"]').setAttribute("content", "#0f0c1b");
                    } else {
                        localStorage.setItem('mali_theme', 'light');
                        if(themeIcon) themeIcon.classList.replace('fa-sun', 'fa-moon');
                        document.querySelector('meta[name="theme-color"]').setAttribute("content", "#5c33be");
                    }
                }
                
                (function initTheme() {
                    if (localStorage.getItem('mali_theme') === 'dark') {
                        const themeIcon = document.getElementById('theme-icon');
                        if(themeIcon) themeIcon.classList.replace('fa-moon', 'fa-sun');
                    }
                })();

                function toggleModoEdicion(isChecked) {
                    window.isEditMode = isChecked;
                    if (currentState.type === 'brand') {
                        showBrand(currentState.query, true); 
                    } else if (currentState.type === 'search') {
                        searchSKU();
                    }
                }

                function guardarEdicion(sku, talle, color, stock) {
                    if(stock < 0) stock = 0;
                    fetch('/api/editar_inventario', {
                        method: 'POST',
                        headers: {'Content-Type': 'application/json'},
                        body: JSON.stringify({sku: sku, talle: talle, color: color, stock: parseInt(stock)})
                    }).then(r => r.json()).then(res => {
                        if(res.status === 'ok') actualizarVistaSilenciosamente();
                        else alert(res.msg);
                    }).catch(e => alert("Error de conexión"));
                }

                function confirmarAddVariante() {
                    let sku = document.getElementById('addVarSku').value;
                    let talle = document.getElementById('addVarTalle').value.trim();
                    let color = document.getElementById('addVarColor').value.trim();
                    let cant = document.getElementById('addVarStock').value;
                    
                    if(!talle || !color) {
                        alert("Talle y Color son obligatorios.");
                        return;
                    }

                    fetch('/api/agregar_variante', {
                        method: 'POST',
                        headers: {'Content-Type': 'application/json'},
                        body: JSON.stringify({sku: sku, talle: talle, color: color, cantidad: parseInt(cant)})
                    }).then(r => r.json()).then(res => {
                        if(res.status === 'ok') {
                            bootstrap.Modal.getInstance(document.getElementById('modalAddVariante')).hide();
                            actualizarVistaSilenciosamente();
                        } else alert(res.msg);
                    }).catch(e => alert("Error de conexión"));
                }

                function verHistorial(sku) {
                    const container = document.getElementById('historialContent');
                    container.innerHTML = '<div class="text-center p-4"><i class="fas fa-spinner fa-spin fa-2x text-primary"></i></div>';
                    
                    const modal = new bootstrap.Modal(document.getElementById('modalHistorial'));
                    modal.show();

                    fetch('/api/historial_sku?sku=' + encodeURIComponent(sku))
                    .then(r => r.json())
                    .then(data => {
                        if(data.length === 0) {
                            container.innerHTML = '<div class="alert alert-light text-center border">No hay registros para este SKU.</div>';
                            return;
                        }
                        let html = '<div class="timeline">';
                        data.forEach(mov => {
                            let color = mov.cantidad > 0 ? 'text-success' : 'text-danger';
                            let signo = mov.cantidad > 0 ? '+' : '';
                            html += `
                            <div class="border-bottom py-2 mb-1">
                                <div class="d-flex justify-content-between align-items-center">
                                    <span class="fw-bold" style="color:var(--mali-secondary); font-size:0.9rem;">${mov.motivo}</span>
                                    <h5 class="fw-bold mb-0 ${color}">${signo}${mov.cantidad}</h5>
                                </div>
                                <div class="text-muted" style="font-size:0.8rem;">
                                    <i class="far fa-calendar-alt me-1"></i>${mov.fecha} | <i class="far fa-user me-1"></i>${mov.usuario}
                                </div>
                                <div class="text-secondary small mt-1">${mov.desc}</div>
                            </div>`;
                        });
                        html += '</div>';
                        container.innerHTML = html;
                    });
                }

                function renderizarTarjetas(data) {
                    const container = document.getElementById('item-list-container');
                    
                    if (!data || data.length === 0) {
                        container.innerHTML = '<div class="text-center text-muted p-5 mt-4"><i class="fas fa-box-open fa-3x mb-3 opacity-25"></i><br>No se encontraron artículos.</div>';
                        return;
                    }

                    let userRole = window.MALI_USER_ROLE || 1;
                    let isAdmin = (userRole === 4 || userRole === 5);
                    let isEdit = window.isEditMode && isAdmin;

                    let html = '<div class="accordion" id="accordionInventario">';
                    
                    data.forEach((fam, idx) => {
                        let varHtml = '';
                        let totalStock = 0;
                        
                        let variantes = fam.variantes || fam.m_variantes || [];
                        let sku = fam.sku || fam.m_sku;
                        let marca = fam.marca || (fam.m_marca ? fam.m_marca.m_name : 'Genérica');
                        let nombre = fam.nombre || fam.m_nombre;
                        let precioBase = fam.precio || fam.m_precioBase || 0;
                        let multiplesPrecios = fam.multiples_precios || false; 

                        variantes.forEach(v => {
                            let stock = v.stock ?? v.m_stock ?? 0;
                            let talle = v.talle || v.m_talle;
                            let color = v.color || v.m_color;
                            let precioVar = v.precio || 0; 
                            totalStock += stock;
                            
                            if (isEdit) {
                                varHtml += `
                                <div class="d-flex justify-content-between align-items-center border-bottom py-2 px-3" style="background-color: rgba(92,51,190,0.03);">
                                    <div style="flex:1; line-height: 1.2;">
                                        <small class="text-muted fw-bold d-block">T: ${talle} | C: ${color}</small>
                                    </div>
                                    <div class="d-flex align-items-center gap-2">
                                        <div class="input-group input-group-sm shadow-sm" style="width: 100px;">
                                            <button class="btn btn-outline-danger px-2" onclick="guardarEdicion('${sku}', '${talle}', '${color}', ${stock - 1})">-</button>
                                            <input type="text" class="form-control text-center fw-bold bg-white" value="${stock}" readonly>
                                            <button class="btn btn-outline-success px-2" onclick="guardarEdicion('${sku}', '${talle}', '${color}', ${stock + 1})">+</button>
                                        </div>
                                    </div>
                                </div>`;
                            } else {
                                let etiquetaPrecioExtra = '';
                                if (precioVar > 0 && precioVar !== precioBase) {
                                    etiquetaPrecioExtra = `<span class="badge bg-warning text-dark me-2 shadow-sm" style="font-size: 0.8rem;">$${precioVar.toLocaleString('es-AR')}</span>`;
                                }
                                
                                let eyeBtn = isAdmin ? `<button class="btn btn-sm btn-link text-primary p-0 ms-2" onclick="verHistorial('${sku}')" title="Ver Historial de Movimientos"><i class="fas fa-eye fs-5"></i></button>` : '';

                                varHtml += `
                                <div class="d-flex justify-content-between align-items-center border-bottom py-2 px-3">
                                    <span class="text-muted small">Talle: <b>${talle}</b> | Color: <b>${color}</b></span>
                                    <div class="d-flex align-items-center">
                                        ${etiquetaPrecioExtra}
                                        <span class="fw-bold px-2 py-1 rounded bg-light border text-dark" style="font-size: 0.85rem;">${stock} un.</span>
                                        ${eyeBtn}
                                    </div>
                                </div>`;
                            }
                        });

                        if (isEdit) {
                            varHtml += `
                            <div class="p-2 text-center" style="background-color: var(--mali-card-bg);">
                                <button class="btn btn-sm rounded-pill fw-bold px-4 shadow-sm" style="color: var(--mali-primary); border: 2px dashed var(--mali-primary); background: transparent;" onclick="promptAgregarVariante('${sku}')">
                                    <i class="fas fa-plus me-1"></i> Añadir Talle/Color
                                </button>
                            </div>`;
                        }

                        let textoPrecioPrincipal = `$${precioBase.toLocaleString('es-AR')}`;
                        if (multiplesPrecios) {
                            textoPrecioPrincipal = `<span style="font-size: 0.75rem; opacity: 0.8; font-weight: normal;">Desde</span> $${precioBase.toLocaleString('es-AR')} <i class="fas fa-asterisk ms-1" style="font-size: 0.5rem; opacity: 0.6; position: relative; top: -5px;"></i>`;
                        }

                        html += `
                        <div class='card-glass mb-2 filterable-card' style='padding: 0;'>
                            <div class='accordion-item'>
                                <h2 class='accordion-header' id='headingInv-${idx}'>
                                    <button class='accordion-button collapsed py-3' type='button' data-bs-toggle='collapse' data-bs-target='#collapseInv-${idx}'>
                                        <div class='d-flex justify-content-between align-items-center w-100 me-2'>
                                            <div class='d-flex flex-column align-items-start'>
                                                <h5 class='fw-bold mb-1' style='color: var(--mali-primary); font-size: 1.15rem;'>
                                                    <span class='marca-expandida'>${marca} <span style='opacity: 0.5; margin: 0 4px;'>&bull;</span> </span>${sku}
                                                </h5>
                                                <div>
                                                    <span class='badge bg-light text-secondary border'>Total: ${totalStock} un.</span>
                                                </div>
                                            </div>
                                            <span class='badge-price rounded-pill fs-6 shadow-sm'>${textoPrecioPrincipal}</span>
                                        </div>
                                    </button>
                                </h2>
                                <div id='collapseInv-${idx}' class='accordion-collapse collapse' data-bs-parent='#accordionInventario'>
                                    <div class='accordion-body bg-white p-0 rounded-bottom' style='border-top: 1px solid var(--mali-border);'>
                                        <div class='px-3 py-2' style='background: rgba(0,0,0,0.02); border-bottom: 1px solid var(--mali-border);'>
                                            <small class='text-muted fw-semibold'>${nombre}</small>
                                        </div>
                                        ${varHtml}
                                    </div>
                                </div>
                            </div>
                        </div>`;
                    });
                    
                    html += '</div>';
                    container.innerHTML = html;
                }

                function showBrand(marcaName, fromHash = false) {
                    currentState = { type: 'brand', query: marcaName };
                    
                    if (!fromHash) history.pushState(null, null, '#ver_marca'); 

                    document.getElementById('brand-grid').style.display = 'none';
                    document.getElementById('item-list').style.display = 'block';
                    document.getElementById('btn-volver').style.display = 'block';
                    
                    let switchCont = document.getElementById('editSwitchContainer');
                    if(switchCont) switchCont.style.setProperty('display', 'flex', 'important');
                    
                    window.scrollTo(0, 0); 
                    
                    document.getElementById('list-title').innerText = "Catálogo: " + marcaName;
                    
                    const container = document.getElementById('item-list-container');
                    if(container) {
                        container.innerHTML = '<div class="text-center p-5 mt-5"><i class="fas fa-spinner fa-spin fa-2x" style="color: var(--mali-primary);"></i><p class="text-muted mt-3 small fw-bold">Cargando catálogo...</p></div>';
                    }

                    let editQuery = window.isEditMode ? '&edit=true' : '';
                    fetch('/api/inventario_marca?marca=' + encodeURIComponent(marcaName) + editQuery)
                    .then(res => res.json())
                    .then(data => { renderizarTarjetas(data); })
                    .catch(err => {
                        if(container) container.innerHTML = '<div class="text-center text-danger p-5"><i class="fas fa-exclamation-triangle fa-2x mb-2"></i><br>Error al cargar.</div>';
                    });
                }

                function showAllBrands(fromHash = false) {
                    currentState = { type: 'home', query: '' };
                    document.getElementById('brand-grid').style.display = 'flex';
                    document.getElementById('item-list').style.display = 'none';
                    document.getElementById('btn-volver').style.display = 'none';
                    document.getElementById('searchBox').value = '';

                    let switchCont = document.getElementById('editSwitchContainer');
                    if(switchCont) switchCont.style.setProperty('display', 'none', 'important');
                    let switchModo = document.getElementById('switchModoEdicion');
                    if(switchModo && switchModo.checked) {
                        switchModo.checked = false;
                        window.isEditMode = false;
                    }

                    if (!fromHash && window.location.hash === '#ver_marca') {
                        history.back();
                    }
                }

                let searchTimeout = null;
                function searchSKU() {
                    const searchInput = document.getElementById('searchBox');
                    if(!searchInput) return;
                    
                    const searchText = searchInput.value.toLowerCase().trim();
                    const brandGrid = document.getElementById('brand-grid');
                    const cards = document.querySelectorAll('.filterable-card');

                    if (brandGrid) {
                        if (searchText === '') {
                            currentState = { type: 'home', query: '' };
                            showAllBrands();
                            return;
                        }
                        
                        currentState = { type: 'search', query: searchText };
                        
                        brandGrid.style.display = 'none';
                        document.getElementById('item-list').style.display = 'block';
                        document.getElementById('btn-volver').style.display = 'block';
                        document.getElementById('list-title').innerText = "Buscando: " + searchText;
                        
                        let switchCont = document.getElementById('editSwitchContainer');
                        if(switchCont) switchCont.style.setProperty('display', 'flex', 'important');
                        
                        const container = document.getElementById('item-list-container');
                        if(container) container.innerHTML = '<div class="text-center p-5 mt-5"><i class="fas fa-circle-notch fa-spin fa-2x" style="color: var(--mali-primary);"></i><p class="text-muted mt-3 small fw-bold">Buscando en la base de datos...</p></div>';

                        clearTimeout(searchTimeout);
                        searchTimeout = setTimeout(() => {
                            let editQuery = window.isEditMode ? '&edit=true' : '';
                            fetch('/api/buscar_articulo?q=' + encodeURIComponent(searchText) + editQuery)
                            .then(res => res.json())
                            .then(data => { renderizarTarjetas(data); });
                        }, 400); 
                    } 
                    else {
                        cards.forEach(card => {
                            const dataSku = card.getAttribute('data-sku');
                            if(dataSku && dataSku.toLowerCase().includes(searchText)) {
                                card.style.display = '';
                            } else {
                                card.style.display = 'none';
                            }
                        });
                    }
                }

                document.addEventListener('DOMContentLoaded', () => {
                    const sb = document.getElementById('searchBox');
                    if(sb) sb.addEventListener('keyup', searchSKU);
                });

                if ('serviceWorker' in navigator) {
                    window.addEventListener('load', () => {
                        navigator.serviceWorker.register('/sw.js').catch(err => console.log('Error instalando PWA:', err));
                    });
                }

                let globalDataVersion = -1;
                let poller = null;
                const TIEMPO_ACTUALIZACION = 1000;

                function chequearVersion() {
                    fetch('/api/version')
                    .then(res => res.text())
                    .then(versionText => {
                        let serverVersion = parseInt(versionText);
                        if (globalDataVersion === -1) {
                            globalDataVersion = serverVersion; 
                        } else if (serverVersion !== globalDataVersion) {
                            globalDataVersion = serverVersion;
                            actualizarVistaSilenciosamente();
                        }
                    }).catch(() => {});
                }

                function iniciarPoller() {
                    if (!poller) {
                        chequearVersion(); 
                        poller = setInterval(chequearVersion, TIEMPO_ACTUALIZACION);
                    }
                }

                function detenerPoller() {
                    if (poller) { clearInterval(poller); poller = null; }
                }

                document.addEventListener("visibilitychange", () => {
                    if (document.hidden) detenerPoller();
                    else iniciarPoller();
                });

                iniciarPoller();

                function actualizarVistaSilenciosamente() {
                    const path = window.location.pathname;

                    if (path === '/inventario' || path === '/') {
                        let openAccordions = [];
                        document.querySelectorAll('.accordion-collapse.show').forEach(el => openAccordions.push(el.id));

                        let editQuery = window.isEditMode ? '&edit=true' : '';

                        if (typeof currentState !== 'undefined') {
                            if (currentState.type === 'brand') {
                                fetch('/api/inventario_marca?marca=' + encodeURIComponent(currentState.query) + editQuery)
                                .then(res => res.json())
                                .then(data => { if(typeof renderizarTarjetas === 'function') renderizarTarjetas(data); restaurarAcordeones(openAccordions); });
                            }
                            else if (currentState.type === 'search') {
                                fetch('/api/buscar_articulo?q=' + encodeURIComponent(currentState.query) + editQuery)
                                .then(res => res.json())
                                .then(data => { if(typeof renderizarTarjetas === 'function') renderizarTarjetas(data); restaurarAcordeones(openAccordions); });
                            }
                            else {
                                sessionStorage.setItem('scrollPos', window.scrollY);
                                window.location.reload(); 
                            }
                        } else {
                            window.location.reload();
                        }
                    }
                    else if (path.includes('/crear_venta') || path.includes('/crear_remito') || path.includes('/scanner')) {
                        if (typeof marcaActual !== 'undefined' && marcaActual !== '') {
                            fetch('/api/inventario_marca?marca=' + encodeURIComponent(marcaActual))
                            .then(res => res.json())
                            .then(data => { 
                                if (typeof db !== 'undefined') {
                                    data.forEach(f => { if(f.nombre && !f.desc) f.desc = f.nombre; });
                                    db[marcaActual] = data; 
                                    let skuAbierto = typeof skuExpandido !== 'undefined' ? skuExpandido : '';
                                    if(typeof renderSKUs === 'function') renderSKUs(db[marcaActual]); 
                                    if (skuAbierto) {
                                        const idx = db[marcaActual].findIndex(f => f.sku === skuAbierto);
                                        if (idx !== -1) {
                                            let col = document.getElementById(`col-${idx}`);
                                            let btn = document.querySelector(`[data-bs-target="#col-${idx}"]`);
                                            if (col) col.classList.add('show');
                                            if (btn) btn.classList.remove('collapsed');
                                        }
                                    }
                                }
                            });
                        }
                        
                        let arrayCarrito = null;
                        if (typeof carrito !== 'undefined' && carrito.length > 0) arrayCarrito = carrito;
                        else if (typeof scanCart !== 'undefined' && scanCart.length > 0) arrayCarrito = scanCart;

                        if (arrayCarrito) {
                            let skusUnicos = [...new Set(arrayCarrito.map(item => item.sku))];
                            skusUnicos.forEach(sku => {
                                fetch('/api/buscar_articulo?q=' + encodeURIComponent(sku))
                                .then(res => res.json())
                                .then(data => {
                                    let cartChanged = false;
                                    data.forEach(fam => {
                                        if(fam.variantes) {
                                            fam.variantes.forEach(v => {
                                                arrayCarrito.forEach(cItem => {
                                                    if(cItem.sku === fam.sku && cItem.talle === v.talle && cItem.color === v.color) {
                                                        if(cItem.precio !== v.precio) {
                                                            cItem.precio = v.precio;
                                                            cartChanged = true;
                                                        }
                                                    }
                                                });
                                            });
                                        }
                                    });
                                    if(cartChanged) {
                                        if (typeof renderCarrito === 'function') renderCarrito(); 
                                        if (typeof renderCart === 'function') renderCart(); 
                                    }
                                });
                            });
                        }
                    }
                    else {
                        let openAccordions = [];
                        document.querySelectorAll('.accordion-collapse.show').forEach(el => openAccordions.push(el.id));
                        if (openAccordions.length > 0) sessionStorage.setItem('openHistorial', JSON.stringify(openAccordions));
                        sessionStorage.setItem('scrollPos', window.scrollY);

                        if (path.includes('/liquidar_remito') && typeof items !== 'undefined') {
                            let devoluciones = items.map(i => ({ sku: i.sku, talle: i.talle, color: i.color, devuelto: i.devuelto }));
                            sessionStorage.setItem('backupDevoluciones', JSON.stringify(devoluciones));
                        }
                        window.location.reload();
                    }
                }

                function restaurarAcordeones(ids) {
                    ids.forEach(id => {
                        let el = document.getElementById(id);
                        if(el) {
                            el.classList.add('show');
                            let btn = document.querySelector(`[data-bs-target="#${id}"]`);
                            if(btn) btn.classList.remove('collapsed');
                        }
                    });
                }

                document.addEventListener("DOMContentLoaded", () => {
                    let scrollPos = sessionStorage.getItem('scrollPos');
                    if (scrollPos) { window.scrollTo(0, parseInt(scrollPos)); sessionStorage.removeItem('scrollPos'); }
                    
                    let openHistorial = sessionStorage.getItem('openHistorial');
                    if (openHistorial) {
                        restaurarAcordeones(JSON.parse(openHistorial));
                        sessionStorage.removeItem('openHistorial');
                    }
                    
                    let backupDevoluciones = sessionStorage.getItem('backupDevoluciones');
                    if (backupDevoluciones && typeof items !== 'undefined' && typeof render === 'function') {
                        let devs = JSON.parse(backupDevoluciones);
                        items.forEach(item => {
                            let bk = devs.find(d => d.sku === item.sku && d.talle === item.talle && d.color === item.color);
                            if (bk) item.devuelto = bk.devuelto;
                        });
                        render();
                        sessionStorage.removeItem('backupDevoluciones');
                    }
                });
            </script>
        </body>
        </html>
        )HTML";
    }
}