/* LEDC (LED Controller) fade example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include <Arduino.h>
/*
 * About this example
 *
 * 1. Start with initializing LEDC module:
 *    a. Set the timer of LEDC first, this determines the frequency
 *       and resolution of PWM.
 *    b. Then set the LEDC channel you want to use,
 *       and bind with one of the timers.
 *
 * 2. You need first to install a default fade function,
 *    then you can use fade APIs.
 *
 * 3. You can also set a target duty directly without fading.
 *
 * 4. This example uses GPIO18/19/4/5 as LEDC output,
 *    and it will change the duty repeatedly.
 *
 * 5. GPIO18/19 are from high speed channel group.
 *    GPIO4/5 are from low speed channel group.
 *
 */
#define LEDC_HS_TIMER LEDC_TIMER_0
#define LEDC_HS_MODE LEDC_HIGH_SPEED_MODE
#define LEDC_HS_CH0_GPIO (33)
#define LEDC_HS_CH0_CHANNEL LEDC_CHANNEL_0

#define LEDC_TEST_CH_NUM (1)
#define LEDC_TEST_DUTY (1023)
#define LEDC_TEST_FADE_TIME (5000)

ledc_timer_config_t ledc_timer;
ledc_channel_config_t ledc_channel[LEDC_TEST_CH_NUM];

void setup()
{
    Serial.begin(115200);
    int c;
    /*
     * Prepare and set configuration of timers
     * that will be used by LED Controller
     */

    ledc_timer.duty_resolution = LEDC_TIMER_10_BIT; // resolution of PWM duty
    ledc_timer.freq_hz = 78000;                     // frequency of PWM signal
    ledc_timer.speed_mode = LEDC_HS_MODE;           // timer mode
    ledc_timer.timer_num = LEDC_HS_TIMER;           // timer index
                                                    // Auto select the source clock

    // Set configuration of timer0 for high speed channels
    ledc_timer_config(&ledc_timer);

    /*
     * Prepare individual configuration
     * for each channel of LED Controller
     * by selecting:
     * - controller's channel number
     * - output duty cycle, set initially to 0
     * - GPIO number where LED is connected to
     * - speed mode, either high or low
     * - timer servicing selected channel
     *   Note: if different channels use one timer,
     *         then frequency and bit_num of these channels
     *         will be the same
     */

    ledc_channel[0].channel = LEDC_HS_CH0_CHANNEL;
    ledc_channel[0].duty = 0;
    ledc_channel[0].gpio_num = LEDC_HS_CH0_GPIO;
    ledc_channel[0].speed_mode = LEDC_HS_MODE;
    ledc_channel[0].hpoint = 0;
    ledc_channel[0].timer_sel = LEDC_HS_TIMER;

    // Set LED Controller with previously prepared configuration

    ledc_channel_config(&ledc_channel[0]);

    // Initialize fade service.
    ledc_fade_func_install(0);
}
void loop()
{
    int ch;

    printf("1. LEDC fade up to duty = %d\n", LEDC_TEST_DUTY);

    ledc_set_fade_with_time(ledc_channel[0].speed_mode,
                            ledc_channel[0].channel, 800, LEDC_TEST_FADE_TIME);
    ledc_fade_start(ledc_channel[0].speed_mode,
                    ledc_channel[0].channel, LEDC_FADE_WAIT_DONE);

    vTaskDelay(LEDC_TEST_FADE_TIME / portTICK_PERIOD_MS);
    ledc_set_fade_with_time(ledc_channel[0].speed_mode,
                            ledc_channel[0].channel, 0, LEDC_TEST_FADE_TIME);
    ledc_fade_start(ledc_channel[0].speed_mode,
                    ledc_channel[0].channel, LEDC_FADE_WAIT_DONE);
    vTaskDelay(LEDC_TEST_FADE_TIME / portTICK_PERIOD_MS);
}