import os
import sqlite3
from contextlib import closing
from datetime import datetime

from flask import current_app, g

from .config import sqlite_path_from_url


def now_iso() -> str:
    return datetime.utcnow().replace(microsecond=0).isoformat() + "Z"


def get_db_path() -> str:
    settings = current_app.config["SETTINGS"]
    base_dir = current_app.config["BASE_DIR"]
    return sqlite_path_from_url(settings.database_url, base_dir)


def get_db():
    if "db" not in g:
        db_path = get_db_path()
        os.makedirs(os.path.dirname(db_path), exist_ok=True)
        g.db = sqlite3.connect(db_path)
        g.db.row_factory = sqlite3.Row
        g.db.execute("PRAGMA foreign_keys = ON")
    return g.db


def close_db(_error=None):
    db = g.pop("db", None)
    if db is not None:
        db.close()


def column_exists(db, table: str, column: str) -> bool:
    rows = db.execute(f"PRAGMA table_info({table})").fetchall()
    return any(r["name"] == column for r in rows)


def init_db(app):
    with app.app_context():
        db_path = get_db_path()
        os.makedirs(os.path.dirname(db_path), exist_ok=True)

        with closing(sqlite3.connect(db_path)) as db:
            db.row_factory = sqlite3.Row
            db.execute("PRAGMA foreign_keys = ON")
            db.executescript(
                """
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
                    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
                    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
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
                    ai_generated INTEGER NOT NULL DEFAULT 0,
                    ai_generated_at TEXT,
                    last_verified_at TEXT,
                    needs_review INTEGER NOT NULL DEFAULT 0,
                    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
                    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
                    FOREIGN KEY(company_id) REFERENCES companies(id) ON DELETE SET NULL
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
                    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
                    FOREIGN KEY(import_job_id) REFERENCES import_jobs(id)
                );

                CREATE TABLE IF NOT EXISTS applications (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    internship_id INTEGER NOT NULL UNIQUE,
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
                    FOREIGN KEY(internship_id) REFERENCES internships(id) ON DELETE CASCADE
                );

                CREATE TABLE IF NOT EXISTS application_attempts (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
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
                    FOREIGN KEY(internship_id) REFERENCES internships(id) ON DELETE CASCADE
                );

                CREATE TABLE IF NOT EXISTS application_history (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    attempt_id INTEGER NOT NULL,
                    event_type TEXT NOT NULL,
                    from_status TEXT,
                    to_status TEXT,
                    event_note TEXT,
                    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
                    FOREIGN KEY(attempt_id) REFERENCES application_attempts(id) ON DELETE CASCADE
                );
                """
            )

            migrations = {
                "created_by_type": "ALTER TABLE internships ADD COLUMN created_by_type TEXT NOT NULL DEFAULT 'human'",
                "ai_generated": "ALTER TABLE internships ADD COLUMN ai_generated INTEGER NOT NULL DEFAULT 0",
                "ai_generated_at": "ALTER TABLE internships ADD COLUMN ai_generated_at TEXT",
                "last_verified_at": "ALTER TABLE internships ADD COLUMN last_verified_at TEXT",
                "needs_review": "ALTER TABLE internships ADD COLUMN needs_review INTEGER NOT NULL DEFAULT 0",
                "company_id": "ALTER TABLE internships ADD COLUMN company_id INTEGER",
            }
            for column_name, ddl in migrations.items():
                if not column_exists(db, "internships", column_name):
                    db.execute(ddl)

            application_migrations = {
                "marker_enabled": "ALTER TABLE applications ADD COLUMN marker_enabled INTEGER NOT NULL DEFAULT 1",
                "stage_completed": "ALTER TABLE applications ADD COLUMN stage_completed INTEGER NOT NULL DEFAULT 0",
                "next_step_time": "ALTER TABLE applications ADD COLUMN next_step_time TEXT",
            }
            for column_name, ddl in application_migrations.items():
                if not column_exists(db, "applications", column_name):
                    db.execute(ddl)

            # Create indexes after schema migrations.
            db.executescript(
                """
                CREATE UNIQUE INDEX IF NOT EXISTS idx_companies_slug ON companies(slug);
                CREATE INDEX IF NOT EXISTS idx_companies_active ON companies(is_active);
                CREATE INDEX IF NOT EXISTS idx_internships_source_url ON internships(source_url);
                CREATE INDEX IF NOT EXISTS idx_internships_created_by_type ON internships(created_by_type);
                CREATE INDEX IF NOT EXISTS idx_internships_needs_review ON internships(needs_review);
                CREATE INDEX IF NOT EXISTS idx_internships_company_id ON internships(company_id);
                CREATE INDEX IF NOT EXISTS idx_import_items_job ON import_job_items(import_job_id);
                CREATE INDEX IF NOT EXISTS idx_applications_status ON applications(status);
                CREATE INDEX IF NOT EXISTS idx_applications_next_step ON applications(next_step_date);
                CREATE INDEX IF NOT EXISTS idx_attempts_status ON application_attempts(status);
                CREATE INDEX IF NOT EXISTS idx_attempts_internship ON application_attempts(internship_id);
                CREATE INDEX IF NOT EXISTS idx_history_attempt ON application_history(attempt_id);
                """
            )

            attempts_count = db.execute(
                "SELECT COUNT(*) AS cnt FROM application_attempts"
            ).fetchone()["cnt"]
            legacy_apps_exists = db.execute(
                "SELECT name FROM sqlite_master WHERE type='table' AND name='applications'"
            ).fetchone()
            if attempts_count == 0 and legacy_apps_exists:
                legacy_rows = db.execute(
                    """
                    SELECT
                        id,
                        internship_id,
                        status,
                        marker_enabled,
                        stage_completed,
                        status_note,
                        next_step_date,
                        next_step_time,
                        note,
                        applied_at,
                        created_at,
                        updated_at
                    FROM applications
                    ORDER BY id ASC
                    """
                ).fetchall()
                for row in legacy_rows:
                    cur = db.execute(
                        """
                        INSERT INTO application_attempts (
                            internship_id, status, marker_enabled, stage_completed,
                            status_note, next_step_date, next_step_time, note, applied_at,
                            created_at, updated_at
                        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                        """,
                        (
                            row["internship_id"],
                            row["status"],
                            row["marker_enabled"] if "marker_enabled" in row.keys() else 1,
                            row["stage_completed"] if "stage_completed" in row.keys() else 1,
                            row["status_note"],
                            row["next_step_date"],
                            row["next_step_time"] if "next_step_time" in row.keys() else None,
                            row["note"],
                            row["applied_at"],
                            row["created_at"] or now_iso(),
                            row["updated_at"] or now_iso(),
                        ),
                    )
                    new_attempt_id = cur.lastrowid
                    db.execute(
                        """
                        INSERT INTO application_history (
                            attempt_id, event_type, to_status, event_note, created_at
                        ) VALUES (?, 'create', ?, NULL, ?)
                        """,
                        (
                            new_attempt_id,
                            row["status"],
                            row["created_at"] or now_iso(),
                        ),
                    )

            company_count = db.execute("SELECT COUNT(*) AS cnt FROM companies").fetchone()["cnt"]
            internship_count = db.execute("SELECT COUNT(*) AS cnt FROM internships").fetchone()["cnt"]

            if company_count == 0 and internship_count == 0:
                company_seed_data = [
                    ("Yandex", "yandex", "https://yandex.ru", "https://yandex.ru/jobs", "Технологическая компания с продуктами для миллионов пользователей.", "Есть треки для frontend, backend, data и ML. Часть программ проходит удаленно.", "Обратите внимание на сроки тестовых заданий и дедлайны отбора.", "#ef4444", 1),
                    ("T-Bank", "t-bank", "https://www.tbank.ru", "https://www.tbank.ru/career", "Финтех-платформа с инженерной культурой и большим количеством backend-задач.", "Стажировки обычно идут с ментором и практическими задачами в прод-командах.", "Подготовьте резюме и краткий рассказ о pet-проектах.", "#22c55e", 1),
                    ("VK", "vk", "https://vk.com", "https://team.vk.company", "Экосистема цифровых сервисов и продуктов.", "Есть стажировки в разработке, QA, аналитике и дизайне.", "Часть направлений требует профильное тестирование.", "#3b82f6", 1),
                    ("Sber", "sber", "https://www.sber.ru", "https://rabota.sber.ru", "Крупная технологическая компания в сфере финансовых сервисов.", "Программы стажировок проходят в офисе и гибридном формате.", "Проверьте соответствие формату занятости и графику обучения.", "#10b981", 1),
                    ("Ozon", "ozon", "https://www.ozon.ru", "https://job.ozon.ru", "E-commerce платформа с сильными продуктовой и инженерной командами.", "Стажерские треки охватывают разработку, аналитику и продукт.", "Часто просят портфолио или примеры задач.", "#f97316", 1),
                    ("Avito", "avito", "https://www.avito.ru", "https://www.avito.ru/company/career", "Крупная онлайн-платформа объявлений и сервисов.", "Есть наборы на мобильную и backend-разработку, а также аналитические роли.", "Полезно приложить ссылки на GitHub и учебные проекты.", "#06b6d4", 1),
                    ("MTS", "mts", "https://www.mts.ru", "https://job.mts.ru", "Телеком и цифровые сервисы с широким стеком ИТ-продуктов.", "Стажировки открываются по направлениям разработки, QA и менеджмента.", "Уточняйте формат работы по конкретной программе.", "#e11d48", 1),
                    ("2GIS", "2gis", "https://2gis.ru", "https://career.2gis.ru", "Геосервисы и продуктовые команды с сильной инженерной школой.", "Есть программы с ротацией между командами и практическими задачами.", "Важно подробно заполнить блок с навыками в анкете.", "#8b5cf6", 1),
                ]
                for row in company_seed_data:
                    db.execute(
                        """
                        INSERT INTO companies (
                            name, slug, website_url, career_url, description,
                            internship_info, application_notes, accent_color, is_active,
                            created_at, updated_at
                        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                        """,
                        (*row, now_iso(), now_iso()),
                    )

                company_ids = {
                    row["slug"]: row["id"]
                    for row in db.execute("SELECT id, slug FROM companies").fetchall()
                }

                internship_seed_data = [
                    ("Frontend Internship", "yandex", "Perm", "remote", "frontend", "Part-time", 1, "Стипендия по результатам отбора", "2026-05-10", "Стажировка по React и TypeScript.", "Работа с интерфейсами и API.", "JS/TS, React, Git.", "Верстка и интеграция.", "Гибкий график.", "https://yandex.ru/jobs", "https://yandex.ru/jobs", "open", 1),
                    ("Backend Internship", "t-bank", "Remote", "remote", "backend", "Full-time", 1, "Оплачиваемая программа", "2026-04-25", "Стажировка по Python/Go.", "Разработка микросервисов.", "Python/Go, SQL.", "API и исправление багов.", "Удаленно.", "https://www.tbank.ru/career/", "https://www.tbank.ru/career/", "open", 1),
                    ("QA Internship", "vk", "Saint Petersburg", "hybrid", "qa", "Part-time", 1, "Оплата по итогам интервью", "2026-04-30", "Тестирование продуктов VK.", "Ручное тестирование и регресс.", "HTTP, SDLC.", "Тест-кейсы и баг-репорты.", "Гибрид.", "https://team.vk.company/", "https://team.vk.company/", "closing_soon", 1),
                    ("Data Analytics Internship", "sber", "Perm", "office", "data/analytics", "Full-time", 1, "Оплачиваемая", "2026-05-15", "Аналитика данных.", "SQL и отчетность.", "SQL, статистика.", "Подготовка отчетов.", "Офис.", "https://rabota.sber.ru/", "https://rabota.sber.ru/", "open", 1),
                    ("Design Internship", "ozon", "Remote", "remote", "design", "Part-time", 0, "", None, "Стажировка по дизайну.", "Работа в Figma.", "Портфолио.", "Дизайн экранов.", "Удаленно.", "https://job.ozon.ru/", "https://job.ozon.ru/", "open", 1),
                    ("Mobile Internship", "avito", "Moscow", "hybrid", "mobile", "Full-time", 1, "Оплачивается", "2026-04-18", "Android/iOS стажировка.", "Разработка мобильных фич.", "Kotlin/Swift.", "Разработка и тестирование.", "Гибрид.", "https://www.avito.ru/company/career", "https://www.avito.ru/company/career", "open", 1),
                    ("Product Management Internship", "mts", "Perm", "office", "management", "Part-time", 1, "По итогам отбора", "2026-05-01", "Стажировка в продуктовой команде.", "Анализ метрик.", "Коммуникация, Excel.", "Подготовка brief.", "Офис.", "https://job.mts.ru/", "https://job.mts.ru/", "closing_soon", 1),
                    ("Internship Program", "2gis", "Remote", "remote", "other", "Part-time", 0, "", None, "Универсальная программа.", "Ротации по командам.", "Базовые IT-навыки.", "Работа в проектной группе.", "Удаленно.", "https://career.2gis.ru/", "https://career.2gis.ru/", "open", 1),
                ]
                for row in internship_seed_data:
                    company = db.execute(
                        "SELECT name, logo_path FROM companies WHERE id = ?",
                        (company_ids[row[1]],),
                    ).fetchone()
                    db.execute(
                        """
                        INSERT INTO internships (
                            title, company_id, company_name, company_logo_url, city, work_format, direction,
                            employment_type, is_paid, salary_info, deadline_date, short_description, full_description,
                            requirements, responsibilities, conditions, source_url, application_url,
                            status, is_published, created_by_type, ai_generated, needs_review, created_at, updated_at
                        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 'human', 0, 0, ?, ?)
                        """,
                        (
                            row[0],
                            company_ids[row[1]],
                            company["name"],
                            company["logo_path"],
                            row[2],
                            row[3],
                            row[4],
                            row[5],
                            row[6],
                            row[7],
                            row[8],
                            row[9],
                            row[10],
                            row[11],
                            row[12],
                            row[13],
                            row[14],
                            row[15],
                            row[16],
                            row[17],
                            now_iso(),
                            now_iso(),
                        ),
                    )

            db.commit()
