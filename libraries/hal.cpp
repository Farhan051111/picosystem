#include <math.h>

#include "hardware/adc.h"
#include "hardware/spi.h"
#include "hardware/dma.h"
#include "hardware/pwm.h"
#include "hardware/pio.h"
#include "hardware/irq.h"

#include "pico/bootrom.h"
#include "pico/stdlib.h"
#include "pico/time.h"

#include "screen.pio.h"
#include "picosystem.hpp"

namespace picosystem {

  enum pin {
    RED = 14, GREEN = 13, BLUE = 15,                  // user rgb led
    CS = 5, SCK = 6, MOSI  = 7,                       // spi
    VSYNC = 8, DC = 9, LCD_RESET = 4, BACKLIGHT = 12, // screen
    AUDIO = 11,                                       // audio
    CHARGE_LED = 2, CHARGING = 24, BATTERY_LEVEL = 26 // battery / charging
  };

  void init_inputs(uint32_t pin_mask) {
    for(uint8_t i = 0; i < 32; i++) {
      uint32_t pin = 1U << i;
      if(pin & pin_mask) {
        gpio_set_function(pin, GPIO_FUNC_SIO);
        gpio_set_dir(pin, GPIO_IN);
        gpio_pull_up(pin);
      }
    }
  }

  void init_outputs(uint32_t pin_mask) {
    for(uint8_t i = 0; i < 32; i++) {
      uint32_t pin = 1U << i;
      if(pin & pin_mask) {
        gpio_set_function(pin, GPIO_FUNC_SIO);
        gpio_set_dir(pin, GPIO_OUT);
        gpio_put(pin, 0);
      }
    }
  }


  uint32_t time() {
    absolute_time_t t = get_absolute_time();
    return to_ms_since_boot(t);
  }

  uint32_t time_us() {
    absolute_time_t t = get_absolute_time();
    return to_us_since_boot(t);
  }

  bool pressed(uint32_t button) {
    return !gpio_get(button);
  }

  void reset_to_dfu() {
    reset_usb_boot(0, 0);
  }

  float charge() {
    adc_select_input(0);
    // calculate a current charge level for the battery from
    // 0.0f..1.0f (battery cutoff voltage is 2.1v)

    // convert adv reading to voltage
    float v = (float(adc_read()) / (1 << 12)) * 3.3f;

    // correct for voltage divider on board
    v *= 3.0f;

    // convert to 0..1 range for battery between 2.8v and 4.1v
    float c = (v - 2.8f) / 1.3f;

    // clamp to 0..1
    c = std::max(0.0f, std::min(1.0f, c));

    return c;
  }


PIO   screen_pio  = pio0;
uint  screen_sm   = 0;

uint32_t         dma_channel;
volatile int16_t dma_scanline = -1;

enum st7789 {
  SWRESET   = 0x01,
  TEON      = 0x35,
  MADCTL    = 0x36,
  COLMOD    = 0x3A,
  GCTRL     = 0xB7,
  VCOMS     = 0xBB,
  LCMCTRL   = 0xC0,
  VDVVRHEN  = 0xC2,
  VRHS      = 0xC3,
  VDVS      = 0xC4,
  FRCTRL2   = 0xC6,
  PWRCTRL1  = 0xD0,
  FRMCTR1   = 0xB1,
  FRMCTR2   = 0xB2,
  GMCTRP1   = 0xE0,
  GMCTRN1   = 0xE1,
  INVOFF    = 0x20,
  SLPOUT    = 0x11,
  DISPON    = 0x29,
  GAMSET    = 0x26,
  DISPOFF   = 0x28,
  RAMWR     = 0x2C,
  INVON     = 0x21,
  CASET     = 0x2A,
  RASET     = 0x2B
};

using namespace picosystem;

void st7789_command(uint8_t command, size_t len = 0, const char *data = NULL) {
  gpio_put(pin::CS, 0);
  gpio_put(pin::DC, 0); // command mode
  spi_write_blocking(spi0, &command, 1);
  if(data) {
    gpio_put(pin::DC, 1); // data mode
    spi_write_blocking(spi0, (const uint8_t*)data, len);
  }
  gpio_put(pin::CS, 1);
}

// scanline data is sent via dma to the pixel doubling pio program which then
// writes the data to the st7789 via an spi-like interface. the pio program
// doubles pixels horizontally, but we need to double them vertically by sending
// each scanline to the pio twice.
//
// to minimise the number of dma transfers we transmit the current scanline and
// the previous scanline in every transfer. the exceptions are the first and final
// scanlines which are sent on their own to start and complete the write.
//
// - transfer #1: scanline 0
// - transfer #2: scanline 0 + scanline 1
// - transfer #3: scanline 1 + scanline 2
// ...
// - transfer #n - 1: scanline (n - 1) + scanline n
// - transfer #n: scanline n

// sets up dma transfer for current and previous scanline (except for
// scanlines 0 and 120 which are sent on their own.)
/*void transmit_scanline() {
  // start of data to transmit
  uint32_t *s = (uint32_t *)&dma_buffer[
    ((dma_scanline - 1) < 0 ? 0 : (dma_scanline - 1)) * 240
  ];
  // number of 32-bit words to transmit
  uint16_t c = (dma_scanline == 0 || dma_scanline == 120) ? 60 : 120;
  dma_channel_set_trans_count(dma_channel, c, false);
  dma_channel_set_read_addr(dma_channel, s, true);
}*/

//volatile bool dma_active = false;

// once the dma transfer of the scanline is complete we move to the
// next scanline (or quit if we're finished)
void __isr dma_complete() {
  if (dma_hw->ints0 & (1u << dma_channel)) {
    dma_hw->ints0 = (1u << dma_channel); // clear irq flag

//    dma_active = false;
  }
}

// when vsync is triggered we request the first scanline to be transferred
/*void on_vsync(uint gpio, uint32_t events) {
  if(dma_scanline != -1) { // already updating, skip
    return;
  }
  // transmit first scanline
  dma_scanline = 0;
  transmit_scanline();
}*/

static inline void screen_program_init(PIO pio, uint sm, uint offset) {
  pio_sm_set_consecutive_pindirs(pio, sm, pin::MOSI, 2, true);

  pio_sm_config c = screen_program_get_default_config(offset);

  // osr shifts left, autopull off, autopull threshold 32
  sm_config_set_out_shift(&c, false, false, 16);

  // configure out, set, and sideset pins
  sm_config_set_out_pins(&c, pin::MOSI, 1);
  sm_config_set_sideset_pins(&c, pin::SCK);

  // dividing the clock by two ensures we keep the spi transfer to
  // around 62.5mhz as per the st7789 datasheet
  sm_config_set_clkdiv_int_frac(&c, 2, 1);

  pio_sm_set_pins_with_mask(pio, sm, 0, (1u << pin::SCK) | (1u << pin::MOSI));
  pio_sm_set_pindirs_with_mask(pio, sm, (1u << pin::SCK) | (1u << pin::MOSI), (1u << pin::SCK) | (1u << pin::MOSI));

  // join fifos as only tx needed (gives 8 deep fifo instead of 4)
  sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);

  pio_gpio_init(screen_pio, pin::MOSI);
  pio_gpio_init(screen_pio, pin::SCK);

  pio_sm_init(pio, sm, offset, &c);
  pio_sm_set_enabled(pio, sm, true);
}

  void wait_vsync() {
    // since the SPI transfer is a touch slower than the screen refresh
    // rate we'll end up with an effective 30fps currently, this can be
    // improved by moving the LCD SPI transfers into PIO
    while(gpio_get(pin::VSYNC))  {}  // if in vsync already need to wait for it to end
    while(!gpio_get(pin::VSYNC)) {}  // now wait for vsync to occur
  }

  bool is_flipping() {return dma_channel_is_busy(dma_channel);}
  void flip() {
    // wait for start of vsync period
   // wait_vsync();
    // take a copy of the framebuffer (this takes < 1ms) so that
    // we can dma transfer over once vsync occurs without any
    // tearing it costs us around 28kb - yolo

    //

    // start the dma transfer of scanline data
    //dma_scanline = 0;
    //transmit_scanline();

    //wait_vsync();

    // if dma transfer already in process then skip
    if(!is_flipping()) {
      // much faster to copy the buffer then dma that in the background
 //     memcpy(dma_buffer, screen.data, screen.w * screen.h * 2);

      uint32_t transfer_count = _fb.w * _fb.h / 2;
      dma_channel_transfer_from_buffer_now(dma_channel, _fb.data, transfer_count);
    }

    // wait for screen transfer to complete before returning control
    //while(dma_active) {
    //}
  }

  uint16_t gamma_correct(uint8_t value) {
    float gamma = 2.8;
    return (uint16_t)(pow((float)(value) / 255.0f, gamma) * 65535.0f + 0.5f);
  }

  void backlight(uint8_t brightness) {
    pwm_set_gpio_level(BACKLIGHT, gamma_correct(brightness));
  }

  void led(uint8_t r, uint8_t g, uint8_t b) {
    pwm_set_gpio_level(RED,   gamma_correct(r));
    pwm_set_gpio_level(GREEN, gamma_correct(g));
    pwm_set_gpio_level(BLUE,  gamma_correct(b));
  }

  void init_screen() {
    spi_init(spi0, 8000000);

    gpio_set_function(pin::LCD_RESET, GPIO_FUNC_SIO);
    gpio_set_dir(pin::LCD_RESET, GPIO_OUT);
    gpio_put(pin::LCD_RESET, 0);
    sleep_ms(100);
    gpio_put(pin::LCD_RESET, 1);

    gpio_set_function(pin::DC, GPIO_FUNC_SIO);
    gpio_set_dir(pin::DC, GPIO_OUT);

    gpio_set_function(pin::CS, GPIO_FUNC_SIO);
    gpio_set_dir(pin::CS, GPIO_OUT);

    gpio_set_function(pin::SCK, GPIO_FUNC_SPI);
    gpio_set_function(pin::MOSI, GPIO_FUNC_SPI);

    // setup the st7789 screen driver
    gpio_put(pin::CS, 1);

    // send configuration for our 240x240 display to configure it
    // as 12-bits per pixel in RGB order
    st7789_command(st7789::SWRESET);
    sleep_ms(150);
    st7789_command(st7789::MADCTL,    1, "\x04");
    st7789_command(st7789::TEON,      1, "\x00");
    st7789_command(st7789::FRMCTR2,   5, "\x0C\x0C\x00\x33\x33");
    st7789_command(st7789::COLMOD,    1, "\x03");
    st7789_command(st7789::GCTRL,     1, "\x14");
    st7789_command(st7789::VCOMS,     1, "\x37");
    st7789_command(st7789::LCMCTRL,   1, "\x2C");
    st7789_command(st7789::VDVVRHEN,  1, "\x01");
    st7789_command(st7789::VRHS,      1, "\x12");
    st7789_command(st7789::VDVS,      1, "\x20");
    st7789_command(st7789::PWRCTRL1,  2, "\xA4\xA1");
    st7789_command(st7789::FRCTRL2,   1, "\x0f");
    st7789_command(st7789::GMCTRP1,  14, "\xD0\x04\x0D\x11\x13\x2B\x3F\x54\x4C\x18\x0D\x0B\x1F\x23");
    st7789_command(st7789::GMCTRN1,  14, "\xD0\x04\x0C\x11\x13\x2C\x3F\x44\x51\x2F\x1F\x1F\x20\x23");
    st7789_command(st7789::INVON);
    st7789_command(st7789::SLPOUT);
    st7789_command(st7789::DISPON);
    sleep_ms(100);
    st7789_command(st7789::CASET,     4, "\x00\x00\x00\xef");
    st7789_command(st7789::RASET,     4, "\x00\x00\x00\xef");
    st7789_command(st7789::RAMWR);

    // switch st7789 into data mode
    gpio_put(pin::CS, 0);
    gpio_put(pin::DC, 1);

    // at this stage the screen is configured and expecting to receive
    // pixel data. each time we write a screen worth of data the
    // st7789 resets the data pointer back to the start meaning that
    // we can now just leave the screen in data writing mode and
    // reassign the spi pins to our pixel doubling pio. so long as
    // we always write the entire screen we'll never get out of sync.

    // enable vsync interrupt to synchronise screen updates
    gpio_init(pin::VSYNC);
    gpio_set_dir(pin::VSYNC, GPIO_IN);
    //gpio_set_irq_enabled_with_callback(pin::VSYNC, GPIO_IRQ_EDGE_RISE, true, &on_vsync);

    // setup the pixel doubling pio program
    uint offset = pio_add_program(screen_pio, &screen_program);
    screen_program_init(screen_pio, screen_sm, offset);

    // initialise dma channel for transmitting pixel data to screen
    // via the pixel doubling pio - initially we configure it with no
    // source or transfer length since these need to be assigned for
    // each scaline
    dma_channel = dma_claim_unused_channel(true);
    dma_channel_config config = dma_channel_get_default_config(dma_channel);
    channel_config_set_bswap(&config, true); // byte swap to reverse little endian
    channel_config_set_dreq(&config, pio_get_dreq(screen_pio, screen_sm, true));

    dma_channel_configure(dma_channel, &config, &screen_pio->txf[screen_sm], nullptr, 0, false);
    dma_channel_set_irq0_enabled(dma_channel, true);
    irq_set_enabled(pio_get_dreq(screen_pio, screen_sm, true), true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_complete);
    irq_set_enabled(DMA_IRQ_0, true);
  }



void init_hardware() {
    // overclock the rp2040 to 250mhz, this should be achieveable
    // on any chip
    set_sys_clock_khz(250000, true);

    init_inputs(A | B | X | Y | UP | DOWN | LEFT | RIGHT);
    init_outputs(CHARGE_LED);

    adc_init();
    adc_gpio_init(BATTERY_LEVEL);

    pwm_config cfg = pwm_get_default_config();

    pwm_set_wrap(pwm_gpio_to_slice_num(BACKLIGHT), 65535);
    pwm_init(pwm_gpio_to_slice_num(BACKLIGHT), &cfg, true);
    gpio_set_function(BACKLIGHT, GPIO_FUNC_PWM);

    pwm_set_wrap(pwm_gpio_to_slice_num(RED), 65535);
    pwm_init(pwm_gpio_to_slice_num(RED), &cfg, true);
    gpio_set_function(RED, GPIO_FUNC_PWM);

    pwm_set_wrap(pwm_gpio_to_slice_num(GREEN), 65535);
    pwm_init(pwm_gpio_to_slice_num(GREEN), &cfg, true);
    gpio_set_function(GREEN, GPIO_FUNC_PWM);

    pwm_set_wrap(pwm_gpio_to_slice_num(BLUE), 65535);
    pwm_init(pwm_gpio_to_slice_num(BLUE), &cfg, true);
    gpio_set_function(BLUE, GPIO_FUNC_PWM);


    init_screen();
    backlight(255);


}
}