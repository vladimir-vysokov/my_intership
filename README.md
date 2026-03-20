# InternStart v2 MVP

Вторая итерация MVP: публичный сайт и админ-панель логически разделены, добавлен AI-импорт стажировок по списку URL через OpenRouter.

## Архитектура

Один backend на Flask, но разделение на зоны:

- `public-site` (blueprint `public`)
  - только публичные страницы
  - показывает все `is_published=1` (включая AI-записи с пометкой)
- `admin-panel` (blueprint `admin`)
  - авторизация по `ADMIN_USERNAME` / `ADMIN_PASSWORD` из env
  - CRUD стажировок, модерация AI-записей, импорт ссылок
- `shared backend/database`
  - единая SQLite БД
  - единые сервисы `db`, `openrouter_service`, `import_service`

## Новые ключевые возможности

- Разделение public/admin в отдельных роутингах и шаблонах.
- Безопасный env-конфиг через `.env`/`.env.local`.
- OpenRouter вызывается только на сервере.
- Импорт `.txt`/`.csv` файла со ссылками в админке.
- Pipeline импорта:
  1. чтение списка URL
  2. загрузка HTML страницы
  3. очистка/извлечение текста
  4. AI-структурирование в JSON
  5. валидация и нормализация
  6. дедупликация
  7. сохранение как AI-записи с немедленной публикацией (`needs_review=1`, `is_published=1`)
  8. логирование по каждой ссылке
- Журнал импортов: `import_jobs`, `import_job_items`.
- Маркировка AI-записей: бейдж `Найдено ИИ` + дата AI-генерации.
- Ручная модерация AI-карточек (подтвердить и опубликовать).

## Структура проекта

- `main.py` — точка входа
- `app/__init__.py` — app factory, регистрация blueprints
- `app/config.py` — загрузка env и settings
- `app/db.py` — БД, миграции, инициализация схемы
- `app/auth.py` — auth для админки
- `app/routes/public.py` — публичные роуты
- `app/routes/admin.py` — админские роуты
- `app/services/openrouter_service.py` — серверный OpenRouter-клиент
- `app/services/content_extractor.py` — очистка HTML
- `app/services/import_service.py` — импорт pipeline + логи + дедуп
- `app/templates/public/*`, `app/templates/admin/*`
- `app/static/styles.css`

## Переменные окружения

Обязательные:

- `ADMIN_USERNAME`
- `ADMIN_PASSWORD`
- `OPENROUTER_API_KEY`

Рекомендуемые:

- `OPENROUTER_BASE_URL=https://openrouter.ai/api/v1`
- `OPENROUTER_MODEL=deepseek/deepseek-v3.2`
- `DATABASE_URL=sqlite:///internships.db`
- `SECRET_KEY=...`

Пример лежит в `.env.example`.

## Куда вставить ключ OpenRouter

Создайте `.env` (или `.env.local`) в корне проекта и добавьте:

```env
OPENROUTER_API_KEY=ваш_ключ
OPENROUTER_BASE_URL=https://openrouter.ai/api/v1
OPENROUTER_MODEL=deepseek/deepseek-v3.2
ADMIN_USERNAME=ваш_логин
ADMIN_PASSWORD=ваш_пароль
SECRET_KEY=длинный_случайный_секрет
DATABASE_URL=sqlite:///internships.db
```

## Запуск

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
python3 main.py
```

- Public: `http://127.0.0.1:5000/`
- Admin login: `http://127.0.0.1:5000/admin/login`

## Как работает импорт ссылок

1. В админке откройте `Импорт`.
2. Загрузите `.txt`/`.csv` файл (1 строка = 1 URL).
3. Если URL похож на страницу списка (например careers/jobs), система сначала пытается извлечь ссылки на отдельные вакансии и пройти по ним.
4. Для каждой найденной вакансии система пытается создать AI-запись и сразу публикует ее с пометкой.
5. Результат отображается как сводка и построчный лог (`expanded/success/duplicate/skipped/error`).
6. Созданные AI-записи появятся в админке как `Требуют проверки`.
7. После ручной проверки нажмите `Подтвердить` (снимается флаг `needs_review`).

## Примечание по отказоустойчивости

Если отдельные ссылки не обработались, импорт не падает полностью: ошибки пишутся в журнал и обработка продолжается для следующих URL.
