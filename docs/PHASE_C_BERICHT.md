# Phase C – Dashboard: Bericht

Stand: 2026-11-06 · Version 0.3.0-phaseC

## Umgesetzt

| Bereich | Inhalt | Geprüft |
|---|---|---|
| Hauptschleife | `App`: Start, 13 Bereiche aus `config/features.json`, ein Screen-Stapel je Bereich, zeitbasierte Animationen, Toasts, Logdatei mit Rotation | Host-Tests |
| Controller | ✕ öffnen, ○ zurück, L1/R1 Bereich wechseln (mit Umlauf), D-Pad + linker Stick (Totzone), Tastenwiederholung, ✕/○-Tausch für JP-Layout | Host-Tests |
| Fokus | räumliche Navigation für beliebige Kachel-Layouts, Fokus-Rahmen mit Puls, animierte Vergrößerung | Host-Tests, Vorschau |
| Startseite | Begrüßung, Zielsystem, Hero-Karte mit „Themes entdecken“, Systemstatus, 12 Modul-Kacheln mit ehrlichem Status | Vorschau |
| Modulseiten | Für jedes noch nicht gebaute Modul: Status, Erklärung, geplante Funktionen, Einschränkungen (z. B. „nur lesend“) | Vorschau, Übersetzungstest |
| Einstellungen | **funktionsfähig**: Sprache, Theme (5), Animationen (voll/reduziert/aus), Bestätigungstaste, Zurücksetzen mit Doppelbestätigung; sofort und atomar gespeichert, Backup-Datei | Host-Tests (inkl. Neustart) |
| DE/EN | 109 Texte je Sprache; Test prüft, dass auf keinem Bildschirm ein unübersetzter Schlüssel erscheint | Host-Tests |
| Themes | Theme-Format mit Validierung (keine Pfade in Bild-/Sounddateinamen), 5 Themes aus dem Starter übernommen | Host-Tests |
| Robustheit | Ohne Sprachdateien/Themes/Konfiguration startet die App trotzdem (Fallback-Theme, minimale Bereiche, Fehler im Log) | Host-Tests |
| SDL2-Backend | Renderer (abgerundete Flächen, Verläufe, Rahmen, Text-Cache), Gamepad + Tastatur, gemeinsamer `main` für PS4/PC | Typ-Check gegen Stub-Header |
| PKG-Grafik | `icon0.png` 512×512, `pic1.png` 1920×1080 (eigenes Motiv, Skript `scripts/make_icons.py`) | angesehen |
| Schrift | Inter (SIL OFL 1.1), Lizenztext liegt bei | – |

## Prüfergebnisse

- `make -f Makefile.host test`: **1104 passed, 0 failed** mit g++ und clang++, `-Wall -Wextra -Wpedantic -Werror`.
- Mit AddressSanitizer + UBSan: fehlerfrei. Dabei wurde ein echter Fehler gefunden und behoben: `I18n::t()` gab bei fehlender Übersetzung eine Referenz auf einen temporären String zurück (Absturzgefahr ohne Sprachdateien).
- Die Zahl der Prüfungen ist hoch, weil der Übersetzungstest jeden gezeichneten Text auf allen 13 Bildschirmen in beiden Sprachen einzeln prüft.

## Nicht in Phase C

- Echter PS4-Build und Konsolentest (Toolchain fehlt in der Entwicklungsumgebung).
- Theme Center, Cheat Center, Spiele, System, Dateien usw. – zeigen ihren Status „Noch nicht umgesetzt“ (Phasen D–F).
- Sounds (IAudio ist vorgesehen, aber noch ohne Umsetzung).
- Firmware-/GoldHEN-Abfrage (siehe `docs/API_NACHWEIS.md`, Y1/G4).

## Bekannte Risiken für den ersten PS4-Build

1. Bibliotheksliste des SDL2-Ports (`LIBS` im Makefile).
2. DS4 als `SDL_GameController` erkannt? Wenn nicht: Joystick-Mapping nötig.
3. `SDL_RenderGeometry` braucht SDL ≥ 2.0.18 im Port.
4. Pfade `/app0` und `/data/OSTLERTEKKERS_HEN` auf der Konsole bestätigen.
