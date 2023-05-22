#include "SPI.h"
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"
#include "Trail.hpp"

const int TFT_CS = 15;
const int TFT_DC = 4;
//const int TFT_MOSI = 23;
//const int TFT_SLK = 18;
//const int TFT_RST = 2;

#define ARRAY_SIZE(array) ((sizeof(array)) / (sizeof(array[0])))

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);
#define LDC_SPI_FREQ 40000000L  // 40MHz

struct Pt3d {
  float x;
  float y;
  float z;
};


uint16_t oneLineBuff[LCD_BUFF_LINES][LCD_COL_PIX_CNT];

struct Attractor {

  Attractor(Trail::AttractorColor color) {
    trail.setColor(color);
  }

  Pt3d currPos;
  Trail trail;
  Pt2d lastClearedPix;
};

Attractor attractor_red(Trail::CLR_RED);
Attractor attractor_green(Trail::CLR_GREEN);
Attractor attractor_blue(Trail::CLR_BLUE);

const float SPEED = 0.05f;  // determines the speed of the simulation
const float ZOOM = 5.2f;
const float MAX_STEP_PX = 0.49f;  // in pixels, takes care of zoom depenency

float sigma = 10.0f;       // will be overwritten from sliders
float rho = 28.0f;         // will be overwritten from sliders
float beta = 8.0f / 3.0f;  // will be overwritten from sliders

float randFloat(float min, float max) {
  return random(min * 255.0f, max * 255.0f) / 255.0f;
}

void randomInitStartPos(Pt3d* pos) {
  pos->x = randFloat(-10, 10);
  pos->y = randFloat(-10, 10);
  pos->z = randFloat(1, 25);
}

void setup() {
  Serial.begin(115200);
  Serial.println("ILI9341 Test!");

  tft.begin(LDC_SPI_FREQ);

  // read diagnostics (optional but can help debug problems)
  uint8_t x = tft.readcommand8(ILI9341_RDMODE);
  Serial.print("Display Power Mode: 0x");
  Serial.println(x, HEX);
  x = tft.readcommand8(ILI9341_RDMADCTL);
  Serial.print("MADCTL Mode: 0x");
  Serial.println(x, HEX);
  x = tft.readcommand8(ILI9341_RDPIXFMT);
  Serial.print("Pixel Format: 0x");
  Serial.println(x, HEX);
  x = tft.readcommand8(ILI9341_RDIMGFMT);
  Serial.print("Image Format: 0x");
  Serial.println(x, HEX);
  x = tft.readcommand8(ILI9341_RDSELFDIAG);
  Serial.print("Self Diagnostic: 0x");
  Serial.println(x, HEX);

  tft.setRotation(0);  // rotation 0 is the col by col mode
  tft.fillScreen(ILI9341_BLACK);

  randomInitStartPos(&attractor_red.currPos);
  randomInitStartPos(&attractor_green.currPos);
  randomInitStartPos(&attractor_blue.currPos);
}


// this is on purposly not a pointer as we need a copy
Pt3d Lorenz(float dt, Pt3d pos) {
  Pt3d retVal;

  retVal.x = pos.x + dt * sigma * (pos.y - pos.x);
  retVal.y = pos.y + dt * (pos.x * (rho - pos.z) - pos.y);
  retVal.z = pos.z + dt * (pos.x * pos.y - beta * pos.z);

  return retVal;
}

float sqr(float val) {
  return powf(val, 2);
}


void updatePosition(float dt, Attractor* attractor) {
  // probably something went wrong
  if (dt < 1e-5) {
    // reset the values to some reasonable values
    randomInitStartPos(&attractor->currPos);

    //trailPoints.Clear();
    //trailStamps.Clear();
    return;
  }

  Pt3d nextStep = Lorenz(dt, attractor->currPos);

  // check if the step is too big
  float step = sqrtf(sqr(attractor->currPos.x - nextStep.x) + sqr(attractor->currPos.y - nextStep.y) + sqr(attractor->currPos.z - nextStep.z));

  // has to take in account the LCD draw ZOOM
  if (step * ZOOM > MAX_STEP_PX) {
    // make this step in 2 halves
    updatePosition(dt / 2.0f, attractor);
    updatePosition(dt / 2.0f, attractor);

    return;
  }

  attractor->currPos.x = nextStep.x;
  attractor->currPos.y = nextStep.y;
  attractor->currPos.z = nextStep.z;

  // convert to LCD resolution
  Pt2d currPix;
  currPix.y = attractor->currPos.z * ZOOM;
  currPix.x = attractor->currPos.x * ZOOM * 1.5 + tft.height() / 2;

  // update the trail
  attractor->trail.addPoint(currPix);
}

void updateScreen() {
  tft.startWrite();
  // write col by col
  for (int col = 0; col < 320; col += LCD_BUFF_LINES) {

    memset(oneLineBuff, 0x00, sizeof(oneLineBuff));
    attractor_red.trail.loadPixelsToBuff(oneLineBuff, col);
    attractor_green.trail.loadPixelsToBuff(oneLineBuff, col);
    attractor_blue.trail.loadPixelsToBuff(oneLineBuff, col);

    // does not need a for cycle as data are correctly arranged
    // we can print the whole array in one go
    tft.writePixels(&oneLineBuff[0][0], LCD_BUFF_LINES * LCD_COL_PIX_CNT);
  }

  tft.endWrite();
}

void loop() {
  updatePosition(SPEED, &attractor_red);
  updatePosition(SPEED, &attractor_green);
  updatePosition(SPEED, &attractor_blue);

  updateScreen();
}
