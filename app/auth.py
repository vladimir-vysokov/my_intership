import hmac
from functools import wraps

from flask import current_app, flash, redirect, request, session, url_for


def admin_config_ready() -> bool:
    settings = current_app.config["SETTINGS"]
    return bool(settings.admin_username and settings.admin_password)


def verify_admin_credentials(username: str, password: str) -> bool:
    settings = current_app.config["SETTINGS"]
    if not admin_config_ready():
        return False
    user_ok = hmac.compare_digest(username, settings.admin_username)
    pass_ok = hmac.compare_digest(password, settings.admin_password)
    return user_ok and pass_ok


def admin_login_required(view):
    @wraps(view)
    def wrapped(*args, **kwargs):
        if "admin_auth" not in session:
            return redirect(url_for("admin.login", next=request.path))
        if not admin_config_ready():
            flash("Админ-логин не настроен. Укажите ADMIN_USERNAME и ADMIN_PASSWORD в .env.", "error")
            return redirect(url_for("admin.login"))
        return view(*args, **kwargs)

    return wrapped
