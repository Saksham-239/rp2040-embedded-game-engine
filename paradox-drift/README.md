# Paradox Drift — Time Echo Dodging Game

**Paradox Drift** is a custom arcade space-dodging game built on the RP2040 Embedded Game Engine. It combines elements of classic *Asteroids* with a unique **Time Echo** temporal replay mechanic: players must dodge incoming obstacles while navigating around a ghost replay of their own past movements and firing paths.

---

## Gameplay Mechanics

1. **Space Flight & Wrap**: Players navigate a spaceship in 4 directions. Movement physics wrap around the boundaries of the $16 \times 8$ coordinate playfield.
2. **Auto-Firing Lasers**: The ship automatically fires laser beams in the direction it is facing every **1 second** (`SHOOT_COOLDOWN = 10` ticks).
3. **Temporal Paradox (Ghost Replay)**:
   - A ring buffer snapshots the player's position, direction, and firing actions every 100ms.
   - A "Ghost Ship" is replayed exactly **3 seconds** (`ECHO_DELAY_FRAMES = 30` ticks) behind the player.
   - **The Hazard**: You must not only dodge incoming asteroids but also avoid colliding with your own ghost ship or running into your ghost ship's past lasers!
   - **Tactical Utility**: The ghost ship's past lasers can destroy asteroids, potentially clearing path hazards if you plan ahead.

---

## Architectural Implementation

### Ring Buffer History Recording

A custom ring buffer records the historical trajectory frames of the player:

```c
typedef struct {
    int8_t x, y;
    int8_t face_dx, face_dy;  // facing direction that tick
    bool fired;               // did we fire this tick?
    int8_t fired_dx, fired_dy;
} history_frame_t;
```

During each game tick (100ms), the current player state is pushed to the buffer:

```c
history_frame_t *f = &s->history[s->history_head];
f->x        = s->px;
f->y        = s->py;
f->face_dx  = s->face_dx;
f->face_dy  = s->face_dy;
f->fired    = fired_this_tick;
f->fired_dx = fired_dx;
f->fired_dy = fired_dy;

s->history_head = (s->history_head + 1) % ECHO_HISTORY;
```

To fetch the ghost coordinates, the engine offsets back by `ECHO_DELAY_FRAMES` frames:

```c
uint8_t idx = (s->history_head + ECHO_HISTORY - 1 - ECHO_DELAY_FRAMES) % ECHO_HISTORY;
history_frame_t *gf = &s->history[idx];
s->ghost_x = gf->x;
s->ghost_y = gf->y;
```

---

## Local Simulation

You can simulate this module directly in your browser using Wokwi:
1. Compile the workspace.
2. Open `diagram.json` inside this directory using VS Code with the **Wokwi** extension.
3. Click **Start Simulation** to verify game loop mechanics, graphics drawing, and button debounce routines in real-time.

### Simulation Screenshots

| Start Menu | Active Gameplay |
| :---: | :---: |
| ![Start Menu](../images/wokwi_menu.png) | ![Gameplay](../images/wokwi_gameplay.png) |

