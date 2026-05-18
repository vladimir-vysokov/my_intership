#pragma once

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace internstart {

struct Request {
    std::string method;
    std::string path;
    std::string query;
    std::string body;
    std::map<std::string, std::string> headers;
    std::map<std::string, std::string> query_params;
    std::map<std::string, std::string> form;
};

struct Response {
    int status = 200;
    std::string content_type = "text/html; charset=utf-8";
    std::string body;
    std::vector<std::string> headers;
};

class HttpServer {
public:
    using Handler = std::function<Response(const Request&)>;

    HttpServer(std::string host, int port);
    ~HttpServer();

    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    void listenAndServe(const Handler& handler) const;

private:
    int server_fd_ = -1;
    std::string host_;
    int port_ = 0;
};

Request parseRequest(const std::string& raw);
Response redirectTo(const std::string& location);
void sendResponse(int client, const Response& response);

} // namespace internstart
