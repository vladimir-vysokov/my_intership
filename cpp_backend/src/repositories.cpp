#include "repositories.hpp"

#include "utils.hpp"

namespace internstart {

CompanyRepository::CompanyRepository(Database& db) : db_(db) {}

std::vector<Row> CompanyRepository::activeCompaniesWithCounts() const {
    return db_.query(
        "SELECT c.*, COUNT(DISTINCT i.id) AS active_internships_count, "
        "COALESCE((SELECT AVG(rating) FROM company_reviews WHERE company_id=c.id), 0) AS rating_average, "
        "(SELECT COUNT(*) FROM company_reviews WHERE company_id=c.id) AS rating_count "
        "FROM companies c "
        "LEFT JOIN internships i ON i.company_id = c.id AND i.is_published = 1 "
        "WHERE c.is_active = 1 AND c.created_by_user_id IS NULL GROUP BY c.id ORDER BY c.name COLLATE NOCASE ASC");
}

std::vector<Row> CompanyRepository::adminList() const {
    return db_.query("SELECT c.*, COUNT(i.id) AS internships_count FROM companies c LEFT JOIN internships i ON i.company_id = c.id GROUP BY c.id ORDER BY c.name COLLATE NOCASE ASC");
}

std::vector<Row> CompanyRepository::activeForSelect() const {
    return db_.query("SELECT id, name FROM companies WHERE is_active = 1 AND created_by_user_id IS NULL ORDER BY name COLLATE NOCASE ASC");
}

Row CompanyRepository::findActiveBySlug(const std::string& slug) const {
    return db_.one("SELECT * FROM companies WHERE slug = ? AND is_active = 1 AND created_by_user_id IS NULL", {slug});
}

Row CompanyRepository::findById(int id) const {
    return db_.one("SELECT * FROM companies WHERE id = ?", {std::to_string(id)});
}

Row CompanyRepository::ensureByName(const std::string& name) const {
    Row existing = db_.one("SELECT * FROM companies WHERE lower(name) = lower(?) AND created_by_user_id IS NULL LIMIT 1", {name});
    if (!existing.empty()) return existing;
    std::string base_slug = slugify(name);
    std::string slug = base_slug;
    int suffix = 2;
    while (!db_.one("SELECT id FROM companies WHERE slug = ?", {slug}).empty()) {
        slug = base_slug + "-" + std::to_string(suffix++);
    }
    std::string now = nowIso();
    int id = db_.run("INSERT INTO companies (name, slug, accent_color, is_active, created_at, updated_at) VALUES (?, ?, '#0e7490', 1, ?, ?)",
                     {name, slug, now, now});
    return findById(id);
}

Row CompanyRepository::ensurePrivateByName(const std::string& name, int user_id) const {
    Row existing = db_.one("SELECT * FROM companies WHERE lower(name)=lower(?) AND created_by_user_id=? LIMIT 1", {name, std::to_string(user_id)});
    if (!existing.empty()) return existing;
    std::string base_slug = slugify(name) + "-u" + std::to_string(user_id);
    std::string slug = base_slug;
    int suffix = 2;
    while (!db_.one("SELECT id FROM companies WHERE slug = ?", {slug}).empty()) {
        slug = base_slug + "-" + std::to_string(suffix++);
    }
    std::string now = nowIso();
    int id = db_.run(
        "INSERT INTO companies (name, slug, accent_color, is_active, created_by_user_id, created_at, updated_at) VALUES (?, ?, '#0e7490', 1, ?, ?, ?)",
        {name, slug, std::to_string(user_id), now, now});
    return findById(id);
}

void CompanyRepository::save(const Row& form, int id) const {
    std::string name = field(form, "name");
    std::string slug = field(form, "slug");
    if (slug.empty()) slug = slugify(name);
    std::string now = nowIso();
    if (id == 0) {
        db_.run("INSERT INTO companies (name, slug, website_url, career_url, description, internship_info, application_notes, accent_color, is_active, created_at, updated_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?, 1, ?, ?)",
                {name, slug, field(form, "website_url"), field(form, "career_url"), field(form, "description"), field(form, "internship_info"), field(form, "application_notes"), field(form, "accent_color", "#0e7490"), now, now});
    } else {
        db_.run("UPDATE companies SET name=?, slug=?, website_url=?, career_url=?, description=?, internship_info=?, application_notes=?, accent_color=?, updated_at=? WHERE id=?",
                {name, slug, field(form, "website_url"), field(form, "career_url"), field(form, "description"), field(form, "internship_info"), field(form, "application_notes"), field(form, "accent_color", "#0e7490"), now, std::to_string(id)});
    }
}

DirectionRepository::DirectionRepository(Database& db) : db_(db) {}

std::vector<Row> DirectionRepository::active() const {
    return db_.query("SELECT * FROM internship_directions WHERE is_active=1 ORDER BY name COLLATE NOCASE ASC");
}

std::vector<Row> DirectionRepository::all() const {
    return db_.query("SELECT * FROM internship_directions ORDER BY is_active DESC, name COLLATE NOCASE ASC");
}

void DirectionRepository::ensure(const std::string& name) const {
    std::string clean = trim(name);
    if (clean.empty()) clean = "other";
    Row existing = db_.one("SELECT id FROM internship_directions WHERE lower(name)=lower(?) LIMIT 1", {clean});
    if (!existing.empty()) return;
    db_.run("INSERT INTO internship_directions (name, is_active, created_at) VALUES (?, 1, ?)", {clean, nowIso()});
}

void DirectionRepository::save(const Row& form) const {
    ensure(field(form, "name", "other"));
}

void DirectionRepository::toggle(int id) const {
    db_.run("UPDATE internship_directions SET is_active = CASE WHEN is_active=1 THEN 0 ELSE 1 END WHERE id=?", {std::to_string(id)});
}

InternshipRepository::InternshipRepository(Database& db) : db_(db) {}

std::vector<Row> InternshipRepository::catalog(const std::map<std::string, std::string>& filters) const {
    std::vector<std::string> clauses = {"i.is_published = 1", "i.created_by_user_id IS NULL"};
    Params params;
    const std::string q = field(filters, "q");
    if (!q.empty()) {
        clauses.push_back("(i.title LIKE ? OR COALESCE(c.name, i.company_name) LIKE ? OR i.city LIKE ? OR i.short_description LIKE ?)");
        std::string pattern = "%" + q + "%";
        params.insert(params.end(), {pattern, pattern, pattern, pattern});
    }
    if (!field(filters, "city").empty()) {
        clauses.push_back("i.city = ?");
        params.push_back(field(filters, "city"));
    }
    if (!field(filters, "work_format").empty()) {
        clauses.push_back("i.work_format = ?");
        params.push_back(field(filters, "work_format"));
    }
    if (!field(filters, "direction").empty()) {
        clauses.push_back("i.direction = ?");
        params.push_back(field(filters, "direction"));
    }
    if (!field(filters, "paid").empty()) {
        clauses.push_back("i.is_paid = ?");
        params.push_back(field(filters, "paid"));
    }
    std::string where;
    for (size_t i = 0; i < clauses.size(); ++i) {
        if (i) where += " AND ";
        where += clauses[i];
    }
    return db_.query(
        "SELECT i.*, c.slug AS company_slug, c.accent_color AS company_accent_color, COALESCE(c.name, i.company_name) AS company_display_name "
        "FROM internships i LEFT JOIN companies c ON c.id = i.company_id WHERE " + where +
        " ORDER BY datetime(i.created_at) DESC", params);
}

std::vector<Row> InternshipRepository::byCompany(int company_id) const {
    return db_.query("SELECT * FROM internships WHERE company_id = ? AND is_published = 1 AND created_by_user_id IS NULL ORDER BY datetime(created_at) DESC", {std::to_string(company_id)});
}

std::vector<Row> InternshipRepository::adminList() const {
    return db_.query("SELECT i.*, COALESCE(c.name, i.company_name) AS company_display_name FROM internships i LEFT JOIN companies c ON c.id = i.company_id ORDER BY datetime(i.created_at) DESC");
}

Row InternshipRepository::publicDetail(int id) const {
    return db_.one(
        "SELECT i.*, c.slug AS company_slug, c.accent_color AS company_accent_color, c.internship_info AS company_internship_info, c.application_notes AS company_application_notes, COALESCE(c.name, i.company_name) AS company_display_name "
        "FROM internships i LEFT JOIN companies c ON c.id = i.company_id WHERE i.id = ? AND i.is_published = 1", {std::to_string(id)});
}

Row InternshipRepository::findById(int id) const {
    return db_.one("SELECT * FROM internships WHERE id = ?", {std::to_string(id)});
}

int InternshipRepository::save(const Row& form, int id) const {
    Row company = db_.one("SELECT id, name, logo_path FROM companies WHERE id = ?", {field(form, "company_id")});
    if (company.empty()) return 0;
    std::string now = nowIso();
    Params params = {
        field(form, "title"), company["id"], company["name"], field(form, "city", "Не указано"),
        field(form, "work_format", "office"), field(form, "direction", "other"),
        field(form, "employment_type", "Не указано"), field(form, "short_description"),
        field(form, "full_description"), field(form, "source_url"), field(form, "source_url"),
        field(form, "status", "open"), form.count("is_published") ? "1" : "0",
        field(form, "created_by_type", "human"), field(form, "created_by_user_id"), now
    };
    if (id == 0) {
        params.push_back(now);
        return db_.run("INSERT INTO internships (title, company_id, company_name, city, work_format, direction, employment_type, is_paid, short_description, full_description, source_url, application_url, status, is_published, created_by_type, created_by_user_id, ai_generated, needs_review, updated_at, created_at) VALUES (?, ?, ?, ?, ?, ?, ?, -1, ?, ?, ?, ?, ?, ?, ?, ?, 0, 0, ?, ?)", params);
    } else {
        params.push_back(std::to_string(id));
        db_.run("UPDATE internships SET title=?, company_id=?, company_name=?, city=?, work_format=?, direction=?, employment_type=?, short_description=?, full_description=?, source_url=?, application_url=?, status=?, is_published=?, created_by_type=?, created_by_user_id=?, updated_at=? WHERE id=?", params);
        return id;
    }
}

CompanyReviewRepository::CompanyReviewRepository(Database& db) : db_(db) {}

CompanyRatingSummary CompanyReviewRepository::summaryForCompany(int company_id) const {
    CompanyRatingSummary summary;
    Row average = db_.one("SELECT COALESCE(AVG(rating), 0) AS average, COUNT(*) AS count FROM company_reviews WHERE company_id=?", {std::to_string(company_id)});
    if (!average.empty()) {
        summary.average = average["average"].empty() ? 0.0 : std::stod(average["average"]);
        summary.count = average["count"].empty() ? 0 : std::stoi(average["count"]);
    }
    for (const auto& row : db_.query("SELECT rating, COUNT(*) AS c FROM company_reviews WHERE company_id=? GROUP BY rating", {std::to_string(company_id)})) {
        int rating = std::stoi(row.at("rating"));
        if (rating >= 1 && rating <= 5) summary.stars[rating - 1] = std::stoi(row.at("c"));
    }
    return summary;
}

std::vector<Row> CompanyReviewRepository::reviewsForCompany(int company_id) const {
    return db_.query(
        "SELECT r.*, i.title AS internship_title, a.status AS application_status "
        "FROM company_reviews r "
        "JOIN application_attempts a ON a.id = r.attempt_id "
        "JOIN internships i ON i.id = a.internship_id "
        "WHERE r.company_id=? ORDER BY datetime(r.updated_at) DESC, r.id DESC",
        {std::to_string(company_id)});
}

std::vector<Row> CompanyReviewRepository::eligibleCompanies(int user_id) const {
    return db_.query(
        "SELECT c.id, c.name, c.slug, c.accent_color, COUNT(a.id) AS attempts_count, "
        "MAX(CASE WHEN r.id IS NULL THEN 0 ELSE 1 END) AS reviewed, COALESCE(MAX(r.rating), 0) AS user_rating "
        "FROM application_attempts a "
        "JOIN internships i ON i.id = a.internship_id "
        "JOIN companies c ON c.id = i.company_id "
        "LEFT JOIN company_reviews r ON r.company_id = c.id AND r.user_id = a.user_id "
        "WHERE a.user_id=? AND a.status IN ('offer', 'rejected') "
        "GROUP BY c.id ORDER BY c.name COLLATE NOCASE ASC",
        {std::to_string(user_id)});
}

std::vector<Row> CompanyReviewRepository::eligibleAttempts(int user_id, int company_id) const {
    return db_.query(
        "SELECT a.id, a.status, i.title AS internship_title "
        "FROM application_attempts a JOIN internships i ON i.id = a.internship_id "
        "WHERE a.user_id=? AND i.company_id=? AND a.status IN ('offer', 'rejected') "
        "ORDER BY datetime(a.updated_at) DESC, a.id DESC",
        {std::to_string(user_id), std::to_string(company_id)});
}

Row CompanyReviewRepository::userReview(int user_id, int company_id) const {
    return db_.one("SELECT * FROM company_reviews WHERE user_id=? AND company_id=?", {std::to_string(user_id), std::to_string(company_id)});
}

void CompanyReviewRepository::saveReview(int user_id, int company_id, const Row& form) const {
    int rating = 0;
    try {
        rating = std::stoi(field(form, "rating", "0"));
    } catch (...) {
        rating = 0;
    }
    if (rating < 1) rating = 1;
    if (rating > 5) rating = 5;
    const std::string attempt_id = field(form, "attempt_id");
    Row eligible = db_.one(
        "SELECT a.id FROM application_attempts a JOIN internships i ON i.id = a.internship_id "
        "WHERE a.id=? AND a.user_id=? AND i.company_id=? AND a.status IN ('offer', 'rejected')",
        {attempt_id, std::to_string(user_id), std::to_string(company_id)});
    if (eligible.empty()) return;
    const std::string now = nowIso();
    Row existing = userReview(user_id, company_id);
    if (existing.empty()) {
        db_.run("INSERT INTO company_reviews (company_id, user_id, attempt_id, rating, comment, created_at, updated_at) VALUES (?, ?, ?, ?, ?, ?, ?)",
                {std::to_string(company_id), std::to_string(user_id), attempt_id, std::to_string(rating), field(form, "comment"), now, now});
    } else {
        db_.run("UPDATE company_reviews SET attempt_id=?, rating=?, comment=?, updated_at=? WHERE user_id=? AND company_id=?",
                {attempt_id, std::to_string(rating), field(form, "comment"), now, std::to_string(user_id), std::to_string(company_id)});
    }
}

void InternshipRepository::togglePublish(int id) const {
    db_.run("UPDATE internships SET is_published = CASE WHEN is_published=1 THEN 0 ELSE 1 END, updated_at=? WHERE id=?", {nowIso(), std::to_string(id)});
}

void InternshipRepository::markVerified(int id) const {
    db_.run("UPDATE internships SET last_verified_at=?, needs_review=0, updated_at=? WHERE id=?", {nowIso(), nowIso(), std::to_string(id)});
}

void InternshipRepository::remove(int id) const {
    db_.run("DELETE FROM internships WHERE id=?", {std::to_string(id)});
}

ApplicationRepository::ApplicationRepository(Database& db) : db_(db) {}

std::vector<Row> ApplicationRepository::board(int user_id) const {
    return db_.query(
        "SELECT a.*, i.title AS internship_title, i.city AS internship_city, COALESCE(c.name, i.company_name) AS company_display_name "
        "FROM application_attempts a JOIN internships i ON i.id = a.internship_id LEFT JOIN companies c ON c.id = i.company_id "
        "WHERE a.user_id = ? "
        "ORDER BY CASE a.status WHEN 'want_to_apply' THEN 1 WHEN 'applied' THEN 2 WHEN 'test' THEN 3 WHEN 'interview' THEN 4 WHEN 'offer' THEN 5 WHEN 'rejected' THEN 6 WHEN 'archived' THEN 7 ELSE 99 END, datetime(a.updated_at) DESC",
        {std::to_string(user_id)});
}

Row ApplicationRepository::latestForInternship(int internship_id, int user_id) const {
    return db_.one("SELECT * FROM application_attempts WHERE internship_id = ? AND user_id = ? ORDER BY datetime(created_at) DESC, id DESC LIMIT 1", {std::to_string(internship_id), std::to_string(user_id)});
}

Row ApplicationRepository::findAttempt(int attempt_id, int user_id) const {
    return db_.one(
        "SELECT a.*, i.title AS internship_title, i.source_url AS internship_source_url, COALESCE(c.name, i.company_name) AS company_display_name "
        "FROM application_attempts a JOIN internships i ON i.id = a.internship_id LEFT JOIN companies c ON c.id = i.company_id WHERE a.id = ? AND a.user_id = ?",
        {std::to_string(attempt_id), std::to_string(user_id)});
}

std::vector<Row> ApplicationRepository::attemptsForInternship(int internship_id, int user_id) const {
    return db_.query(
        "SELECT a.*, i.title AS internship_title FROM application_attempts a JOIN internships i ON i.id = a.internship_id WHERE a.internship_id = ? AND a.user_id = ? ORDER BY datetime(a.created_at) ASC, a.id ASC",
        {std::to_string(internship_id), std::to_string(user_id)});
}

std::vector<Row> ApplicationRepository::historyForAttempt(int attempt_id) const {
    return db_.query("SELECT * FROM application_history WHERE attempt_id = ? ORDER BY datetime(created_at) ASC, id ASC", {std::to_string(attempt_id)});
}

int ApplicationRepository::createAttempt(int internship_id, int user_id, ApplicationStatus status) const {
    const std::string now = nowIso();
    const std::string status_text = toString(status);
    int id = db_.run("INSERT INTO application_attempts (internship_id, user_id, status, marker_enabled, stage_completed, created_at, updated_at) VALUES (?, ?, ?, 1, 1, ?, ?)",
                     {std::to_string(internship_id), std::to_string(user_id), status_text, now, now});
    db_.run("INSERT INTO application_history (attempt_id, event_type, to_status, created_at) VALUES (?, 'create', ?, ?)",
            {std::to_string(id), status_text, now});
    return id;
}

void ApplicationRepository::updateAttemptDetails(int attempt_id, int user_id, const Row& form) const {
    const std::string marker_enabled = field(form, "marker_enabled") == "1" ? "1" : "0";
    Row current = db_.one("SELECT stage_completed, marker_enabled FROM application_attempts WHERE id = ? AND user_id = ?", {std::to_string(attempt_id), std::to_string(user_id)});
    if (current.empty()) return;
    std::string stage_completed = current["stage_completed"].empty() ? "0" : current["stage_completed"];
    if (marker_enabled == "0") stage_completed = "0";
    if (current["marker_enabled"] == "0" && marker_enabled == "1") stage_completed = "0";
    db_.run(
        "UPDATE application_attempts SET marker_enabled=?, stage_completed=?, next_step_date=?, next_step_time=?, note=?, applied_at=?, updated_at=? WHERE id=? AND user_id=?",
        {marker_enabled, stage_completed, field(form, "next_step_date"), field(form, "next_step_time"), field(form, "note"), field(form, "applied_at"), nowIso(), std::to_string(attempt_id), std::to_string(user_id)});

    Row last_stage = db_.one(
        "SELECT id FROM application_history WHERE attempt_id = ? AND event_type != 'completion' ORDER BY id DESC LIMIT 1",
        {std::to_string(attempt_id)});
    if (!last_stage.empty()) {
        db_.run("UPDATE application_history SET stage_deadline_date=?, stage_deadline_time=? WHERE id=?",
                {field(form, "next_step_date"), field(form, "next_step_time"), last_stage["id"]});
    }
}

void ApplicationRepository::moveAttempt(int attempt_id, int user_id, ApplicationStatus status) const {
    Row row = db_.one("SELECT status FROM application_attempts WHERE id = ? AND user_id = ?", {std::to_string(attempt_id), std::to_string(user_id)});
    if (row.empty()) return;
    const std::string old_status = row["status"];
    const std::string new_status = toString(status);
    const std::string now = nowIso();
    db_.run("UPDATE application_attempts SET status = ?, stage_completed = 0, next_step_date = NULL, next_step_time = NULL, updated_at = ? WHERE id = ? AND user_id = ?",
            {new_status, now, std::to_string(attempt_id), std::to_string(user_id)});
    if (old_status != new_status) {
        db_.run("INSERT INTO application_history (attempt_id, event_type, from_status, to_status, created_at) VALUES (?, 'move', ?, ?, ?)",
                {std::to_string(attempt_id), old_status, new_status, now});
    }
}

int ApplicationRepository::addStageEvent(int attempt_id, int user_id) const {
    Row row = db_.one("SELECT status, next_step_date, next_step_time FROM application_attempts WHERE id = ? AND user_id = ?", {std::to_string(attempt_id), std::to_string(user_id)});
    if (row.empty()) return 0;
    Row prev_stage = db_.one("SELECT id FROM application_history WHERE attempt_id = ? AND event_type != 'completion' ORDER BY id DESC LIMIT 1", {std::to_string(attempt_id)});
    if (!prev_stage.empty()) {
        db_.run("UPDATE application_history SET stage_deadline_date=?, stage_deadline_time=? WHERE id=?",
                {row["next_step_date"], row["next_step_time"], prev_stage["id"]});
    }
    db_.run("UPDATE application_attempts SET next_step_date=NULL, next_step_time=NULL, stage_completed=CASE WHEN marker_enabled=1 THEN 0 ELSE stage_completed END, updated_at=? WHERE id=? AND user_id=?",
            {nowIso(), std::to_string(attempt_id), std::to_string(user_id)});
    return db_.run(
        "INSERT INTO application_history (attempt_id, event_type, from_status, to_status, created_at) VALUES (?, 'stage', ?, ?, ?)",
        {std::to_string(attempt_id), row["status"], row["status"], nowIso()});
}

void ApplicationRepository::updateEventComment(int attempt_id, int user_id, int event_id, const std::string& comment) const {
    Row attempt = db_.one("SELECT id FROM application_attempts WHERE id=? AND user_id=?", {std::to_string(attempt_id), std::to_string(user_id)});
    if (attempt.empty()) return;
    db_.run("UPDATE application_history SET event_note=? WHERE id=? AND attempt_id=?", {comment, std::to_string(event_id), std::to_string(attempt_id)});
}

void ApplicationRepository::updateEventCompletion(int attempt_id, int user_id, int event_id, bool completed) const {
    Row attempt = db_.one("SELECT status FROM application_attempts WHERE id=? AND user_id=?", {std::to_string(attempt_id), std::to_string(user_id)});
    if (attempt.empty()) return;
    Row event = db_.one("SELECT id FROM application_history WHERE id=? AND attempt_id=?", {std::to_string(event_id), std::to_string(attempt_id)});
    if (event.empty()) return;
    const std::string completed_text = completed ? "1" : "0";
    db_.run("UPDATE application_attempts SET stage_completed=?, updated_at=? WHERE id=? AND user_id=?", {completed_text, nowIso(), std::to_string(attempt_id), std::to_string(user_id)});
    db_.run(
        "INSERT INTO application_history (attempt_id, event_type, from_status, to_status, completion_state, event_note, created_at) VALUES (?, 'completion', ?, ?, ?, ?, ?)",
        {std::to_string(attempt_id), attempt["status"], attempt["status"], completed_text, completed ? "Этап отмечен как выполненный." : "Отметка выполнения этапа снята.", nowIso()});
}

void ApplicationRepository::deleteAttempt(int attempt_id, int user_id) const {
    db_.run("DELETE FROM application_attempts WHERE id=? AND user_id=?", {std::to_string(attempt_id), std::to_string(user_id)});
}

UserRepository::UserRepository(Database& db) : db_(db) {}

Row UserRepository::findById(int id) const {
    return db_.one("SELECT id, email, created_at FROM users WHERE id=?", {std::to_string(id)});
}

Row UserRepository::findByEmail(const std::string& email) const {
    return db_.one("SELECT id, email, password, created_at FROM users WHERE lower(email)=lower(?) LIMIT 1", {email});
}

std::vector<Row> UserRepository::all() const {
    return db_.query("SELECT id, email, created_at FROM users ORDER BY datetime(created_at) DESC, id DESC");
}

int UserRepository::create(const std::string& email, const std::string& password) const {
    Row existing = findByEmail(email);
    if (!existing.empty()) return std::stoi(existing["id"]);
    return db_.run("INSERT INTO users (email, password, created_at) VALUES (?, ?, ?)", {email, password, nowIso()});
}

bool UserRepository::verify(const std::string& email, const std::string& password) const {
    Row user = findByEmail(email);
    return !user.empty() && user["password"] == password;
}

} // namespace internstart
