#pragma once

#include <string>

struct ServerConfig {
    std::string host = "127.0.0.1";
    int port = 8080;
    std::string staticDir = "../frontend";
};

struct MysqlConfig {
    std::string host = "127.0.0.1";
    int port = 3306;
    std::string user = "shopease_user";
    std::string password;
    std::string database = "shopease";
};

struct AppConfig {
    ServerConfig server;
    MysqlConfig mysql;
};

bool loadConfig(const std::string& path, AppConfig& config, std::string& error);
