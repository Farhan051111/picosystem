#pragma once

#include <memory>
#include <cstdint>

namespace picosystem {

  // first prototype pin layout
  /*
  // dpad pins
  enum dpad {UP = 20, DOWN = 23, LEFT = 22, RIGHT = 21};

  // action button pins
  enum button {A  = 18, B = 19, X = 17, Y = 16};

  // screen and utility pins
  enum pin {
    CS = 5, SCK = 6, MOSI = 7, VSYNC = 8, DC = 9, BACKLIGHT = 12,
    AUDIO = 11, LED = 3, CHARGE_STATUS = 24, BATTERY_SENSE = 26
  };
  */

  // final pin layout
  // dpad pins
  enum dpad {UP = 23, DOWN = 20, LEFT = 22, RIGHT = 21};

  // action button pins
  enum button {A  = 18, B = 19, X = 17, Y = 16};

  // screen and utility pins
  enum pin {
    CS = 5, SCK = 6, MOSI = 7, VSYNC = 8, DC = 9, LCD_RESET = 4, BACKLIGHT = 12,
    AUDIO = 11, LED_R = 14, LED_G = 13, LED_B = 15, CHARGE_LED_ENABLE = 2, CHARGE_STATUS = 24, BATTERY_SENSE = 26
  };

  struct Pen {
    uint16_t v;

    Pen() = default;
    Pen(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 15);
    Pen(uint16_t v);
    static Pen from_hsv(float h, float s, float v);
  };

  struct Rect;
  struct Vec2;

  struct Point {
    int32_t x = 0, y = 0;

    Point() = default;
    Point(int32_t x, int32_t y) : x(x), y(y) {}
    Point(const Vec2 &v);

    inline Point& operator-= (const Point &a) { x -= a.x; y -= a.y; return *this; }
    inline Point& operator+= (const Point &a) { x += a.x; y += a.y; return *this; }

    Point clamp(const Rect &r) const;
  };

  inline Point operator-  (Point lhs, const Point &rhs) { lhs -= rhs; return lhs; }
  inline Point operator-  (const Point &rhs) { return Point(-rhs.x, -rhs.y); }
  inline Point operator+  (Point lhs, const Point &rhs) { lhs += rhs; return lhs; }

  struct Vec2 {
    float x = 0, y = 0;

    Vec2() = default;
    Vec2(float x, float y) : x(x), y(y) {}

    inline Vec2& operator-= (const Vec2 &a) { x -= a.x; y -= a.y; return *this; }
    inline Vec2& operator+= (const Vec2 &a) { x += a.x; y += a.y; return *this; }
  };

  inline Vec2 operator-  (Vec2 lhs, const Vec2 &rhs) { lhs -= rhs; return lhs; }
  inline Vec2 operator-  (const Vec2 &rhs) { return Vec2(-rhs.x, -rhs.y); }
  inline Vec2 operator+  (Vec2 lhs, const Vec2 &rhs) { lhs += rhs; return lhs; }

  struct Rect {
    int32_t x = 0, y = 0, w = 0, h = 0;

    Rect() = default;
    Rect(int32_t x, int32_t y, int32_t w, int32_t h) : x(x), y(y), w(w), h(h) {}

    bool empty() const {return w <= 0 || h <= 0;}

    bool contains(const Point &p) const {
      return p.x >= x && p.y >= y && p.x < x + w && p.y < y + h;
    }

    bool contains(const Rect &p) const {
      return p.x >= x && p.y >= y && p.x + p.w < x + w && p.y + p.h < y + h;
    }

    bool intersects(const Rect &r) const {
      return !(x > r.x + r.w || x + w < r.x || y > r.y + r.h || y + h < r.y);
    }

    Rect intersection(const Rect &r) const {
      return Rect(std::max(x, r.x),
                  std::max(y, r.y),
                  std::min(x + w, r.x + r.w) - std::max(x, r.x),
                  std::min(y + h, r.y + r.h) - std::max(y, r.y));
    }
  };

  inline Point Point::clamp(const Rect &r) const {
    return Point(x < r.x ? r.x : (x > r.x + r.w ? r.x + r.w : x),
                  y < r.y ? r.y : (y > r.y + r.h ? r.y + r.h : y));
  }

  inline Point::Point(const Vec2 &v) : x(v.x), y(v.y) {}

  struct Surface;

  // blending functions
  using BlendFunc = void(*)(Pen* source, uint32_t source_step, Pen* dest, uint32_t count);
  extern void COPY(Pen* source, uint32_t source_step, Pen* dest, uint32_t count);
  extern void BLEND(Pen* source, uint32_t source_step, Pen* dest, uint32_t count);

  struct Surface {
    uint16_t w, h;          // width and height
    Rect clip;              // clipping rectangle
    uint32_t pc;            // total pixel count of surface
    Pen *data;              // pointer to pixel data in 4-bits per channel "argb" format
    Pen pen;                // current pen for drawing operations
    BlendFunc bf = BLEND;   // blend function for drawing operations

    Surface(uint16_t w, uint16_t h) : w(w), h(h), pc(w * h), clip(Rect(0, 0, w, h)), data(new Pen[w * h]) {}
    ~Surface() {delete data;}

    void blend_mode(BlendFunc nbf);

    Pen* ptr(const Rect &r);
    Pen* ptr(const Point &p);
    Pen* ptr(int32_t x, int32_t y);

    // drawing primitives
    void clear();
    void pixel(int32_t x, int32_t y);
    void rectangle(int32_t rx, int32_t ry, int32_t rw, int32_t rh);
    void pixel_span(const Point &p, int32_t l);
    void circle(const Point &p, int32_t radius);
    void text(const std::string &t, int32_t x, int32_t y, int32_t wrap = -1);
    void blit(const Surface &source, const Rect &from, const Point &to);

    // utility functions
    bool contains(int32_t x, int32_t y);
    void clip_rect(int32_t &rx, int32_t &ry, int32_t &rw, int32_t &rh);
    uint32_t offset(int32_t x, int32_t y) const;
  };

  extern Surface &screen;
  extern const uint8_t font_data[475];

  // intialisation
  void init_picosystem();
  void init_screen();
  void init_graphics();
  void intro();

  // utility
  float battery_voltage();
  uint32_t time();

  // screen
  void set_backlight(uint8_t brightness);
  void wait_vsync();
  void update_screen();
  void flip();

  // controls
  bool pressed(uint8_t button);

}