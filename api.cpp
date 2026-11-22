#include "api.h"

#include "apierror.h"
#include "json.h"
#include "store.h"

#include <iostream>
#include <string>

namespace {

Db openDb(const AppConfig& config) {
    Db db;
    std::string error;
    if (!db.connect(config.mysql, error)) {
        std::cerr << "[db] connection failed: " << error << std::endl;
        throw ApiError{500, "Database connection failed."};
    }
    return db;
}

Json parseBody(const HttpRequest& request) {
    if (request.body.empty()) {
        return Json::object();
    }
    try {
        return Json::parse(request.body);
    } catch (const JsonParseError&) {
        throw ApiError{400, "Invalid JSON body."};
    }
}

std::string bearerToken(const HttpRequest& request) {
    const std::string header = request.header("authorization");
    const std::string prefix = "Bearer ";
    if (header.size() <= prefix.size() || header.compare(0, prefix.size(), prefix) != 0) {
        return "";
    }
    return header.substr(prefix.size());
}

long long pathId(const HttpRequest& request, const char* name) {
    auto it = request.params.find(name);
    if (it == request.params.end()) {
        throw ApiError{404, "Not found."};
    }
    try {
        std::size_t consumed = 0;
        const long long value = std::stoll(it->second, &consumed);
        if (consumed != it->second.size()) {
            throw std::invalid_argument("trailing characters");
        }
        return value;
    } catch (...) {
        throw ApiError{404, "Not found."};
    }
}

Json okResponse() {
    Json body = Json::object();
    body["ok"] = true;
    return body;
}

}  // namespace

void registerApiRoutes(HttpServer& server, const AppConfig& config) {
    server.get("/api/health", [](HttpRequest&) {
        Json body = Json::object();
        body["status"] = "ok";
        return HttpResponse::json(body);
    });

    // ---------- authentication ----------

    server.post("/api/auth/register", [&config](HttpRequest& request) {
        Db db = openDb(config);
        return HttpResponse::json(store::registerCustomer(db, parseBody(request)), 201);
    });

    server.post("/api/auth/login", [&config](HttpRequest& request) {
        Db db = openDb(config);
        return HttpResponse::json(store::loginCustomer(db, parseBody(request)));
    });

    server.post("/api/auth/admin-login", [&config](HttpRequest& request) {
        Db db = openDb(config);
        return HttpResponse::json(store::loginAdmin(db, parseBody(request)));
    });

    server.post("/api/auth/logout", [&config](HttpRequest& request) {
        Db db = openDb(config);
        store::destroySession(db, bearerToken(request));
        return HttpResponse::json(okResponse());
    });

    server.get("/api/auth/me", [&config](HttpRequest& request) {
        Db db = openDb(config);
        const SessionUser user = store::requireSession(db, bearerToken(request));
        return HttpResponse::json(store::sessionInfo(user));
    });

    // ---------- products ----------

    server.get("/api/products", [&config](HttpRequest&) {
        Db db = openDb(config);
        Json body = Json::object();
        body["products"] = store::listProducts(db);
        return HttpResponse::json(body);
    });

    server.post("/api/products", [&config](HttpRequest& request) {
        Db db = openDb(config);
        store::requireAdmin(db, bearerToken(request));
        return HttpResponse::json(store::createProduct(db, parseBody(request)), 201);
    });

    server.put("/api/products/:id", [&config](HttpRequest& request) {
        Db db = openDb(config);
        store::requireAdmin(db, bearerToken(request));
        const long long productId = pathId(request, "id");
        return HttpResponse::json(store::updateProduct(db, productId, parseBody(request)));
    });

    server.del("/api/products/:id", [&config](HttpRequest& request) {
        Db db = openDb(config);
        store::requireAdmin(db, bearerToken(request));
        store::deleteProduct(db, pathId(request, "id"));
        return HttpResponse::json(okResponse());
    });

    // ---------- cart (customer) ----------

    server.get("/api/cart", [&config](HttpRequest& request) {
        Db db = openDb(config);
        const SessionUser user = store::requireCustomer(db, bearerToken(request));
        return HttpResponse::json(store::cartFor(db, user.userId));
    });

    server.post("/api/cart/items", [&config](HttpRequest& request) {
        Db db = openDb(config);
        const SessionUser user = store::requireCustomer(db, bearerToken(request));
        return HttpResponse::json(store::addToCart(db, user.userId, parseBody(request)));
    });

    server.put("/api/cart/items", [&config](HttpRequest& request) {
        Db db = openDb(config);
        const SessionUser user = store::requireCustomer(db, bearerToken(request));
        return HttpResponse::json(store::setCartQuantity(db, user.userId, parseBody(request)));
    });

    server.del("/api/cart/items/:productId", [&config](HttpRequest& request) {
        Db db = openDb(config);
        const SessionUser user = store::requireCustomer(db, bearerToken(request));
        return HttpResponse::json(
            store::removeCartItem(db, user.userId, pathId(request, "productId")));
    });

    // ---------- orders ----------

    server.post("/api/orders", [&config](HttpRequest& request) {
        Db db = openDb(config);
        const SessionUser user = store::requireCustomer(db, bearerToken(request));
        return HttpResponse::json(store::placeOrder(db, user, parseBody(request)), 201);
    });

    server.get("/api/orders/mine", [&config](HttpRequest& request) {
        Db db = openDb(config);
        const SessionUser user = store::requireCustomer(db, bearerToken(request));
        Json body = Json::object();
        body["orders"] = store::ordersForCustomer(db, user.userId);
        return HttpResponse::json(body);
    });

    server.post("/api/orders/:id/cancel", [&config](HttpRequest& request) {
        Db db = openDb(config);
        const SessionUser user = store::requireCustomer(db, bearerToken(request));
        return HttpResponse::json(
            store::requestCancellation(db, user.userId, pathId(request, "id")));
    });

    server.del("/api/orders/:id", [&config](HttpRequest& request) {
        Db db = openDb(config);
        const SessionUser user = store::requireCustomer(db, bearerToken(request));
        store::deleteCustomerOrder(db, user.userId, pathId(request, "id"));
        return HttpResponse::json(okResponse());
    });

    // ---------- admin ----------

    server.get("/api/admin/dashboard", [&config](HttpRequest& request) {
        Db db = openDb(config);
        store::requireAdmin(db, bearerToken(request));
        return HttpResponse::json(store::adminDashboard(db));
    });

    server.post("/api/orders/:id/approve-cancel", [&config](HttpRequest& request) {
        Db db = openDb(config);
        store::requireAdmin(db, bearerToken(request));
        return HttpResponse::json(store::approveCancellation(db, pathId(request, "id")));
    });

    server.post("/api/orders/:id/reject-cancel", [&config](HttpRequest& request) {
        Db db = openDb(config);
        store::requireAdmin(db, bearerToken(request));
        return HttpResponse::json(store::rejectCancellation(db, pathId(request, "id")));
    });

    server.put("/api/orders/:id/status", [&config](HttpRequest& request) {
        Db db = openDb(config);
        store::requireAdmin(db, bearerToken(request));
        return HttpResponse::json(
            store::updateOrderStatus(db, pathId(request, "id"), parseBody(request)));
    });

    server.post("/api/admin/reset", [&config](HttpRequest& request) {
        Db db = openDb(config);
        store::requireAdmin(db, bearerToken(request));
        return HttpResponse::json(store::resetDemoData(db));
    });
}
