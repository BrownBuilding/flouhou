/*
 * Set of functions that need to be implemented.
 */

#include <stdbool.h>

#include "pew.h"

typedef struct {
    float x;
    float y;
} Position;

typedef struct {
    int x;
    int y;
    int w;
    int h;
} Rect;

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
    Enemy_Pews enemy_pews;
    Player player;
    Enemy enemy;
    bool paused;
    bool should_quit; // Communicate to event loop that the game should close
} Game_State;

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
} Fou_User_Input_State;

typedef enum {
    FOU_ICON_BADFILL,
    FOU_ICON_BADLAUGH0,
    FOU_ICON_BADLAUGH1,
    FOU_ICON_BAD0,
    FOU_ICON_BAD1,
    FOU_ICON_SHOT,
    FOU_ICON_SPACESHIP,
    FOU_ICON_BADPEW,
    FOU_ICON_HEART,
} Fou_Icon;

void fou_frame(
    Game_State* game_state,
    Fou_User_Input_State current_frame,
    Fou_User_Input_State prev_frame
);

Game_State fou_init_game_state();

// external functions that need to be implementd:

void fou_draw_box(int x, int y, int width, int height);
void fou_draw_disc(int x, int y, int radius);
void fou_draw_dot(int x, int y);
void fou_draw_frame(int x, int y, int width, int height);
void fou_draw_icon(int x, int y, Fou_Icon icon);
void fou_draw_str(int x, int y, const char* string);
void fou_invert_color();
void fou_set_bitmap_mode(bool alpha);
void fou_set_color(bool color);

