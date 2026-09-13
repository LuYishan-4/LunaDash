"""Validate relative assets and fragments without network requests or dependencies."""
from html.parser import HTMLParser
from pathlib import Path
import sys
from urllib.parse import urlsplit

root = (Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[2] / 'site/dist').resolve()


class Page(HTMLParser):
    def __init__(self):
        super().__init__()
        self.ids = set()
        self.links = []
        self.english = False

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if tag == 'html':
            self.english = attrs.get('lang') == 'en'
        if 'id' in attrs:
            assert attrs['id'] not in self.ids, 'Duplicate HTML ID'
            self.ids.add(attrs['id'])
        if tag == 'img':
            assert 'alt' in attrs, 'Image is missing alternative text'
        for key in ('href', 'src'):
            if key in attrs:
                self.links.append(attrs[key])


page = Page()
page.feed((root / 'index.html').read_text())
assert page.english, 'The website must declare English'
for link in page.links:
    url = urlsplit(link)
    if url.scheme or url.netloc:
        assert url.scheme == 'https', link
    elif url.path:
        target = (root / url.path.removeprefix("/LuDash/").lstrip("/")).resolve()
        assert target.is_relative_to(root) and target.is_file(), link
    elif url.fragment:
        assert url.fragment in page.ids, link
assert (root / 'assets/desktop.png').stat().st_size > 10000, 'Missing desktop preview'
print('Website checks passed: English document, relative assets, fragment links and image descriptions.')
