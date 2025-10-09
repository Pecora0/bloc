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

typedef enum {
    SELECT_FST,
    SELECT_SND,
} Mode;

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

    Mode mode = SELECT_FST;
    Vector2 fst_point;

    while (!WindowShouldClose()) {
        Rectangle screen = {
            .x = 0,
            .y = 0,
            .width = GetScreenWidth(),
            .height = GetScreenHeight(),
        };
        Rectangle src = {
            .x = 0,
            .y = 0,
            .width = tex.width,
            .height = tex.height,
        };
        Rectangle dst = fit(screen, (float) tex.width / (float) tex.height);

        Vector2 mouse_view = ilerp_rec(dst, GetMousePosition());

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            switch (mode) {
                case SELECT_FST:
                    fst_point = mouse_view;
                    mode = SELECT_SND;
                    break;
                case SELECT_SND:
                    Vector2 snd_point = mouse_view;
                    Rectangle rec = hull(lerp_rec(src, fst_point), lerp_rec(src, snd_point));
                    ImageDrawRectangleRec(&img, rec, BLOC_COLOR);
                    UnloadTexture(tex);
                    tex = LoadTextureFromImage(img);
                    mode = SELECT_FST;
                    break;
            }
        }
        if (IsKeyPressed(KEY_U)) {
            UNIMPLEMENTED("undo");
        }
        if (IsKeyPressed(KEY_R)) {
            UNIMPLEMENTED("redo");
        }
        if (IsKeyPressed(KEY_S)) {
            UNIMPLEMENTED("save");
        }

        float wheel = GetMouseWheelMove();
        if (wheel > 0.1) {
            UNIMPLEMENTED("zoom in");
        } else if (wheel < -0.1) {
            UNIMPLEMENTED("zoom out");
        }

        BeginDrawing();
        ClearBackground(BACKGROUND_COLOR);
        //DrawRectangleRec(dst, RED);
        DrawTexturePro(tex, src, dst, Vector2Zero(), 0, WHITE);

        switch (mode) {
            case SELECT_FST:
                break;
            case SELECT_SND:
                Rectangle preview = hull(lerp_rec(dst, fst_point), lerp_rec(dst, mouse_view));
                DrawRectangleRec(preview, ColorAlpha(BLOC_COLOR, 0.7));
                break;
        }
        EndDrawing();
    }

    CloseWindow();

    if (!ExportImage(img, "out.jpg")) {
        UNIMPLEMENTED("export failure");
    }
}
