#include "input.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "pico/time.h"

#define GPIO_UP    2
#define GPIO_DOWN  3
#define GPIO_LEFT  1
#define GPIO_RIGHT 0

#define INPUT_QUEUE_SIZE 8
static volatile input_direction_t input_queue[INPUT_QUEUE_SIZE];
static volatile uint8_t queue_head = 0;
static volatile uint8_t queue_tail = 0;

static volatile uint64_t last_input_time = 0;
#define DEBOUNCE_DELAY_US 50000 // 50ms

static void push_input(input_direction_t dir) {
    uint8_t next_head = (queue_head + 1) % INPUT_QUEUE_SIZE;
    if (next_head != queue_tail) {
        input_queue[queue_head] = dir;
        queue_head = next_head;
    }
}

static void gpio_callback(uint gpio, uint32_t events) {
    (void)events;
    
    uint64_t current_time = time_us_64();
    if (current_time - last_input_time < DEBOUNCE_DELAY_US) {
        return;
    }
    
    input_direction_t dir = INPUT_NONE;
    if (gpio == GPIO_UP) {
        dir = INPUT_UP;
    } else if (gpio == GPIO_DOWN) {
        dir = INPUT_DOWN;
    } else if (gpio == GPIO_LEFT) {
        dir = INPUT_LEFT;
    } else if (gpio == GPIO_RIGHT) {
        dir = INPUT_RIGHT;
    }

    if (dir != INPUT_NONE) {
        push_input(dir);
        last_input_time = current_time;
    }
}

void input_init(void) {
    gpio_init(GPIO_UP);
    gpio_init(GPIO_DOWN);
    gpio_init(GPIO_LEFT);
    gpio_init(GPIO_RIGHT);

    gpio_set_dir(GPIO_UP, GPIO_IN);
    gpio_set_dir(GPIO_DOWN, GPIO_IN);
    gpio_set_dir(GPIO_LEFT, GPIO_IN);
    gpio_set_dir(GPIO_RIGHT, GPIO_IN);

    gpio_pull_up(GPIO_UP);
    gpio_pull_up(GPIO_DOWN);
    gpio_pull_up(GPIO_LEFT);
    gpio_pull_up(GPIO_RIGHT);

    gpio_set_irq_enabled_with_callback(GPIO_UP, GPIO_IRQ_EDGE_FALL, true, gpio_callback);
    gpio_set_irq_enabled(GPIO_DOWN, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(GPIO_LEFT, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(GPIO_RIGHT, GPIO_IRQ_EDGE_FALL, true);
}

input_direction_t input_get_direction(void) {
    if (queue_head == queue_tail) return INPUT_NONE;
    input_direction_t dir = input_queue[queue_tail];
    queue_tail = (queue_tail + 1) % INPUT_QUEUE_SIZE;
    return dir;
}

bool input_has_new_input(void) {
    return queue_head != queue_tail;
}
