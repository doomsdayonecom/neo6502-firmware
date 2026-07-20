// *******************************************************************************************************************************
//
//		Name:		control_backend.cpp
//		Purpose:	Retro Remote Debug Controller (RRDC) backend for the Neo6502 emulator.
//		            Maps the retro_control vtable onto the emulator's internals so an
//		            external harness can read/write memory + registers, screenshot, step,
//		            inject keys, and reset the machine deterministically over HTTP.
//		            See extern/.../SPEC.md.
//
// *******************************************************************************************************************************

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <SDL.h>                     // SDL_Scancode, SDL_GetScancodeFromKey

#include "sys_processor.h"          // CPUReadMemory/CPUWriteMemory/CPUReset, WORD16, CPUSTATUS65

extern "C" {
#include "retro_control.h"          // C contract header (no extern "C" of its own)
}

// Defined in sys_debugger.cpp (has videoRAM + palette in scope) / hardware.cpp.
extern const uint8_t *RNDGetRGBFrame(int *w, int *h);
extern int RNDGetFrameCount(void);
extern void HWQueueKeyboardEvent(int sdlCode, int isDown);   // hardware.cpp (feeds KBDEvent)

// --- backend callbacks (called on the emulator thread by retro_control_service) ---------

// Flat 64K address space, bank ignored; wraps at 0xFFFF; side-effect free.
static uint32_t neo_read_mem(uint32_t addr, int32_t bank, uint32_t len,
                             uint8_t *out, uint32_t out_cap) {
    (void)bank;
    uint32_t n = 0;
    for (uint32_t i = 0; i < len && n < out_cap; i++) {
        out[n++] = CPUReadMemory((WORD16)((addr + i) & 0xFFFF));
    }
    return n;
}

static void neo_get_regs_json(char *buf, size_t cap) {
    CPUSTATUS65 *s = CPUGetStatus65();
    snprintf(buf, cap,
             "{\"a\":%u,\"x\":%u,\"y\":%u,\"sp\":%u,\"pc\":%u,\"status\":%u}",
             (unsigned)(s->a & 0xFF), (unsigned)(s->x & 0xFF), (unsigned)(s->y & 0xFF),
             (unsigned)(s->sp & 0xFF), (unsigned)(s->pc & 0xFFFF),
             (unsigned)(s->status & 0xFF));
}

static void neo_get_framebuffer(retro_framebuffer_t *out) {
    int w = 0, h = 0;
    out->pixels = RNDGetRGBFrame(&w, &h);   // indexed -> RGB888 in a static scratch buffer
    out->width = w;
    out->height = h;
    out->fmt = RETRO_PIX_RGB888;
}

static uint64_t neo_get_frame_count(void) {
    return (uint64_t)RNDGetFrameCount();
}

// --- 0.2: key injection -----------------------------------------------------
// HWQueueKeyboardEvent(sdlScancode, isDown) is exactly what gfx.cpp's SDL loop
// feeds real key events into: it matches the scancode against SDL2HIDMapping to
// find the HID index, then calls KBDEvent — so an injected key is indistinguishable
// from a physical press (both the console FIFO and the held-state array update).
static void neo_key_event(SDL_Scancode sc, int action) {
    if (action == RETRO_KEY_DOWN) {
        HWQueueKeyboardEvent((int)sc, 1);
    } else if (action == RETRO_KEY_UP) {
        HWQueueKeyboardEvent((int)sc, 0);
    } else {                                   // RETRO_KEY_TAP: press then release
        HWQueueKeyboardEvent((int)sc, 1);
        HWQueueKeyboardEvent((int)sc, 0);
    }
}

// text => `value` is a character (letters map via the lowercase keycode);
// code => `value` is a raw SDL scancode. Return 0 if it doesn't map (server -> 400).
static int neo_inject_key(int is_text, uint32_t value, int action) {
    SDL_Scancode sc;
    if (is_text) {
        int c = (int)value;
        if (c >= 'A' && c <= 'Z') { c = c - 'A' + 'a'; }   // keycodes are lowercase
        sc = SDL_GetScancodeFromKey((SDL_Keycode)c);
    } else {
        sc = (SDL_Scancode)value;                          // raw SDL scancode
    }
    if (sc == SDL_SCANCODE_UNKNOWN) { return 0; }
    neo_key_event(sc, action);
    return 1;
}

// --- 0.2: reset the machine -------------------------------------------------
static void neo_reset(void) {
    CPUReset();
}

// --- 0.3: debug-write (poke). Flat 64K, bank ignored; wraps at 0xFFFF. -------
// Intended for RAM/state (e.g. poke the lives byte to reach the bust interstitial).
static uint32_t neo_write_mem(uint32_t addr, int32_t bank, uint32_t len,
                              const uint8_t *in) {
    (void)bank;
    for (uint32_t i = 0; i < len; i++) {
        CPUWriteMemory((WORD16)((addr + i) & 0xFFFF), in[i]);
    }
    return len;
}

static const retro_control_backend_t neo_backend = {
    "neo6502",              // platform
    "neo",                  // emulator
    neo_read_mem,           // read_mem        (0.1)
    neo_get_regs_json,      // get_regs_json   (0.1)
    neo_get_framebuffer,    // get_framebuffer (0.1)
    neo_get_frame_count,    // get_frame_count (0.1)
    neo_inject_key,         // inject_key      (0.2)
    neo_reset,              // reset           (0.2)
    neo_write_mem,          // write_mem       (0.3)
    nullptr,                // capture_audio   (0.3) — deferred. The neo's audio is
                            //   generated only in the real-time SDL pull callback
                            //   (SNDGetNextSample), so a deterministic /audio drain
                            //   would need per-frame emulation-driven generation.
                            //   Server therefore advertises contract 0.2.0.
};

// Called once from main() when a control port was requested on the command line.
int neo_control_start(int port) {
    return retro_control_start(port, &neo_backend);
}
