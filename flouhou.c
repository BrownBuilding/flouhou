#include <core/base.h>
#include <core/kernel.h>
#include <core/log.h>
#include <core/message_queue.h>
#include <core/timer.h>
#include <furi_hal_resources.h>
#include <gui/canvas.h>
#include <gui/icon_animation.h>
#include <gui/view_port.h>
#include <input/input.h>
#include <furi.h>
#include <math.h>
#include <gui/gui.h>
#include <notification/notification.h>

#include <flouhou_icons.h>

#include "pew.h"
#include <stdint.h>

#define PLAYER_WIDTH 8
#define PLAYER_HEIGHT 8
#define PLAYER_INVINCIBILITY_FRAMES 32
#define PLAYER_PEW_WIDTH 8
#define PLAYER_PEW_HEIGHT 8
#define PLAYER_DEATH_LENGTH 64
#define PLAYER_DEATH_DEBRIS_COUNT 5

#define ENEMY_WIDTH 16
#define ENEMY_HEIGHT 16
#define ENEMY_HIT_COOLDOWN 16
#define ENEMY_SHOOT_COOLDOWN 32
/// controls how quickly the enemy shoots and how quickly the projectiles
/// become as it takes more hits
#define ENEMY_COOLDOWN_RETENTION_PER_HIT 0.98

#define ENEMY_PEW_WIDTH 8
#define ENEMY_PEW_HEIGHT 8

#define PLAYER_SPEED_RETENTION 0.95
#define MOVEMENT_SPEED 0.5
#define SHOOT_COOLDOWN 8

// TODO: Imporve death animation (explosion)
// TODO: Game over screen that is skippable by input

typedef struct {
    float x;
    float y;
} Position;

typedef struct {
    uint8_t x;
    uint8_t y;
    uint8_t w;
    uint8_t h;
} Rect;

typedef enum {
    FOU_QUEUEEVENTKIND_TICK,
    FOU_QUEUEEVENTKIND_INPUT,
} FouQueueEventKind;

typedef enum {
    FOU_USERINPUT_UP,
    FOU_USERINPUT_DOWN,
    FOU_USERINPUT_LEFT,
    FOU_USERINPUT_RIGHT,
    FOU_USERINPUT_BACK,
    FOU_USERINPUT_SHOOT,
} UserInput;

typedef struct {
} FouQueueEventTick;

typedef struct {
    UserInput user_input;
    bool pressed; // true on press, false on release
} FouQueueEventInput;

typedef struct {
    FouQueueEventKind kind;
    union {
        FouQueueEventInput input;
        FouQueueEventTick tick;
    };
} FouQueueEvent;

typedef struct {
    int lifes_left;
    int ticks_since_death;
    float x;
    float y;
    float h_speed;
    float v_speed;
    int shoot_cooldown_left;
    int invincibility_frames_left; // == 0 means player is vincible
} Player;

typedef struct { // position of enemy is a function of time, so it's not stored.
    int hit_cooldown_ticks_left; // the amount of ticks the enemy is invisible after being hit
    int shoot_cooldown_left;
    int hits_taken;
} Enemy;

typedef struct {
    int ticks;
    Pews pews;
    EnemyPews enemy_pews;
    Player player;
    Enemy enemy;
    bool paused;
    bool should_quit; // Communicate to event loop that the game should close
} GameState;

// typedef struct {
//     GameState game_state;
//     FuriMessageQueue* queue;
// } GameStateAndMsgQueue;

typedef struct {
    bool up;
    bool down;
    bool left;
    bool right;
    bool shoot;
    bool back;
} UserInputState;

float map(float src_min, float src_max, float dst_min, float dst_max, float x) {
    float src_range = src_max - src_min;
    float dst_range = dst_max - dst_min;
    return ((x - src_min) / src_range) * dst_range + dst_min;
}

Position calculate_bad_position(float ticks) {
    // casting ticks from float to double because using floats and sinf will 
    // eventually cause an 'MPU Fault'. No Idea why.
    double ticks_double = (double)ticks;
    float x = sin((double)0.1 * ticks_double);
    float y = sin((double)0.075982851 * ticks_double);
    return (Position){
        .x = map(-1.0, 1.0, 64, 128 - 16, x),
        .y = map(-1.0, 1.0, 0, 64 - 16, y),
    };
}

/// Calcutate the the shoot cooldown of the enemy from the amount of hits it
/// has tacken.
int hits_to_shootcooldown(int hits) {
    return (int)(24 * pow(ENEMY_COOLDOWN_RETENTION_PER_HIT, (double)hits));
}

/// Calculate the speed of enemy pews from the amount of this the enemy has
/// taken
float hits_to_enemy_pew_speed(int hits) {
    return pow(1 / ENEMY_COOLDOWN_RETENTION_PER_HIT, (double)hits);
}

/// Draw stars
void draw_stars(Canvas* canvas, int ticks) {
    canvas_draw_dot(canvas, -(int)((1.6f * ticks) + 23) % 141 + 128, 13);
    canvas_draw_dot(canvas, -(int)((0.5f * ticks) + 2) % 130 + 128, 20);
    canvas_draw_dot(canvas, -(int)((1.0f * ticks) + 40) % 129 + 128, 26);
    canvas_draw_dot(canvas, -(int)((0.76f * ticks) + 210) % 155 + 128, 46);
    canvas_draw_dot(canvas, -(int)((0.45 * ticks) + 428) % 200 + 128, 40);
    canvas_draw_dot(canvas, -(int)((1.0 * ticks) + 220) % 152 + 128, 54);
    // canvas_draw_dot(canvas, -(int)((8 * 1.6f * ticks) + 23) % 141 + 128, 13);
    // canvas_draw_dot(canvas, -(int)((8 * 0.5f * ticks) + 2) % 130 + 128, 20);
    // canvas_draw_dot(canvas, -(int)((8 * 1.0f * ticks) + 40) % 129 + 128, 26);
    // canvas_draw_dot(canvas, -(int)((8 * 0.76f * ticks) + 210) % 155 + 128, 46);
    // canvas_draw_dot(canvas, -(int)((8 * 0.45 * ticks) + 428) % 200 + 128, 40);
    // canvas_draw_dot(canvas, -(int)((8 * 1.0 * ticks) + 220) % 152 + 128, 54);
}

void draw_outlined_str(Canvas* canvas, uint8_t x, uint8_t y, const char* c_str) {
    canvas_draw_str(canvas, x - 1, y, c_str);
    canvas_draw_str(canvas, x + 1, y, c_str);
    canvas_draw_str(canvas, x, y - 1, c_str);
    canvas_draw_str(canvas, x, y + 1, c_str);
    canvas_invert_color(canvas);
    canvas_draw_str(canvas, x, y, c_str);
    canvas_invert_color(canvas);
}

void draw_outlined_icon(Canvas* canvas, int8_t x, int8_t y, const Icon* icon) {
    canvas_invert_color(canvas);
    canvas_draw_icon(canvas, x - 1, y, icon);
    canvas_draw_icon(canvas, x + 1, y, icon);
    canvas_draw_icon(canvas, x, y - 1, icon);
    canvas_draw_icon(canvas, x, y + 1, icon);
    canvas_invert_color(canvas);
    canvas_draw_icon(canvas, x, y, icon);
}

void draw_enemy(Canvas* canvas, const GameState* game_state) {
    Position p = calculate_bad_position(game_state->ticks);
    uint8_t x = p.x;
    uint8_t y = p.y;
    bool invert_color_for_flicker_animation = game_state->enemy.hit_cooldown_ticks_left % 2 == 0;
    if (invert_color_for_flicker_animation) {
        canvas_invert_color(canvas);
    }
    canvas_draw_icon(canvas, x - 1, y, &I_BadFill_16x16);
    canvas_draw_icon(canvas, x, y + 1, &I_BadFill_16x16);
    canvas_draw_icon(canvas, x, y - 1, &I_BadFill_16x16);
    canvas_draw_icon(canvas, x + 1, y, &I_BadFill_16x16);
    canvas_invert_color(canvas);
    // make enemy laugh when player died
    if (game_state->player.lifes_left == 0) {
        canvas_draw_icon(
            canvas, x, y, game_state->ticks % 16 > 8 ? &I_BadLaugh0_16x16 : &I_BadLaugh1_16x16);
    } else {
        canvas_draw_icon(
            canvas, x, y, game_state->ticks % 48 > 24 ? &I_Bad0_16x16 : &I_Bad1_16x16);
    }
    canvas_invert_color(canvas);
    if (invert_color_for_flicker_animation) {
        canvas_invert_color(canvas);
    }
}

void draw_player_death(Canvas* canvas, const GameState* game_state, int ticks_sice_death) {
    // draw explosion
    canvas_set_color(canvas, ColorWhite);
    if (ticks_sice_death == 0) {
        canvas_draw_disc(canvas, game_state->player.x + 3, game_state->player.y + 4, 8);
    } else if(ticks_sice_death == 1) {
        canvas_draw_disc(canvas, game_state->player.x + 3, game_state->player.y + 4, 12);
    } else if (ticks_sice_death == 2) {
        canvas_draw_disc(canvas, game_state->player.x + 3, game_state->player.y + 4, 14);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_disc(canvas, game_state->player.x + 3, game_state->player.y + 4, 8);
    } else if (ticks_sice_death == 3) {
        canvas_draw_disc(canvas, game_state->player.x + 3, game_state->player.y + 4, 15);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_disc(canvas, game_state->player.x + 3, game_state->player.y + 4, 13);
    } else if (ticks_sice_death == 4) {
        canvas_draw_disc(canvas, game_state->player.x + 3, game_state->player.y + 4, 16);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_disc(canvas, game_state->player.x + 3, game_state->player.y + 4, 15);
    }
    canvas_set_color(canvas, ColorBlack);
}

void draw_pause_screen(Canvas* canvas) {
    (void)canvas;
    // canvas_draw_box(canvas, 32, 8, 128 - 2 * 32, 64 - 2 * 8);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, 24, 14, 128 -  2 * 24, 72 - 2 * 16);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_frame(canvas, 25, 15, 126 -  2 * 24, 70 - 2 * 16);
    canvas_draw_str(canvas, 48, 26, "Paused\nBack -> Quit\nShoot -> Resume");
    canvas_draw_str(canvas, 30, 39, "Back  -> Quit");
    canvas_draw_str(canvas, 28, 48, "Shoot -> Resume");
}

bool check_collision(Rect a, Rect b) {
    return (a.x + a.w) > b.x && a.x < (b.x + b.w) && (a.y + a.h) > b.y && a.y < (b.y + b.h);
}

GameState init_game_state() {
    return (GameState){
        .ticks = 0,
        .pews = {0},
        .enemy =
            {.hit_cooldown_ticks_left = 0,
             .shoot_cooldown_left = ENEMY_SHOOT_COOLDOWN,
             .hits_taken = 0},
        .enemy_pews = {0},
        .player = {
            .x = 30.0f,
            .y = 30.0f,
            .shoot_cooldown_left = 0,
            .h_speed = 0,
            .v_speed = 0,
            .lifes_left = 3,
            .ticks_since_death = 0,
            .invincibility_frames_left = 0,
        },
        .paused = false,
        .should_quit = false,
    };
}

static void my_draw_callback(Canvas* canvas, void* context) {
    const GameState* game_state = context;
    if (canvas == NULL) {
        return;
    }
    canvas_set_bitmap_mode(canvas, true);
    canvas_draw_box(canvas, 0, 0, 128, 64);
    canvas_invert_color(canvas);
    draw_stars(canvas, game_state->ticks);
    if (game_state->paused) {
        draw_pause_screen(canvas);
        return;
    }
    // draw shots
    for(int i = 0; i < game_state->pews.len; i++) {
        Pew pew = game_state->pews.items[i];
        draw_outlined_icon(canvas, pew.x, pew.y, &I_Shot_8x8);
    }
    canvas_invert_color(canvas);
    // draw spaceship
    if (game_state->player.lifes_left != 0) {
        canvas_invert_color(canvas);
        if (game_state->player.invincibility_frames_left % 2 == 0) {
            draw_outlined_icon(
                canvas,
                (uint8_t)game_state->player.x,
                (uint8_t)game_state->player.y,
                &I_SpaceShip_8x8);
            // draw space ship twice at screen height offset for seamless transition
            // from bottom to top of screen and vice versa
            draw_outlined_icon(
                canvas,
                (uint8_t)game_state->player.x,
                (uint8_t)game_state->player.y - 64,
                &I_SpaceShip_8x8);
        }
        canvas_invert_color(canvas);
    } else {
        draw_player_death(canvas, game_state, game_state->player.ticks_since_death);
    }
    draw_enemy(canvas, game_state);
    // canvas_invert_color(canvas);
    for(int i = 0; i < game_state->enemy_pews.len; i++) {
        EnemyPew epew = game_state->enemy_pews.items[i];
        draw_outlined_icon(canvas, epew.x, epew.y, &I_BadPew_16x16);
    }
    // display hits
    char hit_string[32] = {0};
    snprintf(hit_string, sizeof(hit_string), "hits: %i", game_state->enemy.hits_taken);
    draw_outlined_str(canvas, 80, 10, hit_string);
    // display lifes left as hearts
    canvas_invert_color(canvas);
    for(int i = 0; i < game_state->player.lifes_left; i++) {
        draw_outlined_icon(canvas, 8 * i + 2, 2, &I_Heart_8x8);
    }
    canvas_invert_color(canvas);
}

static void my_input_callback(InputEvent* inputevent, void* context) {
    FuriMessageQueue** queue = context;
    if (inputevent == NULL) {
        return;
    }
    FouQueueEventInput event_input = {0};
    if (inputevent->type == InputTypeRelease) {
        event_input.pressed = false;
    } else {
        event_input.pressed = true;
    }
    bool no_input = false;
    switch(inputevent->key) {
    case InputKeyUp: {
        event_input.user_input = FOU_USERINPUT_UP;
    } break;
    case InputKeyDown: {
        event_input.user_input = FOU_USERINPUT_DOWN;
    } break;
    case InputKeyRight: {
        event_input.user_input = FOU_USERINPUT_RIGHT;
    } break;
    case InputKeyLeft: {
        event_input.user_input = FOU_USERINPUT_LEFT;
    } break;
    case InputKeyBack: {
        event_input.user_input = FOU_USERINPUT_BACK;
    } break;
    case InputKeyOk: {
        event_input.user_input = FOU_USERINPUT_SHOOT;
    } break;
    default: {
        no_input = true;
    } break;
    };
    if (!no_input) {
        FouQueueEvent event = {.kind = FOU_QUEUEEVENTKIND_INPUT, .input = event_input};
        furi_message_queue_put(*queue, &event, FuriWaitForever);
    }
}

static void my_timer_callback(void* context) {
    FuriMessageQueue** msg_queue = context;
    FouQueueEvent event = {
        .kind = FOU_QUEUEEVENTKIND_TICK,
        .tick = (FouQueueEventTick){},
    };
    furi_message_queue_put(*msg_queue, &event, FuriWaitForever);
};


void do_tick(
    GameState* game_state,
    UserInputState current_frame_input,
    UserInputState prev_frame_input)
{
    (void)prev_frame_input;

    if (game_state->paused) {
        if (current_frame_input.back && !prev_frame_input.back) {
            game_state->should_quit = true;
        }
        if (current_frame_input.shoot) {
            game_state->paused = false;
        }
        return;
    }

    if (current_frame_input.back) {
        game_state->paused = true;
    }

    // Change player ship velocity based on input
    if (game_state->player.lifes_left != 0) {
        if (current_frame_input.up) game_state->player.v_speed -= MOVEMENT_SPEED;
        if (current_frame_input.down) game_state->player.v_speed += MOVEMENT_SPEED;
        if (current_frame_input.left) game_state->player.h_speed -= MOVEMENT_SPEED;
        if (current_frame_input.right) game_state->player.h_speed += MOVEMENT_SPEED;
        // make player shoot on input
        if (current_frame_input.shoot && game_state->player.shoot_cooldown_left == 0) {
            pew_add(
                &(game_state->pews), (Pew){.x = game_state->player.x, .y = game_state->player.y});
            game_state->player.shoot_cooldown_left = SHOOT_COOLDOWN;
        }
        //
        if (game_state->player.shoot_cooldown_left != 0) {
            game_state->player.shoot_cooldown_left--;
        }
    }
    // Bounds checking for player shots
    for(int i = game_state->pews.len - 1; i >= 0; i--) {
        game_state->pews.items[i].x += 4;
        if (game_state->pews.items[i].x > 128) {
            pew_remove(&game_state->pews, i);
        }
    }
    // Detect collision of projectiles with enemy
    Position enemy_position = calculate_bad_position(game_state->ticks);
    if (game_state->enemy.hit_cooldown_ticks_left == 0) {
        for(int i = game_state->pews.len - 1; i >= 0; i--) {
            Pew pew = game_state->pews.items[i];
            if (check_collision(
                   (Rect){.x = pew.x, .y = pew.y, .w = PLAYER_PEW_WIDTH, .h = PLAYER_PEW_HEIGHT},
                   (Rect){
                       .x = enemy_position.x,
                       .y = enemy_position.y,
                       .w = ENEMY_WIDTH,
                       .h = ENEMY_HEIGHT})) {
                pew_remove(&game_state->pews, i);
                game_state->enemy.hit_cooldown_ticks_left = ENEMY_HIT_COOLDOWN;
                game_state->enemy.hits_taken++;
            }
        }
    } else {
        game_state->enemy.hit_cooldown_ticks_left--;
    }
    // do enemy shots
    for(int i = game_state->enemy_pews.len - 1; i >= 0; i--) {
        EnemyPew* epew = &game_state->enemy_pews.items[i];
        epew->x += epew->h_speed;
        epew->y += epew->v_speed;
        if (epew->x < 0 - ENEMY_PEW_WIDTH || epew->y < 0 - ENEMY_PEW_HEIGHT || epew->x > 128 ||
           epew->y > 64) {
            enemypew_remove(&game_state->enemy_pews, i);
        }
    }
    if (game_state->player.lifes_left != 0) {
        // check collision with enemy projectile and player
        if (game_state->player.invincibility_frames_left == 0) {
            bool has_been_hit = false;
            for(int i = 0; i < game_state->enemy_pews.len; i++) {
                EnemyPew epew = game_state->enemy_pews.items[i];
                if (check_collision(
                   (Rect){
                        .x = game_state->player.x,
                        .y = game_state->player.y,
                        .w = PLAYER_WIDTH,
                        .h = PLAYER_HEIGHT},
                   (Rect){
                        .x = epew.x, 
                        .y = epew.y, 
                        .w = ENEMY_PEW_WIDTH, 
                        .h = ENEMY_PEW_HEIGHT})) {
                    has_been_hit = true;
                    break;
                }
            }
            // check collision with player and enemy
            if (!has_been_hit && check_collision(
                                    (Rect){
                                        .x = game_state->player.x,
                                        .y = game_state->player.y,
                                        .w = PLAYER_WIDTH,
                                        .h = PLAYER_HEIGHT},
                                    (Rect){
                                        .x = enemy_position.x,
                                        .y = enemy_position.y,
                                        .w = ENEMY_WIDTH,
                                        .h = ENEMY_HEIGHT})) {
                has_been_hit = true;
            }
            if (has_been_hit) {
                game_state->player.lifes_left--;
                game_state->player.invincibility_frames_left = PLAYER_INVINCIBILITY_FRAMES;
            }
        } else {
            game_state->player.invincibility_frames_left--;
        }
        // make enemy shoot
        if (game_state->enemy.shoot_cooldown_left == 0) {
            game_state->enemy.shoot_cooldown_left =
                hits_to_shootcooldown(game_state->enemy.hits_taken);
            // figure out velocity vector from enemy to player space ship:
            float h_speed = game_state->player.x - enemy_position.x;
            float v_speed = game_state->player.y - enemy_position.y;
            float magnitude = sqrtf(h_speed * h_speed + v_speed * v_speed);
            float speed = hits_to_enemy_pew_speed(game_state->enemy.hits_taken);
            h_speed = speed * (h_speed / magnitude);
            v_speed = speed * (v_speed / magnitude);
            enemypews_add(
                &game_state->enemy_pews,
                (EnemyPew){
                    .x = enemy_position.x,
                    .y = enemy_position.y,
                    .h_speed = h_speed,
                    .v_speed = v_speed,
                });
        } else {
            game_state->enemy.shoot_cooldown_left--;
        }
    } else {
        if (game_state->player.ticks_since_death == 64) {
            *game_state = init_game_state();
            return;
        }
        game_state->player.ticks_since_death++;
    }
    // apply velocity to player spaceship
    game_state->player.x += game_state->player.h_speed;
    game_state->player.y += game_state->player.v_speed;
    game_state->player.h_speed *= PLAYER_SPEED_RETENTION;
    game_state->player.v_speed *= PLAYER_SPEED_RETENTION;
    // spaceship bounds checking
    if (game_state->player.y > 64) {
        game_state->player.y -= 64;
    }
    if (game_state->player.y < 0) {
        game_state->player.y += 64;
    }
    if (game_state->player.x < 0) {
        game_state->player.x = 0;
        game_state->player.h_speed = 0;
    }
    if (game_state->player.x > 128 - PLAYER_WIDTH /* player_width */) {
        game_state->player.h_speed = 0;
        game_state->player.x = 128 - PLAYER_WIDTH;
    }
    game_state->ticks++;
}

int32_t flouhou_app(void* p) {
    (void)(p);
    GameState game_state = init_game_state();
    FuriMessageQueue* queue = furi_message_queue_alloc(128, sizeof(FouQueueEvent));
    ViewPort* my_view_port = view_port_alloc();

    FuriTimer* timer = furi_timer_alloc(my_timer_callback, FuriTimerTypePeriodic, (void*)&queue);
    uint32_t tick_phase = furi_kernel_get_tick_frequency() / 16;
    // uint32_t tick_phase = furi_ms_to_ticks(16);
    furi_timer_start(timer, tick_phase);

    view_port_draw_callback_set(my_view_port, my_draw_callback, (void*)&game_state);
    view_port_input_callback_set(my_view_port, my_input_callback, (void*)&queue);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, my_view_port, GuiLayerFullscreen);

    UserInputState current_frame_input = {0};
    UserInputState previous_frame_input = {0};
    UserInputState released_keys = {0};

    FouQueueEvent event;
    bool should_quit = false;
    while(!should_quit) {
        FuriStatus status = furi_message_queue_get(queue, &event, FuriWaitForever);
        if (status != FuriStatusOk) {
            break;
        }

        switch(event.kind) {
        case FOU_QUEUEEVENTKIND_TICK: {
            do_tick(&game_state, current_frame_input, previous_frame_input);
            if (game_state.should_quit) {
                should_quit = true;
            }
            previous_frame_input = current_frame_input;
            if (released_keys.up) current_frame_input.up = false;
            if (released_keys.down) current_frame_input.down = false;
            if (released_keys.left) current_frame_input.left = false;
            if (released_keys.right) current_frame_input.right = false;
            if (released_keys.back) current_frame_input.back = false;
            if (released_keys.shoot) current_frame_input.shoot = false;
            released_keys = (UserInputState){0};
            // current_frame_input = (UserInputState){0};
            view_port_update(my_view_port);
        } break;

        case FOU_QUEUEEVENTKIND_INPUT: {
            switch(event.input.user_input) {
            case FOU_USERINPUT_UP: {
                if (!event.input.pressed) {
                    released_keys.up = true;
                } else {
                    current_frame_input.up = true;
                }
            } break;
            case FOU_USERINPUT_DOWN: {
                if (!event.input.pressed) {
                    released_keys.down = true;
                } else {
                    current_frame_input.down = true;
                }
            } break;
            case FOU_USERINPUT_LEFT: {
                if (!event.input.pressed) {
                    released_keys.left = true;
                } else {
                    current_frame_input.left = true;
                }
            } break;
            case FOU_USERINPUT_RIGHT: {
                if (!event.input.pressed) {
                    released_keys.right = true;
                } else {
                    current_frame_input.right = true;
                }
            } break;
            case FOU_USERINPUT_BACK: {
                if (!event.input.pressed) {
                    released_keys.back = true;
                } else {
                    current_frame_input.back = true;
                }
            } break;
            case FOU_USERINPUT_SHOOT: {
                if (!event.input.pressed) {
                    released_keys.shoot = true;
                } else {
                    current_frame_input.shoot = true;
                }
            } break;
            }
        } break;
        }
     }

    furi_message_queue_free(queue);

    furi_timer_stop(timer);
    furi_timer_free(timer);

    gui_remove_view_port(gui, my_view_port);
    view_port_enabled_set(my_view_port, false);
    view_port_free(my_view_port);

    furi_record_close(RECORD_GUI);

    return 0;
}
