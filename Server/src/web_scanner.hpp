#pragma once
#include "httplib.h"
#include "class.hpp"

namespace WebRoutes {
    void handleScanner(const httplib::Request& req, httplib::Response& res, Store* tienda);
    void handleApiScanBarcode(const httplib::Request& req, httplib::Response& res, Store* tienda);
    void handleApiLinkBarcode(const httplib::Request& req, httplib::Response& res, Store* tienda);
}