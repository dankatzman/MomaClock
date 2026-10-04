#include <Wire.h>
#include <RTClib.h>
#include <EEPROM.h>

// ============================================================
// MomaClock
// ============================================================
//
// Arduino Nano
//
// DS3231 RTC       = I2C 0x68
// AT24C32 EEPROM   = I2C 0x57
//
// Motor / EasyDriver:
// STEP   = D2
// DIR    = D3
// ENABLE = D4  ACTIVE LOW
//
// Buttons:
// FWD    = D5  ACTIVE LOW
// REV    = D6  ACTIVE LOW
//
// EasyDriver:
// FULL STEP
// 400 full steps = 1 revolution
//
// Clock:
// 1 step = 108 seconds
//
// Motor pulse proven in diagnostic test:
// STEP HIGH
// 25 ms
// STEP LOW
//
// ============================================================


// ============================================================
// PINS
// ============================================================

const byte STEP_PIN = 2;
const byte DIR_PIN = 3;
const byte ENABLE_PIN = 4;

const byte FWD_BUTTON = 5;
const byte REV_BUTTON = 6;


// ============================================================
// MOTOR
// ============================================================

const unsigned int STEPS_PER_REVOLUTION = 400;

const unsigned long SECONDS_PER_STEP = 108;

// Proven working pulse.
const unsigned long STEP_PULSE_US = 25000;

// Time between manual steps while button is held.
const unsigned long MANUAL_STEP_INTERVAL_MS = 100;

const byte ENABLE_ON = LOW;
const byte ENABLE_OFF = HIGH;


// ============================================================
// RTC / EEPROM
// ============================================================

RTC_DS3231 rtc;

const int EEPROM_ADDRESS = 0;


// ============================================================
// AUTOMATIC STEP
// ============================================================

unsigned long lastAutomaticStepMillis = 0;

long lastCountdownValue = -1;


// ============================================================
// MANUAL BUTTON TIMING
// ============================================================

unsigned long lastManualStepMillis = 0;

bool manualMovementActive = false;


// ============================================================
// SETUP
// ============================================================

void setup()
{
    Serial.begin(115200);

    delay(1000);

    Serial.println();
    Serial.println("========================================");
    Serial.println("MomaClock STARTUP");
    Serial.println("========================================");
    Serial.println();


    // --------------------------------------------------------
    // MOTOR PINS
    // --------------------------------------------------------

    pinMode(STEP_PIN, OUTPUT);
    pinMode(DIR_PIN, OUTPUT);
    pinMode(ENABLE_PIN, OUTPUT);

    digitalWrite(STEP_PIN, LOW);
    digitalWrite(DIR_PIN, LOW);

    digitalWrite(ENABLE_PIN, ENABLE_OFF);


    // --------------------------------------------------------
    // BUTTONS
    // --------------------------------------------------------

    pinMode(FWD_BUTTON, INPUT_PULLUP);
    pinMode(REV_BUTTON, INPUT_PULLUP);


    // --------------------------------------------------------
    // I2C
    // --------------------------------------------------------

    Wire.begin();


    // --------------------------------------------------------
    // DS3231
    // --------------------------------------------------------

    if (rtc.begin())
    {
        Serial.println("DS3231 RTC detected.");
    }
    else
    {
        Serial.println("ERROR: DS3231 RTC NOT detected.");
    }


    // --------------------------------------------------------
    // AT24C32
    // --------------------------------------------------------

    Wire.beginTransmission(0x57);

    byte eepromResult = Wire.endTransmission();

    if (eepromResult == 0)
    {
        Serial.println("AT24C32 EEPROM detected.");
    }
    else
    {
        Serial.print("ERROR: AT24C32 EEPROM NOT detected. Code = ");
        Serial.println(eepromResult);
    }


    // --------------------------------------------------------
    // EEPROM SAVED REFERENCE
    // --------------------------------------------------------

    unsigned long savedTimestamp = 0;

    EEPROM.get(EEPROM_ADDRESS, savedTimestamp);

    Serial.println();
    Serial.println("EEPROM SAVED MOTOR REFERENCE:");

    Serial.print("Timestamp = ");
    Serial.println(savedTimestamp);

    if (savedTimestamp != 0 &&
        savedTimestamp != 0xFFFFFFFFUL)
    {
        DateTime savedTime(savedTimestamp);

        Serial.print("Date/time = ");
        printDateTime(savedTime);
    }
    else
    {
        Serial.println("No valid EEPROM timestamp.");
    }


    // --------------------------------------------------------
    // CURRENT RTC
    // --------------------------------------------------------

    DateTime now = rtc.now();

    Serial.println();
    Serial.println("RTC CURRENT TIME:");

    Serial.print("Timestamp = ");
    Serial.println(now.unixtime());

    Serial.print("Date/time = ");
    printDateTime(now);


    // --------------------------------------------------------
    // MOTOR
    // --------------------------------------------------------
    //
    // Driver is left DISABLED here. stepMotor() enables it only
    // for the duration of each pulse so it isn't holding current
    // (and heating up) between steps.
    //
    // --------------------------------------------------------

    digitalWrite(ENABLE_PIN, ENABLE_OFF);

    Serial.println();
    Serial.println("Motor driver idles disabled between steps.");

    Serial.println();
    Serial.println("========================================");
    Serial.println("CLOCK START");
    Serial.println("========================================");

    Serial.print("Seconds per automatic step = ");
    Serial.println(SECONDS_PER_STEP);

    Serial.print("Motor pulse = ");
    Serial.print(STEP_PULSE_US / 1000);
    Serial.println(" ms");

    Serial.println();
    Serial.println("FWD button = D5");
    Serial.println("REV button = D6");
    Serial.println();


    // --------------------------------------------------------
    // START AUTOMATIC TIMER
    // --------------------------------------------------------

    lastAutomaticStepMillis = millis();

    lastManualStepMillis = millis();

    lastCountdownValue = SECONDS_PER_STEP;
}


// ============================================================
// LOOP
// ============================================================

void loop()
{
    unsigned long currentMillis = millis();

    bool fwdPressed = (digitalRead(FWD_BUTTON) == LOW);
    bool revPressed = (digitalRead(REV_BUTTON) == LOW);


    // ========================================================
    // MANUAL BUTTON CONTROL
    // ========================================================
    //
    // If FWD is held:
    //     motor repeatedly steps FORWARD.
    //
    // If REV is held:
    //     motor repeatedly steps REVERSE.
    //
    // If neither is held:
    //     manual movement stops.
    //
    // ========================================================

    if (fwdPressed && !revPressed)
    {
        if (!manualMovementActive)
        {
            Serial.println();
            Serial.println("FORWARD BUTTON PRESSED");

            manualMovementActive = true;

            // Reset manual timing so first step occurs immediately.
            lastManualStepMillis =
                currentMillis - MANUAL_STEP_INTERVAL_MS;
        }


        if (currentMillis - lastManualStepMillis >=
            MANUAL_STEP_INTERVAL_MS)
        {
            lastManualStepMillis = currentMillis;

            digitalWrite(DIR_PIN, HIGH);

            delayMicroseconds(100);

            stepMotor();

            Serial.println("Manual FORWARD step");
        }
    }
    else if (revPressed && !fwdPressed)
    {
        if (!manualMovementActive)
        {
            Serial.println();
            Serial.println("REVERSE BUTTON PRESSED");

            manualMovementActive = true;

            // Reset manual timing so first step occurs immediately.
            lastManualStepMillis =
                currentMillis - MANUAL_STEP_INTERVAL_MS;
        }


        if (currentMillis - lastManualStepMillis >=
            MANUAL_STEP_INTERVAL_MS)
        {
            lastManualStepMillis = currentMillis;

            digitalWrite(DIR_PIN, LOW);

            delayMicroseconds(100);

            stepMotor();

            Serial.println("Manual REVERSE step");
        }
    }
    else
    {
        // Neither button pressed,
        // or both buttons pressed.

        if (manualMovementActive)
        {
            Serial.println();
            Serial.println("MANUAL MOVEMENT STOPPED");

            saveCurrentRtcTimeToEEPROM();

            manualMovementActive = false;

            Serial.println("Current RTC time saved to EEPROM.");
            Serial.println();
        }
    }


    // ========================================================
    // AUTOMATIC CLOCK STEP
    // ========================================================
    //
    // Do not perform an automatic step while a manual button
    // is being held.
    //
    // ========================================================

    if (!fwdPressed && !revPressed)
    {
        unsigned long elapsedSeconds =
            (currentMillis - lastAutomaticStepMillis) / 1000UL;


        if (elapsedSeconds >= SECONDS_PER_STEP)
        {
            Serial.println();
            Serial.println("========================================");
            Serial.println("AUTOMATIC STEP NOW");
            Serial.println("========================================");

            Serial.println("Direction = FORWARD");

            digitalWrite(DIR_PIN, HIGH);

            delayMicroseconds(100);

            stepMotor();

            Serial.println("ONE AUTOMATIC STEP COMPLETE.");

            Serial.println("========================================");
            Serial.println();


            // Start the next 108-second period.
            lastAutomaticStepMillis = millis();

            lastCountdownValue = SECONDS_PER_STEP;
        }
        else
        {
            updateAutomaticCountdown(currentMillis);
        }
    }
    else
    {
        // While manually operating, don't print countdown.
        lastCountdownValue = -1;
    }
}


// ============================================================
// MOTOR STEP
// ============================================================
//
// This is the motor pulse that was tested successfully.
//
// ============================================================

void stepMotor()
{
    // Wake the driver only for this step so it isn't holding full
    // coil current (and heating up) during the long idle periods.
    digitalWrite(ENABLE_PIN, ENABLE_ON);

    delayMicroseconds(500);

    digitalWrite(STEP_PIN, HIGH);

    // delayMicroseconds() is only accurate up to ~16383us on AVR;
    // STEP_PULSE_US (25000) overflows it, so use delay() instead.
    delay(STEP_PULSE_US / 1000UL);

    digitalWrite(STEP_PIN, LOW);

    digitalWrite(ENABLE_PIN, ENABLE_OFF);
}


// ============================================================
// AUTOMATIC COUNTDOWN
// ============================================================

void updateAutomaticCountdown(unsigned long currentMillis)
{
    unsigned long elapsed =
        (currentMillis - lastAutomaticStepMillis) / 1000UL;

    if (elapsed >= SECONDS_PER_STEP)
    {
        return;
    }


    long remaining =
        (long)SECONDS_PER_STEP - (long)elapsed;


    if (remaining != lastCountdownValue)
    {
        // Every 10 seconds during the long wait.
        // Every second during the final 10 seconds.

        if ((remaining % 10) == 0 ||
            remaining <= 10)
        {
            Serial.print("Automatic step in ");
            Serial.print(remaining);
            Serial.println(" seconds");
        }

        lastCountdownValue = remaining;
    }
}


// ============================================================
// SAVE RTC TIME TO EEPROM
// ============================================================

void saveCurrentRtcTimeToEEPROM()
{
    DateTime now = rtc.now();

    unsigned long timestamp = now.unixtime();

    EEPROM.put(EEPROM_ADDRESS, timestamp);

    Serial.print("EEPROM timestamp = ");
    Serial.println(timestamp);

    Serial.print("EEPROM date/time = ");
    printDateTime(now);
}


// ============================================================
// PRINT DATE/TIME
// ============================================================

void printDateTime(const DateTime& dt)
{
    Serial.print(dt.year());
    Serial.print("-");

    if (dt.month() < 10)
    {
        Serial.print("0");
    }

    Serial.print(dt.month());
    Serial.print("-");

    if (dt.day() < 10)
    {
        Serial.print("0");
    }

    Serial.print(dt.day());
    Serial.print(" ");

    if (dt.hour() < 10)
    {
        Serial.print("0");
    }

    Serial.print(dt.hour());
    Serial.print(":");

    if (dt.minute() < 10)
    {
        Serial.print("0");
    }

    Serial.print(dt.minute());
    Serial.print(":");

    if (dt.second() < 10)
    {
        Serial.print("0");
    }

    Serial.println(dt.second());
}