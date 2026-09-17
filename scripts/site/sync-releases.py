#!/usr/bin/env python3
"""Generate Astro release-note pages from GitHub Releases.

GitHub Release bodies remain the source of truth. This script is intended for the
Pages deployment workflow and writes only generated files inside site/src.
"""
from __future__ import annotations

import json
import os
from pathlib import Path
import re
import shutil
import sys
import urllib.request

ROOT = Path(__file__).resolve().parents[2]
SITE = ROOT / "site" / "src"
PAGES = SITE / "pages" / "releases"
DATA = SITE / "data" / "releases.json"
REPOSITORY = os.environ.get("GITHUB_REPOSITORY", "LuYishan-4/LunaDash")
TOKEN = os.environ.get("GITHUB_TOKEN", "")


def request_json(url: str):
    headers = {
        "Accept": "application/vnd.github+json",
        "User-Agent": "LunaDash-site-release-sync",
        "X-GitHub-Api-Version": "2022-11-28",
    }
    if TOKEN:
        headers["Authorization"] = f"Bearer {TOKEN}"
    request = urllib.request.Request(url, headers=headers)
    with urllib.request.urlopen(request, timeout=30) as response:
        return json.load(response)


def release_slug(tag: str) -> str:
    slug = re.sub(r"[^A-Za-z0-9._-]+", "-", tag).strip("-.")
    return slug or "release"


def frontmatter_string(value: str) -> str:
    return json.dumps(value, ensure_ascii=False)


def fetch_releases() -> list[dict]:
    releases: list[dict] = []
    page = 1
    while True:
        batch = request_json(
            f"https://api.github.com/repos/{REPOSITORY}/releases?per_page=100&page={page}"
        )
        if not isinstance(batch, list):
            raise RuntimeError("GitHub releases response was not an array")
        releases.extend(release for release in batch if not release.get("draft"))
        if len(batch) < 100:
            break
        page += 1
    return releases


def main() -> int:
    releases = fetch_releases()
    PAGES.mkdir(parents=True, exist_ok=True)

    for path in PAGES.glob("*.md"):
        path.unlink()

    summary = []
    used_slugs: set[str] = set()
    for release in releases:
        tag = str(release.get("tag_name") or "release")
        slug = release_slug(tag)
        if slug in used_slugs:
            suffix = str(release.get("id") or len(used_slugs))
            slug = f"{slug}-{suffix}"
        used_slugs.add(slug)

        title = str(release.get("name") or tag)
        body = str(release.get("body") or "No release notes were provided.")
        published = str(release.get("published_at") or release.get("created_at") or "")
        url = str(release.get("html_url") or f"https://github.com/{REPOSITORY}/releases/tag/{tag}")
        prerelease = bool(release.get("prerelease"))

        markdown = "\n".join(
            [
                "---",
                "layout: ../../layouts/ReleaseLayout.astro",
                f"title: {frontmatter_string(title)}",
                f"tag: {frontmatter_string(tag)}",
                f"publishedAt: {frontmatter_string(published)}",
                f"releaseUrl: {frontmatter_string(url)}",
                f"prerelease: {'true' if prerelease else 'false'}",
                "---",
                "",
                body.rstrip(),
                "",
            ]
        )
        (PAGES / f"{slug}.md").write_text(markdown, encoding="utf-8")
        summary.append(
            {
                "title": title,
                "tag": tag,
                "slug": slug,
                "publishedAt": published,
                "releaseUrl": url,
                "prerelease": prerelease,
            }
        )

    DATA.parent.mkdir(parents=True, exist_ok=True)
    DATA.write_text(json.dumps(summary, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Generated {len(summary)} release note page(s) from {REPOSITORY}.")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as error:
        print(f"release sync failed: {error}", file=sys.stderr)
        raise SystemExit(1)
