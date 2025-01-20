#pragma once
#include <stdint.h>
#include "fastio.h"

#define PWM_CAN_SET_PWM_FREQ // This PWM supports PWM Frequency adjustment
#define PWM_FREQUENCY 1000u  // Default PWM frequency when set_pwm_duty() is called without set_pwm_frequency()
#define PWM_RESOLUTION 10u   // Default PWM bit resolution
#define CHANNEL_MAX_NUM 15u  // max PWM channel # to allocate (7 to only use low speed, 15 to use low & high)
#define MAX_PWM_IOPIN 33u    // hardware pwm pins < 34

#define _SET_PWM_OUTPUT(IO) (ledcAttach(IO, 1000, 12))
#define MAX_EX_PIN 24

typedef struct pwm_pin
{
    uint32_t pwm_cycle_ticks = 1000000UL / (PWM_FREQUENCY) / 4;
    uint32_t pwm_tick_count = 0;
    uint32_t pwm_duty_ticks = 0;
} pwm_pin_t;

class EX_PWM
{
public:
    EX_PWM();
    

    static pwm_pin_t pwm_pin_data[MAX_EX_PIN];                                // Dữ liệu của các chân PWM
    static bool init_pwm();                                                   // Khởi tạo hệ thống PWM
    static void set_pwm(int pin, int duty = 255, uint32_t f = PWM_FREQUENCY); // Thiết lập PWM
    static void set_pwm_duty(int pin, int duty = 255);                        // Thiết lập chu kỳ PWM
    static void exWrite(int pin, int val);
private:
    static void set_pwm_frequency(int pin, uint32_t f);
};

extern EX_PWM pwm;