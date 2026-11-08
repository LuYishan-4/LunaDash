<!--
WARNING: Every contribution PR must target the `dev` branch. PRs opened against `main`
or any other branch fail the PR policy gate and will not be reviewed until they are
retargeted to `dev`. Read CONTRIBUTING.md before opening a PR.

Title: the title is the patch name and must use the format
  [Category] Specific summary of the change
with one tag for each change type checked below, in the order [Source][Script][Other], e.g.
  [Source] Fix compositor hang when parsing audio device names
  [Script] Report missing packages in install-dependencies.sh
  [Source][Other] Add video wallpapers and document the setting
Titles such as "Dev", "Update" or "Fix bugs" are not accepted.
-->

#### What does this PR change? Check every type the diff contains, across all commits. (If the boxes or the title tags don't match the actual commits, you will be asked to correct them before review continues.)

- [ ] Source: `src/` (C++, C, Slint, shaders), `protocols/`, `CMakeLists.txt`, `cmake/`
- [ ] Script: `scripts/`, `install.sh`
- [ ] Other: `docs/`, `README.md`, `AGENTS.md`, `data/`, `packaging/`, `nix/`, `flake.nix`, `flake.lock`, `.github/` or tool configuration. Files:


#### Describe your PR, what does it fix/add? (What triggers the problem, what happened before, what happens after the change, and how it affects the defaults or optional settings, if relevant.)


#### How did you test it? (CI only builds. List the commands you actually ran with their results, and the platforms or hardware covered. Report failed or skipped checks. For visible UI changes, attach before/after screenshots from a real session.)


#### Is there anything you want to mention? (unverified behavior, new dependencies, configuration changes, updated `shell-revision` or `slintlu-revision` pins, whether users must log in again or restart, security or wlroots/Wayland lifetime concerns, etc.)


#### Did you use AI in any way, shape or form in the making of this PR? Check one. If yes, name the tools, what they generated or changed, and how you checked it. (Undisclosed or inaccurate AI use must be corrected, and deliberate misreporting can get the PR closed. See "AI assistance disclosure" in CONTRIBUTING.md.)

- [ ] No
- [ ] Yes:


#### Is it ready for merging, or does it need work?
