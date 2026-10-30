#pragma once
#include "PngToBmpConverter.h"
class JpegToBmpConverter {
 public:
  static bool jpegFileToGrayBmpStreamNative(HalFile& in, Print& out) {
    return PngToBmpConverter::pngFileToGrayBmpStreamNative(in, out);
  }
  static bool jpegFileTo1BitBmpStreamFitWithin(HalFile& in, Print& out, int w, int h) {
    return PngToBmpConverter::pngFileTo1BitBmpStreamFitWithin(in, out, w, h);
  }
};
