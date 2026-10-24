#include <BleGamepad.h>

// Inițializăm controllerul Bluetooth cu numele pentru jocuri
BleGamepad bleGamepad("ESP32 Racing Wheel", "DIY", 100);

// Configurare Pini
const int PIN_X = 33;   // VRx - Direcție (Stânga / Dreapta)
const int PIN_Y = 32;   // VRy - Accelerație / Frână
const int PIN_SW = 25;  // SW - Buton

// Calibrare Joystick (Modificată pentru plaja citită de ESP32)
const int ADC_MAX = 4095;    
const int ADC_CENTER = 2048; 
const int DEADZONE = 180;    // Zonă neutră ca să nu vireze singură mașina când e pe mijloc

bool aTrimisCentrarea = false; // Rezolvă problema pornirii din colț în joy.cpl

void setup() {
  pinMode(PIN_SW, INPUT_PULLUP);
  
  // Pornim emularea de Gamepad Bluetooth
  bleGamepad.begin();
}

void loop() {
  // Trimitem date doar dacă suntem conectați prin Bluetooth
  if (bleGamepad.isConnected()) {
    
    // Forțăm Windows să vadă centrul imediat după conectare
    if (!aTrimisCentrarea) {
      bleGamepad.setLeftThumb(0, 0);
      bleGamepad.sendReport();
      aTrimisCentrarea = true; 
    }
    
    int rawX = analogRead(PIN_X);
    int rawY = analogRead(PIN_Y);
    int btnState = digitalRead(PIN_SW);

    // Format standard pe care îl înțeleg telefoanele și PC-urile (-32767 la 32767)
    int controlX = 0; // 0 este centrul perfect
    
    // Procesare Axă X (Direcție lină)
    if (abs(rawX - ADC_CENTER) > DEADZONE) {
      if (rawX > ADC_CENTER) {
        // Viraj Dreapta: mapare lină de la centru spre maxim
        controlX = map(rawX, ADC_CENTER + DEADZONE, ADC_MAX, 0, 32767);
      } else {
        // Viraj Stânga: mapare lină spre minim
        controlX = map(rawX, 0, ADC_CENTER - DEADZONE, -32767, 0);
      }
    }

    // Procesare Axă Y (Accelerație/Frână) - INVERSATĂ COMPLET
    int controlY = 0;
    if (abs(rawY - ADC_CENTER) > DEADZONE) {
      if (rawY > ADC_CENTER) {
        // Mapare inversată: de la 0 spre -32767
        controlY = map(rawY, ADC_CENTER + DEADZONE, ADC_MAX, 0, -32767);
      } else {
        // Mapare inversată: de la 32767 spre 0
        controlY = map(rawY, 0, ADC_CENTER - DEADZONE, 32767, 0);
      }
    }

    // Trimitem axele către Joystick-ul Stâng (Left Thumb) al controllerului virtual
    bleGamepad.setLeftThumb(controlX, controlY);

    // Trimitere stare buton (Apasă butonul 1 la click pe manetă)
    if (btnState == LOW) {
      bleGamepad.press(BUTTON_1);
    } else {
      bleGamepad.release(BUTTON_1);
    }

    // Trimitem raportul complet prin Bluetooth
    bleGamepad.sendReport();
  } else {
    // Dacă se deconectează Bluetooth-ul, resetăm starea pentru următoarea conectare
    aTrimisCentrarea = false;
  }
  
  // Mică pauză de stabilitate cerută de protocoalele de telefon
  delay(12);
}
