#ifndef ASTEROIDS_GAME_H
#define ASTEROIDS_GAME_H

#include <stdint.h>
#include <stdbool.h>

// Grid matches your existing snake grid exactly
#define GAME_WIDTH          16
#define GAME_HEIGHT         8

#define MAX_BULLETS         6
#define MAX_ASTEROIDS       5
#define MAX_GHOST_BULLETS   6

// Echo system
// Timer fires every 100ms. ECHO_HISTORY = 60 slots = 6 seconds of replay data.
// Ghost is always ECHO_DELAY_FRAMES ticks behind player (= 3 seconds).
#define ECHO_HISTORY        60
#define ECHO_DELAY_FRAMES   30

// Gameplay timers (in 100ms ticks)
#define SHOOT_COOLDOWN          10   // auto-fires every 1 second in facing direction
#define BULLET_MOVE_EVERY       2    // bullets advance every 200ms
#define ASTEROID_SPAWN_INTERVAL 50   // new asteroid every 5 seconds
#define MAX_ACTIVE_ASTEROIDS    5

typedef struct {
    int8_t x, y;
    int8_t dx, dy;
    bool active;
} bullet_t;

typedef struct {
    int8_t x, y;
    int8_t dx, dy;         // movement direction (-1, 0, or 1 per axis)
    uint8_t move_timer;    // counts up to move_speed
    uint8_t move_speed;    // ticks between moves (randomised 5-9)
    bool active;
} asteroid_t;

// One snapshot of player state stored per tick for ghost replay
typedef struct {
    int8_t x, y;
    int8_t face_dx, face_dy;  // facing direction that tick
    bool fired;               // did we fire this tick?
    int8_t fired_dx, fired_dy;
} history_frame_t;

typedef struct {
    // --- Player ---
    int8_t px, py;
    int8_t face_dx, face_dy;  // current facing (set on move)

    // --- Player bullets ---
    bullet_t bullets[MAX_BULLETS];

    // --- Asteroids ---
    asteroid_t asteroids[MAX_ASTEROIDS];

    // --- Echo history ring buffer ---
    history_frame_t history[ECHO_HISTORY];
    uint8_t history_head;   // next write slot
    uint8_t history_count;  // how many frames recorded so far

    // --- Ghost ---
    bool ghost_active;
    int8_t ghost_x, ghost_y;
    bullet_t ghost_bullets[MAX_GHOST_BULLETS];

    // --- Timers ---
    uint8_t shoot_timer;
    uint8_t asteroid_spawn_timer;
    uint32_t tick_counter;

    // --- Pending player input (set by ISR path, consumed in update) ---
    int8_t next_dx, next_dy;
    bool input_pending;

    // --- Game status ---
    uint32_t score;
    bool game_over;

} asteroids_state_t;

// Called once on game start
void asteroids_init(asteroids_state_t *state);

// Called every 100ms from the hardware timer callback (same as snake_update was)
void asteroids_update(asteroids_state_t *state);

// Called from process_input() with dx/dy direction vector
void asteroids_set_direction(asteroids_state_t *state, int8_t dx, int8_t dy);

uint32_t asteroids_get_score(asteroids_state_t *state);
bool     asteroids_is_game_over(asteroids_state_t *state);

#endif // ASTEROIDS_GAME_H
