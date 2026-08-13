#pragma once
#include "httplib.h"
#include "class.hpp"

namespace WebRoutes
{

    void handleInicio(const httplib::Request &req, httplib::Response &res, Store *tienda);
    void handleDashboard(const httplib::Request &req, httplib::Response &res, Store *tienda);
    void handleInventario(const httplib::Request &req, httplib::Response &res, Store *tienda);
    void handleVentas(const httplib::Request &req, httplib::Response &res, Store *tienda);
    void handleLiquidaciones(const httplib::Request &req, httplib::Response &res, Store *tienda);

    void handleSeguimiento(const httplib::Request &req, httplib::Response &res, Store *tienda);
    void handleApiActualizarVarianteEmpretienda(const httplib::Request &req, httplib::Response &res, Store *tienda);

    void handleApiHistorialSku(const httplib::Request &req, httplib::Response &res, Store *tienda);
    void handleApiEditarInventario(const httplib::Request &req, httplib::Response &res, Store *tienda);
    void handleApiAgregarVariante(const httplib::Request &req, httplib::Response &res, Store *tienda);

    void handleSincronizarEmpretienda(const httplib::Request &req, httplib::Response &res, Store *tienda);
    void handleApiProcesarEmpretienda(const httplib::Request &req, httplib::Response &res, Store *tienda);
    void handleApiCatalogoCompleto(const httplib::Request &req, httplib::Response &res, Store *tienda);
    void handleApiDownloadSync(const httplib::Request &req, httplib::Response &res, Store *tienda);

    void handleApiAplicarCambiosEmpretienda(const httplib::Request &req, httplib::Response &res, Store *tienda);
    void handleApiRevertirCambiosEmpretienda(const httplib::Request &req, httplib::Response &res, Store *tienda);

    void handleSubirPDF(const httplib::Request &req, httplib::Response &res, Store *tienda);
    void handleProcesarPDF(const httplib::Request &req, httplib::Response &res, Store *tienda);
    void handleStatusPDF(const httplib::Request &req, httplib::Response &res, Store *tienda);
    void handleEditorPDF(const httplib::Request &req, httplib::Response &res, Store *tienda);
    void handleConfirmarPDF(const httplib::Request &req, httplib::Response &res, Store *tienda);

    void handleCrearVenta(const httplib::Request &req, httplib::Response &res, Store *tienda);
    void handleCrearRemito(const httplib::Request &req, httplib::Response &res, Store *tienda);

    void handleLiquidarRemito(const httplib::Request &req, httplib::Response &res, Store *tienda);
    void handleApiLiquidarRemito(const httplib::Request &req, httplib::Response &res, Store *tienda);

    void handleApiGetVentas(const httplib::Request &req, httplib::Response &res, Store *tienda);
    void handleApiGetLiquidaciones(const httplib::Request &req, httplib::Response &res, Store *tienda);

    void handleApiBuscarArticulo(const httplib::Request &req, httplib::Response &res, Store *tienda);
    void handleApiInventarioMarca(const httplib::Request &req, httplib::Response &res, Store *tienda);
    void handleApiGuardarVenta(const httplib::Request &req, httplib::Response &res, Store *tienda);
    void handleApiGuardarRemito(const httplib::Request &req, httplib::Response &res, Store *tienda);
}