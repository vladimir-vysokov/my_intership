#include "router.hpp"

#include <algorithm>
#include <regex>

#include "database.hpp"
#include "repositories.hpp"

namespace internstart {

bool pathIntTail(const std::string& path, const std::string& prefix, int& id, const std::string& suffix) {
    if (path.rfind(prefix, 0) != 0) return false;
    std::string middle = path.substr(prefix.size());
    if (!suffix.empty()) {
        if (middle.size() <= suffix.size() || middle.substr(middle.size() - suffix.size()) != suffix) return false;
        middle = middle.substr(0, middle.size() - suffix.size());
    }
    if (middle.empty() || !std::all_of(middle.begin(), middle.end(), [](char c) { return std::isdigit(static_cast<unsigned char>(c)); })) return false;
    id = std::stoi(middle);
    return true;
}

static bool eventPath(const std::string& path, const std::string& suffix, int& attempt_id, int& event_id) {
    std::regex pattern("^/applications/([0-9]+)/events/([0-9]+)" + suffix + "$");
    std::smatch match;
    if (!std::regex_match(path, match, pattern)) return false;
    attempt_id = std::stoi(match[1]);
    event_id = std::stoi(match[2]);
    return true;
}

static std::string cookieValue(const Request& request, const std::string& key) {
    auto it = request.headers.find("cookie");
    if (it == request.headers.end()) return "";
    std::string needle = key + "=";
    size_t pos = it->second.find(needle);
    if (pos == std::string::npos) return "";
    pos += needle.size();
    size_t end = it->second.find(';', pos);
    return it->second.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
}

static int currentUserId(Database& db, const Request& request) {
    std::string user_id = cookieValue(request, "user_id");
    if (user_id.empty()) return 0;
    try {
        UserRepository users(db);
        int id = std::stoi(user_id);
        return !users.findById(id).empty() ? id : 0;
    } catch (...) {
        return 0;
    }
}

Router::Router(std::string db_path, std::string base_dir)
    : db_path_(std::move(db_path)), renderer_(std::move(base_dir)) {}

Response Router::handle(const Request& request) const {
    if (request.path.rfind("/static/", 0) == 0) return renderer_.staticFile(request.path);

    Database db(db_path_);
    PublicController public_controller(db, renderer_);
    AdminController admin_controller(db, renderer_);

    int id = 0;
    if (request.method == "GET" && request.path == "/") return public_controller.home(request);
    if (request.method == "POST" && request.path == "/register") return public_controller.registerUser(request);
    if (request.method == "POST" && request.path == "/login") return public_controller.loginUser(request);
    if (request.method == "POST" && request.path == "/logout") return public_controller.logoutUser();

    const bool admin_path = request.path.rfind("/admin", 0) == 0;
    int user_id = currentUserId(db, request);
    if (!admin_path && user_id == 0) return redirectTo("/");
    ApplicationController application_controller(db, renderer_, user_id);

    if (request.method == "GET" && request.path == "/internships") return public_controller.catalog(request);
    if (request.method == "GET" && pathIntTail(request.path, "/internships/", id)) return public_controller.internshipDetail(id, request);
    if (request.method == "GET" && request.path == "/companies") return public_controller.companies(request);
    if (request.method == "GET" && request.path.rfind("/companies/", 0) == 0) return public_controller.companyDetail(request.path.substr(std::string("/companies/").size()), request);
    if (request.method == "GET" && request.path == "/ratings") return public_controller.myRatings(request);
    if ((request.method == "GET" || request.method == "POST") && pathIntTail(request.path, "/ratings/companies/", id)) return public_controller.rateCompany(id, request);

    if (request.method == "GET" && request.path == "/applications") return application_controller.board();
    if ((request.method == "GET" || request.method == "POST") && request.path == "/applications/new-internship") return application_controller.newInternship(request);
    if (request.method == "POST" && pathIntTail(request.path, "/applications/add/", id)) return application_controller.addToTracker(id);
    if ((request.method == "GET" || request.method == "POST") && pathIntTail(request.path, "/applications/", id, "/edit")) return application_controller.edit(id, request);
    if (request.method == "POST" && pathIntTail(request.path, "/applications/", id, "/move-form")) return application_controller.moveForm(id, request);
    if (request.method == "POST" && pathIntTail(request.path, "/applications/", id, "/move")) return application_controller.moveJson(id, request);
    if (request.method == "POST" && pathIntTail(request.path, "/applications/", id, "/archive-move")) return application_controller.archiveJson(id);
    if (request.method == "POST" && pathIntTail(request.path, "/applications/", id, "/events")) return application_controller.addEvent(id);
    if (request.method == "POST" && pathIntTail(request.path, "/applications/", id, "/delete")) return application_controller.deleteAttempt(id);
    int event_id = 0;
    if (request.method == "POST" && eventPath(request.path, "/comment", id, event_id)) return application_controller.updateEventComment(id, event_id, request);
    if (request.method == "POST" && eventPath(request.path, "/completion", id, event_id)) return application_controller.updateEventCompletion(id, event_id, request);

    if ((request.method == "GET" || request.method == "POST") && request.path == "/admin/login") return admin_controller.login(request);
    if (request.method == "GET" && request.path == "/admin") return redirectTo("/admin/accounts");
    if (request.method == "GET" && request.path == "/admin/accounts") return admin_controller.accounts(request);
    if ((request.method == "GET" || request.method == "POST") && request.path == "/admin/accounts/new") return admin_controller.newAccount(request);
    if (request.method == "GET" && request.path == "/admin/internships") return admin_controller.internships(request);
    if ((request.method == "GET" || request.method == "POST") && request.path == "/admin/internships/new") return admin_controller.newInternship(request);
    if ((request.method == "GET" || request.method == "POST") && pathIntTail(request.path, "/admin/internships/", id, "/edit")) return admin_controller.editInternship(id, request);
    if (request.method == "POST" && pathIntTail(request.path, "/admin/internships/", id, "/toggle-publish")) return admin_controller.togglePublish(id, request);
    if (request.method == "POST" && pathIntTail(request.path, "/admin/internships/", id, "/verify")) return admin_controller.verifyInternship(id, request);
    if (request.method == "POST" && pathIntTail(request.path, "/admin/internships/", id, "/delete")) return admin_controller.deleteInternship(id, request);
    if (request.method == "GET" && request.path == "/admin/companies") return admin_controller.companies(request);
    if ((request.method == "GET" || request.method == "POST") && request.path == "/admin/companies/new") return admin_controller.newCompany(request);
    if ((request.method == "GET" || request.method == "POST") && pathIntTail(request.path, "/admin/companies/", id, "/edit")) return admin_controller.editCompany(id, request);
    if ((request.method == "GET" || request.method == "POST") && request.path == "/admin/directions") return admin_controller.directions(request);
    if (request.method == "POST" && pathIntTail(request.path, "/admin/directions/", id, "/toggle")) return admin_controller.toggleDirection(id, request);
    if (request.method == "GET" && request.path == "/admin/imports") return admin_controller.imports(request);

    Response response;
    response.status = 404;
    response.body = renderer_.layout("404", "<h1>Страница не найдена</h1>");
    return response;
}

} // namespace internstart
