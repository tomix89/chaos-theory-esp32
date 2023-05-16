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

struct pt3d {
  float x;
  float y;
  float z;
};

pt3d currPos_red;
pt3d currPos_green;
pt3d currPos_blue;

char sprintfBuff[128];

const float SPEED = 0.000025f;  // determines the speed of the simulation
const float ZOOM = 4.5f;
const float MAX_STEP_PX = 1.5f;  // in pixels, takes care of zoom depenency

float sigma = 10.0f;       // will be overwritten from sliders
float rho = 28.0f;         // will be overwritten from sliders
float beta = 8.0f / 3.0f;  // will be overwritten from sliders


float randFloat(float min, float max) {
  return random(min * 255.0f, max * 255.0f) / 255.0f;
}

void randomInitStartPos(pt3d* pos) {
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

  randomInitStartPos(&currPos_red);
  randomInitStartPos(&currPos_green);
  randomInitStartPos(&currPos_blue);


  sprintf(sprintfBuff, "pos red x: %f y: %f z: %f", currPos_red.x, currPos_red.y, currPos_red.z);
  Serial.println(sprintfBuff);

  sprintf(sprintfBuff, "pos grn x: %f y: %f z: %f", currPos_green.x, currPos_green.y, currPos_green.z);
  Serial.println(sprintfBuff);

  sprintf(sprintfBuff, "pos blu x: %f y: %f z: %f", currPos_blue.x, currPos_blue.y, currPos_blue.z);
  Serial.println(sprintfBuff);
}


// this is on purposly not a pointer as we need a copy
pt3d Lorenz(float dt, pt3d pos) {
  pt3d retVal;

  retVal.x = pos.x + dt * sigma * (pos.y - pos.x);
  retVal.y = pos.y + dt * (pos.x * (rho - pos.z) - pos.y);
  retVal.z = pos.z + dt * (pos.x * pos.y - beta * pos.z);

  return retVal;
}

float sqr(float val) {
  return powf(val, 2);
}


void updatePosition(float dt, pt3d* pos, uint16_t color) {
  // probably something went wrong
  if (dt < 1e-5) {
    // reset the values to some reasonable values
    randomInitStartPos(pos);

    //trailPoints.Clear();
    //trailStamps.Clear();
    return;
  }

  pt3d nextStep = Lorenz(dt, *pos);

  // check if the step is too big
  float step = sqrtf(sqr(pos->x - nextStep.x) + sqr(pos->y - nextStep.y) + sqr(pos->z - nextStep.z));

  // has to take in account the LCD draw ZOOM
  if (step * ZOOM > MAX_STEP_PX) {

    // make this step in 2 halves
    // sprintf(sprintfBuff, "step: %f dt: %f", step, 2 * SPEED / dt);
    // Serial.println(sprintfBuff);

    updatePosition(dt / 2.0f, pos, color);
    updatePosition(dt / 2.0f, pos, color);
    return;
  }

  pos->x = nextStep.x;
  pos->y = nextStep.y;
  pos->z = nextStep.z;

  // trails needs to be upadted in each iteration
  // trailStamps.Add(time); // it does not make much sense to slice the time inside one update
  // trailPoints.Add(new Vector3(x, y, z));


  // sprintf(sprintfBuff, "pos x: %f y: %f", x, y);
  // Serial.println(sprintfBuff);

  // convert to LCD resolution
  float x = pos->z * ZOOM;
  float y = pos->x * ZOOM * 1.5 + tft.height() / 2;
 
  // center the positions
  tft.drawPixel(x,
                y,
                color);
}


void loop() {
  updatePosition(SPEED, &currPos_red, ILI9341_RED);
  updatePosition(SPEED, &currPos_green, ILI9341_GREEN);
  updatePosition(SPEED, &currPos_blue, ILI9341_BLUE);

  //delay(5);
}
