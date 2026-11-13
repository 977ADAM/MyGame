
#include <raylib.h>

// Типы предметов
enum class ItemType {
    Empty,
    IronOre,
    Coal
};

// Максимальный размер стака
int GetMaxStack(ItemType type) {
    if (type == ItemType::IronOre) {
        return 100;
    }
    else if (type == ItemType::Coal) {
        return 200;
    }

    return 100;
}

int main() {
    InitWindow(800, 450, "My Game");

    int x = 400;
    int y = 200;
    int coinX = 600;
    int coinY = 300;
    bool coinCollected = false;
    int speed = 7;
    int score = 0;
    bool inventoryOpen = false;

    SetTargetFPS(60);
    
    const int TILE_SIZE = 20;
    const int COLS = 40;
    const int ROWS = 23;
    
    enum class TileType {
        Empty,
        Wall,
        Gear,
        Generator,
        IronOre,
        Coal
    };
    
    struct InventorySlot {
        ItemType type;
        int amount;
    };
    
    InventorySlot inventory[5] = {
        {ItemType::IronOre, 0},
        {ItemType::Coal, 0},
        {ItemType::Empty, 0},
        {ItemType::Empty, 0},
        {ItemType::Empty, 0}
    };
    
    inventory[0] = {ItemType::IronOre, 60};
    inventory[1] = {ItemType::IronOre, 70};
    
    int selectedSlot = -1;
    
    TileType grid[ROWS][COLS] = {};
    
    grid[10][15] = TileType::IronOre;
    grid[10][16] = TileType::IronOre;
    grid[11][15] = TileType::IronOre;
    grid[11][16] = TileType::IronOre;
    
    grid[5][9] = TileType::Coal;
    grid[5][10] = TileType::Coal;
    
    // ==========================================
    // BUILDING — Выбранный тип объекта
    // ==========================================

    // Объект, который игрок собирается разместить
    TileType selectedTile = TileType::Wall;

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
        
        player = {(float)x, (float)y, 40, 40};

        if (!coinCollected) {
            if (CheckCollisionCircleRec({(float)coinX, (float)coinY}, 15, player)) {
                coinCollected = true;
                score += 1;
            }
        }
        
        // ==========================================
        // INPUT — Переключение объектов
        // ==========================================

        // Клавиша 1 — стена
        if (IsKeyPressed(KEY_ONE)) {
            selectedTile = TileType::Wall;
        }

        // Клавиша 2 — шестерёнка
        if (IsKeyPressed(KEY_TWO)) {
            selectedTile = TileType::Gear;
        }

        // Клавиша 3 — генератор
        if (IsKeyPressed(KEY_THREE)) {
            selectedTile = TileType::Generator;
        }
        
        if (IsKeyPressed(KEY_ZERO)) {
            selectedTile = TileType::Empty;
        }
        
        // ==========================================
        // BUILDING — Проверка возможности строительства
        // ==========================================

        // Получаем позицию мыши
        Vector2 mouse = GetMousePosition();

        // Преобразуем координаты мыши в координаты клетки
        int cellX = (int)mouse.x / TILE_SIZE;
        int cellY = (int)mouse.y / TILE_SIZE;

        // Проверяем, находится ли курсор внутри карты
        bool insideMap =
            cellX >= 0 && cellX < COLS &&
            cellY >= 0 && cellY < ROWS;

        // Можно ли построить объект?
        bool canBuild = false;

        if (insideMap) {

            // Прямоугольник выбранной клетки
            Rectangle cell = {
                (float)(cellX * TILE_SIZE),
                (float)(cellY * TILE_SIZE),
                (float)TILE_SIZE,
                (float)TILE_SIZE
            };

            // Проверяем, свободна ли клетка
            bool isEmpty =
                grid[cellY][cellX] == TileType::Empty;

            // Проверяем столкновение с персонажем
            bool playerCollision =
                CheckCollisionRecs(cell, player);

            // Проверяем столкновение со стеной
            bool wallCollision =
                CheckCollisionRecs(cell, wall);

            // Строить можно, только если все условия выполнены
            canBuild =
                isEmpty &&
                !playerCollision &&
                !wallCollision;

            // ЛКМ — строим
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && canBuild && !inventoryOpen) {
                grid[cellY][cellX] = selectedTile;
            }

            // ПКМ — удаляем
            if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) && !inventoryOpen) {
                grid[cellY][cellX] = TileType::Empty;
            }
        }

        // ==========================================
        // MINING — Добыча рукой
        // ==========================================

        if (selectedTile == TileType::Empty &&
            insideMap &&
            IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !inventoryOpen) {

            if (grid[cellY][cellX] == TileType::IronOre ||
                grid[cellY][cellX] == TileType::Coal) {

                Vector2 playerCenter = {
                    (float)x + 20,
                    (float)y + 20
                };

                Vector2 oreCenter = {
                    (float)(cellX * TILE_SIZE + 10),
                    (float)(cellY * TILE_SIZE + 10)
                };

                if (CheckCollisionPointCircle(oreCenter, playerCenter, 60.0f)) {

                    ItemType minedType = ItemType::Empty;

                    // Определяем, какой ресурс добыли
                    if (grid[cellY][cellX] == TileType::IronOre) {
                        minedType = ItemType::IronOre;
                    }
                    else if (grid[cellY][cellX] == TileType::Coal) {
                        minedType = ItemType::Coal;
                    }
                    
                    int maxStack = GetMaxStack(minedType);

                    bool itemAdded = false;

                    // Ищем слот с таким же предметом, в котором есть место
                    for (int i = 0; i < 5; i++) {

                        if (inventory[i].type == minedType &&
                            inventory[i].amount < maxStack) {

                            inventory[i].amount += 1;
                            itemAdded = true;
                            break;
                        }
                    }

                    // Если подходящего предмета нет, ищем пустой слот
                    if (!itemAdded) {
                        for (int i = 0; i < 5; i++) {

                            if (inventory[i].type == ItemType::Empty || inventory[i].amount == 0) {
                                inventory[i].type = minedType;
                                inventory[i].amount = 1;
                                itemAdded = true;
                                break;
                            }
                        }
                    }

                    // Удаляем руду только после добавления в инвентарь
                    if (itemAdded) {
                        grid[cellY][cellX] = TileType::Empty;
                    }

                }
            }
        }
        
        if (IsKeyPressed(KEY_E)) {
            inventoryOpen = !inventoryOpen;
        }
        
        // Перемещение предметов в инвентаре
        if (inventoryOpen) {

            // Нажали ЛКМ — выбираем предмет
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {

                selectedSlot = -1;

                for (int i = 0; i < 5; i++) {

                    Rectangle slot = {
                        (float)(220 + i * 70),
                        150,
                        60,
                        60
                    };

                    if (CheckCollisionPointRec(mouse, slot) &&
                        inventory[i].amount > 0) {

                        selectedSlot = i;
                        break;
                    }
                }
            }

            // Отпустили ЛКМ — перемещаем предмет
            if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
                selectedSlot != -1) {

                for (int i = 0; i < 5; i++) {

                    Rectangle slot = {
                        (float)(220 + i * 70),
                        150,
                        60,
                        60
                    };

                    if (CheckCollisionPointRec(mouse, slot)) {

                        if (selectedSlot != i) {

                            // Если предметы одинаковые
                            if (inventory[selectedSlot].type == inventory[i].type &&
                                inventory[i].type != ItemType::Empty) {

                                // Сколько свободного места в целевом слоте
                                int freeSpace = GetMaxStack(inventory[i].type)
                                                - inventory[i].amount;

                                // Сколько предметов хотим переместить
                                int moveCount = inventory[selectedSlot].amount;

                                // Не превышаем ограничение стака
                                if (moveCount > freeSpace) {
                                    moveCount = freeSpace;
                                }

                                // Перемещаем предметы
                                if (moveCount > 0) {
                                    inventory[i].amount += moveCount;
                                    inventory[selectedSlot].amount -= moveCount;
                                }

                                // Если исходный слот опустел
                                if (inventory[selectedSlot].amount == 0) {
                                    inventory[selectedSlot].type = ItemType::Empty;
                                }
                            }
                            else {
                                // Разные предметы — меняем местами
                                InventorySlot temp = inventory[selectedSlot];

                                inventory[selectedSlot] = inventory[i];
                                inventory[i] = temp;
                            }
                        }

                        break;
                    }
                }

                selectedSlot = -1;
            }
        }
        else {
            selectedSlot = -1;
        }
        
        BeginDrawing();

        ClearBackground(RAYWHITE);
        
        for (int row = 0; row < ROWS; row++) {
            for (int col = 0; col < COLS; col++) {

                TileType tile = grid[row][col];

                Color color;

                if (tile == TileType::Wall) {
                    color = GRAY;
                }
                else if (tile == TileType::Generator) {
                    color = GREEN;
                }
                else if (tile == TileType::Gear) {
                    color = ORANGE;
                }
                else if (tile == TileType::IronOre) {
                    color = BROWN;
                }
                else if (tile == TileType::Coal) {
                    color = BLACK;
                }
                else {
                    continue;
                }

                DrawRectangle(
                    col * TILE_SIZE,
                    row * TILE_SIZE,
                    TILE_SIZE,
                    TILE_SIZE,
                    color
                );
            }
        }
        
        for (int x = 0; x < 800; x += TILE_SIZE) {
            DrawLine(x, 0, x, 450, LIGHTGRAY);
        }

        for (int y = 0; y < 450; y += TILE_SIZE) {
            DrawLine(0, y, 800, y, LIGHTGRAY);
        }
        
        DrawText(TextFormat("Score: %i", score), 10, 10, 20, BLACK);
        
        DrawText(
            TextFormat("Iron: %i", inventory[0].amount),
            10, 35, 20, DARKBROWN
        );
        
        DrawRectangle(x, y, 40, 40, BLUE);
        DrawRectangleRec(wall, RED);
        if (!coinCollected) {
            DrawCircle(coinX, coinY, 15, GOLD);
        }
        
        // ==========================================
        // RENDER — Предпросмотр строительства
        // ==========================================

        // Проверяем, что курсор внутри карты
        if (cellX >= 0 && cellX < COLS &&
            cellY >= 0 && cellY < ROWS) {

            // Цвет зависит от выбранного механизма
            Color previewColor = GRAY;

            if (selectedTile == TileType::Wall) {
                previewColor = GRAY;
            }
            else if (selectedTile == TileType::Gear) {
                previewColor = ORANGE;
            }
            else if (selectedTile == TileType::Generator) {
                previewColor = GREEN;
            }

            // Полупрозрачная заливка клетки
            DrawRectangle(
                cellX * TILE_SIZE,
                cellY * TILE_SIZE,
                TILE_SIZE,
                TILE_SIZE,
                Fade(previewColor, 0.4f)
            );

            // Рамка выбранной клетки
            DrawRectangleLines(
                cellX * TILE_SIZE,
                cellY * TILE_SIZE,
                TILE_SIZE,
                TILE_SIZE,
                previewColor
            );
            
            // Если строить нельзя, показываем красный цвет
            if (!canBuild) {
                previewColor = RED;
            }
        }

        // ==========================================
        // UI — Отображение выбранного объекта
        // ==========================================

        // Название выбранного объекта
        const char* selectedName = "Unknown";

        if (selectedTile == TileType::Wall) {
            selectedName = "Wall";
        }
        else if (selectedTile == TileType::Gear) {
            selectedName = "Gear";
        }
        else if (selectedTile == TileType::Generator) {
            selectedName = "Generator";
        }

        // Рисуем интерфейс поверх игрового мира
        DrawRectangle(10, 400, 240, 40, DARKGRAY);

        DrawText(
            TextFormat("Selected: %s", selectedName),
            20, 410, 20, WHITE
        );
        
        if (inventoryOpen) {
            DrawRectangle(200, 80, 400, 290, DARKGRAY);

            DrawText("Inventory", 220, 100, 30, WHITE);

            for (int i = 0; i < 5; i++) {
                int slotX = 220 + i * 70;
                int slotY = 150;

                // Рисуем слот
                DrawRectangle(slotX, slotY, 60, 60, GRAY);
                DrawRectangleLines(slotX, slotY, 60, 60, WHITE);
                
                if (selectedSlot == i) {
                    DrawRectangleLinesEx(
                        {(float)slotX, (float)slotY, 60, 60},
                        3,
                        YELLOW
                    );
                }

                // Если слот не пустой
                if (inventory[i].amount > 0 && i != selectedSlot) {

                    // Пока железо может находиться только в слоте 0
                    if (inventory[i].type == ItemType::IronOre) {
                        DrawCircle(slotX + 30, slotY + 25, 15, BROWN);
                    }
                    else if (inventory[i].type == ItemType::Coal) {
                        DrawCircle(slotX + 30, slotY + 25, 15, BLACK);
                    }

                    // Показываем количество предметов
                    DrawText(
                        TextFormat("%i", inventory[i].amount),
                        slotX + 40,
                        slotY + 40,
                        16,
                        WHITE
                    );
                }
            }
            
            // Предмет, который мы перетаскиваем
            if (selectedSlot != -1) {

                InventorySlot item = inventory[selectedSlot];

                Color itemColor = BROWN;

                if (item.type == ItemType::Coal) {
                    itemColor = BLACK;
                }

                // Рисуем предмет под курсором
                DrawCircle(
                    (int)mouse.x,
                    (int)mouse.y,
                    15,
                    itemColor
                );

                // Показываем количество
                DrawText(
                    TextFormat("%i", item.amount),
                    (int)mouse.x + 10,
                    (int)mouse.y + 10,
                    16,
                    WHITE
                );
            }

        }
        
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
