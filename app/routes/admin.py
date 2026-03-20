from flask import Blueprint, flash, redirect, render_template, request, session, url_for

from app.auth import admin_config_ready, admin_login_required, verify_admin_credentials
from app.constants import DIRECTIONS, STATUSES, WORK_FORMATS
from app.db import get_db, now_iso
from app.services.import_service import run_import

admin_bp = Blueprint("admin", __name__, url_prefix="/admin")


def parse_date(value: str | None):
    return value.strip() if value and value.strip() else None


def collect_form_data():
    return {
        "title": request.form.get("title", "").strip(),
        "company_name": request.form.get("company_name", "").strip(),
        "company_logo_url": request.form.get("company_logo_url", "").strip() or None,
        "city": request.form.get("city", "").strip(),
        "work_format": request.form.get("work_format", "").strip(),
        "direction": request.form.get("direction", "").strip(),
        "employment_type": request.form.get("employment_type", "").strip(),
        "is_paid": 1 if request.form.get("is_paid") == "1" else 0,
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


def validate_form(data):
    errors = []
    required = ["title", "company_name", "city", "short_description", "full_description", "source_url"]
    for field in required:
        if not data[field]:
            errors.append(f"Поле '{field}' обязательно.")

    if data["work_format"] not in WORK_FORMATS:
        errors.append("Некорректный формат работы")
    if data["direction"] not in DIRECTIONS:
        errors.append("Некорректное направление")
    if data["status"] not in STATUSES:
        errors.append("Некорректный статус")
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
        clauses.append("is_published = 1")
    elif tab == "drafts":
        clauses.append("is_published = 0")
    elif tab == "ai":
        clauses.append("ai_generated = 1")
    elif tab == "review":
        clauses.append("needs_review = 1")

    where_sql = f"WHERE {' AND '.join(clauses)}" if clauses else ""
    rows = db.execute(
        f"""
        SELECT * FROM internships
        {where_sql}
        ORDER BY datetime(created_at) DESC
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
        errors = validate_form(form_data)
        if errors:
            for err in errors:
                flash(err, "error")
            return render_template("admin/internship_form.html", form_data=form_data, page_title="Новая стажировка")

        db = get_db()
        db.execute(
            """
            INSERT INTO internships (
                title, company_name, company_logo_url, city, work_format, direction,
                employment_type, is_paid, salary_info, deadline_date, short_description,
                full_description, requirements, responsibilities, conditions, source_url,
                application_url, status, is_published, created_by_type, ai_generated,
                ai_generated_at, needs_review, created_at, updated_at
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 'human', 0, NULL, 0, ?, ?)
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
                now_iso(),
                now_iso(),
            ),
        )
        db.commit()
        flash("Стажировка создана.", "success")
        return redirect(url_for("admin.internships"))

    return render_template("admin/internship_form.html", form_data=form_data, page_title="Новая стажировка")


@admin_bp.route("/internships/<int:internship_id>/edit", methods=["GET", "POST"])
@admin_login_required
def edit_internship(internship_id: int):
    db = get_db()
    internship = db.execute("SELECT * FROM internships WHERE id = ?", (internship_id,)).fetchone()
    if not internship:
        flash("Стажировка не найдена", "error")
        return redirect(url_for("admin.internships"))

    if request.method == "POST":
        form_data = collect_form_data()
        errors = validate_form(form_data)
        if errors:
            for err in errors:
                flash(err, "error")
            return render_template(
                "admin/internship_form.html",
                form_data=form_data,
                page_title=f"Редактирование #{internship_id}",
                internship=internship,
            )

        db.execute(
            """
            UPDATE internships
            SET title = ?, company_name = ?, company_logo_url = ?, city = ?,
                work_format = ?, direction = ?, employment_type = ?, is_paid = ?,
                salary_info = ?, deadline_date = ?, short_description = ?, full_description = ?,
                requirements = ?, responsibilities = ?, conditions = ?, source_url = ?,
                application_url = ?, status = ?, is_published = ?, updated_at = ?
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
                now_iso(),
                internship_id,
            ),
        )
        db.commit()
        flash("Стажировка обновлена.", "success")
        return redirect(url_for("admin.internships"))

    return render_template(
        "admin/internship_form.html",
        form_data=dict(internship),
        page_title=f"Редактирование #{internship_id}",
        internship=internship,
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
