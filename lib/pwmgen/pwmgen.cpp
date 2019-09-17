
#include "pwmgen.h"

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
    return this->_ledState;
}

bool Pwmgen::begin()
{
    SPIFFS.begin();

    if (bLoadConfig() < 0)
    {
        return false;
    }

    pinMode(this->_ledPin, OUTPUT);
    //init LED ON OFF PIN
    this->setLedState(this->_pwmState);
    // init the PWM timer
    DEBUG_PWM("## init duty : %f \r\n", this->_duty);
    DEBUG_PWM("## init freq  : %i \r\n", this->_freq);
    DEBUG_PWM("##  init duty step : %f \r\n", this->_dutyStep);
    DEBUG_PWM("## init freq step : %i \r\n", this->_freqStep);
    DEBUG_PWM("## init led state : %i \r\n", this->_ledState);
    DEBUG_PWM("## init pwm state : %i \r\n", this->_pwmState);
    this->_pwm_timer.duty_resolution = LEDC_TIMER_10_BIT;
    this->_dutyRes = (1U << (this->_pwm_timer.duty_resolution));

    uint16_t temp_duty = 0;
    //this->_pwm_timer.duty_resolu (1U << _pwm_timer.duty_resolution ) - 1U)tion = (ledc_timer_bit_t)this->_get_timer_bit(this->_freq);
    DEBUG_PWM("## init timer bit : %i \r\n", LEDC_TIMER_10_BIT);
    DEBUG_PWM("## init Duty resolution : %i \r\n", this->_dutyRes);
    this->_pwm_timer.freq_hz = this->_freq;
    this->_pwm_timer.speed_mode = LEDC_HIGH_SPEED_MODE;
    this->_pwm_timer.timer_num = LEDC_TIMER_0;
    //chnnle setup
    temp_duty = map((double)(this->_duty / this->_dutyStep), 0, (double)(100 / this->_dutyStep), 0, this->_dutyRes-1);
    this->_pwm_channel.channel = LEDC_CHANNEL_0;
    this->_pwm_channel.duty = temp_duty;
    this->_pwm_channel.gpio_num = this->_pwmPin;
    this->_pwm_channel.speed_mode = LEDC_HIGH_SPEED_MODE;
    this->_pwm_channel.hpoint = 0;
    this->_pwm_channel.timer_sel = LEDC_TIMER_0;

    ledc_timer_config(&this->_pwm_timer);

    ledc_channel_config(&this->_pwm_channel);

    esp_err_t err = ledc_fade_func_install(0);

    if (!this->_pwmState)

        this->setDuty(0);

    if (err != ESP_OK)
    {
        DEBUG_PWM("Init PWM timer Fail !!\r\n");
        return false;
    }

    return true;
}

void Pwmgen::setDutyStep(float dutyStep)
{
    this->_dutyStep = dutyStep;
}

void Pwmgen::setDuty(float duty)
{
    esp_err_t err;

    if (duty != 0)
    {

        this->_duty = duty;
        DEBUG_PWM(" this_duty:%f\r\n", this->_duty);
        DEBUG_PWM(" this_dutyRes:%i\r\n", this->_dutyRes);
        DEBUG_PWM(" this_dutyStep:%f\r\n", this->_dutyStep);
        //this->_get_timer_bit(this->_freq);
        err = ledc_set_duty_and_update(this->_pwm_channel.speed_mode, this->_pwm_channel.channel, map(this->_duty / MIN_DUTY_STEP, 0, 100 / MIN_DUTY_STEP, 0, this->_dutyRes), 0);

        if (err != ESP_OK)
            DEBUG_PWM("SET freq duty fail!!\r\n");
    }
    else
    {
        err = ledc_set_duty_and_update(this->_pwm_channel.speed_mode, this->_pwm_channel.channel, 0, 0);
    }
}

void Pwmgen::setFreq(uint32_t freq)
{
    esp_err_t err;
    // uint32_t freq_div;
    // uint8_t timer_num_bit = 0;

    // freq_div = APB_CLK_FREQ / freq;
    // while (freq_div)
    // {
    //     freq_div >>= 1;
    //     timer_num_bit++;
    // }
    this->_freq = freq;
    this->_pwm_timer.duty_resolution = LEDC_TIMER_10_BIT; //0-1023 dimming range
    this->_dutyRes = (1U << LEDC_TIMER_10_BIT ) ;
    DEBUG_PWM("Duty_resolution 0- %i\r\n", this->_dutyRes);

    err = ledc_set_freq(this->_pwm_timer.speed_mode, this->_pwm_timer.timer_num, this->_freq);
    if (err != ESP_OK)

        DEBUG_PWM("adjust Freq fail!! err:%i\r\n", err);

    if (!this->getPwmState())
        err = ledc_set_duty_and_update(this->_pwm_channel.speed_mode, this->_pwm_channel.channel, 0, 0);
    else
        err = ledc_set_duty_and_update(this->_pwm_channel.speed_mode, this->_pwm_channel.channel, map(this->_duty / MIN_DUTY_STEP, 0, 100 / MIN_DUTY_STEP, 0, this->_dutyRes-1), 0);
}

void Pwmgen::setFreqStep(uint32_t freqStep)
{
}

void Pwmgen::setPwmState(uint8_t pwmState)
{
    this->_pwmState = pwmState;
}

void Pwmgen::setLedState(uint8_t ledState)
{
    this->_ledState = ledState;


    digitalWrite(this->_ledPin, _ledState);
}

void Pwmgen::setOledSleepTime(int16_t oledSleepTime)
{
    this->_oledSleepTime = oledSleepTime;
}

int16_t Pwmgen::getOledSleepTime(void)
{
    return this->_oledSleepTime;
}

uint8_t Pwmgen::bSaveConfig()
{
    const size_t capacity = JSON_OBJECT_SIZE(8);
    // Open file for writing
    File file = SPIFFS.open(USER_JSON_FILE_PATH, FILE_WRITE);
    if (!file)
    {
        DEBUG_PWM("Can't create the %s file.", USER_JSON_FILE_PATH);
        return 1;
    }

    // Allocate a temporary JsonDocument
    // Don't forget to change the capacity to match your requirements.
    // Use arduinojson.org/assistant to compute the capacity.
    DynamicJsonDocument doc(capacity);

    // Set the values in the document
    doc["FREQ"] = 10000;
    doc["DUTY"] = 50.00;
    doc["LED_STATE"] = 1;
    doc["PWM_STATE"] = 1;
    doc["FREQ_STEP"] = 1000;
    doc["DUTY_STEP"] = 1;
    doc["OLED_SLEEP_TIME"] = 5; // minutes

    // Serialize JSON to file
    if (serializeJson(doc, file) == 0)
    {
        Serial.println(F("Failed to write to file"));
    }

    return 0;
}

uint8_t Pwmgen::bResetConfig()
{
    const size_t capacity = JSON_OBJECT_SIZE(7) + 80;
    DynamicJsonDocument doc(capacity);
    DeserializationError err;
    File f;
    String jsonStr;
    size_t byteWrite = 0;
    //Read def.json
    //set signal_gen_t data
    //save to user.json

    f = SPIFFS.open(DEFAULT_JSON_FILE_PATH, FILE_READ);

    if (!f)
    {
        DEBUG_PWM("Open file fail s \r\n");
        return 1;
    }

    err = deserializeJson(doc, f);

    if (err == DeserializationError::Ok)
    {
        this->_freq = doc["FREQ"];          // 10000
        this->_duty = doc["DUTY"];          // 50
        this->_ledState = doc["LED_STATE"]; // 0
        this->_pwmState = doc["PWM_STATE"]; // 0
        this->_freqStep = doc["FREQ_STEP"]; // 100
        this->_dutyStep = doc["DUTY_STEP"]; // 1

        //Delete orignal User,json
        //if (SPIFFS.remove(USER_JSON_FILE_PATH))
        // {
        //    Serial.printf("file :s  delete !\r\n", USER_JSON_FILE_PATH);
        //}
        //close def.json file
        f.close();
        //Create new user.json file
        f = SPIFFS.open(USER_JSON_FILE_PATH, "w+");

        // serial json to user.json
        byteWrite = serializeJson(doc, f);

        if (byteWrite > 0)
        {

            DEBUG_PWM("Reset deafult settings OK !\r\n");
        }
        else
        {
            return 1;
        }
    }
    else
    {
        DEBUG_PWM("Reset deafult settings fail \r\n");
        return 1;
    }
    //close file
    f.close();
    doc.clear();
    return 0;
    //Create File
}

/**
 * @brief Load config 
 * 
 * 
 * path 
 * default : /def.json
 * user : /user.json 
 */
uint8_t Pwmgen::bLoadConfig()
{
    const size_t capacity = JSON_OBJECT_SIZE(7) + 80;
    DynamicJsonDocument doc(capacity);
    DeserializationError err;
    File f;
    size_t byteWrite;
    String tempStr = "";
    // "/user.json" not  exists , Create file
    if (!SPIFFS.exists(USER_JSON_FILE_PATH))
    {
        //copy def.json to user.json
        DEBUG_PWM("\r\nFile %c not exist , will load default settings \r\n", USER_JSON_FILE_PATH);

        if (!SPIFFS.exists(DEFAULT_JSON_FILE_PATH)) // def.json not exitsts then create default file
        {
            DEBUG_PWM("\r\nFile %c  not exist , will auto create  default settings\r\n", DEFAULT_JSON_FILE_PATH);
            //default pwm data
            doc["FREQ"] = 10000;
            doc["DUTY"] = 50.00;
            doc["LED_STATE"] = 0;
            doc["PWM_STATE"] = 0;
            doc["FREQ_STEP"] = MIN_FREQ_STEP;
            doc["DUTY_STEP"] = MIN_DUTY_STEP;
            doc["OLED_SLEEP_TIME"] = 1; // minutes
            f = SPIFFS.open(DEFAULT_JSON_FILE_PATH, "w+");

            if (!f)
            {
                DEBUG_PWM("\r\nFile open fail %c \r\n ", DEFAULT_JSON_FILE_PATH);
                return 1;
            }

            byteWrite = serializeJson(doc, f);
            if (byteWrite <= 0)
            {
                DEBUG_PWM("\r\nDefaule setting created fial ,path:%c.\r\n", DEFAULT_JSON_FILE_PATH);
                return 1;
            }
            DEBUG_PWM("\r\nDefault setting created ,Write %d bytes.\r\n", byteWrite);
            serializeJson(doc, tempStr);
            DEBUG_PWM("json: %s", tempStr.c_str());
            byteWrite = 0;
            f.close();
            doc.clear();
            tempStr = "";
        }

        //load Default config file

        f = SPIFFS.open(DEFAULT_JSON_FILE_PATH, FILE_READ);

        if (!f)
        {
            DEBUG_PWM("\r\nFile open fail %c \r\n ", DEFAULT_JSON_FILE_PATH);
            return 1;
        }

        err = deserializeJson(doc, f);

        if (err != DeserializationError::Ok)
        {
            DEBUG_PWM("\r\n Deserial Json fail %c\r\n ", err.c_str());
            return 1;
        }

        this->_freq = (uint32_t)doc["FREQ"];       // 10000
        this->_duty = (float)doc["DUTY"];          // 50
        this->_ledState = doc["LED_STATE"];        // 0
        this->_pwmState = doc["PWM_STATE"];        // 0
        this->_freqStep = doc["FREQ_STEP"];        // 100
        this->_dutyStep = (float)doc["DUTY_STEP"]; // 1
        this->_oledSleepTime = doc["OLED_SLEEP_TIME"];
        //close file
        f.close();
        //open new file
        f = SPIFFS.open(USER_JSON_FILE_PATH, "w+");
        //save json to file
        byteWrite = serializeJson(doc, f);
        if (byteWrite <= 0)
        {
            DEBUG_PWM("\r\nDefault setting created %c .\r\n", USER_JSON_FILE_PATH);
            return 1;
        }
        DEBUG_PWM("\r\nLoad user config file OK \r\n ");
        serializeJsonPretty(doc, tempStr);
        DEBUG_PWM("json:%s", tempStr.c_str());
        doc.clear();
        f.close();
        tempStr = "";
    }

    f = SPIFFS.open(USER_JSON_FILE_PATH, FILE_READ);

    if (!f)
    {
        DEBUG_PWM("\r\nFile open fail path %c \r\n", USER_JSON_FILE_PATH);
        return 1;
    }

    err = deserializeJson(doc, f);

    if (err != DeserializationError::Ok)
    {
        DEBUG_PWM("\r\nDeserial config fail  path:%c\r\n", USER_JSON_FILE_PATH);
        DEBUG_PWM(err.c_str());
        return 1;
    }
    this->_freq = doc["FREQ"];                     // 10000
    this->_duty = doc["DUTY"];                     // 50
    this->_ledState = doc["LED_STATE"];            // 0
    this->_pwmState = doc["PWM_STATE"];            // 0
    this->_freqStep = doc["FREQ_STEP"];            // 100
    this->_dutyStep = doc["DUTY_STEP"];            // 1
    this->_oledSleepTime = doc["OLED_SLEEP_TIME"]; // 1
    DEBUG_PWM("\r\nLoad settings OK , path:%c \r\n ", USER_JSON_FILE_PATH);
    serializeJsonPretty(doc, tempStr);
    DEBUG_PWM("json :%s", tempStr.c_str());
    f.close();
    doc.clear();
    tempStr = "\0";
    return 0;
}

//private function

/**
 * @brief get timer bit will  auto set duty resolution and duty_step
 * The range of the duty cycle values passed to functions depends on selected duty_resolution and should be from 0 to (2 ** duty_resolution) 1 . 
 * For example, if the selected duty resolution is 10, then the duty cycle values can range from 0 to 1023. This provides the resolution of ~0.1%.
 * @param freq 
 * @return ledc_timer_bit_t 
 */
ledc_timer_bit_t Pwmgen::_get_timer_bit(uint32_t freq)

{
    uint32_t freq_div;
    uint8_t timer_num_bit = 0;

    freq_div = APB_CLK_FREQ / freq;
    while (freq_div)
    {
        freq_div >>= 1;
        timer_num_bit++;
    }

    Serial.printf("how many bit shift :%i\r\n", timer_num_bit);

    this->_dutyRes = (1U << (timer_num_bit - 1)) - 1U;
    Serial.printf("this->_dutyRes:%i\r\n", this->_dutyRes);
    Serial.printf("timer bit is:%i\r\n", (ledc_timer_bit_t)(timer_num_bit - 1));
    return (ledc_timer_bit_t)(timer_num_bit - 1);
}
