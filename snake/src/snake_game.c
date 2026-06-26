#include "snake_game.h"
#include "pico/time.h"
#include <stdbool.h>

static uint32_t random_state = 12345;

static uint32_t simple_rand(void) {
    random_state = (random_state * 1103515245 + 12345) & 0x7fffffff;
    return random_state;
}

static void spawn_food(game_state_t *state) {
    position_t new_food;
    bool valid;

    do {
        valid = true;
        new_food.x = simple_rand() % GAME_WIDTH;
        new_food.y = simple_rand() % GAME_HEIGHT;

        for (uint16_t i = 0; i < state->snake_length; i++) {
            if (state->snake[i].x == new_food.x && state->snake[i].y == new_food.y) {
                valid = false;
                break;
            }
        }
    } while (!valid);

    state->food = new_food;
}

static bool check_collision(game_state_t *state, position_t head) {
    if (head.x < 0 || head.x >= GAME_WIDTH || head.y < 0 || head.y >= GAME_HEIGHT) {
        return true;
    }

    for (uint16_t i = 1; i < state->snake_length; i++) {
        if (state->snake[i].x == head.x && state->snake[i].y == head.y) {
            return true;
        }
    }

    return false;
}

void snake_init(game_state_t *state) {
    random_state = (uint32_t)time_us_64();

    state->snake_length = 3;
    state->snake[0].x = GAME_WIDTH / 2;
    state->snake[0].y = GAME_HEIGHT / 2;
    state->snake[1].x = GAME_WIDTH / 2 - 1;
    state->snake[1].y = GAME_HEIGHT / 2;
    state->snake[2].x = GAME_WIDTH / 2 - 2;
    state->snake[2].y = GAME_HEIGHT / 2;

    state->current_dir = DIR_RIGHT;
    state->next_dir = DIR_RIGHT;
    state->score = 0;
    state->speed = INITIAL_SPEED;
    state->tick_counter = 0;
    state->game_over = false;

    spawn_food(state);
}

void snake_update(game_state_t *state) {
    if (state->game_over) {
        return;
    }

    state->tick_counter++;
    if (state->tick_counter < state->speed) {
        return;
    }
    state->tick_counter = 0;

    if ((state->next_dir == DIR_UP && state->current_dir != DIR_DOWN) ||
        (state->next_dir == DIR_DOWN && state->current_dir != DIR_UP) ||
        (state->next_dir == DIR_LEFT && state->current_dir != DIR_RIGHT) ||
        (state->next_dir == DIR_RIGHT && state->current_dir != DIR_LEFT)) {
        state->current_dir = state->next_dir;
    }

    position_t new_head = state->snake[0];
    switch (state->current_dir) {
        case DIR_UP:
            new_head.y--;
            break;
        case DIR_DOWN:
            new_head.y++;
            break;
        case DIR_LEFT:
            new_head.x--;
            break;
        case DIR_RIGHT:
            new_head.x++;
            break;
        default:
            break;
    }

    if (check_collision(state, new_head)) {
        state->game_over = true;
        return;
    }

    bool food_eaten = (new_head.x == state->food.x && new_head.y == state->food.y);
    if (food_eaten && state->snake_length < SNAKE_MAX_LEN) {
        state->snake_length++;
    }

    for (uint16_t i = state->snake_length - 1; i > 0; i--) {
        state->snake[i] = state->snake[i - 1];
    }
    state->snake[0] = new_head;

    if (food_eaten) {
        state->score += 10;
        if ((state->score % 50) == 0 && state->speed > 2) {
            state->speed--; 
        }
        spawn_food(state);
    }
}

void snake_set_direction(game_state_t *state, direction_t dir) {
    if (dir < DIR_NONE) {
        state->next_dir = dir;
    }
}

uint32_t snake_get_score(game_state_t *state) {
    return state->score;
}

bool snake_is_game_over(game_state_t *state) {
    return state->game_over;
}
