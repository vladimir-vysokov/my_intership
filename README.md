# InternStart C++

InternStart теперь живет как C++17 backend: публичный каталог стажировок, компании, админка и трекер подач работают без Flask/Python.

## Что внутри

- `cpp_backend/include` — заголовки и публичные интерфейсы классов.
- `cpp_backend/src` — реализации backend-слоев.
- `cpp_backend/static/styles.css` — стили приложения.
- `cpp_backend/CMakeLists.txt` — основная сборка через CMake.
- `cpp_backend/Makefile` — запасной путь сборки, пока CMake не установлен.

## Архитектура

Код разнесен по слоям, чтобы архитектура читалась из реальных классов и их взаимодействий:

- `Application` — загрузка env, настройка БД и запуск HTTP-сервера.
- `HttpServer` — socket-based HTTP server.
- `Router` — сопоставляет URL с контроллерами.
- `PublicController` — публичный каталог, компании, детали стажировок.
- `ApplicationController` — трекер подач.
- `AdminController` — логин и управление данными.
- `Database` — обертка над SQLite.
- `CompanyRepository`, `InternshipRepository`, `ApplicationRepository` — доступ к данным.
- `enum class` в `domain.hpp` — типизированные статусы и форматы.

Идеи из учебных материалов по информатике применены прямо в коде: инкапсуляция, конструкторы/деструкторы, структуры данных и перечисления.

## Запуск

Пока CMake не установлен:

```bash
cd cpp_backend
make
PORT=5050 ./internstart_cpp
```

После установки CMake:

```bash
cmake -S cpp_backend -B build
cmake --build build
PORT=5050 ./build/internstart_cpp
```

Админка: `http://127.0.0.1:5050/admin/login`

Для выкладки на сервер смотрите [DEPLOYMENT.md](DEPLOYMENT.md).

## Переменные окружения

```env
ADMIN_USERNAME=admin
ADMIN_PASSWORD=change_me
DATABASE_URL=sqlite:///internships.db
```

`.env` и `.env.local` читаются из корня репозитория.

## Что удалено

- Flask/Python backend.
- Jinja templates.
- `requirements.txt`.
- API/парсеры внешних импортов.

Импорт можно вернуть позже отдельным C++ сервисным слоем.
