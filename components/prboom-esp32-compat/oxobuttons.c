#include "sdkconfig.h"

#ifdef CONFIG_HW_OXOCARD_INPUT

#include "driver/gpio.h"
#include "oxobuttons.h"

// GPIO pin assigned to each logical button, from Kconfig
static const int btn_gpio[OXO_NUM_BTNS] = {
    CONFIG_HW_OXOCARD_BTN_FWD_GPIO,
    CONFIG_HW_OXOCARD_BTN_BACK_GPIO,
    CONFIG_HW_OXOCARD_BTN_TURNL_GPIO,
    CONFIG_HW_OXOCARD_BTN_TURNR_GPIO,
    CONFIG_HW_OXOCARD_BTN_SHOOT_GPIO,
    CONFIG_HW_OXOCARD_BTN_STRAFE_GPIO,
};

typedef struct {
    int last_raw;       // raw gpio_get_level from previous poll cycle
    int confirmed;      // debounced state: 1 = pressed, 0 = released
} btn_state_t;

static btn_state_t state[OXO_NUM_BTNS];

void oxobuttons_init(void)
{
    for (int i = 0; i < OXO_NUM_BTNS; i++) {
        gpio_config_t cfg = {
            .pin_bit_mask  = (1ULL << btn_gpio[i]),
            .mode          = GPIO_MODE_INPUT,
            .pull_up_en    = GPIO_PULLUP_ENABLE,   // no effect on GPIO 36-39; use external pull-up there
            .pull_down_en  = GPIO_PULLDOWN_DISABLE,
            .intr_type     = GPIO_PIN_INTR_DISABLE
        };
        gpio_config(&cfg);

        state[i].last_raw  = 1;   // assume released (pull-up high)
        state[i].confirmed = 0;
    }
}

// Debounce: accept a new level only after two consecutive polls agree.
// At 50 Hz (20 ms/cycle) this gives a 20-40 ms debounce window.
void oxobuttons_poll(void)
{
    for (int i = 0; i < OXO_NUM_BTNS; i++) {
        int raw = gpio_get_level(btn_gpio[i]);
        if (raw == state[i].last_raw) {
            // Two identical reads in a row → accept.  Active-low: level 0 = pressed.
            state[i].confirmed = (raw == 0) ? 1 : 0;
        }
        state[i].last_raw = raw;
    }
}

int oxobuttons_pressed(oxo_btn_t btn)
{
    return state[btn].confirmed;
}

#endif /* CONFIG_HW_OXOCARD_INPUT */
