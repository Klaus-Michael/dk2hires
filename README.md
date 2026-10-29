# dk2hires: Dungeon Keeper 2 in hohen Auflösungen

**English.** dk2hires is a small runtime fix that lets the GOG version of Dungeon Keeper 2 (1.7)
run at resolutions such as 3840×2160 and 3840×2560. It is a `dsound.dll` proxy that patches the
game in memory and leaves every game file untouched. This is a proof of concept, tested only on
Linux with Wine/Proton, without support. The cause and the fix were found and built by an AI agent, Claude Opus 5.5 in Claude Code. The instructions below and the messages of the scripts
are in German.

**Proof of Concept, nur unter Linux/Proton getestet, kein Support.**

Hintergrund zum Fix im Video: [Wird jetzt alles Open Source?](https://youtu.be/kYacWiIPLtY).

## Entstehung

Ursache und Fix hat ein KI-Agent gefunden und gebaut: Claude Opus 5.5 in Claude Code, in knapp fünf Stunden und
ohne eine einzige Rückfrage. Ich habe ihm gesagt, was ich haben will; wie das abgelaufen ist, erzähle ich im
Video.

## Was der Fix macht

In hohen Auflösungen stürzt Dungeon Keeper 2 beim Start eines Levels ab, getestet mit 3840×2160
und 3840×2560. Schon ab 1920×1080 fehlen im Ingame-Menü die Beschriftungen, und je breiter das
Bild wird, desto näher rückt die Kamera heran, bis das Spiel in 4K wie hineingezoomt aussieht.

Die Ursache liegt bei den Textfeldern des Spiels. Jedes zerlegt seinen Inhalt in Kacheln von
128×32 Pixeln und merkt sich deren Texturen in einer Tabelle mit Platz für 146 Einträge. Höhere
Auflösungen brauchen mehr Kacheln, die Tabelle läuft über und überschreibt dabei Breite und Höhe
des Textfelds. dk2hires setzt an vier Stellen an:

1. **Kacheltabelle:** Die Texturen der Textfelder liegen in einer eigenen Tabelle, die mitwächst.
2. **Speicher:** Der Speicherbereich der Engine, in dem auch die Textkacheln liegen, startet groß
   genug für die eingestellte Auflösung.
3. **Kamera:** Senkrecht zeigt das Bild denselben Ausschnitt wie bei 1024×768, breitere
   Bildschirme sehen seitlich mehr.
4. **Schrift:** Ab mehr als 1080 Bildzeilen lädt das Spiel größere Schriften.

Die DLL liegt als `dsound.dll` im Spielordner, ändert das Spiel nur im Arbeitsspeicher und reicht
alle Tonaufrufe an die echte `dsound.dll` weiter. Vor jeder Änderung vergleicht sie die Bytes an der
Stelle; passen sie nicht, bleibt das Spiel unverändert und `dk2hires.log` sagt das.

## Für welche Fassung

Der Fix passt nur zur GOG-Fassung von Dungeon Keeper 2, Version 1.7 (Build `1.7_alttab_hotfix`):

- `DKII-DX.exe`: SHA-256 `a5957e8c52b6dee80ddf6cc1d3ea5079aaaad4d11b40e0f067d34ff4157ce9bb`
- `patch.dll`: SHA-256 `0022e3aaf419f22c3287fb454e9e671ee249f1eefc652465bec96ecfd38f00c4`

`install.sh` prüft beide Dateien und bricht bei jeder anderen Fassung ab. Das Spiel selbst ist
nicht enthalten.

## Getestet

- Heroic 2.22.1 mit GE-Proton11-1 auf einer NVIDIA-Grafikkarte in 3840×2560 und 3840×2160, mit Ton.
- Wine 11.16 mit Software-Rendering in 1024×768, 1920×1080, 2560×1440, 3840×2160 und 3840×2560,
  dazu 60 Minuten Skirmish in 3840×2160 und 30 Minuten in 3840×2560 ohne Fehler.
- Der Wine aus GE-Proton11-1 mit Software-Rendering in 3840×2560.

Windows und andere Launcher sind nicht getestet. In 3840×2560 lief das Spiel im Test mit rund 30
Bildern pro Sekunde; woran das liegt, ist nicht untersucht. Hauptmenü und Filme laufen wie im
Original in 640×480 und werden hochskaliert.

## Einspielen

Unter nativem Windows im Ordner `dk2hires` mit PowerShell:

```powershell
.\install.ps1 -Resolution 2560x1440 -DryRun
.\install.ps1 -Resolution 2560x1440
```

`-res` ist ebenfalls als Alias fuer `-Resolution` erlaubt. PowerShell verwendet
hier einen Bindestrich; `--res` gehoert zum Linux-Skript `install.sh`.
Das Spiel muss beim Einspielen beendet sein. Rueckbau: `.\uninstall.ps1`.

Falls das Spiel unter Windows trotz leistungsstarker Hardware stark ruckelt und
DDrawCompat verwendet wird: In der `DDrawCompat.ini` im Spielordner (neben
`DKII-DX.exe`, nicht im Unterordner `dk2hires`) folgende Zeile ergaenzen:

```ini
CpuAffinity=all
```

Danach das Spiel neu starten. Dies hebt die Beschraenkung von DDrawCompat auf einen
logischen CPU-Kern auf. Die Einstellung hat das starke Ruckeln bei 2560x1440 auf
einem Core i9-13900K mit GeForce RTX 5090 behoben. Falls dadurch Probleme auftreten,
die Zeile wieder entfernen und das Spiel neu starten.

Die lokale Erweiterung unterstuetzt auch Mausrad-Zoom: vorwaerts zum Hineinzoomen,
rueckwaerts zum Herauszoomen. Sie verwendet die vorhandenen Home/End-Zoomfunktionen
und damit auch die eingestellten Zoomgrenzen. Der vorhandene `dinput.dll`-Wrapper
bleibt erhalten; ein zusaetzliches Hintergrundprogramm ist nicht erforderlich.
Die Erweiterung verarbeitet sowohl gepufferte DirectInput-Ereignisse im Spiel
als auch Zustandsabfragen bei der Initialisierung.
Mausrad-Zoom signalisiert die Tastatur-Eingabebenachrichtigung sofort; ein kurzer
Timer liefert auch das Loslassen der Zoomtaste ohne physischen Tastendruck.
In `dk2hires.ini` neben `DKII-DX.exe` stehen unter `[fix]` diese Optionen:

```ini
MouseWheelZoom=1
WheelZoomMs=80
WheelZoomInvert=0
```

`WheelZoomMs` bestimmt die Dauer des Zoom-Impulses pro Rastung (20 bis 250 ms).
Ein kleinerer Wert zoomt langsamer, ein groesserer schneller. `WheelZoomInvert=1`
kehrt die Richtung um; `MouseWheelZoom=0` schaltet die Erweiterung ab.
Aenderungen werden nach einem Neustart wirksam. Bei gedrueckter Strg-, Umschalt-
oder Alt-Taste wird kein Mausrad-Zoom erzeugt, damit Kamera-Tastenkombinationen
nicht versehentlich ausgeloest werden. Im Log bestaetigen `wheelzoom: aktiv`,
`wheelzoom: Maus eingebunden` und `wheelzoom: Tastatur eingebunden` die Initialisierung.

Die mittlere Maustaste laesst sich zum Drehen der 3D-Ansicht halten: Taste halten
und die Maus bewegen. Die Erweiterung verwendet die vorhandene Funktion
`Camera Mouse Rotate` (Standardtaste X). Die Belegung von X muss deshalb im Spiel
auf dem Standard bleiben. Loslassen beendet die Drehung; Fokusverlust gibt die
simulierte Taste ebenfalls frei. Die physische X-Taste bleibt verwendbar.
`MiddleMouseRotate=1` unter `[fix]` aktiviert die Funktion (Standard),
`MiddleMouseRotate=0` schaltet sie ab. Sie funktioniert unabhaengig von
`MouseWheelZoom`. Nach einer Aenderung das Spiel neu starten.

`MiddleMouseRotateSpeed=50` stellt die Drehgeschwindigkeit in Prozent ein:
50 = halbe Geschwindigkeit (Standard), 100 = bisherige Geschwindigkeit,
25 = ein Viertel. Erlaubt sind Werte von 1 bis 200. Die Skalierung betrifft
nur Mausbewegungen beim Halten der mittleren Maustaste; beide Achsen werden
angepasst. Kleine Bewegungen werden ohne Rundungsverlust gesammelt.
`MiddleMouseRotateInvert=1` kehrt nur die horizontale MMB-Drehrichtung um;
`0` ist die bisherige Richtung (Standard). Neigung und normale Mausbewegungen
bleiben davon unabhaengig. Nach einer Aenderung das Spiel neu starten.

Vertikale Neigung: `MiddleMousePitch=1` (Standard) passt die Kameraneigung
direkt auf dem Render-Thread an. Die vorherige Ctrl+Home/End-Umsetzung hat
bei diagonalen Bewegungen die X-Belegung fuer horizontales Drehen blockiert
und wurde entfernt. Es werden keine zusaetzlichen Ctrl-/Zoomtasten simuliert.
`MiddleMousePitchSpeed=100` setzt einen zusaetzlichen Neigungsfaktor in Prozent
(1..200); 50 halbiert die Neigung. `MiddleMousePitchInvert=1` kehrt die Richtung
um. `MiddleMouseRotateSpeed` beeinflusst beide Achsen. Die alte Option
`MiddleMousePitchMs` wird nicht mehr verwendet. `MiddleMousePitch=0` schaltet
die Neigung ab. Die neue Umsetzung ist noch durch einen Benutzertest zu pruefen.

Der Hook bei 0x0059896b erhaelt die aktuelle CCamera vor dem Aufbau der Ansicht,
aendert nur den Neigungswinkel bei +0xd86 und ruft den vorhandenen Matrixaufbau
bei 0x0044d6d0 auf. Die Grenze 0..460 entspricht der Spielroutine 0x00449d40;
Besessenheit, Kamerafahrten, Uebergaenge und Eingabesperren bleiben ausgenommen.
Originalbytes werden vor Installation beider Adressen geprueft. Loslassen und
Fokusverlust verwerfen ausstehende Bewegungen. Im Log steht
`middlepitch: aktiv (direkter Kamera-Hook)`.

Voraussetzungen: Linux, bash, Python 3, coreutils, findutils, diffutils, grep, sed. Das Spiel muss einmal
gestartet worden sein, damit sein Wine-Prefix existiert, und beim Einspielen beendet sein.

```bash
./install.sh --res 3840x2160 --dry-run   # zeigt nur, was passieren würde
./install.sh --res 3840x2160
```

Als Auflösung passt die native Auflösung des Bildschirms. Ohne weitere Angaben nimmt das Skript die
Standardpfade von Heroic, `~/Games/Heroic/Dungeon Keeper 2` und
`~/Games/Heroic/Prefixes/default/Dungeon Keeper 2`. Liegt das Spiel woanders, gibt
`--game-dir DIR` den Spielordner an und `--prefix DIR` den Ordner, in dem `user.reg` liegt.

Das Skript

- legt `dsound.dll` und `dk2hires.ini` in den Spielordner und ersetzt dabei keine vorhandene Datei,
- trägt im Prefix für `DKII-DX.exe` die DLL-Override `dsound=native,builtin` ein, damit Wine und
  Proton die DLL aus dem Spielordner laden,
- setzt die Spielauflösung (`Screen Width` und `Screen Height` unter
  `HKCU\Software\Bullfrog Productions Ltd\Dungeon Keeper II\Configuration\Video`),
- sichert vorher `user.reg`, die alten Werte und eine Prüfsummenliste des Spielordners nach
  `~/.local/state/dk2hires/`.

Nach dem ersten Start liegt im Spielordner `dk2hires.log` mit den Zeilen `tilefix: aktiv`,
`heapfix: aktiv`, `camfix: aktiv` und `fontfix: aktiv`. Fehlt die Datei, hat das Spiel die DLL nicht
geladen.

Einzelne Teile lassen sich in `dk2hires.ini` abschalten: `TileFix=0`, `HeapFix=0`, `CameraFix=0`,
`FontRefHeight=0`.

## Entfernen

```bash
./uninstall.sh --dry-run
./uninstall.sh
```

Das entfernt `dsound.dll`, `dk2hires.ini` und `dk2hires.log`, setzt Auflösung und DLL-Override auf
den Stand vor der Installation und vergleicht den Spielordner mit der Prüfsummenliste. Die
Sicherungen in `~/.local/state/dk2hires/` bleiben liegen und können danach weg.

Von Hand: die drei Dateien aus dem Spielordner löschen, in `user.reg` des Prefix den Abschnitt
`[Software\\Wine\\AppDefaults\\DKII-DX.exe\\DllOverrides]` entfernen, während das Spiel nicht läuft,
und die Auflösung im Spiel neu einstellen.

## Kleineres unteres Ingame-Menue

Die lokale Erweiterung bietet unter `[fix]` in `dk2hires.ini` neben der Spiel-EXE:

```ini
MenuScale=75
```

`75` verkleinert Breite und Hoehe des unteren HUD auf 75 Prozent und verankert es
unten links. Erlaubt sind 50 bis 100; `100` stellt die Originalgroesse wieder her.
Ohne diese Option bleibt die Originalgroesse erhalten. Die Skalierung wirkt nur
bei mehr als 768 Bildzeilen. Nach einer Aenderung das Spiel neu starten.
Minimap, Panel-Tabs, Kreaturen-/Raum-/Zauber-/Werkstatt-Buttons und die zum unteren
HUD gehoerenden Benachrichtigungen werden gemeinsam angepasst. Hauptmenue,
Escape-Optionen und obere Infozeile behalten ihre Groesse.

Bei verkleinertem HUD reicht die 3D-Ansicht bis zum unteren Bildschirmrand und
liegt hinter dem HUD. So bleiben neben dem links ausgerichteten Menue keine schwarzen
Streifen aus dessen urspruenglicher Hoehe stehen. Die bisherige Anpassung der
Schnittkante an die skalierte HUD-Oberkante gab nur einen Teil dieser Flaeche frei.
Der Geometrietest prueft die volle Bildhoehe bei 1080p, 1440p, 2160 und 2560
Bildzeilen sowie unveraendertes Verhalten bei 768 Zeilen, 100 Prozent und im
Hauptmenue. Die Darstellung im laufenden Spiel muss noch visuell geprueft werden.

Die Laufzeit-Hooks skalieren die fertigen Zeichnungsauftraege und die getrennten
Trefferbereiche; die urspruenglichen Layoutdaten und Panel-Iconzahlen bleiben
erhalten. Waehrend untere Fenster und Buttons gezeichnet werden, bleiben deren
Layout-Rechtecke unveraendert. Erst die Sprite-Ausgabe skaliert Position und
Groesse gemeinsam, einschliesslich Kreaturenportraets und Ziffern. Die separate
Kopf-/Trennleiste sowie die texturierten Minimap-Polygone und deren Clip-Rechtecke
verwenden dieselbe Transformation. Texturkoordinaten bleiben erhalten. So werden
Teile mit eigener Groessenberechnung nicht mehr ausgelassen oder doppelt skaliert.
Auch der separat gezeichnete Kamera-Sichtumriss samt Nordmarkierung auf der
Minimap verwendet dieselbe Skalierung. Hover-Erkennung, Klickpruefung und das
Rechteck fuer klickbare Bereiche teilen sich die Skalierung der lokalen Insets;
so bleiben rechts-/unten-ausgerichtete Sell-/Zoom-Buttons und nebeneinander
liegende Benachrichtigungen einzeln bedienbar.
Der 3D-Ausschnitt bekommt die frei gewordene Hoehe zurueck. Alle dreizehn Patchstellen
werden vor Installation auf Originalbytes geprueft. Im Log bestaetigt
`menufix: aktiv (Renderpfade v4, Minimap-Buttons)` die korrigierte Fassung.

Die vier Minimap-Eckbuttons verwenden wie die quadratische Minimap die Bildhoehe
fuer horizontale Abstaende und Breiten. Die native ausgeschnittene Hover-Grafik
bleibt erhalten und passt dadurch auch bei Breitbild zur eingebetteten Iconform.
Die empfindliche Flaeche nutzt
die vom echten `CButton::configure` gespeicherten Endkoordinaten; lokale Insets
werden dort nicht ein zweites Mal addiert. Das korrigiert die nach rechts bzw.
unten zu grossen Flaechen von Sell/Zoom/Options. Ohne Skalierung gilt das
Originalverhalten. Tests laden diese vier Button-Konfigurationen direkt aus der
Spiel-EXE und fuehren die echte Konfigurations- und Zeichenroutine aus.
Referenz fuer Adressen/Felder: [DiaLight/Flame](https://github.com/DiaLight/Flame),
Mapping der GOG-Version 1.7. Es wird kein Flame-Code installiert.

Automatisch geprueft mit den echten Geometrieroutinen aus der lokalen Spiel-EXE
ohne deren Start: 50/75/100 Prozent, niedrige/hohe Aufloesung, Fenster- und
Button-Trefferbereiche samt lokalen Abstaenden, verschachtelte/rechts-/unten-
ausgerichtete Controls, Ausschluss des Frontends und 3D-Viewport. Zusaetzlich
werden die echten Routinen fuer Minimap-Rahmen, Kopf-/Trennleiste,
Kreaturenportraets und das achtteilige Minimap-Polygon gegen einen aufzeichnenden
Renderer ausgefuehrt. Dabei werden Endgroesse, Position, Clip-Ruecksetzung,
unveraenderte Texturkoordinaten und 50/75/100 Prozent bei 4K geprueft. Die erste
Fassung skalierte nur Button-Rechtecke; der Benutzertest zeigte die fehlenden
Renderpfade, die diese Tests jetzt abdecken. Der visuelle Ingame-Test der
korrigierten Fassung steht noch aus. Dabei Tabs wechseln, Raum-/Zauber-Icons anklicken,
Minimap und deren Groessenumschaltung testen, Panel ein-/ausklappen und Escape
oeffnen. Besonders Minimap-Inhalte und dynamische Kreaturenlisten visuell pruefen.

Der Windows-Build fuehrt `test-menufix.exe` automatisch aus, wenn `DKII-DX.exe`
im uebergeordneten Spielordner liegt. Separat:

```powershell
.\src\build\test-menufix.exe '..\DKII-DX.exe'
```

## Selbst bauen

```bash
src/build.sh          # Ergebnis: src/build/dsound.dll
```

Gebraucht wird `i686-w64-mingw32-gcc` (MinGW-w64). Der urspruengliche Release-Build
ohne Mausrad-Erweiterung entstand mit gcc 15.2.0 und binutils 2.46 und hatte SHA-256
`172cd5171b48748a8b6c1988cec158e012966c2f9ef97a9a6f192ea823eb3c24`.

Die lokale Windows-Fassung mit Mausrad-Zoom wird mit Visual Studio C++ Build Tools
(x86/x64) und dem Windows SDK gebaut. Der Build fuehrt die automatisierten
DirectInput-Tests aus; er installiert die DLL nicht automatisch:

```powershell
.\src\build.ps1       # Ergebnis: src\build\dsound.dll
# Optionaler Integrationstest mit dem vorhandenen Eingabe-Wrapper:
.\src\build\test-wheelzoom.exe '..\dinput.dll'
```

Andere Compiler oder Versionen erzeugen eine andere Datei. Die erwartete Pruefsumme
der mitgelieferten Windows-DLL steht in `install.ps1`.

## Lizenz

MIT, siehe `LICENSE`. Die Lizenz gilt für den Code in diesem Repository.
