#include "app.hpp"

#include <unistd.h>

#include <iostream>
#include <string>

#include "database.hpp"
#include "http.hpp"
#include "router.hpp"
#include "utils.hpp"

namespace internstart {

namespace {

std::string findProjectRoot(std::string start) {
    for (int i = 0; i < 8; ++i) {
        if (pathExists(start + "/cpp_backend/static/styles.css")) {
            return start;
        }
        if (pathExists(start + "/static/styles.css")) {
            const auto slash = start.find_last_of('/');
            if (slash != std::string::npos && slash != 0) {
                std::string parent = start.substr(0, slash);
                if (pathExists(parent + "/cpp_backend/static/styles.css")) return parent;
            }
            return start;
        }
        const auto slash = start.find_last_of('/');
        if (slash == std::string::npos || slash == 0) break;
        start = start.substr(0, slash);
    }
    return start;
}

} // namespace

Application::Application() {
    char cwd[4096];
    base_dir_ = getcwd(cwd, sizeof(cwd)) ? cwd : ".";
    base_dir_ = findProjectRoot(base_dir_);

    loadDotenvFile(base_dir_ + "/.env", false);
    loadDotenvFile(base_dir_ + "/.env.local", true);

    db_path_ = sqlitePathFromUrl(envOr("DATABASE_URL", "sqlite:///internships.db"), base_dir_);
    host_ = envOr("HOST", "127.0.0.1");
    port_ = std::stoi(envOr("PORT", "5000"));
}

int Application::run() {
    try {
        Database db(db_path_);
        initializeSchema(db);

        Router router(db_path_, base_dir_);
        HttpServer server(host_, port_);
        server.listenAndServe([&router](const Request& request) {
            return router.handle(request);
        });
        return 0;
    } catch (const std::exception& exc) {
        std::cerr << exc.what() << "\n";
        return 1;
    }
}

} // namespace internstart
