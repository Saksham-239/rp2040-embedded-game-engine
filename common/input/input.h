#ifndef INPUT_H
#define INPUT_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    INPUT_UP = 0,
    INPUT_DOWN = 1,
    INPUT_LEFT = 2,
    INPUT_RIGHT = 3,
    INPUT_NONE = 4
} input_direction_t;

void input_init(void);
input_direction_t input_get_direction(void);
bool input_has_new_input(void);

#endif // INPUT_H
