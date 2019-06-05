/*! \file StrobeConsole.c
    \brief The final code for the strobe module, as of April 17, 2016. THIS IS MY OWN CODE!
*/


// NOTE: This is the official Strobe Console code.

typedef enum {
  READ,           ///< Reads the RF module's buffer, waiting for an activation signal from the Main Console
  DAYLIGHT,       ///< Runs the daylight simulation
  WAIT_FOR_RELAY, ///< Waits for the room light to run its strobe routine
  STROBE,         ///< Strobes the module's strobe light
  SNOOZE,         ///< Waits until SNOOZE time is over, and then switches to STROBE state
  RESET           ///< Resets the system for re-use (sends system back to READ)
} Mode_Types;
Mode_Types StepVariable = READ; ///< Stores offical system state. READ is the first and normal state.

///< Necessary files are included here
#include <Arduino.h>
#include <VirtualWire.h>
#include <Wire.h> ///< this #include still required because the RTClib depends on it
#include "RTClib.h"

/// Software RTC-related define
#if defined(ARDUINO_ARCH_SAMD)
  /// for Zero, output on USB Serial console, remove line below if using programming port to program the Zero!
  #define Serial SerialUSB
#endif

/// Variable declaration
RTC_Millis rtc;                           ///< RTC time is stored here in a specific format
const int DAYLIGHT_DURATION = 11;         ///< Constant for the time the daylight system should run
const int START_PIN = 2;                  ///< The pin the system should start with when turning on LED's
const int RecievePin = 13;                ///< The pin the RF module is connected to
const int STROBE_TIME = 109;              ///< The amount of time the system should run its strobe light for
unsigned long prevMil;                    ///< A universal timer reference
int interval = 0;                         ///< A variable that is used to represent how long the daylight simulation had been running
int changed = 0;                          ///< A modified interval value for switching pins


////////////////////////////////////////////////////////////////////
/// A built-in function, runs on startup, initializes everything //
//////////////////////////////////////////////////////////////////
void setup() {
  /// Starts serial communication
  Serial.begin(57600);
  /// Turns off reciever alert LED (annoying feature build in to VirtualWire)
  pinMode(10, OUTPUT);
  pinMode(10, LOW);
  /// Starts the RTC
  rtc.begin(DateTime(F(__DATE__), F(__TIME__)));
  /// Initialise the IO and ISR
  vw_set_ptt_inverted(true); /// Required for DR3100
  vw_set_rx_pin(RecievePin);
  vw_setup(2000);             /// Bits per sec
  vw_rx_start();              /// Start the receiver PLL running
  /// Turns off reciever alert LED (annoying feature build in to VirtualWire) (I do this again because of library init times, to be safe)
  pinMode(10, OUTPUT);
  pinMode(10, LOW);
}


////////////////////////////////////////////////////////////////////////////////////////
/// A built-in function, runs after setup, loops forever. Uses states with a switch. //
//////////////////////////////////////////////////////////////////////////////////////
void loop() {
  switch (StepVariable) {
    /// Read the RF module's buffer, waiting for an activation signal from the Main Console
    case READ:
      {
        readSys();
        break;
      }

      /// Runs the daylight simulation
    case DAYLIGHT:
      {
        lightsOn();
        break;
      }

      /// Waits until SNOOZE time is over, and then switches to STROBE state
    case SNOOZE:
      {
        /// Turns all LED's off
        for (int i = START_PIN; i < RecievePin; i++) {
          pinMode(i, OUTPUT);
          digitalWrite(i, LOW);
        }

        /// Checks to see if snooze time has ended, and exits if so
        if (millis() - prevMil > 360000) {
          StepVariable = STROBE;
        }

        uint8_t buf[VW_MAX_MESSAGE_LEN];
        uint8_t buflen = VW_MAX_MESSAGE_LEN;

        /// If message is available:
        if (vw_get_message(buf, & buflen)) /// Non-blocking
        {
          /// Constructs message using buffer
          String message;
          for (int i = 0; i < buflen; i++) {
            message = message + (char) buf[i];
          }
          /// If message is a fallback message from the Main Module, reset everything
          if (message == "*2222") {
            StepVariable = RESET;
            interval = 0;
          }
        }
        break;
      }

      /// Resets the system for re-use (sends system back to READ)
    case RESET:
      {
        /// Turns off all LED's
        for (int i = START_PIN; i < RecievePin; i++) {
          pinMode(i, OUTPUT);
          digitalWrite(i, LOW);
        }
        /// Switches back to READ state (waiting for signal from Main Console)
        StepVariable = READ;
        break;
      }

      /// Waits for the room light to run its strobe routine
    case WAIT_FOR_RELAY:
      {
        /// Gets RTC time
        DateTime now = rtc.now();
        /// If the wait time has been reached, send the system to the STROBE state
        if (now.hour() == 0 && now.minute() == DAYLIGHT_DURATION && now.second() == 0) {
          StepVariable = STROBE;
        }
        /// Turns off all LED's
        for (int i = START_PIN; i < RecievePin; i++) {
          pinMode(i, OUTPUT);
          digitalWrite(i, LOW);
        }
        uint8_t buf[VW_MAX_MESSAGE_LEN];
        uint8_t buflen = VW_MAX_MESSAGE_LEN;
        /// If a message is available:
        if (vw_get_message(buf, & buflen)) /// Non-blocking
        {
          /// Constructs the message using the buffer
          String message;
          for (int i = 0; i < buflen; i++) {
            message = message + (char) buf[i];
          }
          /// If the message is *2222, or the fallback message from the Main Module, reset the system
          if (message == "*2222") {
            StepVariable = RESET;
            interval = 0;
          }
        }
        break;
      }

      /// Strobes the module's strobe light
    case STROBE:
      {
        /// Initialzes a "relay" variable to store whether the LED's are on or off
        bool switchRelay = false;
        /// Loops for STROBE_TIME, switching the LED's on and off
        for (int i = 0; i < STROBE_TIME; i++) {
          /// If the LED's are on:
          if (switchRelay) {
            /// Turns all LED"s on
            for (int i = START_PIN; i < RecievePin; i++) {
              pinMode(i, OUTPUT);
              digitalWrite(i, HIGH);
            }
            delay(120);
            /// Changes switch variable
            switchRelay = false;
          }
          else {
            /// Turns LED's off
            for (int i = START_PIN; i < RecievePin; i++) {
              pinMode(i, OUTPUT);
              digitalWrite(i, LOW);
            }
            delay(450);
            /// Changes switch variable
            switchRelay = true;
          }
          uint8_t buf[VW_MAX_MESSAGE_LEN];
          uint8_t buflen = VW_MAX_MESSAGE_LEN;

          /// If a message is avaiable
          if (vw_get_message(buf, & buflen)) /// Non-blocking
          {
            /// Constructs message using buffer
            String message;
            for (int i = 0; i < buflen; i++) {
              message = message + (char) buf[i];
            }
            /// If the message is a fallback signal, reset the module
            if (message == "*2222") {
              StepVariable = RESET;
              interval = 0;
              break;
            }
            /// If the message is a snooze signal, put the system in a SNOOZE state
            if (message == "*5555") {
              prevMil = millis();
              StepVariable = SNOOZE;
              break;
            }
          }
        }
        /// After the light is strobed, reset the module
        StepVariable = RESET;
        break;
      }

    default:
      {
        /// Debugging message for if cases break due to spelling and whatnot
        Serial.print((String) StepVariable);
        break;
      }
  }
}

////////////////////////////////////////////////////////////////////////
/// Part of case READ. Waits for activation signal from Main Module. //
//////////////////////////////////////////////////////////////////////
void readSys() {
  uint8_t buf[VW_MAX_MESSAGE_LEN];
  uint8_t buflen = VW_MAX_MESSAGE_LEN;

  /// If a message has been recieved...
  if (vw_get_message(buf, & buflen)) /// Non-blocking
  {
    /// Construct the message using the buffer
    String message;
    for (int i = 0; i < buflen; i++) {
      message = message + (char) buf[i];
    }
    /// If the message is *1111, begin daylight wake-up simulation
    if (message == "*1111") {
      rtc.adjust(DateTime(2000, 1, 1, 0, 0, 0));
      StepVariable = DAYLIGHT;
      interval = 0;
    }

    /// If the message is *5555, start SNOOZE (this would be used if a person snoozed during the ALARM state of the Main Module.)
    if (message == "*5555") {
      prevMil = millis();
      StepVariable = SNOOZE;
    }
  }
}


//////////////////////////////////////////////////////////////////////////////////////
/// Works with DAYLIGHT. Turns LED's on one at a time. Continues to WAIT_FOR_RELAY //
////////////////////////////////////////////////////////////////////////////////////
void lightsOn() {
  /// Store the RTC's time and date in a unique format
  DateTime now = rtc.now();
  /// If it has been a minute:
  if (now.minute() == interval && now.second() == 0) {
    /// If this is the first run of this function since RESET:
    if (interval == 0) {
      /// Turn on starting LED's (12 is included)
      changed = interval + START_PIN;
      pinMode(changed, OUTPUT);
      digitalWrite(changed, HIGH);
      pinMode(12, OUTPUT);
      digitalWrite(12, HIGH);
      interval = interval + 1;
      Serial.println("Light! Not normal...");
    }
    else {
      /// Turn on LED based on interval
      changed = interval + START_PIN;
      pinMode(changed, OUTPUT);
      digitalWrite(changed, HIGH);
      interval = interval + 1;
      Serial.println("Light!");
    }
  }
  else {
    /// If it isn't time to turn an LED on:
    uint8_t buf[VW_MAX_MESSAGE_LEN];
    uint8_t buflen = VW_MAX_MESSAGE_LEN;
    /// If a message is available...
    if (vw_get_message(buf, & buflen)) /// Non-blocking
    {
      /// Construct the message using the buffer
      String message;
      for (int i = 0; i < buflen; i++) {
        message = message + (char) buf[i];
      }
      /// If the message is *2222, which means that the check mark button was clicked on the main module
      if (message == "*2222") {
        StepVariable = RESET;
        interval = 0;

      }
      /// If the message is *5555, the system must snooze (another signal from the main module)
      if (message == "*5555") {
        prevMil = millis();
        interval = 0;
        StepVariable = SNOOZE;
      }
    }
  }

  /// If all LED's have been turned on, wait for the room light to strobe.
  if (interval == 10) {
    StepVariable = WAIT_FOR_RELAY;
  }
}