#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Update.h>
#include "HX711.h"

#define FIRMWARE_VERSION "1.1"

/**
 * @brief Kurulum öncesi ayar,
 * CIHAZIN MODELİ
 *
 * MODEL => cihazın handfizz, legfizz veya handfizz olup olmadığını belirtin. const string olarak belirleyin.
 * örn: "handfizz"
 */
#define MODEL "handfizz"

/**
 * @brief Kurulum öncesi ayar,
 * CIHAZIN KİMLİĞİ/SERİ NUMARASI
 *
 * MODEL => cihazın handfizz, legfizz veya handfizz olup olmadığını belirtin. const string olarak belirleyin.
 * örn: "1"
 */
#define ID "200524"

/**
 * @brief Kurulum öncesi ayar,
 * AKTİVASYON KODU AYARI
 *
 * ACTIVATION_KEY => esp yi aktif etmek için şifre. const string olarak belirleyin.
 * örn: "Arda+R-OBofizz"
 */
#define ACTIVATION_KEY "key"

/**
 * @brief Kurulum öncesi ayar,
 * SURUCU AYARI
 *
 * surucu => Büyük sürücü için 1, Küçük için 0 giriniz.
 */
#define surucu 1

/**
 * Kurulum ayarlarını yapılandırır,
 * Kurulum öncesi ayarlara göre tanımlama yapar,
 * ne yaptığınızı bilmiyorsanız buraya dokunmayın.
 */
#if surucu == 1
#define speedRes 1000
#elif surucu == 0
#define speedRes 250
#endif
#if speedRes == 250
#define speedDiv 4
#elif speedRes == 1000
#define speedDiv 1
#endif

#if CONFIG_FREERTOS_UNICORE
static const BaseType_t app_cpu = 0;
#else
static const BaseType_t app_cpu = 1;
#endif
#define ADV_NAME "RoboFizz"

#define SERVICE_W_UUID "4fafc201-1fb5-459e-8fcc-c5c9c3319141"

#define SPEED_W_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a1"
#define DURATION_W_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a2"
#define POSITION_W_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a3"
#define CMD_W_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a4"
#define VIBRATE_W_UUID "beb5483e-36e1-4688-b7f5-ea07361b26b1"
#define VIBRATE_RECOIL_W_UUID "beb5483e-36e1-4688-b7f5-ea07361b26b3"
#define IS_VIBRATE_W_UUID "beb5483e-36e1-4688-b7f5-ea07361b26b4"

#define SERVICE_R_UUID "4fafc201-1fb5-459e-8fcc-c5c9c3319142"

#define LOADCELL_R_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a5"
#define TARGET_POS_R_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a6"
#define POSITION_R_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a7"
#define SPEED_R_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define DURATION_R_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a9"
#define ACTIVE_SMP_DUR_R_UUID "beb5483e-36e1-4688-b7f5-ea07361b26b0"
#define DIR_R_UUID "beb5483e-36e1-4688-b7f5-ea07361b26b2"

#define SERVICE_KEY_UUID "4fafc201-1fb5-459e-8fcc-c5c9c3319143"
#define ACTIVATION_KEY_W_UUID "beb5483e-36e1-4688-b7f5-ea07361b26b5"
#define ACTIVATION_KEY_R_UUID "beb5483e-36e1-4688-b7f5-ea07361b26b6"

#define SERVICE_SPECS_UUID "4fafc201-1fb5-459e-8fcc-c5c9c3319144"
#define DEVICE_ID_UUID "beb5483e-36e1-4688-b7f5-ea07361b26c0"
#define DEVICE_MODEL_UUID "beb5483e-36e1-4688-b7f5-ea07361b26c1"

#define SERVICE_OTA_UUID "4fafc201-1fb5-459e-8fcc-c5c9c3319145"
#define UPDATE_CHANNEL_UUID "beb5483e-36e1-4688-b7f5-ea07361b26d0"
#define FIRMWARE_VERSION_UUID "beb5483e-36e1-4688-b7f5-ea07361b26d1"

#define BLE_PROPS_ALL BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
#define BLE_PROPS_READ_NOTY BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ
#define BLE_PROPS_WRITE_NR BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
#define seconds() millis() / 1000

#define pwmChannel 8
#define pwmRes 8

#define DOUT 33
#define SCK 32

#define stepPin 2
#define dirPin 18

#define cw 1
#define ccw -1

#define speedOffset_L 200
#define speedOffset_H 5000

static uint16_t stepSpeed = 0;
static int8_t direction = 0;

static bool isStep = false;
static long stepStartTime = 0;
static long stepEndTime = 0;

static long position1 = 0;
static long position2 = 0;
static long TargetPosition = 0;
static long CurrentVibratePosition = 0;
static long CurrentPosition = 0;
static long lastPosition = 0;

static int vibrationRate = 40;
static int vibrationRecoilRate = 4;
static bool isVibration = false;

static bool isVibDone = false;
static bool isVibPosDone = true;

static int duration = 20; // seconds
static int sweepStartTime = 0;
static int sweepEndTime = 0;

static int loadCellVal = 0;
static int8_t loadVector = 1;

bool sweepCheckerP1 = false;
bool sweepCheckerP2 = false;
bool onSweep = false;

static bool speedOnWrite = false;
static bool posOnWrite = false;
static bool cmdOnWrite = false;
static bool durOnWrite = false;
static bool vibOnWrite = false;
static bool vibRecOnWrite = false;
static bool isVibOnWrite = false;
static bool isActivated = false;

bool deviceConnected = false;
bool isUpdating = false;
size_t totalSize = 0;
size_t receivedSize = 0;

BLEServer *pServer = NULL;

BLEAdvertising *pAdvertising = NULL;

BLEService *write_service = NULL;
BLEService *read_service = NULL;
BLEService *key_service = NULL;
BLEService *dev_service = NULL;
BLEService *OTA_service = NULL;

BLECharacteristic *update_channel_ctsc = NULL;
BLECharacteristic *firmware_version_ctsc = NULL;

/// @brief key servisi
BLECharacteristic *activatioKey_r_ctsc = NULL;
BLECharacteristic *activatioKey_w_ctsc = NULL;

BLECharacteristic *dev_id_ctsc = NULL;
BLECharacteristic *dev_model_ctsc = NULL;

BLECharacteristic *speed_w_ctsc = NULL;
BLECharacteristic *duration_w_ctsc = NULL;
BLECharacteristic *position_w_ctsc = NULL;
BLECharacteristic *cmd_w_ctsc = NULL;
BLECharacteristic *vibration_w_ctsc = NULL;
BLECharacteristic *vibrationRecoil_w_ctsc = NULL;
BLECharacteristic *isVibration_w_ctsc = NULL;

BLECharacteristic *loadcell_r_ctsc = NULL;
BLECharacteristic *targetPos_r_ctsc = NULL;
BLECharacteristic *speed_r_ctsc = NULL;
BLECharacteristic *duration_r_ctsc = NULL;
BLECharacteristic *position_r_ctsc = NULL;
BLECharacteristic *posChangeFlag_ctsc = NULL;
BLECharacteristic *direction_ctsc = NULL;

HX711 scale;


void printBrand(char c1, const char *s, char c2)
{
    Serial.println();
    for (int i = 0; i <= 50; i++)
    {
        i == 25 ? Serial.print(s) : i < 25 ? Serial.print(c1)
                                           : Serial.print(c2);
    }

    Serial.println();
}

String stdToStr(std::string stdStr)
{
    return stdStr.c_str();
}

int stdToInt(std::string stdStr)
{
    return stdToStr(stdStr).toInt();
}

std::string strToStd(String str)
{
    return std::string((str.c_str()));
}

int getLoadCellVal(int8_t vector, uint8_t average)
{
    loadCellVal = scale.get_units(average) * vector;
    return loadCellVal;
}

void stepperStop()
{
    ledcDetachPin(stepPin);
    if (isStep)
    {
        stepEndTime = millis();
        lastPosition += ((stepEndTime - stepStartTime) * (stepSpeed / speedRes) * direction);
        CurrentPosition = lastPosition;
        isStep = false;
    }
}

void stepperRun(uint16_t speed, int8_t dir)
{
    if (direction != dir || stepSpeed != speed)
    {
        stepperStop();
    }
    if (!isStep)
    {
        direction = dir;
        stepSpeed = speed;
        ledcChangeFrequency(pwmChannel, stepSpeed, pwmRes);
        if (direction == cw)
        {
            digitalWrite(dirPin, 1);
        }
        else if (direction == ccw)
        {
            digitalWrite(dirPin, 0);
        }
        ledcAttachPin(stepPin, pwmChannel);
        stepStartTime = millis();
        isStep = true;
    }
}

void posHandler()
{
    if (isStep)
    {
        CurrentPosition = ((millis() - stepStartTime) * (stepSpeed / speedRes) * direction) + lastPosition;
    }
}

void stepperSetSpeed(uint16_t speed)
{
    stepperStop();
    stepSpeed = speed / speedDiv;
}

void savePos(uint8_t p)
{
    stepperStop();
    if (p == 1)
    {
        position1 = CurrentPosition;
    }
    else if (p == 2)
    {
        position2 = CurrentPosition;
    }
}

static bool runToPosition(long p, uint16_t speed)
{
    posHandler();
    if (p > lastPosition)
    {
        stepperRun(speed, cw);
        if (CurrentPosition >= p)
        {
            stepperStop();
            // targetPos_r_ctsc->setValue("1");
            // targetPos_r_ctsc->notify();
            return true;
        }
        else
        {
            return false;
        }
    }
    else if (p < lastPosition)
    {
        stepperRun(speed, ccw);
        if (CurrentPosition <= p)
        {
            stepperStop();
            // targetPos_r_ctsc->setValue("1");
            // targetPos_r_ctsc->notify();
            return true;
        }
        else
        {
            return false;
        }
    }
    else if (isStep)
    {
        stepperStop();
        return false;
    }
    else
    {
        // targetPos_r_ctsc->setValue("1");
        // targetPos_r_ctsc->notify();
        return true;
    }
}

static bool runToPositionVibrate(long p, uint16_t speed)
{
    posHandler();
    if (p > lastPosition)
    {

        if (CurrentPosition >= p)
        {
            isVibDone = false;
            isVibPosDone = true;
            stepperStop();
            return true;
        }
        else
        {
            if (isVibDone)
            {
                if (!isVibPosDone)
                {
                    CurrentVibratePosition = CurrentPosition;
                    isVibPosDone = !isVibPosDone;
                }
                isVibDone = !runToPosition(CurrentVibratePosition - (vibrationRate / vibrationRecoilRate), speed);
            }
            else
            {
                if (isVibPosDone)
                {
                    CurrentVibratePosition = CurrentPosition;
                    isVibPosDone = !isVibPosDone;
                }
                isVibDone = runToPosition(CurrentVibratePosition + vibrationRate, speed);
            }
            return false;
        }
    }
    else if (p < lastPosition)
    {
        if (CurrentPosition <= p)
        {
            stepperStop();
            return true;
        }
        else
        {
            if (isVibDone)
            {
                if (!isVibPosDone)
                {
                    CurrentVibratePosition = CurrentPosition;
                    isVibPosDone = !isVibPosDone;
                }
                isVibDone = !runToPosition(CurrentVibratePosition + (vibrationRate / vibrationRecoilRate), speed);
            }
            else
            {
                if (isVibPosDone)
                {
                    CurrentVibratePosition = CurrentPosition;
                    isVibPosDone = !isVibPosDone;
                }
                isVibDone = runToPosition(CurrentVibratePosition - vibrationRate, speed);
            }
            return false;
        }
    }
    else if (isStep)
    {
        stepperStop();
        return false;
    }
    else
    {
        return true;
    }
}

void dirUpdate()
{
    if (direction == cw)
    {
        direction_ctsc->setValue("ccw");
        direction_ctsc->notify(true);
    }
    else
    {
        direction_ctsc->setValue("cw");
        direction_ctsc->notify(true);
    }
}

boolean posChangeFlag = false;
int activeExSampleTime = 0;

void sweep(uint16_t speed, long p1, long p2)
{
    if (!sweepCheckerP1)
    {
        sweepCheckerP1 = runToPosition(p1, speed);
        if (sweepCheckerP1)
        {
            dirUpdate();
            posChangeFlag_ctsc->setValue(std::to_string(1));
            posChangeFlag_ctsc->notify(true);
            sweepCheckerP2 = false;
        }
        else
        {
            sweepCheckerP2 = true;
        }
    }
    else if (!sweepCheckerP2)
    {
        sweepCheckerP2 = runToPosition(p2, speed);

        if (sweepCheckerP2)
        {
            dirUpdate();
            posChangeFlag_ctsc->setValue(std::to_string(2));
            posChangeFlag_ctsc->notify(true);
            sweepCheckerP1 = false;
        }
        else
        {
            sweepCheckerP1 = true;
        }
    }
}

void sweepVibration(uint16_t speed, long p1, long p2)
{
    if (!sweepCheckerP1)
    {
        sweepCheckerP1 = runToPositionVibrate(p1, speed);
        if (sweepCheckerP1)
        {
            dirUpdate();
            posChangeFlag_ctsc->setValue(std::to_string(1));
            posChangeFlag_ctsc->notify(true);
            sweepCheckerP2 = false;
        }
        else
        {
            sweepCheckerP2 = true;
        }
    }
    else if (!sweepCheckerP2)
    {
        sweepCheckerP2 = runToPositionVibrate(p2, speed);

        if (sweepCheckerP2)
        {
            dirUpdate();
            posChangeFlag_ctsc->setValue(std::to_string(2));
            posChangeFlag_ctsc->notify(true);
            sweepCheckerP1 = false;
        }
        else
        {
            sweepCheckerP1 = true;
        }
    }
}

bool sweepWduration(uint16_t speed, int duration, long p1, long p2, bool vibrate)
{
    if (!onSweep)
    {
        sweepStartTime = seconds();
        onSweep = true;
    }
    if (duration > (seconds() - sweepStartTime))
    {
        if (vibrate)
        {
            sweepVibration(stepSpeed, p1, p2);
        }
        else
        {
            sweep(stepSpeed, p1, p2);
        }
        return false;
    }
    else
    {
        onSweep = false;
        return true;
    }
}

void setCurrentPositonToZero()
{
    stepperStop();
    CurrentPosition = 0;
    lastPosition = 0;
}

void setTargetPos(long p)
{
    TargetPosition = p;
}

bool calibrate(){
    stepperStop();
    printBrand('<',"TARE",'>');
    scale.tare(2);
    printBrand('>',"TARE DONE",'<');
    cmd_w_ctsc->setValue("t");
    cmd_w_ctsc->notify(true);
    return true;
}

bool commandSwitcher(uint8_t x)
{
    if (x != 7)
    {
        onSweep = false;
        sweepCheckerP1 = false;
        activeExSampleTime = 0;
        posChangeFlag = false;
        isVibDone = false;
        isVibPosDone = true;
    }
    switch (x)
    {
    case 1:
        stepperRun(stepSpeed, cw);
        return false;
        break;
    case 2:
        stepperRun(stepSpeed, ccw);
        return false;
        break;
    case 3:
        return runToPosition(1000000, stepSpeed);
        break;
    case 4:
        return runToPosition(-1000000, stepSpeed);
        break;
    case 5:
        return runToPosition(0, stepSpeed);
        break;
    case 6:
        setCurrentPositonToZero();
        return true;
        break;
    case 7:
        return sweepWduration(stepSpeed, duration, position1, position2, isVibration);
        break;
    case 8:
        savePos(1);
        return true;
        break;
    case 9:
        savePos(2);
        return true;
        break;
    case 10:
        return runToPosition(TargetPosition, stepSpeed);
        break;
    case 11:
        sweep(stepSpeed, position1, position2);
        return false;
        break;
    case 12:
        return runToPosition(position1, stepSpeed);
        break;
    case 13:
        return runToPosition(position2, stepSpeed);
        break;
    case 14:
        return calibrate();
        break;    
    default:
        stepperStop();
        return false;
        break;
    }
}
String cmdMode = "SERIAL";
bool serialHandler(char c)
{
    cmdMode = "SERIAL";
    if (c != 'g')
    {
        onSweep = false;
        sweepCheckerP1 = false;
        activeExSampleTime = 0;
        posChangeFlag = false;
        isVibDone = false;
        isVibPosDone = true;
    }
    switch (c)
    {
    case 'w':
        stepperRun(stepSpeed, cw);
        return false;
        break;
    case 's':
        stepperRun(stepSpeed, ccw);
        return false;
        break;
    case 'a':
        return runToPosition(1000000, stepSpeed);
        break;
    case 'd':
        return runToPosition(-1000000, stepSpeed);
        break;
    case 'q':
        return runToPosition(0, stepSpeed);
        break;
    case 'e':
        setCurrentPositonToZero();
        return true;
        break;
    case 'g':
        return sweepWduration(stepSpeed, duration, position1, position2, false);
        break;
    case '1':
        savePos(1);
        return true;
        break;
    case '2':
        savePos(2);
        return true;
        break;
    case 'r':
        return (TargetPosition, stepSpeed);
        break;
    case 'x':
        sweep(stepSpeed, position1, position2);
        return false;
        break;
    default:
        stepperStop();
        return false;
        break;
    }
}

bool BleHandler(uint8_t x)
{
    cmdMode = "BLE";
    return commandSwitcher(x);
}

bool switcher = true;
uint8_t tHandleX = 0;
char tHandleC;

void updateFromBle()
{
    if (speedOnWrite)
    {
        stepperSetSpeed(stdToInt(speed_w_ctsc->getValue()));
        speedOnWrite = false;
    }
    if (posOnWrite)
    {
        setTargetPos(stdToInt(position_w_ctsc->getValue()));
        posOnWrite = false;
    }
    if (durOnWrite)
    {
        duration = stdToInt(duration_w_ctsc->getValue());
        durOnWrite = false;
    }
    if (cmdOnWrite)
    {
        tHandleX = stdToInt(cmd_w_ctsc->getValue());
        cmdOnWrite = false;
    }
    if (vibOnWrite)
    {
        int v = stdToInt(vibration_w_ctsc->getValue());
        if (v >= 40 && v <= 400)
        {
            vibrationRate = v;
        }
        vibOnWrite = false;
    }
    if (vibRecOnWrite)
    {
        int v = stdToInt(vibrationRecoil_w_ctsc->getValue());
        if (v >= 1 && v <= 6)
        {
            vibrationRecoilRate = v;
        }
        vibRecOnWrite = false;
    }
    if (isVibOnWrite)
    {
        int v = stdToInt(isVibration_w_ctsc->getValue());
        if (v == 1)
        {
            isVibration = true;
        }
        else
        {
            isVibration = false;
        }
        isVibOnWrite = false;
    }
}
void taskHandler(void *params)
{
    while (1)
    {
        updateFromBle();

        if (Serial.available() > 0)
        {
            tHandleC = Serial.read();
        }
        if (tHandleC == '\r')
        {
            switcher = !switcher;
            tHandleC = ' ';
            tHandleX = 0;
        }
        if (!switcher)
        {
            if (serialHandler(tHandleC))
            {
                tHandleC = ' ';
            }
        }
        else
        {
            if (BleHandler(tHandleX))
            {
                tHandleX = 0;
            }
        }
    }
}

void readLoadCell(void *params)
{
    while (1)
    {

        loadcell_r_ctsc->setValue(std::to_string(getLoadCellVal(loadVector, 1)));
        loadcell_r_ctsc->notify(true);
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

void bleUpdater(void *params)
{
    while (1)
    {
        position_r_ctsc->setValue(std::to_string(CurrentPosition));
        vTaskDelay(100 / portTICK_PERIOD_MS);
        speed_r_ctsc->setValue(std::to_string(stepSpeed));
        vTaskDelay(100 / portTICK_PERIOD_MS);
        if (onSweep)
        {
            // duration_r_ctsc->setValue(std::to_string((seconds() - sweepStartTime)));
        }
        else
        {
            // duration_r_ctsc->setValue(std::to_string(0));
        }
        //vTaskDelay(100 / portTICK_PERIOD_MS);
        // targetPos_r_ctsc->setValue((std::to_string(onSweep)));
        //vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void eventReporter(void *params)
{
    while (1)
    {
        vTaskDelay(200 / portTICK_PERIOD_MS);
        posHandler();
        if (!isUpdating)
        {
            Serial.print("Position : ");
            Serial.print(CurrentPosition);
            Serial.print("|| LoadCell : ");
            Serial.print(loadCellVal);
            Serial.print(" || Speed : ");
            Serial.print(stepSpeed);
            Serial.print(" || vibration : ");
            Serial.print(isVibration);
            Serial.print(" || Mode : ");
            Serial.print(cmdMode);
            Serial.print(" || Adv Name : ");
            Serial.print(ADV_NAME);
            Serial.print(" || Connected : ");
            Serial.print(pServer->getConnectedCount());
            Serial.print(" || cpu_Temp : ");
            (int)temperatureRead() == (int)53.33 ? Serial.print("-") : Serial.print(temperatureRead());
            Serial.println();
        }
    }
}

class DurationCtscCallBacksW : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic *duration_w_ctsc)
    {
        durOnWrite = true;
    }

    void onRead(BLECharacteristic *duration_w_ctsc)
    {
        Serial.println("Okundu Duration");
    }
};

class PosCtscCallBacksW : public BLECharacteristicCallbacks
{

    void onWrite(BLECharacteristic *position_w_ctsc)
    {
        Serial.println("yazıldı position_w_ctsc");
        posOnWrite = true;
    }

    void onRead(BLECharacteristic *position_w_ctsc)
    {
        Serial.println("okundu position_w_ctsc");
    }
};

class SpeedCtscCallBacksW : public BLECharacteristicCallbacks
{

    void onWrite(BLECharacteristic *speed_w_ctsc)
    {
        Serial.println("yazıldı speed_w_ctsc");
        speedOnWrite = true;
    }

    void onRead(BLECharacteristic *speed_w_ctsc)
    {
        Serial.println("okundu speed_w_ctsc");
    }
};

class ExerciseCtscCallBacksW : public BLECharacteristicCallbacks
{

    void onWrite(BLECharacteristic *targetPos_r_ctsc)
    {
        Serial.println("yazıldı targetPos_r_ctsc");
    }

    void onRead(BLECharacteristic *targetPos_r_ctsc)
    {
        Serial.println("okundu targetPos_r_ctsc");
    }
};

class CmdCtscCallBacksW : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic *cmd_w_ctsc)
    {
        cmdOnWrite = true;
    }
    void onRead(BLECharacteristic *cmd_w_ctsc)
    {
    }
};

class VibCtscCallBacksW : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic *vibration_w_ctsc)
    {
        vibOnWrite = true;
    }
    void onRead(BLECharacteristic *vibration_w_ctsc)
    {
    }
};

class VibRecCtscCallBacksW : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic *vibrationRecoil_w_ctsc)
    {
        vibRecOnWrite = true;
    }
    void onRead(BLECharacteristic *vibrationRecoil_w_ctsc)
    {
    }
};

class IsVibCtscCallBacksW : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic *isVibration_w_ctsc)
    {
        isVibOnWrite = true;
    }
    void onRead(BLECharacteristic *isVibration_w_ctsc)
    {
    }
};

class MyServerCallbacks : public BLEServerCallbacks
{
    void onConnect(BLEServer *pServer)
    {
        deviceConnected = true;
        Serial.println("Connected.");
    };

    void onDisconnect(BLEServer *pServer)
    {
        deviceConnected = false;
        Serial.println("Disconnected.");
        ESP.restart();
        Serial.println("Re-Advertising.");
        pServer->startAdvertising();
    }
};

class OTACharacteristicCallbacks : public BLECharacteristicCallbacks
{
    bool isFirstValue = true;

    void onWrite(BLECharacteristic *pCharacteristic)
    {
        std::string value = pCharacteristic->getValue();
        if (isFirstValue)
        {
            cmd_w_ctsc->setValue("x");
            stepperStop();
            totalSize = stdToInt(value);
            isFirstValue = false;
        }
        else if (value.length() > 0)
        {
            if (!isUpdating)
            {
                isUpdating = true;
                if (Update.begin(totalSize))
                { // start with max available size
                    Serial.println("OTA update started");
                }
            }
            if (isUpdating)
            {
                Update.write((uint8_t *)value.c_str(), value.length());
                receivedSize += value.length();
                Serial.printf("Received %d of %d bytes\n", receivedSize, totalSize);
            }

            if (receivedSize >= totalSize)
            {
                if (Update.end(true))
                {
                    Serial.println("OTA update finished!");
                    ESP.restart();
                }
                else
                {
                    Serial.printf("OTA update failed. Error: %s\n", Update.errorString());
                    ESP.restart();
                }
                isUpdating = false;
                receivedSize = 0;
                totalSize = 0;
            }
        }
    }
};



void setup()
{
    setCpuFrequencyMhz(80);
    Serial.begin(115200);
    printBrand('<', "| RoboFizz |", '>');
    Serial.println();
    Serial.print("Esp => ");
    Serial.print(getCpuFrequencyMhz());
    Serial.println(" Mhz ile çalişiyor.");
    Serial.print("Xtal => ");
    Serial.print(getXtalFrequencyMhz());
    Serial.println(" Mhz");
    printBrand('<', "FIRMWARE VERSION", '>');
    printBrand('>', FIRMWARE_VERSION, '<');
    printBrand('>', "| RoboFizz |", '<');

    pinMode(dirPin, OUTPUT);
    pinMode(stepPin, OUTPUT);
    stepperSetSpeed(1000);
    ledcSetup(pwmChannel, stepSpeed, pwmRes);
    ledcWrite(pwmChannel, 127);

    // loadCell.begin();
    // loadCell.start(1000);
    // loadCell.setCalFactor(28);

    scale.begin(DOUT, SCK);
    delay(200);
    scale.tare();
    delay(200);

    scale.set_scale(105);

    BLEDevice::init(ADV_NAME);

    esp_err_t errRc = esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P9);
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P9);
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_SCAN, ESP_PWR_LVL_P9);

    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    pAdvertising = pServer->getAdvertising();

    pAdvertising->addServiceUUID(SERVICE_R_UUID);
    pAdvertising->addServiceUUID(SERVICE_W_UUID);
    pAdvertising->addServiceUUID(SERVICE_KEY_UUID);
    pAdvertising->addServiceUUID(SERVICE_SPECS_UUID);

    write_service = pServer->createService(SERVICE_W_UUID);
    read_service = pServer->createService(SERVICE_R_UUID);
    key_service = pServer->createService(SERVICE_KEY_UUID);
    dev_service = pServer->createService(SERVICE_SPECS_UUID);
    OTA_service = pServer->createService(SERVICE_OTA_UUID);

    ///@brief update
    update_channel_ctsc = OTA_service->createCharacteristic(UPDATE_CHANNEL_UUID, BLE_PROPS_WRITE_NR);
    ///@brief update
    update_channel_ctsc->setCallbacks(new OTACharacteristicCallbacks);
    ///@brief update
    firmware_version_ctsc = OTA_service->createCharacteristic(FIRMWARE_VERSION_UUID, BLE_PROPS_READ_NOTY);

    speed_w_ctsc = write_service->createCharacteristic(SPEED_W_UUID, BLE_PROPS_WRITE_NR);
    speed_w_ctsc->setCallbacks(new SpeedCtscCallBacksW());

    duration_w_ctsc = write_service->createCharacteristic(DURATION_W_UUID, BLE_PROPS_WRITE_NR);
    duration_w_ctsc->setCallbacks(new DurationCtscCallBacksW());

    position_w_ctsc = write_service->createCharacteristic(POSITION_W_UUID, BLE_PROPS_WRITE_NR);
    position_w_ctsc->setCallbacks(new PosCtscCallBacksW());

    cmd_w_ctsc = write_service->createCharacteristic(CMD_W_UUID, BLE_PROPS_ALL);
    cmd_w_ctsc->setCallbacks(new CmdCtscCallBacksW());

    vibration_w_ctsc = write_service->createCharacteristic(VIBRATE_W_UUID, BLE_PROPS_WRITE_NR);
    vibration_w_ctsc->setCallbacks(new VibCtscCallBacksW());

    vibrationRecoil_w_ctsc = write_service->createCharacteristic(VIBRATE_RECOIL_W_UUID, BLE_PROPS_WRITE_NR);
    vibrationRecoil_w_ctsc->setCallbacks(new VibRecCtscCallBacksW());

    isVibration_w_ctsc = write_service->createCharacteristic(IS_VIBRATE_W_UUID, BLE_PROPS_WRITE_NR);
    isVibration_w_ctsc->setCallbacks(new IsVibCtscCallBacksW());

    loadcell_r_ctsc = read_service->createCharacteristic(LOADCELL_R_UUID, BLE_PROPS_READ_NOTY);
    targetPos_r_ctsc = read_service->createCharacteristic(TARGET_POS_R_UUID, BLE_PROPS_READ_NOTY);
    position_r_ctsc = read_service->createCharacteristic(POSITION_R_UUID, BLE_PROPS_READ_NOTY);
    speed_r_ctsc = read_service->createCharacteristic(SPEED_R_UUID, BLE_PROPS_READ_NOTY);
    duration_r_ctsc = read_service->createCharacteristic(DURATION_R_UUID, BLE_PROPS_READ_NOTY);
    posChangeFlag_ctsc = read_service->createCharacteristic(ACTIVE_SMP_DUR_R_UUID, BLE_PROPS_READ_NOTY);
    direction_ctsc = read_service->createCharacteristic(DIR_R_UUID, BLE_PROPS_READ_NOTY);

    activatioKey_w_ctsc = key_service->createCharacteristic(ACTIVATION_KEY_W_UUID, BLE_PROPS_WRITE_NR);
    activatioKey_r_ctsc = key_service->createCharacteristic(ACTIVATION_KEY_R_UUID, BLE_PROPS_READ_NOTY);

    dev_id_ctsc = dev_service->createCharacteristic(DEVICE_ID_UUID, BLE_PROPS_READ_NOTY);
    dev_model_ctsc = dev_service->createCharacteristic(DEVICE_MODEL_UUID, BLE_PROPS_READ_NOTY);

    write_service->start();
    read_service->start();
    key_service->start();
    dev_service->start();
    OTA_service->start();

    dev_id_ctsc->setValue(ID);
    dev_model_ctsc->setValue(MODEL);

    firmware_version_ctsc->setValue(FIRMWARE_VERSION);

    pServer->startAdvertising();

    printBrand('<', "|Key bekleniyor|", '>');

    while (!isActivated)
    {

        if (ACTIVATION_KEY == activatioKey_w_ctsc->getValue())
        {
            isActivated = true;
        }
    }

    printBrand('>', "|Key SUCCESS|", '<');

    activatioKey_r_ctsc->setValue("SUCCESS");

    vTaskDelay(500 / portTICK_PERIOD_MS);

    xTaskCreatePinnedToCore(
        taskHandler,
        "Task Handler",
        4096,
        NULL,
        1,
        NULL,
        app_cpu);

    xTaskCreatePinnedToCore(
        readLoadCell,
        "Read loadCell",
        2048,
        NULL,
        1,
        NULL,
        app_cpu);

    xTaskCreatePinnedToCore(
        eventReporter,
        "Event reporter",
        1024,
        NULL,
        1,
        NULL,
        app_cpu);

    xTaskCreatePinnedToCore(
        bleUpdater,
        "updates Ble",
        2048,
        NULL,
        1,
        NULL,
        app_cpu);
}

void loop()
{
}
