// *******************************************************************************************************************************
// *******************************************************************************************************************************
//
//		Name:		gfx.c
//		Purpose:	Support library for SDL.
//		Created:	22nd November 2023
//		Author:		Paul Robson (paul@robsons.org.uk)
//
// *******************************************************************************************************************************
// *******************************************************************************************************************************

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

#define MAX_CONTROLLERS (4)

static SDL_Window *mainWindow = NULL;
static SDL_Surface *mainSurface = NULL;
static int controllerCount = 0;
static SDL_Joystick *controllers[MAX_CONTROLLERS];

static int background;

#define RED(x) ((((x) >> 8) & 0xF) * 17)
#define GREEN(x) ((((x) >> 4) & 0xF) * 17)
#define BLUE(x) ((((x) >> 0) & 0xF) * 17)

static void _GFXInitialiseKeyRecord(void);
static void _GFXUpdateKeyRecord(int scancode,int isDown);
static void GFXFindControllers(void);

// *******************************************************************************************************************************
//
//								Open window of specified size, set title and background.
//
// *******************************************************************************************************************************

void GFXOpenWindow(const char *title,int width,int height,int colour) {

	if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_GAMECONTROLLER) < 0)	{	// Try to initialise SDL Video and Audio
		exit(printf( "SDL could not initialize! SDL_Error: %s\n", SDL_GetError()));
	}

	mainWindow = SDL_CreateWindow(title, SDL_WINDOWPOS_UNDEFINED, 					// Try to create a window
							SDL_WINDOWPOS_UNDEFINED, width,height, SDL_WINDOW_SHOWN );
	if (mainWindow == NULL) {
		exit(printf( "Window could not be created! SDL_Error: %s\n", SDL_GetError() ));
	}

	mainSurface = SDL_GetWindowSurface(mainWindow);									// Get a surface to draw on.

	GFXFindControllers();


	background = colour;															// Remember required backgrounds.
	_GFXInitialiseKeyRecord();														// Set up key system.
	SOUNDOpen();
	SOUNDPlay();

	SDL_ShowCursor(SDL_DISABLE);                                                    // Hide mouse cursor
}

// *******************************************************************************************************************************
//
//												Start the main rendering loop
//
// *******************************************************************************************************************************

static int isRunning = -1;															// Is app running

static void _GFXMainLoop(void *arg);

void GFXStart(void) {
	while(isRunning) {																// While still running.
		_GFXMainLoop(NULL);
	}
	SOUNDStop();
	SDL_CloseAudio();
}

static void _GFXMainLoop(void *arg) {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {													// While events in event queue.
		if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {		// Exit if ESC pressed.
			int ctrl = ((SDL_GetModState() & KMOD_LCTRL) != 0);						// If control pressed
			if (CPUUseDebugKeys() == 0) ctrl = (ctrl == 0);							// Debugger in use, ESC on its own
			if (ctrl) isRunning = 0; 												// Exit
		}
		if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {					// Handle other keys.
			_GFXUpdateKeyRecord(event.key.keysym.sym,event.type == SDL_KEYDOWN);
			HWQueueKeyboardEvent(event.key.keysym.scancode,event.type == SDL_KEYDOWN);
		}
		if (event.type == SDL_MOUSEMOTION || event.type == SDL_MOUSEBUTTONDOWN 		// Mouse button/position  update
														|| event.type == SDL_MOUSEBUTTONUP) {
			HWUpdateMouse();
		}
		if (event.type == SDL_MOUSEWHEEL) {  									// Handle scroll wheel events.
			int dy = event.wheel.y;
			if (event.wheel.type == SDL_MOUSEWHEEL_FLIPPED) dy = -dy;
			MSEUpdateScrollWheel(dy);
		}
		if (event.type == SDL_QUIT) {  												// GUI hardware.
			isRunning = 0;
		}
	}
	SDL_FillRect(mainSurface, NULL, 												// Draw the background.
						SDL_MapRGB(mainSurface->format, RED(background),GREEN(background),BLUE(background)));
	int render = GFXXRender(mainSurface);											// Ask app to render state
	if (render) SDL_UpdateWindowSurface(mainWindow);								// And update the main window.
}

// *******************************************************************************************************************************
//
//											Exit Program
//
// *******************************************************************************************************************************

void GFXExit(void) {
	isRunning = 0;
}

// *******************************************************************************************************************************
//
//												Force exit.
//
// *******************************************************************************************************************************

void GFXCloseOnDebug(void) {
	isRunning = 0;
}
// *******************************************************************************************************************************
//
//													Close the gfx system
//
// *******************************************************************************************************************************

void GFXCloseWindow(void) {
	SDL_DestroyWindow(mainWindow);													// Destroy working window
	SDL_Quit();																		// Exit SDL.
}

// *******************************************************************************************************************************
//
//											Support Routine - Draw solid rectangle
//
// *******************************************************************************************************************************

void GFXRectangle(SDL_Rect *rc,int colour) {
	SDL_FillRect(mainSurface,rc,SDL_MapRGB(mainSurface->format,RED(colour),GREEN(colour),BLUE(colour)));
}

// *******************************************************************************************************************************
//
//									Support Routine - Draw 5 x 7 bitmap font character
//
// *******************************************************************************************************************************

#include "font.h"

void GFXCharacter(int xc,int yc,int character,int size,int colour,int back) {

	Uint32 col = SDL_MapRGB(mainSurface->format,RED(colour),GREEN(colour),BLUE(colour));				
	SDL_Rect rc;
	if (back >= 0) {
		Uint32 col2 = SDL_MapRGB(mainSurface->format,RED(back),GREEN(back),BLUE(back));				
		rc.x = xc-size/2;rc.y = yc-size/2;rc.w = 6 * size;rc.h = 8*size;
		SDL_FillRect(mainSurface,&rc,col2);
	}
	if (character < 32 || character >= 128) character = '?';						// Unknown character
	character = character - 32;														// First font item is $20 (Space)
	rc.w = rc.h = size;																// Width and Height of pixel.
	for (int x = 0;x < 5;x++) {														// 5 Across
		rc.x = xc + x * size;
		for (int y = 0;y < 7;y++) {													// 7 Down
			rc.y = yc + y * size;
			if (fontdata[character*5+x] & (0x01 << y)) {							// Is bit set ? (note inversion)
				SDL_FillRect(mainSurface,&rc,col);									// If so, draw the pixel
			}
		}
	}
}

// *******************************************************************************************************************************
//
//													Redefine a character
//
// *******************************************************************************************************************************

void GFXDefineCharacter(int nChar,int b1,int b2,int b3,int b4,int b5) {
	if (nChar >= 32 && nChar < 128) {
		nChar = (nChar-32) * 5;
		fontdata[nChar++] = b1;
		fontdata[nChar++] = b2;
		fontdata[nChar++] = b3;
		fontdata[nChar++] = b4;
		fontdata[nChar++] = b5;
	}
}

// *******************************************************************************************************************************
//
//									Support Routine - Draw 5 x 7 bitmap font string
//
// *******************************************************************************************************************************

void GFXString(int xc,int yc,const char *text,int size,int colour,int back) {
	while (*text != '\0') {															// While more text
		GFXCharacter(xc,yc,*text,size,colour,back);									// Draw text
		text++;																		// Next character
		xc = xc + 6 * size;															// Advance horizontally.
	}
}

// *******************************************************************************************************************************
//
//												Display Number in base
//
// *******************************************************************************************************************************

void GFXNumber(int xc,int yc,int number,int base,int width,int size,int colour,int back) {
	if (width > 0) {
		GFXNumber(xc,yc,number/base,base,width-1,size,colour,back);
		GFXCharacter(xc + size*6*(width-1),yc,"0123456789ABCDEF"[number % base],size,colour,back);
	}
}

// *******************************************************************************************************************************
//
//													Set Grid Size.
//	
// *******************************************************************************************************************************

static SDL_Rect gridInfo;

void GFXSetCharacterSize(int xSize,int ySize) {
	gridInfo.w = mainSurface->w / xSize / 6;
	gridInfo.h = mainSurface->h / ySize / 8;
	gridInfo.w = (gridInfo.w < gridInfo.h) ? gridInfo.w : gridInfo.h;
	gridInfo.h = gridInfo.w;
	gridInfo.x = mainSurface->w / 2 - gridInfo.w * xSize * 6 / 2;
	gridInfo.y = mainSurface->h / 2 - gridInfo.h * ySize * 8 / 2;
}

int _GFXX(int x) { return gridInfo.x + gridInfo.w * 6 * x; }
int _GFXY(int y) { return gridInfo.y + gridInfo.h * 8 * y; }
int _GFXS(void)  { return gridInfo.w; }

// *******************************************************************************************************************************
//
//				This table is a mapping of GFXKEY_ values (0-127) to SDLK_Values (can be anything.)
//
// *******************************************************************************************************************************

static int keyTable[] = {
	GFXKEY_UP,SDLK_UP, GFXKEY_DOWN,SDLK_DOWN, GFXKEY_LEFT,SDLK_LEFT, GFXKEY_RIGHT,SDLK_RIGHT,
	GFXKEY_RETURN,SDLK_RETURN,GFXKEY_BACKSPACE,SDLK_BACKSPACE,GFXKEY_TAB,SDLK_TAB,
	GFXKEY_LSHIFT,SDLK_LSHIFT,GFXKEY_RSHIFT,SDLK_RSHIFT,GFXKEY_SHIFT,-1,GFXKEY_CONTROL,SDLK_LCTRL,
	GFXKEY_ALT,SDLK_LALT, GFXKEY_ALTGR, SDLK_RALT,

	GFXKEY_F1,SDLK_F1, GFXKEY_F2,SDLK_F2, GFXKEY_F3,SDLK_F3, GFXKEY_F4,SDLK_F4, GFXKEY_F5,SDLK_F5, 
	GFXKEY_F6,SDLK_F6, GFXKEY_F7,SDLK_F7, GFXKEY_F8,SDLK_F8, GFXKEY_F9,SDLK_F9, 
	GFXKEY_F10,SDLK_F10, GFXKEY_F11,SDLK_F11, GFXKEY_F12,SDLK_F12,

	'A',SDLK_a, 'B',SDLK_b, 'C',SDLK_c, 'D',SDLK_d, 'E',SDLK_e, 'F',SDLK_f, 'G',SDLK_g, 'H',SDLK_h, 'I',SDLK_i,
	'J',SDLK_j, 'K',SDLK_k, 'L',SDLK_l, 'M',SDLK_m, 'N',SDLK_n, 'O',SDLK_o, 'P',SDLK_p, 'Q',SDLK_q, 'R',SDLK_r,
	'S',SDLK_s, 'T',SDLK_t, 'U',SDLK_u, 'V',SDLK_v, 'W',SDLK_w, 'X',SDLK_x, 'Y',SDLK_y, 'Z',SDLK_z,

	'0',SDLK_0, '1',SDLK_1, '2',SDLK_2, '3',SDLK_3, '4',SDLK_4, '5',SDLK_5, '6',SDLK_6, '7',SDLK_7, '8',SDLK_8, '9',SDLK_9,

	'-',SDLK_MINUS,'\\',SDLK_BACKSLASH,'@',SDLK_QUOTE,'[',SDLK_LEFTBRACKET,']',SDLK_RIGHTBRACKET,';',SDLK_SEMICOLON,':',SDLK_COLON,
	'.',SDLK_PERIOD,',',SDLK_COMMA,'/',SDLK_SLASH,'#',SDLK_HASH,'=',SDLK_EQUALS,' ',SDLK_SPACE,

-1 };

// *******************************************************************************************************************************
//
//													Handles Key State
//
// *******************************************************************************************************************************

struct _KeyRecord {
	int 	sdlKey;																	// Key representation in SDL.
	int 	gfxKey;																	// Key representation in GFX. (same as array index)
	int 	isPressed;																// Non zero if is pressed.
};

static struct _KeyRecord keyState[128];												// Array of key state records.

static void _GFXInitialiseKeyRecord(void) {
	for (int i = 0;i < 128;i++) {													// Erase the whole structure.
		keyState[i].sdlKey = keyState[i].gfxKey = keyState[i].isPressed = 0;
	}
	int n = 0;
	while (keyTable[n] != -1) {														// Scan the list of known keys.
		keyState[keyTable[n]].gfxKey = keyTable[n];									// Save gfx number of the key.
		keyState[keyTable[n]].sdlKey = keyTable[n+1];								// Save the corresponding SDL key.
		n = n + 2;
	}
}

static void _GFXUpdateKeyRecord(int scancode,int isDown) {
	for (int i = 0;i < 128;i++)														// Find key with corresponding scan code
		if (keyState[i].sdlKey == scancode)
			keyState[i].isPressed = (isDown != 0);									// Copy state into it.
	keyState[GFXKEY_SHIFT].isPressed = 												// Either shift key operates SHIFT.
					keyState[GFXKEY_LSHIFT].isPressed || keyState[GFXKEY_RSHIFT].isPressed;
}

// *******************************************************************************************************************************
//
//											Check to see if key is pressed.
//
// *******************************************************************************************************************************

int  GFXIsKeyPressed(int character) {
	if (character >= 'a' && character <= 'z') character = character - 'a' + 'A';	// Make lower case upper case
	return keyState[character].isPressed;							
}

// *******************************************************************************************************************************
//
//													Return stick status (Button Right Left Down Up)
//
// *******************************************************************************************************************************

int GFXReadJoystick0(void) {
	int joystickStatus = 0;
	if (GFXIsKeyPressed('K')) joystickStatus |= 1;
	if (GFXIsKeyPressed('M')) joystickStatus |= 2;
	if (GFXIsKeyPressed('Z')) joystickStatus |= 4;
	if (GFXIsKeyPressed('X')) joystickStatus |= 8;
	if (GFXIsKeyPressed('L')) joystickStatus |= 16;
	return joystickStatus;
}

// *******************************************************************************************************************************
//
//												Convert character to ASCII
//
//	UK Keyboard layout, will probably behave bizarrely elsewhere.
// *******************************************************************************************************************************

int  GFXToASCII(int ch,int applyModifiers) {
	if (ch >= ' ' && ch < 127) {													// Legitimate key.
		ch = tolower(ch);
		if (ch == '@') ch = '\'';													// @ is actually '
		if (applyModifiers != 0) {
			if (GFXIsKeyPressed(GFXKEY_SHIFT)) {
				switch(ch) {
					case '\'':	ch = '@';break;
					case '-':	ch = '_';break;
					case '#':	ch = '~';break;
					case '=':	ch = '+';break;
					case ';':	ch = ':';break;
					case '6':	ch = '^';break;
					case '7':	ch = '&';break;
					case '8':	ch = '*';break;
					case '9':	ch = '(';break;
					case '0':	ch = ')';break;
					default:	ch = ch ^ ((ch < 64) ? 0x10:0x20);break;
				}
			}
			if (GFXIsKeyPressed(GFXKEY_CONTROL)) ch = ch & 31;						// Handle control
		}

	} else {
		switch (ch) {																// Control characters
			case GFXKEY_TAB:		ch = 0x09;break;								// Handle TAB, Backspace and CR.
			case GFXKEY_RETURN:		ch = 0x0D;break;
			case GFXKEY_BACKSPACE:	ch = 0x7F;break;
			case GFXKEY_LEFT:		ch = 0x08;break;								// Arrow keys, Apple ][ mapping
			case GFXKEY_RIGHT:		ch = 0x15;break;
			case GFXKEY_UP:			ch = 0x0B;break;
			case GFXKEY_DOWN:		ch = 0x0A;break;
			default:				ch = 0x00;break;

		}
	}
	return ch;
}

// *******************************************************************************************************************************
//
//												Get elapsed time in ms.
//
// *******************************************************************************************************************************

int GFXTimer(void) {
	return SDL_GetTicks();
}

// *******************************************************************************************************************************
//
//												    Get controller count
//
// *******************************************************************************************************************************

// RRDC 0.5 virtual pads. A test harness has no joystick -- headless CI
// certainly has none -- so injection presents a pad HERE, at the same place
// the real ones are reported, rather than driving SDL. Everything above this
// line (the firmware, the game) cannot tell the difference, which is the
// whole point: what a test exercises is the path a physical pad takes.
#define VPAD_MAX	4
static bool     vpadPresent[VPAD_MAX];
static uint32_t vpadButtons[VPAD_MAX];

void GFXSetVirtualPad(int index,int buttons,int connected) {
	if (index < 0 || index >= VPAD_MAX) return;
	if (connected >= 0) vpadPresent[index] = (connected != 0);
	if (buttons >= 0) vpadButtons[index] = (uint32_t)buttons;
}

bool GFXGetVirtualPad(int index,int *buttons,int *connected) {
	if (index < 0 || index >= VPAD_MAX) return false;
	// AN INJECTION WINS over a physical pad at the same index. That is not
	// the obvious ordering -- "real hardware should take precedence" was the
	// first attempt -- but it is the correct one for a test contract: a suite
	// must behave the same on a machine with a controller plugged in as on
	// one without, or results depend on the developer's desk.
	//
	// It also happens to be load-bearing. SDL under Xvfb reports a joystick
	// that reads as all zeros, so deferring to "real" hardware silently
	// swallowed every injection and reported buttons=0, connected=true.
	if (vpadPresent[index]) {
		if (buttons != NULL) *buttons = (int)vpadButtons[index];
		if (connected != NULL) *connected = 1;
		return true;
	}
	if (index < controllerCount) {
		if (buttons != NULL) *buttons = (int)GFXReadController(index);
		if (connected != NULL) *connected = 1;
		return true;
	}
	if (buttons != NULL) *buttons = 0;
	if (connected != NULL) *connected = 0;
	return true;
}

int GFXControllerCount(void) {
	// Virtual pads extend the count so the firmware's own "how many
	// controllers" answer includes them -- a game that gates two-player on
	// the count has to see them or the injection is untestable.
	int n = controllerCount;
	for (int i = controllerCount;i < VPAD_MAX;i++) {
		if (vpadPresent[i]) n = i+1;
	}
	return n;
}

// *******************************************************************************************************************************
//
//										Read controller, format compatible with Firmware
//
// *******************************************************************************************************************************

unsigned int GFXReadController(int id) {
	int bitPattern = 0;
	// An injected pad answers first, at any index -- see GFXGetVirtualPad for
	// why injection outranks physical hardware here.
	if (id >= 0 && id < VPAD_MAX && vpadPresent[id]) return vpadButtons[id];
	if (id >= controllerCount) return 0;
	Sint16 dx = SDL_JoystickGetAxis(controllers[id],0);
	if (abs(dx) >= 1024) {
		bitPattern |= (dx < 0) ? 0x01:0x02;
	}
	Sint16 dy = SDL_JoystickGetAxis(controllers[id],1);
	if (abs(dy) >= 1024) {
		bitPattern |= (dy < 0) ? 0x04:0x08;
	}
	// Many USB pads — SNES-style ones especially (vendor 0x081f / SINO WEALTH) —
	// report the d-pad as a HAT rather than axes 0/1, so read hat 0 as well and
	// OR it into the same direction bits (left 0x01, right 0x02, up 0x04, down 0x08).
	if (SDL_JoystickNumHats(controllers[id]) > 0) {
		Uint8 hat = SDL_JoystickGetHat(controllers[id],0);
		if (hat & SDL_HAT_LEFT)  bitPattern |= 0x01;
		if (hat & SDL_HAT_RIGHT) bitPattern |= 0x02;
		if (hat & SDL_HAT_UP)    bitPattern |= 0x04;
		if (hat & SDL_HAT_DOWN)  bitPattern |= 0x08;
	}
	// Eight buttons, not four. The cap was 4, which reaches A/B/X/Y (0x10,
	// 0x20, 0x40, 0x80) and stops -- so SELECT and START, which a SNES-style
	// pad reports as buttons 4 and 5, could never be seen. A game wanting the
	// arcade's coin/start semantics had no way to read them, and the RRDC
	// canonical layout has bits for both (8 = START, 9 = SELECT) that nothing
	// could ever set.
	//
	// Widened to 8, which is what the mask has room for below A/B/X/Y and
	// covers the usual SNES/NES-style pad. Buttons past that are still
	// dropped; a pad with 12 is not the case being served here.
	int buttons = SDL_JoystickNumButtons(controllers[id]);
	buttons = (buttons >= 8) ? 8 : buttons;
	for (int b = 0;b < buttons;b++) {
		if (SDL_JoystickGetButton(controllers[id],b)) {
			// 0..3 -> A/B/X/Y at 0x10..0x80; 4 -> START (0x100),
			// 5 -> SELECT (0x200), matching the RRDC canonical layout so a
			// game reads the same bit whatever it is running on.
			bitPattern |= (b < 4) ? (0x10 << b) : (0x100 << (b - 4));
		}
	}
	return bitPattern;
}

// *******************************************************************************************************************************
//
//												    Search for controllers
//
// *******************************************************************************************************************************

static void GFXFindControllers(void) {
 	controllerCount = 0;  															// Discover controllers. 
	for (int i = 0; i < SDL_NumJoysticks(); i++) {
		if (controllerCount < MAX_CONTROLLERS) {
    		controllers[controllerCount] = SDL_JoystickOpen(i);
    		if (controllers[controllerCount] == NULL) {
    			exit(printf("Failed to open controller %d\n",i));
    		}
    		controllerCount++;
		}
	}
}
