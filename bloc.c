#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "devutils.h"

#include <raylib.h>

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

    InitWindow(img.width, img.height, "Bloc");
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    SetTargetFPS(60);

    Texture tex = LoadTextureFromImage(img);

    Mode mode = SELECT_FST;
    Vector2 fst_point;

    while (!WindowShouldClose()) {
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            switch (mode) {
                case SELECT_FST:
                    fst_point = GetMousePosition();
                    mode = SELECT_SND;
                    break;
                case SELECT_SND:
                    Rectangle rec = hull(fst_point, GetMousePosition());
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

        float wheel = GetMouseWheelMove();
        if (wheel > 0.1) {
            UNIMPLEMENTED("zoom in");
        } else if (wheel < -0.1) {
            UNIMPLEMENTED("zoom out");
        }

        BeginDrawing();
        ClearBackground(BACKGROUND_COLOR);
        DrawTexture(tex, 0, 0, WHITE);
        switch (mode) {
            case SELECT_FST:
                break;
            case SELECT_SND:
                Rectangle preview = hull(fst_point, GetMousePosition());
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
