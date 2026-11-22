#include "api.h"
#include "config.h"
#include "db.h"
#include "http.h"
#include "store.h"

#include <iostream>
#include <string>

int main(int argc, char** argv) {
    const std::string configPath = argc > 1 ? argv[1] : "config.json";

    AppConfig config;
    std::string error;
    if (!loadConfig(configPath, config, error)) {
        std::cerr << "Configuration error: " << error << std::endl;
        return 1;
    }

    {
        Db db;
        if (!db.connect(config.mysql, error)) {
            std::cerr << "MySQL connection failed: " << error << std::endl;
            std::cerr << "Check backend/config.json and make sure the MySQL80 service is running."
                      << std::endl;
            return 1;
        }
        try {
            store::migrateSchema(db);
            store::seedDefaults(db);
        } catch (const ApiError& apiError) {
            std::cerr << "Database setup failed: " << apiError.message << std::endl;
            return 1;
        }
    }

    std::cout << "Connected to MySQL database '" << config.mysql.database << "'." << std::endl;

    HttpServer server(config.server.host, config.server.port, config.server.staticDir);
    registerApiRoutes(server, config);

    if (!server.start(error)) {
        std::cerr << error << std::endl;
        return 1;
    }
    return 0;
}
