#!/usr/bin/env bash
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
cd "$project_dir"

python3 scripts/security/check_repository_hygiene.py workflows
python3 scripts/security/check_repository_hygiene.py secrets
python3 scripts/security/check_repository_hygiene.py paths
python3 tests/security/test_source_language.py

shellcheck --version >/dev/null
while IFS= read -r -d '' file; do
    case "$(head -n 1 "$file")" in
        *bash*) bash -n "$file" ;;
        *) sh -n "$file" ;;
    esac
done < <(find scripts -type f -name '*.sh' -print0)
find scripts -type f -name '*.sh' -print0 | xargs -0 shellcheck --severity=error

python3 tests/security/test_analyzer.py clang-tidy

cmake -S . -B build-local-audit -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_CXX_CLANG_TIDY=clang-tidy
cmake --build build-local-audit --parallel "${LUDASH_BUILD_JOBS:-2}"

printf '%s\n' 'Local security and quality audit passed.'
