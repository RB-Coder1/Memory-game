#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoPixel.h>
#include <EEPROM.h>

// =====================================================
// PIXEL RECALL — STAGE 3
// Complete playable memory-sequence game
// Arduino Nano ATmega328P
// =====================================================

// ---------------- OLED ----------------

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  OLED_RESET
);

// ---------------- WS2812B MATRIX ----------------

const byte MATRIX_PIN = 7;
const byte LED_COUNT = 16;
const byte MATRIX_BRIGHTNESS = 45;

Adafruit_NeoPixel matrix(
  LED_COUNT,
  MATRIX_PIN,
  NEO_GRB + NEO_KHZ800
);

// ---------------- PIN ASSIGNMENTS ----------------

const byte START_PIN  = 2;
const byte RIGHT_PIN  = 3;
const byte UP_PIN     = 4;
const byte LEFT_PIN   = 5;
const byte SELECT_PIN = 6;
const byte DOWN_PIN   = 8;
const byte BUZZER_PIN = A2;

// ---------------- BUTTON INDEXES ----------------

const byte BUTTON_START  = 0;
const byte BUTTON_RIGHT  = 1;
const byte BUTTON_UP     = 2;
const byte BUTTON_LEFT   = 3;
const byte BUTTON_SELECT = 4;
const byte BUTTON_DOWN   = 5;

const byte BUTTON_COUNT = 6;

const byte buttonPins[BUTTON_COUNT] = {
  START_PIN,
  RIGHT_PIN,
  UP_PIN,
  LEFT_PIN,
  SELECT_PIN,
  DOWN_PIN
};

// ---------------- BUTTON DEBOUNCING ----------------

bool buttonReading[BUTTON_COUNT];
bool buttonStable[BUTTON_COUNT];
bool buttonPreviousStable[BUTTON_COUNT];
bool buttonPressedEvent[BUTTON_COUNT];

unsigned long buttonChangeTime[BUTTON_COUNT];
const unsigned long DEBOUNCE_DELAY = 30;

// ---------------- GAME SETTINGS ----------------

const byte MAX_LEVEL = 16;
const byte STARTING_LIVES = 3;

byte sequence[MAX_LEVEL];

byte level = 1;
byte score = 0;
byte highScore = 0;

const int EEPROM_MARKER_ADDRESS = 0;
const int EEPROM_SCORE_ADDRESS = 1;

const byte EEPROM_MARKER = 0xA5;

bool newHighScore = false;

byte lives = STARTING_LIVES;
byte inputStep = 0;

byte cursorX = 0;
byte cursorY = 0;

unsigned int sequenceOnTime = 900;

// ---------------- CURSOR BLINK ----------------

bool cursorVisible = true;
unsigned long lastCursorBlink = 0;
const unsigned long CURSOR_BLINK_TIME = 350;

// ---------------- TITLE ANIMATION ----------------

byte titlePixel = 0;
unsigned long lastTitleAnimation = 0;

// ---------------- GAME STATES ----------------

enum GameState {
  TITLE,
  COUNTDOWN,
  SHOW_SEQUENCE,
  PLAYER_INPUT,
  LEVEL_COMPLETE,
  GAME_OVER,
  VICTORY
};

GameState gameState = TITLE;

// =====================================================
// SETUP
// =====================================================

void setup() {
  Serial.begin(9600);

  for (byte i = 0; i < BUTTON_COUNT; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);

    buttonReading[i] = HIGH;
    buttonStable[i] = HIGH;
    buttonPreviousStable[i] = HIGH;
    buttonPressedEvent[i] = false;
    buttonChangeTime[i] = 0;
  }

  pinMode(BUZZER_PIN, OUTPUT);

  matrix.begin();
  matrix.setBrightness(MATRIX_BRIGHTNESS);
  matrix.clear();
  matrix.show();

  Wire.begin();

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    // OLED failure indication
    while (true) {
      fillMatrix(matrix.Color(80, 0, 0));
      delay(300);

      matrix.clear();
      matrix.show();
      delay(300);
    }
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextWrap(false);

  // Use an unconnected analog pin to help randomise games.
 randomSeed(analogRead(A0));

loadHighScore();

playStartupSound();
showTitleScreen();
}

// =====================================================
// MAIN LOOP
// =====================================================

void loop() {
  updateButtons();

  switch (gameState) {
    case TITLE:
      runTitle();
      break;

    case COUNTDOWN:
      runCountdown();
      break;

    case SHOW_SEQUENCE:
      runShowSequence();
      break;

    case PLAYER_INPUT:
      runPlayerInput();
      break;

    case LEVEL_COMPLETE:
      runLevelComplete();
      break;

    case GAME_OVER:
      runGameOver();
      break;

    case VICTORY:
      runVictory();
      break;
  }
}

// =====================================================
// TITLE SCREEN
// =====================================================

void runTitle() {
  animateTitleMatrix();

  if (buttonPressed(BUTTON_START)) {
    startNewGame();
  }
}

void showTitleScreen() {
  display.clearDisplay();

  display.setTextSize(2);
  display.setCursor(9, 2);
  display.println(F("PIXEL"));

  display.setCursor(9, 21);
  display.println(F("RECALL"));

  display.setTextSize(1);
  display.setCursor(46, 40);
  display.println(F("(^_^)"));

  display.setCursor(24, 51);
  display.println(F("Press START"));

  display.setCursor(85, 0);
  display.print(F("HI:"));
  display.println(highScore);

  display.display();
}

void animateTitleMatrix() {
  if (millis() - lastTitleAnimation < 130) {
    return;
  }

  lastTitleAnimation = millis();

  matrix.clear();

  matrix.setPixelColor(
    titlePixel,
    matrix.Color(40, 0, 65)
  );

  // A dim trailing pixel
  byte trailPixel =
    (titlePixel == 0) ? LED_COUNT - 1 : titlePixel - 1;

  matrix.setPixelColor(
    trailPixel,
    matrix.Color(10, 0, 18)
  );

  matrix.show();

  titlePixel++;

  if (titlePixel >= LED_COUNT) {
    titlePixel = 0;
  }
}

// =====================================================
// START GAME
// =====================================================

void startNewGame() {
  level = 1;
  score = 0;
  lives = STARTING_LIVES;
  inputStep = 0;

  newHighScore = false;

  cursorX = 0;
  cursorY = 0;

  sequenceOnTime = 900;

  sequence[0] = random(0, LED_COUNT);

  matrix.clear();
  matrix.show();

  playStartSound();

  gameState = COUNTDOWN;
}

// =====================================================
// COUNTDOWN
// =====================================================

void runCountdown() {
  showCentredMessage(F("GET READY!"), F("Watch carefully"));

  for (int number = 3; number >= 1; number--) {
    display.clearDisplay();

    display.setTextSize(1);
    display.setCursor(34, 5);
    display.println(F("GET READY"));

    display.setTextSize(4);
    display.setCursor(52, 22);
    display.println(number);

    display.display();

    fillMatrix(matrix.Color(90, 55, 0));

    tone(BUZZER_PIN, 550 + ((3 - number) * 150), 150);
    delay(300);

    matrix.clear();
    matrix.show();
    delay(300);
  }

  tone(BUZZER_PIN, 1000, 180);
  delay(250);

  gameState = SHOW_SEQUENCE;
}

// =====================================================
// SHOW MEMORY SEQUENCE
// =====================================================

void runShowSequence() {
  showWatchScreen();

  matrix.clear();
  matrix.show();

  delay(500);

  for (byte i = 0; i < level; i++) {
    byte pixel = sequence[i];

    matrix.clear();

    matrix.setPixelColor(
      pixel,
      matrix.Color(120, 120, 120)
    );

    matrix.show();

    tone(BUZZER_PIN, 650 + (pixel * 18), 70);

    delay(sequenceOnTime);

    matrix.clear();
    matrix.show();

    delay(max(120, sequenceOnTime / 3));
  }

  inputStep = 0;
  cursorX = 0;
  cursorY = 0;
  cursorVisible = true;
  lastCursorBlink = millis();

  waitForAllButtonsReleased();
  clearButtonEvents();

  showPlayerScreen();
  drawCursor();

  gameState = PLAYER_INPUT;
}

void showWatchScreen() {
  display.clearDisplay();

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(F("LEVEL "));
  display.println(level);

  display.drawLine(0, 11, 127, 11, SSD1306_WHITE);

  display.setTextSize(2);
  display.setCursor(28, 20);
  display.println(F("WATCH"));

  display.setTextSize(1);
  display.setCursor(46, 43);
  display.println(F("(O_O)"));

  display.setCursor(28, 55);
  display.print(F("Length: "));
  display.println(level);

  display.display();
}

// =====================================================
// PLAYER INPUT
// =====================================================

void runPlayerInput() {
  bool cursorMoved = false;

  if (buttonPressed(BUTTON_UP) && cursorY > 0) {
    cursorY--;
    cursorMoved = true;
  }

  if (buttonPressed(BUTTON_DOWN) && cursorY < 3) {
    cursorY++;
    cursorMoved = true;
  }

  if (buttonPressed(BUTTON_LEFT) && cursorX > 0) {
    cursorX--;
    cursorMoved = true;
  }

  if (buttonPressed(BUTTON_RIGHT) && cursorX < 3) {
    cursorX++;
    cursorMoved = true;
  }

  if (cursorMoved) {
    cursorVisible = true;
    lastCursorBlink = millis();

    tone(BUZZER_PIN, 650, 25);

    drawCursor();
  }

  if (buttonPressed(BUTTON_SELECT)) {
    checkPlayerSelection();

    // checkPlayerSelection may change the game state.
    if (gameState != PLAYER_INPUT) {
      return;
    }
  }

  // Holding Start for this stage simply returns to the title.
  if (buttonPressed(BUTTON_START)) {
    matrix.clear();
    matrix.show();

    tone(BUZZER_PIN, 400, 100);

    gameState = TITLE;
    showTitleScreen();
    return;
  }

  if (millis() - lastCursorBlink >= CURSOR_BLINK_TIME) {
    lastCursorBlink = millis();
    cursorVisible = !cursorVisible;

    drawCursor();
  }
}

void checkPlayerSelection() {
  byte selectedPixel = xyToPixel(cursorX, cursorY);
  byte expectedPixel = sequence[inputStep];

  if (selectedPixel == expectedPixel) {
    flashPixelGreen(selectedPixel);
    playCorrectStepSound();

    inputStep++;

    if (inputStep >= level) {
      score = level;

      if (score > highScore) {
  highScore = score;
  newHighScore = true;
  saveHighScore();
}

      gameState = LEVEL_COMPLETE;
      return;
    }

    showPlayerScreen();
    drawCursor();
  }
  else {
    lives--;

    flashMatrixRed();
    playWrongSound();

    if (lives == 0) {
      gameState = GAME_OVER;
      return;
    }

    showMistakeScreen();
    delay(1200);

    // Replay the same sequence after a mistake.
    inputStep = 0;
    gameState = SHOW_SEQUENCE;
  }
}

void showPlayerScreen() {
  display.clearDisplay();

  display.setTextSize(1);

  display.setCursor(0, 0);
  display.print(F("LEVEL: "));
  display.print(level);

  display.setCursor(72, 0);
  display.print(F("LIVES: "));
  display.print(lives);

  display.drawLine(0, 11, 127, 11, SSD1306_WHITE);

  display.setTextSize(2);
  display.setCursor(25, 18);
  display.println(F("REPEAT"));

  display.setTextSize(1);
  display.setCursor(46, 38);
  display.println(F("(^_^)"));

  display.setCursor(27, 52);
  display.print(F("Step "));
  display.print(inputStep + 1);
  display.print(F(" of "));
  display.println(level);

  display.display();
}

void showMistakeScreen() {
  display.clearDisplay();

  display.setTextSize(2);
  display.setCursor(34, 5);
  display.println(F("OOPS!"));

  display.setTextSize(1);
  display.setCursor(47, 29);
  display.println(F("(>_<)"));

  display.setCursor(31, 43);
  display.print(F("Lives left: "));
  display.println(lives);

  display.setCursor(23, 55);
  display.println(F("Watch it again"));

  display.display();
}

// =====================================================
// LEVEL COMPLETE
// =====================================================

void runLevelComplete() {
  showLevelCompleteScreen();
  playLevelCompleteSound();
  greenWaveAnimation();

  delay(600);

  if (level >= MAX_LEVEL) {
    gameState = VICTORY;
    return;
  }

  level++;

  sequence[level - 1] = random(0, LED_COUNT);

  // Speed up gradually, but never below 280 ms.
  if (sequenceOnTime > 320) {
    sequenceOnTime -= 40;
  }

  gameState = SHOW_SEQUENCE;
}

void showLevelCompleteScreen() {
  display.clearDisplay();

  display.setTextSize(1);
  display.setCursor(19, 4);
  display.println(F("LEVEL COMPLETE"));

  display.setTextSize(2);
  display.setCursor(42, 19);
  display.println(F("GOOD!"));

  display.setTextSize(1);
  display.setCursor(45, 42);
  display.println(F("(^o^)"));

  display.setCursor(27, 54);
  display.print(F("Score: "));
  display.println(score);

  display.display();
}

// =====================================================
// GAME OVER
// =====================================================

void runGameOver() {
  matrix.clear();
  matrix.show();

  showGameOverScreen();
  playGameOverSound();

  for (byte flash = 0; flash < 3; flash++) {
    fillMatrix(matrix.Color(80, 0, 0));
    delay(220);

    matrix.clear();
    matrix.show();
    delay(170);
  }

  waitForAllButtonsReleased();
  clearButtonEvents();

  while (gameState == GAME_OVER) {
    updateButtons();

    if (buttonPressed(BUTTON_START)) {
      gameState = TITLE;
      showTitleScreen();
      return;
    }

    delay(5);
  }
}

void showGameOverScreen() {
  display.clearDisplay();

  display.setTextSize(2);
  display.setCursor(10, 3);
  display.println(F("GAME OVER"));

  display.setTextSize(1);
  display.setCursor(31, 29);
  display.print(F("Score: "));
  display.println(score);

  display.setCursor(25, 41);
  display.print(F("High score: "));
  display.println(highScore);

  display.setCursor(25, 55);
  display.println(F("Press START"));

  display.display();
}

// =====================================================
// VICTORY
// =====================================================

void runVictory() {
  if (MAX_LEVEL > highScore) {
    highScore = MAX_LEVEL;
  }

  showVictoryScreen();
  playVictorySound();
  rainbowAnimation();

  waitForAllButtonsReleased();
  clearButtonEvents();

  while (gameState == VICTORY) {
    updateButtons();

    if (buttonPressed(BUTTON_START)) {
      gameState = TITLE;
      showTitleScreen();
      return;
    }

    delay(5);
  }
}

void showVictoryScreen() {
  display.clearDisplay();

  display.setTextSize(2);
  display.setCursor(18, 4);
  display.println(F("YOU WIN!"));

  display.setTextSize(1);
  display.setCursor(39, 29);
  display.println(F("\\(^o^)/"));

  display.setCursor(18, 43);
  display.println(F("Perfect memory!"));

  display.setCursor(24, 55);
  display.println(F("Press START"));

  display.display();
}

// =====================================================
// MATRIX FUNCTIONS
// =====================================================

void drawCursor() {
  matrix.clear();

  if (cursorVisible) {
    byte pixel = xyToPixel(cursorX, cursorY);

    matrix.setPixelColor(
      pixel,
      matrix.Color(0, 0, 150)
    );
  }

  matrix.show();
}

void flashPixelGreen(byte pixel) {
  for (byte flash = 0; flash < 2; flash++) {
    matrix.clear();

    matrix.setPixelColor(
      pixel,
      matrix.Color(0, 150, 0)
    );

    matrix.show();
    delay(130);

    matrix.clear();
    matrix.show();
    delay(70);
  }
}

void flashMatrixRed() {
  for (byte flash = 0; flash < 3; flash++) {
    fillMatrix(matrix.Color(120, 0, 0));
    delay(150);

    matrix.clear();
    matrix.show();
    delay(100);
  }
}

void greenWaveAnimation() {
  matrix.clear();
  matrix.show();

  for (byte y = 0; y < 4; y++) {
    for (byte x = 0; x < 4; x++) {
      byte pixel = xyToPixel(x, y);

      matrix.setPixelColor(
        pixel,
        matrix.Color(0, 100, 0)
      );
    }

    matrix.show();
    delay(130);
  }

  delay(180);

  matrix.clear();
  matrix.show();
}

void rainbowAnimation() {
  for (unsigned int frame = 0; frame < 256; frame += 6) {
    for (byte i = 0; i < LED_COUNT; i++) {
      byte hue = frame + (i * 16);

      matrix.setPixelColor(
        i,
        colourWheel(hue)
      );
    }

    matrix.show();
    delay(35);
  }

  matrix.clear();
  matrix.show();
}

uint32_t colourWheel(byte position) {
  position = 255 - position;

  if (position < 85) {
    return matrix.Color(
      255 - position * 3,
      0,
      position * 3
    );
  }

  if (position < 170) {
    position -= 85;

    return matrix.Color(
      0,
      position * 3,
      255 - position * 3
    );
  }

  position -= 170;

  return matrix.Color(
    position * 3,
    255 - position * 3,
    0
  );
}

void fillMatrix(uint32_t colour) {
  for (byte i = 0; i < LED_COUNT; i++) {
    matrix.setPixelColor(i, colour);
  }

  matrix.show();
}

byte xyToPixel(byte x, byte y) {
  /*
    Serpentine matrix arrangement:

    Row 0:  0  1  2  3
    Row 1:  7  6  5  4
    Row 2:  8  9 10 11
    Row 3: 15 14 13 12
  */

  if (y % 2 == 0) {
    return (y * 4) + x;
  }

  return (y * 4) + (3 - x);
}

// =====================================================
// BUTTON FUNCTIONS
// =====================================================

void updateButtons() {
  for (byte i = 0; i < BUTTON_COUNT; i++) {
    bool newReading = digitalRead(buttonPins[i]);

    if (newReading != buttonReading[i]) {
      buttonReading[i] = newReading;
      buttonChangeTime[i] = millis();
    }

    if (
      millis() - buttonChangeTime[i] >= DEBOUNCE_DELAY &&
      buttonStable[i] != buttonReading[i]
    ) {
      buttonPreviousStable[i] = buttonStable[i];
      buttonStable[i] = buttonReading[i];

      if (
        buttonPreviousStable[i] == HIGH &&
        buttonStable[i] == LOW
      ) {
        buttonPressedEvent[i] = true;
      }
    }
  }
}

bool buttonPressed(byte buttonIndex) {
  if (buttonPressedEvent[buttonIndex]) {
    buttonPressedEvent[buttonIndex] = false;
    return true;
  }

  return false;
}

void clearButtonEvents() {
  for (byte i = 0; i < BUTTON_COUNT; i++) {
    buttonPressedEvent[i] = false;
  }
}

void waitForAllButtonsReleased() {
  bool anyPressed;

  do {
    anyPressed = false;

    for (byte i = 0; i < BUTTON_COUNT; i++) {
      if (digitalRead(buttonPins[i]) == LOW) {
        anyPressed = true;
      }
    }

    delay(5);
  }
  while (anyPressed);

  delay(40);
}

// =====================================================
// OLED HELPER
// =====================================================

void showCentredMessage(
  const __FlashStringHelper* line1,
  const __FlashStringHelper* line2
) {
  display.clearDisplay();

  display.setTextSize(2);
  display.setCursor(9, 13);
  display.println(line1);

  display.setTextSize(1);
  display.setCursor(20, 43);
  display.println(line2);

  display.display();
}

// =====================================================
// EEPROM HIGH-SCORE STORAGE
// =====================================================

void loadHighScore() {
  byte savedMarker = EEPROM.read(EEPROM_MARKER_ADDRESS);

  if (savedMarker == EEPROM_MARKER) {
    highScore = EEPROM.read(EEPROM_SCORE_ADDRESS);

    if (highScore > MAX_LEVEL) {
      highScore = 0;
      saveHighScore();
    }
  } else {
    highScore = 0;

    EEPROM.update(EEPROM_MARKER_ADDRESS, EEPROM_MARKER);
    EEPROM.update(EEPROM_SCORE_ADDRESS, highScore);
  }
}

void saveHighScore() {
  EEPROM.update(EEPROM_MARKER_ADDRESS, EEPROM_MARKER);
  EEPROM.update(EEPROM_SCORE_ADDRESS, highScore);
}

// =====================================================
// SOUNDS
// =====================================================

void playStartupSound() {
  tone(BUZZER_PIN, 523, 100);
  delay(130);

  tone(BUZZER_PIN, 659, 100);
  delay(130);

  tone(BUZZER_PIN, 784, 180);
  delay(210);

  noTone(BUZZER_PIN);
}

void playStartSound() {
  tone(BUZZER_PIN, 600, 90);
  delay(110);

  tone(BUZZER_PIN, 850, 90);
  delay(110);

  tone(BUZZER_PIN, 1100, 150);
  delay(170);
}

void playCorrectStepSound() {
  tone(BUZZER_PIN, 1050, 70);
  delay(75);
  noTone(BUZZER_PIN);
}

void playWrongSound() {
  tone(BUZZER_PIN, 230, 250);
  delay(270);

  tone(BUZZER_PIN, 160, 350);
  delay(370);

  noTone(BUZZER_PIN);
}

void playLevelCompleteSound() {
  tone(BUZZER_PIN, 660, 100);
  delay(120);

  tone(BUZZER_PIN, 880, 100);
  delay(120);

  tone(BUZZER_PIN, 1100, 180);
  delay(210);

  noTone(BUZZER_PIN);
}

void playGameOverSound() {
  tone(BUZZER_PIN, 400, 180);
  delay(200);

  tone(BUZZER_PIN, 300, 180);
  delay(200);

  tone(BUZZER_PIN, 190, 350);
  delay(370);

  noTone(BUZZER_PIN);
}

void playVictorySound() {
  const int notes[] = {
    523, 659, 784, 1047,
    784, 1047, 1319
  };

  const int durations[] = {
    120, 120, 120, 240,
    120, 120, 400
  };

  for (byte i = 0; i < 7; i++) {
    tone(BUZZER_PIN, notes[i], durations[i]);
    delay(durations[i] + 40);
  }

  noTone(BUZZER_PIN);
}