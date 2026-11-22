#pragma once

#include "json.h"

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

struct HttpRequest {
    std::string method;
    std::string path;
    std::string query;
    std::string body;
    std::map<std::string, std::string> headers;
    std::map<std::string, std::string> params;

    std::string header(const std::string& name) const;
};

struct HttpResponse {
    int status = 200;
    std::string contentType = "application/json; charset=utf-8";
    std::string body;
    std::vector<std::pair<std::string, std::string>> headers;

    static HttpResponse json(const Json& payload, int status = 200);
    static HttpResponse error(int status, const std::string& message);
    static HttpResponse noContent();
};

using HttpHandler = std::function<HttpResponse(HttpRequest&)>;

class HttpServer {
public:
    HttpServer(std::string host, int port, std::string staticDir);

    void get(const std::string& path, HttpHandler handler);
    void post(const std::string& path, HttpHandler handler);
    void put(const std::string& path, HttpHandler handler);
    void del(const std::string& path, HttpHandler handler);

    // Blocks until the process is stopped.
    bool start(std::string& error);

private:
    struct Route {
        std::string method;
        std::vector<std::string> segments;
        HttpHandler handler;
    };

    void addRoute(const std::string& method, const std::string& path, HttpHandler handler);
    HttpResponse dispatch(HttpRequest& request);
    HttpResponse serveStatic(const HttpRequest& request);
    void handleClient(std::uintptr_t socketHandle);

    std::string host_;
    int port_;
    std::string staticDir_;
    std::vector<Route> routes_;
};
