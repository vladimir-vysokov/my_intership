import re

from bs4 import BeautifulSoup


def clean_whitespace(text: str) -> str:
    text = re.sub(r"\s+", " ", text)
    return text.strip()


def html_to_clean_text(html: str) -> str:
    soup = BeautifulSoup(html, "html.parser")

    for tag_name in ["script", "style", "noscript", "svg", "iframe", "footer", "header"]:
        for tag in soup.find_all(tag_name):
            tag.decompose()

    main = soup.find("main") or soup.find("article") or soup.body or soup
    text = main.get_text(" ", strip=True)
    return clean_whitespace(text)
