#include "Arduino.h"

#define MAX_BUTON_ACTION           3
typedef enum : uint8_t {
    ACTION_NONE,
    ACTION_INCREASE_POWER,
    ACTION_START_WIFI,
    ACTION_BIND,
    ACTION_BLE,
    ACTION_RESET_REBOOT,
    ACTION_UPLOAD_STM32,
    ACTION_UPLOAD_ESP32,

    ACTION_LAST
} action_e;

typedef struct {
    uint8_t     pressType:1,    // 0 short, 1 long
                count:3,        // 1-8 click count for short, .5sec hold count for long
                action:4;       // action to execute
} button_action_t;

typedef union {
    struct {              // RRRGGGBB
        button_action_t actions[MAX_BUTON_ACTION];
        uint8_t unused;
    } val;
    // uint32_t raw;
} button_color_t;