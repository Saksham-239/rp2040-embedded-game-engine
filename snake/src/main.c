/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "display_ssd1306.h"
#include "input.h"
#include "snake_game.h"
#include <stdio.h>
#include "hardware/i2c.h"
#include "pico/sync.h"

// Game state FSM
typedef enum {
    STATE_INIT = 0,
    STATE_MENU = 1,
    STATE_PLAYING = 2,
    STATE_GAME_OVER = 3
} game_state_enum;

static game_state_enum current_state = STATE_INIT;
static game_state_t game_state;
static volatile uint32_t tick_counter = 0;
static volatile bool menu_blink = false;
static uint32_t high_scores[3] = {0, 0, 0};
static bool scores_saved = false;

static critical_section_t state_lock;

static void error_blink(void) {
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    while (true) {
        int discovered = 0;
        int found_addr = -1;
        // Dynamically sweep the bus to see if it's completely disconnected or at the wrong address
        for (int addr = 0x08; addr < 0x78; ++addr) {
            uint8_t rxdata;
            int ret = i2c_read_timeout_us(i2c0, addr, &rxdata, 1, false, 5000);
            if (ret >= 0) {
                discovered++;
                found_addr = addr;
            }
        }

        if (discovered > 0) {
            printf("HARDWARE ERROR: Game Halted. BUT SCANNER FOUND A DEVICE AT: 0x%02X! (Expected 0x3C)\n", found_addr);
            printf(">>> I2C FAILED: Command 0x%02X returned SDK Error Code %d (Check pull-up resistors!)\n\n", last_i2c_cmd, last_i2c_error);
        } else {
            printf("HARDWARE ERROR: Game Halted. I2C SCANNER DETECTS ZERO DEVICES. Wire is entirely disconnected/loose.\n\n");
        }
        
        printf("DEBUG BUTTONS: UP(2): %d | DOWN(3): %d | LEFT(1): %d | RIGHT(0): %d\n",
                gpio_get(2), gpio_get(3), gpio_get(1), gpio_get(0));
        fflush(stdout);

        gpio_put(PICO_DEFAULT_LED_PIN, 1);
        sleep_ms(500);
        gpio_put(PICO_DEFAULT_LED_PIN, 0);
        sleep_ms(500);
    }
}

// Hardware timer callback for deterministic 100ms game tick.
static int64_t timer_callback(alarm_id_t id, void *user_data) {
    (void)id;
    (void)user_data;

    tick_counter++;

    critical_section_enter_blocking(&state_lock);
    if (current_state == STATE_PLAYING && !snake_is_game_over(&game_state)) {
        snake_update(&game_state);
    }
    critical_section_exit(&state_lock);

    if ((tick_counter % 5) == 0) {
        menu_blink = !menu_blink;
    }

    return 100000;  // 100ms in microseconds
}

static void render_menu(void) {
    ssd1306_clear();
    ssd1306_draw_string(30, 5, "SNAKE");

    if (menu_blink) {
        ssd1306_draw_string(15, 25, "PRESS UP START");
    } else {
        ssd1306_draw_string(15, 27, "PRESS UP START");
    }

    ssd1306_draw_string(20, 45, "Use D-Pad");
    ssd1306_update();
}

// Insert new_score into the sorted high_scores[3] top-3 table.
static void update_high_scores(uint32_t new_score) {
    // Find insertion point (scores kept descending)
    for (int i = 0; i < 3; i++) {
        if (new_score > high_scores[i]) {
            // Shift lower scores down
            for (int j = 2; j > i; j--) {
                high_scores[j] = high_scores[j - 1];
            }
            high_scores[i] = new_score;
            break;
        }
    }
}

static void render_playing(void) {
    // Reset flag so next game-over records that run's score
    scores_saved = false;
    ssd1306_clear();

    game_state_t local_state;
    critical_section_enter_blocking(&state_lock);
    local_state = game_state;
    critical_section_exit(&state_lock);

    char score_str[16];
    snprintf(score_str, sizeof(score_str), "Score: %u", snake_get_score(&local_state));
    ssd1306_draw_string(0, 0, score_str);

    for (uint8_t x = 0; x < SSD1306_WIDTH; x++) {
        ssd1306_draw_pixel(x, 10, true);
        ssd1306_draw_pixel(x, 63, true);
    }
    for (uint8_t y = 10; y < 64; y++) {
        ssd1306_draw_pixel(0, y, true);
        ssd1306_draw_pixel(127, y, true);
    }

    if (local_state.snake_length > 0) {
        for (uint16_t i = 0; i < local_state.snake_length; i++) {
            uint8_t screen_x = 10 + (local_state.snake[i].x * 7);
            uint8_t screen_y = 12 + (local_state.snake[i].y * 6);
            ssd1306_draw_rect(screen_x, screen_y, 5, 5, true, true);
        }
    }

    if ((tick_counter % 10) < 5) {
        uint8_t food_x = 10 + (local_state.food.x * 7);
        uint8_t food_y = 12 + (local_state.food.y * 6);
        ssd1306_draw_rect(food_x + 1, food_y + 1, 3, 3, true, true);
    }

    ssd1306_update();
}

static void render_game_over(void) {
    ssd1306_clear();

    // Title
    ssd1306_draw_string(30, 0, "GAME OVER");

    // Capture final score (locked) and update high-score table exactly once
    critical_section_enter_blocking(&state_lock);
    uint32_t final_score = snake_get_score(&game_state);
    critical_section_exit(&state_lock);

    if (!scores_saved) {
        update_high_scores(final_score);
        scores_saved = true;
    }

    char score_str[20];
    snprintf(score_str, sizeof(score_str), "Sc: %u", final_score);
    ssd1306_draw_string(0, 10, score_str);

    // Top 3 label
    ssd1306_draw_string(0, 20, "TOP 3:");

    // High score entries
    char hs_str[20];
    snprintf(hs_str, sizeof(hs_str), "#1 %u", (unsigned int)high_scores[0]);
    ssd1306_draw_string(0, 30, hs_str);

    snprintf(hs_str, sizeof(hs_str), "#2 %u", (unsigned int)high_scores[1]);
    ssd1306_draw_string(0, 40, hs_str);

    snprintf(hs_str, sizeof(hs_str), "#3 %u", (unsigned int)high_scores[2]);
    ssd1306_draw_string(0, 50, hs_str);

    // Blinking menu prompt, right-aligned at x=44, y=56
    if (menu_blink) {
        ssd1306_draw_string(44, 56, "^=Menu");
    }

    ssd1306_update();
}

static void process_input(void) {
    while (input_has_new_input()) {
        input_direction_t input_dir = input_get_direction();

        switch (current_state) {
            case STATE_MENU:
                if (input_dir == INPUT_UP) {
                    critical_section_enter_blocking(&state_lock);
                    current_state = STATE_PLAYING;
                    snake_init(&game_state);
                    tick_counter = 0;
                    critical_section_exit(&state_lock);
                }
                break;

            case STATE_PLAYING:
                critical_section_enter_blocking(&state_lock);
                switch (input_dir) {
                    case INPUT_UP:
                        snake_set_direction(&game_state, DIR_UP);
                        break;
                    case INPUT_DOWN:
                        snake_set_direction(&game_state, DIR_DOWN);
                        break;
                    case INPUT_LEFT:
                        snake_set_direction(&game_state, DIR_LEFT);
                        break;
                    case INPUT_RIGHT:
                        snake_set_direction(&game_state, DIR_RIGHT);
                        break;
                    default:
                        break;
                }
                critical_section_exit(&state_lock);
                break;

            case STATE_GAME_OVER:
                if (input_dir == INPUT_UP) {
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

static void update_display(void) {
    switch (current_state) {
        case STATE_MENU:
            render_menu();
            break;
        case STATE_PLAYING:
            if (snake_is_game_over(&game_state)) {
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

int main(void) {
    stdio_init_all();
    
    // Wait for the host OS to attach to the USB CDC Serial port
    sleep_ms(3000); 

    printf("\n=== RASPBERRY PI PICO I2C FAST-SCAN ===\n");
    
    // Pre-initialize I2C immediately to test lines
    // Under-clocking to 40KHz to aggressively combat wire capacitance without pullups
    i2c_init(i2c0, 40000);
    gpio_set_function(4, GPIO_FUNC_I2C);
    gpio_set_function(5, GPIO_FUNC_I2C);
    gpio_pull_up(4);
    gpio_pull_up(5);
    sleep_ms(50); // Stabilize line

    int discovered = 0;
    for (int addr = 0x08; addr < 0x78; ++addr) {
        uint8_t rxdata;
        int ret = i2c_read_timeout_us(i2c0, addr, &rxdata, 1, false, 5000);
        if (ret >= 0) {
            printf(">>> SUCCESS: Found active I2C device at address: 0x%02X\n", addr);
            discovered++;
        }
    }
    
    if (discovered == 0) {
        printf(">>> FAILURE: ZERO devices found on the I2C bus! Check wiring.\n");
    }
    fflush(stdout);

    critical_section_init(&state_lock);

    if (!ssd1306_init()) {
        error_blink();
    }
    printf("Starting game loop...\n");
    fflush(stdout);
    
    input_init();

    add_alarm_in_ms(100, timer_callback, NULL, true);

    current_state = STATE_MENU;
    tick_counter = 0;

    int debug_delay = 0;
    while (true) {
        if (debug_delay++ % 5 == 0) { // Print every 100ms
            printf("RAW GPIO PINS -> UP(GP2): %d | DOWN(GP3): %d | LEFT(GP1): %d | RIGHT(GP0): %d\n",
                gpio_get(2), gpio_get(3), gpio_get(1), gpio_get(0));
            fflush(stdout);
        }

        process_input();
        update_display();
        sleep_ms(20);
    }

    return 0;
}