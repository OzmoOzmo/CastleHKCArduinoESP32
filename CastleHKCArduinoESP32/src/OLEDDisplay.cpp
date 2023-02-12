
//A larger display library reduced in size to 3 files - AClarke Feb 2023

/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 by ThingPulse, Daniel Eichhorn
 * Copyright (c) 2018 by Fabrice Weinberg
 * Copyright (c) 2019 by Helmut Tschemernjak - www.radioshuttle.de
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * ThingPulse invests considerable time and money to develop these open source libraries.
 * Please support us by buying our products (and not the clones) from
 * https://thingpulse.com
 *
 */

/*
 * TODO Helmut
 * - test/finish dislplay.printf() on mbed-os
 * - Finish _putc with drawLogBuffer when running display
 */

#include "OLEDDisplay.h"

OLEDDisplay::OLEDDisplay()
{

  displayWidth = 128;
  displayHeight = 64;
  displayBufferSize = displayWidth * displayHeight / 8;
  color = WHITE;
  geometry = GEOMETRY_128_64;
  textAlignment = TEXT_ALIGN_LEFT;
  fontData = ArialMT_Plain_10;
  fontTableLookupFunction = DefaultFontTableLookup;
  buffer = NULL;
#ifdef OLEDDISPLAY_DOUBLE_BUFFER
  buffer_back = NULL;
#endif
}

OLEDDisplay::~OLEDDisplay()
{
  end();
}

bool OLEDDisplay::allocateBuffer()
{

  logBufferSize = 0;
  logBufferFilled = 0;
  logBufferLine = 0;
  logBufferMaxLines = 0;
  logBuffer = NULL;

  if (!connect())
  {
    DEBUG_OLEDDISPLAY("[OLEDDISPLAY][init] Can't establish connection to display\n");
    return false;
  }

  if (this->buffer == NULL)
  {
    this->buffer = (uint8_t *)malloc((sizeof(uint8_t) * displayBufferSize) + BufferOffset);
    this->buffer += BufferOffset;

    if (!this->buffer)
    {
      DEBUG_OLEDDISPLAY("[OLEDDISPLAY][init] Not enough memory to create display\n");
      return false;
    }
  }

#ifdef OLEDDISPLAY_DOUBLE_BUFFER
  if (this->buffer_back == NULL)
  {
    this->buffer_back = (uint8_t *)malloc((sizeof(uint8_t) * displayBufferSize) + BufferOffset);
    this->buffer_back += BufferOffset;

    if (!this->buffer_back)
    {
      DEBUG_OLEDDISPLAY("[OLEDDISPLAY][init] Not enough memory to create back buffer\n");
      free(this->buffer - BufferOffset);
      return false;
    }
  }
#endif

  return true;
}

bool OLEDDisplay::init()
{

  BufferOffset = getBufferOffset();

  if (!allocateBuffer())
  {
    return false;
  }

  sendInitCommands();
  resetDisplay();

  return true;
}

void OLEDDisplay::end()
{
  if (this->buffer)
  {
    free(this->buffer - BufferOffset);
    this->buffer = NULL;
  }
#ifdef OLEDDISPLAY_DOUBLE_BUFFER
  if (this->buffer_back)
  {
    free(this->buffer_back - BufferOffset);
    this->buffer_back = NULL;
  }
#endif
  if (this->logBuffer != NULL)
  {
    free(this->logBuffer);
    this->logBuffer = NULL;
  }
}

void OLEDDisplay::resetDisplay(void)
{
  clear();
#ifdef OLEDDISPLAY_DOUBLE_BUFFER
  memset(buffer_back, 1, displayBufferSize);
#endif
  display();
}

void OLEDDisplay::setColor(OLEDDISPLAY_COLOR color)
{
  this->color = color;
}

OLEDDISPLAY_COLOR OLEDDisplay::getColor()
{
  return this->color;
}

void OLEDDisplay::setPixel(int16_t x, int16_t y)
{
  if (x >= 0 && x < this->width() && y >= 0 && y < this->height())
  {
    switch (color)
    {
    case WHITE:
      buffer[x + (y / 8) * this->width()] |= (1 << (y & 7));
      break;
    case BLACK:
      buffer[x + (y / 8) * this->width()] &= ~(1 << (y & 7));
      break;
    case INVERSE:
      buffer[x + (y / 8) * this->width()] ^= (1 << (y & 7));
      break;
    }
  }
}

void OLEDDisplay::setPixelColor(int16_t x, int16_t y, OLEDDISPLAY_COLOR color)
{
  if (x >= 0 && x < this->width() && y >= 0 && y < this->height())
  {
    switch (color)
    {
    case WHITE:
      buffer[x + (y / 8) * this->width()] |= (1 << (y & 7));
      break;
    case BLACK:
      buffer[x + (y / 8) * this->width()] &= ~(1 << (y & 7));
      break;
    case INVERSE:
      buffer[x + (y / 8) * this->width()] ^= (1 << (y & 7));
      break;
    }
  }
}

void OLEDDisplay::clearPixel(int16_t x, int16_t y)
{
  if (x >= 0 && x < this->width() && y >= 0 && y < this->height())
  {
    switch (color)
    {
    case BLACK:
      buffer[x + (y >> 3) * this->width()] |= (1 << (y & 7));
      break;
    case WHITE:
      buffer[x + (y >> 3) * this->width()] &= ~(1 << (y & 7));
      break;
    case INVERSE:
      buffer[x + (y >> 3) * this->width()] ^= (1 << (y & 7));
      break;
    }
  }
}

// Bresenham's algorithm - thx wikipedia and Adafruit_GFX
void OLEDDisplay::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
  int16_t steep = abs(y1 - y0) > abs(x1 - x0);
  if (steep)
  {
    _swap_int16_t(x0, y0);
    _swap_int16_t(x1, y1);
  }

  if (x0 > x1)
  {
    _swap_int16_t(x0, x1);
    _swap_int16_t(y0, y1);
  }

  int16_t dx, dy;
  dx = x1 - x0;
  dy = abs(y1 - y0);

  int16_t err = dx / 2;
  int16_t ystep;

  if (y0 < y1)
  {
    ystep = 1;
  }
  else
  {
    ystep = -1;
  }

  for (; x0 <= x1; x0++)
  {
    if (steep)
    {
      setPixel(y0, x0);
    }
    else
    {
      setPixel(x0, y0);
    }
    err -= dy;
    if (err < 0)
    {
      y0 += ystep;
      err += dx;
    }
  }
}

void OLEDDisplay::drawRect(int16_t x, int16_t y, int16_t width, int16_t height)
{
  drawHorizontalLine(x, y, width);
  drawVerticalLine(x, y, height);
  drawVerticalLine(x + width - 1, y, height);
  drawHorizontalLine(x, y + height - 1, width);
}

void OLEDDisplay::fillRect(int16_t xMove, int16_t yMove, int16_t width, int16_t height)
{
  for (int16_t x = xMove; x < xMove + width; x++)
  {
    drawVerticalLine(x, yMove, height);
  }
}

void OLEDDisplay::drawCircle(int16_t x0, int16_t y0, int16_t radius)
{
  int16_t x = 0, y = radius;
  int16_t dp = 1 - radius;
  do
  {
    if (dp < 0)
      dp = dp + (x++) * 2 + 3;
    else
      dp = dp + (x++) * 2 - (y--) * 2 + 5;

    setPixel(x0 + x, y0 + y); // For the 8 octants
    setPixel(x0 - x, y0 + y);
    setPixel(x0 + x, y0 - y);
    setPixel(x0 - x, y0 - y);
    setPixel(x0 + y, y0 + x);
    setPixel(x0 - y, y0 + x);
    setPixel(x0 + y, y0 - x);
    setPixel(x0 - y, y0 - x);

  } while (x < y);

  setPixel(x0 + radius, y0);
  setPixel(x0, y0 + radius);
  setPixel(x0 - radius, y0);
  setPixel(x0, y0 - radius);
}

void OLEDDisplay::drawCircleQuads(int16_t x0, int16_t y0, int16_t radius, uint8_t quads)
{
  int16_t x = 0, y = radius;
  int16_t dp = 1 - radius;
  while (x < y)
  {
    if (dp < 0)
      dp = dp + (x++) * 2 + 3;
    else
      dp = dp + (x++) * 2 - (y--) * 2 + 5;
    if (quads & 0x1)
    {
      setPixel(x0 + x, y0 - y);
      setPixel(x0 + y, y0 - x);
    }
    if (quads & 0x2)
    {
      setPixel(x0 - y, y0 - x);
      setPixel(x0 - x, y0 - y);
    }
    if (quads & 0x4)
    {
      setPixel(x0 - y, y0 + x);
      setPixel(x0 - x, y0 + y);
    }
    if (quads & 0x8)
    {
      setPixel(x0 + x, y0 + y);
      setPixel(x0 + y, y0 + x);
    }
  }
  if (quads & 0x1 && quads & 0x8)
  {
    setPixel(x0 + radius, y0);
  }
  if (quads & 0x4 && quads & 0x8)
  {
    setPixel(x0, y0 + radius);
  }
  if (quads & 0x2 && quads & 0x4)
  {
    setPixel(x0 - radius, y0);
  }
  if (quads & 0x1 && quads & 0x2)
  {
    setPixel(x0, y0 - radius);
  }
}

void OLEDDisplay::fillCircle(int16_t x0, int16_t y0, int16_t radius)
{
  int16_t x = 0, y = radius;
  int16_t dp = 1 - radius;
  do
  {
    if (dp < 0)
      dp = dp + (x++) * 2 + 3;
    else
      dp = dp + (x++) * 2 - (y--) * 2 + 5;

    drawHorizontalLine(x0 - x, y0 - y, 2 * x);
    drawHorizontalLine(x0 - x, y0 + y, 2 * x);
    drawHorizontalLine(x0 - y, y0 - x, 2 * y);
    drawHorizontalLine(x0 - y, y0 + x, 2 * y);

  } while (x < y);
  drawHorizontalLine(x0 - radius, y0, 2 * radius);
}

void OLEDDisplay::drawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                               int16_t x2, int16_t y2)
{
  drawLine(x0, y0, x1, y1);
  drawLine(x1, y1, x2, y2);
  drawLine(x2, y2, x0, y0);
}

void OLEDDisplay::fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                               int16_t x2, int16_t y2)
{
  int16_t a, b, y, last;

  if (y0 > y1)
  {
    _swap_int16_t(y0, y1);
    _swap_int16_t(x0, x1);
  }
  if (y1 > y2)
  {
    _swap_int16_t(y2, y1);
    _swap_int16_t(x2, x1);
  }
  if (y0 > y1)
  {
    _swap_int16_t(y0, y1);
    _swap_int16_t(x0, x1);
  }

  if (y0 == y2)
  {
    a = b = x0;
    if (x1 < a)
    {
      a = x1;
    }
    else if (x1 > b)
    {
      b = x1;
    }
    if (x2 < a)
    {
      a = x2;
    }
    else if (x2 > b)
    {
      b = x2;
    }
    drawHorizontalLine(a, y0, b - a + 1);
    return;
  }

  int16_t
      dx01 = x1 - x0,
      dy01 = y1 - y0,
      dx02 = x2 - x0,
      dy02 = y2 - y0,
      dx12 = x2 - x1,
      dy12 = y2 - y1;
  int32_t
      sa = 0,
      sb = 0;

  if (y1 == y2)
  {
    last = y1; // Include y1 scanline
  }
  else
  {
    last = y1 - 1; // Skip it
  }

  for (y = y0; y <= last; y++)
  {
    a = x0 + sa / dy01;
    b = x0 + sb / dy02;
    sa += dx01;
    sb += dx02;

    if (a > b)
    {
      _swap_int16_t(a, b);
    }
    drawHorizontalLine(a, y, b - a + 1);
  }

  sa = dx12 * (y - y1);
  sb = dx02 * (y - y0);
  for (; y <= y2; y++)
  {
    a = x1 + sa / dy12;
    b = x0 + sb / dy02;
    sa += dx12;
    sb += dx02;

    if (a > b)
    {
      _swap_int16_t(a, b);
    }
    drawHorizontalLine(a, y, b - a + 1);
  }
}

void OLEDDisplay::drawHorizontalLine(int16_t x, int16_t y, int16_t length)
{
  if (y < 0 || y >= this->height())
  {
    return;
  }

  if (x < 0)
  {
    length += x;
    x = 0;
  }

  if ((x + length) > this->width())
  {
    length = (this->width() - x);
  }

  if (length <= 0)
  {
    return;
  }

  uint8_t *bufferPtr = buffer;
  bufferPtr += (y >> 3) * this->width();
  bufferPtr += x;

  uint8_t drawBit = 1 << (y & 7);

  switch (color)
  {
  case WHITE:
    while (length--)
    {
      *bufferPtr++ |= drawBit;
    };
    break;
  case BLACK:
    drawBit = ~drawBit;
    while (length--)
    {
      *bufferPtr++ &= drawBit;
    };
    break;
  case INVERSE:
    while (length--)
    {
      *bufferPtr++ ^= drawBit;
    };
    break;
  }
}

void OLEDDisplay::drawVerticalLine(int16_t x, int16_t y, int16_t length)
{
  if (x < 0 || x >= this->width())
    return;

  if (y < 0)
  {
    length += y;
    y = 0;
  }

  if ((y + length) > this->height())
  {
    length = (this->height() - y);
  }

  if (length <= 0)
    return;

  uint8_t yOffset = y & 7;
  uint8_t drawBit;
  uint8_t *bufferPtr = buffer;

  bufferPtr += (y >> 3) * this->width();
  bufferPtr += x;

  if (yOffset)
  {
    yOffset = 8 - yOffset;
    drawBit = ~(0xFF >> (yOffset));

    if (length < yOffset)
    {
      drawBit &= (0xFF >> (yOffset - length));
    }

    switch (color)
    {
    case WHITE:
      *bufferPtr |= drawBit;
      break;
    case BLACK:
      *bufferPtr &= ~drawBit;
      break;
    case INVERSE:
      *bufferPtr ^= drawBit;
      break;
    }

    if (length < yOffset)
      return;

    length -= yOffset;
    bufferPtr += this->width();
  }

  if (length >= 8)
  {
    switch (color)
    {
    case WHITE:
    case BLACK:
      drawBit = (color == WHITE) ? 0xFF : 0x00;
      do
      {
        *bufferPtr = drawBit;
        bufferPtr += this->width();
        length -= 8;
      } while (length >= 8);
      break;
    case INVERSE:
      do
      {
        *bufferPtr = ~(*bufferPtr);
        bufferPtr += this->width();
        length -= 8;
      } while (length >= 8);
      break;
    }
  }

  if (length > 0)
  {
    drawBit = (1 << (length & 7)) - 1;
    switch (color)
    {
    case WHITE:
      *bufferPtr |= drawBit;
      break;
    case BLACK:
      *bufferPtr &= ~drawBit;
      break;
    case INVERSE:
      *bufferPtr ^= drawBit;
      break;
    }
  }
}

void OLEDDisplay::drawProgressBar(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t progress)
{
  uint16_t radius = height / 2;
  uint16_t xRadius = x + radius;
  uint16_t yRadius = y + radius;
  uint16_t doubleRadius = 2 * radius;
  uint16_t innerRadius = radius - 2;

  setColor(WHITE);
  drawCircleQuads(xRadius, yRadius, radius, 0b00000110);
  drawHorizontalLine(xRadius, y, width - doubleRadius + 1);
  drawHorizontalLine(xRadius, y + height, width - doubleRadius + 1);
  drawCircleQuads(x + width - radius, yRadius, radius, 0b00001001);

  uint16_t maxProgressWidth = (width - doubleRadius + 1) * progress / 100;

  fillCircle(xRadius, yRadius, innerRadius);
  fillRect(xRadius + 1, y + 2, maxProgressWidth, height - 3);
  fillCircle(xRadius + maxProgressWidth, yRadius, innerRadius);
}

void OLEDDisplay::drawFastImage(int16_t xMove, int16_t yMove, int16_t width, int16_t height, const uint8_t *image)
{
  drawInternal(xMove, yMove, width, height, image, 0, 0);
}

void OLEDDisplay::drawXbm(int16_t xMove, int16_t yMove, int16_t width, int16_t height, const uint8_t *xbm)
{
  int16_t widthInXbm = (width + 7) / 8;
  uint8_t data = 0;

  for (int16_t y = 0; y < height; y++)
  {
    for (int16_t x = 0; x < width; x++)
    {
      if (x & 7)
      {
        data >>= 1; // Move a bit
      }
      else
      { // Read new data every 8 bit
        data = pgm_read_byte(xbm + (x / 8) + y * widthInXbm);
      }
      // if there is a bit draw it
      if (data & 0x01)
      {
        setPixel(xMove + x, yMove + y);
      }
    }
  }
}

void OLEDDisplay::drawIco16x16(int16_t xMove, int16_t yMove, const uint8_t *ico, bool inverse)
{
  uint16_t data;

  for (int16_t y = 0; y < 16; y++)
  {
    data = pgm_read_byte(ico + (y << 1)) + (pgm_read_byte(ico + (y << 1) + 1) << 8);
    for (int16_t x = 0; x < 16; x++)
    {
      if ((data & 0x01) ^ inverse)
      {
        setPixelColor(xMove + x, yMove + y, WHITE);
      }
      else
      {
        setPixelColor(xMove + x, yMove + y, BLACK);
      }
      data >>= 1; // Move a bit
    }
  }
}

uint16_t OLEDDisplay::drawStringInternal(int16_t xMove, int16_t yMove, const char *text, uint16_t textLength, uint16_t textWidth, bool utf8)
{
  uint8_t textHeight = pgm_read_byte(fontData + HEIGHT_POS);
  uint8_t firstChar = pgm_read_byte(fontData + FIRST_CHAR_POS);
  uint16_t sizeOfJumpTable = pgm_read_byte(fontData + CHAR_NUM_POS) * JUMPTABLE_BYTES;

  uint16_t cursorX = 0;
  uint16_t cursorY = 0;
  uint16_t charCount = 0;

  switch (textAlignment)
  {
  case TEXT_ALIGN_CENTER_BOTH:
    yMove -= textHeight >> 1;
  // Fallthrough
  case TEXT_ALIGN_CENTER:
    xMove -= textWidth >> 1; // divide by 2
    break;
  case TEXT_ALIGN_RIGHT:
    xMove -= textWidth;
    break;
  case TEXT_ALIGN_LEFT:
    break;
  }

  // Don't draw anything if it is not on the screen.
  if (xMove + textWidth < 0 || xMove >= this->width())
  {
    return 0;
  }
  if (yMove + textHeight < 0 || yMove >= this->height())
  {
    return 0;
  }

  for (uint16_t j = 0; j < textLength; j++)
  {
    int16_t xPos = xMove + cursorX;
    int16_t yPos = yMove + cursorY;
    if (xPos > this->width())
      break; // no need to continue
    charCount++;

    uint8_t code;
    if (utf8)
    {
      code = (this->fontTableLookupFunction)(text[j]);
      if (code == 0)
        continue;
    }
    else
      code = text[j];
    if (code >= firstChar)
    {
      uint8_t charCode = code - firstChar;

      // 4 Bytes per char code
      uint8_t msbJumpToChar = pgm_read_byte(fontData + JUMPTABLE_START + charCode * JUMPTABLE_BYTES);                      // MSB  \ JumpAddress
      uint8_t lsbJumpToChar = pgm_read_byte(fontData + JUMPTABLE_START + charCode * JUMPTABLE_BYTES + JUMPTABLE_LSB);      // LSB /
      uint8_t charByteSize = pgm_read_byte(fontData + JUMPTABLE_START + charCode * JUMPTABLE_BYTES + JUMPTABLE_SIZE);      // Size
      uint8_t currentCharWidth = pgm_read_byte(fontData + JUMPTABLE_START + charCode * JUMPTABLE_BYTES + JUMPTABLE_WIDTH); // Width

      // Test if the char is drawable
      if (!(msbJumpToChar == 255 && lsbJumpToChar == 255))
      {
        // Get the position of the char data
        uint16_t charDataPosition = JUMPTABLE_START + sizeOfJumpTable + ((msbJumpToChar << 8) + lsbJumpToChar);
        drawInternal(xPos, yPos, currentCharWidth, textHeight, fontData, charDataPosition, charByteSize);
      }

      cursorX += currentCharWidth;
    }
  }
  return charCount;
}

uint16_t OLEDDisplay::drawString(int16_t xMove, int16_t yMove, const String &strUser)
{
  uint16_t lineHeight = pgm_read_byte(fontData + HEIGHT_POS);

  // char* text must be freed!
  char *text = strdup(strUser.c_str());
  if (!text)
  {
    DEBUG_OLEDDISPLAY("[OLEDDISPLAY][drawString] Can't allocate char array.\n");
    return 0;
  }

  uint16_t yOffset = 0;
  // If the string should be centered vertically too
  // we need to now how heigh the string is.
  if (textAlignment == TEXT_ALIGN_CENTER_BOTH)
  {
    uint16_t lb = 0;
    // Find number of linebreaks in text
    for (uint16_t i = 0; text[i] != 0; i++)
    {
      lb += (text[i] == 10);
    }
    // Calculate center
    yOffset = (lb * lineHeight) / 2;
  }

  uint16_t charDrawn = 0;
  uint16_t line = 0;
  char *textPart = strtok(text, "\n");
  while (textPart != NULL)
  {
    uint16_t length = strlen(textPart);
    charDrawn += drawStringInternal(xMove, yMove - yOffset + (line++) * lineHeight, textPart, length, getStringWidth(textPart, length, true), true);
    textPart = strtok(NULL, "\n");
  }
  free(text);
  return charDrawn;
}

void OLEDDisplay::drawStringf(int16_t x, int16_t y, char *buffer, String format, ...)
{
  va_list myargs;
  va_start(myargs, format);
  vsprintf(buffer, format.c_str(), myargs);
  va_end(myargs);
  drawString(x, y, buffer);
}

uint16_t OLEDDisplay::drawStringMaxWidth(int16_t xMove, int16_t yMove, uint16_t maxLineWidth, const String &strUser)
{
  uint16_t firstChar = pgm_read_byte(fontData + FIRST_CHAR_POS);
  uint16_t lineHeight = pgm_read_byte(fontData + HEIGHT_POS);

  const char *text = strUser.c_str();

  uint16_t length = strlen(text);
  uint16_t lastDrawnPos = 0;
  uint16_t lineNumber = 0;
  uint16_t strWidth = 0;

  uint16_t preferredBreakpoint = 0;
  uint16_t widthAtBreakpoint = 0;
  uint16_t firstLineChars = 0;
  uint16_t drawStringResult = 1; // later tested for 0 == error, so initialize to 1

  for (uint16_t i = 0; i < length; i++)
  {
    char c = (this->fontTableLookupFunction)(text[i]);
    if (c == 0)
      continue;
    strWidth += pgm_read_byte(fontData + JUMPTABLE_START + (c - firstChar) * JUMPTABLE_BYTES + JUMPTABLE_WIDTH);

    // Always try to break on a space, dash or slash
    if (text[i] == ' ' || text[i] == '-' || text[i] == '/')
    {
      preferredBreakpoint = i + 1;
      widthAtBreakpoint = strWidth;
    }

    if (strWidth >= maxLineWidth)
    {
      if (preferredBreakpoint == 0)
      {
        preferredBreakpoint = i;
        widthAtBreakpoint = strWidth;
      }
      drawStringResult = drawStringInternal(xMove, yMove + (lineNumber++) * lineHeight, &text[lastDrawnPos], preferredBreakpoint - lastDrawnPos, widthAtBreakpoint, true);
      if (firstLineChars == 0)
        firstLineChars = preferredBreakpoint;
      lastDrawnPos = preferredBreakpoint;
      // It is possible that we did not draw all letters to i so we need
      // to account for the width of the chars from `i - preferredBreakpoint`
      // by calculating the width we did not draw yet.
      strWidth = strWidth - widthAtBreakpoint;
      preferredBreakpoint = 0;
      if (drawStringResult == 0) // we are past the display already?
        break;
    }
  }

  // Draw last part if needed
  if (drawStringResult != 0 && lastDrawnPos < length)
  {
    drawStringResult = drawStringInternal(xMove, yMove + (lineNumber++) * lineHeight, &text[lastDrawnPos], length - lastDrawnPos, getStringWidth(&text[lastDrawnPos], length - lastDrawnPos, true), true);
  }

  if (drawStringResult == 0 || (yMove + lineNumber * lineHeight) >= this->height()) // text did not fit on screen
    return firstLineChars;
  return 0; // everything was drawn
}

uint16_t OLEDDisplay::getStringWidth(const char *text, uint16_t length, bool utf8)
{
  uint16_t firstChar = pgm_read_byte(fontData + FIRST_CHAR_POS);

  uint16_t stringWidth = 0;
  uint16_t maxWidth = 0;

  for (uint16_t i = 0; i < length; i++)
  {
    char c = text[i];
    if (utf8)
    {
      c = (this->fontTableLookupFunction)(c);
      if (c == 0)
        continue;
    }
    stringWidth += pgm_read_byte(fontData + JUMPTABLE_START + (c - firstChar) * JUMPTABLE_BYTES + JUMPTABLE_WIDTH);
    if (c == 10)
    {
      maxWidth = max(maxWidth, stringWidth);
      stringWidth = 0;
    }
  }

  return max(maxWidth, stringWidth);
}

uint16_t OLEDDisplay::getStringWidth(const String &strUser)
{
  uint16_t width = getStringWidth(strUser.c_str(), strUser.length());
  return width;
}

void OLEDDisplay::setTextAlignment(OLEDDISPLAY_TEXT_ALIGNMENT textAlignment)
{
  this->textAlignment = textAlignment;
}

void OLEDDisplay::setFont(const uint8_t *fontData)
{
  this->fontData = fontData;
}

void OLEDDisplay::displayOn(void)
{
  sendCommand(DISPLAYON);
}

void OLEDDisplay::displayOff(void)
{
  sendCommand(DISPLAYOFF);
}

void OLEDDisplay::invertDisplay(void)
{
  sendCommand(INVERTDISPLAY);
}

void OLEDDisplay::normalDisplay(void)
{
  sendCommand(NORMALDISPLAY);
}

void OLEDDisplay::setContrast(uint8_t contrast, uint8_t precharge, uint8_t comdetect)
{
  sendCommand(SETPRECHARGE); // 0xD9
  sendCommand(precharge);    // 0xF1 default, to lower the contrast, put 1-1F
  sendCommand(SETCONTRAST);
  sendCommand(contrast);      // 0-255
  sendCommand(SETVCOMDETECT); // 0xDB, (additionally needed to lower the contrast)
  sendCommand(comdetect);     // 0x40 default, to lower the contrast, put 0
  sendCommand(DISPLAYALLON_RESUME);
  sendCommand(NORMALDISPLAY);
  sendCommand(DISPLAYON);
}

void OLEDDisplay::setBrightness(uint8_t brightness)
{
  uint8_t contrast = brightness;
  if (brightness < 128)
  {
    // Magic values to get a smooth/ step-free transition
    contrast = brightness * 1.171;
  }
  else
  {
    contrast = brightness * 1.171 - 43;
  }

  uint8_t precharge = 241;
  if (brightness == 0)
  {
    precharge = 0;
  }
  uint8_t comdetect = brightness / 8;

  setContrast(contrast, precharge, comdetect);
}

void OLEDDisplay::resetOrientation()
{
  sendCommand(SEGREMAP);
  sendCommand(COMSCANINC); // Reset screen rotation or mirroring
}

void OLEDDisplay::flipScreenVertically()
{
  sendCommand(SEGREMAP | 0x01);
  sendCommand(COMSCANDEC); // Rotate screen 180 Deg
}

void OLEDDisplay::mirrorScreen()
{
  sendCommand(SEGREMAP);
  sendCommand(COMSCANDEC); // Mirror screen
}

void OLEDDisplay::clear(void)
{
  memset(buffer, 0, displayBufferSize);
}

void OLEDDisplay::drawLogBuffer(uint16_t xMove, uint16_t yMove)
{
  uint16_t lineHeight = pgm_read_byte(fontData + HEIGHT_POS);
  // Always align left
  setTextAlignment(TEXT_ALIGN_LEFT);

  // State values
  uint16_t length = 0;
  uint16_t line = 0;
  uint16_t lastPos = 0;

  for (uint16_t i = 0; i < this->logBufferFilled; i++)
  {
    // Everytime we have a \n print
    if (this->logBuffer[i] == 10)
    {
      length++;
      // Draw string on line `line` from lastPos to length
      // Passing 0 as the lenght because we are in TEXT_ALIGN_LEFT
      drawStringInternal(xMove, yMove + (line++) * lineHeight, &this->logBuffer[lastPos], length, 0, false);
      // Remember last pos
      lastPos = i;
      // Reset length
      length = 0;
    }
    else
    {
      // Count chars until next linebreak
      length++;
    }
  }
  // Draw the remaining string
  if (length > 0)
  {
    drawStringInternal(xMove, yMove + line * lineHeight, &this->logBuffer[lastPos], length, 0, false);
  }
}

uint16_t OLEDDisplay::getWidth(void)
{
  return displayWidth;
}

uint16_t OLEDDisplay::getHeight(void)
{
  return displayHeight;
}

bool OLEDDisplay::setLogBuffer(uint16_t lines, uint16_t chars)
{
  if (logBuffer != NULL)
    free(logBuffer);
  uint16_t size = lines * chars;
  if (size > 0)
  {
    this->logBufferLine = 0;         // Lines printed
    this->logBufferFilled = 0;       // Nothing stored yet
    this->logBufferMaxLines = lines; // Lines max printable
    this->logBufferSize = size;      // Total number of characters the buffer can hold
    this->logBuffer = (char *)malloc(size * sizeof(uint8_t));
    if (!this->logBuffer)
    {
      DEBUG_OLEDDISPLAY("[OLEDDISPLAY][setLogBuffer] Not enough memory to create log buffer\n");
      return false;
    }
  }
  return true;
}

size_t OLEDDisplay::write(uint8_t c)
{
  if (this->logBufferSize > 0)
  {
    // Don't waste space on \r\n line endings, dropping \r
    if (c == 13)
      return 1;

    // convert UTF-8 character to font table index
    c = (this->fontTableLookupFunction)(c);
    // drop unknown character
    if (c == 0)
      return 1;

    bool maxLineNotReached = this->logBufferLine < this->logBufferMaxLines;
    bool bufferNotFull = this->logBufferFilled < this->logBufferSize;

    // Can we write to the buffer?
    if (bufferNotFull && maxLineNotReached)
    {
      this->logBuffer[logBufferFilled] = c;
      this->logBufferFilled++;
      // Keep track of lines written
      if (c == 10)
        this->logBufferLine++;
    }
    else
    {
      // Max line number is reached
      if (!maxLineNotReached)
        this->logBufferLine--;

      // Find the end of the first line
      uint16_t firstLineEnd = 0;
      for (uint16_t i = 0; i < this->logBufferFilled; i++)
      {
        if (this->logBuffer[i] == 10)
        {
          // Include last char too
          firstLineEnd = i + 1;
          break;
        }
      }
      // If there was a line ending
      if (firstLineEnd > 0)
      {
        // Calculate the new logBufferFilled value
        this->logBufferFilled = logBufferFilled - firstLineEnd;
        // Now we move the lines infront of the buffer
        memcpy(this->logBuffer, &this->logBuffer[firstLineEnd], logBufferFilled);
      }
      else
      {
        // Let's reuse the buffer if it was full
        if (!bufferNotFull)
        {
          this->logBufferFilled = 0;
        } // else {
        //  Nothing to do here
        //}
      }
      write(c);
    }
  }
  // We are always writing all uint8_t to the buffer
  return 1;
}

size_t OLEDDisplay::write(const char *str)
{
  if (str == NULL)
    return 0;
  size_t length = strlen(str);
  for (size_t i = 0; i < length; i++)
  {
    write(str[i]);
  }
  return length;
}

// Private functions
void OLEDDisplay::setGeometry(OLEDDISPLAY_GEOMETRY g, uint16_t width, uint16_t height)
{
  this->geometry = g;

  switch (g)
  {
  case GEOMETRY_128_64:
    this->displayWidth = 128;
    this->displayHeight = 64;
    break;
  case GEOMETRY_128_32:
    this->displayWidth = 128;
    this->displayHeight = 32;
    break;
  case GEOMETRY_64_48:
    this->displayWidth = 64;
    this->displayHeight = 48;
    break;
  case GEOMETRY_64_32:
    this->displayWidth = 64;
    this->displayHeight = 32;
    break;
  case GEOMETRY_RAWMODE:
    this->displayWidth = width > 0 ? width : 128;
    this->displayHeight = height > 0 ? height : 64;
    break;
  }
  this->displayBufferSize = displayWidth * displayHeight / 8;
}

void OLEDDisplay::sendInitCommands(void)
{
  if (geometry == GEOMETRY_RAWMODE)
    return;
  sendCommand(DISPLAYOFF);
  sendCommand(SETDISPLAYCLOCKDIV);
  sendCommand(0xF0); // Increase speed of the display max ~96Hz
  sendCommand(SETMULTIPLEX);
  sendCommand(this->height() - 1);
  sendCommand(SETDISPLAYOFFSET);
  sendCommand(0x00);
  if (geometry == GEOMETRY_64_32)
    sendCommand(0x00);
  else
    sendCommand(SETSTARTLINE);
  sendCommand(CHARGEPUMP);
  sendCommand(0x14);
  sendCommand(MEMORYMODE);
  sendCommand(0x00);
  sendCommand(SEGREMAP);
  sendCommand(COMSCANINC);
  sendCommand(SETCOMPINS);

  if (geometry == GEOMETRY_128_64 || geometry == GEOMETRY_64_48 || geometry == GEOMETRY_64_32)
  {
    sendCommand(0x12);
  }
  else if (geometry == GEOMETRY_128_32)
  {
    sendCommand(0x02);
  }

  sendCommand(SETCONTRAST);

  if (geometry == GEOMETRY_128_64 || geometry == GEOMETRY_64_48 || geometry == GEOMETRY_64_32)
  {
    sendCommand(0xCF);
  }
  else if (geometry == GEOMETRY_128_32)
  {
    sendCommand(0x8F);
  }

  sendCommand(SETPRECHARGE);
  sendCommand(0xF1);
  sendCommand(SETVCOMDETECT); // 0xDB, (additionally needed to lower the contrast)
  sendCommand(0x40);          // 0x40 default, to lower the contrast, put 0
  sendCommand(DISPLAYALLON_RESUME);
  sendCommand(NORMALDISPLAY);
  sendCommand(0x2e); // stop scroll
  sendCommand(DISPLAYON);
}

void inline OLEDDisplay::drawInternal(int16_t xMove, int16_t yMove, int16_t width, int16_t height, const uint8_t *data, uint16_t offset, uint16_t bytesInData)
{
  if (width < 0 || height < 0)
    return;
  if (yMove + height < 0 || yMove > this->height())
    return;
  if (xMove + width < 0 || xMove > this->width())
    return;

  uint8_t rasterHeight = 1 + ((height - 1) >> 3); // fast ceil(height / 8.0)
  int8_t yOffset = yMove & 7;

  bytesInData = bytesInData == 0 ? width * rasterHeight : bytesInData;

  int16_t initYMove = yMove;
  int8_t initYOffset = yOffset;

  for (uint16_t i = 0; i < bytesInData; i++)
  {

    // Reset if next horizontal drawing phase is started.
    if (i % rasterHeight == 0)
    {
      yMove = initYMove;
      yOffset = initYOffset;
    }

    uint8_t currentByte = pgm_read_byte(data + offset + i);

    int16_t xPos = xMove + (i / rasterHeight);
    int16_t yPos = ((yMove >> 3) + (i % rasterHeight)) * this->width();

    //    int16_t yScreenPos = yMove + yOffset;
    int16_t dataPos = xPos + yPos;

    if (dataPos >= 0 && dataPos < displayBufferSize &&
        xPos >= 0 && xPos < this->width())
    {

      if (yOffset >= 0)
      {
        switch (this->color)
        {
        case WHITE:
          buffer[dataPos] |= currentByte << yOffset;
          break;
        case BLACK:
          buffer[dataPos] &= ~(currentByte << yOffset);
          break;
        case INVERSE:
          buffer[dataPos] ^= currentByte << yOffset;
          break;
        }

        if (dataPos < (displayBufferSize - this->width()))
        {
          switch (this->color)
          {
          case WHITE:
            buffer[dataPos + this->width()] |= currentByte >> (8 - yOffset);
            break;
          case BLACK:
            buffer[dataPos + this->width()] &= ~(currentByte >> (8 - yOffset));
            break;
          case INVERSE:
            buffer[dataPos + this->width()] ^= currentByte >> (8 - yOffset);
            break;
          }
        }
      }
      else
      {
        // Make new offset position
        yOffset = -yOffset;

        switch (this->color)
        {
        case WHITE:
          buffer[dataPos] |= currentByte >> yOffset;
          break;
        case BLACK:
          buffer[dataPos] &= ~(currentByte >> yOffset);
          break;
        case INVERSE:
          buffer[dataPos] ^= currentByte >> yOffset;
          break;
        }

        // Prepare for next iteration by moving one block up
        yMove -= 8;

        // and setting the new yOffset
        yOffset = 8 - yOffset;
      }
      yield();
    }
  }
}

// You need to free the char!
char *OLEDDisplay::utf8ascii(const String &str)
{
  uint16_t k = 0;
  uint16_t length = str.length() + 1;

  // Copy the string into a char array
  char *s = (char *)malloc(length * sizeof(char));
  if (!s)
  {
    DEBUG_OLEDDISPLAY("[OLEDDISPLAY][utf8ascii] Can't allocate another char array. Drop support for UTF-8.\n");
    return (char *)str.c_str();
  }
  str.toCharArray(s, length);

  length--;

  for (uint16_t i = 0; i < length; i++)
  {
    char c = (this->fontTableLookupFunction)(s[i]);
    if (c != 0)
    {
      s[k++] = c;
    }
  }

  s[k] = 0;

  // This will leak 's' be sure to free it in the calling function.
  return s;
}

void OLEDDisplay::setFontTableLookupFunction(FontTableLookupFunction function)
{
  this->fontTableLookupFunction = function;
}

char DefaultFontTableLookup(const uint8_t ch)
{
  // UTF-8 to font table index converter
  // Code form http://playground.arduino.cc/Main/Utf8ascii
  static uint8_t LASTCHAR;

  if (ch < 128)
  { // Standard ASCII-set 0..0x7F handling
    LASTCHAR = 0;
    return ch;
  }

  uint8_t last = LASTCHAR; // get last char
  LASTCHAR = ch;

  switch (last)
  { // conversion depnding on first UTF8-character
  case 0xC2:
    return (uint8_t)ch;
  case 0xC3:
    return (uint8_t)(ch | 0xC0);
  case 0x82:
    if (ch == 0xAC)
      return (uint8_t)0x80; // special case Euro-symbol
  }

  return (uint8_t)0; // otherwise: return zero, if character has to be ignored
}

uint8_t OLEDDisplay::ArialMT_Plain_10[]{
    0x0A, // Width: 10
    0x0D, // Height: 13
    0x20, // First Char: 32
    0xE0, // Numbers of Chars: 224

    // Jump Table:
    0xFF, 0xFF, 0x00, 0x03, // 32:65535
    0x00, 0x00, 0x04, 0x03, // 33:0
    0x00, 0x04, 0x05, 0x04, // 34:4
    0x00, 0x09, 0x09, 0x06, // 35:9
    0x00, 0x12, 0x0A, 0x06, // 36:18
    0x00, 0x1C, 0x10, 0x09, // 37:28
    0x00, 0x2C, 0x0E, 0x07, // 38:44
    0x00, 0x3A, 0x01, 0x02, // 39:58
    0x00, 0x3B, 0x06, 0x03, // 40:59
    0x00, 0x41, 0x06, 0x03, // 41:65
    0x00, 0x47, 0x05, 0x04, // 42:71
    0x00, 0x4C, 0x09, 0x06, // 43:76
    0x00, 0x55, 0x04, 0x03, // 44:85
    0x00, 0x59, 0x03, 0x03, // 45:89
    0x00, 0x5C, 0x04, 0x03, // 46:92
    0x00, 0x60, 0x05, 0x03, // 47:96
    0x00, 0x65, 0x0A, 0x06, // 48:101
    0x00, 0x6F, 0x08, 0x06, // 49:111
    0x00, 0x77, 0x0A, 0x06, // 50:119
    0x00, 0x81, 0x0A, 0x06, // 51:129
    0x00, 0x8B, 0x0B, 0x06, // 52:139
    0x00, 0x96, 0x0A, 0x06, // 53:150
    0x00, 0xA0, 0x0A, 0x06, // 54:160
    0x00, 0xAA, 0x09, 0x06, // 55:170
    0x00, 0xB3, 0x0A, 0x06, // 56:179
    0x00, 0xBD, 0x0A, 0x06, // 57:189
    0x00, 0xC7, 0x04, 0x03, // 58:199
    0x00, 0xCB, 0x04, 0x03, // 59:203
    0x00, 0xCF, 0x0A, 0x06, // 60:207
    0x00, 0xD9, 0x09, 0x06, // 61:217
    0x00, 0xE2, 0x09, 0x06, // 62:226
    0x00, 0xEB, 0x0B, 0x06, // 63:235
    0x00, 0xF6, 0x14, 0x0A, // 64:246
    0x01, 0x0A, 0x0E, 0x07, // 65:266
    0x01, 0x18, 0x0C, 0x07, // 66:280
    0x01, 0x24, 0x0C, 0x07, // 67:292
    0x01, 0x30, 0x0B, 0x07, // 68:304
    0x01, 0x3B, 0x0C, 0x07, // 69:315
    0x01, 0x47, 0x09, 0x06, // 70:327
    0x01, 0x50, 0x0D, 0x08, // 71:336
    0x01, 0x5D, 0x0C, 0x07, // 72:349
    0x01, 0x69, 0x04, 0x03, // 73:361
    0x01, 0x6D, 0x08, 0x05, // 74:365
    0x01, 0x75, 0x0E, 0x07, // 75:373
    0x01, 0x83, 0x0C, 0x06, // 76:387
    0x01, 0x8F, 0x10, 0x08, // 77:399
    0x01, 0x9F, 0x0C, 0x07, // 78:415
    0x01, 0xAB, 0x0E, 0x08, // 79:427
    0x01, 0xB9, 0x0B, 0x07, // 80:441
    0x01, 0xC4, 0x0E, 0x08, // 81:452
    0x01, 0xD2, 0x0C, 0x07, // 82:466
    0x01, 0xDE, 0x0C, 0x07, // 83:478
    0x01, 0xEA, 0x0B, 0x06, // 84:490
    0x01, 0xF5, 0x0C, 0x07, // 85:501
    0x02, 0x01, 0x0D, 0x07, // 86:513
    0x02, 0x0E, 0x11, 0x09, // 87:526
    0x02, 0x1F, 0x0E, 0x07, // 88:543
    0x02, 0x2D, 0x0D, 0x07, // 89:557
    0x02, 0x3A, 0x0C, 0x06, // 90:570
    0x02, 0x46, 0x06, 0x03, // 91:582
    0x02, 0x4C, 0x06, 0x03, // 92:588
    0x02, 0x52, 0x04, 0x03, // 93:594
    0x02, 0x56, 0x09, 0x05, // 94:598
    0x02, 0x5F, 0x0C, 0x06, // 95:607
    0x02, 0x6B, 0x03, 0x03, // 96:619
    0x02, 0x6E, 0x0A, 0x06, // 97:622
    0x02, 0x78, 0x0A, 0x06, // 98:632
    0x02, 0x82, 0x0A, 0x05, // 99:642
    0x02, 0x8C, 0x0A, 0x06, // 100:652
    0x02, 0x96, 0x0A, 0x06, // 101:662
    0x02, 0xA0, 0x05, 0x03, // 102:672
    0x02, 0xA5, 0x0A, 0x06, // 103:677
    0x02, 0xAF, 0x0A, 0x06, // 104:687
    0x02, 0xB9, 0x04, 0x02, // 105:697
    0x02, 0xBD, 0x04, 0x02, // 106:701
    0x02, 0xC1, 0x08, 0x05, // 107:705
    0x02, 0xC9, 0x04, 0x02, // 108:713
    0x02, 0xCD, 0x10, 0x08, // 109:717
    0x02, 0xDD, 0x0A, 0x06, // 110:733
    0x02, 0xE7, 0x0A, 0x06, // 111:743
    0x02, 0xF1, 0x0A, 0x06, // 112:753
    0x02, 0xFB, 0x0A, 0x06, // 113:763
    0x03, 0x05, 0x05, 0x03, // 114:773
    0x03, 0x0A, 0x08, 0x05, // 115:778
    0x03, 0x12, 0x06, 0x03, // 116:786
    0x03, 0x18, 0x0A, 0x06, // 117:792
    0x03, 0x22, 0x09, 0x05, // 118:802
    0x03, 0x2B, 0x0E, 0x07, // 119:811
    0x03, 0x39, 0x0A, 0x05, // 120:825
    0x03, 0x43, 0x09, 0x05, // 121:835
    0x03, 0x4C, 0x0A, 0x05, // 122:844
    0x03, 0x56, 0x06, 0x03, // 123:854
    0x03, 0x5C, 0x04, 0x03, // 124:860
    0x03, 0x60, 0x05, 0x03, // 125:864
    0x03, 0x65, 0x09, 0x06, // 126:869
    0xFF, 0xFF, 0x00, 0x00, // 127:65535
    0xFF, 0xFF, 0x00, 0x0A, // 128:65535
    0xFF, 0xFF, 0x00, 0x0A, // 129:65535
    0xFF, 0xFF, 0x00, 0x0A, // 130:65535
    0xFF, 0xFF, 0x00, 0x0A, // 131:65535
    0xFF, 0xFF, 0x00, 0x0A, // 132:65535
    0xFF, 0xFF, 0x00, 0x0A, // 133:65535
    0xFF, 0xFF, 0x00, 0x0A, // 134:65535
    0xFF, 0xFF, 0x00, 0x0A, // 135:65535
    0xFF, 0xFF, 0x00, 0x0A, // 136:65535
    0xFF, 0xFF, 0x00, 0x0A, // 137:65535
    0xFF, 0xFF, 0x00, 0x0A, // 138:65535
    0xFF, 0xFF, 0x00, 0x0A, // 139:65535
    0xFF, 0xFF, 0x00, 0x0A, // 140:65535
    0xFF, 0xFF, 0x00, 0x0A, // 141:65535
    0xFF, 0xFF, 0x00, 0x0A, // 142:65535
    0xFF, 0xFF, 0x00, 0x0A, // 143:65535
    0xFF, 0xFF, 0x00, 0x0A, // 144:65535
    0xFF, 0xFF, 0x00, 0x0A, // 145:65535
    0xFF, 0xFF, 0x00, 0x0A, // 146:65535
    0xFF, 0xFF, 0x00, 0x0A, // 147:65535
    0xFF, 0xFF, 0x00, 0x0A, // 148:65535
    0xFF, 0xFF, 0x00, 0x0A, // 149:65535
    0xFF, 0xFF, 0x00, 0x0A, // 150:65535
    0xFF, 0xFF, 0x00, 0x0A, // 151:65535
    0xFF, 0xFF, 0x00, 0x0A, // 152:65535
    0xFF, 0xFF, 0x00, 0x0A, // 153:65535
    0xFF, 0xFF, 0x00, 0x0A, // 154:65535
    0xFF, 0xFF, 0x00, 0x0A, // 155:65535
    0xFF, 0xFF, 0x00, 0x0A, // 156:65535
    0xFF, 0xFF, 0x00, 0x0A, // 157:65535
    0xFF, 0xFF, 0x00, 0x0A, // 158:65535
    0xFF, 0xFF, 0x00, 0x0A, // 159:65535
    0xFF, 0xFF, 0x00, 0x03, // 160:65535
    0x03, 0x6E, 0x04, 0x03, // 161:878
    0x03, 0x72, 0x0A, 0x06, // 162:882
    0x03, 0x7C, 0x0C, 0x06, // 163:892
    0x03, 0x88, 0x0A, 0x06, // 164:904
    0x03, 0x92, 0x0A, 0x06, // 165:914
    0x03, 0x9C, 0x04, 0x03, // 166:924
    0x03, 0xA0, 0x0A, 0x06, // 167:928
    0x03, 0xAA, 0x05, 0x03, // 168:938
    0x03, 0xAF, 0x0D, 0x07, // 169:943
    0x03, 0xBC, 0x07, 0x04, // 170:956
    0x03, 0xC3, 0x0A, 0x06, // 171:963
    0x03, 0xCD, 0x09, 0x06, // 172:973
    0x03, 0xD6, 0x03, 0x03, // 173:982
    0x03, 0xD9, 0x0D, 0x07, // 174:985
    0x03, 0xE6, 0x0B, 0x06, // 175:998
    0x03, 0xF1, 0x07, 0x04, // 176:1009
    0x03, 0xF8, 0x0A, 0x05, // 177:1016
    0x04, 0x02, 0x05, 0x03, // 178:1026
    0x04, 0x07, 0x05, 0x03, // 179:1031
    0x04, 0x0C, 0x05, 0x03, // 180:1036
    0x04, 0x11, 0x0A, 0x06, // 181:1041
    0x04, 0x1B, 0x09, 0x05, // 182:1051
    0x04, 0x24, 0x03, 0x03, // 183:1060
    0x04, 0x27, 0x06, 0x03, // 184:1063
    0x04, 0x2D, 0x05, 0x03, // 185:1069
    0x04, 0x32, 0x07, 0x04, // 186:1074
    0x04, 0x39, 0x0A, 0x06, // 187:1081
    0x04, 0x43, 0x10, 0x08, // 188:1091
    0x04, 0x53, 0x10, 0x08, // 189:1107
    0x04, 0x63, 0x10, 0x08, // 190:1123
    0x04, 0x73, 0x0A, 0x06, // 191:1139
    0x04, 0x7D, 0x0E, 0x07, // 192:1149
    0x04, 0x8B, 0x0E, 0x07, // 193:1163
    0x04, 0x99, 0x0E, 0x07, // 194:1177
    0x04, 0xA7, 0x0E, 0x07, // 195:1191
    0x04, 0xB5, 0x0E, 0x07, // 196:1205
    0x04, 0xC3, 0x0E, 0x07, // 197:1219
    0x04, 0xD1, 0x12, 0x0A, // 198:1233
    0x04, 0xE3, 0x0C, 0x07, // 199:1251
    0x04, 0xEF, 0x0C, 0x07, // 200:1263
    0x04, 0xFB, 0x0C, 0x07, // 201:1275
    0x05, 0x07, 0x0C, 0x07, // 202:1287
    0x05, 0x13, 0x0C, 0x07, // 203:1299
    0x05, 0x1F, 0x05, 0x03, // 204:1311
    0x05, 0x24, 0x04, 0x03, // 205:1316
    0x05, 0x28, 0x04, 0x03, // 206:1320
    0x05, 0x2C, 0x05, 0x03, // 207:1324
    0x05, 0x31, 0x0B, 0x07, // 208:1329
    0x05, 0x3C, 0x0C, 0x07, // 209:1340
    0x05, 0x48, 0x0E, 0x08, // 210:1352
    0x05, 0x56, 0x0E, 0x08, // 211:1366
    0x05, 0x64, 0x0E, 0x08, // 212:1380
    0x05, 0x72, 0x0E, 0x08, // 213:1394
    0x05, 0x80, 0x0E, 0x08, // 214:1408
    0x05, 0x8E, 0x0A, 0x06, // 215:1422
    0x05, 0x98, 0x0D, 0x08, // 216:1432
    0x05, 0xA5, 0x0C, 0x07, // 217:1445
    0x05, 0xB1, 0x0C, 0x07, // 218:1457
    0x05, 0xBD, 0x0C, 0x07, // 219:1469
    0x05, 0xC9, 0x0C, 0x07, // 220:1481
    0x05, 0xD5, 0x0D, 0x07, // 221:1493
    0x05, 0xE2, 0x0B, 0x07, // 222:1506
    0x05, 0xED, 0x0C, 0x06, // 223:1517
    0x05, 0xF9, 0x0A, 0x06, // 224:1529
    0x06, 0x03, 0x0A, 0x06, // 225:1539
    0x06, 0x0D, 0x0A, 0x06, // 226:1549
    0x06, 0x17, 0x0A, 0x06, // 227:1559
    0x06, 0x21, 0x0A, 0x06, // 228:1569
    0x06, 0x2B, 0x0A, 0x06, // 229:1579
    0x06, 0x35, 0x10, 0x09, // 230:1589
    0x06, 0x45, 0x0A, 0x05, // 231:1605
    0x06, 0x4F, 0x0A, 0x06, // 232:1615
    0x06, 0x59, 0x0A, 0x06, // 233:1625
    0x06, 0x63, 0x0A, 0x06, // 234:1635
    0x06, 0x6D, 0x0A, 0x06, // 235:1645
    0x06, 0x77, 0x05, 0x03, // 236:1655
    0x06, 0x7C, 0x04, 0x03, // 237:1660
    0x06, 0x80, 0x05, 0x03, // 238:1664
    0x06, 0x85, 0x05, 0x03, // 239:1669
    0x06, 0x8A, 0x0A, 0x06, // 240:1674
    0x06, 0x94, 0x0A, 0x06, // 241:1684
    0x06, 0x9E, 0x0A, 0x06, // 242:1694
    0x06, 0xA8, 0x0A, 0x06, // 243:1704
    0x06, 0xB2, 0x0A, 0x06, // 244:1714
    0x06, 0xBC, 0x0A, 0x06, // 245:1724
    0x06, 0xC6, 0x0A, 0x06, // 246:1734
    0x06, 0xD0, 0x09, 0x05, // 247:1744
    0x06, 0xD9, 0x0A, 0x06, // 248:1753
    0x06, 0xE3, 0x0A, 0x06, // 249:1763
    0x06, 0xED, 0x0A, 0x06, // 250:1773
    0x06, 0xF7, 0x0A, 0x06, // 251:1783
    0x07, 0x01, 0x0A, 0x06, // 252:1793
    0x07, 0x0B, 0x09, 0x05, // 253:1803
    0x07, 0x14, 0x0A, 0x06, // 254:1812
    0x07, 0x1E, 0x09, 0x05, // 255:1822

    // Font Data:
    0x00, 0x00, 0xF8, 0x02,                                                                                                 // 33
    0x38, 0x00, 0x00, 0x00, 0x38,                                                                                           // 34
    0xA0, 0x03, 0xE0, 0x00, 0xB8, 0x03, 0xE0, 0x00, 0xB8,                                                                   // 35
    0x30, 0x01, 0x28, 0x02, 0xF8, 0x07, 0x48, 0x02, 0x90, 0x01,                                                             // 36
    0x00, 0x00, 0x30, 0x00, 0x48, 0x00, 0x30, 0x03, 0xC0, 0x00, 0xB0, 0x01, 0x48, 0x02, 0x80, 0x01,                         // 37
    0x80, 0x01, 0x50, 0x02, 0x68, 0x02, 0xA8, 0x02, 0x18, 0x01, 0x80, 0x03, 0x80, 0x02,                                     // 38
    0x38,                                                                                                                   // 39
    0xE0, 0x03, 0x10, 0x04, 0x08, 0x08,                                                                                     // 40
    0x08, 0x08, 0x10, 0x04, 0xE0, 0x03,                                                                                     // 41
    0x28, 0x00, 0x18, 0x00, 0x28,                                                                                           // 42
    0x40, 0x00, 0x40, 0x00, 0xF0, 0x01, 0x40, 0x00, 0x40,                                                                   // 43
    0x00, 0x00, 0x00, 0x06,                                                                                                 // 44
    0x80, 0x00, 0x80,                                                                                                       // 45
    0x00, 0x00, 0x00, 0x02,                                                                                                 // 46
    0x00, 0x03, 0xE0, 0x00, 0x18,                                                                                           // 47
    0xF0, 0x01, 0x08, 0x02, 0x08, 0x02, 0x08, 0x02, 0xF0, 0x01,                                                             // 48
    0x00, 0x00, 0x20, 0x00, 0x10, 0x00, 0xF8, 0x03,                                                                         // 49
    0x10, 0x02, 0x08, 0x03, 0x88, 0x02, 0x48, 0x02, 0x30, 0x02,                                                             // 50
    0x10, 0x01, 0x08, 0x02, 0x48, 0x02, 0x48, 0x02, 0xB0, 0x01,                                                             // 51
    0xC0, 0x00, 0xA0, 0x00, 0x90, 0x00, 0x88, 0x00, 0xF8, 0x03, 0x80,                                                       // 52
    0x60, 0x01, 0x38, 0x02, 0x28, 0x02, 0x28, 0x02, 0xC8, 0x01,                                                             // 53
    0xF0, 0x01, 0x28, 0x02, 0x28, 0x02, 0x28, 0x02, 0xD0, 0x01,                                                             // 54
    0x08, 0x00, 0x08, 0x03, 0xC8, 0x00, 0x38, 0x00, 0x08,                                                                   // 55
    0xB0, 0x01, 0x48, 0x02, 0x48, 0x02, 0x48, 0x02, 0xB0, 0x01,                                                             // 56
    0x70, 0x01, 0x88, 0x02, 0x88, 0x02, 0x88, 0x02, 0xF0, 0x01,                                                             // 57
    0x00, 0x00, 0x20, 0x02,                                                                                                 // 58
    0x00, 0x00, 0x20, 0x06,                                                                                                 // 59
    0x00, 0x00, 0x40, 0x00, 0xA0, 0x00, 0xA0, 0x00, 0x10, 0x01,                                                             // 60
    0xA0, 0x00, 0xA0, 0x00, 0xA0, 0x00, 0xA0, 0x00, 0xA0,                                                                   // 61
    0x00, 0x00, 0x10, 0x01, 0xA0, 0x00, 0xA0, 0x00, 0x40,                                                                   // 62
    0x10, 0x00, 0x08, 0x00, 0x08, 0x00, 0xC8, 0x02, 0x48, 0x00, 0x30,                                                       // 63
    0x00, 0x00, 0xC0, 0x03, 0x30, 0x04, 0xD0, 0x09, 0x28, 0x0A, 0x28, 0x0A, 0xC8, 0x0B, 0x68, 0x0A, 0x10, 0x05, 0xE0, 0x04, // 64
    0x00, 0x02, 0xC0, 0x01, 0xB0, 0x00, 0x88, 0x00, 0xB0, 0x00, 0xC0, 0x01, 0x00, 0x02,                                     // 65
    0x00, 0x00, 0xF8, 0x03, 0x48, 0x02, 0x48, 0x02, 0x48, 0x02, 0xF0, 0x01,                                                 // 66
    0x00, 0x00, 0xF0, 0x01, 0x08, 0x02, 0x08, 0x02, 0x08, 0x02, 0x10, 0x01,                                                 // 67
    0x00, 0x00, 0xF8, 0x03, 0x08, 0x02, 0x08, 0x02, 0x10, 0x01, 0xE0,                                                       // 68
    0x00, 0x00, 0xF8, 0x03, 0x48, 0x02, 0x48, 0x02, 0x48, 0x02, 0x48, 0x02,                                                 // 69
    0x00, 0x00, 0xF8, 0x03, 0x48, 0x00, 0x48, 0x00, 0x08,                                                                   // 70
    0x00, 0x00, 0xE0, 0x00, 0x10, 0x01, 0x08, 0x02, 0x48, 0x02, 0x50, 0x01, 0xC0,                                           // 71
    0x00, 0x00, 0xF8, 0x03, 0x40, 0x00, 0x40, 0x00, 0x40, 0x00, 0xF8, 0x03,                                                 // 72
    0x00, 0x00, 0xF8, 0x03,                                                                                                 // 73
    0x00, 0x03, 0x00, 0x02, 0x00, 0x02, 0xF8, 0x01,                                                                         // 74
    0x00, 0x00, 0xF8, 0x03, 0x80, 0x00, 0x60, 0x00, 0x90, 0x00, 0x08, 0x01, 0x00, 0x02,                                     // 75
    0x00, 0x00, 0xF8, 0x03, 0x00, 0x02, 0x00, 0x02, 0x00, 0x02, 0x00, 0x02,                                                 // 76
    0x00, 0x00, 0xF8, 0x03, 0x30, 0x00, 0xC0, 0x01, 0x00, 0x02, 0xC0, 0x01, 0x30, 0x00, 0xF8, 0x03,                         // 77
    0x00, 0x00, 0xF8, 0x03, 0x30, 0x00, 0x40, 0x00, 0x80, 0x01, 0xF8, 0x03,                                                 // 78
    0x00, 0x00, 0xF0, 0x01, 0x08, 0x02, 0x08, 0x02, 0x08, 0x02, 0x08, 0x02, 0xF0, 0x01,                                     // 79
    0x00, 0x00, 0xF8, 0x03, 0x48, 0x00, 0x48, 0x00, 0x48, 0x00, 0x30,                                                       // 80
    0x00, 0x00, 0xF0, 0x01, 0x08, 0x02, 0x08, 0x02, 0x08, 0x03, 0x08, 0x03, 0xF0, 0x02,                                     // 81
    0x00, 0x00, 0xF8, 0x03, 0x48, 0x00, 0x48, 0x00, 0xC8, 0x00, 0x30, 0x03,                                                 // 82
    0x00, 0x00, 0x30, 0x01, 0x48, 0x02, 0x48, 0x02, 0x48, 0x02, 0x90, 0x01,                                                 // 83
    0x00, 0x00, 0x08, 0x00, 0x08, 0x00, 0xF8, 0x03, 0x08, 0x00, 0x08,                                                       // 84
    0x00, 0x00, 0xF8, 0x01, 0x00, 0x02, 0x00, 0x02, 0x00, 0x02, 0xF8, 0x01,                                                 // 85
    0x08, 0x00, 0x70, 0x00, 0x80, 0x01, 0x00, 0x02, 0x80, 0x01, 0x70, 0x00, 0x08,                                           // 86
    0x18, 0x00, 0xE0, 0x01, 0x00, 0x02, 0xF0, 0x01, 0x08, 0x00, 0xF0, 0x01, 0x00, 0x02, 0xE0, 0x01, 0x18,                   // 87
    0x00, 0x02, 0x08, 0x01, 0x90, 0x00, 0x60, 0x00, 0x90, 0x00, 0x08, 0x01, 0x00, 0x02,                                     // 88
    0x08, 0x00, 0x10, 0x00, 0x20, 0x00, 0xC0, 0x03, 0x20, 0x00, 0x10, 0x00, 0x08,                                           // 89
    0x08, 0x03, 0x88, 0x02, 0xC8, 0x02, 0x68, 0x02, 0x38, 0x02, 0x18, 0x02,                                                 // 90
    0x00, 0x00, 0xF8, 0x0F, 0x08, 0x08,                                                                                     // 91
    0x18, 0x00, 0xE0, 0x00, 0x00, 0x03,                                                                                     // 92
    0x08, 0x08, 0xF8, 0x0F,                                                                                                 // 93
    0x40, 0x00, 0x30, 0x00, 0x08, 0x00, 0x30, 0x00, 0x40,                                                                   // 94
    0x00, 0x08, 0x00, 0x08, 0x00, 0x08, 0x00, 0x08, 0x00, 0x08, 0x00, 0x08,                                                 // 95
    0x08, 0x00, 0x10,                                                                                                       // 96
    0x00, 0x00, 0x00, 0x03, 0xA0, 0x02, 0xA0, 0x02, 0xE0, 0x03,                                                             // 97
    0x00, 0x00, 0xF8, 0x03, 0x20, 0x02, 0x20, 0x02, 0xC0, 0x01,                                                             // 98
    0x00, 0x00, 0xC0, 0x01, 0x20, 0x02, 0x20, 0x02, 0x40, 0x01,                                                             // 99
    0x00, 0x00, 0xC0, 0x01, 0x20, 0x02, 0x20, 0x02, 0xF8, 0x03,                                                             // 100
    0x00, 0x00, 0xC0, 0x01, 0xA0, 0x02, 0xA0, 0x02, 0xC0, 0x02,                                                             // 101
    0x20, 0x00, 0xF0, 0x03, 0x28,                                                                                           // 102
    0x00, 0x00, 0xC0, 0x05, 0x20, 0x0A, 0x20, 0x0A, 0xE0, 0x07,                                                             // 103
    0x00, 0x00, 0xF8, 0x03, 0x20, 0x00, 0x20, 0x00, 0xC0, 0x03,                                                             // 104
    0x00, 0x00, 0xE8, 0x03,                                                                                                 // 105
    0x00, 0x08, 0xE8, 0x07,                                                                                                 // 106
    0xF8, 0x03, 0x80, 0x00, 0xC0, 0x01, 0x20, 0x02,                                                                         // 107
    0x00, 0x00, 0xF8, 0x03,                                                                                                 // 108
    0x00, 0x00, 0xE0, 0x03, 0x20, 0x00, 0x20, 0x00, 0xE0, 0x03, 0x20, 0x00, 0x20, 0x00, 0xC0, 0x03,                         // 109
    0x00, 0x00, 0xE0, 0x03, 0x20, 0x00, 0x20, 0x00, 0xC0, 0x03,                                                             // 110
    0x00, 0x00, 0xC0, 0x01, 0x20, 0x02, 0x20, 0x02, 0xC0, 0x01,                                                             // 111
    0x00, 0x00, 0xE0, 0x0F, 0x20, 0x02, 0x20, 0x02, 0xC0, 0x01,                                                             // 112
    0x00, 0x00, 0xC0, 0x01, 0x20, 0x02, 0x20, 0x02, 0xE0, 0x0F,                                                             // 113
    0x00, 0x00, 0xE0, 0x03, 0x20,                                                                                           // 114
    0x40, 0x02, 0xA0, 0x02, 0xA0, 0x02, 0x20, 0x01,                                                                         // 115
    0x20, 0x00, 0xF8, 0x03, 0x20, 0x02,                                                                                     // 116
    0x00, 0x00, 0xE0, 0x01, 0x00, 0x02, 0x00, 0x02, 0xE0, 0x03,                                                             // 117
    0x20, 0x00, 0xC0, 0x01, 0x00, 0x02, 0xC0, 0x01, 0x20,                                                                   // 118
    0xE0, 0x01, 0x00, 0x02, 0xC0, 0x01, 0x20, 0x00, 0xC0, 0x01, 0x00, 0x02, 0xE0, 0x01,                                     // 119
    0x20, 0x02, 0x40, 0x01, 0x80, 0x00, 0x40, 0x01, 0x20, 0x02,                                                             // 120
    0x20, 0x00, 0xC0, 0x09, 0x00, 0x06, 0xC0, 0x01, 0x20,                                                                   // 121
    0x20, 0x02, 0x20, 0x03, 0xA0, 0x02, 0x60, 0x02, 0x20, 0x02,                                                             // 122
    0x80, 0x00, 0x78, 0x0F, 0x08, 0x08,                                                                                     // 123
    0x00, 0x00, 0xF8, 0x0F,                                                                                                 // 124
    0x08, 0x08, 0x78, 0x0F, 0x80,                                                                                           // 125
    0xC0, 0x00, 0x40, 0x00, 0xC0, 0x00, 0x80, 0x00, 0xC0,                                                                   // 126
    0x00, 0x00, 0xA0, 0x0F,                                                                                                 // 161
    0x00, 0x00, 0xC0, 0x01, 0xA0, 0x0F, 0x78, 0x02, 0x40, 0x01,                                                             // 162
    0x40, 0x02, 0x70, 0x03, 0xC8, 0x02, 0x48, 0x02, 0x08, 0x02, 0x10, 0x02,                                                 // 163
    0x00, 0x00, 0xE0, 0x01, 0x20, 0x01, 0x20, 0x01, 0xE0, 0x01,                                                             // 164
    0x48, 0x01, 0x70, 0x01, 0xC0, 0x03, 0x70, 0x01, 0x48, 0x01,                                                             // 165
    0x00, 0x00, 0x38, 0x0F,                                                                                                 // 166
    0xD0, 0x04, 0x28, 0x09, 0x48, 0x09, 0x48, 0x0A, 0x90, 0x05,                                                             // 167
    0x08, 0x00, 0x00, 0x00, 0x08,                                                                                           // 168
    0xE0, 0x00, 0x10, 0x01, 0x48, 0x02, 0xA8, 0x02, 0xA8, 0x02, 0x10, 0x01, 0xE0,                                           // 169
    0x68, 0x00, 0x68, 0x00, 0x68, 0x00, 0x78,                                                                               // 170
    0x00, 0x00, 0x80, 0x01, 0x40, 0x02, 0x80, 0x01, 0x40, 0x02,                                                             // 171
    0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0x20, 0x00, 0xE0,                                                                   // 172
    0x80, 0x00, 0x80,                                                                                                       // 173
    0xE0, 0x00, 0x10, 0x01, 0xE8, 0x02, 0x68, 0x02, 0xC8, 0x02, 0x10, 0x01, 0xE0,                                           // 174
    0x02, 0x00, 0x02, 0x00, 0x02, 0x00, 0x02, 0x00, 0x02, 0x00, 0x02,                                                       // 175
    0x00, 0x00, 0x38, 0x00, 0x28, 0x00, 0x38,                                                                               // 176
    0x40, 0x02, 0x40, 0x02, 0xF0, 0x03, 0x40, 0x02, 0x40, 0x02,                                                             // 177
    0x48, 0x00, 0x68, 0x00, 0x58,                                                                                           // 178
    0x48, 0x00, 0x58, 0x00, 0x68,                                                                                           // 179
    0x00, 0x00, 0x10, 0x00, 0x08,                                                                                           // 180
    0x00, 0x00, 0xE0, 0x0F, 0x00, 0x02, 0x00, 0x02, 0xE0, 0x03,                                                             // 181
    0x70, 0x00, 0xF8, 0x0F, 0x08, 0x00, 0xF8, 0x0F, 0x08,                                                                   // 182
    0x00, 0x00, 0x40,                                                                                                       // 183
    0x00, 0x00, 0x00, 0x14, 0x00, 0x18,                                                                                     // 184
    0x00, 0x00, 0x10, 0x00, 0x78,                                                                                           // 185
    0x30, 0x00, 0x48, 0x00, 0x48, 0x00, 0x30,                                                                               // 186
    0x00, 0x00, 0x40, 0x02, 0x80, 0x01, 0x40, 0x02, 0x80, 0x01,                                                             // 187
    0x00, 0x00, 0x10, 0x02, 0x78, 0x01, 0xC0, 0x00, 0x20, 0x01, 0x90, 0x01, 0xC8, 0x03, 0x00, 0x01,                         // 188
    0x00, 0x00, 0x10, 0x02, 0x78, 0x01, 0x80, 0x00, 0x60, 0x00, 0x50, 0x02, 0x48, 0x03, 0xC0, 0x02,                         // 189
    0x48, 0x00, 0x58, 0x00, 0x68, 0x03, 0x80, 0x00, 0x60, 0x01, 0x90, 0x01, 0xC8, 0x03, 0x00, 0x01,                         // 190
    0x00, 0x00, 0x00, 0x06, 0x00, 0x09, 0xA0, 0x09, 0x00, 0x04,                                                             // 191
    0x00, 0x02, 0xC0, 0x01, 0xB0, 0x00, 0x89, 0x00, 0xB2, 0x00, 0xC0, 0x01, 0x00, 0x02,                                     // 192
    0x00, 0x02, 0xC0, 0x01, 0xB0, 0x00, 0x8A, 0x00, 0xB1, 0x00, 0xC0, 0x01, 0x00, 0x02,                                     // 193
    0x00, 0x02, 0xC0, 0x01, 0xB2, 0x00, 0x89, 0x00, 0xB2, 0x00, 0xC0, 0x01, 0x00, 0x02,                                     // 194
    0x00, 0x02, 0xC2, 0x01, 0xB1, 0x00, 0x8A, 0x00, 0xB1, 0x00, 0xC0, 0x01, 0x00, 0x02,                                     // 195
    0x00, 0x02, 0xC0, 0x01, 0xB2, 0x00, 0x88, 0x00, 0xB2, 0x00, 0xC0, 0x01, 0x00, 0x02,                                     // 196
    0x00, 0x02, 0xC0, 0x01, 0xBE, 0x00, 0x8A, 0x00, 0xBE, 0x00, 0xC0, 0x01, 0x00, 0x02,                                     // 197
    0x00, 0x03, 0xC0, 0x00, 0xE0, 0x00, 0x98, 0x00, 0x88, 0x00, 0xF8, 0x03, 0x48, 0x02, 0x48, 0x02, 0x48, 0x02,             // 198
    0x00, 0x00, 0xF0, 0x01, 0x08, 0x02, 0x08, 0x16, 0x08, 0x1A, 0x10, 0x01,                                                 // 199
    0x00, 0x00, 0xF8, 0x03, 0x49, 0x02, 0x4A, 0x02, 0x48, 0x02, 0x48, 0x02,                                                 // 200
    0x00, 0x00, 0xF8, 0x03, 0x48, 0x02, 0x4A, 0x02, 0x49, 0x02, 0x48, 0x02,                                                 // 201
    0x00, 0x00, 0xFA, 0x03, 0x49, 0x02, 0x4A, 0x02, 0x48, 0x02, 0x48, 0x02,                                                 // 202
    0x00, 0x00, 0xF8, 0x03, 0x4A, 0x02, 0x48, 0x02, 0x4A, 0x02, 0x48, 0x02,                                                 // 203
    0x00, 0x00, 0xF9, 0x03, 0x02,                                                                                           // 204
    0x02, 0x00, 0xF9, 0x03,                                                                                                 // 205
    0x01, 0x00, 0xFA, 0x03,                                                                                                 // 206
    0x02, 0x00, 0xF8, 0x03, 0x02,                                                                                           // 207
    0x40, 0x00, 0xF8, 0x03, 0x48, 0x02, 0x48, 0x02, 0x10, 0x01, 0xE0,                                                       // 208
    0x00, 0x00, 0xFA, 0x03, 0x31, 0x00, 0x42, 0x00, 0x81, 0x01, 0xF8, 0x03,                                                 // 209
    0x00, 0x00, 0xF0, 0x01, 0x08, 0x02, 0x09, 0x02, 0x0A, 0x02, 0x08, 0x02, 0xF0, 0x01,                                     // 210
    0x00, 0x00, 0xF0, 0x01, 0x08, 0x02, 0x0A, 0x02, 0x09, 0x02, 0x08, 0x02, 0xF0, 0x01,                                     // 211
    0x00, 0x00, 0xF0, 0x01, 0x08, 0x02, 0x0A, 0x02, 0x09, 0x02, 0x0A, 0x02, 0xF0, 0x01,                                     // 212
    0x00, 0x00, 0xF0, 0x01, 0x0A, 0x02, 0x09, 0x02, 0x0A, 0x02, 0x09, 0x02, 0xF0, 0x01,                                     // 213
    0x00, 0x00, 0xF0, 0x01, 0x0A, 0x02, 0x08, 0x02, 0x0A, 0x02, 0x08, 0x02, 0xF0, 0x01,                                     // 214
    0x10, 0x01, 0xA0, 0x00, 0xE0, 0x00, 0xA0, 0x00, 0x10, 0x01,                                                             // 215
    0x00, 0x00, 0xF0, 0x02, 0x08, 0x03, 0xC8, 0x02, 0x28, 0x02, 0x18, 0x03, 0xE8,                                           // 216
    0x00, 0x00, 0xF8, 0x01, 0x01, 0x02, 0x02, 0x02, 0x00, 0x02, 0xF8, 0x01,                                                 // 217
    0x00, 0x00, 0xF8, 0x01, 0x02, 0x02, 0x01, 0x02, 0x00, 0x02, 0xF8, 0x01,                                                 // 218
    0x00, 0x00, 0xF8, 0x01, 0x02, 0x02, 0x01, 0x02, 0x02, 0x02, 0xF8, 0x01,                                                 // 219
    0x00, 0x00, 0xF8, 0x01, 0x02, 0x02, 0x00, 0x02, 0x02, 0x02, 0xF8, 0x01,                                                 // 220
    0x08, 0x00, 0x10, 0x00, 0x20, 0x00, 0xC2, 0x03, 0x21, 0x00, 0x10, 0x00, 0x08,                                           // 221
    0x00, 0x00, 0xF8, 0x03, 0x10, 0x01, 0x10, 0x01, 0x10, 0x01, 0xE0,                                                       // 222
    0x00, 0x00, 0xF0, 0x03, 0x08, 0x01, 0x48, 0x02, 0xB0, 0x02, 0x80, 0x01,                                                 // 223
    0x00, 0x00, 0x00, 0x03, 0xA4, 0x02, 0xA8, 0x02, 0xE0, 0x03,                                                             // 224
    0x00, 0x00, 0x00, 0x03, 0xA8, 0x02, 0xA4, 0x02, 0xE0, 0x03,                                                             // 225
    0x00, 0x00, 0x00, 0x03, 0xA8, 0x02, 0xA4, 0x02, 0xE8, 0x03,                                                             // 226
    0x00, 0x00, 0x08, 0x03, 0xA4, 0x02, 0xA8, 0x02, 0xE4, 0x03,                                                             // 227
    0x00, 0x00, 0x00, 0x03, 0xA8, 0x02, 0xA0, 0x02, 0xE8, 0x03,                                                             // 228
    0x00, 0x00, 0x00, 0x03, 0xAE, 0x02, 0xAA, 0x02, 0xEE, 0x03,                                                             // 229
    0x00, 0x00, 0x40, 0x03, 0xA0, 0x02, 0xA0, 0x02, 0xC0, 0x01, 0xA0, 0x02, 0xA0, 0x02, 0xC0, 0x02,                         // 230
    0x00, 0x00, 0xC0, 0x01, 0x20, 0x16, 0x20, 0x1A, 0x40, 0x01,                                                             // 231
    0x00, 0x00, 0xC0, 0x01, 0xA4, 0x02, 0xA8, 0x02, 0xC0, 0x02,                                                             // 232
    0x00, 0x00, 0xC0, 0x01, 0xA8, 0x02, 0xA4, 0x02, 0xC0, 0x02,                                                             // 233
    0x00, 0x00, 0xC0, 0x01, 0xA8, 0x02, 0xA4, 0x02, 0xC8, 0x02,                                                             // 234
    0x00, 0x00, 0xC0, 0x01, 0xA8, 0x02, 0xA0, 0x02, 0xC8, 0x02,                                                             // 235
    0x00, 0x00, 0xE4, 0x03, 0x08,                                                                                           // 236
    0x08, 0x00, 0xE4, 0x03,                                                                                                 // 237
    0x08, 0x00, 0xE4, 0x03, 0x08,                                                                                           // 238
    0x08, 0x00, 0xE0, 0x03, 0x08,                                                                                           // 239
    0x00, 0x00, 0xC0, 0x01, 0x28, 0x02, 0x38, 0x02, 0xE0, 0x01,                                                             // 240
    0x00, 0x00, 0xE8, 0x03, 0x24, 0x00, 0x28, 0x00, 0xC4, 0x03,                                                             // 241
    0x00, 0x00, 0xC0, 0x01, 0x24, 0x02, 0x28, 0x02, 0xC0, 0x01,                                                             // 242
    0x00, 0x00, 0xC0, 0x01, 0x28, 0x02, 0x24, 0x02, 0xC0, 0x01,                                                             // 243
    0x00, 0x00, 0xC0, 0x01, 0x28, 0x02, 0x24, 0x02, 0xC8, 0x01,                                                             // 244
    0x00, 0x00, 0xC8, 0x01, 0x24, 0x02, 0x28, 0x02, 0xC4, 0x01,                                                             // 245
    0x00, 0x00, 0xC0, 0x01, 0x28, 0x02, 0x20, 0x02, 0xC8, 0x01,                                                             // 246
    0x40, 0x00, 0x40, 0x00, 0x50, 0x01, 0x40, 0x00, 0x40,                                                                   // 247
    0x00, 0x00, 0xC0, 0x02, 0xA0, 0x03, 0x60, 0x02, 0xA0, 0x01,                                                             // 248
    0x00, 0x00, 0xE0, 0x01, 0x04, 0x02, 0x08, 0x02, 0xE0, 0x03,                                                             // 249
    0x00, 0x00, 0xE0, 0x01, 0x08, 0x02, 0x04, 0x02, 0xE0, 0x03,                                                             // 250
    0x00, 0x00, 0xE8, 0x01, 0x04, 0x02, 0x08, 0x02, 0xE0, 0x03,                                                             // 251
    0x00, 0x00, 0xE0, 0x01, 0x08, 0x02, 0x00, 0x02, 0xE8, 0x03,                                                             // 252
    0x20, 0x00, 0xC0, 0x09, 0x08, 0x06, 0xC4, 0x01, 0x20,                                                                   // 253
    0x00, 0x00, 0xF8, 0x0F, 0x20, 0x02, 0x20, 0x02, 0xC0, 0x01,                                                             // 254
    0x20, 0x00, 0xC8, 0x09, 0x00, 0x06, 0xC8, 0x01, 0x20                                                                    // 255
};
