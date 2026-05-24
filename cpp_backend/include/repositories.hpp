#pragma once

#include <string>
#include <vector>

#include "database.hpp"

namespace internstart {

class CompanyRepository {
public:
    explicit CompanyRepository(Database& db);

    std::vector<Row> activeCompaniesWithCounts() const;
    std::vector<Row> adminList() const;
    std::vector<Row> activeForSelect() const;
    Row findActiveBySlug(const std::string& slug) const;
    Row findById(int id) const;
    Row ensureByName(const std::string& name) const;
    Row ensurePrivateByName(const std::string& name, int user_id) const;
    void save(const Row& form, int id = 0) const;
    void remove(int id) const;

private:
    Database& db_;
};

class DirectionRepository {
public:
    explicit DirectionRepository(Database& db);

    std::vector<Row> active() const;
    std::vector<Row> all() const;
    void ensure(const std::string& name) const;
    void save(const Row& form) const;
    void toggle(int id) const;

private:
    Database& db_;
};

class InternshipRepository {
public:
    explicit InternshipRepository(Database& db);

    std::vector<Row> catalog(const std::map<std::string, std::string>& filters) const;
    std::vector<Row> byCompany(int company_id) const;
    std::vector<Row> adminList() const;
    Row publicDetail(int id) const;
    Row findById(int id) const;
    int save(const Row& form, int id = 0) const;
    void togglePublish(int id) const;
    void markVerified(int id) const;
    void remove(int id) const;

private:
    Database& db_;
};

class CompanyReviewRepository {
public:
    explicit CompanyReviewRepository(Database& db);

    CompanyRatingSummary summaryForCompany(int company_id) const;
    std::vector<Row> reviewsForCompany(int company_id) const;
    std::vector<Row> eligibleCompanies(int user_id) const;
    std::vector<Row> eligibleAttempts(int user_id, int company_id) const;
    Row userReview(int user_id, int company_id) const;
    void saveReview(int user_id, int company_id, const Row& form) const;

private:
    Database& db_;
};

class ApplicationRepository {
public:
    explicit ApplicationRepository(Database& db);

    std::vector<Row> board(int user_id) const;
    Row latestForInternship(int internship_id, int user_id) const;
    Row findAttempt(int attempt_id, int user_id) const;
    std::vector<Row> attemptsForInternship(int internship_id, int user_id) const;
    std::vector<Row> historyForAttempt(int attempt_id) const;
    int createAttempt(int internship_id, int user_id, ApplicationStatus status) const;
    void updateAttemptDetails(int attempt_id, int user_id, const Row& form) const;
    void moveAttempt(int attempt_id, int user_id, ApplicationStatus status) const;
    int addStageEvent(int attempt_id, int user_id) const;
    void updateEventComment(int attempt_id, int user_id, int event_id, const std::string& comment) const;
    void updateEventCompletion(int attempt_id, int user_id, int event_id, bool completed) const;
    void deleteAttempt(int attempt_id, int user_id) const;

private:
    Database& db_;
};

class UserRepository {
public:
    explicit UserRepository(Database& db);

    Row findById(int id) const;
    Row findByEmail(const std::string& email) const;
    std::vector<Row> all() const;
    int create(const std::string& email, const std::string& password) const;
    bool verify(const std::string& email, const std::string& password) const;

private:
    Database& db_;
};

} // namespace internstart
