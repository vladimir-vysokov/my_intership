#include "http.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>
#include <iostream>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <thread>

#include "utils.hpp"

namespace internstart {

static std::string statusText(int status) {
    if (status == 200) return "OK";
    if (status == 302) return "Found";
    if (status == 400) return "Bad Request";
    if (status == 403) return "Forbidden";
    if (status == 404) return "Not Found";
    if (status == 500) return "Internal Server Error";
    return "OK";
}

HttpServer::HttpServer(std::string host, int port) : host_(std::move(host)), port_(port) {
    signal(SIGPIPE, SIG_IGN);
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) throw std::runtime_error("socket failed");

    int yes = 1;
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port_));
    if (inet_pton(AF_INET, host_.c_str(), &addr.sin_addr) != 1) {
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    }
    if (bind(server_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        throw std::runtime_error("bind failed on " + host_ + ":" + std::to_string(port_) + " (" + std::strerror(errno) + ")");
    }
    if (listen(server_fd_, 64) < 0) {
        throw std::runtime_error("listen failed");
    }
}

HttpServer::~HttpServer() {
    if (server_fd_ >= 0) close(server_fd_);
}

void HttpServer::listenAndServe(const Handler& handler) const {
    std::cout << "InternStart C++ backend: http://" << host_ << ":" << port_ << "\n";
    while (true) {
        int client = accept(server_fd_, nullptr, nullptr);
        if (client < 0) continue;
        std::thread([client, handler]() {
            try {
                std::string raw;
                char buffer[8192];
                ssize_t n;
                while ((n = recv(client, buffer, sizeof(buffer), 0)) > 0) {
                    raw.append(buffer, buffer + n);
                    size_t header_end = raw.find("\r\n\r\n");
                    if (header_end != std::string::npos) {
                        size_t content_length = 0;
                        std::string header = raw.substr(0, header_end);
                        std::smatch match;
                        if (std::regex_search(header, match, std::regex("Content-Length:\\s*([0-9]+)", std::regex_constants::icase))) {
                            content_length = static_cast<size_t>(std::stoul(match[1]));
                        }
                        if (raw.size() >= header_end + 4 + content_length) break;
                    }
                }
                sendResponse(client, handler(parseRequest(raw)));
            } catch (const std::exception& exc) {
                Response r;
                r.status = 500;
                r.body = std::string("Internal error: ") + htmlEscape(exc.what());
                sendResponse(client, r);
            }
            close(client);
        }).detach();
    }
}

Request parseRequest(const std::string& raw) {
    Request request;
    size_t header_end = raw.find("\r\n\r\n");
    std::string head = raw.substr(0, header_end);
    request.body = header_end == std::string::npos ? "" : raw.substr(header_end + 4);

    std::istringstream stream(head);
    std::string line;
    std::getline(stream, line);
    if (!line.empty() && line.back() == '\r') line.pop_back();
    std::istringstream first(line);
    std::string target;
    first >> request.method >> target;
    size_t qpos = target.find('?');
    request.path = qpos == std::string::npos ? target : target.substr(0, qpos);
    request.query = qpos == std::string::npos ? "" : target.substr(qpos + 1);
    request.query_params = parseKeyValue(request.query);

    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string key = line.substr(0, colon);
        std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return std::tolower(c); });
        request.headers[key] = trim(line.substr(colon + 1));
    }
    if (request.method == "POST") request.form = parseKeyValue(request.body);
    return request;
}

Response redirectTo(const std::string& location) {
    Response response;
    response.status = 302;
    response.headers.push_back("Location: " + location);
    return response;
}

void sendResponse(int client, const Response& response) {
    std::ostringstream out;
    out << "HTTP/1.1 " << response.status << " " << statusText(response.status) << "\r\n"
        << "Content-Type: " << response.content_type << "\r\n"
        << "Content-Length: " << response.body.size() << "\r\n"
        << "Connection: close\r\n";
    for (const auto& header : response.headers) out << header << "\r\n";
    out << "\r\n" << response.body;
    const std::string bytes = out.str();
    ::send(client, bytes.data(), bytes.size(), 0);
}

} // namespace internstart
