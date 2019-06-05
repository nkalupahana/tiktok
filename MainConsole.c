/*! \file MainConsole.c
    \brief The final code for the main module, as of April 17, 2016. THIS IS MY OWN CODE!
*/

/*! \mainpage Home
 *
 * \section intro_sec Welcome!
 *
 * Hello, and welcome to my code documentation site! Please browse around.
 * Hint: Try clicking on "Files." Thank you!
 *
 */

// NOTE: Final normal program

typedef enum {READ_HOUR,           ///< Step for setting the hour (clock)
              READ_MINUTE,         ///< Step for setting the minute (clock)
              READ_ALARM_HOUR,     ///< Step for setting the hour (alarm)
              READ_ALARM_MINUTE,   ///< Step for setting the minute (alarm)
              SET_TIMES,           ///< Initialization routine for time (setting the 2 alarm times, etc.)
              MAIN,                ///< Main routine (main screen)
              SEND_STROBE,         ///< Send signal to strobe via RF, wait for next step
              WAIT_FOR_STROBE,     ///< Wait for the strobe console
              ALARM,               ///< Play the alarm
              SNOOZE,              ///< Run the snooze routine (5 mins)
              FALLBACK,            ///< Initialize the fallback routine for clicking the check button during alarm
              SLEEPING,            ///< Run the white noise generator
              WAITING              ///< Used as an "NULL" for exiting a loop (nothing happened, continue)
} ModeTypes; ///< Different modes the system can be in
ModeTypes StepVar = READ_HOUR;  ///< Initializes the system step to READ_HOUR (to read the hour)

#include <Arduino.h>        ///< General stuff include
#include <Wire.h>           ///< This include is still required because the RTClib depends on it
#include "RTClib.h"         ///< Clock
#include <LiquidCrystal.h>  ///< LCD use

#include <VirtualWire.h>    ///< RF transmission system
/// Initialize the library with the interface pins
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

/// Required by RTCLib
#if defined(ARDUINO_ARCH_SAMD)
  #define Serial SerialUSB
#endif
//

/// White noise generator defines
#define LFSR_INIT  0xfeedfaceUL
/* Choose bits 32, 30, 26, 24 from  http://arduino.stackexchange.com/a/6725/6628
 *  or 32, 22, 2, 1 from
 *  http://www.xilinx.com/support/documentation/application_notes/xapp052.pdf
 *  or bits 32, 16, 3,2  or 0x80010006UL per http://users.ece.cmu.edu/~koopman/lfsr/index.html
 *  and http://users.ece.cmu.edu/~koopman/lfsr/32.dat.gz
 */
#define LFSR_MASK  ((unsigned long)( 1UL<<31 | 1UL <<15 | 1UL <<2 | 1UL <<1  ))
#define speakerPin 9
unsigned long lastClick;
//

RTC_Millis rtc;
int hourvar = 0;          ///< Stores hour for reference and modification during setup
int minvar = 0;           ///< Stores minute for reference and modification during setup
const int RELAY_PIN = 40; ///< Pin for relay
const int UP_PIN = 42;    ///< Pin for "Up" Button
const int DOWN_PIN = 44;  ///< Pin for "Down" Button
const int CHECK_PIN = 46; ///< Pin for "Check" Button
int alreset = 0;          ///< If alarm time should be switched during setup after init
int ahourvar = 0;         ///< Stores hour trigger time (10 mins before)
int aminvar = 0;          ///< Stores minute trigger time (10 mins before)
int aahourvar = 0;        ///< Stores actual alarm hour
int aaminvar = 0;         ///< Stores actual alarm minute
bool alarmwind = false;   ///< Tells main loop to broadcast fallback
int alarmstate = 1;       ///< Alarm On/Off
int messagestate = 1;     ///< Tells main loop what message should be displayed
int relayState = 0;       ///< Declares the relay's state (On/Off)
String previousTime = ""; ///< Used to store the previous time to see if the screen should be updated or redrawn
unsigned long previousMil = 0; ///< Used as starting time for timer 1
unsigned long previousMil2 = 0; ///< Used as starting time for timer 2
unsigned long previousMil3 = 0; ///< Used as starting time for timer 3
unsigned long previousMil4 = 0; ///< Used as starting time for timer 4
unsigned long previousMil5 = 0; ///< Used as starting time for timer 5
bool readNow = false;           ///< Used to stop re-snoozing on snooze routine (because same functions are being called)

void setup() {
  /// Set up pins for use (buttons + relay)
  pinMode(speakerPin, OUTPUT);
  pinMode(UP_PIN, INPUT);
  pinMode(DOWN_PIN, INPUT);
  pinMode(CHECK_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(UP_PIN, HIGH);
  digitalWrite(DOWN_PIN, HIGH);
  digitalWrite(CHECK_PIN, HIGH);
  digitalWrite(RELAY_PIN, HIGH);

  /// Start serial communication at below baud rates
  Serial.begin(57600);

  /// Sets default time to RTC
  rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));

  /// Set up TX
  vw_set_ptt_inverted(true); /// Required for DR3100
  vw_set_tx_pin(22);
  vw_setup(2000); /// Bits per sec

  /// Set up RX
  vw_set_rx_pin(24);
  vw_rx_start(); /// Start the receiver PLL running

  /// Start LCD
  lcd.begin(16, 2);
  lcd.print("Welcome!");

  delay(1000); /// Wait (opening message) before entering main system
}

void loop() {
  /// This switch is used to allow different parts of the program to run at different times
  switch (StepVar) {

    /// See the typedef for ModeTypes (and StepVar) on line 1 to understand what each case is used for

    /// Gets the hour (to display clock and run alarm at specified time)
    case READ_HOUR:
      {
        clearLCD(); /// Clears the LCD
        setHour(); /// Runs the "Set Hour" routine
        StepVar = READ_MINUTE; /// Changes the system step to get the clock's minute
        break;
      }

      /// Gets the minute (to display clock and run alarm at specified time)
    case READ_MINUTE:
      {
        clearLCD(); /// Clears the LCD
        setMinute(); /// Runs the "Set Minute" routine
        StepVar = READ_ALARM_HOUR; /// Changes the system step to get the alarm's hour
        break;
      }

      /// Gets the hour that the alarm needs to run
    case READ_ALARM_HOUR:
      {
        clearLCD(); /// Clears the LCD
        if (alreset == 1) { /// If alarm is not supposed to be reset (redirect from MAIN case)
          StepVar = SET_TIMES; /// Go directly to the Time Setting system step
          Serial.print("Other route taken!");
          break;
        }
        else {
          setAlarmHour(); /// Runs the "Set Alarm Hour" routine
          StepVar = READ_ALARM_MINUTE; /// Changes the system step in order to get the alarm's minute
          break;
        }
      }

      /// Gets the minute that the alarm needs to run
    case READ_ALARM_MINUTE:
      {
        clearLCD(); /// Clears the LCD
        setAlarmMin(); /// Runs the "Set Alarm Minute" routine
        StepVar = SET_TIMES; /// Changes the system step in order to get the set and initialize
        break;
      }

      /// Sets all of the times that were just recieved to the designated variables and to the software RTC
    case SET_TIMES:
      {
        settimes(); /// Run the time initialization routine
        StepVar = MAIN; /// Switch to the main system loop
        break;
      }

      /// Runs the main loop (Displays clock, main communcation, triggers alarm/white noise when ready)
    case MAIN:
      {
        ModeTypes checkStr = mainLoop(); /// Run the main loop
        if (checkStr != WAITING) { /// If the function returns a valid mode to switch to
          StepVar = checkStr; /// Switch to the returned mode
        }
        break;
      }

      /// Runs the white noise generator for 10 minutes
    case SLEEPING:
      {
        bool exit = sleepNow(); /// Run the sleep function, which plays a randomly generated tone on the piezo speaker
        if (exit) { /// If the function returns true
          StepVar = MAIN; /// Set the system step to main
        }
        break;
      }

      /// Activates the strobe module, runs the room light strobe (after 10 minutes)
    case SEND_STROBE:
      {
        readNow = true; /// Used later (makes it so you can only snooze once)
        DateTime now = rtc.now();
        if (now.hour() == ahourvar && now.minute() == aminvar && (now.second() == 0 || now.second() == 1 || now.second() == 2 || now.second() == 3) && alarmstate == 1) { /// If alarm is on and the time to broadcast is correct
          const char * msg = "*1111"; /// Set the message
          vw_send((uint8_t * ) msg, strlen(msg)); /// Send it
          vw_wait_tx(); /// Wait until the whole message is gone
        }
        else if (now.hour() == aahourvar && now.minute() == aaminvar && alarmstate == 1) { /// If the daylight waiting time is over
          relay(); /// Run the room light strobe routine
          if (StepVar == SNOOZE || StepVar == FALLBACK) { /// If the step is set to SNOOZE or FALLBACK during the room light routine
            break; /// Break out of this case
          }
          previousMil = millis(); /// Set the time when the system switched out of this mode (timer)
          StepVar = WAIT_FOR_STROBE; /// Switch to a different mode
        }
        int buttonStateCheck = digitalRead(CHECK_PIN); /// Get the value of the check button
        if (buttonStateCheck == LOW) { /// If the check button is clicked
          StepVar = FALLBACK; /// Exit the alarm system
        }
        uint8_t buf[VW_MAX_MESSAGE_LEN]; /// Get the buffer
        uint8_t buflen = VW_MAX_MESSAGE_LEN; /// Get the maximum message length
        //Serial.println("Ran this section!");
        if (vw_get_message(buf, & buflen)) /// Non-blocking, if the system got a message
        {
          Serial.println("Message available!");
          String message;
          for (int i = 0; i < buflen; i++) {
            Serial.println(message);
            message = message + (char) buf[i]; /// Create the message using the buffer
          }
          if (message == "*3333") { /// If the message is the snooze message
            Serial.println("Message good!");
            previousMil5 = millis(); /// Set the state to "SNOOZE"
            StepVar = SNOOZE;
          }
        }
        break;
      }

      /// Waits for the strobe module to run its strobe routine
    case WAIT_FOR_STROBE:
      {
        waitForStrobe(); /// Check to see if the time is right
        break;
      }

      /// Runs the snooze procedure (Waits 5 minutes, then runs the regular wake-up routine (without daylight wake-up))
    case SNOOZE:
      {
        Serial.println("S"); /// Serial message to show that system is in snooze
        /// Checks to see if room light is on, turns off is on
        if (digitalRead(RELAY_PIN) == LOW) {
          relayState = 0;
          digitalWrite(RELAY_PIN, HIGH);
        }

        /// If it hasn't been 5 minutes, keep transmitting a fallback signal and waiting for the check button

        if (millis() - previousMil5 < 300000) {
          const char * msg = "*5555";
          vw_send((uint8_t * ) msg, strlen(msg));
          vw_wait_tx(); /// Wait until the whole message is gone
          int buttonStateCheck = digitalRead(CHECK_PIN);
          if (buttonStateCheck == LOW) {
            StepVar = FALLBACK;
            break;
          }
        }
        else {
          readNow = false; /// Tells all functions to stop looking for a snooze signal (you can only snooze once)
          relay(); /// Strobes the room light for 1 min
          if (StepVar == FALLBACK) { /// If the relay function was broken via button
            break; /// Go to fallback
          }
          previousMil = millis();
          StepVar = WAIT_FOR_STROBE; /// Sets the state to "WAIT_FOR_STROBE"
        }
        break;
      }

      /// Runs the multi-frequency alarm
    case ALARM:
      {
        bool check = alarm(); /// Runs alarm function (returns whether check was pressed or not
        /// If check mark was pressed, stop, else, keep going
        if (check == 1) {
          StepVar = FALLBACK;
        }
        else if (check == 2) {
          previousMil5 = millis();
          StepVar = SNOOZE;
        }
        break;
      }

      /// Sets the fallback variable to tell the strobe light module to turn off/snooze
    case FALLBACK:
      {
        previousMil2 = millis();
        alarmwind = true; /// Turn on the fallback system in "MAIN", which tells all modules to reset
        StepVar = MAIN;
        break;
      }

      /// The default mode (should NEVER activate)
    default:
      {
        Serial.print((String) StepVar); /// Used to debug the incorrect state the alarm went to
      }
  }

}

//////////////////////
/// Clears the LCD //
////////////////////
void clearLCD() {
  lcd.setCursor(0, 0); /// Sets the cursor at the start of the first line
  lcd.print("                        "); /// Clears the LCD py printing spaces to it
  lcd.setCursor(0, 1); /// Sets the cursor at the start of the second line
  lcd.print("                        "); /// Clears the LCD by printing spaces to it
}

/////////////////////////////////////////////////////////
/// Used to set the hour (called by state READ_HOUR)  //
///////////////////////////////////////////////////////
void setHour() {
  int setvar = 0;
  lcd.setCursor(0, 0); /// Sets the cursor at the top left corner
  lcd.print("Press u/d | Hour"); /// Print message
  while (setvar == 0) { /// If check mark hasn't been pressed...
    lcd.setCursor(0, 1); /// Set cursor to next line
    lcd.print(hourvar); /// Print the current hour onto the LCD
    int buttonStateUp = digitalRead(UP_PIN); /// Get state of up button
    int buttonStateDown = digitalRead(DOWN_PIN); /// Get state of down button
    int buttonStateCheck = digitalRead(CHECK_PIN); /// Get state of check button
    if (buttonStateUp == LOW) { /// If up button is pressed...
      hourvar = hourvar + 1; /// Increase the hour
      if (hourvar == 24) { /// Loop back if neccessary
        hourvar = 0;
      }
      lcd.setCursor(0, 1); /// Rewrite the hour to the LCD
      lcd.print("    ");
      lcd.setCursor(0, 1);
      lcd.print(hourvar);
    }

    if (buttonStateDown == LOW) { /// If down button is pressed...
      hourvar = hourvar - 1; /// Subtract one from hour
      if (hourvar == -1) { /// Loop back if neccessary
        hourvar = 23;
      }
      lcd.setCursor(0, 1); /// Rewrite the hour to the LCD
      lcd.print("    ");
      lcd.setCursor(0, 1);
      lcd.print(hourvar);
    }

    if (buttonStateCheck == LOW) { /// If check mark is pressed...
      setvar = 1; /// Set check mark as pressed
    }

    delay(300); /// Makes it so that the numbers don't increase super fast
  }
}


///////////////////////////////////////////////////////////
/// Used to set the hour (called by state READ_MINUTE)  //
/////////////////////////////////////////////////////////
void setMinute() {
  /// Resets the while
  int setvar = 0;
  /// Waits for user to release the check mark from the previous state
  int buttonStateCheck = digitalRead(CHECK_PIN);
  while (buttonStateCheck == LOW) {
    Serial.println("waiting");
    buttonStateCheck = digitalRead(CHECK_PIN);
  }
  /// Clears the LCD, resets it, and puts a new message on
  clearLCD();
  lcd.setCursor(0, 0);
  lcd.print("Press u/d | Min");
  /// While the minute has not been confirmed...
  while (setvar == 0) {
    /// Print the current minute
    lcd.setCursor(0, 1);
    lcd.setCursor(0, 1);
    lcd.print("    ");
    lcd.setCursor(0, 1);
    lcd.print(minvar);

    /// Gets the button's states, acts based on them
    int buttonStateUp = digitalRead(UP_PIN);
    int buttonStateDown = digitalRead(DOWN_PIN);
    buttonStateCheck = digitalRead(CHECK_PIN);

    /// If up button, put minute up 1
    if (buttonStateUp == LOW) {
      minvar = minvar + 1;
      if (minvar == 60) {
        minvar = 0;
      }
      lcd.setCursor(0, 1);
      lcd.print("    ");
      lcd.setCursor(0, 1);
      lcd.print(minvar);
    }

    /// If down button, put minute down 1
    if (buttonStateDown == LOW) {
      minvar = minvar - 1;
      if (minvar == -1) {
        minvar = 59;
      }
      lcd.setCursor(0, 1);
      lcd.print("    ");
      lcd.setCursor(0, 1);
      lcd.print(minvar);
    }

    /// If check button, exit the loop
    if (buttonStateCheck == LOW) {
      setvar = 1;
    }

    /// Delay to stop the number from going up crazily
    delay(300);
  }

  /// Set the RTC to the new time (date is irrelevant)
  rtc.adjust(DateTime(2000, 1, 1, hourvar, minvar, 0));
}


///////////////////////////////////////////////////////////////////////
/// Used to set the alarm's hour (called by state READ_ALARM_HOUR)  //
/////////////////////////////////////////////////////////////////////
void setAlarmHour() {
  /// Resets the while
  int setvar = 0;
  /// Waits for user to release the check mark from the previous state
  int buttonStateCheck = digitalRead(CHECK_PIN);
  while (buttonStateCheck == LOW) {
    Serial.println("waiting");
    buttonStateCheck = digitalRead(CHECK_PIN);
  }
  /// If the alarm is not supposed to be set, go directly to SET_TIMES
  if (alreset == 1) {
    clearLCD();
    StepVar = SET_TIMES;
  }
  else {
    /// Reset the LCD and set a new message
    clearLCD();
    lcd.setCursor(0, 0);
    lcd.print("u/d | Alarm H");
    /// While the check button has not been pressed...
    while (setvar == 0) {
      /// Clear the LCD
      lcd.setCursor(0, 1);
      lcd.print(ahourvar);
      /// Retrieve all button values (pressed/not pressed)
      int buttonStateUp = digitalRead(UP_PIN);
      int buttonStateDown = digitalRead(DOWN_PIN);
      buttonStateCheck = digitalRead(CHECK_PIN);
      /// If the up button is pressed, increase the alarm's hour counter by 1
      if (buttonStateUp == LOW) {
        ahourvar = ahourvar + 1;
        if (ahourvar == 24) {
          ahourvar = 0;
        }
        /// Re-write the alarm's hour onto the LCD
        lcd.setCursor(0, 1);
        lcd.print("    ");
        lcd.setCursor(0, 1);
        lcd.print(ahourvar);
      }

      /// If the down button is pressed, decrease the alarm's hour counter by 1
      if (buttonStateDown == LOW) {
        ahourvar = ahourvar - 1;
        if (ahourvar == -1) {
          ahourvar = 23;
        }
        /// Re-write the alarm's hour onto the LCD
        lcd.setCursor(0, 1);
        lcd.print("    ");
        lcd.setCursor(0, 1);
        lcd.print(ahourvar);
      }

      /// If the check mark button is pressed, set a variable to exit the while loop
      if (buttonStateCheck == LOW) {
        setvar = 1;
      }

      /// Delay to stop the number from going up crazily
      delay(300);
    }
  }
}


///////////////////////////////////////////////////////////////////////////
/// Used to set the alarm's minute (called by state READ_ALARM_MINUTE)  //
/////////////////////////////////////////////////////////////////////////
void setAlarmMin() {
  /// Wait for the check mark button to be released from the previous step
  int buttonStateCheck = digitalRead(CHECK_PIN);
  while (buttonStateCheck == LOW) {
    Serial.println("waiting");
    buttonStateCheck = digitalRead(CHECK_PIN);
  }
  /// Initializes the while loop variable, and clears the LCD (and prints the default message)
  int setvar = 0;
  clearLCD();
  lcd.setCursor(0, 0);
  lcd.print("u/d | Alarm M");
  while (setvar == 0) {
    /// Clears the bottom section of the LCD, and
    lcd.setCursor(0, 1);
    lcd.print("     ");
    lcd.setCursor(0, 1);
    lcd.print(aminvar);
    /// Get the states of all buttons
    int buttonStateUp = digitalRead(UP_PIN);
    int buttonStateDown = digitalRead(DOWN_PIN);
    buttonStateCheck = digitalRead(CHECK_PIN);
    /// If the up button is pressed, increase the alarm's minute counter by 1
    if (buttonStateUp == LOW) {
      aminvar = aminvar + 1;
      if (aminvar == 60) {
        aminvar = 0;
      }
      /// Re-writes the alarm's minute onto the LCD
      lcd.setCursor(0, 1);
      lcd.print("    ");
      lcd.setCursor(0, 1);
      lcd.print(aminvar);
    }
    /// If the down button is pressed, decrease the alarm's minute counter by 1
    if (buttonStateDown == LOW) {
      aminvar = aminvar - 1;
      if (aminvar == -1) {
        aminvar = 59;
      }
      /// Re-writes the alarm's minute onto the LCD
      lcd.setCursor(0, 1);
      lcd.print("    ");
      lcd.setCursor(0, 1);
      lcd.print(aminvar);
    }

    /// If the check mark button is pressed, set a variable to exit the while loop
    if (buttonStateCheck == LOW) {
      setvar = 1;
    }
    delay(300);
  }
}

///////////////////////////////////////////////////////////////////////////////////////
/// Used to set the read times from the above 4 functions (Used by case SET_TIMES)  //
/////////////////////////////////////////////////////////////////////////////////////
void settimes() {
  /// If the alarm is supposed to be set, set the alarm start time and strobe time
  if (alreset == 0) {
    aaminvar = aminvar;
    aahourvar = ahourvar;
    aminvar = aminvar - 10;
    if (aminvar < 0) {
      aminvar = aminvar + 60;
      ahourvar = ahourvar - 1;
      if (ahourvar == -1) {
        ahourvar = 23;
      }
    }
  }
  /// Clear the LCD, start up, get ready for MAIN
  Serial.print(ahourvar);
  Serial.println();
  Serial.print(aminvar);
  Serial.println();
  clearLCD();
  lcd.setCursor(0, 0);
  lcd.print("Alarm ON");
  alarmwind = false;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// The main loop (where the alarm normally is) (Used by case MAIN) Returns mode for where to switch to  //
//////////////////////////////////////////////////////////////////////////////////////////////////////////
ModeTypes mainLoop() {
  /// Get the up and down button's states
  int buttonStateUp = digitalRead(UP_PIN);
  int buttonStateDown = digitalRead(DOWN_PIN);
  /// If either of the buttons are clicked...
  if (buttonStateUp == LOW || buttonStateDown == LOW) {
    /// Clear the LCD
    clearLCD();
    lcd.setCursor(0, 0);
    lcd.print("C:Time, H:Alarm");
    delay(1000);
    buttonStateUp = digitalRead(UP_PIN);
    buttonStateDown = digitalRead(DOWN_PIN);
    /// If the button was being held, set up the system to reset the alarm
    if (buttonStateUp == LOW || buttonStateDown == LOW) {
      alreset = 0;
      aminvar = 0;
      ahourvar = 0;
      return READ_ALARM_HOUR;
    }
    else { /// If the button was only clicked, set up the system to reset the time
      hourvar = 0;
      minvar = 0;
      alreset = 1;
      return READ_HOUR;
    }
  }

  /// Get the check mark button's state
  int buttonStateCheck = digitalRead(CHECK_PIN);
  /// If the check mark button is pressed, switch the relay's state and LED message
  if (buttonStateCheck == LOW) {
    if (relayState == 0) {
      relayState = 1;
      digitalWrite(RELAY_PIN, LOW);
      delay(1000);
      buttonStateCheck = digitalRead(CHECK_PIN);
      if (buttonStateCheck == LOW) {
        if (alarmstate == 0) {
          alarmstate = 1;
        }
        else {
          alarmstate = 0;
        }
      }
      if (alarmstate == 1) {
        messagestate = 2;
      }
      else {
        messagestate = 6;
      }
    }
    else {
      relayState = 0;
      digitalWrite(RELAY_PIN, HIGH);
      delay(1000);
      buttonStateCheck = digitalRead(CHECK_PIN);
      if (buttonStateCheck == LOW) {
        if (alarmstate == 0) {
          alarmstate = 1;
        }
        else {
          alarmstate = 0;
        }
      }
      if (alarmstate == 1) {
        messagestate = 1;
      }
      else {
        messagestate = 0;
      }
    }
  }
  /// Based on the messagestate variable, print the correct message onto the top half of the LCD
  lcd.setCursor(0, 0);
  if (messagestate == 0) {
    lcd.print("All Systems OFF              ");
  }
  else if (messagestate == 1) {
    lcd.print("Alarm ON                 ");
  }
  else if (messagestate == 2) {
    lcd.print("Alarm + Light ON              ");
  }
  else {
    lcd.print("Light ON                     ");
  }
  /// Update the LCD's time if necessary
  DateTime now = rtc.now();
  String timeset = String(now.hour()) + ":" + String(now.minute()) + ":" + String(now.second()) + "                   ";
  if (timeset == previousTime) {}
  else {
    Serial.print(now.hour(), DEC);
    Serial.print(':');
    Serial.print(now.minute(), DEC);
    Serial.print(':');
    Serial.print(now.second(), DEC);
    Serial.println();
    lcd.setCursor(0, 1);
    if (previousTime.length() == timeset.length()) {}
    else {
      lcd.setCursor(0, 1);
      lcd.print("                    ");
    }
    lcd.setCursor(0, 1);
    lcd.print(timeset);
    /// If it is alarm time, get ready for SEND_STROBE
    if (now.hour() == ahourvar && now.minute() == aminvar && now.second() == 0 && alarmstate == 1) {
      clearLCD();
      lcd.setCursor(0, 0);
      lcd.print("Alarm READY");
      lcd.setCursor(0, 1);
      lcd.print("Press check -OFF");
      return SEND_STROBE;
      //break;
    }
    previousTime = timeset;
  }
  /// If the alarm just ended due to earlier intervention, send a signal to the strobe module to stop running
  if ((millis() - previousMil2 < 6000) && alarmwind == true) {
    const char * msg = "*2222";
    vw_send((uint8_t * ) msg, strlen(msg));
    vw_wait_tx(); /// Wait until the whole message is gone
  }
  else {
    alarmwind = false;
  }

  /// Check for a message in VirtualWire's buffer
  uint8_t buf[VW_MAX_MESSAGE_LEN];
  uint8_t buflen = VW_MAX_MESSAGE_LEN;
  String message;
  /// If a message is found, assemble it using the buffer
  if (vw_get_message(buf, & buflen)) /// Non-blocking
  {
    for (int i = 0; i < buflen; i++) {
      message = message + (char) buf[i];
    }
  }
  /// If the message is from the button module (and is signed properly)
  if (message == "*3333" || message == "*4444") {
    /// If the message was not a repeat (the button module brodcasts for 4 seconds)
    if (millis() - previousMil3 > 4000) {
      /// If the message is *3333, which means that it should switch the room light:
      if (message == "*3333") {
        /// Switch the relay based on its current state
        Serial.print("Switch!");
        if (relayState == 0) {
          relayState = 1;
          digitalWrite(RELAY_PIN, LOW);
          /// Change the screen message
          if (alarmstate == 1) {
            messagestate = 2;
          }
          else {
            messagestate = 3;
          }
          /// Set a previous time so that a repeat doesn't occur
          previousMil3 = millis();
        }
        else {
          /// Switch the relay and state
          relayState = 0;
          digitalWrite(RELAY_PIN, HIGH);
          /// Set a previous time so that a repeat doesn't occur
          previousMil3 = millis();
          if (alarmstate == 1) {
            messagestate = 1;
          }
          else {
            messagestate = 0;
          }
        }
      }
      else { /// If the message is *4444, which means that the system should start the white noise generator:
        /// Set previous times to stop repeats and make sure the white noise generator only runs for a certain amount of time
        Serial.print("Sleep!");
        previousMil3 = millis();
        previousMil4 = millis();
        /// Clear the LCD, write new messages
        clearLCD();
        lcd.setCursor(0, 0);
        lcd.print("SLEEPING!");
        lcd.setCursor(0, 1);
        lcd.print("Check to exit...");
        return SLEEPING;
      }
    }
  }
  return WAITING; /// Equivalent of "Do Nothing"
}

///////////////////////////////////////////////////////////////////////////////////
/// Genetrates random values for white noise generator (Used by state SLEEPING) //
/////////////////////////////////////////////////////////////////////////////////
unsigned int generateNoise() {
  /// See https://en.wikipedia.org/wiki/Linear_feedback_shift_register#Galois_LFSRs
  static unsigned long int lfsr = LFSR_INIT; /* 32 bit init, nonzero */
  /* If the output bit is 1, apply toggle mask.
   * The value has 1 at bits corresponding
   * to taps, 0 elsewhere. */

  if (lfsr & 1) {
    lfsr = (lfsr >> 1) ^ LFSR_MASK;
    return (1);
  }
  else {
    lfsr >>= 1;
    return (0);
  }
}


/////////////////////////////////////////////////////////////////////
/// Used to run the white noise generator (used by case SLEEPING) //
///////////////////////////////////////////////////////////////////
bool sleepNow() {
  /// If the room light is on, turn it off
  if (digitalRead(RELAY_PIN) == LOW) {
    relayState = 0;
    digitalWrite(RELAY_PIN, HIGH);
  }
  /// If it hasn't been 10 minutes, just keep playing...
  if (millis() - previousMil4 < 600000) {
    if ((micros() - lastClick) > 50) { /// Changing this value changes the frequency.
      lastClick = micros();
      digitalWrite(speakerPin, generateNoise());
    }
    /// If the check button is clicked, exit
    int buttonStateCheck = digitalRead(CHECK_PIN);
    if (buttonStateCheck == LOW) {
      return true;
    }
  }
  else {
    return true; /// Exit
  }

  /// Warning: BELOW CODE DOES NOT WORK! TD \\
  uint8_t buf[VW_MAX_MESSAGE_LEN];
  uint8_t buflen = VW_MAX_MESSAGE_LEN;
  //Serial.println("r");
  if (vw_have_message()) {
    Serial.println("Message here!");
  }
  if (vw_get_message(buf, & buflen)) /// Non-blocking
  {
    Serial.println("Message available!");
    String message;
    for (int i = 0; i < buflen; i++) {
      Serial.println(message);
      message = message + (char) buf[i];
    }
    if (message == "*3333") {
      Serial.println("Done!");
      return true;
    }
  }
  /// END OF BROKEN CODE \\
  return false;
}


///////////////////////////////////////////////////////////////////////////////////
/// Used to run the room light strobe (called by state SEND_STROBE and SNOOZE)  //
/////////////////////////////////////////////////////////////////////////////////
void relay() {
  bool switchRelay = false; /// Initialize if variable
  /// Runs the relay for i cycles
  for (int i = 0; i < 133; i++) {
    /// If the relay is on:
    if (switchRelay) {
      /// Switches the relay and relay var
      digitalWrite(RELAY_PIN, LOW);
      switchRelay = false;
      /// Checks to see if the check mark button has been pressed, every 150 ms
      int buttonStateCheck = digitalRead(CHECK_PIN);
      if (buttonStateCheck == LOW) {
        /// stepvar = 13;
        StepVar = FALLBACK;
        break;
      }
      delay(150);
      buttonStateCheck = digitalRead(CHECK_PIN);
      if (buttonStateCheck == LOW) {
        StepVar = FALLBACK;
        /// stepvar = 13;
        break;
      }
      delay(150);
      buttonStateCheck = digitalRead(CHECK_PIN);
      if (buttonStateCheck == LOW) {
        StepVar = FALLBACK;
        /// stepvar = 13;
        break;
      }
      delay(150);
      buttonStateCheck = digitalRead(CHECK_PIN);
      if (buttonStateCheck == LOW) {
        StepVar = FALLBACK;
        /// stepvar = 13;
        break;
      }
    }
    else {
      /// Switch the relay and relay state variables
      digitalWrite(RELAY_PIN, HIGH);
      switchRelay = true;
      /// Check the check mark button's state every 150 ms
      int buttonStateCheck = digitalRead(CHECK_PIN);
      if (buttonStateCheck == LOW) {
        StepVar = FALLBACK;
        /// stepvar = 13;
        break;
      }
      delay(150);
      buttonStateCheck = digitalRead(CHECK_PIN);
      if (buttonStateCheck == LOW) {
        StepVar = FALLBACK;
        /// stepvar = 13;
        break;
      }
      delay(150);
      buttonStateCheck = digitalRead(CHECK_PIN);
      if (buttonStateCheck == LOW) {
        StepVar = FALLBACK;
        /// stepvar = 13;
        break;
      }
      delay(150);
      buttonStateCheck = digitalRead(CHECK_PIN);
      if (buttonStateCheck == LOW) {
        StepVar = FALLBACK;
        /// stepvar = 13;
        break;
      }
    }
    /// If this is not a SNOOZE cycle:
    if (readNow) {
      Serial.print("true");
      uint8_t buf[VW_MAX_MESSAGE_LEN];
      uint8_t buflen = VW_MAX_MESSAGE_LEN;

      if (vw_get_message(buf, & buflen)) /// Non-blocking, if a message has been recieved
      {
        /// Assemble the message using the buffer
        String message;
        for (int i = 0; i < buflen; i++) {
          message = message + (char) buf[i];
        }
        /// If the message is *3333, setup and run the SNOOZE state
        if (message == "*3333") {
          previousMil5 = millis();
          StepVar = SNOOZE;
          break;
        }
      }
    }
  }
}


//////////////////////////////////////////////////////////////////////
/// Used to run the multi-frequency alarm (called by state ALARM)  //
////////////////////////////////////////////////////////////////////
int alarm() {
  /// Play the multi-freqency alarm, checking the state of the check mark button every tone
  tone(8, 200, 100);
  int buttonStateCheck = digitalRead(CHECK_PIN);
  if (buttonStateCheck == LOW) {
    return 1;
  }
  delay(125);
  tone(8, 200, 100);
  buttonStateCheck = digitalRead(CHECK_PIN);
  if (buttonStateCheck == LOW) {
    return 1;
  }
  delay(125);
  tone(8, 1300, 100);
  buttonStateCheck = digitalRead(CHECK_PIN);
  if (buttonStateCheck == LOW) {
    return 1;
  }
  delay(125);
  tone(8, 1300, 100);
  buttonStateCheck = digitalRead(CHECK_PIN);
  if (buttonStateCheck == LOW) {
    return 1;
  }
  delay(300);
  buttonStateCheck = digitalRead(CHECK_PIN);
  if (buttonStateCheck == LOW) {
    return 1;
  }
  delay(358);
  buttonStateCheck = digitalRead(CHECK_PIN);
  if (buttonStateCheck == LOW) {
    return 1;
  }
  /// If this isn't part of SNOOZE:
  if (readNow) {
    uint8_t buf[VW_MAX_MESSAGE_LEN];
    uint8_t buflen = VW_MAX_MESSAGE_LEN;
    /// If a message is available, assemble the message
    if (vw_get_message(buf, & buflen)) /// Non-blocking
    {
      String message;
      for (int i = 0; i < buflen; i++) {
        message = message + (char) buf[i];
      }
      /// If the message is correct, return 2 for SNOOZE
      if (message == "*3333") {
        return 2;
      }
    }
  }
  return 0; /// Do nothing
}


///////////////////////////////////////////////////////////////////////////////
/// Waits for strobe light module to run (called by state WAIT_FOR_STROBE)  //
/////////////////////////////////////////////////////////////////////////////
void waitForStrobe() {
  /// If it hasn't been 30 seconds (waiting for the strobe module), keep waiting
  if (millis() - previousMil > 30000) {
    StepVar = ALARM;
  }

  /// Check the check mark button's state to exit via FALLBACK, which tells the strobe module to stop
  int buttonStateCheck = digitalRead(CHECK_PIN);
  if (buttonStateCheck == LOW) {
    StepVar = FALLBACK;
  }

  /// If this function wasn't called by SNOOZE:
  if (readNow) {
    uint8_t buf[VW_MAX_MESSAGE_LEN];
    uint8_t buflen = VW_MAX_MESSAGE_LEN;
    /// If a message is sitting in the buffer:
    if (vw_get_message(buf, & buflen)) /// Non-blocking
    {
      /// Assemble the message
      String message;
      for (int i = 0; i < buflen; i++) {
        message = message + (char) buf[i];
      }
      /// If the message is correct, setup and change the system state to SNOOZE
      if (message == "*3333") {
        previousMil5 = millis();
        StepVar = SNOOZE;
      }
    }
  }
}
