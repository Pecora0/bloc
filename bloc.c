#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "devutils.h"
#define ARENA_IMPLEMENTATION
#include "arena.h"

// TODO: raylib feels like bloat for this, consider migration to glfw or rgfw
#include <raylib.h>
#include <raymath.h>

#define MIN(x, y) ((x) <= (y) ? (x) : (y))
#define MAX(x, y) ((x) >= (y) ? (x) : (y))
#define ABS(x) ((x) >= 0 ? (x) : -(x))

#define BACKGROUND_COLOR DARKGRAY
#define DEFAULT_BLOCK_COLOR BLACK
#define ZOOM_STEP 0.1
#define PAN_STEP 0.01

typedef struct {
    const char **items;
    size_t capacity;
    size_t count;
} String_DA;

typedef struct {
    Vector2 *items;
    size_t cursor;
    size_t count;
    size_t capacity;
} Vector_Stack;

typedef struct {
    Image img;
    Texture tex;
    // TODO: 'part' has to much information. We just need a point and a zoom level, 
    // the aspect ratio should be determined by the screen dimensions
    Rectangle part;
    Vector_Stack stack;
} Draw_Context;

Arena global_arena = {0};
static String_DA input_paths  = {0};
static String_DA output_paths = {0};
Color block_color = DEFAULT_BLOCK_COLOR;

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

Rectangle texture_rectangle(Texture t) {
    Rectangle result = {
        .x = 0,
        .y = 0,
        .width = t.width,
        .height = t.height,
    };
    return result;
}

// NOTE: it makes sense to view a rectangle r as the affine transformation
// (x , y) -> (r.width * x + r.x , r.height * y + r.y)
// represented by the matrix
// [ r.width        0 r.x ]
// [       0 r.height r.y ]
// [       0        0   1 ]

Matrix rectangle_to_matrix(Rectangle r) {
    Matrix result = {
        .m0 = r.width, .m4 = 0,        .m8  = 0, .m12 = r.x,
        .m1 =       0, .m5 = r.height, .m9  = 0, .m13 = r.y,
        .m2 =       0, .m6 = 0,        .m10 = 1, .m14 = 0,
        .m3 =       0, .m7 = 0,        .m11 = 0, .m15 = 1,
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
        const char *name = GetFileNameWithoutExt(input_path);
        const char *ext = GetFileExtension(input_path);
        char *result = arena_sprintf(&global_arena, "%s.bloc%s", name, ext);
        assert(result != NULL);
        arena_da_append(&global_arena, &output_paths, result);
    }
}

Image load_image_from_index(size_t index) {
    if (!FileExists(input_paths.items[index])) {
        printf("[ERROR]: file '%s' does not exist\n", input_paths.items[0]);
        exit(1);
    }

    Image img = LoadImage(input_paths.items[index]);
    if (img.width <= 0 || img.height <= 0) {
        printf("[ERROR]: could not load image '%s'\n", input_paths.items[0]);
        exit(1);
    }
    return img;
}

void edit_image(Vector_Stack stack, Image *img) {
    for (size_t i=0; i<stack.cursor/2; i++) {
        Rectangle rec = hull(stack.items[2*i], stack.items[2*i+1]);
        ImageDrawRectangleRec(img, rec, block_color);
    }
}

void draw_context_load(Draw_Context *ctx, Image img) {
    Texture t = LoadTextureFromImage(img);
    ctx->img = img;
    ctx->tex = t;
    ctx->part = (Rectangle) {
        .x = 0, .y = 0,
        .width = t.width,
        .height = t.height,
    };
}

Draw_Context draw_context_new(Image img) {
    Draw_Context result = {
        .stack = {0},
    };
    draw_context_load(&result, img);
    return result;
}

void draw_context_reset(Draw_Context *ctx) {
    UnloadTexture(ctx->tex);
    UnloadImage(ctx->img);
    ctx->stack.count = 0;
    ctx->stack.cursor = 0;
}

void draw(Draw_Context *ctx) {
    Rectangle screen = {
        .x = 0,
        .y = 0,
        .width = GetScreenWidth(),
        .height = GetScreenHeight(),
    };
    Rectangle display = fit(screen, ctx->part.width / ctx->part.height);
    Matrix screen_to_tex = MatrixMultiply(
            MatrixInvert(rectangle_to_matrix(display)),
            rectangle_to_matrix(ctx->part)
            );
    Matrix tex_to_screen = MatrixInvert(screen_to_tex);

    Vector2 mouse_screen = GetMousePosition();

    DrawTexturePro(ctx->tex, ctx->part, display, Vector2Zero(), 0, WHITE);

    for (size_t i=0; i< ctx->stack.cursor/2; i++) {
        Rectangle r = hull(
                Vector2Transform(ctx->stack.items[2*i+0], tex_to_screen),
                Vector2Transform(ctx->stack.items[2*i+1], tex_to_screen)
                );
        DrawRectangleRec(r, block_color);
    }

    if (ctx->stack.cursor % 2 == 1) {
        Rectangle preview = hull(
                Vector2Transform(ctx->stack.items[ctx->stack.cursor-1], tex_to_screen),
                mouse_screen
                );
        DrawRectangleRec(preview, ColorAlpha(block_color, 0.7));
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
    Rectangle screen = {
        .x = 0,
        .y = 0,
        .width = GetScreenWidth(),
        .height = GetScreenHeight(),
    };
    Rectangle display = fit(screen, ctx->part.width / ctx->part.height);

    Matrix screen_to_tex = MatrixMultiply(
            MatrixInvert(rectangle_to_matrix(display)),
            rectangle_to_matrix(ctx->part)
            );
    Vector2 mouse_screen = GetMousePosition();
    Vector2 mouse_tex    = Vector2Transform(mouse_screen, screen_to_tex);

    push_point(&ctx->stack, mouse_tex);
}

void pan_down(Draw_Context *ctx) {
    float d = PAN_STEP * ctx->part.height;
    ctx->part.y += d;
    float border = texture_rectangle(ctx->tex).y + texture_rectangle(ctx->tex).height - ctx->part.height;
    ctx->part.y = MIN(ctx->part.y, border);
}

void pan_up(Draw_Context *ctx) {
    float d = PAN_STEP * ctx->part.height;
    ctx->part.y -= d;
    float border = texture_rectangle(ctx->tex).y;
    ctx->part.y = MAX(ctx->part.y, border);
}

void pan_left(Draw_Context *ctx) {
    float d = PAN_STEP * ctx->part.width;
    ctx->part.x -= d;
    float border = texture_rectangle(ctx->tex).x;
    ctx->part.x = MAX(ctx->part.x, border);
}

void pan_right(Draw_Context *ctx) {
    float d = PAN_STEP * ctx->part.width;
    ctx->part.x += d;
    float border = texture_rectangle(ctx->tex).x + texture_rectangle(ctx->tex).width - ctx->part.width;
    ctx->part.x = MIN(ctx->part.x, border);
}

void export(Draw_Context *ctx, const char *path) {
    edit_image(ctx->stack, &ctx->img);

    if (!ExportImage(ctx->img, path)) {
        UNIMPLEMENTED("export");
    }
}

// TODO: this behaves weirdly, we should change the aspect ratio when zooming in to fit the screen better
void zoom(Draw_Context *ctx, float steps) {
    float scaler = 1 - ZOOM_STEP*steps;
    Rectangle screen = {
        .x = 0,
        .y = 0,
        .width = GetScreenWidth(),
        .height = GetScreenHeight(),
    };
    Rectangle display = fit(screen, ctx->part.width / ctx->part.height);
    Vector2 mouse_screen = GetMousePosition();

    // math that makes sure that width and height are scaled by scaler and that mouse_tex stays constant
    Vector2 help = Vector2Transform(mouse_screen, MatrixInvert(rectangle_to_matrix(display)));
    ctx->part.x += (1 - scaler) * ctx->part.width  * help.x;
    ctx->part.y += (1 - scaler) * ctx->part.height * help.y;
    ctx->part.width  *= scaler;
    ctx->part.height *= scaler;

    ctx->part = intersect(texture_rectangle(ctx->tex), ctx->part);
}

int main(int argc, const char **argv) {
    parse_commands(argc, argv);

    InitWindow(800, 600, "Bloc");
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    SetTargetFPS(60);

    size_t index = 0;
    Draw_Context ctx = draw_context_new(load_image_from_index(index));

    bool exit_window = false;
    while (!exit_window) {
        exit_window |= WindowShouldClose();

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            click(&ctx);
        }
        if (IsKeyPressed(KEY_U)) {
            undo(&ctx);
        }
        if (IsKeyPressed(KEY_R)) {
            redo(&ctx);
        }
        if (IsKeyPressed(KEY_ENTER)) {
            if (ctx.stack.cursor >= 2) {
                export(&ctx, output_paths.items[index]);
            }
            draw_context_reset(&ctx);
            index++;
            if (index < input_paths.count) {
                Image next = load_image_from_index(index);
                draw_context_load(&ctx, next);
            } else {
                exit_window = true;
            }
        }

        // TODO: make it possible to zoom by keyboard presses (+/-)
        zoom(&ctx, GetMouseWheelMove());

        // pan
        if (IsKeyDown(KEY_J)) {
            pan_down(&ctx);
        }
        if (IsKeyDown(KEY_K)) {
            pan_up(&ctx);
        }
        if (IsKeyDown(KEY_H)) {
            pan_left(&ctx);
        }
        if (IsKeyDown(KEY_L)) {
            pan_right(&ctx);
        }

        BeginDrawing();

        ClearBackground(BACKGROUND_COLOR);
        draw(&ctx);

        EndDrawing();
    }

    if (index < input_paths.count) {
        // if we did not edit all given images export the current one anyways
        if (ctx.stack.cursor >= 2) {
            export(&ctx, output_paths.items[index]);
        }
        draw_context_reset(&ctx);
    }

    CloseWindow();

    arena_free(&global_arena);
}
