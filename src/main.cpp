#include <Arduino.h>
#include <Wire.h>
#include <EEPROM.h>
#include "Task.h"
#include "FunctionTask.h"
#include "lcd/Adafruit_I2C_SH1106.h"

#define WHEEL_DIAMETER_INCHES 21.5
#define INCHES_BETWEEN_TRIGGERS (WHEEL_DIAMETER_INCHES / 2.0)
#define FEET_BETWEEN_TRIGGERS (INCHES_BETWEEN_TRIGGERS / 12.0)
#define MAX_CHARS_WIDTH 21
#define LINE0 0
#define LINE1 17
#define LINE2 30
#define LINE3 43
#define LINE4 56
// #define LINE5 53

#define FPS_TO_MPH(x) (x * 0.681818)
#define FEET_TO_MILES(x) (x * 0.000189394)
#define FPS_T0_RPM(x) ((x / (WHEEL_DIAMETER_INCHES / 12)) * 60)
#define HEARTBEAT_INTERVAL 500

#define DATA_VERSION  "004"
#define DATA_START    16

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
const byte sensorInterruptPin = PD2;
const char* rpmIndicator = "|/-\\";

/***
 * Local Variables
 ***/
FunctionTask mHeartbeatTask(heartbeat, MsToTaskTime(HEARTBEAT_INTERVAL));
Adafruit_I2C_SH1106 mDisplay;

unsigned long mPreviousTriggerMillis = 0;
unsigned long mTriggerMillis = 1;
float mFeetPerSecond = 0;
//float mFeetTraveled = 0;
unsigned int mRpmIndexIndex = 0;
char mDisplayLineBuffer[MAX_CHARS_WIDTH+1];
bool mIsDirty = true;

struct StorageDataStruct
{
	char version[4];
	float feetTraveled;
	unsigned long millis;
} mDataStore =
{
	DATA_VERSION,
	1042002.72,  /* 197.349 miles */
	3433800000   /* 39:17:50:24 */
};

/************************************************
 * Obtain singleton task manager
 ************************************************/
TaskManager& getTaskManager()
{
        static TaskManager taskManager;
        return taskManager;
}



/*************************************************************************
 *
 *************************************************************************/
void writeToEEPROM()
{
	Serial.println(F("Write to EEPROM"));
	Serial.println(mDataStore.version);
	Serial.println(mDataStore.feetTraveled);
	Serial.println(mDataStore.millis);

	for (unsigned int t=0; t<sizeof(mDataStore); t++)
	{
		EEPROM.write(DATA_START + t, *((char*)&mDataStore + t));
	}

}



/*************************************************************************
 *
 ************************************************************************/
void loadFromEEPROM()
{
	Serial.println(F("Load from EEPROM"));

	if (EEPROM.read(DATA_START + 0) == DATA_VERSION[0] &&
		EEPROM.read(DATA_START + 1) == DATA_VERSION[1] &&
		EEPROM.read(DATA_START + 2) == DATA_VERSION[2])
	{
		for (unsigned int t=0; t<sizeof(mDataStore); t++)
		{
			*((char*)&mDataStore + t) = EEPROM.read(DATA_START + t);
		}

		Serial.println(mDataStore.version);
		Serial.println(mDataStore.feetTraveled);
		Serial.println(mDataStore.millis);
	}
	else
	{
		Serial.println(F("Incompatible data store version, default values used"));
	}

}



/******************************************************************************
 *
 *****************************************************************************/
void setup()
{
	Serial.begin(9600);
	Serial.println(F("Staring..."));

	loadFromEEPROM();

	getTaskManager().StartTask(&mHeartbeatTask);

	pinMode(ledPin, OUTPUT);
	pinMode(sensorInterruptPin, INPUT);
	attachInterrupt(digitalPinToInterrupt(sensorInterruptPin), sensorTrigger, FALLING);

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
	mDisplay.drawLine(0, LINE0 + 10, mDisplay.width(), LINE0 + 10, WHITE);

	/*                ...................... */
	mDisplay.setCursor(2, LINE1);
	mDisplay.print(F("Miles / Hour: "));
	dtostrf(FPS_TO_MPH(mFeetPerSecond), 7, 4, mDisplayLineBuffer);
	mDisplay.print(mDisplayLineBuffer);

	mDisplay.setCursor(2, LINE2);
	mDisplay.print(F("Miles Total:  "));
	dtostrf(FEET_TO_MILES(mDataStore.feetTraveled), 7, 4, mDisplayLineBuffer);
	mDisplay.print(mDisplayLineBuffer);

	mDisplay.setCursor(2, LINE3);
	mDisplay.print(F("RPM:     "));
	mDisplay.print(rpmIndicator[mRpmIndexIndex]);
	mDisplay.print(F("    "));
	dtostrf(FPS_T0_RPM(mFeetPerSecond), 7, 2, mDisplayLineBuffer);
	mDisplay.print(mDisplayLineBuffer);

	long d, h, m, s;
	d = mDataStore.millis / (24l * 60l * 60l * 1000l);
	h = (mDataStore.millis / (60l * 60l * 1000l)) % 24l;
	m = (mDataStore.millis / (60l * 1000l)) % 60l;
	s = (mDataStore.millis / 1000l) % 60l;
	sprintf_P(mDisplayLineBuffer, PSTR("%4ld:%02ld:%02ld:%02ld"), d, h, m, s);
	mDisplay.setCursor(2, LINE4);
	mDisplay.print(F("Time:   "));
	mDisplay.print(mDisplayLineBuffer);

	mDisplay.flushDisplay();

	/* Store to the EEProm every 30 seconds */
	if (m == 0 | m == 30)
	{
		if (s == 0)
		{
			writeToEEPROM();
		}
	}
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
	if (now - mTriggerMillis < 100)
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
	mDataStore.feetTraveled += FEET_BETWEEN_TRIGGERS;

	mIsDirty = true;
}

/**************************************************************************
 *
 *************************************************************************/
void heartbeat(uint32_t deltaTime)
{

	/* Increment our main time tracker value */
	mDataStore.millis += deltaTime;

	if (millis() - mTriggerMillis > 5000)
	{
		mFeetPerSecond = 0;
	}

	digitalWrite(ledPin, digitalRead(ledPin) == HIGH ? LOW : HIGH);


	refreshDisplay();
}

