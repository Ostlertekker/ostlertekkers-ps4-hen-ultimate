# Changelog

## v0.6.2 – Modul-Auflösung & Updater-Hinweis (2026-11-07)

### Behoben
- **Installierte Spiele / Spielstart**: Der Konsolen-Log zeigte `0x80020002` (ENOENT) – die App-Sandbox darf `/system/common/lib/*.sprx` nicht per absolutem Pfad öffnen. Die System-Bibliotheken (libSceAppInstUtil, libSceSystemService) werden jetzt zusätzlich über die bereits geladene Modulliste aufgelöst (`sceKernelGetModuleList`/`sceKernelGetModuleInfo`, reines OpenOrbis-SDK, kein Exploit-Code). Scheitert der Pfad-Load, wird das per `sceSysmoduleLoadModuleInternal` geladene Modul anhand des Dateinamens gefunden
- **Online-Updater „HTTP 404"**: Ursache ist ein **privates** GitHub-Repo – GitHub liefert für private Repos ohne Login 404. Lösung: Repo öffentlich schalten (Settings → Danger Zone → Change visibility → Public). Updater-Code bleibt unverändert, zieht Prereleases korrekt
- Leer-Anzeige in „Meine Spiele" nennt jetzt klar die GoldHEN-Voraussetzung und den manuellen Fallback (`cheat_titles.txt`, Zeilen `ID=Name`)

## v0.6.1 – Spiele ohne GoldHEN-Freischaltung, Updater (2026-11-07), PKG-Version 01.05

### Behoben / neu
- **Spiele erscheinen auch, wenn GoldHEN den Sandbox-Zugriff verweigert** („goldhen-sdk: no answer“): Die App fragt den Installationsdienst der PS4 (`sceAppInstUtilAppExists`, kein Root nötig) nach jeder bekannten Title-ID – aus den Cheat-Dateien auf der Konsole und aus der GoldHEN-Cheat-Liste (wird beim Laden im Cheat Center gespeichert, ~5000 Titel). Läuft im Hintergrund mit Fortschrittsanzeige; Version bleibt dann unbekannt
- **Updater / Cheats online**: Kann die PS4 das Server-Zertifikat nicht prüfen, meldet die App das jetzt mit Fehlercode und fragt einmal, ob sie trotzdem verbinden darf. Updates bleiben per SHA-256 geprüft, Cheat-Dateien per Format-Prüfung
- GoldHEN-Diagnose zeigt jetzt den System-Fehlercode (z. B. `errno 78 ENOSYS` = SDK-Schnittstelle nicht aktiv)
- Datenrennen beim Hintergrund-Abgleich (von ThreadSanitizer gefunden) behoben

## v0.6.0 – Spielstart mit Animation, System-Theme (2026-11-06), PKG-Version 01.04

### Hinzugefügt
- **Spiele starten** von der Spielseite („Spiel starten“): eigene Startanimation (Kulisse des gewählten Stils, Spiel-Icon, rotierendes Pentagramm, Lichtstrahlen, Aufprall-Blitz) mit eigenem Klang, danach startet das Spiel über `sceLncUtilLaunchApp` (Aufrufweise wie Itemzflow). ○ bricht während der Animation ab; Fehler werden mit Code und Klartext angezeigt
- Einstellung **Spielstart-Animation** (6 Stile, Zufall, aus)
- **PS4-System-Theme „OSTLERTEKKERS HEN“** als eigenes, experimentelles PKG (`docs/SYSTEM_THEME.md`)

### Geändert
- Bild-Skalierung (Icons) mit Festkomma-Schritten, Kulisse der Startanimation wird als Ebene zwischengespeichert: langsamstes Startbild 86 ms → 11 ms (einkernig, PC)
- Online-Updater wählt im Release gezielt das App-PKG, auch wenn dort weitere PKGs liegen

### Nicht umgesetzt
- **Autostart mit GoldHEN**: GoldHEN hat dafür keinen dokumentierten Weg (Plugins laden nur beim Start eines Spiels/einer App); ein Eingriff in GoldHEN ist ausgeschlossen

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
