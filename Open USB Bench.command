#!/bin/sh
set -eu
USB_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
USB_APP="$USB_ROOT/benchmark-app/dist/USB Bench.app"
if [ ! -d "$USB_APP" ]; then
    echo 'Bygg appen först: python3 benchmark-app/build_app.py' >&2
    exit 1
fi
open "$USB_APP"
