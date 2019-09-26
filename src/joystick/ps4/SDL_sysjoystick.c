/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2019 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include "../../SDL_internal.h"

#ifdef __INTELLISENSE__
#define SDL_JOYSTICK_PS4 1
#endif

#if SDL_JOYSTICK_PS4

/* This is the PSP implementation of the SDL joystick API */
#include <pad.h>
#include <kernel.h>

#include <stdio.h>      /* For the definition of NULL */
#include <stdlib.h>

#include "../SDL_sysjoystick.h"
#include "../SDL_joystick_c.h"

#include "SDL_events.h"
#include "SDL_error.h"
#include "SDL_mutex.h"
#include "SDL_timer.h"
#include "../../thread/SDL_systhread.h"


#ifndef SCE_OK
#define SCE_OK 0
#endif

#define MAX_PADS 4
#define DS4_BTN_COUNT 20
#define DS4_AXE_COUNT 6		// lsX/Y, rsX/Y, L2, R2
#define DS4_HAT_COUNT 1

uint32_t nPads = 0;

SceUserServiceUserId userId, pad_users[MAX_PADS];
SceUserServiceLoginUserIdList userIdList;



int PS4_JoystickGetCount(void)
{
    return nPads;
}

void PS4_JoystickDetect(void)
{
	if(SCE_OK != sceUserServiceGetLoginUserIdList(&userIdList))
		printf("WARNING, sceUserServiceGetLoginUserIdList() failed for JoystickDetect()!\n");

	nPads = 0;

	for (int i = 0; i < SCE_USER_SERVICE_MAX_LOGIN_USERS; i++) {
		if (SCE_USER_SERVICE_USER_ID_INVALID != (userId = userIdList.userId[i])) {
			int32_t handle = scePadOpen(userId, SCE_PAD_PORT_TYPE_STANDARD, 0, NULL);
			if (handle > 0 || handle== SCE_PAD_ERROR_ALREADY_OPENED) {
				if(handle>0) printf("@@@@ got pad[%d] handle %X\n", nPads, handle);
				pad_users[nPads++] = userId;
				if (nPads >= MAX_PADS)
					break;
			}
		}
	}

}

#include "SDL_hints.h"


/* Function to scan the system for joysticks.
 * Joystick 0 should be the system default joystick.
 * It should return number of joysticks, or -1 on an unrecoverable fatal error.
 */
int PS4_JoystickInit(void)
{
	SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");	// Checks for keyboard focus ability, 'or something'

	int32_t res = sceUserServiceInitialize(NULL);
	if(SCE_OK != res && SCE_USER_SERVICE_ERROR_ALREADY_INITIALIZED != res)
		return SDL_SetError("Error sceUserServiceInitialize() failed!");

	if (SCE_OK != sceUserServiceGetInitialUser(&userId))
		return SDL_SetError("Error sceUserServiceGetInitialUser() failed!");

	if(SCE_OK != scePadInit())
		return SDL_SetError("Error scePadInit() failed!");

	PS4_JoystickDetect();

    return nPads;
}

/* Function to get the device-dependent name of a joystick */
const char * PS4_JoystickGetDeviceName(int device_index)
{
    return "Dualshock4";
}

/* Function to get the player index of a joystick */
int PS4_JoystickGetDevicePlayerIndex(int device_index)
{
	if (device_index >= MAX_PADS) device_index = MAX_PADS - 1;

	for (int i = 0; i < SCE_USER_SERVICE_MAX_LOGIN_USERS; i++)
		if(pad_users[device_index] == userIdList.userId[i])
			return i;

	return -1;
}



SDL_JoystickGUID PS4_JoystickGetDeviceGUID(int device_index)
{
	if (device_index >= MAX_PADS) device_index = MAX_PADS - 1;

	SDL_JoystickGUID guid;
	SDL_zero(guid);
#if 0
	*(int*)&guid.data[0] = 0x20200420;
	*(int*)&guid.data[4] = pad_users[device_index];
	*(int*)&guid.data[10] = device_index;
#if 0
	guid.data[14] = (Uint8)'h';	// use HID mapping h - 104 - 0x68
#endif
#else

	((Uint32*)guid.data)[0] = 0x00000005;	// 05000000;
	((Uint32*)guid.data)[0] = 0x00004c05;	// 4c050000;
	((Uint32*)guid.data)[0] = 0x000009cc;	// cc090000;
	((Uint32*)guid.data)[0] = 0x00008001;	// 01800000;
	// "050000004c050000cc09000001800000,PS4 Controller,
#endif
	return guid;
}


/* Function to perform the mapping from device index to the instance id for this index */
SDL_JoystickID PS4_JoystickGetDeviceInstanceID(int device_index)
{
	if (device_index >= MAX_PADS) device_index = MAX_PADS - 1;

    return scePadGetHandle(pad_users[device_index], SCE_PAD_PORT_TYPE_STANDARD, 0);
}

#if 0
typedef struct joystick_hwdata_t
{
} joystick_hwdata;
#endif

/* Function to open a joystick for use.
   The joystick to open is specified by the device index.
   This should fill the nbuttons and naxes fields of the joystick structure.
   It returns 0, or -1 if there is an error.
 */
int PS4_JoystickOpen(SDL_Joystick *joystick, int device_index)
{
	if (device_index >= MAX_PADS) device_index = MAX_PADS - 1;

	PS4_JoystickDetect();	// should already be done but ...

	int32_t handle = PS4_JoystickGetDeviceInstanceID(device_index);	// They are already open , 
	if (handle > 0) {
		joystick->nbuttons	= DS4_BTN_COUNT;
		joystick->naxes		= DS4_AXE_COUNT;
		joystick->nhats		= DS4_HAT_COUNT;

		/* Create the joystick data structure */
		joystick->instance_id = handle;
#if 0
		joystick->hwdata = (struct joystick_hwdata *)SDL_malloc(sizeof(*joystick->hwdata));
		if (joystick->hwdata == NULL) {
			return SDL_OutOfMemory();
		}
		SDL_memset(joystick->hwdata, 0, sizeof(*joystick->hwdata));
#endif
		return 0;
	}
	printf("PS4_JoystickOpen(idx: %d) failed to get handled!\n", device_index);

    return -1;
}

int PS4_JoystickRumble(SDL_Joystick * joystick, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble, Uint32 duration_ms)
{

	return 0;
}



/* Function to update the state of a joystick - called as a device poll.
 * This function shouldn't update the joystick structure directly,
 * but instead should call SDL_PrivateJoystick*() to deliver events
 * and update joystick device state.
 */
void PS4_JoystickUpdate(SDL_Joystick *joystick)
{
	ScePadData data;

//	printf("(((((((((( JS UPDATE %X ))))))))) \n", joystick->instance_id);

	int ret = scePadReadState(joystick->instance_id, &data);
	if (ret != SCE_OK || !data.connected) {
		printf("Warning, Controller is not connected or failed to read data! \n");
		return;
	}




	//a:b0,b:b1,x:b3,y:b2,leftshoulder:b4,rightshoulder:b5
	//back:b8,start:b9,guide:b10,leftstick:b11,rightstick:b12,
	//dpdown:h0.4, dpleft:h0.8, dpright:h0.2, dpup:h0.1,
	//leftx:a0,lefty:a1,lefttrigger:a2,rightx:a3,righty:a4,righttrigger:a5,

	const static uint32_t ds4_map[DS4_BTN_COUNT] = {
		SCE_PAD_BUTTON_CROSS, SCE_PAD_BUTTON_CIRCLE, SCE_PAD_BUTTON_SQUARE, SCE_PAD_BUTTON_TRIANGLE, SCE_PAD_BUTTON_L1, SCE_PAD_BUTTON_R1,
		0, /* b6? */ 0, /* b7? */ 0, /* no share/back btn atm */
		SCE_PAD_BUTTON_OPTIONS, 0 /* no guide button atm */, SCE_PAD_BUTTON_L3, SCE_PAD_BUTTON_R3,
		SCE_PAD_BUTTON_UP, SCE_PAD_BUTTON_DOWN, SCE_PAD_BUTTON_LEFT, SCE_PAD_BUTTON_RIGHT,
		SCE_PAD_BUTTON_TOUCH_PAD, SCE_PAD_BUTTON_L2, SCE_PAD_BUTTON_R2,
	};

#if 0
	static uint32_t s_btns=0;
	uint32_t m_btns=0;

	m_btns = data.buttons ^ s_btns;
	s_btns = data.buttons;
#else
	Uint32 s_btns = data.buttons;
#endif

//	if (m_btns) {
		for (uint32_t bn = 0; bn < DS4_BTN_COUNT; bn++) {
		//	if (m_btns & ds4_map[bn]) {
				//if (!
					SDL_PrivateJoystickButton(joystick, bn, (s_btns & ds4_map[bn]) ? SDL_PRESSED : SDL_RELEASED);
				//	printf("$$$$$$$$$$$$$$ UPDATE BUTTON %d  NOT POSTED \n", bn);
		//	}
		}
//	}

#define aS16(axe) -32768 + ((axe) << 8)

#define setA(n,axe) { Sint16 tmp = aS16(axe); if (tmp < -1000 || tmp > 1000) { SDL_PrivateJoystickAxis(joystick, (n), tmp); } }
#if 0
	Sint16 lX = -32768 + (data.leftStick.x << 8);	//-128 + data.leftStick.x;
	Sint16 lY = -32768 + (data.leftStick.y << 8);	//-128 + data.leftStick.y;

	Sint16 rX = -32768 + (data.rightStick.x << 8);	//-128 + data.rightStick.x;
	Sint16 rY = -32768 + (data.rightStick.y << 8);	//-128 + data.rightStick.y;

	// **FIXME** get actual deadzone and scale properly //
	if (lX < -1000 || lX > 1000)	SDL_PrivateJoystickAxis(joystick, 0, lX);
	if (lY < -1000 || lY > 1000)	SDL_PrivateJoystickAxis(joystick, 1, lY);
	// L2.analogue	data.analogButtons.l2;
	if (rX < -1000 || rX > 1000)	SDL_PrivateJoystickAxis(joystick, 4, rX);
	if (rY < -1000 || rY > 1000)	SDL_PrivateJoystickAxis(joystick, 5, rY);
	// R2.analogue	data.analogButtons.r2;
#endif

	setA(0, data.leftStick.x);
	setA(1, data.leftStick.y);
	setA(2, data.analogButtons.l2);

	setA(4, data.rightStick.x);
	setA(5, data.rightStick.y);
	setA(6, data.analogButtons.r2);

	Uint8 hat = SDL_HAT_CENTERED;
	if (s_btns & SCE_PAD_BUTTON_UP)		hat |= SDL_HAT_UP;
	if (s_btns & SCE_PAD_BUTTON_DOWN)	hat |= SDL_HAT_DOWN;
	if (s_btns & SCE_PAD_BUTTON_LEFT)	hat |= SDL_HAT_LEFT;
	if (s_btns & SCE_PAD_BUTTON_RIGHT)	hat |= SDL_HAT_RIGHT;
	SDL_PrivateJoystickHat(joystick, 0, hat);
}

#if 0

    static WORD s_XInputButtons[] = {
        XINPUT_GAMEPAD_A, XINPUT_GAMEPAD_B, XINPUT_GAMEPAD_X, XINPUT_GAMEPAD_Y,
        XINPUT_GAMEPAD_LEFT_SHOULDER, XINPUT_GAMEPAD_RIGHT_SHOULDER, XINPUT_GAMEPAD_BACK, XINPUT_GAMEPAD_START,
        XINPUT_GAMEPAD_LEFT_THUMB, XINPUT_GAMEPAD_RIGHT_THUMB,
        XINPUT_GAMEPAD_GUIDE
    };
    WORD wButtons = pXInputState->Gamepad.wButtons;
    Uint8 button;
    Uint8 hat = SDL_HAT_CENTERED;

    SDL_PrivateJoystickAxis(joystick, 0, pXInputState->Gamepad.sThumbLX);
    SDL_PrivateJoystickAxis(joystick, 1, ~pXInputState->Gamepad.sThumbLY);
    SDL_PrivateJoystickAxis(joystick, 2, ((int)pXInputState->Gamepad.bLeftTrigger * 257) - 32768);
    SDL_PrivateJoystickAxis(joystick, 3, pXInputState->Gamepad.sThumbRX);
    SDL_PrivateJoystickAxis(joystick, 4, ~pXInputState->Gamepad.sThumbRY);
    SDL_PrivateJoystickAxis(joystick, 5, ((int)pXInputState->Gamepad.bRightTrigger * 257) - 32768);

    for (button = 0; button < SDL_arraysize(s_XInputButtons); ++button) {
        SDL_PrivateJoystickButton(joystick, button, (wButtons & s_XInputButtons[button]) ? SDL_PRESSED : SDL_RELEASED);
    }

    if (wButtons & XINPUT_GAMEPAD_DPAD_UP) {
        hat |= SDL_HAT_UP;
    }
    if (wButtons & XINPUT_GAMEPAD_DPAD_DOWN) {
        hat |= SDL_HAT_DOWN;
    }
    if (wButtons & XINPUT_GAMEPAD_DPAD_LEFT) {
        hat |= SDL_HAT_LEFT;
    }
    if (wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) {
        hat |= SDL_HAT_RIGHT;
    }
    SDL_PrivateJoystickHat(joystick, 0, hat);
#endif










/* Function to close a joystick after use */
void PS4_JoystickClose(SDL_Joystick *joystick)
{
	scePadClose(joystick->instance_id);
#if 0
	if (joystick->hwdata)
		SDL_free(joystick->hwdata);
#endif
}



/* Function to perform any system-specific joystick related cleanup */
void PS4_JoystickQuit(void)
{
	nPads = 0;
}







SDL_JoystickDriver SDL_PS4_JoystickDriver =
{
	PS4_JoystickInit,
	PS4_JoystickGetCount,
	PS4_JoystickDetect,
	PS4_JoystickGetDeviceName,
	PS4_JoystickGetDevicePlayerIndex,
	PS4_JoystickGetDeviceGUID,
	PS4_JoystickGetDeviceInstanceID,
	PS4_JoystickOpen,
	PS4_JoystickRumble,
	PS4_JoystickUpdate,
	PS4_JoystickClose,
	PS4_JoystickQuit,
};


#endif /* SDL_JOYSTICK_PS4 */