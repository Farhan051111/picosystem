#pragma once

#include <memory>
#include <cstdint>

extern void init();
extern void update(uint32_t time_ms);
extern void render();

namespace picosystem {

  typedef uint16_t pen_t;

  struct buffer_t {
    uint32_t w, h;
    pen_t *data;
  };

  using blend_func_t = void(*)(pen_t* source, uint32_t source_step, pen_t* dest, uint32_t count);
  extern void COPY(pen_t* source, uint32_t source_step, pen_t* dest, uint32_t count);
  extern void BLEND(pen_t* source, uint32_t source_step, pen_t* dest, uint32_t count);

  extern pen_t _pen;
  extern int32_t _cx, _cy, _cw, _ch;
  extern blend_func_t _bf;
  extern buffer_t _fb; // framebuffer

  extern void init_hardware();

  pen_t create_pen(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
  void pen(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
  void pen(uint8_t r, uint8_t g, uint8_t b);
  void pen(pen_t p);

  void clip(int32_t x, int32_t y, uint32_t w, uint32_t h);

  void blend_mode(blend_func_t bf);

  void clear();
  void rectangle(int32_t x, int32_t y, int32_t w, int32_t h);
  void text(const std::string &t, int32_t x, int32_t y);
  void clip_rect(int32_t &x, int32_t &y, int32_t &w, int32_t &h);
  bool clip_contains(int32_t x, int32_t y);
  uint32_t offset(int32_t x, int32_t y);

  // helpers
  std::string str(float v, uint8_t precision);
  std::string str(uint32_t v);

  bool pressed(uint32_t button);
  void led(uint8_t r, uint8_t g, uint8_t b);

  // blending functions
/*
  struct Surface {

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

  extern Surface &screen;*/
  extern const uint8_t font8x8_basic[128][8];

  // utility
  float charge();
  uint32_t time();
  uint32_t time_us();
  void reset_to_dfu();

  // screen
  void backlight(uint8_t brightness);
  void update_screen();
  void wait_vsync();
  void flip();
  bool is_flipping();

  // input pins
  enum button {
    UP    = 23,
    DOWN  = 20,
    LEFT  = 22,
    RIGHT = 21,
    A     = 18,
    B     = 19,
    X     = 17,
    Y     = 16
  };

}