#pragma once

#include <string>

#include "controllers.hpp"

namespace internstart {

class Router {
public:
    Router(std::string db_path, std::string base_dir);

    Response handle(const Request& request) const;

private:
    std::string db_path_;
    PageRenderer renderer_;
};

bool pathIntTail(const std::string& path, const std::string& prefix, int& id, const std::string& suffix = "");

} // namespace internstart
