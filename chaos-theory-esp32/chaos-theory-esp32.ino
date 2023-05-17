#include "SPI.h"
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"

const int TFT_CS = 15;
const int TFT_DC = 4;
//const int TFT_MOSI = 23;
//const int TFT_SLK = 18;
//const int TFT_RST = 2;

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);
#define LDC_SPI_FREQ 40000000L  // 40MHz

struct Pt3d {
  float x;
  float y;
  float z;
};

struct Pt2d {
  uint16_t x;
  uint16_t y;

  bool operator==(const Pt2d& other) {
    return (x == other.x) && (y == other.y);
  }

  bool operator!=(const Pt2d& other) {
    return !(*this == other);
  }
};

char sprintfBuff[128];

class Trail {

public:
  void addPoint(Pt2d pt2d) {
    // update only if the last has changed
    if (lastPt != pt2d) {
      lastPt = pt2d;
      ptr = (ptr + 1) & (LEN - 1);

      trail[ptr] = pt2d;
    }
  }

  Pt2d* getTailPt() {
    // the oldest element is just ahead of the pointer
    uint16_t tailPtr = (ptr + 1) & (LEN - 1);
    return &trail[tailPtr];
  }

private:
  static const int LEN_BITS = 11;
  static const int LEN = pow(2, LEN_BITS);
  Pt2d trail[LEN];
  Pt2d lastPt{ -1, -1 };
  uint16_t ptr = 0;
};

struct Attractor {
  Attractor(uint16_t color) {
    (*this).color = color;
  }

  Pt3d currPos;
  Trail trail;
  Pt2d lastClearedPix;
  uint16_t color;
};

Attractor attractor_red(ILI9341_RED);
Attractor attractor_green(ILI9341_GREEN);
Attractor attractor_blue(ILI9341_BLUE);

const float SPEED = 0.00005f;  // determines the speed of the simulation
const float ZOOM = 5.2f;
const float MAX_STEP_PX = 1.5f;  // in pixels, takes care of zoom depenency

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
  tft.fillScreen(0x0000);

  randomInitStartPos(&attractor_red.currPos);
  randomInitStartPos(&attractor_green.currPos);
  randomInitStartPos(&attractor_blue.currPos);

  /*
  sprintf(sprintfBuff, "pos red x: %f y: %f z: %f", currPos_red.x, currPos_red.y, currPos_red.z);
  Serial.println(sprintfBuff);

  sprintf(sprintfBuff, "pos grn x: %f y: %f z: %f", currPos_green.x, currPos_green.y, currPos_green.z);
  Serial.println(sprintfBuff);

  sprintf(sprintfBuff, "pos blu x: %f y: %f z: %f", currPos_blue.x, currPos_blue.y, currPos_blue.z);
  Serial.println(sprintfBuff);
  */
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
    // sprintf(sprintfBuff, "step: %f dt: %f", step, 2 * SPEED / dt);
    // Serial.println(sprintfBuff);

    updatePosition(dt / 2.0f, attractor);
    updatePosition(dt / 2.0f, attractor);
    return;
  }

  attractor->currPos.x = nextStep.x;
  attractor->currPos.y = nextStep.y;
  attractor->currPos.z = nextStep.z;

  // trails needs to be upadted in each iteration
  // first delete the last point on screen
  // but only if it was not already deleted before
  Pt2d* tailPt = attractor->trail.getTailPt();
  if (attractor->lastClearedPix != *tailPt) {
    attractor->lastClearedPix = *tailPt;
    tft.drawPixel(tailPt->x,
                  tailPt->y,
                  ILI9341_BLACK);
  }

  /*
  sprintf(sprintfBuff, "tailPt x: %d y: %d", tailPt.x, tailPt.y);
  Serial.println(sprintfBuff);
*/


  // convert to LCD resolution
  Pt2d currPix;
  currPix.x = attractor->currPos.z * ZOOM;
  currPix.y = attractor->currPos.x * ZOOM * 1.5 + tft.height() / 2;


  // print the new point
  // for timing consistency we need to always print a point
  // even if we were printing the exact same before
  // other ways the speed is uncontrollable as the print itself is making up significant time to do
  attractor->trail.addPoint(currPix);
  tft.drawPixel(currPix.x,
                currPix.y,
                attractor->color);
}

void loop() {
  updatePosition(SPEED, &attractor_red);
  updatePosition(SPEED, &attractor_green);
  updatePosition(SPEED, &attractor_blue);

  //delay(5);
}
