// *******************************************************************************************************************************
// *******************************************************************************************************************************
//
//		Name:		main.c
//		Purpose:	Main program
//		Created:	22nd November 2023
//		Author:		Paul Robson (paul@robsons.org.uk)
//
// *******************************************************************************************************************************
// *******************************************************************************************************************************

#include <stdio.h>
#include "gfx.h"
#include "sys_processor.h"
#include "sys_debug_system.h"
#include "debugger.h"
#include "common.h"

int main(int argc,char *argv[]) {
	DSPReset();
	DEBUG_ARGUMENTS(argc,argv);
	DEBUG_RESET();
	GFXOpenWindow(WIN_TITLE,WIN_WIDTH,WIN_HEIGHT,WIN_BACKCOLOUR);
#ifdef RRDC
	if (DBGGetControlPort() > 0) {													// RRDC: start the HTTP control server if requested.
		neo_control_start(DBGGetControlPort());
	}
#endif
	GFXStart();
	CPUEndRun();
	GFXCloseWindow();
	return(0);
}


