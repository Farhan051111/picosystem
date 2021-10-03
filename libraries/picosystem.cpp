#include <stdio.h>
#include <cstdlib>
#include <climits>

#include <math.h>
#include <string.h>

#include <vector>

#include "picosystem.hpp"

namespace picosystem {

  pen_t _pen;
  pen_t _framebuffer[240 * 240];
  int32_t _cx = 0, _cy = 0, _cw = 240, _ch = 240;
  blend_func_t _bf = BLEND;
  buffer_t _fb{.w = 240, .h = 240, .data = _framebuffer};

  void COPY(pen_t *source, uint32_t source_step, pen_t *dest, uint32_t count) {
    if(source_step) {
      // for blits (i.e. source_step == 1) we're unlikely to do much
      // better than the built in memcpy implementation!
      memcpy(dest, source, count * 2);
    }else{
      // for pen drawing (i.e. source_step == 0)

      // for longer runs of pixels we can almost double the performance
      // by copying two pixels at a time

      // align destination to 32bits
      if((uintptr_t(dest) & 0b11) && count) {
        *dest++ = *source;
        count--;
      }

      if(!count) return;

      // two pixels at a time
      uint32_t dpen = (*source << 16) | *source;
      uint32_t *dwd = (uint32_t *)dest;
      while(count > 1) {
        *dwd++ = dpen;
        count -= 2;
      }

      // finish off with last pixel if needed
      if(count) {
        *dest = *source;
      }
    }
  }

  void BLEND(pen_t *source, uint32_t source_step, pen_t *dest, uint32_t count) {
    // unpack source into 32 bits with space for alpha multiplication
    // we start with ggggbbbbaaaarrrr and end up with
    // ------------gggg----bbbb----rrrr
    uint32_t s = (*source | ((*source & 0xf000) << 4)) & 0xf0f0f;
    uint8_t sa = (*source &0xf0) >> 4;

    while(count--) {
      // unpack dest into 32 bits with space for alpha multiplication
      // we start with ggggbbbbaaaarrrr and end up with
      // ------------gggg----bbbb----rrrr
      uint32_t d = (*dest | ((*dest & 0xf000) << 4)) & 0xf0f0f;
      uint8_t da = (*dest & 0xf0); // extract alpha to add back later

      // blend the three channels in one go
      d = d + ((sa * (s - d) + 0x070707) >> 4);

      // reconstruct blended colour components into original format
      //dest->v &= 0x00f0; // mask out r, g, b channels
      //dest->v |= (d & 0x0f0f) | ((d & 0xf0000) >> 4);
      *dest = (d & 0x0f0f) | ((d & 0xf0000) >> 4) | da;
      dest++;

      if(source_step == 1) {
        source += source_step;

        // unpack and prepare next source pixel
        s = (*source | ((*source & 0xf000) << 4)) & 0xf0f0f;
        sa = (*source & 0xf0) >> 4;
      }
    }
  }


  pen_t create_pen(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
    // pen_t will contain pixel data in the format aaaarrrrggggbbbb
    return (r & 0xf) | ((a & 0xf) << 4) | ((b & 0xf) << 8) | ((g & 0xf) << 12);
  }

  void pen(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    _pen = create_pen(r, g, b, a);
  }
  void pen(uint8_t r, uint8_t g, uint8_t b) {
    _pen = create_pen(r, g, b, 255);
  }
  void pen(pen_t p) { _pen = p; }

  void clip(int32_t x, int32_t y, uint32_t w, uint32_t h) {
    _cx = x; _cy = y; _cw = w; _ch = h;
  }

  void blend_mode(blend_func_t bf) {_bf = bf;}

  void clear() {
    rectangle(0, 0, 240, 240);
  }

/*
return Rect(std::max(x, r.x),
                  std::max(y, r.y),
                  std::min(x + w, r.x + r.w) - std::max(x, r.x),
                  std::min(y + h, r.y + r.h) - std::max(y, r.y));
                  */

  void clip_rect(int32_t &x, int32_t &y, int32_t &w, int32_t &h) {
    int32_t mx = std::max(x, _cx);
    int32_t my = std::max(y, _cy);
    w = std::max(0L, std::min(x + w, _cx + _cw) - mx);
    h = std::max(0L, std::min(y + h, _cy + _ch) - my);
    x = mx;
    y = my;
  }

  bool clip_contains(int32_t x, int32_t y) {
    return x >= _cx && x < _cx + _cw && y >= _cy && y < _cy + _cw;
  }

  uint32_t offset(int32_t x, int32_t y) {
    return x + y * _fb.w;
  }

  void rectangle(int32_t x, int32_t y, int32_t w, int32_t h) {
    clip_rect(x, y, w, h);

    pen_t *dest = _fb.data + offset(x, y);

    while(h--) {
      _bf(&_pen, 0, dest, w); // draw row
      dest += _fb.w;
    }
  }

  std::string str(float v, uint8_t precision) {
    static char b[32];
    snprintf(b, 32, "%.*f", precision, v);
    return b;
  }

  std::string str(uint32_t v) {
    static char b[32];
    snprintf(b, 32, "%d", v);
    return b;
  }

  void text(const std::string &t, int32_t x, int32_t y) {
    uint32_t co = 0, lo = 0; // character and line (if wrapping) offset
    uint32_t wrap = INT_MAX; // should be a flag?

    for(std::size_t i = 0, len = t.length(); i < len; i++) {
      const uint8_t *d = &font8x8_basic[t[i]][0];
      for(uint8_t cy = 0; cy < 8; cy++) {
        for(uint8_t cx = 0; cx < 8; cx++) {
          if((1U << cx) & *d && clip_contains(x + cx + co, y + cy + lo)) {
            _bf(&_pen, 0, _fb.data + offset(x + cx + co, y + cy + lo), 1);
          }
        }

        d++;
      }

      // search ahead for next space character so we can decide if we need to wrap or not
      if(t[i] == ' ') {
        size_t next = t.find(' ', i + 1);
        if(co + (next - i + 1) * 9 > wrap) {
          co = 0;
          lo += 9;
        }else{
          co += 5;
        }
      }else{
        co += 9;
      }
    }
  }


}

using namespace picosystem;

// main entry point - the users' code will be automatically
// called when they implement the init(), update(), and render()
// functions in their project

int main() {
  init_hardware();

  backlight(255);

  // call users init() function so they can perform any needed
  // setup for world state etc
  init();

  uint32_t update_rate_ms = 10;
  uint32_t pending_update_ms = 0;
  uint32_t last_ms = time();

  while(true) {
    uint32_t ms = time();

    // work out how many milliseconds of updates we're waiting
    // to process and then call the users update() function as
    // many times as needed to catch up
    pending_update_ms += (ms - last_ms);
    while(pending_update_ms >= update_rate_ms) {
      update(ms - pending_update_ms);
      pending_update_ms -= update_rate_ms;
    }

    // if current flipping the framebuffer in the background
    // then wait until that is complete before allow the user
    // to render
    while(is_flipping()) {}

    // call user render function to draw world
    render();

    // wait for the screen to vsync before triggering flip
    // to ensure no tearing
    wait_vsync();

    // flip the framebuffer to the screen
    flip();

    last_ms = ms;
  }


}
