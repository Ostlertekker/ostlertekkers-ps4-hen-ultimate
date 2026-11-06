# PS4-System-Theme „OSTLERTEKKERS HEN“ (experimentell)

Ein eigenes Theme für den normalen PS4-Startbildschirm. Es kommt als **eigenes PKG**
(`IV0000-HENT00001_00-OSTLERTEKKERS000.pkg`) und hat mit der App nichts zu tun:
Die App bleibt auch ohne Theme vollständig nutzbar.

## Status: ungetestet

- Gebaut wird es im GitHub-Build mit dem offenen PkgTool (OpenOrbis LibOrbisPkg, MIT),
  **nicht** mit Sonys Werkzeugen. Ob die PS4 so gebaute Theme-Pakete annimmt, ist
  **noch auf keiner Konsole geprüft**.
- Laut ConsoleMods-Wiki gilt für selbst gebaute (Fake-)Themes: Sie lassen sich
  **nach einem Neustart nicht dauerhaft halten**. Nach jedem Neustart der PS4 (und
  erneutem Laden von GoldHEN) muss das Theme unter *Einstellungen → Themes* wieder
  ausgewählt werden.
- Das Theme ist **statisch** (Bild), ohne eigene Musik und ohne eigene Icons.

## Inhalt

| Datei | Größe | Zweck |
|---|---|---|
| `theme.xml` | – | Beschreibung; `format-version` 4.0 und `themecolor` sind Pflicht |
| `texture/background/homescreen.png` | 1920×1080 | Startbildschirm |
| `texture/background/functionscreen.png` | 1920×1080 | Funktionsbereich (oben) |
| `texture/preview.png` | 740×416 | Vorschau in der Theme-Liste |
| `sce_sys/icon0.png` | 512×512 | Symbol |
| `sce_sys/pic1.png` | 1920×1080 | Hintergrundbild des Pakets |
| `sce_sys/param.sfo` | – | `CATEGORY=ac`, `IRO_TAG=2`, `CONTENT_VER=01.00`, `FORMAT=obs` |

Alle Bilder werden von `tools/theme/make_theme.cpp` mit dem App-Renderer erzeugt
(eigene Grafik: Pentagramm, Glut, Funken).

## Quellen

- Aufbau, Größen, `theme.xml`-Schema, `CATEGORY=ac` + `IRO_TAG=2`, Pakettyp
  `pkg_ps4_theme`: **PS4 Ultimate Theme Creator** (MIT),
  `app/config.py`, `app/theme_xml.py`, `app/pkg.py` – dort an einer funktionierenden
  Konsole und einem Sony-Retail-Theme geprüft (mit Sonys Werkzeugen gebaut).
- Pakettyp `pkg_ps4_theme` und `IROTag.SS_THEME = 2`: LibOrbisPkg
  `LibOrbisPkg/GP4/Gp4Project.cs`, `LibOrbisPkg/PKG/Enums.cs`.
- **Änderung an LibOrbisPkg** (`scripts/patch_liborbispkg_theme.py`): Upstream schreibt
  in den PKG-Kopf immer `iro_tag = None`. Für Theme-Pakete wird dort jetzt
  `SS_THEME` eingetragen. Das ist die einzige Änderung.
- Nicht dauerhaft nach Neustart: ConsoleMods-Wiki „PS4:Custom Themes“.

## Installation

1. `IV0000-HENT00001_00-OSTLERTEKKERS000.pkg` aus dem Release ins Hauptverzeichnis des
   USB-Sticks kopieren.
2. GoldHEN-Menü → Package Installer → installieren.
3. *Einstellungen → Themes → Theme auswählen* → „OSTLERTEKKERS HEN Theme“.

Entfernen: *Einstellungen → Speicher → Themes* → löschen.

Bitte melden, was passiert (Theme erscheint / erscheint nicht / Fehlercode).
