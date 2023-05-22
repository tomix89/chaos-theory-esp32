#include "stdint.h"
#include "math.h"
#include "Adafruit_ILI9341.h"

#define LCD_COL_PIX_CNT 240
#define LCD_BUFF_LINES  64

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


class Trail {
public:

  Trail();

  enum AttractorColor {
    CLR_UNKNOWN,
    CLR_RED,
    CLR_GREEN,
    CLR_BLUE
  };

  void setColor(AttractorColor color);
  void addPoint(Pt2d pt2d);

  // returns true if there is
  void loadPixelsToBuff(uint16_t (&LCD_line)[LCD_BUFF_LINES][LCD_COL_PIX_CNT], uint16_t col);

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

  uint16_t getLCDcolorForId(uint16_t age, AttractorColor color);
};