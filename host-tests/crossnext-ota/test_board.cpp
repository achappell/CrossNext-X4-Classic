#include "../../src/network/FirmwareBoardTag.h"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <string>
#include <cstdio>

int main() {
  const std::string matching = std::string("prefix") + board_tag::TAG + "suffix";
  const std::string wrong = std::string("CROSSPOINT-BOARD-V1:") +
      (std::strcmp(CROSSPOINT_BOARD_NAME, "x4c") == 0 ? "x4pro;" : "x4c;");
  for (size_t chunk = 1; chunk <= 32; ++chunk) {
    for (const auto& image : {matching, std::string("untagged legacy image"), wrong, matching + wrong}) {
      board_tag::Scanner scanner;
      for (size_t at = 0; at < image.size(); at += chunk)
        scanner.feed(reinterpret_cast<const uint8_t*>(image.data() + at), std::min(chunk, image.size() - at));
      assert(scanner.mismatch() == (image == wrong || image == matching + wrong));
    }
  }
  std::printf("board-tag %s: 128 streaming compatibility cases passed\n", CROSSPOINT_BOARD_NAME);
}
