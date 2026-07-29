// ***************************************************************************************
// ***************************************************************************************
//
//      Name :      Gamepad.h
//      Authors :   Sascha Schneider
//      Date :      25th March 2024
//      Reviewed :  No
//      Purpose :   Gamepad class
//
// ***************************************************************************************
// ***************************************************************************************

#pragma once
#include <cstdint>

// ***************************************************************************************
//
//		The digital button mask returned by getState(), and read by the 6502
//		through the Read Controller API call.
//
//		Bits 0..7 are the historic layout, unchanged, and are given names here
//		only so the drivers stop repeating magic numbers.
//
//		Bits 8..11 are new, and the reason is worth stating. Seven of the nine
//		drivers in this directory already decode START and SELECT out of the
//		HID report and store them in m_start / m_select -- and then every
//		single getState() returned without ever reading those members. The
//		work was being done and thrown away one line before it left the class.
//
//		The visible effect on the 6502 side is that START and SELECT could not
//		be read AT ALL on hardware, whatever pad was plugged in. An arcade
//		port wanting the cabinet's coin and start buttons had no bits to read,
//		so it had to fall back on a fire button -- which is how "the fire
//		button inserts a credit" happens. The emulator's own controller read
//		(GFXReadController in framework/gfx.cpp) does report these bits, so
//		the same binary behaved differently on the emulator and on the Neo.
//
//		Two drivers -- Gamepad05832060 and Gamepad1c59002X, both SNES-style --
//		partly worked around this by folding start and select into 0x40 and
//		0x80, the X and Y positions. Those emissions are LEFT IN PLACE: any
//		program already reading them keeps working, and the cost is only that
//		those two pads report their start and select twice.
//
// ***************************************************************************************

#define GAMEPAD_LEFT		(0x001)
#define GAMEPAD_RIGHT		(0x002)
#define GAMEPAD_UP			(0x004)
#define GAMEPAD_DOWN		(0x008)
#define GAMEPAD_A			(0x010)
#define GAMEPAD_B			(0x020)
#define GAMEPAD_X			(0x040)
#define GAMEPAD_Y			(0x080)
#define GAMEPAD_START		(0x100)
#define GAMEPAD_SELECT		(0x200)
#define GAMEPAD_L			(0x400)
#define GAMEPAD_R			(0x800)

class Gamepad {
public:
	virtual ~Gamepad(){};
	virtual uint32_t getState() = 0;
	virtual void update(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) = 0;
};