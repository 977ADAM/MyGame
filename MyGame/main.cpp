
#include <raylib.h>
#include <algorithm>
#include <queue>
#include <string>
#include <vector>
#include <utility>
#include <random>
#include <array>

// Типы предметов
enum class ItemType {
    Empty,
    IronOre,
    Coal,
    Stone,
    Wood,
    Furnace,
    IronPlate,
    Drill,
    Chest
};

// Максимальный размер стака
int GetMaxStack(ItemType type) {
    if (type == ItemType::IronOre) {
        return 100;
    }
    else if (type == ItemType::Coal) {
        return 200;
    }
    else if (type == ItemType::Furnace || type == ItemType::Drill || type == ItemType::Chest) {
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

Rectangle GetChestSlotRect(int index) {
    return {
        (float)(25 + index % 2 * 75),
        (float)(130 + index / 2 * 85),
        60, 60
    };
}

void DrawChestItem(int x, int y) {
    DrawRectangle(x, y, 34, 24, DARKBROWN);
    DrawRectangle(x, y + 9, 34, 2, BROWN);
    DrawRectangle(x + 14, y + 7, 6, 7, GOLD);
}

Rectangle GetFurnaceSlotRect(int index) {
    return {25.0f, (float)(130 + index * 85), 60.0f, 60.0f};
}


struct CellPosition {
    int x;
    int y;
};

int main() {
    InitWindow(800, 450, "My Game");
    // ESC закрывает окна, а не игру.
    SetExitKey(KEY_NULL);

    int x = 400;
    int y = 200;
    int speed = 7;
    bool inventoryOpen = false;
    
    SetTargetFPS(60);
    
    const int TILE_SIZE = 20;
    const int COLS = 40;
    const int ROWS = 23;
    
    enum class TileType {
        Empty,
        IronOre,
        Coal,
        Drill,
        Boulder,
        Tree,
        Furnace,
        Chest
    };
    

    struct InventorySlot {
        ItemType type = ItemType::Empty;
        int amount = 0;
    };

    auto drawStoredStack = [](const InventorySlot& item, int slotX, int slotY) {
        if (item.amount == 0) {
            return;
        }

        if (item.type == ItemType::IronOre) {
            DrawCircle(slotX + 30, slotY + 25, 15, BROWN);
        }
        else if (item.type == ItemType::Coal) {
            DrawCircle(slotX + 30, slotY + 25, 15, BLACK);
        }
        else if (item.type == ItemType::Stone) {
            DrawCircle(slotX + 30, slotY + 25, 15, DARKGRAY);
        }
        else if (item.type == ItemType::Wood) {
            DrawCircle(slotX + 30, slotY + 25, 15, DARKBROWN);
        }
        else if (item.type == ItemType::IronPlate) {
            DrawRectangle(slotX + 12, slotY + 18, 36, 14, LIGHTGRAY);
        }
        else if (item.type == ItemType::Drill) {
            DrawRectangle(slotX + 15, slotY + 10, 30, 30, PURPLE);
        }
        else if (item.type == ItemType::Chest) {
            DrawChestItem(slotX + 13, slotY + 13);
        }
        else if (item.type == ItemType::Furnace) {
            DrawRectangle(
                slotX + 15,
                slotY + 10,
                30, 30,
                ORANGE
            );
        }


        // Показываем количество предметов
        DrawText(
            TextFormat("%i", item.amount),
            slotX + 40,
            slotY + 40,
            16,
            WHITE
        );
    };

    auto moveChestStack = [](InventorySlot& source, InventorySlot& target, bool single) {
        if (source.amount == 0 || (target.amount > 0 && target.type != source.type)) {
            return;
        }

        const int space = GetMaxStack(source.type) - target.amount;
        const int count = std::min(single ? 1 : source.amount, space);
        if (count <= 0) {
            return;
        }


        target.type = source.type;
        target.amount += count;
        source.amount -= count;
        if (source.amount == 0) {
            source = {};
        }
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

    // ==========================================
    // ECONOMY — Деньги и процедурные контракты
    // ==========================================
    long long money = 0;
    long long experiencePoints = 0; // Накопленный опыт за выполненные контракты
    int completedContracts = 0;
    bool contractsOpen = false;
    bool shopOpen = false;
    bool researchOpen = false;
    float drillHitDuration = 0.4f;
    constexpr float CONTRACT_REFRESH_TIME = 90.0f;
    float contractRefresh = CONTRACT_REFRESH_TIME;
    int nextContractId = 1;
    std::mt19937 contractRng(std::random_device{}());
    auto randomInt = [&](int low, int high) {
        return std::uniform_int_distribution<int>(low, high)(contractRng);
    };
    struct ContractProduct {
        ItemType type;
        const char* name;
        int minAmount, maxAmount, unitPrice;
        bool equipment;
    };
    // Только предметы, доступные в текущем прототипе.
    const ContractProduct contractProducts[] = {
        {ItemType::IronOre, "Iron ore", 5, 25, 3, false},
        {ItemType::Coal, "Coal", 5, 30, 2, false},
        {ItemType::IronPlate, "Iron plates", 3, 12, 8, false},
        {ItemType::Wood, "Wood", 1, 4, 5, false},
        {ItemType::Stone, "Stone", 1, 2, 8, false},
        {ItemType::Furnace, "Furnaces", 1, 1, 35, true},
        {ItemType::Drill, "Drills", 1, 3, 40, true},
        {ItemType::Chest, "Chests", 1, 2, 25, true},
    };
    const char* contractClients[] = {
        "Royal Railway Company", "Manchester Steel Works",
        "London Mechanical Works", "British Mining Company",
        "Babbage Engineering"
    };
    struct Contract {
        int id = 0;
        int productIndex = 0;
        int clientIndex = 0;
        int required = 0;
        int delivered = 0;
        int reward = 0;
        bool accepted = false;
    };
    // Опыт зависит от согласованной награды: 1 XP за каждые 5 GBP,
    // с округлением вверх. Условия не меняются после принятия заказа.
    auto contractExperienceReward = [](const Contract& c) {
        return std::max(1, (c.reward + 4) / 5);
    };
    std::array<Contract, 3> contracts{};
    auto generateContract = [&](int index) {
        // В первой строке — ресурс, во второй — изделие,
        // третья строка выбирает любую категорию.
        const int productIndex = index == 0 ? randomInt(0, 4) :
            (index == 1 ? randomInt(5, 7) : randomInt(0, 7));
        const ContractProduct& product = contractProducts[productIndex];
        Contract c;
        c.id = nextContractId++;
        c.productIndex = productIndex;
        c.clientIndex = randomInt(0, 4);
        c.required = randomInt(product.minAmount, product.maxAmount);
        c.reward = std::max(1, c.required * product.unitPrice * randomInt(90, 125) / 100);
        contracts[index] = c;
    };
    for (int i = 0; i < (int)contracts.size(); ++i) generateContract(i);

    auto contractInventoryAmount = [&](ItemType type) {
        int total = 0;
        for (const InventorySlot& slot : inventory) {
            if (slot.type == type) total += slot.amount;
        }
        return total;
    };
    // RESEARCH — одноразовые исследования за деньги, XP и материалы.
    struct ResearchTechnology {
        const char* name;
        const char* description;
        int moneyCost, xpCost;
        ItemType material;
        const char* materialName;
        int materialCost;
        int prerequisite;
        bool unlocked = false;
    };
    std::array<ResearchTechnology, 1> technologies{{
        {"Precision drilling", "All drills: 0.4s > 0.25s per hit", 55, 12,
            ItemType::IronPlate, "Plates", 3, -1}
    }};
    std::string researchNotice = "Choose a technology to research.";
    auto researchBlocker = [&](int index) -> const char* {
        if (index < 0 || index >= (int)technologies.size()) return "Invalid technology.";
        const auto& t = technologies[index];
        if (t.unlocked) return "Already researched.";
        if (t.prerequisite >= 0 && !technologies[t.prerequisite].unlocked)
            return "Research prerequisite first.";
        if (money < t.moneyCost) return "Not enough money.";
        if (experiencePoints < t.xpCost) return "Not enough XP.";
        if (contractInventoryAmount(t.material) < t.materialCost) return "Not enough materials.";
        return nullptr;
    };
    auto researchTechnology = [&](int index) {
        if (const char* reason = researchBlocker(index)) {
            researchNotice = reason;
            return false;
        }
        auto& t = technologies[index];
        int remaining = t.materialCost;
        for (auto& slot : inventory) {
            if (remaining == 0) break;
            if (slot.type != t.material || slot.amount <= 0) continue;
            int used = std::min(slot.amount, remaining);
            slot.amount -= used;
            remaining -= used;
            if (slot.amount == 0) slot = {};
        }
        money -= t.moneyCost;
        experiencePoints -= t.xpCost;
        t.unlocked = true;
        drillHitDuration = 0.25f;
        researchNotice = std::string("Researched: ") + t.name;
        return true;
    };
    auto researchRect = [](int index) -> Rectangle {
        return {550, (float)(92 + index * 74), 200, 34};
    };

    auto acceptContract = [&](int index) {
        if (!contracts[index].accepted) contracts[index].accepted = true;
    };
    std::string contractNotice = "Choose an offer to begin.";
    auto deliverContract = [&](int index) {
        Contract& c = contracts[index];
        if (!c.accepted) return 0;
        const ItemType type = contractProducts[c.productIndex].type;
        int remaining = c.required - c.delivered;
        int deliveredNow = 0;
        for (InventorySlot& slot : inventory) {
            if (remaining <= 0) break;
            if (slot.type != type || slot.amount <= 0) continue;
            const int take = std::min(remaining, slot.amount);
            slot.amount -= take;
            remaining -= take;
            deliveredNow += take;
            if (slot.amount == 0) slot = {};
        }
        c.delivered += deliveredNow;
        if (c.delivered == c.required) {
            const int earnedXP = contractExperienceReward(c);
            money += c.reward;
            experiencePoints += earnedXP;
            ++completedContracts;
            contractNotice = "Completed #" + std::to_string(c.id) +
                ": +GBP " + std::to_string(c.reward) +
                " | +" + std::to_string(earnedXP) + " XP";
            generateContract(index);
        } else if (deliveredNow > 0) {
            contractNotice = "Delivered " + std::to_string(deliveredNow) + " units.";
        } else {
            contractNotice = "No matching goods in inventory.";
        }
        return deliveredNow;
    };
    auto refreshContracts = [&](float dt) {
        contractRefresh -= dt;
        if (contractRefresh > 0.0f) return;
        contractRefresh = CONTRACT_REFRESH_TIME;
        for (int i = 0; i < (int)contracts.size(); ++i) {
            if (!contracts[i].accepted) generateContract(i);
        }
    };
    // SHOP — цены за единицу выше максимальной оплаты контракта.
    struct ShopProduct {
        int contractProductIndex;
        int pack, unitPrice;
    };
    const ShopProduct shopProducts[] = {
        {0, 5, 4}, {1, 5, 3}, {2, 5, 11}, {3, 5, 7}, {4, 5, 11},
        {5, 1, 45}, {6, 1, 51}, {7, 1, 32}
    };
    constexpr int SHOP_PRODUCT_COUNT = sizeof(shopProducts) / sizeof(shopProducts[0]);
    std::string shopNotice = "Choose a product to buy a pack.";
    auto shopFreeSpace = [&](ItemType type) {
        int space = 0;
        const int maxStack = GetMaxStack(type);
        for (const InventorySlot& slot : inventory) {
            if (slot.amount == 0) space += maxStack;
            else if (slot.type == type) space += std::max(0, maxStack - slot.amount);
        }
        return space;
    };
    auto canBuyShopProduct = [&](int index) {
        if (index < 0 || index >= SHOP_PRODUCT_COUNT) return false;
        const auto& p = shopProducts[index];
        return money >= p.pack * p.unitPrice &&
            shopFreeSpace(contractProducts[p.contractProductIndex].type) >= p.pack;
    };
    auto buyShopProduct = [&](int index) {
        if (index < 0 || index >= SHOP_PRODUCT_COUNT) return false;
        const auto& p = shopProducts[index];
        const auto& product = contractProducts[p.contractProductIndex];
        const int price = p.pack * p.unitPrice;
        if (money < price) { shopNotice = "Not enough money."; return false; }
        if (shopFreeSpace(product.type) < p.pack) {
            shopNotice = "Not enough inventory space."; return false;
        }
        // Предварительная проверка гарантирует размещение всей партии.
        addToInventory(inventory, INVENTORY_SIZE, product.type, p.pack);
        money -= price;
        shopNotice = "Bought " + std::to_string(p.pack) + " x " + product.name +
            " for GBP " + std::to_string(price);
        return true;
    };
    auto shopProductRect = [](int index) -> Rectangle {
        return {(float)(24 + (index / 5) * 384), (float)(80 + (index % 5) * 61), 368, 55};
    };

    auto contractActionRect = [](int index) -> Rectangle {
        return {548, (float)(116 + index * 92), 200, 30};
    };
    auto contractAbandonRect = [](int index) -> Rectangle {
        return {548, (float)(150 + index * 92), 200, 24};
    };


    int selectedSlot = -1;
    int splitSource = -1;
    struct CraftOrder {
        ItemType result;
        float duration;
        int amount = 1;
    };

    std::queue<CraftOrder> craftQueue;
    float craftTimer = 0.0f;
    // Частично помещённая партия не дублируется при следующем кадре.
    auto finishCraftOrder = [&]() {
        if (craftQueue.empty() || craftTimer > 0.0f) return false;
        auto& order = craftQueue.front();
        order.amount = addToInventory(inventory, INVENTORY_SIZE, order.result, order.amount);
        if (order.amount > 0) return false;
        craftQueue.pop();
        if (!craftQueue.empty()) craftTimer = craftQueue.front().duration;
        return true;
    };
    
    static TileType workshopGrid[ROWS][COLS] = {};
    auto* grid = workshopGrid;
    const int LARGE_BUILDING_SIZE = 2;
    static CellPosition workshopBuildingOrigins[ROWS][COLS] = {};
    auto* buildingOrigins = workshopBuildingOrigins;

    static TileType workshopDrillDeposits[ROWS][COLS] = {};
    auto* drillDeposits = workshopDrillDeposits;
    static int workshopDurability[ROWS][COLS] = {};
    auto* durability = workshopDurability;
    static int workshopResourceAmount[ROWS][COLS] = {};
    auto* resourceAmount = workshopResourceAmount;
    
    auto isLargeBuilding = [](TileType tile) {
        return tile == TileType::Furnace || tile == TileType::Drill || tile == TileType::Chest;
    };

    auto isBuildingOrigin = [&](int col, int row) {
        const CellPosition origin = buildingOrigins[row][col];
        return origin.x == col && origin.y == row;
    };

    auto placeLargeBuilding = [&](CellPosition origin, TileType type) {
        for (int row = origin.y; row < origin.y + LARGE_BUILDING_SIZE; row++) {
            for (int col = origin.x; col < origin.x + LARGE_BUILDING_SIZE; col++) {
                if (type == TileType::Drill) {
                    drillDeposits[row][col] = grid[row][col];
                }
                grid[row][col] = type;
                buildingOrigins[row][col] = origin;
            }
        }
    };

    auto removeLargeBuilding = [&](CellPosition origin) {
        for (int row = origin.y; row < origin.y + LARGE_BUILDING_SIZE; row++) {
            for (int col = origin.x; col < origin.x + LARGE_BUILDING_SIZE; col++) {
                grid[row][col] = resourceAmount[row][col] > 0
                    ? drillDeposits[row][col] : TileType::Empty;
                drillDeposits[row][col] = TileType::Empty;
                buildingOrigins[row][col] = {};
            }
        }
    };

    auto getDepositItem = [](TileType tile) {
        if (tile == TileType::IronOre) return ItemType::IronOre;
        if (tile == TileType::Coal) return ItemType::Coal;
        return ItemType::Empty;
    };

    auto findDrillDeposit = [&](CellPosition origin, bool includeDepleted = false) {
        for (int row = origin.y; row < origin.y + LARGE_BUILDING_SIZE; row++) {
            for (int col = origin.x; col < origin.x + LARGE_BUILDING_SIZE; col++) {
                if (drillDeposits[row][col] != TileType::Empty &&
                    (includeDepleted || resourceAmount[row][col] > 0)) {
                    return CellPosition{col, row};
                }
            }
        }
        return CellPosition{-1, -1};
    };

    struct FurnaceStorage {
        InventorySlot ore = {};
        InventorySlot fuel = {};
        InventorySlot output = {};
        int energy = 0;
        bool smelting = false;
        float smeltTimer = 0.0f;
        const char* status = "Idle";
    };

    static FurnaceStorage workshopFurnaces[ROWS][COLS] = {};
    auto* furnaces = workshopFurnaces;

    struct DrillStorage {
        InventorySlot fuel = {};
        InventorySlot output = {};
        int energy = 0;
        float hitTimer = 0.0f;
        const char* status = "Idle";
    };

    static DrillStorage workshopDrills[ROWS][COLS] = {};
    auto* drills = workshopDrills;

    const int CHEST_SIZE = 5;
    struct ChestStorage {
        InventorySlot slots[CHEST_SIZE] = {};
    };
    static ChestStorage workshopChests[ROWS][COLS] = {};
    auto* chests = workshopChests;

    grid[10][15] = TileType::IronOre;
    grid[10][16] = TileType::IronOre;
    grid[11][15] = TileType::IronOre;
    grid[11][16] = TileType::IronOre;
    
    grid[5][9] = TileType::Coal;
    grid[5][10] = TileType::Coal;
    grid[12][14] = TileType::Coal;
    
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

    // Режим размещения постройки из инвентаря.
    bool placingFurnace = false;
    bool placingDrill = false;
    bool placingChest = false;
    
    float inventoryFullTimer = 0.0f;

    int dismantleX = -1;
    int dismantleY = -1;
    float dismantleTimer = 0.0f;
    const float DISMANTLE_TIME = 1.0f;

    int openedFurnaceX = -1;
    int openedFurnaceY = -1;
    int openedDrillX = -1;
    int openedDrillY = -1;
    CellPosition openedChest = {-1, -1};
    int selectedChestSlot = -1;

    // ==========================================
    // SIMULATION — Производство
    // ==========================================
    bool productionEnabled = false;

    auto updateFurnace = [&](FurnaceStorage& furnace, float dt, bool enabled) {
        if (!enabled) { furnace.status = "Paused"; return; }
        if (furnace.output.amount >= GetMaxStack(ItemType::IronPlate)) {
            furnace.status = "OUTPUT FULL"; return;
        }
        if (!furnace.smelting) {
            if (furnace.ore.amount == 0) { furnace.status = "NO ORE"; return; }
            if (furnace.energy == 0) {
                if (furnace.fuel.amount == 0) { furnace.status = "FUEL EXHAUSTED"; return; }
                if (--furnace.fuel.amount == 0) furnace.fuel = {};
                furnace.energy = 3;
            }
            if (--furnace.ore.amount == 0) furnace.ore = {};
            --furnace.energy;
            furnace.smelting = true;
            furnace.smeltTimer = 3.0f;
        }
        furnace.status = "Smelting";
        furnace.smeltTimer -= dt;
        if (furnace.smeltTimer <= 0.0f) {
            furnace.output.type = ItemType::IronPlate;
            ++furnace.output.amount;
            furnace.smelting = false;
            furnace.smeltTimer = 0.0f;
        }
    };

    auto updateDrill = [&](CellPosition origin, DrillStorage& drill, float dt, bool enabled) {
        if (!enabled) { drill.status = "Paused"; return; }
        const CellPosition deposit = findDrillDeposit(origin);
        if (deposit.x == -1) { drill.status = "DEPOSIT DEPLETED"; return; }
        const ItemType resource = getDepositItem(drillDeposits[deposit.y][deposit.x]);
        if (drill.output.amount > 0 && drill.output.type != resource) {
            drill.status = "CLEAR OUTPUT"; return;
        }
        if (drill.output.amount >= GetMaxStack(resource)) {
            drill.status = "OUTPUT FULL"; return;
        }
        if (drill.energy == 0) {
            if (drill.fuel.amount == 0) { drill.status = "FUEL EXHAUSTED"; return; }
            if (--drill.fuel.amount == 0) drill.fuel = {};
            drill.energy = 3;
        }
        drill.status = "Mining";
        drill.hitTimer += dt;
        if (drill.hitTimer < drillHitDuration) return;
        drill.hitTimer = 0.0f;
        --drill.energy;
        int& hitsLeft = durability[deposit.y][deposit.x];
        int& remaining = resourceAmount[deposit.y][deposit.x];
        if (--hitsLeft <= 0) {
            drill.output.type = resource;
            ++drill.output.amount;
            if (--remaining > 0) hitsLeft = resource == ItemType::IronOre ? 3 : 2;
        }
    };

    auto updateMachines = [&](float dt) {
        const bool enabled = productionEnabled;
        for (int row = 0; row < ROWS; ++row) {
            for (int col = 0; col < COLS; ++col) {
                const TileType tile = grid[row][col];
                if (tile != TileType::Furnace && tile != TileType::Drill) continue;
                if (!isBuildingOrigin(col, row)) continue;
                if (tile == TileType::Furnace) updateFurnace(furnaces[row][col], dt, enabled);
                else updateDrill({col, row}, drills[row][col], dt, enabled);
            }
        }
    };

    while (!WindowShouldClose()) {
        
        // Закрывает интерфейс игрока при открытии меню.
        auto closeInteractionUI = [&]() {
            inventoryOpen = false;
            openedFurnaceX = openedFurnaceY = -1;
            openedDrillX = openedDrillY = -1;
            openedChest = {-1, -1};
            selectedChestSlot = -1;
            selectedSlot = -1;
            splitSource = -1;
            dismantleX = dismantleY = -1;
            dismantleTimer = 0.0f;
        };

        Vector2 mouse = GetMousePosition();
        int cellX = (int)mouse.x / TILE_SIZE;
        int cellY = (int)mouse.y / TILE_SIZE;
        bool insideMap =
            cellX >= 0 && cellX < COLS &&
            cellY >= 0 && cellY < ROWS;
        bool canBuild = false;
        int stoneCount = 0;
        int woodCount = 0;
        int plateCount = 0;
        Rectangle craftButton = {615, 132, 170, 24};
        Rectangle drillButton = {615, 192, 170, 24};
        Rectangle chestButton = {615, 221, 170, 25};

        // Биржа. Модальное окно не пропускает
        // нажатия мыши и клавиш в игровую карту.
        bool researchToggled = false;
        if (researchOpen && (IsKeyPressed(KEY_L) || IsKeyPressed(KEY_ESCAPE))) {
            researchOpen = false;
            researchToggled = true;
        } else if (!researchOpen && !shopOpen && !contractsOpen &&
                   !inventoryOpen && IsKeyPressed(KEY_L)) {
            closeInteractionUI();
            researchOpen = true;
            researchToggled = true;
        }
        if (researchOpen && !researchToggled && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            for (int i = 0; i < (int)technologies.size(); ++i) {
                if (CheckCollisionPointRec(mouse, researchRect(i))) {
                    researchTechnology(i);
                    break;
                }
            }
        }
        bool shopToggled = false;
        if (shopOpen && (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_ESCAPE))) {
            shopOpen = false;
            shopToggled = true;
        } else if (!shopOpen && !contractsOpen && !researchOpen && !researchToggled &&
                   !inventoryOpen && IsKeyPressed(KEY_S)) {
            closeInteractionUI();
            shopOpen = true;
            shopToggled = true;
        }
        if (shopOpen && !shopToggled && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            for (int i = 0; i < SHOP_PRODUCT_COUNT; ++i) {
                if (CheckCollisionPointRec(mouse, shopProductRect(i))) {
                    buyShopProduct(i);
                    break;
                }
            }
        }
        bool contractToggled = false;
        if (contractsOpen && (IsKeyPressed(KEY_B) || IsKeyPressed(KEY_ESCAPE))) {
            contractsOpen = false;
            contractToggled = true;
        } else if (!contractsOpen && !shopOpen && !shopToggled && !researchOpen && !researchToggled &&
                   !inventoryOpen && IsKeyPressed(KEY_B)) {
            closeInteractionUI();
            contractsOpen = true;
            contractToggled = true;
        }
        if (contractsOpen && !contractToggled && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            for (int i = 0; i < (int)contracts.size(); ++i) {
                if (CheckCollisionPointRec(mouse, contractActionRect(i))) {
                    if (!contracts[i].accepted) {
                        acceptContract(i);
                        contractNotice = "Accepted #" + std::to_string(contracts[i].id);
                    } else {
                        deliverContract(i);
                    }
                    break;
                }
                if (contracts[i].accepted && contracts[i].delivered == 0 &&
                    CheckCollisionPointRec(mouse, contractAbandonRect(i))) {
                    generateContract(i);
                    contractNotice = "Order abandoned. New offer available.";
                    break;
                }
            }
        }
        refreshContracts(GetFrameTime());

        if (inventoryFullTimer > 0.0f) {
            inventoryFullTimer -= GetFrameTime();
        }

        // Модальные окна приостанавливают ввод, но не производство.
        if (!contractsOpen && !contractToggled && !shopOpen && !shopToggled &&
            !researchOpen && !researchToggled) {

            bool furnacePlacedThisFrame = false;
            bool drillPlacedThisFrame = false;
            bool chestPlacedThisFrame = false;
            bool chestOpenedThisFrame = false;

            if (IsKeyDown(KEY_RIGHT)) x += speed;
            if (IsKeyDown(KEY_LEFT)) x -= speed;

            if (x < 0) x = 0;
            if (x > 760) x = 760;

            if (IsKeyDown(KEY_UP)) y -= speed;
            if (IsKeyDown(KEY_DOWN)) y += speed;

            if (y < 0) y = 0;
            if (y > 410) y = 410;

            Rectangle player = {(float)x, (float)y, 40, 40};

            // Клавиша 0 — отмена размещения.
            if (IsKeyPressed(KEY_ZERO)) {
                placingFurnace = false;
                placingDrill = false;

                placingChest = false;
            }

            // ==========================================
            // BUILDING — Проверка возможности строительства
            // ==========================================

            // Проверяем всю площадь выбранной постройки.
            if (insideMap && (placingFurnace || placingDrill || placingChest)) {
                const int buildSize = LARGE_BUILDING_SIZE;
                const float buildPixels = (float)(buildSize * TILE_SIZE);
                Rectangle cell = {
                    (float)(cellX * TILE_SIZE),
                    (float)(cellY * TILE_SIZE),
                    buildPixels, buildPixels
                };

                bool areaAvailable = cellX + buildSize <= COLS &&
                    cellY + buildSize <= ROWS;
                bool hasDeposit = false;
                for (int row = cellY; areaAvailable && row < cellY + buildSize; row++) {
                    for (int col = cellX; col < cellX + buildSize; col++) {
                        const TileType tile = grid[row][col];
                        if (tile == TileType::Empty) {
                            continue;
                        }
                        if (placingDrill && getDepositItem(tile) != ItemType::Empty &&
                            resourceAmount[row][col] > 0) {
                            hasDeposit = true;
                            continue;
                        }
                        areaAvailable = false;
                        break;
                    }
                }
                
                canBuild = areaAvailable && (!placingDrill || hasDeposit) &&
                    !CheckCollisionRecs(cell, player);
                
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
                            placeLargeBuilding({cellX, cellY}, TileType::Furnace);
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
                        placeLargeBuilding({cellX, cellY}, TileType::Drill);
                        drillPlacedThisFrame = true;
                        inventory[i].amount--;
                        if (inventory[i].amount == 0) {
                            inventory[i] = {};
                        }
                        placingDrill = false;

                        break;
                    }
                }

                if (placingChest &&
                    !inventoryOpen &&
                    canBuild && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    for (InventorySlot& slot : inventory) {
                        if (slot.type != ItemType::Chest || slot.amount == 0) {
                            continue;
                        }
                        placeLargeBuilding({cellX, cellY}, TileType::Chest);
                        chestPlacedThisFrame = true;
                        slot.amount--;
                        if (slot.amount == 0) {
                            slot = {};
                        }
                        placingChest = false;
                        break;
                    }
                }
            }

            // ==========================================
            // Разбор печи или бура
            // ==========================================

            bool canDismantle = false;

            if (insideMap &&
                !inventoryOpen &&
                IsMouseButtonDown(MOUSE_BUTTON_RIGHT) &&
                isLargeBuilding(grid[cellY][cellX])) {

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
                const CellPosition target = isLargeBuilding(grid[cellY][cellX])
                    ? buildingOrigins[cellY][cellX] : CellPosition{cellX, cellY};
                // При переходе на другую постройку начинаем заново.
                if (dismantleX != target.x || dismantleY != target.y) {
                    dismantleX = target.x;
                    dismantleY = target.y;
                    dismantleTimer = 0.0f;
                }

                dismantleTimer += GetFrameTime();

                if (dismantleTimer >= DISMANTLE_TIME) {
                    dismantleTimer = DISMANTLE_TIME;

                    if (grid[cellY][cellX] == TileType::Chest) {
                        ChestStorage& chest = chests[target.y][target.x];
                        InventorySlot candidate[INVENTORY_SIZE];
                        std::copy_n(inventory, INVENTORY_SIZE, candidate);

                        bool fits = addToInventory(candidate, INVENTORY_SIZE, ItemType::Chest, 1) == 0;
                        for (const InventorySlot& item : chest.slots) {
                            if (!fits) {
                                break;
                            }
                                fits = addToInventory(
                                    candidate, INVENTORY_SIZE, item.type, item.amount
                                ) == 0;
                        }

                        if (fits) {
                            std::copy_n(candidate, INVENTORY_SIZE, inventory);
                            chest = {};
                            removeLargeBuilding(target);
                            dismantleX = -1;
                            dismantleY = -1;
                            dismantleTimer = 0.0f;
                        }
                        else {
                            inventoryFullTimer = 1.0f;
                        }
                    }
                    else if (grid[cellY][cellX] == TileType::Drill) {
                        DrillStorage& drill = drills[target.y][target.x];

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


                        if (drillLeft == 0 && coalLeft == 0 &&
                            outputLeft == 0) {
                            for (int i = 0; i < INVENTORY_SIZE; i++) {
                                inventory[i] = candidate[i];
                            }

                            drill = {};
                            removeLargeBuilding(target);
                            dismantleX = -1;
                            dismantleY = -1;
                            dismantleTimer = 0.0f;
                        }
                        else {
                            inventoryFullTimer = 1.0f;
                        }
                    }
                    else {
                        FurnaceStorage& furnace = furnaces[target.y][target.x];

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


                        if (furnaceLeft == 0 && oreLeft == 0 && coalLeft == 0 &&
                            platesLeft == 0) {
                            // Всё помещается — применяем результат.
                            for (int i = 0; i < INVENTORY_SIZE; i++) {
                                inventory[i] = candidate[i];
                            }

                            furnace = {};
                            removeLargeBuilding(target);

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

            if (IsKeyPressed(KEY_E)) {
                inventoryOpen = !inventoryOpen;
                if (!inventoryOpen) splitSource = -1;
            }
            if (!inventoryOpen && IsKeyPressed(KEY_F2)) {
                productionEnabled = !productionEnabled;
            }

            // Закрываем окно постройки вместе с инвентарём.
            if (!inventoryOpen) {
                openedFurnaceX = -1;
                openedFurnaceY = -1;
                openedDrillX = -1;
                openedDrillY = -1;
                openedChest = {-1, -1};
                selectedChestSlot = -1;
            }

            // Открываем печь отдельным кликом.
            if (!inventoryOpen &&
                !placingFurnace && !placingDrill && !placingChest &&
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
                    const CellPosition origin = buildingOrigins[cellY][cellX];
                    openedFurnaceX = origin.x;
                    openedFurnaceY = origin.y;
                    inventoryOpen = true;
                }
            }

            // Открываем бур отдельным кликом в пределах досягаемости.
            if (!inventoryOpen &&
                !placingFurnace && !placingDrill && !placingChest &&
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
                    const CellPosition origin = buildingOrigins[cellY][cellX];
                    openedDrillX = origin.x;
                    openedDrillY = origin.y;
                    inventoryOpen = true;
                }
            }

            if (!inventoryOpen &&
                !placingFurnace && !placingDrill && !placingChest &&
                !chestPlacedThisFrame && insideMap &&
                IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
                grid[cellY][cellX] == TileType::Chest) {
                Vector2 playerCenter = {(float)x + 20, (float)y + 20};
                Vector2 cellCenter = {
                    (float)(cellX * TILE_SIZE + TILE_SIZE / 2),
                    (float)(cellY * TILE_SIZE + TILE_SIZE / 2)
                };
                if (CheckCollisionPointCircle(cellCenter, playerCenter, 60.0f)) {
                    openedChest = buildingOrigins[cellY][cellX];
                    inventoryOpen = true;
                    chestOpenedThisFrame = true;
                }
            }

            // Перемещение предметов в инвентаре
            if (inventoryOpen && !chestOpenedThisFrame) {
                bool leftPressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
                bool rightPressed = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);

                if (openedChest.x != -1 && (leftPressed || rightPressed)) {
                    ChestStorage& chest = chests[openedChest.y][openedChest.x];
                    for (int i = 0; i < CHEST_SIZE; i++) {
                        if (!CheckCollisionPointRec(mouse, GetChestSlotRect(i))) {
                            continue;
                        }

                        if (selectedSlot != -1) {
                            InventorySlot& source = inventory[selectedSlot];
                            moveChestStack(source, chest.slots[i], rightPressed);
                            if (source.amount == 0) {
                                selectedSlot = -1;
                                placingFurnace = false;
                                placingDrill = false;

                                placingChest = false;
                            }
                        }
                        else if (leftPressed) {
                            selectedChestSlot = chest.slots[i].amount > 0 && selectedChestSlot != i
                                ? i : -1;
                            splitSource = -1;
                        }

                        leftPressed = false;
                        rightPressed = false;
                        break;
                    }

                    if (selectedChestSlot != -1 && (leftPressed || rightPressed)) {
                        for (int i = 0; i < INVENTORY_SIZE; i++) {
                            if (!CheckCollisionPointRec(mouse, GetInventorySlotRect(i))) {
                                continue;
                            }
                            InventorySlot& source = chest.slots[selectedChestSlot];
                            moveChestStack(source, inventory[i], rightPressed);
                            if (source.amount == 0) {
                                selectedChestSlot = -1;
                            }
                            leftPressed = false;
                            rightPressed = false;
                            break;
                        }
                    }
                }


                int selectedBeforeClick = selectedSlot;

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
                            placingChest = inventory[i].type == ItemType::Chest;
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

                            placingChest = false;
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

            if (inventoryOpen && !chestOpenedThisFrame &&
                IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                if (CheckCollisionPointRec(mouse, craftButton) && stoneCount >= 3) {
                    ingredients = {{ItemType::Stone, 3}};
                    order = {ItemType::Furnace, 2.0f};
                }
                else if (CheckCollisionPointRec(mouse, drillButton) && plateCount >= 3) {
                    ingredients = {{ItemType::IronPlate, 3}};
                    order = {ItemType::Drill, 3.0f};
                }
                else if (CheckCollisionPointRec(mouse, chestButton) && woodCount >= 3) {
                    ingredients = {{ItemType::Wood, 3}};
                    order = {ItemType::Chest, 2.0f};
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

                if (craftTimer == 0.0f) finishCraftOrder();
            }


        }

        // Механизмы работают независимо от открытых окон.
        updateMachines(GetFrameTime());

        BeginDrawing();

        ClearBackground(Color{218, 208, 190, 255});
        
        for (int row = 0; row < ROWS; row++) {
            for (int col = 0; col < COLS; col++) {

                TileType tile = grid[row][col];

                const bool largeBuilding = isLargeBuilding(tile);
                if (largeBuilding && !isBuildingOrigin(col, row)) {
                    continue;
                }

                Color color;

                if (tile == TileType::Drill) {
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
                else if (tile == TileType::Chest) {
                    color = DARKBROWN;
                }
                else {
                    continue;
                }

                const int size = (largeBuilding ? LARGE_BUILDING_SIZE : 1) * TILE_SIZE;
                DrawRectangle(
                    col * TILE_SIZE,
                    row * TILE_SIZE,
                    size, size,
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

            const int size = isLargeBuilding(grid[dismantleY][dismantleX])
                ? LARGE_BUILDING_SIZE * TILE_SIZE : TILE_SIZE;
            // У верхнего края рисуем полоску под постройкой.
            if (barY < 0) {
                barY = dismantleY * TILE_SIZE + size + 2;
            }

            float progress = dismantleTimer / DISMANTLE_TIME;

            DrawRectangle(barX, barY, size, 5, DARKGRAY);
            DrawRectangle(
                barX, barY,
                (int)(size * progress), 5,
                ORANGE
            );
        }
        
        DrawText(TextFormat("Money: GBP %lld", money), 10, 35, 18, DARKGREEN);
        DrawText(TextFormat("XP: %lld", experiencePoints), 10, 57, 18, DARKBLUE);
        DrawText("B - CONTRACTS | S - SHOP | L - RESEARCH", 10, 80, 16, DARKBLUE);
        if (!inventoryOpen) {
            DrawText(productionEnabled ? "F2 - PRODUCTION ON" : "F2 - PRODUCTION OFF",
                     10, 100, 16, productionEnabled ? DARKGREEN : DARKBLUE);
        }

        int ironCount = 0;

        for (int i = 0; i < INVENTORY_SIZE; i++) {
            if (inventory[i].type == ItemType::IronOre) {
                ironCount += inventory[i].amount;
            }
        }

        DrawText(
            TextFormat("Iron: %i", ironCount),
            10, 10, 20, DARKBROWN
        );

        DrawRectangle(x, y, 40, 40, BLUE);

        // ==========================================
        // RENDER — Предпросмотр строительства
        // ==========================================

        if (insideMap && !inventoryOpen &&
            (placingFurnace || placingDrill || placingChest)) {

            Color previewColor = GRAY;

            if (placingFurnace) {
                previewColor = MAROON;
            }
            else if (placingDrill) {
                previewColor = PURPLE;
            }
            else if (placingChest) {
                previewColor = DARKBROWN;
            }

            // Запрет строительства обозначаем красным ДО рисования.
            if (!canBuild) {
                previewColor = RED;
            }

            const int size = LARGE_BUILDING_SIZE * TILE_SIZE;
            DrawRectangle(
                cellX * TILE_SIZE,
                cellY * TILE_SIZE,
                size, size,
                Fade(previewColor, 0.4f)
            );

            DrawRectangleLines(
                cellX * TILE_SIZE,
                cellY * TILE_SIZE,
                size, size,
                previewColor
            );
        }

        // ==========================================
        // UI — Отображение выбранного объекта
        // ==========================================

        // Название выбранного объекта
        const char* selectedName = "None";

        if (placingFurnace) {
            selectedName = "Furnace";
        }
        else if (placingDrill) {
            selectedName = "Drill";
        }
        else if (placingChest) {
            selectedName = "Chest";
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

            if (openedChest.x != -1) {
                const ChestStorage& chest = chests[openedChest.y][openedChest.x];
                DrawRectangle(10, 60, 180, 350, DARKGRAY);
                DrawText("Wooden Chest", 25, 75, 20, WHITE);
                for (int i = 0; i < CHEST_SIZE; i++) {
                    const Rectangle slot = GetChestSlotRect(i);
                    DrawText(TextFormat("%i", i + 1), (int)slot.x, (int)slot.y - 18, 16, WHITE);
                    DrawRectangleRec(slot, GRAY);
                    DrawRectangleLinesEx(slot, selectedChestSlot == i ? 3 : 1,
                                         selectedChestSlot == i ? YELLOW : WHITE);
                    if (selectedChestSlot != i) {
                        drawStoredStack(chest.slots[i], (int)slot.x, (int)slot.y);
                    }
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
                const FurnaceStorage& furnace =
                    furnaces[openedFurnaceY][openedFurnaceX];
                DrawRectangle(10, 425, 590, 25, DARKGRAY);
                DrawText(furnace.status, 20, 430, 16, WHITE);
                if (furnace.smelting) {
                    float progress = 1.0f - furnace.smeltTimer / 3.0f;
                    if (progress < 0.0f) progress = 0.0f;
                    if (progress > 1.0f) progress = 1.0f;
                    DrawRectangle(100, 215, 75, 10, GRAY);
                    DrawRectangle(100, 215, (int)(75 * progress), 10, GREEN);
                    DrawRectangleLines(100, 215, 75, 10, WHITE);
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


                if (drill.output.amount > 0) {
                    Color color = drill.output.type == ItemType::Coal ? BLACK : BROWN;
                    DrawCircle(55, 240, 15, color);
                    DrawText(
                        TextFormat("%i", drill.output.amount),
                        55, 255, 16, WHITE
                    );
                }


                    float progress = drill.hitTimer / drillHitDuration;
                    if (progress < 0.0f) progress = 0.0f;
                    if (progress > 1.0f) progress = 1.0f;

                    DrawRectangle(25, 320, 150, 10, GRAY);
                    DrawRectangle(25, 320, (int)(150 * progress), 10, GREEN);
                    DrawRectangleLines(25, 320, 150, 10, WHITE);

                DrawRectangle(10, 425, 590, 25, DARKGRAY);
                DrawText(drill.status, 20, 430, 16, WHITE);
            }

            // Панель крафта справа от инвентаря.
            DrawRectangle(610, 60, 185, 385, DARKGRAY);
            DrawText("Crafting", 620, 75, 22, WHITE);
            DrawText("Furnace", 620, 96, 18, WHITE);
            DrawRectangleRec(craftButton, stoneCount >= 3 ? GREEN : GRAY);
            DrawText("Craft Furnace", 625, 136, 16, WHITE);
            DrawText(
                TextFormat("Stone: %i/3", stoneCount),
                620, 115, 14, stoneCount >= 3 ? GREEN : RED
            );


            DrawText("Drill", 620, 156, 18, WHITE);
            DrawText(
                TextFormat("Plates: %i/3", plateCount),
                620, 175, 14, plateCount >= 3 ? GREEN : RED
            );
            DrawRectangleRec(drillButton, plateCount >= 3 ? GREEN : GRAY);
            DrawText("Craft Drill", 625, 196, 16, WHITE);

            DrawRectangleRec(chestButton, woodCount >= 3 ? GREEN : GRAY);
            DrawText(TextFormat("Chest  Wood %i/3", woodCount), 623, 226, 14, WHITE);

            if (!craftQueue.empty()) {
                const char* craftingName = "Furnace";
                if (craftQueue.front().result == ItemType::Drill) {
                    craftingName = "Drill";
                }
                else if (craftQueue.front().result == ItemType::Chest) {
                    craftingName = "Wooden Chest";
                }
                DrawText(craftingName, 620, 372, 16, WHITE);

                if (craftTimer > 0.0f) {
                    DrawText(
                        TextFormat("Time: %.1f", craftTimer),
                        620, 390, 15, WHITE
                    );
                }
                else {
                    DrawText("INVENTORY FULL", 620, 390, 15, RED);
                }

                float progress = 1.0f - craftTimer / craftQueue.front().duration;
                if (progress < 0.0f) progress = 0.0f;
                if (progress > 1.0f) progress = 1.0f;
                DrawRectangle(620, 408, 165, 9, GRAY);
                DrawRectangle(620, 408, (int)(165 * progress), 9, GREEN);
                DrawRectangleLines(620, 408, 165, 9, WHITE);
            }

            int waitingOrders = craftQueue.empty() ? 0 : (int)craftQueue.size() - 1;
            DrawText(
                TextFormat("Queue: %i", waitingOrders),
                620, 422, 15, WHITE
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

                if (i != selectedSlot) {
                    drawStoredStack(inventory[i], slotX, slotY);
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
                else if (item.type == ItemType::Chest) {
                    DrawChestItem((int)mouse.x - 17, (int)mouse.y - 12);
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
            const InventorySlot* carriedItem = selectedSlot != -1
                ? &inventory[selectedSlot] : nullptr;
            if (openedChest.x != -1 && selectedChestSlot != -1) {
                carriedItem = &chests[openedChest.y][openedChest.x].slots[selectedChestSlot];
            }
            if (carriedItem != nullptr) {
                const InventorySlot& item = *carriedItem;

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
                else if (item.type == ItemType::Chest) {
                    DrawChestItem((int)mouse.x - 17, (int)mouse.y - 12);
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
                    else if (inventory[i].type == ItemType::Chest) {
                        itemName = "Wooden Chest";
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


        // Биржа рисуется поверх мира. Производство и доставка продолжаются.
        if (contractsOpen) {
            DrawRectangle(0, 0, 800, 450, Color{43, 35, 29, 255});
            DrawText("INDUSTRIAL EXCHANGE", 28, 15, 28, GOLD);
            DrawText(TextFormat("GBP %lld | Completed: %i", money, completedContracts), 460, 22, 17, WHITE);
            DrawText(TextFormat("XP: %lld", experiencePoints), 460, 46, 17, SKYBLUE);
            DrawText(TextFormat("New offers in %is | Accepted orders stay", (int)(contractRefresh + 0.999f)), 28, 56, 17, LIGHTGRAY);
            for (int i = 0; i < (int)contracts.size(); ++i) {
                const Contract& c = contracts[i];
                const ContractProduct& product = contractProducts[c.productIndex];
                const int y = 98 + i * 92;
                DrawRectangle(24, y, 752, 86, DARKGRAY);
                DrawRectangleLines(24, y, 752, 86, c.accepted ? GREEN : GRAY);
                DrawText(TextFormat("#%i  %s", c.id, contractClients[c.clientIndex]), 38, y + 7, 17, WHITE);
                DrawText(TextFormat("%s | %s: %i", product.equipment ? "GOODS" : "MATERIALS", product.name, c.required), 38, y + 29, 18, GOLD);
                DrawText(TextFormat("Delivered: %i/%i | Inventory: %i", c.delivered, c.required, contractInventoryAmount(product.type)), 38, y + 51, 15, LIGHTGRAY);
                DrawText(TextFormat("GBP %i", c.reward), 411, y + 31, 18, GREEN);
                DrawText(TextFormat("+%i XP", contractExperienceReward(c)), 411, y + 53, 16, SKYBLUE);
                Rectangle action = contractActionRect(i);
                const bool enabled = !c.accepted || contractInventoryAmount(product.type) > 0;
                DrawRectangleRec(action, enabled ? DARKGREEN : GRAY);
                DrawRectangleLinesEx(action, 1, WHITE);
                DrawText(c.accepted ? "DELIVER GOODS" : "ACCEPT ORDER", 560, (int)action.y + 7, 16, WHITE);
                if (c.accepted && c.delivered == 0) {
                    Rectangle abandon = contractAbandonRect(i);
                    DrawRectangleRec(abandon, MAROON);
                    DrawText("ABANDON", 560, (int)abandon.y + 4, 14, WHITE);
                }
            }
            DrawText(contractNotice.c_str(), 28, 385, 16, YELLOW);
            DrawText("Inventory deliveries | Payment on completion | B / ESC: Close", 28, 417, 16, WHITE);
        }

        // Промышленный магазин: покупка целой партии одним нажатием.
        if (shopOpen) {
            DrawRectangle(0, 0, 800, 450, Color{43, 35, 29, 255});
            DrawText("INDUSTRIAL SHOP", 28, 15, 28, GOLD);
            DrawText(TextFormat("Balance: GBP %lld", money), 520, 22, 18, WHITE);
            DrawText("Click a product to buy one pack", 28, 52, 16, LIGHTGRAY);
            for (int i = 0; i < SHOP_PRODUCT_COUNT; ++i) {
                const auto& p = shopProducts[i];
                const auto& product = contractProducts[p.contractProductIndex];
                const Rectangle rect = shopProductRect(i);
                const bool enabled = canBuyShopProduct(i);
                DrawRectangleRec(rect, DARKGRAY);
                DrawRectangleLinesEx(rect, 1, enabled ? GREEN : GRAY);
                DrawText(product.name, (int)rect.x + 10, (int)rect.y + 7, 18, WHITE);
                DrawText(TextFormat("Pack: %i | Owned: %i", p.pack, contractInventoryAmount(product.type)),
                    (int)rect.x + 10, (int)rect.y + 31, 15, LIGHTGRAY);
                DrawRectangle((int)rect.x + 247, (int)rect.y + 8, 110, 38, enabled ? DARKGREEN : GRAY);
                DrawText(TextFormat("GBP %i", p.pack * p.unitPrice), (int)rect.x + 258, (int)rect.y + 19, 17, WHITE);
            }
            DrawText(shopNotice.c_str(), 28, 395, 16, YELLOW);
            DrawText("S / ESC - CLOSE", 580, 420, 16, WHITE);
        }

        // Исследовательская лаборатория: модальное окно поверх мира.
        if (researchOpen) {
            DrawRectangle(0, 0, 800, 450, Color{43, 35, 29, 255});
            DrawText("RESEARCH LAB", 28, 15, 28, GOLD);
            DrawText(TextFormat("GBP %lld | XP %lld", money, experiencePoints), 460, 22, 18, SKYBLUE);
            DrawText("Research once. Upgrades apply to all locations.", 28, 56, 17, LIGHTGRAY);
            for (int i = 0; i < (int)technologies.size(); ++i) {
                const auto& t = technologies[i];
                int y = 78 + i * 74;
                const char* blocker = researchBlocker(i);
                DrawRectangle(24, y, 752, 70, DARKGRAY);
                DrawRectangleLines(24, y, 752, 70, t.unlocked ? GREEN : GRAY);
                DrawText(t.name, 38, y + 7, 20, WHITE);
                DrawText(t.description, 38, y + 28, 16, GOLD);
                DrawText(TextFormat("GBP %i | %i XP | %s %i/%i", t.moneyCost, t.xpCost,
                    t.materialName, contractInventoryAmount(t.material), t.materialCost), 38, y + 48, 16, LIGHTGRAY);
                Rectangle action = researchRect(i);
                DrawRectangleRec(action, t.unlocked ? DARKGREEN : blocker ? GRAY : DARKBLUE);
                DrawRectangleLinesEx(action, 1, WHITE);
                DrawText(t.unlocked ? "RESEARCHED" : blocker ? "LOCKED" : "RESEARCH", 565, (int)action.y + 8, 18, WHITE);
                if (t.prerequisite >= 0 && !technologies[t.prerequisite].unlocked)
                    DrawText("Needs prerequisite", 551, y + 53, 14, YELLOW);
            }
            DrawText(researchNotice.c_str(), 28, 390, 17, YELLOW);
            DrawText("L / ESC - CLOSE", 580, 420, 16, WHITE);
        }


        EndDrawing();
    }

    CloseWindow();
    return 0;
}
