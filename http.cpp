#include "http.h"

#include "apierror.h"

#include <winsock2.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <thread>

namespace {

std::string trimText(const std::string& value) {
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

const char* reasonPhrase(int status) {
    switch (status) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 409: return "Conflict";
        case 413: return "Payload Too Large";
        case 431: return "Request Header Fields Too Large";
        default: return "Internal Server Error";
    }
}

std::string mimeType(const std::string& extension) {
    if (extension == ".html" || extension == ".htm") return "text/html; charset=utf-8";
    if (extension == ".css") return "text/css; charset=utf-8";
    if (extension == ".js") return "application/javascript; charset=utf-8";
    if (extension == ".json") return "application/json; charset=utf-8";
    if (extension == ".svg") return "image/svg+xml";
    if (extension == ".png") return "image/png";
    if (extension == ".jpg" || extension == ".jpeg") return "image/jpeg";
    if (extension == ".ico") return "image/x-icon";
    if (extension == ".txt") return "text/plain; charset=utf-8";
    return "application/octet-stream";
}

int hexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

std::string urlDecode(const std::string& value, bool plusAsSpace) {
    std::string out;
    out.reserve(value.size());
    for (std::size_t i = 0; i < value.size(); ++i) {
        const char c = value[i];
        if (c == '%' && i + 2 < value.size()) {
            const int high = hexValue(value[i + 1]);
            const int low = hexValue(value[i + 2]);
            if (high >= 0 && low >= 0) {
                out += static_cast<char>((high << 4) | low);
                i += 2;
                continue;
            }
        }
        if (c == '+' && plusAsSpace) {
            out += ' ';
            continue;
        }
        out += c;
    }
    return out;
}

std::vector<std::string> splitPath(const std::string& path) {
    std::vector<std::string> segments;
    std::string current;
    for (char c : path) {
        if (c == '/') {
            if (!current.empty()) {
                segments.push_back(current);
                current.clear();
            }
        } else {
            current += c;
        }
    }
    if (!current.empty()) {
        segments.push_back(current);
    }
    return segments;
}

bool parseHead(const std::string& head, HttpRequest& request) {
    std::istringstream stream(head);
    std::string requestLine;

    if (!std::getline(stream, requestLine)) {
        return false;
    }
    if (!requestLine.empty() && requestLine.back() == '\r') {
        requestLine.pop_back();
    }

    std::istringstream lineStream(requestLine);
    std::string target;
    std::string version;
    if (!(lineStream >> request.method >> target >> version)) {
        return false;
    }
    if (request.method == "HEAD") {
        request.method = "GET";
    }

    const std::size_t queryPos = target.find('?');
    if (queryPos != std::string::npos) {
        request.query = urlDecode(target.substr(queryPos + 1), true);
        target = target.substr(0, queryPos);
    }
    target = urlDecode(target, false);
    request.path = target.empty() ? "/" : target;

    std::string headerLine;
    while (std::getline(stream, headerLine)) {
        if (!headerLine.empty() && headerLine.back() == '\r') {
            headerLine.pop_back();
        }
        if (headerLine.empty()) {
            continue;
        }
        const std::size_t colon = headerLine.find(':');
        if (colon == std::string::npos) {
            continue;
        }
        const std::string name = lowercase(trimText(headerLine.substr(0, colon)));
        request.headers[name] = trimText(headerLine.substr(colon + 1));
    }
    return true;
}

bool sendAll(SOCKET client, const std::string& data) {
    std::size_t sent = 0;
    while (sent < data.size()) {
        const int written = send(client,
                                 data.data() + sent,
                                 static_cast<int>(data.size() - sent),
                                 0);
        if (written <= 0) {
            return false;
        }
        sent += static_cast<std::size_t>(written);
    }
    return true;
}

void writeResponse(SOCKET client, const HttpResponse& response) {
    std::ostringstream head;
    head << "HTTP/1.1 " << response.status << ' ' << reasonPhrase(response.status) << "\r\n";
    head << "Content-Type: " << response.contentType << "\r\n";
    head << "Content-Length: " << response.body.size() << "\r\n";
    head << "Connection: close\r\n";
    head << "Access-Control-Allow-Origin: *\r\n";
    head << "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n";
    head << "Access-Control-Allow-Headers: Content-Type, Authorization\r\n";
    head << "Access-Control-Max-Age: 86400\r\n";
    head << "X-Content-Type-Options: nosniff\r\n";
    for (const auto& header : response.headers) {
        head << header.first << ": " << header.second << "\r\n";
    }
    head << "\r\n";

    sendAll(client, head.str() + response.body);
}

const std::size_t kMaxHeaderBytes = 64 * 1024;
const std::size_t kMaxBodyBytes = 2 * 1024 * 1024;

}  // namespace

std::string HttpRequest::header(const std::string& name) const {
    auto it = headers.find(lowercase(name));
    return it == headers.end() ? std::string() : it->second;
}

HttpResponse HttpResponse::json(const Json& payload, int status) {
    HttpResponse response;
    response.status = status;
    response.contentType = "application/json; charset=utf-8";
    response.body = payload.dump();
    return response;
}

HttpResponse HttpResponse::error(int status, const std::string& message) {
    Json payload = Json::object();
    payload["error"] = message;
    return HttpResponse::json(payload, status);
}

HttpResponse HttpResponse::noContent() {
    HttpResponse response;
    response.status = 204;
    response.body.clear();
    return response;
}

HttpServer::HttpServer(std::string host, int port, std::string staticDir)
    : host_(std::move(host)), port_(port), staticDir_(std::move(staticDir)) {}

void HttpServer::get(const std::string& path, HttpHandler handler) {
    addRoute("GET", path, std::move(handler));
}

void HttpServer::post(const std::string& path, HttpHandler handler) {
    addRoute("POST", path, std::move(handler));
}

void HttpServer::put(const std::string& path, HttpHandler handler) {
    addRoute("PUT", path, std::move(handler));
}

void HttpServer::del(const std::string& path, HttpHandler handler) {
    addRoute("DELETE", path, std::move(handler));
}

void HttpServer::addRoute(const std::string& method,
                          const std::string& path,
                          HttpHandler handler) {
    Route route;
    route.method = method;
    route.segments = splitPath(path);
    route.handler = std::move(handler);
    routes_.push_back(std::move(route));
}

HttpResponse HttpServer::dispatch(HttpRequest& request) {
    const std::vector<std::string> segments = splitPath(request.path);

    for (const Route& route : routes_) {
        if (route.method != request.method) {
            continue;
        }
        if (route.segments.size() != segments.size()) {
            continue;
        }
        bool matched = true;
        std::map<std::string, std::string> params;
        for (std::size_t i = 0; i < segments.size(); ++i) {
            const std::string& pattern = route.segments[i];
            if (!pattern.empty() && pattern[0] == ':') {
                params[pattern.substr(1)] = segments[i];
            } else if (pattern != segments[i]) {
                matched = false;
                break;
            }
        }
        if (matched) {
            request.params = std::move(params);
            return route.handler(request);
        }
    }

    if (request.method != "GET") {
        return HttpResponse::error(404, "Not found.");
    }

    HttpResponse response = serveStatic(request);
    if (response.status == 404 && request.path.rfind("/api/", 0) == 0) {
        return HttpResponse::error(404, "Not found.");
    }
    return response;
}

HttpResponse HttpServer::serveStatic(const HttpRequest& request) {
    if (request.path.find("..") != std::string::npos) {
        return HttpResponse::error(403, "Forbidden.");
    }

    std::string relative = request.path == "/" ? "index.html" : request.path.substr(1);
    const std::filesystem::path root =
        std::filesystem::absolute(std::filesystem::path(staticDir_)).lexically_normal();
    const std::filesystem::path file = (root / relative).lexically_normal();

    const std::string rootText = root.string();
    const std::string fileText = file.string();
    if (fileText.compare(0, rootText.size(), rootText) != 0) {
        return HttpResponse::error(403, "Forbidden.");
    }

    std::error_code ec;
    if (!std::filesystem::is_regular_file(file, ec)) {
        return HttpResponse::error(404, "Not found.");
    }

    std::ifstream input(file, std::ios::binary);
    if (!input) {
        return HttpResponse::error(404, "Not found.");
    }
    const std::string content((std::istreambuf_iterator<char>(input)),
                              std::istreambuf_iterator<char>());

    HttpResponse response;
    response.status = 200;
    response.contentType = mimeType(lowercase(file.extension().string()));
    response.body = content;
    response.headers.emplace_back("Cache-Control", "no-store");
    return response;
}

void HttpServer::handleClient(std::uintptr_t socketHandle) {
    SOCKET client = static_cast<SOCKET>(socketHandle);

    std::string buffer;
    char chunk[8192];
    std::size_t headerEnd = std::string::npos;

    while (true) {
        headerEnd = buffer.find("\r\n\r\n");
        if (headerEnd != std::string::npos) {
            break;
        }
        const int received = recv(client, chunk, sizeof(chunk), 0);
        if (received <= 0) {
            closesocket(client);
            return;
        }
        buffer.append(chunk, static_cast<std::size_t>(received));
        if (buffer.size() > kMaxHeaderBytes) {
            writeResponse(client, HttpResponse::error(431, "Request headers are too large."));
            closesocket(client);
            return;
        }
    }

    HttpRequest request;
    if (!parseHead(buffer.substr(0, headerEnd), request)) {
        writeResponse(client, HttpResponse::error(400, "Malformed HTTP request."));
        closesocket(client);
        return;
    }

    std::size_t contentLength = 0;
    const std::string lengthHeader = request.header("content-length");
    if (!lengthHeader.empty()) {
        try {
            contentLength = static_cast<std::size_t>(std::stoull(lengthHeader));
        } catch (...) {
            writeResponse(client, HttpResponse::error(400, "Invalid Content-Length."));
            closesocket(client);
            return;
        }
    }
    if (contentLength > kMaxBodyBytes) {
        writeResponse(client, HttpResponse::error(413, "Request body is too large."));
        closesocket(client);
        return;
    }

    const std::size_t bodyStart = headerEnd + 4;
    while (buffer.size() - bodyStart < contentLength) {
        const int received = recv(client, chunk, sizeof(chunk), 0);
        if (received <= 0) {
            break;
        }
        buffer.append(chunk, static_cast<std::size_t>(received));
    }
    const std::size_t available =
        buffer.size() > bodyStart ? buffer.size() - bodyStart : 0;
    request.body = buffer.substr(bodyStart, std::min(contentLength, available));

    HttpResponse response;
    try {
        if (request.method == "OPTIONS") {
            response = HttpResponse::noContent();
        } else {
            response = dispatch(request);
        }
    } catch (const ApiError& error) {
        response = HttpResponse::error(error.status, error.message);
    } catch (const JsonParseError& error) {
        response = HttpResponse::error(400, std::string("Invalid JSON: ") + error.what());
    } catch (const std::exception& exception) {
        std::cerr << "[server] " << exception.what() << std::endl;
        response = HttpResponse::error(500, "Internal server error.");
    }

    writeResponse(client, response);
    closesocket(client);
}

bool HttpServer::start(std::string& error) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        error = "WSAStartup failed.";
        return false;
    }

    SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == INVALID_SOCKET) {
        error = "Could not create server socket.";
        WSACleanup();
        return false;
    }

    BOOL reuseAddress = TRUE;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&reuseAddress), sizeof(reuseAddress));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<unsigned short>(port_));
    address.sin_addr.s_addr = inet_addr(host_.c_str());
    if (address.sin_addr.s_addr == INADDR_NONE) {
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    }

    if (bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) ==
        SOCKET_ERROR) {
        error = "Cannot bind to " + host_ + ":" + std::to_string(port_) +
                ". Is another program using this port?";
        closesocket(listener);
        WSACleanup();
        return false;
    }

    if (listen(listener, 64) == SOCKET_ERROR) {
        error = "Could not listen on the server socket.";
        closesocket(listener);
        WSACleanup();
        return false;
    }

    std::cout << "ShopEase backend listening on http://" << host_ << ':' << port_ << std::endl;
    std::cout << "Serving frontend from " << staticDir_ << std::endl;

    while (true) {
        SOCKET client = accept(listener, nullptr, nullptr);
        if (client == INVALID_SOCKET) {
            continue;
        }
        DWORD timeout = 15000;
        setsockopt(client, SOL_SOCKET, SO_RCVTIMEO,
                   reinterpret_cast<const char*>(&timeout), sizeof(timeout));
        setsockopt(client, SOL_SOCKET, SO_SNDTIMEO,
                   reinterpret_cast<const char*>(&timeout), sizeof(timeout));

        std::thread(&HttpServer::handleClient, this, static_cast<std::uintptr_t>(client))
            .detach();
    }
}
