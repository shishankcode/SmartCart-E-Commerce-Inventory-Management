#pragma once

#include "apierror.h"
#include "db.h"
#include "json.h"

#include <string>

struct SessionUser {
    long long userId = 0;
    std::string role;
    std::string name;
    std::string email;
    std::string username;
};

namespace store {

void migrateSchema(Db& db);
void seedDefaults(Db& db);

std::string createSession(Db& db, long long userId, const std::string& role);
SessionUser requireSession(Db& db, const std::string& token);
SessionUser requireCustomer(Db& db, const std::string& token);
SessionUser requireAdmin(Db& db, const std::string& token);
void destroySession(Db& db, const std::string& token);
Json sessionInfo(const SessionUser& user);

Json registerCustomer(Db& db, const Json& body);
Json loginCustomer(Db& db, const Json& body);
Json loginAdmin(Db& db, const Json& body);

Json listProducts(Db& db);
Json createProduct(Db& db, const Json& body);
Json updateProduct(Db& db, long long productId, const Json& body);
void deleteProduct(Db& db, long long productId);

Json cartFor(Db& db, long long userId);
Json addToCart(Db& db, long long userId, const Json& body);
Json setCartQuantity(Db& db, long long userId, const Json& body);
Json removeCartItem(Db& db, long long userId, long long productId);

Json placeOrder(Db& db, const SessionUser& user, const Json& body);
Json ordersForCustomer(Db& db, long long userId);
Json requestCancellation(Db& db, long long userId, long long orderId);
void deleteCustomerOrder(Db& db, long long userId, long long orderId);

Json adminDashboard(Db& db);
Json approveCancellation(Db& db, long long orderId);
Json rejectCancellation(Db& db, long long orderId);
Json updateOrderStatus(Db& db, long long orderId, const Json& body);
Json resetDemoData(Db& db);

}  // namespace store
