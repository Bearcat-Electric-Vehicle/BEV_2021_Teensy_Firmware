// Visual Micro is in vMicro>General>Tutorial Mode
// 
/*
    Name:       BEV_2021_Teensy_Firmware.ino
    Created:	1/30/2021 12:41:27 PM
    Author:     ElonMusk
*/

#include <avr/io.h>
#include <avr/interrupt.h>
#include <Bounce.h>



//##################################### Pin Definitions ###########################################

//Spare Pins
int spareIn0 = 3;
int spareIn1 = 4;
int spareIn2 = 5;
int spareIn3 = 6;
int spareIn4 = 7;
int spareOut0 = 8;
int spareOut1 = 9;
int spareOut2 = 10;
int spareOut3 = 11;
int spareOut4 = 12;

//Comm
int canRX = 23;
int canTx = 22;

int i2cSCL = 19;
int i2cSDA = 18;

//io

int Vehicle_PWR = 2;
int Accel_0 = 24;
int Accel_1 = 25;
int Brake_Pos = 26;
int WheelSense0 = 21;
int WheelSense1 = 22;
int WheelSense2 = 23;
int WheelSense3 = 24;
int ECU_OK = 15;
int PreChg_Fin = 14;
int Ready_TO_GO = 13;
int SHUTDOWN = 41;
int Speaker = 37;
int RESET_BUTTON = 36;
//debounced reset button
Bounce Reset_Button = Bounce(RESET_BUTTON, 10);

//##################################### End Pin Def ###########################################

//##################################### Global Variables ###########################################

//time duration for reset button press
static unsigned long ResetButtonTime = 2000;

//##################################### End Global Variables ###########################################



//######################################## Function Prototypes ########################################

//Initialization state, called when teensy boots
//ecu_ok stays low, precharge_fin goes high, go to reset next
bool Init();

//enter when reset, left init
//ecu_ok low, prechg high
//resets memory
bool Reset();

/*ECU enter this state when:

The ECU was in ERROR state, AND the reset switch outside the cockpit was pressed DOWN. Input Falling-Edge Interrrupt.
This is the intermediate state from ERROR to RESET. In this state:

the ECU should NOT change output signals.
A count down timer is assigned in this state, when the reset button is released half way, ECU return to ERROR state.

If the time RESET_HOLD_DURATION runs out, and the reset botton is still pressed DOWN. The ECU enters RESET state. This is intended to prevent any accidental press.

Note: During the confirm, ECU still does routine check, if routine check fails, ECU immediately returns to ERROR state*/
bool ResetConfirm();

/*ECU enters this state when:

An error occured in the shutdown circuit. Input Rising-Edge Interrupt
The watchdog timer ran off. Timer Interrupt
The reset was not successful.
In this state, ECU_OK stays LOW. PRECHARGE_FINISHED- stays HIGH.

Upon entering this state, ECU is trapped here for 10 seconds or more at a least (to ensure the state does not bounce back), 
after that in order to leave this state, (to RESET_CONFIRM state) the shutdown reset button has to be pressed DOWN. Input Falling-Edge Interrupt*/
void Error();

/*ECU enters this state when:

The scheduled data check was triggered.
In this state:

ECU collects data from BMS, BSPD and motor controller.
ECU checks itself for any memory errors.
the ECU should NOT change output signals.
If data are not good, ECU enters the ERROR state. Transition to this state is scheduled with a timer during any \*_WAIT states. (Proper trigger interval 10ms~100ms).

To avoid false alarm, multiple data fetches can be made consecutively. Error will only be raised when the error data persist.*/
bool RoutineCheck();

/*ECU enters this state when left RESET state.

In this state:

ECU does similar job as ROUTINE_CHECK state.*/
void PreHVCheck();

/*ECU enters this state when PRE_HV_CHECK state has finished.

In this state:

ECU HV_READY outputs HIGH, ECU_OK outputs HIGH.
ECU leaves this state to PRECHARGE state, when SHUTDOWN_TTL_OK- shows a falling edge.*/
void HVReadyWait();

/*ECU enters this state from PRE_HV_CHECK state.

In this state:

ECU assigns a count down timer.
ECU unmask SHUTDOWN_TTL_OK falling edge interrupt, from this point, if the shutdown circuit is open by any mean, ECU goes to ERROR state.
After the precharge timer runs out, meaning precharge is finished, ECU goes to READY_TO_GO_WAIT state.*/
bool PrechargeWait();

/*ECU enters this state from PRECHAGE_WAIT state.

In this state:

ECU plays the ready to go sound once.
ECU READY_TO_GO outputs HIGH.
In this state, the driver is able to drive the car.*/
bool ReadyToGo();

//######################################## End Function Prototypes ########################################


// Define Functions below here or use other .ino or cpp files
//

// The setup() function runs once each time the micro-controller starts
void setup()
{
    //set up pins
    pinMode(spareIn0, INPUT);
    pinMode(spareIn1, INPUT);
    pinMode(spareIn2, INPUT);
    pinMode(spareIn3, INPUT);
    pinMode(spareIn4, INPUT);
    pinMode(spareOut0, OUTPUT);
    pinMode(spareOut1, OUTPUT);
    pinMode(spareOut2, OUTPUT);
    pinMode(spareOut3, OUTPUT);
    pinMode(spareOut4, OUTPUT);

    pinMode(canRX, INPUT);
    pinMode(canTx, OUTPUT);

    pinMode(i2cSCL, OUTPUT);
    pinMode(i2cSDA, OUTPUT);

    pinMode(Vehicle_PWR, OUTPUT);
    pinMode(Accel_0, INPUT_PULLDOWN);
    pinMode(Accel_1, INPUT_PULLDOWN);
    pinMode(Brake_Pos, INPUT_PULLDOWN);
    pinMode(WheelSense0, INPUT_PULLDOWN);
    pinMode(WheelSense1, INPUT_PULLDOWN);
    pinMode(WheelSense2, INPUT_PULLDOWN);
    pinMode(WheelSense3, INPUT_PULLDOWN);
    pinMode(ECU_OK, OUTPUT);
    pinMode(PreChg_Fin, OUTPUT);
    pinMode(Ready_TO_GO, OUTPUT);
    pinMode(SHUTDOWN, INPUT_PULLUP);
    pinMode(Speaker, OUTPUT);
    pinMode(RESET_BUTTON, INPUT);

    //attach interrupts
    attachInterrupt(SHUTDOWN, Error, FALLING);

    

    Init();
}

// Add the main program code into the continuous loop() function
void loop()
{
    //todo: make apps map

}

bool Init()
{
    //clear errors
    digitalWrite(ECU_OK, 0);
    //disable vehicle
    digitalWrite(PreChg_Fin, 1);
    digitalWrite(Ready_TO_GO, 0);

    Reset();
    return false;
}

bool Reset()
{
    //TODO: reset mem blocks

    return false;
}

bool ResetConfirm()
{
    unsigned long time0 = millis();
    bool resetSuccess = false;

    do {
        if (Reset_Button.update()) {
            if (Reset_Button.risingEdge()) {
                resetSuccess = true;
            }
        }
    } while ((millis() - time0) < ResetButtonTime + 1000);

    if (resetSuccess) {
        return true;
    }
    return false;
}

void Error()
{
    digitalWrite(ECU_OK, LOW);
    digitalWrite(PreChg_Fin, HIGH);

    //keep ecu here until reset button pressed for 2 sec -- down, one, two, up
    while (1) {
        if (ResetConfirm()) {
            Reset();
            return;
        }
    }

}

bool RoutineCheck()
{
    //TODO: WHat checks do we need?
    return false;
}

void PreHVCheck()
{
    //TODO: what checks?
    PrechargeWait();
}

void HVReadyWait()
{
    digitalWrite(ECU_OK, HIGH);
    //TODO: what is hv_ready?
    //TODO: what is precharge state
}

bool PrechargeWait()
{
    attachInterrupt(SHUTDOWN, Error, FALLING);
    //todo:make precharge timer?
    ReadyToGo();
    return false;
}

bool ReadyToGo()
{
    //todo: play buzzer
    digitalWrite(Ready_TO_GO, HIGH);
    return false;
}
