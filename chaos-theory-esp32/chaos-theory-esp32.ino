#include "SPI.h"
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"

const int TFT_CS = 15;
const int TFT_DC = 4;
//const int TFT_MOSI = 23;
//const int TFT_SLK = 18;
//const int TFT_RST = 2;

#define ARRAY_SIZE(array) ((sizeof(array)) / (sizeof(array[0])))

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);
#define LDC_SPI_FREQ 40000000L  // 40MHz

unsigned long sumLCDDrawtime = 0;
unsigned long sumUpdatetime = 0;
unsigned long sumLCDLinestime = 0;

enum AttractorColor {
  CLR_UNKNOWN,
  CLR_RED,
  CLR_GREEN,
  CLR_BLUE
};

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

uint16_t oneLineBuff[240];
char sprintfBuff[128];

class Trail {

public:

  Trail() {
    for (int i = 0; i < LEN; ++i) {
      trail[i] = { -1, -1 };
    }
  }

  void setColor(AttractorColor color) {
    (*this).color = color;
  }

  void addPoint(Pt2d pt2d) {
    // do not filter out duplicates - makes the head and tail move speed independent
    // because results in a lot of duplicates, but are time exact.
    // also makes the trail a lot shorter so it needs tweaking of LEN

    // update only if the last has changed or have rechaed the timeout for same
    if ((lastPt != pt2d) || (lastPtRpt >= POINT_REPEAT_LIMIT)) {
      lastPt = pt2d;
      lastPtRpt = 0;

      // sprintf(sprintfBuff, "addPoint() ptr: %d x: %d y: %d", ptr, pt2d.x, pt2d.y);
      // Serial.println(sprintfBuff);

      trail[ptr] = pt2d;
      ptr = (ptr + 1) & (LEN - 1);
    } else {
      lastPtRpt++;
    }
  }

  // returns true if there is
  void loadPixelsToBuff(uint16_t* LCD_line, uint16_t col) {
    // each time we need to go through the whole trail
    for (int i = 0; i < LEN; ++i) {
      // if we have a pixel in our trace for this col, add it
      if (trail[i].x == col) {
        // add with shaded color represented by the trace age
        // we have to consider the current pointer's pos as the freshest
        // freshest = 0, oldest = LEN
        uint16_t age = (ptr - i) & (LEN - 1);


        // sprintf(sprintfBuff, "loadPixelsToBuff() i: %d ptr: %d age: %d col: %d y: %d", i, ptr, age, col, trail[i].y);
        // Serial.println(sprintfBuff);


        // make colors additive
        LCD_line[trail[i].y] |= getLCDcolorForId(age, color);
      }
    }
  }

private:
  static const int LEN_BITS = 11;
  static const int LEN = pow(2, LEN_BITS);
  Pt2d trail[LEN];
  Pt2d lastPt{ -1, -1 };
  // make it -1 then the tail and head will have it's own independent 'real' speed, but the line will be a lot shorter
  // making it 1,2,3 makes the tail move less and less 'real', e.g. more dependent on the head's spead
  // making it bigger than 1024 is basically making the tail speed dependent almost entirely on the head speed - e.g. each point is unique - max visible length
  const int16_t POINT_REPEAT_LIMIT = -1;
  int lastPtRpt = 0;
  uint16_t ptr = 0;
  AttractorColor color;

  uint16_t getLCDcolorForId(uint16_t age, AttractorColor color) {
    uint32_t retVal;  // we need it because of the multiplication

    age -= 1;  // to match the zero based indexing

    switch (color) {
      // 5 bit clr
      case CLR_RED:
        retVal = 0x1F * (LEN - age) / LEN;
        retVal = (retVal << 11) & ILI9341_RED;  // shift it to RED position of the 565 color
        //retVal = ILI9341_RED;
        break;

      case CLR_GREEN:
        retVal = 0x3F * (LEN - age) / LEN;
        retVal = (retVal << 5) & ILI9341_GREEN;  // shift it to GREEN position of the 565 color
        //retVal = ILI9341_GREEN;
        break;

      case CLR_BLUE:
        retVal = 0x1F * (LEN - age) / LEN;
        retVal &= ILI9341_BLUE;  // BLUE does not need a shift in the 565 color
        //retVal = ILI9341_BLUE;
        break;
    }

    return (uint16_t)retVal;
  }
};

struct Attractor {

  Attractor(AttractorColor color) {
    trail.setColor(color);
  }

  Pt3d currPos;
  Trail trail;
  Pt2d lastClearedPix;
};

Attractor attractor_red(CLR_RED);
Attractor attractor_green(CLR_GREEN);
Attractor attractor_blue(CLR_BLUE);

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

    // Serial.println("---SPLIT");
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
  for (int col = 0; col < 320; ++col) {

    unsigned long start = micros();
    memset(oneLineBuff, 0x00, sizeof(oneLineBuff));

    attractor_red.trail.loadPixelsToBuff(oneLineBuff, col);
    attractor_green.trail.loadPixelsToBuff(oneLineBuff, col);
    attractor_blue.trail.loadPixelsToBuff(oneLineBuff, col);
    unsigned long end = micros();
    sumLCDLinestime += end - start;

    start = micros();
    tft.writePixels(&oneLineBuff[0], 240);
    end = micros();
    sumLCDDrawtime += end - start;
  }

  tft.endWrite();
}

int cntr = 0;
int itr = 0;

void loop() {

  unsigned long start = micros();
  updatePosition(SPEED, &attractor_red);
  updatePosition(SPEED, &attractor_green);
  updatePosition(SPEED, &attractor_blue);
  unsigned long end = micros();
  sumUpdatetime += end - start;


  updateScreen();

  cntr++;
  if (cntr > 25) {
    cntr = 0;
    itr++;
    sprintf(sprintfBuff, "-------[%d]\nsumUpdatetime: %f\nsumLCDDrawtime: %f\nsumLCDLinestime: %f", itr, sumUpdatetime / 1000.0f, sumLCDDrawtime / 1000.0f, sumLCDLinestime / 1000.0f);
    Serial.println(sprintfBuff);
  }
}
