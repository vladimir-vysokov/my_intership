#pragma once

#include <string>

namespace internstart {

class Application {
public:
    Application();
    int run();

private:
    std::string base_dir_;
    std::string db_path_;
    std::string host_;
    int port_ = 5000;
};

} // namespace internstart
