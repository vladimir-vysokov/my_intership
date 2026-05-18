#pragma once

#include <map>
#include <string>

namespace internstart {

void loadDotenvFile(const std::string& path, bool override_existing);
std::string envOr(const std::string& key, const std::string& fallback);
std::string trim(const std::string& value);
std::string nowIso();
std::string htmlEscape(const std::string& value);
std::string urlDecode(const std::string& value);
std::map<std::string, std::string> parseKeyValue(const std::string& body);
std::string field(const std::map<std::string, std::string>& values, const std::string& key, const std::string& fallback = "");
bool pathExists(const std::string& path);
std::string readFile(const std::string& path);
std::string slugify(const std::string& value);
std::string sqlitePathFromUrl(const std::string& database_url, const std::string& base_dir);

} // namespace internstart
