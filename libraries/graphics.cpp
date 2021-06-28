#include <math.h>
#include <cstring>
#include <cwchar>

#include "hardware/interp.h"

#include "picosystem.hpp"

namespace picosystem {

  void init_graphics() {
  }

  void COPY(Pen *source, uint32_t source_step, Pen *dest, uint32_t count) {
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
      uint32_t dpen = (source->v << 16) | source->v;
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

  void BLEND(Pen *source, uint32_t source_step, Pen *dest, uint32_t count) {
    // unpack source into 32 bits with space for alpha multiplication
    // we start with ggggbbbbaaaarrrr and end up with
    // ------------gggg----bbbb----rrrr
    uint32_t s = (source->v | ((source->v & 0xf000) << 4)) & 0xf0f0f;
    uint8_t sa = (source->v &0xf0) >> 4;

    while(count--) {
      // unpack dest into 32 bits with space for alpha multiplication
      // we start with ggggbbbbaaaarrrr and end up with
      // ------------gggg----bbbb----rrrr
      uint32_t d = (dest->v | ((dest->v & 0xf000) << 4)) & 0xf0f0f;
      uint8_t da = (dest->v & 0xf0); // extract alpha to add back later

      // blend the three channels in one go
      d = d + ((sa * (s - d) + 0x070707) >> 4);

      // reconstruct blended colour components into original format
      //dest->v &= 0x00f0; // mask out r, g, b channels
      //dest->v |= (d & 0x0f0f) | ((d & 0xf0000) >> 4);
      dest->v = (d & 0x0f0f) | ((d & 0xf0000) >> 4) | da;
      dest++;

      if(source_step == 1) {
        source += source_step;

        // unpack and prepare next source pixel
        s = (source->v | ((source->v & 0xf000) << 4)) & 0xf0f0f;
        sa = (source->v &0xf0) >> 4;
      }
    }
  }

  Pen::Pen(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    // order looks odd because uint16_t is little endian. the resulting
    // two bytes will be contain pixel data in the format aaaarrrrggggbbbb
    v = (r & 0xf) | ((a & 0xf) << 4) | ((b & 0xf) << 8) | ((g & 0xf) << 12);
  }

  Pen::Pen(uint16_t v) : v(v) {}

  Pen Pen::from_hsv(float h, float s, float v) {
    float i = floor(h * 6.0f);
    float f = h * 6.0f - i;
    v *= 15.0f;
    uint8_t p = v * (1.0f - s);
    uint8_t q = v * (1.0f - f * s);
    uint8_t t = v * (1.0f - (1.0f - f) * s);

    switch (int(i) % 6) {
      case 0: return Pen(v, t, p); break;
      case 1: return Pen(q, v, p); break;
      case 2: return Pen(p, v, t); break;
      case 3: return Pen(p, q, v); break;
      case 4: return Pen(t, p, v); break;
      case 5: return Pen(v, p, q); break;
    }

    return Pen(0, 0, 0);
  }

  void Surface::blend_mode(BlendFunc nbf) {
    bf = nbf;
  }

  void Surface::clear() {
    bf(&pen, 0, data, pc);
  }

  void Surface::pixel(int32_t x, int32_t y) {
    if(contains(x, y)) {
      bf(&pen, 0, data + offset(x, y), 1);
    }
  }

  void Surface::rectangle(int32_t rx, int32_t ry, int32_t rw, int32_t rh) {
    clip_rect(rx, ry, rw, rh);

    Pen *d = data + offset(rx, ry);

    while(rh--) {
      bf(&pen, 0, d, rw); // draw row
      d += w;
    }
  }

  void Surface::rectangle(Rect r) {
    clip_rect(r.x, r.y, r.w, r.h);

    Pen *d = data + offset(r.x, r.y);
    uint32_t h = r.h;

    while(h--) {
      bf(&pen, 0, d, r.w); // draw row
      d += w;
    }
  }

  // blit from one surface to another
  void Surface::blit(const Surface &source, const Rect &from, const Point &to) {
    Rect dr = clip.intersection(Rect(to.x, to.y, from.w, from.h));  // clipped destination rect

    if(dr.empty()) return;

    int32_t left   = dr.x - to.x;
    int32_t top    = dr.y - to.y;
    int32_t right  = from.w - (from.w - dr.w) + left - 1;
    int32_t bottom = from.h - (from.h - dr.h) + top - 1;

    // transforms
    uint32_t rows = dr.h;

    uint32_t dest_offset = offset(dr.x, dr.y);
    Pen *s = source.data + source.offset(from.x + left, from.y + top);

    while(rows--) {
      bf(s, 1, data + dest_offset, dr.w); // draw row

      dest_offset += w;
      s += source.w;
    }
  }

  void Surface::text(const std::string &t, int32_t x, int32_t y, int32_t wrap) {
    uint32_t co = 0, lo = 0; // character and line (if wrapping) offset

    for(std::size_t i = 0, len = t.length(); i < len; i++) {
      const uint8_t *d = &font_data[(uint8_t)(t[i] - 32) * 5];
      for(uint8_t cx = 0; cx < 5; cx++) {
        for(uint8_t cy = 0; cy < 5; cy++) {
          if((1U << cy) & *d && contains(x + cx + co, y + cy + lo)) {
            bf(&pen, 0, data + offset(x + cx + co, y + cy + lo), 1);
          }
        }

        d++;
      }

      // search ahead for next space character so we can decide if we need to wrap or not
      if(t[i] == ' ') {
        size_t next = t.find(' ', i + 1);
        if(co + (next - i + 1) * 6 > wrap) {
          co = 0;
          lo += 6;
        }else{
          co += 3;
        }
      }else{
        co += 6;
      }
    }
  }

  Pen* Surface::ptr(const Rect &r) {
    return screen.data + r.x + r.y * screen.w;
  }

  Pen* Surface::ptr(const Point &p) {
    return screen.data + p.x + p.y * screen.w;
  }

  Pen* Surface::ptr(int32_t x, int32_t y) {
    return screen.data + x + y * screen.w;
  }

  void Surface::pixel_span(const Point &p, int32_t l) {
    // check if span in bounds
    if( p.x + l < clip.x || p.x >= clip.x + clip.w ||
        p.y     < clip.y || p.y >= clip.y + clip.h) return;

    // clamp span horizontally
    Point clipped = p;
    if(clipped.x     <  clip.x)           {l += clipped.x - clip.x; clipped.x = clip.x;}
    if(clipped.x + l >= clip.x + clip.w)  {l  = clip.x + clip.w - clipped.x;}

    Pen *dest = ptr(clipped);
    while(l--) {
      *dest++ = pen;
    }
  }

  void Surface::circle(const Point &p, int32_t radius) {
      // circle in screen bounds?
    Rect bounds = Rect(p.x - radius, p.y - radius, radius * 2, radius * 2);
    if(!bounds.intersects(clip)) return;

    int ox = radius, oy = 0, err = -radius;
    while (ox >= oy)
    {
      int last_oy = oy;

      err += oy; oy++; err += oy;

      pixel_span(Point(p.x - ox, p.y + last_oy), ox * 2 + 1);
      if (last_oy != 0) {
        pixel_span(Point(p.x - ox, p.y - last_oy), ox * 2 + 1);
      }

      if(err >= 0 && ox != last_oy) {
        pixel_span(Point(p.x - last_oy, p.y + ox), last_oy * 2 + 1);
        if (ox != 0) {
          pixel_span(Point(p.x - last_oy, p.y - ox), last_oy * 2 + 1);
        }

        err -= ox; ox--; err -= ox;
      }
    }
  }

  bool Surface::contains(int32_t x, int32_t y) {
    return (x >= 0 && y >= 0 && x < w && y < h);
  }

  void Surface::clip_rect(int32_t &rx, int32_t &ry, int32_t &rw, int32_t &rh) {
    if(rx < 0)        {rw += rx; rx = 0;}     // clamp left and adjust width
    if(ry < 0)        {rh += ry; ry = 0;}     // clamp top and adjust height
    if(rx + rw >= w)  {rw = w - rx;}          // clamp width
    if(ry + rh >= h)  {rh = h - ry;}          // clamp height
  }

  uint32_t Surface::offset(int32_t x, int32_t y) const {
    return x + y * w;
  }

  // todo need to replace this font with our own, it's just for testing
  // PITCHFORK-5x5 font (c) 2018, Stefan Marsiske

  // Font developed for Nokia 3310 displays (Philips PCD8544) which have a
  // resolution of 84x48 pixels allowing 16x8 character display. Contains ASCII
  // characters 0x20-0x7e.

  // pitchfork5x5.bmf is licensed under a Creative Commons Attribution-ShareAlike 4.0
  // Generic License.

  // https://creativecommons.org/licenses/by-sa/4.0/

  const uint8_t font_data[475] = {
    0x00, 0x00, 0x00, 0x00, 0x00, // ''
    0x00, 0x00, 0x17, 0x00, 0x00, // '!'
    0x00, 0x03, 0x00, 0x03, 0x00, // '"'
    0x0a, 0x1f, 0x0a, 0x1f, 0x0a, // '#'
    0x12, 0x15, 0x1f, 0x15, 0x09, // '$'
    0x13, 0x0b, 0x04, 0x1a, 0x19, // '%'
    0x0d, 0x12, 0x15, 0x08, 0x14, // '&'
    0x00, 0x00, 0x03, 0x00, 0x00, // '''
    0x00, 0x0e, 0x11, 0x00, 0x00, // '('
    0x00, 0x00, 0x11, 0x0e, 0x00, // ')'
    0x00, 0x05, 0x02, 0x05, 0x00, // '*'
    0x04, 0x04, 0x1f, 0x04, 0x04, // '+'
    0x00, 0x10, 0x08, 0x00, 0x00, // ','
    0x04, 0x04, 0x04, 0x04, 0x04, // '-'
    0x00, 0x18, 0x18, 0x00, 0x00, // '.'
    0x10, 0x08, 0x04, 0x02, 0x01, // '/'
    0x0e, 0x11, 0x15, 0x11, 0x0e, // '0'
    0x00, 0x12, 0x1f, 0x10, 0x00, // '1'
    0x00, 0x1d, 0x15, 0x15, 0x17, // '2'
    0x00, 0x11, 0x15, 0x15, 0x0e, // '3'
    0x00, 0x07, 0x04, 0x04, 0x1f, // '4'
    0x00, 0x17, 0x15, 0x15, 0x09, // '5'
    0x00, 0x1f, 0x15, 0x15, 0x1d, // '6'
    0x00, 0x03, 0x19, 0x05, 0x03, // '7'
    0x00, 0x0a, 0x15, 0x15, 0x0a, // '8'
    0x00, 0x02, 0x15, 0x15, 0x0e, // '9'
    0x00, 0x00, 0x12, 0x00, 0x00, // ':'
    0x00, 0x08, 0x1a, 0x00, 0x00, // ';'
    0x00, 0x04, 0x0a, 0x11, 0x00, // '<'
    0x00, 0x0a, 0x0a, 0x0a, 0x0a, // '='
    0x00, 0x11, 0x0a, 0x04, 0x00, // '>'
    0x00, 0x01, 0x15, 0x05, 0x02, // '?'
    0x0e, 0x11, 0x15, 0x05, 0x06, // '@'
    0x00, 0x1f, 0x05, 0x05, 0x1f, // 'A'
    0x00, 0x1f, 0x15, 0x15, 0x0e, // 'B'
    0x00, 0x0e, 0x11, 0x11, 0x0a, // 'C'
    0x00, 0x1f, 0x11, 0x11, 0x0e, // 'D'
    0x00, 0x1f, 0x15, 0x15, 0x11, // 'E'
    0x00, 0x1f, 0x05, 0x05, 0x01, // 'F'
    0x00, 0x0e, 0x11, 0x15, 0x1d, // 'G'
    0x00, 0x1f, 0x04, 0x04, 0x1f, // 'H'
    0x00, 0x11, 0x1f, 0x11, 0x00, // 'I'
    0x00, 0x18, 0x10, 0x10, 0x1f, // 'J'
    0x00, 0x1f, 0x04, 0x0a, 0x11, // 'K'
    0x00, 0x1f, 0x10, 0x10, 0x10, // 'L'
    0x1f, 0x02, 0x0c, 0x02, 0x1f, // 'M'
    0x00, 0x1f, 0x06, 0x0c, 0x1f, // 'N'
    0x00, 0x0e, 0x11, 0x11, 0x0e, // 'O'
    0x00, 0x1f, 0x05, 0x05, 0x02, // 'P'
    0x00, 0x0e, 0x15, 0x19, 0x1e, // 'Q'
    0x00, 0x1f, 0x05, 0x0d, 0x12, // 'R'
    0x00, 0x12, 0x15, 0x15, 0x09, // 'S'
    0x01, 0x01, 0x1f, 0x01, 0x01, // 'T'
    0x00, 0x0f, 0x10, 0x10, 0x0f, // 'U'
    0x03, 0x0c, 0x10, 0x0c, 0x03, // 'V'
    0x0f, 0x10, 0x0c, 0x10, 0x0f, // 'W'
    0x11, 0x0a, 0x04, 0x0a, 0x11, // 'X'
    0x01, 0x02, 0x1c, 0x02, 0x01, // 'Y'
    0x00, 0x19, 0x15, 0x13, 0x11, // 'Z'
    0x00, 0x1f, 0x11, 0x11, 0x00, // '['
    0x01, 0x02, 0x04, 0x08, 0x10, // '\'
    0x00, 0x11, 0x11, 0x1f, 0x00, // ']'
    0x00, 0x02, 0x01, 0x02, 0x00, // '^'
    0x10, 0x10, 0x10, 0x10, 0x10, // '_'
    0x00, 0x01, 0x02, 0x00, 0x00, // '`'
    0x00, 0x08, 0x1a, 0x1a, 0x1c, // 'a'
    0x00, 0x1f, 0x14, 0x14, 0x08, // 'b'
    0x00, 0x0c, 0x12, 0x12, 0x12, // 'c'
    0x00, 0x08, 0x14, 0x14, 0x1f, // 'd'
    0x00, 0x0c, 0x16, 0x16, 0x14, // 'e'
    0x00, 0x04, 0x1e, 0x05, 0x01, // 'f'
    0x00, 0x0c, 0x12, 0x1a, 0x08, // 'g'
    0x00, 0x1f, 0x04, 0x04, 0x18, // 'h'
    0x00, 0x14, 0x14, 0x1d, 0x10, // 'i'
    0x00, 0x18, 0x10, 0x10, 0x1d, // 'j'
    0x00, 0x1f, 0x08, 0x0c, 0x10, // 'k'
    0x00, 0x11, 0x1f, 0x10, 0x00, // 'l'
    0x1e, 0x02, 0x1e, 0x02, 0x1c, // 'm'
    0x00, 0x1e, 0x02, 0x02, 0x1c, // 'n'
    0x00, 0x0c, 0x12, 0x12, 0x0c, // 'o'
    0x00, 0x1e, 0x0a, 0x0a, 0x04, // 'p'
    0x00, 0x04, 0x0a, 0x0a, 0x1e, // 'q'
    0x00, 0x1c, 0x02, 0x02, 0x02, // 'r'
    0x00, 0x14, 0x1e, 0x1a, 0x0a, // 's'
    0x00, 0x04, 0x0f, 0x14, 0x10, // 't'
    0x00, 0x0e, 0x10, 0x10, 0x1e, // 'u'
    0x00, 0x06, 0x18, 0x18, 0x06, // 'v'
    0x0e, 0x10, 0x0e, 0x10, 0x0e, // 'w'
    0x00, 0x12, 0x0c, 0x0c, 0x12, // 'x'
    0x00, 0x02, 0x14, 0x1c, 0x02, // 'y'
    0x00, 0x12, 0x1a, 0x16, 0x12, // 'z'
    0x00, 0x04, 0x1f, 0x11, 0x11, // '{'
    0x00, 0x00, 0x1f, 0x00, 0x00, // '|'
    0x00, 0x11, 0x11, 0x1f, 0x04, // '}'
    0x00, 0x01, 0x03, 0x02, 0x02, // '~'
  };
}