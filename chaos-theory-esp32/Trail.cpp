#include "Trail.hpp"

Trail::Trail() {
  for (int i = 0; i < LEN; ++i) {
    trail[i] = { -1, -1 };
  }
}

void Trail::setColor(AttractorColor color) {
  (*this).color = color;
}

void Trail::addPoint(Pt2d pt2d) {
  // do not filter out duplicates - makes the head and tail move speed independent
  // because results in a lot of duplicates, but are time exact.
  // also makes the trail a lot shorter so it needs tweaking of LEN

  // update only if the last has changed or have rechaed the timeout for same
  if ((lastPt != pt2d) || (lastPtRpt >= POINT_REPEAT_LIMIT)) {
    lastPt = pt2d;
    lastPtRpt = 0;

    trail[ptr] = pt2d;
    ptr = (ptr + 1) & (LEN - 1);
  } else {
    lastPtRpt++;
  }
}

// returns true if there is
void Trail::loadPixelsToBuff(uint16_t (&LCD_line)[LCD_BUFF_LINES][LCD_COL_PIX_CNT], uint16_t col) {
  // each time we need to go through the whole trail
  for (int i = 0; i < LEN; ++i) {
    // if we have a pixel in our trace for this col, add it
    if ((trail[i].x >= col) && (trail[i].x < col + LCD_BUFF_LINES)) {
      // add with shaded color represented by the trace age
      // we have to consider the current pointer's pos as the freshest
      // freshest = 0, oldest = LEN
      uint16_t age = (ptr - i - 1) & (LEN - 1);

      // make colors additive
      LCD_line[trail[i].x - col][trail[i].y] |= getLCDcolorForId(age, color);
    }
  }
}

uint16_t Trail::getLCDcolorForId(uint16_t age, AttractorColor color) {
  uint32_t retVal;  // we need it because of the multiplication

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
