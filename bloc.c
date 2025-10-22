#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

#include "devutils.h"
#define ARENA_IMPLEMENTATION
#include "arena.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define RGFW_IMPORT
#include "RGFW.h"

#define MIN(x, y) ((x) <= (y) ? (x) : (y))
#define MAX(x, y) ((x) >= (y) ? (x) : (y))
#define ABS(x) ((x) >= 0 ? (x) : -(x))

#define BACKGROUND_COLOR    ((Color) {80, 80, 80, 255})
#define DEFAULT_BLOCK_COLOR ((Color) { 0,  0,  0, 255})
#define ZOOM_STEP 0.1
#define PAN_STEP 0.01

typedef struct {
    const char **items;
    size_t capacity;
    size_t count;
} String_DA;

typedef struct {
    float x, y;
} Vector2;

typedef struct {
    float x, y;
    float width, height;
} Rectangle;

typedef struct {
    Vector2 *items;
    size_t cursor;
    size_t count;
    size_t capacity;
} Vector_Stack;

typedef struct Color {
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
} Color;

typedef struct {
    unsigned char *pixel_data;
    int width, height;
    // TODO: 'part' has to much information. We just need a point and a zoom level, 
    // the aspect ratio should be determined by the screen dimensions
    Rectangle part;
    Vector_Stack stack;
} Draw_Context;

Arena global_arena = {0};
static String_DA input_paths  = {0};
static String_DA output_paths = {0};
RGFW_window *win = NULL;
RGFW_surface *surface = NULL;
unsigned char *pixel_buffer;
size_t pixel_stride;
Color block_color = DEFAULT_BLOCK_COLOR;

Vector2 vector2_zero() {
    Vector2 result = {
        .x = 0,
        .y = 0,
    };
    return result;
}

Rectangle hull(Vector2 a, Vector2 b) {
    Rectangle result = {
        .x = MIN(a.x, b.x),
        .y = MIN(a.y, b.y),
        .width  = ABS(b.x - a.x),
        .height = ABS(b.y - a.y),
    };
    return result;
}

Rectangle fit(Rectangle outer, float aspect) {
    // aspect = width / height
    assert(aspect > 0);
    Rectangle result;
    result.width = MIN(aspect * outer.height, outer.width);
    result.height = result.width / aspect;

    //center
    result.x = (outer.width - result.width) / 2   + outer.x;
    result.y = (outer.height - result.height) / 2 + outer.y;

    return result;
}

Rectangle intersect(Rectangle r, Rectangle s) {
    if (r.width  < 0) UNIMPLEMENTED("intersect");
    if (r.height < 0) UNIMPLEMENTED("intersect");
    if (s.width  < 0) UNIMPLEMENTED("intersect");
    if (s.height < 0) UNIMPLEMENTED("intersect");

    float x = fmax(r.x, s.x);
    float y = fmax(r.y, s.y);
    Rectangle result = {
        .x = x,
        .y = y,
        .width  = fmin(r.x + r.width , s.x + s.width)  - x,
        .height = fmin(r.y + r.height, s.y + s.height) - y,
    };
    return result;
}

Rectangle texture_rectangle(Draw_Context *ctx) {
    Rectangle result = {
        .x = 0,
        .y = 0,
        .width = ctx->width,
        .height = ctx->height,
    };
    return result;
}

// NOTE: it makes sense to view a rectangle r as the affine transformation
// (x , y) -> (r.width * x + r.x , r.height * y + r.y)
// represented by the matrix
// [ r.width        0 r.x ]
// [       0 r.height r.y ]
// [       0        0   1 ]

Vector2 rectangle_transform(Vector2 v, Rectangle r) {
    Vector2 result = {
        .x = r.width  * v.x + r.x,
        .y = r.height * v.y + r.y,
    };
    return result;
}

Rectangle rectangle_multiply(Rectangle a, Rectangle b) {
    Rectangle result = {
        .width  = b.width  * a.width,
        .height = b.height * a.height,
        .x = b.width  * a.x + b.x,
        .y = b.height * a.y + b.y,
    };
    return result;
}

Rectangle rectangle_invert(Rectangle a) {
    Rectangle result = {
        .width  = 1 / a.width,
        .height = 1 / a.height,
        .x = - a.x / a.width,
        .y = - a.y / a.height,
    };
    return result;
}

void push_point(Vector_Stack *stack, Vector2 v) {
    if (stack->capacity == 0) {
        stack->capacity = 16;
        stack->cursor = 0;
        stack->count = 0;
        stack->items = arena_alloc(&global_arena, sizeof(v) * stack->capacity);
    }
    assert(stack->cursor <= stack->count);
    assert(stack->count <= stack->capacity);
    if (stack->cursor == stack->capacity) {
        size_t old_capacity = stack->capacity;
        stack->capacity *= 2;
        stack->items = arena_realloc(&global_arena, stack->items, sizeof(v) * old_capacity, sizeof(v) * stack->capacity);
        assert(stack->items != NULL);
    }
    assert(stack->cursor < stack->capacity);
    stack->items[stack->cursor] = v;
    stack->cursor++;
    stack->count = stack->cursor;
}

void print_usage(const char *program) {
    printf("Usage: %s <IMAGE-FILE>\n", program);
}

Color parse_color(const char *str) {
    if (strlen(str) != 6) UNIMPLEMENTED("parse_color");
    long code = strtol(str, NULL, 16);
    assert(0x0 <= code);
    assert(code <= 0xFFFFFF);
    Color result = {
        .r = (code >> 2*8) & 0xFF,
        .g = (code >> 1*8) & 0xFF,
        .b = (code >> 0*8) & 0xFF,
        .a = 0xFF,
    };
    return result;
}

const char *get_file_name(const char *path) {
    char *last_slash = strrchr(path, '/');
    if (last_slash == NULL) {
        return path;
    }
    return arena_strdup(&global_arena, last_slash+1);
}

const char *get_file_name_without_ext(const char *path) {
    const char *name = get_file_name(path);
    char *fst_point = strchr(name, '.');
    if (fst_point == NULL) {
        return name;
    }
    *fst_point = '\0';
    const char *result = arena_strdup(&global_arena, name);
    *fst_point = '.';
    return result;
}

const char *get_file_ext(const char *path) {
    const char *name = get_file_name(path);
    char *fst_point = strchr(name, '.');
    if (fst_point == NULL) {
        return "";
    }
    return arena_strdup(&global_arena, fst_point);
}

void parse_commands(int argc, const char **argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        exit(1);
    }
    for (int i=1; i<argc; i++) {
        if (strcmp(argv[i], "-o") == 0) {
            if (i == argc-1) {
                printf("[ERROR] no matching argument found to '-o' flag\n");
                print_usage(argv[0]);
                exit(1);
            }
            arena_da_append(&global_arena, &output_paths, argv[i+1]);
            i++;
        } else if (strcmp(argv[i], "-c") == 0) {
            if (i == argc-1) {
                printf("[ERROR] no matching argument found to '-i' flag\n");
                print_usage(argv[0]);
                exit(1);
            }
            block_color = parse_color(argv[i+1]);
            i++;
        } else {
            arena_da_append(&global_arena, &input_paths, argv[i]);
        }
    }
    if (input_paths.count == 0) {
        printf("[ERROR] No input file was given\n");
        print_usage(argv[0]);
        exit(1);
    }
    if (output_paths.count > input_paths.count) UNIMPLEMENTED("parse_flags");
    // fill output_paths with default values
    for (size_t i=output_paths.count; i<input_paths.count; i++) {
        const char *input_path = input_paths.items[i];
        const char *name = get_file_name_without_ext(input_path);
        const char *ext = get_file_ext(input_path);
        char *result = arena_sprintf(&global_arena, "%s.bloc%s", name, ext);
        assert(result != NULL);
        arena_da_append(&global_arena, &output_paths, result);
    }
}

// Image load_image_from_index(size_t index) {
//     if (!FileExists(input_paths.items[index])) {
//         printf("[ERROR]: file '%s' does not exist\n", input_paths.items[0]);
//         exit(1);
//     }
// 
//     Image img = LoadImage(input_paths.items[index]);
//     if (img.width <= 0 || img.height <= 0) {
//         printf("[ERROR]: could not load image '%s'\n", input_paths.items[0]);
//         exit(1);
//     }
//     return img;
// }

void draw_context_load(Draw_Context *ctx, const char *path) {
    int width, height, channels_in_file;
    ctx->pixel_data = stbi_load(path, &width, &height, &channels_in_file, 4);
    ctx->width = width;
    ctx->height = height;

    ctx->part = (Rectangle) {
        .x = 0, .y = 0,
        .width = width,
        .height = height,
    };
}

Draw_Context draw_context_new(const char *path) {
    Draw_Context result = {
        .stack = {0},
    };
    draw_context_load(&result, path);
    return result;
}

void draw_context_reset(Draw_Context *ctx) {
    stbi_image_free(ctx->pixel_data);
    ctx->pixel_data = NULL;
    ctx->stack.count = 0;
    ctx->stack.cursor = 0;
}

Vector2 get_mouse_position() {
    int x, y;
    RGFW_window_getMouse(win, &x, &y);
    Vector2 result = {
        .x = x,
        .y = y,
    };
    return result;
}

float get_mouse_wheel_move() {
    UNIMPLEMENTED("get_mouse_wheel_move");
}

Rectangle window_rectangle() {
    int width, height;
    RGFW_window_getSize(win, &width, &height);
    Rectangle result = {
        .x = 0,
        .y = 0,
        .width = width,
        .height = height,
    };
    return result;
}

Color get_color(uint8_t *buffer, size_t pixel_index) {
    Color result = {
        .r = buffer[pixel_index * 4 + 0],
        .g = buffer[pixel_index * 4 + 1],
        .b = buffer[pixel_index * 4 + 2],
        .a = buffer[pixel_index * 4 + 3],
    };
    return result;
}

void set_color(uint8_t *buffer, size_t pixel_index, Color c) {
    buffer[pixel_index * 4 + 0] = c.r;
    buffer[pixel_index * 4 + 1] = c.g;
    buffer[pixel_index * 4 + 2] = c.b;
    buffer[pixel_index * 4 + 3] = c.a;
}

// Algorithm taken from https://github.com/raysan5/raylib/blob/master/src/rtextures.c
void blend_color(uint8_t *buffer, size_t pixel_index, Color c) {
    if (c.a == 0) {
        return;
    } else if (c.a == 255) {
        set_color(buffer, pixel_index, c);
    } else {
        Color out = {255, 255, 255, 255};
        Color dst = get_color(buffer, pixel_index);
        unsigned int alpha = (unsigned int)c.a + 1;     // We are shifting by 8 (dividing by 256), so we need to take that excess into account
        out.a = (unsigned char)(((unsigned int)alpha*256 + (unsigned int)dst.a*(256 - alpha)) >> 8);

        if (out.a > 0)
        {
            out.r = (unsigned char)((((unsigned int)c.r*alpha*256 + (unsigned int)dst.r*(unsigned int)dst.a*(256 - alpha))/out.a) >> 8);
            out.g = (unsigned char)((((unsigned int)c.g*alpha*256 + (unsigned int)dst.g*(unsigned int)dst.a*(256 - alpha))/out.a) >> 8);
            out.b = (unsigned char)((((unsigned int)c.b*alpha*256 + (unsigned int)dst.b*(unsigned int)dst.a*(256 - alpha))/out.a) >> 8);
        }
        set_color(buffer, pixel_index, out);
    }
}

void clear(Color c) {
    Rectangle screen = window_rectangle();
    for (size_t i=0; i<screen.height; i++) {
        for (size_t j=0; j<screen.width; j++) {
            set_color(pixel_buffer, i*pixel_stride + j, c);
        }
    }
}

void draw_image(Draw_Context *ctx, Rectangle dst) {
    dst = intersect(dst, window_rectangle());
    assert(dst.width >= 0);
    assert(dst.height >= 0);
    for (int i=dst.y; i<dst.y+dst.height; i++) {
        for (int j=dst.x; j<dst.x+dst.width; j++) {
            Vector2 pos = {(float) j, (float) i};
            pos = rectangle_transform(pos, rectangle_invert(dst));
            pos = rectangle_transform(pos, ctx->part);

            int index = floorf(pos.y) * ctx->width + floorf(pos.x);
            index = MAX(0, index);
            Color c = get_color(ctx->pixel_data, index);
            blend_color(pixel_buffer, i*pixel_stride + j, c);
        }
    }
}

void draw_rectangle(Rectangle r, Color c) {
    r = intersect(r, window_rectangle());
    assert(r.width >= 0);
    assert(r.height >= 0);
    for (int i=r.y; i<r.y+r.height; i++) {
        for (int j=r.x; j<r.x+r.width; j++) {
            blend_color(pixel_buffer, i*pixel_stride + j, c);
        }
    }
}

Color color_alpha(Color c, float a) {
    a = fmaxf(a, 0);
    a = fminf(a, 1);
    c.a = (uint8_t) (255.0f * a);
    return c;
}

void draw(Draw_Context *ctx) {
    Rectangle screen = window_rectangle();
    Rectangle display = fit(screen, ctx->part.width / ctx->part.height);
    Rectangle screen_to_tex = rectangle_multiply(
            rectangle_invert(display),
            ctx->part
            );
    Rectangle tex_to_screen = rectangle_invert(screen_to_tex);

    Vector2 mouse_screen = get_mouse_position();

    draw_image(ctx, display);

    for (size_t i=0; i< ctx->stack.cursor/2; i++) {
        Rectangle r = hull(
                rectangle_transform(ctx->stack.items[2*i+0], tex_to_screen),
                rectangle_transform(ctx->stack.items[2*i+1], tex_to_screen)
                );
        draw_rectangle(r, block_color);
    }

    if (ctx->stack.cursor % 2 == 1) {
        Rectangle preview = hull(
                rectangle_transform(ctx->stack.items[ctx->stack.cursor-1], tex_to_screen),
                mouse_screen
                );
        draw_rectangle(preview, color_alpha(block_color, 0.7));
    }
}

void undo(Draw_Context *ctx) {
    if (ctx->stack.cursor > 0) {
        ctx->stack.cursor--;
    }
}

void redo(Draw_Context *ctx) {
    if (ctx->stack.cursor < ctx->stack.count) {
        ctx->stack.cursor++;
    }
}

void click(Draw_Context *ctx) {
    Rectangle screen = window_rectangle();
    Rectangle display = fit(screen, ctx->part.width / ctx->part.height);

    Rectangle screen_to_tex = rectangle_multiply(
            rectangle_invert(display),
            ctx->part
            );
    Vector2 mouse_screen = get_mouse_position();
    Vector2 mouse_tex    = rectangle_transform(mouse_screen, screen_to_tex);

    push_point(&ctx->stack, mouse_tex);
}

void pan_down(Draw_Context *ctx) {
    float d = PAN_STEP * ctx->part.height;
    ctx->part.y += d;
    float border = texture_rectangle(ctx).y + texture_rectangle(ctx).height - ctx->part.height;
    ctx->part.y = MIN(ctx->part.y, border);
}

void pan_up(Draw_Context *ctx) {
    float d = PAN_STEP * ctx->part.height;
    ctx->part.y -= d;
    float border = texture_rectangle(ctx).y;
    ctx->part.y = MAX(ctx->part.y, border);
}

void pan_left(Draw_Context *ctx) {
    float d = PAN_STEP * ctx->part.width;
    ctx->part.x -= d;
    float border = texture_rectangle(ctx).x;
    ctx->part.x = MAX(ctx->part.x, border);
}

void pan_right(Draw_Context *ctx) {
    float d = PAN_STEP * ctx->part.width;
    ctx->part.x += d;
    float border = texture_rectangle(ctx).x + texture_rectangle(ctx).width - ctx->part.width;
    ctx->part.x = MIN(ctx->part.x, border);
}

void export(Draw_Context *ctx, const char *path) {
    for (size_t i=0; i<ctx->stack.cursor/2; i++) {
        Rectangle rec = hull(ctx->stack.items[2*i], ctx->stack.items[2*i+1]);
        assert(rec.x >= 0);
        assert(rec.y >= 0);
        assert(rec.x+rec.width < ctx->width);
        assert(rec.y+rec.height < ctx->height);

        for (int i=rec.y; i<rec.y+rec.height; i++) {
            for (int j=rec.x; j<rec.x+rec.width; j++) {
                blend_color(ctx->pixel_data, i*ctx->width + j, block_color);
            }
        }
    }

    if (!stbi_write_jpg(path, ctx->width, ctx->height, 4, ctx->pixel_data, 50)) {
        UNIMPLEMENTED("export");
    }
    printf("[INFO] wrote file '%s'\n", path);
}

// TODO: this behaves weirdly, we should change the aspect ratio when zooming in to fit the screen better
void zoom(Draw_Context *ctx, float steps) {
    float scaler = 1 - ZOOM_STEP*steps;
    Rectangle screen = window_rectangle();
    Rectangle display = fit(screen, ctx->part.width / ctx->part.height);
    Vector2 mouse_screen = get_mouse_position();

    // math that makes sure that width and height are scaled by scaler and that mouse_tex stays constant
    Vector2 help = rectangle_transform(mouse_screen, rectangle_invert(display));
    ctx->part.x += (1 - scaler) * ctx->part.width  * help.x;
    ctx->part.y += (1 - scaler) * ctx->part.height * help.y;
    ctx->part.width  *= scaler;
    ctx->part.height *= scaler;

    ctx->part = intersect(texture_rectangle(ctx), ctx->part);
}

int main(int argc, const char **argv) {
    parse_commands(argc, argv);

    win = RGFW_createWindow("Bloc", 0, 0, 800, 600, RGFW_windowCenter);
	RGFW_window_setExitKey(win, RGFW_escape);

    {
        RGFW_monitor mon = RGFW_window_getMonitor(win);
        pixel_buffer = arena_alloc(&global_arena, sizeof(u8) * mon.mode.w * mon.mode.h * 4);
        pixel_stride = mon.mode.w;
        surface = RGFW_createSurface(pixel_buffer, mon.mode.w, mon.mode.h, RGFW_formatRGBA8);
    }

    size_t index = 0;
    Draw_Context ctx = draw_context_new(input_paths.items[index]);

    bool exit_window = false;
    while (!exit_window) {
        RGFW_pollEvents();
        exit_window |= RGFW_window_shouldClose(win);

        if (RGFW_window_isMousePressed(win, RGFW_mouseLeft)) {
            click(&ctx);
        }
        if (RGFW_isKeyPressed(RGFW_u)) {
            undo(&ctx);
        }
        if (RGFW_isKeyPressed(RGFW_r)) {
            redo(&ctx);
        }
        if (RGFW_isKeyPressed(RGFW_enter)) {
            if (ctx.stack.cursor >= 2) {
                export(&ctx, output_paths.items[index]);
            }
            draw_context_reset(&ctx);
            index++;
            if (index < input_paths.count) {
                draw_context_load(&ctx, input_paths.items[index]);
            } else {
                exit_window = true;
                break;
            }
        }

        // TODO: make it possible to zoom by keyboard presses (+/-)
        // zoom(&ctx, get_mouse_wheel_move());

        // pan
        if (RGFW_isKeyDown(RGFW_j)) {
            pan_down(&ctx);
        }
        if (RGFW_isKeyDown(RGFW_k)) {
            pan_up(&ctx);
        }
        if (RGFW_isKeyDown(RGFW_h)) {
            pan_left(&ctx);
        }
        if (RGFW_isKeyDown(RGFW_l)) {
            pan_right(&ctx);
        }

        // drawing
        
        clear(BACKGROUND_COLOR);
        draw(&ctx);
        RGFW_window_blitSurface(win, surface);
    }

    if (index < input_paths.count) {
        // if we did not edit all given images export the current one anyways
        if (ctx.stack.cursor >= 2) {
            export(&ctx, output_paths.items[index]);
        }
        draw_context_reset(&ctx);
    }

    RGFW_window_close(win);

    arena_free(&global_arena);
}
