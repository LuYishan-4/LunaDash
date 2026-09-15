"""Validate relative assets and fragments without network requests or dependencies."""

import sys
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import urlsplit

root = (
    Path(sys.argv[1])
    if len(sys.argv) > 1
    else Path(__file__).resolve().parents[2] / "site/dist"
).resolve()


class Page(HTMLParser):
    def __init__(self):
        super().__init__()
        self.ids = set()
        self.links = []
        self.english = False

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if tag == "html":
            self.english = attrs.get("lang") == "en"
        if "id" in attrs:
            assert attrs["id"] not in self.ids, "Duplicate HTML ID"
            self.ids.add(attrs["id"])
        if tag == "img":
            assert "alt" in attrs, "Image is missing alternative text"
        for key in ("href", "src"):
            if key in attrs:
                self.links.append(attrs[key])


pages = {}
for path in root.rglob("*.html"):
    page = Page()
    page.feed(path.read_text())
    assert page.english, path
    pages[path.resolve()] = page
for path, page in pages.items():
    for link in page.links:
        url = urlsplit(link)
        if url.scheme or url.netloc:
            assert url.scheme == "https", link
            continue
        target = path
        if url.path:
            target = (
                (root / url.path.removeprefix("/LuDash/").lstrip("/"))
                if url.path.startswith("/")
                else path.parent / url.path
            ).resolve()
            if target.is_dir():
                target /= "index.html"
            assert target.is_relative_to(root) and target.is_file(), (path, link)
        if url.fragment and target in pages:
            assert url.fragment in pages[target].ids, (path, link)
assert len(pages) >= 5, "Missing documentation routes"
banner = root / "assets/banner.svg"
assert banner.is_file() and banner.stat().st_size > 1000, "Missing website banner"
readme_banner = Path(__file__).resolve().parents[2] / "docs/brand/banner.svg"
assert banner.read_bytes() == readme_banner.read_bytes(), (
    "Website banner differs from docs/brand/banner.svg"
)
print(
    f"Website checks passed: {len(pages)} English pages, assets, internal links, fragments and image descriptions."
)
