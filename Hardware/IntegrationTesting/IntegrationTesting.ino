/*
  Dual-Chamber Haptic Pump Controller — Symmetric Release (grams)

  ── Hardware (TB6600, common GND) ───────────────────────────────────────────
    LEFT pump:  DIR_L = 24, STEP_L = 25
    RIGHT pump: DIR_R = 22, STEP_R = 23
    HX711 LEFT  : DOUT=2, SCK=3
    HX711 RIGHT : DOUT=4, SCK=5
    Wiring tip: PUL+→STEP_x, DIR+→DIR_x, PUL-/DIR-→GND, ENA 可省略或常使能

  ── Direction levels (edit here if plumbing reversed) ───────────────────────
    LEFT:  IN = LOW,  OUT = HIGH
    RIGHT: IN = LOW,  OUT = HIGH
    (After changing DIR, wait ≥20 µs before the first STEP pulse)

  ── Serial commands (newline terminated) ────────────────────────────────────
    1) Closed-loop from Unity (grams):
       "a,b,c"
         a = 0 left / 1 right
         b = 1 grab / 0 release
         c = mass in grams: XXX.XX (g)
       Behavior:
         - On grab(b=1):  target = current_weight - c;   lastMass(hand)=c
         - On release(b=0): target = current_weight + lastMass(hand)   // symmetric
         - Stop at target ± TOL (TOL=0.2 g)

    2) Manual jogging (continuous until stop):
       "0,in"   left manual IN
       "0,out"  left manual OUT
       "1,in"   right manual IN
       "1,out"  right manual OUT

    3) Run control:
       "s" or "stop" : emergency stop (clear state, STEP low)
       "p"           : pause current transfer (keeps target)
       "g"           : resume paused transfer

    4) Zeroing:
       "tz" : manual baseline zero  (record current raw as baseline; display→0 g)
       "t"  : HX711 tare (library), set our baseline to 0 (getData() ~ 0)

    5) Busy & dedupe:
       - While transferring: only "s/stop", "p", "g" are honored; others ignored.
       - Duplicate same-state (b) for the same hand is ignored until state flips.

  ── Notes ───────────────────────────────────────────────────────────────────
    - BAUD=57600; print weights every 50 ms.
    - Step pulse: 400 µs half-period (even spacing, low noise).
    - Set TB6600 microstep 1/8~1/16, current ~1A to start, then fine-tune.
*/

#include <Arduino.h>
#include <HX711_ADC.h>

// ── Pins ────────────────────────────────────────────────────────────────────
#define DIR_L   24
#define STEP_L  25
#define DIR_R   22
#define STEP_R  23

// HX711 channels (match Read_2x_load_cell)
HX711_ADC LoadCellL(2, 3);   // LEFT chamber
HX711_ADC LoadCellR(4, 5);   // RIGHT chamber

// ── Calibration ─────────────────────────────────────────────────────────────
const float CAL_L = 450.96f;
const float CAL_R = 1104.97f;

// ── Serial & printing ───────────────────────────────────────────────────────
const long BAUD = 57600;
const unsigned long PRINT_MS = 50;
unsigned long lastPrint = 0;

// ── Step pulse & DIR timing ─────────────────────────────────────────────────
const unsigned long STEP_HALF_US = 400;    // half period
const uint8_t DIR_IN_LEVEL_LEFT   = LOW;
const uint8_t DIR_OUT_LEVEL_LEFT  = HIGH;
const uint8_t DIR_IN_LEVEL_RIGHT  = LOW;
const uint8_t DIR_OUT_LEVEL_RIGHT = HIGH;

unsigned long tLastL = 0, tLastR = 0;
bool stepLevelL = false, stepLevelR = false;

// ── State machine ───────────────────────────────────────────────────────────
enum Mode {
  Idle,
  L_PumpIn, L_PumpOut,
  R_PumpIn, R_PumpOut,
  L_ManualIn, L_ManualOut,
  R_ManualIn, R_ManualOut,
  Paused
};
Mode mode = Idle, pausedMode = Idle;
bool busy = false, paused = false;

// ── Weights & targets (grams) ───────────────────────────────────────────────
float wL = 0.0f, wR = 0.0f;       // displayed weights (g)
float zeroL = 0.0f, zeroR = 0.0f; // display baselines (raw)
float W0_L = 0.0f, W0_R = 0.0f;   // not used for symmetric release but kept if needed
float lastMassL = 0.0f, lastMassR = 0.0f; // last grabbed mass per hand (g)
float target = 0.0f;              // current target for closed-loop
const float TOL = 0.2f;           // stop tolerance (g)

// Dedupe last grab state: -1 unset, 0 release, 1 grab
int lastGrabState[2] = { -1, -1 };

// ── Helpers ─────────────────────────────────────────────────────────────────
inline void setDirLeft(uint8_t level)  { digitalWrite(DIR_L, level);  delayMicroseconds(20); }
inline void setDirRight(uint8_t level) { digitalWrite(DIR_R, level);  delayMicroseconds(20); }

inline void tickLeft() {
  unsigned long now = micros();
  if (now - tLastL >= STEP_HALF_US) {
    stepLevelL = !stepLevelL;
    digitalWrite(STEP_L, stepLevelL);
    tLastL = now;
  }
}
inline void tickRight() {
  unsigned long now = micros();
  if (now - tLastR >= STEP_HALF_US) {
    stepLevelR = !stepLevelR;
    digitalWrite(STEP_R, stepLevelR);
    tLastR = now;
  }
}

void stopAll(const char* reason) {
  mode = Idle; busy = false; paused = false;
  digitalWrite(STEP_L, LOW); digitalWrite(STEP_R, LOW);
  Serial.print("STOP: "); Serial.println(reason);
}

void pauseAll() {
  if (busy && !paused) {
    pausedMode = mode; mode = Paused; paused = true;
    digitalWrite(STEP_L, LOW); digitalWrite(STEP_R, LOW);
    Serial.println("PAUSE");
  }
}
void resumeAll() {
  if (paused) {
    mode = pausedMode; paused = false;
    Serial.println("RESUME");
  }
}

void updateWeights() {
  LoadCellL.update();
  LoadCellR.update();
  // display weights relative to baseline (0 g at startup/tz)
  wL = LoadCellL.getData() - zeroL;
  wR = LoadCellR.getData() - zeroR;
}

void printWeightsThrottled() {
  unsigned long now = millis();
  if (now - lastPrint >= PRINT_MS) {
    Serial.print("L: "); Serial.print(wL, 1); Serial.print(" g  R: ");
    Serial.print(wR, 1); Serial.println(" g");
    lastPrint = now;
  }
}

// ── Command handling ────────────────────────────────────────────────────────
void handleCommand(const String& line) {
  String cmd = line; cmd.trim(); if (cmd.length()==0) return;
  String low = cmd; low.toLowerCase();

  // global: stop / pause / resume
  if (low == "s" || low == "stop") { stopAll("user"); return; }
  if (low == "p") { pauseAll(); return; }
  if (low == "g") { resumeAll(); return; }

  // manual baseline zero (no HX711 tare) -> display becomes 0 g
  if (low == "tz") {
    LoadCellL.update(); LoadCellR.update();
    zeroL = LoadCellL.getData();
    zeroR = LoadCellR.getData();
    Serial.println("Zeroed (tz).");
    return;
  }

  // HX711 tare + set our baseline to 0
  if (low == "t") {
    LoadCellL.tareNoDelay();
    LoadCellR.tareNoDelay();
    zeroL = 0.0f; zeroR = 0.0f;
    Serial.println("Tare requested (t).");
    return;
  }

  // busy guard: while transferring only accept s/stop, p, g
  if (busy && !paused) { Serial.println("BUSY - ignored"); return; }

  // manual jogging
  if (low == "0,in")  { mode = L_ManualIn;  Serial.println("LEFT MANUAL IN");  return; }
  if (low == "0,out") { mode = L_ManualOut; Serial.println("LEFT MANUAL OUT"); return; }
  if (low == "1,in")  { mode = R_ManualIn;  Serial.println("RIGHT MANUAL IN"); return; }
  if (low == "1,out") { mode = R_ManualOut; Serial.println("RIGHT MANUAL OUT");return; }

  // closed-loop: a,b,c (grams)
  int p1 = cmd.indexOf(',');
  int p2 = cmd.indexOf(',', p1+1);
  if (p1 > 0 && p2 > p1) {
    int hand  = cmd.substring(0, p1).toInt();   // 0 left / 1 right
    int state = cmd.substring(p1+1, p2).toInt();// 1 grab / 0 release
    float mass = cmd.substring(p2+1).toFloat(); // grams

    if (hand < 0 || hand > 1) { Serial.println("ERR hand"); return; }
    if (state != 0 && state != 1) { Serial.println("ERR state"); return; }

    // dedupe: same-state repeated -> ignore
    if (lastGrabState[hand] == state) { Serial.println("DUP state - ignored"); return; }

    updateWeights();
    if (hand == 0) { // LEFT
      if (state == 1) { // grab -> pump IN (chamber lighter)
        lastMassL = mass;                     // record symmetric mass
        target = wL - mass;
        mode = L_PumpIn;  busy = true; paused = false;
        Serial.print("L IN START target="); Serial.print(target,1); Serial.println(" g");
      } else {           // release -> pump OUT (use lastMassL, symmetric)
        target = wL + lastMassL;
        mode = L_PumpOut; busy = true; paused = false;
        Serial.print("L OUT START target="); Serial.print(target,1); Serial.println(" g");
      }
    } else {     // RIGHT
      if (state == 1) {
        lastMassR = mass;
        target = wR - mass;
        mode = R_PumpIn;  busy = true; paused = false;
        Serial.print("R IN START target="); Serial.print(target,1); Serial.println(" g");
      } else {
        target = wR + lastMassR;
        mode = R_PumpOut; busy = true; paused = false;
        Serial.print("R OUT START target="); Serial.print(target,1); Serial.println(" g");
      }
    }
    lastGrabState[hand] = state;
    return;
  }

  Serial.println("UNKNOWN CMD");
}

// ── Arduino lifecycle ───────────────────────────────────────────────────────
void setup() {
  Serial.begin(BAUD);

  pinMode(DIR_L, OUTPUT); pinMode(STEP_L, OUTPUT);
  pinMode(DIR_R, OUTPUT); pinMode(STEP_R, OUTPUT);
  digitalWrite(STEP_L, LOW); digitalWrite(STEP_R, LOW);

  // HX711 init
  LoadCellL.begin(); LoadCellR.begin();
  LoadCellL.start(2000, true);   // stabilize + tare (library)
  LoadCellR.start(2000, true);
  LoadCellL.setCalFactor(CAL_L);
  LoadCellR.setCalFactor(CAL_R);

  // Warm up & record startup baseline so display begins at 0 g
  for (int i=0;i<15;i++){ LoadCellL.update(); LoadCellR.update(); delay(5); }
  zeroL = LoadCellL.getData();
  zeroR = LoadCellR.getData();
  Serial.println("Zeroed at startup (L/R).");

  updateWeights();
  Serial.println("System Ready. Commands: a,b,c | 0,in/out | 1,in/out | p | g | s/stop | tz | t");
}

void loop() {
  // serial
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    handleCommand(line);
  }

  // weights & print
  updateWeights();
  printWeightsThrottled();

  // state machine
  switch (mode) {
    // manual jogging
    case L_ManualIn:   setDirLeft(DIR_IN_LEVEL_LEFT);     tickLeft();  break;
    case L_ManualOut:  setDirLeft(DIR_OUT_LEVEL_LEFT);    tickLeft();  break;
    case R_ManualIn:   setDirRight(DIR_IN_LEVEL_RIGHT);   tickRight(); break;
    case R_ManualOut:  setDirRight(DIR_OUT_LEVEL_RIGHT);  tickRight(); break;

    // closed-loop LEFT
    case L_PumpIn:
      setDirLeft(DIR_IN_LEVEL_LEFT);
      if (wL <= target + TOL) stopAll("L IN complete");
      else tickLeft();
      break;

    case L_PumpOut:
      setDirLeft(DIR_OUT_LEVEL_LEFT);
      if (wL >= target - TOL) stopAll("L OUT complete");
      else tickLeft();
      break;

    // closed-loop RIGHT
    case R_PumpIn:
      setDirRight(DIR_IN_LEVEL_RIGHT);
      if (wR <= target + TOL) stopAll("R IN complete");
      else tickRight();
      break;

    case R_PumpOut:
      setDirRight(DIR_OUT_LEVEL_RIGHT);
      if (wR >= target - TOL) stopAll("R OUT complete");
      else tickRight();
      break;

    case Paused:
    case Idle:
    default:
      // idle
      break;
  }
}
