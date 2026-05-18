#pragma once

#include <sqlite3.h>

#include <string>
#include <vector>

#include "domain.hpp"

namespace internstart {

class Database {
public:
    explicit Database(std::string path);
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    void exec(const std::string& sql);
    std::vector<Row> query(const std::string& sql, const Params& params = {});
    Row one(const std::string& sql, const Params& params = {});
    int run(const std::string& sql, const Params& params = {});

private:
    sqlite3_stmt* prepare(const std::string& sql, const Params& params);

    sqlite3* db_ = nullptr;
    std::string path_;
};

void initializeSchema(Database& db);

} // namespace internstart
