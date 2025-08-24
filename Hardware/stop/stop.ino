/*
  Simple Pump In/Out Control (fixed L/R mapping)
  Commands (Serial, newline-terminated):
    "0,out" -> LEFT pump OUT
    "0,in"  -> LEFT pump IN
    "1,out" -> RIGHT pump OUT
    "1,in"  -> RIGHT pump IN
    "stop"  -> immediately stop any pumping
*/

#include <Arduino.h>

// ---- Pin mapping (SWAPPED to fix your current wiring) ----
// Your observation showed 0,out drove RIGHT and 1,out drove LEFT.
// So we swap logical LEFT/RIGHT to match the physical wiring.
#define DIR_L   24
#define STEP_L  25
#define DIR_R   22
#define STEP_R  23

// ---- Direction levels (tweak here if your driver polarity differs) ----
const uint8_t DIR_OUT_LEVEL_LEFT  = HIGH;  // OUT (pump to exhaust) for LEFT
const uint8_t DIR_IN_LEVEL_LEFT   = LOW;   // IN  (pump into band) for LEFT
const uint8_t DIR_OUT_LEVEL_RIGHT = HIGH;  // OUT for RIGHT
const uint8_t DIR_IN_LEVEL_RIGHT  = LOW;   // IN  for RIGHT

// Pulse timing (4Pump style)
const unsigned long PULSE_DELAY = 400;  // microseconds

enum State { Idle, PumpLeftOut, PumpRightOut, PumpLeftIn, PumpRightIn };
State state = Idle;

void stepPump(int stepPin) {
  digitalWrite(stepPin, HIGH);
  delayMicroseconds(PULSE_DELAY);
  digitalWrite(stepPin, LOW);
  delayMicroseconds(PULSE_DELAY);
}

void setup() {
  Serial.begin(57600);

  pinMode(DIR_L, OUTPUT);
  pinMode(STEP_L, OUTPUT);
  pinMode(DIR_R, OUTPUT);
  pinMode(STEP_R, OUTPUT);

  // idle default (no stepping)
  digitalWrite(DIR_L, LOW);
  digitalWrite(DIR_R, LOW);

  Serial.println("Pump control ready. Commands: 0,in | 0,out | 1,in | 1,out | stop");
}

void loop() {
  // 1) Parse serial commands (line-based)
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toLowerCase(); // case-insensitive

    if (cmd == "stop") {
      state = Idle;
      Serial.println(">> STOP");
    }
    else if (cmd == "0,out") {
      state = PumpLeftOut;
      Serial.println(">> LEFT OUT start");
    }
    else if (cmd == "0,in") {
      state = PumpLeftIn;
      Serial.println(">> LEFT IN start");
    }
    else if (cmd == "1,out") {
      state = PumpRightOut;
      Serial.println(">> RIGHT OUT start");
    }
    else if (cmd == "1,in") {
      state = PumpRightIn;
      Serial.println(">> RIGHT IN start");
    }
    // else ignore unknown commands
  }

  // 2) Run according to state
  switch (state) {
    case PumpLeftOut:
      digitalWrite(DIR_L, DIR_OUT_LEVEL_LEFT);
      stepPump(STEP_L);
      break;

    case PumpLeftIn:
      digitalWrite(DIR_L, DIR_IN_LEVEL_LEFT);
      stepPump(STEP_L);
      break;

    case PumpRightOut:
      digitalWrite(DIR_R, DIR_OUT_LEVEL_RIGHT);
      stepPump(STEP_R);
      break;

    case PumpRightIn:
      digitalWrite(DIR_R, DIR_IN_LEVEL_RIGHT);
      stepPump(STEP_R);
      break;

    case Idle:
    default:
      // do nothing
      break;
  }
}
