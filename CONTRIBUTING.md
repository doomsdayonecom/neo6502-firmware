# Contributing to the Doomsday One Neo6502 firmware

This is Doomsday One's fork of the
[Olimex Neo6502](https://www.olimex.com/Products/Retro-Computers/Neo6502/)
firmware, originally by Paul Scott Robson
([paulscottrobson/neo6502-firmware](https://github.com/paulscottrobson/neo6502-firmware)).
We maintain it as the reference emulator + firmware for our Neo6502 projects
(such as [pac-man-neo](https://github.com/doomsdayonecom/pac-man-neo)), carrying
fixes and the RRDC headless-control tooling. Upstream's licence and authorship
are retained — see [`LICENSE`](LICENSE).

## The workflow: issue → branch → PR

Every change — code, docs, or pipeline — follows the same path:

1. **Open an issue** (pick a template).
2. **Branch from the issue**, named `<issue-number>-short-slug`:
   ```sh
   gh issue develop <n> --name <n>-short-slug --base main --checkout
   ```
3. **Make the change**, keeping commits focused.
4. **Open a PR** against `main` with `Closes #<n>` in the body; fill in the
   template.
5. **Review + merge.**

Issues and PRs are tracked on the
[project board](https://github.com/orgs/doomsdayonecom/projects/1).

## Building

The build needs 64tass, `arm-none-eabi`, SDL2, mingw-w64 and the Raspberry Pi
Pico SDK. The simplest route is the published toolchain image:

```sh
docker run --rm -v "$PWD:/work" -w /work \
  ghcr.io/doomsdayonecom/neo6502-firmware-builder:latest make
```

| Command | What it does |
|---|---|
| `make` | Build everything → `release/neo6502.zip` (RP2040 firmware, emulators, kernel, BASIC, examples) |
| `make -C emulator` | Build just the Linux emulator |
| `make -C emulator RRDC=1` | Build the emulator with the RRDC control server (below) |
| `make clean` | Remove build artifacts |

## RRDC (Retro Remote Debug Controller)

RRDC is an opt-in HTTP control server compiled into the emulator for automated,
headless testing of downstream ports — screenshot, memory read/write, key
injection and deterministic audio capture (contract 0.3.0). It is **off by
default** so the ship build carries no debug server; build with `RRDC=1` to
include it. This mirrors the mednafen fork's `--enable-rrdc` switch. `RRDC=1`
affects only the Linux emulator (the platform used for headless CI).

## Commits

- Focused commits, imperative subjects ("Gate RRDC behind RRDC=1").
- **Signed commits (`git commit -S`) are preferred.**
- Reference the issue where it helps.

## Continuous integration

Every push / PR runs, in GitHub Actions:

- **🟡 Build** — builds the clean and `RRDC=1` configurations; uploads
  `neo6502.zip` and asserts the RRDC backend linked.

On merge to `main` / on a tag:

- **🐳 Publish toolchain image** — (re)publishes the build image to GHCR.
- **🚀 Release** — on a `v*` tag, publishes `neo6502.zip` (clean) +
  `neo6502-rrdc.zip` (RRDC Linux emulator).
- **🪞 Mirror to Codeberg** — mirrors the repo to Codeberg.
