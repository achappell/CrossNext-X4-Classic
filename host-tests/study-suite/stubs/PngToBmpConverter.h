#pragma once
#include "HalStorage.h"
// The firmware/browser build exercises the real decoder. This fixed 2x2 BMP
// isolates media indexing, top-down rows and zero-is-black palette handling.
class PngToBmpConverter {
 public:
  static bool pngFileToGrayBmpStreamNative(HalFile&, Print& out) {
    // A decoder fixture, not a decoder: retain the same two native rows as the
    // one-bit fixture below, expressed as an 8-bit top-down grayscale BMP.
    uint8_t b[1086]{};
    b[0] = 'B';
    b[1] = 'M';
    b[2] = 0x3e;
    b[3] = 4;
    b[10] = 0x36;
    b[11] = 4;
    b[14] = 40;
    b[18] = 2;
    b[22] = 0xfe;
    b[23] = b[24] = b[25] = 0xff;
    b[26] = 1;
    b[28] = 8;
    for (int n = 0; n < 256; ++n) b[54 + n * 4] = b[55 + n * 4] = b[56 + n * 4] = n;
    b[1078] = 255;
    b[1079] = 0;
    b[1082] = 0;
    b[1083] = 255;
    return out.write(b, sizeof(b)) == sizeof(b);
  }
  static bool pngFileTo1BitBmpStreamFitWithin(HalFile&, Print& out, int, int) {
    uint8_t b[70]{};
    b[0] = 'B';
    b[1] = 'M';
    b[2] = 70;
    b[10] = 62;
    b[14] = 40;
    b[18] = 2;
    b[22] = 0xfe;
    b[23] = b[24] = b[25] = 0xff;
    b[26] = 1;
    b[28] = 1;
    b[58] = b[59] = b[60] = 255;
    b[62] = 0x40;
    b[66] = 0x80;
    return out.write(b, sizeof(b)) == sizeof(b);
  }
};
