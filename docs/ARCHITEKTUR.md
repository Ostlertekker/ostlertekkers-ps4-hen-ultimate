# PS4 HEN ULTIMATE – Phase B: Architektur

Stand: 2026-11-06 · baut auf `docs/PHASE_A_ANALYSE.md` auf

## 1. Leitprinzipien

1. **Fail closed.** Was nicht nachweislich funktioniert, wird im UI als „nicht verfügbar“ bzw. „eingeschränkt“ angezeigt. Unbekannte Firmware oder unbekannte GoldHEN-Version führt nie zu riskanten Aufrufen.
2. **GoldHEN ist extern und read-only.** Das Dashboard liest GoldHEN-Informationen, verändert aber keine GoldHEN-Programmdateien, Payloads oder Konfigurationen. Einzige Schreibausnahme: die dokumentierten Cheat-Ordner (siehe 6.3) – nur nach Bestätigung und mit Backup.
3. **Plattform-Kern getrennt.** Die gesamte Logik (Cheat-DB, Themes, Einstellungen, Versionsprüfung, Pfadschutz) ist plattformneutrales C++17 und wird auf dem PC mit Tests geprüft. Nur die Schicht `platform/ps4` spricht PS4-Bibliotheken an.
4. **Daten statt Code.** Themes, Sprachen, Modulstatus und Cheat-Index sind JSON-Dateien. Themes und Sprachen lassen sich wechseln oder ergänzen, ohne das PKG neu zu installieren.
5. **Keine erfundenen APIs.** Jede PS4-Systemfunktion, die genutzt wird, steht mit Quelle in `docs/API_NACHWEIS.md` (wird ab Phase C geführt). Ohne Nachweis → Funktion bleibt `unavailable`.

## 2. Schichtenmodell

```
┌──────────────────────────────────────────────────────────────┐
│ app/          Hauptschleife, Start, Screen-Stack, Fehlergrenze│
├──────────────────────────────────────────────────────────────┤
│ ui/           Kacheln, Listen, Fokus, Animation, Toasts,     │
│               Dialoge, Layout (1920×1080 logisch, skaliert)  │
├──────────────────────────────────────────────────────────────┤
│ Module (je ein Screen + Logik):                              │
│ home  games  cheats  themes  goldhen  system  files  net     │
│ homebrew  settings  backup  update  diag                     │
├──────────────────────────────────────────────────────────────┤
│ core/         Status/Capability-Registry, Version, Log,      │
│               Result, JSON, i18n                             │
│ files/        PathGuard (Schreibschutz), sichere Datei-Ops   │
├──────────────────────────────────────────────────────────────┤
│ platform/     Schnittstellen: IRenderer, IInput, IFileSystem,│
│               ISystemInfo, INetwork, IAudio                  │
│   ps4/        nativ: VideoOut + ScePad (nur OpenOrbis)       │
│   sdl/        Desktop-Version für Entwicklung (SDL2)         │
└──────────────────────────────────────────────────────────────┘
```

Regel: Module dürfen nur `core/`, `files/`, `ui/` und die **Schnittstellen** aus `platform/` benutzen – nie direkt `platform/ps4`.

## 3. Verzeichnisstruktur

```
PS4_HEN_ULTIMATE/
├── Makefile               PS4-Build (OpenOrbis) → eboot.bin, param.sfo, pkg
├── Makefile.host          PC-Build: Kernlogik + Tests (läuft ohne Toolchain)
├── config/features.json   Modulregister: 13 Module + Status
├── assets/
│   ├── lang/de.json, en.json
│   ├── themes/            mitgelieferte Themes (eine JSON-Datei je Theme)
│   └── fonts/             nur frei lizenzierte Schriften (OFL)
├── schemas/               JSON-Schemas für Theme, Einstellungen, Cheat-Index
├── sce_sys/               icon0.png (512×512), pic1.png (1920×1080); param.sfo wird gebaut
├── src/
│   ├── app/  core/  ui/  i18n/  platform/{ps4,host}/
│   ├── cheats/  themes/  games/  goldhen/  system/  files/
│   ├── net/  homebrew/  settings/  backup/  update/  diag/
├── tests/                 Host-Tests (laufen auf PC)
├── scripts/               Hilfsskripte (Prüfsummen, Release)
├── prototype/             ursprünglicher Browser-Prototyp (Design-Referenz)
└── docs/
```

## 4. Plattform-Entscheidungen

| Thema | Entscheidung | Begründung |
|---|---|---|
| Sprache | C++17 | Wird von der OpenOrbis-libcxx unterstützt; Kern auf PC testbar. |
| Renderer | **eigener Software-Renderer** (`src/gfx/`) hinter `IRenderer`; PS4: VideoOut, Desktop: SDL2 *(v0.3.0: ersetzt den SDL2-Plan, weil der PS4-SDL-Port nicht reproduzierbar baubar dokumentiert ist)* | Identische Pixel auf PC und PS4 → komplett auf dem PC testbar. Austauschbar, weil gekapselt. |
| Schrift | vorgerenderte Atlanten aus Inter (OFL) | keine FreeType-Abhängigkeit auf der PS4; Messung auf PC und Konsole identisch |
| Bilder | PNG-Decoder folgt mit den Modulen, die ihn brauchen (Cover, Hintergründe) | |
| JSON | eigener Parser in `core/json` (umgesetzt in Phase C) | Keine Abhängigkeit, volle Kontrolle über Fehlerverhalten bei kaputten Importdateien. |
| Auflösung | logisches Raster 1920×1080, Skalierung auf Ausgabeauflösung | Alle Layoutwerte in logischen Pixeln. |
| Bildrate | Ziel 60 fps; Animationen zeitbasiert (nicht framebasiert) | Gleiches Tempo auch bei Einbrüchen. |

## 5. Bedienung (Controller-Modell)

| Taste | Funktion |
|---|---|
| D-Pad / linker Stick | Fokus bewegen (Stick mit Totzone + Wiederholrate) |
| ✕ | Öffnen / Bestätigen |
| ○ | Zurück / Abbrechen |
| L1 / R1 | Hauptbereich wechseln (Tab-Leiste oben) |
| △ | Kontextaktion (z. B. Favorit) |
| □ | Suche / Filter |
| OPTIONS | Kontextmenü |

Hinweis: Auf manchen PS4-Systemen ist ○ als Bestätigungstaste eingestellt (Region JP/Asien). Ob und wie diese Systemeinstellung abgefragt werden kann, wird in Phase C geprüft; bis dahin gilt ✕ = Bestätigen und ist in den Einstellungen umschaltbar.

Fokus-System: Jeder Screen ist ein Raster aus fokussierbaren Elementen. Navigation sucht das nächstgelegene Element in Pfeilrichtung (räumliche Navigation), damit beliebige Kachel-Layouts funktionieren.

## 6. Module und Datenflüsse

### 6.1 Status-/Capability-Registry (`core/capability`)

Jede Funktion hat einen Status:

| Status | Bedeutung im UI |
|---|---|
| `available` | funktioniert, getestet |
| `limited` | funktioniert mit Einschränkung (Grund wird angezeigt) |
| `unverified` | Code vorhanden, auf Konsole noch nicht bestätigt |
| `unavailable` | nicht möglich auf diesem System (Grund wird angezeigt) |
| `not_implemented` | noch nicht gebaut |

Beim Start prüft jedes Modul seine Voraussetzungen (Firmware, GoldHEN-Erkennung, Pfade lesbar …) und meldet den Status. Das UI liest ausschließlich diese Registry, um Kacheln, Hinweise und gesperrte Funktionen darzustellen.

### 6.2 GoldHEN-Modul (`goldhen/`) – nur lesend

- Erkennung: Vorhandensein und Inhalt dokumentierter Pfade (z. B. `/data/GoldHEN/`), Version nur wenn sie zuverlässig auslesbar ist. Die konkreten Erkennungsquellen werden in Phase F mit Nachweis festgelegt.
- Firmware: aus dem System, nicht aus GoldHEN abgeleitet.
- Kompatibilitätsbewertung über `docs/KOMPATIBILITAET.md`-Tabelle, die auch im App-Bundle liegt.
- Kein Update, Patch oder Austausch. Hinweise auf neuere Versionen nur als Text mit offizieller Quelle.

### 6.3 Cheat Center (`cheats/`)

Datenquelle: Dateien im GoldHEN-Cheat-Format. Dokumentiert sind:

- Ordner: `/user/data/GoldHEN/cheats/json/`, `…/shn/`, `…/mc4/`
- Dateiname: `{TITLEID}_{VERSION}.{format}`, für Nicht-eboot-Prozesse `{TITLEID}_{VERSION}_{process}.{format}`
- Nur ein Format pro Title ID + Version wird von GoldHEN berücksichtigt.

Ablauf:

```
Cheat-Ordner + Offline-Paket ──► Scanner ──► Index (cheat_index.json im App-Datenordner)
                                                │
installierte Spiele (Title ID, Version) ────────┤
                                                ▼
                          Kompatibilitätsprüfung: Title ID ✓  Version exakt ✓
                                                ▼
                    UI: Suche (Name/Title ID/Region), Kategorien, Favoriten, Verlauf
```

- **Kompatibel** wird nur angezeigt, wenn Title ID **und** Spielversion exakt übereinstimmen. Region ergibt sich aus der Title ID (jede Region hat eine eigene CUSA-Nummer) bzw. dem Content-ID-Präfix des installierten Spiels.
- **Cheats ein-/ausschalten:** Eine dokumentierte Schnittstelle, mit der eine andere App GoldHEN-Cheats im laufenden Spiel schaltet, wurde nicht gefunden. Deshalb: Das Dashboard verwaltet **welche Cheat-Datei** für ein Spiel aktiv ist und welche Einträge als Favorit markiert sind. Das eigentliche Ein-/Ausschalten im Spiel geschieht im GoldHEN-Cheat-Menü. Das UI sagt das deutlich (`limited`).
- **Kategorien** (Leben, Geld, Munition …) werden per Stichwortregeln aus den Cheat-Namen abgeleitet (DE/EN), weil das Dateiformat keine Kategorien enthält. Nicht zuordenbare Einträge → „Sonstige“.
- **Lizenz:** Das offizielle GoldHEN-Cheat-Repository steht unter GPL-3.0. Cheat-Daten werden **nicht ins PKG eingebaut**, sondern auf ausdrücklichen Wunsch aus der offiziellen Quelle geladen oder als ZIP von USB importiert. Die Lizenz wird im UI angezeigt.
- **Import:** Datei wird zuerst in einen Prüfordner kopiert, Dateiname und Inhalt werden validiert, erst dann (nach Bestätigung, mit Backup einer gleichnamigen Datei) in den Cheat-Ordner verschoben.

### 6.4 Spielebibliothek (`games/`) – nur lesend

Liest installierte Titel (Title ID, Name, Version, Content ID, Cover). Die genauen Quellpfade werden in Phase F mit Nachweis festgelegt und fehlertolerant gelesen. Keine Start-, Kopier- oder Änderungsfunktion für fremde Spiele, solange keine dokumentierte Schnittstelle nachgewiesen ist.

### 6.5 Theme Center (`themes/`)

- Ein Theme = eine JSON-Datei (Schema `schemas/theme.schema.json`) + optionale Bilder/Sounds im gleichen Ordner.
- Quellen: mitgelieferte Themes (`assets/themes/`, im PKG, read-only) und eigene Themes im App-Datenordner (`themes/`). Import/Export als ZIP oder Ordner über USB.
- Wechsel zur Laufzeit; das UI liest alle Farben über Rollen (`background`, `surface`, `accent`, `text`, …), nie als feste Werte.

### 6.6 Einstellungen, Profile (`settings/`)

- `settings.json` im App-Datenordner, Schema `schemas/settings.schema.json`, mit `schemaVersion` für spätere Migrationen.
- Speichern atomar: in `settings.json.tmp` schreiben → prüfen → umbenennen. Vorherige Version bleibt als `settings.json.bak`.

### 6.7 Datei-Manager (`files/`) und PathGuard

Alle schreibenden Operationen laufen durch `PathGuard`:

1. Pfad normalisieren (`.`/`..`/doppelte `/` auflösen); relative Pfade werden abgelehnt.
2. `/user/data/…` und `/data/…` werden als gleicher Ort behandelt.
3. Schreiben erlaubt nur in: App-Datenordner, USB-Laufwerken (`/mnt/usb0`–`/mnt/usb7`), den drei GoldHEN-Cheat-Ordnern.
4. Alles andere ist schreibgeschützt, ausdrücklich u. a. `/system*`, `/preinst*`, `/update`, `/user/app`, `/user/patch`, `/system_data`, und `/data/GoldHEN` außerhalb der Cheat-Ordner.
5. Löschen, Überschreiben und Verschieben nur nach Bestätigung.

### 6.8 Backup (`backup/`)

Sichert nur Daten des Dashboards (Einstellungen, eigene Themes, Favoriten, Cheat-Index) und – auf Wunsch – die Cheat-Ordner. Ziel: App-Datenordner oder USB. Jedes Backup hat ein Manifest mit SHA-256 je Datei; Wiederherstellung prüft die Prüfsummen vorher.

### 6.9 Update Center (`update/`)

Nur für das Dashboard selbst. Ablauf: Versionsdatei von einer festen HTTPS-Quelle → Changelog anzeigen → Benutzer bestätigt → Konfig-Backup → PKG herunterladen → SHA-256 prüfen → Installation durch den Benutzer. Ob ein PKG direkt aus der App installiert werden kann, hängt von einer nachweisbaren Schnittstelle ab (Phase F); sonst wird das PKG in einen Ordner gelegt und der Benutzer installiert es über das GoldHEN-Paketinstallationsmenü. GoldHEN-Updates sind nicht Teil dieses Moduls.

### 6.10 System, Netzwerk, Homebrew, Diagnose

- `system/`: Firmware, Speicher, Laufzeit, Netzwerkstatus – jeder Wert einzeln mit Status; nicht abrufbare Werte = „nicht verfügbar“.
- `net/`: Verbindungsstatus, IP; HTTPS-Downloads für Cheat-DB und Updates.
- `homebrew/`: Liste installierter Homebrew-Apps (gleiche Quelle wie `games/`, gefiltert), nur Anzeige.
- `diag/`: Logdatei anzeigen, Diagnose-ZIP auf USB exportieren, Capability-Übersicht.

## 7. App-Datenordner

```
/data/PS4HENULTIMATE/
├── settings.json (+ .bak)
├── themes/           eigene Themes
├── cheats/           cheat_index.json, favorites.json, history.json, staging/
├── backups/
├── logs/             app.log (rotierend, max. 5 × 1 MB)
└── cache/            Cover, Downloads
```

## 8. Fehlerbehandlung und Logging

- Kein Absturz bei kaputten Dateien: jede Lade-Funktion gibt `Result<T>` zurück (Wert oder Fehlercode + Text), Ausnahmen werden an der Modulgrenze abgefangen.
- Ein Modul, das beim Start scheitert, wird als `unavailable` mit Grund markiert; das Dashboard läuft weiter.
- Log-Stufen: ERROR, WARN, INFO, DEBUG. Keine Zugangsdaten im Log.

## 9. Identität des PKG

| Feld | Wert |
|---|---|
| Titel | PS4 HEN ULTIMATE |
| Title ID | `HENU00001` |
| Content ID | `IV0000-HENU00001_00-PS4HENULTIMATE00` |
| Kategorie | `gd` (Anwendung) |
| Version | `01.00` |

Die Werte sind frei gewählt und kollidieren nach aktuellem Wissen mit keiner offiziellen ID; vor dem ersten Release wird das gegen bekannte Homebrew-Listen geprüft.

## 10. Build-Ablauf (Phase G)

```
clang/clang++ (Ziel x86_64-pc-freebsd12-elf) ──► ld.lld ──► app.elf
create-fself ──► eboot.bin
PkgTool.Core sfo_new / sfo_setentry ──► sce_sys/param.sfo
create-gp4 ──► pkg.gp4
PkgTool.Core pkg_build ──► IV0000-HENU00001_00-PS4HENULTIMATE00.pkg
```

Die exakten Compiler- und Linkerflags werden aus dem Sample der installierten Toolchain-Version übernommen (siehe Kommentare im `Makefile`). Ohne installierte Toolchain bricht `make` mit klarer Meldung ab, statt ein unvollständiges Paket zu erzeugen.

## 11. Offene Entscheidungen

| # | Frage | Vorschlag |
|---|---|---|
| E1 | Projektlizenz | **GPL-3.0** – passt zur Lizenz der GoldHEN-Cheat-Daten und erlaubt, Code aus vergleichbaren GPL-Projekten (z. B. PS4 Cheats Manager von bucanero) zu lernen oder zu übernehmen. Alternative MIT wäre nur möglich, wenn kein GPL-Code übernommen wird. |
| E2 | Exakte GoldHEN-Version / Firmware | ✅ geklärt: FW 13.52 + GoldHEN v2.4b18.12 (Beta), vom Entwickler unterstützt – siehe `docs/KOMPATIBILITAET.md`. |
