#ifndef SNAKE_GAME_H
#define SNAKE_GAME_H

#include <stdint.h>
#include <stdbool.h>

#define GAME_WIDTH      16
#define GAME_HEIGHT     8
#define SNAKE_MAX_LEN   64
#define INITIAL_SPEED   10

typedef enum {
    DIR_UP = 0,
    DIR_DOWN = 1,
    DIR_LEFT = 2,
    DIR_RIGHT = 3,
    DIR_NONE = 4
} direction_t;

typedef struct {
    int8_t x;
    int8_t y;
} position_t;

typedef struct {
    position_t snake[SNAKE_MAX_LEN];
    uint16_t snake_length;
    position_t food;
    direction_t current_dir;
    direction_t next_dir;
    uint32_t score;
    uint16_t speed;
    uint16_t tick_counter;
    bool game_over;
} game_state_t;

void snake_init(game_state_t *state);
void snake_update(game_state_t *state);
void snake_set_direction(game_state_t *state, direction_t dir);
uint32_t snake_get_score(game_state_t *state);
bool snake_is_game_over(game_state_t *state);

#endif // SNAKE_GAME_H