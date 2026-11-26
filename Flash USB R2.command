#!/bin/sh
# Owner-operated entry point. The sealed R2 package remains unchanged.
# --check is file-only; ordinary execution requires the owner's typed FLASH.
set -eu

case "$#:${1-}" in
    0:|1:--check) ;;
    *) echo 'Använd inga argument, eller --check för enbart filkontroll.' >&2; exit 2 ;;
esac

PROBE_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROBE_LAUNCHER="$PROBE_ROOT/manual-flash/usb-host-probe-r2/Installera USB Host Probe.command"
if [ ! -x "$PROBE_LAUNCHER" ]; then
    echo 'R2-startfilen saknas. Ingen installation startades.' >&2
    exit 1
fi

if [ "$#" -eq 0 ]; then
    echo 'USB R2 – separat testfirmware för FM-1.'
    echo 'Digitakt II och Behringer-kombinationen är ännu inte kompatibilitetsverifierad.'
fi
exec "$PROBE_LAUNCHER" "$@"
