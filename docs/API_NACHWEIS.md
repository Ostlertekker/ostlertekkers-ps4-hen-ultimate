# API-Nachweis

Jede externe Schnittstelle, die der Code benutzt, steht hier mit Quelle und Prüfstand. Regel: ohne Eintrag kein Aufruf.

Legende: ✅ belegt und getestet · 🟡 öffentlich dokumentiert, im Projekt noch nicht auf PS4 kompiliert/getestet · ❌ nicht belegt → nicht verwendet

## PS4-Plattform (nativ, `src/platform/ps4/ps4_platform.cpp`)

Seit v0.3.0 braucht die PS4-Version **nur die OpenOrbis-Toolchain**. Gezeichnet wird mit dem eigenen Software-Renderer (`src/gfx/`), angezeigt über VideoOut.

| # | Schnittstelle | Quelle | Stand |
|---|---|---|---|
| P1 | `scePadInit`, `scePadOpen(user, type, index, param)`, `scePadReadState(handle, OrbisPadData*)`, `ORBIS_PAD_BUTTON_*`, `OrbisPadData.connected` | OpenOrbis `include/orbis/Pad.h`; SDL3-PS4-Joystick-Treiber nutzt dieselben Aufrufe | ✅ auf Konsole bestätigt (v0.3.0, FW 13.52) |
| P2 | `sceUserServiceInitialize`, `sceUserServiceGetInitialUser` | OpenOrbis-Header, Apollo-PS4, SDL3-PS4-Treiber | ✅ auf Konsole bestätigt (v0.3.0) |
| P3 | `sceVideoOutOpen`, `sceVideoOutSetBufferAttribute`, `sceVideoOutRegisterBuffers`, `sceVideoOutSetFlipRate`, `sceVideoOutSubmitFlip`, `sceVideoOutGetFlipStatus` | OpenOrbis-Grafik-Beispiel (`samples/_common/graphics.cpp`): zwei lineare 1920×1080-Puffer im Abstand von 8.294.400 Byte | ✅ auf Konsole bestätigt (v0.3.0) |
| P4 | `sceKernelAllocateDirectMemory`, `sceKernelMapDirectMemory` (Typ 3, Schutz 0x33), `sceKernelGetDirectMemorySize` | OpenOrbis `docs/MD/PS4 Libraries/Libkernel.md` + Grafik-Beispiel | ✅ auf Konsole bestätigt (v0.3.0) |
| P5 | `sceSystemServiceHideSplashScreen` | in Homebrew-Projekten als notwendig dokumentiert | ✅ auf Konsole bestätigt (v0.3.0) |
| P6 | `sce_module/libc.prx`, `libSceFios2.prx` aus `bin/data/modules/` ins PKG | OpenOrbis v0.5-Changelog | ✅ auf Konsole bestätigt (v0.3.0) |

Zahlenwerte (Pixelformat `0x80000000`, lineares Tiling `1`, Vsync-Flip `1`) stehen als benannte Konstanten im Code, so wie im OpenOrbis-Beispiel verwendet.

| P7 | `sceNetCtlInit`, `sceNetCtlGetInfo(14, …)` (IP-Adresse) | OpenOrbis `include/orbis/NetCtl.h` | 🟡 v0.4.0, auf Konsole ungetestet; Fehler → „nicht verbunden“ |
| P8 | `sceKernelGetSystemSwVersion` (BCD → „13.52“) | OpenOrbis `include/orbis/libkernel.h` | 🟡 v0.4.0 |
| P9 | `sceSystemServiceParamGetInt` (Sprache = 1, Bestätigungstaste = 1000) | OpenOrbis `include/orbis/SystemService.h` | 🟡 v0.4.0; Fehler → Standardwerte |

## Desktop-Plattform (`src/platform/sdl/`, nur Entwicklung)

| # | Schnittstelle | Stand |
|---|---|---|
| S1 | SDL2: Fenster, Streaming-Textur `ARGB8888`, GameController, Tastatur | 🟡 typgeprüft gegen Stub-Header; `make -f Makefile.host app` auf dem Mac |

## Dateisystem (`src/platform/posix_fs.cpp`)

| # | Schnittstelle | Quelle | Stand |
|---|---|---|---|
| F1 | POSIX `open/read/write/fsync/rename/mkdir/opendir/stat/unlink/rmdir` | OpenOrbis-Toolchain linkt gegen eine musl-libc (github.com/OpenOrbis/musl) | ✅ auf PC getestet · 🟡 PS4 |
| F2 | `statvfs` (Speicherplatz) | POSIX/musl | 🟡 auf PS4 unbestätigt; Fehler → „nicht verfügbar“ |
| F3 | `/app0/…` = Dateien aus dem PKG (nur lesen) | Verwendung in OpenOrbis-Samples | 🟡 auf Konsole prüfen |
| F4 | `/data/PS4HENULTIMATE/` = App-Daten (schreibbar) | Übliche Homebrew-Praxis (z. B. LÖVE-PS4: `/data/love/`) | 🟡 |

## GoldHEN

| # | Schnittstelle | Quelle | Stand |
|---|---|---|---|
| G1 | Cheat-Ordner `/user/data/GoldHEN/cheats/{json,shn,mc4}/`, Namensschema `{TITLEID}_{VERSION}[_{process}].{format}` | README GoldHEN_Cheat_Repository | ✅ Parser gegen 4730 Dateien des offiziellen Repos getestet · Zugriff auf Konsole ungetestet |
| G2 | Plugins `/data/GoldHEN/plugins`, `plugins.ini` | consolemods.org (GoldHEN-Doku) | Phase F, nur lesend |
| G3 | `config.ini` `/data/GoldHEN/config.ini` | GoldHEN-Changelog v2.1 | Phase F, nur lesend |
| G4 | Laufende GoldHEN-Version auslesen | – | ❌ keine dokumentierte Quelle gefunden → wird als „nicht abgefragt“ angezeigt |
| G6 | Installierte Spiele: `/user/app/<ID>/`, Metadaten `/system_data/priv/appmeta/<ID>/param.sfo`; Version = max(APP_VER, VERSION) wie im GoldHEN Cheat Manager | GoldHEN_Cheat_Manager-Quellcode, Apollo-PS4 | 🟡 nur lesend, auf Konsole ungetestet |
| G7 | Homebrew-Erkennung: Content-ID beginnt mit `IV` | OpenOrbis/PkgTool-Konvention | 🟡 |
| G5 | Cheats einer laufenden Sitzung aus fremder App schalten | – | ❌ nicht belegt → nicht umgesetzt |

## Systeminformationen

| # | Schnittstelle | Stand |
|---|---|---|
| Y1 | Firmware-Version | ❌ noch nicht belegt → „nicht abgefragt“ (Phase F) |
| Y2 | Systemsprache / O-als-Bestätigung | ❌ noch nicht belegt → in den Einstellungen manuell wählbar |
| Y3 | Laufende Anwendung, Laufzeit, Netzwerk | ❌ Phase F |
