#include <math.h>
#include <array>
#include "pico/time.h"
#include "picosystem.hpp"

using namespace picosystem;

struct circle {
  float x, y;   // position
  float vx, vy; // movement vector
};

std::array<circle, 200> circles;

void init() {
  // perform any intialisation for your game here

  for(auto &c: circles) {
    c.x = std::rand() % 220;
    c.y = std::rand() % 220;

    c.vx = (float(std::rand() % 100) / 100.0f) - 0.5f;
    c.vy = (float(std::rand() % 100) / 100.0f) - 0.5f;

    c.vx *= 5.0f;
    c.vy *= 5.0f;
  }

  blend_mode(COPY);
  pen(1, 2, 3);
  clear();

}

void update(uint32_t time_ms) {
  // perform any world state updates here, always called at 100fps

  // you should never do drawing operations during update() as it
  // may causing tearing/artifacts if they occur mid screen udpate

  if(pressed(UP)) {
    reset_to_dfu();
  }

  for(auto &c: circles) {
    c.x += c.vx;
    c.y += c.vy;

    if(c.x > 230 || c.x < -10) {
      c.vx *= -1.0f;
    }

    if(c.y > 230 || c.y < -10) {
      c.vy *= -1.0f;
    }
  }

  led(time_ms / 10, 0, 0);
}

void render() {
  // do all drawing here based on current world state

/*  blend_mode(COPY);
  pen(1, 2, 3);
  clear();*/

/*
  blend_mode(BLEND);
  for(auto &c: circles) {
    uint8_t r = (std::rand() % 5) + 5;
    pen(8, 10, 12, 4);
    rectangle(c.x, c.y, 20, 20);
  }*/


  pen(0, 0, 0);
  rectangle(0, 0, 240, 50);
  pen(15, 15, 15);
  text(str(time()), 10, 10);
  text("battery: " + str(charge(), 2), 10, 30);

  uint32_t x = time() / 10000;
  uint32_t y = charge() * 100;
  pen(0, 15, 0);
  rectangle(x, y + 50, 2, 2);



/*
  static uint32_t flip_duration = 0;
  static uint32_t draw_duration = 0;


    blend_mode(COPY);
    //std::srand(20);

    uint32_t draw_start = time_us_64();
    for(int j = 0; j < 1000; j++) {
      pen(std::rand() % 16, std::rand() % 16, std::rand() % 16, 4);
      rectangle(std::rand() % 220, std::rand() % 220, 20, 20);
    }
    draw_duration = time_us_64() - draw_start;


    /*screen.blend_mode(COPY);
    screen.pen = Pen(i / 10, 7, 0);
    screen.clear();

    screen.pen = Pen(15, 15, 15);
    //screen.circle(Point(100, 100), 50);

    screen.pen = Pen(15, 15, 15);
    screen.text("ALSO", 10, 1, 200);
    screen.text("this is a test", 10, 20, 200);



    screen.text("HERE", 10, 50, 200);

    screen.pen = Pen(15, 15, 15);
    for(int j = 0; j < duration; j++) {
      screen.rectangle(j * 10 + 2, 2, 8, 8);
    }

    screen.blend_mode(BLEND);
    screen.pen = Pen(15, 9, 3, 2);

    screen.circle(Point(200, 200), 30);
    screen.circle(Point(190, 200), 30);
    screen.circle(Point(200, 190), 30);

    uint32_t xx = (sin(float(i) / 10.0f) * 30.0f) + 120;
    uint32_t yy = (cos(float(i) / 10.0f) * 30.0f) + 120;
    screen.circle(Point(xx, yy), 30);


*/
/*
    pen(15, 15, 15);

    if(pressed(A)) {
      text("PRESSED", 50, 100);
    }



    rectangle(10, (time / 10) % 220, 10, 10);
    text("draw ms: " + to_string(float(draw_duration) / 1000.0f, 2), 50, 50);
*/
}
