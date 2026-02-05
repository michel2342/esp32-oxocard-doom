#ifndef OXOBUTTONS_H
#define OXOBUTTONS_H

typedef enum {
    OXO_BTN_FWD    = 0,
    OXO_BTN_BACK   = 1,
    OXO_BTN_TURNL  = 2,
    OXO_BTN_TURNR  = 3,
    OXO_BTN_SHOOT  = 4,
    OXO_BTN_STRAFE = 5,
    OXO_NUM_BTNS   = 6
} oxo_btn_t;

// Configure GPIO pins and start polling.  Call once at startup.
void oxobuttons_init(void);

// Read all buttons and update debounced state.  Call once per poll cycle (50 Hz).
void oxobuttons_poll(void);

// Current debounced state: 1 = pressed, 0 = released.
int  oxobuttons_pressed(oxo_btn_t btn);

#endif
