import os
from dataclasses import dataclass

from dotenv import load_dotenv


@dataclass(frozen=True)
class Settings:
    database_url: str
    secret_key: str
    admin_username: str | None
    admin_password: str | None
    openrouter_api_key: str | None
    openrouter_base_url: str
    openrouter_model: str


class ConfigError(Exception):
    pass


def load_settings() -> Settings:
    load_dotenv()
    load_dotenv(".env.local", override=True)

    database_url = os.getenv("DATABASE_URL", "sqlite:///internships.db")

    return Settings(
        database_url=database_url,
        secret_key=os.getenv("SECRET_KEY", "dev-secret-change-me"),
        admin_username=os.getenv("ADMIN_USERNAME"),
        admin_password=os.getenv("ADMIN_PASSWORD"),
        openrouter_api_key=os.getenv("OPENROUTER_API_KEY"),
        openrouter_base_url=os.getenv("OPENROUTER_BASE_URL", "https://openrouter.ai/api/v1"),
        openrouter_model=os.getenv("OPENROUTER_MODEL", "deepseek/deepseek-v3.2"),
    )


def sqlite_path_from_url(database_url: str, base_dir: str) -> str:
    if database_url.startswith("sqlite:///"):
        db_part = database_url.replace("sqlite:///", "", 1)
    else:
        db_part = database_url

    if not db_part:
        raise ConfigError("DATABASE_URL пустой. Укажите sqlite:///internships.db или путь к файлу.")

    if os.path.isabs(db_part):
        return db_part
    return os.path.join(base_dir, db_part)
