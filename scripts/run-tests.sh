#!/usr/bin/env bash
# Runs the CI suites locally, safely: every compositor runs headless in its own runtime
# directory and on a private D-Bus, nothing reaches the running desktop.
set -uo pipefail

project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
build_dir=${LUNADASH_TEST_BUILD_DIR:-"$project_dir/build-tests"}
jobs=${LUNADASH_TEST_JOBS:-$(nproc 2>/dev/null || echo 2)}
build=true
suites=()

usage() {
    cat <<'EOF'
Usage: scripts/run-tests.sh [--build-dir DIR] [--no-build] [--list] [SUITE...]

Suites (default: contracts unit runtime):
  contracts  repository, source, packaging and translation checks (no build needed)
  unit       CTest
  runtime    headless Wayland sessions: lifecycle, setup, Slint shell, session.json,
             plugin SDK
  installer  installer control flow with fake package tools
  all        every suite above

The build goes to build-tests/ (LUNADASH_TEST_BUILD_DIR), never to build/. The pinned
LunaDash-Shell and SlintLu are fetched into it unless LUDASH_SHELL_SOURCE_DIR and
LUDASH_SLINTLU_SOURCE_DIR name checkouts. Logs: <build dir>/test-logs/.
EOF
}

while (($#)); do
    case "$1" in
        --build-dir) (($# >= 2)) || { usage >&2; exit 2; }; build_dir=$2; shift ;;
        --no-build) build=false ;;
        --list) usage | sed -n '/^Suites/,/^$/p'; exit 0 ;;
        --help|-h) usage; exit 0 ;;
        contracts|unit|runtime|installer) suites+=("$1") ;;
        all) suites+=(contracts unit runtime installer) ;;
        *) printf 'Unknown option or suite: %s\n' "$1" >&2; usage >&2; exit 2 ;;
    esac
    shift
done
((${#suites[@]})) || suites=(contracts unit runtime)
build_dir=$(mkdir -p -- "$build_dir" && cd -- "$build_dir" && pwd)
logs=$build_dir/test-logs
mkdir -p -- "$logs"

results=()
failed=0
check() { # NAME COMMAND...: run one check, keep its output in a log, record the result
    local name=$1 log
    shift
    log=$logs/$(printf '%s' "$name" | tr -c 'A-Za-z0-9._-' '_').log
    printf '  %-44s ' "$name"
    if "$@" >"$log" 2>&1; then
        printf 'ok\n'
        results+=("ok    $name")
    else
        printf 'FAILED  (%s)\n' "$log"
        results+=("FAIL  $name  $log")
        failed=1
    fi
}

# Pinned checkouts, fetched once into the build directory.
pinned() { # VARIABLE SCRIPT DIRECTORY MARKER
    local variable=$1 script=$2 directory=$build_dir/_deps/$3 marker=$4
    if [[ -n ${!variable:-} ]]; then
        printf '%s' "${!variable}"
        return
    fi
    if [[ ! -e $directory/$marker ]]; then
        mkdir -p -- "$(dirname -- "$directory")"
        bash "$project_dir/scripts/$script" "$directory" >&2 || return 1
    fi
    printf '%s' "$directory"
}
shell_source=$(pinned LUDASH_SHELL_SOURCE_DIR fetch-shell.sh LunaDash-Shell shell.slint) || {
    echo 'Could not fetch the pinned LunaDash-Shell.' >&2; exit 1; }
slintlu_source=$(pinned LUDASH_SLINTLU_SOURCE_DIR fetch-slintlu.sh SlintLu Cargo.toml) || {
    echo 'Could not fetch the pinned SlintLu.' >&2; exit 1; }
export LUDASH_SHELL_SOURCE_DIR=$shell_source LUDASH_SLINTLU_SOURCE_DIR=$slintlu_source

# Runtime tests never see the desktop they are started from.
isolated() {
    env -u WAYLAND_DISPLAY -u DISPLAY -u XDG_SESSION_ID -u NIRI_SOCKET -u SWAYSOCK \
        -u HYPRLAND_INSTANCE_SIGNATURE -u LUNADASH_CONTROL \
        SLINTLU_PAM_SERVICE=slintlu-test-nonexistent-service LUNADASH_DISABLE_FCITX=1 \
        dbus-run-session -- "$@"
}

needs_build=false
for suite in "${suites[@]}"; do
    [[ $suite == unit || $suite == runtime ]] && needs_build=true
done
if $needs_build && $build; then
    printf 'Building in %s\n' "$build_dir"
    cmake -S "$project_dir" -B "$build_dir" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DLUDASH_SHELL_SOURCE_DIR="$shell_source" -DLUDASH_SLINTLU_SOURCE_DIR="$slintlu_source" \
        >"$logs/configure.log" 2>&1 || { echo "Configure failed: $logs/configure.log" >&2; exit 1; }
    cmake --build "$build_dir" --parallel "$jobs" >"$logs/build.log" 2>&1 || {
        tail -n 30 "$logs/build.log" >&2
        echo "Build failed: $logs/build.log" >&2
        exit 1
    }
fi

cd -- "$project_dir"
for suite in "${suites[@]}"; do
    printf '\n%s\n' "$suite"
    case "$suite" in
        contracts)
            for test in tests/ci/test_changes.py tests/ci/test_pr_policy.py \
                scripts/check-source-layout.py tests/architecture/test_source_layout.py \
                tests/architecture/test_window_template_contract.py \
                tests/security/test_source_language.py \
                tests/renderer/test_render_architecture.py tests/renderer/test_shader_extensions.py \
                tests/wayland/test_no_qtwayland.py \
                tests/wayland/test_graphics_protocol_contract.py tests/wayland/test_input_contract.py \
                scripts/ci/check_shell_actions.py scripts/check-translations.py \
                scripts/test-translation-checker.py tests/packaging/test_make_source.py \
                tests/packaging/test_nix_update.py tests/update/test_update_contract.py \
                tests/settings/test_schema.py; do
                check "$test" python3 "$test"
            done
            for mode in workflows secrets paths; do
                check "repository hygiene: $mode" python3 scripts/security/check_repository_hygiene.py "$mode"
            done
            check "icons are current" python3 scripts/gen-ui-icons.py --check
            if command -v shellcheck >/dev/null 2>&1; then
                check "shellcheck" bash -c 'git ls-files -z scripts install.sh | while IFS= read -r -d "" f; do
                    case "$(head -n 1 "$f")" in *bash*|*/sh) shellcheck --severity=error "$f" || exit 1 ;; esac
                done'
            else
                results+=("skip  shellcheck (not installed)")
            fi
            ;;
        unit)
            check "ctest" ctest --test-dir "$build_dir" --output-on-failure --timeout 120 -j "$jobs"
            ;;
        runtime)
            check "xdg lifecycle" isolated ./scripts/test-xdg-lifecycle.sh "$build_dir"
            check "wayland session" isolated env LUDASH_TEST_NO_SHELL=1 \
                LUDASH_BUILD_DIR="$build_dir" ./scripts/test-wayland.sh
            check "welcome and setup" isolated python3 tests/wayland/test_setup.py "$build_dir"
            check "slint shell session" isolated python3 tests/wayland/test_session_shell.py "$build_dir"
            check "session.json shells" isolated python3 tests/wayland/test_session_config.py "$build_dir"
            check "plugin sdk" python3 tests/plugins/test_sdk.py --sdk "$build_dir/sdk-build"
            ;;
        installer)
            check "install-session.sh" python3 tests/update/test_installer.py
            check "setup profile" python3 tests/update/test_setup_profile.py
            check "install.sh" python3 tests/installer/test_install.py
            check "login scripts" python3 tests/wayland/test_session_launcher.py
            ;;
    esac
done

printf '\nSummary\n'
printf '  %s\n' "${results[@]}"
exit "$failed"
