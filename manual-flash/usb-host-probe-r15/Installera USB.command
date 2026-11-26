#!/bin/sh
set -eu
PROBE_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROBE_PY="$PROBE_DIR/../../.build-env/midi-venv/bin/python"
if [ ! -x "$PROBE_PY" ]; then echo 'Projektets Pythonmiljö saknas. Ingen åtgärd startades.'; exit 1; fi
exec "$PROBE_PY" "$PROBE_DIR/manual_install.py" "$@"
