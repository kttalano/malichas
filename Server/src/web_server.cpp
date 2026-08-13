#include "web_server.h"
#include "web_routes.hpp"
#include "httplib.h"
#include "web_scanner.hpp"
#include <mutex>
#include "class.hpp"

extern std::recursive_mutex mutexTienda;

namespace WebServer
{
    httplib::Server *svr_ptr = nullptr;
    void Iniciar(Store *tienda)
    {
        httplib::Server svr;
        svr_ptr = &svr;

        svr.Get("/", [tienda](const httplib::Request &req, httplib::Response &res)
                { WebRoutes::handleInicio(req, res, tienda); });
        svr.Get("/panel", [tienda](const httplib::Request &req, httplib::Response &res)
                { WebRoutes::handleDashboard(req, res, tienda); });
        svr.Get("/inventario", [tienda](const httplib::Request &req, httplib::Response &res)
                { WebRoutes::handleInventario(req, res, tienda); });

        svr.Get("/ventas", [tienda](const httplib::Request &req, httplib::Response &res)
                { WebRoutes::handleCrearVenta(req, res, tienda); });
        svr.Get("/liquidaciones", [tienda](const httplib::Request &req, httplib::Response &res)
                { WebRoutes::handleCrearRemito(req, res, tienda); });

        svr.Get("/historial_ventas", [tienda](const httplib::Request &req, httplib::Response &res)
                { WebRoutes::handleVentas(req, res, tienda); });
        svr.Get("/historial_liquidaciones", [tienda](const httplib::Request &req, httplib::Response &res)
                { WebRoutes::handleLiquidaciones(req, res, tienda); });

        svr.Get("/seguimiento", [tienda](const httplib::Request &req, httplib::Response &res)
                { WebRoutes::handleSeguimiento(req, res, tienda); });

        svr.Get("/api/historial_sku", [tienda](const httplib::Request &req, httplib::Response &res)
                { WebRoutes::handleApiHistorialSku(req, res, tienda); });
        svr.Post("/api/editar_inventario", [tienda](const httplib::Request &req, httplib::Response &res)
                 { WebRoutes::handleApiEditarInventario(req, res, tienda); });
        svr.Post("/api/agregar_variante", [tienda](const httplib::Request &req, httplib::Response &res)
                 { WebRoutes::handleApiAgregarVariante(req, res, tienda); });

        svr.Get("/sincronizar_empretienda", [tienda](const httplib::Request &req, httplib::Response &res)
                { WebRoutes::handleSincronizarEmpretienda(req, res, tienda); });
        svr.Post("/api/procesar_empretienda", [tienda](const httplib::Request &req, httplib::Response &res)
                 { WebRoutes::handleApiProcesarEmpretienda(req, res, tienda); });
        svr.Get("/api/catalogo_completo", [&](const httplib::Request &req, httplib::Response &res)
                { WebRoutes::handleApiCatalogoCompleto(req, res, tienda); });
        svr.Get("/api/download_sync", [tienda](const httplib::Request &req, httplib::Response &res)
                { WebRoutes::handleApiDownloadSync(req, res, tienda); });

        svr.Post("/api/aplicar_cambios_empretienda", [tienda](const httplib::Request &req, httplib::Response &res)
                 { WebRoutes::handleApiAplicarCambiosEmpretienda(req, res, tienda); });
        svr.Post("/api/revertir_cambios_empretienda", [tienda](const httplib::Request &req, httplib::Response &res)
                 { WebRoutes::handleApiRevertirCambiosEmpretienda(req, res, tienda); });

        svr.Get("/api/get_ventas", [tienda](const httplib::Request &req, httplib::Response &res)
                { WebRoutes::handleApiGetVentas(req, res, tienda); });
        svr.Get("/api/get_liquidaciones", [tienda](const httplib::Request &req, httplib::Response &res)
                { WebRoutes::handleApiGetLiquidaciones(req, res, tienda); });

        svr.Get("/importar_pdf", [tienda](const httplib::Request &req, httplib::Response &res)
                { WebRoutes::handleSubirPDF(req, res, tienda); });
        svr.Post("/procesar_pdf", [tienda](const httplib::Request &req, httplib::Response &res)
                 { WebRoutes::handleProcesarPDF(req, res, tienda); });
        svr.Post("/confirmar_pdf", [tienda](const httplib::Request &req, httplib::Response &res)
                 { WebRoutes::handleConfirmarPDF(req, res, tienda); });
        svr.Get("/status_pdf", [tienda](const httplib::Request &req, httplib::Response &res)
                { WebRoutes::handleStatusPDF(req, res, tienda); });
        svr.Get("/editor_pdf", [tienda](const httplib::Request &req, httplib::Response &res)
                { WebRoutes::handleEditorPDF(req, res, tienda); });

        svr.Get("/api/version", [tienda](const httplib::Request &req, httplib::Response &res)
                {
            unsigned int versionActual;
            {
                std::lock_guard<std::recursive_mutex> lock(mutexTienda);
                versionActual = tienda->dataVersion;
            }
            res.set_content(std::to_string(versionActual), "text/plain"); });

        svr.Get("/api/buscar_articulo", [tienda](const httplib::Request &req, httplib::Response &res)
                { WebRoutes::handleApiBuscarArticulo(req, res, tienda); });
        svr.Post("/api/ventas", [tienda](const httplib::Request &req, httplib::Response &res)
                 { WebRoutes::handleApiGuardarVenta(req, res, tienda); });
        svr.Post("/api/remitos", [tienda](const httplib::Request &req, httplib::Response &res)
                 { WebRoutes::handleApiGuardarRemito(req, res, tienda); });

        svr.Get("/scanner", [tienda](const httplib::Request &req, httplib::Response &res)
                { WebRoutes::handleScanner(req, res, tienda); });
        svr.Get("/api/scan_barcode", [tienda](const httplib::Request &req, httplib::Response &res)
                { WebRoutes::handleApiScanBarcode(req, res, tienda); });
        svr.Post("/api/link_barcode", [tienda](const httplib::Request &req, httplib::Response &res)
                 { WebRoutes::handleApiLinkBarcode(req, res, tienda); });

        svr.Get("/liquidar_remito", [tienda](const httplib::Request &req, httplib::Response &res)
                { WebRoutes::handleLiquidarRemito(req, res, tienda); });
        svr.Post("/api/liquidar_remito", [tienda](const httplib::Request &req, httplib::Response &res)
                 { WebRoutes::handleApiLiquidarRemito(req, res, tienda); });
        svr.Get("/api/inventario_marca", [tienda](const httplib::Request &req, httplib::Response &res)
                { WebRoutes::handleApiInventarioMarca(req, res, tienda); });

        svr.Get("/manifest.json", [](const httplib::Request &req, httplib::Response &res)
                {
            std::string manifest = R"({
                "name": "",
                "short_name": "Malichas",
                "start_url": "/",
                "display": "standalone",
                "background_color": "#f5f4f9",
                "theme_color": "#a92adb",
                "orientation": "portrait",
                "icons": [
                    {
                        "src": "/logo_512.png",
                        "sizes": "192x192 512x512",
                        "type": "image/png",
                        "purpose": "any maskable"
                    }
                ]
            })";
            res.set_content(manifest, "application/json"); });

        svr.Get("/sw.js", [](const httplib::Request &req, httplib::Response &res)
                {
            std::string sw = R"(
                self.addEventListener('install', (e) => {
                    self.skipWaiting();
                });
                self.addEventListener('activate', (e) => {
                    e.waitUntil(clients.claim());
                });
                self.addEventListener('fetch', (e) => {
                    e.respondWith(
                        fetch(e.request).catch(() => new Response('Malichas: Sin conexion a internet.'))
                    );
                });
            )";
            res.set_content(sw, "application/javascript"); });

        svr.Get("/logo_512.png", [](const httplib::Request &req, httplib::Response &res)
                {
            std::ifstream file("logo_512.png", std::ios::binary);
            if (file) {
                std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                res.set_content(content, "image/png");
            } else {
                res.status = 404;
            } });
        svr.listen("0.0.0.0", 8080);
    }
    void Detener()
    {
        if (svr_ptr)
        {
            svr_ptr->stop();
            Logger::Log("Puerto 8080 del Servidor Web liberado.");
        }
    }
}