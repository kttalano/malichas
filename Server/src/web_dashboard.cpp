#include "web_routes.hpp"
#include "web_utils.hpp"
#include "web_templates.hpp"
#include "web_auth.hpp"
#include <mutex>
#include <map>
#include <vector>
#include <algorithm>
#include <string>

extern std::recursive_mutex mutexTienda;

namespace WebRoutes
{

    struct MetricasVend
    {
        int tickets = 0;
        int prendas = 0;
        float ingresos = 0.0f;
        float deudaEnCalle = 0.0f;
    };

    struct Deudora
    {
        std::string nombre;
        float deuda = 0.0f;
        int remitos = 0;
    };

    void handleInicio(const httplib::Request &req, httplib::Response &res, Store *tienda)
    {
        std::string userEmail = req.get_header_value("Cf-Access-Authenticated-User-Email");
        std::string miNombre = WebAuth::obtenerNombreVendedora(userEmail);

        Rol usuarioRol = WebAuth::obtenerRolPorEmail(userEmail);

        std::string html = WebTemplates::getHeadAndNav("Inicio");

        html += R"(
            <div class='d-flex justify-content-between align-items-end mb-4 mt-2'>
                <h2 class='page-title mb-0'>Hola, )" +
                (miNombre.empty() ? "Administrador" : miNombre) + R"( 👋</h2>
            </div>

            <div class="app-grid">
        )";

        html += R"(
                <a href="/inventario" class="app-btn" style="grid-column: span 2; background: linear-gradient(135deg, var(--mali-primary), #7c52e4); color: white; border: none;">
                    <i class="fas fa-box-open" style="color: white; margin-bottom: 8px;"></i>
                    <span style="color: white; font-weight: 800;">Gestión de Stock</span>
                </a>
                
                <a href="/scanner" class="app-btn">
                    <i class="fas fa-barcode"></i><span>Leer Código</span>
                </a>
                
                <a href="/ventas" class="app-btn">
                    <i class="fas fa-cash-register"></i><span>Ventas</span>
                </a>
                
                <a href="/liquidaciones" class="app-btn">
                    <i class="fas fa-file-invoice-dollar"></i><span>Remitos</span>
                </a>
                
                <a href="/panel" class="app-btn">
                    <i class="fas fa-chart-pie"></i><span>Métricas</span>
                </a>
        )";

        if (usuarioRol == Rol::Supervisor || usuarioRol == Rol::Admin)
        {
            html += R"(
                <a href="/sincronizar_empretienda" class="app-btn">
                    <i class="fas fa-store"></i><span>Empretienda</span>
                </a>
                <a href="/importar_pdf" class="app-btn">
                    <i class="fas fa-file-pdf"></i><span>Cargar PDF</span>
                </a>
            )";
        }

        html += R"(
                <a href="#" onclick="cerrarSesionSeguro(); return false;" class="app-btn" style="grid-column: span 2; background: rgba(220, 53, 69, 0.05); border: 1px solid #dc3545;">
                    <i class="fas fa-sign-out-alt" style="color: #dc3545; margin-bottom: 8px;"></i>
                    <span style="color: #dc3545; font-weight: 800;">Cerrar Sesión</span>
                </a>
            </div>
        )";

        html += WebTemplates::getFooter("inicio");
        res.set_content(html, "text/html");
    }

    void handleDashboard(const httplib::Request &req, httplib::Response &res, Store *tienda)
    {
        std::string html = WebTemplates::getHeadAndNav("Dashboard");

        std::string userEmail = req.get_header_value("Cf-Access-Authenticated-User-Email");
        Rol usuarioRol = WebAuth::obtenerRolPorEmail(userEmail);
        std::string miNombre = WebAuth::obtenerNombreVendedora(userEmail);

        int totalStock = 0;
        int ventasMesGlobal = 0;
        float ingresosMesGlobal = 0.0f;
        int remitosPendientesGlobal = 0;
        float deudaTotalGlobal = 0.0f;
        std::map<std::string, MetricasVend> ranking;
        std::map<std::string, Deudora> globalDeudorasMap;

        int misTickets = 0;
        int misPrendas = 0;
        int misRemitos = 0;
        float miDeudaEnCalle = 0.0f;
        std::map<std::string, Deudora> misDeudorasMap;

        {

            std::lock_guard<std::recursive_mutex> lock(mutexTienda);

            for (const auto &fam : tienda->inventory)
            {
                for (const auto &var : fam.m_variantes)
                {
                    totalStock += var.m_stock;
                }
            }

            for (auto &v : tienda->sales)
            {

                if (v.m_totalAbonado <= 0.0f && !v.m_items.empty())
                {
                    float totalVenta = 0.0f;
                    for (const auto &item : v.m_items)
                        totalVenta += item.m_quantity * item.m_price;
                    v.m_totalAbonado = totalVenta;
                }

                if (WebUtils::esMesActual(v.m_fecha))
                {
                    if (usuarioRol == Rol::Admin || usuarioRol == Rol::Owner)
                    {
                        ventasMesGlobal++;
                        ingresosMesGlobal += v.m_totalAbonado;
                        std::string nombreVendedora = v.m_cliente.empty() ? "Desconocida" : v.m_cliente;
                        ranking[nombreVendedora].tickets++;
                        ranking[nombreVendedora].prendas += v.m_totalArticulos;
                        ranking[nombreVendedora].ingresos += v.m_totalAbonado;
                    }
                    if (v.m_cliente == miNombre)
                    {
                        misTickets++;
                        misPrendas += v.m_totalArticulos;
                    }
                }
            }

            for (auto &r : tienda->consignments)
            {
                if (r.m_estado == "Pendiente")
                {

                    r.RecalcularTotales();

                    std::string clienta = r.m_cliente.empty() ? "Sin Nombre" : r.m_cliente;

                    if (usuarioRol == Rol::Admin || usuarioRol == Rol::Owner)
                    {
                        remitosPendientesGlobal++;
                        deudaTotalGlobal += r.m_totalAPagar;
                        ranking[r.m_vendedora].deudaEnCalle += r.m_totalAPagar;

                        globalDeudorasMap[clienta].nombre = clienta;
                        globalDeudorasMap[clienta].deuda += r.m_totalAPagar;
                        globalDeudorasMap[clienta].remitos++;
                    }
                    if (r.m_vendedora == miNombre)
                    {
                        misRemitos++;
                        miDeudaEnCalle += r.m_totalAPagar;

                        misDeudorasMap[clienta].nombre = clienta;
                        misDeudorasMap[clienta].deuda += r.m_totalAPagar;
                        misDeudorasMap[clienta].remitos++;
                    }
                }
            }
        }

        std::vector<Deudora> topGlobalDeudoras;
        for (const auto &kv : globalDeudorasMap)
            topGlobalDeudoras.push_back(kv.second);
        std::sort(topGlobalDeudoras.begin(), topGlobalDeudoras.end(), [](const Deudora &a, const Deudora &b)
                  { return a.deuda > b.deuda; });

        std::vector<Deudora> topMisDeudoras;
        for (const auto &kv : misDeudorasMap)
            topMisDeudoras.push_back(kv.second);
        std::sort(topMisDeudoras.begin(), topMisDeudoras.end(), [](const Deudora &a, const Deudora &b)
                  { return a.deuda > b.deuda; });

        html += R"(
            <div class='d-flex justify-content-between align-items-end mb-3'>
                <h2 class='page-title mb-0'><i class='fas fa-chart-pie me-2 text-primary'></i>Panel de Métricas</h2>
                <span class='badge bg-light text-primary border px-3 py-2 rounded-pill'><i class='fas fa-calendar-alt me-1'></i> Mes Actual</span>
            </div>
            <div class='row g-3 mb-4'>
        )";

        if (usuarioRol == Rol::Admin || usuarioRol == Rol::Owner)
        {

            html += R"(
                <div class='col-6 col-md-3'>
                    <div class='card-glass text-center p-3 h-100' style='border-bottom: 4px solid #28a745;'>
                        <i class='fas fa-wallet fs-2 text-success'></i>
                        <h4 class='fw-bold mt-2 mb-0'>)" +
                    WebUtils::formatMoney(ingresosMesGlobal) + R"(</h4>
                        <small class='text-muted text-uppercase fw-semibold' style='font-size: 0.65rem'>Ingresos del Mes</small>
                    </div>
                </div>
                <div class='col-6 col-md-3'>
                    <div class='card-glass text-center p-3 h-100' style='border-bottom: 4px solid #dc3545;'>
                        <i class='fas fa-hand-holding-usd fs-2 text-danger'></i>
                        <h4 class='fw-bold mt-2 mb-0'>)" +
                    WebUtils::formatMoney(deudaTotalGlobal) + R"(</h4>
                        <small class='text-muted text-uppercase fw-semibold' style='font-size: 0.65rem'>Deuda en Calle</small>
                    </div>
                </div>
                <div class='col-6 col-md-3'>
                    <div class='card-glass text-center p-3 h-100'>
                        <i class='fas fa-tags fs-2' style='color: var(--mali-primary)'></i>
                        <h4 class='fw-bold mt-2 mb-0'>)" +
                    std::to_string(totalStock) + R"(</h4>
                        <small class='text-muted text-uppercase fw-semibold' style='font-size: 0.65rem'>Prendas en Local</small>
                    </div>
                </div>
                <div class='col-6 col-md-3'>
                    <div class='card-glass text-center p-3 h-100'>
                        <i class='fas fa-shopping-bag fs-2 text-info'></i>
                        <h4 class='fw-bold mt-2 mb-0'>)" +
                    std::to_string(ventasMesGlobal) + R"(</h4>
                        <small class='text-muted text-uppercase fw-semibold' style='font-size: 0.65rem'>Tickets de Venta</small>
                    </div>
                </div>
            </div>

            <div class='row g-3'>
                <div class='col-12 col-lg-7'>
                    <h5 class='fw-bold mt-2 mb-3' style='color: var(--mali-secondary);'><i class='fas fa-users text-primary me-2'></i>Rendimiento del Equipo</h5>
                    <div class='card-glass p-0 overflow-hidden mb-4'>
                        <div class='table-responsive'>
                            <table class='table table-borderless table-hover mb-0 text-center align-middle' style='--bs-table-bg: transparent; --bs-table-color: var(--mali-text);'>
                                <thead style='background-color: rgba(92, 51, 190, 0.05);'>
                                    <tr>
                                        <th class='text-start ps-3 text-muted small text-uppercase'>Vendedora</th>
                                        <th class='text-muted small text-uppercase'>Ingresos</th>
                                        <th class='text-end pe-3 text-muted small text-uppercase'>Deuda a Cobrar</th>
                                    </tr>
                                </thead>
                                <tbody>
            )";

            if (ranking.empty())
            {
                html += "<tr><td colspan='3' class='text-center py-4 text-muted'>No hay datos registrados en este período.</td></tr>";
            }
            else
            {
                for (const auto &[nombre, data] : ranking)
                {
                    html += R"(
                        <tr style='border-bottom: 1px solid var(--mali-border);'>
                            <td class='text-start ps-3 fw-bold' style='color: var(--mali-text);'>)" +
                            nombre + R"(</td>
                            <td class='fw-bold text-success'>)" +
                            WebUtils::formatMoney(data.ingresos) + R"(<br><small class='text-muted fw-normal' style='font-size:0.7rem;'>)" + std::to_string(data.tickets) + " ventas</small></td>" + R"(
                            <td class='text-end pe-3 fw-bold text-danger'>)" +
                            WebUtils::formatMoney(data.deudaEnCalle) + R"(</td>
                        </tr>
                    )";
                }
            }

            html += R"(
                                </tbody>
                            </table>
                        </div>
                    </div>
                </div>

                <div class='col-12 col-lg-5'>
                    <h5 class='fw-bold mt-2 mb-3' style='color: var(--mali-secondary);'><i class='fas fa-exclamation-triangle text-danger me-2'></i>Top Morosas (Global)</h5>
                    <div class='card-glass p-3'>
            )";

            if (topGlobalDeudoras.empty())
            {
                html += "<div class='text-center text-muted p-4 small'>No hay deudas pendientes en el local. ¡Excelente!</div>";
            }
            else
            {
                int limit = std::min(5, (int)topGlobalDeudoras.size());
                for (int i = 0; i < limit; i++)
                {
                    std::string badgeColor = (i == 0) ? "bg-danger" : "bg-warning text-dark";
                    html += R"(
                        <div class='d-flex justify-content-between align-items-center mb-3 pb-2 border-bottom'>
                            <div>
                                <span class='fw-bold' style='color: var(--mali-text);'>)" +
                            topGlobalDeudoras[i].nombre + R"(</span>
                                <small class='d-block text-muted' style='font-size:0.75rem;'>)" +
                            std::to_string(topGlobalDeudoras[i].remitos) + R"( bolsos activos</small>
                            </div>
                            <span class='badge )" +
                            badgeColor + R"( fs-6 rounded-pill'>)" + WebUtils::formatMoney(topGlobalDeudoras[i].deuda) + R"(</span>
                        </div>
                    )";
                }
            }
            html += "</div></div></div>";
        }
        else
        {

            html += R"(
                <div class='col-12'>
                    <div class='card-glass text-center p-4 mb-2 shadow-sm' style='background: linear-gradient(135deg, var(--mali-primary), #7c52e4); border: none;'>
                        <i class='fas fa-chart-line fs-1 text-white mb-2'></i>
                        <h3 class='fw-bold text-white mb-0'>Mi Rendimiento</h3>
                        <small class='text-white-50'>Tus estadísticas comerciales personales de este mes</small>
                    </div>
                </div>
                
                <div class='col-6'>
                    <div class='card-glass text-center p-3 h-100' style='border-left: 4px solid #0d6efd;'>
                        <i class='fas fa-shopping-bag fs-2 text-primary'></i>
                        <h4 class='fw-bold mt-2 mb-0'>)" +
                    std::to_string(misTickets) + R"(</h4>
                        <small class='text-muted text-uppercase fw-semibold' style='font-size: 0.65rem; line-height: 1.1;'>Ventas<br>Directas</small>
                    </div>
                </div>
                <div class='col-6'>
                    <div class='card-glass text-center p-3 h-100' style='border-left: 4px solid #dc3545;'>
                        <i class='fas fa-hand-holding-usd fs-2 text-danger'></i>
                        <h4 class='fw-bold mt-2 mb-0'>)" +
                    WebUtils::formatMoney(miDeudaEnCalle) + R"(</h4>
                        <small class='text-muted text-uppercase fw-semibold' style='font-size: 0.65rem; line-height: 1.1;'>Plata a<br>Cobrar</small>
                    </div>
                </div>
            </div>

            <h5 class='fw-bold mt-4 mb-3' style='color: var(--mali-secondary);'><i class='fas fa-bullseye text-warning me-2'></i>Tus Mayores Deudoras</h5>
            <div class='card-glass p-3 mb-4'>
            )";

            if (topMisDeudoras.empty())
            {
                html += "<div class='text-center text-muted p-4 small'><i class='fas fa-check-circle text-success fs-3 mb-2 d-block'></i>No tenés plata en la calle.<br>¡Buen trabajo!</div>";
            }
            else
            {
                int limit = std::min(3, (int)topMisDeudoras.size());
                for (int i = 0; i < limit; i++)
                {
                    html += R"(
                        <div class='d-flex justify-content-between align-items-center mb-3 pb-2 border-bottom'>
                            <div>
                                <span class='fw-bold' style='color: var(--mali-text);'>)" +
                            topMisDeudoras[i].nombre + R"(</span>
                                <small class='d-block text-muted' style='font-size:0.75rem;'>)" +
                            std::to_string(topMisDeudoras[i].remitos) + R"( bolsos activos</small>
                            </div>
                            <span class='fw-bold text-danger fs-6'>)" +
                            WebUtils::formatMoney(topMisDeudoras[i].deuda) + R"(</span>
                        </div>
                    )";
                }
                html += "<div class='text-center mt-3'><a href='/liquidaciones' class='btn btn-sm btn-outline-danger rounded-pill fw-bold px-4'>Ir a cobrarles</a></div>";
            }

            html += R"(
            </div>
            
            <div class='alert alert-light border shadow-sm text-center mb-5'>
                <i class='fas fa-info-circle text-primary mb-2 fs-4'></i>
                <p class='mb-0 small text-muted'>Para registrar operaciones nuevas, usá los botones <strong>Ventas</strong> o <strong>Remitos</strong> de la barra de abajo.</p>
            </div>
            )";
        }

        html += "</div>" + WebTemplates::getFooter("dashboard");
        res.set_content(html, "text/html");
    }
}