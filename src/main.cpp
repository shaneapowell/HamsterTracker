#include <Arduino.h>
#include <Wire.h>
#include "Task.h"
#include "FunctionTask.h"
#include "lcd/Adafruit_I2C_SH1106.h"

#define WHEEL_DIAMETER_INCHES 21.5
#define INCHES_BETWEEN_TRIGGERS (WHEEL_DIAMETER_INCHES / 2.0)
#define FEET_BETWEEN_TRIGGERS (INCHES_BETWEEN_TRIGGERS / 12.0)
#define MAX_CHARS_WIDTH 21
#define LINE0 0
#define LINE1 13
#define LINE2 23
#define LINE3 33
#define LINE4 43
#define LINE5 53

#define FPS_TO_MPH(x) (x * 0.681818)
#define FEET_TO_MILES(x) (x * 0.000189394)
#define FPS_T0_RPM(x) ((x / (WHEEL_DIAMETER_INCHES / 12)) * 60)


/***
 * Private Prototypes
 ***/
void refreshDisplay();
void sensorTrigger();
void heartbeat(uint32_t deltaTime);

/***
 * Constants
 ***/
const byte ledPin = 13;
const byte sensorInterruptPin = 2;
const char* rpmIndicator = "|/-\\";

/***
 * Local Variables
 ***/
FunctionTask mHeartbeatTask(heartbeat, MsToTaskTime(500));
Adafruit_I2C_SH1106 mDisplay;

unsigned long mPreviousTriggerMillis = 0;
unsigned long mTriggerMillis = 1;
float mFeetPerSecond = 0;
float mFeetTraveled = 0;
unsigned int mRpmIndexIndex = 0;
char mDisplayLineBuffer[MAX_CHARS_WIDTH+1];
bool mIsDirty = true;

/************************************************
 * Obtain singleton task manager
 ************************************************/
TaskManager& getTaskManager()
{
        static TaskManager taskManager;
        return taskManager;
}



/******************************************************************************
 *
 *****************************************************************************/
void setup()
{
	Serial.begin(9600);
	Serial.println(F("Staring..."));
	// getTaskManager().StartTask(&mHeartbeatTask);
	getTaskManager().StartTask(&mHeartbeatTask);

	// getTaskManager().StartTask(&mTriggerTask);

	pinMode(ledPin, OUTPUT);
	pinMode(sensorInterruptPin, INPUT_PULLUP);
	attachInterrupt(digitalPinToInterrupt(sensorInterruptPin), sensorTrigger, RISING);

	Wire.begin();
	Wire.setClock(400000L);
	delay(100);

	mDisplay.init();
	//mDisplay.setFont(&Roboto_Bold6pt7b);
	mDisplay.setTextSize(1);
	mDisplay.setTextColor(WHITE);
	mDisplay.flushDisplay();
	mDisplay.setTextWrap(false);
	delay(5000);

	refreshDisplay();
}



/******************************************************************************
 * Text Size 2 = 11 characters wide, 4 characters high
 * TextSize 1 = 22 characters wide, 8 characters high
 *****************************************************************************/
void loop()
{
	getTaskManager().Loop();

	if (mIsDirty)
	{
		refreshDisplay();
		mIsDirty = false;
	}

	yield();
}

/*****************************************************************************
 *  +  Buddy & Jellybean
 * Feet / Second:   0.00
 * Feet Total:     00.00
 * Miles / Hour:  0.0000
 * Miles Total :  0.0000
 *
 *****************************************************************************/
void refreshDisplay()
{

	mDisplay.fillScreen(BLACK);

	mDisplay.setCursor(0, LINE0);
	mDisplay.print(digitalRead(ledPin) ? F("+") : F(" "));
	mDisplay.println(F(" Buddy & Jellybean"));
	mDisplay.drawLine(0, LINE1 - 3, mDisplay.width(), LINE1 - 3, WHITE);

	/*                ...................... */
	mDisplay.setCursor(0, LINE1);
	mDisplay.print(F("Feet / Second:"));
	dtostrf(mFeetPerSecond, 7, 2, mDisplayLineBuffer);
	mDisplay.print(mDisplayLineBuffer);

	mDisplay.setCursor(0, LINE2);
	mDisplay.print(F("Feet Total:  "));
	dtostrf(mFeetTraveled, 8, 2, mDisplayLineBuffer);
	mDisplay.print(mDisplayLineBuffer);

	mDisplay.setCursor(0, LINE3);
	mDisplay.print(F("Miles / Hour: "));
	dtostrf(FPS_TO_MPH(mFeetPerSecond), 7, 4, mDisplayLineBuffer);
	mDisplay.print(mDisplayLineBuffer);

	mDisplay.setCursor(0, LINE4);
	mDisplay.print(F("Miles Total:  "));
	dtostrf(FEET_TO_MILES(mFeetTraveled), 7, 4, mDisplayLineBuffer);
	mDisplay.print(mDisplayLineBuffer);

	mDisplay.setCursor(0, LINE5);
	mDisplay.print(F("RPM:     "));
	mDisplay.print(rpmIndicator[mRpmIndexIndex]);
	mDisplay.print(F("    "));
	dtostrf(FPS_T0_RPM(mFeetPerSecond), 7, 2, mDisplayLineBuffer);
	mDisplay.print(mDisplayLineBuffer);
	// mDisplay.print(F("                     "));
	// mDisplay.setCursor(mTriggerMarkerIndex * 6, LINE5);
	// mDisplay.print(F("*"));

	mDisplay.flushDisplay();

}


/***************************************************************************
 * Every time the interrupt detects a trigger.
 ***************************************************************************/
void sensorTrigger()
{
	/* Move the trigger display marker */
	mRpmIndexIndex++;
	if (mRpmIndexIndex >= strlen(rpmIndicator))
	{
		mRpmIndexIndex = 0;
	}

	/* Try to handle double-triggers / debounce when we get a bit of noise on the pin */
	unsigned long now = millis();
	if (now - mTriggerMillis < 10)
	{
		return;
	}

	mPreviousTriggerMillis = mTriggerMillis;
	mTriggerMillis = now;

	// Serial.print(mPreviousTriggerMillis);
	// Serial.print(" ");
	// Serial.println(mTriggerMillis);

	float secondsBetweenTriggers = (float)(mTriggerMillis - mPreviousTriggerMillis) / 1000;
	mFeetPerSecond = FEET_BETWEEN_TRIGGERS / secondsBetweenTriggers;
	mFeetTraveled += FEET_BETWEEN_TRIGGERS;

	mIsDirty = true;
}

/**************************************************************************
 *
 *************************************************************************/
void heartbeat(uint32_t deltaTime)
{
	if (millis() - mTriggerMillis > 5000)
	{
		mFeetPerSecond = 0;
	}
	digitalWrite(ledPin, digitalRead(ledPin) == HIGH ? LOW : HIGH);
	refreshDisplay();
}
