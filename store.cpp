#include "store.h"

#include "crypto.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <set>
#include <sstream>
#include <vector>

namespace {

const int PASSWORD_ITERATIONS = 60000;
const int LOW_STOCK_LIMIT = 5;

[[noreturn]] void fail(int status, const std::string& message) {
    throw ApiError{status, message};
}

std::string valueOf(const Row& row, const char* column) {
    auto it = row.find(column);
    return it == row.end() ? std::string() : it->second;
}

long long intOf(const Row& row, const char* column) {
    const std::string value = valueOf(row, column);
    return value.empty() ? 0 : std::stoll(value);
}

double numberOf(const Row& row, const char* column) {
    const std::string value = valueOf(row, column);
    return value.empty() ? 0.0 : std::stod(value);
}

void logDbError(const std::string& error) {
    std::cerr << "[db] " << error << std::endl;
}

void execOrFail(Db& db, const std::string& sql) {
    std::string error;
    if (!db.exec(sql, error)) {
        logDbError(error);
        fail(500, "Database error.");
    }
}

std::vector<Row> queryOrFail(Db& db, const std::string& sql) {
    std::vector<Row> rows;
    std::string error;
    if (!db.select(sql, rows, error)) {
        logDbError(error);
        fail(500, "Database error.");
    }
    return rows;
}

class Transaction {
public:
    explicit Transaction(Db& db) : db_(db) {
        execOrFail(db_, "START TRANSACTION");
    }

    ~Transaction() {
        if (!committed_) {
            std::string ignored;
            db_.exec("ROLLBACK", ignored);
        }
    }

    void commit() {
        execOrFail(db_, "COMMIT");
        committed_ = true;
    }

private:
    Db& db_;
    bool committed_ = false;
};

std::string trim(const std::string& value) {
    const std::size_t start = value.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    const std::size_t end = value.find_last_not_of(" \t\r\n");
    return value.substr(start, end - start + 1);
}

std::string lowercase(std::string value) {
    for (char& c : value) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return value;
}

bool allDigits(const std::string& value, std::size_t expectedLength) {
    if (value.size() != expectedLength) {
        return false;
    }
    for (char c : value) {
        if (c < '0' || c > '9') {
            return false;
        }
    }
    return true;
}

bool looksLikeEmail(const std::string& email) {
    const std::size_t at = email.find('@');
    if (at == std::string::npos || at == 0) {
        return false;
    }
    const std::size_t dot = email.find('.', at);
    if (dot == std::string::npos || dot <= at + 1 || dot + 1 >= email.size()) {
        return false;
    }
    if (email.find(' ') != std::string::npos) {
        return false;
    }
    return true;
}

std::string money(double value) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.2f", value);
    return buffer;
}

double round2(double value) {
    return std::round(value * 100.0) / 100.0;
}

long long nowMillis() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

std::string requiredString(const Json& body,
                           const char* key,
                           const char* label,
                           std::size_t maxLength,
                           bool allowEmpty = false) {
    const Json* value = body.find(key);
    std::string text = value && value->isString() ? trim(value->asString()) : "";
    if (!allowEmpty && text.empty()) {
        fail(400, std::string(label) + " is required.");
    }
    if (text.size() > maxLength) {
        fail(400, std::string(label) + " is too long.");
    }
    return text;
}

double requiredNumber(const Json& body, const char* key, const char* label) {
    const Json* value = body.find(key);
    if (!value) {
        fail(400, std::string(label) + " is required.");
    }
    if (value->isNumber()) {
        return value->asNumber();
    }
    if (value->isString()) {
        const std::string text = trim(value->asString());
        char* end = nullptr;
        double parsed = std::strtod(text.c_str(), &end);
        if (!text.empty() && end && *end == '\0') {
            return parsed;
        }
    }
    fail(400, std::string(label) + " must be a number.");
}

long long requiredInt(const Json& body, const char* key, const char* label) {
    const Json* value = body.find(key);
    if (!value) {
        fail(400, std::string(label) + " is required.");
    }
    if (value->isNumber()) {
        return value->asInt();
    }
    if (value->isString()) {
        const std::string text = trim(value->asString());
        if (allDigits(text, text.size()) && !text.empty()) {
            return std::stoll(text);
        }
    }
    fail(400, std::string(label) + " must be a whole number.");
}

std::string newSalt() {
    return randomHex(16);
}

std::string hashPassword(const std::string& password, const std::string& saltHex) {
    return hexEncode(pbkdf2Sha256(password, hexDecode(saltHex), PASSWORD_ITERATIONS, 32));
}

bool passwordMatches(const std::string& password, const Row& row) {
    const std::string storedHash = valueOf(row, "password_hash");
    const std::string salt = valueOf(row, "password_salt");
    if (storedHash.empty() || salt.empty()) {
        return false;
    }
    const std::string candidate = hashPassword(password, salt);
    return constantTimeEquals(candidate, storedHash);
}

Json userJson(const Row& row) {
    Json user = Json::object();
    user["id"] = intOf(row, "id");
    user["name"] = valueOf(row, "name");
    user["email"] = valueOf(row, "email");
    return user;
}

Json productJson(const Row& row) {
    Json product = Json::object();
    product["id"] = intOf(row, "id");
    product["name"] = valueOf(row, "name");
    product["category"] = valueOf(row, "category");
    product["price"] = numberOf(row, "price");
    product["stock"] = intOf(row, "stock");
    return product;
}

Json itemJson(const Row& row) {
    Json item = Json::object();
    item["productId"] = intOf(row, "product_id");
    item["productName"] = valueOf(row, "product_name");
    item["price"] = numberOf(row, "price");
    item["quantity"] = intOf(row, "quantity");
    item["total"] = numberOf(row, "line_total");
    return item;
}

Json orderFromRow(const Row& row) {
    Json order = Json::object();
    order["id"] = intOf(row, "id");
    order["customerId"] = intOf(row, "customer_id");
    order["customerName"] = valueOf(row, "customer_name");
    order["customerEmail"] = valueOf(row, "customer_email");
    order["phone"] = valueOf(row, "phone");
    order["address"] = valueOf(row, "address");
    order["pincode"] = valueOf(row, "pincode");
    order["paymentMethod"] = valueOf(row, "payment_method");
    order["subtotal"] = numberOf(row, "subtotal");
    order["discount"] = numberOf(row, "discount");
    order["total"] = numberOf(row, "total");
    order["status"] = valueOf(row, "status");

    const bool cancellationRequested = valueOf(row, "cancellation_requested") == "1";
    order["cancellationRequested"] = cancellationRequested;

    const std::string requestedAt = valueOf(row, "cancel_requested_at_iso");
    order["cancellationRequestedAt"] = requestedAt.empty() ? Json() : Json(requestedAt);

    const std::string previousStatus = valueOf(row, "previous_status");
    order["previousStatus"] = previousStatus.empty() ? Json() : Json(previousStatus);

    order["cancellationApproved"] = valueOf(row, "cancellation_approved") == "1";
    order["cancellationRejected"] = valueOf(row, "cancellation_rejected") == "1";

    const std::string resolvedAt = valueOf(row, "resolved_at_iso");
    order["cancellationResolvedAt"] = resolvedAt.empty() ? Json() : Json(resolvedAt);

    order["date"] = valueOf(row, "created_at_iso");
    return order;
}

const char* kOrderSelect =
    "SELECT o.id, o.customer_id, o.customer_name, o.customer_email, o.phone, o.address, "
    "o.pincode, o.payment_method, o.subtotal, o.discount, o.total, o.status, "
    "o.cancellation_requested, o.previous_status, o.cancellation_approved, "
    "o.cancellation_rejected, "
    "DATE_FORMAT(o.created_at, '%Y-%m-%dT%H:%i:%sZ') AS created_at_iso, "
    "DATE_FORMAT(o.cancellation_requested_at, '%Y-%m-%dT%H:%i:%sZ') AS cancel_requested_at_iso, "
    "DATE_FORMAT(o.cancellation_resolved_at, '%Y-%m-%dT%H:%i:%sZ') AS resolved_at_iso "
    "FROM orders o ";

std::vector<Json> loadOrders(Db& db, const std::string& clause) {
    const std::vector<Row> rows = queryOrFail(db, std::string(kOrderSelect) + clause);
    if (rows.empty()) {
        return {};
    }

    std::string idList;
    for (const Row& row : rows) {
        if (!idList.empty()) {
            idList += ",";
        }
        idList += std::to_string(intOf(row, "id"));
    }

    const std::vector<Row> itemRows = queryOrFail(
        db,
        "SELECT order_id, product_id, product_name, price, quantity, line_total "
        "FROM order_items WHERE order_id IN (" + idList + ") ORDER BY id");

    std::map<long long, Json> itemsByOrder;
    for (const Row& row : itemRows) {
        const long long orderId = intOf(row, "order_id");
        auto it = itemsByOrder.find(orderId);
        if (it == itemsByOrder.end()) {
            it = itemsByOrder.emplace(orderId, Json::array()).first;
        }
        it->second.push(itemJson(row));
    }

    std::vector<Json> orders;
    orders.reserve(rows.size());
    for (const Row& row : rows) {
        Json order = orderFromRow(row);
        auto it = itemsByOrder.find(intOf(row, "id"));
        order["items"] = it == itemsByOrder.end() ? Json::array() : it->second;
        orders.push_back(order);
    }
    return orders;
}

Json loadOrderById(Db& db, long long orderId) {
    std::vector<Json> orders =
        loadOrders(db, "WHERE o.id = " + std::to_string(orderId) + " LIMIT 1");
    if (orders.empty()) {
        fail(404, "Order not found.");
    }
    return orders.front();
}

struct DefaultProduct {
    long long id;
    const char* name;
    const char* category;
    int price;
    int stock;
};

const DefaultProduct kDefaultProducts[] = {
    {101, "Laptop", "Electronics", 55000, 5},
    {102, "Mouse", "Electronics", 800, 20},
    {103, "Keyboard", "Electronics", 1500, 10},
    {104, "Headphones", "Electronics", 2500, 15},
};

void insertDefaultProducts(Db& db) {
    for (const DefaultProduct& product : kDefaultProducts) {
        execOrFail(db,
                   "INSERT INTO products (id, name, category, price, stock) VALUES (" +
                       std::to_string(product.id) + ", " + db.quote(product.name) + ", " +
                       db.quote(product.category) + ", " + money(product.price) + ", " +
                       std::to_string(product.stock) + ")");
    }
}

void ensureAdminAccount(Db& db) {
    const std::vector<Row> admins =
        queryOrFail(db, "SELECT id FROM users WHERE role = 'admin' LIMIT 1");
    if (!admins.empty()) {
        return;
    }
    const std::string salt = newSalt();
    const std::string hash = hashPassword("admin123", salt);
    execOrFail(db,
               "INSERT INTO users (name, email, username, role, password_hash, password_salt, "
               "created_at) VALUES ('Administrator', 'admin@shopease.local', 'admin', 'admin', " +
                   db.quote(hash) + ", " + db.quote(salt) + ", UTC_TIMESTAMP())");
}

void loadOrderItemsForUpdate(Db& db, long long orderId, std::vector<Row>& items) {
    items = queryOrFail(
        db,
        "SELECT product_id, quantity FROM order_items WHERE order_id = " +
            std::to_string(orderId) + " FOR UPDATE");
}

}  // namespace

namespace store {

void migrateSchema(Db& db) {
    const char* statements[] = {
        "CREATE TABLE IF NOT EXISTS users ("
        " id BIGINT NOT NULL AUTO_INCREMENT,"
        " name VARCHAR(120) NOT NULL,"
        " email VARCHAR(190) NULL,"
        " username VARCHAR(60) NULL,"
        " role VARCHAR(20) NOT NULL DEFAULT 'customer',"
        " password_hash CHAR(64) NOT NULL,"
        " password_salt CHAR(32) NOT NULL,"
        " created_at DATETIME NOT NULL,"
        " PRIMARY KEY (id),"
        " UNIQUE KEY uk_users_email (email),"
        " UNIQUE KEY uk_users_username (username)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4",

        "CREATE TABLE IF NOT EXISTS sessions ("
        " token CHAR(64) NOT NULL,"
        " user_id BIGINT NOT NULL,"
        " role VARCHAR(20) NOT NULL,"
        " created_at DATETIME NOT NULL,"
        " expires_at DATETIME NOT NULL,"
        " PRIMARY KEY (token),"
        " KEY idx_sessions_user (user_id),"
        " CONSTRAINT fk_sessions_user FOREIGN KEY (user_id) REFERENCES users (id) ON DELETE CASCADE"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4",

        "CREATE TABLE IF NOT EXISTS products ("
        " id BIGINT NOT NULL,"
        " name VARCHAR(150) NOT NULL,"
        " category VARCHAR(60) NOT NULL,"
        " price DECIMAL(12,2) NOT NULL,"
        " stock INT NOT NULL,"
        " PRIMARY KEY (id)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4",

        "CREATE TABLE IF NOT EXISTS carts ("
        " user_id BIGINT NOT NULL,"
        " product_id BIGINT NOT NULL,"
        " quantity INT NOT NULL,"
        " PRIMARY KEY (user_id, product_id),"
        " CONSTRAINT fk_carts_user FOREIGN KEY (user_id) REFERENCES users (id) ON DELETE CASCADE,"
        " CONSTRAINT fk_carts_product FOREIGN KEY (product_id) REFERENCES products (id) ON DELETE CASCADE"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4",

        "CREATE TABLE IF NOT EXISTS orders ("
        " id BIGINT NOT NULL,"
        " customer_id BIGINT NOT NULL,"
        " customer_name VARCHAR(120) NOT NULL,"
        " customer_email VARCHAR(190) NOT NULL,"
        " phone VARCHAR(20) NOT NULL,"
        " address TEXT NOT NULL,"
        " pincode VARCHAR(10) NOT NULL,"
        " payment_method VARCHAR(40) NOT NULL,"
        " subtotal DECIMAL(12,2) NOT NULL,"
        " discount DECIMAL(12,2) NOT NULL,"
        " total DECIMAL(12,2) NOT NULL,"
        " status VARCHAR(40) NOT NULL DEFAULT 'Placed',"
        " cancellation_requested TINYINT(1) NOT NULL DEFAULT 0,"
        " cancellation_requested_at DATETIME NULL,"
        " previous_status VARCHAR(40) NULL,"
        " cancellation_approved TINYINT(1) NOT NULL DEFAULT 0,"
        " cancellation_rejected TINYINT(1) NOT NULL DEFAULT 0,"
        " cancellation_resolved_at DATETIME NULL,"
        " created_at DATETIME NOT NULL,"
        " PRIMARY KEY (id),"
        " KEY idx_orders_customer (customer_id),"
        " KEY idx_orders_status (status),"
        " CONSTRAINT fk_orders_customer FOREIGN KEY (customer_id) REFERENCES users (id) ON DELETE CASCADE"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4",

        "CREATE TABLE IF NOT EXISTS order_items ("
        " id BIGINT NOT NULL AUTO_INCREMENT,"
        " order_id BIGINT NOT NULL,"
        " product_id BIGINT NOT NULL,"
        " product_name VARCHAR(150) NOT NULL,"
        " price DECIMAL(12,2) NOT NULL,"
        " quantity INT NOT NULL,"
        " line_total DECIMAL(12,2) NOT NULL,"
        " PRIMARY KEY (id),"
        " KEY idx_items_order (order_id),"
        " CONSTRAINT fk_items_order FOREIGN KEY (order_id) REFERENCES orders (id) ON DELETE CASCADE"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4",
    };

    for (const char* statement : statements) {
        execOrFail(db, statement);
    }
}

void seedDefaults(Db& db) {
    ensureAdminAccount(db);

    const std::vector<Row> products =
        queryOrFail(db, "SELECT id FROM products LIMIT 1");
    if (products.empty()) {
        insertDefaultProducts(db);
    }
}

std::string createSession(Db& db, long long userId, const std::string& role) {
    const std::string token = randomHex(32);
    execOrFail(db, "DELETE FROM sessions WHERE expires_at <= UTC_TIMESTAMP()");
    execOrFail(db,
               "INSERT INTO sessions (token, user_id, role, created_at, expires_at) VALUES (" +
                   db.quote(token) + ", " + std::to_string(userId) + ", " + db.quote(role) +
                   ", UTC_TIMESTAMP(), DATE_ADD(UTC_TIMESTAMP(), INTERVAL 7 DAY))");
    return token;
}

SessionUser requireSession(Db& db, const std::string& token) {
    if (token.empty()) {
        fail(401, "Please login to continue.");
    }
    const std::vector<Row> rows = queryOrFail(
        db,
        "SELECT s.user_id, s.role, u.name, u.email, u.username FROM sessions s "
        "JOIN users u ON u.id = s.user_id "
        "WHERE s.token = " + db.quote(token) + " AND s.expires_at > UTC_TIMESTAMP() LIMIT 1");
    if (rows.empty()) {
        fail(401, "Your session has expired. Please login again.");
    }
    SessionUser user;
    user.userId = intOf(rows.front(), "user_id");
    user.role = valueOf(rows.front(), "role");
    user.name = valueOf(rows.front(), "name");
    user.email = valueOf(rows.front(), "email");
    user.username = valueOf(rows.front(), "username");
    return user;
}

SessionUser requireCustomer(Db& db, const std::string& token) {
    SessionUser user = requireSession(db, token);
    if (user.role != "customer") {
        fail(403, "Please login as a customer.");
    }
    return user;
}

SessionUser requireAdmin(Db& db, const std::string& token) {
    SessionUser user = requireSession(db, token);
    if (user.role != "admin") {
        fail(403, "Admin access required.");
    }
    return user;
}

void destroySession(Db& db, const std::string& token) {
    if (!token.empty()) {
        execOrFail(db, "DELETE FROM sessions WHERE token = " + db.quote(token));
    }
}

Json sessionInfo(const SessionUser& user) {
    Json info = Json::object();
    Json userJsonObject = Json::object();
    userJsonObject["id"] = user.userId;
    userJsonObject["name"] = user.name;
    userJsonObject["email"] = user.email;
    if (!user.username.empty()) {
        userJsonObject["username"] = user.username;
    }
    info["user"] = userJsonObject;
    info["role"] = user.role;
    return info;
}

Json registerCustomer(Db& db, const Json& body) {
    const std::string name = requiredString(body, "name", "Full name", 120);
    const std::string email = lowercase(requiredString(body, "email", "Email", 190));
    const std::string password = requiredString(body, "password", "Password", 200);

    if (!looksLikeEmail(email)) {
        fail(400, "Please enter a valid email address.");
    }
    if (password.size() < 4) {
        fail(400, "Password must contain at least 4 characters.");
    }

    const std::vector<Row> existing =
        queryOrFail(db, "SELECT id FROM users WHERE email = " + db.quote(email) + " LIMIT 1");
    if (!existing.empty()) {
        fail(409, "An account with this email already exists.");
    }

    const std::string salt = newSalt();
    const std::string hash = hashPassword(password, salt);

    std::string error;
    const std::string sql =
        "INSERT INTO users (name, email, role, password_hash, password_salt, created_at) VALUES (" +
        db.quote(name) + ", " + db.quote(email) + ", 'customer', " + db.quote(hash) + ", " +
        db.quote(salt) + ", UTC_TIMESTAMP())";
    if (!db.exec(sql, error)) {
        logDbError(error);
        if (db.errorCode() == 1062) {
            fail(409, "An account with this email already exists.");
        }
        fail(500, "Database error.");
    }

    Json user = Json::object();
    user["id"] = db.lastInsertId();
    user["name"] = name;
    user["email"] = email;

    Json response = Json::object();
    response["user"] = user;
    return response;
}

Json loginCustomer(Db& db, const Json& body) {
    const std::string email = lowercase(requiredString(body, "email", "Email", 190));
    const Json* passwordField = body.find("password");
    const std::string password =
        passwordField && passwordField->isString() ? passwordField->asString() : "";

    const std::vector<Row> rows = queryOrFail(
        db,
        "SELECT id, name, email, password_hash, password_salt FROM users "
        "WHERE email = " + db.quote(email) + " AND role = 'customer' LIMIT 1");

    if (rows.empty() || !passwordMatches(password, rows.front())) {
        fail(401, "Invalid email or password.");
    }

    const Row& user = rows.front();
    const std::string token = createSession(db, intOf(user, "id"), "customer");

    Json response = Json::object();
    response["token"] = token;
    response["role"] = "customer";
    response["user"] = userJson(user);
    return response;
}

Json loginAdmin(Db& db, const Json& body) {
    const std::string username = requiredString(body, "username", "Username", 60);
    const Json* passwordField = body.find("password");
    const std::string password =
        passwordField && passwordField->isString() ? passwordField->asString() : "";

    const std::vector<Row> rows = queryOrFail(
        db,
        "SELECT id, name, email, username, password_hash, password_salt FROM users "
        "WHERE username = " + db.quote(username) + " AND role = 'admin' LIMIT 1");

    if (rows.empty() || !passwordMatches(password, rows.front())) {
        fail(401, "Invalid admin credentials.");
    }

    const Row& user = rows.front();
    const std::string token = createSession(db, intOf(user, "id"), "admin");

    Json response = Json::object();
    response["token"] = token;
    response["role"] = "admin";
    response["user"] = userJson(user);
    return response;
}

Json listProducts(Db& db) {
    const std::vector<Row> rows =
        queryOrFail(db, "SELECT id, name, category, price, stock FROM products ORDER BY id");
    Json products = Json::array();
    for (const Row& row : rows) {
        products.push(productJson(row));
    }
    return products;
}

Json createProduct(Db& db, const Json& body) {
    const long long id = requiredInt(body, "id", "Product ID");
    const std::string name = requiredString(body, "name", "Product name", 150);
    const std::string category = requiredString(body, "category", "Category", 60);
    const double price = requiredNumber(body, "price", "Price");
    const long long stock = requiredInt(body, "stock", "Stock");

    if (id <= 0) {
        fail(400, "Product ID must be a positive number.");
    }
    if (price < 0 || stock < 0) {
        fail(400, "Price and stock cannot be negative.");
    }

    const std::vector<Row> existing =
        queryOrFail(db, "SELECT id FROM products WHERE id = " + std::to_string(id) + " LIMIT 1");
    if (!existing.empty()) {
        fail(409, "A product with this ID already exists.");
    }

    std::string error;
    const std::string sql =
        "INSERT INTO products (id, name, category, price, stock) VALUES (" +
        std::to_string(id) + ", " + db.quote(name) + ", " + db.quote(category) + ", " +
        money(price) + ", " + std::to_string(stock) + ")";
    if (!db.exec(sql, error)) {
        logDbError(error);
        if (db.errorCode() == 1062) {
            fail(409, "A product with this ID already exists.");
        }
        fail(500, "Database error.");
    }

    return productJson(Row{{"id", std::to_string(id)},
                           {"name", name},
                           {"category", category},
                           {"price", money(price)},
                           {"stock", std::to_string(stock)}});
}

Json updateProduct(Db& db, long long productId, const Json& body) {
    const std::string name = requiredString(body, "name", "Product name", 150);
    const std::string category = requiredString(body, "category", "Category", 60);
    const double price = requiredNumber(body, "price", "Price");
    const long long stock = requiredInt(body, "stock", "Stock");

    if (price < 0 || stock < 0) {
        fail(400, "Price and stock cannot be negative.");
    }

    const std::vector<Row> existing =
        queryOrFail(db,
                    "SELECT id FROM products WHERE id = " + std::to_string(productId) + " LIMIT 1");
    if (existing.empty()) {
        fail(404, "Product ID not found.");
    }

    execOrFail(db,
               "UPDATE products SET name = " + db.quote(name) + ", category = " +
                   db.quote(category) + ", price = " + money(price) + ", stock = " +
                   std::to_string(stock) + " WHERE id = " + std::to_string(productId));

    return productJson(Row{{"id", std::to_string(productId)},
                           {"name", name},
                           {"category", category},
                           {"price", money(price)},
                           {"stock", std::to_string(stock)}});
}

void deleteProduct(Db& db, long long productId) {
    // carts rows are removed by the foreign key cascade
    execOrFail(db, "DELETE FROM products WHERE id = " + std::to_string(productId));
}

Json cartFor(Db& db, long long userId) {
    const std::vector<Row> rows = queryOrFail(
        db,
        "SELECT c.product_id, c.quantity, p.name, p.category, p.price, p.stock "
        "FROM carts c JOIN products p ON p.id = c.product_id "
        "WHERE c.user_id = " + std::to_string(userId) + " ORDER BY c.product_id");

    Json items = Json::array();
    double subtotal = 0.0;
    long long count = 0;

    for (const Row& row : rows) {
        const long long quantity = intOf(row, "quantity");
        const double price = numberOf(row, "price");
        const double lineTotal = price * static_cast<double>(quantity);
        subtotal += lineTotal;
        count += quantity;

        Json product = Json::object();
        product["id"] = intOf(row, "product_id");
        product["name"] = valueOf(row, "name");
        product["category"] = valueOf(row, "category");
        product["price"] = price;
        product["stock"] = intOf(row, "stock");

        Json item = Json::object();
        item["productId"] = intOf(row, "product_id");
        item["quantity"] = quantity;
        item["total"] = round2(lineTotal);
        item["product"] = product;
        items.push(item);
    }

    const double discount = subtotal >= 5000.0 ? subtotal * 0.10 : 0.0;
    const double total = subtotal - discount;

    Json cart = Json::object();
    cart["items"] = items;
    cart["subtotal"] = round2(subtotal);
    cart["discount"] = round2(discount);
    cart["total"] = round2(total);
    cart["count"] = count;
    return cart;
}

Json addToCart(Db& db, long long userId, const Json& body) {
    const long long productId = requiredInt(body, "productId", "Product ID");

    const std::vector<Row> products = queryOrFail(
        db,
        "SELECT id, stock FROM products WHERE id = " + std::to_string(productId) + " LIMIT 1");
    if (products.empty()) {
        fail(404, "Product not found.");
    }

    const long long stock = intOf(products.front(), "stock");
    const std::vector<Row> existing = queryOrFail(
        db,
        "SELECT quantity FROM carts WHERE user_id = " + std::to_string(userId) +
            " AND product_id = " + std::to_string(productId) + " LIMIT 1");
    const long long quantity = existing.empty() ? 0 : intOf(existing.front(), "quantity");

    if (stock <= 0) {
        fail(409, "This product is out of stock.");
    }
    if (quantity >= stock) {
        fail(409, "You cannot add more than available stock.");
    }

    execOrFail(db,
               "INSERT INTO carts (user_id, product_id, quantity) VALUES (" +
                   std::to_string(userId) + ", " + std::to_string(productId) + ", 1) "
                   "ON DUPLICATE KEY UPDATE quantity = quantity + 1");

    return cartFor(db, userId);
}

Json setCartQuantity(Db& db, long long userId, const Json& body) {
    const long long productId = requiredInt(body, "productId", "Product ID");
    const long long quantity = requiredInt(body, "quantity", "Quantity");

    if (quantity <= 0) {
        execOrFail(db,
                   "DELETE FROM carts WHERE user_id = " + std::to_string(userId) +
                       " AND product_id = " + std::to_string(productId));
        return cartFor(db, userId);
    }

    const std::vector<Row> products = queryOrFail(
        db,
        "SELECT stock FROM products WHERE id = " + std::to_string(productId) + " LIMIT 1");
    if (products.empty()) {
        fail(404, "Product not found.");
    }
    const long long stock = intOf(products.front(), "stock");
    if (quantity > stock) {
        fail(409, "Only " + std::to_string(stock) + " units are available.");
    }

    execOrFail(db,
               "INSERT INTO carts (user_id, product_id, quantity) VALUES (" +
                   std::to_string(userId) + ", " + std::to_string(productId) + ", " +
                   std::to_string(quantity) + ") ON DUPLICATE KEY UPDATE quantity = " +
                   std::to_string(quantity));

    return cartFor(db, userId);
}

Json removeCartItem(Db& db, long long userId, long long productId) {
    execOrFail(db,
               "DELETE FROM carts WHERE user_id = " + std::to_string(userId) +
                   " AND product_id = " + std::to_string(productId));
    return cartFor(db, userId);
}

Json placeOrder(Db& db, const SessionUser& user, const Json& body) {
    const std::string phone = requiredString(body, "phone", "Phone number", 20);
    const std::string address = requiredString(body, "address", "Delivery address", 1000);
    const std::string pincode = requiredString(body, "pincode", "Pincode", 10);
    const std::string payment = requiredString(body, "paymentMethod", "Payment method", 40);

    if (!allDigits(phone, 10)) {
        fail(400, "Please enter a valid 10-digit phone number.");
    }
    if (!allDigits(pincode, 6)) {
        fail(400, "Please enter a valid 6-digit pincode.");
    }
    const std::set<std::string> payments = {"Cash on Delivery", "UPI", "Card"};
    if (payments.find(payment) == payments.end()) {
        fail(400, "Please select a valid payment method.");
    }

    Transaction transaction(db);

    const std::vector<Row> cartRows = queryOrFail(
        db,
        "SELECT c.product_id, c.quantity, p.name, p.price, p.stock "
        "FROM carts c JOIN products p ON p.id = c.product_id "
        "WHERE c.user_id = " + std::to_string(user.userId) + " FOR UPDATE");

    if (cartRows.empty()) {
        fail(400, "Your cart is empty.");
    }

    double subtotal = 0.0;
    for (const Row& row : cartRows) {
        const long long quantity = intOf(row, "quantity");
        if (quantity > intOf(row, "stock")) {
            fail(409, valueOf(row, "name") + " does not have enough stock.");
        }
        subtotal += numberOf(row, "price") * static_cast<double>(quantity);
    }

    const double discount = subtotal >= 5000.0 ? subtotal * 0.10 : 0.0;
    const double total = subtotal - discount;

    long long orderId = 0;
    std::string insertError;
    for (int attempt = 0; attempt < 5; ++attempt) {
        orderId = nowMillis() + attempt;
        const std::string sql =
            "INSERT INTO orders (id, customer_id, customer_name, customer_email, phone, address, "
            "pincode, payment_method, subtotal, discount, total, status, created_at) VALUES (" +
            std::to_string(orderId) + ", " + std::to_string(user.userId) + ", " +
            db.quote(user.name) + ", " + db.quote(user.email) + ", " + db.quote(phone) + ", " +
            db.quote(address) + ", " + db.quote(pincode) + ", " + db.quote(payment) + ", " +
            money(round2(subtotal)) + ", " + money(round2(discount)) + ", " +
            money(round2(total)) + ", 'Placed', UTC_TIMESTAMP())";
        if (db.exec(sql, insertError)) {
            break;
        }
        if (db.errorCode() == 1062) {
            orderId = 0;
            continue;
        }
        logDbError(insertError);
        fail(500, "Database error.");
    }
    if (orderId == 0) {
        fail(500, "Could not create the order. Please try again.");
    }

    for (const Row& row : cartRows) {
        const long long productId = intOf(row, "product_id");
        const long long quantity = intOf(row, "quantity");
        const double lineTotal = numberOf(row, "price") * static_cast<double>(quantity);

        execOrFail(db,
                   "INSERT INTO order_items (order_id, product_id, product_name, price, quantity, "
                   "line_total) VALUES (" + std::to_string(orderId) + ", " +
                       std::to_string(productId) + ", " + db.quote(valueOf(row, "name")) + ", " +
                       money(numberOf(row, "price")) + ", " + std::to_string(quantity) + ", " +
                       money(round2(lineTotal)) + ")");

        execOrFail(db,
                   "UPDATE products SET stock = stock - " + std::to_string(quantity) +
                       " WHERE id = " + std::to_string(productId));
    }

    execOrFail(db, "DELETE FROM carts WHERE user_id = " + std::to_string(user.userId));

    transaction.commit();
    return loadOrderById(db, orderId);
}

Json ordersForCustomer(Db& db, long long userId) {
    const std::vector<Json> orders =
        loadOrders(db,
                   "WHERE o.customer_id = " + std::to_string(userId) +
                       " ORDER BY o.created_at DESC, o.id DESC");
    Json list = Json::array();
    for (const Json& order : orders) {
        list.push(order);
    }
    return list;
}

Json requestCancellation(Db& db, long long userId, long long orderId) {
    const std::vector<Row> rows = queryOrFail(
        db,
        "SELECT id, status, cancellation_requested FROM orders WHERE id = " +
            std::to_string(orderId) + " AND customer_id = " + std::to_string(userId) + " LIMIT 1");
    if (rows.empty()) {
        fail(404, "Order not found.");
    }

    const std::string status = valueOf(rows.front(), "status");
    if (status != "Placed" && status != "Processing") {
        fail(409, "This order can no longer be cancelled.");
    }
    if (valueOf(rows.front(), "cancellation_requested") == "1") {
        fail(409, "Cancellation request has already been sent.");
    }

    execOrFail(db,
               "UPDATE orders SET previous_status = " + db.quote(status) +
                   ", cancellation_requested = 1, cancellation_requested_at = UTC_TIMESTAMP(), "
                   "status = 'Cancellation Requested' WHERE id = " +
                   std::to_string(orderId));

    return loadOrderById(db, orderId);
}

void deleteCustomerOrder(Db& db, long long userId, long long orderId) {
    const std::vector<Row> rows = queryOrFail(
        db,
        "SELECT id, status FROM orders WHERE id = " + std::to_string(orderId) +
            " AND customer_id = " + std::to_string(userId) + " LIMIT 1");
    if (rows.empty()) {
        fail(404, "Order not found.");
    }
    if (valueOf(rows.front(), "status") == "Cancellation Requested") {
        fail(409, "Please wait until the admin responds to the cancellation request.");
    }
    execOrFail(db,
               "DELETE FROM orders WHERE id = " + std::to_string(orderId) +
                   " AND customer_id = " + std::to_string(userId));
}

Json adminDashboard(Db& db) {
    const std::vector<Row> products =
        queryOrFail(db, "SELECT id, name, category, price, stock FROM products ORDER BY id");

    const std::vector<Row> totals = queryOrFail(
        db,
        "SELECT MIN(stock) AS min_stock, COUNT(*) AS product_count, "
        "COALESCE(SUM(stock), 0) AS total_stock FROM products");

    long long totalProducts = 0;
    long long totalStock = 0;
    if (!totals.empty()) {
        totalProducts = intOf(totals.front(), "product_count");
        totalStock = intOf(totals.front(), "total_stock");
    }

    const std::vector<Row> orderTotals = queryOrFail(
        db,
        "SELECT COUNT(*) AS order_count, "
        "COALESCE(SUM(CASE WHEN status NOT IN ('Cancelled', 'Cancellation Requested') "
        "THEN total ELSE 0 END), 0) AS total_sales FROM orders");
    long long totalOrders = 0;
    double totalSales = 0.0;
    if (!orderTotals.empty()) {
        totalOrders = intOf(orderTotals.front(), "order_count");
        totalSales = numberOf(orderTotals.front(), "total_sales");
    }

    const std::vector<Row> customerCount =
        queryOrFail(db, "SELECT COUNT(*) AS customer_count FROM users WHERE role = 'customer'");
    const long long totalCustomers =
        customerCount.empty() ? 0 : intOf(customerCount.front(), "customer_count");

    const std::vector<Row> lowStockRows = queryOrFail(
        db,
        "SELECT id, name, category, price, stock FROM products WHERE stock <= " +
            std::to_string(LOW_STOCK_LIMIT) + " ORDER BY stock ASC, id ASC");

    const std::vector<Row> customerRows = queryOrFail(
        db,
        "SELECT u.id, u.name, u.email, COUNT(o.id) AS total_orders, "
        "COALESCE(SUM(CASE WHEN o.status NOT IN ('Cancelled', 'Cancellation Requested') "
        "THEN o.total ELSE 0 END), 0) AS total_spent "
        "FROM users u LEFT JOIN orders o ON o.customer_id = u.id "
        "WHERE u.role = 'customer' GROUP BY u.id, u.name, u.email ORDER BY u.id");

    Json stats = Json::object();
    stats["totalProducts"] = totalProducts;
    stats["totalStock"] = totalStock;
    stats["totalOrders"] = totalOrders;
    stats["totalSales"] = round2(totalSales);
    stats["totalCustomers"] = totalCustomers;
    stats["lowStockCount"] = static_cast<long long>(lowStockRows.size());

    Json productList = Json::array();
    for (const Row& row : products) {
        productList.push(productJson(row));
    }

    Json lowStock = Json::array();
    for (const Row& row : lowStockRows) {
        lowStock.push(productJson(row));
    }

    Json customers = Json::array();
    for (const Row& row : customerRows) {
        Json customer = Json::object();
        customer["id"] = intOf(row, "id");
        customer["name"] = valueOf(row, "name");
        customer["email"] = valueOf(row, "email");
        customer["totalOrders"] = intOf(row, "total_orders");
        customer["totalSpent"] = round2(numberOf(row, "total_spent"));
        customers.push(customer);
    }

    const std::vector<Json> orderList =
        loadOrders(db, "ORDER BY o.created_at DESC, o.id DESC");
    Json orders = Json::array();
    for (const Json& order : orderList) {
        orders.push(order);
    }

    Json dashboard = Json::object();
    dashboard["stats"] = stats;
    dashboard["lowStock"] = lowStock;
    dashboard["products"] = productList;
    dashboard["customers"] = customers;
    dashboard["orders"] = orders;
    return dashboard;
}

Json approveCancellation(Db& db, long long orderId) {
    Transaction transaction(db);

    const std::vector<Row> orders = queryOrFail(
        db,
        "SELECT id, status FROM orders WHERE id = " + std::to_string(orderId) + " FOR UPDATE");
    if (orders.empty()) {
        fail(404, "Order not found.");
    }
    if (valueOf(orders.front(), "status") != "Cancellation Requested") {
        fail(409, "This order does not have a pending cancellation request.");
    }

    std::vector<Row> items;
    loadOrderItemsForUpdate(db, orderId, items);
    for (const Row& item : items) {
        execOrFail(db,
                   "UPDATE products SET stock = stock + " + std::to_string(intOf(item, "quantity")) +
                       " WHERE id = " + std::to_string(intOf(item, "product_id")));
    }

    execOrFail(db,
               "UPDATE orders SET status = 'Cancelled', cancellation_requested = 0, "
               "cancellation_approved = 1, cancellation_rejected = 0, "
               "cancellation_resolved_at = UTC_TIMESTAMP() WHERE id = " + std::to_string(orderId));

    transaction.commit();
    return loadOrderById(db, orderId);
}

Json rejectCancellation(Db& db, long long orderId) {
    Transaction transaction(db);

    const std::vector<Row> orders = queryOrFail(
        db,
        "SELECT id, status, previous_status FROM orders WHERE id = " +
            std::to_string(orderId) + " FOR UPDATE");
    if (orders.empty()) {
        fail(404, "Order not found.");
    }
    if (valueOf(orders.front(), "status") != "Cancellation Requested") {
        fail(409, "This order does not have a pending cancellation request.");
    }

    std::string previous = valueOf(orders.front(), "previous_status");
    if (previous.empty()) {
        previous = "Placed";
    }

    execOrFail(db,
               "UPDATE orders SET status = " + db.quote(previous) +
                   ", cancellation_requested = 0, cancellation_approved = 0, "
                   "cancellation_rejected = 1, cancellation_resolved_at = UTC_TIMESTAMP() "
                   "WHERE id = " + std::to_string(orderId));

    transaction.commit();
    return loadOrderById(db, orderId);
}

Json updateOrderStatus(Db& db, long long orderId, const Json& body) {
    const std::string newStatus = requiredString(body, "status", "Status", 40);
    const std::set<std::string> allowed = {"Placed", "Processing", "Shipped", "Delivered",
                                           "Cancelled"};
    if (allowed.find(newStatus) == allowed.end()) {
        fail(400, "Invalid order status.");
    }

    Transaction transaction(db);

    const std::vector<Row> orders = queryOrFail(
        db,
        "SELECT id, status FROM orders WHERE id = " + std::to_string(orderId) + " FOR UPDATE");
    if (orders.empty()) {
        fail(404, "Order not found.");
    }

    const std::string currentStatus = valueOf(orders.front(), "status");
    if (currentStatus == "Cancellation Requested") {
        fail(409, "Please approve or reject the cancellation request first.");
    }
    if (currentStatus == newStatus) {
        transaction.commit();
        return loadOrderById(db, orderId);
    }

    std::vector<Row> items;
    loadOrderItemsForUpdate(db, orderId, items);

    if (newStatus == "Cancelled") {
        for (const Row& item : items) {
            execOrFail(db,
                       "UPDATE products SET stock = stock + " +
                           std::to_string(intOf(item, "quantity")) + " WHERE id = " +
                           std::to_string(intOf(item, "product_id")));
        }
    } else if (currentStatus == "Cancelled") {
        for (const Row& item : items) {
            const std::vector<Row> products = queryOrFail(
                db,
                "SELECT name, stock FROM products WHERE id = " +
                    std::to_string(intOf(item, "product_id")) + " FOR UPDATE");
            if (products.empty()) {
                continue;
            }
            if (intOf(products.front(), "stock") < intOf(item, "quantity")) {
                fail(409, "Cannot restore order. Not enough stock for " +
                               valueOf(products.front(), "name") + ".");
            }
        }
        for (const Row& item : items) {
            execOrFail(db,
                       "UPDATE products SET stock = stock - " +
                           std::to_string(intOf(item, "quantity")) + " WHERE id = " +
                           std::to_string(intOf(item, "product_id")));
        }
    }

    execOrFail(db,
               "UPDATE orders SET status = " + db.quote(newStatus) +
                   (newStatus == "Cancelled" ? ", cancellation_requested = 0" : "") +
                   " WHERE id = " + std::to_string(orderId));

    transaction.commit();
    return loadOrderById(db, orderId);
}

Json resetDemoData(Db& db) {
    Transaction transaction(db);

    execOrFail(db, "DELETE FROM users WHERE role = 'customer'");
    execOrFail(db, "DELETE FROM products");
    insertDefaultProducts(db);

    transaction.commit();
    ensureAdminAccount(db);

    Json response = Json::object();
    response["ok"] = true;
    return response;
}

}  // namespace store
