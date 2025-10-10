#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "devutils.h"

#include <raylib.h>
#include <raymath.h>

#define MIN(x, y) ((x) <= (y) ? (x) : (y))
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

Vector2 lerp_rec(Rectangle r, Vector2 t) {
    Vector2 result = {
        .x = r.x + t.x * r.width,
        .y = r.y + t.y * r.height,
    };
    return result;
}

Vector2 ilerp_rec(Rectangle r, Vector2 t) {
    assert(r.width != 0);
    assert(r.height != 0);
    Vector2 result = {
        .x = (t.x - r.x) / r.width,
        .y = (t.y - r.y) / r.height,
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

int main(int argc, const char **argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        exit(1);
    }
    const char *input_path = argv[1];

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
    Rectangle src = tex_rec;

    while (!WindowShouldClose()) {
        Rectangle screen = {
            .x = 0,
            .y = 0,
            .width = GetScreenWidth(),
            .height = GetScreenHeight(),
        };
        Rectangle dst = fit(screen, src.width / src.height);

        Vector2 mouse_view = ilerp_rec(tex_rec, lerp_rec(src, ilerp_rec(dst, GetMousePosition())));

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            push_point(mouse_view);
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
            if (wheel > 0) {
                src.width  *= 1 - ZOOM_STEP;
                src.height *= 1 - ZOOM_STEP;
            } else if (wheel < 0) {
                src.width  *= 1 + ZOOM_STEP;
                src.height *= 1 + ZOOM_STEP;
                src.width = MIN(src.width, tex.width);
                src.height = MIN(src.height, tex.height);
            }
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
        DrawTexturePro(tex, src, dst, Vector2Zero(), 0, WHITE);

        for (size_t i=0; i< stack.cursor/2; i++) {
            Rectangle r = hull(
                    lerp_rec(dst, ilerp_rec(src, lerp_rec(tex_rec, stack.items[2*i+0]))), 
                    lerp_rec(dst, ilerp_rec(src, lerp_rec(tex_rec, stack.items[2*i+1]))) 
                    );
            DrawRectangleRec(r, BLOC_COLOR);
        }

        if (stack.cursor % 2 == 1) {
            Rectangle preview = hull(
                    lerp_rec(dst, ilerp_rec(src, lerp_rec(tex_rec, stack.items[stack.cursor-1]))), 
                    lerp_rec(dst, ilerp_rec(src, lerp_rec(tex_rec, mouse_view)))
                    );
            DrawRectangleRec(preview, ColorAlpha(BLOC_COLOR, 0.7));
        }

        DrawText(TextFormat("%.2f %.2f", mouse_view.x, mouse_view.y), 10, 10, 20, GREEN);

        EndDrawing();
    }

    CloseWindow();

    if (stack.cursor >= 2) {
        Rectangle src = {
            .x = 0,
            .y = 0,
            .width = img.width,
            .height = img.height,
        };
        for (size_t i=0; i<stack.cursor/2; i++) {
            Rectangle rec = hull(lerp_rec(src, stack.items[2*i]), lerp_rec(src, stack.items[2*i+1]));
            ImageDrawRectangleRec(&img, rec, BLOC_COLOR);
        }

        if (!ExportImage(img, "out.jpg")) {
            UNIMPLEMENTED("export failure");
        }
    }
    if (stack.items != NULL) free(stack.items);
}
