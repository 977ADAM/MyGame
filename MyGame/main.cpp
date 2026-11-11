
#include <raylib.h>

int main() {
    InitWindow(800, 450, "My Game");

    int x = 400;
    int y = 200;
    int coinX = 600;
    int coinY = 300;
    bool coinCollected = false;
    int speed = 7;
    int score = 0;

    SetTargetFPS(60);

    while (!WindowShouldClose()) {


        int oldX = x;

        if (IsKeyDown(KEY_RIGHT)) x += speed;
        if (IsKeyDown(KEY_LEFT)) x -= speed;

        if (x < 0) x = 0;
        if (x > 760) x = 760;

        Rectangle wall = {500, 150, 100, 50};
        Rectangle player = {(float)x, (float)y, 40, 40};

        if (CheckCollisionRecs(player, wall)) {
            x = oldX;
        }

        int oldY = y;

        if (IsKeyDown(KEY_UP)) y -= speed;
        if (IsKeyDown(KEY_DOWN)) y += speed;

        if (y < 0) y = 0;
        if (y > 410) y = 410;

        player = {(float)x, (float)y, 40, 40};

        if (CheckCollisionRecs(player, wall)) {
            y = oldY;
        }

        if (!coinCollected) {
            if (CheckCollisionCircleRec({(float)coinX, (float)coinY}, 15, player)) {
                coinCollected = true;
                score += 1;
            }
        }

        BeginDrawing();

        ClearBackground(RAYWHITE);
        DrawText(TextFormat("Score: %i", score), 10, 10, 20, BLACK);
        DrawRectangle(x, y, 40, 40, BLUE);
        DrawRectangleRec(wall, RED);
        if (!coinCollected) {
            DrawCircle(coinX, coinY, 15, GOLD);
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
