# Changelog

Notable changes to the Doomsday One fork of the
[neo6502-firmware](https://github.com/paulscottrobson/neo6502-firmware) by
Paul Scott Robson.

## v1.0.1 — Doomsday One edition (first release from this fork)

The first release built and published from this fork. It tracks upstream's
`main` (past the v1.0.0 release) and adds the following.

### Fixed
- **Case-insensitive `.neo` cold-boot auto-load** (upstream PR #631): uppercase
  `.NEO` container names — e.g. the SD-card ship name `PACMAN.NEO` — now
  auto-load and cold-boot on case-sensitive filesystems (Linux/macOS) instead
  of dropping to the monitor. The last public release (v1.0.0) predates this,
  so downstream ports needed a lowercase-alias workaround; this release makes
  that unnecessary.
- **Tagless builds** no longer fail version stamping — `prompt.py` falls back to
  `0.0.0` when there is no version tag (a real `v*` tag still stamps its own).
- **Windows emulator cross-compile** now defines `__time_critical_func`, so
  shared sources that annotate hot paths with it build under mingw.

### Added
- **RRDC (Retro Remote Debug Controller), contract 0.3.0** in the emulator — an
  opt-in (`make RRDC=1`) HTTP control server bound to `127.0.0.1`: memory
  read/write, register + frame reads, real-framebuffer screenshot, deterministic
  frame stepping, key injection and deterministic audio capture. **Off by
  default**, so the ship build carries no debug server. Mirrors the mednafen
  fork's `--enable-rrdc` switch. `RRDC=1` affects only the Linux emulator.
- **Doomsday One CI/CD** — 🟡 Build (clean + `RRDC=1`), 🐳 GHCR toolchain image,
  🚀 Release, 🪞 Codeberg mirror — plus issue/PR templates,
  [`CONTRIBUTING.md`](CONTRIBUTING.md) and Dependabot.

### Release artifacts
- **`neo6502.zip`** — the clean firmware bundle (RP2040 firmware, Linux + Windows
  emulators, kernel, BASIC, examples). No debug server.
- **`neo6502-rrdc.zip`** — identical, but the Linux `neo` emulator is built with
  `RRDC=1` for automated headless testing of downstream ports.
