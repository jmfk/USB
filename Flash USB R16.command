#!/bin/sh
# Owner-operated entry point. Sealed older packages remain unchanged.
# --check is file-only; ordinary execution requires the owner's typed FLASH.
set -eu

case "$#:${1-}" in
    0:|1:--check) ;;
    *) echo 'Använd inga argument, eller --check för enbart filkontroll.' >&2; exit 2 ;;
esac

PROBE_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROBE_LAUNCHER="$PROBE_ROOT/manual-flash/usb-host-probe-r16/Installera USB.command"
if [ ! -x "$PROBE_LAUNCHER" ]; then
    echo 'R16-startfilen saknas. Ingen installation startades.' >&2
    exit 1
fi

if [ "$#" -eq 0 ]; then
    echo 'USB R16 – MIDI-router och separat ljudexperiment för FM-1.'
    echo 'USB MIDI1: varje input till valfria outputs. Läs README för fysiska reglage.'
    echo 'Digitakt II och Behringer-kombinationen är ännu inte kompatibilitetsverifierad.'
fi
exec "$PROBE_LAUNCHER" "$@"
