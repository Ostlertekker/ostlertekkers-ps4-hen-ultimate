# OSTLERTEKKERS HEN

Zweisprachiges (DE/EN), PS5-inspiriertes Homebrew-Dashboard für die PS4: Spielebibliothek mit Spielseite, Cheat Center (inkl. Online-Download und Bearbeitung), Online-Updater, Start-Intros, Theme Center, GoldHEN-Infos, Systemmonitor, Datei- und Backup-Verwaltung.

## Status: v0.5.0 – Online, Intros, Cheat-Bearbeitung

| | |
|---|---|
| Zielsystem | FW 13.52 + GoldHEN v2.4b18.12 (Beta, vom Entwickler unterstützt) |
| Dashboard | Startseite, Meine Spiele, Cheat Center, Theme Center (24 Themes), GoldHEN-Info, System, Dateien, Netzwerk, Homebrew, Einstellungen, Backup, Update Center, Hilfe & Diagnose |
| Grafik | eigener Software-Renderer → PS4 braucht nur die OpenOrbis-Toolchain |
| Tests | 1543 Prüfungen grün (g++/clang++, -Werror, ASan/UBSan), Cheat-Parser gegen 4730 echte GoldHEN-Cheat-Dateien geprüft |
| PKG | wird von GitHub Actions gebaut (`.github/workflows/build.yml`) |
| Auf der Konsole getestet | Start, Navigation, Controller, Bild, Einstellungen (v0.3/v0.4) bestätigt. Neu in v0.5.0 (Intros/Ton, Internet, Spiele über GoldHEN-SDK, Cheat-Bearbeitung): **noch nicht** |

Installation per USB: `docs/INSTALLATION.md`.

## Inhalt

| Pfad | Inhalt |
|---|---|
| `docs/PHASE_A_ANALYSE.md` | Prüfung des Starter-Projekts |
| `docs/ARCHITEKTUR.md` | Schichten, Module, Datenflüsse, Entscheidungen |
| `docs/KOMPATIBILITAET.md` | FW/GoldHEN-Belege und Testmatrix |
| `docs/BUILD.md` | Build-Anleitung, Abhängigkeiten |
| `docs/INSTALLATION.md` | PKG per USB auf der PS4 installieren |
| `docs/API_NACHWEIS.md` | jede genutzte Schnittstelle mit Quelle und Prüfstand |
| `docs/PHASE_C_BERICHT.md` | was in Phase C umgesetzt und geprüft wurde |
| `docs/preview/` | Screenshots der UI-Vorschau |
| `src/` | C++-Quellcode (Kern + Schnittstellen) |
| `tests/` | Host-Tests |
| `assets/lang/` | Deutsch / Englisch |
| `schemas/` | JSON-Schemas für Themes und Einstellungen |
| `Makefile` / `Makefile.host` | PS4-Build / PC-Tests |
| `prototype/` | ursprünglicher Browser-Prototyp (nur Design-Referenz) |

## Sicherheitsregeln

- GoldHEN bleibt extern und unverändert. Das Dashboard schreibt nie in GoldHEN-Programm- oder Konfigurationsdateien; einzige Ausnahme sind die dokumentierten Cheat-Ordner `/data/GoldHEN/cheats/{json,shn,mc4}` – nur nach Bestätigung, mit Backup.
- Alle Schreibzugriffe laufen durch `PathGuard` (Standard: verboten).
- Kein Autostart, kein Austausch des Systemmenüs, keine Firmware-Updates.
- Cheat-Daten (GPL-3.0, GoldHEN-Cheat-Repository) werden nicht ins PKG eingebaut.

## Schnelltest

```sh
make -f Makefile.host test      # Tests
make -f Makefile.host preview   # Screenshots (Python + Pillow + numpy)
make -f Makefile.host app       # Desktop-Version (brew install sdl2 pkg-config)
```

## Lizenzen

- Schrift Inter: SIL Open Font License 1.1 (`assets/fonts/OFL-Inter.txt`)
- Tastensymbole ✕○△□◂▸ aus DejaVu Sans (`assets/fonts/LICENSE-DejaVu.txt`)
- OpenOrbis-Toolchain: GPL-3.0 (nur Build-Werkzeug)
- Projektlizenz: offen (Vorschlag GPL-3.0, siehe `docs/ARCHITEKTUR.md` E1)
