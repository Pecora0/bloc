#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "devutils.h"

#include <raylib.h>
#include <raymath.h>

#define MIN(x, y) ((x) <= (y) ? (x) : (y))
#define MAX(x, y) ((x) >= (y) ? (x) : (y))
#define ABS(x) ((x) >= 0 ? (x) : -(x))

#define BACKGROUND_COLOR DARKGRAY
#define BLOC_COLOR BLACK
#define ZOOM_STEP 0.1

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

struct {
    Vector2 *items;
    size_t cursor;
    size_t count;
    size_t capacity;
} stack = {
    .items = NULL,
    .cursor = 0,
    .count = 0,
    .capacity = 0,
};

void push_point(Vector2 v) {
    if (stack.capacity == 0) {
        stack.capacity = 16;
        stack.cursor = 0;
        stack.count = 0;
        stack.items = malloc(sizeof(v) * stack.capacity);
    }
    assert(stack.cursor <= stack.count);
    assert(stack.count <= stack.capacity);
    if (stack.cursor == stack.capacity) {
        stack.capacity *= 2;
        stack.items = realloc(stack.items, sizeof(v) * stack.capacity);
        assert(stack.items != NULL);
    }
    assert(stack.cursor < stack.capacity);
    stack.items[stack.cursor] = v;
    stack.cursor++;
    stack.count = stack.cursor;
}

void print_usage(const char *program) {
    printf("Usage: %s <IMAGE-FILE>\n", program);
}

static const char *input_path;
static const char *output_path;

void parse_flags(int argc, const char **argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        exit(1);
    }
    bool found_input = false;
    bool found_output = false;
    for (int i=1; i<argc; i++) {
        if (strcmp(argv[i], "-o") == 0) {
            if (i == argc-1) {
                printf("[ERROR] no matching argument found to '-o' flag\n");
                print_usage(argv[0]);
                exit(1);
            }
            output_path = argv[i+1];
            i++;
            found_output = true;
        } else {
            input_path = argv[i];
            found_input = true;
        }
    }
    if (!found_input) {
        printf("[ERROR] No input file was given\n");
        print_usage(argv[0]);
        exit(1);
    }
    if (!found_output) {
        const char *name = GetFileNameWithoutExt(input_path);
        const char *ext = GetFileExtension(input_path);
        const char *infix = ".bloc";
        int n = strlen(name) + strlen(ext) + strlen(infix) + 1;
        char *result = malloc(n*sizeof(char));
        int r = sprintf(result, "%s%s%s", name, infix, ext);
        assert(r >= 0);
        output_path = result;
    }
}

int main(int argc, const char **argv) {
    parse_flags(argc, argv);

    if (!FileExists(input_path)) {
        printf("[ERROR]: file '%s' does not exist\n", input_path);
        exit(1);
    }

    Image img = LoadImage(input_path);
    if (img.width <= 0 || img.height <= 0) {
        printf("[ERROR]: could not load image '%s'\n", input_path);
        exit(1);
    }

    InitWindow(800, 600, "Bloc");
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    SetTargetFPS(60);

    Texture tex = LoadTextureFromImage(img);

    Rectangle tex_rec = {
        .x = 0,
        .y = 0,
        .width = tex.width,
        .height = tex.height,
    };
    Rectangle tex_part = tex_rec;

    while (!WindowShouldClose()) {
        Rectangle screen = {
            .x = 0,
            .y = 0,
            .width = GetScreenWidth(),
            .height = GetScreenHeight(),
        };
        Rectangle display = fit(screen, tex_part.width / tex_part.height);

        Matrix screen_to_tex = MatrixMultiply(
                MatrixInvert(rectangle_to_matrix(display)),
                rectangle_to_matrix(tex_part)
                );
        Matrix tex_to_screen = MatrixInvert(screen_to_tex);

        Vector2 mouse_screen = GetMousePosition();
        Vector2 mouse_tex    = Vector2Transform(mouse_screen, screen_to_tex);

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            push_point(mouse_tex);
        }
        if (IsKeyPressed(KEY_U)) {
            if (stack.cursor > 0) {
                stack.cursor--;
            }
        }
        if (IsKeyPressed(KEY_R)) {
            if (stack.cursor < stack.count) {
                stack.cursor++;
            }
        }
        if (IsKeyPressed(KEY_S)) {
            UNIMPLEMENTED("save");
        }

        // Zoom
        {
            float wheel = GetMouseWheelMove();
            float scaler = 1 - ZOOM_STEP*wheel;

            {
                // math that makes sure that width and height are scaled by scaler and that mouse_tex stays constant
                Vector2 help = Vector2Transform(mouse_screen, MatrixInvert(rectangle_to_matrix(display)));
                tex_part.x += (1 - scaler) * tex_part.width  * help.x;
                tex_part.y += (1 - scaler) * tex_part.height * help.y;
                tex_part.width  *= scaler;
                tex_part.height *= scaler;
            }

            tex_part = intersect(tex_rec, tex_part);
        }

        {
            int key = GetKeyPressed();
            if (key != 0) {
                printf("[INFO] Keycode: %d (%c)\n", key, key);
            }
            int unicode = GetCharPressed();
            if (unicode != 0) {
                printf("[INFO] Char: %d (%c)\n", unicode, unicode);
            }
        }

        BeginDrawing();
        ClearBackground(BACKGROUND_COLOR);
        //DrawRectangleRec(dst, RED);
        DrawTexturePro(tex, tex_part, display, Vector2Zero(), 0, WHITE);

        for (size_t i=0; i< stack.cursor/2; i++) {
            Rectangle r = hull(
                    Vector2Transform(stack.items[2*i+0], tex_to_screen),
                    Vector2Transform(stack.items[2*i+1], tex_to_screen)
                    );
            DrawRectangleRec(r, BLOC_COLOR);
        }

        if (stack.cursor % 2 == 1) {
            Rectangle preview = hull(
                    Vector2Transform(stack.items[stack.cursor-1], tex_to_screen),
                    mouse_screen
                    );
            DrawRectangleRec(preview, ColorAlpha(BLOC_COLOR, 0.7));
        }

        {
            int line = 10;
            DrawText(TextFormat("Mouse screen: %.2f %.2f", mouse_screen.x, mouse_screen.y), 10, line, 20, GREEN); line+=30;
            DrawText(TextFormat("Mouse tex   : %.2f %.2f", mouse_tex.x, mouse_tex.y),       10, line, 20, GREEN); line+=30;
        }

        EndDrawing();
    }

    CloseWindow();

    if (stack.cursor >= 2) {
        for (size_t i=0; i<stack.cursor/2; i++) {
            Rectangle rec = hull(stack.items[2*i], stack.items[2*i+1]);
            ImageDrawRectangleRec(&img, rec, BLOC_COLOR);
        }

        if (!ExportImage(img, output_path)) {
            UNIMPLEMENTED("export failure");
        }
    }
    if (stack.items != NULL) free(stack.items);
}
