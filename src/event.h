#ifndef JEVENT_H
#define JEVENT_H

//for the functions we need to generate keypresses / mouse actions
#include <X11/extensions/XTest.h>

//types of events QJoyPad can create.
//KeyRelease, KeyPress, ButtonRelease, ButtonPress, and MouseMove
enum eventType {KREL, KPRESS, BREL, BPRESS, WARP};


//a simplified event structure that can handle buttons and mouse movements
struct xevent {
	eventType type;
	int value1;	//button, keycode, or x
	int value2; //y
};


void sendevent( xevent e );

#endif
