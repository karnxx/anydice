#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <MPU6050_light.h>

#define SCREEN_W   128
#define SCREEN_H   64
#define OLED_ADDR  0x3C

Adafruit_SH1106G display(SCREEN_W, SCREEN_H, &Wire, -1);
MPU6050 mpu(Wire);

#define BTN_PIN         3
#define DEBOUNCE_MS     40
#define DOUBLE_TAP_MS   350
#define HOLD_MS         600
#define HOLD_REPEAT_MS  150

#define SHAKE_THRESHOLD  1.8f
#define SHAKE_SETTLE_MS  600

enum Mode { ROLL_MODE, SET_MODE };

int   maxNum      = 20;
int   lastRoll    = 0;
Mode  currentMode = ROLL_MODE;
bool  rolling     = false;

unsigned long rollStartMs    = 0;
unsigned long lastShakeMs    = 0;
unsigned long btnDownMs      = 0;
unsigned long lastTapMs      = 0;
unsigned long lastHoldRepeat = 0;

bool btnPressed    = false;
bool btnWasPressed = false;
bool holdFired     = false;
bool seedDone      = false;

int tapCount = 0;

void setup() {
  Serial.begin(115200);

  pinMode(BTN_PIN, INPUT_PULLUP);
  Wire.begin(4, 5);

  if (!display.begin(OLED_ADDR, true)) {
    Serial.println("OLED init failed");
    while (true) delay(100);
  }

  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  showSplash();

  byte mpuStatus = mpu.begin();
  if (mpuStatus != 0) {
    Serial.print("MPU error: ");
    Serial.println(mpuStatus);
    showError("MPU6050\nNot Found");
    while (true) delay(100);
  }

  mpu.calcOffsets();
  seedRNG();
  showRollScreen();
}

void loop() {
  mpu.update();
  handleButton();
  handleShake();

  if (rolling) {
    animateRoll();
    if (millis() - lastShakeMs > SHAKE_SETTLE_MS) {
      finishRoll();
    }
  }
}

void handleShake() {
  if (currentMode != ROLL_MODE) return;

  float ax = mpu.getAccX();
  float ay = mpu.getAccY();
  float az = mpu.getAccZ();
  float mag = sqrt(ax*ax + ay*ay + az*az);
  float delta = abs(mag - 1.0f);

  if (delta > SHAKE_THRESHOLD) {
    lastShakeMs = millis();
    if (!rolling) {
      rolling = true;
      rollStartMs = millis();
    }
  }
}

void finishRoll() {
  rolling  = false;
  lastRoll = random(1, maxNum + 1);
  showRollScreen();
}

void handleButton() {
  bool raw = (digitalRead(BTN_PIN) == LOW);

  static bool debounced = false;
  static unsigned long lastChange = 0;

  if (raw != debounced) {
    if (millis() - lastChange > DEBOUNCE_MS) {
      debounced  = raw;
      lastChange = millis();
      onBtnChange(debounced);
    }
  }

  if (currentMode == SET_MODE && debounced && holdFired) {
    if (millis() - lastHoldRepeat > HOLD_REPEAT_MS) {
      lastHoldRepeat = millis();
      changeMax(+10);
    }
  }

  if (tapCount == 1 && (millis() - lastTapMs > DOUBLE_TAP_MS)) {
    tapCount = 0;
    onSingleTap();
  }
}

void onBtnChange(bool pressed) {
  if (pressed) {
    btnDownMs      = millis();
    holdFired      = false;
    lastHoldRepeat = millis();
  } else {
    unsigned long held = millis() - btnDownMs;

    if (held >= HOLD_MS && !holdFired) {
      if (currentMode == SET_MODE) {
        holdFired = true;
        changeMax(+10);
      }
    }

    if (!holdFired) {
      unsigned long now = millis();
      if (tapCount == 1 && (now - lastTapMs <= DOUBLE_TAP_MS)) {
        tapCount = 0;
        onDoubleTap();
      } else {
        tapCount  = 1;
        lastTapMs = now;
      }
    }

    if (held >= HOLD_MS) holdFired = true;
  }
}

void onSingleTap() {
  if (currentMode == SET_MODE) {
    changeMax(+1);
  }
}

void onDoubleTap() {
  if (currentMode == ROLL_MODE) {
    currentMode = SET_MODE;
    showSetScreen();
  } else {
    currentMode = ROLL_MODE;
    showRollScreen();
  }
}

void changeMax(int delta) {
  maxNum += delta;
  if (maxNum < 2)    maxNum = 2;
  if (maxNum > 9999) maxNum = 9999;
  showSetScreen();
}

void showSplash() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(20, 10);
  display.print("AnyDice");
  display.setTextSize(1);
  display.setCursor(22, 38);
  display.print("Shake to roll!");
  display.setCursor(10, 52);
  display.print("DblTap: set max");
  display.display();
  delay(2000);
}

void showRollScreen() {
  display.clearDisplay();

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("d");
  display.print(maxNum);
  display.setCursor(80, 0);
  display.print("ROLL");
  display.drawLine(0, 10, 127, 10, SH110X_WHITE);

  if (lastRoll == 0) {
    display.setTextSize(2);
    display.setCursor(14, 24);
    display.print("Shake me!");
  } else {
    String s = String(lastRoll);
    int textSize = (lastRoll < 100) ? 4 : 3;
    display.setTextSize(textSize);
    int x = (SCREEN_W - s.length() * 6 * textSize) / 2;
    display.setCursor(max(0, x), 18);
    display.print(lastRoll);
  }

  display.drawLine(0, 53, 127, 53, SH110X_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 56);
  display.print("DblTap to set max");
  display.display();
}

void showSetScreen() {
  display.clearDisplay();

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("SET MAX NUMBER");
  display.drawLine(0, 10, 127, 10, SH110X_WHITE);

  String s = String(maxNum);
  int textSize = (maxNum < 100) ? 4 : 3;
  display.setTextSize(textSize);
  int x = (SCREEN_W - (int)s.length() * 6 * textSize) / 2;
  display.setCursor(max(0, x), 18);
  display.print(maxNum);

  display.drawLine(0, 53, 127, 53, SH110X_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 56);
  display.print("Tap:+1  Hold:+10  Dbl:OK");
  display.display();
}

void animateRoll() {
  static unsigned long lastFrame = 0;
  if (millis() - lastFrame < 80) return;
  lastFrame = millis();

  int n = random(1, maxNum + 1);

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("d");
  display.print(maxNum);
  display.setCursor(70, 0);
  display.print("ROLLING..");
  display.drawLine(0, 10, 127, 10, SH110X_WHITE);

  String s = String(n);
  int textSize = (maxNum < 100) ? 4 : 3;
  display.setTextSize(textSize);
  int x = (SCREEN_W - (int)s.length() * 6 * textSize) / 2;
  display.setCursor(max(0, x), 18);
  display.print(n);
  display.display();
}

void showError(const char* msg) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("ERROR:");
  display.setCursor(0, 16);
  display.print(msg);
  display.display();
}

void seedRNG() {
  uint32_t seed = 0;
  for (int i = 0; i < 64; i++) {
    mpu.update();
    seed ^= (uint32_t)(mpu.getAccX() * 100000) << (i % 3);
    seed ^= (uint32_t)(mpu.getAccY() * 100000) << ((i + 1) % 5);
    seed ^= (uint32_t)(mpu.getAccZ() * 100000) << ((i + 2) % 7);
    seed ^= esp_random();
    delay(5);
  }
  randomSeed(seed);
}
