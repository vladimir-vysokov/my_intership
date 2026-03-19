import os
import sqlite3
from contextlib import closing
from datetime import datetime
from functools import wraps
from urllib.parse import urlparse

from flask import (
    Flask,
    flash,
    g,
    redirect,
    render_template,
    request,
    session,
    url_for,
)
from werkzeug.security import check_password_hash, generate_password_hash

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
DB_PATH = os.path.join(BASE_DIR, "internships.db")

WORK_FORMATS = ["office", "remote", "hybrid"]
DIRECTIONS = [
    "frontend",
    "backend",
    "mobile",
    "data/analytics",
    "qa",
    "design",
    "management",
    "other",
]
STATUSES = ["open", "closing_soon", "closed"]

WORK_FORMAT_LABELS = {
    "office": "Офис",
    "remote": "Удаленно",
    "hybrid": "Гибрид",
}
STATUS_LABELS = {
    "open": "Набор открыт",
    "closing_soon": "Скоро закрывается",
    "closed": "Набор завершен",
}

app = Flask(__name__)
app.config["SECRET_KEY"] = os.getenv("SECRET_KEY", "dev-secret-change-me")


def get_db():
    if "db" not in g:
        g.db = sqlite3.connect(DB_PATH)
        g.db.row_factory = sqlite3.Row
    return g.db


@app.teardown_appcontext
def close_db(_error):
    db = g.pop("db", None)
    if db is not None:
        db.close()


def parse_date(value):
    if not value:
        return None
    try:
        return datetime.strptime(value, "%Y-%m-%d").date()
    except ValueError:
        return None


def is_valid_url(value):
    if not value:
        return False
    parsed = urlparse(value)
    return parsed.scheme in {"http", "https"} and bool(parsed.netloc)


def login_required(view):
    @wraps(view)
    def wrapped(*args, **kwargs):
        if "admin_id" not in session:
            return redirect(url_for("admin_login", next=request.path))
        return view(*args, **kwargs)

    return wrapped


def init_db():
    schema = """
    CREATE TABLE IF NOT EXISTS admins (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        username TEXT UNIQUE NOT NULL,
        password_hash TEXT NOT NULL,
        created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
    );

    CREATE TABLE IF NOT EXISTS internships (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        title TEXT NOT NULL,
        company_name TEXT NOT NULL,
        company_logo_url TEXT,
        city TEXT NOT NULL,
        work_format TEXT NOT NULL,
        direction TEXT NOT NULL,
        employment_type TEXT NOT NULL,
        is_paid INTEGER NOT NULL DEFAULT 0,
        salary_info TEXT,
        deadline_date TEXT,
        short_description TEXT NOT NULL,
        full_description TEXT NOT NULL,
        requirements TEXT,
        responsibilities TEXT,
        conditions TEXT,
        source_url TEXT NOT NULL,
        application_url TEXT,
        status TEXT NOT NULL,
        is_published INTEGER NOT NULL DEFAULT 1,
        created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
        updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
    );
    """

    seed_data = [
        {
            "title": "Frontend Internship",
            "company_name": "Yandex",
            "city": "Perm",
            "work_format": "remote",
            "direction": "frontend",
            "employment_type": "Part-time",
            "is_paid": 1,
            "salary_info": "Стипендия по результатам отбора",
            "deadline_date": "2026-05-10",
            "short_description": "Стажировка по React и TypeScript в продуктовой команде.",
            "full_description": "Работа с интерфейсами внутренних и внешних сервисов, участие в код-ревью и релизах.",
            "requirements": "Базовый JavaScript/TypeScript, понимание React, Git.",
            "responsibilities": "Верстка, интеграция API, поддержка компонентов.",
            "conditions": "Гибкий график 20-30 часов в неделю, ментор.",
            "source_url": "https://yandex.ru/jobs",
            "application_url": "https://yandex.ru/jobs",
            "status": "open",
            "is_published": 1,
        },
        {
            "title": "Backend Internship",
            "company_name": "T-Bank",
            "city": "Remote",
            "work_format": "remote",
            "direction": "backend",
            "employment_type": "Full-time",
            "is_paid": 1,
            "salary_info": "Оплачиваемая программа",
            "deadline_date": "2026-04-25",
            "short_description": "Стажировка по Python/Go в банковских сервисах.",
            "full_description": "Разработка микросервисов, работа с БД, нагрузочное тестирование.",
            "requirements": "Python или Go, SQL, основы алгоритмов.",
            "responsibilities": "Разработка API, исправление дефектов, документация.",
            "conditions": "Официальное оформление, удаленный формат.",
            "source_url": "https://www.tbank.ru/career/",
            "application_url": "https://www.tbank.ru/career/",
            "status": "open",
            "is_published": 1,
        },
        {
            "title": "QA Internship",
            "company_name": "VK",
            "city": "Saint Petersburg",
            "work_format": "hybrid",
            "direction": "qa",
            "employment_type": "Part-time",
            "is_paid": 1,
            "salary_info": "Оплата по итогам интервью",
            "deadline_date": "2026-04-30",
            "short_description": "Тестирование веб и мобильных продуктов VK.",
            "full_description": "Ручное тестирование, подготовка тест-кейсов, участие в регрессе.",
            "requirements": "Понимание SDLC, базовые знания HTTP, внимательность.",
            "responsibilities": "Тест-планы, баг-репорты, проверка фиксов.",
            "conditions": "Гибрид, обучение с наставником.",
            "source_url": "https://team.vk.company/",
            "application_url": "https://team.vk.company/",
            "status": "closing_soon",
            "is_published": 1,
        },
        {
            "title": "Data Analytics Internship",
            "company_name": "Sber",
            "city": "Perm",
            "work_format": "office",
            "direction": "data/analytics",
            "employment_type": "Full-time",
            "is_paid": 1,
            "salary_info": "Оплачиваемая стажировка",
            "deadline_date": "2026-05-15",
            "short_description": "Аналитика данных и построение отчетности.",
            "full_description": "Поддержка витрин данных, SQL-запросы, визуализация показателей.",
            "requirements": "SQL, Excel/BI, базовая статистика.",
            "responsibilities": "Подготовка отчетов и аналитических срезов.",
            "conditions": "Офис в Перми, полная занятость.",
            "source_url": "https://rabota.sber.ru/",
            "application_url": "https://rabota.sber.ru/",
            "status": "open",
            "is_published": 1,
        },
        {
            "title": "Design Internship",
            "company_name": "Ozon",
            "city": "Remote",
            "work_format": "remote",
            "direction": "design",
            "employment_type": "Part-time",
            "is_paid": 0,
            "salary_info": "",
            "deadline_date": None,
            "short_description": "Стажировка по продуктового дизайну в e-commerce.",
            "full_description": "Работа в Figma, подготовка UI-компонентов, дизайн-гипотезы.",
            "requirements": "Портфолио, Figma, понимание UX-процессов.",
            "responsibilities": "Дизайн экранов и пользовательских сценариев.",
            "conditions": "Удаленно, гибкий график.",
            "source_url": "https://job.ozon.ru/",
            "application_url": "https://job.ozon.ru/",
            "status": "open",
            "is_published": 1,
        },
        {
            "title": "Mobile Internship",
            "company_name": "Avito",
            "city": "Moscow",
            "work_format": "hybrid",
            "direction": "mobile",
            "employment_type": "Full-time",
            "is_paid": 1,
            "salary_info": "Оплачивается",
            "deadline_date": "2026-04-18",
            "short_description": "Стажировка по Android/iOS разработке.",
            "full_description": "Разработка мобильных фич, интеграция SDK, профилирование.",
            "requirements": "Kotlin/Swift, ООП, основы сетевого взаимодействия.",
            "responsibilities": "Разработка экранов, поддержка кода, тестирование.",
            "conditions": "Гибридный формат, офис + удаленка.",
            "source_url": "https://www.avito.ru/company/career",
            "application_url": "https://www.avito.ru/company/career",
            "status": "open",
            "is_published": 1,
        },
        {
            "title": "Product Management Internship",
            "company_name": "MTS",
            "city": "Perm",
            "work_format": "office",
            "direction": "management",
            "employment_type": "Part-time",
            "is_paid": 1,
            "salary_info": "По итогам отбора",
            "deadline_date": "2026-05-01",
            "short_description": "Стажировка в продуктовой команде телеком-сервиса.",
            "full_description": "Анализ метрик, постановка задач, приоритизация бэклога.",
            "requirements": "Системное мышление, Excel, коммуникация.",
            "responsibilities": "Подготовка product brief, участие в discovery.",
            "conditions": "Офис в Перми, частичная занятость.",
            "source_url": "https://job.mts.ru/",
            "application_url": "https://job.mts.ru/",
            "status": "closing_soon",
            "is_published": 1,
        },
        {
            "title": "Internship Program",
            "company_name": "2GIS",
            "city": "Remote",
            "work_format": "remote",
            "direction": "other",
            "employment_type": "Part-time",
            "is_paid": 0,
            "salary_info": "",
            "deadline_date": None,
            "short_description": "Универсальная программа для начинающих специалистов.",
            "full_description": "Ротации по командам, базовое обучение и проектная работа.",
            "requirements": "Мотивация, самообучение, базовые IT-навыки.",
            "responsibilities": "Выполнение задач в проектной группе.",
            "conditions": "Удаленный формат, набор открыт круглый год.",
            "source_url": "https://career.2gis.ru/",
            "application_url": "https://career.2gis.ru/",
            "status": "open",
            "is_published": 1,
        },
    ]

    with closing(sqlite3.connect(DB_PATH)) as db:
        db.row_factory = sqlite3.Row
        db.executescript(schema)

        admin_user = os.getenv("ADMIN_USERNAME", "admin")
        admin_password = os.getenv("ADMIN_PASSWORD", "admin123")
        existing_admin = db.execute(
            "SELECT id FROM admins WHERE username = ?", (admin_user,)
        ).fetchone()
        if not existing_admin:
            db.execute(
                "INSERT INTO admins (username, password_hash) VALUES (?, ?)",
                (admin_user, generate_password_hash(admin_password)),
            )

        existing_count = db.execute("SELECT COUNT(*) AS cnt FROM internships").fetchone()["cnt"]
        if existing_count == 0:
            for item in seed_data:
                db.execute(
                    """
                    INSERT INTO internships (
                        title, company_name, company_logo_url, city, work_format, direction,
                        employment_type, is_paid, salary_info, deadline_date,
                        short_description, full_description, requirements, responsibilities,
                        conditions, source_url, application_url, status, is_published
                    ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                    """,
                    (
                        item["title"],
                        item["company_name"],
                        item.get("company_logo_url", ""),
                        item["city"],
                        item["work_format"],
                        item["direction"],
                        item["employment_type"],
                        item["is_paid"],
                        item.get("salary_info", ""),
                        item.get("deadline_date"),
                        item["short_description"],
                        item["full_description"],
                        item.get("requirements", ""),
                        item.get("responsibilities", ""),
                        item.get("conditions", ""),
                        item["source_url"],
                        item.get("application_url", ""),
                        item["status"],
                        item["is_published"],
                    ),
                )

        db.commit()


def collect_form_data():
    return {
        "title": request.form.get("title", "").strip(),
        "company_name": request.form.get("company_name", "").strip(),
        "company_logo_url": request.form.get("company_logo_url", "").strip(),
        "city": request.form.get("city", "").strip(),
        "work_format": request.form.get("work_format", "").strip(),
        "direction": request.form.get("direction", "").strip(),
        "employment_type": request.form.get("employment_type", "").strip(),
        "is_paid": 1 if request.form.get("is_paid") == "1" else 0,
        "salary_info": request.form.get("salary_info", "").strip(),
        "deadline_date": request.form.get("deadline_date", "").strip() or None,
        "short_description": request.form.get("short_description", "").strip(),
        "full_description": request.form.get("full_description", "").strip(),
        "requirements": request.form.get("requirements", "").strip(),
        "responsibilities": request.form.get("responsibilities", "").strip(),
        "conditions": request.form.get("conditions", "").strip(),
        "source_url": request.form.get("source_url", "").strip(),
        "application_url": request.form.get("application_url", "").strip(),
        "status": request.form.get("status", "").strip(),
        "is_published": 1 if request.form.get("is_published") == "1" else 0,
    }


def validate_internship_data(data):
    errors = []

    required_fields = [
        ("title", "Укажите название стажировки."),
        ("company_name", "Укажите компанию."),
        ("city", "Укажите город."),
        ("employment_type", "Укажите тип занятости."),
        ("short_description", "Укажите краткое описание."),
        ("full_description", "Укажите полное описание."),
        ("source_url", "Укажите ссылку на источник."),
    ]

    for key, message in required_fields:
        if not data[key]:
            errors.append(message)

    if data["work_format"] not in WORK_FORMATS:
        errors.append("Выберите корректный формат работы.")
    if data["direction"] not in DIRECTIONS:
        errors.append("Выберите корректное направление.")
    if data["status"] not in STATUSES:
        errors.append("Выберите корректный статус.")

    if data["deadline_date"] and not parse_date(data["deadline_date"]):
        errors.append("Некорректная дата дедлайна. Используйте формат YYYY-MM-DD.")

    if not is_valid_url(data["source_url"]):
        errors.append("Ссылка на источник должна быть валидным URL (http/https).")

    if data["application_url"] and not is_valid_url(data["application_url"]):
        errors.append("Ссылка для отклика должна быть валидным URL (http/https).")

    if data["company_logo_url"] and not is_valid_url(data["company_logo_url"]):
        errors.append("Ссылка на логотип должна быть валидным URL (http/https).")

    return errors


init_db()


@app.context_processor
def inject_helpers():
    return {
        "work_format_labels": WORK_FORMAT_LABELS,
        "status_labels": STATUS_LABELS,
        "directions": DIRECTIONS,
        "work_formats": WORK_FORMATS,
        "statuses": STATUSES,
        "is_admin": "admin_id" in session,
    }


@app.route("/")
def home():
    db = get_db()
    latest = db.execute(
        """
        SELECT * FROM internships
        WHERE is_published = 1
        ORDER BY datetime(created_at) DESC
        LIMIT 6
        """
    ).fetchall()
    return render_template("home.html", latest=latest)


@app.route("/internships")
def internships_catalog():
    db = get_db()

    q = request.args.get("q", "").strip()
    city = request.args.get("city", "").strip()
    work_format = request.args.get("work_format", "").strip()
    direction = request.args.get("direction", "").strip()
    paid = request.args.get("paid", "").strip()
    deadline_filter = request.args.get("deadline", "").strip()
    sort = request.args.get("sort", "newest").strip()

    where_clauses = ["is_published = 1"]
    params = []

    if q:
        where_clauses.append(
            "(title LIKE ? OR company_name LIKE ? OR direction LIKE ? OR city LIKE ? OR short_description LIKE ? OR full_description LIKE ?)"
        )
        search_term = f"%{q}%"
        params.extend([search_term] * 6)

    if city:
        where_clauses.append("city = ?")
        params.append(city)

    if work_format in WORK_FORMATS:
        where_clauses.append("work_format = ?")
        params.append(work_format)

    if direction in DIRECTIONS:
        where_clauses.append("direction = ?")
        params.append(direction)

    if paid in {"1", "0"}:
        where_clauses.append("is_paid = ?")
        params.append(int(paid))

    if deadline_filter == "has_deadline":
        where_clauses.append("deadline_date IS NOT NULL")
    elif deadline_filter == "open_enrollment":
        where_clauses.append("status = 'open'")
    elif deadline_filter == "unknown":
        where_clauses.append("deadline_date IS NULL")

    order_by = "datetime(created_at) DESC"
    if sort == "deadline":
        order_by = "CASE WHEN deadline_date IS NULL THEN 1 ELSE 0 END, date(deadline_date) ASC"
    elif sort == "company":
        order_by = "company_name COLLATE NOCASE ASC"

    city_rows = db.execute(
        "SELECT DISTINCT city FROM internships WHERE is_published = 1 ORDER BY city ASC"
    ).fetchall()
    cities = [row["city"] for row in city_rows]

    query = f"""
        SELECT * FROM internships
        WHERE {' AND '.join(where_clauses)}
        ORDER BY {order_by}
    """
    internships = db.execute(query, params).fetchall()

    return render_template(
        "catalog.html",
        internships=internships,
        cities=cities,
        filters={
            "q": q,
            "city": city,
            "work_format": work_format,
            "direction": direction,
            "paid": paid,
            "deadline": deadline_filter,
            "sort": sort,
        },
    )


@app.route("/internships/<int:internship_id>")
def internship_detail(internship_id):
    db = get_db()
    internship = db.execute(
        "SELECT * FROM internships WHERE id = ? AND is_published = 1", (internship_id,)
    ).fetchone()

    if not internship:
        return render_template("404.html"), 404

    return render_template("detail.html", internship=internship)


@app.route("/admin/login", methods=["GET", "POST"])
def admin_login():
    if request.method == "POST":
        username = request.form.get("username", "").strip()
        password = request.form.get("password", "")

        db = get_db()
        admin = db.execute("SELECT * FROM admins WHERE username = ?", (username,)).fetchone()

        if admin and check_password_hash(admin["password_hash"], password):
            session["admin_id"] = admin["id"]
            session["admin_username"] = admin["username"]
            next_url = request.args.get("next") or url_for("admin_dashboard")
            return redirect(next_url)

        flash("Неверный логин или пароль.", "error")

    return render_template("admin_login.html")


@app.route("/admin/logout")
@login_required
def admin_logout():
    session.clear()
    return redirect(url_for("admin_login"))


@app.route("/admin")
@login_required
def admin_dashboard():
    db = get_db()
    internships = db.execute(
        """
        SELECT * FROM internships
        ORDER BY datetime(created_at) DESC
        """
    ).fetchall()
    return render_template("admin_list.html", internships=internships)


@app.route("/admin/internships/new", methods=["GET", "POST"])
@login_required
def admin_create_internship():
    form_data = {
        "title": "",
        "company_name": "",
        "company_logo_url": "",
        "city": "",
        "work_format": "office",
        "direction": "frontend",
        "employment_type": "",
        "is_paid": 1,
        "salary_info": "",
        "deadline_date": "",
        "short_description": "",
        "full_description": "",
        "requirements": "",
        "responsibilities": "",
        "conditions": "",
        "source_url": "",
        "application_url": "",
        "status": "open",
        "is_published": 1,
    }

    if request.method == "POST":
        form_data = collect_form_data()
        errors = validate_internship_data(form_data)

        if not errors:
            db = get_db()
            db.execute(
                """
                INSERT INTO internships (
                    title, company_name, company_logo_url, city, work_format, direction,
                    employment_type, is_paid, salary_info, deadline_date, short_description,
                    full_description, requirements, responsibilities, conditions, source_url,
                    application_url, status, is_published, updated_at
                ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, CURRENT_TIMESTAMP)
                """,
                (
                    form_data["title"],
                    form_data["company_name"],
                    form_data["company_logo_url"],
                    form_data["city"],
                    form_data["work_format"],
                    form_data["direction"],
                    form_data["employment_type"],
                    form_data["is_paid"],
                    form_data["salary_info"],
                    form_data["deadline_date"],
                    form_data["short_description"],
                    form_data["full_description"],
                    form_data["requirements"],
                    form_data["responsibilities"],
                    form_data["conditions"],
                    form_data["source_url"],
                    form_data["application_url"],
                    form_data["status"],
                    form_data["is_published"],
                ),
            )
            db.commit()
            flash("Стажировка добавлена.", "success")
            return redirect(url_for("admin_dashboard"))

        for err in errors:
            flash(err, "error")

    return render_template(
        "admin_form.html",
        form_data=form_data,
        form_action=url_for("admin_create_internship"),
        page_title="Добавить стажировку",
    )


@app.route("/admin/internships/<int:internship_id>/edit", methods=["GET", "POST"])
@login_required
def admin_edit_internship(internship_id):
    db = get_db()
    internship = db.execute(
        "SELECT * FROM internships WHERE id = ?", (internship_id,)
    ).fetchone()

    if not internship:
        flash("Стажировка не найдена.", "error")
        return redirect(url_for("admin_dashboard"))

    if request.method == "POST":
        form_data = collect_form_data()
        errors = validate_internship_data(form_data)

        if not errors:
            db.execute(
                """
                UPDATE internships
                SET title = ?, company_name = ?, company_logo_url = ?, city = ?,
                    work_format = ?, direction = ?, employment_type = ?, is_paid = ?,
                    salary_info = ?, deadline_date = ?, short_description = ?,
                    full_description = ?, requirements = ?, responsibilities = ?,
                    conditions = ?, source_url = ?, application_url = ?, status = ?,
                    is_published = ?, updated_at = CURRENT_TIMESTAMP
                WHERE id = ?
                """,
                (
                    form_data["title"],
                    form_data["company_name"],
                    form_data["company_logo_url"],
                    form_data["city"],
                    form_data["work_format"],
                    form_data["direction"],
                    form_data["employment_type"],
                    form_data["is_paid"],
                    form_data["salary_info"],
                    form_data["deadline_date"],
                    form_data["short_description"],
                    form_data["full_description"],
                    form_data["requirements"],
                    form_data["responsibilities"],
                    form_data["conditions"],
                    form_data["source_url"],
                    form_data["application_url"],
                    form_data["status"],
                    form_data["is_published"],
                    internship_id,
                ),
            )
            db.commit()
            flash("Стажировка обновлена.", "success")
            return redirect(url_for("admin_dashboard"))

        for err in errors:
            flash(err, "error")
    else:
        form_data = dict(internship)

    return render_template(
        "admin_form.html",
        form_data=form_data,
        form_action=url_for("admin_edit_internship", internship_id=internship_id),
        page_title=f"Редактировать стажировку #{internship_id}",
    )


@app.route("/admin/internships/<int:internship_id>/delete", methods=["POST"])
@login_required
def admin_delete_internship(internship_id):
    db = get_db()
    db.execute("DELETE FROM internships WHERE id = ?", (internship_id,))
    db.commit()
    flash("Стажировка удалена.", "success")
    return redirect(url_for("admin_dashboard"))


@app.errorhandler(404)
def not_found(_error):
    return render_template("404.html"), 404


if __name__ == "__main__":
    app.run(debug=True)
