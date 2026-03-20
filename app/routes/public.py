from flask import Blueprint, abort, render_template, request

from app.constants import DIRECTIONS, STATUSES, WORK_FORMATS
from app.db import get_db

public_bp = Blueprint("public", __name__)


@public_bp.route("/")
def home():
    db = get_db()
    latest = db.execute(
        """
        SELECT * FROM internships
        WHERE is_published = 1
        ORDER BY datetime(created_at) DESC
        LIMIT 8
        """
    ).fetchall()
    return render_template("public/home.html", latest=latest)


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

    where_clauses = ["is_published = 1"]
    params = []

    if q:
        where_clauses.append(
            "(title LIKE ? OR company_name LIKE ? OR direction LIKE ? OR city LIKE ? OR short_description LIKE ? OR full_description LIKE ?)"
        )
        pattern = f"%{q}%"
        params.extend([pattern] * 6)

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

    cities_rows = db.execute(
        "SELECT DISTINCT city FROM internships WHERE is_published = 1 ORDER BY city"
    ).fetchall()

    internships = db.execute(
        f"""
        SELECT * FROM internships
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
        """
        SELECT * FROM internships
        WHERE id = ? AND is_published = 1
        """,
        (internship_id,),
    ).fetchone()

    if not internship:
        abort(404)

    return render_template("public/detail.html", internship=internship)
