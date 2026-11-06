# Build-Anleitung

## A. PKG automatisch mit GitHub (empfohlen)

Jeder Push startet `.github/workflows/build.yml`:

1. Host-Tests (g++, -Werror) + Tests mit AddressSanitizer/UBSan
2. UI-Vorschau-Bilder (Artifact `ui-preview`)
3. PS4-Build: LLVM 18, .NET 8, OpenOrbis v0.5.4 (`toolchain-llvm-18.tar.gz`) → `make check` → `make`
4. Artifact `PS4_HEN_ULTIMATE-pkg` mit `.pkg`, `SHA256SUMS.txt`, Installationsanleitung

Installation: `docs/INSTALLATION.md`.

## B. Lokal

### Tests (nur C++17-Compiler nötig)

```sh
make -f Makefile.host test
```

### UI-Vorschau (PNG, exakt die PS4-Pixel)

```sh
make -f Makefile.host preview      # -> build/preview/*.png
```

### Desktop-Version (zum Ausprobieren am Mac)

```sh
brew install sdl2 pkg-config
make -f Makefile.host app
./build/host/henu-desktop          # Pfeile, Enter=✕, Esc=○, Q=L1, E=R1
```

### PS4-PKG lokal

| Komponente | Quelle |
|---|---|
| OpenOrbis PS4 Toolchain v0.5.4 | github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/releases |
| LLVM (clang++, ld.lld) | `brew install llvm` / `apt install clang-18 lld-18` |
| .NET Runtime 8 | für PkgTool.Core |

```sh
export OO_PS4_TOOLCHAIN=/pfad/zur/toolchain
make check
make
```

### Assets neu erzeugen (nur bei Änderungen nötig, Python + Pillow)

```sh
python3 scripts/make_fonts.py   # Schrift-Atlanten aus Inter (+ ✕○△□ aus DejaVu Sans)
python3 scripts/make_icons.py   # icon0.png, pic1.png
```

## Status

| Schritt | Status |
|---|---|
| Host-Tests (1215 Prüfungen), ASan/UBSan | ✅ |
| UI-Vorschau mit dem PS4-Renderer | ✅ |
| PS4-Backend | 🟡 typgeprüft gegen Stub-Header; echter Build im CI |
| PKG | ⏳ entsteht im ersten CI-Lauf |
| Test auf Konsole | ❌ |
