#ifndef _PWMGEN_H_
#define _PWMGEN_H_
#include <Arduino.h>
#include <esp_err.h>
#include <driver/ledc.h> //ESP IDF led driver library
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <driver/gpio.h>

/* a=target variable, b=bit number to act upon 0-n */
#define BIT_SET(a, b) ((a) |= (1ULL << (b)))
#define BIT_CLEAR(a, b) ((a) &= ~(1ULL << (b)))
#define BIT_FLIP(a, b) ((a) ^= (1ULL << (b)))
#define BIT_CHECK(a, b) (!!((a) & (1ULL << (b)))) // '!!' to make sure this returns 0 or 1

/* x=target variable, y=mask */
#define BITMASK_SET(x, y) ((x) |= (y))
#define BITMASK_CLEAR(x, y) ((x) &= (~(y)))
#define BITMASK_FLIP(x, y) ((x) ^= (y))
#define BITMASK_CHECK_ALL(x, y) (((x) & (y)) == (y)) // warning: evaluates y twice
#define BITMASK_CHECK_ANY(x, y) ((x) & (y))
//#include <math.h>
#define DEFAULT_JSON_FILE_PATH ("/def.json")
#define USER_JSON_FILE_PATH ("/user.json")
#define DEFAULT_PWM_OUTPUT_PIN GPIO_NUM_33
#define DEFAULT_LED_OUTPUT_PIN GPIO_NUM_32
#define MAX_ADJUST_FREQ (100000) //max adjust freq 100Khz
#define MIN_ADJUST_FREQ (1000)   //min adjust freq 1Khz
#define MIN_FREQ_STEP (100)      //min  freq step 0.1Khz
#define MIN_DUTY_STEP (0.5)

#define DEBUG_PWM(...) Serial.printf(__VA_ARGS__)

#ifndef DEBUG_PWM
#define DEBUG_PWM(...)
#endif
typedef enum
{
    PWM_OK = 0,
    PWM_ERR,

} pwm_err;

typedef enum
{
    INDEPENDENT = 0,
    LED_ON_THEN_PWM_ON,
    PWM_ON_THEN_LED_ON

} PWM_on_sequence_t;

class Pwmgen
{
private:
    /* data */

    uint32_t _freq;
    float _duty;
    uint32_t _freqStep;
    float _dutyStep;
    uint8_t _pwmState;
    uint8_t _ledState;
    uint8_t _pwmPin;
    uint8_t _ledPin;
    uint16_t _oledSleepTime;
    uint16_t _ledOnDelay;
    uint16_t _pwmOnDelay;
    uint8_t _turnOnPriority;
    uint16_t _autoDimDuration;

    // _timerBit
    ledc_channel_config_t _pwm_channel;
    ledc_timer_config_t _pwm_timer;
    ledc_timer_bit_t _get_timer_bit(uint32_t freq);
    //Resolution of PWM duty (0-  2**timer_bit_ -1 )
    uint16_t _dutyRes;

    pwm_err _createDefaultSettings(const char *file); // create def.json and user.json file

public:
    // uint8_t settingConfig( PWM_config_t config);

    pwm_err resetSettings(void);            // load def.json to user.json
    pwm_err saveSettings(void);             //save setting to user.json
    pwm_err loadSettings(const char *file); //load "user,json" to system

    bool begin(); // load config
    float getDuty(void);
    uint32_t getFreq(void);
    float getDutyStep(void);
    uint32_t getFreqStep(void);
    uint8_t getPwmState(void);
    uint8_t getLedState(void);
    uint16_t getLedOnDealy();
    uint16_t getPwmOnDealy();

    PWM_on_sequence_t getOnPriority(void);

    void setOnPriority(uint8_t type);
    void setDuty(float duty);
    void setDutyStep(float dutyStep);

    void setFreq(uint32_t freq);
    void setFreqStep(uint32_t freqStep);

    void setLedOnDealy(uint16_t onDelay);
    void setPwmOnDealy(uint16_t onDelay);

    void setPwmState(uint8_t pwmState);
    void setLedState(uint8_t ledState);

    void setOledSleepTime(uint16_t oledSleepTime);

    uint16_t getOledSleepTime(void);

    Pwmgen(int pwmOutputPin, int ledOutputPin);

    Pwmgen();

    virtual ~Pwmgen();
};

#endif