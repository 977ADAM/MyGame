
#include <raylib.h>
#include <queue>
#include <vector>

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
    Drill,
    Roller
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

void DrawRollerItem(int x, int y) {
    DrawRectangle(x, y, 36, 22, DARKBROWN);
    for (int i = 0; i < 3; i++) {
        DrawRectangle(x + 3 + i * 11, y + 3, 8, 16, LIGHTGRAY);
    }
}

enum class RollerSide {
    Left,
    Right,
    Up,
    Down
};

struct RollerCell {
    int x;
    int y;
};

struct RollerEnd {
    RollerCell cell;
    RollerSide side;
};

struct RollerStep {
    RollerCell cell;
    RollerSide exitSide;
};

struct RollerDirectionChoice {
    RollerEnd start;
    Rectangle hitbox;
};

struct RollerLine {
    std::vector<RollerCell> cells;
    std::vector<RollerEnd> ends;
};

bool HasRollerCell(
    const std::vector<RollerCell>& cells,
    RollerCell target
) {
    for (const RollerCell& cell : cells) {
        if (cell.x == target.x && cell.y == target.y) {
            return true;
        }
    }
    return false;
}

bool HasRollerSide(int shape, RollerSide side) {
    // Порядок форм: ─ │ └ ┌ ┐ ┘
    switch (side) {
        case RollerSide::Left:
            return shape == 0 || shape == 4 || shape == 5;
        case RollerSide::Right:
            return shape == 0 || shape == 2 || shape == 3;
        case RollerSide::Up:
            return shape == 1 || shape == 2 || shape == 5;
        case RollerSide::Down:
            return shape == 1 || shape == 3 || shape == 4;
    }

    return false;
}

RollerSide OppositeSide(RollerSide side) {
    switch (side) {
        case RollerSide::Left:  return RollerSide::Right;
        case RollerSide::Right: return RollerSide::Left;
        case RollerSide::Up:    return RollerSide::Down;
        case RollerSide::Down:  return RollerSide::Up;
    }
    return RollerSide::Left;
}

RollerCell NeighborCell(RollerCell cell, RollerSide side) {
    switch (side) {
        case RollerSide::Left:  cell.x--; break;
        case RollerSide::Right: cell.x++; break;
        case RollerSide::Up:    cell.y--; break;
        case RollerSide::Down:  cell.y++; break;
    }
    return cell;
}

RollerSide OtherRollerSide(int shape, RollerSide side) {
    const RollerSide sides[] = {
        RollerSide::Left, RollerSide::Right,
        RollerSide::Up, RollerSide::Down
    };
    for (RollerSide candidate : sides) {
        if (candidate != side && HasRollerSide(shape, candidate)) {
            return candidate;
        }
    }
    return side;
}

void DrawRollerArrow(RollerCell cell, RollerSide side, int tileSize) {
    Vector2 center = {
        cell.x * tileSize + tileSize / 2.0f,
        cell.y * tileSize + tileSize / 2.0f
    };
    RollerCell neighbor = NeighborCell(cell, side);
    Vector2 direction = {
        (float)(neighbor.x - cell.x),
        (float)(neighbor.y - cell.y)
    };
    Vector2 tip = {
        center.x + direction.x * 6,
        center.y + direction.y * 6
    };
    Vector2 tail = {
        center.x - direction.x * 4,
        center.y - direction.y * 4
    };
    Vector2 wingA = {
        tip.x - direction.x * 3 - direction.y * 3,
        tip.y - direction.y * 3 + direction.x * 3
    };
    Vector2 wingB = {
        tip.x - direction.x * 3 + direction.y * 3,
        tip.y - direction.y * 3 - direction.x * 3
    };
    DrawLineEx(tail, tip, 2, BLUE);
    DrawLineEx(tip, wingA, 2, BLUE);
    DrawLineEx(tip, wingB, 2, BLUE);
}

void DrawRollerTile(int x, int y, int shape, Color color) {
    bool left  = HasRollerSide(shape, RollerSide::Left);
    bool right = HasRollerSide(shape, RollerSide::Right);
    bool up    = HasRollerSide(shape, RollerSide::Up);
    bool down  = HasRollerSide(shape, RollerSide::Down);

    Color base = Fade(color, 0.35f);

    DrawRectangle(x + 6, y + 6, 8, 8, base);

    if (left) {
        DrawRectangle(x, y + 6, 6, 8, base);
        DrawRectangle(x + 2, y + 6, 2, 8, color);
    }
    if (right) {
        DrawRectangle(x + 14, y + 6, 6, 8, base);
        DrawRectangle(x + 16, y + 6, 2, 8, color);
    }
    if (up) {
        DrawRectangle(x + 6, y, 8, 6, base);
        DrawRectangle(x + 6, y + 2, 8, 2, color);
    }
    if (down) {
        DrawRectangle(x + 6, y + 14, 8, 6, base);
        DrawRectangle(x + 6, y + 16, 8, 2, color);
    }
}

struct DeveloperItem {
    ItemType type;
    const char* name;
};

const DeveloperItem developerItems[] = {
    {ItemType::IronOre,   "Iron Ore"},
    {ItemType::Coal,      "Coal"},
    {ItemType::Stone,     "Stone"},
    {ItemType::Wood,      "Wood"},
    {ItemType::IronPlate, "Iron Plate"},
    {ItemType::Furnace,   "Furnace"},
    {ItemType::Drill,     "Drill"},
    {ItemType::Roller,    "Roller"},
    {ItemType::BlankCard, "Blank Card"}
};

Rectangle GetDeveloperButtonRect(int index) {
    return {
        (float)(145 + index % 3 * 175),
        (float)(125 + index / 3 * 75),
        160,
        60
    };
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
        Furnace,
        Roller
    };
    
    struct CardInstruction {
        int command = 0;
        int argument = 0;
    };

    struct InventorySlot {
        ItemType type = ItemType::Empty;
        int amount = 0;
        std::vector<CardInstruction> cards;
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
    int previewSlot = -1;
    int previewOffset = 0;
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
    int rollerShapes[ROWS][COLS] = {};
    bool rollerControllers[ROWS][COLS] = {};
    InventorySlot rollerFuel[ROWS][COLS] = {};
    InventorySlot rollerReaders[ROWS][COLS] = {};
    InventorySlot rollerCargo[ROWS][COLS] = {};
    struct RollerProgram {
        int energy = 0;
        int completedActions = 0;
        int cardIndex = 0;
        float commandTimer = 0.0f;
        const char* status = "Insert card";
        std::vector<RollerCell> lineCells;
        bool lineBroken = false;
    };
    RollerProgram rollerPrograms[ROWS][COLS] = {};
    bool rollerDirectionSet[ROWS][COLS] = {};
    RollerSide rollerEntrySides[ROWS][COLS] = {};
    int rollerEndSlots[ROWS][COLS][4] = {};
    auto isRollerConnected = [&](int col, int row, RollerSide side) {
        if (col < 0 || col >= COLS || row < 0 || row >= ROWS) {
            return false;
        }

        if (grid[row][col] != TileType::Roller ||
            !HasRollerSide(rollerShapes[row][col], side)) {
            return false;
        }

        int neighborX = col;
        int neighborY = row;
        RollerSide opposite = RollerSide::Right;

        switch (side) {
            case RollerSide::Left:
                neighborX--;
                opposite = RollerSide::Right;
                break;
            case RollerSide::Right:
                neighborX++;
                opposite = RollerSide::Left;
                break;
            case RollerSide::Up:
                neighborY--;
                opposite = RollerSide::Down;
                break;
            case RollerSide::Down:
                neighborY++;
                opposite = RollerSide::Up;
                break;
        }

        if (neighborX < 0 || neighborX >= COLS ||
            neighborY < 0 || neighborY >= ROWS) {
            return false;
        }

        return grid[neighborY][neighborX] == TileType::Roller &&
            HasRollerSide(rollerShapes[neighborY][neighborX], opposite);
    };

    auto getRollerLine = [&](int startX, int startY) {
        RollerLine line;

        if (startX < 0 || startX >= COLS ||
            startY < 0 || startY >= ROWS ||
            grid[startY][startX] != TileType::Roller) {
            return line;
        }

        const RollerSide sides[] = {
            RollerSide::Left, RollerSide::Right,
            RollerSide::Up, RollerSide::Down
        };
        const int dx[] = {-1, 1, 0, 0};
        const int dy[] = {0, 0, -1, 1};

        bool visited[ROWS][COLS] = {};
        visited[startY][startX] = true;
        line.cells.push_back({startX, startY});

        for (int i = 0; i < (int)line.cells.size(); i++) {
            // Копируем клетку: push_back может переместить массив.
            RollerCell cell = line.cells[i];

            for (int direction = 0; direction < 4; direction++) {
                RollerSide side = sides[direction];

                if (!HasRollerSide(
                        rollerShapes[cell.y][cell.x], side)) {
                    continue;
                }

                if (!isRollerConnected(cell.x, cell.y, side)) {
                    line.ends.push_back({cell, side});
                    continue;
                }

                int nextX = cell.x + dx[direction];
                int nextY = cell.y + dy[direction];

                if (!visited[nextY][nextX]) {
                    visited[nextY][nextX] = true;
                    line.cells.push_back({nextX, nextY});
                }
            }
        }

        return line;
    };

    auto getDirectedLine = [&](RollerEnd start) {
        std::vector<RollerStep> route;
        bool visited[ROWS][COLS] = {};
        RollerCell cell = start.cell;
        RollerSide entrySide = start.side;
        const RollerSide sides[] = {
            RollerSide::Left, RollerSide::Right,
            RollerSide::Up, RollerSide::Down
        };

        while (cell.x >= 0 && cell.x < COLS &&
               cell.y >= 0 && cell.y < ROWS &&
               grid[cell.y][cell.x] == TileType::Roller &&
               !visited[cell.y][cell.x]) {
            int shape = rollerShapes[cell.y][cell.x];
            if (!HasRollerSide(shape, entrySide)) {
                break;
            }
            visited[cell.y][cell.x] = true;
            RollerSide exitSide = entrySide;
            for (RollerSide side : sides) {
                if (side != entrySide && HasRollerSide(shape, side)) {
                    exitSide = side;
                    break;
                }
            }
            route.push_back({cell, exitSide});
            if (!isRollerConnected(cell.x, cell.y, exitSide)) {
                break;
            }
            cell = NeighborCell(cell, exitSide);
            entrySide = OppositeSide(exitSide);
        }
        return route;
    };

    auto getDirectionChoices = [&](int controllerX, int controllerY) {
        std::vector<RollerDirectionChoice> choices;
        RollerLine line = getRollerLine(controllerX, controllerY);
        if (line.cells.empty()) {
            return choices;
        }
        if (!line.ends.empty()) {
            for (const RollerEnd& end : line.ends) {
                Rectangle hitbox = {
                    (float)(end.cell.x * TILE_SIZE),
                    (float)(end.cell.y * TILE_SIZE),
                    (float)TILE_SIZE, (float)TILE_SIZE
                };
                if (line.cells.size() == 1) {
                    RollerCell neighbor = NeighborCell(end.cell, end.side);
                    float markerX = hitbox.x + TILE_SIZE / 2.0f +
                        (neighbor.x - end.cell.x) * 7;
                    float markerY = hitbox.y + TILE_SIZE / 2.0f +
                        (neighbor.y - end.cell.y) * 7;
                    hitbox = {markerX - 3, markerY - 3, 6, 6};
                }
                choices.push_back({end, hitbox});
            }
        }
        else {
            const RollerSide sides[] = {
                RollerSide::Left, RollerSide::Right,
                RollerSide::Up, RollerSide::Down
            };
            RollerCell controller = {controllerX, controllerY};
            int shape = rollerShapes[controllerY][controllerX];
            for (RollerSide exitSide : sides) {
                if (!isRollerConnected(controllerX, controllerY, exitSide)) {
                    continue;
                }
                RollerCell neighbor = NeighborCell(controller, exitSide);
                RollerSide entrySide = OtherRollerSide(shape, exitSide);
                choices.push_back({
                    {controller, entrySide},
                    {
                        (float)(neighbor.x * TILE_SIZE),
                        (float)(neighbor.y * TILE_SIZE),
                        (float)TILE_SIZE, (float)TILE_SIZE
                    }
                });
            }
        }
        return choices;
    };

    auto getControllerRoute = [&](int col, int row) {
        RollerSide entrySide = rollerEntrySides[row][col];
        int shape = rollerShapes[row][col];
        auto backward = getDirectedLine({
            {col, row}, OtherRollerSide(shape, entrySide)
        });
        if (backward.empty()) {
            return std::vector<RollerStep>{};
        }
        const RollerStep& last = backward.back();
        if (!isRollerConnected(last.cell.x, last.cell.y, last.exitSide)) {
            return getDirectedLine({last.cell, last.exitSide});
        }
        return getDirectedLine({{col, row}, entrySide});
    };

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
        int cardIndex = 0;
        float commandTimer = 0.0f;
        bool smelting = false;
        float smeltTimer = 0.0f;
        const char* status = "Insert card";
    };

    FurnaceStorage furnaces[ROWS][COLS] = {};

    struct DrillStorage {
        InventorySlot fuel = {};
        InventorySlot reader = {};
        InventorySlot output = {};
        int energy = 0;
        int completedActions = 0;
        int cardIndex = 0;
        float commandTimer = 0.0f;
        float hitTimer = 0.0f;
        const char* status = "Insert card";
    };

    DrillStorage drills[ROWS][COLS] = {};

    auto getBuildingSlotCount = [&](RollerCell cell) {
        if (cell.x < 0 || cell.x >= COLS ||
            cell.y < 0 || cell.y >= ROWS) {
            return 0;
        }

        if (grid[cell.y][cell.x] == TileType::Furnace) {
            return 3;
        }
        if (grid[cell.y][cell.x] == TileType::Drill) {
            return 2;
        }

        return 0;
    };

    auto getBuildingSlot = [&](RollerCell cell, int slot) -> InventorySlot* {
        if (cell.x < 0 || cell.x >= COLS ||
            cell.y < 0 || cell.y >= ROWS) {
            return nullptr;
        }

        if (grid[cell.y][cell.x] == TileType::Furnace) {
            FurnaceStorage& furnace = furnaces[cell.y][cell.x];
            if (slot == 1) return &furnace.ore;
            if (slot == 2) return &furnace.fuel;
            if (slot == 3) return &furnace.output;
        }
        else if (grid[cell.y][cell.x] == TileType::Drill) {
            DrillStorage& drill = drills[cell.y][cell.x];
            if (slot == 1) return &drill.fuel;
            if (slot == 2) return &drill.output;
        }

        return nullptr;
    };

    auto getAcceptedItem = [&](RollerCell cell, int slot) {
        if (grid[cell.y][cell.x] == TileType::Furnace) {
            if (slot == 1) return ItemType::IronOre;
            if (slot == 2) return ItemType::Coal;
            if (slot == 3) return ItemType::IronPlate;
        }
        else if (grid[cell.y][cell.x] == TileType::Drill) {
            if (slot == 1) return ItemType::Coal;
            if (slot == 2) {
                return drillDeposits[cell.y][cell.x] == TileType::IronOre
                    ? ItemType::IronOre : ItemType::Coal;
            }
        }

        return ItemType::Empty;
    };

    auto unloadRollerCargo = [&](RollerEnd end) {
        InventorySlot& cargo = rollerCargo[end.cell.y][end.cell.x];
        if (cargo.amount == 0) {
            return false;
        }

        int slotNumber = rollerEndSlots[end.cell.y][end.cell.x]
                                        [static_cast<int>(end.side)];
        RollerCell building = NeighborCell(end.cell, end.side);
        InventorySlot* target = getBuildingSlot(building, slotNumber);

        if (target == nullptr ||
            getAcceptedItem(building, slotNumber) != cargo.type ||
            (target->amount > 0 && target->type != cargo.type) ||
            target->amount >= GetMaxStack(cargo.type)) {
            return false;
        }

        target->type = cargo.type;
        target->amount++;
        cargo = {};
        return true;
    };

    auto loadRollerCargo = [&](RollerEnd end) {
        InventorySlot& cargo = rollerCargo[end.cell.y][end.cell.x];
        if (cargo.amount > 0) {
            return false;
        }

        int slotNumber = rollerEndSlots[end.cell.y][end.cell.x]
                                        [static_cast<int>(end.side)];
        RollerCell building = NeighborCell(end.cell, end.side);
        InventorySlot* source = getBuildingSlot(building, slotNumber);

        if (source == nullptr || source->amount == 0) {
            return false;
        }

        cargo.type = source->type;
        cargo.amount = 1;
        source->amount--;
        if (source->amount == 0) {
            *source = {};
        }
        return true;
    };

    auto moveLoopCargo = [&](const std::vector<RollerStep>& route) {
        int count = (int)route.size();
        if (count == 0) {
            return false;
        }

        std::vector<InventorySlot> previous;
        previous.reserve(count);

        bool hasEmpty = false;
        for (const RollerStep& step : route) {
            const InventorySlot& cargo =
                rollerCargo[step.cell.y][step.cell.x];
            previous.push_back(cargo);
            if (cargo.amount == 0) {
                hasEmpty = true;
            }
        }

        if (!hasEmpty) {
            return false;
        }

        std::vector<InventorySlot> next = previous;
        bool moved = false;

        for (int empty = 0; empty < count; empty++) {
            if (previous[empty].amount > 0) {
                continue;
            }

            int to = empty;
            int from = (to + count - 1) % count;

            while (previous[from].amount > 0) {
                next[to] = previous[from];
                next[from] = {};
                moved = true;

                to = from;
                from = (from + count - 1) % count;
            }
        }

        if (moved) {
            for (int i = 0; i < count; i++) {
                RollerCell cell = route[i].cell;
                rollerCargo[cell.y][cell.x] = next[i];
            }
        }

        return moved;
    };
    
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
    grid[6][7] = TileType::Tree;
    grid[6][8] = TileType::Tree;
    grid[7][6] = TileType::Tree;
    grid[12][10] = TileType::Tree;
    grid[13][10] = TileType::Tree;
    grid[13][11] = TileType::Tree;
    
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
    bool placingRoller = false;
    int selectedRollerShape = 0;
    
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
    int openedDrillX = -1;
    int openedDrillY = -1;
    int openedRollerX = -1;
    int openedRollerY = -1;

    int choosingControllerX = -1;
    int choosingControllerY = -1;
    bool developerPanelOpen = false;

    while (!WindowShouldClose()) {

        Rectangle wall = {500, 150, 100, 50};
        Vector2 mouse = GetMousePosition();
        int cellX = (int)mouse.x / TILE_SIZE;
        int cellY = (int)mouse.y / TILE_SIZE;
        bool insideMap =
            cellX >= 0 && cellX < COLS &&
            cellY >= 0 && cellY < ROWS;
        bool canBuild = false;
        int command = 0;
        int argument = 0;
        const char* commandName = "UNKNOWN";
        int stoneCount = 0;
        int woodCount = 0;
        int plateCount = 0;
        Rectangle craftButton = {615, 132, 170, 24};
        Rectangle cardButton = {615, 192, 170, 24};
        Rectangle drillButton = {615, 252, 170, 24};
        Rectangle rollerButton = {615, 312, 170, 24};
        Rectangle punchButton = {185, 375, 170, 30};
        Rectangle controllerButton = {25, 130, 150, 40};
        Rectangle rollerFuelRect = {25, 215, 60, 60};
        Rectangle rollerReaderRect = {25, 330, 150, 35};
        int punchedCardSlot = -1;
        bool validPattern = false;
        bool hasBlank = false;
        bool canPunch = false;

        // Сохраняем блокировку до конца кадра отмены или выбора.
        bool directionInputBlocked = choosingControllerX != -1;
        bool directionSelectionStartedThisFrame = false;
        bool panelToggled = IsKeyPressed(KEY_F1);
        if (panelToggled) {
            developerPanelOpen = !developerPanelOpen;
            if (developerPanelOpen) {
                choosingControllerX = -1;
                choosingControllerY = -1;
                inventoryOpen = false;
                puncherOpen = false;
                puncherSlot = -1;
                openedFurnaceX = -1;
                openedFurnaceY = -1;
                openedDrillX = -1;
                openedDrillY = -1;
                openedRollerX = -1;
                openedRollerY = -1;
                selectedSlot = -1;
                splitSource = -1;
                previewSlot = -1;
                previewOffset = 0;
            }
        }

        if (inventoryFullTimer > 0.0f) {
            inventoryFullTimer -= GetFrameTime();
        }

        if (developerPanelOpen && !panelToggled &&
            IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            for (int i = 0; i < 9; i++) {
                if (!CheckCollisionPointRec(mouse, GetDeveloperButtonRect(i))) {
                    continue;
                }
                int remaining = addToInventory(
                    inventory, INVENTORY_SIZE, developerItems[i].type, 10
                );
                if (remaining > 0) {
                    inventoryFullTimer = 1.0f;
                }
                break;
            }
        }

        // На паузе рисуем мир, но не обновляем его состояние.
        if (!developerPanelOpen && !panelToggled) {
            bool furnacePlacedThisFrame = false;
            bool drillPlacedThisFrame = false;
            bool rollerPlacedThisFrame = false;
            bool rollerOpenedThisFrame = false;

            int oldX = x;

            if (IsKeyDown(KEY_RIGHT)) x += speed;
            if (IsKeyDown(KEY_LEFT)) x -= speed;

            if (x < 0) x = 0;
            if (x > 760) x = 760;

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
            if (!directionInputBlocked && IsKeyPressed(KEY_ONE)) {
                selectedTile = TileType::Wall;
                placingFurnace = false;
                placingDrill = false;
                placingRoller = false;
            }

            // Клавиша 2 — шестерёнка
            if (!directionInputBlocked && IsKeyPressed(KEY_TWO)) {
                selectedTile = TileType::Gear;
                placingFurnace = false;
                placingDrill = false;
                placingRoller = false;
            }

            // Клавиша 3 — генератор
            if (!directionInputBlocked && IsKeyPressed(KEY_THREE)) {
                selectedTile = TileType::Generator;
                placingFurnace = false;
                placingDrill = false;
                placingRoller = false;
            }

            if (!directionInputBlocked && IsKeyPressed(KEY_ZERO)) {
                selectedTile = TileType::Empty;
                placingFurnace = false;
                placingDrill = false;
                placingRoller = false;
            }

            // ==========================================
            // BUILDING — Проверка возможности строительства
            // ==========================================

            if (placingRoller && !inventoryOpen && !directionInputBlocked && IsKeyPressed(KEY_R)) {
                selectedRollerShape = (selectedRollerShape + 1) % 6;
            }

            // Проверяем возможность строительства в выбранной клетке.
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
                    !inventoryOpen && !directionInputBlocked &&
                    !placingFurnace && !placingDrill && !placingRoller &&
                    selectedTile != TileType::Empty) {

                    grid[cellY][cellX] = selectedTile;
                }

                // Установка печи из инвентаря
                if (placingFurnace &&
                    !inventoryOpen && !directionInputBlocked &&
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
                            placingRoller = false;

                            break;
                        }
                    }
                }

                // Установка изготовленного бура поверх залежи.
                if (placingDrill &&
                    !inventoryOpen && !directionInputBlocked &&
                    canBuild &&
                    IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    for (int i = 0; i < INVENTORY_SIZE; i++) {
                        if (inventory[i].type != ItemType::Drill ||
                            inventory[i].amount == 0) {
                            continue;
                        }
                        drillDeposits[cellY][cellX] = grid[cellY][cellX];
                        grid[cellY][cellX] = TileType::Drill;
                        drillPlacedThisFrame = true;
                        inventory[i].amount--;
                        if (inventory[i].amount == 0) {
                            inventory[i] = {};
                        }
                        placingDrill = false;
                        placingRoller = false;
                        break;
                    }
                }

                if (placingRoller &&
                    !inventoryOpen && !directionInputBlocked &&
                    canBuild &&
                    IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    for (int i = 0; i < INVENTORY_SIZE; i++) {
                        if (inventory[i].type != ItemType::Roller ||
                            inventory[i].amount == 0) {
                            continue;
                        }
                        grid[cellY][cellX] = TileType::Roller;
                        rollerShapes[cellY][cellX] = selectedRollerShape;
                        rollerControllers[cellY][cellX] = false;
                        rollerDirectionSet[cellY][cellX] = false;
                        rollerPlacedThisFrame = true;
                        rollerFuel[cellY][cellX] = {};
                        rollerReaders[cellY][cellX] = {};
                        rollerCargo[cellY][cellX] = {};
                        rollerPrograms[cellY][cellX] = {};
                        for (int side = 0; side < 4; side++) {
                            rollerEndSlots[cellY][cellX][side] = 0;
                        }
                        inventory[i].amount--;
                        if (inventory[i].amount == 0) {
                            inventory[i] = {};
                        }
                        placingRoller = false;
                        break;
                    }
                }

                // Пока ПКМ удаляет только тестовые постройки.
                if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) && !inventoryOpen && !directionInputBlocked) {
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

            // ==========================================
            // Разбор печи или бура
            // ==========================================

            bool canDismantle = false;

            if (insideMap &&
                !inventoryOpen && !directionInputBlocked &&
                IsMouseButtonDown(MOUSE_BUTTON_RIGHT) &&
                (grid[cellY][cellX] == TileType::Furnace ||
                 grid[cellY][cellX] == TileType::Drill ||
                 grid[cellY][cellX] == TileType::Roller)) {

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

                    if (grid[cellY][cellX] == TileType::Roller) {
                        InventorySlot candidate[INVENTORY_SIZE];
                        for (int i = 0; i < INVENTORY_SIZE; i++) {
                            candidate[i] = inventory[i];
                        }
                        int rollerLeft = addToInventory(
                            candidate, INVENTORY_SIZE, ItemType::Roller, 1
                        );
                        int coalLeft = addToInventory(
                            candidate, INVENTORY_SIZE, ItemType::Coal,
                            rollerFuel[cellY][cellX].amount
                        );
                        int cargoLeft = addToInventory(
                            candidate, INVENTORY_SIZE,
                            rollerCargo[cellY][cellX].type,
                            rollerCargo[cellY][cellX].amount
                        );
                        const InventorySlot& reader = rollerReaders[cellY][cellX];
                        bool cardsFit = reader.amount == 0;
                        if (!cardsFit) {
                            for (int i = 0; i < INVENTORY_SIZE; i++) {
                                if (candidate[i].amount == 0) {
                                    candidate[i] = reader;
                                    cardsFit = true;
                                    break;
                                }
                            }
                        }
                        if (rollerLeft == 0 && coalLeft == 0 && cargoLeft == 0 && cardsFit) {
                            for (int i = 0; i < INVENTORY_SIZE; i++) {
                                inventory[i] = candidate[i];
                            }
                            rollerFuel[cellY][cellX] = {};
                            rollerReaders[cellY][cellX] = {};
                            rollerCargo[cellY][cellX] = {};
                            rollerPrograms[cellY][cellX] = {};
                            for (int side = 0; side < 4; side++) {
                                rollerEndSlots[cellY][cellX][side] = 0;
                            }
                            grid[cellY][cellX] = TileType::Empty;
                            rollerShapes[cellY][cellX] = 0;
                            rollerControllers[cellY][cellX] = false;
                            rollerDirectionSet[cellY][cellX] = false;
                            dismantleX = -1;
                            dismantleY = -1;
                            dismantleTimer = 0.0f;
                        }
                        else {
                            inventoryFullTimer = 1.0f;
                        }
                    }
                    else if (grid[cellY][cellX] == TileType::Drill) {
                        DrillStorage& drill = drills[cellY][cellX];

                        InventorySlot candidate[INVENTORY_SIZE];
                        for (int i = 0; i < INVENTORY_SIZE; i++) {
                            candidate[i] = inventory[i];
                        }

                        int drillLeft = addToInventory(
                            candidate, INVENTORY_SIZE, ItemType::Drill, 1
                        );
                        int coalLeft = addToInventory(
                            candidate, INVENTORY_SIZE, ItemType::Coal, drill.fuel.amount
                        );

                        int outputLeft = addToInventory(
                            candidate, INVENTORY_SIZE,
                            drill.output.type, drill.output.amount
                        );

                        bool cardFits = drill.reader.amount == 0;
                        if (!cardFits) {
                            for (int i = 0; i < INVENTORY_SIZE; i++) {
                                if (candidate[i].amount == 0) {
                                    candidate[i] = drill.reader;
                                    cardFits = true;
                                    break;
                                }
                            }
                        }

                        if (drillLeft == 0 && coalLeft == 0 &&
                            outputLeft == 0 && cardFits) {
                            for (int i = 0; i < INVENTORY_SIZE; i++) {
                                inventory[i] = candidate[i];
                            }

                            drill = {};
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
                !placingFurnace && !placingDrill && !placingRoller &&
                insideMap &&
                IsMouseButtonDown(MOUSE_BUTTON_LEFT) && !inventoryOpen && !directionInputBlocked && miningCooldown <= 0.0f) {

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
                if (directionInputBlocked) {
                    choosingControllerX = -1;
                    choosingControllerY = -1;
                }
                else if (puncherOpen) {
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

            for (int i = 0; i < 4; i++) {
                command = command * 2 + (commandHoles[i] ? 1 : 0);
                if (argumentHoles[i]) {
                    argument += 1 << i;
                }
            }

            if (command == 1) commandName = "RUN";
            else if (command == 2) commandName = "BACK";
            else if (command == 3) commandName = "REVERSE";
            else if (command == 4) commandName = "SLOT";

            validPattern =
                ((command == 1 || command == 2 || command == 4) && argument > 0) ||
                (command == 3 && argument == 0);

            // Закрываем окно постройки вместе с инвентарём.
            if (!inventoryOpen) {
                openedFurnaceX = -1;
                openedFurnaceY = -1;
                openedDrillX = -1;
                openedDrillY = -1;
                openedRollerX = -1;
                openedRollerY = -1;
            }

            // Открываем печь отдельным кликом.
            if (!inventoryOpen && !directionInputBlocked &&
                !placingFurnace && !placingDrill && !placingRoller &&
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

            // Открываем бур отдельным кликом в пределах досягаемости.
            if (!inventoryOpen && !directionInputBlocked &&
                !placingFurnace && !placingDrill && !placingRoller &&
                !drillPlacedThisFrame &&
                insideMap &&
                IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
                grid[cellY][cellX] == TileType::Drill) {

                Vector2 playerCenter = {
                    (float)x + 20,
                    (float)y + 20
                };

                Vector2 drillCenter = {
                    (float)(cellX * TILE_SIZE + TILE_SIZE / 2),
                    (float)(cellY * TILE_SIZE + TILE_SIZE / 2)
                };

                if (CheckCollisionPointCircle(
                    drillCenter, playerCenter, 60.0f
                )) {
                    openedDrillX = cellX;
                    openedDrillY = cellY;
                    inventoryOpen = true;
                }
            }

            // Обрабатываем кнопку до открытия окна, чтобы не нажать её
            // тем же кликом, которым игрок открыл ролик.
            if (inventoryOpen && !puncherOpen &&
                openedRollerX != -1 && openedRollerY != -1 &&
                IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
                CheckCollisionPointRec(mouse, controllerButton)) {
                if (!rollerDirectionSet[openedRollerY][openedRollerX]) {
                    rollerControllers[openedRollerY][openedRollerX] = true;
                    choosingControllerX = openedRollerX;
                    choosingControllerY = openedRollerY;
                    directionInputBlocked = true;
                    directionSelectionStartedThisFrame = true;
                    inventoryOpen = false;
                    selectedSlot = -1;
                    splitSource = -1;
                    openedRollerX = -1;
                    openedRollerY = -1;
                }
            }

            if (!inventoryOpen && !directionInputBlocked &&
                !placingFurnace && !placingDrill && !placingRoller &&
                !rollerPlacedThisFrame &&
                insideMap &&
                IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
                grid[cellY][cellX] == TileType::Roller) {
                Vector2 playerCenter = {
                    (float)x + 20,
                    (float)y + 20
                };
                Vector2 rollerCenter = {
                    (float)(cellX * TILE_SIZE + TILE_SIZE / 2),
                    (float)(cellY * TILE_SIZE + TILE_SIZE / 2)
                };
                if (CheckCollisionPointCircle(
                        rollerCenter, playerCenter, 60.0f)) {
                    openedRollerX = cellX;
                    openedRollerY = cellY;
                    inventoryOpen = true;
                    rollerOpenedThisFrame = true;
                }
            }

            // Перемещение предметов в инвентаре
            if (inventoryOpen && !puncherOpen) {
                bool leftPressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
                bool rightPressed = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
                if (selectedSlot != -1 &&
                    inventory[selectedSlot].type == ItemType::PunchedCard &&
                    inventory[selectedSlot].cards.size() > 1 &&
                    IsKeyPressed(KEY_X)) {

                    int freeSlot = -1;
                    for (int i = 0; i < INVENTORY_SIZE; i++) {
                        if (inventory[i].amount == 0) {
                            freeSlot = i;
                            break;
                        }
                    }

                    if (freeSlot != -1) {
                        InventorySlot& chain = inventory[selectedSlot];
                        inventory[freeSlot] = {
                            ItemType::PunchedCard, 1, {chain.cards.back()}
                        };
                        chain.cards.pop_back();
                    }
                    else {
                        inventoryFullTimer = 1.0f;
                    }
                }

                int selectedBeforeClick = selectedSlot;

                if (openedRollerX != -1 && openedRollerY != -1 &&
                    !rollerOpenedThisFrame &&
                    rollerControllers[openedRollerY][openedRollerX] &&
                    (leftPressed || rightPressed) &&
                    CheckCollisionPointRec(mouse, rollerFuelRect)) {
                    InventorySlot& fuel = rollerFuel[openedRollerY][openedRollerX];
                    if (selectedBeforeClick != -1) {
                        InventorySlot& source = inventory[selectedBeforeClick];
                        if (source.type == ItemType::Coal) {
                            int count = rightPressed ? 1 : source.amount;
                            int space = GetMaxStack(ItemType::Coal) - fuel.amount;
                            if (count > space) {
                                count = space;
                            }
                            if (count > 0) {
                                fuel.type = ItemType::Coal;
                                fuel.amount += count;
                                source.amount -= count;
                                if (source.amount == 0) {
                                    source = {};
                                    selectedSlot = -1;
                                }
                            }
                        }
                    }
                    else if (fuel.amount > 0) {
                        int count = rightPressed ? 1 : fuel.amount;
                        int remaining = addToInventory(
                            inventory, INVENTORY_SIZE, ItemType::Coal, count
                        );
                        fuel.amount -= count - remaining;
                        if (fuel.amount == 0) {
                            fuel = {};
                        }
                        if (remaining > 0) {
                            inventoryFullTimer = 1.0f;
                        }
                    }
                }

                if (openedRollerX != -1 && openedRollerY != -1 &&
                    !rollerOpenedThisFrame &&
                    rollerControllers[openedRollerY][openedRollerX] &&
                    leftPressed &&
                    CheckCollisionPointRec(mouse, rollerReaderRect)) {
                    InventorySlot& reader =
                        rollerReaders[openedRollerY][openedRollerX];
                    if (selectedBeforeClick != -1) {
                        InventorySlot& source = inventory[selectedBeforeClick];
                        if (source.type == ItemType::PunchedCard &&
                            reader.amount == 0) {
                            reader = source;
                            RollerProgram& program =
                                rollerPrograms[openedRollerY][openedRollerX];
                            program.completedActions = 0;
                            program.cardIndex = 0;
                            program.commandTimer = 0.0f;
                            program.status = "Insert card";
                            source = {};
                            selectedSlot = -1;
                        }
                    }
                    else if (reader.amount > 0) {
                        int freeSlot = -1;
                        for (int i = 0; i < INVENTORY_SIZE; i++) {
                            if (inventory[i].amount == 0) {
                                freeSlot = i;
                                break;
                            }
                        }
                        if (freeSlot != -1) {
                            inventory[freeSlot] = reader;
                            reader = {};
                        }
                        else {
                            inventoryFullTimer = 1.0f;
                        }
                    }
                }

                if (openedDrillX != -1 && openedDrillY != -1 &&
                    (leftPressed || rightPressed)) {

                    InventorySlot& fuel =
                        drills[openedDrillY][openedDrillX].fuel;
                    Rectangle fuelRect = {25, 130, 60, 60};

                    if (CheckCollisionPointRec(mouse, fuelRect)) {
                        if (selectedBeforeClick != -1) {
                            InventorySlot& source = inventory[selectedBeforeClick];

                            if (source.type == ItemType::Coal) {
                                int count = rightPressed ? 1 : source.amount;
                                int space = GetMaxStack(ItemType::Coal) - fuel.amount;

                                if (count > space) {
                                    count = space;
                                }

                                if (count > 0) {
                                    fuel.type = ItemType::Coal;
                                    fuel.amount += count;
                                    source.amount -= count;

                                    if (source.amount == 0) {
                                        source = {};
                                        selectedSlot = -1;
                                    }
                                }
                            }
                        }
                        else if (fuel.amount > 0) {
                            int count = rightPressed ? 1 : fuel.amount;
                            int remaining = addToInventory(
                                inventory, INVENTORY_SIZE, ItemType::Coal, count
                            );

                            fuel.amount -= count - remaining;

                            if (fuel.amount == 0) {
                                fuel = {};
                            }
                            if (remaining > 0) {
                                inventoryFullTimer = 1.0f;
                            }
                        }
                    }
                }

                if (openedDrillX != -1 && openedDrillY != -1 &&
                    selectedBeforeClick == -1 &&
                    (leftPressed || rightPressed)) {

                    Rectangle outputRect = {25, 215, 60, 60};
                    InventorySlot& output =
                        drills[openedDrillY][openedDrillX].output;

                    if (CheckCollisionPointRec(mouse, outputRect) &&
                        output.amount > 0) {
                        int count = rightPressed ? 1 : output.amount;
                        int remaining = addToInventory(
                            inventory, INVENTORY_SIZE, output.type, count
                        );

                        output.amount -= count - remaining;
                        if (output.amount == 0) {
                            output = {};
                        }
                        if (remaining > 0) {
                            inventoryFullTimer = 1.0f;
                        }
                    }
                }

                if (openedDrillX != -1 && openedDrillY != -1 &&
                    leftPressed) {

                    InventorySlot& card =
                        drills[openedDrillY][openedDrillX].reader;
                    Rectangle readerRect = {25, 365, 150, 35};

                    if (CheckCollisionPointRec(mouse, readerRect)) {
                        if (selectedBeforeClick != -1) {
                            InventorySlot& source = inventory[selectedBeforeClick];

                            if (source.type == ItemType::PunchedCard &&
                                card.amount == 0) {
                                card = source;
                                DrillStorage& drill = drills[openedDrillY][openedDrillX];
                                drill.completedActions = 0;
                                drill.cardIndex = 0;
                                drill.commandTimer = 0.0f;
                                source = {};
                                selectedSlot = -1;
                            }
                        }
                        else if (card.amount > 0) {
                            int freeSlot = -1;
                            for (int i = 0; i < INVENTORY_SIZE; i++) {
                                if (inventory[i].amount == 0) {
                                    freeSlot = i;
                                    break;
                                }
                            }

                            if (freeSlot != -1) {
                                inventory[freeSlot] = card;
                                card = {};
                            }
                            else {
                                inventoryFullTimer = 1.0f;
                            }
                        }
                    }
                }

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
                            placingRoller = inventory[i].type == ItemType::Roller;
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

                                bool joiningCards =
                                    (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) &&
                                    inventory[selectedSlot].type == ItemType::PunchedCard &&
                                    inventory[i].type == ItemType::PunchedCard;

                                if (joiningCards) {
                                    InventorySlot& source = inventory[selectedSlot];
                                    InventorySlot& target = inventory[i];

                                    // Сначала выбранная цепочка, затем та, по которой нажали.
                                    target.cards.insert(
                                        target.cards.begin(),
                                        source.cards.begin(),
                                        source.cards.end()
                                    );
                                    source = {};
                                }
                                // Одинаковые предметы объединяем.
                                else if (inventory[selectedSlot].type == inventory[i].type &&
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
                            placingRoller = false;
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
                        furnace.cardIndex = 0;
                        furnace.commandTimer = 0.0f;
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

            struct Ingredient {
                ItemType type;
                int amount;
            };
            std::vector<Ingredient> ingredients;
            CraftOrder order = {};

            if (inventoryOpen && !puncherOpen &&
                IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                if (CheckCollisionPointRec(mouse, craftButton) && stoneCount >= 3) {
                    ingredients = {{ItemType::Stone, 3}};
                    order = {ItemType::Furnace, 2.0f};
                }
                else if (CheckCollisionPointRec(mouse, cardButton) && woodCount >= 1) {
                    ingredients = {{ItemType::Wood, 1}};
                    order = {ItemType::BlankCard, 1.0f};
                }
                else if (CheckCollisionPointRec(mouse, drillButton) && plateCount >= 3) {
                    ingredients = {{ItemType::IronPlate, 3}};
                    order = {ItemType::Drill, 3.0f};
                }
                else if (CheckCollisionPointRec(mouse, rollerButton) &&
                         plateCount >= 1 && woodCount >= 1) {
                    ingredients = {{ItemType::IronPlate, 1}, {ItemType::Wood, 1}};
                    order = {ItemType::Roller, 2.0f};
                }
            }

            if (!ingredients.empty()) {
                for (const Ingredient& ingredient : ingredients) {
                    int required = ingredient.amount;
                    for (int i = 0; i < INVENTORY_SIZE && required > 0; i++) {
                        if (inventory[i].type != ingredient.type) {
                            continue;
                        }
                        int take = inventory[i].amount;
                        if (take > required) {
                            take = required;
                        }
                        inventory[i].amount -= take;
                        required -= take;
                        if (inventory[i].amount == 0) {
                            inventory[i] = {};
                            if (selectedSlot == i) selectedSlot = -1;
                            if (splitSource == i) splitSource = -1;
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
            hasBlank =
                puncherSlot >= 0 &&
                inventory[puncherSlot].type == ItemType::BlankCard &&
                inventory[puncherSlot].amount > 0;

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

            canPunch =
                puncherOpen && hasBlank && validPattern && punchedCardSlot != -1;

            if (canPunch &&
                IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
                CheckCollisionPointRec(mouse, punchButton)) {
                inventory[puncherSlot].amount--;
                if (inventory[puncherSlot].amount == 0) {
                    inventory[puncherSlot] = {};
                }
                inventory[punchedCardSlot] = {
                    ItemType::PunchedCard, 1, {{command, argument}}
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
                    if (furnace.cardIndex >= (int)furnace.reader.cards.size()) {
                        furnace.status = "Done";
                        continue;
                    }

                    const CardInstruction& instruction =
                        furnace.reader.cards[furnace.cardIndex];

                    if (instruction.command == 1 &&
                        furnace.output.amount >= GetMaxStack(ItemType::IronPlate)) {
                        furnace.status = "OUTPUT FULL";
                        continue;
                    }

                    bool continuingSmelt = instruction.command == 1 && furnace.smelting;
                    if (!continuingSmelt && furnace.energy == 0) {
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

                    if (instruction.command == 2) {
                        furnace.status = "Reading BACK";
                        furnace.commandTimer += GetFrameTime();
                        if (furnace.commandTimer < 0.2f) {
                            continue;
                        }
                        if (instruction.argument > furnace.cardIndex) {
                            furnace.status = "INVALID BACK";
                            continue;
                        }
                        furnace.cardIndex -= instruction.argument;
                        furnace.completedActions = 0;
                        furnace.commandTimer = 0.0f;
                        continue;
                    }

                    if (instruction.command != 1) {
                        furnace.status = "UNSUPPORTED COMMAND";
                        continue;
                    }

                    if (!furnace.smelting) {
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
                        if (furnace.completedActions >= instruction.argument) {
                            furnace.cardIndex++;
                            furnace.commandTimer = 0.0f;
                            if (furnace.cardIndex >= (int)furnace.reader.cards.size()) {
                                furnace.status = "Done";
                            }
                            else {
                                furnace.completedActions = 0;
                                furnace.status = "Next card";
                            }
                        }
                    }
                }
            }

            // Добыча по установленной карте RUN.
            for (int row = 0; row < ROWS; row++) {
                for (int col = 0; col < COLS; col++) {
                    if (grid[row][col] != TileType::Drill) {
                        continue;
                    }

                    DrillStorage& drill = drills[row][col];

                    if (drill.reader.amount == 0) {
                        drill.status = "Insert card";
                        continue;
                    }

                    if (drill.cardIndex >= (int)drill.reader.cards.size()) {
                        drill.status = "Done";
                        continue;
                    }

                    const CardInstruction& instruction =
                        drill.reader.cards[drill.cardIndex];

                    bool isIron = drillDeposits[row][col] == TileType::IronOre;
                    ItemType resource = isIron ? ItemType::IronOre : ItemType::Coal;

                    if (instruction.command == 1) {
                        if (resourceAmount[row][col] == 0) {
                            drill.status = "DEPOSIT DEPLETED";
                            continue;
                        }
                        if (drill.output.amount >= GetMaxStack(resource)) {
                            drill.status = "OUTPUT FULL";
                            continue;
                        }
                    }

                    if (drill.energy == 0) {
                        if (drill.fuel.amount == 0) {
                            drill.status = "FUEL EXHAUSTED";
                            continue;
                        }

                        drill.fuel.amount--;
                        if (drill.fuel.amount == 0) {
                            drill.fuel = {};
                        }
                        drill.energy = 3;
                    }

                    if (instruction.command == 2) {
                        drill.status = "Reading BACK";
                        drill.commandTimer += GetFrameTime();

                        if (drill.commandTimer < 0.2f) {
                            continue;
                        }

                        if (instruction.argument > drill.cardIndex) {
                            drill.status = "INVALID BACK";
                            continue;
                        }

                        drill.cardIndex -= instruction.argument;
                        drill.completedActions = 0;
                        drill.commandTimer = 0.0f;
                        continue;
                    }

                    if (instruction.command != 1) {
                        drill.status = "UNSUPPORTED COMMAND";
                        continue;
                    }

                    drill.status = "Mining";
                    drill.hitTimer += GetFrameTime();

                    if (drill.hitTimer < 0.4f) {
                        continue;
                    }

                    drill.hitTimer = 0.0f;
                    drill.energy--;
                    drill.completedActions++;
                    durability[row][col]--;

                    if (durability[row][col] <= 0) {
                        drill.output.type = resource;
                        drill.output.amount++;
                        resourceAmount[row][col]--;

                        if (resourceAmount[row][col] > 0) {
                            durability[row][col] = isIron ? 3 : 2;
                        }
                    }

                    if (drill.completedActions >= instruction.argument) {
                        drill.cardIndex++;
                        drill.commandTimer = 0.0f;

                        if (drill.cardIndex >= (int)drill.reader.cards.size()) {
                            drill.status = "Done";
                        }
                        else {
                            drill.completedActions = 0;
                            drill.status = "Next card";
                        }
                    }
                    else if (resourceAmount[row][col] == 0) {
                        drill.status = "DEPOSIT DEPLETED";
                    }
                    else if (drill.energy == 0 && drill.fuel.amount == 0) {
                        drill.status = "FUEL EXHAUSTED";
                    }
                }
            }

            // Исполнение программы роликовой линии.
            for (int row = 0; row < ROWS; row++) {
                for (int col = 0; col < COLS; col++) {
                    if (grid[row][col] != TileType::Roller ||
                        !rollerControllers[row][col]) {
                        continue;
                    }
                    RollerProgram& program = rollerPrograms[row][col];
                    const InventorySlot& reader = rollerReaders[row][col];
                    InventorySlot& fuel = rollerFuel[row][col];
                    if (!rollerDirectionSet[row][col]) {
                        program.status = "Set direction";
                        continue;
                    }
                    RollerLine line = getRollerLine(col, row);
                    int controllerCount = 0;
                    for (const RollerCell& cell : line.cells) {
                        if (rollerControllers[cell.y][cell.x]) {
                            controllerCount++;
                        }
                    }
                    if (program.lineCells.empty()) {
                        program.lineCells = line.cells;
                    }

                    bool missingCell = false;
                    for (const RollerCell& expected : program.lineCells) {
                        if (!HasRollerCell(line.cells, expected)) {
                            missingCell = true;
                            break;
                        }
                    }

                    if (missingCell) {
                        program.lineBroken = true;
                    }

                    if (program.lineBroken) {
                        if (missingCell) {
                            program.status = "LINE BROKEN";
                            continue;
                        }

                        program.lineBroken = false;
                        program.lineCells = line.cells;
                    }

                    if (controllerCount == 1) {
                        program.lineCells = line.cells;
                    }

                    if (controllerCount > 1) {
                        program.status = "CONFLICT";
                        continue;
                    }
                    if (reader.amount == 0) {
                        program.status = "Insert card";
                        continue;
                    }
                    if (program.cardIndex >= (int)reader.cards.size()) {
                        program.status = "Done";
                        continue;
                    }
                    const CardInstruction& instruction =
                        reader.cards[program.cardIndex];
                    if (instruction.command == 1) {
                        bool isLoop = line.ends.empty();

                        auto route = getControllerRoute(col, row);
                        if (route.empty()) {
                            program.status = "LINE BROKEN";
                            continue;
                        }

                        RollerEnd sourceEnd = {};
                        RollerEnd targetEnd = {};

                        if (!isLoop) {
                            RollerCell firstCell = route.front().cell;
                            sourceEnd = {
                                firstCell,
                                OtherRollerSide(
                                    rollerShapes[firstCell.y][firstCell.x],
                                    route.front().exitSide
                                )
                            };
                            targetEnd = {
                                route.back().cell,
                                route.back().exitSide
                            };
                        }

                        if (!isLoop) {
                            bool connectionError = false;
                            const RollerEnd ends[] = {sourceEnd, targetEnd};

                            for (const RollerEnd& end : ends) {
                                int slotNumber =
                                    rollerEndSlots[end.cell.y][end.cell.x]
                                                  [static_cast<int>(end.side)];
                                if (slotNumber == 0) {
                                    program.status = "SLOT NOT SET";
                                    connectionError = true;
                                    break;
                                }

                                RollerCell building = NeighborCell(
                                    end.cell, end.side
                                );
                                int slotCount = getBuildingSlotCount(building);
                                if (slotCount == 0) {
                                    program.status = "NO BUILDING CONNECTED";
                                    connectionError = true;
                                    break;
                                }
                                if (slotNumber > slotCount) {
                                    program.status = "INVALID SLOT";
                                    connectionError = true;
                                    break;
                                }
                            }

                            if (connectionError) {
                                continue;
                            }
                        }

                        if (program.energy == 0) {
                            if (fuel.amount == 0) {
                                program.status = "FUEL EXHAUSTED";
                                continue;
                            }
                            fuel.amount--;
                            if (fuel.amount == 0) {
                                fuel = {};
                            }
                            program.energy = 20;
                        }

                        program.status = "Running";
                        program.commandTimer += GetFrameTime();
                        if (program.commandTimer < 0.5f) {
                            continue;
                        }

                        bool moved = isLoop && moveLoopCargo(route);

                        if (!isLoop) {
                            if (unloadRollerCargo(targetEnd)) {
                                moved = true;
                            }

                            for (int i = (int)route.size() - 1; i > 0; i--) {
                                RollerCell from = route[i - 1].cell;
                                RollerCell to = route[i].cell;

                                if (rollerCargo[to.y][to.x].amount == 0 &&
                                    rollerCargo[from.y][from.x].amount > 0) {
                                    rollerCargo[to.y][to.x] =
                                        rollerCargo[from.y][from.x];
                                    rollerCargo[from.y][from.x] = {};
                                    moved = true;
                                }
                            }

                            if (loadRollerCargo(sourceEnd)) {
                                moved = true;
                            }
                        }

                        if (!moved) {
                            program.status = "WAITING FOR MOVEMENT";
                            continue;
                        }

                        program.energy--;
                        program.completedActions++;
                        program.commandTimer = 0.0f;

                        if (program.completedActions >= instruction.argument) {
                            program.cardIndex++;
                            program.completedActions = 0;
                            program.status =
                                program.cardIndex >= (int)reader.cards.size()
                                ? "Done" : "Next card";
                        }
                        continue;
                    }
                    if (program.energy == 0) {
                        if (fuel.amount == 0) {
                            program.status = "FUEL EXHAUSTED";
                            continue;
                        }
                        fuel.amount--;
                        if (fuel.amount == 0) {
                            fuel = {};
                        }
                        program.energy = 20;
                    }
                    if (instruction.command == 2) {
                        program.status = "Reading BACK";
                        program.commandTimer += GetFrameTime();

                        if (program.commandTimer < 0.2f) {
                            continue;
                        }
                        if (instruction.argument > program.cardIndex) {
                            program.status = "INVALID BACK";
                            continue;
                        }

                        program.cardIndex -= instruction.argument;
                        program.completedActions = 0;
                        program.commandTimer = 0.0f;
                        continue;
                    }
                    if (instruction.command == 4) {
                        program.status = "Reading SLOT";
                        program.commandTimer += GetFrameTime();

                        if (program.commandTimer < 0.2f) {
                            continue;
                        }
                        if (line.ends.empty()) {
                            program.status = "NO LINE END";
                            continue;
                        }

                        auto route = getControllerRoute(col, row);
                        if (route.empty()) {
                            program.status = "LINE BROKEN";
                            continue;
                        }

                        RollerEnd activeEnd = {
                            route.back().cell,
                            route.back().exitSide
                        };
                        RollerCell building = NeighborCell(
                            activeEnd.cell, activeEnd.side
                        );
                        int slotCount = getBuildingSlotCount(building);

                        if (slotCount == 0) {
                            program.status = "NO BUILDING CONNECTED";
                            continue;
                        }
                        if (instruction.argument > slotCount) {
                            program.status = "INVALID SLOT";
                            continue;
                        }

                        rollerEndSlots[activeEnd.cell.y][activeEnd.cell.x]
                                      [static_cast<int>(activeEnd.side)] =
                            instruction.argument;

                        program.commandTimer = 0.0f;
                        program.cardIndex++;
                        program.status =
                            program.cardIndex >= (int)reader.cards.size()
                            ? "Done" : "Next card";
                        continue;
                    }
                    program.status = "Reading REVERSE";
                    program.commandTimer += GetFrameTime();
                    if (program.commandTimer < 0.2f) {
                        continue;
                    }
                    rollerEntrySides[row][col] = OtherRollerSide(
                        rollerShapes[row][col], rollerEntrySides[row][col]
                    );
                    program.commandTimer = 0.0f;
                    program.cardIndex++;
                    program.status =
                        program.cardIndex >= (int)reader.cards.size()
                        ? "Done" : "Next card";
                }
            }

            if (choosingControllerX != -1 && choosingControllerY != -1 &&
                !directionSelectionStartedThisFrame) {
                auto choices = getDirectionChoices(
                    choosingControllerX, choosingControllerY
                );
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    for (const RollerDirectionChoice& choice : choices) {
                        if (!CheckCollisionPointRec(mouse, choice.hitbox)) {
                            continue;
                        }
                        auto route = getDirectedLine(choice.start);
                        for (const RollerStep& step : route) {
                            if (step.cell.x != choosingControllerX ||
                                step.cell.y != choosingControllerY) {
                                continue;
                            }
                            int shape = rollerShapes[choosingControllerY][choosingControllerX];
                            rollerEntrySides[choosingControllerY][choosingControllerX] =
                                OtherRollerSide(shape, step.exitSide);
                            rollerDirectionSet[choosingControllerY][choosingControllerX] = true;
                            break;
                        }
                        choosingControllerX = -1;
                        choosingControllerY = -1;
                        break;
                    }
                }
            }
        }

        BeginDrawing();

        ClearBackground(RAYWHITE);
        
        for (int row = 0; row < ROWS; row++) {
            for (int col = 0; col < COLS; col++) {

                TileType tile = grid[row][col];
                if (tile == TileType::Roller) {
                    DrawRollerTile(
                        col * TILE_SIZE, row * TILE_SIZE,
                        rollerShapes[row][col], DARKGRAY
                    );
                    int tileX = col * TILE_SIZE;
                    int tileY = row * TILE_SIZE;
                    const InventorySlot& cargo = rollerCargo[row][col];
                    if (cargo.amount > 0) {
                        if (cargo.type == ItemType::IronPlate) {
                            DrawRectangle(
                                col * TILE_SIZE + 4,
                                row * TILE_SIZE + 7,
                                12, 6, LIGHTGRAY
                            );
                        }
                        else {
                            Color cargoColor =
                                cargo.type == ItemType::Coal ? BLACK : BROWN;
                            DrawCircle(
                                col * TILE_SIZE + TILE_SIZE / 2,
                                row * TILE_SIZE + TILE_SIZE / 2,
                                5, cargoColor
                            );
                        }
                    }

                    if (isRollerConnected(col, row, RollerSide::Left)) {
                        DrawRectangle(tileX + 1, tileY + 8, 3, 4, GREEN);
                    }
                    if (isRollerConnected(col, row, RollerSide::Right)) {
                        DrawRectangle(tileX + TILE_SIZE - 4, tileY + 8, 3, 4, GREEN);
                    }
                    if (isRollerConnected(col, row, RollerSide::Up)) {
                        DrawRectangle(tileX + 8, tileY + 1, 4, 3, GREEN);
                    }
                    if (isRollerConnected(col, row, RollerSide::Down)) {
                        DrawRectangle(tileX + 8, tileY + TILE_SIZE - 4, 4, 3, GREEN);
                    }
                    if (rollerControllers[row][col]) {
                        DrawRectangleLines(
                            tileX + 1, tileY + 1,
                            TILE_SIZE - 2, TILE_SIZE - 2,
                            GOLD
                        );
                    }
                    continue;
                }

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

        if (!inventoryOpen && !directionInputBlocked && !developerPanelOpen &&
            insideMap &&
            grid[cellY][cellX] == TileType::Roller &&
            rollerControllers[cellY][cellX]) {
            RollerLine line = getRollerLine(cellX, cellY);

            for (const RollerCell& cell : line.cells) {
                DrawRectangle(
                    cell.x * TILE_SIZE + 2,
                    cell.y * TILE_SIZE + 2,
                    TILE_SIZE - 4,
                    TILE_SIZE - 4,
                    Fade(SKYBLUE, 0.35f)
                );
            }

            for (const RollerEnd& end : line.ends) {
                int edgeX = end.cell.x * TILE_SIZE + TILE_SIZE / 2;
                int edgeY = end.cell.y * TILE_SIZE + TILE_SIZE / 2;

                switch (end.side) {
                    case RollerSide::Left:
                        edgeX = end.cell.x * TILE_SIZE + 2;
                        break;
                    case RollerSide::Right:
                        edgeX = (end.cell.x + 1) * TILE_SIZE - 3;
                        break;
                    case RollerSide::Up:
                        edgeY = end.cell.y * TILE_SIZE + 2;
                        break;
                    case RollerSide::Down:
                        edgeY = (end.cell.y + 1) * TILE_SIZE - 3;
                        break;
                }

                DrawCircle(edgeX, edgeY, 3, ORANGE);
                int slotNumber =
                    rollerEndSlots[end.cell.y][end.cell.x]
                                  [static_cast<int>(end.side)];
                if (slotNumber > 0) {
                    DrawText(
                        TextFormat("%i", slotNumber),
                        edgeX + 5, edgeY - 10, 14, DARKBLUE
                    );
                }
            }

            DrawText(
                TextFormat(
                    "Line: %i rollers | %i ends",
                    (int)line.cells.size(),
                    (int)line.ends.size()
                ),
                10, 90, 18, DARKBLUE
            );
        }

        for (int row = 0; row < ROWS; row++) {
            for (int col = 0; col < COLS; col++) {
                if (grid[row][col] != TileType::Roller ||
                    !rollerControllers[row][col] ||
                    !rollerDirectionSet[row][col]) {
                    continue;
                }
                RollerLine line = getRollerLine(col, row);
                int controllerCount = 0;
                for (const RollerCell& cell : line.cells) {
                    if (rollerControllers[cell.y][cell.x]) {
                        controllerCount++;
                    }
                }
                if (controllerCount != 1) {
                    continue;
                }
                for (const RollerStep& step : getControllerRoute(col, row)) {
                    DrawRollerArrow(step.cell, step.exitSide, TILE_SIZE);
                }
            }
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

        if (!developerPanelOpen && insideMap && !inventoryOpen && !directionInputBlocked &&
            (placingFurnace || placingDrill || placingRoller || selectedTile != TileType::Empty)) {

            if (placingRoller) {
                DrawRollerTile(
                    cellX * TILE_SIZE, cellY * TILE_SIZE,
                    selectedRollerShape,
                    canBuild ? GREEN : RED
                );
            }
            else {
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
        else if (placingRoller) {
            selectedName = "Roller (R)";
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

            if (openedRollerX != -1 && openedRollerY != -1) {
                bool isController =
                    rollerControllers[openedRollerY][openedRollerX];
                DrawRectangle(10, 60, 180, 350, DARKGRAY);
                DrawText("Roller", 25, 75, 24, WHITE);
                if (isController) {
                    DrawText("Controller", 25, 110, 20, GOLD);
                    if (rollerDirectionSet[openedRollerY][openedRollerX]) {
                        const RollerProgram& program =
                            rollerPrograms[openedRollerY][openedRollerX];
                        const InventorySlot& reader =
                            rollerReaders[openedRollerY][openedRollerX];
                        DrawText(program.status, 25, 135, 14, WHITE);
                        DrawText(
                            TextFormat("Energy: %i", program.energy),
                            25, 153, 14, GOLD
                        );
                        if (reader.amount > 0 &&
                            program.cardIndex < (int)reader.cards.size() &&
                            reader.cards[program.cardIndex].command == 1) {
                            DrawText(
                                TextFormat(
                                    "Steps: %i/%i",
                                    program.completedActions,
                                    reader.cards[program.cardIndex].argument
                                ),
                                100, 153, 14, LIGHTGRAY
                            );
                        }
                        if (reader.amount > 0) {
                            int total = (int)reader.cards.size();
                            int shownCard = program.cardIndex + 1;
                            if (shownCard > total) {
                                shownCard = total;
                            }
                            DrawText(
                                TextFormat("Card: %i/%i", shownCard, total),
                                25, 171, 14, LIGHTGRAY
                            );
                        }
                    }
                    const InventorySlot& fuel =
                        rollerFuel[openedRollerY][openedRollerX];
                    DrawText("1. Coal", 25, 190, 18, WHITE);
                    DrawRectangleRec(rollerFuelRect, GRAY);
                    DrawRectangleLinesEx(rollerFuelRect, 1, WHITE);
                    if (fuel.amount > 0) {
                        DrawRectangle(42, 232, 26, 26, BLACK);
                    }
                    DrawText(
                        TextFormat("%i / 200", fuel.amount),
                        25, 285, 18, WHITE
                    );
                    const InventorySlot& reader =
                        rollerReaders[openedRollerY][openedRollerX];
                    DrawText("Reader", 25, 308, 18, WHITE);
                    DrawRectangleRec(rollerReaderRect, GRAY);
                    DrawRectangleLinesEx(rollerReaderRect, 1, GOLD);
                    if (reader.amount == 0) {
                        DrawText("Insert card", 35, 339, 18, LIGHTGRAY);
                    }
                    else {
                        DrawText(
                            TextFormat("Cards: %i", (int)reader.cards.size()),
                            35, 339, 18, GOLD
                        );
                    }
                }
                else {
                    DrawText("Regular roller", 25, 110, 16, LIGHTGRAY);
                }
                if (!rollerDirectionSet[openedRollerY][openedRollerX]) {
                    DrawRectangleRec(controllerButton, GREEN);
                    DrawText(
                        isController ? "Set direction" : "Set controller",
                        32, 142, 18, WHITE
                    );
                }
                DrawText("E: Close", 25, 380, 18, LIGHTGRAY);
            }

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
                const CardInstruction* shownCard = nullptr;
                int shownIndex = furnace.cardIndex;
                if (card.amount > 0 && !card.cards.empty()) {
                    if (shownIndex >= (int)card.cards.size()) {
                        shownIndex = (int)card.cards.size() - 1;
                    }
                    shownCard = &card.cards[shownIndex];
                    DrawText(
                        TextFormat("Card: %i/%i", shownIndex + 1, (int)card.cards.size()),
                        100, 270, 16, WHITE
                    );
                }

                DrawRectangle(10, 425, 590, 25, DARKGRAY);
                DrawText(furnace.status, 20, 430, 16, WHITE);
                if (shownCard != nullptr &&
                    shownCard->command == 2 && shownCard->argument > shownIndex) {
                    DrawText(
                        TextFormat("Card %i: BACK max %i", shownIndex + 1, shownIndex),
                        250, 430, 16, RED
                    );
                }
                if (shownCard != nullptr && shownCard->command == 1) {
                    DrawText(
                        TextFormat("%i/%i", furnace.completedActions, shownCard->argument),
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
                if (shownCard == nullptr) {
                    DrawText("Insert card", 35, 375, 16, LIGHTGRAY);
                }
                else {
                    const char* name = "UNKNOWN";
                    if (shownCard->command == 1) name = "RUN";
                    else if (shownCard->command == 2) name = "BACK";
                    else if (shownCard->command == 3) name = "REVERSE";
                    else if (shownCard->command == 4) name = "SLOT";

                    const char* label = shownCard->command == 3
                        ? name : TextFormat("%s %i", name, shownCard->argument);
                    DrawText(label, 35, 375, 18, GOLD);
                }
            }

            if (openedDrillX != -1 && openedDrillY != -1) {
                DrawRectangle(10, 60, 180, 350, DARKGRAY);
                DrawText("Drill", 25, 75, 24, WHITE);

                const char* labels[] = {
                    "1. Coal",
                    "2. Output"
                };

                for (int i = 0; i < 2; i++) {
                    int slotY = 130 + i * 85;
                    Rectangle slot = {
                        25.0f, (float)slotY, 60.0f, 60.0f
                    };

                    DrawText(labels[i], 25, slotY - 20, 16, WHITE);
                    DrawRectangleRec(slot, GRAY);
                    DrawRectangleLinesEx(slot, 1, WHITE);
                }

                const InventorySlot& fuel =
                    drills[openedDrillY][openedDrillX].fuel;

                if (fuel.amount > 0) {
                    DrawCircle(55, 155, 15, BLACK);
                    DrawText(TextFormat("%i", fuel.amount), 55, 170, 16, WHITE);
                }

                const DrillStorage& drill = drills[openedDrillY][openedDrillX];

                const CardInstruction* shownCard = nullptr;
                int shownIndex = drill.cardIndex;

                if (drill.reader.amount > 0 && !drill.reader.cards.empty()) {
                    if (shownIndex >= (int)drill.reader.cards.size()) {
                        shownIndex = (int)drill.reader.cards.size() - 1;
                    }
                    shownCard = &drill.reader.cards[shownIndex];
                    DrawText(
                        TextFormat(
                            "Card: %i/%i",
                            shownIndex + 1, (int)drill.reader.cards.size()
                        ),
                        25, 340, 16, WHITE
                    );
                }

                if (drill.output.amount > 0) {
                    Color color = drill.output.type == ItemType::Coal ? BLACK : BROWN;
                    DrawCircle(55, 240, 15, color);
                    DrawText(
                        TextFormat("%i", drill.output.amount),
                        55, 255, 16, WHITE
                    );
                }

                if (shownCard != nullptr && shownCard->command == 1) {
                    DrawText(
                        TextFormat(
                            "Hits: %i/%i",
                            drill.completedActions, shownCard->argument
                        ),
                        25, 290, 18, WHITE
                    );
                }

                if (shownCard != nullptr && shownCard->command == 1) {

                    float progress = drill.hitTimer / 0.4f;
                    if (progress < 0.0f) progress = 0.0f;
                    if (progress > 1.0f) progress = 1.0f;

                    DrawRectangle(25, 320, 150, 10, GRAY);
                    DrawRectangle(25, 320, (int)(150 * progress), 10, GREEN);
                    DrawRectangleLines(25, 320, 150, 10, WHITE);
                }

                DrawRectangle(10, 425, 590, 25, DARKGRAY);
                DrawText(drill.status, 20, 430, 16, WHITE);
                if (shownCard != nullptr &&
                    shownCard->command == 2 &&
                    shownCard->argument > shownIndex) {
                    DrawText(
                        TextFormat("Card %i: BACK max %i", shownIndex + 1, shownIndex),
                        250, 430, 16, RED
                    );
                }

                Rectangle reader = {25, 365, 150, 35};
                DrawRectangleRec(reader, GRAY);
                DrawRectangleLinesEx(reader, 1, WHITE);
                if (shownCard == nullptr) {
                    DrawText("Insert card", 35, 375, 16, LIGHTGRAY);
                }
                else {
                    const char* name = "UNKNOWN";
                    if (shownCard->command == 1) name = "RUN";
                    else if (shownCard->command == 2) name = "BACK";
                    else if (shownCard->command == 3) name = "REVERSE";
                    else if (shownCard->command == 4) name = "SLOT";

                    const char* label = shownCard->command == 3
                        ? name : TextFormat("%s %i", name, shownCard->argument);
                    DrawText(label, 35, 375, 18, GOLD);
                }
            }

            // Панель крафта справа от инвентаря.
            DrawRectangle(610, 60, 185, 375, DARKGRAY);
            DrawText("Crafting", 620, 75, 22, WHITE);
            DrawText("Furnace", 620, 96, 18, WHITE);
            DrawRectangleRec(craftButton, stoneCount >= 3 ? GREEN : GRAY);
            DrawText("Craft Furnace", 625, 136, 16, WHITE);
            DrawText(
                TextFormat("Stone: %i/3", stoneCount),
                620, 115, 14, stoneCount >= 3 ? GREEN : RED
            );

            DrawText("Blank Card", 620, 156, 18, WHITE);
            DrawRectangleRec(cardButton, woodCount >= 1 ? GREEN : GRAY);
            DrawText("Craft Card", 625, 196, 16, WHITE);
            DrawText(
                TextFormat("Wood: %i/1", woodCount),
                620, 175, 14, woodCount >= 1 ? GREEN : RED
            );

            DrawText("Drill", 620, 216, 18, WHITE);
            DrawText(
                TextFormat("Plates: %i/3", plateCount),
                620, 235, 14, plateCount >= 3 ? GREEN : RED
            );
            DrawRectangleRec(drillButton, plateCount >= 3 ? GREEN : GRAY);
            DrawText("Craft Drill", 625, 256, 16, WHITE);

            bool canCraftRoller = plateCount >= 1 && woodCount >= 1;
            DrawText("Roller", 620, 276, 18, WHITE);
            DrawText(
                TextFormat("Plate %i/1 Wood %i/1", plateCount, woodCount),
                620, 295, 14, canCraftRoller ? GREEN : RED
            );
            DrawRectangleRec(rollerButton, canCraftRoller ? GREEN : GRAY);
            DrawText("Craft Roller", 625, 316, 16, WHITE);

            if (!craftQueue.empty()) {
                const char* craftingName = "Furnace";
                if (craftQueue.front().result == ItemType::BlankCard) {
                    craftingName = "Blank Card";
                }
                else if (craftQueue.front().result == ItemType::Drill) {
                    craftingName = "Drill";
                }
                else if (craftQueue.front().result == ItemType::Roller) {
                    craftingName = "Roller";
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
                    else if (inventory[i].type == ItemType::Roller) {
                        DrawRollerItem(slotX + 12, slotY + 15);
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
                            inventory[i].cards.front().command, inventory[i].cards.front().argument
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
                else if (item.type == ItemType::Roller) {
                    DrawRollerItem((int)mouse.x - 18, (int)mouse.y - 11);
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
                        item.cards.front().command, item.cards.front().argument
                    );
                }
                else if (item.type == ItemType::Roller) {
                    DrawRollerItem((int)mouse.x - 18, (int)mouse.y - 11);
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

                    if (!puncherOpen &&
                        inventory[i].type == ItemType::PunchedCard) {

                        const auto& cards = inventory[i].cards;
                        int count = (int)cards.size();

                        if (previewSlot != i) {
                            previewSlot = i;
                            previewOffset = 0;
                        }

                        int visibleCount = count < 6 ? count : 6;
                        int maxOffset = count - visibleCount;

                        float wheel = GetMouseWheelMove();
                        if (wheel > 0) previewOffset--;
                        if (wheel < 0) previewOffset++;
                        if (previewOffset < 0) previewOffset = 0;
                        if (previewOffset > maxOffset) previewOffset = maxOffset;

                        int width = 240;
                        int height = 40 + visibleCount * 24;
                        if (maxOffset > 0) height += 24;

                        int tooltipX = (int)mouse.x + 15;
                        int tooltipY = (int)mouse.y + 15;
                        if (tooltipX + width > GetScreenWidth()) {
                            tooltipX = GetScreenWidth() - width;
                        }
                        if (tooltipY + height > GetScreenHeight()) {
                            tooltipY = GetScreenHeight() - height;
                        }

                        DrawRectangle(tooltipX, tooltipY, width, height, DARKGRAY);
                        DrawRectangleLines(tooltipX, tooltipY, width, height, GOLD);
                        DrawText(
                            TextFormat("Cards: %i", count),
                            tooltipX + 10, tooltipY + 8, 18, GOLD
                        );

                        for (int row = 0; row < visibleCount; row++) {
                            int index = previewOffset + row;
                            const CardInstruction& instruction = cards[index];
                            const char* name = "UNKNOWN";
                            if (instruction.command == 1) name = "RUN";
                            else if (instruction.command == 2) name = "BACK";
                            else if (instruction.command == 3) name = "REVERSE";
                            else if (instruction.command == 4) name = "SLOT";

                            const char* line = instruction.command == 3
                                ? TextFormat("%i. %s", index + 1, name)
                                : TextFormat(
                                    "%i. %s %i", index + 1, name, instruction.argument
                                );
                            DrawText(
                                line,
                                tooltipX + 10, tooltipY + 34 + row * 24,
                                18, WHITE
                            );
                        }

                        if (maxOffset > 0) {
                            DrawText(
                                "Mouse wheel: scroll",
                                tooltipX + 10, tooltipY + height - 24,
                                16, LIGHTGRAY
                            );
                        }

                        // Подсказку для карты уже нарисовали.
                        break;
                    }

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
                    else if (inventory[i].type == ItemType::Roller) {
                        itemName = "Roller";
                    }

                    else if (inventory[i].type == ItemType::BlankCard) {
                        itemName = "Blank Card";
                    }
                    else if (inventory[i].type == ItemType::PunchedCard) {
                        const char* name = "UNKNOWN";
                        if (inventory[i].cards.front().command == 1) name = "RUN";
                        else if (inventory[i].cards.front().command == 2) name = "BACK";
                        else if (inventory[i].cards.front().command == 3) name = "REVERSE";
                        else if (inventory[i].cards.front().command == 4) name = "SLOT";

                        itemName = inventory[i].cards.front().command == 3
                            ? name : TextFormat("%s %i", name, inventory[i].cards.front().argument);
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
        
        if (inventoryOpen && !puncherOpen &&
            selectedSlot != -1 &&
            inventory[selectedSlot].type == ItemType::PunchedCard) {
            DrawText(
                "Shift+Click: join cards | X: detach last",
                220, 40, 16, WHITE
            );
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

        if (!developerPanelOpen && choosingControllerX != -1) {
            auto choices = getDirectionChoices(
                choosingControllerX, choosingControllerY
            );
            for (const RollerDirectionChoice& choice : choices) {
                DrawRectangleLinesEx(choice.hitbox, 2, ORANGE);
            }
            DrawText(
                "Choose direction: click a marker | E: Cancel",
                10, 90, 18, DARKBLUE
            );
        }

        if (developerPanelOpen) {
            DrawRectangle(
                0, 0, GetScreenWidth(), GetScreenHeight(),
                Fade(BLACK, 0.55f)
            );
            DrawRectangle(125, 55, 550, 340, DARKGRAY);
            DrawRectangleLines(125, 55, 550, 340, GOLD);
            DrawText("Developer Panel", 145, 70, 26, WHITE);
            DrawText("PAUSED | F1: Close", 145, 102, 16, GOLD);

            for (int i = 0; i < 9; i++) {
                Rectangle button = GetDeveloperButtonRect(i);
                bool hovered = CheckCollisionPointRec(mouse, button);
                DrawRectangleRec(button, hovered ? GRAY : DARKBLUE);
                DrawRectangleLinesEx(button, 1, LIGHTGRAY);
                DrawText(
                    developerItems[i].name,
                    (int)button.x + 10, (int)button.y + 10,
                    18, WHITE
                );
                DrawText(
                    "+10", (int)button.x + 10, (int)button.y + 35,
                    18, GOLD
                );
            }

            if (inventoryFullTimer > 0.0f) {
                DrawText("INVENTORY FULL", 145, 365, 20, RED);
            }
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
