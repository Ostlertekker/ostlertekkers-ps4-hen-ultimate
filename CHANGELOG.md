# Changelog

## v0.4.0 – alle Bereiche (2026-11-06), PKG-Version 01.01

### Hinzugefügt
- **Meine Spiele / Homebrew**: liest installierte Titel aus `/user/app` + `param.sfo` (Name, Title ID, Version, Region), Cheat-Anzahl je Spiel, ✕ springt ins Cheat Center
- **Cheat Center**: liest GoldHEN-Cheats (JSON, SHN inkl. UTF-16; MC4 nur gezählt), Suche nach Name/Title ID/Region, Filter (alle/passend/Favoriten/zuletzt), Versionsprüfung gegen installierte Version, Kategorien, Favoriten, Import von USB (mit Sicherung ersetzter Dateien), Export auf USB, Warnungen bei unpassender/unbekannter Version
- **Theme Center**: Raster mit 24 Themes, Sofortwechsel, Favorit, Zufall, Import/Export
- **GoldHEN-Info** (nur lesend): Datenordner, benötigter Build für die FW, Cheat-/Patch-Zählung, config.ini, plugins.ini mit Prüfung, ob die Plugin-Dateien existieren
- **System**: Firmware, Sprache, Bestätigungstaste, Laufzeit, Speicher intern/USB, Zähler, IP
- **Dateien**: Browser für Dashboard-Daten, GoldHEN-Ordner und USB; Kopieren auf USB, Löschen nur in erlaubten Ordnern mit Bestätigung
- **Netzwerk**: Verbindungsstatus/IP, Hinweis auf GoldHEN-FTP (Port 2121)
- **Backup**: Sicherungen (Dashboard-Daten, optional GoldHEN-Cheats) intern oder auf USB, SHA-256-Prüfung, Wiederherstellen mit Sicherheitskopie
- **Update Center**: Version, manueller Weg über GitHub Releases, Backup vor dem Update
- **Hilfe & Diagnose**: Status aller Bereiche, Protokoll, Export auf USB
- Bildschirmtastatur, Aktionsmenüs (OPTIONS), Bestätigungsdialoge
- Erster Start übernimmt Systemsprache und Bestätigungstaste der PS4

### Geändert
- Listenzeilen ohne Untertitel werden vertikal zentriert

## v0.3.0 – PKG-Build (2026-11-06), PKG-Version 01.00

### Hinzugefügt
- Eigener Software-Renderer (`src/gfx/`): kantengeglättete abgerundete Flächen, Verläufe mit Dithering, Rahmen, Hintergrund-Cache
- Vorgerenderte Schrift-Atlanten (Inter + ✕○△□ aus DejaVu Sans), Kerning
- Natives PS4-Backend: VideoOut (2 Puffer, Vsync), ScePad, UserService – keine Fremdbibliotheken
- Leerlauf-Modus: kein Neuzeichnen, solange sich nichts ändert
- GitHub-Actions-Pipeline: Tests, Vorschau, PS4-PKG als Download
- `docs/INSTALLATION.md` (USB-Installation)

### Geändert
- SDL2/SDL_ttf/SDL_image entfallen für die PS4; die Desktop-Version zeigt dieselben Pixel über SDL2
- Vorschau-Werkzeug erzeugt PNGs direkt mit dem PS4-Renderer (kein Python mehr nötig)
- Fokusrahmen statisch (spart Neuzeichnen)


## [Unreleased] – Phase C (2026-11-06) – v0.3.0

### Hinzugefügt
- App-Hauptschleife mit 13 Bereichen (L1/R1), Screen-Stapel, Toasts, Logdatei mit Rotation
- Controller: Eingabe-Mapping mit Wiederholung, Stick-Totzone, ✕/○-Tausch; räumliche Fokus-Navigation
- Startseite (Hero, Systemstatus, 12 Modul-Kacheln), Modulseiten mit ehrlichem Status, funktionsfähige Einstellungen
- JSON-Parser, i18n (109 Texte DE/EN), Theme-Format + Katalog, atomarer Einstellungsspeicher mit Backup
- 5 Themes aus dem Starter als vollständige Theme-Dateien
- SDL2-Backend (Renderer, Gamepad, Tastatur) für PS4 und Desktop, gemeinsamer Einstieg
- UI-Vorschau-Werkzeug (Aufzeichnung + PNG-Rendering)
- PKG-Grafik icon0.png/pic1.png (eigenes Motiv), Schrift Inter (OFL)
- docs/API_NACHWEIS.md, docs/PHASE_C_BERICHT.md

### Behoben
- `I18n::t()` gab bei fehlender Übersetzung eine hängende Referenz zurück (gefunden mit ASan)

### Geändert
- Schrift-/Bildlösung: SDL2_ttf/SDL2_image statt stb (Ports für OpenOrbis vorhanden)
- `home`, `settings` in `features.json` jetzt `unverified` (gebaut, auf Konsole ungetestet)

## [Unreleased] – Phase B (2026-11-06)

### Hinzugefügt
- Architektur-Dokument mit 13 Modulen, Schichtenmodell, Datenflüssen und Controller-Modell
- Kompatibilitätsdokument: FW 13.52 + GoldHEN v2.4b18.12 mit Quellen
- C++17-Kern: `CapabilityRegistry`, `AppVersion`/`SemVer`, `PathGuard` (Schreibschutz),
  GoldHEN-Cheat-Dateinamen, Region aus Content ID, Kompatibilitätsprüfung, Kategorie-Erkennung,
  GoldHEN-Firmware-Support-Tabelle
- Plattform-Schnittstellen (Renderer, Input, Dateisystem, System, Netzwerk, Audio)
- Service-Schnittstellen (Einstellungen, Themes, Spiele, Cheats, Backup, Update)
- Sprachdateien DE/EN (aus Prototyp übernommen und erweitert)
- JSON-Schemas für Themes und Einstellungen
- PS4-Makefile mit Umgebungsprüfung, Host-Makefile mit 104 Tests

### Geändert
- `config/features.json`: von 6 auf 13 Module, ehrliche Status-Werte

## v1.0 Starter
- Browser-Prototyp, 5 Themes, Modulregister, Portierungsplan
