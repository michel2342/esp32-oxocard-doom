// Copyright 2016-2017 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.


#include <stdlib.h>

#include "sdkconfig.h"
#include "doomdef.h"
#include "doomtype.h"
#include "m_argv.h"
#include "d_event.h"
#include "g_game.h"
#include "d_main.h"
#include "gamepad.h"
#include "lprintf.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef CONFIG_HW_OXOCARD_INPUT
#include "oxobuttons.h"
#else
#include "psxcontroller.h"
#endif


//The gamepad uses keyboard emulation, but for compilation, these variables need to be placed
//somewhere. THis is as good a place as any.
int usejoystick=0;
int joyleft, joyright, joyup, joydown;


// ---------------------------------------------------------------------------
// Oxocard 6-button input path
// ---------------------------------------------------------------------------
#ifdef CONFIG_HW_OXOCARD_INPUT

// Key mapping for each of the 5 action buttons.
// Index matches oxo_btn_t: FWD=0, BACK=1, TURNL=2, TURNR=3, SHOOT=4.
// key_normal is used when Strafe modifier is released;
// key_strafe is used when Strafe modifier is held.
// Pointers because key_* values are set at runtime by the engine.
static const struct {
    int *key_normal;
    int *key_strafe;
} oxo_key_map[5] = {
    {&key_up,    &key_up},              // FWD   - unaffected by strafe
    {&key_down,  &key_down},            // BACK  - unaffected by strafe
    {&key_left,  &key_strafeleft},      // TURNL - becomes strafe left
    {&key_right, &key_straferight},     // TURNR - becomes strafe right
    {&key_fire,  &key_fire},            // SHOOT - unaffected by strafe
};

// Tracks which Doom key is currently "held down" for each action button.
// 0 means no key is currently posted for that button.
static int last_posted_key[5] = {0};

void gamepadPoll(void)
{
    event_t ev;
    int strafe_held = oxobuttons_pressed(OXO_BTN_STRAFE);

    for (int i = 0; i < 5; i++) {
        int pressed   = oxobuttons_pressed((oxo_btn_t)i);
        int mapped_key = strafe_held ? *oxo_key_map[i].key_strafe
                                     : *oxo_key_map[i].key_normal;

        if (pressed) {
            if (last_posted_key[i] == 0) {
                // Button freshly pressed
                ev.type  = ev_keydown;
                ev.data1 = mapped_key;
                D_PostEvent(&ev);
                last_posted_key[i] = mapped_key;
            } else if (last_posted_key[i] != mapped_key) {
                // Strafe modifier toggled while this button was already held:
                // release the old key, press the new one
                ev.type  = ev_keyup;
                ev.data1 = last_posted_key[i];
                D_PostEvent(&ev);
                ev.type  = ev_keydown;
                ev.data1 = mapped_key;
                D_PostEvent(&ev);
                last_posted_key[i] = mapped_key;
            }
            // else: same key still held, nothing to do
        } else {
            if (last_posted_key[i] != 0) {
                // Button released
                ev.type  = ev_keyup;
                ev.data1 = last_posted_key[i];
                D_PostEvent(&ev);
                last_posted_key[i] = 0;
            }
        }
    }
}

void jsTask(void *arg) {
    printf("Oxocard button task starting.\n");
    while(1) {
        vTaskDelay(20/portTICK_PERIOD_MS);
        oxobuttons_poll();
    }
}

void gamepadInit(void)
{
    lprintf(LO_INFO, "gamepadInit: Oxocard 6-button input.\n");
}

void jsInit() {
    oxobuttons_init();
    xTaskCreatePinnedToCore(&jsTask, "js", 5000, NULL, 7, NULL, 0);
}

// ---------------------------------------------------------------------------
// PSX controller input path (original code, unchanged)
// ---------------------------------------------------------------------------
#else

volatile int joyVal=0;

typedef struct {
	int ps2mask;
	int *key;
} JsKeyMap;

static const JsKeyMap keymap[]={
	{0x10, &key_up},
	{0x40, &key_down},
	{0x80, &key_left},
	{0x20, &key_right},

	{0x4000, &key_use},				//cross
	{0x2000, &key_fire},			//circle
	{0x2000, &key_menu_enter},		//circle
	{0x8000, &key_pause},			//square
	{0x1000, &key_weapontoggle},	//triangle

	{0x8, &key_escape},				//start
	{0x1, &key_map},				//select

	{0x400, &key_strafeleft},		//L1
	{0x100, &key_speed},			//L2
	{0x800, &key_straferight},		//R1
	{0x200, &key_strafe},			//R2

	{0, NULL},
};

void gamepadPoll(void)
{
	static int oldPollJsVal=0xffff;
	int newJoyVal=joyVal;
	event_t ev;

	for (int i=0; keymap[i].key!=NULL; i++) {
		if ((oldPollJsVal^newJoyVal)&keymap[i].ps2mask) {
			ev.type=(newJoyVal&keymap[i].ps2mask)?ev_keyup:ev_keydown;
			ev.data1=*keymap[i].key;
			D_PostEvent(&ev);
		}
	}

	oldPollJsVal=newJoyVal;
}

void jsTask(void *arg) {
	int oldJoyVal=0xFFFF;
	printf("Joystick task starting.\n");
	while(1) {
		vTaskDelay(20/portTICK_PERIOD_MS);
		joyVal=psxReadInput();
		oldJoyVal=joyVal;
	}
}

void gamepadInit(void)
{
	lprintf(LO_INFO, "gamepadInit: Initializing game pad.\n");
}

void jsInit() {
	psxcontrollerInit();
	xTaskCreatePinnedToCore(&jsTask, "js", 5000, NULL, 7, NULL, 0);
}

#endif /* CONFIG_HW_OXOCARD_INPUT */

