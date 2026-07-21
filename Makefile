# ***************************************************************************************
# ***************************************************************************************
#
# Name     : Makefile
# Author   : Paul Robson (paul@robsons.org.uk)
# Date     : 20th November 2023
# Reviewed : No
# Purpose  : Main firmware makefile, most of the work is done by CMake.
#
# ***************************************************************************************
# ***************************************************************************************

ifeq ($(OS),Windows_NT)
include build_env\common.make
else
include build_env/common.make
endif

# --- Doomsday One build output (matches pac-man-neo; see CONTRIBUTING.md) ------------
BOLD   := \033[1m
RESET  := \033[0m
CYAN   := \033[0;36m
GREEN  := \033[0;32m
YELLOW := \033[1;33m
RED    := \033[0;31m
DIM    := \033[2m
RULE   := ──────────────────────────────────────────────────────────────

.PHONY: all help firmware windows linux macos clean \
        firmware-deps emulator-deps-nix emulator-deps-win

# ***************************************************************************************
#
# Remake everything to release state
#
# ***************************************************************************************

all: firmware-deps emulator-deps-nix emulator-deps-win
	@printf "\n$(BOLD)$(YELLOW)🟡 Neo6502 firmware — Doomsday One$(RESET)\n"
	@printf "$(CYAN)$(RULE)$(RESET)\n"
	$(CMAKEDIR) bin
	@printf "\n$(BOLD)$(CYAN)🔨 Firmware + ROMs (kernel, BASIC, RP2040)$(RESET)\n"
	@$(MAKE) -s -B -C kernel release
	@$(MAKE) -s -B -C basic release
	@$(MAKE) -s -B -C firmware release
	@printf "\n$(BOLD)$(CYAN)🐧 Emulators (Linux + Windows)$(RESET)\n"
	@$(MAKE) -s -B -C emulator release
	@$(MAKE) -s -B -C examples release
	@printf "\n$(BOLD)$(CYAN)📦 Release bundle$(RESET)\n"
	@$(MAKE) -s -B -C release
	@printf "\n$(BOLD)$(GREEN)✅ Build complete$(RESET) $(DIM)→ release/neo6502.zip$(RESET)\n"

# ***************************************************************************************
#
# Help
#
# ***************************************************************************************

help:
	@printf "$(BOLD)$(YELLOW)🟡 Neo6502 firmware (Doomsday One) — make targets$(RESET)\n"
	@printf "$(CYAN)$(RULE)$(RESET)\n"
	@printf "  $(BOLD)make$(RESET)            build everything $(DIM)→ release/neo6502.zip$(RESET)\n"
	@printf "  $(BOLD)make firmware$(RESET)   RP2040 firmware only $(DIM)(needs arm-none-eabi + cmake)$(RESET)\n"
	@printf "  $(BOLD)make linux$(RESET)      Linux emulator only\n"
	@printf "  $(BOLD)make windows$(RESET)    Windows emulator only\n"
	@printf "  $(BOLD)make macos$(RESET)      macOS emulator only\n"
	@printf "  $(BOLD)make clean$(RESET)      remove build artifacts\n"
	@printf "  $(BOLD)make help$(RESET)       show this help\n"
	@printf "\n  $(BOLD)$(CYAN)RRDC$(RESET) headless control server $(DIM)(opt-in, off by default)$(RESET):\n"
	@printf "    $(BOLD)make RRDC=1$(RESET)              full build, emulator with RRDC\n"
	@printf "    $(BOLD)make -C emulator RRDC=1$(RESET)  just the Linux emulator with RRDC\n"
	@printf "$(DIM)  Deps: 64tass, arm-none-eabi, cmake, SDL2, mingw-w64, pico-sdk. See README.md.$(RESET)\n"

# ***************************************************************************************
#
# Make firmware only
#
# ***************************************************************************************

firmware: firmware-deps
	@printf "\n$(BOLD)$(CYAN)🔨 Building firmware (kernel, BASIC, RP2040)$(RESET)\n"
	$(CMAKEDIR) bin
	@$(MAKE) -s -B -C kernel release
	@$(MAKE) -s -B -C basic release
	@$(MAKE) -s -B -C firmware release
	@printf "$(GREEN)✅ Firmware built$(RESET)\n"

# ***************************************************************************************
#
# Make emulator only
#
# ***************************************************************************************

windows: emulator-deps-nix emulator-deps-win
	@printf "\n$(BOLD)$(CYAN)🪟 Building Windows emulator$(RESET)\n"
	$(CMAKEDIR) bin
	@$(MAKE) -s -B -C kernel
	@$(MAKE) -s -B -C basic release
	@$(MAKE) -s -B -C emulator clean
	@$(MAKE) -s -B -C emulator ewindows
	@$(MAKE) -s -B -C examples release
	@printf "$(GREEN)✅ Windows emulator built$(RESET)\n"

linux: emulator-deps-nix
	@printf "\n$(BOLD)$(CYAN)🐧 Building Linux emulator$(RESET)\n"
	$(CMAKEDIR) bin
	@$(MAKE) -s -B -C kernel
	@$(MAKE) -s -B -C basic release
	@$(MAKE) -s -B -C emulator clean
	@$(MAKE) -s -B -C emulator elinux
	@$(MAKE) -s -B -C examples release
	@printf "$(GREEN)✅ Linux emulator built$(RESET)\n"

macos: emulator-deps-nix
	@printf "\n$(BOLD)$(CYAN)🍏 Building macOS emulator$(RESET)\n"
	@make -s -B -C emulator emacos
	@make -s -B -C examples release
	@printf "$(GREEN)✅ macOS emulator built$(RESET)\n"

# ***************************************************************************************
#
# Verify that dependencies are installed
#
# ***************************************************************************************

firmware-deps:
	@echo checking for firmware dependencies:
	@cmake             --version
	@g++               --version
	@arm-none-eabi-g++ --version
	@# NOTE: this is not accounting for 'arm-none-eabi-newlib'

emulator-deps-win:
	@x86_64-w64-mingw32-g++ --version

emulator-deps-nix:
	@echo checking for emulator dependencies:
	@g++         --version
	@64tass      --version
	@sdl2-config --version
	@zip         --version
	@python3     --version
	@python3 -c 'from importlib.metadata import version ; pkg="gitpython" ; print("python-%s: %s" % (pkg , version(pkg)))'
	@python3 -c 'from importlib.metadata import version ; pkg="pillow" ; print("python-%s: %s" % (pkg , version(pkg)))'

# ***************************************************************************************
#
# Clean everything
#
# ***************************************************************************************

clean:
	@printf "\n$(BOLD)$(YELLOW)🧹 Cleaning build artifacts$(RESET)\n"
	@$(MAKE) -s -B -C kernel clean
	@$(MAKE) -s -B -C basic clean
	@$(MAKE) -s -B -C emulator clean
	@$(MAKE) -s -B -C firmware clean
	@printf "$(GREEN)✅ Clean$(RESET)\n"
