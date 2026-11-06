# OSTLERTEKKERS HEN – Phase A: Projektprüfung

Stand: 2026-11-06 · Grundlage: `PS4_HEN_ULTIMATE_v1_starter.zip` (7 Dateien, ca. 18 KB)

## 1. Inhalt des Archivs

| Datei | Größe | Art | Bewertung |
|---|---|---|---|
| `README.md` | 2,2 KB | Doku | Klare Statusangabe („kein PKG“), Sicherheitsregeln, 7-Schritte-Plan. Übernehmen. |
| `ps4-native/README.md` | 1,5 KB | Spezifikation | Schichten `ui/ platform/ storage/ compat/ goldhen/` und 7 Abnahmekriterien. Wird Basis der Architektur (Phase B). |
| `config/features.json` | 0,6 KB | Modul-Register | 6 Module mit Statusfeld. Gutes Konzept, aber nur 6 von 13 geforderten Modulen. |
| `config/themes.json` | 0,7 KB | Theme-Katalog | 5 Themes (3 aktiv, 2 `included:false`), nur 2 Farben je Theme. |
| `prototype/index.html` | 4,3 KB | Browser-Mockup | Home-Ansicht: Topbar, Hero, 6 Kacheln, Status-Panel, Theme-Swatches. |
| `prototype/style.css` | 5,5 KB | Design | PS5-artiges Dark-Design, Verläufe, Kachel-Grid 4 Spalten, Hover-Animation. |
| `prototype/app.js` | 2,9 KB | Logik | DE/EN-Wörterbuch, Sprachumschaltung, Theme-Wechsel, Toast „Modul folgt“. |

Es gibt **keinen** C/C++-Code, kein Makefile, keine `param.sfo`, kein GP4, kein EBOOT, keine Assets (Bilder, Fonts, Sounds).

## 2. Technische Prüfung (tatsächlich ausgeführt)

| Prüfung | Ergebnis |
|---|---|
| `node --check prototype/app.js` | OK, keine Syntaxfehler |
| JSON-Validierung `themes.json`, `features.json` | OK |
| Alle `data-i18n`-Schlüssel aus `index.html` in DE und EN vorhanden | OK, keine fehlenden Schlüssel |

Gefundene Mängel im Prototyp:

1. **Fest eingebaute Texte ohne Übersetzung:** „GoldHEN integration: not queried“ (auch in DE nur Englisch), „HOME · CONTROL CENTER“, „UI CONCEPT 1.0“, „PS4 · FW 13.52“.
2. **Theme-Farben doppelt gepflegt:** `app.js` hat eigene Farbwerte und liest `themes.json` nicht. `style.css` hat die Swatch-Verläufe noch ein drittes Mal (`.s1–.s3`).
3. **Keine Controller- bzw. Tastaturnavigation:** Nur Maus-Klick und `:hover`. Es gibt keine Fokuslogik, kein D-Pad-Modell, kein X/O/L1/R1.
4. **Keine Speicherung:** Sprache und Theme gehen beim Neuladen verloren.
5. **Kacheln ohne Funktion:** Jede Kachel zeigt nur den Toast „Modul folgt“. Das ist korrekt gekennzeichnet, also keine falsche Attrappe.
6. **Kein Light-Mode**, keine Hintergrundbilder, keine Transparenz-Einstellung.
7. `innerHTML` wird für Übersetzungen genutzt (wegen `<br>`). Für den Prototyp unkritisch; im nativen Code gibt es kein HTML.

Wichtig: Der Prototyp ist HTML/CSS/JS und **läuft nicht auf der PS4**. Er dient nur als Design-Referenz. Für das native Dashboard müssen Layout, Farben, Typografie und Animationen in C/C++ mit einem eigenen Renderer neu umgesetzt werden. Übernommen werden können: Design-Sprache, Texte (DE/EN), Theme-Daten, Modul-Register und die Sicherheitsregeln.

## 3. Funktionsabgleich: vorhanden vs. gefordert

Legende: ✅ vorhanden · 🟡 nur als Mockup/Konzept · ❌ fehlt

### 3.1 Plattform & Build

| Anforderung | Status | Anmerkung |
|---|---|---|
| Nativer C/C++-Code | ❌ | |
| OpenOrbis-Projekt (Makefile/CMake) | ❌ | |
| `param.sfo`, GP4, `sce_sys/`-Assets (icon0.png, pic1.png) | ❌ | |
| EBOOT.BIN / PKG | ❌ | In dieser Sitzung nicht baubar, Toolchain fehlt |
| Build-Skripte | ❌ | |
| Fehlerprotokollierung | ❌ | |
| README | ✅ | Ehrlich formuliert, wird erweitert |
| Changelog, Testanleitung | ❌ | |

### 3.2 Dashboard-Oberfläche (Abschnitt 2)

| Anforderung | Status |
|---|---|
| Große moderne Kacheln | 🟡 Browser-Mockup |
| Flüssige Animationen / Übergänge | 🟡 nur Hover-Anhebung |
| Dark-Mode | 🟡 |
| Light-Mode | ❌ |
| Hintergrundbilder | ❌ |
| Transparente Oberflächen | ❌ |
| Frei konfigurierbare Farben | 🟡 3 feste Paletten |
| D-Pad / Analogstick | ❌ |
| X öffnen / O zurück | ❌ |
| L1/R1 Hauptbereiche | ❌ |
| Fokusmarkierung | ❌ |
| Skalierbare Darstellung | 🟡 CSS-Responsive, nicht 720p/1080p/4K-nativ |

### 3.3 Module (Abschnitt 3)

| # | Modul | Im Starter | Status |
|---|---|---|---|
| 1 | Home | ✅ | 🟡 Mockup |
| 2 | Meine Spiele | ✅ `games` | 🟡 nur Kachel |
| 3 | Cheat Center | – | ❌ |
| 4 | Theme Center | ✅ `themes` | 🟡 3 Swatches |
| 5 | GoldHEN-Info | ✅ `goldhen` | 🟡 „nicht abgefragt“ |
| 6 | Systeminfo | ✅ `system` | 🟡 nur Kachel |
| 7 | Datei-Manager | ✅ `files` | ❌ `not-implemented` |
| 8 | Netzwerk | – | ❌ |
| 9 | Homebrew-Verwaltung | – | ❌ |
| 10 | Profile & Einstellungen | ✅ `settings` | 🟡 nur Sprache/Theme |
| 11 | Backup & Wiederherstellung | – | ❌ |
| 12 | Update Center | – | ❌ |
| 13 | Hilfe & Diagnose | – | ❌ |

→ 6 von 13 Modulen sind als Kachel angelegt, **keines ist funktionsfähig**. 7 Module fehlen komplett.

### 3.4 Cheat Center (Abschnitt 4)

Komplett fehlend: Datenbank, Suche (Name / Title ID / Region), Versionsprüfung, Kategorien, Ein/Aus, Favoriten, Verlauf, Import/Export, Download, Offline-DB, Updates, Fehlermeldungen, Versionswarnung.

### 3.5 Theme Center (Abschnitt 5)

| Anforderung | Status |
|---|---|
| ≥ 20 Themes | ❌ (5 definiert, 3 aktiv) |
| Kategorien PS5 / Dark / Neon / Minimal / Gaming | ❌ |
| Eigene Hintergründe, Icons, Farben | ❌ |
| Transparenz, Animationen, Sound | ❌ |
| Favoriten, Vorschau, Import/Export, Zufall | ❌ |
| Wechsel ohne Neuinstallation | 🟡 im Browser ja; nativ fehlt das Datenformat |

### 3.6 Weitere Bereiche

| Bereich | Status |
|---|---|
| GoldHEN-Erkennung (Abschnitt 6) | ❌ bewusst „not queried“, richtig so |
| Spielebibliothek (Abschnitt 7) | ❌ |
| Datei-Manager & Backups (Abschnitt 8) | ❌ |
| Systemmonitor (Abschnitt 9) | ❌ |
| Einstellungen, dauerhaft gespeichert (Abschnitt 10) | ❌ |
| Update-System (Abschnitt 11) | ❌ |
| Autostart (Abschnitt 12) | ❌ und laut README zu Recht nicht versprochen |

## 4. Was vom Starter erhalten bleibt

1. **Sicherheitsprinzipien** aus beiden READMEs → werden 1:1 in die Architektur übernommen (GoldHEN read-only, „fail closed“ bei unbekannter Firmware, kein Autostart-Versprechen).
2. **Schichtenmodell** `ui / platform / storage / compat / goldhen` → wird erweitert um `cheats / themes / games / files / backup / update / net / log`.
3. **Modul-Register mit Statusfeld** (`features.json`) → wird zum zentralen Mechanismus, um jede Funktion im UI als „verfügbar / eingeschränkt / nicht verfügbar“ zu kennzeichnen.
4. **Theme-Katalog als JSON** → wird zum erweiterten Theme-Format (mehr Farbrollen, Hintergrund, Transparenz, Sound, Kategorie).
5. **DE/EN-Texte** → werden in Sprachdateien (`lang/de.json`, `lang/en.json`) überführt.
6. **Design-Sprache** des Prototyps → Referenz für den nativen Renderer. Der Prototyp bleibt als `prototype/` im Repo.

## 5. Offene Punkte und Risiken (vor Phase B zu klären)

| # | Punkt | Status | Folge |
|---|---|---|---|
| R1 | **GoldHEN auf FW 13.52** | **Unbestätigt.** Offizielle GoldHEN-Release-Notes, die gefunden wurden, nennen max. FW 11.00; für 13.52 gibt es nur Forenberichte. | Erkennung wird zur Laufzeit gemacht, nichts wird angenommen. Exakte GoldHEN-Version vom Nutzer nötig. |
| R2 | **OpenOrbis-Toolchain** | In dieser Build-Umgebung nicht vorhanden; Paket-Downloads sind gesperrt. | Code und Build-System werden geschrieben, aber nicht kompiliert. Kein EBOOT/PKG aus dieser Sitzung. Build lokal beim Nutzer. |
| R3 | **Renderer** | OpenOrbis bietet einen SDL2-Port und eigene Beispiele (u. a. 2D-Grafik direkt über VideoOut). Welche Variante mit der aktuellen Toolchain-Version sauber baut, muss im Upstream-Repo geprüft werden. | Entscheidung in Phase B; Renderer wird hinter einer Schnittstelle gekapselt, damit er austauschbar bleibt. |
| R4 | **GoldHEN-Cheat-Schnittstelle** | Ob GoldHEN eine dokumentierte API zum Aktivieren von Cheats aus einer anderen App bietet, ist ungeklärt. | Bis zum Nachweis: Cheat Center verwaltet nur Cheat-Dateien (anzeigen, prüfen, importieren, exportieren). Aktivierung erfolgt im GoldHEN-Menü im Spiel. Wird im UI so angezeigt. |
| R5 | **Lizenz der Cheat-Daten** | Lizenz öffentlicher Cheat-Sammlungen (z. B. das GoldHEN-Cheat-Repository) ist noch nicht geprüft. | Keine Cheat-Daten mitliefern, bevor die Lizenz geklärt ist. Download nur aus Quellen mit geprüfter Lizenz. |
| R6 | **Spieleerkennung** | Lesen installierter Spiele über Systempfade ist in der Szene verbreitet, aber nicht offiziell dokumentiert. | Nur lesend, mit Fehlertoleranz, Werte bei Fehlschlag als „nicht verfügbar“. Spiele werden nicht gestartet, kopiert oder verändert. |
| R7 | **Assets** | Keine Icons, Hintergründe, Fonts, Sounds im Starter. | Eigene, lizenzfreie Assets erstellen (prozedural/selbst gezeichnet); Fonts nur mit freier Lizenz (z. B. OFL). |

## 6. Ergebnis Phase A

- Der Starter ist ein **ehrliches Design-Konzept**, kein lauffähiges Programm. Er enthält 0 Zeilen nativen Code.
- Wiederverwendbar: Design, Texte, Theme-/Modul-Daten, Sicherheitsregeln, Schichtenplan.
- Umfang bis zum Ziel: alle 13 Module, Renderer, Input, Speicherung, Build-System und PKG-Layout müssen neu entstehen.
- Nächster Schritt: **Phase B – Architektur** (Verzeichnisstruktur, Modulgrenzen, Datenformate für Themes/Cheats/Einstellungen, Renderer-Entscheidung, Build-Gerüst).
