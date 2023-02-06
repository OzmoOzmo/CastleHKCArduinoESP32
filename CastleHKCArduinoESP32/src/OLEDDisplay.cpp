/**
 * Greatly reduced code for LCD display - Based on code found on Github
 * at: https://github.com/ThingPulse/esp8266-oled-ssd1306/blob/master/src/OLEDDisplay.cpp
 *
 */

#include "OLEDDisplay.h"
#include <Wire.h>

OLEDDisplay::OLEDDisplay(uint8_t _address, uint8_t _sda, uint8_t _scl)
{
    this->_address = _address;
    this->_sda = _sda;
    this->_scl = _scl;
}

OLEDDisplay::~OLEDDisplay() {
}

bool OLEDDisplay::init()
{
    if (!this->connect()) {
        //DEBUG_OLEDDISPLAY("[OLEDDISPLAY][init] Can't establish connection to display\n");
        return false;
    }

  sendInitCommands();
  resetDisplay();

  return true;
}

void OLEDDisplay::resetDisplay(void) {
  clear();
  display();
}

void OLEDDisplay::drawString(int16_t xMove, int16_t yMove, String strUser)
{
    const char* text = strUser.c_str();
    uint16_t textLength = strlen(text);
    
    uint8_t textHeight = pgm_read_byte(fontData + HEIGHT_POS);
    uint8_t firstChar = pgm_read_byte(fontData + FIRST_CHAR_POS);
    uint16_t sizeOfJumpTable = pgm_read_byte(fontData + CHAR_NUM_POS) * JUMPTABLE_BYTES;

    uint16_t cursorX = 0;
    uint16_t cursorY = 0;

    for (uint16_t j = 0; j < textLength; j++) {
        int16_t xPos = xMove + cursorX;
        int16_t yPos = yMove + cursorY;

        uint8_t code = text[j];
        if (code >= firstChar) {
            uint8_t charCode = code - firstChar;

            // 4 Bytes per char code
            uint8_t msbJumpToChar = pgm_read_byte(fontData + JUMPTABLE_START + charCode * JUMPTABLE_BYTES);                  // MSB  \ JumpAddress
            uint8_t lsbJumpToChar = pgm_read_byte(fontData + JUMPTABLE_START + charCode * JUMPTABLE_BYTES + JUMPTABLE_LSB);   // LSB /
            uint8_t charByteSize = pgm_read_byte(fontData + JUMPTABLE_START + charCode * JUMPTABLE_BYTES + JUMPTABLE_SIZE);  // Size
            uint8_t currentCharWidth = pgm_read_byte(fontData + JUMPTABLE_START + charCode * JUMPTABLE_BYTES + JUMPTABLE_WIDTH); // Width

            // Test if the char is drawable
            if (!(msbJumpToChar == 255 && lsbJumpToChar == 255)) {
                // Get the position of the char data
                uint16_t charDataPosition = JUMPTABLE_START + sizeOfJumpTable + ((msbJumpToChar << 8) + lsbJumpToChar);
                drawInternal(xPos, yPos, currentCharWidth, textHeight, fontData, charDataPosition, charByteSize);
            }

            cursorX += currentCharWidth;
        }
    }
}

void OLEDDisplay::flipScreenVertically() {
  sendCommand(SEGREMAP | 0x01);
  sendCommand(COMSCANDEC);           //Rotate screen 180 Deg
}

void OLEDDisplay::clear(void) {
  memset(buffer, 0, displayBufferSize);
}

void OLEDDisplay::sendInitCommands(void) {
  sendCommand(DISPLAYOFF);
  sendCommand(SETDISPLAYCLOCKDIV);
  sendCommand(0xF0); // Increase speed of the display max ~96Hz
  sendCommand(SETMULTIPLEX);
  sendCommand(displayHeight - 1);
  sendCommand(SETDISPLAYOFFSET);
  sendCommand(0x00);
  //sendCommand(SETSTARTLINE);
  if(geometry == GEOMETRY_64_32)
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
  //sendCommand(0x02); //GEOMETRY_128_32
  if (geometry == GEOMETRY_128_64 || geometry == GEOMETRY_64_48 || geometry == GEOMETRY_64_32)
    sendCommand(0x12);
  else if (geometry == GEOMETRY_128_32)
    sendCommand(0x02);
  sendCommand(SETCONTRAST);
  if (geometry == GEOMETRY_128_64 || geometry == GEOMETRY_64_48 || geometry == GEOMETRY_64_32)
    sendCommand(0xCF);
  else if (geometry == GEOMETRY_128_32)
    sendCommand(0x8F);
  sendCommand(SETPRECHARGE);
  sendCommand(0xF1);
  sendCommand(SETVCOMDETECT); //0xDB, (additionally needed to lower the contrast)
  sendCommand(0x40);	        //0x40 default, to lower the contrast, put 0
  sendCommand(DISPLAYALLON_RESUME);
  sendCommand(NORMALDISPLAY);
  sendCommand(0x2e);            // stop scroll
  sendCommand(DISPLAYON);
}

void inline OLEDDisplay::drawInternal(int16_t xMove, int16_t yMove, int16_t width, int16_t height, const uint8_t *data, uint16_t offset, uint16_t bytesInData) {
  if (width < 0 || height < 0) return;
  if (yMove + height < 0 || yMove > displayHeight)  return;
  if (xMove + width  < 0 || xMove > displayWidth)   return;

  uint8_t  rasterHeight = 1 + ((height - 1) >> 3); // fast ceil(height / 8.0)
  int8_t   yOffset      = yMove & 7;

  bytesInData = bytesInData == 0 ? width * rasterHeight : bytesInData;

  int16_t initYMove   = yMove;
  int8_t  initYOffset = yOffset;

  for (uint16_t i = 0; i < bytesInData; i++) {

    // Reset if next horizontal drawing phase is started.
    if ( i % rasterHeight == 0) {
      yMove   = initYMove;
      yOffset = initYOffset;
    }

    uint8_t currentByte = pgm_read_byte(data + offset + i);

    int16_t xPos = xMove + (i / rasterHeight);
    int16_t yPos = ((yMove >> 3) + (i % rasterHeight)) * displayWidth;

    int16_t dataPos    = xPos  + yPos;

    if (dataPos >=  0  && dataPos < displayBufferSize &&
        xPos    >=  0  && xPos    < displayWidth ) {

      if (yOffset >= 0) {
        buffer[dataPos] |= currentByte << yOffset; // WHITE

        if (dataPos < (displayBufferSize - displayWidth)) {
          buffer[dataPos + displayWidth] |= currentByte >> (8 - yOffset); // WHITE:   
         }
      } else {
        // Make new offset position
        yOffset = -yOffset;

         buffer[dataPos] |= currentByte >> yOffset; //]case WHITE: 

        // Prepare for next iteration by moving one block up
        yMove -= 8;

        // and setting the new yOffset
        yOffset = 8 - yOffset;
      }
    }
  }
}

inline void OLEDDisplay::sendCommand(uint8_t command)
{
    Wire.beginTransmission(_address);
    Wire.write(0x80);
    Wire.write(command);
    Wire.endTransmission();
}

bool OLEDDisplay::connect()
{
    Wire.begin(this->_sda, this->_scl);
    // Let's use ~700khz if ESP8266 is in 160Mhz mode
    // this will be limited to ~400khz if the ESP8266 in 80Mhz mode.
    Wire.setClock(700000);
    return true;
}

void OLEDDisplay::display(void)
{
    const int x_offset = (128 - displayWidth) / 2;
    sendCommand(COLUMNADDR);
    sendCommand(x_offset);
    sendCommand(x_offset + (displayWidth - 1));

    sendCommand(PAGEADDR);
    sendCommand(0x0);

    sendCommand(geometry); //0x3 for GEOMETRY_128_32

    for (uint16_t i = 0; i < displayBufferSize; i++) {
        Wire.beginTransmission(this->_address);
        Wire.write(0x40);
        for (uint8_t x = 0; x < 16; x++) {
            Wire.write(buffer[i]);
            i++;
        }
        i--;
        Wire.endTransmission();
    }
}


uint8_t OLEDDisplay::fontData[]{
  0x0A, // Width: 10
  0x0D, // Height: 13
  0x20, // First Char: 32
  0xE0, // Numbers of Chars: 224

  // Jump Table:
  0xFF, 0xFF, 0x00, 0x03,  // 32:65535
  0x00, 0x00, 0x04, 0x03,  // 33:0
  0x00, 0x04, 0x05, 0x04,  // 34:4
  0x00, 0x09, 0x09, 0x06,  // 35:9
  0x00, 0x12, 0x0A, 0x06,  // 36:18
  0x00, 0x1C, 0x10, 0x09,  // 37:28
  0x00, 0x2C, 0x0E, 0x07,  // 38:44
  0x00, 0x3A, 0x01, 0x02,  // 39:58
  0x00, 0x3B, 0x06, 0x03,  // 40:59
  0x00, 0x41, 0x06, 0x03,  // 41:65
  0x00, 0x47, 0x05, 0x04,  // 42:71
  0x00, 0x4C, 0x09, 0x06,  // 43:76
  0x00, 0x55, 0x04, 0x03,  // 44:85
  0x00, 0x59, 0x03, 0x03,  // 45:89
  0x00, 0x5C, 0x04, 0x03,  // 46:92
  0x00, 0x60, 0x05, 0x03,  // 47:96
  0x00, 0x65, 0x0A, 0x06,  // 48:101
  0x00, 0x6F, 0x08, 0x06,  // 49:111
  0x00, 0x77, 0x0A, 0x06,  // 50:119
  0x00, 0x81, 0x0A, 0x06,  // 51:129
  0x00, 0x8B, 0x0B, 0x06,  // 52:139
  0x00, 0x96, 0x0A, 0x06,  // 53:150
  0x00, 0xA0, 0x0A, 0x06,  // 54:160
  0x00, 0xAA, 0x09, 0x06,  // 55:170
  0x00, 0xB3, 0x0A, 0x06,  // 56:179
  0x00, 0xBD, 0x0A, 0x06,  // 57:189
  0x00, 0xC7, 0x04, 0x03,  // 58:199
  0x00, 0xCB, 0x04, 0x03,  // 59:203
  0x00, 0xCF, 0x0A, 0x06,  // 60:207
  0x00, 0xD9, 0x09, 0x06,  // 61:217
  0x00, 0xE2, 0x09, 0x06,  // 62:226
  0x00, 0xEB, 0x0B, 0x06,  // 63:235
  0x00, 0xF6, 0x14, 0x0A,  // 64:246
  0x01, 0x0A, 0x0E, 0x07,  // 65:266
  0x01, 0x18, 0x0C, 0x07,  // 66:280
  0x01, 0x24, 0x0C, 0x07,  // 67:292
  0x01, 0x30, 0x0B, 0x07,  // 68:304
  0x01, 0x3B, 0x0C, 0x07,  // 69:315
  0x01, 0x47, 0x09, 0x06,  // 70:327
  0x01, 0x50, 0x0D, 0x08,  // 71:336
  0x01, 0x5D, 0x0C, 0x07,  // 72:349
  0x01, 0x69, 0x04, 0x03,  // 73:361
  0x01, 0x6D, 0x08, 0x05,  // 74:365
  0x01, 0x75, 0x0E, 0x07,  // 75:373
  0x01, 0x83, 0x0C, 0x06,  // 76:387
  0x01, 0x8F, 0x10, 0x08,  // 77:399
  0x01, 0x9F, 0x0C, 0x07,  // 78:415
  0x01, 0xAB, 0x0E, 0x08,  // 79:427
  0x01, 0xB9, 0x0B, 0x07,  // 80:441
  0x01, 0xC4, 0x0E, 0x08,  // 81:452
  0x01, 0xD2, 0x0C, 0x07,  // 82:466
  0x01, 0xDE, 0x0C, 0x07,  // 83:478
  0x01, 0xEA, 0x0B, 0x06,  // 84:490
  0x01, 0xF5, 0x0C, 0x07,  // 85:501
  0x02, 0x01, 0x0D, 0x07,  // 86:513
  0x02, 0x0E, 0x11, 0x09,  // 87:526
  0x02, 0x1F, 0x0E, 0x07,  // 88:543
  0x02, 0x2D, 0x0D, 0x07,  // 89:557
  0x02, 0x3A, 0x0C, 0x06,  // 90:570
  0x02, 0x46, 0x06, 0x03,  // 91:582
  0x02, 0x4C, 0x06, 0x03,  // 92:588
  0x02, 0x52, 0x04, 0x03,  // 93:594
  0x02, 0x56, 0x09, 0x05,  // 94:598
  0x02, 0x5F, 0x0C, 0x06,  // 95:607
  0x02, 0x6B, 0x03, 0x03,  // 96:619
  0x02, 0x6E, 0x0A, 0x06,  // 97:622
  0x02, 0x78, 0x0A, 0x06,  // 98:632
  0x02, 0x82, 0x0A, 0x05,  // 99:642
  0x02, 0x8C, 0x0A, 0x06,  // 100:652
  0x02, 0x96, 0x0A, 0x06,  // 101:662
  0x02, 0xA0, 0x05, 0x03,  // 102:672
  0x02, 0xA5, 0x0A, 0x06,  // 103:677
  0x02, 0xAF, 0x0A, 0x06,  // 104:687
  0x02, 0xB9, 0x04, 0x02,  // 105:697
  0x02, 0xBD, 0x04, 0x02,  // 106:701
  0x02, 0xC1, 0x08, 0x05,  // 107:705
  0x02, 0xC9, 0x04, 0x02,  // 108:713
  0x02, 0xCD, 0x10, 0x08,  // 109:717
  0x02, 0xDD, 0x0A, 0x06,  // 110:733
  0x02, 0xE7, 0x0A, 0x06,  // 111:743
  0x02, 0xF1, 0x0A, 0x06,  // 112:753
  0x02, 0xFB, 0x0A, 0x06,  // 113:763
  0x03, 0x05, 0x05, 0x03,  // 114:773
  0x03, 0x0A, 0x08, 0x05,  // 115:778
  0x03, 0x12, 0x06, 0x03,  // 116:786
  0x03, 0x18, 0x0A, 0x06,  // 117:792
  0x03, 0x22, 0x09, 0x05,  // 118:802
  0x03, 0x2B, 0x0E, 0x07,  // 119:811
  0x03, 0x39, 0x0A, 0x05,  // 120:825
  0x03, 0x43, 0x09, 0x05,  // 121:835
  0x03, 0x4C, 0x0A, 0x05,  // 122:844
  0x03, 0x56, 0x06, 0x03,  // 123:854
  0x03, 0x5C, 0x04, 0x03,  // 124:860
  0x03, 0x60, 0x05, 0x03,  // 125:864
  0x03, 0x65, 0x09, 0x06,  // 126:869
  0xFF, 0xFF, 0x00, 0x00,  // 127:65535
  0xFF, 0xFF, 0x00, 0x0A,  // 128:65535
  0xFF, 0xFF, 0x00, 0x0A,  // 129:65535
  0xFF, 0xFF, 0x00, 0x0A,  // 130:65535
  0xFF, 0xFF, 0x00, 0x0A,  // 131:65535
  0xFF, 0xFF, 0x00, 0x0A,  // 132:65535
  0xFF, 0xFF, 0x00, 0x0A,  // 133:65535
  0xFF, 0xFF, 0x00, 0x0A,  // 134:65535
  0xFF, 0xFF, 0x00, 0x0A,  // 135:65535
  0xFF, 0xFF, 0x00, 0x0A,  // 136:65535
  0xFF, 0xFF, 0x00, 0x0A,  // 137:65535
  0xFF, 0xFF, 0x00, 0x0A,  // 138:65535
  0xFF, 0xFF, 0x00, 0x0A,  // 139:65535
  0xFF, 0xFF, 0x00, 0x0A,  // 140:65535
  0xFF, 0xFF, 0x00, 0x0A,  // 141:65535
  0xFF, 0xFF, 0x00, 0x0A,  // 142:65535
  0xFF, 0xFF, 0x00, 0x0A,  // 143:65535
  0xFF, 0xFF, 0x00, 0x0A,  // 144:65535
  0xFF, 0xFF, 0x00, 0x0A,  // 145:65535
  0xFF, 0xFF, 0x00, 0x0A,  // 146:65535
  0xFF, 0xFF, 0x00, 0x0A,  // 147:65535
  0xFF, 0xFF, 0x00, 0x0A,  // 148:65535
  0xFF, 0xFF, 0x00, 0x0A,  // 149:65535
  0xFF, 0xFF, 0x00, 0x0A,  // 150:65535
  0xFF, 0xFF, 0x00, 0x0A,  // 151:65535
  0xFF, 0xFF, 0x00, 0x0A,  // 152:65535
  0xFF, 0xFF, 0x00, 0x0A,  // 153:65535
  0xFF, 0xFF, 0x00, 0x0A,  // 154:65535
  0xFF, 0xFF, 0x00, 0x0A,  // 155:65535
  0xFF, 0xFF, 0x00, 0x0A,  // 156:65535
  0xFF, 0xFF, 0x00, 0x0A,  // 157:65535
  0xFF, 0xFF, 0x00, 0x0A,  // 158:65535
  0xFF, 0xFF, 0x00, 0x0A,  // 159:65535
  0xFF, 0xFF, 0x00, 0x03,  // 160:65535
  0x03, 0x6E, 0x04, 0x03,  // 161:878
  0x03, 0x72, 0x0A, 0x06,  // 162:882
  0x03, 0x7C, 0x0C, 0x06,  // 163:892
  0x03, 0x88, 0x0A, 0x06,  // 164:904
  0x03, 0x92, 0x0A, 0x06,  // 165:914
  0x03, 0x9C, 0x04, 0x03,  // 166:924
  0x03, 0xA0, 0x0A, 0x06,  // 167:928
  0x03, 0xAA, 0x05, 0x03,  // 168:938
  0x03, 0xAF, 0x0D, 0x07,  // 169:943
  0x03, 0xBC, 0x07, 0x04,  // 170:956
  0x03, 0xC3, 0x0A, 0x06,  // 171:963
  0x03, 0xCD, 0x09, 0x06,  // 172:973
  0x03, 0xD6, 0x03, 0x03,  // 173:982
  0x03, 0xD9, 0x0D, 0x07,  // 174:985
  0x03, 0xE6, 0x0B, 0x06,  // 175:998
  0x03, 0xF1, 0x07, 0x04,  // 176:1009
  0x03, 0xF8, 0x0A, 0x05,  // 177:1016
  0x04, 0x02, 0x05, 0x03,  // 178:1026
  0x04, 0x07, 0x05, 0x03,  // 179:1031
  0x04, 0x0C, 0x05, 0x03,  // 180:1036
  0x04, 0x11, 0x0A, 0x06,  // 181:1041
  0x04, 0x1B, 0x09, 0x05,  // 182:1051
  0x04, 0x24, 0x03, 0x03,  // 183:1060
  0x04, 0x27, 0x06, 0x03,  // 184:1063
  0x04, 0x2D, 0x05, 0x03,  // 185:1069
  0x04, 0x32, 0x07, 0x04,  // 186:1074
  0x04, 0x39, 0x0A, 0x06,  // 187:1081
  0x04, 0x43, 0x10, 0x08,  // 188:1091
  0x04, 0x53, 0x10, 0x08,  // 189:1107
  0x04, 0x63, 0x10, 0x08,  // 190:1123
  0x04, 0x73, 0x0A, 0x06,  // 191:1139
  0x04, 0x7D, 0x0E, 0x07,  // 192:1149
  0x04, 0x8B, 0x0E, 0x07,  // 193:1163
  0x04, 0x99, 0x0E, 0x07,  // 194:1177
  0x04, 0xA7, 0x0E, 0x07,  // 195:1191
  0x04, 0xB5, 0x0E, 0x07,  // 196:1205
  0x04, 0xC3, 0x0E, 0x07,  // 197:1219
  0x04, 0xD1, 0x12, 0x0A,  // 198:1233
  0x04, 0xE3, 0x0C, 0x07,  // 199:1251
  0x04, 0xEF, 0x0C, 0x07,  // 200:1263
  0x04, 0xFB, 0x0C, 0x07,  // 201:1275
  0x05, 0x07, 0x0C, 0x07,  // 202:1287
  0x05, 0x13, 0x0C, 0x07,  // 203:1299
  0x05, 0x1F, 0x05, 0x03,  // 204:1311
  0x05, 0x24, 0x04, 0x03,  // 205:1316
  0x05, 0x28, 0x04, 0x03,  // 206:1320
  0x05, 0x2C, 0x05, 0x03,  // 207:1324
  0x05, 0x31, 0x0B, 0x07,  // 208:1329
  0x05, 0x3C, 0x0C, 0x07,  // 209:1340
  0x05, 0x48, 0x0E, 0x08,  // 210:1352
  0x05, 0x56, 0x0E, 0x08,  // 211:1366
  0x05, 0x64, 0x0E, 0x08,  // 212:1380
  0x05, 0x72, 0x0E, 0x08,  // 213:1394
  0x05, 0x80, 0x0E, 0x08,  // 214:1408
  0x05, 0x8E, 0x0A, 0x06,  // 215:1422
  0x05, 0x98, 0x0D, 0x08,  // 216:1432
  0x05, 0xA5, 0x0C, 0x07,  // 217:1445
  0x05, 0xB1, 0x0C, 0x07,  // 218:1457
  0x05, 0xBD, 0x0C, 0x07,  // 219:1469
  0x05, 0xC9, 0x0C, 0x07,  // 220:1481
  0x05, 0xD5, 0x0D, 0x07,  // 221:1493
  0x05, 0xE2, 0x0B, 0x07,  // 222:1506
  0x05, 0xED, 0x0C, 0x06,  // 223:1517
  0x05, 0xF9, 0x0A, 0x06,  // 224:1529
  0x06, 0x03, 0x0A, 0x06,  // 225:1539
  0x06, 0x0D, 0x0A, 0x06,  // 226:1549
  0x06, 0x17, 0x0A, 0x06,  // 227:1559
  0x06, 0x21, 0x0A, 0x06,  // 228:1569
  0x06, 0x2B, 0x0A, 0x06,  // 229:1579
  0x06, 0x35, 0x10, 0x09,  // 230:1589
  0x06, 0x45, 0x0A, 0x05,  // 231:1605
  0x06, 0x4F, 0x0A, 0x06,  // 232:1615
  0x06, 0x59, 0x0A, 0x06,  // 233:1625
  0x06, 0x63, 0x0A, 0x06,  // 234:1635
  0x06, 0x6D, 0x0A, 0x06,  // 235:1645
  0x06, 0x77, 0x05, 0x03,  // 236:1655
  0x06, 0x7C, 0x04, 0x03,  // 237:1660
  0x06, 0x80, 0x05, 0x03,  // 238:1664
  0x06, 0x85, 0x05, 0x03,  // 239:1669
  0x06, 0x8A, 0x0A, 0x06,  // 240:1674
  0x06, 0x94, 0x0A, 0x06,  // 241:1684
  0x06, 0x9E, 0x0A, 0x06,  // 242:1694
  0x06, 0xA8, 0x0A, 0x06,  // 243:1704
  0x06, 0xB2, 0x0A, 0x06,  // 244:1714
  0x06, 0xBC, 0x0A, 0x06,  // 245:1724
  0x06, 0xC6, 0x0A, 0x06,  // 246:1734
  0x06, 0xD0, 0x09, 0x05,  // 247:1744
  0x06, 0xD9, 0x0A, 0x06,  // 248:1753
  0x06, 0xE3, 0x0A, 0x06,  // 249:1763
  0x06, 0xED, 0x0A, 0x06,  // 250:1773
  0x06, 0xF7, 0x0A, 0x06,  // 251:1783
  0x07, 0x01, 0x0A, 0x06,  // 252:1793
  0x07, 0x0B, 0x09, 0x05,  // 253:1803
  0x07, 0x14, 0x0A, 0x06,  // 254:1812
  0x07, 0x1E, 0x09, 0x05,  // 255:1822

  // Font Data:
  0x00,0x00,0xF8,0x02,  // 33
  0x38,0x00,0x00,0x00,0x38, // 34
  0xA0,0x03,0xE0,0x00,0xB8,0x03,0xE0,0x00,0xB8, // 35
  0x30,0x01,0x28,0x02,0xF8,0x07,0x48,0x02,0x90,0x01,  // 36
  0x00,0x00,0x30,0x00,0x48,0x00,0x30,0x03,0xC0,0x00,0xB0,0x01,0x48,0x02,0x80,0x01,  // 37
  0x80,0x01,0x50,0x02,0x68,0x02,0xA8,0x02,0x18,0x01,0x80,0x03,0x80,0x02,  // 38
  0x38, // 39
  0xE0,0x03,0x10,0x04,0x08,0x08,  // 40
  0x08,0x08,0x10,0x04,0xE0,0x03,  // 41
  0x28,0x00,0x18,0x00,0x28, // 42
  0x40,0x00,0x40,0x00,0xF0,0x01,0x40,0x00,0x40, // 43
  0x00,0x00,0x00,0x06,  // 44
  0x80,0x00,0x80, // 45
  0x00,0x00,0x00,0x02,  // 46
  0x00,0x03,0xE0,0x00,0x18, // 47
  0xF0,0x01,0x08,0x02,0x08,0x02,0x08,0x02,0xF0,0x01,  // 48
  0x00,0x00,0x20,0x00,0x10,0x00,0xF8,0x03,  // 49
  0x10,0x02,0x08,0x03,0x88,0x02,0x48,0x02,0x30,0x02,  // 50
  0x10,0x01,0x08,0x02,0x48,0x02,0x48,0x02,0xB0,0x01,  // 51
  0xC0,0x00,0xA0,0x00,0x90,0x00,0x88,0x00,0xF8,0x03,0x80, // 52
  0x60,0x01,0x38,0x02,0x28,0x02,0x28,0x02,0xC8,0x01,  // 53
  0xF0,0x01,0x28,0x02,0x28,0x02,0x28,0x02,0xD0,0x01,  // 54
  0x08,0x00,0x08,0x03,0xC8,0x00,0x38,0x00,0x08, // 55
  0xB0,0x01,0x48,0x02,0x48,0x02,0x48,0x02,0xB0,0x01,  // 56
  0x70,0x01,0x88,0x02,0x88,0x02,0x88,0x02,0xF0,0x01,  // 57
  0x00,0x00,0x20,0x02,  // 58
  0x00,0x00,0x20,0x06,  // 59
  0x00,0x00,0x40,0x00,0xA0,0x00,0xA0,0x00,0x10,0x01,  // 60
  0xA0,0x00,0xA0,0x00,0xA0,0x00,0xA0,0x00,0xA0, // 61
  0x00,0x00,0x10,0x01,0xA0,0x00,0xA0,0x00,0x40, // 62
  0x10,0x00,0x08,0x00,0x08,0x00,0xC8,0x02,0x48,0x00,0x30, // 63
  0x00,0x00,0xC0,0x03,0x30,0x04,0xD0,0x09,0x28,0x0A,0x28,0x0A,0xC8,0x0B,0x68,0x0A,0x10,0x05,0xE0,0x04,  // 64
  0x00,0x02,0xC0,0x01,0xB0,0x00,0x88,0x00,0xB0,0x00,0xC0,0x01,0x00,0x02,  // 65
  0x00,0x00,0xF8,0x03,0x48,0x02,0x48,0x02,0x48,0x02,0xF0,0x01,  // 66
  0x00,0x00,0xF0,0x01,0x08,0x02,0x08,0x02,0x08,0x02,0x10,0x01,  // 67
  0x00,0x00,0xF8,0x03,0x08,0x02,0x08,0x02,0x10,0x01,0xE0, // 68
  0x00,0x00,0xF8,0x03,0x48,0x02,0x48,0x02,0x48,0x02,0x48,0x02,  // 69
  0x00,0x00,0xF8,0x03,0x48,0x00,0x48,0x00,0x08, // 70
  0x00,0x00,0xE0,0x00,0x10,0x01,0x08,0x02,0x48,0x02,0x50,0x01,0xC0, // 71
  0x00,0x00,0xF8,0x03,0x40,0x00,0x40,0x00,0x40,0x00,0xF8,0x03,  // 72
  0x00,0x00,0xF8,0x03,  // 73
  0x00,0x03,0x00,0x02,0x00,0x02,0xF8,0x01,  // 74
  0x00,0x00,0xF8,0x03,0x80,0x00,0x60,0x00,0x90,0x00,0x08,0x01,0x00,0x02,  // 75
  0x00,0x00,0xF8,0x03,0x00,0x02,0x00,0x02,0x00,0x02,0x00,0x02,  // 76
  0x00,0x00,0xF8,0x03,0x30,0x00,0xC0,0x01,0x00,0x02,0xC0,0x01,0x30,0x00,0xF8,0x03,  // 77
  0x00,0x00,0xF8,0x03,0x30,0x00,0x40,0x00,0x80,0x01,0xF8,0x03,  // 78
  0x00,0x00,0xF0,0x01,0x08,0x02,0x08,0x02,0x08,0x02,0x08,0x02,0xF0,0x01,  // 79
  0x00,0x00,0xF8,0x03,0x48,0x00,0x48,0x00,0x48,0x00,0x30, // 80
  0x00,0x00,0xF0,0x01,0x08,0x02,0x08,0x02,0x08,0x03,0x08,0x03,0xF0,0x02,  // 81
  0x00,0x00,0xF8,0x03,0x48,0x00,0x48,0x00,0xC8,0x00,0x30,0x03,  // 82
  0x00,0x00,0x30,0x01,0x48,0x02,0x48,0x02,0x48,0x02,0x90,0x01,  // 83
  0x00,0x00,0x08,0x00,0x08,0x00,0xF8,0x03,0x08,0x00,0x08, // 84
  0x00,0x00,0xF8,0x01,0x00,0x02,0x00,0x02,0x00,0x02,0xF8,0x01,  // 85
  0x08,0x00,0x70,0x00,0x80,0x01,0x00,0x02,0x80,0x01,0x70,0x00,0x08, // 86
  0x18,0x00,0xE0,0x01,0x00,0x02,0xF0,0x01,0x08,0x00,0xF0,0x01,0x00,0x02,0xE0,0x01,0x18, // 87
  0x00,0x02,0x08,0x01,0x90,0x00,0x60,0x00,0x90,0x00,0x08,0x01,0x00,0x02,  // 88
  0x08,0x00,0x10,0x00,0x20,0x00,0xC0,0x03,0x20,0x00,0x10,0x00,0x08, // 89
  0x08,0x03,0x88,0x02,0xC8,0x02,0x68,0x02,0x38,0x02,0x18,0x02,  // 90
  0x00,0x00,0xF8,0x0F,0x08,0x08,  // 91
  0x18,0x00,0xE0,0x00,0x00,0x03,  // 92
  0x08,0x08,0xF8,0x0F,  // 93
  0x40,0x00,0x30,0x00,0x08,0x00,0x30,0x00,0x40, // 94
  0x00,0x08,0x00,0x08,0x00,0x08,0x00,0x08,0x00,0x08,0x00,0x08,  // 95
  0x08,0x00,0x10, // 96
  0x00,0x00,0x00,0x03,0xA0,0x02,0xA0,0x02,0xE0,0x03,  // 97
  0x00,0x00,0xF8,0x03,0x20,0x02,0x20,0x02,0xC0,0x01,  // 98
  0x00,0x00,0xC0,0x01,0x20,0x02,0x20,0x02,0x40,0x01,  // 99
  0x00,0x00,0xC0,0x01,0x20,0x02,0x20,0x02,0xF8,0x03,  // 100
  0x00,0x00,0xC0,0x01,0xA0,0x02,0xA0,0x02,0xC0,0x02,  // 101
  0x20,0x00,0xF0,0x03,0x28, // 102
  0x00,0x00,0xC0,0x05,0x20,0x0A,0x20,0x0A,0xE0,0x07,  // 103
  0x00,0x00,0xF8,0x03,0x20,0x00,0x20,0x00,0xC0,0x03,  // 104
  0x00,0x00,0xE8,0x03,  // 105
  0x00,0x08,0xE8,0x07,  // 106
  0xF8,0x03,0x80,0x00,0xC0,0x01,0x20,0x02,  // 107
  0x00,0x00,0xF8,0x03,  // 108
  0x00,0x00,0xE0,0x03,0x20,0x00,0x20,0x00,0xE0,0x03,0x20,0x00,0x20,0x00,0xC0,0x03,  // 109
  0x00,0x00,0xE0,0x03,0x20,0x00,0x20,0x00,0xC0,0x03,  // 110
  0x00,0x00,0xC0,0x01,0x20,0x02,0x20,0x02,0xC0,0x01,  // 111
  0x00,0x00,0xE0,0x0F,0x20,0x02,0x20,0x02,0xC0,0x01,  // 112
  0x00,0x00,0xC0,0x01,0x20,0x02,0x20,0x02,0xE0,0x0F,  // 113
  0x00,0x00,0xE0,0x03,0x20, // 114
  0x40,0x02,0xA0,0x02,0xA0,0x02,0x20,0x01,  // 115
  0x20,0x00,0xF8,0x03,0x20,0x02,  // 116
  0x00,0x00,0xE0,0x01,0x00,0x02,0x00,0x02,0xE0,0x03,  // 117
  0x20,0x00,0xC0,0x01,0x00,0x02,0xC0,0x01,0x20, // 118
  0xE0,0x01,0x00,0x02,0xC0,0x01,0x20,0x00,0xC0,0x01,0x00,0x02,0xE0,0x01,  // 119
  0x20,0x02,0x40,0x01,0x80,0x00,0x40,0x01,0x20,0x02,  // 120
  0x20,0x00,0xC0,0x09,0x00,0x06,0xC0,0x01,0x20, // 121
  0x20,0x02,0x20,0x03,0xA0,0x02,0x60,0x02,0x20,0x02,  // 122
  0x80,0x00,0x78,0x0F,0x08,0x08,  // 123
  0x00,0x00,0xF8,0x0F,  // 124
  0x08,0x08,0x78,0x0F,0x80, // 125
  0xC0,0x00,0x40,0x00,0xC0,0x00,0x80,0x00,0xC0, // 126
  0x00,0x00,0xA0,0x0F,  // 161
  0x00,0x00,0xC0,0x01,0xA0,0x0F,0x78,0x02,0x40,0x01,  // 162
  0x40,0x02,0x70,0x03,0xC8,0x02,0x48,0x02,0x08,0x02,0x10,0x02,  // 163
  0x00,0x00,0xE0,0x01,0x20,0x01,0x20,0x01,0xE0,0x01,  // 164
  0x48,0x01,0x70,0x01,0xC0,0x03,0x70,0x01,0x48,0x01,  // 165
  0x00,0x00,0x38,0x0F,  // 166
  0xD0,0x04,0x28,0x09,0x48,0x09,0x48,0x0A,0x90,0x05,  // 167
  0x08,0x00,0x00,0x00,0x08, // 168
  0xE0,0x00,0x10,0x01,0x48,0x02,0xA8,0x02,0xA8,0x02,0x10,0x01,0xE0, // 169
  0x68,0x00,0x68,0x00,0x68,0x00,0x78, // 170
  0x00,0x00,0x80,0x01,0x40,0x02,0x80,0x01,0x40,0x02,  // 171
  0x20,0x00,0x20,0x00,0x20,0x00,0x20,0x00,0xE0, // 172
  0x80,0x00,0x80, // 173
  0xE0,0x00,0x10,0x01,0xE8,0x02,0x68,0x02,0xC8,0x02,0x10,0x01,0xE0, // 174
  0x02,0x00,0x02,0x00,0x02,0x00,0x02,0x00,0x02,0x00,0x02, // 175
  0x00,0x00,0x38,0x00,0x28,0x00,0x38, // 176
  0x40,0x02,0x40,0x02,0xF0,0x03,0x40,0x02,0x40,0x02,  // 177
  0x48,0x00,0x68,0x00,0x58, // 178
  0x48,0x00,0x58,0x00,0x68, // 179
  0x00,0x00,0x10,0x00,0x08, // 180
  0x00,0x00,0xE0,0x0F,0x00,0x02,0x00,0x02,0xE0,0x03,  // 181
  0x70,0x00,0xF8,0x0F,0x08,0x00,0xF8,0x0F,0x08, // 182
  0x00,0x00,0x40, // 183
  0x00,0x00,0x00,0x14,0x00,0x18,  // 184
  0x00,0x00,0x10,0x00,0x78, // 185
  0x30,0x00,0x48,0x00,0x48,0x00,0x30, // 186
  0x00,0x00,0x40,0x02,0x80,0x01,0x40,0x02,0x80,0x01,  // 187
  0x00,0x00,0x10,0x02,0x78,0x01,0xC0,0x00,0x20,0x01,0x90,0x01,0xC8,0x03,0x00,0x01,  // 188
  0x00,0x00,0x10,0x02,0x78,0x01,0x80,0x00,0x60,0x00,0x50,0x02,0x48,0x03,0xC0,0x02,  // 189
  0x48,0x00,0x58,0x00,0x68,0x03,0x80,0x00,0x60,0x01,0x90,0x01,0xC8,0x03,0x00,0x01,  // 190
  0x00,0x00,0x00,0x06,0x00,0x09,0xA0,0x09,0x00,0x04,  // 191
  0x00,0x02,0xC0,0x01,0xB0,0x00,0x89,0x00,0xB2,0x00,0xC0,0x01,0x00,0x02,  // 192
  0x00,0x02,0xC0,0x01,0xB0,0x00,0x8A,0x00,0xB1,0x00,0xC0,0x01,0x00,0x02,  // 193
  0x00,0x02,0xC0,0x01,0xB2,0x00,0x89,0x00,0xB2,0x00,0xC0,0x01,0x00,0x02,  // 194
  0x00,0x02,0xC2,0x01,0xB1,0x00,0x8A,0x00,0xB1,0x00,0xC0,0x01,0x00,0x02,  // 195
  0x00,0x02,0xC0,0x01,0xB2,0x00,0x88,0x00,0xB2,0x00,0xC0,0x01,0x00,0x02,  // 196
  0x00,0x02,0xC0,0x01,0xBE,0x00,0x8A,0x00,0xBE,0x00,0xC0,0x01,0x00,0x02,  // 197
  0x00,0x03,0xC0,0x00,0xE0,0x00,0x98,0x00,0x88,0x00,0xF8,0x03,0x48,0x02,0x48,0x02,0x48,0x02,  // 198
  0x00,0x00,0xF0,0x01,0x08,0x02,0x08,0x16,0x08,0x1A,0x10,0x01,  // 199
  0x00,0x00,0xF8,0x03,0x49,0x02,0x4A,0x02,0x48,0x02,0x48,0x02,  // 200
  0x00,0x00,0xF8,0x03,0x48,0x02,0x4A,0x02,0x49,0x02,0x48,0x02,  // 201
  0x00,0x00,0xFA,0x03,0x49,0x02,0x4A,0x02,0x48,0x02,0x48,0x02,  // 202
  0x00,0x00,0xF8,0x03,0x4A,0x02,0x48,0x02,0x4A,0x02,0x48,0x02,  // 203
  0x00,0x00,0xF9,0x03,0x02, // 204
  0x02,0x00,0xF9,0x03,  // 205
  0x01,0x00,0xFA,0x03,  // 206
  0x02,0x00,0xF8,0x03,0x02, // 207
  0x40,0x00,0xF8,0x03,0x48,0x02,0x48,0x02,0x10,0x01,0xE0, // 208
  0x00,0x00,0xFA,0x03,0x31,0x00,0x42,0x00,0x81,0x01,0xF8,0x03,  // 209
  0x00,0x00,0xF0,0x01,0x08,0x02,0x09,0x02,0x0A,0x02,0x08,0x02,0xF0,0x01,  // 210
  0x00,0x00,0xF0,0x01,0x08,0x02,0x0A,0x02,0x09,0x02,0x08,0x02,0xF0,0x01,  // 211
  0x00,0x00,0xF0,0x01,0x08,0x02,0x0A,0x02,0x09,0x02,0x0A,0x02,0xF0,0x01,  // 212
  0x00,0x00,0xF0,0x01,0x0A,0x02,0x09,0x02,0x0A,0x02,0x09,0x02,0xF0,0x01,  // 213
  0x00,0x00,0xF0,0x01,0x0A,0x02,0x08,0x02,0x0A,0x02,0x08,0x02,0xF0,0x01,  // 214
  0x10,0x01,0xA0,0x00,0xE0,0x00,0xA0,0x00,0x10,0x01,  // 215
  0x00,0x00,0xF0,0x02,0x08,0x03,0xC8,0x02,0x28,0x02,0x18,0x03,0xE8, // 216
  0x00,0x00,0xF8,0x01,0x01,0x02,0x02,0x02,0x00,0x02,0xF8,0x01,  // 217
  0x00,0x00,0xF8,0x01,0x02,0x02,0x01,0x02,0x00,0x02,0xF8,0x01,  // 218
  0x00,0x00,0xF8,0x01,0x02,0x02,0x01,0x02,0x02,0x02,0xF8,0x01,  // 219
  0x00,0x00,0xF8,0x01,0x02,0x02,0x00,0x02,0x02,0x02,0xF8,0x01,  // 220
  0x08,0x00,0x10,0x00,0x20,0x00,0xC2,0x03,0x21,0x00,0x10,0x00,0x08, // 221
  0x00,0x00,0xF8,0x03,0x10,0x01,0x10,0x01,0x10,0x01,0xE0, // 222
  0x00,0x00,0xF0,0x03,0x08,0x01,0x48,0x02,0xB0,0x02,0x80,0x01,  // 223
  0x00,0x00,0x00,0x03,0xA4,0x02,0xA8,0x02,0xE0,0x03,  // 224
  0x00,0x00,0x00,0x03,0xA8,0x02,0xA4,0x02,0xE0,0x03,  // 225
  0x00,0x00,0x00,0x03,0xA8,0x02,0xA4,0x02,0xE8,0x03,  // 226
  0x00,0x00,0x08,0x03,0xA4,0x02,0xA8,0x02,0xE4,0x03,  // 227
  0x00,0x00,0x00,0x03,0xA8,0x02,0xA0,0x02,0xE8,0x03,  // 228
  0x00,0x00,0x00,0x03,0xAE,0x02,0xAA,0x02,0xEE,0x03,  // 229
  0x00,0x00,0x40,0x03,0xA0,0x02,0xA0,0x02,0xC0,0x01,0xA0,0x02,0xA0,0x02,0xC0,0x02,  // 230
  0x00,0x00,0xC0,0x01,0x20,0x16,0x20,0x1A,0x40,0x01,  // 231
  0x00,0x00,0xC0,0x01,0xA4,0x02,0xA8,0x02,0xC0,0x02,  // 232
  0x00,0x00,0xC0,0x01,0xA8,0x02,0xA4,0x02,0xC0,0x02,  // 233
  0x00,0x00,0xC0,0x01,0xA8,0x02,0xA4,0x02,0xC8,0x02,  // 234
  0x00,0x00,0xC0,0x01,0xA8,0x02,0xA0,0x02,0xC8,0x02,  // 235
  0x00,0x00,0xE4,0x03,0x08, // 236
  0x08,0x00,0xE4,0x03,  // 237
  0x08,0x00,0xE4,0x03,0x08, // 238
  0x08,0x00,0xE0,0x03,0x08, // 239
  0x00,0x00,0xC0,0x01,0x28,0x02,0x38,0x02,0xE0,0x01,  // 240
  0x00,0x00,0xE8,0x03,0x24,0x00,0x28,0x00,0xC4,0x03,  // 241
  0x00,0x00,0xC0,0x01,0x24,0x02,0x28,0x02,0xC0,0x01,  // 242
  0x00,0x00,0xC0,0x01,0x28,0x02,0x24,0x02,0xC0,0x01,  // 243
  0x00,0x00,0xC0,0x01,0x28,0x02,0x24,0x02,0xC8,0x01,  // 244
  0x00,0x00,0xC8,0x01,0x24,0x02,0x28,0x02,0xC4,0x01,  // 245
  0x00,0x00,0xC0,0x01,0x28,0x02,0x20,0x02,0xC8,0x01,  // 246
  0x40,0x00,0x40,0x00,0x50,0x01,0x40,0x00,0x40, // 247
  0x00,0x00,0xC0,0x02,0xA0,0x03,0x60,0x02,0xA0,0x01,  // 248
  0x00,0x00,0xE0,0x01,0x04,0x02,0x08,0x02,0xE0,0x03,  // 249
  0x00,0x00,0xE0,0x01,0x08,0x02,0x04,0x02,0xE0,0x03,  // 250
  0x00,0x00,0xE8,0x01,0x04,0x02,0x08,0x02,0xE0,0x03,  // 251
  0x00,0x00,0xE0,0x01,0x08,0x02,0x00,0x02,0xE8,0x03,  // 252
  0x20,0x00,0xC0,0x09,0x08,0x06,0xC4,0x01,0x20, // 253
  0x00,0x00,0xF8,0x0F,0x20,0x02,0x20,0x02,0xC0,0x01,  // 254
  0x20,0x00,0xC8,0x09,0x00,0x06,0xC8,0x01,0x20  // 255
};
