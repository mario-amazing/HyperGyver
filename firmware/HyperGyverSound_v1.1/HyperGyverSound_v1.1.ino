/*
  Скетч к проекту "Гиперкуб"
  ПЕРЕКЛЮЧЕНИЕ КНОПКОЙ НА D3 и GND!!!

  Страница проекта (схемы, описания): https://alexgyver.ru/hypergyver/
  Исходники на GitHub: https://github.com/AlexGyver/hypergyver/
  Нравится, как написан и закомментирован код? Поддержи автора! https://alexgyver.ru/support_alex/
  Автор: AlexGyver Technologies, 2020
  http://AlexGyver.ru/
*/

// Версия 1.1: исправлен случайный глюк. Виноваты random функции из либы FastLED!!!
// Версия с микрофоном

// ===== НАСТРОЙКИ =====
#define EDGE_LEDS 11    // кол-во диодов на ребре куба
#define LED_DI 2        // пин подключения
#define SWITCH_MODE_DI 5// пин подключения свича модов
#define BRIGHT 255      // яркость
#define CHANGE_PRD 20   // смена режима, секунд
#define CUR_LIMIT 3000  // лимит тока в мА (0 - выкл)

#define VOL_THR 20
#define ADC_PIN A0

// ===== ДЛЯ РАЗРАБОВ =====
const int NUM_LEDS = (EDGE_LEDS * 24);
#define USE_MICROLED 0
#include <FastLED.h>
CRGBPalette16 currentPalette, linearPalette;
const int FACE_SIZE = EDGE_LEDS * 4;
const int LINE_SIZE = EDGE_LEDS;
#define PAL_STEP 30
CRGB leds[NUM_LEDS];

#include "GyverButton.h"
GButton manualChangeModeButton(3, HIGH_PULL, NORM_CLOSE);

int curBright = BRIGHT;
bool fadeFlag = false;
bool mode = false;
bool colorMode = false;
bool autoChangeMode = true;
uint16_t counter = 0;
byte speed = 15;
uint32_t tmrDraw, tmrColor, tmrFade;
bool soundMode = 1;

#define DEBUG(x) //Serial.println(x)

uint32_t getPixColor(CRGB color) {
  return (((uint32_t)color.r << 16) | (color.g << 8) | color.b);
}

void setup() {
  //Serial.begin(9600);
  FastLED.addLeds<WS2812, LED_DI, GRB>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );
  if (CUR_LIMIT > 0) FastLED.setMaxPowerInVoltsAndMilliamps(5, CUR_LIMIT);
  FastLED.setBrightness(BRIGHT);
  FastLED.clear();

  randomSeed(getEntropy(A1));   // My system's in decline, EMBRACING ENTROPY!
  fadeFlag = true;  // сразу флаг на смену режима

  // ускоряем АЦП
  bitSet(ADCSRA, ADPS2);
  bitClear(ADCSRA, ADPS1);
  bitSet(ADCSRA, ADPS0);

  manualChangeModeButton.setTimeout(400);
  pinMode(SWITCH_MODE_DI, INPUT);
}

void loop() {
  bool switchState = !digitalRead(SWITCH_MODE_DI);
  if (soundMode != switchState){
    soundMode = switchState;
    autoChangeMode = true;
    tmrColor = millis();
  }

  manualChangeModeButton.tick();
  if (manualChangeModeButton.isPress()) {
    autoChangeMode = false;
    tmrColor = millis();
    changeMode();
  }
  if (manualChangeModeButton.isHolded()) {
    autoChangeMode = true;
  }

  // отрисовка
  if (millis() - tmrDraw >= 10) {
    tmrDraw = millis();

    if (soundMode) {
      FastLED.clear();
      int thisSound = getSoundLength();

      if (colorMode) {
        // VU метр
        if (mode) {
          int thisL = (float)FACE_SIZE * thisSound / 100.0 + 1;
          thisL = constrain(thisL, 0, FACE_SIZE);
          for (int i = 0; i < thisL; i++) {
            fillVertex(i, ColorFromPalette(currentPalette, (float)255 * i / FACE_SIZE / 2 + counter / 4, 255, LINEARBLEND));
            //fillColumns(i, ColorFromPalette(currentPalette, (float)255 * i / FACE_SIZE / 2 + counter / 10, 255, LINEARBLEND));
          }
        } else {

          // летят под бит
          static byte linearCounter = 0;
          static int maxVol = 0;
          if (thisSound > maxVol) maxVol = thisSound;
          if (linearCounter % 3 == 0) {
            for (byte i = 0; i < 15; i++) {
              linearPalette[i] = linearPalette[i + 1];
            }
            linearPalette[15] = CHSV(linearCounter, 255, (float)255 * maxVol / 100);
            maxVol = 0;
          }

          linearCounter += 2;
          for (int i = 0; i < FACE_SIZE; i++) {
            fillVertex(i, ColorFromPalette(linearPalette, (float)255 * i / FACE_SIZE, 255, LINEARBLEND));
          }
        }

      } else {
        // ускоряются от громкости
        counter += thisSound / 2;
        for (int i = 0; i < FACE_SIZE; i++) {
          fillSimple(i, ColorFromPalette(currentPalette, getMaxNoise(i * PAL_STEP / 2 + counter / 3, 0), 255, LINEARBLEND));
        }
      }

    } else {

      // обычные режимы
      for (int i = 0; i < FACE_SIZE; i++) {
        if (mode) fillSimple(i, ColorFromPalette(currentPalette, getMaxNoise(i * PAL_STEP + counter, counter), 255, LINEARBLEND));
        else fillVertex(i, ColorFromPalette(currentPalette, (float)255 * i / FACE_SIZE / 2 + counter / 4, 255, LINEARBLEND));
      }
    }

    FastLED.show();
    counter += speed;
  }

  // смена режима и цвета
  if (millis() - tmrColor >= CHANGE_PRD * 1000L) {
    tmrColor = millis();
    if (autoChangeMode) {
      fadeFlag = true;
    }
    else {
      changePalette();
    }
  }

  // фейдер для смены через чёрный
  if (fadeFlag && millis() - tmrFade >= 30) {
    static int8_t fadeDir = -1;
    tmrFade = millis();
    if (fadeFlag) {
      curBright += 5 * fadeDir;

      if (curBright < 5) {
        curBright = 5;
        fadeDir = 1;
        changeMode();
      }
      if (curBright > BRIGHT) {
        curBright = BRIGHT;
        fadeDir = -1;
        fadeFlag = false;
      }
      FastLED.setBrightness(curBright);
    }
  }
} // луп

void changeMode() {
  if (!random(3)) mode = !mode;
  colorMode = !colorMode;
  changePalette();
}

void changePalette() {
  speed = random(1, 5);
  int thisDebth = random(0, 32768);
  byte thisStep = random(2, 7) * 5;
  bool sparkles = !random(4);

  if (colorMode) {
    for (int i = 0; i < 16; i++) {
      currentPalette[i] = CHSV(getMaxNoise(thisDebth + i * thisStep, thisDebth),
                               sparkles ? (!random(9) ? 30 : 255) : 255,
                               soundMode ? 255 : (constrain((i + 7) * (i + 7), 0, 255)));
    }
  } else {
    for (int i = 0; i < 4; i++) {
      CHSV color = CHSV(random(0, 256), bool(random(0, 3)) * 255, random(0, 256));
      for (byte j = 0; j < 4; j++) {
        currentPalette[i * 4 + j] = color;
      }
    }
  }
}

// масштабируем шум
byte getMaxNoise(uint16_t x, uint16_t y) {
  return constrain(map(inoise8(x, y), 50, 200, 0, 255), 0, 255);
}

// заливка всех 6 граней в одинаковом порядке
void fillSimple(int num, CRGB color) {  // num 0-NUM_LEDS / 6
  for (byte i = 0; i < 6; i++) {
    leds[i * FACE_SIZE + num] = color;
  }
}

// заливка из четырёх вершин
void fillVertex(int num, CRGB color) { // num 0-NUM_LEDS / 6
  num /= 4;
  byte thisRow = 0;
  for (byte i = 0; i < 3; i++) {
    leds[LINE_SIZE * thisRow + num] = color;
    thisRow += 2;
    leds[LINE_SIZE * thisRow - num - 1] = color;
    leds[LINE_SIZE * thisRow + num] = color;
    thisRow += 2;
    leds[LINE_SIZE * thisRow - num - 1] = color;
  }
  thisRow = 13;
  for (byte i = 0; i < 3; i++) {
    leds[LINE_SIZE * thisRow - num - 1] = color;
    leds[LINE_SIZE * thisRow + num] = color;
    thisRow += 2;
    leds[LINE_SIZE * thisRow - num - 1] = color;
    leds[LINE_SIZE * thisRow + num] = color;
    thisRow += 2;
  }
}

void fillColumns(int num, CRGB color) {
  num /= 4;
  leds[LINE_SIZE * 0 + num] = color;
  leds[LINE_SIZE * 3 - num - 1] = color;

  leds[LINE_SIZE * 9 + num] = color;
  leds[LINE_SIZE * 12 - num - 1] = color;

  leds[LINE_SIZE * 14 - num - 1] = color;
  leds[LINE_SIZE * 15 + num] = color;

  leds[LINE_SIZE * 21 - num - 1] = color;
  leds[LINE_SIZE * 22 + num] = color;
}

// рандом сид из сырого аналога
uint32_t getEntropy(byte pin) {
  unsigned long seed = 0;
  for (int i = 0; i < 400; i++) {
    seed = 1;
    for (byte j = 0; j < 16; j++) {
      seed *= 4;
      seed += analogRead(pin) & 3;
    }
  }
  return seed;
}
