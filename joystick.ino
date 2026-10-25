#include <BleGamepad.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <Adafruit_NeoPixel.h>

// =====================================================
// ESP32 RACING PRO (Cu 8 Butoane + Touch)
// Compatibilitate vizata: ESP32 Arduino Core 3.x
// =====================================================

// OLED SSD1306 128x64, I2C: SDA=21, SCL=22
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(
  U8G2_R0, U8X8_PIN_NONE, 22, 21
);

// Controller Bluetooth
BleGamepad bleGamepad("ESP32 RPM Maker", "F1", 67);

// Pini joystick
const int PIN_X = 33;
const int PIN_Y = 32;
const int PIN_SW = 25;

// Butoane externe
const int PIN_BTN1 = 14;
const int PIN_BTN2 = 27;
const int PIN_BTN3 = 13;
const int PIN_BTN4 = 23;
const int PIN_BTN5 = 15;
const int PIN_BTN6 = 5;
const int PIN_BTN7 = 26; // Butonul touch (VCC, GND, I/O)

// Audio si LED-uri
const int PIN_BUZZER = 12;
const int PIN_BUZZER2 = 4;
const int PIN_LED = 2;
const int PIN_LED2 = 18;

// Inel NeoPixel
const int PIN_RING_LED = 19;
#define NUM_LEDS 12
Adafruit_NeoPixel ring(
  NUM_LEDS, PIN_RING_LED, NEO_GRB + NEO_KHZ800
);

// Configurare audio
const int FRECVENTA_TON = 1500;
const int VOLUM_BUZZER = 100;

// Calibrare joystick
const int ADC_MIN = 1200;
const int ADC_MAX = 2300;
const int ADC_CENTER = 2048;
const int DEADZONE_X = 180;
const int DEADZONE_Y = 120;

// Filtru acceleratie/frana
float smoothedY = 0.0f;
const float SMOOTHING = 0.04f;

// Stari butoane
bool ultimulBtn1State = HIGH;
bool ultimulBtn2State = HIGH;
bool ultimulBtn3State = HIGH;
bool ultimulBtn4State = HIGH;
bool ultimulBtn5State = HIGH;
bool ultimulBtn6State = HIGH;
bool ultimulBtn7State = LOW;  // Pentru touch (idle = LOW, touch = HIGH)
bool ultimulSwState = HIGH;

// Temporizari
unsigned long timpUltimulBipLimitator = 0;
unsigned long timpUltimulOLED = 0;
unsigned long timpUltimulABS = 0;

// Stari efecte
bool stareLimitatorActiva = false;
bool stareLedABS = false;
bool absActiv = false;
bool launchControlActiv = false;

// =====================================================
// FUNCTII AUDIO
// =====================================================

void opresteBuzzer1() {
  ledcWrite(PIN_BUZZER, 0);
  digitalWrite(PIN_LED, LOW);
}

void pornesteSemnalizare(int frecventa, int volum, bool aprindeLed) {
  ledcWriteTone(PIN_BUZZER, frecventa);
  ledcWrite(PIN_BUZZER, volum);
  digitalWrite(PIN_LED, aprindeLed ? HIGH : LOW);
}

void opresteBuzzer2() {
  ledcWrite(PIN_BUZZER2, 0);
}

void clickMecanicBuzzer2(int frecventa) {
  if (absActiv) return;

  ledcWriteTone(PIN_BUZZER2, frecventa);
  ledcWrite(PIN_BUZZER2, 80);
  delay(25);
  opresteBuzzer2();
}

// =====================================================
// FUNCTII LED
// =====================================================

void deseneazaSteagCarouri() {
  for (int y = 0; y < 64; y += 8) {
    for (int x = 0; x < 128; x += 8) {
      if (((x / 8) + (y / 8)) % 2 == 0) {
        u8g2.drawBox(x, y, 8, 8);
      }
    }
  }
}

void seteazaInel(uint8_t r, uint8_t g, uint8_t b) {
  for (int i = 0; i < NUM_LEDS; i++) {
    ring.setPixelColor(i, ring.Color(r, g, b));
  }
  ring.show();
}

void actualizeazaInel(int controlY) {
  if (controlY >= 0) {
    int leduriAprinse = map(
      constrain(controlY, 0, 32767),
      0, 32767, 0, NUM_LEDS
    );

    if (launchControlActiv) {
      if (millis() % 200 < 100) {
        seteazaInel(255, 100, 0);
      } else {
        ring.clear();
        ring.show();
      }
    }
    else if (controlY > 31000) {
      if (stareLimitatorActiva) {
        seteazaInel(0, 0, 255);
      } else {
        ring.clear();
        ring.show();
      }
    }
    else {
      for (int i = 0; i < NUM_LEDS; i++) {
        if (i < leduriAprinse) {
          if (i < 5) {
            ring.setPixelColor(i, ring.Color(0, 255, 0));
          }
          else if (i < 9) {
            ring.setPixelColor(i, ring.Color(255, 150, 0));
          }
          else {
            ring.setPixelColor(i, ring.Color(255, 0, 0));
          }
        } else {
          ring.setPixelColor(i, ring.Color(0, 0, 0));
        }
      }
      ring.show();
    }
  }
  else {
    if (controlY < -30000 && stareLedABS) {
      seteazaInel(255, 0, 0);
    } else {
      ring.clear();
      ring.show();
    }
  }
}

// =====================================================
// FUNCTII BUTOANE
// =====================================================

void proceseazaButon(int stare, bool &stareAnterioara, uint8_t numarButon, int frecventaClick) {
  if (stare == LOW) {
    bleGamepad.press(numarButon);

    if (stareAnterioara == HIGH) {
      if (!stareLimitatorActiva) {
        digitalWrite(PIN_LED, HIGH);
      }

      clickMecanicBuzzer2(frecventaClick);

      if (!stareLimitatorActiva) {
        digitalWrite(PIN_LED, LOW);
      }
    }
  } else {
    bleGamepad.release(numarButon);
  }

  stareAnterioara = stare;
}

void proceseazaButonTouch(int stare, bool &stareAnterioara, uint8_t numarButon, int frecventaClick) {
  if (stare == HIGH) {
    bleGamepad.press(numarButon);

    if (stareAnterioara == LOW) {
      if (!stareLimitatorActiva) {
        digitalWrite(PIN_LED, HIGH);
      }

      clickMecanicBuzzer2(frecventaClick);

      if (!stareLimitatorActiva) {
        digitalWrite(PIN_LED, LOW);
      }
    }
  } else {
    bleGamepad.release(numarButon);
  }

  stareAnterioara = stare;
}

// =====================================================
// SETUP
// =====================================================

void setup() {
  Serial.begin(115200);

  analogReadResolution(12);

  pinMode(PIN_SW, INPUT_PULLUP);
  pinMode(PIN_BTN1, INPUT_PULLUP);
  pinMode(PIN_BTN2, INPUT_PULLUP);
  pinMode(PIN_BTN3, INPUT_PULLUP);
  pinMode(PIN_BTN4, INPUT_PULLUP);
  pinMode(PIN_BTN5, INPUT_PULLUP);
  pinMode(PIN_BTN6, INPUT_PULLUP);
  pinMode(PIN_BTN7, INPUT_PULLDOWN);

  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_LED2, OUTPUT);

  digitalWrite(PIN_LED, LOW);
  digitalWrite(PIN_LED2, LOW);

  bool pwm1 = ledcAttach(PIN_BUZZER, FRECVENTA_TON, 8);
  bool pwm2 = ledcAttach(PIN_BUZZER2, 800, 8);

  if (!pwm1 || !pwm2) {
    Serial.println("Eroare: configurare PWM buzzer.");
  }

  opresteBuzzer1();
  opresteBuzzer2();

  u8g2.begin();

  ring.begin();
  ring.setBrightness(50);
  ring.clear();
  ring.show();

  u8g2.clearBuffer();
  deseneazaSteagCarouri();
  u8g2.sendBuffer();

  for (int i = 0; i < 3; i++) {
    ledcWriteTone(PIN_BUZZER2, 1000);
    ledcWrite(PIN_BUZZER2, 80);

    digitalWrite(PIN_LED, HIGH);
    digitalWrite(PIN_LED2, HIGH);
    delay(150);

    opresteBuzzer2();
    digitalWrite(PIN_LED, LOW);
    digitalWrite(PIN_LED2, LOW);
    delay(150);
  }

  ledcWriteTone(PIN_BUZZER2, 1800);
  ledcWrite(PIN_BUZZER2, 90);
  digitalWrite(PIN_LED, HIGH);
  digitalWrite(PIN_LED2, HIGH);

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_logisoso16_tf);
  u8g2.drawStr(15, 38, "READY?");
  u8g2.sendBuffer();

  delay(400);

  opresteBuzzer2();
  digitalWrite(PIN_LED, LOW);
  digitalWrite(PIN_LED2, LOW);

  delay(200);

  BleGamepadConfiguration config;
  config.setAutoReport(true);
  config.setControllerType(CONTROLLER_TYPE_GAMEPAD);
  config.setButtonCount(16);

  bleGamepad.begin(&config);

  Serial.println("ESP32 Racing Pro pornit cu 8 butoane.");
}

// =====================================================
// LOOP
// =====================================================

void loop() {
  bool conectat = bleGamepad.isConnected();

  int rawX = constrain(analogRead(PIN_X), ADC_MIN, ADC_MAX);
  int rawY = constrain(analogRead(PIN_Y), ADC_MIN, ADC_MAX);

  int swState = digitalRead(PIN_SW);
  int btn1State = digitalRead(PIN_BTN1);
  int btn2State = digitalRead(PIN_BTN2);
  int btn3State = digitalRead(PIN_BTN3);
  int btn4State = digitalRead(PIN_BTN4);
  int btn5State = digitalRead(PIN_BTN5);
  int btn6State = digitalRead(PIN_BTN6);
  int btn7State = digitalRead(PIN_BTN7);

  int controlX = 0;
  int controlY = 0;
  int targetY = 0;

  launchControlActiv = false;

  if (conectat) {
    if (abs(rawX - ADC_CENTER) > DEADZONE_X) {
      if (rawX > ADC_CENTER) {
        controlX = map(rawX, ADC_CENTER + DEADZONE_X, ADC_MAX, 0, 32767);
      } else {
        controlX = map(rawX, ADC_MIN, ADC_CENTER - DEADZONE_X, -32767, 0);
      }
    }

    controlX = constrain(controlX, -32767, 32767);

    if (abs(rawY - ADC_CENTER) > DEADZONE_Y) {
      if (rawY > ADC_CENTER) {
        targetY = map(rawY, ADC_CENTER + DEADZONE_Y, ADC_MAX, 0, 32767);
      } else {
        targetY = map(rawY, ADC_MIN, ADC_CENTER - DEADZONE_Y, -32767, 0);
      }
    }

    targetY = constrain(targetY, -32767, 32767);

    smoothedY += SMOOTHING * (targetY - smoothedY);
    controlY = constrain((int)smoothedY, -32767, 32767);

    if (btn6State == LOW && controlY > 18000) {
      controlY = 20000;
      launchControlActiv = true;
    }

    bleGamepad.setLeftThumb(controlX, controlY);

    absActiv = (controlY < -30000);

    if (absActiv) {
      ledcWriteTone(PIN_BUZZER2, 600);
      ledcWrite(PIN_BUZZER2, 70);

      if (millis() - timpUltimulABS >= 60) {
        stareLedABS = !stareLedABS;
        digitalWrite(PIN_LED2, stareLedABS ? HIGH : LOW);
        timpUltimulABS = millis();
      }
    } else {
      digitalWrite(PIN_LED2, LOW);
      stareLedABS = false;
      opresteBuzzer2();
    }

    if (controlY > 31000 || launchControlActiv) {
      unsigned long intervalBip = launchControlActiv ? 60 : 80;

      if (millis() - timpUltimulBipLimitator >= intervalBip) {
        stareLimitatorActiva = !stareLimitatorActiva;
        timpUltimulBipLimitator = millis();

        if (stareLimitatorActiva) {
          int frecventa = launchControlActiv ? 1200 : FRECVENTA_TON + 300;
          pornesteSemnalizare(frecventa, VOLUM_BUZZER, true);
        } else {
          opresteBuzzer1();
        }
      }
    } else {
      stareLimitatorActiva = false;
      opresteBuzzer1();
    }

    proceseazaButon(btn1State, ultimulBtn1State, BUTTON_2, 900);
    proceseazaButon(btn2State, ultimulBtn2State, BUTTON_3, 600);
    proceseazaButon(btn3State, ultimulBtn3State, BUTTON_4, 1200);
    proceseazaButon(btn4State, ultimulBtn4State, BUTTON_5, 1000);
    proceseazaButon(btn5State, ultimulBtn5State, BUTTON_6, 700);
    proceseazaButon(btn6State, ultimulBtn6State, BUTTON_7, 2000);
    proceseazaButonTouch(btn7State, ultimulBtn7State, BUTTON_8, 1300);

    if (swState == LOW) {
      bleGamepad.press(BUTTON_1);
    } else {
      bleGamepad.release(BUTTON_1);
    }
    ultimulSwState = swState;

    actualizeazaInel(controlY);

  } else {
    smoothedY = 0.0f;
    absActiv = false;
    stareLimitatorActiva = false;
    stareLedABS = false;

    opresteBuzzer1();
    opresteBuzzer2();

    digitalWrite(PIN_LED2, LOW);

    ring.clear();
    ring.show();
  }

  if (millis() - timpUltimulOLED >= 100) {
    timpUltimulOLED = millis();

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);

    u8g2.drawStr(0, 10, "BT:");
    if (conectat) {
      u8g2.drawStr(25, 10, "CONECTAT");
    } else {
      u8g2.drawStr(25, 10, "DEconectat");
    }

    u8g2.drawStr(0, 26, "STR:");
    u8g2.drawFrame(30, 18, 95, 10);

    int baraX = map(controlX, -32767, 32767, 2, 89);
    u8g2.drawBox(30 + baraX, 20, 4, 6);

    char bufferProcent[12];

    if (controlY >= 0) {
      if (launchControlActiv) {
        u8g2.drawStr(0, 44, "L C:");
      } else {
        u8g2.drawStr(0, 44, "THR:");
      }

      int procentY = map(constrain(controlY, 0, 32767), 0, 32767, 0, 100);
      snprintf(bufferProcent, sizeof(bufferProcent), "%d%%", procentY);
    } else {
      u8g2.drawStr(0, 44, "BRK:");

      int procentBrake = map(constrain(controlY, -32767, 0), -32767, 0, 100, 0);
      snprintf(bufferProcent, sizeof(bufferProcent), "%d%%", procentBrake);
    }

    u8g2.setFont(u8g2_font_logisoso16_tf);
    u8g2.drawStr(30, 48, bufferProcent);

    if (controlY > 31000) {
      u8g2.setFont(u8g2_font_6x10_tf);
      u8g2.drawStr(40, 62, "SHIFT NOW!");
    } else if (absActiv) {
      u8g2.setFont(u8g2_font_6x10_tf);
      u8g2.drawStr(40, 62, "ABS ACTIVE");
    }

    u8g2.sendBuffer();
  }

  delay(10);
}
