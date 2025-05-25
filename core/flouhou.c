#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include "pew.h"
#include "flouhou.h"

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

#define ColorWhite 0
#define ColorBlack 1

// TODO: Imporve death animation (explosion)
// TODO: Game over screen that is skippable by input

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
        .x = map(-1.0, 1.0, 64, 128 - ENEMY_WIDTH, x),
        .y = map(-1.0, 1.0, 0, 64 - ENEMY_HEIGHT, y),
    };
}

/// Calcutate the the shoot cooldown of the enemy from the amount of hits it
/// has tacken.
int hits_to_enemy_shootcooldown(int hits) {
    return (int)(24 * pow(ENEMY_COOLDOWN_RETENTION_PER_HIT, (double)hits));
}

/// Calculate the speed of enemy pews from the amount of this the enemy has
/// taken
float hits_to_enemy_pew_speed(int hits) {
    return pow(1 / ENEMY_COOLDOWN_RETENTION_PER_HIT, (double)hits);
}

/// Draw stars
void draw_stars(int ticks) {
    fou_draw_dot(-(int)((1.6f * ticks) + 23) % 141 + 128, 13);
    fou_draw_dot(-(int)((0.5f * ticks) + 2) % 130 + 128, 20);
    fou_draw_dot(-(int)((1.0f * ticks) + 40) % 129 + 128, 26);
    fou_draw_dot(-(int)((0.76f * ticks) + 210) % 155 + 128, 46);
    fou_draw_dot(-(int)((0.45 * ticks) + 428) % 200 + 128, 40);
    fou_draw_dot(-(int)((1.0 * ticks) + 220) % 152 + 128, 54);
    // fou_draw_dot(-(int)((8 * 1.6f * ticks) + 23) % 141 + 128, 13);
    // fou_draw_dot(-(int)((8 * 0.5f * ticks) + 2) % 130 + 128, 20);
    // fou_draw_dot(-(int)((8 * 1.0f * ticks) + 40) % 129 + 128, 26);
    // fou_draw_dot(-(int)((8 * 0.76f * ticks) + 210) % 155 + 128, 46);
    // fou_draw_dot(-(int)((8 * 0.45 * ticks) + 428) % 200 + 128, 40);
    // fou_draw_dot(-(int)((8 * 1.0 * ticks) + 220) % 152 + 128, 54);
}

void draw_outlined_str(uint8_t x, uint8_t y, const char* c_str)  {
    fou_draw_str(x - 1, y, c_str);
    fou_draw_str(x + 1, y, c_str);
    fou_draw_str(x, y - 1, c_str);
    fou_draw_str(x, y + 1, c_str);
    fou_invert_color();
    fou_draw_str(x, y, c_str);
    fou_invert_color();
}

void draw_outlined_icon(int8_t x, int8_t y, Fou_Icon icon) {
    fou_invert_color();
    fou_draw_icon(x - 1, y, icon);
    fou_draw_icon(x + 1, y, icon);
    fou_draw_icon(x, y - 1, icon);
    fou_draw_icon(x, y + 1, icon);
    fou_invert_color();
    fou_draw_icon(x, y, icon);
}

void draw_enemy(const Game_State* game_state) {
    Position p = calculate_bad_position(game_state->ticks);
    uint8_t x = p.x;
    uint8_t y = p.y;
    bool invert_color_for_flicker_animation = game_state->enemy.hit_cooldown_ticks_left % 2 == 0;
    if (invert_color_for_flicker_animation) {
        fou_invert_color();
    }
    fou_draw_icon(x - 1, y, FOU_ICON_BADFILL);
    fou_draw_icon(x, y + 1, FOU_ICON_BADFILL);
    fou_draw_icon(x, y - 1, FOU_ICON_BADFILL);
    fou_draw_icon(x + 1, y, FOU_ICON_BADFILL);
    fou_invert_color();
    // make enemy laugh when player died
    if (game_state->player.lifes_left == 0) {
        fou_draw_icon(x, y, game_state->ticks % 16 > 8 ? FOU_ICON_BADLAUGH0 : FOU_ICON_BADLAUGH1);
    } else {
        fou_draw_icon(x, y, game_state->ticks % 48 > 24 ? FOU_ICON_BAD0 : FOU_ICON_BAD1);
    }
    fou_invert_color();
    if (invert_color_for_flicker_animation) {
        fou_invert_color();
    }
}

void draw_player_death(const Game_State* game_state, int ticks_sice_death) {
    // draw explosion
    fou_set_color(ColorWhite);
    if (ticks_sice_death == 0) {
        fou_draw_disc(game_state->player.x + 3, game_state->player.y + 4, 8);
    } else if(ticks_sice_death == 1) {
        fou_draw_disc(game_state->player.x + 3, game_state->player.y + 4, 12);
    } else if (ticks_sice_death == 2) {
        fou_draw_disc(game_state->player.x + 3, game_state->player.y + 4, 14);
        fou_set_color(ColorBlack);
        fou_draw_disc(game_state->player.x + 3, game_state->player.y + 4, 8);
    } else if (ticks_sice_death == 3) {
        fou_draw_disc(game_state->player.x + 3, game_state->player.y + 4, 15);
        fou_set_color(ColorBlack);
        fou_draw_disc(game_state->player.x + 3, game_state->player.y + 4, 13);
    } else if (ticks_sice_death == 4) {
        fou_draw_disc(game_state->player.x + 3, game_state->player.y + 4, 16);
        fou_set_color(ColorBlack);
        fou_draw_disc(game_state->player.x + 3, game_state->player.y + 4, 15);
    }
    fou_set_color(ColorBlack);
}

void draw_pause_screen() {
    // fou_draw_box(32, 8, 128 - 2 * 32, 64 - 2 * 8);
    fou_set_color(ColorWhite);
    fou_draw_box(24, 14, 128 -  2 * 24, 72 - 2 * 16);
    fou_set_color(ColorBlack);
    fou_draw_frame(25, 15, 126 -  2 * 24, 70 - 2 * 16);
    fou_draw_str(48, 26, "Paused\nBack -> Quit\nShoot -> Resume");
    fou_draw_str(30, 39, "Back  -> Quit");
    fou_draw_str(28, 48, "Shoot -> Resume");
}

bool check_collision(Rect a, Rect b) {
    return (a.x + a.w) > b.x && a.x < (b.x + b.w) && (a.y + a.h) > b.y && a.y < (b.y + b.h);
}

Game_State fou_init_game_state() {
    return (Game_State){
        .ticks = 0,
        .pews = {0},
        .enemy =
            {.hit_cooldown_ticks_left = 0,
             .shoot_cooldown_left = hits_to_enemy_shootcooldown(0),
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

void fou_frame(
    Game_State* game_state,
    Fou_User_Input_State current_frame_input,
    Fou_User_Input_State prev_frame_input)
{
    (void)prev_frame_input;

    // fou_draw_frame(0, 0, 64, 64);
    // fou_invert_color();
    // fou_draw_frame(0, 64, 64, 64);
    // fou_draw_str(0, 64, "hello my name ist twilight sparkle");

    // if (current_frame_input.shoot) {
    //     game_state->should_quit = true;
    // }
    //
    // return;

    if (game_state->paused) {
        if (current_frame_input.back && !prev_frame_input.back) {
            game_state->should_quit = true;
        }
        if (current_frame_input.shoot) {
            game_state->paused = false;
        }
        draw_pause_screen();
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
        Rect enemy_hitbox = {
            .x = epew->x,
            .y = epew->y,
            .w = ENEMY_PEW_WIDTH,
            .h = ENEMY_PEW_HEIGHT,
        };
        Rect screen_hitbox = {
            .x = 0,
            .y = 0,
            .w = 128,
            .h = 64,
        };
        if (!check_collision(enemy_hitbox, screen_hitbox)) {
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
                hits_to_enemy_shootcooldown(game_state->enemy.hits_taken);
            // figure out velocity vector from enemy to player space ship:
            float h_speed = game_state->player.x - enemy_position.x;
            float v_speed = game_state->player.y - enemy_position.y;
            float magnitude = sqrt((double)(h_speed * h_speed + v_speed * v_speed));
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
            *game_state = fou_init_game_state();
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

    fou_set_bitmap_mode(true);
    fou_draw_box(0, 0, 128, 64);
    fou_invert_color();
    draw_stars(game_state->ticks);

    // draw shots
    for(int i = 0; i < game_state->pews.len; i++) {
        Pew pew = game_state->pews.items[i];
        draw_outlined_icon(pew.x, pew.y, FOU_ICON_SHOT);
    }
    fou_invert_color();
    // draw spaceship
    if (game_state->player.lifes_left != 0) {
        fou_invert_color();
        if (game_state->player.invincibility_frames_left % 2 == 0) {
            draw_outlined_icon(
                (uint8_t)game_state->player.x,
                (uint8_t)game_state->player.y,
                FOU_ICON_SPACESHIP);
            // draw space ship twice at screen height offset for seamless transition
            // from bottom to top of screen and vice versa
            draw_outlined_icon(
                (uint8_t)game_state->player.x,
                (uint8_t)game_state->player.y - 64,
                FOU_ICON_SPACESHIP);
        }
        fou_invert_color();
    } else {
        draw_player_death(game_state, game_state->player.ticks_since_death);
    }
    draw_enemy(game_state);
    // fou_invert_color(canvas);
    for(int i = 0; i < game_state->enemy_pews.len; i++) {
        EnemyPew epew = game_state->enemy_pews.items[i];
        draw_outlined_icon(epew.x, epew.y, FOU_ICON_BADPEW);
    }
    // display hits
    char hit_string[32] = {0};
    snprintf(hit_string, sizeof(hit_string), "hits: %i", game_state->enemy.hits_taken);
    draw_outlined_str(80, 10, hit_string);
    // display lifes left as hearts
    fou_invert_color();
    for(int i = 0; i < game_state->player.lifes_left; i++) {
        draw_outlined_icon(8 * i + 2, 2, FOU_ICON_HEART);
    }
    fou_invert_color();
}


