
#include "pwmgen.h"
//#define DEBUG_OUTPUT
#ifdef DEBUG_OUTPUT
#define DEBUG_PRINTF(format, ...) printf(format, ##__VA_ARGS__)
#else
#define DEBUG_PRINTF(format, ...)
#endif

Pwmgen::~Pwmgen()
{
}
/**
 * @brief Construct a new pwm signal::pwm signal object
 * 
 * @param pwmPin   PWM output pin 
 * @param ledPin   LED on off Control 
 */
Pwmgen::Pwmgen(int pwmPin, int ledPin) : _pwmPin(pwmPin), _ledPin(ledPin)
{
}

Pwmgen::Pwmgen() : _pwmPin(DEFAULT_PWM_OUTPUT_PIN), _ledPin(DEFAULT_LED_OUTPUT_PIN)
{
}
float Pwmgen::getDuty()
{
    return this->_duty;
}
uint32_t Pwmgen::getFreq()
{
    return this->_freq;
}
float Pwmgen::getDutyStep()
{
    return this->_dutyStep;
}
uint32_t Pwmgen::getFreqStep()
{
    return this->_freqStep;
}
uint8_t Pwmgen::getPwmState()
{

    return this->_pwmState;
}
uint8_t Pwmgen::getLedState()
{

    int level;
    level = gpio_get_level((gpio_num_t)this->_ledPin);

    if (this->_ledState != level)
    {
        gpio_set_level((gpio_num_t)this->_ledPin, this->_ledState);
    }
    return gpio_get_level((gpio_num_t)this->_ledPin);
}

bool Pwmgen::begin()
{
    pwm_err pwmerr;
    //begin SPIFFS for load settings
    if (!SPIFFS.begin())
    {
        return false;
    }
    //check file is exists
    if (!SPIFFS.exists(USER_JSON_FILE_PATH))
    {
        Pwmgen::_createDefaultSettings(USER_JSON_FILE_PATH);
    }

    if (!SPIFFS.exists(DEFAULT_JSON_FILE_PATH))
    {
        Pwmgen::_createDefaultSettings(DEFAULT_JSON_FILE_PATH);
    }

    // Pwmgen::_createDefaultSettings(USER_JSON_FILE_PATH);

    if (loadSettings(USER_JSON_FILE_PATH) == PWM_ERR)
    {
        return false;
    }
    /*gpio setting*/

    gpio_pad_select_gpio(this->_ledPin);
    gpio_set_direction((gpio_num_t)this->_ledPin, GPIO_MODE_INPUT_OUTPUT);
    gpio_set_level((gpio_num_t)this->_ledPin, 0); // init gpio state

    /* PWM timer setting */
    this->_pwm_timer.duty_resolution = LEDC_TIMER_10_BIT;
    this->_dutyRes = (1U << (this->_pwm_timer.duty_resolution));
    uint16_t temp_duty = 0;
    //this->_pwm_timer.duty_resolu (1U << _pwm_timer.duty_resolution ) - 1U)tion = (ledc_timer_bit_t)this->_get_timer_bit(this->_freq);
    DEBUG_PRINTF("## init timer bit : %i \r\n", LEDC_TIMER_10_BIT);
    DEBUG_PRINTF("## init Duty resolution : %i \r\n", this->_dutyRes);
    this->_pwm_timer.freq_hz = this->_freq;
    this->_pwm_timer.speed_mode = LEDC_HIGH_SPEED_MODE;
    this->_pwm_timer.timer_num = LEDC_TIMER_0;
    temp_duty = map((double)(this->_duty / this->_dutyStep), 0, (double)(100 / this->_dutyStep), 0, this->_dutyRes - 1);
    this->_pwm_channel.channel = LEDC_CHANNEL_0;
    this->_pwm_channel.duty = temp_duty;
    this->_pwm_channel.gpio_num = this->_pwmPin;
    this->_pwm_channel.speed_mode = LEDC_HIGH_SPEED_MODE;
    this->_pwm_channel.hpoint = 0;
    this->_pwm_channel.timer_sel = LEDC_TIMER_0;
    ledc_timer_config(&this->_pwm_timer);
    ledc_channel_config(&this->_pwm_channel);
    esp_err_t err = ledc_fade_func_install(0);
    /* set default state */
    err = ledc_set_duty_and_update(this->_pwm_channel.speed_mode, this->_pwm_channel.channel, 0, 0);
    this->_pwmState = 0;
    this->_ledState = 0;
    if (err != ESP_OK)
    {
        DEBUG_PRINTF("Init PWM timer Fail !!\r\n");

        return false;
    }

    return true;
}

void Pwmgen::setDutyStep(float dutyStep)
{
    if (dutyStep > MAX_DUTY_STEP)
        dutyStep = MAX_DUTY_STEP;
    if (dutyStep < MIN_DUTY_STEP)
        dutyStep = MIN_DUTY_STEP;
    this->_dutyStep = dutyStep;
}

int16_t Pwmgen::getAutoDimStepTime(void)
{
    return _autoDimStepTime;
}
void Pwmgen::setAutoDimStepTime(int16_t dimTime)
{
    if (dimTime > MAX_AUTO_DIM_STEP_TIME)
    {
        dimTime = MAX_AUTO_DIM_STEP_TIME;
    }

    if (dimTime < MIN_AUTO_DIM_STEP_TIME)
    {
        dimTime = MIN_AUTO_DIM_STEP_TIME;
    }
    this->_autoDimStepTime = dimTime;
}
/**
 * @brief Get the pwm led on priority 
 * 
 * 0-->INDEPENDENT  
 * 1-->led on then pwm on 
 * 2-->pwm on then led on 
 * 
 * @return PWM_on_sequence_t  
 */
PWM_on_sequence_t Pwmgen::getOnPriority(void)
{
    return (PWM_on_sequence_t)this->_turnOnPriority;
}
void Pwmgen::setOnPriority(int8_t type)
{
    if (type > PWM_ON_THEN_LED_ON)
    {
        type = PWM_ON_THEN_LED_ON;
    }

    if (type < 0)
    {
        type = INDEPENDENT;
    }

    this->_turnOnPriority = type;
}

void Pwmgen::setDuty(float duty)
{
    esp_err_t err;
    this->_duty = duty;
    if (this->_pwmState == 1)
    {
        if (duty != 0)
        {

            err = ledc_set_duty_and_update(this->_pwm_channel.speed_mode, this->_pwm_channel.channel, map((this->_duty / MIN_DUTY_STEP), 0, (100 / MIN_DUTY_STEP), 0, this->_dutyRes), 0);

            if (err != ESP_OK)
                DEBUG_PRINTF("Set pwm signal fail!!\r\n");
            else
                DEBUG_PRINTF("Set pwm signal Completed!!\r\n");
        }
        else
        {
            err = ledc_set_duty_and_update(this->_pwm_channel.speed_mode, this->_pwm_channel.channel, 0, 0);
        }
    }
}

void Pwmgen::setFreq(uint32_t freq)
{
    esp_err_t err;
    // uint32_t freq_div;
    // uint8_t timer_num_bit = 0;

    // freq_div = APB_CLK_FREQ / freq;
    // while (freq_div)Aut
    // {
    //     freq_div >>= 1;
    //     timer_num_bit++;
    // }
    this->_freq = freq;
    this->_pwm_timer.duty_resolution = LEDC_TIMER_10_BIT; //0-1023 dimming range
    this->_dutyRes = (1U << LEDC_TIMER_10_BIT);
    DEBUG_PRINTF("Duty_resolution 0- %i\r\n", this->_dutyRes);

    err = ledc_set_freq(this->_pwm_timer.speed_mode, this->_pwm_timer.timer_num, this->_freq);
    if (err != ESP_OK)

        DEBUG_PRINTF("adjust Freq fail!! err:%i\r\n", err);

    if (!this->getPwmState())
        err = ledc_set_duty_and_update(this->_pwm_channel.speed_mode, this->_pwm_channel.channel, 0, 0);
    else
        err = ledc_set_duty_and_update(this->_pwm_channel.speed_mode, this->_pwm_channel.channel, map(this->_duty / MIN_DUTY_STEP, 0, 100 / MIN_DUTY_STEP, 0, this->_dutyRes - 1), 0);
}

void Pwmgen::setFreqStep(uint32_t freqStep)
{
    if (freqStep > MAX_FREQ_STEP)
        freqStep = MAX_FREQ_STEP;
    if (freqStep < MIN_FREQ_STEP)
        freqStep = MIN_FREQ_STEP;
    this->_freqStep = freqStep;
}

void Pwmgen::setPwmState(uint8_t pwmState)
{
    esp_err_t err;
    if (pwmState == 1)
    {
        this->_pwmState = 1;
        setDuty(getDuty());
    }
    if (pwmState == 0)
    {
        this->_pwmState = 0;
        err = ledc_set_duty_and_update(this->_pwm_channel.speed_mode, this->_pwm_channel.channel, 0, 0);
    }
}

void Pwmgen::setLedState(uint8_t ledState)
{
    int level;
    this->_ledState = ledState;

    /*check pin level*/
    level = gpio_get_level((gpio_num_t)this->_ledPin);
    DEBUG_PRINTF("1 GPIO PWM State is %d \r\n", level);
    if ((uint8_t)level != ledState)
    {
        Serial.println("Set pin ");

        gpio_set_level((gpio_num_t)this->_ledPin, this->_ledState);
    }
    Serial.println("Read pin state:");
    Serial.println(gpio_get_level((gpio_num_t)this->_ledPin));
}

void Pwmgen::setOledSleepTime(int16_t oledSleepTime)
{
    if (oledSleepTime > MAX_SLEEP_TIME)

        oledSleepTime = MAX_SLEEP_TIME;
    else if (oledSleepTime < 0)
        oledSleepTime = 0;

    this->_oledSleepTime = oledSleepTime;
}

int16_t Pwmgen::getOledSleepTime(void)
{
    return this->_oledSleepTime;
}

int16_t Pwmgen::getLedOnDealy()
{
    return this->_ledOnDelay;
}
int16_t Pwmgen::getPwmOnDealy()
{
    return this->_pwmOnDelay;
}

void Pwmgen::setLedOnDealy(int16_t onDelay)
{
    if (onDelay >= LED_MAX_ON_DELAY)

        onDelay = LED_MAX_ON_DELAY;

    if (onDelay <= 0)

        onDelay = 0;

    this->_ledOnDelay = onDelay;
}
void Pwmgen::setPwmOnDealy(int16_t onDelay)
{
    if (onDelay >= PWM_MAX_ON_DELAY)

        onDelay = PWM_MAX_ON_DELAY;

    if (onDelay <= 0)

        onDelay = 0;

    this->_pwmOnDelay = onDelay;
}

pwm_err Pwmgen::saveSettings(void)
{
    const size_t capacity = JSON_OBJECT_SIZE(11) + 140;
    //DynamicJsonDocument doc(capacity);
    StaticJsonDocument<capacity> doc;
    DeserializationError err;
    File f;
    size_t byteWrite;

    //Create JSON for Default system settings
    doc["FREQ"] = this->_freq; //10K hz
    doc["DUTY"] = this->_duty; // 50% duty
    //doc["LED_STATE"] = this->_ledState;            //  normal off
    //doc["PWM_STATE"] = this->_pwmState;            //  normal off
    doc["FREQ_STEP"] = this->_freqStep;                 // min frequency step value 100 means 0.1kHZ
    doc["DUTY_STEP"] = this->_dutyStep;                 // min  frequency step value 0.5 means 0.5% duty
    doc["OLED_SLEEP_TIME"] = this->_oledSleepTime;      //oled screen off saving time 1 minute
    doc["LED_ON_DELAY"] = this->_ledOnDelay;            // LED turn on delay 0mS
    doc["PWM_ON_DELAY"] = this->_pwmOnDelay;            // PWM signal turn on delay 0mS
    doc["TURN_ON_PRI"] = this->_turnOnPriority;         //  LED and PWM signal turn on sequence
    doc["AUTO_DIM_STEP_TIME"] = this->_autoDimStepTime; //auto dimming time 500 mSecond .

    DEBUG_PRINTF("Create JSON contents below :\r\n  ");

    byteWrite = serializeJsonPretty(doc, Serial);

    // if (SPIFFS.exists(USER_JSON_FILE_PATH))
    // {
    //     if (SPIFFS.remove(USER_JSON_FILE_PATH))
    //         DEBUG_PRINTF("File removed %s \r\n", USER_JSON_FILE_PATH);
    //     else
    //         DEBUG_PRINTF("File not remove %s \r\n", USER_JSON_FILE_PATH);
    // }
    f = SPIFFS.open(USER_JSON_FILE_PATH, "w+");
    if (!f)
    {
        DEBUG_PRINTF("File not open %s \r\n", USER_JSON_FILE_PATH);
        return PWM_ERR;
    }
    String str;
    byteWrite = serializeJson(doc, str);

    if (byteWrite <= 0)

    {
        DEBUG_PRINTF("Json not save %s \r\n", USER_JSON_FILE_PATH);
        return PWM_ERR;
    }
    f.print(str);
    DEBUG_PRINTF("JSON write  %d bytes \r\n", byteWrite);

    f.close();
    doc.clear();
    return PWM_OK;
}

pwm_err Pwmgen::resetSettings(void)
{
    const size_t capacity = JSON_OBJECT_SIZE(11) + 140;
    //DynamicJsonDocument doc(capacity);
    StaticJsonDocument<capacity> doc;
    DeserializationError err;
    File f1, f2;
    String jsonStr;
    size_t fileWrite = 0;

    f1 = SPIFFS.open(DEFAULT_JSON_FILE_PATH, "r");
    f2 = SPIFFS.open(USER_JSON_FILE_PATH, "w+");
    if (!f1)
    {
        DEBUG_PRINTF("%s open failed  \r\n", f1.name());
        return PWM_ERR;
    }
    else if (!f2)
    {
        DEBUG_PRINTF("%s open failed  \r\n", f2.name());
        return PWM_ERR;
    }
    err = deserializeJson(doc, f1);

    if (err)
    {
        Serial.printf("deserialize Json fail %d ", err.code());
        return PWM_ERR;
    }
    this->_freq = doc["FREQ"];                          // 10000
    this->_duty = doc["DUTY"];                          // 50
    this->_ledState = 0;                                // 0
    this->_pwmState = 0;                                // 0
    this->_freqStep = doc["FREQ_STEP"];                 // 100
    this->_dutyStep = doc["DUTY_STEP"];                 // 0.5
    this->_oledSleepTime = doc["OLED_SLEEP_TIME"];      // 1
    this->_ledOnDelay = doc["LED_ON_DELAY"];            // 0
    this->_pwmOnDelay = doc["PWM_ON_DELAY"];            // 0
    this->_turnOnPriority = doc["TURN_ON_PRI"];         // 0
    this->_autoDimStepTime = doc["AUTO_DIM_STEP_TIME"]; // 500

    fileWrite = serializeJson(doc, f2);
    if (!fileWrite)
    {
        Serial.printf("Serialize Json fail %s ", f2.name());
        return PWM_ERR;
    }

    f1.close();
    f2.close();
    doc.clear();
    return PWM_OK;
    //Create File
}
/**
 * @brief 
 * json =
 * "{\"FREQ\":10000,\"DUTY\":50,\"LED_STATE\":0,\"PWM_STATE\":0,\"FREQ_STEP\":100,\"DUTY_STEP\":0.5,\"OLED_SLEEP_TIME\":1,\"LED_ON_DELAY\":0,\"PWM_ON_DELAY\":0,\"TURN_ON_PRI\":0,\"AUTO_DIM_STEP\":500}";
 * @param file 
 * @return pwm_err 
 */
pwm_err Pwmgen::_createDefaultSettings(const char *file)
{
    const size_t capacity = JSON_OBJECT_SIZE(11) + 140;
    //DynamicJsonDocument doc(capacity);
    StaticJsonDocument<capacity> doc;

    File f;
    size_t byteWrite;

    //Create JSON for Default system settings
    doc["FREQ"] = 10000; //10K hz
    doc["DUTY"] = 50.0f; // 50% duty
    //doc["LED_STATE"] = 0;                      //  normal off
    //doc["PWM_STATE"] = 0;                      //  normal off
    doc["FREQ_STEP"] = 500;                             // min frequency step value 100 means 0.1kHZ
    doc["DUTY_STEP"] = 1.0f;                            // min  frequency step value 0.5 means 0.5% duty
    doc["OLED_SLEEP_TIME"] = 1;                         //oled screen off saving time 1 minute
    doc["LED_ON_DELAY"] = 0;                            // LED turn on delay 0mS
    doc["PWM_ON_DELAY"] = 0;                            // PWM signal turn on delay 0mS
    doc["TURN_ON_PRI"] = (uint8_t)INDEPENDENT;          //  LED and PWM signal turn on sequence
    doc["AUTO_DIM_STEP_TIME"] = MIN_AUTO_DIM_STEP_TIME; //auto dimming time 500 mSecond .

    DEBUG_PRINTF("Create JSON contents below :\r\n  ");
    serializeJsonPretty(doc, Serial);
    f = SPIFFS.open(file, "w+");

    if (!f)
    {
        DEBUG_PRINTF("\r\nFile open failed %s \r\n ", file);
        return PWM_ERR;
    }

    byteWrite = serializeJson(doc, f);

    if (byteWrite <= 0)
    {
        DEBUG_PRINTF("\r\nFile write failed. path %s \r\n ", DEFAULT_JSON_FILE_PATH);

        return PWM_ERR;
    }
    DEBUG_PRINTF("\r\nFile write completed. byte write : %d \r\n ", byteWrite);
    f.close();
    doc.clear();
    return PWM_OK;
}

/**
 * @brief 
 *  load settings from SPIFFS 
 * 
 * @param file 
 * @return pwm_err 
 */
pwm_err Pwmgen::loadSettings(const char *file)
{
    const size_t capacity = JSON_OBJECT_SIZE(11) + 140;
    //DynamicJsonDocument doc(capacity); // documents smaller than 1KB
    StaticJsonDocument<capacity> doc; // documents larger than 1KB
    DeserializationError err;
    String str;
    File f;

    f = SPIFFS.open(file, "r+");

    if (!f)
    {
        DEBUG_PRINTF("file open fail %d:\r\n", f);
        return PWM_ERR;
    }
    else if (f.size() == 0)
    {
        DEBUG_PRINTF("file is empty %s:\r\n", f.name());
        _createDefaultSettings(f.name());
        loadSettings(f.name());
        //return PWM_ERR;
    }

    //DEBUG_PRINTF("file size is %d\r\n", f.size());
    // str = f.readString();
    // Serial.println(str);

    err = deserializeJson(doc, f);

    if (err)
    {
        ///DEBUG_PRINTF("Serialization error  , code: %d \r\n", err.code());
        ///return PWM_ERR;
        _createDefaultSettings(USER_JSON_FILE_PATH);
        loadSettings(USER_JSON_FILE_PATH);
    }

    this->_freq = (uint32_t)doc["FREQ"];                    // 10000
    this->_duty = (float)doc["DUTY"];                       // 50
    this->_freqStep = (uint32_t)doc["FREQ_STEP"];           // 100
    this->_dutyStep = (float)doc["DUTY_STEP"];              // 0.5
    this->_oledSleepTime = (int16_t)doc["OLED_SLEEP_TIME"]; // 1
    this->_ledOnDelay = (int16_t)doc["LED_ON_DELAY"];       // 0
    this->_pwmOnDelay = (int16_t)doc["PWM_ON_DELAY"];       // 0
    this->_turnOnPriority = (int8_t)doc["TURN_ON_PRI"];     // 0

    this->_autoDimStepTime = (uint16_t)doc["AUTO_DIM_STEP_TIME"]; // 500

    DEBUG_PRINTF("Serialization data:%d \r\n", err.code());
    serializeJsonPretty(doc, Serial);
    DEBUG_PRINTF("\r\n");
    f.close();
    doc.clear();
    return PWM_OK;
}
