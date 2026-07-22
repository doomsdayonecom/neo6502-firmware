# Building

Two ways to build the Neo6502 firmware + emulators: the **Docker toolchain
image** (recommended — nothing to install) or a **native** toolchain.

> **TL;DR** — on a recent distro, use Docker. Native builds need CMake
> **< 4.0**; Ubuntu 26.04 / Arch / other current distros ship CMake 4.x, which
> cannot configure pico-sdk 1.5.1.

## Builds that need no toolchain

`make help` and `make clean` need nothing. Everything else (`make`,
`make firmware`, `make linux`, …) compiles code and needs the toolchain below.

## Option A — Docker (recommended)

You only need Docker. The image has CMake 3.28, `arm-none-eabi` + newlib,
64tass, mingw-w64, SDL2, Python + GitPython + Pillow, and pico-sdk baked in.

```sh
docker run --rm -v "$PWD:/work" -w /work --user "$(id -u):$(id -g)" \
  ghcr.io/doomsdayonecom/neo6502-firmware-builder:latest make
```

Add `RRDC=1` for the emulator with the RRDC control server compiled in.

The image is published to GHCR by `.github/workflows/docker.yml`. If
`docker run` reports **`unauthorized`**, the package is private — either make
it public (the repo's Packages → the image → *Package settings → Change
visibility → Public*, one-time), or authenticate:

```sh
echo "$GITHUB_TOKEN" | docker login ghcr.io -u <your-user> --password-stdin
```

## Option B — native toolchain

### CMake 4.x + pico-sdk 1.5.1 (handled for you)

pico-sdk 1.5.1 declares an old `cmake_minimum_required` that CMake 4.x rejects
(*"Compatibility with CMake < 3.5 has been removed"*). The build **works around
this automatically**: `firmware/Makefile` exports `CMAKE_POLICY_VERSION_MINIMUM`,
which CMake ≥ 3.31 honours and older CMake (the 3.28 in the toolchain image)
ignores — so a modern-CMake distro builds with no special steps.

If you ever need to change it: `make CMAKE_POLICY_VERSION_MINIMUM=3.5` (or set
it in the environment).

### Dependencies — Debian / Ubuntu

```sh
sudo apt install \
  cmake gcc-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib \
  64tass mingw-w64 g++-mingw-w64-x86-64 libsdl2-dev zip \
  python3 python3-git python3-pil
```

`python3-git` = GitPython, `python3-pil` = Pillow (or
`pip install --break-system-packages gitpython pillow`).

### Dependencies — Arch

```sh
sudo pacman -S cmake arm-none-eabi-gcc arm-none-eabi-newlib 64tass \
  mingw-w64-gcc sdl2 zip python-gitpython python-pillow
```

(Arch's `cmake` is 4.x — see the gotcha above.)

## The RRDC submodule

The RRDC control core is a git submodule. Clone with `--recursive`, or after
cloning run:

```sh
git submodule update --init --recursive
```

Only the `RRDC=1` build compiles it; the default build does not.

## Targets

| Command | What it does |
|---|---|
| `make` | Everything → `release/neo6502.zip` |
| `make firmware` | RP2040 firmware only |
| `make linux` / `make windows` / `make macos` | one emulator |
| `make -C emulator [RRDC=1]` | just the Linux emulator |
| `make clean` | remove build artifacts |
| `make help` | list the targets |
