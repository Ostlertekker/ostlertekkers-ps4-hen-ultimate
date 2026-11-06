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
5. Danach liegt **PS4 HEN ULTIMATE** als eigene Kachel im Startbildschirm.

## 4. Bedienung

| Taste | Funktion |
|---|---|
| D-Pad / linker Stick | Fokus bewegen |
| ✕ | Öffnen / Ändern |
| ○ | Zurück (im Bereich → Startseite) |
| L1 / R1 | Bereich wechseln |
| PS-Taste | zurück ins System-Menü (App läuft weiter) |

In **Einstellungen** kannst du Sprache, Theme, Animationen und die Bestätigungstaste (✕ oder ○) ändern; alles wird sofort gespeichert unter `/data/PS4HENULTIMATE/`.

## 5. Wenn etwas nicht klappt

| Problem | Was tun |
|---|---|
| „Debug-Einstellungen“ fehlen | GoldHEN ist nicht aktiv → GoldHEN erneut laden. |
| PKG wird auf dem Stick nicht gefunden | Datei liegt in einem Ordner oder Stick ist NTFS → ins Hauptverzeichnis, exFAT/FAT32. |
| App startet mit schwarzem Bild / stürzt ab | Bitte melden: Protokoll liegt unter `/data/PS4HENULTIMATE/logs/app.log` (per GoldHEN-FTP auf Port 2121 abrufbar). |
| Controller reagiert nicht | Controller einmal aus- und wieder einschalten, App neu starten; bitte melden. |

## 6. Deinstallieren

Kachel im Startbildschirm → OPTIONS → Löschen. Die Einstellungen bleiben in `/data/PS4HENULTIMATE/` liegen und können per FTP gelöscht werden. GoldHEN wird von der App nicht verändert.

## Status

Diese Version (01.00 / v0.3.0) ist der **erste Konsolen-Test**. Sie wurde nie zuvor auf einer PS4 gestartet. Funktionsfähig sind Startseite, Navigation, Sprache und Einstellungen; alle anderen Bereiche zeigen ehrlich „Noch nicht umgesetzt“.
