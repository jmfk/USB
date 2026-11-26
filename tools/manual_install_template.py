#!/usr/bin/env python3
"""Manual-only FM-1 updater. --check validates files without opening MIDI."""
import contextlib
import datetime
import fcntl
import hashlib
import json
import os
from pathlib import Path
import sys

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent


def validate():
    manifest = json.loads((HERE / 'manifest.json').read_text())
    for name, digest in manifest['sha256'].items():
        if Path(name).name != name:
            raise ValueError('Ogiltigt filnamn i manifestet')
        if hashlib.sha256((HERE / name).read_bytes()).hexdigest() != digest:
            raise ValueError('Filkontrollen misslyckades: ' + name)
    from fm1_install import load_package
    product, _ = load_package(str(HERE / manifest['package']), False)
    if product != 'FM-1_990':
        raise ValueError('Fel modell eller paketidentitet')
    return manifest


def save_status(path, value):
    temporary = path.with_suffix('.tmp')
    temporary.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n')
    os.replace(temporary, path)


class Tee:
    def __init__(self, terminal, logfile):
        self.terminal, self.logfile = terminal, logfile

    def write(self, text):
        self.terminal.write(text)
        self.logfile.write(text)
        self.flush()
        return len(text)

    def flush(self):
        self.terminal.flush()
        self.logfile.flush()

    def isatty(self):
        return self.terminal.isatty()


def main():
    manifest = validate()
    print('FM-1 · manuell firmwareinstallation')
    print('Version:', manifest['version'])
    print('Innehåll:', manifest['description'])
    print('Paket och updater: filkontroll GODKÄND')
    if sys.argv[1:] == ['--check']:
        print('Enbart filkontroll. MIDI har inte öppnats. Ingen flashning.')
        return 0
    if sys.argv[1:]:
        raise ValueError('Okänt argument')
    with (HERE.parent / '.install.lock').open('a') as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            print('En annan manuell installation är redan öppen. Ingen ny startades.')
            return 1
        print('\nAnslut FM-1 med USB och slå på den normalt.')
        print('Stäng MIDI-anslutningen i HTML-simulatorn och andra uppdateringsprogram.')
        print('Denna version installerades senast som menu-r2; den nya layouten ingår INTE.')
        print('Installationen ersätter appen och rensar det RAM-lagrade mönstret.')
        print('Låt Terminal vara öppet och USB-kabeln ansluten tills resultatet visas.')
        if input('\nSkriv FLASH och tryck Retur för att starta, eller bara Retur för att avbryta: ').strip() != 'FLASH':
            print('Avbrutet. Ingenting skrevs.')
            return 0
        # Recheck after the user's decision so the confirmed files are the files installed.
        validate()
        now = datetime.datetime.now(datetime.timezone.utc)
        logs = HERE / 'logs'
        logs.mkdir(exist_ok=True)
        logfile = logs / (now.strftime('%Y%m%dT%H%M%S-%fZ') + '.log')
        statusfile = HERE / 'status.json'
        state = {'version': manifest['version'], 'started_at_utc': now.isoformat(),
                 'state': 'running', 'pid': os.getpid(), 'log': str(logfile)}
        save_status(statusfile, state)
        result = 1
        try:
            with logfile.open('x') as log:
                with contextlib.redirect_stdout(Tee(sys.stdout, log)), contextlib.redirect_stderr(Tee(sys.stderr, log)):
                    from fm1_install import main as install
                    result = install([str(HERE / manifest['package']), '--port', 'Felucca', '--yes'], out=sys.stdout)
        finally:
            state.update(state='success' if result == 0 else 'failed_or_interrupted', exit_code=result,
                         finished_at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat())
            save_status(statusfile, state)
        print('\nKLART – updatern bekräftade installation och omstart.' if result == 0
              else '\nINTE BEKRÄFTAT KLART. Behåll USB anslutet och läs loggen innan du försöker igen.')
        print('Status:', statusfile)
        print('Logg:', logfile)
        return result


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (Exception, KeyboardInterrupt) as error:
        print('\nInstallationen avbröts eller misslyckades:', str(error), file=sys.stderr)
        sys.exit(1)
