#include "db.h"

#include <cstring>

Db::~Db() {
    if (conn_) {
        mysql_close(conn_);
    }
}

bool Db::connect(const MysqlConfig& config, std::string& error) {
    conn_ = mysql_init(nullptr);
    if (!conn_) {
        error = "mysql_init failed";
        return false;
    }

    unsigned int connectTimeout = 5;
    mysql_options(conn_, MYSQL_OPT_CONNECT_TIMEOUT, &connectTimeout);
    mysql_options(conn_, MYSQL_SET_CHARSET_NAME, "utf8mb4");

    if (!mysql_real_connect(conn_,
                            config.host.c_str(),
                            config.user.c_str(),
                            config.password.c_str(),
                            config.database.c_str(),
                            static_cast<unsigned int>(config.port),
                            nullptr,
                            0)) {
        error = mysql_error(conn_);
        return false;
    }
    return true;
}

std::string Db::quote(const std::string& value) const {
    std::vector<char> buffer(value.size() * 2 + 1);
    unsigned long length = mysql_real_escape_string(
        conn_, buffer.data(), value.c_str(), static_cast<unsigned long>(value.size()));
    return "'" + std::string(buffer.data(), length) + "'";
}

std::string Db::error() const {
    return conn_ ? mysql_error(conn_) : "no connection";
}

unsigned int Db::errorCode() const {
    return conn_ ? mysql_errno(conn_) : 0;
}

bool Db::exec(const std::string& sql, std::string& error) {
    if (mysql_query(conn_, sql.c_str()) != 0) {
        error = mysql_error(conn_);
        return false;
    }
    MYSQL_RES* result = mysql_store_result(conn_);
    if (result) {
        mysql_free_result(result);
    }
    return true;
}

bool Db::select(const std::string& sql, std::vector<Row>& rows, std::string& error) {
    rows.clear();
    if (mysql_query(conn_, sql.c_str()) != 0) {
        error = mysql_error(conn_);
        return false;
    }
    MYSQL_RES* result = mysql_store_result(conn_);
    if (!result) {
        error = mysql_error(conn_);
        if (error.empty()) {
            error = "query returned no result set";
        }
        return false;
    }

    unsigned int fieldCount = mysql_num_fields(result);
    MYSQL_FIELD* fields = mysql_fetch_fields(result);
    std::vector<std::string> names;
    names.reserve(fieldCount);
    for (unsigned int i = 0; i < fieldCount; ++i) {
        names.emplace_back(fields[i].name);
    }

    MYSQL_ROW mysqlRow = nullptr;
    while ((mysqlRow = mysql_fetch_row(result)) != nullptr) {
        unsigned long* lengths = mysql_fetch_lengths(result);
        Row row;
        for (unsigned int i = 0; i < fieldCount; ++i) {
            row[names[i]] = mysqlRow[i]
                                ? std::string(mysqlRow[i], lengths[i])
                                : std::string();
        }
        rows.push_back(std::move(row));
    }
    mysql_free_result(result);
    return true;
}

long long Db::affectedRows() const {
    return static_cast<long long>(mysql_affected_rows(conn_));
}

long long Db::lastInsertId() const {
    return static_cast<long long>(mysql_insert_id(conn_));
}
