/*! \file Button.c
    \brief The final code for the button module, as of April 17, 2016. THIS IS MY OWN CODE!
*/

// NOTE: Official button code.

/// Import neccessary files
#include <VirtualWire.h>

/// Declare variables
const int checkPin = 3;       ///< Check  button pin
const int ledPin = 2;         ///< LED pin
int buttonStateCheck = 0;     ///< Variable that temporarily stores the check button's state
int stepvar = 0;              ///< The step that the program is on
unsigned long prevMil = 0;    ///< The time before the wait began
const int WAIT_TIME = 3000;   ///< The amount of time the system should broadcast a signal after the check button is clicked

////////////////////////////////////////////////////
/// System setup and initialization of libraries //
//////////////////////////////////////////////////
void setup() {
  Serial.begin(1200);               /// Starts serial communcation
  pinMode(checkPin, INPUT);         /// Allows the check button's pin to recieve inputs
  digitalWrite(checkPin, HIGH);     /// Enables the pullup resister for the check button pin
  pinMode(ledPin, OUTPUT);          /// Sets the LED's pin's mode to output power
  digitalWrite(ledPin, LOW);        /// Sets the LED to OFF
  vw_set_ptt_inverted(true);        /// Required for DR3100 (Turns off PTT)
  vw_set_tx_pin(13);                /// Sets the transmission pin
  vw_setup(2000);                   /// Starts up VirtualWire at 2000 bits per sec
}



//////////////////////
/// Main program loop //
///////////////////////
void loop() {
  if (stepvar == 0) {                             /// Waits for the check button to be pressed
    buttonStateCheck = digitalRead(checkPin);     /// Gets the state of the check button
    if (buttonStateCheck == LOW) {                /// If the check button is pressed...
      delay(750);                                 /// Wait, and...
      buttonStateCheck = digitalRead(checkPin);
      if (buttonStateCheck == LOW) {              /// ...if the button is still pressed...
        prevMil = millis();                       /// Set prevMil to current millis() value
        stepvar = 2;                              /// Go to state 2
      }
      else {                                     /// ... if the button isn't still pressed...
        prevMil = millis();                      /// Set prevMil to current millis() value
        stepvar = 1;                             /// Go to state 1
      }
    }
  }

  if (stepvar == 1) {                            /// If the button was clicked and not held...
    digitalWrite(ledPin, HIGH);                  /// Turn on LED to show that system is transmitting
    if ((millis() - prevMil) > WAIT_TIME) {      /// If the system has transmitted for WAIT_TIME...
      digitalWrite(ledPin, LOW);                 /// Turn off the LED
      stepvar = 0;                               /// Go back to state 0
    }
    else {                                       /// If the system has not transmitted for WAIT_TIME...
      const char *msg = "*3333";                 /// Set the message to a string
      vw_send((uint8_t *)msg, strlen(msg));      /// Transmit the message
      vw_wait_tx();                              /// Wait until the whole message is gone
    }
  }

  if (stepvar == 2) {                            /// If the button was clicked and not held...
    digitalWrite(ledPin, HIGH);                  /// Turn on LED to show that system is transmitting
    if ((millis() - prevMil) > WAIT_TIME) {      /// If the system has transmitted for WAIT_TIME...
      digitalWrite(ledPin, LOW);                 /// Turn off the LED
      stepvar = 0;                               /// Go back to state 0
    }
    else {                                       /// If the system has not transmitted for WAIT_TIME...
      const char *msg = "*4444";                 /// Set the message to a string
      vw_send((uint8_t *)msg, strlen(msg));      /// Transmit the message
      vw_wait_tx();                              /// Wait until the whole message is gone
    }
  }
}
