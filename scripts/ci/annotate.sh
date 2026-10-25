#!/usr/bin/env bash
# Runs a command; when it fails, reports its last output lines as a GitHub annotation, which
# can be read without access to the logs.
#
# Usage: scripts/annotate.sh TITLE COMMAND [ARGS ...]
set -uo pipefail
title=$1
shift
log=$(mktemp)
"$@" 2>&1 | tee "$log"
status=${PIPESTATUS[0]}
if ((status != 0)); then
    message="$* failed with status $status"$'\n'"$(grep -v '^\s*$' "$log" | tail -n "${ANNOTATE_LINES:-30}")"
    message=${message//'%'/'%25'}
    message=${message//$'\r'/}
    message=${message//$'\n'/'%0A'}
    echo "::error title=${title}::${message}"
fi
rm -f "$log"
exit "$status"
