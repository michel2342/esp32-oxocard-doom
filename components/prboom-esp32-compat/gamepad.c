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
#include "doomstat.h"
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


#ifdef CONFIG_HW_OXOCARD_INPUT

//Key mapping per button. Index matches oxo_btn_t.
static int *oxo_key_map[5] = {
    &key_up, &key_down, &key_left, &key_right, &key_fire,
};

static int button_is_pressed[5] = {0};
static int middle_button_last_key = 0;

void gamepadPoll(void)
{
    for (int i = 0; i < 5; i++) {
        int pressed = oxobuttons_pressed((oxo_btn_t)i);
        int was_pressed = button_is_pressed[i];

        if (pressed && !was_pressed) {
            event_t ev = {0};
            ev.type  = ev_keydown;

            if (i == 4) {
                //Middle: menu_enter in menus, fire in gameplay
                ev.data1 = menuactive ? key_menu_enter : key_fire;
                middle_button_last_key = ev.data1;
            } else {
                ev.data1 = *oxo_key_map[i];
            }

            D_PostEvent(&ev);
            button_is_pressed[i] = 1;
        } else if (!pressed && was_pressed) {
            event_t ev = {0};
            ev.type  = ev_keyup;

            if (i == 4) {
                //Must match the key posted on keydown
                ev.data1 = middle_button_last_key;
            } else {
                ev.data1 = *oxo_key_map[i];
            }

            D_PostEvent(&ev);
            button_is_pressed[i] = 0;
        }
    }
}

void jsTask(void *arg) {
    while(1) {
        vTaskDelay(20/portTICK_PERIOD_MS);
        oxobuttons_poll();
    }
}

void gamepadInit(void)
{
    lprintf(LO_INFO, "gamepadInit: Oxocard 5-button input.\n");
}

void jsInit() {
    oxobuttons_init();
    xTaskCreatePinnedToCore(&jsTask, "js", 5000, NULL, 7, NULL, 0);
}

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

