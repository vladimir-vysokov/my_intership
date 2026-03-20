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
            db.executescript(
                """
                CREATE TABLE IF NOT EXISTS internships (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    title TEXT NOT NULL,
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
                    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
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
                """
            )

            migrations = {
                "created_by_type": "ALTER TABLE internships ADD COLUMN created_by_type TEXT NOT NULL DEFAULT 'human'",
                "ai_generated": "ALTER TABLE internships ADD COLUMN ai_generated INTEGER NOT NULL DEFAULT 0",
                "ai_generated_at": "ALTER TABLE internships ADD COLUMN ai_generated_at TEXT",
                "last_verified_at": "ALTER TABLE internships ADD COLUMN last_verified_at TEXT",
                "needs_review": "ALTER TABLE internships ADD COLUMN needs_review INTEGER NOT NULL DEFAULT 0",
            }
            for column_name, ddl in migrations.items():
                if not column_exists(db, "internships", column_name):
                    db.execute(ddl)

            # Create indexes after schema migrations.
            db.executescript(
                """
                CREATE INDEX IF NOT EXISTS idx_internships_source_url ON internships(source_url);
                CREATE INDEX IF NOT EXISTS idx_internships_created_by_type ON internships(created_by_type);
                CREATE INDEX IF NOT EXISTS idx_internships_needs_review ON internships(needs_review);
                CREATE INDEX IF NOT EXISTS idx_import_items_job ON import_job_items(import_job_id);
                """
            )

            # Seed only if table is empty.
            count_row = db.execute("SELECT COUNT(*) AS cnt FROM internships").fetchone()
            if count_row["cnt"] == 0:
                seed_data = [
                    ("Frontend Internship", "Yandex", "Perm", "remote", "frontend", "Part-time", 1, "Стипендия по результатам отбора", "2026-05-10", "Стажировка по React и TypeScript.", "Работа с интерфейсами и API.", "JS/TS, React, Git.", "Верстка и интеграция.", "Гибкий график.", "https://yandex.ru/jobs", "https://yandex.ru/jobs", "open", 1),
                    ("Backend Internship", "T-Bank", "Remote", "remote", "backend", "Full-time", 1, "Оплачиваемая программа", "2026-04-25", "Стажировка по Python/Go.", "Разработка микросервисов.", "Python/Go, SQL.", "API и исправление багов.", "Удаленно.", "https://www.tbank.ru/career/", "https://www.tbank.ru/career/", "open", 1),
                    ("QA Internship", "VK", "Saint Petersburg", "hybrid", "qa", "Part-time", 1, "Оплата по итогам интервью", "2026-04-30", "Тестирование продуктов VK.", "Ручное тестирование и регресс.", "HTTP, SDLC.", "Тест-кейсы и баг-репорты.", "Гибрид.", "https://team.vk.company/", "https://team.vk.company/", "closing_soon", 1),
                    ("Data Analytics Internship", "Sber", "Perm", "office", "data/analytics", "Full-time", 1, "Оплачиваемая", "2026-05-15", "Аналитика данных.", "SQL и отчетность.", "SQL, статистика.", "Подготовка отчетов.", "Офис.", "https://rabota.sber.ru/", "https://rabota.sber.ru/", "open", 1),
                    ("Design Internship", "Ozon", "Remote", "remote", "design", "Part-time", 0, "", None, "Стажировка по дизайну.", "Работа в Figma.", "Портфолио.", "Дизайн экранов.", "Удаленно.", "https://job.ozon.ru/", "https://job.ozon.ru/", "open", 1),
                    ("Mobile Internship", "Avito", "Moscow", "hybrid", "mobile", "Full-time", 1, "Оплачивается", "2026-04-18", "Android/iOS стажировка.", "Разработка мобильных фич.", "Kotlin/Swift.", "Разработка и тестирование.", "Гибрид.", "https://www.avito.ru/company/career", "https://www.avito.ru/company/career", "open", 1),
                    ("Product Management Internship", "MTS", "Perm", "office", "management", "Part-time", 1, "По итогам отбора", "2026-05-01", "Стажировка в продуктовой команде.", "Анализ метрик.", "Коммуникация, Excel.", "Подготовка brief.", "Офис.", "https://job.mts.ru/", "https://job.mts.ru/", "closing_soon", 1),
                    ("Internship Program", "2GIS", "Remote", "remote", "other", "Part-time", 0, "", None, "Универсальная программа.", "Ротации по командам.", "Базовые IT-навыки.", "Работа в проектной группе.", "Удаленно.", "https://career.2gis.ru/", "https://career.2gis.ru/", "open", 1),
                ]
                for row in seed_data:
                    db.execute(
                        """
                        INSERT INTO internships (
                            title, company_name, city, work_format, direction, employment_type,
                            is_paid, salary_info, deadline_date, short_description, full_description,
                            requirements, responsibilities, conditions, source_url, application_url,
                            status, is_published, created_by_type, ai_generated, needs_review
                        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 'human', 0, 0)
                        """,
                        row,
                    )

            db.commit()
