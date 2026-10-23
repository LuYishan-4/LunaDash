# LunaDash documentation

Technical documentation is organized by language.

- [English](en/README.md)
- [Traditional Chinese](zh/README.md)

Shared screenshots and branding remain in `docs/image/` and `docs/brand/`. README translations remain in `docs/readme/`.

The language directories are the canonical locations for new documentation links. The website source lives only in LunaDash-docs; the QML shell source lives only in LunaDash-Shell. LunaDash records the selected Shell commit in `packaging/shell-revision` and does not contain either component's source. The older Markdown paths directly under `docs/` remain small compatibility forwarding pages so existing external links continue to work. The canonical document text lives only in the language directories.

## Current development documentation

The documentation tracks the `dev` branch and current **1.0.1a** behavior, including the wlroots compositor, redesigned Dashboard/Settings surfaces, LunaDash-styled portal pickers, bounded tiling and optional stacking layout, Plugin SDK 2, wallpaper-layer desktop widgets, media/MPRIS support, DDC/CI display controls, region capture, and on-demand XWayland helpers.

When behavior changes, update the matching English and Traditional Chinese document together. Website changes are committed to LunaDash-docs and run through that repository's CI; the root Pages workflow fetches its current `main` branch at deployment time.
