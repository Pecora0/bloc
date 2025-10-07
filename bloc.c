#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "devutils.h"

#include <raylib.h>

#define MIN(x, y) ((x) <= (y) ? (x) : (y))
#define ABS(x) ((x) >= 0 ? (x) : -(x))

#define PREVIEW_COLOR (Color) {0, 0, 0, 200}
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

int main(int argc, const char **argv) {
    if (argc < 2) {
        printf("Usage: %s <FILE>\n", argv[0]);
        exit(1);
    }
    const char *input_path = argv[1];

    Image img = LoadImage(input_path);
    assert(img.width > 0);
    assert(img.height > 0);

    InitWindow(img.width, img.height, "Bloc");
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
        BeginDrawing();
        DrawTexture(tex, 0, 0, WHITE);
        switch (mode) {
            case SELECT_FST:
                break;
            case SELECT_SND:
                Rectangle preview = hull(fst_point, GetMousePosition());
                DrawRectangleRec(preview, PREVIEW_COLOR);
                break;
        }
        EndDrawing();
    }

    CloseWindow();

    if (!ExportImage(img, "out.jpg")) {
        UNIMPLEMENTED("export failure");
    }
}
