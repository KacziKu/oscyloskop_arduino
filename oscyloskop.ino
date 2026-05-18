#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

#define BUFOR_SIZE 160
#define WAVE_HEIGHT 115
#define BOTTOM_SPACE 15
#define GRID_SIZE 23
#define markerTimeout 3000
#define BUTTON_FREEZE 6
#define BUTTON_DELTA 7
#define SAMPLE_INTERVAL 35

int value;
int bufor[BUFOR_SIZE];
int bufor2[BUFOR_SIZE];

int markerY = -1;
int markerX = -1;

int lastPotValueY = 0;
int lastPotValueX = 0;

unsigned long lastMarkerMoveY = 0;
unsigned long lastMarkerMoveX = 0;

bool isFrozen = false;
int lastFreezeButtonState = HIGH;
int currentFreezeButtonState;

bool isDelta = false;
int lastDeltaButtonState = HIGH;
int currentDeltaButtonState;
int deltaMarkerX = -1;
int deltaMarkerY = -1;

bool deltaMarkerDrawn = false; 

unsigned long lastSampleTime = 0;
unsigned long sampleEnd = 0;
bool bottomInfoNeedsUpdate = true;
bool bottomStaticInfoDrawn = false;

// przywróć czerwony marker Y (linia delta)
void restoreDeltaMarkerYLine(int y) {  
  if (y < 0 || y > WAVE_HEIGHT) return;

  for (int x = 0; x < tft.width(); x++) {
    uint16_t color = TFT_BLACK;
    if (y % GRID_SIZE == 0 || x % GRID_SIZE == 0) {
      color = TFT_DARKGREY;
    }
    tft.drawPixel(x, y, color);
  }
}

// przywróć czerwony marker X (linia delta)
void restoreDeltaMarkerXLine(int x) {  
  if (x < 0 || x >= BUFOR_SIZE) return;

  for (int y = 0; y <= WAVE_HEIGHT; y++) {
    uint16_t color = TFT_BLACK;
    if (y % GRID_SIZE == 0 || x % GRID_SIZE == 0) {
      color = TFT_DARKGREY;
    }
    tft.drawPixel(x, y, color);
  }
}

// rysowanie siatki
void drawGrid() {
  tft.fillScreen(TFT_BLACK);

  for (int y = 0; y <= WAVE_HEIGHT; y += GRID_SIZE) {
    tft.drawFastHLine(0, y, tft.width(), TFT_DARKGREY);
  }

  for (int x = 0; x < tft.width(); x += GRID_SIZE) {
    tft.drawFastVLine(x, 0, WAVE_HEIGHT, TFT_DARKGREY);
  }

  drawStaticBottomInfo();
  bottomInfoNeedsUpdate = true;
}

// statyczne dane
void drawStaticBottomInfo() {
  if (!bottomStaticInfoDrawn) {
    tft.fillRect(0, WAVE_HEIGHT + 1, tft.width(), BOTTOM_SPACE - 1, TFT_BLACK);
    bottomStaticInfoDrawn = true;
    tft.drawFastHLine(0, WAVE_HEIGHT, tft.width(), TFT_DARKGREY);
  }

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(ML_DATUM);
  tft.drawString("1V/dz", 2, WAVE_HEIGHT + BOTTOM_SPACE / 2);

  int timePerDiv = SAMPLE_INTERVAL * GRID_SIZE;
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  char timeLabel[20];
  sprintf(timeLabel, "%dms/dz", timePerDiv);
  tft.drawString(timeLabel, tft.width() / 2, WAVE_HEIGHT + BOTTOM_SPACE / 2);
}

// dynamiczne informacje na dole
void drawDynamicBottomInfo() {
  int clearX = tft.width() * 2 / 3;
  int clearW = tft.width() - clearX;
  tft.fillRect(clearX, WAVE_HEIGHT + 1, clearW, BOTTOM_SPACE - 1, TFT_BLACK);

  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);

  if (isDelta) {
    if (markerY != -1 && deltaMarkerY != -1) {
      float volts1 = ((float)(WAVE_HEIGHT - markerY)) * (5.0 / WAVE_HEIGHT);
      float volts2 = ((float)(WAVE_HEIGHT - deltaMarkerY)) * (5.0 / WAVE_HEIGHT);
      float deltaV = fabs(volts1 - volts2);
      char buf[20];
      dtostrf(deltaV, 0, 2, buf);
      tft.drawString("d:" + String(buf) + "V", tft.width() - 2, WAVE_HEIGHT + BOTTOM_SPACE / 2);
    } else if (markerX != -1 && deltaMarkerX != -1) {
      int deltaT = abs((markerX - deltaMarkerX) * SAMPLE_INTERVAL);
      char buf[20];
      sprintf(buf, "d:%dms", deltaT);
      tft.drawString(String(buf), tft.width() - 2, WAVE_HEIGHT + BOTTOM_SPACE / 2);
    }
  } else {
    if (markerY != -1) {
      float volts = ((float)(WAVE_HEIGHT - markerY)) * (5.0 / WAVE_HEIGHT);
      char buf[10];
      dtostrf(volts, 0, 2, buf);
      tft.drawString(String(buf) + "V", tft.width() - 2, WAVE_HEIGHT + BOTTOM_SPACE / 2);
    } else if (markerX != -1) {
      int timeMs = markerX * SAMPLE_INTERVAL;
      char buf[20];
      sprintf(buf, "%dms", timeMs);
      tft.drawString(String(buf), tft.width() - 2, WAVE_HEIGHT + BOTTOM_SPACE / 2);
    }
  }
}

// przywróć stan po markerze Y
void restoreMarkerYLine(int y) {
  if (y < 0 || y > WAVE_HEIGHT) return;

  for (int x = 0; x < BUFOR_SIZE; x++) {
    uint16_t color = TFT_BLACK;
    if (y % GRID_SIZE == 0 || x % GRID_SIZE == 0) {
      color = TFT_DARKGREY;
    }

    int y0 = WAVE_HEIGHT - bufor[x];
    int y1 = (x < BUFOR_SIZE - 1) ? (WAVE_HEIGHT - bufor[x + 1]) : y0;

    if ((y == y0 && y == y1) || (y >= min(y0, y1) && y <= max(y0, y1))) {
      color = TFT_BLUE;
    }

    tft.drawPixel(x, y, color);
  }
}

// rysuj marker Y
void drawMarkerY(int y) {
  if (y < 0 || y > WAVE_HEIGHT) return;

  // czerwony marker Y
  if (isDelta && deltaMarkerY != -1 && deltaMarkerY != y) {
    tft.drawFastHLine(0, deltaMarkerY, tft.width(), TFT_RED);
  }

  // żółty marker Y
  tft.drawFastHLine(0, y, tft.width(), TFT_YELLOW);
}

// przywróć stan po markerze X
void restoreMarkerXLine(int x) {
  if (x < 0 || x >= BUFOR_SIZE) return;

  int y0 = WAVE_HEIGHT - bufor[x];
  int y1 = (x < BUFOR_SIZE - 1) ? (WAVE_HEIGHT - bufor[x + 1]) : y0;

  for (int y = 0; y <= WAVE_HEIGHT; y++) {
    uint16_t color = TFT_BLACK;
    if (y % GRID_SIZE == 0 || x % GRID_SIZE == 0) {
      color = TFT_DARKGREY;
    }
    if ((y == y0 && y == y1) || (y >= min(y0, y1) && y <= max(y0, y1))) {
      color = TFT_BLUE;
    }
    tft.drawPixel(x, y, color);
  }
}

// rysuj marker X
void drawMarkerX(int x) {
  if (x < 0 || x >= BUFOR_SIZE) return;

  // czerwony marker X
  if (isDelta && deltaMarkerX != -1 && deltaMarkerX != x) {
    tft.drawFastVLine(deltaMarkerX, 0, WAVE_HEIGHT, TFT_RED);
  }

  // żółty marker X
  tft.drawFastVLine(x, 0, WAVE_HEIGHT, TFT_YELLOW);
}

//------------------------------------------------------------------------------------------------------------------------------

void setup() {
  pinMode(TFT_BL, OUTPUT);
  pinMode(BUTTON_FREEZE, INPUT_PULLUP);
  pinMode(BUTTON_DELTA, INPUT_PULLUP);
  analogWrite(TFT_BL, 255);
  tft.init();
  tft.setRotation(1);
  drawGrid();

  for (int i = 0; i < BUFOR_SIZE; i++) {
    bufor[i] = 0;
    bufor2[i] = 0;
  }
}

//--------------------------------------------------------------------------------------------------------------------------------

void loop() {
  if (millis() - lastSampleTime < SAMPLE_INTERVAL) return;
  lastSampleTime = millis();

  // obsługa przycisku do zamrażania ekranu
  currentFreezeButtonState = digitalRead(BUTTON_FREEZE);
  if (lastFreezeButtonState == HIGH && currentFreezeButtonState == LOW) {
    isFrozen = !isFrozen;

    if (!isFrozen) {
      if (markerY != -1) {
        restoreMarkerYLine(markerY);
        markerY = -1;
      }
      if (markerX != -1) {
        restoreMarkerXLine(markerX);
        markerX = -1;
      }
    }
    bottomInfoNeedsUpdate = true;
  }
  lastFreezeButtonState = currentFreezeButtonState;

  // obsługa przycisku trybu delta
  currentDeltaButtonState = digitalRead(BUTTON_DELTA);
  if (lastDeltaButtonState == HIGH && currentDeltaButtonState == LOW && isFrozen) {
    isDelta = !isDelta;

    if (isDelta) {
  deltaMarkerX = markerX;
  deltaMarkerY = markerY;
  if (deltaMarkerX != -1) tft.drawFastVLine(deltaMarkerX, 0, WAVE_HEIGHT, TFT_RED);
  if (deltaMarkerY != -1) tft.drawFastHLine(0, deltaMarkerY, tft.width(), TFT_RED);
}
     else {
      // usuń czerwony marker delta
      if (deltaMarkerY != -1) {
        restoreDeltaMarkerYLine(deltaMarkerY);
      }
      if (deltaMarkerX != -1) {
        restoreDeltaMarkerXLine(deltaMarkerX);
      }
      deltaMarkerX = -1;
      deltaMarkerY = -1;
      deltaMarkerDrawn = false;
    }
    bottomInfoNeedsUpdate = true;
  }
  lastDeltaButtonState = currentDeltaButtonState;

  if (!isFrozen) {
    for (int i = 0; i < BUFOR_SIZE - 1; i++) {
      bufor[i] = bufor[i + 1];
    }
    value = analogRead(A5);
    bufor[BUFOR_SIZE - 1] = map(value, 0, 1023, 0, WAVE_HEIGHT);
  }


  // inicjalizacja potencjometrów od markerów
  int potValueY = analogRead(A4);
  int potValueX = analogRead(A3);
  bool markerChanged = false;

  // obsługa markera Y
  if (abs(potValueY - lastPotValueY) > 5) {
    lastPotValueY = potValueY;

    // usun marker X
    if (markerX != -1) {
      restoreMarkerXLine(markerX);
      markerX = -1;
    }

    // aktualizuj pozycje markera Y
    if (markerY != -1) restoreMarkerYLine(markerY);
    markerY = map(potValueY, 0, 1023, 0, WAVE_HEIGHT);
    drawMarkerY(markerY);
    lastMarkerMoveY = millis();
    markerChanged = true;
    bottomInfoNeedsUpdate = true;
  }

  // obsługa markera X
  if (abs(potValueX - lastPotValueX) > 5) {
    lastPotValueX = potValueX;

    // usun marker Y
    if (markerY != -1) {
      restoreMarkerYLine(markerY);
      markerY = -1;
    }

    // aktualizuj pozycje markera X
    if (markerX != -1) restoreMarkerXLine(markerX);
    markerX = map(potValueX, 0, 1023, 0, BUFOR_SIZE - 1);
    drawMarkerX(markerX);
    lastMarkerMoveX = millis();
    markerChanged = true;
    bottomInfoNeedsUpdate = true;
  }

  // usun marker Y po nieaktywności
  if (markerY != -1 && millis() - lastMarkerMoveY > markerTimeout && !markerChanged) {
    restoreMarkerYLine(markerY);
    markerY = -1;
    bottomInfoNeedsUpdate = true;
  }

  // usun marker X po nieaktywnosci
  if (markerX != -1 && millis() - lastMarkerMoveX > markerTimeout && !markerChanged) {
    restoreMarkerXLine(markerX);
    markerX = -1;
    bottomInfoNeedsUpdate = true;
  }

  // rysowanie wykresu 
  for (int i = 0; i < BUFOR_SIZE - 1; i++) {
    if (bufor[i] != bufor2[i] || bufor[i + 1] != bufor2[i + 1]) {
      int x0 = i;
      int y0 = WAVE_HEIGHT - bufor2[i];
      int x1 = i + 1;
      int y1 = WAVE_HEIGHT - bufor2[i + 1];

      // algorytm Bresenhama do wymazania starej linii
      int dx = abs(x1 - x0);
      int dy = -abs(y1 - y0);
      int sx = x0 < x1 ? 1 : -1;
      int sy = y0 < y1 ? 1 : -1;
      int err = dx + dy;

      while (true) {
        uint16_t eraseColor = ((x0 % GRID_SIZE == 0) || (y0 % GRID_SIZE == 0)) ? TFT_DARKGREY : TFT_BLACK;
        tft.drawPixel(x0, y0, eraseColor);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
      }

      // narysowanie nowej lini
      tft.drawLine(i, WAVE_HEIGHT - bufor[i], i + 1, WAVE_HEIGHT - bufor[i + 1], TFT_BLUE);
    }
  }

  // aktualizacja danych w buforach
  for (int i = 0; i < BUFOR_SIZE; i++) {
    bufor2[i] = bufor[i];
  }

  // rysuj marker jesli trzeba 
  if (markerY != -1) drawMarkerY(markerY);
  if (markerX != -1) drawMarkerX(markerX);

  if (isDelta) {
  if (deltaMarkerY != -1) tft.drawFastHLine(0, deltaMarkerY, tft.width(), TFT_RED);
  if (deltaMarkerX != -1) tft.drawFastVLine(deltaMarkerX, 0, WAVE_HEIGHT, TFT_RED);
}

  // aktualizuj dane jesli trzeba
  if (bottomInfoNeedsUpdate) {
    drawDynamicBottomInfo();
    bottomInfoNeedsUpdate = false;
  }
}