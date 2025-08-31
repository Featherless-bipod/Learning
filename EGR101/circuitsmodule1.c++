#include <TM1637Display.h>

// ----------- Pins -----------
const uint8_t PIN_BTN_MODE  = 13;
const uint8_t PIN_BTN_START = 12;
const uint8_t PIN_BTN_SET   = 11;

const uint8_t LED_STOPWATCH = 2;
const uint8_t LED_COUNTDOWN = 3;

const uint8_t TM_CLK = 6;
const uint8_t TM_DIO = 7;

TM1637Display display(TM_CLK, TM_DIO);

// ----------- Timing / Debounce -----------
const unsigned long DEBOUNCE_MS   = 30;
const unsigned long LONG_PRESS_MS = 600;     
unsigned long lastTickMs = 0;                

// ----------- Countdown preset -----------
const uint8_t COUNT_START_MIN = 5;
const uint8_t COUNT_START_SEC = 0;

// ----------- Modes & timers -----------
enum Mode { STOPWATCH, COUNTDOWN };
Mode mode = STOPWATCH;

struct Timer {
  uint16_t totalSeconds;  
  bool     running;
};

Timer sw = { 0, false };
Timer cd = { (uint16_t)(COUNT_START_MIN * 60 + COUNT_START_SEC), false };

bool colonOn = true;                     
const uint8_t TM_COLON = 0b01000000;     

// ----------- Button Bookkeeping -----------
struct Button {
  uint8_t pin;
  bool stable;        
  bool lastRaw;       
  unsigned long lastChangeMs;
  unsigned long pressedMs;
};

Button bMode  { PIN_BTN_MODE,  HIGH, HIGH, 0, 0 };
Button bStart { PIN_BTN_START, HIGH, HIGH, 0, 0 };
Button bSet   { PIN_BTN_SET,   HIGH, HIGH, 0, 0 };

// ----------- Declarations -----------
void render();
void tickTimers();
void flashZero();

typedef void (*PressHandler)();
void serviceButton(Button &b, PressHandler onShort, PressHandler onLong);

void onModePress();
void onStartPress();
void onSetPress();
void onLongSetPress();

void setup() {
  // buttons
  pinMode(PIN_BTN_MODE,  INPUT_PULLUP);
  pinMode(PIN_BTN_START, INPUT_PULLUP);
  pinMode(PIN_BTN_SET,   INPUT_PULLUP);

  // LEDs
  pinMode(LED_STOPWATCH, OUTPUT);
  pinMode(LED_COUNTDOWN, OUTPUT);

  // display
  display.setBrightness(5, true);
  display.clear();

  // start in STOPWATCH mode (LEDs)
  digitalWrite(LED_STOPWATCH, HIGH);
  digitalWrite(LED_COUNTDOWN, LOW);

  render();
}

void loop() {
// button detection
  serviceButton(bMode,  onModePress,    nullptr);
  serviceButton(bStart, onStartPress,   nullptr);
  serviceButton(bSet,   onSetPress,     onLongSetPress);

//non-blocking ticker
  unsigned long now = millis();
  if (now - lastTickMs >= 1000) {
    lastTickMs += 1000;
    tickTimers();
    render(); //repeatedly updates function
  }
}

// ---------- button detection function ----------
void serviceButton(Button &b, PressHandler onShort, PressHandler onLong) {
  bool raw = digitalRead(b.pin);
  unsigned long now = millis();

  if (raw != b.lastRaw) {                
    b.lastRaw = raw;
    b.lastChangeMs = now;
  }

  if ((now - b.lastChangeMs) >= DEBOUNCE_MS && raw != b.stable) {
    b.stable = raw;                      

    if (b.stable == LOW) {               
      b.pressedMs = now;
    } else {                             
      unsigned long held = now - b.pressedMs;
      if (onLong && held >= LONG_PRESS_MS) onLong();
      else if (onShort) onShort();
    }
  }
}

// ---------- Button handlers ----------
void onModePress() {
  sw.running = false;
  cd.running = false;

  mode = (mode == STOPWATCH) ? COUNTDOWN : STOPWATCH;

  if (mode == STOPWATCH) {
    digitalWrite(LED_STOPWATCH, HIGH);
    digitalWrite(LED_COUNTDOWN, LOW);
  } else {
    digitalWrite(LED_STOPWATCH, LOW);
    digitalWrite(LED_COUNTDOWN, HIGH);
  }

  render();
}

void onStartPress() {
  if (mode == STOPWATCH) sw.running = !sw.running;
  else                   cd.running = !cd.running;
  render();
}

void onSetPress() {
  if (mode == COUNTDOWN && !cd.running) {
    if (cd.totalSeconds <= (99 * 60 + 59 - 60)) cd.totalSeconds += 60;
  } else if (mode == STOPWATCH && !sw.running) {
    sw.totalSeconds = 0;
  }
  render();
}

void onLongSetPress() {
  if (mode == COUNTDOWN && !cd.running) {
    cd.totalSeconds = (uint16_t)(COUNT_START_MIN * 60 + COUNT_START_SEC);
  } else if (mode == STOPWATCH && !sw.running) {
    sw.totalSeconds = 0;
  }
  render();
}

// ---------- time control  ----------
void tickTimers() {
  colonOn = !colonOn;

  if (sw.running && sw.totalSeconds < (99 * 60 + 59)) {
    sw.totalSeconds++;
  }

  if (cd.running) {
    if (cd.totalSeconds > 0) cd.totalSeconds--;
    else {
      cd.running = false;
      flashZero();
    }
  }
}

void render() {
  // choose which timer to show
  uint16_t sec = (mode == STOPWATCH) ? sw.totalSeconds : cd.totalSeconds;
  uint8_t mm = sec / 60;
  uint8_t ss = sec % 60;

  // into 4 digits
  int value = (mm % 100) * 100 + ss;  // mm in [0..99], ss in [0..59]

  // colon blink
  display.showNumberDecEx(value, colonOn ? TM_COLON : 0x00, true);
}

void flashZero() {
  //for countdown reach 0
  for (int i = 0; i < 6; i++) {
    display.clear();
    delay(250);
    display.showNumberDecEx(0, TM_COLON, true); // 00:00 with colon
    delay(250);
  }
}
