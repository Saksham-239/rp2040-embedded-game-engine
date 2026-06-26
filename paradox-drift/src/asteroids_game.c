#include "asteroids_game.h"
#include "pico/time.h"
#include <string.h>

// ─── RNG (same xorshift as snake used) ───────────────────────────────────────
static uint32_t rng_state = 12345;

static uint32_t rng(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

// ─── Helpers ─────────────────────────────────────────────────────────────────

static int count_active_asteroids(asteroids_state_t *s) {
    int n = 0;
    for (int i = 0; i < MAX_ASTEROIDS; i++) {
        if (s->asteroids[i].active) n++;
    }
    return n;
}

static void spawn_asteroid(asteroids_state_t *s) {
    // Find a free slot
    int slot = -1;
    for (int i = 0; i < MAX_ASTEROIDS; i++) {
        if (!s->asteroids[i].active) { slot = i; break; }
    }
    if (slot < 0) return;

    asteroid_t *a = &s->asteroids[slot];
    memset(a, 0, sizeof(*a));
    a->active     = true;
    a->move_speed = (uint8_t)(5 + (rng() % 5)); // 5-9 ticks per move step

    // Spawn at a random edge; one axis is clamped, the other randomised.
    // One axis of the direction vector is fixed (pointing inward) so the
    // asteroid always moves. The other axis is -1/0/1 for a slight angle.
    int8_t edge = (int8_t)(rng() % 4);
    switch (edge) {
        case 0: // top edge → moves downward
            a->x  = (int8_t)(rng() % GAME_WIDTH);
            a->y  = 0;
            a->dy = 1;
            a->dx = (int8_t)((rng() % 3) - 1);
            break;
        case 1: // bottom edge → moves upward
            a->x  = (int8_t)(rng() % GAME_WIDTH);
            a->y  = GAME_HEIGHT - 1;
            a->dy = -1;
            a->dx = (int8_t)((rng() % 3) - 1);
            break;
        case 2: // left edge → moves rightward
            a->x  = 0;
            a->y  = (int8_t)(rng() % GAME_HEIGHT);
            a->dx = 1;
            a->dy = (int8_t)((rng() % 3) - 1);
            break;
        case 3: // right edge → moves leftward
            a->x  = GAME_WIDTH - 1;
            a->y  = (int8_t)(rng() % GAME_HEIGHT);
            a->dx = -1;
            a->dy = (int8_t)((rng() % 3) - 1);
            break;
    }

    // Don't spawn directly on the player
    if (a->x == s->px && a->y == s->py) {
        a->active = false;
    }
}

// Fire a bullet from one of the arrays if a free slot exists
static void fire_bullet(bullet_t *arr, int max, int8_t x, int8_t y, int8_t dx, int8_t dy) {
    if (dx == 0 && dy == 0) return;
    for (int i = 0; i < max; i++) {
        if (!arr[i].active) {
            arr[i].x  = x;  arr[i].y  = y;
            arr[i].dx = dx; arr[i].dy = dy;
            arr[i].active = true;
            return;
        }
    }
}

// Advance all active bullets in an array, deactivate on out-of-bounds
static void advance_bullets(bullet_t *arr, int max) {
    for (int i = 0; i < max; i++) {
        if (!arr[i].active) continue;
        arr[i].x += arr[i].dx;
        arr[i].y += arr[i].dy;
        if (arr[i].x < 0 || arr[i].x >= GAME_WIDTH ||
            arr[i].y < 0 || arr[i].y >= GAME_HEIGHT) {
            arr[i].active = false;
        }
    }
}

// ─── Public API ──────────────────────────────────────────────────────────────

void asteroids_init(asteroids_state_t *s) {
    rng_state = (uint32_t)time_us_64();  // seed from hardware clock
    memset(s, 0, sizeof(*s));

    s->px      = GAME_WIDTH  / 2;
    s->py      = GAME_HEIGHT / 2;
    s->face_dx = 0;
    s->face_dy = -1;  // start facing up

    // Two asteroids to kick things off
    spawn_asteroid(s);
    spawn_asteroid(s);
}

void asteroids_set_direction(asteroids_state_t *s, int8_t dx, int8_t dy) {
    s->next_dx    = dx;
    s->next_dy    = dy;
    s->input_pending = true;
}

void asteroids_update(asteroids_state_t *s) {
    if (s->game_over) return;
    s->tick_counter++;

    // ── 1. Apply pending player movement ─────────────────────────────────────
    if (s->input_pending) {
        s->face_dx = s->next_dx;
        s->face_dy = s->next_dy;
        s->px += s->next_dx;
        s->py += s->next_dy;
        s->input_pending = false;

        // Wrap at grid edges (gives a bit more room to manoeuvre)
        if (s->px < 0)          s->px = GAME_WIDTH  - 1;
        if (s->px >= GAME_WIDTH) s->px = 0;
        if (s->py < 0)          s->py = GAME_HEIGHT - 1;
        if (s->py >= GAME_HEIGHT) s->py = 0;
    }

    // ── 2. Auto-shoot in facing direction ─────────────────────────────────────
    bool fired_this_tick = false;
    int8_t fired_dx = 0, fired_dy = 0;

    s->shoot_timer++;
    if (s->shoot_timer >= SHOOT_COOLDOWN) {
        s->shoot_timer = 0;
        if (s->face_dx != 0 || s->face_dy != 0) {
            fire_bullet(s->bullets, MAX_BULLETS, s->px, s->py, s->face_dx, s->face_dy);
            fired_this_tick = true;
            fired_dx = s->face_dx;
            fired_dy = s->face_dy;
        }
    }

    // ── 3. Record history snapshot (BEFORE updating ghost so ghost lags) ──────
    {
        history_frame_t *f = &s->history[s->history_head];
        f->x        = s->px;
        f->y        = s->py;
        f->face_dx  = s->face_dx;
        f->face_dy  = s->face_dy;
        f->fired    = fired_this_tick;
        f->fired_dx = fired_dx;
        f->fired_dy = fired_dy;

        s->history_head = (s->history_head + 1) % ECHO_HISTORY;
        if (s->history_count < ECHO_HISTORY) s->history_count++;
    }

    // ── 4. Replay ghost from ECHO_DELAY_FRAMES ticks ago ─────────────────────
    //
    // history_head now points to the *next* write slot.
    // Slot written this tick: (history_head - 1 + ECHO_HISTORY) % ECHO_HISTORY
    // Slot ECHO_DELAY_FRAMES ticks before that:
    //   (history_head - 1 - ECHO_DELAY_FRAMES + N*ECHO_HISTORY) % ECHO_HISTORY
    //
    if (s->history_count >= (uint8_t)(ECHO_DELAY_FRAMES + 1)) {
        s->ghost_active = true;

        uint8_t idx = (uint8_t)((s->history_head + ECHO_HISTORY - 1 - ECHO_DELAY_FRAMES) % ECHO_HISTORY);
        history_frame_t *gf = &s->history[idx];

        s->ghost_x = gf->x;
        s->ghost_y = gf->y;

        // If the past-player fired that tick, spawn a live ghost bullet now
        if (gf->fired) {
            fire_bullet(s->ghost_bullets, MAX_GHOST_BULLETS,
                        gf->x, gf->y, gf->fired_dx, gf->fired_dy);
        }
    }

    // ── 5. Advance bullets every BULLET_MOVE_EVERY ticks ─────────────────────
    if (s->tick_counter % BULLET_MOVE_EVERY == 0) {
        advance_bullets(s->bullets,       MAX_BULLETS);
        advance_bullets(s->ghost_bullets, MAX_GHOST_BULLETS);
    }

    // ── 6. Player bullet × asteroid collision ─────────────────────────────────
    for (int b = 0; b < MAX_BULLETS; b++) {
        if (!s->bullets[b].active) continue;
        for (int a = 0; a < MAX_ASTEROIDS; a++) {
            if (!s->asteroids[a].active) continue;
            if (s->bullets[b].x == s->asteroids[a].x &&
                s->bullets[b].y == s->asteroids[a].y) {
                s->bullets[b].active   = false;
                s->asteroids[a].active = false;
                s->score += 10;
            }
        }
    }

    // ── 7. Ghost bullet × asteroid collision (ghost can clear the field for you) ─
    for (int b = 0; b < MAX_GHOST_BULLETS; b++) {
        if (!s->ghost_bullets[b].active) continue;
        for (int a = 0; a < MAX_ASTEROIDS; a++) {
            if (!s->asteroids[a].active) continue;
            if (s->ghost_bullets[b].x == s->asteroids[a].x &&
                s->ghost_bullets[b].y == s->asteroids[a].y) {
                s->ghost_bullets[b].active = false;
                s->asteroids[a].active     = false;
                // No score — your past self did the work
            }
        }
    }

    // ── 8. Move asteroids (each has its own speed) ────────────────────────────
    for (int a = 0; a < MAX_ASTEROIDS; a++) {
        if (!s->asteroids[a].active) continue;
        s->asteroids[a].move_timer++;
        if (s->asteroids[a].move_timer >= s->asteroids[a].move_speed) {
            s->asteroids[a].move_timer = 0;
            s->asteroids[a].x += s->asteroids[a].dx;
            s->asteroids[a].y += s->asteroids[a].dy;
            // Wrap asteroids too
            if (s->asteroids[a].x < 0)           s->asteroids[a].x = GAME_WIDTH  - 1;
            if (s->asteroids[a].x >= GAME_WIDTH)  s->asteroids[a].x = 0;
            if (s->asteroids[a].y < 0)           s->asteroids[a].y = GAME_HEIGHT - 1;
            if (s->asteroids[a].y >= GAME_HEIGHT) s->asteroids[a].y = 0;
        }
    }

    // ── 9. Spawn new asteroids ────────────────────────────────────────────────
    s->asteroid_spawn_timer++;
    if (s->asteroid_spawn_timer >= ASTEROID_SPAWN_INTERVAL) {
        s->asteroid_spawn_timer = 0;
        if (count_active_asteroids(s) < MAX_ACTIVE_ASTEROIDS) {
            spawn_asteroid(s);
        }
    }

    // ── 10. Death checks ─────────────────────────────────────────────────────

    // Asteroid hits player
    for (int a = 0; a < MAX_ASTEROIDS; a++) {
        if (!s->asteroids[a].active) continue;
        if (s->asteroids[a].x == s->px && s->asteroids[a].y == s->py) {
            s->game_over = true;
            return;
        }
    }

    // Ghost body hits player (you ran into your own past)
    if (s->ghost_active && s->ghost_x == s->px && s->ghost_y == s->py) {
        s->game_over = true;
        return;
    }

    // Ghost bullet hits player
    for (int b = 0; b < MAX_GHOST_BULLETS; b++) {
        if (!s->ghost_bullets[b].active) continue;
        if (s->ghost_bullets[b].x == s->px && s->ghost_bullets[b].y == s->py) {
            s->game_over = true;
            return;
        }
    }
}

uint32_t asteroids_get_score(asteroids_state_t *s) {
    return s->score;
}

bool asteroids_is_game_over(asteroids_state_t *s) {
    return s->game_over;
}
