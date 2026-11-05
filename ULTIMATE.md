# GEOS-U64 — GEOS 2.0 voor de Commodore 64 Ultimate

Een eigen variant (`u64`) van de gereconstrueerde GEOS 2.0-kernal van
[mist64/geos](https://github.com/mist64/geos), gemaakt om zoveel mogelijk uit
de C64 Ultimate (en de Ultimate 64 / Elite II) te halen.

## Wat zit erin

**Kernal (`VARIANT=u64`)**

| Onderdeel | Wat het doet |
|---|---|
| Netwerk-API | TCP/UDP-sockets via het Ultimate Command Interface ($DF1C–$DF1F), als vaste jump table op **$FE50** voor elke GEOS-applicatie. |
| Turbo | Zet bij het opstarten de turbo aan ($D030 bit 0). Alle GEOS-diskdrivers zetten $D030 tijdens disktoegang al op 0 en daarna terug, dus de fastloader draait altijd op 1 MHz en de rest op volle snelheid (tot 48/64 MHz). |
| Klok | Datum en tijd komen bij het opstarten uit de RTC van de Ultimate (die via SNTP gelijk kan lopen met internet). |
| REU | Bij het opstarten wordt de REU gedetecteerd (echte DMA-test) en `ramExpSize` gezet: het aantal banken van 64 KB, maximaal 8 (512 KB, wat Configure 2.0 aankan). Dat deed vroeger `GEOBOOT`, maar dat staat niet op onze disk; zonder deze stap zag GEOS de REU niet. Getest in VICE: geen REU → 0, 128 KB → 2, 512 KB → 8, 16 MB → 8. |
| Toetsenbord | De toetsenbordscan draait op 1 MHz (via $D030), zodat de toetsenbordlijnen in turbo genoeg tijd hebben om stabiel te worden. |
| Muis en joystick | Eigen gecombineerde input-driver (`input/u64input.s`): **1351-muis** (USB-muis via de Ultimate) in poort 1 én een **joystick in poort 1 of 2**, tegelijk, zonder instellen. De joystick versnelt van 1 naar 8 pixels per frame zolang je een richting vasthoudt; de vuurknop klikt. De muis beweegt lineair; snelheid en versnelling stel je in de Ultimate in (*Mouse Sensitivity/Acceleration*). Turbo-bestendig: de wachtlus van ~1 ms voor de SID-potmeters draait op 1 MHz. De disk bevat geen losse input-drivers, want deskTop zou de eerste die hij vindt laden en daarmee de ingebouwde driver vervangen. |
| deskTop | Start elke `DESK TOP` die op de disk staat, zonder versiecontrole. |
| Dialoogvensters | GEOS2000 `speedupDlgBox`: de schaduw wordt als twee smalle stroken getekend in plaats van een volle rechthoek. Gerepareerd: het origineel liet bij het sluiten de schaduw op het scherm staan (in VICE aangetoond en na de fix gecontroleerd). |
| Menu's | GEOS2000 `newMenu_5`: kortere code voor het knipperen van een menukeuze, zelfde gedrag. Assembleerde niet meer in mist64; gerepareerd. |
| ToBASIC | Om ruimte te maken voor de netwerk-API reset deskTop naar BASIC in plaats van een BASIC-programma te starten (`removeToBASIC`). Start PRG's gewoon vanuit het Ultimate-menu. De turbo gaat daarbij uit, zodat de C64-KERNAL weer normaal kan laden. |
| Origineel | Alle varianten van mist64 blijven ongewijzigd: `bsw` bouwt nog steeds byte voor byte de originele GEOS 2.0-kernal. |

**Applicaties op de disk** (de eerste drie staan op de eerste deskTop-pagina)

| Icoon | Programma | Functie |
|---|---|---|
| twee computers + wereldbol | **Network** | Netwerkconfiguratie: DHCP aan/uit, statisch IP/netmask/gateway/DNS, hostnaam, netwerkwachtwoord, FTP/Telnet, internettijd (SNTP), tijdzone, tijdserver. Menu *test → internet* controleert DNS + TCP. |
| chip met "U64" | **U64 Setup** | Instellingen van de Ultimate zelf: Turbo Control, CPU-snelheid, badlines, REU/GeoRAM en grootte, muismodus, joystick-swapper, drive A/B. Menu *presets*: **GEOS optimal** (turbo, REU 512 KB, muis) of **Commodore 64 default** (1 MHz, badlines, geen REU, drive B uit; de Command Interface blijft aan). |
| scherm met "NET" | **NetTest** | Diagnose: kernal, Command Interface, turbo, het IP-adres van elke interface (Ethernet en WiFi) en een echte HTTP-request naar example.com. |

Network en U64 Setup praten met de REST-API van de Ultimate (`/v1/configs`,
firmware 3.11 of nieuwer). Ze verbinden via de kernal-sockets met het eigen
IP-adres van de Ultimate. Bij het starten proberen ze eerst 127.0.0.1 en
daarna de interfaces, de laatste (WiFi) eerst. Lukt geen enkel adres, dan toont
een venster per adres de reden, en kun je via *file → ultimate IP* zelf een
adres invullen. Per categorie komen alle waarden in één request binnen (U64
Setup: 4 in plaats van 13), met een voortgangsbalk onderin; presets tonen die
balk ook. Klik op een regel: een keuzelijst springt naar de volgende waarde,
een tekstveld opent een invoervenster. Wijzigingen werken meteen, maar gaan bij
een herstart verloren tot je *file → save to flash* kiest. Regels met `*` werken
pas na *save to flash* plus *reboot ultimate*.

**Offline-modus.** Is de webservice niet bereikbaar, dan schakelen de apps over
op een config-bestand en melden dat op het scherm ("Ultimate offline"). Ze
schrijven `/temp/uci_config.cfg` (regels `[Categorie]` en `Item=Waarde`) via de
DOS-commando's van de Command Interface en laden het met `LOAD_CONFIG`. Dat
commando zit in de huidige firmwarebron, maar misschien nog niet in oudere
firmware; dan toont de statusregel de foutmelding van de Ultimate.
Beperkingen van deze modus:
- Instellingen uitlezen kan niet. U64 Setup toont wat de C64 zelf kan zien
  (turbo, snelheid, badlines, REU); de rest staat op `-`.
- Opslaan in flash kan niet; doe dat in het Ultimate-menu.
- *Time Zone* is offline niet te wijzigen.

Op een C64U via WiFi gaf de verbinding met zichzelf `connect error 118` voor
127.0.0.1 en fout 113 voor het eigen WiFi-adres. Volgens de nummering van newlib
(de C-bibliotheek die de firmware waarschijnlijk gebruikt) is dat `EHOSTUNREACH`
en `ECONNABORTED`. De oorzaak is nog niet opgehelderd.

Veiligheidsgrenzen:
- *Turbo Control* biedt alleen `Off` en `TurboEnable Bit`. Andere standen
  houden de turbo ook tijdens disktoegang aan; dan faalt de 1541-fastloader
  en kan een schrijfactie de disk beschadigen.
- *Command Interface* en *Web Remote Control* kun je hier niet uitzetten:
  zonder die twee werken de programma's zelf niet meer.

## De C64 Ultimate instellen — maximaal uit de kast

Druk op de Ultimate-knop (of F2 in het menu) en ga per categorie langs.
Met **U64 Setup → presets → GEOS optimal** stel je de vetgedrukte waarden ook
vanuit GEOS in.

### U64 Specific Settings

| Instelling | Waarde | Waarom |
|---|---|---|
| **Turbo Control** | **TurboEnable Bit** | Verplicht. Alleen in deze stand kan GEOS de turbo uitzetten ($D030) tijdens disktoegang. |
| **CPU Speed** | **hoogste waarde** (48 MHz op U64, 64 MHz op Elite II / C64U) | Alles behalve disk-I/O loopt zoveel sneller. |
| **Badline Timing** | **Disabled** | Extra CPU-cycli. De VIC houdt voorrang, het beeld blijft goed. |
| **Mouse Mode** | **Mouse** | USB-muis als 1351 in poort 1, past bij de ingebouwde driver. Een joystick in poort 1 of 2 werkt daarnaast gewoon. Werkt de muis niet, probeer dan *Joystick Swapper*. |
| Mouse Sensitivity | naar smaak (8 = standaard) | |

### C64 and Cartridge Settings

| Instelling | Waarde | Waarom |
|---|---|---|
| **Command Interface** | **Enabled** | Verplicht voor netwerk, klok en de apps. |
| **RAM Expansion Unit** | **Enabled** | GEOS 2.0 gebruikt een REU voor RAM-disk, snelle deskTop-wissel en desk-accessory-swap. |
| **REU Size** | **512 KB** | Het grootste formaat dat het originele GEOS 2.0 *Configure* betrouwbaar ondersteunt (RAM 1581). Grotere REU's (tot 16 MB) werken met MegaPatch/Wheels, niet met standaard GEOS 2.0. |
| GeoRAM | niet gebruiken | Standaard GEOS 2.0 heeft geen GeoRAM-driver (dat hebben MegaPatch en Wheels wel). Op de Ultimate is GeoRAM de derde keuze van *RAM Expansion Unit* (`GeoRAM Mode`) en sluit daarmee de REU uit. Kies dus **Enabled**, niet GeoRAM. |
| Cartridge | geen | Een cartridge kan $DE00–$DFFF bezetten. |
| Fast Reset | naar smaak | |

### Drive A / Drive B Settings

| Instelling | Waarde |
|---|---|
| **Drive A** | Enabled, **Drive Type 1541**, Bus ID 8, `GEOS-U64.d64` gemount |
| Drive B | optioneel: 1541/1571/1581 op ID 9. Kies in GEOS *Configure* hetzelfde type. |
| SoftIEC | Disabled (GEOS praat niet met de DOS-emulatie) |

Houd drive 8 een echte (geëmuleerde) 1541: de kernal bevat de 1541-driver.
Andere drive-types voor A: bouw met `--drive drv1581` of `drv1571`.

### Netwerk (Ethernet Settings, WiFi settings, Network Settings)

| Instelling | Waarde |
|---|---|
| Use DHCP | Enabled (of een vast IP, ook in te stellen met **Network**) |
| **Web Remote Control Service** | **Enabled** (nodig voor Network en U64 Setup) |
| Network Password | leeg, of vul hetzelfde in via *file → password* in de apps |
| **SNTP Enable** | **Enabled**, met de juiste *TimeZone*: GEOS krijgt bij het opstarten de juiste tijd |

Sla alles op met **Save to Flash** (Ultimate-menu of *file → save to flash*).

## Opstarten

1. Zet `GEOS-U64.d64` op een USB-stick, mount hem op drive A (8).
2. Kies in het bestandsmenu van de Ultimate bij het bestand `GEOS` voor **Run**.
   Dat laadt via DMA en is bijna direct klaar. `LOAD"GEOS",8,1` + `RUN` werkt
   ook, maar laadt de 62 blocks op 1 MHz via de seriële bus (tientallen seconden).
3. GEOS start meteen deskTop. Open de eerste keer **CONFIGURE**: die ziet nu
   de REU (tot 512 KB). Zet daar de RAM-opties aan (RAM-disk, deskTop/drivers
   in RAM) en sla op; Configure past ze daarna bij elke start automatisch toe.

In VICE: `x64sc -reu -reusize 512 -drive8truedrive -autostart "GEOS-U64.d64:geos"`.
VICE emuleert het Command Interface en de turbo niet: het netwerk en de turbo
werken daar niet, de rest wel.

## Bouwen

Benodigd: cc65, VICE (`c1541`), exomizer (in `C:\Users\aegwh\OneDrive\dev\c64`)
en een GEOS 2.0-disk als `GEOS64.D64` in deze map (voor deskTop en Configure).

```bash
python build.py u64
```

Resultaat: `build/u64/geos.d64` (kernal + Network + U64 Setup + NetTest).
Opties: `--drive drv1541|drv1571|drv1581`, `--input mse1351|joydrv|…`,
`-D naam=waarde` voor extra assembler-defines. `python build.py bsw` bouwt de
originele kernal; `./regress.sh bsw` vergelijkt die met de referentie.

Instellingen van de variant staan in `config.inc` onder `.ifdef u64`
(o.a. `ultimateBootSpeed` om bij het opstarten een vaste snelheid te forceren).

## Netwerk-API voor programmeurs

Signatuur: `"UNET", versie` op **$FE7A** (nu versie 2). Controleer die eerst.
Elke call geeft **X = foutcode** terug (0 = ok, 13 = `DEV_NOT_FOUND` = geen
Command Interface, $50 = de Ultimate meldt een fout; de tekst staat bij
`UltGetStatus`). De Z-vlag volgt X, dus `bnex fout` werkt. r0–r3 kunnen
veranderen. Strings zijn ASCII.

| Adres | Call | In | Uit |
|---|---|---|---|
| $FE50 | UltDetect | – | A = aantal netwerkinterfaces; r0 → 2 bytes: $D030, $D031 |
| $FE53 | NetGetIP | A = interface, r0 = buffer van 12 bytes | IP, netmask, gateway |
| $FE56 | NetTCPConnect | r0 = hostnaam/IP (0-terminated), r1 = poort | A = socket |
| $FE59 | NetUDPConnect | idem | A = socket |
| $FE5C | NetClose | A = socket | |
| $FE5F | NetRead | A = socket, r0 = buffer, r1 = max (≤ 1472) | r1 = aantal bytes; 0 = verbinding dicht (socket vrijgegeven); $FFFF = nog niets binnen (de Ultimate wacht 40 ms; probeer opnieuw) |
| $FE62 | NetWrite | A = socket, r0 = data, r1 = lengte | (verstuurd in blokken van 255 bytes) |
| $FE65–$FE70 | NetListen, NetListenStop, NetListenState, NetListenSocket | | gereserveerd, zie noot |
| $FE71 | UltSetSpeed | A = $D031-waarde (bits 0–3 snelheid, bit 7 = badlines uit) | turbo aan |
| $FE74 | UltGetStatus | | r0 → statustekst van de laatste call |
| $FE77 | UltCommand (API-versie 2) | r0 = commandobytes (target, commando, parameters), r1 = lengte (2–896), r2 = antwoordbuffer, r3L = grootte (max. 255) | r1L = opgeslagen antwoordbytes; status via UltGetStatus |

Noot: de listener-commando's ($12–$15) bestaan in de ultimateii-dos-lib, maar
niet in de firmware. De vier adressen blijven gereserveerd en geven nu direct
X = $50 terug (dat scheelde 27 bytes kernalruimte).

Een C-binding voor cc65 staat in `apps/common/ultnet.h` / `ultnet.s`,
een HTTP/REST-client in `apps/common/rest.c`.

## Andere GEOS-versies (onderzocht oktober 2026)

| Project | Bron | Licentie | Wat we ermee doen |
|---|---|---|---|
| mist64/geos (onze basis) | GitHub, laatste update april 2026 | geen licentie vermeld | Up-to-date. Geen fork heeft C64-verbeteringen die wij missen. |
| GEOS2000 (ytmytm/c64-GEOS2000) | GitHub, ACME-broncode | "Other" | Alle opties zitten al in mist64. `speedupDlgBox` en `newMenu_5` zijn gerepareerd en staan aan in `u64`. |
| Wheels 64 | commercieel, geen broncode | – | Deels gereconstrueerd in mist64. Wij gebruiken al de optimalisaties en bugfixes. De overige Wheels-functies hebben de REU-modules van Wheels nodig, die er niet zijn. |
| MegaPatch 3.3r12 en GDOS64 1.03 | Bitbucket mkgit64/area6510, broncode als geoWrite-documenten voor de GEOS MegaAssembler op D81 | "All rights reserved", gratis te gebruiken en te verspreiden | Geen code overnemen zonder toestemming van Markus Kanet. Ze draaien wel als alternatief systeem op de C64U (512 KB+ REU, 1581). MegaPatch noemt de U64-turbo zelf "unsupported". |
| Area6510 GEOS-Update | idem | idem | Hun `SuperMouse_U64` wees op het turboprobleem in de 1351-driver. Dat hebben we zelf opgelost (zie Muis). |

## Status: wat is getest en wat niet

| Gezien op een echte C64U | Alleen getest in VICE / emulator | Nog te testen op een echte C64U |
|---|---|---|
| Kernal start, deskTop en U64 Setup draaien | Instellingenscherm en waarden wisselen (met nep-REST) | Offline-modus: config-bestand schrijven en `LOAD_CONFIG` |
| Command Interface antwoordt, IP-adressen van Ethernet en WiFi worden gelezen | Offline-scherm (zonder Command Interface, dus zonder echt schrijven) | Turbo aan bij het opstarten en uit tijdens disk-I/O |
| Foutmeldingen per adres | REU-detectie (geen, 128 KB, 512 KB, 16 MB) | RTC-tijd bij het opstarten |
| **Niet gelukt:** REST naar de eigen Ultimate via WiFi | `bsw` en `gateway` byte voor byte gelijk aan het origineel | Joystick en toetsenbord op 48–64 MHz |

`build.py` zet de apps zelf op de disk, omdat `c1541 -geoswrite` (VICE 3.9)
soms maar één datablok schrijft. Na elke build controleert het of de blokketen
van elk GEOS-bestand klopt met de directory.

Start op de C64U eerst **NetTest**: die laat stap voor stap zien wat werkt.
