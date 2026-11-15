
#include <raylib.h>
#include <queue>

// Типы предметов
enum class ItemType {
    Empty,
    IronOre,
    Coal,
    Stone,
    Wood,
    Furnace,
    BlankCard,
    PunchedCard,
    IronPlate,
    Drill
};

// Максимальный размер стака
int GetMaxStack(ItemType type) {
    if (type == ItemType::IronOre) {
        return 100;
    }
    else if (type == ItemType::Coal) {
        return 200;
    }
    else if (type == ItemType::Furnace || type == ItemType::Drill) {
        return 10;
    }

    if (type == ItemType::BlankCard) {
        return 20;
    }
    if (type == ItemType::PunchedCard) {
        return 1;
    }

    return 100;
}

Rectangle GetInventorySlotRect(int index) {
    int col = index % 5;
    int row = index / 5;

    return {
        (float)(220 + col * 70),
        (float)(115 + row * 65),
        60,
        60
    };
}

Rectangle GetFurnaceSlotRect(int index) {
    return {25.0f, (float)(130 + index * 85), 60.0f, 60.0f};
}

Rectangle GetFurnaceReaderRect() {
    return {25, 365, 150, 35};
}

Rectangle GetPunchHoleRect(int row, int column) {
    return {
        (float)(270 + column * 70),
        (float)(155 + row * 85),
        40,
        40
    };
}

void DrawPunchedCard(int x, int y, int command, int argument) {
    DrawRectangle(x, y, 36, 22, BEIGE);
    for (int i = 0; i < 4; i++) {
        if (command & (8 >> i)) {
            DrawCircle(x + 6 + i * 8, y + 6, 2, BLACK);
        }
        if (argument & (1 << i)) {
            DrawCircle(x + 6 + i * 8, y + 16, 2, BLACK);
        }
    }
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
        Coal,
        Drill,
        Boulder,
        Tree,
        Furnace
    };
    
    struct InventorySlot {
        ItemType type;
        int amount;
        int command = 0;
        int argument = 0;
    };

    // Возвращает количество предметов, которые не поместились.
    auto addToInventory = [](
        InventorySlot* slots, int slotCount, ItemType type, int amount
    ) {
        int maxStack = GetMaxStack(type);

        // Сначала дополняем существующие стопки.
        for (int i = 0; i < slotCount && amount > 0; i++) {
            if (slots[i].type != type || slots[i].amount == 0) {
                continue;
            }

            int count = maxStack - slots[i].amount;
            if (count > amount) {
                count = amount;
            }
            slots[i].amount += count;
            amount -= count;
        }

        // Затем занимаем пустые слоты.
        for (int i = 0; i < slotCount && amount > 0; i++) {
            if (slots[i].amount != 0) {
                continue;
            }

            int count = amount;
            if (count > maxStack) {
                count = maxStack;
            }
            slots[i].type = type;
            slots[i].amount = count;
            amount -= count;
        }

        return amount;
    };
    
    const int INVENTORY_SIZE = 20;

    InventorySlot inventory[INVENTORY_SIZE] = {};
    
    int selectedSlot = -1;
    int splitSource = -1;
    bool puncherOpen = false;
    int puncherSlot = -1;
    bool commandHoles[4] = {};
    bool argumentHoles[4] = {};
    struct CraftOrder {
        ItemType result;
        float duration;
    };

    std::queue<CraftOrder> craftQueue;
    float craftTimer = 0.0f;
    
    TileType grid[ROWS][COLS] = {};
    TileType drillDeposits[ROWS][COLS] = {};
    int durability[ROWS][COLS] = {};
    int resourceAmount[ROWS][COLS] = {};

    struct FurnaceStorage {
        InventorySlot ore = {};
        InventorySlot fuel = {};
        InventorySlot output = {};
        InventorySlot reader = {};
        int energy = 0;
        int completedActions = 0;
        bool smelting = false;
        float smeltTimer = 0.0f;
        const char* status = "Insert card";
    };

    FurnaceStorage furnaces[ROWS][COLS] = {};
    
    // Временно для проверки установки бура.
    inventory[0] = {ItemType::Drill, 1};
    
    grid[10][15] = TileType::IronOre;
    grid[10][16] = TileType::IronOre;
    grid[11][15] = TileType::IronOre;
    grid[11][16] = TileType::IronOre;
    
    grid[5][9] = TileType::Coal;
    grid[5][10] = TileType::Coal;
    
    grid[8][12] = TileType::Boulder;
    grid[8][13] = TileType::Boulder;
    grid[9][12] = TileType::Boulder;
    
    grid[7][8] = TileType::Tree;
    grid[7][9] = TileType::Tree;
    grid[8][8] = TileType::Tree;
    
    for (int row = 0; row < ROWS; row++) {
        for (int col = 0; col < COLS; col++) {

            if (grid[row][col] == TileType::IronOre) {
                durability[row][col] = 3;
                resourceAmount[row][col] = 5;
            }
            else if (grid[row][col] == TileType::Coal) {
                durability[row][col] = 2;
                resourceAmount[row][col] = 5;
            }
            else if (grid[row][col] == TileType::Boulder) {
                durability[row][col] = 5;
                resourceAmount[row][col] = 1;
            }
            else if (grid[row][col] == TileType::Tree) {
                durability[row][col] = 4;
                resourceAmount[row][col] = 1;
            }
        }
    }
    
    // ==========================================
    // BUILDING — Выбранный тип объекта
    // ==========================================

    // Объект, который игрок собирается разместить
    TileType selectedTile = TileType::Wall;
    bool placingFurnace = false;
    bool placingDrill = false;
    
    float miningCooldown = 0.0f;
    float hitAnimationTimer = 0.0f;
    float inventoryFullTimer = 0.0f;
    Vector2 hitPosition = {0, 0};

    int dismantleX = -1;
    int dismantleY = -1;
    float dismantleTimer = 0.0f;
    const float DISMANTLE_TIME = 1.0f;

    int openedFurnaceX = -1;
    int openedFurnaceY = -1;

    while (!WindowShouldClose()) {

        bool furnacePlacedThisFrame = false;

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
            placingFurnace = false;
            placingDrill = false;
        }

        // Клавиша 2 — шестерёнка
        if (IsKeyPressed(KEY_TWO)) {
            selectedTile = TileType::Gear;
            placingFurnace = false;
            placingDrill = false;
        }

        // Клавиша 3 — генератор
        if (IsKeyPressed(KEY_THREE)) {
            selectedTile = TileType::Generator;
            placingFurnace = false;
            placingDrill = false;
        }

        
        if (IsKeyPressed(KEY_ZERO)) {
            selectedTile = TileType::Empty;
            placingFurnace = false;
            placingDrill = false;
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

            bool isDeposit =
                grid[cellY][cellX] == TileType::IronOre ||
                grid[cellY][cellX] == TileType::Coal;
            bool suitableTile = placingDrill
                ? isDeposit && resourceAmount[cellY][cellX] > 0
                : isEmpty;

            // Проверяем место для выбранной постройки.
            canBuild =
                suitableTile &&
                !playerCollision &&
                !wallCollision;

            // ЛКМ — строим
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
                canBuild &&
                !inventoryOpen &&
                !placingFurnace && !placingDrill &&
                selectedTile != TileType::Empty) {

                grid[cellY][cellX] = selectedTile;
            }
            
            // Установка печи из инвентаря
            if (placingFurnace &&
                !inventoryOpen &&
                canBuild &&
                IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {

                // Ищем печь в инвентаре
                for (int i = 0; i < INVENTORY_SIZE; i++) {

                    if (inventory[i].type == ItemType::Furnace &&
                        inventory[i].amount > 0) {

                        // Устанавливаем печь
                        grid[cellY][cellX] = TileType::Furnace;
                        furnacePlacedThisFrame = true;

                        // Расходуем один предмет
                        inventory[i].amount--;

                        if (inventory[i].amount == 0) {
                            inventory[i].type = ItemType::Empty;
                        }

                        // Выходим из режима размещения
                        placingFurnace = false;
                        placingDrill = false;

                        break;
                    }
                }
            }

            // Установка изготовленного бура поверх залежи.
            if (placingDrill &&
                !inventoryOpen &&
                canBuild &&
                IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                for (int i = 0; i < INVENTORY_SIZE; i++) {
                    if (inventory[i].type != ItemType::Drill ||
                        inventory[i].amount == 0) {
                        continue;
                    }
                    drillDeposits[cellY][cellX] = grid[cellY][cellX];
                    grid[cellY][cellX] = TileType::Drill;
                    inventory[i].amount--;
                    if (inventory[i].amount == 0) {
                        inventory[i] = {};
                    }
                    placingDrill = false;
                    break;
                }
            }

            // Пока ПКМ удаляет только тестовые постройки.
            if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) && !inventoryOpen) {
                TileType tile = grid[cellY][cellX];
                if (tile == TileType::Wall ||
                    tile == TileType::Gear ||
                    tile == TileType::Generator) {
                    grid[cellY][cellX] = TileType::Empty;
                }
            }
        }
        
        if (miningCooldown > 0.0f) {
            miningCooldown -= GetFrameTime();
        }
        
        if (hitAnimationTimer > 0.0f) {
            hitAnimationTimer -= GetFrameTime();
        }
        
        if (inventoryFullTimer > 0.0f) {
            inventoryFullTimer -= GetFrameTime();
        }

        // ==========================================
        // Разбор печи или бура
        // ==========================================

        bool canDismantle = false;

        if (insideMap &&
            !inventoryOpen &&
            IsMouseButtonDown(MOUSE_BUTTON_RIGHT) &&
            (grid[cellY][cellX] == TileType::Furnace ||
             grid[cellY][cellX] == TileType::Drill)) {

            Vector2 playerCenter = {
                (float)x + 20,
                (float)y + 20
            };

            Vector2 furnaceCenter = {
                (float)(cellX * TILE_SIZE + TILE_SIZE / 2),
                (float)(cellY * TILE_SIZE + TILE_SIZE / 2)
            };

            canDismantle = CheckCollisionPointCircle(
                furnaceCenter, playerCenter, 60.0f
            );
        }

        if (canDismantle) {
            // При переходе на другую постройку начинаем заново.
            if (dismantleX != cellX || dismantleY != cellY) {
                dismantleX = cellX;
                dismantleY = cellY;
                dismantleTimer = 0.0f;
            }

            dismantleTimer += GetFrameTime();

            if (dismantleTimer >= DISMANTLE_TIME) {
                dismantleTimer = DISMANTLE_TIME;

                if (grid[cellY][cellX] == TileType::Drill) {
                    int remaining = addToInventory(
                        inventory, INVENTORY_SIZE, ItemType::Drill, 1
                    );
                    if (remaining == 0) {
                        grid[cellY][cellX] = resourceAmount[cellY][cellX] > 0
                            ? drillDeposits[cellY][cellX] : TileType::Empty;
                        drillDeposits[cellY][cellX] = TileType::Empty;
                        dismantleX = -1;
                        dismantleY = -1;
                        dismantleTimer = 0.0f;
                    }
                    else {
                        inventoryFullTimer = 1.0f;
                    }
                }
                else {
                    FurnaceStorage& furnace = furnaces[cellY][cellX];

                    // Проверяем вместимость на копии инвентаря.
                    InventorySlot candidate[INVENTORY_SIZE];
                    for (int i = 0; i < INVENTORY_SIZE; i++) {
                        candidate[i] = inventory[i];
                    }

                    int furnaceLeft = addToInventory(
                        candidate, INVENTORY_SIZE, ItemType::Furnace, 1
                    );
                    int oreLeft = addToInventory(
                        candidate, INVENTORY_SIZE,
                        ItemType::IronOre,
                        furnace.ore.amount + (furnace.smelting ? 1 : 0)
                    );
                    int platesLeft = addToInventory(
                        candidate, INVENTORY_SIZE,
                        ItemType::IronPlate, furnace.output.amount
                    );
                    int coalLeft = addToInventory(
                        candidate, INVENTORY_SIZE,
                        ItemType::Coal, furnace.fuel.amount
                    );

                    bool cardFits = furnace.reader.amount == 0;
                    if (!cardFits) {
                        for (int i = 0; i < INVENTORY_SIZE; i++) {
                            if (candidate[i].amount == 0) {
                                candidate[i] = furnace.reader;
                                cardFits = true;
                                break;
                            }
                        }
                    }

                    if (furnaceLeft == 0 && oreLeft == 0 && coalLeft == 0 &&
                        platesLeft == 0 && cardFits) {
                        // Всё помещается — применяем результат.
                        for (int i = 0; i < INVENTORY_SIZE; i++) {
                            inventory[i] = candidate[i];
                        }

                        furnace = {};
                        grid[cellY][cellX] = TileType::Empty;

                        dismantleX = -1;
                        dismantleY = -1;
                        dismantleTimer = 0.0f;
                    }
                    else {
                        inventoryFullTimer = 1.0f;
                    }
                }
            }
        }
        else {
            // Отпустили кнопку, отвели курсор или отошли.
            dismantleX = -1;
            dismantleY = -1;
            dismantleTimer = 0.0f;
        }

        // ==========================================
        // MINING — Добыча рукой
        // ==========================================

        if (selectedTile == TileType::Empty &&
            !placingFurnace && !placingDrill &&
            insideMap &&
            IsMouseButtonDown(MOUSE_BUTTON_LEFT) && !inventoryOpen && miningCooldown <= 0.0f) {

            if (grid[cellY][cellX] == TileType::IronOre ||
                grid[cellY][cellX] == TileType::Coal ||
                grid[cellY][cellX] == TileType::Boulder ||
                grid[cellY][cellX] == TileType::Tree) {

                Vector2 playerCenter = {
                    (float)x + 20,
                    (float)y + 20
                };

                Vector2 oreCenter = {
                    (float)(cellX * TILE_SIZE + 10),
                    (float)(cellY * TILE_SIZE + 10)
                };
                
                // Определяем, какой предмет получим
                ItemType resourceType = ItemType::Empty;

                if (grid[cellY][cellX] == TileType::IronOre) {
                    resourceType = ItemType::IronOre;
                }
                else if (grid[cellY][cellX] == TileType::Coal) {
                    resourceType = ItemType::Coal;
                }
                else if (grid[cellY][cellX] == TileType::Boulder) {
                    resourceType = ItemType::Stone;
                }
                else if (grid[cellY][cellX] == TileType::Tree) {
                    resourceType = ItemType::Wood;
                }

                // Проверяем, есть ли место для предмета
                bool hasSpace = false;

                for (int i = 0; i < INVENTORY_SIZE; i++) {

                    // Подходит пустой слот
                    if (inventory[i].amount == 0) {
                        hasSpace = true;
                        break;
                    }

                    // Или неполная стопка такого же ресурса
                    if (inventory[i].type == resourceType &&
                        inventory[i].amount < GetMaxStack(resourceType)) {

                        hasSpace = true;
                        break;
                    }
                }
                
                bool inReach = CheckCollisionPointCircle(
                    oreCenter, playerCenter, 60.0f
                );

                if (inReach && !hasSpace) {
                    inventoryFullTimer = 1.0f;
                }

                if (inReach && hasSpace) {
                    
                    miningCooldown = 0.4f;
                    
                    hitAnimationTimer = 0.15f;
                    hitPosition = oreCenter;
                    
                    durability[cellY][cellX] -= 1;
                    
                    if (durability[cellY][cellX] <= 0) {
                        
                        ItemType minedType = ItemType::Empty;
                        
                        // Определяем, какой ресурс добыли
                        if (grid[cellY][cellX] == TileType::IronOre) {
                            minedType = ItemType::IronOre;
                        }
                        else if (grid[cellY][cellX] == TileType::Coal) {
                            minedType = ItemType::Coal;
                        }
                        else if (grid[cellY][cellX] == TileType::Boulder) {
                            minedType = ItemType::Stone;
                        }
                        else if (grid[cellY][cellX] == TileType::Tree) {
                            minedType = ItemType::Wood;
                        }
                        
                        int maxStack = GetMaxStack(minedType);
                        
                        bool itemAdded = false;
                        
                        // Ищем слот с таким же предметом, в котором есть место
                        for (int i = 0; i < INVENTORY_SIZE; i++) {
                            
                            if (inventory[i].type == minedType &&
                                inventory[i].amount < maxStack) {
                                
                                inventory[i].amount += 1;
                                itemAdded = true;
                                break;
                            }
                        }
                        
                        // Если подходящего предмета нет, ищем пустой слот
                        if (!itemAdded) {
                            for (int i = 0; i < INVENTORY_SIZE; i++) {
                                
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
                            resourceAmount[cellY][cellX]--;

                            if (resourceAmount[cellY][cellX] <= 0) {
                                grid[cellY][cellX] = TileType::Empty;
                            }
                            else {
                                if (minedType == ItemType::IronOre) {
                                    durability[cellY][cellX] = 3;
                                }
                                else if (minedType == ItemType::Coal) {
                                    durability[cellY][cellX] = 2;
                                }
                            }
                        }
                    }

                }
            }
        }
        
        if (inventoryOpen &&
            !puncherOpen &&
            selectedSlot != -1 &&
            inventory[selectedSlot].type == ItemType::BlankCard &&
            IsKeyPressed(KEY_P)) {

            puncherOpen = true;
            puncherSlot = selectedSlot;
            for (int i = 0; i < 4; i++) {
                commandHoles[i] = false;
                argumentHoles[i] = false;
            }
            selectedSlot = -1;
            splitSource = -1;
        }

        if (IsKeyPressed(KEY_E)) {
            if (puncherOpen) {
                puncherOpen = false;
                puncherSlot = -1;
            }
            else {
                inventoryOpen = !inventoryOpen;
                if (!inventoryOpen) {
                    splitSource = -1;
                }
            }
        }

        if (puncherOpen && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            for (int i = 0; i < 4; i++) {
                if (CheckCollisionPointRec(mouse, GetPunchHoleRect(0, i))) {
                    commandHoles[i] = !commandHoles[i];
                }
                if (CheckCollisionPointRec(mouse, GetPunchHoleRect(1, i))) {
                    argumentHoles[i] = !argumentHoles[i];
                }
            }
        }

        int command = 0;
        int argument = 0;
        for (int i = 0; i < 4; i++) {
            command = command * 2 + (commandHoles[i] ? 1 : 0);
            if (argumentHoles[i]) {
                argument += 1 << i;
            }
        }

        const char* commandName = "UNKNOWN";
        if (command == 1) commandName = "RUN";
        else if (command == 2) commandName = "BACK";
        else if (command == 3) commandName = "REVERSE";
        else if (command == 4) commandName = "SLOT";

        bool validPattern =
            ((command == 1 || command == 2 || command == 4) && argument > 0) ||
            (command == 3 && argument == 0);
        
        // Закрываем окно печи вместе с инвентарём.
        if (!inventoryOpen) {
            openedFurnaceX = -1;
            openedFurnaceY = -1;
        }

        // Открываем печь отдельным кликом.
        if (!inventoryOpen &&
            !placingFurnace && !placingDrill &&
            !furnacePlacedThisFrame &&
            insideMap &&
            IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
            grid[cellY][cellX] == TileType::Furnace) {

            Vector2 playerCenter = {
                (float)x + 20,
                (float)y + 20
            };

            Vector2 furnaceCenter = {
                (float)(cellX * TILE_SIZE + TILE_SIZE / 2),
                (float)(cellY * TILE_SIZE + TILE_SIZE / 2)
            };

            if (CheckCollisionPointCircle(
                furnaceCenter, playerCenter, 60.0f
            )) {
                openedFurnaceX = cellX;
                openedFurnaceY = cellY;
                inventoryOpen = true;
            }
        }

        // Перемещение предметов в инвентаре
        if (inventoryOpen && !puncherOpen) {
            bool leftPressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
            bool rightPressed = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
            int selectedBeforeClick = selectedSlot;

            // Первый клик выбирает стопку до следующего клика.
            if (leftPressed && selectedBeforeClick == -1) {
                splitSource = -1;

                for (int i = 0; i < INVENTORY_SIZE; i++) {

                    Rectangle slot = GetInventorySlotRect(i);

                    if (CheckCollisionPointRec(mouse, slot) &&
                        inventory[i].amount > 0) {

                        selectedSlot = i;
                        placingFurnace = inventory[i].type == ItemType::Furnace;
                        placingDrill = inventory[i].type == ItemType::Drill;
                        break;
                    }
                }
            }

            // Выгрузка предметов из печи по клику.
            if (openedFurnaceX != -1 &&
                openedFurnaceY != -1 &&
                selectedBeforeClick == -1 &&
                (leftPressed || rightPressed)) {
                FurnaceStorage& furnace =
                    furnaces[openedFurnaceY][openedFurnaceX];

                InventorySlot* source = nullptr;
                if (CheckCollisionPointRec(mouse, GetFurnaceSlotRect(0))) {
                    source = &furnace.ore;
                }
                else if (CheckCollisionPointRec(mouse, GetFurnaceSlotRect(1))) {
                    source = &furnace.fuel;
                }
                else if (CheckCollisionPointRec(mouse, GetFurnaceSlotRect(2))) {
                    source = &furnace.output;
                }

                if (source != nullptr && source->amount > 0) {
                    int moveCount = source->amount;
                    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
                        moveCount = 1;
                    }

                    int remaining = addToInventory(
                        inventory, INVENTORY_SIZE, source->type, moveCount
                    );
                    source->amount -= moveCount - remaining;

                    if (source->amount == 0) {
                        source->type = ItemType::Empty;
                    }
                    if (remaining > 0) {
                        inventoryFullTimer = 1.0f;
                    }
                }
            }

            // Следующий ЛКМ переносит стопку между слотами инвентаря.
            if (leftPressed && selectedBeforeClick != -1) {
                for (int i = 0; i < INVENTORY_SIZE; i++) {

                    Rectangle slot = GetInventorySlotRect(i);

                    if (CheckCollisionPointRec(mouse, slot)) {

                        if (selectedSlot != i) {

                            // Одинаковые предметы объединяем
                            if (inventory[selectedSlot].type == inventory[i].type &&
                                inventory[i].type != ItemType::Empty) {

                                int freeSpace =
                                    GetMaxStack(inventory[i].type) -
                                    inventory[i].amount;

                                int moveCount = inventory[selectedSlot].amount;

                                if (moveCount > freeSpace) {
                                    moveCount = freeSpace;
                                }

                                if (moveCount > 0) {
                                    inventory[i].amount += moveCount;
                                    inventory[selectedSlot].amount -= moveCount;
                                }

                                if (inventory[selectedSlot].amount == 0) {
                                    inventory[selectedSlot].type = ItemType::Empty;
                                }
                            }
                            else {
                                // Разные предметы меняем местами
                                InventorySlot temp = inventory[selectedSlot];

                                inventory[selectedSlot] = inventory[i];
                                inventory[i] = temp;
                            }
                        }
                        // Клик по исходному слоту отменяет выбор.
                        placingFurnace = false;
                        placingDrill = false;
                        selectedSlot = -1;
                        break;
                    }
                }
            }

            // Выбранная стопка: ЛКМ кладёт всё, ПКМ — один предмет.
            if (selectedBeforeClick != -1 &&
                selectedSlot != -1 &&
                openedFurnaceX != -1 &&
                openedFurnaceY != -1 &&
                (leftPressed || rightPressed)) {

                FurnaceStorage& furnace =
                    furnaces[openedFurnaceY][openedFurnaceX];

                InventorySlot& source = inventory[selectedSlot];
                InventorySlot* target = nullptr;

                if (CheckCollisionPointRec(mouse, GetFurnaceSlotRect(0)) &&
                    source.type == ItemType::IronOre) {
                    target = &furnace.ore;
                }
                else if (CheckCollisionPointRec(mouse, GetFurnaceSlotRect(1)) &&
                         source.type == ItemType::Coal) {
                    target = &furnace.fuel;
                }

                if (target != nullptr) {
                    int moveCount = rightPressed ? 1 : source.amount;
                    int freeSpace = GetMaxStack(source.type) - target->amount;
                    if (moveCount > freeSpace) {
                        moveCount = freeSpace;
                    }

                    if (moveCount > 0) {
                        target->type = source.type;
                        target->amount += moveCount;
                        source.amount -= moveCount;

                        if (source.amount == 0) {
                            source.type = ItemType::Empty;
                            selectedSlot = -1;
                        }
                    }
                }
            }

            // Установка карты сохраняет её команду и аргумент.
            if (openedFurnaceX != -1 &&
                openedFurnaceY != -1 &&
                leftPressed &&
                CheckCollisionPointRec(mouse, GetFurnaceReaderRect()) &&
                selectedBeforeClick != -1 &&
                selectedSlot != -1) {

                FurnaceStorage& furnace =
                    furnaces[openedFurnaceY][openedFurnaceX];
                InventorySlot& source = inventory[selectedSlot];
                if (source.type == ItemType::PunchedCard &&
                    furnace.reader.amount == 0) {
                    furnace.reader = source;
                    furnace.completedActions = 0;
                    source = {};
                    selectedSlot = -1;
                }
            }

            // Для извлечения карты нужен отдельный свободный слот.
            if (openedFurnaceX != -1 &&
                openedFurnaceY != -1 &&
                leftPressed &&
                selectedBeforeClick == -1 &&
                CheckCollisionPointRec(mouse, GetFurnaceReaderRect())) {

                FurnaceStorage& furnace =
                    furnaces[openedFurnaceY][openedFurnaceX];
                if (furnace.reader.amount > 0) {
                    int freeSlot = -1;
                    for (int i = 0; i < INVENTORY_SIZE; i++) {
                        if (inventory[i].amount == 0) {
                            freeSlot = i;
                            break;
                        }
                    }

                    if (freeSlot != -1) {
                        inventory[freeSlot] = furnace.reader;
                        furnace.reader = {};
                    }
                    else {
                        inventoryFullTimer = 1.0f;
                    }
                }
            }

            if (rightPressed && selectedBeforeClick == -1) {
                
                bool clickedSlot = false;

                for (int i = 0; i < INVENTORY_SIZE; i++) {
                    
                    Rectangle slot = GetInventorySlotRect(i);

                    if (CheckCollisionPointRec(mouse, slot)) {
                        
                        clickedSlot = true;

                        // Первый ПКМ — выбираем исходный слот
                        if (splitSource == -1) {

                            if (inventory[i].amount >= 2) {
                                splitSource = i;
                            }
                        }
                        // Второй ПКМ — выбираем место назначения
                        else {

                            if (i != splitSource &&
                                inventory[i].amount == 0) {

                                int half =
                                    inventory[splitSource].amount / 2;

                                inventory[i].type =
                                    inventory[splitSource].type;

                                inventory[i].amount = half;

                                inventory[splitSource].amount -= half;
                            }

                            splitSource = -1;
                        }

                        break;
                    }
                }
                
                if (!clickedSlot) {
                    splitSource = -1;
                }
            }
            
        }
        else {
            selectedSlot = -1;
        }
        
        // ==========================================
        // CRAFTING
        // ==========================================
        
        int stoneCount = 0;
        int woodCount = 0;
        int plateCount = 0;

        for (int i = 0; i < INVENTORY_SIZE; i++) {
            if (inventory[i].type == ItemType::Stone) {
                stoneCount += inventory[i].amount;
            }
            else if (inventory[i].type == ItemType::Wood) {
                woodCount += inventory[i].amount;
            }
            else if (inventory[i].type == ItemType::IronPlate) {
                plateCount += inventory[i].amount;
            }
        }

        Rectangle craftButton = {615, 145, 170, 30};
        Rectangle cardButton = {615, 225, 170, 30};
        Rectangle drillButton = {615, 305, 170, 30};

        ItemType material = ItemType::Empty;
        CraftOrder order = {};
        int required = 0;

        if (inventoryOpen && !puncherOpen &&
            IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (CheckCollisionPointRec(mouse, craftButton) && stoneCount >= 3) {
                material = ItemType::Stone;
                required = 3;
                order = {ItemType::Furnace, 2.0f};
            }
            else if (CheckCollisionPointRec(mouse, cardButton) && woodCount >= 1) {
                material = ItemType::Wood;
                required = 1;
                order = {ItemType::BlankCard, 1.0f};
            }
            else if (CheckCollisionPointRec(mouse, drillButton) && plateCount >= 3) {
                material = ItemType::IronPlate;
                required = 3;
                order = {ItemType::Drill, 3.0f};
            }
        }

        if (required > 0) {
            // Материалы расходуются при заказе.
            for (int i = 0; i < INVENTORY_SIZE && required > 0; i++) {
                if (inventory[i].type != material) {
                    continue;
                }

                int take = inventory[i].amount;
                if (take > required) {
                    take = required;
                }
                inventory[i].amount -= take;
                required -= take;

                if (inventory[i].amount == 0) {
                    inventory[i].type = ItemType::Empty;
                    if (selectedSlot == i) {
                        selectedSlot = -1;
                    }
                    if (splitSource == i) {
                        splitSource = -1;
                    }
                }
            }

            bool wasEmpty = craftQueue.empty();
            craftQueue.push(order);
            if (wasEmpty) {
                craftTimer = order.duration;
            }
        }

        // Готовый заказ ждёт места и задерживает следующие заказы.
        if (!craftQueue.empty()) {
            if (craftTimer > 0.0f) {
                craftTimer -= GetFrameTime();
                if (craftTimer < 0.0f) {
                    craftTimer = 0.0f;
                }
            }

            if (craftTimer == 0.0f) {
                int remaining = addToInventory(
                    inventory, INVENTORY_SIZE, craftQueue.front().result, 1
                );
                if (remaining == 0) {
                    craftQueue.pop();
                    if (!craftQueue.empty()) {
                        craftTimer = craftQueue.front().duration;
                    }
                }
            }
        }

        // Проверяем место после крафта: готовый заказ мог занять свободный слот.
        Rectangle punchButton = {185, 375, 170, 30};
        bool hasBlank =
            puncherSlot >= 0 &&
            inventory[puncherSlot].type == ItemType::BlankCard &&
            inventory[puncherSlot].amount > 0;
        int punchedCardSlot = -1;

        if (puncherOpen && hasBlank) {
            if (inventory[puncherSlot].amount == 1) {
                punchedCardSlot = puncherSlot;
            }
            else {
                for (int i = 0; i < INVENTORY_SIZE; i++) {
                    if (inventory[i].amount == 0) {
                        punchedCardSlot = i;
                        break;
                    }
                }
            }
        }

        bool canPunch =
            puncherOpen && hasBlank && validPattern && punchedCardSlot != -1;

        if (canPunch &&
            IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
            CheckCollisionPointRec(mouse, punchButton)) {
            inventory[puncherSlot].amount--;
            if (inventory[puncherSlot].amount == 0) {
                inventory[puncherSlot] = {};
            }
            inventory[punchedCardSlot] = {
                ItemType::PunchedCard, 1, command, argument
            };
            puncherOpen = false;
            puncherSlot = -1;
        }

        // Плавка по установленной карте RUN.
        for (int row = 0; row < ROWS; row++) {
            for (int col = 0; col < COLS; col++) {
                if (grid[row][col] != TileType::Furnace) {
                    continue;
                }
                FurnaceStorage& furnace = furnaces[row][col];
                if (furnace.reader.amount == 0) {
                    furnace.status = "Insert card";
                    continue;
                }
                if (furnace.reader.command != 1) {
                    furnace.status = furnace.reader.command == 2
                        ? "INVALID BACK" : "UNSUPPORTED COMMAND";
                    continue;
                }
                if (furnace.completedActions >= furnace.reader.argument) {
                    furnace.status = "Done";
                    continue;
                }
                if (furnace.output.amount >= GetMaxStack(ItemType::IronPlate)) {
                    furnace.status = "OUTPUT FULL";
                    continue;
                }
                if (!furnace.smelting) {
                    if (furnace.energy == 0) {
                        if (furnace.fuel.amount == 0) {
                            furnace.status = "FUEL EXHAUSTED";
                            continue;
                        }
                        furnace.fuel.amount--;
                        if (furnace.fuel.amount == 0) {
                            furnace.fuel = {};
                        }
                        furnace.energy = 3;
                    }
                    if (furnace.ore.amount == 0) {
                        furnace.status = "NO ORE";
                        continue;
                    }
                    furnace.ore.amount--;
                    if (furnace.ore.amount == 0) {
                        furnace.ore = {};
                    }
                    furnace.energy--;
                    furnace.smelting = true;
                    furnace.smeltTimer = 3.0f;
                }

                furnace.status = "Smelting";
                furnace.smeltTimer -= GetFrameTime();
                if (furnace.smeltTimer <= 0.0f) {
                    furnace.output.type = ItemType::IronPlate;
                    furnace.output.amount++;
                    furnace.smelting = false;
                    furnace.smeltTimer = 0.0f;
                    furnace.completedActions++;
                    if (furnace.completedActions >= furnace.reader.argument) {
                        furnace.status = "Done";
                    }
                }
            }
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
                else if (tile == TileType::Drill) {
                    color = PURPLE;
                }
                else if (tile == TileType::IronOre) {
                    color = BROWN;
                }
                else if (tile == TileType::Coal) {
                    color = BLACK;
                }
                else if (tile == TileType::Boulder) {
                    color = DARKGRAY;
                }
                else if (tile == TileType::Tree) {
                    color = DARKGREEN;
                }
                else if (tile == TileType::Furnace) {
                    color = MAROON;
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
                
                if (tile == TileType::IronOre ||
                    tile == TileType::Coal ||
                    tile == TileType::Boulder ||
                    tile == TileType::Tree) {

                    int maxDurability = 3;
                    
                    if (tile == TileType::Tree) {
                        maxDurability = 4;
                    }
                    
                    if (tile == TileType::Boulder) {
                        maxDurability = 5;
                    }

                    if (tile == TileType::Coal) {
                        maxDurability = 2;
                    }

                    // Текущая прочность
                    int hp = durability[row][col];

                    // Не допускаем отрицательной ширины
                    if (hp < 0) {
                        hp = 0;
                    }

                    // Ширина заполненной полоски
                    int barWidth = 16 * hp / maxDurability;

                    // Координаты полоски
                    int barX = col * TILE_SIZE + 2;
                    int barY = row * TILE_SIZE - 6;

                    // Фон полоски
                    DrawRectangle(barX, barY, 16, 4, DARKGRAY);

                    // Оставшаяся прочность
                    DrawRectangle(barX, barY, barWidth, 4, GREEN);
                }

            }
        }
        
        for (int x = 0; x < 800; x += TILE_SIZE) {
            DrawLine(x, 0, x, 450, LIGHTGRAY);
        }

        for (int y = 0; y < 450; y += TILE_SIZE) {
            DrawLine(0, y, 800, y, LIGHTGRAY);
        }

        if (dismantleX != -1 && dismantleTimer > 0.0f) {
            int barX = dismantleX * TILE_SIZE;
            int barY = dismantleY * TILE_SIZE - 8;

            // У верхнего края рисуем полоску под печью.
            if (barY < 0) {
                barY = dismantleY * TILE_SIZE + TILE_SIZE + 2;
            }

            float progress = dismantleTimer / DISMANTLE_TIME;

            DrawRectangle(barX, barY, TILE_SIZE, 5, DARKGRAY);
            DrawRectangle(
                barX, barY,
                (int)(TILE_SIZE * progress), 5,
                ORANGE
            );
        }
        
        DrawText(TextFormat("Score: %i", score), 10, 10, 20, BLACK);
        
        int ironCount = 0;

        for (int i = 0; i < INVENTORY_SIZE; i++) {
            if (inventory[i].type == ItemType::IronOre) {
                ironCount += inventory[i].amount;
            }
        }

        DrawText(
            TextFormat("Iron: %i", ironCount),
            10, 35, 20, DARKBROWN
        );
        
        DrawRectangle(x, y, 40, 40, BLUE);
        
        // Анимация удара
        if (hitAnimationTimer > 0.0f) {
            DrawCircleLines(
                (int)hitPosition.x,
                (int)hitPosition.y,
                14,
                ORANGE
            );
        }
        
        DrawRectangleRec(wall, RED);
        if (!coinCollected) {
            DrawCircle(coinX, coinY, 15, GOLD);
        }
        
        // ==========================================
        // RENDER — Предпросмотр строительства
        // ==========================================

        if (insideMap && !inventoryOpen &&
            (placingFurnace || placingDrill || selectedTile != TileType::Empty)) {

            Color previewColor = GRAY;

            if (placingFurnace) {
                previewColor = MAROON;
            }
            else if (placingDrill) {
                previewColor = PURPLE;
            }
            else if (selectedTile == TileType::Gear) {
                previewColor = ORANGE;
            }
            else if (selectedTile == TileType::Generator) {
                previewColor = GREEN;
            }

            // Запрет строительства обозначаем красным ДО рисования.
            if (!canBuild) {
                previewColor = RED;
            }

            DrawRectangle(
                cellX * TILE_SIZE,
                cellY * TILE_SIZE,
                TILE_SIZE,
                TILE_SIZE,
                Fade(previewColor, 0.4f)
            );

            DrawRectangleLines(
                cellX * TILE_SIZE,
                cellY * TILE_SIZE,
                TILE_SIZE,
                TILE_SIZE,
                previewColor
            );
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
        else if (selectedTile == TileType::Drill) {
            selectedName = "Drill";
        }
        else if (selectedTile == TileType::Empty) {
            selectedName = "Mining";
        }

        if (placingFurnace) {
            selectedName = "Furnace";
        }
        else if (placingDrill) {
            selectedName = "Drill";
        }

        // Рисуем интерфейс поверх игрового мира
        DrawRectangle(10, 400, 240, 40, DARKGRAY);

        DrawText(
            TextFormat("Selected: %s", selectedName),
            20, 410, 20, WHITE
        );
        
        if (inventoryOpen) {
            DrawRectangle(200, 60, 400, 330, DARKGRAY);

            DrawText("Inventory", 220, 75, 30, WHITE);

            if (openedFurnaceX != -1 && openedFurnaceY != -1) {
                DrawRectangle(10, 60, 180, 350, DARKGRAY);
                DrawText("Furnace", 25, 75, 24, WHITE);

                const char* labels[] = {
                    "1. Iron Ore",
                    "2. Coal",
                    "3. Plates"
                };

                for (int i = 0; i < 3; i++) {
                    int slotY = 130 + i * 85;

                    DrawText(labels[i], 25, slotY - 20, 16, WHITE);
                    Rectangle slot = GetFurnaceSlotRect(i);
                    DrawRectangleRec(slot, GRAY);
                    DrawRectangleLinesEx(slot, 1, WHITE);

                    FurnaceStorage& furnace =
                        furnaces[openedFurnaceY][openedFurnaceX];
                    InventorySlot item = {};
                    if (i == 0) {
                        item = furnace.ore;
                    }
                    else if (i == 1) {
                        item = furnace.fuel;
                    }
                    else {
                        item = furnace.output;
                    }

                    if (item.amount > 0) {
                        Color color = item.type == ItemType::Coal ? BLACK : BROWN;
                        if (item.type == ItemType::IronPlate) {
                            DrawRectangle((int)slot.x + 12, (int)slot.y + 18, 36, 14, LIGHTGRAY);
                        }
                        else {
                            DrawCircle(
                                (int)slot.x + 30, (int)slot.y + 25, 15, color
                            );
                        }
                        DrawText(
                            TextFormat("%i", item.amount),
                            (int)slot.x + 30, (int)slot.y + 40, 16, WHITE
                        );
                    }
                }
            }
            
            if (openedFurnaceX != -1 && openedFurnaceY != -1) {
                Rectangle readerRect = GetFurnaceReaderRect();
                DrawRectangleRec(readerRect, GRAY);
                DrawRectangleLinesEx(readerRect, 1, WHITE);

                const InventorySlot& card =
                    furnaces[openedFurnaceY][openedFurnaceX].reader;
                const FurnaceStorage& furnace =
                    furnaces[openedFurnaceY][openedFurnaceX];
                DrawRectangle(10, 425, 590, 25, DARKGRAY);
                DrawText(furnace.status, 20, 430, 16, WHITE);
                if (card.amount > 0 && card.command == 1) {
                    DrawText(
                        TextFormat("%i/%i", furnace.completedActions, card.argument),
                        105, 235, 18, WHITE
                    );
                }
                if (furnace.smelting) {
                    float progress = 1.0f - furnace.smeltTimer / 3.0f;
                    if (progress < 0.0f) progress = 0.0f;
                    if (progress > 1.0f) progress = 1.0f;
                    DrawRectangle(100, 215, 75, 10, GRAY);
                    DrawRectangle(100, 215, (int)(75 * progress), 10, GREEN);
                    DrawRectangleLines(100, 215, 75, 10, WHITE);
                }
                if (card.amount == 0) {
                    DrawText("Insert card", 35, 375, 16, LIGHTGRAY);
                }
                else {
                    const char* name = "UNKNOWN";
                    if (card.command == 1) name = "RUN";
                    else if (card.command == 2) name = "BACK";
                    else if (card.command == 3) name = "REVERSE";
                    else if (card.command == 4) name = "SLOT";

                    const char* label = card.command == 3
                        ? name : TextFormat("%s %i", name, card.argument);
                    DrawText(label, 35, 375, 18, GOLD);
                }
            }

            // Панель крафта справа от инвентаря.
            DrawRectangle(610, 60, 185, 375, DARKGRAY);
            DrawText("Crafting", 620, 75, 22, WHITE);
            DrawText("Furnace", 620, 105, 20, WHITE);
            DrawRectangleRec(craftButton, stoneCount >= 3 ? GREEN : GRAY);
            DrawText("Craft Furnace", 625, 152, 17, WHITE);
            DrawText(
                TextFormat("Stone: %i/3", stoneCount),
                620, 127, 16, stoneCount >= 3 ? GREEN : RED
            );

            DrawText("Blank Card", 620, 185, 20, WHITE);
            DrawRectangleRec(cardButton, woodCount >= 1 ? GREEN : GRAY);
            DrawText("Craft Card", 625, 232, 17, WHITE);
            DrawText(
                TextFormat("Wood: %i/1", woodCount),
                620, 207, 16, woodCount >= 1 ? GREEN : RED
            );

            DrawText("Drill", 620, 265, 20, WHITE);
            DrawText(
                TextFormat("Plates: %i/3", plateCount),
                620, 287, 16, plateCount >= 3 ? GREEN : RED
            );
            DrawRectangleRec(drillButton, plateCount >= 3 ? GREEN : GRAY);
            DrawText("Craft Drill", 625, 312, 17, WHITE);

            if (!craftQueue.empty()) {
                const char* craftingName = "Furnace";
                if (craftQueue.front().result == ItemType::BlankCard) {
                    craftingName = "Blank Card";
                }
                else if (craftQueue.front().result == ItemType::Drill) {
                    craftingName = "Drill";
                }
                DrawText(craftingName, 620, 345, 18, WHITE);

                if (craftTimer > 0.0f) {
                    DrawText(
                        TextFormat("Time: %.1f", craftTimer),
                        620, 365, 18, WHITE
                    );
                }
                else {
                    DrawText("INVENTORY FULL", 620, 365, 16, RED);
                }

                float progress = 1.0f - craftTimer / craftQueue.front().duration;
                if (progress < 0.0f) progress = 0.0f;
                if (progress > 1.0f) progress = 1.0f;
                DrawRectangle(620, 390, 165, 14, GRAY);
                DrawRectangle(620, 390, (int)(165 * progress), 14, GREEN);
                DrawRectangleLines(620, 390, 165, 14, WHITE);
            }

            int waitingOrders = craftQueue.empty() ? 0 : (int)craftQueue.size() - 1;
            DrawText(
                TextFormat("Queue: %i", waitingOrders),
                620, 410, 18, WHITE
            );

            for (int i = 0; i < INVENTORY_SIZE; i++) {
                
                Rectangle slot = GetInventorySlotRect(i);

                int slotX = (int)slot.x;
                int slotY = (int)slot.y;

                // Рисуем слот
                DrawRectangle(slotX, slotY, 60, 60, GRAY);
                DrawRectangleLines(slotX, slotY, 60, 60, WHITE);
                
                if (splitSource == i) {
                    DrawRectangleLinesEx(
                        {(float)slotX, (float)slotY, 60, 60},
                        3,
                        SKYBLUE
                    );
                }
                
                if (selectedSlot == i) {
                    DrawRectangleLinesEx(
                        {(float)slotX, (float)slotY, 60, 60},
                        3,
                        YELLOW
                    );
                }

                // Если слот не пустой
                if (inventory[i].amount > 0 && i != selectedSlot) {

                    // Показываем в инвентаре
                    if (inventory[i].type == ItemType::IronOre) {
                        DrawCircle(slotX + 30, slotY + 25, 15, BROWN);
                    }
                    else if (inventory[i].type == ItemType::Coal) {
                        DrawCircle(slotX + 30, slotY + 25, 15, BLACK);
                    }
                    else if (inventory[i].type == ItemType::Stone) {
                        DrawCircle(slotX + 30, slotY + 25, 15, DARKGRAY);
                    }
                    else if (inventory[i].type == ItemType::Wood) {
                        DrawCircle(slotX + 30, slotY + 25, 15, DARKBROWN);
                    }
                    else if (inventory[i].type == ItemType::IronPlate) {
                        DrawRectangle(slotX + 12, slotY + 18, 36, 14, LIGHTGRAY);
                    }
                    else if (inventory[i].type == ItemType::Drill) {
                        DrawRectangle(slotX + 15, slotY + 10, 30, 30, PURPLE);
                    }
                    else if (inventory[i].type == ItemType::Furnace) {
                        DrawRectangle(
                            slotX + 15,
                            slotY + 10,
                            30, 30,
                            ORANGE
                        );
                    }

                    if (inventory[i].type == ItemType::BlankCard) {
                        DrawRectangle(slotX + 12, slotY + 15, 36, 22, BEIGE);
                    }
                    else if (inventory[i].type == ItemType::PunchedCard) {
                        DrawPunchedCard(
                            slotX + 12, slotY + 15,
                            inventory[i].command, inventory[i].argument
                        );
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
            
            
            if (splitSource != -1) {

                // Получаем предмет из выбранного слота
                InventorySlot item = inventory[splitSource];

                // Вычисляем половину
                int half = item.amount / 2;

                // Выбираем цвет предмета
                Color itemColor = BROWN;

                if (item.type == ItemType::Coal) {
                    itemColor = BLACK;
                }
                else if (item.type == ItemType::Stone) {
                    itemColor = DARKGRAY;
                }
                else if (item.type == ItemType::Wood) {
                    itemColor = DARKBROWN;
                }
                else if (item.type == ItemType::Furnace) {
                    itemColor = ORANGE;
                }
                else if (item.type == ItemType::Drill) {
                    itemColor = PURPLE;
                }

                // Рисуем полупрозрачный предмет возле мыши.
                if (item.type == ItemType::IronPlate) {
                    DrawRectangle(
                        (int)mouse.x - 18, (int)mouse.y - 7,
                        36, 14, Fade(LIGHTGRAY, 0.7f)
                    );
                }
                else if (item.type == ItemType::BlankCard) {
                    DrawRectangle(
                        (int)mouse.x - 18, (int)mouse.y - 11,
                        36, 22, Fade(BEIGE, 0.7f)
                    );
                }
                else DrawCircle(
                    (int)mouse.x,
                    (int)mouse.y,
                    15,
                    Fade(itemColor, 0.7f)
                );

                // Показываем количество переносимых предметов
                DrawText(
                    TextFormat("%i", half),
                    (int)mouse.x + 12,
                    (int)mouse.y + 12,
                    16,
                    WHITE
                );
            }
            
            
            // Предмет, который мы перетаскиваем
            if (selectedSlot != -1) {

                InventorySlot item = inventory[selectedSlot];

                Color itemColor = BROWN;

                if (item.type == ItemType::Coal) {
                    itemColor = BLACK;
                }
                else if (item.type == ItemType::Stone) {
                    itemColor = DARKGRAY;
                }
                else if (item.type == ItemType::Wood) {
                    itemColor = DARKBROWN;
                }
                else if (item.type == ItemType::Furnace) {
                    itemColor = ORANGE;
                }
                else if (item.type == ItemType::Drill) {
                    itemColor = PURPLE;
                }

                // Рисуем выбранный предмет под курсором.
                if (item.type == ItemType::IronPlate) {
                    DrawRectangle(
                        (int)mouse.x - 18, (int)mouse.y - 7, 36, 14, LIGHTGRAY
                    );
                }
                else if (item.type == ItemType::PunchedCard) {
                    DrawPunchedCard(
                        (int)mouse.x - 18, (int)mouse.y - 11,
                        item.command, item.argument
                    );
                }
                else if (item.type == ItemType::BlankCard) {
                    DrawRectangle(
                        (int)mouse.x - 18, (int)mouse.y - 11, 36, 22, BEIGE
                    );
                }
                else DrawCircle(
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
            
            for (int i = 0; i < INVENTORY_SIZE; i++) {

                Rectangle slot = GetInventorySlotRect(i);

                // Мышь находится над слотом с предметом?
                if (CheckCollisionPointRec(mouse, slot) &&
                    inventory[i].amount > 0) {

                    const char* itemName = "Unknown";

                    if (inventory[i].type == ItemType::IronOre) {
                        itemName = "Iron Ore";
                    }
                    else if (inventory[i].type == ItemType::Coal) {
                        itemName = "Coal";
                    }
                    else if (inventory[i].type == ItemType::Stone) {
                        itemName = "Stone";
                    }
                    else if (inventory[i].type == ItemType::Wood) {
                        itemName = "Wood";
                    }
                    else if (inventory[i].type == ItemType::IronPlate) {
                        itemName = "Iron Plate";
                    }
                    else if (inventory[i].type == ItemType::Furnace) {
                        itemName = "Furnace";
                    }
                    else if (inventory[i].type == ItemType::Drill) {
                        itemName = "Drill";
                    }

                    else if (inventory[i].type == ItemType::BlankCard) {
                        itemName = "Blank Card";
                    }
                    else if (inventory[i].type == ItemType::PunchedCard) {
                        const char* name = "UNKNOWN";
                        if (inventory[i].command == 1) name = "RUN";
                        else if (inventory[i].command == 2) name = "BACK";
                        else if (inventory[i].command == 3) name = "REVERSE";
                        else if (inventory[i].command == 4) name = "SLOT";

                        itemName = inventory[i].command == 3
                            ? name : TextFormat("%s %i", name, inventory[i].argument);
                    }

                    // Размер подсказки
                    int tooltipWidth = MeasureText(itemName, 18) + 20;

                    if (tooltipWidth < 160) {
                        tooltipWidth = 160;
                    }

                    int tooltipX = (int)mouse.x + 15;
                    int tooltipY = (int)mouse.y + 15;
                    
                    // Проверяем правую границу экрана
                    if (tooltipX + tooltipWidth > GetScreenWidth()) {
                        tooltipX = (int)mouse.x - tooltipWidth - 15;
                    }

                    // Проверяем нижнюю границу экрана
                    if (tooltipY + 55 > GetScreenHeight()) {
                        tooltipY = (int)mouse.y - 55 - 15;
                    }

                    // Защита от выхода за левую и верхнюю границы
                    if (tooltipX < 0) {
                        tooltipX = 0;
                    }

                    if (tooltipY < 0) {
                        tooltipY = 0;
                    }

                    // Фон подсказки
                    DrawRectangle(
                        tooltipX,
                        tooltipY,
                        tooltipWidth,
                        55,
                        DARKGRAY
                    );

                    // Рамка
                    DrawRectangleLines(
                        tooltipX,
                        tooltipY,
                        tooltipWidth,
                        55,
                        GOLD
                    );

                    // Название предмета
                    DrawText(
                        itemName,
                        tooltipX + 10,
                        tooltipY + 7,
                        18,
                        WHITE
                    );
                    
                    int maxStack = GetMaxStack(inventory[i].type);

                    DrawText(
                        TextFormat("Max stack: %i", maxStack),
                        tooltipX + 10,
                        tooltipY + 30,
                        16,
                        LIGHTGRAY
                    );

                    break;
                }
            }
            
        }
        
        if (inventoryFullTimer > 0.0f) {
            DrawText("INVENTORY FULL", 10, 65, 20, RED);
        }

        if (inventoryOpen && !puncherOpen &&
            selectedSlot != -1 &&
            inventory[selectedSlot].type == ItemType::BlankCard) {
            DrawText("P: Open puncher", 220, 375, 18, GOLD);
        }

        if (puncherOpen) {
            DrawRectangle(160, 35, 480, 405, DARKGRAY);
            DrawText("Card Puncher", 185, 50, 24, WHITE);
            DrawText("RUN 0001   BACK 0010", 185, 85, 18, LIGHTGRAY);
            DrawText("REVERSE 0011   SLOT 0100", 185, 110, 18, LIGHTGRAY);

            for (int row = 0; row < 2; row++) {
                for (int i = 0; i < 4; i++) {
                    Rectangle hole = GetPunchHoleRect(row, i);
                    bool marked = row == 0 ? commandHoles[i] : argumentHoles[i];
                    DrawRectangleRec(hole, BEIGE);
                    if (marked) {
                        DrawCircle(
                            (int)hole.x + 20, (int)hole.y + 20, 12, BLACK
                        );
                    }
                    if (row == 1) {
                        DrawText(
                            TextFormat("%i", 1 << i),
                            (int)hole.x + 15, (int)hole.y + 45, 18, WHITE
                        );
                    }
                }
            }

            const char* preview = command == 3
                ? commandName
                : TextFormat("%s %i", commandName, argument);
            DrawText(preview, 185, 325, 24, validPattern ? GREEN : RED);
            const char* error = "";
            if (!hasBlank) {
                error = "NO BLANK CARD";
            }
            else if (command < 1 || command > 4) {
                error = "UNKNOWN COMMAND";
            }
            else if (command == 3 && argument != 0) {
                error = "REVERSE NEEDS NO ARGUMENT";
            }
            else if (command != 3 && argument == 0) {
                error = "ARGUMENT REQUIRED";
            }
            else if (punchedCardSlot == -1) {
                error = "INVENTORY FULL";
            }

            DrawText(error, 185, 352, 16, RED);
            DrawRectangleRec(punchButton, canPunch ? GREEN : GRAY);
            DrawText("Punch", 235, 381, 18, WHITE);
            DrawText("E: Close", 185, 415, 18, LIGHTGRAY);
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
