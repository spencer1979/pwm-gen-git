/**
 * @file main.ino
 * @author Spencer Chen (spencer.chen7@gmail.com)
 * @brief 
 * you@do{name}m
 * @date 2019-08-15
 * 
 * @copyright Copyright (c) 2019
 * 
 */
#include <math.h>
#include <Arduino.h>
#include <SSD1306.h>
#include <ClickEncoder.h>
#include <sdkconfig.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/timers.h>
#include <driver/ledc.h>
#include <driver/gpio.h>
#include <esp_err.h>
#include <Pwmgen.h>
#include <ArduinoJson.h>
//Wifi include
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <SPIFFS.h>
#include <ESPAsyncWebServer.h>
#include <config.h>
//PWM gpio//PWM Duty and freq Setting (output)
#define PWM_TIMER LEDC_TIMER_0
#define PWM_TEST_FADE_TIME (60000)
#define PWM_TIMER_SPEED_MODE LEDC_HIGH_SPEED_MODE
#define PWM_CHANNEL LEDC_CHANNEL_0

// key button pinout
#ifndef USER_BUTTON
#define Key1 5
#define Key2 17
#define Key3 16
#define Key4 4
#endif
// Rotary Encoder GPIO Pin
#define ROTARYENCODER_DEFAULT_A_PIN 27
#define ROTARYENCODER_DEFAULT_B_PIN 26
#define ROTARYENCODER_DEFAULT_BUT_PIN 23
#define ROTARYENCODER_DEFAULT_STEPS 2
// DEBUG OUTPUT
#define DEBUG_PRINTF(...) Serial.printf(__VA_ARGS__)

#ifndef DEBUG_PRINTF
#define DEBUG_PRINTF (...)
#endif

int menuIndex = 0;
int frame = 1;
int page = 1;
int lastmenuIndex = 0;
//
bool setup_mode = false;
int setup_page = 1;
int setup_frame = 1;
int setup_menu_index = 0;
int setup_last_menu_index = 0;
//
bool up = false;
bool down = false;
bool middle_click = false; // button click
bool middle_held = false;  // button hold
int16_t last, value;
bool save_yes_no = false;
//oled sleep counter
bool isScreenSleep = false;
uint16_t ScreenIdleTime = 0;
bool ScreenSleepEnable = false;

// menu Text
String main_menu_item[9] = {"PWM:", "LED:", "Duty SETUP", "Freq SETUP", "AUTO DIM", "SAVE", "RESET ", "SETUP", "WIFI IP"};
String setup_menu_item[7] = {"Back to Main", "PWM ON delay", "LED On delay", "Turn-On Priority", "OLED Sleep", "Duty Step", "Freq Step "};
//Wifi ssid and password
char ssid[23];
const char *password = NULL;
//check ID
enum APP_ID
{
    ID_SAVE = 0,
    ID_FREQ,
    ID_DUTY,
    ID_PWM_ONOFF,
    ID_LED_ONOFF,
    ID_FREQ_STEP,
    ID_DUTY_STEP
};
//create  struct
Pwmgen mypwm;
// object
SSD1306Wire display(0x3c, SDA, SCL, GEOMETRY_128_32);
ClickEncoder encoder(ROTARYENCODER_DEFAULT_A_PIN, ROTARYENCODER_DEFAULT_B_PIN, ROTARYENCODER_DEFAULT_BUT_PIN, ROTARYENCODER_DEFAULT_STEPS, false);
AsyncWebSocketClient *myclient = NULL;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
IPAddress apIP(10, 0, 0, 1);
//create Timer for Rotary encoder
hw_timer_t *encoderTimer = NULL;

//FreeRTOS software Timer
BaseType_t xSleepTimer;
TimerHandle_t xSleepTimer_Handler = NULL;

volatile SemaphoreHandle_t oledSleepTimerSemaphore;
portMUX_TYPE mymux = portMUX_INITIALIZER_UNLOCKED;

TaskHandle_t xDisplayTask_Handler;

//FreeRTOS Queue
QueueHandle_t
    xQappPwm_Handler,
    xQappFreq_Handler,
    xQappDuty_Handler,
    xQappPwn_onoff_Handler,
    xQappLed_onoff_Handler;

void setup()
{

    Serial.begin(115200);
    uint64_t chipid;
    chipid = ESP.getEfuseMac();
    uint16_t chip = (uint16_t)(chipid >> 32);
    snprintf(ssid, 23, "PWMGEN-%04X%08X", chip, (uint32_t)chipid);
    //The chip ID is essentially its MAC address(length: 6 bytes).
    Serial.println(ssid); //print Low 4bytes.
    // SPIFFS.begin();
    // if (SPIFFS.format())
    // {
    //     DEBUG_PRINTF("\r\nformat ok!\r\n");
    // }

    if (mypwm.begin())
    {
        DEBUG_PRINTF("\r\nSystem begin  \r\n");

        mypwm.bResetConfig();
    }
    else
    {
        DEBUG_PRINTF("\r\nCan't not begin \r\n");
    }

    xTaskCreatePinnedToCore(vDisplayTask, "Dispaly Task", 6000, NULL, 3, NULL, 1);
}

void loop()
{

    delay(2000);
}

/***********************************************************************************************************
 * *********************************************WEB APP Fucnction ****************************************** 
 * *********************************************************************************************************/

void vDisplayTask(void *arg)
{
    //init the oled display
    display.init();
    //set oled Contrast
    //display.setContrast(127);
    //display.clear();
    //display.drawXbm(0, 0, pi_logo_width, pi_logo_height, pi_logo_bits);
    //display.display();
    //vTaskDelay(pdMS_TO_TICKS(5000));
    //disble encoder accle
    encoder.setAccelerationEnabled(false);
    encoderTimer = timerBegin(0, 80, true);
    // Attach onTimer function to our timer.
    timerAttachInterrupt(encoderTimer, &rotaryISR, true);
    // Set alarm to call onTimer function every second (value in microseconds).
    // Repeat the alarm (third parameter)
    timerAlarmWrite(encoderTimer, 1000, true);
    // Start an alarm
    timerAlarmEnable(encoderTimer);
    TickType_t timer_periods;

    // oledSleepTimerSemaphore = xSemaphoreCreateBinary();
    if ((bool)mypwm.getOledSleepTime() == false)
    {
        ScreenSleepEnable = false;
        DEBUG_PRINTF("Sleep timer disable !! \r\n");
    }
    else
    {
        ScreenSleepEnable = true;
        DEBUG_PRINTF("Sleep timer Enable !! \r\n");
    }

    if (ScreenSleepEnable)
    {
        // call SleepTimerCallBackFun  function every 60sec.
        xSleepTimer_Handler = xTimerCreate("sleepTimer", pdMS_TO_TICKS(60000), pdTRUE, (void *)1, &SleepTimerCallBackFun);

        if (xTimerStart(xSleepTimer_Handler, 10) == pdTRUE)
        {
            DEBUG_PRINTF("Sleep timer Start  %d\r\n", xSleepTimer_Handler);
        }
    }

    for (;;)
    {

        CheckScreenSleep();
        drawMainMenu();
        readRotaryEncoder();
        middle_button_process();
        taskYIELD();
    }
    vTaskDelete(NULL);
}
/**
 * 
 * 
 */
void CheckScreenSleep()
{
    if (ScreenSleepEnable != (bool)mypwm.getOledSleepTime())
    {
        DEBUG_PRINTF("Sleep timer State chaged  \r\n");
        if ((bool)mypwm.getOledSleepTime())
        {
            xSleepTimer_Handler = xTimerCreate("sleepTimer", pdMS_TO_TICKS(60000), pdTRUE, (void *)1, &SleepTimerCallBackFun);
            if (xTimerStart(xSleepTimer_Handler, 10) == pdTRUE)
            {
                DEBUG_PRINTF("Sleep timer Start again. %d\r\n", xSleepTimer_Handler);
            }
        }
        else
        {
            xTimerStop(xSleepTimer_Handler, 10);
            DEBUG_PRINTF("Sleep timer Stop !! \r\n");
            xTimerDelete(xSleepTimer_Handler, 10);
            DEBUG_PRINTF("Sleep timer Deleted!! \r\n");

            DEBUG_PRINTF("Sleep timer Disable \r\n");
            xSleepTimer_Handler = NULL;
        }
        ScreenIdleTime = 0;
        ScreenSleepEnable = (bool)mypwm.getOledSleepTime();
    }

    // OLED SCREEN Saving
    if (ScreenSleepEnable)
    {
        if (ScreenIdleTime >= mypwm.getOledSleepTime())
        {
            xTimerStop(xSleepTimer_Handler, 10);
            display.displayOff();
            isScreenSleep = true;
        }
    }
}
void drawDutyFreq()
{
    char buffer[20];

    display.setFont(Serif_plain_8);
    display.setTextAlignment(TEXT_ALIGN_RIGHT);
    sprintf(buffer, "D:%.1f%%", mypwm.getDuty());
    display.drawString((display.getWidth()), 3, buffer);
    // memset(buffer, '0', sizeof(buffer));
    sprintf(buffer, "F:%.1fK", (float)(mypwm.getFreq() / 1000));
    display.drawString((display.getWidth()), 19, buffer);
}
/**
 * *************************************************************************
 * @brief Draw MAIN  MENU on OLED  Screen
 * *************************************************************************
 */
void drawMainMenu()
{

    if (page == 1) //show page 1 menuCOM20
    {
        char buffer[50];
        display.clear();
        display.setColor(WHITE);
        // show the freq and duty on right side

        // show the menu bar

        if (menuIndex == 0 && frame == 1)
        {

            /**
             * --------------
             * | PWM:ON(OFF) |
             * --------------
             * 
             * LED:ON(OFF)
             * 
             */

            drawDutyFreq();
            display.setFont(Serif_plain_12);
            if (mypwm.getPwmState())
                MenuItemSelected(main_menu_item[0] + "ON", 0, true, setup_mode);
            else
                MenuItemSelected(main_menu_item[0] + "OFF", 0, true, setup_mode);

            if (mypwm.getLedState())
                MenuItemSelected(main_menu_item[1] + "ON", 16, false, setup_mode);
            else
                MenuItemSelected(main_menu_item[1] + "OFF", 16, false, setup_mode);
        }
        else if (menuIndex == 1 && frame == 1)
        {
            /**
             * 
             *  PWM:ON(OFF) 
             *
             * --------------
             * | LED:ON(OFF) |
             * --------------
             */
            drawDutyFreq();
            display.setFont(Serif_plain_12);
            if (mypwm.getPwmState())
                MenuItemSelected(main_menu_item[0] + "ON", 0, false, setup_mode);
            else
                MenuItemSelected(main_menu_item[0] + "OFF", 0, false, setup_mode);

            if (mypwm.getLedState())
                MenuItemSelected(main_menu_item[1] + "ON", 16, true, setup_mode);
            else
                MenuItemSelected(main_menu_item[1] + "OFF", 16, true, setup_mode);
        }

        else if (menuIndex == 1 && frame == 2)
        {
            /**
             * ---------------
             * | LED:ON(OFF) |
             * ---------------
             * 
             *  DUTY Setup
             * 
             */
            drawDutyFreq();
            display.setFont(Serif_plain_12);
            if (mypwm.getLedState())
                MenuItemSelected(main_menu_item[1] + "ON", 0, true, setup_mode);
            else
                MenuItemSelected(main_menu_item[1] + "OFF", 0, true, setup_mode);

            MenuItemSelected(main_menu_item[2], 16, false, setup_mode);
        }
        else if (menuIndex == 2 && frame == 2)
        {

            /**
             * 
             *  LED:ON(OFF) 
             *
             * --------------
             * | DUTY Setup |
             * --------------
             */
            display.drawXbm(display.getWidth() - 32, 0, bell_curve_width, bell_curve_height, bell_curve_bits);
            display.setFont(Serif_plain_12);
            if (mypwm.getLedState())
                MenuItemSelected(main_menu_item[1] + "ON", 0, false, setup_mode);
            else
                MenuItemSelected(main_menu_item[1] + "OFF", 0, false, setup_mode);

            MenuItemSelected(main_menu_item[2], 16, true, setup_mode);
        }
        else if (menuIndex == 2 && frame == 3)
        {
            /**
             * 
             * --------------
             * | Duty setup |
             * --------------
             * 
             *  Freq setup 
             * 
             */
            display.drawXbm(display.getWidth() - 32, 0, bell_curve_width, bell_curve_height, bell_curve_bits);
            display.setFont(Serif_plain_12);
            MenuItemSelected(main_menu_item[2], 0, true, setup_mode);
            MenuItemSelected(main_menu_item[3], 16, false, setup_mode);
        }
        else if (menuIndex == 3 && frame == 3)
        {
            /**
             * 
             *  Duty setup 
             * 
             * --------------
             * | Freq setup |
             * --------------
             */
            display.drawXbm(display.getWidth() - 32, 0, frequency_icon_width, frequency_icon_height, frequency_icon_bits);
            display.setFont(Serif_plain_12);
            MenuItemSelected(main_menu_item[2], 0, false, setup_mode);
            MenuItemSelected(main_menu_item[3], 16, true, setup_mode);
        }
        else if (menuIndex == 3 && frame == 4)
        {
            /**
             *--------------
             *| Freq setup |
             * -------------
             * 
             *  Auto dim 
             * 
             */
            display.drawXbm(display.getWidth() - 32, 0, frequency_icon_width, frequency_icon_height, frequency_icon_bits);
            display.setFont(Serif_plain_12);
            MenuItemSelected(main_menu_item[3], 0, true, setup_mode);
            MenuItemSelected(main_menu_item[4], 16, false, setup_mode);
        }
        else if (menuIndex == 4 && frame == 4)
        {
            /**
             *
             *  Freq setup 
             * 
             * --------------
             * | AUTO DIM|
             * --------------
             */
            display.drawXbm(display.getWidth() - 32, 0, auto_dim_icon_width, auto_dim_icon_height, auto_dim_icon_bits);
            display.setFont(Serif_plain_12);
            MenuItemSelected(main_menu_item[3], 0, false, setup_mode);
            MenuItemSelected(main_menu_item[4], 16, true, setup_mode);
        }
        else if (menuIndex == 4 && frame == 5)
        {

            /**  
             * ----------------
             * | Auto dim       |
             * -----------------
             * 
             *  SAVE
             * 
             * */
            display.drawXbm(display.getWidth() - 32, 0, auto_dim_icon_width, auto_dim_icon_height, auto_dim_icon_bits);
            display.setFont(Serif_plain_12);
            MenuItemSelected(main_menu_item[4], 0, true, setup_mode);
            MenuItemSelected(main_menu_item[5], 16, false, setup_mode);
        }
        else if (menuIndex == 5 && frame == 5)
        {
            /** 
             *  
             * Auto dim 
             * 
             *-----------------
             *| Factory Reset|
             * ---------------
             */
            display.drawXbm(display.getWidth() - 32, 0, factory_reset_icon_width, factory_reset_icon_height, factory_reset_icon_bits);
            display.setFont(Serif_plain_12);
            MenuItemSelected(main_menu_item[4], 0, false, setup_mode);
            MenuItemSelected(main_menu_item[5], 16, true, setup_mode);
        }
        else if (menuIndex == 5 && frame == 6)
        {
            /** 
             * ---------------   
             * |SAVE|
             * ---------------
             *            
             *  RESET
             * 
             * 
             * */
            display.setFont(Serif_plain_12);
            display.drawXbm(display.getWidth() - 32, 0, save_icon_width, save_icon_height, save_icon_bits);
            MenuItemSelected(main_menu_item[5], 0, true, setup_mode);
            MenuItemSelected(main_menu_item[6], 16, false, setup_mode);
        }
        else if (menuIndex == 6 && frame == 6)
        {
            /** 
             *   
             * SAVE
             * 
             * --------------
             * |RESET|
             * --------------
             * 
             * */
            display.setFont(Serif_plain_12);
            display.drawXbm(display.getWidth() - 32, 0, factory_reset_icon_width, factory_reset_icon_height, factory_reset_icon_bits);
            MenuItemSelected(main_menu_item[5], 0, false, setup_mode);
            MenuItemSelected(main_menu_item[6], 16, true, setup_mode);
        }
        else if (menuIndex == 6 && frame == 7)
        {
            /** 
             *----------------   
             * |RESET|
             * ---------------           
             * SETUP

             * */
            display.setFont(Serif_plain_12);
            display.drawXbm(display.getWidth() - 32, 0, factory_reset_icon_width, factory_reset_icon_height, factory_reset_icon_bits);
            MenuItemSelected(main_menu_item[6], 0, true, setup_mode);
            MenuItemSelected(main_menu_item[7], 16, false, setup_mode);
        }
        else if (menuIndex == 7 && frame == 7)
        {
            /** 
             *   
             * RESET
             * 
             * -----------
             * |SETUP  |
             * -----------
             * 
             * */
            display.setFont(Serif_plain_12);
            display.drawXbm(display.getWidth() - 32, 0, setting_icon_width, setting_icon_height, setting_icon_bits);
            MenuItemSelected(main_menu_item[6], 0, false, setup_mode);
            MenuItemSelected(main_menu_item[7], 16, true, setup_mode);
        }
        else if (menuIndex == 7 && frame == 8)
        {
            /** 
             *----------------   
             * |SETUP |
             * ---------------         
             *WIFI IP 
             * */
            display.setFont(Serif_plain_12);
            display.drawXbm(display.getWidth() - 32, 0, setting_icon_width, setting_icon_height, setting_icon_bits);
            MenuItemSelected(main_menu_item[7], 0, true, setup_mode);
            MenuItemSelected(main_menu_item[8], 16, false, setup_mode);
        }
        else if (menuIndex == 8 && frame == 8)
        {
            /** 
             *  SETUP
             * ----------------   
             * |WIFI IP |
             * ---------------
             * */
            display.setFont(Serif_plain_12);
            display.drawXbm(display.getWidth() - 32, 0, wifi_icon_width, wifi_icon_height, wifi_icon_bits);
            MenuItemSelected(main_menu_item[7], 0, false, setup_mode);
            MenuItemSelected(main_menu_item[8], 16, true, setup_mode);
        }

        display.display();
    }
    else if (page == 2) //show page 2
    {
        char strTemp[50] = {0};
        display.clear();
        display.setFont(Serif_plain_8);
        display.setColor(WHITE);
        display.setTextAlignment(TEXT_ALIGN_CENTER);
        display.drawHorizontalLine(0, 0, 128);
        display.drawString(display.getWidth() / 2, 1, main_menu_item[menuIndex]);
        display.drawHorizontalLine(0, 12, 128);
        display.setTextAlignment(TEXT_ALIGN_CENTER);

        switch (menuIndex)
        {
        case 2 /* Duty setup */:
            display.setFont(Serif_plain_18);
            sprintf(strTemp, "%.2f%%", mypwm.getDuty());
            display.drawString((display.getWidth() / 2), 13, strTemp);
            display.display();
            break;
        case 3 /*frequency setup */:
            display.setFont(Serif_plain_18);
            sprintf(strTemp, "%.2fKHz", (float)mypwm.getFreq() / 1000);
            display.drawString(display.getWidth() / 2, 13, strTemp);
            display.display();
            break;
        case 4 /* AUTO dim */:
            display.setFont(Serif_plain_18);
            confirmDisplay(save_yes_no);
            display.display();
            break;
        case 5 /* SAVE*/:
            display.setFont(Serif_plain_18);
            confirmDisplay(save_yes_no);
            display.display();
            break;
        case 6 /*RESET*/:
            display.setFont(Serif_plain_18);
            confirmDisplay(save_yes_no);
            display.display();
        case 7 /* SETUP*/:
            drawSetupMenu();

        case 8 /* WiFI iP */:
            break;
        }
    }
}
/**
 * @brief Draw setup menu 
 * 
 */
void drawSetupMenu()
{
    if (setup_page == 1)
    {
        char buffer[50];
        display.clear();
        display.setColor(WHITE);
        // show the freq and duty on right side

        // show the menu bar
        display.setFont(Serif_plain_8);

        if (setup_menu_index == 0 && setup_frame == 1)
        {

            /**
             * -------------------
             * |Back to main menu |
             * -------------------
             * 
             *   PWM ON delay setup
             * 
             */
            display.drawXbm(display.getWidth() - home_logo_width, 0, home_logo_width, home_logo_height, back_icon_bits);
            MenuItemSelected(setup_menu_item[0], 0, true, setup_mode);

            MenuItemSelected(setup_menu_item[1], 16, false, setup_mode);
        }
        else if (setup_menu_index == 1 && setup_frame == 1)
        {

            display.drawXbm(display.getWidth() - time_icon_width, 0, time_icon_width, time_icon_height, time_icon_bits);
            /**
             * 
             *  Back to main menu 
             *
             * --------------
             * |  PWM ON delay setup |
             * --------------
             */

            MenuItemSelected(setup_menu_item[0], 0, false, setup_mode);
            MenuItemSelected(setup_menu_item[1], 16, true, setup_mode);
        }
        else if (setup_menu_index == 2 && setup_frame == 2)
        {

            /**
             * 
             *  PWM ON delay time
             *
             * --------------
             * | LED ON delay time  |
             * --------------
             */

            display.drawXbm(display.getWidth() - time_icon_width, 0, time_icon_width, time_icon_height, time_icon_bits);
            MenuItemSelected(setup_menu_item[1], 0, false, setup_mode);

            MenuItemSelected(setup_menu_item[2], 16, true, setup_mode);
        }
        else if (setup_menu_index == 1 && setup_frame == 2)
        {
            /**
             * ---------------
             * |PWM on delay tiem |
             * ---------------
             * 
             * LED on delay timer 
             * 
             */
            display.drawXbm(display.getWidth() - time_icon_width, 0, time_icon_width, time_icon_height, time_icon_bits);
            MenuItemSelected(setup_menu_item[1], 0, true, setup_mode);

            MenuItemSelected(setup_menu_item[2], 16, false, setup_mode);
        }

        else if (setup_menu_index == 3 && setup_frame == 3)
        {
            /**
             * 
             * LED ON delay time 
             * 
             * --------------
             * | On sequence|
             * --------------
             */
            display.drawXbm(display.getWidth() - 32, 0, sequence_icon_width, sequence_icon_height, sequence_icon_bits);
            MenuItemSelected(setup_menu_item[2], 0, false, setup_mode);
            MenuItemSelected(setup_menu_item[3], 16, true, setup_mode);
        }
        else if (setup_menu_index == 2 && setup_frame == 3)
        {
            /**
             * 
             * --------------
             * |LED on delay time |
             * --------------
             * 
             *  ON sequence 
             * 
             */
            display.drawXbm(display.getWidth() - 32, 0, time_icon_width, time_icon_height, time_icon_bits);
            MenuItemSelected(setup_menu_item[2], 0, true, setup_mode);
            MenuItemSelected(setup_menu_item[3], 16, false, setup_mode);
        }

        else if (setup_menu_index == 4 && setup_frame == 4)
        {
            /**
             *
             *  On sequence
             * 
             * --------------
             * | Screen saver|
             * --------------
             */
            display.drawXbm(display.getWidth() - 32, 0, sleep_icon_width, sleep_icon_height, sleep_icon_bits);
            MenuItemSelected(setup_menu_item[3], 0, false, setup_mode);
            MenuItemSelected(setup_menu_item[4], 16, true, setup_mode);
        }
        else if (setup_menu_index == 3 && setup_frame == 4)
        {
            /**
             *--------------
             *|  On sequence|
             * -------------
             * 
             *  Screen saver
             * 
             */
            display.drawXbm(display.getWidth() - 32, 0, sequence_icon_width, sequence_icon_height, sequence_icon_bits);
            MenuItemSelected(setup_menu_item[3], 0, true, setup_mode);
            MenuItemSelected(setup_menu_item[4], 16, false, setup_mode);
        }

        else if (setup_menu_index == 5 && setup_frame == 5)
        {
            /** 
             *  
             * Screenb saver
             * 
             *-----------------
             *| Duty step
             * ---------------
             */
            display.drawXbm(display.getWidth() - 32, 0, step_icon_width, step_icon_height, step_icon_bits);
            MenuItemSelected(setup_menu_item[4], 0, false, setup_mode);
            MenuItemSelected(setup_menu_item[5], 16, true, setup_mode);
        }
        else if (setup_menu_index == 4 && setup_frame == 5)
        {

            /**  
             * ----------------
             * |  Screen Saver  |
             * -----------------
             * 
             *  Duty step 
             * 
             * */
            display.drawXbm(display.getWidth() - 32, 0, sleep_icon_width, sleep_icon_height, sleep_icon_bits);
            MenuItemSelected(setup_menu_item[4], 0, true, setup_mode);
            MenuItemSelected(setup_menu_item[5], 16, false, setup_mode);
        }
        else if (setup_menu_index == 6 && setup_frame == 6)
        {
            /** 
             *   
             * Duty step 
             * 
             * --------------
             * |Freq Step |
             * --------------
             * 
             * */
            display.drawXbm(display.getWidth() - 32, 0, step_icon_width, step_icon_height, step_icon_bits);
            MenuItemSelected(setup_menu_item[5], 0, false, setup_mode);
            MenuItemSelected(setup_menu_item[6], 16, true, setup_mode);
        }
        else if (setup_menu_index == 5 && setup_frame == 6)
        {
            /** 
             * ---------------   
             * |Duty step
             * ---------------
             *            
             * Freqstep 
             * 
             * 
             * */
            display.drawXbm(display.getWidth() - 32, 0, step_icon_width, step_icon_height, step_icon_bits);
            MenuItemSelected(setup_menu_item[5], 0, true, setup_mode);
            MenuItemSelected(setup_menu_item[6], 16, false, setup_mode);
        }

        display.display();
    }
    else if (setup_page = 2)
    {
    }
}

/* *************************************************************************
 * @brief  Read the Rotary encoder , determine up or down 
 * *************************************************************************
 */
void readRotaryEncoder()
{

    int16_t temp = 0;

    value += encoder.getValue();

    if (value / 2 > last)
    {

        if (ScreenSleepEnable)

        {
            if (isScreenSleep)
            {
                xTimerStart(xSleepTimer_Handler, 10);
                display.displayOn();
                DEBUG_PRINTF("start Sleep Timer\r\n");
                isScreenSleep = false;
            }
            else
            {
                xTimerReset(xSleepTimer_Handler, 2);
                DEBUG_PRINTF("Reset Sleep Timer\r\n");
                isScreenSleep = false;
            }
            ScreenIdleTime = 0;
        }

        last = value / 2;
        up = true;
        vTaskDelay(150 / portTICK_RATE_MS);
    }
    else if (value / 2 < last)
    {
        if (ScreenSleepEnable)

        {
            if (isScreenSleep)
            {
                xTimerStart(xSleepTimer_Handler, 2);
                display.displayOn();
                DEBUG_PRINTF("start Sleep Timer\r\n");
                isScreenSleep = false;
            }
            else
            {
                xTimerReset(xSleepTimer_Handler, 2);
                DEBUG_PRINTF("Reset Sleep Timer\r\n");
                isScreenSleep = false;
            }
            ScreenIdleTime = 0;
        }

        last = value / 2;
        down = true;
        vTaskDelay(150 / portTICK_RATE_MS);
    }

    if (!setup_mode)
    {

        // up Rotary encoder
        if (up && page == 1)
        {
            up = false;
            if (menuIndex == 1 && frame == 2)
            {
                frame--;
            }

            if (menuIndex == 2 && frame == 3)
            {
                frame--;
            }
            if (menuIndex == 3 && frame == 4)
            {
                frame--;
            }
            if (menuIndex == 4 && frame == 5)
            {
                frame--;
            }
            if (menuIndex == 5 && frame == 6)
            {
                frame--;
            }
            if (menuIndex == 6 && frame == 7)
            {
                frame--;
            }
            if (menuIndex == 7 && frame == 8)
            {
                frame--;
            }

            lastmenuIndex = menuIndex;
            menuIndex--;
            if (menuIndex < 0)
            {
                menuIndex = 0;
            }
        }

        else if (up && page == 2 && menuIndex == 2) //Duty adjust
        {
            up = false; //reset button
            float up_duty = 0;
            up_duty = mypwm.getDuty() + mypwm.getDutyStep();
            if (up_duty >= 100)
                up_duty = 100;

            DEBUG_PRINTF("up_duty :%f\r\n", up_duty);
            mypwm.setDuty(up_duty);
        }
        else if (up && page == 2 && menuIndex == 3) //Freq adjust
        {
            uint32_t up_freq = 0;
            up = false; //reset button
            up_freq = mypwm.getFreq() + mypwm.getFreqStep();

            if (up_freq >= MAX_ADJUST_FREQ)
                up_freq = MAX_ADJUST_FREQ;

            DEBUG_PRINTF("up_freq :%i\r\n", up_freq);
            mypwm.setFreq(up_freq);
        }
        else if (up && page == 2 && menuIndex == 4) // auto dim
        {
            up = false; //reset button
            save_yes_no = !save_yes_no;
        }

        else if (up && page == 2 && menuIndex == 5) // save setting
        {
            up = false; //reset button
            save_yes_no = !save_yes_no;
        }
        else if (up && page == 2 && menuIndex == 6) // Reset
        {
            up = false; //reset button
            save_yes_no = !save_yes_no;
        }
        else if (up && page == 2 && menuIndex == 7) // Setup menu
        {
            /**************************************************************************
     * @     this is show setup menu when up
     * *******************************************************************************
     */
            setup_mode = true;

            Serial.printf("  setup_menu_index  %i \r\n ", setup_menu_index);
        }
        else if (up && page == 2 && menuIndex == 8) // Wifi ip
        {
            up = false; //reset button
            save_yes_no = !save_yes_no;
        }

        // down  Rotary encoder
        if (down && page == 1) //We have turned the Rotary Encoder Clockwise
        {

            down = false;
            if (menuIndex == 1 && lastmenuIndex == 0)
            {
                frame++;
            }
            else if (menuIndex == 2 && lastmenuIndex == 1)
            {
                frame++;
            }
            else if (menuIndex == 3 && lastmenuIndex == 2)
            {
                frame++;
            }
            else if (menuIndex == 4 && lastmenuIndex == 3)
            {
                frame++;
            }

            else if (menuIndex == 5 && lastmenuIndex == 4)
            {
                frame++;
            }
            else if (menuIndex == 6 && lastmenuIndex == 5)
            {
                frame++;
            }
            else if (menuIndex == 7 && lastmenuIndex == 6 && frame != 8)
            {
                frame++;
            }

            lastmenuIndex = menuIndex;
            menuIndex++;
            if (menuIndex == 9)
            {
                menuIndex--;
            }
        }
        else if (down && page == 2 && menuIndex == 2) // descrease Duty
        {
            down = false; //reset button
            float down_duty = 0;

            down_duty = mypwm.getDuty() - mypwm.getDutyStep();

            if (down_duty <= MIN_DUTY_STEP)
                down_duty = MIN_DUTY_STEP;

            DEBUG_PRINTF("down_duty :%f\r\n", down_duty);
            mypwm.setDuty(down_duty);
        }
        else if (down && page == 2 && menuIndex == 3) // descrease freq
        {
            down = false; //reset button
            uint32_t down_freq = 0;

            down_freq = mypwm.getFreq() - mypwm.getFreqStep();

            if (down_freq <= MIN_ADJUST_FREQ)
                down_freq = MIN_ADJUST_FREQ;

            DEBUG_PRINTF("down_freq :%i\r\n", down_freq);
            mypwm.setFreq(down_freq);
        }

        else if (down && page == 2 && menuIndex == 4) // auto dim
        {
            down = false; //reset button
            save_yes_no = !save_yes_no;
        }

        else if (down && page == 2 && menuIndex == 5) // save default
        {
            down = false; //reset button
            save_yes_no = !save_yes_no;
        }
        else if (down && page == 2 && menuIndex == 6) //reset
        {
            down = false; //reset button
            save_yes_no = !save_yes_no;
        }
        else if (down && page == 2 && menuIndex == 7) //Setup
        {
            /**************************************************************************
     * @     this is show setup menu when down 
     * *******************************************************************************
     */
            setup_mode = true;
        }
        else if (down && page == 2 && menuIndex == 8) //wifi ip
        {
            down = false; //reset button
            save_yes_no = !save_yes_no;
        }
    }
    else
    {
        // up Rotary encoder
        if (up && setup_page == 1)
        {
            up = false;
            if (setup_menu_index == 1 && setup_frame == 2)
            {
                setup_frame--;
            }

            if (setup_menu_index == 2 && setup_frame == 3)
            {
                setup_frame--;
            }
            if (setup_menu_index == 3 && setup_frame == 4)
            {
                setup_frame--;
            }
            if (setup_menu_index == 4 && setup_frame == 5)
            {
                setup_frame--;
            }
            if (setup_menu_index == 5 && setup_frame == 6)
            {
                setup_frame--;
            }
            if (setup_menu_index == 6 && setup_frame == 7)
            {
                setup_frame--;
            }

            setup_last_menu_index = setup_menu_index;
            setup_menu_index--;
            if (setup_menu_index < 0)
            {
                setup_menu_index = 0;
            }
        }

        else if (up && page == 2 && setup_menu_index == 2) //increase Duty
        {
            up = false; //reset button
        }
        else if (up && page == 2 && setup_menu_index == 3) //increase Freq
        {

            up = false; //reset button
        }
        else if (up && page == 2 && setup_menu_index == 4) // save setting
        {
            up = false; //reset button
        }
        else if (up && page == 2 && setup_menu_index == 5) // Reset
        {
            up = false; //reset button
        }
        else if (up && page == 2 && setup_menu_index == 6) // Setup menu
        {
            up = false; //reset button
        }

        // down  Rotary encoder
        if (down && setup_page == 1) //We have turned the Rotary Encoder Clockwise
        {

            down = false;
            if (setup_menu_index == 1 && setup_last_menu_index == 0)
            {
                setup_frame++;
            }
            else if (setup_menu_index == 2 && setup_last_menu_index == 1)
            {
                setup_frame++;
            }
            else if (setup_menu_index == 3 && setup_last_menu_index == 2)
            {
                setup_frame++;
            }
            else if (setup_menu_index == 4 && setup_last_menu_index == 3)
            {
                setup_frame++;
            }

            else if (setup_menu_index == 5 && setup_last_menu_index == 4 && setup_frame != 6)
            {
                setup_frame++;
            }

            setup_last_menu_index = setup_menu_index;
            setup_menu_index++;
            if (setup_menu_index == 7)
            {
                setup_menu_index--;
            }
        }

        else if (down && setup_page == 2 && setup_menu_index == 2) // descrease Duty
        {
            down = false; //reset button
        }
        else if (down && setup_page == 2 && setup_menu_index == 3) // descrease freq
        {
            down = false; //reset button
        }
        else if (down && setup_page == 2 && setup_menu_index == 4) // save default
        {
            down = false; //reset button
        }
        else if (down && setup_page == 2 && setup_menu_index == 5) //reset
        {
            down = false; //reset button
        }
        else if (down && setup_page == 2 && setup_menu_index == 6) //Setup
        {
            down = false; //reset button
        }
    }
}

/**
 * *************************************************************************
 * @brief  software timer call back function for Screen sleep function 
 * *************************************************************************
 */
void SleepTimerCallBackFun(TimerHandle_t xtimer)
{

    ScreenIdleTime++; // count up ScreenIdleTime
}

/**
 * *************************************************************************
 *  @brief ESP32 hardware timer interrupt call back function for Rotary encoder 
 * *************************************************************************
 */

void IRAM_ATTR rotaryISR()
{
    // Increment the counter and set the time of ISR
    encoder.service();
    // Give a semaphore that we can check in the loop

    //xSemaphoreGiveFromISR(timerSemaphore, NULL);
    // It is safe to use digitalRead/Write here if you want to toggle an output
}

/**
 * *************************************************************************
 * @brief  middel button scan function 
 * *************************************************************************
 */

void middle_button_process()
{
    // check button
    ClickEncoder::Button b = encoder.getButton();
    if (b != ClickEncoder::Open)
    {
        switch (b)
        {
        case ClickEncoder::Clicked:
            middle_click = true;

            break;
        case ClickEncoder::DoubleClicked:

            break;
        case ClickEncoder::Held:
            middle_held = true;
            break;
        }
    }

    if (middle_click) //middle_click Button is Pressed
    {
        middle_click = false; //middle_click Button is hold
        if (setup_mode == false)
        {
            //main menu
            switch (menuIndex)
            {
            case 0 /* PWM on off */:
                // if (page == 1)
                //{
                mypwm.setPwmState(!mypwm.getPwmState());
                if (!mypwm.getPwmState())
                {
                    mypwm.setDuty(0);
                }
                else
                {
                    mypwm.setDuty(mypwm.getDuty());
                }
                //}

                break;

            case 1 /* LED on off*/:

                // if (page == 1)
                // {

                mypwm.setLedState(!mypwm.getLedState());
                //  }

                break;
            case 2 /* Duty setup  */:
                if (page == 1)
                {
                    page = 2;
                }
                else if (page == 2)
                {
                    page = 1;
                }

                break;
            case 3 /* Freq setup*/:
                if (page == 1)
                {
                    page = 2;
                }
                else if (page == 2)
                {
                    page = 1;
                }
                break;
            case 4 /* Auto dim */:
                if (page == 1)
                {
                    page = 2;
                }
                else if (page == 2)
                {
                }
                break;

            case 5 /*Save  */:
                if (page == 1)
                {
                    page = 2;
                }
                else if (page == 2)
                {
                    page = 1;
                }
                break;
            case 6 /*  factor reset  */:
                if (page == 1)
                {
                    page = 2;
                }
                else if (page == 2)
                {
                    page = 1;
                }
                break;
            case 7 /*setup  */:
                if (page == 1)
                {
                    page = 2;
                }
                else if (page == 2)
                {
                    setup_mode = true;
                }
                break;
            case 8 /*WIFI  */:
                if (page == 1)
                {
                    page = 2;
                }
                else if (page == 2)
                {
                    page = 1;
                }
                break;
            }
        }
        else
        { //setup menu
            switch (setup_menu_index)
            {
            case 0: //back to main menu

                DEBUG_PRINTF("go to main menu %d \r\n", setup_menu_index);
                setup_mode = false;
                setup_menu_index = 0;
                page = 1;

                break;
            case 1:

                /* code */
                break;
            case 2:
                /* code */
                break;
            case 3:
                /* code */
                break;
            case 4:
                /* code */
                break;
            case 5:
                /* code */
                break;
            case 6:
                /* code */
                break;
            }
        }
    }
}

/***************************************************************************
 * @brief  flash the text on screen 
 * *************************************************************************
 * @param text to flash  
 * @param flashTimes  how many times flash 
 */
void flash_text(String text, uint8_t flashTimes)
{
    bool a = true;

    display.setFont(Serif_plain_12);
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.clear();
    display.drawStringMaxWidth(display.getWidth() / 2, 0, display.getWidth(), text);
    display.display();
    for (int i = 0; i < (flashTimes * 2); i++)
    {
        if (a)
        {
            display.displayOn();
        }
        else
        {
            display.displayOff();
        }
        a = !a;
        vTaskDelay(500 / portTICK_RATE_MS);
    }

    display.displayOn();
}

/**
 * @brief 
 * 
 * @param item 
 * @param position 
 * @param selected 
 */
void MenuItemSelected(String item, int position, boolean selected, bool mode)
{
    if (selected)
    {
        display.drawXbm(0, position, arrow_icon_width, arrow_icon_height, arrow_icon_bits);
        // (!mode) ? display.drawRect(0, position, display.getWidth() - 45, 16) : display.drawRect(0, position, display.getWidth(), 16);
        display.fillRect(arrow_icon_width - 6, position + 1, display.getWidth() - arrow_icon_width - 32 - 3, display.getHeight() / 2 - 3);
        display.setColor(INVERSE);
        display.setTextAlignment(TEXT_ALIGN_LEFT);
    }
    else
    {
        display.setColor(WHITE);
        display.setTextAlignment(TEXT_ALIGN_LEFT);
    }

    if (mode)
    {
        display.drawString(arrow_icon_width - 6, position + 1, item);
    }
    else
    {
        display.drawString(arrow_icon_width - 6, position, item);
    }
}

/**
 * ***********************************************************
 * @brief    Show page 2
 * *********************************************************
 * @param menuIndex 
 */
void displaySubMenuPage(int menuIndex)
{
    char strTemp[50] = {0};

    display.clear();
    display.setFont(Serif_plain_8);
    display.setColor(WHITE);
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.drawHorizontalLine(0, 0, 128);
    display.drawString(display.getWidth() / 2, 1, main_menu_item[menuIndex]);
    display.drawHorizontalLine(0, 12, 128);
    display.setTextAlignment(TEXT_ALIGN_CENTER);

    switch (menuIndex)
    {

    case 2 /* Adjust duty  */:
        display.setFont(Serif_plain_18);
        sprintf(strTemp, "%.2f%%", mypwm.getDuty());
        display.drawString((display.getWidth() / 2), 13, strTemp);
        break;
    case 3 /* Adjust Freq*/:
        display.setFont(Serif_plain_18);
        sprintf(strTemp, "%.2fKHz", (float)mypwm.getFreq() / 1000);
        display.drawString(display.getWidth() / 2, 13, strTemp);
        break;
    case 4 /* Save Default */:
        display.setFont(Serif_plain_18);
        confirmDisplay(save_yes_no);

        break;
    case 5 /* Reset */:
        display.setFont(Serif_plain_18);
        confirmDisplay(save_yes_no);
        break;
    case 6 /*Setup*/:
        display.setFont(Serif_plain_12);

        if (mypwm.getOledSleepTime() != 0)
        {
            sprintf(strTemp, "%d mins screen Off", mypwm.getOledSleepTime());
        }
        else
        {

            sprintf(strTemp, "Disable Screen Off", mypwm.getOledSleepTime());
        }

        display.drawString(display.getWidth() / 2, 13, strTemp);

        break;
    case 7 /* WiFI iP */:

        break;
    }

    display.display();
}

/***********************************************************
 * @brief    Show confirm text YES NO  on screen 
 * *********************************************************
 * @param show Yes or NO 
 * 
 */
void confirmDisplay(bool yes_no)
{

    if (yes_no) //show yes or no
    {
        display.setColor(WHITE);
        display.fillRect(3, 14, 61, 32);
        display.setColor(BLACK);
        display.drawString(32, 14, "YES");
        display.setColor(WHITE);
        display.drawString(96, 14, "NO");
    }
    else
    {
        display.setColor(WHITE);
        display.fillRect(67, 14, 61, 32);
        display.setColor(BLACK);
        display.drawString(96, 14, "NO");
        display.setColor(WHITE);
        display.drawString(32, 14, "YES");
    }
}

/****************************************************************
 * 
 * Auto step up and dowm dutyclcle  function 
 * 
 * *****************************************************************/

//Call back function
