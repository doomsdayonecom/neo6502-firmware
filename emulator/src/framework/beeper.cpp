// ***************************************************************************************
// ***************************************************************************************
//
//		Name:		beeper.cpp
//		Purpose:	SoundSupport library for SDL.
//		Created:	12th February 2024.
//		Author:		qxxxb (https://github.com/qxxxb/sdl2-beeper)
//					Paul Robson (paul@robsons.org.uk)
//
// ***************************************************************************************
// ***************************************************************************************

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <ctype.h>
#include "gfx.h"
#include <queue>
#include <cmath>
#include "sys_processor.h"
#include <hardware.h>
#include <common.h>

#include <iostream>
#include <string>

static SDL_AudioDeviceID m_audioDevice;
static SDL_AudioSpec m_obtainedSpec;

void (*m_writeData)(uint8_t* ptr, double data);
int (*m_calculateOffset)(int sample, int channel);

// ***************************************************************************************
//
//		Emulation-driven audio generation + RRDC /audio capture.
//
//		SNDGetNextSample() advances the channel state per call, so it can only be
//		driven from one place. To make the control-API /audio drain DETERMINISTIC
//		under pause+step (the audio analogue of /screenshot), generation is driven
//		from the emulation loop (HWSync, once per frame) rather than the real-time
//		SDL pull callback: BEEPERGenerateFrame() produces one frame's worth of
//		samples and tees them into a playback FIFO (drained by the SDL callback)
//		and a capture ring (drained by BEEPERCaptureDrain). Stepping N frames thus
//		yields exactly N*(rate/FRAME_RATE) captured samples, regardless of wall time.
//
// ***************************************************************************************

#define AUDIO_RATE_DEFAULT 44100

// Playback FIFO: written by the emulator thread (BEEPERGenerateFrame), read by
// the SDL audio thread (audioCallback). Single-producer/single-consumer: the
// producer only advances pb_wr, the consumer only advances pb_rd.
#define PB_RING  (1u << 13)
static int16_t           pb_ring[PB_RING];
static volatile uint32_t pb_wr = 0, pb_rd = 0;

// Capture ring: written + drained on the emulator thread only (retro_control
// services /audio at a frame boundary), so no cross-thread concern here.
#define CAP_RING (1u << 15)
static int16_t  cap_ring[CAP_RING];
static uint32_t cap_wr = 0, cap_rd = 0, cap_dropped = 0;

static int BEEPERRate(void) {
	return (m_obtainedSpec.freq > 0) ? m_obtainedSpec.freq : AUDIO_RATE_DEFAULT;
}

// Called once per frame from HWSync (emulator thread).
void BEEPERGenerateFrame(void) {
	int n = BEEPERRate() / FRAME_RATE;
	for (int i = 0; i < n; i++) {
		int raw = SNDGetNextSample();                       // ~ -128..127 (AGC-bounded)
		// Playback FIFO — store the raw value; the callback normalises /128 as before.
		// On full, drop the new sample (producer must not touch pb_rd).
		uint32_t pnext = (pb_wr + 1u) & (PB_RING - 1u);
		if (pnext != pb_rd) { pb_ring[pb_wr] = (int16_t)raw; pb_wr = pnext; }
		// Capture ring — full-scale S16 (matches the played level: data*range/2).
		int16_t s16 = (int16_t)((raw / 128.0) * ((double)INT16_MAX));
		uint32_t cnext = (cap_wr + 1u) & (CAP_RING - 1u);
		if (cnext == cap_rd) { cap_rd = (cap_rd + 1u) & (CAP_RING - 1u); cap_dropped++; }
		cap_ring[cap_wr] = s16; cap_wr = cnext;
	}
}

// Drain up to `cap` mono int16 samples for GET /audio (emulator thread).
uint32_t BEEPERCaptureDrain(int16_t *out, uint32_t cap, uint32_t *dropped) {
	uint32_t k = 0;
	while (k < cap && cap_rd != cap_wr) {
		out[k++] = cap_ring[cap_rd];
		cap_rd = (cap_rd + 1u) & (CAP_RING - 1u);
	}
	if (dropped) { *dropped = cap_dropped; cap_dropped = 0; }
	return k;
}

int BEEPERCaptureRate(void) { return BEEPERRate(); }

// ***************************************************************************************
//
// 		Calculate the offset in bytes from the start of the audio stream to the
// 		memory address at `sample` and `channel`.
//
// 		Channels are interleaved.
//
// ***************************************************************************************

static int calculateOffset_s16(int sample, int channel) {
	return
		(sample * sizeof(int16_t) * m_obtainedSpec.channels) +
		(channel * sizeof(int16_t));
}

static int calculateOffset_f32(int sample, int channel) {
	return
		(sample * sizeof(float) * m_obtainedSpec.channels) +
		(channel * sizeof(float));
}

// ***************************************************************************************
//
// 		Convert a normalized data value (range: 0.0 .. 1.0) to a data value matching
// 		the audio format.
//
// ***************************************************************************************

static void writeData_s16(uint8_t* ptr, double data) {
	int16_t* ptrTyped = (int16_t*)ptr;
	double range = (double)INT16_MAX - (double)INT16_MIN;
	double dataScaled = data * range / 2.0;
	*ptrTyped = dataScaled;
}

static void writeData_f32(uint8_t* ptr, double data) {
	float* ptrTyped = (float*)ptr;
	*ptrTyped = data;
}

// ***************************************************************************************
//
//						Callback when requesting buffer be filled
//
// ***************************************************************************************

static void audioCallback(void* userdata,uint8_t* stream,int len) {
	// Unused parameters
	(void)userdata;
	(void)len;

	// Write data to the entire buffer by iterating through all samples and
	// channels.
	for (int sample = 0; sample < m_obtainedSpec.samples; ++sample) {
		// Pull the emulation-generated sample from the playback FIFO; silence on
		// underrun (the FIFO is filled once per frame by BEEPERGenerateFrame).
		int16_t raw = 0;
		if (pb_rd != pb_wr) {
			raw = pb_ring[pb_rd];
			pb_rd = (pb_rd + 1u) & (PB_RING - 1u);
		}
		double data = raw / 128.0;

		// Write the same data to all channels
		for (int channel = 0; channel < m_obtainedSpec.channels; ++channel) {
			int offset = m_calculateOffset(sample, channel);
			uint8_t* ptrData = stream + offset;
			m_writeData(ptrData, data);
		}
	}
}

// ***************************************************************************************
//
//								Open a sound device
//
// ***************************************************************************************

void SOUNDOpen() {
	// First define the specifications we want for the audio device
	SDL_AudioSpec desiredSpec;
	SDL_zero(desiredSpec);

	// Commonly used sampling frequency
	desiredSpec.freq = 44100;

	// Currently this program supports two audio formats:
	// - AUDIO_S16: 16 bits per sample
	// - AUDIO_F32: 32 bits per sample
	//
	// We need this flexibility because some sound cards do not support some
	// formats.

	// Higher bit depth means higher resolution the sound, lower bit depth
	// means lower resolution for the sound. Since we are just playing a simple
	// sine wave, 16 bits is fine.
	desiredSpec.format = AUDIO_S16;

	// Smaller buffer means less latency with the sound card, but higher CPU
	// usage. Bigger buffers means more latency with the sound card, but lower
	// CPU usage. 512 is fairly small, since I don't want a delay before a beep
	// is played.
	desiredSpec.samples = 512;

	// Since we are just playing a simple sine wave, we only need one channel.
	desiredSpec.channels = 1;

	// Set the callback (pointer to a function) to our callback. This function
	// will be called by SDL2 in a separate thread when it needs to write data
	// to the audio buffer. In other words, we don't control when this function
	// is called; SDL2 manages it.
	desiredSpec.callback = audioCallback;

	// When we open the audio device, we tell SDL2 what audio specifications we
	// desire. SDL2 will try to get these specifications when opening the audio
	// device, but sometimes the audio device does not support some of our
	// desired specifications. In that case, we have to be flexible and adapt
	// to what the audio device supports. The obtained specifications that the
	// audio device supports will be stored in `m_obtainedSpec`

	m_audioDevice = SDL_OpenAudioDevice(
		NULL, // device (name of the device, which we don't care about)
		0, // iscapture (we are not recording sound)
		&desiredSpec, // desired
		&m_obtainedSpec, // obtained
		0 // allowed_changes (allow any changes between desired and obtained)
	);

	if (m_audioDevice == 0) {
		SDL_Log("Failed to open audio: %s", SDL_GetError());
		// TODO: throw exception
	} else {
		std::string formatName;
		switch (m_obtainedSpec.format) {
			case AUDIO_S16:
				m_writeData = writeData_s16;
				m_calculateOffset = calculateOffset_s16;
				formatName = "AUDIO_S16";
				break;
			case AUDIO_F32:
				m_writeData = writeData_f32;
				m_calculateOffset = calculateOffset_f32;
				formatName = "AUDIO_F32";
				break;
			default:
				SDL_Log("Unsupported audio format: %i", m_obtainedSpec.format);
				// TODO: throw exception
		}

		// std::cout << "[Beeper] frequency: " << m_obtainedSpec.freq << std::endl;
		// std::cout << "[Beeper] format: " << formatName << std::endl;

		// std::cout
		// 	<< "[Beeper] channels: "
		// 	<< (int)(m_obtainedSpec.channels)
		// 	<< std::endl;

		// std::cout << "[Beeper] samples: " << m_obtainedSpec.samples << std::endl;
		// std::cout << "[Beeper] padding: " << m_obtainedSpec.padding << std::endl;
		// std::cout << "[Beeper] size: " << m_obtainedSpec.size << std::endl;
	}
}

// ***************************************************************************************
//
//								End the Sound system
//
// ***************************************************************************************

void SOUNDClose() {
	SDL_CloseAudioDevice(m_audioDevice);
}

// ***************************************************************************************
//
//								Start playing sound
//
// ***************************************************************************************

void SOUNDPlay() {
	SDL_PauseAudioDevice(m_audioDevice, 0);
}

// ***************************************************************************************
//
//								Stop playing sound
//
// ***************************************************************************************

void SOUNDStop() {
	SDL_PauseAudioDevice(m_audioDevice, 1);
}

// ***************************************************************************************
//
//      Function that returns the sample rate in Hz of the implementeing hardware
//
// ***************************************************************************************

int SNDGetSampleFrequency(void) {
    return m_obtainedSpec.freq;
}
