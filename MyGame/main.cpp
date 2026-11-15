
#include <raylib.h>

// Типы предметов
enum class ItemType {
    Empty,
    IronOre,
    Coal,
    Stone,
    Wood,
    Furnace
};

// Максимальный размер стака
int GetMaxStack(ItemType type) {
    if (type == ItemType::IronOre) {
        return 100;
    }
    else if (type == ItemType::Coal) {
        return 200;
    }
    else if (type == ItemType::Furnace) {
        return 10;
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
    bool furnaceCrafting = false;
    float furnaceCraftTimer = 0.0f;
    bool furnaceWaiting = false;
    int furnaceQueue = 0;
    
    TileType grid[ROWS][COLS] = {};
    int durability[ROWS][COLS] = {};
    int resourceAmount[ROWS][COLS] = {};

    struct FurnaceStorage {
        InventorySlot ore = {};
        InventorySlot fuel = {};
    };

    FurnaceStorage furnaces[ROWS][COLS] = {};
    
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
        }

        // Клавиша 2 — шестерёнка
        if (IsKeyPressed(KEY_TWO)) {
            selectedTile = TileType::Gear;
            placingFurnace = false;
        }

        // Клавиша 3 — генератор
        if (IsKeyPressed(KEY_THREE)) {
            selectedTile = TileType::Generator;
            placingFurnace = false;
        }

        if (IsKeyPressed(KEY_FOUR)) {
            selectedTile = TileType::Drill;
            placingFurnace = false;
        }
        
        if (IsKeyPressed(KEY_ZERO)) {
            selectedTile = TileType::Empty;
            placingFurnace = false;
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
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
                canBuild &&
                !inventoryOpen &&
                !placingFurnace &&
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

                        break;
                    }
                }
            }

            // Пока ПКМ удаляет только тестовые постройки.
            // Разбор печи с возвратом в инвентарь добавим отдельно.
            if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) && !inventoryOpen) {
                TileType tile = grid[cellY][cellX];
                if (tile == TileType::Wall ||
                    tile == TileType::Gear ||
                    tile == TileType::Generator ||
                    tile == TileType::Drill) {
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
        // Разбор печи
        // ==========================================

        bool canDismantle = false;

        if (insideMap &&
            !inventoryOpen &&
            IsMouseButtonDown(MOUSE_BUTTON_RIGHT) &&
            grid[cellY][cellX] == TileType::Furnace) {

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
            // При переходе на другую печь начинаем заново.
            if (dismantleX != cellX || dismantleY != cellY) {
                dismantleX = cellX;
                dismantleY = cellY;
                dismantleTimer = 0.0f;
            }

            dismantleTimer += GetFrameTime();

            if (dismantleTimer >= DISMANTLE_TIME) {
                dismantleTimer = DISMANTLE_TIME;

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
                    ItemType::IronOre, furnace.ore.amount
                );
                int coalLeft = addToInventory(
                    candidate, INVENTORY_SIZE,
                    ItemType::Coal, furnace.fuel.amount
                );

                if (furnaceLeft == 0 && oreLeft == 0 && coalLeft == 0) {
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
            !placingFurnace &&
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
        
        if (IsKeyPressed(KEY_E)) {
            inventoryOpen = !inventoryOpen;
            
            if (!inventoryOpen) {
                splitSource = -1;
            }
        }
        
        // Закрываем окно печи вместе с инвентарём.
        if (!inventoryOpen) {
            openedFurnaceX = -1;
            openedFurnaceY = -1;
        }

        // Открываем печь отдельным кликом.
        if (!inventoryOpen &&
            !placingFurnace &&
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
        if (inventoryOpen) {

            // Нажали ЛКМ — выбираем предмет
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {

                splitSource = -1;
                selectedSlot = -1;

                for (int i = 0; i < INVENTORY_SIZE; i++) {

                    Rectangle slot = GetInventorySlotRect(i);

                    if (CheckCollisionPointRec(mouse, slot) &&
                        inventory[i].amount > 0) {

                        selectedSlot = i;
                        placingFurnace = false;
                        break;
                    }
                }
            }

            // Выгрузка предметов из печи по клику.
            if (openedFurnaceX != -1 &&
                openedFurnaceY != -1 &&
                selectedSlot == -1 &&
                (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) ||
                 IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))) {
                FurnaceStorage& furnace =
                    furnaces[openedFurnaceY][openedFurnaceX];

                InventorySlot* source = nullptr;
                if (CheckCollisionPointRec(mouse, GetFurnaceSlotRect(0))) {
                    source = &furnace.ore;
                }
                else if (CheckCollisionPointRec(mouse, GetFurnaceSlotRect(1))) {
                    source = &furnace.fuel;
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

            // Отпустили ЛКМ — перемещаем предмет
            if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
                selectedSlot != -1) {

                if (openedFurnaceX != -1 && openedFurnaceY != -1) {
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
                        int freeSpace = GetMaxStack(source.type) - target->amount;
                        int moveCount = source.amount;
                        if (moveCount > freeSpace) {
                            moveCount = freeSpace;
                        }

                        if (moveCount > 0) {
                            target->type = source.type;
                            target->amount += moveCount;
                            source.amount -= moveCount;
                            if (source.amount == 0) {
                                source.type = ItemType::Empty;
                            }
                        }
                    }
                }

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
                        else if (inventory[i].type == ItemType::Furnace &&
                                 inventory[i].amount > 0) {

                            placingFurnace = true;
                        }

                        break;
                    }
                }

                selectedSlot = -1;
            }
            
            // ПКМ при перетаскивании кладёт один предмет в печь.
            if (inventoryOpen &&
                openedFurnaceX != -1 &&
                openedFurnaceY != -1 &&
                selectedSlot != -1 &&
                IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {

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

                if (target != nullptr &&
                    source.amount > 0 &&
                    target->amount < GetMaxStack(source.type)) {

                    target->type = source.type;
                    target->amount++;
                    source.amount--;

                    if (source.amount == 0) {
                        source.type = ItemType::Empty;
                        selectedSlot = -1;
                    }
                }
            }

            if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) &&
                selectedSlot == -1) {
                
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
        
        // Считаем камни во всех слотах
        int stoneCount = 0;

        for (int i = 0; i < INVENTORY_SIZE; i++) {
            if (inventory[i].type == ItemType::Stone) {
                stoneCount += inventory[i].amount;
            }
        }

        // Кнопка крафта
        Rectangle craftButton = {615, 140, 170, 40};

        // Можно ли создать печь?
        bool canCraftFurnace = stoneCount >= 3;

        // Нажатие кнопки
        if (inventoryOpen &&
            canCraftFurnace &&
            IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
            CheckCollisionPointRec(mouse, craftButton)) {

            int remaining = 3;

            // Забираем 3 камня из инвентаря
            for (int i = 0; i < INVENTORY_SIZE; i++) {

                if (inventory[i].type != ItemType::Stone) {
                    continue;
                }

                int take = inventory[i].amount;

                if (take > remaining) {
                    take = remaining;
                }

                inventory[i].amount -= take;
                remaining -= take;

                if (inventory[i].amount == 0) {
                    inventory[i].type = ItemType::Empty;
                }

                if (remaining == 0) {
                    break;
                }
            }

            if (!furnaceCrafting && !furnaceWaiting &&
                furnaceQueue == 0) {

                // Если ничего не изготавливается,
                // начинаем сразу
                furnaceCrafting = true;
                furnaceCraftTimer = 2.0f;
            }
            else {

                // Иначе добавляем заказ в очередь
                furnaceQueue++;
            }
        }
        
        // Обновляем время крафта
        if (furnaceCrafting) {

            furnaceCraftTimer -= GetFrameTime();

            if (furnaceCraftTimer <= 0.0f) {
                furnaceCraftTimer = 0.0f;
                furnaceCrafting = false;
                furnaceWaiting = true;
            }
        }
        
        if (furnaceWaiting) {

            // Сначала ищем неполную стопку печей
            for (int i = 0; i < INVENTORY_SIZE; i++) {

                if (inventory[i].type == ItemType::Furnace &&
                    inventory[i].amount < 10) {

                    inventory[i].amount++;
                    furnaceWaiting = false;
                    break;
                }
            }

            // Если подходящей стопки нет, ищем пустой слот
            if (furnaceWaiting) {

                for (int i = 0; i < INVENTORY_SIZE; i++) {

                    if (inventory[i].amount == 0) {

                        inventory[i].type = ItemType::Furnace;
                        inventory[i].amount = 1;

                        furnaceWaiting = false;
                        break;
                    }
                }
            }
        }
        
        // Если предыдущая печь получена,
        // запускаем следующий заказ
        if (!furnaceCrafting &&
            !furnaceWaiting &&
            furnaceQueue > 0) {

            furnaceQueue--;

            furnaceCrafting = true;
            furnaceCraftTimer = 2.0f;
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
            (placingFurnace || selectedTile != TileType::Empty)) {

            Color previewColor = GRAY;

            if (placingFurnace) {
                previewColor = MAROON;
            }
            else if (selectedTile == TileType::Gear) {
                previewColor = ORANGE;
            }
            else if (selectedTile == TileType::Generator) {
                previewColor = GREEN;
            }
            else if (selectedTile == TileType::Drill) {
                previewColor = PURPLE;
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
                DrawRectangle(10, 60, 180, 330, DARKGRAY);
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

                    if (item.amount > 0) {
                        Color color = item.type == ItemType::Coal ? BLACK : BROWN;
                        DrawCircle(
                            (int)slot.x + 30, (int)slot.y + 25, 15, color
                        );
                        DrawText(
                            TextFormat("%i", item.amount),
                            (int)slot.x + 30, (int)slot.y + 40, 16, WHITE
                        );
                    }
                }
            }
            
            // Панель крафта справа от инвентаря
            DrawRectangle(610, 60, 185, 255, DARKGRAY);

            DrawText("Crafting", 620, 75, 22, WHITE);

            DrawText("Furnace", 620, 110, 20, WHITE);

            DrawText(
                TextFormat("Stone: %i/3", stoneCount),
                620, 195, 18,
                stoneCount >= 3 ? GREEN : RED
            );

            // Кнопка создания
            DrawRectangleRec(
                craftButton,
                canCraftFurnace ? GREEN : GRAY
            );

            DrawText("Craft Furnace", 625, 150, 17, WHITE);

            // Состояние крафта
            if (furnaceCrafting) {

                DrawText(
                    TextFormat("Time: %.1f", furnaceCraftTimer),
                    620, 225, 18, WHITE
                );

                // Вычисляем прогресс от 0 до 1
                float progress = 1.0f - furnaceCraftTimer / 2.0f;

                if (progress < 0.0f) progress = 0.0f;
                if (progress > 1.0f) progress = 1.0f;

                // Фон полоски
                DrawRectangle(620, 260, 165, 14, GRAY);

                // Заполненная часть
                DrawRectangle(
                    620, 260,
                    (int)(165 * progress),
                    14,
                    GREEN
                );

                // Рамка
                DrawRectangleLines(620, 260, 165, 14, WHITE);
            }
            else if (furnaceWaiting) {

                DrawText(
                    "INVENTORY FULL",
                    620, 225, 16, RED
                );
            }
            
            DrawText(
                TextFormat("Queue: %i", furnaceQueue),
                620, 285, 18, WHITE
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
                    else if (inventory[i].type == ItemType::Furnace) {
                        DrawRectangle(
                            slotX + 15,
                            slotY + 10,
                            30, 30,
                            ORANGE
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

                // Рисуем полупрозрачный предмет возле мыши
                DrawCircle(
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
                    else if (inventory[i].type == ItemType::Furnace) {
                        itemName = "Furnace";
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

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
