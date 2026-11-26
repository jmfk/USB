# USB R1 — FM-1

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
  ingår inte. Att en enhet är class compliant räcker inte.
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
USB-host startas bara med fysisk PLAY: först upptäckt, sedan ett andra PLAY för
att börja vidarebefordra MIDI/ljud. Audio kräver först armering från datorn; MIDI
kan köras separat. PLAY under körning stoppar med MIDI-cleanup; avbrutet
ljudprov räknas inte som godkänt. REC och musikklaviaturen är inaktiva.

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
4. Tryck fysisk PLAY på FM-1 för upptäckt. Välj MIDI-rutter enligt avsnittet
   nedan, eller lämna matrisen tom för ljudprov utan MIDI-trafik. Tryck PLAY
   igen för att starta. Displayen visar upptäckt, körning och resultat.
   Provet använder de två första möjliga källorna med en tredje separat utgång.
   Saknas en kompatibel kombination visas NEED 2 IN + 1 OUT; det är inte bevis
   för en controllerbegränsning.
5. Efter avslut återgår porten till device. Behåll FM-1 påslagen, flytta USB till
   datorn och hämta resultatet:

   ```sh
   .build-env/midi-venv/bin/python manual-flash/usb-host-probe-r1/collect.py fetch --run-dir usb-host-probe/hardware-results/test-001 --rate 48000 --gain 25
   ```

6. Efter godkänd canary med aktivt ljud: upprepa `arm`/kabelbyte/PLAY/upptäckt/PLAY/`fetch` med
   `--unit stress` (60 s), sedan `--unit long` (300 s). Samma run-dir, rate, gain,
   enheter och hubbportar. En ändring är ett nytt experiment med ny run-dir.

USB GAIN styr den externa utgången. **FM-1:s MASTER styr inte ljudkortets utgång.**
Gain 25 ger varje källa ungefär -24 dB; gain 100 ger ungefär -12 dB per källa.
FM-1:s egna analoga utgång är tyst. Inga effekter eller limiter med musikalisk
funktion ingår.

## MIDI-router (separat eller tillsammans med ljudprovet)

MIDI1-bulkportar upptäcks även på enheter utan USB-ljud. Varje virtuell kabel är
en separat logisk port: högst32 in och32 ut, högst16 accepterade enheter inom
ovanstående hubbgränser. En input kan gå till noll, en, flera eller alla outputs.
Ljudkortets MIDI-portar kan också användas om sådana finns och är kompatibla.
Vanliga USB-hubbar är anslutningar, inte egna MIDI-portar.

1. **USB → MODE**: K1 vänster väljer MIDI ONLY, K1 höger AUDIO+MIDI.
   Lägesbyte kräver stoppat device-läge utan skyddat ljudresultat.
2. Övre knappraden: **USB**, **MIDI/MON**, **BUS**, **SYNC**, tom, tom.
   Ett nytt tryck på vald MIDI-knapp växlar ROUTING/MONITOR. Undre raden har
   tre kontextuella undermenyer, en tom position, **PLAY/STOP** och kontextuell **SET/CC**.
   GLO/SHIFT och SEQ/ALT saknar funktion här och visas släckta.
3. MIDI/ROUTING har **LINK**, **PORT** och **ACT**. LINK visar vald förbindelse;
   SELECT väljer input och ALGORITHM output. K1 höger slår på, vänster av.
   PORT har två sidor, input/output, med VID/PID/hubbväg/interface/endpoint/kabel.
4. ACT: K1 höger väljer ALL, vänster NONE. Först fysisk REC/APPLY ändrar den
   valda inputens rutter. Navigation ändrar aldrig rutter. Redigering är stoppad
   medan routing eller MIDI-cleanup körs.
5. MIDI/MONITOR har TRAF och ERR. USB har MODE, FMT och RES (två resultatsidor).
   BUS har kortknapparna ST och VER för status/version. Tryck igen på vald undermeny för nästa sida;
   varje huvudmenysida och undermeny minns sitt eget läge. M och S i rubriken
   visar respektive sidnummer. Tomma knappositioner är inaktiva.
6. PLAY upptäcker anslutna enheter; nästa PLAY i READY startar. PLAY under
   körning stoppar med befintlig SysEx/F7 och kanal-cleanup till använda outputs.
   Växla menyer fritt utan att stoppa eller ändra USB-rollen. Alla enheter ska
   vara anslutna före upptäckt; ingen hotplug. För AUDIO+MIDI krävs först samma
   datorarmering och kabelbyte som i ljudprovet ovan.

## MIDI-synk (R14)

**SYNC → CLK**: K1 väljer30–300 BPM, K2 slår intern klocka av/på medan
transporten är stoppad. Klockan är av från start. Aktivering i ledigt device-läge
väljer MIDI ONLY; ett armerat/skyddat ljudprov ändras inte. PLAY upptäcker först
USB-portarna; nästa PLAY startar routing och, när aktiverad, MIDI Start +24 PPQN.
Även enheter med enbart MIDI-utgång kan då upptäckas i MIDI ONLY.

**SYNC → OUT**: ALGORITHM väljer en logisk USB-hostutgång medan stoppad.
**CTRL**: K1 väljer CC0–119, K2 kanal1–16. Tryck CTRL igen för K1 på-värde och
K2 av-värde0–127. Standard CC20, kanal1, på127/av0. REC växlar CC enbart på
SYNC under körning. STOP på valfri sida skickar MIDI Stop och befintlig cleanup;
ett aktivt CC återställs till av-värdet efter SysEx/kanal-cleanup.

Ett nytt tryck på CLK visar antal köade klockpulser, överhoppade pulser,
servicefördröjningar och nekade CC. Realtime kan passera SysEx, men CC väntar på
ett nytt REC-tryck om utgången är upptagen. Sena pulser tas inte igen i en burst.
Räkneverken är programvaruobservationer, inte uppmätt fysisk precision/jitter.
Undvik att samtidigt routa en extern klocka till samma utgång. Ingen synkgenerator
till Mac via USB-device, TRS eller Bluetooth ingår. Inställningarna lever i RAM.

LINK använder färgade IN/OUT-fält, kabelnummer och förbindelselinjer. Markerad
meny har färgbakgrund; rubrik och M/S-sidor ligger separat. Alla12 knappositioner
finns kvar, med tomma inaktiva positioner. Panelens faktiska läsbarhet och MIDI-
timing kräver ägarens hårdvarutest efter manuell installation.

Alla rutter börjar tomma. Matrisen lever bara i RAM; identisk återupptäckt med
samma VID/PID/hubbväg/interface/endpoint/kabel/jack bevarar den i samma session.
Ändrad topologi tömmer hela matrisen. Ingen flashlagring, automatisk omkoppling
eller hotplug finns. Efter fristående MIDI väljer USB/MODE och K1 höger ljudprovet igen.
Ett ohämtat, armerat ljudresultat är skyddat tills collectorn hämtat/kvitterat det.

Noter, CC, programbyte, pitchbend, aftertouch, system common, MIDI clock/start/
continue/stop och korrekt paketerad SysEx vidarebefordras utan kanaländring.
Endast USB-kabelnumret skrivs om för respektive destination. SysEx från olika
inputs till samma output serialiseras; realtime kan passera. En oavslutad SysEx
kan därför hålla andra meddelanden till samma output tills STOP gör cleanup.
MIDI2/UMP, vendor-protokoll, CIN-F rå byte-ström utöver realtime, TRS/Bluetooth,
kanalfilter, kanalöversättning och USB-device-brygga till dator ingår inte.

Varje input och output-endpoint har64 eventplatser. Fulla destinationsköer
bromsar input i stället för att delvis kopiera ett fan-out-event. USB NAK behåller
utdata; köposter tas bort först vid observerad ACK/toggle-ändring. En input-enhets
egen buffert kan fortfarande svämma över vid långvarig belastning. Blockerad OUT
över500ms eller ogiltiga paket stoppar försöket. STOP har högst2s cleanup-budget.
Ingen obegränsad throughput eller maximal portlast är hårdvaruverifierad.

I kombinerat läge har ljudets tre paket per millisekund företräde. MIDI får bara
kvarvarande tid; den delade USB-adressen byts efter avslutad eller säkert avbruten
transaktion. En otillräcklig deadline/quiescence avbryter med fel. Detta är ett
mättbart experiment, inte ett löfte om glitchfritt ljud+MIDI på din uppsättning.
Digitakt II:s och den ännu ospecificerade Behringer-mixerns ljudformat måste
fortfarande kontrolleras; MIDI-stöd bevisar inte stöd för deras USB-ljud.

## Vad som mäts

Per källa: paket, frames, fel, L/R-peak, innehållshash, buffertens min/max och
första/sista stabila fyllnad. Utgång: färdiga paket/frames, fel, peak och hash.
Dessutom: USB-frameglapp, ofärdiga överföringar vid nästa frame, adressbyten,
längsta observerade överföringslatens (100 us pollupplösning), registerfel,
upptäckta/avvisade enheter samt vald VID/PID/hubbväg/endpoint/interface.
MIDI: portantal, RX/TX-paket, event/fan-out, idle-polls, retries, ogiltiga paket,
backpressure, fel/cleanup samt topologi- och routinghash. `midi_hardware_verified`
förblir false tills separat hårdvaruevidens finns.

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
hostkoden och MIDI-kärnan med ASan/UBSan, provar virtuella kablar, SysEx-merge,
atomisk fan-out, fulla köer, NAK/late-ACK utan dubbletter, stop-cleanup, fysiska
UI-reglage och felvägar. Ljudmodellen provar båda rates, tre separata adresser med samma
endpointnummer, ljudblandning och felvägar, bygger FM-1 och renderar testskärmen.
Pythonkontroller provar CRC, faktisk C-wirefixture, identitetskontroller,
avbrott/återupptagning, idempotens och att ingen analog verifiering hittas på.

Bygget körs i befintlig OrbStack-image, utan nätverk eller USB-enheter. Lokal
verifiering och paketets minnesuppgifter dokumenteras i identity.json och
build-log.txt. Nästa ändring efter överlämning ska bli en ny versionskatalog.


## MIDI-prestanda och diagnostikloggar (R12 och senare)

MIDI/MON-knappen växlar till MON; välj PERF för maximal servicetid och samplade
kötoppar. Detta är inte end-to-end-latens eller total CPU-belastning. Kör samma
externa MIDI-sekvens, tempo, routning och hubbportar när prestanda jämförs.

1. Installera manuellt. Med FM-1 ansluten till datorn, spara ett nytt körnings-ID:

   ```sh
   .build-env/midi-venv/bin/python manual-flash/usb-host-probe-r1/performance.py arm --run-dir usb-host-probe/hardware-results/midi-canary-001
   ```

   Kommandot startar ingen trafik eller host. Välj USB/MODE MIDI ONLY med K1 vänster och sätt önskade
   rutter i MIDI/LINK. PLAY upptäcker; nästa PLAY börjar vidarebefordra.
2. Kör först en kort, cirka tio sekunders kontroll med låg trafik. Stoppa med
   fysisk PLAY, låt cleanup avslutas. Ingen benchmark startas från datorn.
3. Behåll FM-1 påslagen och flytta USB från den strömförsörjda hubben till datorn.
   USB0 kan inte vara host och datoransluten MIDI-device samtidigt.
4. Från projektroten, hämta via MIDI till en ny beständig lokal körningskatalog:

   ```sh
   .build-env/midi-venv/bin/python manual-flash/usb-host-probe-r1/performance.py fetch --run-dir usb-host-probe/hardware-results/midi-canary-001
   ```

   Vid avbrott: kör exakt samma kommando/katalog igen. Manifest, rådata, rapport
   och COMPLETE skrivs atomiskt med fsync före ACK. Det slumpade 112-bitars körnings-ID:t skyddar även mot identitetskrock efter omstart. En ny fysisk körning kräver
   en ny katalog; blanda aldrig resultat eller ändrad firmware i en gammal.
5. Läs/validera eller jämför lokalt utan MIDI/enhetsåtkomst:

   ```sh
   .build-env/midi-venv/bin/python manual-flash/usb-host-probe-r1/performance.py validate --run-dir usb-host-probe/hardware-results/midi-canary-001
   .build-env/midi-venv/bin/python manual-flash/usb-host-probe-r1/performance.py report --run-dir usb-host-probe/hardware-results/midi-canary-001
   .build-env/midi-venv/bin/python manual-flash/usb-host-probe-r1/performance.py compare --run-dir usb-host-probe/hardware-results/midi-canary-001 --other usb-host-probe/hardware-results/midi-canary-002
   ```

Rapporten innehåller events/s, fan-out/s, paket/s före stop-cleanup, retries,
backpressure, fel, samplade kötoppar och maximal/total uppmätt servicetid. Loggen
innehåller de senaste 15 tidsstämplade start-/räknar-/fel-/stopphändelserna samt
antal bortfallna poster. Den fångar inte individuella noter eller SysEx-innehåll.
Topology/routing/audio-läge måste matcha vid jämförelse; samma externa indata
måste också säkerställas av ägaren. En rapport betyder inte godkänt hårdvarutest.

Vanlig routing utan performance.py arm fryser ingen skyddad diagnostik och kan
startas om utan dator. Under ett armerat prestandaprov gäller:
Ohämtad diagnostik blockerar nästa PLAY i device-läge. Hämtning kvitterar endast
diagnostiken. För ett armerat ljudprov ska även ordinarie collect.py fetch köras
för ljudresultatet. Befintliga audio-CRC/ACK-regler ändras inte. Enhetens resultat
ligger bara i RAM: stäng inte av före hämtning. En fysisk körning kan inte
återupptas efter strömförlust. Insamlade filer på Mac-disken kan däremot läsas
utan enhet och redan durabla hämtningar återhämtas utan att provet körs om.
Långa körningar måste avslutas före 32-bitars räknar-/millisekundwrap.
