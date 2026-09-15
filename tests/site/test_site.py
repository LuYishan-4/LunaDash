"""Validate relative assets and fragments without network requests or dependencies."""

import re
import sys
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import urlsplit

repository_root = Path(__file__).resolve().parents[2]
# Absolute links in the built pages start with the configured Astro base, which
# is the repository name GitHub Pages serves the project site under.
_base_match = re.search(
    r'base:\s*"([^"]*)"', (repository_root / "site/astro.config.mjs").read_text()
)
_base_slug = _base_match.group(1).strip("/") if _base_match else ""
base_prefix = f"/{_base_slug}/" if _base_slug else "/"

root = (
    Path(sys.argv[1]) if len(sys.argv) > 1 else repository_root / "site/dist"
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
            if url.path.startswith("/"):
                assert url.path == base_prefix.rstrip("/") or url.path.startswith(
                    base_prefix
                ), (path, link)
                relative = url.path[len(base_prefix) :]
                target = (root / relative).resolve()
            else:
                target = (path.parent / url.path).resolve()
            if target.is_dir():
                target /= "index.html"
            assert target.is_relative_to(root) and target.is_file(), (path, link)
        if url.fragment and target in pages:
            assert url.fragment in pages[target].ids, (path, link)
assert len(pages) >= 5, "Missing documentation routes"
banner = root / "assets/banner.svg"
assert banner.is_file() and banner.stat().st_size > 1000, "Missing website banner"
readme_banner = repository_root / "docs/brand/banner.svg"
assert banner.read_bytes() == readme_banner.read_bytes(), (
    "Website banner differs from docs/brand/banner.svg"
)
print(
    f"Website checks passed: {len(pages)} English pages, assets, internal links, fragments and image descriptions."
)
