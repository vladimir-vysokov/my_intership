from flask import Blueprint, abort, flash, jsonify, redirect, render_template, request, url_for

from app.constants import APPLICATION_STATUSES, DIRECTIONS, STATUSES, WORK_FORMATS
from app.db import get_db, now_iso

public_bp = Blueprint("public", __name__)

APPLICATION_COLUMN_ORDER = [
    "want_to_apply",
    "applied",
    "test",
    "interview",
    "offer",
    "rejected",
]
ALLOWED_STATUS_TRANSITIONS = {
    "want_to_apply": {"want_to_apply", "applied", "offer", "rejected", "archived"},
    "applied": {"applied", "test", "interview", "offer", "rejected", "archived"},
    "test": {"test", "interview", "offer", "rejected", "archived"},
    "interview": {"interview", "test", "offer", "rejected", "archived"},
    "offer": {"offer", "rejected", "archived"},
    "rejected": {"rejected", "offer", "archived"},
    "archived": {"archived"},
}


def is_transition_allowed(from_status: str, to_status: str) -> bool:
    return to_status in ALLOWED_STATUS_TRANSITIONS.get(from_status, {from_status})


def internship_select_fragment():
    return """
    SELECT
        i.*,
        c.id AS company_rel_id,
        c.name AS company_rel_name,
        c.slug AS company_slug,
        c.logo_path AS company_logo_path,
        c.accent_color AS company_accent_color,
        c.website_url AS company_website_url,
        c.career_url AS company_career_url,
        c.description AS company_description,
        c.internship_info AS company_internship_info,
        c.application_notes AS company_application_notes,
        COALESCE(c.name, i.company_name) AS company_display_name,
        COALESCE(c.logo_path, i.company_logo_url) AS company_display_logo
    FROM internships i
    LEFT JOIN companies c ON c.id = i.company_id
    """


def parse_form_date(value: str | None):
    raw = (value or "").strip()
    return raw or None


def create_attempt(
    db,
    internship_id: int,
    status: str,
    marker_enabled: int,
    stage_completed: int,
    status_note: str | None,
    next_step_date: str | None,
    next_step_time: str | None,
    note: str | None,
    applied_at: str | None,
):
    now = now_iso()
    cursor = db.execute(
        """
        INSERT INTO application_attempts (
            internship_id, status, marker_enabled, stage_completed,
            status_note, next_step_date, next_step_time, note, applied_at,
            created_at, updated_at
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        """,
        (
            internship_id,
            status,
            marker_enabled,
            stage_completed,
            status_note,
            next_step_date,
            next_step_time,
            note,
            applied_at,
            now,
            now,
        ),
    )
    attempt_id = cursor.lastrowid
    db.execute(
        """
        INSERT INTO application_history (
            attempt_id, event_type, to_status, event_note, created_at
        ) VALUES (?, 'create', ?, ?, ?)
        """,
        (attempt_id, status, status_note, now),
    )
    return attempt_id


def add_history_event(
    db,
    attempt_id: int,
    event_type: str,
    from_status: str | None,
    to_status: str | None,
    note: str | None = None,
):
    db.execute(
        """
        INSERT INTO application_history (
            attempt_id, event_type, from_status, to_status, event_note, created_at
        ) VALUES (?, ?, ?, ?, ?, ?)
        """,
        (attempt_id, event_type, from_status, to_status, note, now_iso()),
    )


@public_bp.route("/")
def home():
    return render_template("public/home.html")


@public_bp.route("/internships")
def catalog():
    db = get_db()

    q = request.args.get("q", "").strip()
    city = request.args.get("city", "").strip()
    work_format = request.args.get("work_format", "").strip()
    direction = request.args.get("direction", "").strip()
    paid = request.args.get("paid", "").strip()
    deadline_filter = request.args.get("deadline", "").strip()
    sort = request.args.get("sort", "newest").strip()

    where_clauses = ["i.is_published = 1"]
    params = []

    if q:
        where_clauses.append(
            "(i.title LIKE ? OR COALESCE(c.name, i.company_name) LIKE ? OR i.direction LIKE ? OR i.city LIKE ? OR i.short_description LIKE ? OR i.full_description LIKE ?)"
        )
        pattern = f"%{q}%"
        params.extend([pattern] * 6)

    if city:
        where_clauses.append("i.city = ?")
        params.append(city)

    if work_format in WORK_FORMATS:
        where_clauses.append("i.work_format = ?")
        params.append(work_format)

    if direction in DIRECTIONS:
        where_clauses.append("i.direction = ?")
        params.append(direction)

    if paid in {"1", "0", "-1"}:
        where_clauses.append("i.is_paid = ?")
        params.append(int(paid))

    if deadline_filter == "has_deadline":
        where_clauses.append("i.deadline_date IS NOT NULL")
    elif deadline_filter == "open_enrollment":
        where_clauses.append("i.status = 'open'")
    elif deadline_filter == "unknown":
        where_clauses.append("i.deadline_date IS NULL")

    order_by = "datetime(i.created_at) DESC"
    if sort == "deadline":
        order_by = "CASE WHEN i.deadline_date IS NULL THEN 1 ELSE 0 END, date(i.deadline_date) ASC"
    elif sort == "company":
        order_by = "COALESCE(c.name, i.company_name) COLLATE NOCASE ASC"

    cities_rows = db.execute(
        "SELECT DISTINCT city FROM internships WHERE is_published = 1 ORDER BY city"
    ).fetchall()

    internships = db.execute(
        f"""
        {internship_select_fragment()}
        WHERE {' AND '.join(where_clauses)}
        ORDER BY {order_by}
        """,
        params,
    ).fetchall()

    return render_template(
        "public/catalog.html",
        internships=internships,
        cities=[r["city"] for r in cities_rows],
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


@public_bp.route("/internships/<int:internship_id>")
def detail(internship_id: int):
    db = get_db()
    internship = db.execute(
        f"""
        {internship_select_fragment()}
        WHERE i.id = ? AND i.is_published = 1
        """,
        (internship_id,),
    ).fetchone()

    if not internship:
        abort(404)

    latest_attempt = db.execute(
        """
        SELECT *
        FROM application_attempts
        WHERE internship_id = ?
        ORDER BY datetime(created_at) DESC, id DESC
        LIMIT 1
        """,
        (internship_id,),
    ).fetchone()
    attempts_count = db.execute(
        "SELECT COUNT(*) AS cnt FROM application_attempts WHERE internship_id = ?",
        (internship_id,),
    ).fetchone()["cnt"]
    selected_attempt_number = None
    selected_attempt_id = request.args.get("attempt_id", type=int)
    if selected_attempt_id:
        attempt_rows = db.execute(
            """
            SELECT id
            FROM application_attempts
            WHERE internship_id = ?
            ORDER BY datetime(created_at) ASC, id ASC
            """,
            (internship_id,),
        ).fetchall()
        for idx, item in enumerate(attempt_rows, start=1):
            if item["id"] == selected_attempt_id:
                selected_attempt_number = idx
                break

    return render_template(
        "public/detail.html",
        internship=internship,
        application=latest_attempt,
        attempts_count=attempts_count,
        selected_attempt_number=selected_attempt_number,
    )


@public_bp.route("/companies")
def companies():
    db = get_db()
    rows = db.execute(
        """
        SELECT
            c.*,
            COUNT(i.id) AS active_internships_count
        FROM companies c
        LEFT JOIN internships i
            ON i.company_id = c.id
            AND i.is_published = 1
        WHERE c.is_active = 1
        GROUP BY c.id
        ORDER BY c.name COLLATE NOCASE ASC
        """
    ).fetchall()
    return render_template("public/companies.html", companies=rows)


@public_bp.route("/companies/<slug>")
def company_detail(slug: str):
    db = get_db()
    company = db.execute(
        """
        SELECT *
        FROM companies
        WHERE slug = ? AND is_active = 1
        """,
        (slug,),
    ).fetchone()
    if not company:
        abort(404)

    internships = db.execute(
        f"""
        {internship_select_fragment()}
        WHERE i.company_id = ? AND i.is_published = 1
        ORDER BY datetime(i.created_at) DESC
        """,
        (company["id"],),
    ).fetchall()

    return render_template("public/company_detail.html", company=company, internships=internships)


@public_bp.route("/applications")
def applications_board():
    db = get_db()
    show_archive = (request.args.get("view") or "").strip() == "archive"

    rows = db.execute(
        f"""
        SELECT
            a.*,
            i.title AS internship_title,
            i.city AS internship_city,
            i.deadline_date AS internship_deadline_date,
            i.source_url AS internship_source_url,
            i.application_url AS internship_application_url,
            c.slug AS company_slug,
            COALESCE(c.name, i.company_name) AS company_display_name,
            c.accent_color AS company_accent_color,
            (
                SELECT h.event_note
                FROM application_history h
                WHERE h.attempt_id = a.id
                ORDER BY datetime(h.created_at) DESC, h.id DESC
                LIMIT 1
            ) AS current_stage_comment
        FROM application_attempts a
        JOIN internships i ON i.id = a.internship_id
        LEFT JOIN companies c ON c.id = i.company_id
        ORDER BY
            CASE a.status
                WHEN 'want_to_apply' THEN 1
                WHEN 'applied' THEN 2
                WHEN 'test' THEN 3
                WHEN 'interview' THEN 4
                WHEN 'offer' THEN 5
                WHEN 'rejected' THEN 6
                WHEN 'archived' THEN 7
                ELSE 99
            END,
            datetime(a.updated_at) DESC
        """
    ).fetchall()

    ordered_rows = sorted(
        rows,
        key=lambda r: (
            r["internship_id"],
            r["created_at"] or "",
            r["id"],
        ),
    )
    attempt_numbers: dict[int, int] = {}
    number_by_attempt: dict[int, int] = {}
    for row in ordered_rows:
        iid = row["internship_id"]
        attempt_numbers[iid] = attempt_numbers.get(iid, 0) + 1
        number_by_attempt[row["id"]] = attempt_numbers[iid]

    grouped = {status: [] for status in APPLICATION_COLUMN_ORDER}
    archive_rows = []
    for row in rows:
        row_dict = dict(row)
        row_dict["attempt_number"] = number_by_attempt.get(row["id"], 1)
        status = row["status"]
        if status == "archived":
            archive_rows.append(row_dict)
            continue
        if status not in grouped:
            grouped["want_to_apply"].append(row_dict)
            continue
        grouped[status].append(row_dict)

    return render_template(
        "public/applications.html",
        grouped_applications=grouped,
        column_order=APPLICATION_COLUMN_ORDER,
        show_archive=show_archive,
        archive_rows=archive_rows,
        archive_count=len(archive_rows),
    )


@public_bp.route("/applications/new-internship", methods=["GET", "POST"])
def create_custom_internship_for_tracker():
    db = get_db()
    companies = db.execute(
        """
        SELECT id, name
        FROM companies
        WHERE is_active = 1
        ORDER BY name COLLATE NOCASE ASC
        """
    ).fetchall()

    if request.method == "POST":
        title = (request.form.get("title") or "").strip()
        company_name_raw = (request.form.get("company_name") or "").strip()
        desired_status = (request.form.get("status") or "want_to_apply").strip()
        next_step_date = parse_form_date(request.form.get("next_step_date"))
        next_step_time = parse_form_date(request.form.get("next_step_time"))
        applied_at = parse_form_date(request.form.get("applied_at"))
        note = (request.form.get("note") or "").strip() or None
        marker_enabled = 1 if request.form.get("marker_enabled") == "1" else 0
        stage_completed = 1 if request.form.get("stage_completed") == "1" else 0
        if marker_enabled == 0:
            stage_completed = 0

        if desired_status not in APPLICATION_STATUSES:
            desired_status = "want_to_apply"

        if not title:
            flash("Укажите должность.", "error")
            return render_template("public/custom_internship_form.html", companies=companies)
        if not company_name_raw:
            flash("Укажите компанию или выберите из списка.", "error")
            return render_template("public/custom_internship_form.html", companies=companies)

        company = db.execute(
            "SELECT id, name FROM companies WHERE lower(name) = lower(?) LIMIT 1",
            (company_name_raw,),
        ).fetchone()
        company_id = company["id"] if company else None
        company_name = company["name"] if company else company_name_raw

        now = now_iso()
        cursor = db.execute(
            """
            INSERT INTO internships (
                title,
                company_id,
                company_name,
                city,
                work_format,
                direction,
                employment_type,
                is_paid,
                short_description,
                full_description,
                source_url,
                status,
                is_published,
                created_by_type,
                ai_generated,
                needs_review,
                created_at,
                updated_at
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 'human', 0, 0, ?, ?)
            """,
            (
                title,
                company_id,
                company_name,
                "Не указано",
                "office",
                "other",
                "Не указано",
                -1,
                "Добавлено пользователем в трекер.",
                "Пользовательская стажировка для личного трекера подач.",
                "user://custom-tracker-entry",
                "open",
                0,
                now,
                now,
            ),
        )
        internship_id = cursor.lastrowid
        create_attempt(
            db=db,
            internship_id=internship_id,
            status=desired_status,
            marker_enabled=marker_enabled,
            stage_completed=stage_completed,
            status_note=None,
            next_step_date=next_step_date,
            next_step_time=next_step_time,
            note=note,
            applied_at=applied_at,
        )
        db.commit()
        flash("Стажировка добавлена в ваш трекер.", "success")
        return redirect(url_for("public.applications_board"))

    return render_template("public/custom_internship_form.html", companies=companies)


@public_bp.route("/applications/add/<int:internship_id>", methods=["POST"])
def add_to_tracker(internship_id: int):
    db = get_db()
    internship = db.execute(
        "SELECT id FROM internships WHERE id = ? AND is_published = 1",
        (internship_id,),
    ).fetchone()
    if not internship:
        abort(404)

    desired_status = (request.form.get("status") or "want_to_apply").strip()
    if desired_status not in APPLICATION_STATUSES:
        desired_status = "want_to_apply"

    existing_count = db.execute(
        "SELECT COUNT(*) AS cnt FROM application_attempts WHERE internship_id = ?",
        (internship_id,),
    ).fetchone()["cnt"]
    confirm_reapply = request.form.get("confirm_reapply") == "1"
    if existing_count > 0 and not confirm_reapply:
        flash("Вы уже подавались на эту стажировку. Нажмите еще раз и подтвердите повторную подачу.", "error")
        return redirect(url_for("public.detail", internship_id=internship_id))

    create_attempt(
        db=db,
        internship_id=internship_id,
        status=desired_status,
        marker_enabled=1,
        stage_completed=1,
        status_note=None,
        next_step_date=None,
        next_step_time=None,
        note=None,
        applied_at=None,
    )
    db.commit()
    flash("Подача добавлена в трекер.", "success")
    return redirect(url_for("public.applications_board"))


@public_bp.route("/applications/archive-from-internship/<int:internship_id>", methods=["POST"])
def archive_from_internship(internship_id: int):
    db = get_db()
    internship = db.execute(
        "SELECT id FROM internships WHERE id = ? AND is_published = 1",
        (internship_id,),
    ).fetchone()
    if not internship:
        abort(404)

    existing = db.execute(
        """
        SELECT id, status
        FROM application_attempts
        WHERE internship_id = ?
        ORDER BY datetime(updated_at) DESC, id DESC
        LIMIT 1
        """,
        (internship_id,),
    ).fetchone()
    if existing:
        prev_status = existing["status"]
        db.execute(
            "UPDATE application_attempts SET status = 'archived', updated_at = ? WHERE id = ?",
            (now_iso(), existing["id"]),
        )
        add_history_event(
            db=db,
            attempt_id=existing["id"],
            event_type="move",
            from_status=prev_status,
            to_status="archived",
            note="Перенос в архив со страницы стажировки",
        )
    else:
        create_attempt(
            db=db,
            internship_id=internship_id,
            status="archived",
            marker_enabled=1,
            stage_completed=1,
            status_note="Добавлено сразу в архив",
            next_step_date=None,
            next_step_time=None,
            note=None,
            applied_at=None,
        )
    db.commit()
    flash("Стажировка добавлена в ваш архив.", "success")
    return redirect(url_for("public.applications_board", view="archive"))


@public_bp.route("/applications/<int:application_id>/edit", methods=["GET", "POST"])
def edit_application(application_id: int):
    db = get_db()
    application = db.execute(
        """
        SELECT
            a.*,
            i.title AS internship_title,
            COALESCE(c.name, i.company_name) AS company_display_name
        FROM application_attempts a
        JOIN internships i ON i.id = a.internship_id
        LEFT JOIN companies c ON c.id = i.company_id
        WHERE a.id = ?
        """,
        (application_id,),
    ).fetchone()
    if not application:
        abort(404)

    if request.method == "POST":
        next_step_date = parse_form_date(request.form.get("next_step_date"))
        next_step_time = parse_form_date(request.form.get("next_step_time"))
        applied_at = parse_form_date(request.form.get("applied_at"))
        note = (request.form.get("note") or "").strip() or None
        marker_enabled = 1 if request.form.get("marker_enabled") == "1" else 0
        stage_completed = int(application["stage_completed"] or 0)
        old_marker_enabled = int(application["marker_enabled"] or 0)
        if marker_enabled == 0:
            stage_completed = 0
        elif old_marker_enabled == 0 and marker_enabled == 1:
            stage_completed = 0

        db.execute(
            """
            UPDATE application_attempts
            SET marker_enabled = ?,
                stage_completed = ?,
                next_step_date = ?,
                next_step_time = ?,
                note = ?,
                applied_at = ?,
                updated_at = ?
            WHERE id = ?
            """,
            (
                marker_enabled,
                stage_completed,
                next_step_date,
                next_step_time,
                note,
                applied_at,
                now_iso(),
                application_id,
            ),
        )
        db.commit()
        if old_marker_enabled != marker_enabled:
            flash(
                "Маркер выполнения включен." if marker_enabled == 1 else "Маркер выполнения выключен.",
                "success",
            )
        flash("Подача обновлена.", "success")
        return redirect(url_for("public.edit_application", application_id=application_id))

    attempts_rows = db.execute(
        """
        SELECT
            a.*,
            i.title AS internship_title
        FROM application_attempts a
        JOIN internships i ON i.id = a.internship_id
        WHERE a.internship_id = ?
        ORDER BY datetime(a.created_at) ASC, a.id ASC
        """,
        (application["internship_id"],),
    ).fetchall()

    attempts = []
    current_attempt_number = 1
    for index, attempt in enumerate(attempts_rows, start=1):
        history = db.execute(
            """
            SELECT *
            FROM application_history
            WHERE attempt_id = ?
            ORDER BY datetime(created_at) ASC, id ASC
            """,
            (attempt["id"],),
        ).fetchall()
        checkbox_event_id = None
        for history_event in reversed(history):
            if history_event["event_type"] != "completion":
                checkbox_event_id = history_event["id"]
                break
        attempts.append(
            {
                "number": index,
                "attempt": attempt,
                "history": history,
                "checkbox_event_id": checkbox_event_id,
                "is_current": attempt["id"] == application_id,
            }
        )
        if attempt["id"] == application_id:
            current_attempt_number = index

    return render_template(
        "public/application_edit.html",
        application=application,
        attempts=attempts,
        current_attempt_number=current_attempt_number,
    )


@public_bp.route("/applications/<int:application_id>/move", methods=["POST"])
def move_application(application_id: int):
    db = get_db()
    row = db.execute(
        "SELECT id, status, marker_enabled FROM application_attempts WHERE id = ?",
        (application_id,),
    ).fetchone()
    if not row:
        return jsonify({"ok": False, "message": "Подача не найдена"}), 404

    payload = request.get_json(silent=True) or {}
    new_status = (payload.get("status") or "").strip()
    if new_status not in APPLICATION_STATUSES:
        return jsonify({"ok": False, "message": "Некорректный статус"}), 400

    old_status = row["status"]
    if not is_transition_allowed(old_status, new_status):
        return jsonify(
            {
                "ok": False,
                "message": "Перенос между выбранными этапами запрещен.",
            }
        ), 400

    if old_status != new_status and int(row["marker_enabled"] or 0) == 1:
        db.execute(
            """
            UPDATE application_attempts
            SET status = ?, stage_completed = 0, updated_at = ?
            WHERE id = ?
            """,
            (new_status, now_iso(), application_id),
        )
    else:
        db.execute(
            "UPDATE application_attempts SET status = ?, updated_at = ? WHERE id = ?",
            (new_status, now_iso(), application_id),
        )
    if old_status != new_status:
        add_history_event(
            db=db,
            attempt_id=application_id,
            event_type="move",
            from_status=old_status,
            to_status=new_status,
            note=None,
        )
    db.commit()
    return jsonify({"ok": True, "status": new_status})


@public_bp.route("/applications/<int:application_id>/archive", methods=["POST"])
def archive_application(application_id: int):
    db = get_db()
    row = db.execute(
        "SELECT id, status FROM application_attempts WHERE id = ?",
        (application_id,),
    ).fetchone()
    if not row:
        abort(404)
    db.execute(
        "UPDATE application_attempts SET status = 'archived', updated_at = ? WHERE id = ?",
        (now_iso(), application_id),
    )
    if row["status"] != "archived":
        add_history_event(
            db=db,
            attempt_id=application_id,
            event_type="move",
            from_status=row["status"],
            to_status="archived",
            note="Перенос в архив",
        )
    db.commit()
    flash("Подача отправлена в архив.", "success")
    return redirect(url_for("public.applications_board"))


@public_bp.route("/applications/<int:application_id>/archive-move", methods=["POST"])
def archive_application_move(application_id: int):
    db = get_db()
    row = db.execute(
        "SELECT id, status FROM application_attempts WHERE id = ?",
        (application_id,),
    ).fetchone()
    if not row:
        return jsonify({"ok": False, "message": "Подача не найдена"}), 404

    old_status = row["status"]
    db.execute(
        "UPDATE application_attempts SET status = 'archived', updated_at = ? WHERE id = ?",
        (now_iso(), application_id),
    )
    if old_status != "archived":
        add_history_event(
            db=db,
            attempt_id=application_id,
            event_type="move",
            from_status=old_status,
            to_status="archived",
            note="Перенос в архив",
        )
    db.commit()
    return jsonify({"ok": True, "status": "archived"})


@public_bp.route("/applications/<int:application_id>/restore", methods=["POST"])
def restore_application_from_archive(application_id: int):
    db = get_db()
    row = db.execute(
        "SELECT id, status FROM application_attempts WHERE id = ?",
        (application_id,),
    ).fetchone()
    if not row:
        abort(404)

    previous = db.execute(
        """
        SELECT from_status
        FROM application_history
        WHERE attempt_id = ? AND to_status = 'archived' AND from_status IS NOT NULL
        ORDER BY datetime(created_at) DESC, id DESC
        LIMIT 1
        """,
        (application_id,),
    ).fetchone()
    target_status = previous["from_status"] if previous and previous["from_status"] in APPLICATION_STATUSES else "want_to_apply"

    db.execute(
        "UPDATE application_attempts SET status = ?, updated_at = ? WHERE id = ?",
        (target_status, now_iso(), application_id),
    )
    add_history_event(
        db=db,
        attempt_id=application_id,
        event_type="move",
        from_status="archived",
        to_status=target_status,
        note="Возврат из архива",
    )
    db.commit()
    flash("Подача возвращена из архива на доску.", "success")
    return redirect(url_for("public.applications_board"))


@public_bp.route("/applications/<int:application_id>/delete", methods=["POST"])
def delete_application(application_id: int):
    db = get_db()
    db.execute("DELETE FROM application_attempts WHERE id = ?", (application_id,))
    db.commit()
    flash("Подача удалена из трекера. Нумерация подач пересчитана автоматически.", "success")
    return redirect(url_for("public.applications_board"))


@public_bp.route("/applications/<int:application_id>/events", methods=["POST"])
def add_attempt_event(application_id: int):
    db = get_db()
    row = db.execute(
        "SELECT id, status FROM application_attempts WHERE id = ?",
        (application_id,),
    ).fetchone()
    if not row:
        abort(404)

    # Adds a generic stage event tied to the current attempt status.
    note = None
    add_history_event(
        db=db,
        attempt_id=application_id,
        event_type="stage",
        from_status=row["status"],
        to_status=row["status"],
        note=note,
    )
    db.commit()
    flash("Этап добавлен в таймлайн подачи.", "success")
    return redirect(url_for("public.edit_application", application_id=application_id))


@public_bp.route("/applications/<int:application_id>/events/<int:event_id>/completion", methods=["POST"])
def update_attempt_event_completion(application_id: int, event_id: int):
    db = get_db()
    attempt = db.execute(
        "SELECT id, status, marker_enabled, stage_completed FROM application_attempts WHERE id = ?",
        (application_id,),
    ).fetchone()
    if not attempt:
        abort(404)
    if int(attempt["marker_enabled"] or 0) == 0:
        flash("Сначала включите маркер выполнения в левой панели.", "error")
        return redirect(url_for("public.edit_application", application_id=application_id))
    if attempt["status"] == "want_to_apply":
        flash("На этапе 'Хочу податься' отметка выполнения не используется.", "error")
        return redirect(url_for("public.edit_application", application_id=application_id))

    event = db.execute(
        "SELECT id FROM application_history WHERE id = ? AND attempt_id = ?",
        (event_id, application_id),
    ).fetchone()
    if not event:
        abort(404)

    last_non_completion_event = db.execute(
        """
        SELECT id
        FROM application_history
        WHERE attempt_id = ? AND event_type != 'completion'
        ORDER BY datetime(created_at) DESC, id DESC
        LIMIT 1
        """,
        (application_id,),
    ).fetchone()
    if not last_non_completion_event or int(last_non_completion_event["id"]) != int(event_id):
        flash("Отметка выполнения доступна только для последнего этапа.", "error")
        return redirect(url_for("public.edit_application", application_id=application_id))

    new_stage_completed = 1 if request.form.get("stage_completed") == "1" else 0
    old_stage_completed = int(attempt["stage_completed"] or 0)
    if new_stage_completed == old_stage_completed:
        return redirect(url_for("public.edit_application", application_id=application_id))

    db.execute(
        "UPDATE application_attempts SET stage_completed = ?, updated_at = ? WHERE id = ?",
        (new_stage_completed, now_iso(), application_id),
    )
    add_history_event(
        db=db,
        attempt_id=application_id,
        event_type="completion",
        from_status=attempt["status"],
        to_status=attempt["status"],
        note="Этап отмечен как выполненный." if new_stage_completed == 1 else "Отметка выполнения этапа снята.",
    )
    db.commit()
    flash("Отметка выполнения обновлена.", "success")
    return redirect(url_for("public.edit_application", application_id=application_id))


@public_bp.route("/applications/<int:application_id>/events/<int:event_id>/comment", methods=["POST"])
def update_attempt_event_comment(application_id: int, event_id: int):
    db = get_db()
    row = db.execute(
        "SELECT id FROM application_attempts WHERE id = ?",
        (application_id,),
    ).fetchone()
    if not row:
        abort(404)

    event = db.execute(
        "SELECT id FROM application_history WHERE id = ? AND attempt_id = ?",
        (event_id, application_id),
    ).fetchone()
    if not event:
        abort(404)

    comment = (request.form.get("event_note") or "").strip() or None
    db.execute(
        "UPDATE application_history SET event_note = ? WHERE id = ? AND attempt_id = ?",
        (comment, event_id, application_id),
    )
    db.commit()
    flash("Комментарий к этапу обновлен.", "success")
    return redirect(url_for("public.edit_application", application_id=application_id))


@public_bp.route("/applications/<int:application_id>/events/<int:event_id>/delete", methods=["POST"])
def delete_attempt_event(application_id: int, event_id: int):
    db = get_db()
    attempt = db.execute(
        "SELECT id, status, marker_enabled FROM application_attempts WHERE id = ?",
        (application_id,),
    ).fetchone()
    if not attempt:
        abort(404)

    event = db.execute(
        "SELECT * FROM application_history WHERE id = ? AND attempt_id = ?",
        (event_id, application_id),
    ).fetchone()
    if not event:
        abort(404)
    if event["event_type"] == "create":
        flash("Нельзя удалить базовый этап создания подачи.", "error")
        return redirect(url_for("public.edit_application", application_id=application_id))

    last_event = db.execute(
        """
        SELECT id, event_type
        FROM application_history
        WHERE attempt_id = ?
        ORDER BY datetime(created_at) DESC, id DESC
        LIMIT 1
        """,
        (application_id,),
    ).fetchone()
    if not last_event or int(last_event["id"]) != int(event_id):
        flash("Можно удалить только последний этап в истории подачи.", "error")
        return redirect(url_for("public.edit_application", application_id=application_id))

    current_status = attempt["status"]
    affects_current_stage = (
        event["event_type"] == "move"
        and event["from_status"] is not None
        and event["to_status"] is not None
        and event["to_status"] == current_status
    )
    confirm_rollback = request.form.get("confirm_rollback") == "1"

    if affects_current_stage and not confirm_rollback:
        flash("Удаление этого этапа вернет подачу на предыдущую стадию. Подтвердите действие.", "error")
        return redirect(url_for("public.edit_application", application_id=application_id))

    if affects_current_stage:
        prev_status = event["from_status"]
        if prev_status in APPLICATION_STATUSES:
            if int(attempt["marker_enabled"] or 0) == 1:
                db.execute(
                    """
                    UPDATE application_attempts
                    SET status = ?, stage_completed = 0, updated_at = ?
                    WHERE id = ?
                    """,
                    (prev_status, now_iso(), application_id),
                )
            else:
                db.execute(
                    "UPDATE application_attempts SET status = ?, updated_at = ? WHERE id = ?",
                    (prev_status, now_iso(), application_id),
                )

    db.execute("DELETE FROM application_history WHERE id = ? AND attempt_id = ?", (event_id, application_id))
    db.commit()
    flash("Этап удален из истории.", "success")
    return redirect(url_for("public.edit_application", application_id=application_id))
