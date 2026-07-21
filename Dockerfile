# Doomsday One — Neo6502 firmware build toolchain.
#
# Everything `make` needs: the RP2040 firmware (.uf2) via arm-none-eabi +
# pico-sdk, the SDL emulator (Linux and the Windows mingw cross-build), the
# 64tass kernel/BASIC ROMs, and the release bundler. Published to GHCR by
# .github/workflows/docker.yml.
#
# Build the firmware with it:
#   docker run --rm -v "$PWD:/work" -w /work \
#     ghcr.io/doomsdayonecom/neo6502-firmware-builder:latest make

FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive \
    PICO_SDK_PATH=/usr/share/pico_sdk

RUN apt-get update -y -qq \
 && apt-get install -y --no-install-recommends \
      build-essential cmake git ca-certificates \
      ccache \
      python3 python3-pip \
      64tass zip \
      libsdl2-dev \
      gcc-arm-none-eabi \
      mingw-w64 g++-mingw-w64-x86-64 \
 && rm -rf /var/lib/apt/lists/*

# Python build deps, from requirements.txt so Dependabot can track them.
COPY requirements.txt /tmp/requirements.txt
RUN pip3 install --no-cache-dir --break-system-packages -r /tmp/requirements.txt

# Raspberry Pi Pico SDK for the RP2040 firmware build (matches the branch the
# upstream build tracks); --recurse for its tinyusb/etc. submodules.
RUN git clone --depth 1 --branch master https://github.com/raspberrypi/pico-sdk "$PICO_SDK_PATH" \
 && git -C "$PICO_SDK_PATH" submodule update --init --depth 1

WORKDIR /work
