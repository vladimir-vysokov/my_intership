# Деплой InternStart

Проект собирается в один бинарник `internstart_cpp` и запускается как один веб-сервис.

Публичная часть:

```text
https://example.com
```

Админ-панель:

```text
https://example.com/admin
```

Если администратор не вошел, `/admin` отправит на `/admin/login`. После входа `/admin` ведет в рабочую часть панели.

## Сборка

```bash
cd /opt/internstart/cpp_backend
make
```

## Переменные окружения

```env
HOST=127.0.0.1
PORT=5050
DATABASE_URL=sqlite:////opt/internstart/data/internships.db
ADMIN_USERNAME=admin
ADMIN_PASSWORD=change_me
```

Если сервис должен слушать внешний интерфейс без nginx, поставьте `HOST=0.0.0.0`. Для обычной выкладки за nginx безопаснее оставить `HOST=127.0.0.1`.

## systemd

```ini
[Unit]
Description=InternStart web
After=network.target

[Service]
WorkingDirectory=/opt/internstart/cpp_backend
ExecStart=/opt/internstart/cpp_backend/internstart_cpp
Environment=HOST=127.0.0.1
Environment=PORT=5050
Environment=DATABASE_URL=sqlite:////opt/internstart/data/internships.db
EnvironmentFile=-/opt/internstart/.env
Restart=always
RestartSec=3

[Install]
WantedBy=multi-user.target
```

## nginx

```nginx
server {
    server_name example.com;

    location / {
        proxy_pass http://127.0.0.1:5050;
        proxy_set_header Host $host;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
    }
}
```

Health-check:

```text
/healthz
```
