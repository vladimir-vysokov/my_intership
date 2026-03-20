from flask import current_app

from app.config import ConfigError


class OpenRouterService:
    def __init__(self):
        settings = current_app.config["SETTINGS"]
        self.api_key = settings.openrouter_api_key
        self.base_url = settings.openrouter_base_url.rstrip("/")
        self.model = settings.openrouter_model

    def _ensure_configured(self):
        if not self.api_key:
            raise ConfigError("OPENROUTER_API_KEY не задан. Добавьте ключ в .env или .env.local")

    def extract_internship_json(self, source_url: str, cleaned_text: str):
        self._ensure_configured()
        raise RuntimeError("Интеграция с нейросетью временно отключена.")
