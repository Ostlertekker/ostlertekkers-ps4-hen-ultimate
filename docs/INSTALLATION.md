# Installation per USB-Stick

Für: PS4 mit Firmware 13.52 und GoldHEN v2.4b18.12 (aktiv).

## 1. PKG holen

Die PKG wird von GitHub Actions gebaut: Repository → **Actions** → letzter grüner Lauf „Build PS4 PKG“ → unten bei **Artifacts** → `PS4_HEN_ULTIMATE-pkg` herunterladen und entpacken.

Darin:
- `IV0000-HENU00001_00-PS4HENULTIMATE00.pkg`
- `SHA256SUMS.txt` (Prüfsumme, optional vergleichen: `shasum -a 256 *.pkg`)

## 2. USB-Stick vorbereiten

1. Stick als **exFAT** oder **FAT32** formatieren (Mac: Festplattendienstprogramm → Löschen → Format „ExFAT“, Schema „Master Boot Record“).
2. Die `.pkg`-Datei **direkt ins Hauptverzeichnis** des Sticks kopieren (nicht in einen Ordner).
3. Stick sicher auswerfen.

## 3. Auf der PS4 installieren

1. GoldHEN wie gewohnt starten (muss nach jedem Neustart aktiv sein).
2. USB-Stick an die PS4 anstecken.
3. **Einstellungen → Debug-Einstellungen → Spiel → Paket-Installationsprogramm** (Englisch: *Settings → Debug Settings → Game → Package Installer*).
4. Den USB-Speicher wählen und `IV0000-HENU00001_00-PS4HENULTIMATE00.pkg` installieren.
5. Danach liegt **OSTLERTEKKERS HEN** als eigene Kachel im Startbildschirm.

## 4. Bedienung

| Taste | Funktion |
|---|---|
| D-Pad / linker Stick | Fokus bewegen |
| ✕ | Öffnen / Ändern |
| ○ | Zurück (im Bereich → Startseite) |
| L1 / R1 | Bereich wechseln |
| PS-Taste | zurück ins System-Menü (App läuft weiter) |

In **Einstellungen** kannst du Sprache, Theme, Animationen und die Bestätigungstaste (✕ oder ○) ändern; alles wird sofort gespeichert unter `/data/OSTLERTEKKERS_HEN/`.

## 5. Wenn etwas nicht klappt

| Problem | Was tun |
|---|---|
| „Debug-Einstellungen“ fehlen | GoldHEN ist nicht aktiv → GoldHEN erneut laden. |
| PKG wird auf dem Stick nicht gefunden | Datei liegt in einem Ordner oder Stick ist NTFS → ins Hauptverzeichnis, exFAT/FAT32. |
| App startet mit schwarzem Bild / stürzt ab | Bitte melden: Protokoll liegt unter `/data/OSTLERTEKKERS_HEN/logs/app.log` (per GoldHEN-FTP auf Port 2121 abrufbar). |
| Controller reagiert nicht | Controller einmal aus- und wieder einschalten, App neu starten; bitte melden. |

## 6. Deinstallieren

Kachel im Startbildschirm → OPTIONS → Löschen. Die Einstellungen bleiben in `/data/OSTLERTEKKERS_HEN/` liegen und können per FTP gelöscht werden. GoldHEN wird von der App nicht verändert.

## Status

Diese Version (01.04 / v0.6.0) heißt **OSTLERTEKKERS HEN**. Neu: Spiele aus der App starten (mit Startanimation). Im Release liegt zusätzlich ein experimentelles PS4-System-Theme als eigenes PKG – siehe `docs/SYSTEM_THEME.md`. Auf der Konsole bestätigt sind bisher Start, Navigation, Controller, Bildausgabe und Einstellungen. Neu und **noch nicht auf einer echten PS4 getestet**: Start-Intro mit Ton, Internet (Cheat-Download, Updater), Einlesen der installierten Spiele über das GoldHEN-SDK, Bearbeiten von Cheats.

Eingeschränkt (in der App gekennzeichnet):
- Cheats werden verwaltet, bearbeitet und geladen; **ein- und ausgeschaltet werden sie im Spiel über das GoldHEN-Menü**.
- Der Updater legt das neue PKG in `/data/pkg` ab; installiert wird es über den Package Installer (GoldHEN-Menü oder Debug-Einstellungen).
- `.mc4`-Dateien sind verschlüsselt und werden nur gezählt.

Eigener Startsound: eine Datei `boot.wav` (PCM oder Float, mono/stereo, max. 30 s) ins Hauptverzeichnis des USB-Sticks legen, dann Einstellungen → „boot.wav von USB übernehmen“.

Update von 01.01: das neue PKG installieren. Lehnt die PS4 ab, die alte App löschen; Einstellungen bleiben erhalten (der Ordner `/data/PS4HENULTIMATE` wird automatisch nach `/data/OSTLERTEKKERS_HEN` übernommen).
