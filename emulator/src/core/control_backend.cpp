// *******************************************************************************************************************************
//
//		Name:		control_backend.cpp
//		Purpose:	Retro Remote Debug Controller (RRDC) backend for the Neo6502 emulator.
//		            Maps the retro_control vtable onto the emulator's internals (read-only),
//		            so an external harness can read memory/registers, screenshot, and step
//		            the machine deterministically over HTTP. See extern/.../SPEC.md.
//
// *******************************************************************************************************************************

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "sys_processor.h"          // CPUReadMemory, CPUGetStatus65, CPUSTATUS65, WORD16

extern "C" {
#include "retro_control.h"          // C contract header (no extern "C" of its own)
}

// Defined in sys_debugger.cpp (has videoRAM + palette in scope) / hardware.cpp.
extern const uint8_t *RNDGetRGBFrame(int *w, int *h);
extern int RNDGetFrameCount(void);

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

static const retro_control_backend_t neo_backend = {
    "neo6502",              // platform
    "neo",                  // emulator
    neo_read_mem,
    neo_get_regs_json,
    neo_get_framebuffer,
    neo_get_frame_count,
};

// Called once from main() when a control port was requested on the command line.
int neo_control_start(int port) {
    return retro_control_start(port, &neo_backend);
}
