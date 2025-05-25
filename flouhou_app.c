#include "core/mutex.h"
#include "gui/canvas.h"
#include <stdint.h>
#include <stdlib.h>

#include <flouhou_icons.h>
#include "core/flouhou.h"
#include <gui/gui.h>

typedef enum {
    DRAW_CALL_FOU_DRAW_BOX,
    DRAW_CALL_FOU_DRAW_DISC,
    DRAW_CALL_FOU_DRAW_DOT,
    DRAW_CALL_FOU_DRAW_FRAME,
    DRAW_CALL_FOU_DRAW_ICON,
    DRAW_CALL_FOU_DRAW_STR,
    DRAW_CALL_FOU_INVERT_COLOR,
    DRAW_CALL_FOU_SET_BITMAP_MODE,
    DRAW_CALL_FOU_SET_COLOR,
} Draw_Call_Kind;

typedef enum {
    FOU_USERINPUT_UP,
    FOU_USERINPUT_DOWN,
    FOU_USERINPUT_LEFT,
    FOU_USERINPUT_RIGHT,
    FOU_USERINPUT_BACK,
    FOU_USERINPUT_SHOOT,
} Fouapp_User_Input;

typedef struct {
    Fouapp_User_Input user_input;
    bool pressed; // true on press, false on release
} Fouapp_Queue_Event_Input;

typedef enum {
    FOUAPP_QUEUEEVENTKIND_TICK,
    FOUAPP_QUEUEEVENTKIND_INPUT,
} Fouapp_Queue_Event_Kind;

typedef struct {
    Fouapp_Queue_Event_Kind kind;
    Fouapp_Queue_Event_Input input;
} Fouapp_Queue_Event;

typedef struct {
    Draw_Call_Kind kind;
    union {
        struct {int x; int y; int width; int height;} fou_draw_box;
        struct {int x; int y; int radius;} fou_draw_disc;
        struct {int x; int y;} fou_draw_dot;
        struct {int x; int y; int width; int height;} fou_draw_frame;
        struct {int x; int y; Fou_Icon icon;} fou_draw_icon;
        struct {int x; int y; size_t string_idx;} fou_draw_str;
        struct {;} fou_invert_color;
        struct {bool alpha;} fou_set_bitmap_mode;
        struct {bool color;} fou_set_color;
    };
} Draw_Call;

struct {
    Draw_Call* items;
    size_t size;
    size_t cap;
} draw_calls = {0};

struct {
    char* items;
    size_t size;
    size_t cap;
} draw_strings = {0};


typedef struct {
    FuriMessageQueue* message_queue;
    FuriMutex* draw_call_mutex;
} DrawCallbackData;


#define MAX_DRAW_CALLS 128

// maybe have `Draw_Call`s and `Draw_String`s live on the same arena in the future?

void push_draw_call(Draw_Call draw_call) {
    if (draw_calls.items == NULL) {
        draw_calls.items = malloc(sizeof(Draw_Call) * MAX_DRAW_CALLS);
    }
    if (draw_calls.size != MAX_DRAW_CALLS) {
        draw_calls.items[draw_calls.size++] = draw_call;
    } else {
        furi_check(false, "no more room for draw calls :(");
    }
}

size_t push_draw_string(const char* string) {
    int len = strlen(string) + 1;
    size_t start_of_string = draw_strings.size;
    while (draw_strings.cap < draw_strings.size + len) {
        draw_strings.cap = 2 * (draw_strings.cap + 1);
    }
    draw_strings.items = realloc(draw_strings.items, sizeof(char) * draw_strings.cap);
    furi_check(draw_strings.items != NULL);

    memcpy(draw_strings.items + draw_strings.size, string, len - 1);
    draw_strings.items[draw_strings.size + len - 1] = 0;
    draw_strings.size += len;

    return start_of_string;
}

void fou_draw_box(int x, int y, int width, int height) {
    Draw_Call draw_call;
    draw_call.kind = DRAW_CALL_FOU_DRAW_BOX;
    draw_call.fou_draw_box.x = x;
    draw_call.fou_draw_box.y = y;
    draw_call.fou_draw_box.width = width;
    draw_call.fou_draw_box.height = height;
    push_draw_call(draw_call);
}

void fou_draw_disc(int x, int y, int radius) {
    Draw_Call draw_call;
    draw_call.kind = DRAW_CALL_FOU_DRAW_DISC;
    draw_call.fou_draw_disc.x = x;
    draw_call.fou_draw_disc.y = y;
    draw_call.fou_draw_disc.radius = radius;
    push_draw_call(draw_call);
}

void fou_draw_dot(int x, int y) {
    Draw_Call draw_call;
    draw_call.kind = DRAW_CALL_FOU_DRAW_DOT;
    draw_call.fou_draw_disc.x = x;
    draw_call.fou_draw_disc.y = y;
    push_draw_call(draw_call);
}

void fou_draw_frame(int x, int y, int width, int height) {
    Draw_Call draw_call;
    draw_call.kind = DRAW_CALL_FOU_DRAW_FRAME;
    draw_call.fou_draw_frame.x = x;
    draw_call.fou_draw_frame.y = y;
    draw_call.fou_draw_frame.width = width;
    draw_call.fou_draw_frame.height = height;
    push_draw_call(draw_call);
}

void fou_draw_icon(int x, int y, Fou_Icon icon) {
    Draw_Call draw_call;
    draw_call.kind = DRAW_CALL_FOU_DRAW_ICON;
    draw_call.fou_draw_icon.x = x;
    draw_call.fou_draw_icon.y = y;
    draw_call.fou_draw_icon.icon = icon;
    push_draw_call(draw_call);
}

void fou_draw_str(int x, int y, const char* string) {
    Draw_Call draw_call;
    draw_call.kind = DRAW_CALL_FOU_DRAW_STR;
    draw_call.fou_draw_str.x = x;
    draw_call.fou_draw_str.y = y;
    draw_call.fou_draw_str.string_idx = push_draw_string(string);
    push_draw_call(draw_call);
}

void fou_invert_color() {
    Draw_Call draw_call;
    draw_call.kind = DRAW_CALL_FOU_INVERT_COLOR;
    push_draw_call(draw_call);
}

void fou_set_bitmap_mode(bool alpha) {
    Draw_Call draw_call;
    draw_call.kind = DRAW_CALL_FOU_SET_BITMAP_MODE;
    draw_call.fou_set_bitmap_mode.alpha = alpha;
    push_draw_call(draw_call);
}

void fou_set_color(bool color) {
    Draw_Call draw_call;
    draw_call.kind = DRAW_CALL_FOU_SET_COLOR;
    draw_call.fou_set_color.color = color;
    push_draw_call(draw_call);
}

const Icon* icon_enum_to_actual_icon(Fou_Icon icon) {
    switch (icon) {
        case FOU_ICON_BADFILL: return &I_BadFill_16x16;
        case FOU_ICON_BADLAUGH1: return &I_BadLaugh1_16x16;
        case FOU_ICON_BAD1: return &I_Bad1_16x16;
        case FOU_ICON_SHOT: return &I_Shot_8x8;
        case FOU_ICON_SPACESHIP: return &I_SpaceShip_8x8;
        case FOU_ICON_BADPEW: return &I_BadPew_16x16;
        case FOU_ICON_HEART: return &I_Heart_8x8;
    }
    furi_check(false);
}

static void my_draw_callback(Canvas* canvas, void* context) {
    FuriMutex* draw_call_mutex = context;

    if (canvas == NULL) {
        return;
    }

    furi_check(
        furi_mutex_acquire(draw_call_mutex, 0) == FuriStatusOk,
        "could not aquire mutex"
    );
    for (size_t i = 0; i < draw_calls.size; i++) {
        Draw_Call dc = draw_calls.items[i];
        switch (dc.kind) {
            case DRAW_CALL_FOU_DRAW_BOX:
                canvas_draw_box(
                    canvas,
                    dc.fou_draw_box.x,
                    dc.fou_draw_box.y,
                    dc.fou_draw_box.width,
                    dc.fou_draw_box.height
                );
            break;
            case DRAW_CALL_FOU_DRAW_DISC:
                canvas_draw_disc(
                    canvas,
                    dc.fou_draw_disc.x,
                    dc.fou_draw_disc.y,
                    dc.fou_draw_disc.radius
                );
            break;
            case DRAW_CALL_FOU_DRAW_DOT:
                canvas_draw_dot(
                    canvas,
                    dc.fou_draw_dot.x,
                    dc.fou_draw_dot.y
                );
            break;
            case DRAW_CALL_FOU_DRAW_FRAME:
                canvas_draw_frame(
                    canvas,
                    dc.fou_draw_frame.x,
                    dc.fou_draw_frame.y,
                    dc.fou_draw_frame.width,
                    dc.fou_draw_frame.height
                );
            break;
            case DRAW_CALL_FOU_DRAW_ICON:
                canvas_draw_icon(
                    canvas,
                    dc.fou_draw_icon.x,
                    dc.fou_draw_icon.y,
                    icon_enum_to_actual_icon(dc.fou_draw_icon.icon)
                );
            break;
            case DRAW_CALL_FOU_DRAW_STR:
                canvas_draw_str(
                    canvas,
                    dc.fou_draw_str.x,
                    dc.fou_draw_str.y,
                    &draw_strings.items[dc.fou_draw_str.string_idx]
                );
            break;
            case DRAW_CALL_FOU_INVERT_COLOR:
                canvas_invert_color(canvas);
            break;
            case DRAW_CALL_FOU_SET_BITMAP_MODE:
                canvas_set_bitmap_mode(canvas, dc.fou_set_bitmap_mode.alpha);
            break;
            case DRAW_CALL_FOU_SET_COLOR:
                canvas_set_color(canvas, dc.fou_set_color.color);
            break;
        }
    }

    furi_check(
        furi_mutex_release(draw_call_mutex) == FuriStatusOk,
        "could not release mutex"
    );
}

static void my_input_callback(InputEvent* inputevent, void* context) {
    FuriMessageQueue** queue = context;
    if (inputevent == NULL) {
        return;
    }
    Fouapp_Queue_Event_Input event_input = {0};
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
        Fouapp_Queue_Event event = {.kind = FOUAPP_QUEUEEVENTKIND_INPUT, .input = event_input};
        furi_message_queue_put(*queue, &event, FuriWaitForever);
    }
}

static void my_timer_callback(void* context) {
    FuriMessageQueue** msg_queue = context;
    Fouapp_Queue_Event event = {
        .kind = FOUAPP_QUEUEEVENTKIND_TICK,
    };
    furi_message_queue_put(*msg_queue, &event, FuriWaitForever);
}; 


int32_t flouhou_app(void* p) {
    (void)(p);

    // I think the game state struct is too big for the stack so now it lives on
    // the heap.
    Game_State* game_state = malloc(sizeof(Game_State));
    assert(game_state);
    *game_state = fou_init_game_state();


    FuriMutex* draw_call_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    FuriMessageQueue* queue = furi_message_queue_alloc(16, sizeof(Fouapp_Queue_Event));
    furi_check(queue != NULL, "failed to allocate message queue");
    ViewPort* my_view_port = view_port_alloc();

    FuriTimer* timer = furi_timer_alloc(my_timer_callback, FuriTimerTypePeriodic, (void*)&queue);
    furi_check(queue != NULL, "failed to allocate timer");
    uint32_t tick_phase = furi_kernel_get_tick_frequency() / 16;
    furi_check(tick_phase > 0);
    // uint32_t tick_phase = furi_ms_to_ticks(16);
    furi_check(furi_timer_start(timer, tick_phase) == FuriStatusOk, "failed to set timer");

    view_port_draw_callback_set(my_view_port, my_draw_callback, (void*)draw_call_mutex);
    view_port_input_callback_set(my_view_port, my_input_callback, (void*)&queue);

    Gui* gui = furi_record_open(RECORD_GUI);
    furi_check(gui, "could not open furi record (RECORD_GUI)");
    gui_add_view_port(gui, my_view_port, GuiLayerFullscreen);

    Fou_User_Input_State current_frame_input = {0};
    Fou_User_Input_State previous_frame_input = {0};
    (void)previous_frame_input;
    Fou_User_Input_State released_keys = {0};

    Fouapp_Queue_Event event;
    bool should_quit = false;

    while(!should_quit) {

        FuriStatus status = furi_message_queue_get(queue, &event, FuriWaitForever);
        if (status != FuriStatusOk) {
            break;
        }

        switch(event.kind) {
        case FOUAPP_QUEUEEVENTKIND_TICK: {
            furi_check(furi_mutex_acquire(draw_call_mutex, FuriWaitForever) == FuriStatusOk);
            draw_calls.size = 0;
            draw_strings.size = 0;
            fou_frame(game_state, current_frame_input, previous_frame_input);
            furi_check(furi_mutex_release(draw_call_mutex) == FuriStatusOk);
            view_port_update(my_view_port);
            if (game_state->should_quit) {
                should_quit = true;
            }
            previous_frame_input = current_frame_input;
            if (released_keys.up) current_frame_input.up = false;
            if (released_keys.down) current_frame_input.down = false;
            if (released_keys.left) current_frame_input.left = false;
            if (released_keys.right) current_frame_input.right = false;
            if (released_keys.back) current_frame_input.back = false;
            if (released_keys.shoot) current_frame_input.shoot = false;
            released_keys = (Fou_User_Input_State){0};
        } break;

        case FOUAPP_QUEUEEVENTKIND_INPUT: {
            // We could simply mark each button as 'pressed' or 'not pressed'
            // as soon as the user presses or releases the button. But that
            // means that if the user presses and releases the same button
            // within a single frame, that button wouldn't be registered as
            // being pressed at all.
            // That is why every button will be marked as pressed when it is
            // pressed. But as soon as it is released it is marked to be 'unset
            // as pressed'.
            // After the next tick, when a button is makred as being 'unset as
            // pressed', its pressed status will be set to false.
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

    free(game_state);
    furi_message_queue_free(queue);

    furi_timer_stop(timer);
    furi_timer_free(timer);

    gui_remove_view_port(gui, my_view_port);
    view_port_enabled_set(my_view_port, false);
    view_port_free(my_view_port);

    furi_record_close(RECORD_GUI);

    return 0;
}
