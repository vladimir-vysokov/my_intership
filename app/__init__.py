import os
from datetime import datetime

from flask import Flask, render_template, session

from app.config import load_settings
from app.constants import DIRECTIONS, STATUS_LABELS, STATUSES, WORK_FORMAT_LABELS, WORK_FORMATS
from app.db import close_db, init_db
from app.routes import admin_bp, public_bp


def format_dt(value):
    if not value:
        return ""

    normalized = str(value).replace("Z", "+00:00")
    dt = None

    for fmt in ["%Y-%m-%d", "%Y-%m-%d %H:%M:%S"]:
        try:
            dt = datetime.strptime(str(value), fmt)
            break
        except ValueError:
            continue

    if dt is None:
        try:
            dt = datetime.fromisoformat(normalized)
        except ValueError:
            return str(value)

    return dt.strftime("%d.%m.%Y")


def create_app() -> Flask:
    base_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    settings = load_settings()

    app = Flask(
        __name__,
        template_folder="templates",
        static_folder="static",
    )
    app.config["SETTINGS"] = settings
    app.config["BASE_DIR"] = base_dir
    app.config["SECRET_KEY"] = settings.secret_key

    app.register_blueprint(public_bp)
    app.register_blueprint(admin_bp)

    app.teardown_appcontext(close_db)
    init_db(app)

    @app.context_processor
    def inject_ui_values():
        return {
            "work_format_labels": WORK_FORMAT_LABELS,
            "status_labels": STATUS_LABELS,
            "directions": DIRECTIONS,
            "statuses": STATUSES,
            "work_formats": WORK_FORMATS,
            "is_admin": bool(session.get("admin_auth")),
        }

    @app.template_filter("display_date")
    def display_date(value):
        return format_dt(value)

    @app.errorhandler(404)
    def not_found(_error):
        return render_template("404.html"), 404

    return app
