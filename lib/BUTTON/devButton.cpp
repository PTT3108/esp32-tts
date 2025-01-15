#include "devButton.h"

#include "button.h"
#include "helps.h"
#include "config.h"

static Button button1;
static Button button2;

// only check every second if the device is in-use, i.e. RX connected, or TX is armed
static constexpr int MS_IN_USE = 1000;
// #ifndef (GATEWAY)
// static constexpr struct {
//     bool pressType;
//     uint8_t count;
//     action_e action;
// } button_actions[] = {
//     // half second durations + 1 (i.e. 2=1.5s)
//     {true, 2, ACTION_BIND},             // 1.5s
//     {true, 9, ACTION_START_WIFI},       // 5.0s
//     {true, 23, ACTION_RESET_REBOOT}     // 12.0s
// };
// #endif

static ButtonAction_fn actions[ACTION_LAST] = {nullptr};
const button_color_t bt1 = {
    .val = {
        .actions = {
            {true, 15, ACTION_UPLOAD_STM32},
            {true, 25, ACTION_START_WIFI},
            {true, 30, ACTION_RESET_REBOOT},
        },
        .unused = 0
    }
};

const button_color_t bt2 = {
    .val = {
        .actions = {
            {false, 1, ACTION_INCREASE_POWER},
            {true, 5, ACTION_BLE},
            {true, 10, ACTION_RESET_REBOOT}
        },
        .unused = 0
    }
};

const button_color_t *button_actions_a[2] = { &bt1, &bt2 };
// button_action_t const *GetButtonActions(uint8_t button) const { return &buttonColors[button]; }
void registerButtonFunction(action_e action, ButtonAction_fn function)
{
    actions[action] = function;
}

size_t button_GetActionCnt()
{

    return ARRAY_SIZE(button_actions_a);
}

static void handlePress(uint8_t button, bool longPress, uint8_t count)
{
    ESP_LOGI(BUTTON_TAG,"handlePress(%u, %u, %u)", button, (uint8_t)longPress, count);
    const button_action_t *button_actions = button_actions_a[button]->val.actions;
    for (unsigned i = 0; i < button_GetActionCnt(); i++)
    {
        if (button_actions[i].action != ACTION_NONE && button_actions[i].pressType == longPress && button_actions[i].count == count - 1)
        {
            if (actions[button_actions[i].action])
            {
                actions[button_actions[i].action]();
            }
        }
    }
}
static bool initialize()
{
    return GPIO_PIN_BUTTON != UNDEF_PIN || GPIO_PIN_BUTTON2 != UNDEF_PIN;
}
static int start()
{
    if (GPIO_PIN_BUTTON == UNDEF_PIN && GPIO_PIN_BUTTON2 == UNDEF_PIN)
    {
        return DURATION_NEVER;
    }

    if (GPIO_PIN_BUTTON != UNDEF_PIN)
    {
        button1.init(GPIO_PIN_BUTTON);
        button1.OnShortPress = []()
        { handlePress(0, false, button1.getCount()); };
        button1.OnLongPress = []()
        { handlePress(0, true, button1.getLongCount() + 1); };
    }
    if (GPIO_PIN_BUTTON2 != UNDEF_PIN)
    {
        button2.init(GPIO_PIN_BUTTON2);
        button2.OnShortPress = []()
        { handlePress(1, false, button2.getCount()); };
        button2.OnLongPress = []()
        { handlePress(1, true, button2.getLongCount() + 1); };
    }

    return DURATION_IMMEDIATELY;
}

static int event()
{
    if (GPIO_PIN_BUTTON == UNDEF_PIN && GPIO_PIN_BUTTON2 == UNDEF_PIN)
    {
        return DURATION_NEVER;
    }
    return DURATION_IMMEDIATELY;
}

static int timeout()
{
    int timeout = DURATION_NEVER;
    if (GPIO_PIN_BUTTON != UNDEF_PIN)
    {
        timeout = button1.update();
    }
    if (GPIO_PIN_BUTTON2 != UNDEF_PIN)
    {
        timeout = button2.update();
    }
    return timeout;
}

device_t Button_device = {
    .initialize = initialize,
    .start = start,
    .event = event,
    .timeout = timeout,
    .subscribe = EVENT_ARM_FLAG_CHANGED | EVENT_CONNECTION_CHANGED};
