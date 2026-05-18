#include "database.hpp"

#include <stdexcept>
#include <map>
#include <vector>

#include "utils.hpp"

namespace internstart {

namespace {

struct DemoUserSeed {
    const char* email;
    const char* password;
    const char* reviewer_name;
};

const std::vector<DemoUserSeed> kDemoUsers = {
    {"demo.intern1@internstart.test", "demo1234", "frontend intern"},
    {"demo.intern2@internstart.test", "demo1234", "backend intern"},
    {"demo.intern3@internstart.test", "demo1234", "analytics intern"},
    {"demo.intern4@internstart.test", "demo1234", "qa intern"},
    {"demo.intern5@internstart.test", "demo1234", "product intern"}
};

std::string companyFallback(const Row& row, const std::string& key, const std::string& fallback) {
    auto it = row.find(key);
    return it == row.end() || it->second.empty() ? fallback : it->second;
}

int ensureUser(Database& db, const DemoUserSeed& seed) {
    Row existing = db.one("SELECT id FROM users WHERE lower(email)=lower(?)", {seed.email});
    if (!existing.empty()) return std::stoi(existing["id"]);
    return db.run("INSERT INTO users (email, password, created_at) VALUES (?, ?, ?)", {seed.email, seed.password, nowIso()});
}

int ensureDemoInternship(Database& db, const Row& company, int index) {
    const std::string company_id = company.at("id");
    const std::string company_name = company.at("name");
    const std::string slug = companyFallback(company, "slug", "company");
    const std::string source = "demo://internship/" + slug + "/" + std::to_string(index);
    Row existing = db.one("SELECT id FROM internships WHERE source_url=?", {source});
    const std::string title = index == 1 ? "Software Engineering Intern" : "Product Analytics Intern";
    const std::string direction = index == 1 ? "backend" : "data/analytics";
    const std::string city = index == 1 ? "Москва" : "Удалённо";
    const std::string format = index == 1 ? "hybrid" : "remote";
    const std::string deadline = index == 1 ? "2026-06-12" : "2026-06-24";
    const std::string short_description = "Демо-стажировка в " + company_name + " для витрины MVP.";
    const std::string full_description =
        "Команда " + company_name + " ищет стажера, который хочет делать реальные продуктовые задачи.\n"
        "На программе есть наставник, понятный план развития и регулярная обратная связь.\n"
        "Формат рассчитан на студентов, которым важно совмещать учебу и практику.";
    const std::string requirements =
        "1. Базовое понимание выбранного направления\n"
        "2. Умение задавать вопросы и разбирать обратную связь\n"
        "3. Готовность работать 20-30 часов в неделю";
    const std::string responsibilities =
        "1. Разбирать задачи вместе с наставником\n"
        "2. Делать небольшие продуктовые улучшения\n"
        "3. Показывать результат на ревью команды";
    const std::string conditions =
        "1. Оплачиваемая стажировка\n"
        "2. Гибкий график и понятные дедлайны\n"
        "3. Возможность получить оффер по итогам программы";
    Params params = {
        title, company_id, company_name, city, format, direction, "Стажировка", "1",
        "Оплачиваемая программа", deadline, short_description, full_description,
        requirements, responsibilities, conditions, source, source, "open", "1", nowIso()
    };
    if (existing.empty()) {
        params.push_back(nowIso());
        return db.run(R"SQL(
INSERT INTO internships (
    title, company_id, company_name, city, work_format, direction, employment_type,
    is_paid, salary_info, deadline_date, short_description, full_description,
    requirements, responsibilities, conditions, source_url, application_url,
    status, is_published, created_by_type, ai_generated, needs_review, updated_at, created_at
) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 'demo', 0, 0, ?, ?)
)SQL", params);
    }
    params.push_back(existing["id"]);
    db.run(R"SQL(
UPDATE internships SET title=?, company_id=?, company_name=?, city=?, work_format=?, direction=?,
employment_type=?, is_paid=?, salary_info=?, deadline_date=?, short_description=?, full_description=?,
requirements=?, responsibilities=?, conditions=?, source_url=?, application_url=?, status=?,
is_published=?, updated_at=? WHERE id=?
)SQL", params);
    return std::stoi(existing["id"]);
}

void seedDemoReviews(Database& db) {
    db.run("DELETE FROM company_reviews WHERE user_id NOT IN (SELECT id FROM users)");

    std::vector<int> user_ids;
    for (const auto& seed : kDemoUsers) user_ids.push_back(ensureUser(db, seed));

    const std::vector<std::string> comments = {
        "Хороший онбординг: быстро объяснили продукт, дали наставника и не бросали один на один с задачами.",
        "Понравились честные ревью и понятные критерии оффера. Задачи были рабочие, не учебные.",
        "Команда спокойно отвечала на вопросы, дедлайны были адекватные. После стажировки стало понятнее, куда расти.",
        "Сильная техническая культура и много обратной связи. Иногда темп высокий, но наставник помогал держаться.",
        "Отбор был понятный, коммуникация аккуратная. Для первой стажировки это очень комфортный опыт."
    };
    const int ratings[5] = {5, 5, 4, 5, 4};
    const std::string now = nowIso();

    for (const auto& company : db.query("SELECT * FROM companies WHERE is_active=1 AND created_by_user_id IS NULL ORDER BY id")) {
        int primary_internship_id = ensureDemoInternship(db, company, 1);
        ensureDemoInternship(db, company, 2);

        for (size_t i = 0; i < user_ids.size(); ++i) {
            const std::string marker = "demo_review_seed:" + company.at("id") + ":" + std::to_string(user_ids[i]);
            Row attempt = db.one("SELECT id FROM application_attempts WHERE note=?", {marker});
            const std::string status = i % 2 == 0 ? "offer" : "rejected";
            int attempt_id = 0;
            if (attempt.empty()) {
                attempt_id = db.run(
                    "INSERT INTO application_attempts (user_id, internship_id, status, marker_enabled, stage_completed, note, applied_at, created_at, updated_at) VALUES (?, ?, ?, 1, 1, ?, '2026-04-10', ?, ?)",
                    {std::to_string(user_ids[i]), std::to_string(primary_internship_id), status, marker, now, now});
                db.run("INSERT INTO application_history (attempt_id, event_type, to_status, event_note, created_at) VALUES (?, 'create', ?, 'Демо-подача для отзывов MVP.', ?)",
                       {std::to_string(attempt_id), status, now});
            } else {
                attempt_id = std::stoi(attempt["id"]);
                db.run("UPDATE application_attempts SET internship_id=?, status=?, updated_at=? WHERE id=?",
                       {std::to_string(primary_internship_id), status, now, std::to_string(attempt_id)});
            }
            db.run(R"SQL(
INSERT INTO company_reviews (company_id, user_id, attempt_id, rating, comment, created_at, updated_at)
VALUES (?, ?, ?, ?, ?, ?, ?)
ON CONFLICT(user_id, company_id) DO UPDATE SET attempt_id=excluded.attempt_id, rating=excluded.rating, comment=excluded.comment, updated_at=excluded.updated_at
)SQL", {company.at("id"), std::to_string(user_ids[i]), std::to_string(attempt_id), std::to_string(ratings[i]), comments[i], now, now});
        }
    }
}

} // namespace

Database::Database(std::string path) : path_(std::move(path)) {
    if (sqlite3_open(path_.c_str(), &db_) != SQLITE_OK) {
        throw std::runtime_error("Cannot open database: " + std::string(sqlite3_errmsg(db_)));
    }
    exec("PRAGMA foreign_keys = ON");
}

Database::~Database() {
    if (db_) sqlite3_close(db_);
}

void Database::exec(const std::string& sql) {
    char* err = nullptr;
    if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
        std::string message = err ? err : sqlite3_errmsg(db_);
        sqlite3_free(err);
        throw std::runtime_error(message);
    }
}

std::vector<Row> Database::query(const std::string& sql, const Params& params) {
    sqlite3_stmt* stmt = prepare(sql, params);
    std::vector<Row> rows;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Row row;
        int count = sqlite3_column_count(stmt);
        for (int i = 0; i < count; ++i) {
            const char* name = sqlite3_column_name(stmt, i);
            const unsigned char* text = sqlite3_column_text(stmt, i);
            row[name ? name : ""] = text ? reinterpret_cast<const char*>(text) : "";
        }
        rows.push_back(row);
    }
    sqlite3_finalize(stmt);
    return rows;
}

Row Database::one(const std::string& sql, const Params& params) {
    auto rows = query(sql, params);
    return rows.empty() ? Row{} : rows.front();
}

int Database::run(const std::string& sql, const Params& params) {
    sqlite3_stmt* stmt = prepare(sql, params);
    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
        std::string message = sqlite3_errmsg(db_);
        sqlite3_finalize(stmt);
        throw std::runtime_error(message);
    }
    sqlite3_finalize(stmt);
    return static_cast<int>(sqlite3_last_insert_rowid(db_));
}

sqlite3_stmt* Database::prepare(const std::string& sql, const Params& params) {
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db_));
    }
    for (size_t i = 0; i < params.size(); ++i) {
        sqlite3_bind_text(stmt, static_cast<int>(i + 1), params[i].c_str(), -1, SQLITE_TRANSIENT);
    }
    return stmt;
}

void initializeSchema(Database& db) {
    db.exec(R"SQL(
CREATE TABLE IF NOT EXISTS companies (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    slug TEXT NOT NULL UNIQUE,
    logo_path TEXT,
    website_url TEXT,
    career_url TEXT,
    description TEXT,
    internship_info TEXT,
    application_notes TEXT,
    accent_color TEXT NOT NULL DEFAULT '#0e7490',
    is_active INTEGER NOT NULL DEFAULT 1,
    created_by_user_id INTEGER,
    auto_generated INTEGER NOT NULL DEFAULT 0,
    auto_generated_at TEXT,
    needs_review INTEGER NOT NULL DEFAULT 0,
    last_verified_at TEXT,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
    ,
    FOREIGN KEY(created_by_user_id) REFERENCES users(id) ON DELETE CASCADE
);
CREATE TABLE IF NOT EXISTS internships (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    title TEXT NOT NULL,
    company_id INTEGER,
    company_name TEXT NOT NULL,
    company_logo_url TEXT,
    city TEXT,
    work_format TEXT,
    direction TEXT,
    employment_type TEXT,
    is_paid INTEGER NOT NULL DEFAULT 0,
    salary_info TEXT,
    deadline_date TEXT,
    short_description TEXT,
    full_description TEXT,
    requirements TEXT,
    responsibilities TEXT,
    conditions TEXT,
    source_url TEXT,
    application_url TEXT,
    status TEXT,
    is_published INTEGER NOT NULL DEFAULT 0,
    created_by_type TEXT NOT NULL DEFAULT 'human',
    created_by_user_id INTEGER,
    ai_generated INTEGER NOT NULL DEFAULT 0,
    ai_generated_at TEXT,
    last_verified_at TEXT,
    needs_review INTEGER NOT NULL DEFAULT 0,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY(company_id) REFERENCES companies(id) ON DELETE SET NULL,
    FOREIGN KEY(created_by_user_id) REFERENCES users(id) ON DELETE SET NULL
);
CREATE TABLE IF NOT EXISTS import_jobs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    started_at TEXT NOT NULL,
    finished_at TEXT,
    file_name TEXT NOT NULL,
    total_urls INTEGER NOT NULL DEFAULT 0,
    success_count INTEGER NOT NULL DEFAULT 0,
    duplicate_count INTEGER NOT NULL DEFAULT 0,
    skipped_count INTEGER NOT NULL DEFAULT 0,
    error_count INTEGER NOT NULL DEFAULT 0,
    status TEXT NOT NULL,
    message TEXT
);
CREATE TABLE IF NOT EXISTS import_job_items (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    import_job_id INTEGER NOT NULL,
    url TEXT NOT NULL,
    status TEXT NOT NULL,
    message TEXT,
    internship_id INTEGER,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);
CREATE TABLE IF NOT EXISTS import_candidates (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    source_key TEXT NOT NULL,
    import_job_id INTEGER NOT NULL,
    source_url TEXT NOT NULL,
    title TEXT NOT NULL,
    company_name TEXT NOT NULL,
    city TEXT,
    work_format TEXT,
    direction TEXT,
    employment_type TEXT,
    is_paid INTEGER NOT NULL DEFAULT -1,
    salary_info TEXT,
    deadline_date TEXT,
    short_description TEXT,
    full_description TEXT,
    requirements TEXT,
    responsibilities TEXT,
    conditions TEXT,
    status TEXT NOT NULL DEFAULT 'found',
    published_internship_id INTEGER,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);
CREATE TABLE IF NOT EXISTS app_settings (
    key TEXT PRIMARY KEY,
    value TEXT,
    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);
CREATE TABLE IF NOT EXISTS application_attempts (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER,
    internship_id INTEGER NOT NULL,
    status TEXT NOT NULL,
    marker_enabled INTEGER NOT NULL DEFAULT 1,
    stage_completed INTEGER NOT NULL DEFAULT 1,
    status_note TEXT,
    next_step_date TEXT,
    next_step_time TEXT,
    note TEXT,
    applied_at TEXT,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY(user_id) REFERENCES users(id) ON DELETE CASCADE,
    FOREIGN KEY(internship_id) REFERENCES internships(id) ON DELETE CASCADE
);
CREATE TABLE IF NOT EXISTS application_history (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    attempt_id INTEGER NOT NULL,
    event_type TEXT NOT NULL,
    from_status TEXT,
    to_status TEXT,
    stage_deadline_date TEXT,
    stage_deadline_time TEXT,
    completion_state INTEGER,
    event_note TEXT,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY(attempt_id) REFERENCES application_attempts(id) ON DELETE CASCADE
);
CREATE TABLE IF NOT EXISTS company_reviews (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    company_id INTEGER NOT NULL,
    user_id INTEGER NOT NULL,
    attempt_id INTEGER NOT NULL,
    rating INTEGER NOT NULL CHECK(rating BETWEEN 1 AND 5),
    comment TEXT,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(user_id, company_id),
    FOREIGN KEY(company_id) REFERENCES companies(id) ON DELETE CASCADE,
    FOREIGN KEY(user_id) REFERENCES users(id) ON DELETE CASCADE,
    FOREIGN KEY(attempt_id) REFERENCES application_attempts(id) ON DELETE CASCADE
);
CREATE TABLE IF NOT EXISTS internship_directions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL UNIQUE,
    is_active INTEGER NOT NULL DEFAULT 1,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);
CREATE TABLE IF NOT EXISTS users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    email TEXT NOT NULL UNIQUE,
    password TEXT NOT NULL,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);
CREATE INDEX IF NOT EXISTS idx_companies_slug ON companies(slug);
CREATE INDEX IF NOT EXISTS idx_internships_published ON internships(is_published);
CREATE INDEX IF NOT EXISTS idx_attempts_status ON application_attempts(status);
CREATE INDEX IF NOT EXISTS idx_users_email ON users(email);
CREATE INDEX IF NOT EXISTS idx_reviews_company ON company_reviews(company_id);
CREATE INDEX IF NOT EXISTS idx_directions_name ON internship_directions(name);
)SQL");

    Row internship_user_column = db.one("SELECT COUNT(*) AS c FROM pragma_table_info('internships') WHERE name='created_by_user_id'");
    if (internship_user_column["c"] == "0") {
        db.exec("ALTER TABLE internships ADD COLUMN created_by_user_id INTEGER");
    }
    Row attempt_user_column = db.one("SELECT COUNT(*) AS c FROM pragma_table_info('application_attempts') WHERE name='user_id'");
    if (attempt_user_column["c"] == "0") {
        db.exec("ALTER TABLE application_attempts ADD COLUMN user_id INTEGER");
    }
    db.exec("UPDATE internships SET city='Удалённо' WHERE city='Remote'");
    Row company_user_column = db.one("SELECT COUNT(*) AS c FROM pragma_table_info('companies') WHERE name='created_by_user_id'");
    if (company_user_column["c"] == "0") {
        db.exec("ALTER TABLE companies ADD COLUMN created_by_user_id INTEGER");
    }
    db.exec(R"SQL(
UPDATE companies
SET created_by_user_id = (
    SELECT i.created_by_user_id
    FROM internships i
    WHERE i.company_id = companies.id AND i.created_by_user_id IS NOT NULL
    ORDER BY i.id ASC
    LIMIT 1
)
WHERE created_by_user_id IS NULL
  AND EXISTS (
      SELECT 1 FROM internships i
      WHERE i.company_id = companies.id AND i.created_by_user_id IS NOT NULL
  )
  AND NOT EXISTS (
      SELECT 1 FROM internships i
      WHERE i.company_id = companies.id
        AND i.created_by_user_id IS NULL
        AND i.created_by_type != 'demo'
  )
)SQL");
    db.exec("DELETE FROM internships WHERE created_by_type='demo' AND company_id IN (SELECT id FROM companies WHERE created_by_user_id IS NOT NULL)");
    db.exec("CREATE INDEX IF NOT EXISTS idx_internships_user ON internships(created_by_user_id)");
    db.exec("CREATE INDEX IF NOT EXISTS idx_attempts_user ON application_attempts(user_id)");
    db.exec("CREATE INDEX IF NOT EXISTS idx_companies_user ON companies(created_by_user_id)");

    const std::vector<std::string> default_directions = {
        "frontend", "backend", "mobile", "data/analytics", "qa", "design", "management", "other"
    };
    for (const auto& direction : default_directions) {
        db.run("INSERT OR IGNORE INTO internship_directions (name, is_active, created_at) VALUES (?, 1, ?)", {direction, nowIso()});
    }

    Row counts = db.one("SELECT (SELECT COUNT(*) FROM companies) AS c, (SELECT COUNT(*) FROM internships) AS i");
    if (counts["c"] != "0" || counts["i"] != "0") {
        seedDemoReviews(db);
        return;
    }

    const std::string now = nowIso();
    struct CompanySeed {
        const char* name;
        const char* slug;
        const char* site;
        const char* career;
        const char* description;
        const char* color;
    };
    const std::vector<CompanySeed> companies = {
        {"Yandex", "yandex", "https://yandex.ru", "https://yandex.ru/jobs", "Технологическая компания с продуктами для миллионов пользователей.", "#ef4444"},
        {"T-Bank", "t-bank", "https://www.tbank.ru", "https://www.tbank.ru/career", "Финтех-платформа с инженерной культурой.", "#22c55e"},
        {"VK", "vk", "https://vk.com", "https://team.vk.company", "Экосистема цифровых сервисов и продуктов.", "#3b82f6"},
        {"Sber", "sber", "https://www.sber.ru", "https://rabota.sber.ru", "Технологическая компания в сфере финансовых сервисов.", "#10b981"}
    };
    for (const auto& c : companies) {
        db.run("INSERT INTO companies (name, slug, website_url, career_url, description, internship_info, application_notes, accent_color, is_active, created_at, updated_at) VALUES (?, ?, ?, ?, ?, 'Стажировки и junior-треки для студентов.', 'Проверьте дедлайны и формат отбора.', ?, 1, ?, ?)",
               {c.name, c.slug, c.site, c.career, c.description, c.color, now, now});
    }

    std::map<std::string, Row> by_slug;
    for (const auto& row : db.query("SELECT id, slug, name, career_url FROM companies")) {
        by_slug[row.at("slug")] = row;
    }
    struct InternshipSeed {
        const char* title;
        const char* slug;
        const char* city;
        const char* format;
        const char* direction;
        const char* deadline;
    };
    const std::vector<InternshipSeed> internships = {
        {"Frontend Internship", "yandex", "Perm", "remote", "frontend", "2026-05-10"},
        {"Backend Internship", "t-bank", "Удалённо", "remote", "backend", "2026-04-25"},
        {"QA Internship", "vk", "Saint Petersburg", "hybrid", "qa", "2026-04-30"},
        {"Data Analytics Internship", "sber", "Perm", "office", "data/analytics", "2026-05-15"}
    };
    for (const auto& item : internships) {
        Row company = by_slug[item.slug];
        db.run(R"SQL(
INSERT INTO internships (
    title, company_id, company_name, city, work_format, direction, employment_type,
    is_paid, salary_info, deadline_date, short_description, full_description,
    requirements, responsibilities, conditions, source_url, application_url,
    status, is_published, created_by_type, ai_generated, needs_review, created_at, updated_at
) VALUES (?, ?, ?, ?, ?, ?, 'Part-time', 1, 'Оплачиваемая программа', ?, ?, ?, 'Базовые навыки по направлению.', 'Практические задачи в команде.', 'Гибкий график.', ?, ?, 'open', 1, 'human', 0, 0, ?, ?)
)SQL", {item.title, company["id"], company["name"], item.city, item.format, item.direction, item.deadline,
               std::string("Стажировка: ") + item.title, std::string("Описание программы ") + item.title + ".",
               company["career_url"], company["career_url"], now, now});
    }
    seedDemoReviews(db);
}

} // namespace internstart
