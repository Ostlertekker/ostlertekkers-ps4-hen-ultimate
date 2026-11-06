# Changelog

## v0.5.1 – Spiele-Erkennung, flüssiges Intro (2026-11-06), PKG-Version 01.03

### Behoben
- **Spiele wurden nicht angezeigt**: Der Zugriffstest akzeptierte einen leeren `/user/app`-Ordner der App-Sandbox als „sichtbar“ und versuchte die GoldHEN-Freischaltung gar nicht erst. Jetzt zählt nur ein Ordner mit echten Title-IDs. Zusätzlich dient `/system_data/priv/appmeta` als zweite Quelle, und Ordner werden auch erkannt, wenn `stat` verweigert wird
- **Intro ruckelte**: Der Renderer ist rund 3-mal schneller (Pixelmischung ohne Divisionen, direkte Füllung voller Flächen, Polygone mit aktiver Kantenliste) und rendert jetzt auf bis zu 6 CPU-Kernen in Streifen; das Bild ist pixelgleich mit dem Einkern-Ergebnis (Test)

## v0.5.0 – OSTLERTEKKERS HEN (2026-11-06), PKG-Version 01.02

### Hinzugefügt
- Neuer Name **OSTLERTEKKERS HEN**, Pentagramm-Logo (Vektor, in App und PKG-Grafik)
- **Start-Intros**: 6 eigene Animationen (Inferno, Polygone, Nebel, Wellen, Lichtspuren, Glanz), wählbar oder zufällig, mit Taste überspringbar. Keine Logos, Namen, Klänge oder Bildmotive von Konsolenherstellern
- **Startsound**: eigener, im Programm erzeugter Klang je Intro; alternativ eigene `boot.wav` (über Einstellungen von USB übernehmen); Lautstärke einstellbar
- **Online-Updater**: prüft GitHub Releases, lädt das PKG, prüft SHA-256, legt es in `/data/pkg` ab (Installation über den Package Installer)
- **Cheats online**: GoldHEN Cheat Repository durchsuchen und Dateien für installierte Spiele laden (Kennzeichnung „passt“ / „andere Version“ / „vorhanden“)
- **Spielseite** in „Meine Spiele“: Infos, Cheats des Spiels, online laden, USB-Import, Export (Infodatei + Cheat-Dateien), Favorit
- **Cheats bearbeiten** (JSON und SHN): einzelne Cheats entfernen oder umbenennen, Original wiederherstellen, Datei löschen – vor der ersten Änderung wird das Original gesichert
- Einmalige Abfrage, bevor die App ins Internet geht
- Installierte Spiele werden über das GoldHEN-SDK gelesen (die App-Sandbox sperrt `/user/app` sonst)

### Geändert
- Datenordner jetzt `/data/OSTLERTEKKERS_HEN` (alter Ordner `/data/PS4HENULTIMATE` wird beim ersten Start übernommen)
- Startseite zeigt echte Firmware und GoldHEN-Status
- Nach dem Ende eines Downloads oder einer Animation wird sofort neu gezeichnet (vorher erst nach dem nächsten Tastendruck)

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
