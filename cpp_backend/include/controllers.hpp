#pragma once

#include <string>

#include "http.hpp"
#include "repositories.hpp"

namespace internstart {

class PageRenderer {
public:
    explicit PageRenderer(std::string base_dir);

    std::string layout(const std::string& title, const std::string& body, bool admin = false, const std::string& active = "", const std::string& user_email = "") const;
    Response staticFile(const std::string& request_path) const;

private:
    std::string base_dir_;
};

class PublicController {
public:
    PublicController(Database& db, const PageRenderer& renderer);

    Response home(const Request& request) const;
    Response registerUser(const Request& request) const;
    Response loginUser(const Request& request) const;
    Response logoutUser() const;
    Response catalog(const Request& request) const;
    Response internshipDetail(int id, const Request& request) const;
    Response companies(const Request& request) const;
    Response companyDetail(const std::string& slug, const Request& request) const;
    Response myRatings(const Request& request) const;
    Response rateCompany(int company_id, const Request& request) const;

private:
    Row currentUser(const Request& request) const;
    Response requireUser(const Request& request) const;

    Database& db_;
    const PageRenderer& renderer_;
};

class ApplicationController {
public:
    ApplicationController(Database& db, const PageRenderer& renderer, int user_id = 0);

    Response board() const;
    Response newInternship(const Request& request) const;
    Response addToTracker(int internship_id) const;
    Response edit(int attempt_id, const Request& request) const;
    Response moveForm(int attempt_id, const Request& request) const;
    Response moveJson(int attempt_id, const Request& request) const;
    Response archiveJson(int attempt_id) const;
    Response addEvent(int attempt_id) const;
    Response updateEventComment(int attempt_id, int event_id, const Request& request) const;
    Response updateEventCompletion(int attempt_id, int event_id, const Request& request) const;
    Response deleteAttempt(int attempt_id) const;

private:
    Database& db_;
    const PageRenderer& renderer_;
    int user_id_ = 0;
};

class AdminController {
public:
    AdminController(Database& db, const PageRenderer& renderer);

    Response login(const Request& request) const;
    Response accounts(const Request& request) const;
    Response newAccount(const Request& request) const;
    Response internships(const Request& request) const;
    Response newInternship(const Request& request) const;
    Response editInternship(int id, const Request& request) const;
    Response togglePublish(int id, const Request& request) const;
    Response verifyInternship(int id, const Request& request) const;
    Response deleteInternship(int id, const Request& request) const;
    Response companies(const Request& request) const;
    Response newCompany(const Request& request) const;
    Response editCompany(int id, const Request& request) const;
    Response directions(const Request& request) const;
    Response toggleDirection(int id, const Request& request) const;
    Response imports(const Request& request) const;

private:
    bool isAdmin(const Request& request) const;
    Response requireAdmin(const Request& request) const;
    std::string internshipForm(const Row& row, const std::string& action) const;
    std::string companyForm(const Row& row, const std::string& action) const;

    Database& db_;
    const PageRenderer& renderer_;
};

} // namespace internstart
