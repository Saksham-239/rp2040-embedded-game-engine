/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Modified: Snake → Time Echo (Asteroids + Ghost replay)
 * Hardware unchanged: GP0-3 buttons, GP4/5 I2C OLED.
 */

#include "paradox_drift.h"
#include "display_ssd1306.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/timer.h"
#include "input.h"
#include "pico/stdlib.h"
#include "pico/sync.h"
#include <stdio.h>

// ─── FSM ─────────────────────────────────────────────────────────────────────
typedef enum {
  STATE_INIT = 0,
  STATE_MENU = 1,
  STATE_PLAYING = 2,
  STATE_GAME_OVER = 3
} game_state_enum;

static game_state_enum current_state = STATE_INIT;
static paradox_drift_state_t game_state;
static volatile uint32_t tick_counter = 0;
static volatile bool menu_blink = false;
static uint32_t high_scores[3] = {0, 0, 0};
static bool scores_saved = false;

static critical_section_t state_lock;

// ─── Error handler (unchanged) ───────────────────────────────────────────────
static void error_blink(void) {
  gpio_init(PICO_DEFAULT_LED_PIN);
  gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
  while (true) {
    int discovered = 0;
    int found_addr = -1;
    for (int addr = 0x08; addr < 0x78; ++addr) {
      uint8_t rxdata;
      int ret = i2c_read_timeout_us(i2c0, addr, &rxdata, 1, false, 5000);
      if (ret >= 0) {
        discovered++;
        found_addr = addr;
      }
    }
    if (discovered > 0) {
      printf("HARDWARE ERROR: Found device at 0x%02X (Expected 0x3C)\n",
             found_addr);
      printf(">>> I2C FAILED: Cmd 0x%02X returned %d\n\n", last_i2c_cmd,
             last_i2c_error);
    } else {
      printf("HARDWARE ERROR: Zero I2C devices found.\n\n");
    }
    printf("DEBUG BUTTONS: UP(2):%d DOWN(3):%d LEFT(1):%d RIGHT(0):%d\n",
           gpio_get(2), gpio_get(3), gpio_get(1), gpio_get(0));
    fflush(stdout);
    gpio_put(PICO_DEFAULT_LED_PIN, 1);
    sleep_ms(500);
    gpio_put(PICO_DEFAULT_LED_PIN, 0);
    sleep_ms(500);
  }
}

// ─── Hardware timer callback (100 ms) ────────────────────────────────────────
static int64_t timer_callback(alarm_id_t id, void *user_data) {
  (void)id;
  (void)user_data;
  tick_counter++;

  critical_section_enter_blocking(&state_lock);
  if (current_state == STATE_PLAYING && !paradox_drift_is_game_over(&game_state)) {
    paradox_drift_update(&game_state);
  }
  critical_section_exit(&state_lock);

  if ((tick_counter % 5) == 0)
    menu_blink = !menu_blink;

  return 100000; // reschedule in 100 ms
}

// ─── Rendering ───────────────────────────────────────────────────────────────

static void render_menu(void) {
  ssd1306_clear();
  ssd1306_draw_string(20, 4, "TIME  ECHO");
  if (menu_blink) {
    ssd1306_draw_string(15, 24, "PRESS UP START");
  } else {
    ssd1306_draw_string(15, 26, "PRESS UP START");
  }
  ssd1306_draw_string(8, 44, "Dodge your past!");
  ssd1306_update();
}

// Grid → screen pixel helpers (identical mapping to snake)
// Each cell is 7 px wide, 6 px tall. Play area starts at screen (10, 12).
#define CELL_W 7
#define CELL_H 6
#define ORIG_X 10
#define ORIG_Y 12

static inline uint8_t gx(int8_t col) {
  return (uint8_t)(ORIG_X + col * CELL_W);
}
static inline uint8_t gy(int8_t row) {
  return (uint8_t)(ORIG_Y + row * CELL_H);
}

// ── Sprite: draw player arrow pointing in (fdx, fdy) direction ─────────────
// 5×5 bitmask arrow. Drawn at cell pixel origin (cx, cy).
static void draw_player_ship(uint8_t cx, uint8_t cy, int8_t fdx, int8_t fdy) {
  // Arrow bitmaps (5 rows of 5 bits, MSB = left column)
  // Facing UP (default / fallback)
  static const uint8_t arrow_up[5]    = {0x04, 0x0E, 0x15, 0x04, 0x04};
  //   ..#..        00100
  //   .###.        01110
  //   #.#.#        10101
  //   ..#..        00100
  //   ..#..        00100

  static const uint8_t arrow_down[5]  = {0x04, 0x04, 0x15, 0x0E, 0x04};
  static const uint8_t arrow_left[5]  = {0x04, 0x08, 0x1F, 0x08, 0x04};
  static const uint8_t arrow_right[5] = {0x04, 0x02, 0x1F, 0x02, 0x04};

  const uint8_t *bmp = arrow_up; // default
  if (fdy < 0)      bmp = arrow_up;
  else if (fdy > 0) bmp = arrow_down;
  else if (fdx < 0) bmp = arrow_left;
  else if (fdx > 0) bmp = arrow_right;

  for (int r = 0; r < 5; r++) {
    for (int c = 0; c < 5; c++) {
      if (bmp[r] & (1 << (4 - c))) {
        ssd1306_draw_pixel(cx + c, cy + r, true);
      }
    }
  }
}

// ── Sprite: ghost — checkerboard 5×5 (looks spectral / flickery) ───────────
static void draw_ghost_ship(uint8_t cx, uint8_t cy) {
  for (int r = 0; r < 5; r++) {
    for (int c = 0; c < 5; c++) {
      if ((r + c) % 2 == 0) {
        ssd1306_draw_pixel(cx + c, cy + r, true);
      }
    }
  }
}

// ── Sprite: asteroid — diamond shape (spiky = danger) ──────────────────────
static void draw_asteroid(uint8_t cx, uint8_t cy) {
  //  ..#..    row 0: centre only
  //  .###.    row 1: 3 wide
  //  #####    row 2: full
  //  .###.    row 3: 3 wide
  //  ..#..    row 4: centre only
  static const uint8_t diamond[5] = {0x04, 0x0E, 0x1F, 0x0E, 0x04};
  for (int r = 0; r < 5; r++) {
    for (int c = 0; c < 5; c++) {
      if (diamond[r] & (1 << (4 - c))) {
        ssd1306_draw_pixel(cx + c, cy + r, true);
      }
    }
  }
}

static void render_playing(void) {
  scores_saved = false;
  ssd1306_clear();

  // Snapshot game state under lock
  paradox_drift_state_t local;
  critical_section_enter_blocking(&state_lock);
  local = game_state;
  critical_section_exit(&state_lock);

  // Score line
  char buf[16];
  snprintf(buf, sizeof(buf), "Score:%u", (unsigned)local.score);
  ssd1306_draw_string(0, 0, buf);

  // Border
  for (uint8_t x = 0; x < SSD1306_WIDTH; x++) {
    ssd1306_draw_pixel(x, 10, true);
    ssd1306_draw_pixel(x, 63, true);
  }
  for (uint8_t y = 10; y < 64; y++) {
    ssd1306_draw_pixel(0, y, true);
    ssd1306_draw_pixel(127, y, true);
  }

  // ── Asteroids: diamond shape (spiky = DANGER) ────────────────────────────
  for (int i = 0; i < MAX_ASTEROIDS; i++) {
    if (!local.asteroids[i].active)
      continue;
    draw_asteroid(gx(local.asteroids[i].x), gy(local.asteroids[i].y));
  }

  // ── Player bullets: 3-pixel line in travel direction ─────────────────────
  for (int i = 0; i < MAX_BULLETS; i++) {
    if (!local.bullets[i].active)
      continue;
    uint8_t bx = gx(local.bullets[i].x) + 2;
    uint8_t by = gy(local.bullets[i].y) + 2;
    ssd1306_draw_pixel(bx, by, true);
    ssd1306_draw_pixel(bx + local.bullets[i].dx, by + local.bullets[i].dy, true);
    ssd1306_draw_pixel(bx - local.bullets[i].dx, by - local.bullets[i].dy, true);
  }

  // ── Ghost bullets: small cross/plus shape — distinct from player bullets ─
  for (int i = 0; i < MAX_GHOST_BULLETS; i++) {
    if (!local.ghost_bullets[i].active)
      continue;
    uint8_t bx = gx(local.ghost_bullets[i].x) + 2;
    uint8_t by = gy(local.ghost_bullets[i].y) + 2;
    ssd1306_draw_pixel(bx,     by,     true);  // centre
    ssd1306_draw_pixel(bx - 1, by,     true);  // left
    ssd1306_draw_pixel(bx + 1, by,     true);  // right
    ssd1306_draw_pixel(bx,     by - 1, true);  // up
    ssd1306_draw_pixel(bx,     by + 1, true);  // down
  }

  // ── Ghost ship: checkerboard 5×5 (spectral / dithered) ──────────────────
  if (local.ghost_active) {
    draw_ghost_ship(gx(local.ghost_x), gy(local.ghost_y));
  }

  // ── Player ship: arrow pointing in facing direction (drawn LAST = on top) ─
  draw_player_ship(gx(local.px), gy(local.py), local.face_dx, local.face_dy);

  ssd1306_update();
}

static void update_high_scores(uint32_t new_score) {
  for (int i = 0; i < 3; i++) {
    if (new_score > high_scores[i]) {
      for (int j = 2; j > i; j--)
        high_scores[j] = high_scores[j - 1];
      high_scores[i] = new_score;
      break;
    }
  }
}

static void render_game_over(void) {
  ssd1306_clear();
  ssd1306_draw_string(20, 0, "GAME OVER");

  critical_section_enter_blocking(&state_lock);
  uint32_t final_score =
      paradox_drift_get_score(&game_state);
  critical_section_exit(&state_lock);

  if (!scores_saved) {
    update_high_scores(final_score);
    scores_saved = true;
  }

  char buf[20];
  snprintf(buf, sizeof(buf), "Sc: %u", (unsigned)final_score);
  ssd1306_draw_string(0, 10, buf);

  ssd1306_draw_string(0, 20, "TOP 3:");
  snprintf(buf, sizeof(buf), "#1 %u", (unsigned)high_scores[0]);
  ssd1306_draw_string(0, 30, buf);
  snprintf(buf, sizeof(buf), "#2 %u", (unsigned)high_scores[1]);
  ssd1306_draw_string(0, 40, buf);
  snprintf(buf, sizeof(buf), "#3 %u", (unsigned)high_scores[2]);
  ssd1306_draw_string(0, 50, buf);

  if (menu_blink)
    ssd1306_draw_string(44, 56, "^=Menu");

  ssd1306_update();
}

// ─── Input processing ────────────────────────────────────────────────────────
static void process_input(void) {
  while (input_has_new_input()) {
    input_direction_t dir = input_get_direction();

    switch (current_state) {

    case STATE_MENU:
      if (dir == INPUT_UP) {
        critical_section_enter_blocking(&state_lock);
        current_state = STATE_PLAYING;
        paradox_drift_init(&game_state);
        tick_counter = 0;
        critical_section_exit(&state_lock);
      }
      break;

    case STATE_PLAYING: {
      // Convert 4-way enum to dx/dy — no other changes needed here
      int8_t dx = 0, dy = 0;
      switch (dir) {
      case INPUT_UP:
        dy = -1;
        break;
      case INPUT_DOWN:
        dy = 1;
        break;
      case INPUT_LEFT:
        dx = -1;
        break;
      case INPUT_RIGHT:
        dx = 1;
        break;
      default:
        break;
      }
      if (dx != 0 || dy != 0) {
        critical_section_enter_blocking(&state_lock);
        paradox_drift_set_direction(&game_state, dx, dy);
        critical_section_exit(&state_lock);
      }
      break;
    }

    case STATE_GAME_OVER:
      if (dir == INPUT_UP) {
        critical_section_enter_blocking(&state_lock);
        current_state = STATE_MENU;
        critical_section_exit(&state_lock);
      }
      break;

    default:
      break;
    }
  }
}

// ─── Display dispatch (unchanged logic) ──────────────────────────────────────
static void update_display(void) {
  switch (current_state) {
  case STATE_MENU:
    render_menu();
    break;
  case STATE_PLAYING:
    if (paradox_drift_is_game_over(&game_state)) {
      current_state = STATE_GAME_OVER;
      render_game_over();
    } else {
      render_playing();
    }
    break;
  case STATE_GAME_OVER:
    render_game_over();
    break;
  default:
    break;
  }
}

// ─── main (unchanged from snake except include + game name) ──────────────────
int main(void) {
  stdio_init_all();
  sleep_ms(3000);

  printf("\n=== TIME ECHO — RP2040 ===\n");

  i2c_init(i2c0, 40000);
  gpio_set_function(4, GPIO_FUNC_I2C);
  gpio_set_function(5, GPIO_FUNC_I2C);
  gpio_pull_up(4);
  gpio_pull_up(5);
  sleep_ms(50);

  int discovered = 0;
  for (int addr = 0x08; addr < 0x78; ++addr) {
    uint8_t rxdata;
    int ret = i2c_read_timeout_us(i2c0, addr, &rxdata, 1, false, 5000);
    if (ret >= 0) {
      printf(">>> Found I2C device at 0x%02X\n", addr);
      discovered++;
    }
  }
  if (discovered == 0)
    printf(">>> FAILURE: No I2C devices found!\n");
  fflush(stdout);

  critical_section_init(&state_lock);

  if (!ssd1306_init())
    error_blink();
  printf("Display OK. Starting game loop...\n");
  fflush(stdout);

  input_init();
  add_alarm_in_ms(100, timer_callback, NULL, true);

  current_state = STATE_MENU;
  tick_counter = 0;

  int debug_delay = 0;
  while (true) {
    if (debug_delay++ % 5 == 0) {
      printf("PINS -> UP(GP2):%d DOWN(GP3):%d LEFT(GP1):%d RIGHT(GP0):%d\n",
             gpio_get(2), gpio_get(3), gpio_get(1), gpio_get(0));
      fflush(stdout);
    }
    process_input();
    update_display();
    sleep_ms(20);
  }

  return 0;
}