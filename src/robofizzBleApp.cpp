#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <HX711_ADC.h>
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

#define SERVICE_R_UUID "4fafc201-1fb5-459e-8fcc-c5c9c3319142"

#define LOADCELL_R_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a5"
#define EXERCISE_R_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a6"
#define POSITION_R_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a7"
#define SPEED_R_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define DURATION_R_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a9"

#define BLE_PROPS_ALL BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
#define BLE_PROPS_READ_NOTY BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ
#define BLE_PROPS_WRITE_NR BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
#define seconds() millis() / 1000

#define pwmChannel 8
#define pwmRes 8

#define DOUT 21
#define SCK 22

#define stepPin 2
#define dirPin 18

#define cw 1
#define ccw -1

#define speedOffset_L 200
#define speedOffset_H 5000
#define speedRes 100

static uint16_t stepSpeed = 0;
static int8_t direction = 0;

static bool isStep = false;
static long stepStartTime = 0;
static long stepEndTime = 0;

static long position1 = 0;
static long position2 = 0;
static long TargetPosition = 0;
static long CurrentPosition = 0;
static long lastPosition = 0;

static int duration = 10; // seconds
static int sweepStartTime = 0;
static int sweepEndTime = 0;
static int loadCellVal = 0;

bool sweepCheckerP1 = false;
bool sweepCheckerP2 = false;
bool onSweep = false;

static bool speedOnWrite = false;
static bool posOnWrite = false;
static bool testOnWrite = false;
static bool durOnWrite = false;

BLEServer *pServer = NULL;

BLEService *write_service = NULL;
BLEService *read_service = NULL;

BLECharacteristic *speed_w_ctsc = NULL;
BLECharacteristic *duration_w_ctsc = NULL;
BLECharacteristic *position_w_ctsc = NULL;
BLECharacteristic *cmd_w_ctsc = NULL;

BLECharacteristic *loadcell_r_ctsc = NULL;
BLECharacteristic *exercise_r_ctsc = NULL;
BLECharacteristic *speed_r_ctsc = NULL;
BLECharacteristic *duration_r_ctsc = NULL;
BLECharacteristic *position_r_ctsc = NULL;

HX711_ADC loadCell(DOUT, SCK);

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

int getLoadCellVal(int vector)
{
    if (loadCell.update())
    {
        loadCellVal = loadCell.getData() * vector;
    }
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
    stepSpeed = speed;
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
        return true;
    }
}

void sweep(uint16_t speed, long p1, long p2)
{
    if (!sweepCheckerP1)
    {
        sweepCheckerP1 = runToPosition(p1, speed);
        sweepCheckerP1 ? sweepCheckerP2 = false : sweepCheckerP2 = true;
    }
    else if (!sweepCheckerP2)
    {
        sweepCheckerP2 = runToPosition(p2, speed);
        sweepCheckerP2 ? sweepCheckerP1 = false : sweepCheckerP1 = true;
    }
}

bool sweepWduration(uint16_t speed, int duration, long p1, long p2)
{
    if (!onSweep)
    {
        sweepStartTime = seconds();
        onSweep = true;
    }

    if (duration > (seconds() - sweepStartTime))
    {
        sweep(stepSpeed, p1, p2);

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

bool commandSwitcher(uint8_t x)
{
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
        return sweepWduration(stepSpeed, duration, position1, position2);
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
        return (TargetPosition, stepSpeed);
        break;
    case 11:
        sweep(stepSpeed, position1, position2);
        return false;
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
        return sweepWduration(stepSpeed, duration, position1, position2);
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
    if (testOnWrite)
    {
        tHandleX = stdToInt(cmd_w_ctsc->getValue());
        testOnWrite = false;
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
        loadcell_r_ctsc->setValue(std::to_string(getLoadCellVal(1)));
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void bleUpdater(void *params)
{
    while (1)
    {
        position_r_ctsc->setValue(std::to_string(CurrentPosition));
        vTaskDelay(100/portTICK_PERIOD_MS);
        speed_r_ctsc->setValue(std::to_string(stepSpeed));
        vTaskDelay(100/portTICK_PERIOD_MS);
        if(onSweep){
            duration_r_ctsc->setValue(std::to_string((seconds() - sweepStartTime)));
        }else{
            duration_r_ctsc->setValue(std::to_string(0));
        }
        vTaskDelay(100/portTICK_PERIOD_MS);
        exercise_r_ctsc->setValue((std::to_string(onSweep)));
        vTaskDelay(100/portTICK_PERIOD_MS);
    }
}

void eventReporter(void *params)
{
    while (1)
    {
        vTaskDelay(200 / portTICK_PERIOD_MS);
        posHandler();
        Serial.print("Position : ");
        Serial.print(CurrentPosition);
        Serial.print("\t|| LoadCell : ");
        Serial.print(getLoadCellVal(1));
        Serial.print("\t|| Speed : ");
        Serial.print(stepSpeed);
        Serial.print("\t|| Mode : ");
        Serial.print(cmdMode);
        Serial.print("\t|| Adv Name : ");
        Serial.print(ADV_NAME);
        Serial.print("\t|| Connected : ");
        Serial.print(pServer->getConnectedCount());
        Serial.println();
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

    void onWrite(BLECharacteristic *exercise_r_ctsc)
    {
        Serial.println("yazıldı exercise_r_ctsc");
    }

    void onRead(BLECharacteristic *exercise_r_ctsc)
    {
        Serial.println("okundu exercise_r_ctsc");
    }
};

class CmdCtscCallBacksW : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic *cmd_w_ctsc)
    {
        testOnWrite = true;
    }
    void onRead(BLECharacteristic *cmd_w_ctsc)
    {
    }
};

class MyServerCallbacks : public BLEServerCallbacks
{
    void onConnect(BLEServer *pServer)
    {
        Serial.println("Connected.");
    };

    void onDisconnect(BLEServer *pServer)
    {
        Serial.println("Disconnected.");
        Serial.println("Re-Advertising.");
        pServer->startAdvertising();
    }
};

void setup()
{
    Serial.begin(115200);
    Serial.println("Esp runing.");

    pinMode(dirPin, OUTPUT);
    pinMode(stepPin, OUTPUT);
    stepperSetSpeed(1000);
    ledcSetup(pwmChannel, stepSpeed, pwmRes);
    ledcWrite(pwmChannel, 127);

    loadCell.begin();
    loadCell.start(1000);
    loadCell.setCalFactor(28);

    BLEDevice::init(ADV_NAME);

    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    write_service = pServer->createService(SERVICE_W_UUID);
    read_service = pServer->createService(SERVICE_R_UUID);

    speed_w_ctsc = write_service->createCharacteristic(SPEED_W_UUID, BLE_PROPS_WRITE_NR);
    speed_w_ctsc->setCallbacks(new SpeedCtscCallBacksW());

    duration_w_ctsc = write_service->createCharacteristic(DURATION_W_UUID, BLE_PROPS_WRITE_NR);
    duration_w_ctsc->setCallbacks(new DurationCtscCallBacksW());

    position_w_ctsc = write_service->createCharacteristic(POSITION_W_UUID, BLE_PROPS_WRITE_NR);
    position_w_ctsc->setCallbacks(new PosCtscCallBacksW());

    cmd_w_ctsc = write_service->createCharacteristic(CMD_W_UUID, BLE_PROPS_WRITE_NR);
    cmd_w_ctsc->setCallbacks(new CmdCtscCallBacksW());

    loadcell_r_ctsc = read_service->createCharacteristic(LOADCELL_R_UUID, BLE_PROPS_READ_NOTY);
    exercise_r_ctsc = read_service->createCharacteristic(EXERCISE_R_UUID, BLE_PROPS_READ_NOTY);
    position_r_ctsc = read_service->createCharacteristic(POSITION_R_UUID, BLE_PROPS_READ_NOTY);
    speed_r_ctsc = read_service->createCharacteristic(SPEED_R_UUID, BLE_PROPS_READ_NOTY);
    duration_r_ctsc = read_service->createCharacteristic(DURATION_R_UUID, BLE_PROPS_READ_NOTY);

    write_service->start();
    read_service->start();

    pServer->startAdvertising();

    vTaskDelay(1000 / portTICK_PERIOD_MS);

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

    vTaskDelete(NULL);
}

void loop()
{
}