import os
import re
from datetime import datetime

from flask import Flask, render_template, session

from app.config import load_settings
from app.constants import (
    APPLICATION_STATUS_LABELS,
    APPLICATION_STATUSES,
    DIRECTIONS,
    STATUS_LABELS,
    STATUSES,
    WORK_FORMAT_LABELS,
    WORK_FORMATS,
)
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


def normalize_hex_color(value, default="#0e7490"):
    candidate = (value or "").strip().lower()
    if not candidate:
        return default
    if not candidate.startswith("#"):
        candidate = f"#{candidate}"
    if re.fullmatch(r"#[0-9a-f]{6}", candidate):
        return candidate
    return default


def hex_to_rgb(hex_color: str):
    color = normalize_hex_color(hex_color)
    return int(color[1:3], 16), int(color[3:5], 16), int(color[5:7], 16)


def company_card_style(accent_color):
    r, g, b = hex_to_rgb(accent_color)
    normalized = normalize_hex_color(accent_color)
    return (
        f"--company-accent:{normalized};"
        f"--company-soft-bg:rgba({r},{g},{b},0.12);"
        f"--company-border:rgba({r},{g},{b},0.34);"
        f"--company-hover:rgba({r},{g},{b},0.2);"
        f"--company-glow:rgba({r},{g},{b},0.28);"
    )


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
            "application_statuses": APPLICATION_STATUSES,
            "application_status_labels": APPLICATION_STATUS_LABELS,
            "is_admin": bool(session.get("admin_auth")),
            "company_card_style": company_card_style,
            "normalize_hex_color": normalize_hex_color,
        }

    @app.template_filter("display_date")
    def display_date(value):
        return format_dt(value)

    @app.errorhandler(404)
    def not_found(_error):
        return render_template("404.html"), 404

    return app
