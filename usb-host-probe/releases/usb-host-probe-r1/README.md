# USB Host Probe R1 — FM-1

Separat genomförbarhetsprov, inte färdig mixer. Två USB-ljudkällor med stereo in
och ett tredje USB-ljudkort med stereo ut delar en strömförsörjd hubb. FM-1 är
host. En paketöverföring åt gången får använda USB0:s gemensamma FADDR.

## Status och avgränsning

Paketet förbereds och kontrolleras lokalt. Hårdvarustöd är inte verifierat förrän
provet körts på FM-1 och resultaten granskats. Modellen verifierar vår kod mot
antagna registerbeteenden; den bevisar inte att kretsen beter sig så.

- UAC1, vanlig PCM, stereo, 16 bit, gemensamt vald 44 100 eller 48 000 Hz.
- Källorna får vara asynkrona. Utgången måste vara adaptiv eller synkron.
- Asynkron USB-utgång, feedback, implicit feedback, UAC2, 24/32 bit, andra rates
  och MIDI-host ingår inte. Att en enhet är class compliant räcker inte.
- Två hubbnivåer, högst sju portar per hubb, högst sexton ljudkandidater.
  Anslut alla enheter före start; ingen hotplug under provet.
- Varje konfigurationsdescriptor är begränsad till 4 KiB. Endast första
  konfigurationen och första kompatibla alternate per riktning väljs.
- Källorna och utgången måste ha tre olika USB-adresser. Källor på samma interface
  är inte tre-enhetsprovet. Ljudkortets oanvända USB-ingång öppnas inte.
- Ingen samplingsomvandling: buffertdrift mäts öppet. En godkänd femminuterskörning
  bevisar inte att fria ljudklockor kan mixas obegränsat länge.
- FM-1 levererar inte VBUS-ström. Använd strömförsörjd hubb och lämplig OTG-kabel.

## Installation och bevarande

Endast ägaren startar den separata manuella installationsfilen i
`manual-flash/usb-host-probe-r1/`. Codex får inte starta den. `--check` är enbart
filkontroll utan MIDI/enhetsåtkomst. Paketet ersätter den körande appen; inga
andra firmwarefunktioner finns i testappen. Befintliga överlämnade paket ändras
inte. Provet skriver inte patterns, inställningar eller diagnostik till flash.

Boot sker alltid som USB-device; återställnings-/updatervägen från Dinghy behålls.
USB-host startas bara med fysisk PLAY efter armering. REC och musikknappar är
inaktiva. PLAY under körning avbryter; avbrutet räknas inte som godkänt.

## Kör ett prov

Använd projektets Pythonmiljö `.build-env/midi-venv/bin/python`, som redan har
MIDI-stöd. Följande kommandon är för ägaren efter manuell installation; de
installerar inte firmware och startar inte hostläget.

1. Slå på FM-1 normalt och anslut den till datorn. Ha batteriet laddat.
2. Armera en tiosekunders canary från projektroten:

   ```sh
   .build-env/midi-venv/bin/python manual-flash/usb-host-probe-r1/collect.py arm --run-dir usb-host-probe/hardware-results/test-001 --unit canary --rate 48000 --gain 25
   ```

3. Behåll FM-1 påslagen när USB-kabeln flyttas till hubben. Anslut båda syntharna
   och ljudkortet till hubben. Starta olika, tydligt hörbara stereoljud på
   syntharna. Börja med låg förstärkar-/hörlursnivå på det externa ljudkortet.
4. Tryck fysisk PLAY på FM-1. Displayen visar upptäckt, körning och resultat.
   Provet använder de två första möjliga källorna med en tredje separat utgång.
   Saknas en kompatibel kombination visas NEED 2 IN + 1 OUT; det är inte bevis
   för en controllerbegränsning.
5. Efter avslut återgår porten till device. Behåll FM-1 påslagen, flytta USB till
   datorn och hämta resultatet:

   ```sh
   .build-env/midi-venv/bin/python manual-flash/usb-host-probe-r1/collect.py fetch --run-dir usb-host-probe/hardware-results/test-001 --rate 48000 --gain 25
   ```

6. Efter godkänd canary med aktivt ljud: upprepa `arm`/kabelbyte/PLAY/`fetch` med
   `--unit stress` (60 s), sedan `--unit long` (300 s). Samma run-dir, rate, gain,
   enheter och hubbportar. En ändring är ett nytt experiment med ny run-dir.

USB GAIN styr den externa utgången. **FM-1:s MASTER styr inte ljudkortets utgång.**
Gain 25 ger varje källa ungefär -24 dB; gain 100 ger ungefär -12 dB per källa.
FM-1:s egna analoga utgång är tyst. Inga effekter eller limiter med musikalisk
funktion ingår.

## Vad som mäts

Per källa: paket, frames, fel, L/R-peak, innehållshash, buffertens min/max och
första/sista stabila fyllnad. Utgång: färdiga paket/frames, fel, peak och hash.
Dessutom: USB-frameglapp, ofärdiga överföringar vid nästa frame, adressbyten,
längsta observerade överföringslatens (100 us pollupplösning), registerfel,
upptäckta/avvisade enheter samt vald VID/PID/hubbväg/endpoint/interface.

BUS TEST COMPLETE kräver tre färdiga paket per frame, inga observerade glapp eller
överföringsfel, primade källbuffertar och ingen registrerad över-/underfyllnad.
Fel avbryter utan att byta FADDR mitt i en pågående överföring. Lokal reset
avbryter överföringen innan porten återställs till device.

Paketantal och hash är inte oberoende ljudbevis. Kontrollera separat att båda
syntharna hörs i rätt kanaler och att en tystad synth faktiskt försvinner ur
mixen. Rapporten lämnar audio_integrity_verified och analog_output_verified false
tills oberoende mätning/ägarevidens finns. En missad mjukvarudeadline kan bero på
drivrutinen; den bevisar inte att hårdvaran är principiellt oförmögen.

## Avbrott och återupptagning

Senaste snapshot ligger bara i FM-1:s RAM. Stäng inte av före `fetch`. En
strömförlust förlorar den pågående/ohämtade körningen; ett fysiskt USB-prov kan
inte fortsätta mitt i en ljudström. Använd `arm --retry` för ett nytt attempt-ID.
På datorn bevaras alla hämtade försök; färdiga arbetsenheter hoppas över.

Collector använder en exklusiv körningslåsning, oföränderliga build/config/run-ID:n,
CRC, atomiska filer med fsync och återuppbyggd status från resultat. Snapshot
kvitteras på FM-1 först när datorns resultat är durabelt. Ett avbrott före/efter
kvitteringen får inte starta om ett färdigt prov. COMPLETE skrivs bara när alla
tre arbetsenheter är lokalt verifierade och insamlade; det betyder inte bekräftad
analog ljudkvalitet.

Resultat sparas på den lokala Mac-disken under vald run-dir. Flytta/säkerhetskopiera
hela katalogen för fortsatt förvaring. Den får inte läggas på en temporär disk.

## Lokal verifiering

`python3 usb-host-probe/build.py` kör en register-/USB-enhetsmodell av den verkliga
hostkoden med ASan/UBSan, provar båda rates, tre separata adresser med samma
endpointnummer, ljudblandning och felvägar, bygger FM-1 och renderar testskärmen.
Pythonkontroller provar CRC, faktisk C-wirefixture, identitetskontroller,
avbrott/återupptagning, idempotens och att ingen analog verifiering hittas på.

Bygget körs i befintlig OrbStack-image, utan nätverk eller USB-enheter. Lokal
verifiering och paketets minnesuppgifter dokumenteras i identity.json och
build-log.txt. Nästa ändring efter överlämning ska bli en ny versionskatalog.
