import os
import re
import unicodedata
import uuid

from flask import Blueprint, current_app, flash, redirect, render_template, request, session, url_for
from werkzeug.utils import secure_filename

from app.auth import admin_config_ready, admin_login_required, verify_admin_credentials
from app.constants import DIRECTIONS, STATUSES, WORK_FORMATS
from app.db import get_db, now_iso
from app.services.import_service import run_import

admin_bp = Blueprint("admin", __name__, url_prefix="/admin")

HEX_COLOR_RE = re.compile(r"^#[0-9a-fA-F]{6}$")
MAX_SHORT_DESCRIPTION_LEN = 220


def parse_date(value: str | None):
    return value.strip() if value and value.strip() else None


def parse_int(value: str | None):
    if value is None:
        return None
    value = value.strip()
    if not value:
        return None
    try:
        return int(value)
    except ValueError:
        return None


def normalize_slug(value: str) -> str:
    normalized = unicodedata.normalize("NFKD", value).encode("ascii", "ignore").decode("ascii")
    normalized = normalized.lower().strip()
    normalized = re.sub(r"[^a-z0-9]+", "-", normalized).strip("-")
    return normalized


def normalize_accent_color(value: str | None) -> str:
    candidate = (value or "").strip()
    if not candidate:
        return "#0e7490"
    if not candidate.startswith("#"):
        candidate = f"#{candidate}"
    candidate = candidate.lower()
    if HEX_COLOR_RE.match(candidate):
        return candidate
    return "#0e7490"


def safe_admin_next_url(value: str | None) -> str | None:
    if not value:
        return None
    value = value.strip()
    if value.startswith("/admin/"):
        return value
    return None


def save_company_logo(file_storage, company_slug: str):
    if not file_storage or not file_storage.filename:
        return None

    safe_name = secure_filename(file_storage.filename)
    extension = os.path.splitext(safe_name)[1].lower()
    if extension != ".png":
        raise ValueError("Логотип должен быть в формате PNG.")

    uploads_dir = os.path.join(current_app.static_folder, "uploads", "company_logos")
    os.makedirs(uploads_dir, exist_ok=True)

    slug_part = normalize_slug(company_slug) or "company"
    filename = f"{slug_part}-{uuid.uuid4().hex[:12]}.png"
    full_path = os.path.join(uploads_dir, filename)
    file_storage.save(full_path)

    return f"uploads/company_logos/{filename}"


def get_company_for_internship(db, company_id: int | None):
    if not company_id:
        return None
    return db.execute(
        "SELECT id, name, logo_path, is_active FROM companies WHERE id = ?",
        (company_id,),
    ).fetchone()


def fetch_companies_for_select(db, selected_company_id: int | None = None):
    if selected_company_id:
        return db.execute(
            """
            SELECT id, name, is_active
            FROM companies
            WHERE is_active = 1 OR id = ?
            ORDER BY name COLLATE NOCASE ASC
            """,
            (selected_company_id,),
        ).fetchall()
    return db.execute(
        """
        SELECT id, name, is_active
        FROM companies
        WHERE is_active = 1
        ORDER BY name COLLATE NOCASE ASC
        """
    ).fetchall()


def collect_internship_form_data():
    paid_raw = request.form.get("is_paid", "").strip()
    is_paid = -1
    if paid_raw == "1":
        is_paid = 1
    elif paid_raw == "0":
        is_paid = 0

    return {
        "title": request.form.get("title", "").strip(),
        "company_id": parse_int(request.form.get("company_id")),
        "city": request.form.get("city", "").strip(),
        "work_format": request.form.get("work_format", "").strip(),
        "direction": request.form.get("direction", "").strip(),
        "employment_type": request.form.get("employment_type", "").strip(),
        "is_paid": is_paid,
        "salary_info": request.form.get("salary_info", "").strip() or None,
        "deadline_date": parse_date(request.form.get("deadline_date")),
        "short_description": request.form.get("short_description", "").strip(),
        "full_description": request.form.get("full_description", "").strip(),
        "requirements": request.form.get("requirements", "").strip() or None,
        "responsibilities": request.form.get("responsibilities", "").strip() or None,
        "conditions": request.form.get("conditions", "").strip() or None,
        "source_url": request.form.get("source_url", "").strip(),
        "application_url": request.form.get("application_url", "").strip() or None,
        "status": request.form.get("status", "").strip(),
        "is_published": 1 if request.form.get("is_published") == "1" else 0,
    }


def validate_internship_form(db, data):
    errors = []
    required = ["title", "city", "short_description", "full_description", "source_url"]
    for field in required:
        if not data[field]:
            errors.append(f"Поле '{field}' обязательно.")

    if not data["company_id"]:
        errors.append("Выберите компанию.")
    else:
        company = get_company_for_internship(db, data["company_id"])
        if not company:
            errors.append("Выбранная компания не найдена.")

    if data["work_format"] not in WORK_FORMATS:
        errors.append("Некорректный формат работы.")
    if data["direction"] not in DIRECTIONS:
        errors.append("Некорректное направление.")
    if data["status"] not in STATUSES:
        errors.append("Некорректный статус.")
    return errors


def apply_short_description_publish_guard(data):
    short_description = (data.get("short_description") or "").strip()
    if len(short_description) > MAX_SHORT_DESCRIPTION_LEN and data.get("is_published") == 1:
        data["is_published"] = 0
        return True
    return False


def collect_company_form_data():
    name = request.form.get("name", "").strip()
    slug = request.form.get("slug", "").strip()
    if not slug and name:
        slug = normalize_slug(name)

    return {
        "name": name,
        "slug": slug,
        "website_url": request.form.get("website_url", "").strip() or None,
        "career_url": request.form.get("career_url", "").strip() or None,
        "description": request.form.get("description", "").strip() or None,
        "internship_info": request.form.get("internship_info", "").strip() or None,
        "application_notes": request.form.get("application_notes", "").strip() or None,
        "accent_color": normalize_accent_color(request.form.get("accent_color")),
        "is_active": 1 if request.form.get("is_active") == "1" else 0,
    }


def validate_company_form(db, data, current_company_id: int | None = None):
    errors = []
    if not data["name"]:
        errors.append("Название компании обязательно.")
    if not data["slug"]:
        errors.append("Slug обязателен. Используйте латиницу и дефисы.")
    elif not re.fullmatch(r"[a-z0-9]+(?:-[a-z0-9]+)*", data["slug"]):
        errors.append("Slug может содержать только латиницу, цифры и дефисы.")

    if data["website_url"] and not data["website_url"].startswith(("http://", "https://")):
        errors.append("Website URL должен начинаться с http:// или https://.")
    if data["career_url"] and not data["career_url"].startswith(("http://", "https://")):
        errors.append("Career URL должен начинаться с http:// или https://.")

    if not HEX_COLOR_RE.match(data["accent_color"]):
        errors.append("Accent color должен быть в формате #RRGGBB.")

    if data["slug"]:
        if current_company_id:
            existing = db.execute(
                "SELECT id FROM companies WHERE slug = ? AND id != ?",
                (data["slug"], current_company_id),
            ).fetchone()
        else:
            existing = db.execute(
                "SELECT id FROM companies WHERE slug = ?",
                (data["slug"],),
            ).fetchone()
        if existing:
            errors.append("Компания с таким slug уже существует.")

    return errors


@admin_bp.route("/login", methods=["GET", "POST"])
def login():
    if request.method == "POST":
        if not admin_config_ready():
            flash("Админка не настроена: задайте ADMIN_USERNAME и ADMIN_PASSWORD в .env", "error")
            return render_template("admin/login.html", config_error=True)

        username = request.form.get("username", "").strip()
        password = request.form.get("password", "")
        if verify_admin_credentials(username, password):
            session["admin_auth"] = True
            next_url = request.args.get("next") or url_for("admin.internships")
            return redirect(next_url)

        flash("Неверный логин или пароль.", "error")

    return render_template("admin/login.html", config_error=not admin_config_ready())


@admin_bp.route("/logout")
@admin_login_required
def logout():
    session.clear()
    return redirect(url_for("admin.login"))


@admin_bp.route("/")
@admin_login_required
def root():
    return redirect(url_for("admin.internships"))


@admin_bp.route("/internships")
@admin_login_required
def internships():
    tab = request.args.get("tab", "all")
    db = get_db()

    clauses = []
    if tab == "published":
        clauses.append("i.is_published = 1")
    elif tab == "drafts":
        clauses.append("i.is_published = 0")
    elif tab == "ai":
        clauses.append("i.ai_generated = 1")
    elif tab == "review":
        clauses.append("i.needs_review = 1")

    where_sql = f"WHERE {' AND '.join(clauses)}" if clauses else ""
    rows = db.execute(
        f"""
        SELECT
            i.*,
            c.name AS company_rel_name,
            c.slug AS company_slug,
            c.accent_color AS company_accent_color,
            c.logo_path AS company_logo_path,
            COALESCE(c.name, i.company_name) AS company_display_name
        FROM internships i
        LEFT JOIN companies c ON c.id = i.company_id
        {where_sql}
        ORDER BY datetime(i.created_at) DESC
        """
    ).fetchall()

    latest_import = db.execute(
        "SELECT * FROM import_jobs ORDER BY id DESC LIMIT 1"
    ).fetchone()

    return render_template(
        "admin/internships_list.html",
        internships=rows,
        tab=tab,
        latest_import=latest_import,
    )


@admin_bp.route("/internships/new", methods=["GET", "POST"])
@admin_login_required
def create_internship():
    db = get_db()
    companies = fetch_companies_for_select(db)
    if not companies:
        flash("Сначала создайте хотя бы одну активную компанию.", "error")
        return redirect(url_for("admin.create_company", next=url_for("admin.create_internship")))

    form_data = {
        "title": "",
        "company_id": companies[0]["id"],
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
        form_data = collect_internship_form_data()
        errors = validate_internship_form(db, form_data)
        if errors:
            for err in errors:
                flash(err, "error")
            companies = fetch_companies_for_select(db, form_data.get("company_id"))
            return render_template(
                "admin/internship_form.html",
                form_data=form_data,
                page_title="Новая стажировка",
                companies=companies,
            )

        if apply_short_description_publish_guard(form_data):
            flash(
                f"Краткое описание длиннее {MAX_SHORT_DESCRIPTION_LEN} символов. Стажировка сохранена как черновик.",
                "error",
            )

        company = get_company_for_internship(db, form_data["company_id"])
        db.execute(
            """
            INSERT INTO internships (
                title, company_id, company_name, company_logo_url, city, work_format, direction,
                employment_type, is_paid, salary_info, deadline_date, short_description,
                full_description, requirements, responsibilities, conditions, source_url,
                application_url, status, is_published, created_by_type, ai_generated,
                ai_generated_at, needs_review, created_at, updated_at
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 'human', 0, NULL, 0, ?, ?)
            """,
            (
                form_data["title"],
                form_data["company_id"],
                company["name"],
                company["logo_path"],
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
                now_iso(),
                now_iso(),
            ),
        )
        db.commit()
        flash("Стажировка создана.", "success")
        return redirect(url_for("admin.internships"))

    return render_template(
        "admin/internship_form.html",
        form_data=form_data,
        page_title="Новая стажировка",
        companies=companies,
    )


@admin_bp.route("/internships/<int:internship_id>/edit", methods=["GET", "POST"])
@admin_login_required
def edit_internship(internship_id: int):
    db = get_db()
    internship = db.execute("SELECT * FROM internships WHERE id = ?", (internship_id,)).fetchone()
    if not internship:
        flash("Стажировка не найдена", "error")
        return redirect(url_for("admin.internships"))

    if request.method == "POST":
        form_data = collect_internship_form_data()
        errors = validate_internship_form(db, form_data)
        if errors:
            for err in errors:
                flash(err, "error")
            companies = fetch_companies_for_select(db, form_data.get("company_id"))
            return render_template(
                "admin/internship_form.html",
                form_data=form_data,
                page_title=f"Редактирование #{internship_id}",
                internship=internship,
                companies=companies,
            )

        if apply_short_description_publish_guard(form_data):
            flash(
                f"Краткое описание длиннее {MAX_SHORT_DESCRIPTION_LEN} символов. Стажировка сохранена как черновик.",
                "error",
            )

        company = get_company_for_internship(db, form_data["company_id"])
        db.execute(
            """
            UPDATE internships
            SET title = ?, company_id = ?, company_name = ?, company_logo_url = ?, city = ?,
                work_format = ?, direction = ?, employment_type = ?, is_paid = ?,
                salary_info = ?, deadline_date = ?, short_description = ?, full_description = ?,
                requirements = ?, responsibilities = ?, conditions = ?, source_url = ?,
                application_url = ?, status = ?, is_published = ?, updated_at = ?
            WHERE id = ?
            """,
            (
                form_data["title"],
                form_data["company_id"],
                company["name"],
                company["logo_path"],
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
                now_iso(),
                internship_id,
            ),
        )
        db.commit()
        flash("Стажировка обновлена.", "success")
        return redirect(url_for("admin.internships"))

    form_data = dict(internship)
    companies = fetch_companies_for_select(db, internship["company_id"])
    return render_template(
        "admin/internship_form.html",
        form_data=form_data,
        page_title=f"Редактирование #{internship_id}",
        internship=internship,
        companies=companies,
    )


@admin_bp.route("/internships/<int:internship_id>/delete", methods=["POST"])
@admin_login_required
def delete_internship(internship_id: int):
    db = get_db()
    db.execute("DELETE FROM internships WHERE id = ?", (internship_id,))
    db.commit()
    flash("Стажировка удалена.", "success")
    return redirect(url_for("admin.internships"))


@admin_bp.route("/internships/<int:internship_id>/toggle-publish", methods=["POST"])
@admin_login_required
def toggle_publish(internship_id: int):
    db = get_db()
    row = db.execute("SELECT is_published FROM internships WHERE id = ?", (internship_id,)).fetchone()
    if not row:
        flash("Стажировка не найдена", "error")
        return redirect(url_for("admin.internships"))

    next_value = 0 if row["is_published"] else 1
    db.execute(
        "UPDATE internships SET is_published = ?, updated_at = ? WHERE id = ?",
        (next_value, now_iso(), internship_id),
    )
    db.commit()
    flash("Статус публикации обновлен.", "success")
    return redirect(url_for("admin.internships"))


@admin_bp.route("/internships/<int:internship_id>/verify", methods=["POST"])
@admin_login_required
def verify_ai_internship(internship_id: int):
    db = get_db()
    row = db.execute("SELECT id FROM internships WHERE id = ?", (internship_id,)).fetchone()
    if not row:
        flash("Стажировка не найдена", "error")
        return redirect(url_for("admin.internships"))

    publish = 1 if request.form.get("publish") == "1" else 0
    db.execute(
        """
        UPDATE internships
        SET needs_review = 0,
            last_verified_at = ?,
            is_published = CASE WHEN ? = 1 THEN 1 ELSE is_published END,
            updated_at = ?
        WHERE id = ?
        """,
        (now_iso(), publish, now_iso(), internship_id),
    )
    db.commit()
    flash("Карточка отмечена как просмотренная человеком.", "success")
    return redirect(url_for("admin.internships", tab="review"))


@admin_bp.route("/companies")
@admin_login_required
def companies():
    db = get_db()
    rows = db.execute(
        """
        SELECT
            c.*,
            COUNT(i.id) AS internships_count
        FROM companies c
        LEFT JOIN internships i ON i.company_id = c.id
        GROUP BY c.id
        ORDER BY datetime(c.created_at) DESC
        """
    ).fetchall()
    return render_template("admin/companies_list.html", companies=rows)


@admin_bp.route("/companies/new", methods=["GET", "POST"])
@admin_login_required
def create_company():
    form_data = {
        "name": "",
        "slug": "",
        "website_url": "",
        "career_url": "",
        "description": "",
        "internship_info": "",
        "application_notes": "",
        "accent_color": "#0e7490",
        "is_active": 1,
    }
    next_url = safe_admin_next_url(request.args.get("next") or request.form.get("next"))

    if request.method == "POST":
        db = get_db()
        form_data = collect_company_form_data()
        errors = validate_company_form(db, form_data)

        logo_path = None
        try:
            logo_path = save_company_logo(request.files.get("logo_file"), form_data["slug"])
        except ValueError as exc:
            errors.append(str(exc))

        if errors:
            for err in errors:
                flash(err, "error")
            return render_template(
                "admin/company_form.html",
                form_data=form_data,
                page_title="Новая компания",
                next_url=next_url,
                logo_path=None,
            )

        db.execute(
            """
            INSERT INTO companies (
                name, slug, logo_path, website_url, career_url, description,
                internship_info, application_notes, accent_color, is_active, created_at, updated_at
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            """,
            (
                form_data["name"],
                form_data["slug"],
                logo_path,
                form_data["website_url"],
                form_data["career_url"],
                form_data["description"],
                form_data["internship_info"],
                form_data["application_notes"],
                form_data["accent_color"],
                form_data["is_active"],
                now_iso(),
                now_iso(),
            ),
        )
        db.commit()
        flash("Компания создана.", "success")
        if next_url:
            return redirect(next_url)
        return redirect(url_for("admin.companies"))

    return render_template(
        "admin/company_form.html",
        form_data=form_data,
        page_title="Новая компания",
        next_url=next_url,
        logo_path=None,
    )


@admin_bp.route("/companies/<int:company_id>/edit", methods=["GET", "POST"])
@admin_login_required
def edit_company(company_id: int):
    db = get_db()
    company = db.execute("SELECT * FROM companies WHERE id = ?", (company_id,)).fetchone()
    if not company:
        flash("Компания не найдена.", "error")
        return redirect(url_for("admin.companies"))

    next_url = safe_admin_next_url(request.args.get("next") or request.form.get("next"))

    if request.method == "POST":
        form_data = collect_company_form_data()
        errors = validate_company_form(db, form_data, current_company_id=company_id)

        logo_path = company["logo_path"]
        try:
            new_logo = save_company_logo(request.files.get("logo_file"), form_data["slug"])
            if new_logo:
                logo_path = new_logo
        except ValueError as exc:
            errors.append(str(exc))

        if errors:
            for err in errors:
                flash(err, "error")
            return render_template(
                "admin/company_form.html",
                form_data=form_data,
                page_title=f"Редактирование компании #{company_id}",
                company=company,
                logo_path=logo_path,
                next_url=next_url,
            )

        db.execute(
            """
            UPDATE companies
            SET name = ?, slug = ?, logo_path = ?, website_url = ?, career_url = ?,
                description = ?, internship_info = ?, application_notes = ?,
                accent_color = ?, is_active = ?, updated_at = ?
            WHERE id = ?
            """,
            (
                form_data["name"],
                form_data["slug"],
                logo_path,
                form_data["website_url"],
                form_data["career_url"],
                form_data["description"],
                form_data["internship_info"],
                form_data["application_notes"],
                form_data["accent_color"],
                form_data["is_active"],
                now_iso(),
                company_id,
            ),
        )
        db.execute(
            """
            UPDATE internships
            SET company_name = ?, company_logo_url = ?, updated_at = ?
            WHERE company_id = ?
            """,
            (
                form_data["name"],
                logo_path,
                now_iso(),
                company_id,
            ),
        )
        db.commit()
        flash("Компания обновлена.", "success")
        if next_url:
            return redirect(next_url)
        return redirect(url_for("admin.companies"))

    return render_template(
        "admin/company_form.html",
        form_data=dict(company),
        page_title=f"Редактирование компании #{company_id}",
        company=company,
        logo_path=company["logo_path"],
        next_url=next_url,
    )


@admin_bp.route("/companies/<int:company_id>/toggle-active", methods=["POST"])
@admin_login_required
def toggle_company_active(company_id: int):
    db = get_db()
    row = db.execute("SELECT id, is_active FROM companies WHERE id = ?", (company_id,)).fetchone()
    if not row:
        flash("Компания не найдена.", "error")
        return redirect(url_for("admin.companies"))

    next_value = 0 if row["is_active"] else 1
    db.execute(
        "UPDATE companies SET is_active = ?, updated_at = ? WHERE id = ?",
        (next_value, now_iso(), company_id),
    )
    db.commit()
    flash("Статус компании обновлен.", "success")
    return redirect(url_for("admin.companies"))


@admin_bp.route("/companies/<int:company_id>/delete", methods=["POST"])
@admin_login_required
def delete_company(company_id: int):
    db = get_db()
    company = db.execute("SELECT id FROM companies WHERE id = ?", (company_id,)).fetchone()
    if not company:
        flash("Компания не найдена.", "error")
        return redirect(url_for("admin.companies"))

    related = db.execute(
        "SELECT COUNT(*) AS cnt FROM internships WHERE company_id = ?",
        (company_id,),
    ).fetchone()
    if related["cnt"] > 0:
        flash("Нельзя удалить компанию, у которой есть стажировки. Сначала деактивируйте её.", "error")
        return redirect(url_for("admin.companies"))

    db.execute("DELETE FROM companies WHERE id = ?", (company_id,))
    db.commit()
    flash("Компания удалена.", "success")
    return redirect(url_for("admin.companies"))


@admin_bp.route("/imports", methods=["GET", "POST"])
@admin_login_required
def imports_page():
    db = get_db()

    if request.method == "POST":
        file = request.files.get("links_file")
        if not file or not file.filename:
            flash("Выберите файл .txt или .csv со ссылками.", "error")
            return redirect(url_for("admin.imports_page"))

        try:
            import_job_id = run_import(file)
            return redirect(url_for("admin.import_result", import_job_id=import_job_id))
        except Exception as exc:
            flash(f"Импорт завершился ошибкой: {exc}", "error")
            return redirect(url_for("admin.imports_page"))

    jobs = db.execute("SELECT * FROM import_jobs ORDER BY id DESC LIMIT 30").fetchall()
    return render_template("admin/imports.html", jobs=jobs)


@admin_bp.route("/imports/<int:import_job_id>")
@admin_login_required
def import_result(import_job_id: int):
    db = get_db()
    job = db.execute("SELECT * FROM import_jobs WHERE id = ?", (import_job_id,)).fetchone()
    if not job:
        flash("Импорт не найден", "error")
        return redirect(url_for("admin.imports_page"))

    items = db.execute(
        "SELECT * FROM import_job_items WHERE import_job_id = ? ORDER BY id ASC",
        (import_job_id,),
    ).fetchall()
    return render_template("admin/import_result.html", job=job, items=items)
