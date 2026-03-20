import csv
import io

from app.db import get_db, now_iso


def parse_urls_from_uploaded_file(file_storage) -> list[str]:
    filename = (file_storage.filename or "").lower()
    content = file_storage.read().decode("utf-8", errors="ignore")

    urls: list[str] = []
    if filename.endswith(".csv"):
        reader = csv.reader(io.StringIO(content))
        for row in reader:
            if not row:
                continue
            candidate = row[0].strip()
            if candidate:
                urls.append(candidate)
    else:
        for line in content.splitlines():
            candidate = line.strip()
            if candidate:
                urls.append(candidate)

    seen = set()
    deduped = []
    for url in urls:
        if url not in seen:
            deduped.append(url)
            seen.add(url)
    return deduped


def create_import_job(file_name: str, total_urls: int) -> int:
    db = get_db()
    cur = db.execute(
        """
        INSERT INTO import_jobs (started_at, file_name, total_urls, status)
        VALUES (?, ?, ?, 'running')
        """,
        (now_iso(), file_name, total_urls),
    )
    db.commit()
    return cur.lastrowid


def add_import_item(import_job_id: int, url: str, status: str, message: str | None = None):
    db = get_db()
    db.execute(
        """
        INSERT INTO import_job_items (import_job_id, url, status, message, internship_id)
        VALUES (?, ?, ?, ?, NULL)
        """,
        (import_job_id, url, status, message),
    )
    db.commit()


def finalize_import_job(import_job_id: int, summary: dict[str, int], status: str, message: str):
    db = get_db()
    db.execute(
        """
        UPDATE import_jobs
        SET finished_at = ?, success_count = ?, duplicate_count = ?, skipped_count = ?,
            error_count = ?, status = ?, message = ?
        WHERE id = ?
        """,
        (
            now_iso(),
            summary["success"],
            summary["duplicate"],
            summary["skipped"],
            summary["error"],
            status,
            message,
            import_job_id,
        ),
    )
    db.commit()


def run_import(file_storage) -> int:
    urls = parse_urls_from_uploaded_file(file_storage)
    import_job_id = create_import_job(file_storage.filename or "links.txt", len(urls))

    summary = {"success": 0, "duplicate": 0, "skipped": 0, "error": 0}

    for url in urls:
        add_import_item(
            import_job_id,
            url,
            "skipped",
            "ИИ-скрапер временно недоступен. Импорт по ссылкам отключен.",
        )
        summary["skipped"] += 1

    final_message = (
        "ИИ-скрапер временно недоступен. "
        f"Пропущено ссылок: {summary['skipped']} из {len(urls)}."
    )
    finalize_import_job(import_job_id, summary, "disabled", final_message)
    return import_job_id
