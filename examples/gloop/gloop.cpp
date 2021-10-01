#include <math.h>
#include "pico/time.h"
#include "picosystem.hpp"

using namespace picosystem;

int main() {
  init_picosystem();

  set_backlight(255);

  screen.blend_mode(COPY);

  uint8_t i = 0;

  uint32_t duration = 0;
  while(true) {
    screen.pen = Pen(i, 7, 0);
    screen.clear();

    screen.pen = Pen(15, 15, 15);
    //screen.circle(Point(100, 100), 50);

    screen.pen = Pen(15, 15, 15);
    screen.text("ALSO", 10, 1, 200);
    screen.text("this is a test", 10, 20, 200);
    screen.text("ms: " + std::to_string(duration), 10, 10, 100);


    screen.text("HERE", 10, 50, 200);

    screen.pen = Pen(15, 15, 15);
    for(int j = 0; j < duration; j++) {
      screen.rectangle(j * 10 + 2, 2, 8, 8);
    }

    uint32_t start = time();
    flip();


    uint32_t end = time();

    duration = end - start;
    

    i++;
  }
}
