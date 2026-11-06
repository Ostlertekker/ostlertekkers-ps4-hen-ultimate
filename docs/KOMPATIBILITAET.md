# Kompatibilität – Firmware 13.52 / GoldHEN

Stand: 2026-11-06

## Zielsystem des Nutzers

| Feld | Angabe | Status |
|---|---|---|
| Firmware | 13.52 | Angabe des Nutzers |
| GoldHEN | **v2.4b18.12** (Beta) | Angabe des Nutzers |

## Belegter Stand

| Quelle | Aussage |
|---|---|
| Ko-fi-Shop des Entwicklers SiSTRo – „GoldHEN v2.4b18.11“ | Changelog: „Added support for 13.52“ |
| Ko-fi-Shop SiSTRo – „GoldHEN v2.4b18.12“ | Changelog: „Added support for 13.02, 13.04 and 13.50“ |
| Logic-Sunrise (Bericht zu 2.4b18.12) | Unterstützt 13.02, 13.04, 13.50, 13.52; weiterhin Beta |
| github.com/GoldHEN/GoldHEN/releases | Wird nicht mehr gepflegt (neueste Seite dort: v2.4b18, bis FW 11.00). Neue Builds erscheinen über Ko-fi. |

**Bewertung:** FW 13.52 + GoldHEN v2.4b18.12 ist eine **vom Entwickler unterstützte Beta-Kombination**. Status im Dashboard: `supported_beta`. Nutzerberichte nennen Beta-Probleme (z. B. Rest-Mode auf 13.02, „Enable Game Patch“). Diese betreffen GoldHEN selbst, nicht das Dashboard, werden aber in der Hilfe erwähnt.

Hinweis für Updates (Abschnitt 6 Master-Prompt): Offizielle Bezugsquelle neuer GoldHEN-Builds ist der Ko-fi-Shop von SiSTRo. Spiegel auf GitHub (z. B. Drittkonten) gelten **nicht** als offiziell.

## Relevante GoldHEN-Funktionen (aus Changelogs)

| Funktion | Seit | Bedeutung für das Dashboard |
|---|---|---|
| Cheat-Menü, Formate json/shn/mc4 | 2.2.x / 2.3 | Cheat Center verwaltet genau diese Dateien |
| Cheat Downloader | 2.4b18 | GoldHEN kann Cheats selbst laden → Dashboard-Download ist optional |
| Auto-Apply Cheats beim Spielstart | 2.4b18.3 | Gewählte Cheats werden von GoldHEN automatisch aktiviert → Dashboard zeigt Hinweis, schaltet aber selbst nichts |
| Interne PKG-Installation `/data/pkg` | 2.3 | Möglicher Weg für Dashboard-Updates (Benutzer installiert selbst) |
| Plugins (`/data/GoldHEN/plugins`, `plugins.ini`) | 2.3 | Plugin-Status kann lesend angezeigt werden |
| Config `/data/GoldHEN/config.ini` | 2.1 | Nur lesend, nie schreiben |

## Kompatibilitätsmatrix (wird in Phase H auf der Konsole gefüllt)

| Bereich | FW 13.52 + GoldHEN v2.4b18.12 | Nachweis |
|---|---|---|
| App startet als Homebrew | offen | – |
| Controller-Eingabe | offen | – |
| Einstellungen speichern | offen | – |
| GoldHEN-Erkennung | offen | – |
| Cheat-Ordner lesen | offen | – |
| Cheat-Datei importieren | offen | – |
| Spielebibliothek lesen | offen | – |
| Systeminformationen | offen | – |
| Netzwerk / HTTPS | offen | – |
| Backup auf USB | offen | – |

Ein Feld wird nur auf „OK“ gesetzt, wenn es auf der Konsole getestet wurde.
