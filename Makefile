# OSTLERTEKKERS HEN - PS4 build with the OpenOrbis PS4 Toolchain
#
# Requirements (see docs/BUILD.md):
#   - OpenOrbis PS4 Toolchain (v0.5.4 tested in CI), env var OO_PS4_TOOLCHAIN
#   - clang/clang++/ld.lld (LLVM 18 recommended for toolchain-llvm-18)
#   - .NET runtime for PkgTool.Core
# Nothing else: graphics are drawn by our own software renderer and shown
# through VideoOut, input comes from ScePad (OpenOrbis headers/stubs).
#
# Targets:
#   make            -> eboot.bin + sce_sys/param.sfo + pkg
#   make check      -> only verifies the build environment
#   make clean
#
# Compiler/linker flags and tool calls follow the OpenOrbis v0.5 samples.
# Override compilers with e.g.:  make OO_CXX=clang++-18 OO_LD=ld.lld-18

# ---------------------------------------------------------------- identity
TITLE       := OSTLERTEKKERS HEN
TITLE_ID    := HENU00001
CONTENT_ID  := IV0000-HENU00001_00-PS4HENULTIMATE00
APP_VER     := 01.03
CATEGORY    := gd
# GitHub Actions run number (the online updater compares it)
HENU_BUILD  ?= 0

# ---------------------------------------------------------------- toolchain
TOOLCHAIN := $(OO_PS4_TOOLCHAIN)

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
  CDIR := macos
  LLVM_BIN ?= $(shell brew --prefix llvm 2>/dev/null)/bin
  OO_CXX ?= $(LLVM_BIN)/clang++
  OO_LD  ?= $(LLVM_BIN)/ld.lld
else
  CDIR := linux
  OO_CXX ?= clang++
  OO_LD  ?= ld.lld
endif

TOOLBIN := $(TOOLCHAIN)/bin/$(CDIR)

# libjbc (sandbox escape used by the official GoldHEN Cheat Manager) is not
# part of this repository: CI clones bucanero/ps4-libjbc at a fixed commit and
# passes LIBJBC_DIR. Without it the app falls back to the GoldHEN SDK only.
LIBJBC_DIR ?=
OO_OBJCOPY ?= llvm-objcopy

# ---------------------------------------------------------------- sources
BUILD := build/ps4

# Platform-neutral code (same set as Makefile.host) + POSIX file system
CORE_SRC := $(filter-out src/platform/%,$(wildcard src/*/*.cpp)) \
            src/platform/posix_fs.cpp

# Native PS4 backend (VideoOut + ScePad)
PS4_SRC := $(wildcard src/platform/ps4/*.cpp)

SRC  := $(CORE_SRC) $(PS4_SRC)
OBJS := $(patsubst %.cpp,$(BUILD)/%.o,$(SRC))
ELF  := $(BUILD)/henu.elf
OELF := $(BUILD)/henu.oelf

# ---------------------------------------------------------------- flags
# libc++ headers must come before the C headers (otherwise <cmath> as first
# include misses ::abs etc.). gnu++17 keeps musl's POSIX declarations visible.
CXXFLAGS_PS4 := --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -std=gnu++17 \
                -isysroot $(TOOLCHAIN) -isystem $(TOOLCHAIN)/include/c++/v1 -isystem $(TOOLCHAIN)/include \
                -Isrc -I. -Wall -Wextra -O2 -DHENU_PS4=1 -DHENU_HAS_MAIN=1 -DHENU_BUILD=$(HENU_BUILD)

ifneq ($(strip $(LIBJBC_DIR)),)
  CXXFLAGS_PS4 += -DHENU_HAVE_LIBJBC=1 -I$(LIBJBC_DIR)
  LIBJBC_OBJS := $(patsubst $(LIBJBC_DIR)/%.c,$(BUILD)/libjbc/%.o,$(wildcard $(LIBJBC_DIR)/*.c))
  OBJS += $(LIBJBC_OBJS)
endif

LIBS := -lc -lkernel -lc++ -lSceVideoOut -lScePad -lSceUserService -lSceSystemService -lSceNetCtl \
        -lSceAudioOut -lSceSysmodule -lSceNet -lSceSsl -lSceHttp

LDFLAGS := -m elf_x86_64 -pie --script $(TOOLCHAIN)/link.x --eh-frame-hdr \
           -L$(TOOLCHAIN)/lib $(LIBS) $(TOOLCHAIN)/lib/crt1.o

# Stub system modules shipped with the toolchain; they belong in sce_module/
# of every homebrew package (OpenOrbis v0.5 changelog). Their location changed
# between releases (bin/data/modules, samples/*/sce_module), so search for them.
PRX_DIR := $(patsubst %/,%,$(dir $(firstword $(shell find "$(TOOLCHAIN)" -name libc.prx 2>/dev/null))))
SCE_MODULES := $(if $(PRX_DIR),$(foreach m,libc libSceFios2,$(if $(wildcard $(PRX_DIR)/$(m).prx),sce_module/$(m).prx)))
# right.sprx (by IDC) is part of every OpenOrbis sample package (sce_sys/about/).
RIGHT_SPRX_SRC := $(firstword $(shell find "$(TOOLCHAIN)/samples" -path '*sce_sys/about/right.sprx' 2>/dev/null))
RIGHT_SPRX := $(if $(RIGHT_SPRX_SRC),sce_sys/about/right.sprx)

# Files packed into the PKG (relative to project root, = /app0 on the console)
PKG_FILES := eboot.bin sce_sys/param.sfo sce_sys/icon0.png sce_sys/pic1.png $(RIGHT_SPRX) $(SCE_MODULES) \
             $(wildcard assets/lang/*.json) $(wildcard assets/themes/*.json) \
             $(wildcard assets/fonts/*.hfnt) assets/fonts/OFL-Inter.txt assets/fonts/LICENSE-DejaVu.txt \
             config/features.json

PKG := $(CONTENT_ID).pkg

# ---------------------------------------------------------------- rules
.PHONY: all check clean

all: check $(PKG)

check:
	@test -n "$(TOOLCHAIN)" || { echo "FEHLER: OO_PS4_TOOLCHAIN ist nicht gesetzt (siehe docs/BUILD.md)."; exit 1; }
	@test -f "$(TOOLCHAIN)/link.x" || { echo "FEHLER: $(TOOLCHAIN)/link.x fehlt - Toolchain unvollständig."; exit 1; }
	@test -f "$(TOOLCHAIN)/lib/crt1.o" || { echo "FEHLER: $(TOOLCHAIN)/lib/crt1.o fehlt."; exit 1; }
	@test -f "$(TOOLCHAIN)/include/orbis/VideoOut.h" || { echo "FEHLER: OpenOrbis-Header fehlen."; exit 1; }
	@test -x "$(TOOLBIN)/create-fself" || { echo "FEHLER: $(TOOLBIN)/create-fself fehlt oder ist nicht ausführbar."; exit 1; }
	@test -x "$(TOOLBIN)/create-gp4" || { echo "FEHLER: $(TOOLBIN)/create-gp4 fehlt oder ist nicht ausführbar."; exit 1; }
	@test -e "$(TOOLBIN)/PkgTool.Core" || { echo "FEHLER: $(TOOLBIN)/PkgTool.Core fehlt."; exit 1; }
	@command -v $(OO_CXX) >/dev/null 2>&1 || { echo "FEHLER: Compiler $(OO_CXX) nicht gefunden."; exit 1; }
	@command -v $(OO_LD) >/dev/null 2>&1 || { echo "FEHLER: Linker $(OO_LD) nicht gefunden."; exit 1; }
	@if [ -n "$(PRX_DIR)" ]; then echo "sce_module aus: $(PRX_DIR) -> $(SCE_MODULES)"; else echo "WARNUNG: keine libc.prx in der Toolchain gefunden - PKG ohne sce_module."; fi
	@test -f sce_sys/icon0.png || { echo "FEHLER: sce_sys/icon0.png fehlt (python3 scripts/make_icons.py)."; exit 1; }
	@test -n "$(wildcard assets/fonts/*.hfnt)" || { echo "FEHLER: Schrift-Atlanten fehlen (python3 scripts/make_fonts.py)."; exit 1; }
	@echo "Build-Umgebung OK: $(TOOLCHAIN)"

sce_module/%.prx:
	@mkdir -p sce_module
	cp "$(PRX_DIR)/$*.prx" $@

$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(OO_CXX) $(CXXFLAGS_PS4) -MMD -MP -c $< -o $@

# libjbc: freestanding C. Its private open/close wrappers are renamed so they
# can never replace the libc functions of the whole app.
$(BUILD)/libjbc/%.o: $(LIBJBC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(subst clang++,clang,$(OO_CXX)) --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -O2 \
	    -nostdlib -ffreestanding -mno-red-zone -I$(LIBJBC_DIR) -c $< -o $@
	$(OO_OBJCOPY) --redefine-sym open=henu_jbc_open --redefine-sym close=henu_jbc_close $@

-include $(filter-out $(LIBJBC_OBJS:.o=.d),$(OBJS:.o=.d))

$(ELF): $(OBJS)
	$(OO_LD) $(OBJS) -o $@ $(LDFLAGS)

eboot.bin: $(ELF)
	$(TOOLBIN)/create-fself -in=$(ELF) -out=$(OELF) --eboot "eboot.bin" --paid 0x3800000000000011

sce_sys/param.sfo: Makefile
	$(TOOLBIN)/PkgTool.Core sfo_new $@
	$(TOOLBIN)/PkgTool.Core sfo_setentry $@ APP_TYPE   --type Integer --maxsize 4   --value 1
	$(TOOLBIN)/PkgTool.Core sfo_setentry $@ APP_VER    --type Utf8    --maxsize 8   --value '$(APP_VER)'
	$(TOOLBIN)/PkgTool.Core sfo_setentry $@ ATTRIBUTE  --type Integer --maxsize 4   --value 0
	$(TOOLBIN)/PkgTool.Core sfo_setentry $@ CATEGORY   --type Utf8    --maxsize 4   --value '$(CATEGORY)'
	$(TOOLBIN)/PkgTool.Core sfo_setentry $@ CONTENT_ID --type Utf8    --maxsize 48  --value '$(CONTENT_ID)'
	$(TOOLBIN)/PkgTool.Core sfo_setentry $@ DOWNLOAD_DATA_SIZE --type Integer --maxsize 4 --value 0
	$(TOOLBIN)/PkgTool.Core sfo_setentry $@ SYSTEM_VER --type Integer --maxsize 4   --value 0
	$(TOOLBIN)/PkgTool.Core sfo_setentry $@ TITLE      --type Utf8    --maxsize 128 --value '$(TITLE)'
	$(TOOLBIN)/PkgTool.Core sfo_setentry $@ TITLE_ID   --type Utf8    --maxsize 12  --value '$(TITLE_ID)'
	$(TOOLBIN)/PkgTool.Core sfo_setentry $@ VERSION    --type Utf8    --maxsize 8   --value '$(APP_VER)'

sce_sys/about/right.sprx:
	@mkdir -p sce_sys/about
	cp "$(RIGHT_SPRX_SRC)" $@

pkg.gp4: eboot.bin sce_sys/param.sfo $(RIGHT_SPRX) $(SCE_MODULES)
	$(TOOLBIN)/create-gp4 -out $@ --content-id=$(CONTENT_ID) --files "$(PKG_FILES)"
	python3 scripts/fix_gp4.py $@

$(PKG): pkg.gp4
	$(TOOLBIN)/PkgTool.Core pkg_build pkg.gp4 .
	@echo "PKG erstellt: $(PKG)  (noch NICHT auf der Konsole getestet)"

clean:
	rm -rf $(BUILD) eboot.bin sce_sys/param.sfo sce_sys/about pkg.gp4 *.pkg sce_module
