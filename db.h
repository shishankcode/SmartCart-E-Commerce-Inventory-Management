#pragma once

#include "config.h"

#include <mysql.h>

#include <map>
#include <string>
#include <vector>

using Row = std::map<std::string, std::string>;

// Thin wrapper around one MySQL connection.
// The server opens one connection per request, which keeps the code simple
// and avoids sharing state between worker threads.
class Db {
public:
    Db() = default;
    ~Db();

    Db(const Db&) = delete;
    Db& operator=(const Db&) = delete;

    Db(Db&& other) noexcept : conn_(other.conn_) { other.conn_ = nullptr; }
    Db& operator=(Db&& other) noexcept {
        if (this != &other) {
            if (conn_) {
                mysql_close(conn_);
            }
            conn_ = other.conn_;
            other.conn_ = nullptr;
        }
        return *this;
    }

    bool connect(const MysqlConfig& config, std::string& error);
    bool isOpen() const { return conn_ != nullptr; }

    std::string quote(const std::string& value) const;
    std::string error() const;
    unsigned int errorCode() const;

    bool exec(const std::string& sql, std::string& error);
    bool select(const std::string& sql, std::vector<Row>& rows, std::string& error);

    long long affectedRows() const;
    long long lastInsertId() const;

private:
    MYSQL* conn_ = nullptr;
};
