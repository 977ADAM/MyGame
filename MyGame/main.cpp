
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
    BlankCard,
    PunchedCard,
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

constexpr int SET_COMMAND = 6;
constexpr int ADD_COMMAND = 7;
constexpr int SUB_COMMAND = 8;
constexpr int MUL_COMMAND = 9;
constexpr int DIV_COMMAND = 10;

bool IsArithmeticCommand(int command) {
    return command >= SET_COMMAND && command <= DIV_COMMAND;
}

// 8-bit unsigned register: arithmetic wraps modulo 256.
// Invalid instructions never change the register.
bool ExecuteArithmetic(int& value, int command, int argument) {
    if (!IsArithmeticCommand(command) || argument < 0 || argument > 15 ||
        (command == DIV_COMMAND && argument == 0)) return false;
    switch (command) {
        case SET_COMMAND: value = argument; break;
        case ADD_COMMAND: value = (value + argument) & 255; break;
        case SUB_COMMAND: value = (value + 256 - argument) & 255; break;
        case MUL_COMMAND: value = (value * argument) & 255; break;
        case DIV_COMMAND: value /= argument; break;
    }
    return true;
}

const char* FormatCardCommand(int command, int argument) {
    const char* name = "UNKNOWN";
    if (command == 1) name = "RUN";
    else if (command == 2) name = "BACK";
    else if (command == SET_COMMAND) name = "SET";
    else if (command == ADD_COMMAND) name = "ADD";
    else if (command == SUB_COMMAND) name = "SUB";
    else if (command == MUL_COMMAND) name = "MUL";
    else if (command == DIV_COMMAND) name = "DIV";
    return argument == 0 && !IsArithmeticCommand(command) ? name : TextFormat("%s %i", name, argument);
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

struct CellPosition {
    int x;
    int y;
};

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
    // ESC возвращает с карты мира, а не закрывает игру.
    SetExitKey(KEY_NULL);

    int x = 400;
    int y = 200;
    int coinX = 600;
    int coinY = 300;
    bool coinCollected = false;
    int speed = 7;
    int score = 0;
    bool inventoryOpen = false;
    
    // Игровые локации
    enum class Location {
        Workshop,
        IronMine,
        CoalMine,
        Woodland
    };

    // Текущая локация игрока
    Location currentLocation = Location::Workshop;
    
    // Открыта ли карта мира
    bool worldMapOpen = false;

    // Грузовой склад железной шахты
    int mineCargoIron = 0;

    // Максимальная вместимость склада
    const int MINE_CARGO_CAPACITY = 200;
    
    // Грузовой склад мастерской
    int workshopCargoIron = 0;

    // Вместимость склада мастерской
    const int WORKSHOP_CARGO_CAPACITY = 200;

    // ==========================================
    // CARGO DELIVERY — Груз в пути и автоматические рейсы
    // ==========================================
    int CARGO_BATCH_SIZE = 10; // Улучшается исследованием грузоподъёмности.
    const float CARGO_TRAVEL_TIME = 5.0f;
    int cargoInTransit = 0;
    float cargoTravelRemaining = 0.0f;
    bool autoCargoDelivery = false;

    // Отправляем только один груз за раз. Место на складе назначения
    // резервируется размером груза; другой рейс до прибытия невозможен.
    auto dispatchCargo = [&](bool fullBatchOnly) {
        if (cargoInTransit > 0) return false;

        const int freeSpace = WORKSHOP_CARGO_CAPACITY - workshopCargoIron;
        const int amount = std::min(CARGO_BATCH_SIZE,
            std::min(mineCargoIron, freeSpace));
        if (amount <= 0 || (fullBatchOnly && amount < CARGO_BATCH_SIZE)) {
            return false;
        }

        mineCargoIron -= amount;
        cargoInTransit = amount;
        cargoTravelRemaining = CARGO_TRAVEL_TIME;
        return true;
    };

    // Таймер работает независимо от открытой локации или карты мира.
    auto updateCargoDelivery = [&](float dt) {
        if (cargoInTransit <= 0) return;

        cargoTravelRemaining = std::max(0.0f, cargoTravelRemaining - dt);
        if (cargoTravelRemaining > 0.0f) return;

        // Защита от переполнения даже при будущих новых источниках руды.
        const int freeSpace = std::max(0,
            WORKSHOP_CARGO_CAPACITY - workshopCargoIron);
        const int delivered = std::min(cargoInTransit, freeSpace);
        workshopCargoIron += delivered;
        cargoInTransit -= delivered;
        // Если места не хватило, остаток ждёт разгрузки в пути.
    };

    // Уголь: отдельный склад отправления и два склада назначения.
    const int COAL_CARGO_CAPACITY = 200;
    int coalMineCargo = 0;
    int workshopCargoCoal = 0;
    int ironMineCargoCoal = 0;
    struct CoalRoute {
        int inTransit = 0;
        float remaining = 0.0f;
    };
    CoalRoute coalToWorkshop, coalToIronMine;
    bool preferIronMine = false;
    auto dispatchCoal = [&](CoalRoute& route, int destination, bool fullBatchOnly) {
        if (route.inTransit > 0) return false;
        int amount = std::min({CARGO_BATCH_SIZE, coalMineCargo,
            COAL_CARGO_CAPACITY - destination});
        if (amount <= 0 || (fullBatchOnly && amount < CARGO_BATCH_SIZE)) return false;
        coalMineCargo -= amount;
        route.inTransit = amount;
        route.remaining = CARGO_TRAVEL_TIME;
        return true;
    };
    auto updateCoal = [&](CoalRoute& route, int& destination, float dt) {
        if (route.inTransit <= 0) return;
        route.remaining = std::max(0.0f, route.remaining - dt);
        if (route.remaining > 0.0f) return;
        int delivered = std::min(route.inTransit,
            std::max(0, COAL_CARGO_CAPACITY - destination));
        destination += delivered;
        route.inTransit -= delivered;
    };
    auto dispatchAutoCoal = [&]() {
        // Чередуем приоритет, чтобы при малом запасе не голодал второй маршрут.
        bool sent = false;
        if (preferIronMine) {
            sent = dispatchCoal(coalToIronMine, ironMineCargoCoal, true);
            sent = dispatchCoal(coalToWorkshop, workshopCargoCoal, true) || sent;
        } else {
            sent = dispatchCoal(coalToWorkshop, workshopCargoCoal, true);
            sent = dispatchCoal(coalToIronMine, ironMineCargoCoal, true) || sent;
        }
        if (sent) preferIronMine = !preferIronMine;
    };

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
        Chest,
        CargoStation
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

        if (item.type == ItemType::BlankCard) {
            DrawRectangle(slotX + 12, slotY + 15, 36, 22, BEIGE);
        }
        else if (item.type == ItemType::PunchedCard) {
            DrawPunchedCard(
                slotX + 12, slotY + 15,
                item.cards.front().command, item.cards.front().argument
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

        if (source.type == ItemType::PunchedCard) {
            target = source;
            source = {};
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
    int cardBatchSize = 1;
    float drillHitDuration = 0.4f;
    bool woodlandUnlocked = false;
    constexpr int WOODLAND_PRICE = 100;
    std::string mapNotice;
    auto buyWoodland = [&]() {
        if (woodlandUnlocked) return false;
        if (money < WOODLAND_PRICE) {
            mapNotice = "Not enough money for woodland access.";
            return false;
        }
        money -= WOODLAND_PRICE;
        woodlandUnlocked = true;
        mapNotice = "Woodland unlocked. Click it again to travel.";
        return true;
    };
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
        {ItemType::BlankCard, "Blank cards", 1, 4, 8, true}
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
            (index == 1 ? randomInt(5, 8) : randomInt(0, 8));
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
    std::array<ResearchTechnology, 4> technologies{{
        {"Card manufacturing", "Craft 2 blank cards from 1 wood", 30, 8,
            ItemType::Wood, "Wood", 2, -1},
        {"Precision drilling", "All drills: 0.4s > 0.25s per hit", 55, 12,
            ItemType::IronPlate, "Plates", 3, 0},
        {"Heavy freight", "All routes: 10 > 20 units per trip", 70, 16,
            ItemType::IronPlate, "Plates", 4, 0},
        {"Mechanical computing", "Unlock the 8-bit register and arithmetic cards", 60, 15,
            ItemType::IronPlate, "Plates", 5, 0}
    }};
    std::string researchNotice = "Choose a technology to research.";
    auto researchBlocker = [&](int index) -> const char* {
        if (index < 0 || index >= (int)technologies.size()) return "Invalid technology.";
        const auto& t = technologies[index];
        if (t.unlocked) return "Already researched.";
        if (t.prerequisite >= 0 && !technologies[t.prerequisite].unlocked)
            return "Research Card manufacturing first.";
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
        if (index == 0) cardBatchSize = 2;
        if (index == 1) drillHitDuration = 0.25f;
        if (index == 2) CARGO_BATCH_SIZE = 20;
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
        {5, 1, 45}, {6, 1, 51}, {7, 1, 32}, {8, 1, 11}
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

    // Mechanical computing prototype: one workshop register, one physical card reader.
    bool registerOpen = false;
    int registerValue = 0;
    InventorySlot registerReader = {};
    size_t registerPC = 0;
    bool registerRunning = false;
    float registerTimer = 0.0f;
    std::string registerNotice = "Insert a punched card or program.";
    auto registerInventoryRect = [](int i) -> Rectangle {
        return {(float)(420 + (i % 5) * 70), (float)(96 + (i / 5) * 66), 60, 60};
    };
    auto insertRegisterCard = [&](int index) {
        if (!technologies[3].unlocked || registerRunning || registerReader.amount != 0 ||
            index < 0 || index >= INVENTORY_SIZE ||
            inventory[index].type != ItemType::PunchedCard || inventory[index].amount != 1 ||
            inventory[index].cards.empty()) return false;
        registerReader = std::move(inventory[index]);
        inventory[index] = {};
        registerPC = 0;
        registerTimer = 0;
        registerNotice = "Program inserted. STEP or RUN.";
        return true;
    };
    auto ejectRegisterCard = [&]() {
        if (registerRunning || registerReader.amount == 0) return false;
        for (auto& slot : inventory) {
            if (slot.amount != 0) continue;
            slot = std::move(registerReader);
            registerReader = {};
            registerPC = 0;
            registerTimer = 0;
            registerNotice = "Program returned to inventory.";
            return true;
        }
        registerNotice = "Inventory full. Card stays in reader.";
        return false;
    };
    auto stepRegister = [&]() {
        if (!technologies[3].unlocked || registerPC >= registerReader.cards.size()) {
            registerRunning = false;
            registerNotice = registerReader.cards.empty() ? "Insert a program." : "Done. REWIND to repeat.";
            return false;
        }
        const auto& instruction = registerReader.cards[registerPC];
        if (!ExecuteArithmetic(registerValue, instruction.command, instruction.argument)) {
            registerRunning = false;
            registerNotice = instruction.command == DIV_COMMAND && instruction.argument == 0
                ? "Error: division by zero." : "Error: unsupported instruction.";
            return false;
        }
        ++registerPC;
        if (registerPC == registerReader.cards.size()) {
            registerRunning = false;
            registerNotice = "Program complete.";
        } else registerNotice = "Instruction executed.";
        return true;
    };
    auto updateRegister = [&](float dt) {
        if (!registerRunning) return;
        registerTimer += dt;
        while (registerRunning && registerTimer >= 0.35f) {
            registerTimer -= 0.35f;
            stepRegister();
        }
    };
    auto rewindRegister = [&]() {
        registerRunning = false;
        registerPC = 0;
        registerTimer = 0;
        registerNotice = "Reader rewound; register value preserved.";
    };
    const Rectangle registerStepButton{28, 248, 170, 32};
    const Rectangle registerRunButton{208, 248, 170, 32};
    const Rectangle registerRewindButton{28, 288, 170, 32};
    const Rectangle registerResetButton{208, 288, 170, 32};
    const Rectangle registerEjectButton{28, 328, 350, 32};

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
    
    // Отдельная карта железной шахты
    static TileType ironMineGrid[ROWS][COLS] = {};

    // Прочность и количество ресурсов в шахте
    static int ironMineDurability[ROWS][COLS] = {};
    static int ironMineResourceAmount[ROWS][COLS] = {};

    // Создаём месторождение железа в шахте
    for (int row = 8; row <= 12; row++) {
        for (int col = 18; col <= 23; col++) {

            ironMineGrid[row][col] = TileType::IronOre;
            ironMineDurability[row][col] = 3;
            ironMineResourceAmount[row][col] = 5;
        }
    }

    static TileType coalMineGrid[ROWS][COLS] = {};
    static int coalMineDurability[ROWS][COLS] = {};
    static int coalMineResourceAmount[ROWS][COLS] = {};
    for (int row = 6; row <= 12; ++row) {
        for (int col = 10; col <= 17; ++col) {
            coalMineGrid[row][col] = TileType::Coal;
            coalMineDurability[row][col] = 2;
            coalMineResourceAmount[row][col] = 20;
        }
    }

    // WOODLAND — отдельная территория с конечными запасами.
    static TileType woodlandGrid[ROWS][COLS] = {};
    static int woodlandDurability[ROWS][COLS] = {};
    static int woodlandResourceAmount[ROWS][COLS] = {};
    for (int row = 3; row < 11; ++row) {
        for (int col = 3; col < 9; ++col) {
            woodlandGrid[row][col] = TileType::Tree;
            woodlandDurability[row][col] = 4;
            woodlandResourceAmount[row][col] = 1;
        }
    }
    for (int row = 12; row < 16; ++row) {
        for (int col = 25; col < 31; ++col) {
            woodlandGrid[row][col] = TileType::Boulder;
            woodlandDurability[row][col] = 5;
            woodlandResourceAmount[row][col] = 1;
        }
    }

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
        InventorySlot reader = {};
        int energy = 0;
        int completedActions = 0;
        int cardIndex = 0;
        float commandTimer = 0.0f;
        bool smelting = false;
        float smeltTimer = 0.0f;
        const char* status = "Insert card";
    };

    static FurnaceStorage workshopFurnaces[ROWS][COLS] = {};
    auto* furnaces = workshopFurnaces;

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

    static DrillStorage workshopDrills[ROWS][COLS] = {};
    auto* drills = workshopDrills;

    const int CHEST_SIZE = 5;
    struct ChestStorage {
        InventorySlot slots[CHEST_SIZE] = {};
    };
    static ChestStorage workshopChests[ROWS][COLS] = {};
    auto* chests = workshopChests;

    // ==========================================
    // IRON MINE — Данные механизмов шахты
    // ==========================================

    // Расположение крупных построек
    static CellPosition ironMineBuildingOrigins[ROWS][COLS] = {};

    // Буры и месторождения под ними
    static TileType ironMineDrillDeposits[ROWS][COLS] = {};
    static DrillStorage ironMineDrills[ROWS][COLS] = {};

    // Печи
    static FurnaceStorage ironMineFurnaces[ROWS][COLS] = {};

    // Сундуки
    static ChestStorage ironMineChests[ROWS][COLS] = {};

    // Независимые механизмы угольной шахты.
    static CellPosition coalMineBuildingOrigins[ROWS][COLS] = {};
    static TileType coalMineDrillDeposits[ROWS][COLS] = {};
    static DrillStorage coalMineDrills[ROWS][COLS] = {};
    static FurnaceStorage coalMineFurnaces[ROWS][COLS] = {};
    static ChestStorage coalMineChests[ROWS][COLS] = {};

    // Независимые механизмы лесной территории.
    static CellPosition woodlandBuildingOrigins[ROWS][COLS] = {};
    static TileType woodlandDrillDeposits[ROWS][COLS] = {};
    static DrillStorage woodlandDrills[ROWS][COLS] = {};
    static FurnaceStorage woodlandFurnaces[ROWS][COLS] = {};
    static ChestStorage woodlandChests[ROWS][COLS] = {};

    // CARGO STATION — клетки станции в обеих локациях
    for (int row = 14; row < 17; row++) {
        for (int col = 34; col < 37; col++) {
            grid[row][col] = TileType::CargoStation;
            ironMineGrid[row][col] = TileType::CargoStation;
            coalMineGrid[row][col] = TileType::CargoStation;
        }
    }

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

    // Объект, который игрок собирается разместить
    TileType selectedTile = TileType::Wall;
    bool placingFurnace = false;
    bool placingDrill = false;
    bool placingChest = false;
    
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
    CellPosition openedChest = {-1, -1};
    int selectedChestSlot = -1;

    bool developerPanelOpen = false;

    // ==========================================
    // LOCATION — Выбор активных массивов без копирования
    // ==========================================

    auto switchLocation = [&](Location destination) {

        // Если уже находимся в этой локации,
        // ничего переключать не нужно
        if (currentLocation == destination) {
            return;
        }

        const bool inMine = destination == Location::IronMine;
        const bool inCoal = destination == Location::CoalMine;
        const bool inWoodland = destination == Location::Woodland;

        // Карта и ресурсы
        grid = inWoodland ? woodlandGrid : inCoal ? coalMineGrid : (inMine ? ironMineGrid : workshopGrid);
        durability = inWoodland ? woodlandDurability : inCoal ? coalMineDurability : (inMine ? ironMineDurability : workshopDurability);
        resourceAmount = inWoodland ? woodlandResourceAmount : inCoal ? coalMineResourceAmount : (inMine ? ironMineResourceAmount : workshopResourceAmount);

        // Крупные постройки
        buildingOrigins = inWoodland ? woodlandBuildingOrigins : inCoal ? coalMineBuildingOrigins : (inMine ? ironMineBuildingOrigins : workshopBuildingOrigins);

        // Месторождения под бурами
        drillDeposits = inWoodland ? woodlandDrillDeposits : inCoal ? coalMineDrillDeposits : (inMine ? ironMineDrillDeposits : workshopDrillDeposits);

        // Производственные механизмы
        drills = inWoodland ? woodlandDrills : inCoal ? coalMineDrills : (inMine ? ironMineDrills : workshopDrills);
        furnaces = inWoodland ? woodlandFurnaces : inCoal ? coalMineFurnaces : (inMine ? ironMineFurnaces : workshopFurnaces);
        chests = inWoodland ? woodlandChests : inCoal ? coalMineChests : (inMine ? ironMineChests : workshopChests);

        // Запоминаем выбранную локацию
        currentLocation = destination;
    };

    // ==========================================
    // SIMULATION — Производство независимо от игрока
    // ==========================================
    // Обновляет только механизмы выбранной территории, без ввода и UI.
    auto updateMachines = [&](float dt) {
        // Плавка по установленной карте RUN.
        for (int row = 0; row < ROWS; row++) {
            for (int col = 0; col < COLS; col++) {
                if (grid[row][col] != TileType::Furnace ||
                    !isBuildingOrigin(col, row)) {
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
                    furnace.commandTimer += dt;
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
                furnace.smeltTimer -= dt;
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
                if (grid[row][col] != TileType::Drill ||
                    !isBuildingOrigin(col, row)) {
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

                CellPosition deposit = {-1, -1};
                ItemType resource = ItemType::Empty;
                if (instruction.command == 1) {
                    deposit = findDrillDeposit({col, row});
                    if (deposit.x == -1) {
                        drill.status = "DEPOSIT DEPLETED";
                        continue;
                    }
                    resource = getDepositItem(drillDeposits[deposit.y][deposit.x]);
                    if (drill.output.amount > 0 && drill.output.type != resource) {
                        drill.status = resource == ItemType::IronOre
                            ? "Clear output. Next resource: iron ore"
                            : "Clear output. Next resource: coal";
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
                    drill.commandTimer += dt;

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
                drill.hitTimer += dt;

                if (drill.hitTimer < drillHitDuration) {
                    continue;
                }

                drill.hitTimer = 0.0f;
                drill.energy--;
                drill.completedActions++;
                int& hitsLeft = durability[deposit.y][deposit.x];
                int& remaining = resourceAmount[deposit.y][deposit.x];
                hitsLeft--;

                if (hitsLeft <= 0) {
                    drill.output.type = resource;
                    drill.output.amount++;
                    remaining--;

                    if (remaining > 0) {
                        hitsLeft = resource == ItemType::IronOre ? 3 : 2;
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
            }
        }

    };

    auto updateAllLocations = [&](float dt) {
        const Location playerLocation = currentLocation;
        // Каждая территория получает ровно один шаг симуляции.
        for (Location location : {Location::Workshop, Location::IronMine, Location::CoalMine, Location::Woodland}) {
            if (location == Location::Woodland && !woodlandUnlocked) continue;
            switchLocation(location);
            updateMachines(dt);
        }
        // До отрисовки и ввода возвращаем контекст игрока.
        switchLocation(playerLocation);
    };

    while (!WindowShouldClose()) {
        
        // Переключение между локациями
        // Возвращение из карты мира
        if (worldMapOpen && IsKeyPressed(KEY_ESCAPE)) {
            worldMapOpen = false;

            // Возвращаем игрока в центр территории
            x = 400;
            y = 200;
        }

        // ==========================================
        // CARGO DELIVERY — Доставка железа
        // ==========================================

        // Доставка продолжается при нахождении в любой локации.
        updateCargoDelivery(GetFrameTime());
        updateCoal(coalToWorkshop, workshopCargoCoal, GetFrameTime());
        updateCoal(coalToIronMine, ironMineCargoCoal, GetFrameTime());

        if (worldMapOpen && IsKeyPressed(KEY_A)) {
            autoCargoDelivery = !autoCargoDelivery;
        }

        if (worldMapOpen && IsKeyPressed(KEY_T)) {
            dispatchCargo(false);
        }

        if (worldMapOpen && IsKeyPressed(KEY_C)) {
            dispatchCoal(coalToWorkshop, workshopCargoCoal, false);
        }
        if (worldMapOpen && IsKeyPressed(KEY_I)) {
            dispatchCoal(coalToIronMine, ironMineCargoCoal, false);
        }

        // Автоматический рейс только при наличии полной партии и места.
        if (autoCargoDelivery) {
            dispatchCargo(true);
            dispatchAutoCoal();
        }

        // При смене территории сбрасываем только интерфейс игрока.
        // Сами механизмы и их программы остаются на своих местах.
        auto closeLocationUI = [&]() {
            inventoryOpen = false;
            puncherOpen = false;
            puncherSlot = -1;
            openedFurnaceX = openedFurnaceY = -1;
            openedDrillX = openedDrillY = -1;
            openedChest = {-1, -1};
            selectedChestSlot = -1;
            selectedSlot = -1;
            splitSource = -1;
            previewSlot = -1;
            previewOffset = 0;
            dismantleX = dismantleY = -1;
            dismantleTimer = 0.0f;
            hitAnimationTimer = 0.0f;
        };

        // Выбор локации на карте мира
        bool locationSelectedThisFrame = false;

        if (worldMapOpen && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {

            Vector2 mapMouse = GetMousePosition();

            Rectangle workshopButton = {12, 90, 185, 100};
            Rectangle mineButton = {209, 90, 185, 100};
            Rectangle coalButton = {406, 90, 185, 100};
            Rectangle woodlandButton = {603, 90, 185, 100};

            if (CheckCollisionPointRec(mapMouse, workshopButton)) {

                closeLocationUI();
                switchLocation(Location::Workshop);
                worldMapOpen = false;

                x = 400;
                y = 200;

                locationSelectedThisFrame = true;
            }
            else if (CheckCollisionPointRec(mapMouse, mineButton)) {

                closeLocationUI();
                switchLocation(Location::IronMine);
                worldMapOpen = false;
                x = 400;
                y = 200;
                locationSelectedThisFrame = true;
            }
            else if (CheckCollisionPointRec(mapMouse, coalButton)) {
                closeLocationUI();
                switchLocation(Location::CoalMine);
                worldMapOpen = false;
                x = 400;
                y = 200;
                locationSelectedThisFrame = true;
            }
            else if (CheckCollisionPointRec(mapMouse, woodlandButton)) {
                if (!woodlandUnlocked) {
                    buyWoodland();
                } else {
                    closeLocationUI();
                    switchLocation(Location::Woodland);
                    worldMapOpen = false;
                    x = 400;
                    y = 200;
                    locationSelectedThisFrame = true;
                }
            }
        }

        Rectangle wall = {500, 150, 100, 50};

        // Грузовая станция
        const bool hasCargoStation = currentLocation != Location::Woodland;
        Rectangle cargoStation = hasCargoStation ? Rectangle{680, 280, 60, 60} : Rectangle{-200, -200, 60, 60};

        // Проверяем, находится ли игрок рядом
        bool nearCargoStation = hasCargoStation && CheckCollisionPointCircle(
            {(float)x + 20, (float)y + 20},
            {710, 310},
            85.0f
        );

        Vector2 mouse = GetMousePosition();
        int cellX = (int)mouse.x / TILE_SIZE;
        int cellY = (int)mouse.y / TILE_SIZE;
        bool insideMap =
            cellX >= 0 && cellX < COLS &&
            cellY >= 0 && cellY < ROWS;
        bool canBuild = false;
        int command = 0;
        int argument = 0;
        int stoneCount = 0;
        int woodCount = 0;
        int plateCount = 0;
        Rectangle craftButton = {615, 132, 170, 24};
        Rectangle cardButton = {615, 192, 170, 24};
        Rectangle drillButton = {615, 252, 170, 24};
        Rectangle chestButton = {615, 281, 170, 25};
        Rectangle punchButton = {185, 375, 170, 30};
        int punchedCardSlot = -1;
        bool validPattern = false;
        bool hasBlank = false;
        bool canPunch = false;

        // Биржа доступна в мастерской. Модальное окно не пропускает
        // нажатия мыши и клавиш в игровую карту.
        bool registerToggled = false;
        if (registerOpen && (IsKeyPressed(KEY_K) || IsKeyPressed(KEY_ESCAPE))) {
            registerOpen = false;
            registerToggled = true;
        } else if (!registerOpen && !researchOpen && !shopOpen && !contractsOpen &&
                   technologies[3].unlocked && currentLocation == Location::Workshop &&
                   !worldMapOpen && !inventoryOpen && !puncherOpen && !developerPanelOpen &&
                   !locationSelectedThisFrame && !IsKeyPressed(KEY_F1) &&
                   IsKeyPressed(KEY_K)) {
            closeLocationUI();
            registerOpen = true;
            registerToggled = true;
        }
        if (registerOpen && !registerToggled && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (CheckCollisionPointRec(mouse, registerRunButton)) {
                if (registerRunning) { registerRunning = false; registerNotice = "Paused."; }
                else if (registerPC < registerReader.cards.size()) {
                    registerRunning = true;
                    registerTimer = 0;
                    registerNotice = "Running program...";
                } else registerNotice = "Insert a program or REWIND.";
            } else if (CheckCollisionPointRec(mouse, registerStepButton) && !registerRunning) {
                stepRegister();
            } else if (CheckCollisionPointRec(mouse, registerRewindButton)) {
                rewindRegister();
            } else if (CheckCollisionPointRec(mouse, registerResetButton) && !registerRunning) {
                registerValue = 0;
                registerNotice = "Register reset to zero.";
            } else if (CheckCollisionPointRec(mouse, registerEjectButton)) {
                ejectRegisterCard();
            } else if (!registerRunning) {
                for (int i = 0; i < INVENTORY_SIZE; ++i) {
                    if (CheckCollisionPointRec(mouse, registerInventoryRect(i))) {
                        if (!insertRegisterCard(i)) registerNotice = "Use a punched card and an empty reader.";
                        break;
                    }
                }
            }
        }
        bool researchToggled = false;
        if (researchOpen && (IsKeyPressed(KEY_L) || IsKeyPressed(KEY_ESCAPE))) {
            researchOpen = false;
            researchToggled = true;
        } else if (!researchOpen && !shopOpen && !contractsOpen && !registerOpen && !registerToggled &&
                   currentLocation == Location::Workshop && !worldMapOpen &&
                   !inventoryOpen && !puncherOpen && !developerPanelOpen &&
                   !locationSelectedThisFrame &&
                   !IsKeyPressed(KEY_F1) && IsKeyPressed(KEY_L)) {
            closeLocationUI();
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
        } else if (!shopOpen && !contractsOpen && !researchOpen && !researchToggled && !registerOpen && !registerToggled && currentLocation == Location::Workshop &&
                   !worldMapOpen && !inventoryOpen && !puncherOpen &&
                   !developerPanelOpen &&
                   !locationSelectedThisFrame && !IsKeyPressed(KEY_F1) &&
                   IsKeyPressed(KEY_S)) {
            closeLocationUI();
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
        } else if (!contractsOpen && !shopOpen && !shopToggled && !researchOpen && !researchToggled && !registerOpen && !registerToggled && currentLocation == Location::Workshop &&
                   !worldMapOpen && !inventoryOpen && !puncherOpen &&
                   !developerPanelOpen &&
                   !locationSelectedThisFrame && !IsKeyPressed(KEY_F1) &&
                   IsKeyPressed(KEY_B)) {
            closeLocationUI();
            contractsOpen = true;
            contractToggled = true;
        }
        bool panelToggled = !contractsOpen && !contractToggled && !shopOpen && !shopToggled && !researchOpen && !researchToggled && !registerOpen && !registerToggled && IsKeyPressed(KEY_F1);
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
        if (!developerPanelOpen && !panelToggled) refreshContracts(GetFrameTime());
        if (panelToggled) {
            developerPanelOpen = !developerPanelOpen;
            if (developerPanelOpen) {
                inventoryOpen = false;
                puncherOpen = false;
                puncherSlot = -1;
                openedFurnaceX = -1;
                openedFurnaceY = -1;
                openedDrillX = -1;
                openedDrillY = -1;
                openedChest = {-1, -1};
                selectedChestSlot = -1;
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
            for (int i = 0; i < (int)(sizeof(developerItems) / sizeof(developerItems[0])); i++) {
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
        if (!developerPanelOpen && !panelToggled &&
            !worldMapOpen && !locationSelectedThisFrame &&
            !contractsOpen && !contractToggled && !shopOpen && !shopToggled &&
            !researchOpen && !researchToggled && !registerOpen && !registerToggled) {

            // ==========================================
            // CARGO — Ручная работа со складами рядом со станцией
            // ==========================================
            if (nearCargoStation && !inventoryOpen) {
                auto loadStation = [&](ItemType type, int& stock, int capacity) {
                    int remaining = std::min(10, capacity - stock);
                    for (int i = 0; i < INVENTORY_SIZE && remaining > 0; ++i) {
                        if (inventory[i].type != type || inventory[i].amount <= 0) continue;
                        int amount = std::min(remaining, inventory[i].amount);
                        inventory[i].amount -= amount;
                        stock += amount;
                        remaining -= amount;
                        if (inventory[i].amount == 0) inventory[i] = {};
                    }
                };
                auto takeStation = [&](ItemType type, int& stock) {
                    int amount = std::min(10, stock);
                    if (amount <= 0) return;
                    int remaining = addToInventory(inventory, INVENTORY_SIZE, type, amount);
                    stock -= amount - remaining;
                    if (remaining > 0) inventoryFullTimer = 1.0f;
                };
                if (IsKeyPressed(KEY_T)) {
                    if (currentLocation == Location::IronMine)
                        loadStation(ItemType::IronOre, mineCargoIron, MINE_CARGO_CAPACITY);
                    else if (currentLocation == Location::CoalMine)
                        loadStation(ItemType::Coal, coalMineCargo, COAL_CARGO_CAPACITY);
                    else takeStation(ItemType::IronOre, workshopCargoIron);
                }
                if (IsKeyPressed(KEY_C)) {
                    if (currentLocation == Location::Workshop)
                        takeStation(ItemType::Coal, workshopCargoCoal);
                    else if (currentLocation == Location::IronMine)
                        takeStation(ItemType::Coal, ironMineCargoCoal);
                }
            }

            bool furnacePlacedThisFrame = false;
            bool drillPlacedThisFrame = false;
            bool chestPlacedThisFrame = false;
            bool chestOpenedThisFrame = false;

            int oldX = x;

            if (IsKeyDown(KEY_RIGHT)) x += speed;
            if (IsKeyDown(KEY_LEFT)) x -= speed;

            if (x < 0) x = 0;
            if (x > 760) x = 760;

            Rectangle player = {(float)x, (float)y, 40, 40};

            if (CheckCollisionRecs(player, wall) ||
                CheckCollisionRecs(player, cargoStation)) {
                x = oldX;
            }

            int oldY = y;

            if (IsKeyDown(KEY_UP)) y -= speed;
            if (IsKeyDown(KEY_DOWN)) y += speed;

            if (y < 0) y = 0;
            if (y > 410) y = 410;
            
            // Выход за границу любой локации
            bool leaving =
                (x == 0 && IsKeyDown(KEY_LEFT)) ||
                (x == 760 && IsKeyDown(KEY_RIGHT)) ||
                (y == 0 && IsKeyDown(KEY_UP)) ||
                (y == 410 && IsKeyDown(KEY_DOWN));

            if (leaving) {
                worldMapOpen = true;
            }

            player = {(float)x, (float)y, 40, 40};

            if (CheckCollisionRecs(player, wall) ||
                CheckCollisionRecs(player, cargoStation)) {
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

                placingChest = false;
            }

            // Клавиша 2 — шестерёнка
            if (IsKeyPressed(KEY_TWO)) {
                selectedTile = TileType::Gear;
                placingFurnace = false;
                placingDrill = false;

                placingChest = false;
            }

            // Клавиша 3 — генератор
            if (IsKeyPressed(KEY_THREE)) {
                selectedTile = TileType::Generator;
                placingFurnace = false;
                placingDrill = false;

                placingChest = false;
            }

            if (IsKeyPressed(KEY_ZERO)) {
                selectedTile = TileType::Empty;
                placingFurnace = false;
                placingDrill = false;

                placingChest = false;
            }

            // ==========================================
            // BUILDING — Проверка возможности строительства
            // ==========================================

            // Проверяем всю площадь выбранной постройки.
            if (insideMap) {
                const int buildSize = (placingFurnace || placingDrill || placingChest)
                    ? LARGE_BUILDING_SIZE : 1;
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
                    !CheckCollisionRecs(cell, player) &&
                    !CheckCollisionRecs(cell, wall) &&
                    !CheckCollisionRecs(cell, cargoStation) &&
                    !(currentLocation != Location::Workshop && placingFurnace);
                
                // ЛКМ — строим
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
                    canBuild &&
                    !inventoryOpen &&
                    !placingFurnace && !placingDrill && !placingChest &&
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
                            if (item.type == ItemType::PunchedCard) {
                                fits = false;
                                for (InventorySlot& slot : candidate) {
                                    if (slot.amount == 0) {
                                        slot = item;
                                        fits = true;
                                        break;
                                    }
                                }
                            }
                            else {
                                fits = addToInventory(
                                    candidate, INVENTORY_SIZE, item.type, item.amount
                                ) == 0;
                            }
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

            // ==========================================
            // MINING — Добыча рукой
            // ==========================================

            if (selectedTile == TileType::Empty &&
                !placingFurnace && !placingDrill && !placingChest &&
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

            for (int i = 0; i < 4; i++) {
                command = command * 2 + (commandHoles[i] ? 1 : 0);
                if (argumentHoles[i]) {
                    argument += 1 << i;
                }
            }

            validPattern =
                ((command == 1 || command == 2) && argument > 0) ||
                (technologies[3].unlocked && IsArithmeticCommand(command) &&
                 (command != DIV_COMMAND || argument != 0));

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
            if (inventoryOpen && !puncherOpen && !chestOpenedThisFrame) {
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

            if (inventoryOpen && !puncherOpen && !chestOpenedThisFrame &&
                IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                if (CheckCollisionPointRec(mouse, craftButton) && stoneCount >= 3) {
                    ingredients = {{ItemType::Stone, 3}};
                    order = {ItemType::Furnace, 2.0f};
                }
                else if (CheckCollisionPointRec(mouse, cardButton) && woodCount >= 1) {
                    ingredients = {{ItemType::Wood, 1}};
                    order = {ItemType::BlankCard, 1.0f, cardBatchSize};
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

        }

        // Карта мира не останавливает производство. F1 остаётся
        // отладочной паузой для механизмов в обеих локациях.
        if (!developerPanelOpen && !panelToggled) {
            updateAllLocations(GetFrameTime());
            updateRegister(GetFrameTime());
        }

        BeginDrawing();

        // Цвет земли зависит от текущей локации
        Color groundColor;

        if (currentLocation == Location::Workshop) {
            // Светлая земля мастерской
            groundColor = Color{218, 208, 190, 255};
        }
        else if (currentLocation == Location::Woodland) {
            groundColor = Color{164, 186, 139, 255};
        }
        else if (currentLocation == Location::CoalMine) {
            groundColor = Color{145, 143, 137, 255};
        }
        else {
            // Каменистая земля шахты
            groundColor = Color{175, 172, 165, 255};
        }

        ClearBackground(groundColor);
        
        for (int row = 0; row < ROWS; row++) {
            for (int col = 0; col < COLS; col++) {

                TileType tile = grid[row][col];
                // Станция рисуется отдельно.
                if (tile == TileType::CargoStation) {
                    continue;
                }

                const bool largeBuilding = isLargeBuilding(tile);
                if (largeBuilding && !isBuildingOrigin(col, row)) {
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
        
        DrawText(TextFormat("Score: %i", score), 10, 10, 20, BLACK);
        DrawText(TextFormat("Money: GBP %lld", money), 10, 60, 18, DARKGREEN);
        DrawText(TextFormat("XP: %lld", experiencePoints), 10, 82, 18, DARKBLUE);
        if (currentLocation == Location::Workshop) {
            DrawText("B - CONTRACTS | S - SHOP | L - RESEARCH", 10, 105, 16, DARKBLUE);
            if (technologies[3].unlocked) DrawText("K - MECHANICAL REGISTER", 10, 125, 16, DARKBLUE);
        }

        // Склад активной территории и краткий статус перевозок.
        if (!worldMapOpen && hasCargoStation) {
            DrawRectangle(540, 40, 250, 100, DARKGRAY);
            if (currentLocation == Location::CoalMine) {
                DrawText(TextFormat("COAL OUT: %i / %i", coalMineCargo, COAL_CARGO_CAPACITY), 550, 48, 16, WHITE);
                DrawText("T - LOAD COAL (1)", 550, 69, 15, YELLOW);
            } else {
                bool workshop = currentLocation == Location::Workshop;
                DrawText(TextFormat("IRON %s: %i / 200", workshop ? "IN" : "OUT", workshop ? workshopCargoIron : mineCargoIron), 550, 48, 16, WHITE);
                DrawText(TextFormat("COAL IN: %i / 200", workshop ? workshopCargoCoal : ironMineCargoCoal), 550, 69, 16, WHITE);
                DrawText(workshop ? "T - TAKE IRON | C - COAL" : "T - LOAD IRON | C - COAL", 550, 90, 14, YELLOW);
                DrawText("SLOT 1 IRON | SLOT 2 COAL", 550, 111, 14, LIGHTGRAY);
            }
            DrawRectangle(540, 143, 250, 42, DARKGRAY);
            DrawText(TextFormat("TRANSIT: Fe %i | C %i + %i", cargoInTransit, coalToWorkshop.inTransit, coalToIronMine.inTransit), 550, 149, 14, WHITE);
            DrawText(autoCargoDelivery ? "AUTO DELIVERY: ON" : "AUTO DELIVERY: OFF", 550, 166, 14, autoCargoDelivery ? GREEN : LIGHTGRAY);
        }
        const char* locationName = currentLocation == Location::Workshop ? "WORKSHOP" :
            (currentLocation == Location::IronMine ? "IRON MINE" :
            (currentLocation == Location::CoalMine ? "COAL MINE" : "WOODLAND"));

        DrawText(
            TextFormat("Location: %s", locationName),
            560, 10, 16, DARKBLUE
        );
        
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

        // ==========================================
        // CARGO STATION — Отрисовка
        // ==========================================

        if (hasCargoStation) {
        // Основание станции
        DrawRectangleRec(cargoStation, DARKBROWN);
        DrawRectangleLinesEx(cargoStation, 2, BLACK);

        // Грузовые ящики
        DrawRectangle(688, 290, 20, 20, BROWN);
        DrawRectangle(714, 290, 18, 20, BROWN);

        DrawRectangleLines(688, 290, 20, 20, BLACK);
        DrawRectangleLines(714, 290, 18, 20, BLACK);
        
        // Название станции
        DrawText("CARGO", 686, 318, 14, WHITE);

        // Подсказка, если игрок рядом
        if (nearCargoStation) {
            DrawText(currentLocation == Location::CoalMine ? "PRESS T" : "PRESS T / C", 640, 260, 16, DARKGREEN);
        }
        
        }
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

        if (!developerPanelOpen && insideMap && !inventoryOpen &&
                (placingFurnace || placingDrill || placingChest ||
             selectedTile != TileType::Empty)) {

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

            const int size = (placingFurnace || placingDrill || placingChest)
                ? LARGE_BUILDING_SIZE * TILE_SIZE : TILE_SIZE;
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
                    const char* label = FormatCardCommand(shownCard->command, shownCard->argument);
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

                    float progress = drill.hitTimer / drillHitDuration;
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
                    const char* label = FormatCardCommand(shownCard->command, shownCard->argument);
                    DrawText(label, 35, 375, 18, GOLD);
                }
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

            DrawText("Blank Card", 620, 156, 18, WHITE);
            DrawRectangleRec(cardButton, woodCount >= 1 ? GREEN : GRAY);
            DrawText(TextFormat("Craft Card x%i", cardBatchSize), 625, 196, 16, WHITE);
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

            DrawRectangleRec(chestButton, woodCount >= 3 ? GREEN : GRAY);
            DrawText(TextFormat("Chest  Wood %i/3", woodCount), 623, 286, 14, WHITE);

            if (!craftQueue.empty()) {
                const char* craftingName = "Furnace";
                if (craftQueue.front().result == ItemType::BlankCard) {
                    craftingName = "Blank Card";
                }
                else if (craftQueue.front().result == ItemType::Drill) {
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
                else if (item.type == ItemType::PunchedCard) {
                    DrawPunchedCard(
                        (int)mouse.x - 18, (int)mouse.y - 11,
                        item.cards.front().command, item.cards.front().argument
                    );
                }
                else if (item.type == ItemType::Chest) {
                    DrawChestItem((int)mouse.x - 17, (int)mouse.y - 12);
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
                            const std::string line = std::to_string(index + 1) + ". " +
                                FormatCardCommand(instruction.command, instruction.argument);
                            DrawText(
                                line.c_str(),
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
                    else if (inventory[i].type == ItemType::Chest) {
                        itemName = "Wooden Chest";
                    }

                    else if (inventory[i].type == ItemType::BlankCard) {
                        itemName = "Blank Card";
                    }
                    else if (inventory[i].type == ItemType::PunchedCard) {
                        const CardInstruction& card = inventory[i].cards.front();
                        itemName = FormatCardCommand(card.command, card.argument);
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
            if (technologies[3].unlocked) {
                DrawText("SET 0110  ADD 0111  SUB 1000  MUL 1001  DIV 1010", 185, 135, 14, SKYBLUE);
            }

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

            const char* preview = FormatCardCommand(command, argument);
            DrawText(preview, 185, 325, 24, validPattern ? GREEN : RED);
            const char* error = "";
            if (!hasBlank) {
                error = "NO BLANK CARD";
            }
            else if (command != 1 && command != 2 && !IsArithmeticCommand(command)) {
                error = "UNKNOWN COMMAND";
            }
            else if (IsArithmeticCommand(command) && !technologies[3].unlocked) {
                error = "RESEARCH MECHANICAL COMPUTING";
            }
            else if (command == DIV_COMMAND && argument == 0) {
                error = "DIVISION BY ZERO";
            }
            else if ((command == 1 || command == 2) && argument == 0) {
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

        if (developerPanelOpen) {
            DrawRectangle(
                0, 0, GetScreenWidth(), GetScreenHeight(),
                Fade(BLACK, 0.55f)
            );
            DrawRectangle(125, 55, 550, 340, DARKGRAY);
            DrawRectangleLines(125, 55, 550, 340, GOLD);
            DrawText("Developer Panel", 145, 70, 26, WHITE);
            DrawText("PAUSED | F1: Close", 145, 102, 16, GOLD);

            for (int i = 0; i < (int)(sizeof(developerItems) / sizeof(developerItems[0])); i++) {
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
        
        // Экран карты мира: три территории и три независимых рейса.
        if (worldMapOpen) {
            DrawRectangle(0, 0, 800, 450, DARKBROWN);
            DrawText("WORLD MAP", 300, 20, 32, WHITE);
            DrawText(TextFormat("GBP %lld", money), 610, 27, 19, GOLD);
            auto drawLocation = [&](int left, Color color, const char* name, const char* line1, const char* line2) {
                DrawRectangle(left, 90, 185, 100, color);
                DrawRectangleLines(left, 90, 185, 100, WHITE);
                DrawText(name, left + 12, 100, 20, WHITE);
                DrawText(line1, left + 12, 137, 16, WHITE);
                DrawText(line2, left + 12, 160, 16, WHITE);
            };
            const std::string workshopIronText = TextFormat("IRON IN: %i/200", workshopCargoIron);
            const std::string mineIronText = TextFormat("IRON OUT: %i/200", mineCargoIron);
            drawLocation(12, BROWN, "WORKSHOP", workshopIronText.c_str(), TextFormat("COAL IN: %i/200", workshopCargoCoal));
            drawLocation(209, GRAY, "IRON MINE", mineIronText.c_str(), TextFormat("COAL IN: %i/200", ironMineCargoCoal));
            drawLocation(406, DARKGRAY, "COAL MINE", TextFormat("COAL OUT: %i/200", coalMineCargo), "SLOT 1: LOAD COAL");
            drawLocation(603, woodlandUnlocked ? DARKGREEN : GRAY, "WOODLAND",
                woodlandUnlocked ? "WOOD AND STONE" : "LOCKED",
                woodlandUnlocked ? "CLICK TO TRAVEL" : "BUY ACCESS GBP 100");

            auto drawRoute = [&](int y, const char* name, int amount, float remaining, Color color) {
                DrawText(name, 30, y, 16, WHITE);
                const float progress = amount > 0 ? std::clamp(1.0f - remaining / CARGO_TRAVEL_TIME, 0.0f, 1.0f) : 0.0f;
                DrawRectangle(410, y + 3, 160, 12, GRAY);
                DrawRectangle(410, y + 3, (int)(160 * progress), 12, color);
                DrawRectangleLines(410, y + 3, 160, 12, WHITE);
                const char* status = amount == 0 ? "IDLE" : (remaining > 0 ? TextFormat("%i | %.1fs", amount, remaining) : "WAITING");
                DrawText(status, 585, y, 16, WHITE);
            };
            drawRoute(215, "T - IRON MINE > WORKSHOP", cargoInTransit, cargoTravelRemaining, GOLD);
            drawRoute(250, "C - COAL MINE > WORKSHOP", coalToWorkshop.inTransit, coalToWorkshop.remaining, LIGHTGRAY);
            drawRoute(285, "I - COAL MINE > IRON MINE", coalToIronMine.inTransit, coalToIronMine.remaining, LIGHTGRAY);
            DrawText(autoCargoDelivery ? "A - AUTO: ON (ALL ROUTES)" : "A - AUTO: OFF (ALL ROUTES)", 30, 335, 19, autoCargoDelivery ? GREEN : YELLOW);
            DrawText(TextFormat("%i units / 5 seconds per route", CARGO_BATCH_SIZE), 30, 365, 17, WHITE);
            DrawText(mapNotice.c_str(), 30, 399, 15, YELLOW);
            DrawText("ESC - RETURN", 590, 395, 20, WHITE);
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
                    DrawText("Needs Card manufacturing", 551, y + 53, 14, YELLOW);
            }
            DrawText(researchNotice.c_str(), 28, 390, 17, YELLOW);
            DrawText("L / ESC - CLOSE", 580, 420, 16, WHITE);
        }

        if (registerOpen) {
            DrawRectangle(0, 0, 800, 450, Color{43, 35, 29, 255});
            DrawText("MECHANICAL REGISTER", 28, 16, 27, GOLD);
            DrawText("8-bit unsigned | 0 - 255 | arithmetic modulo 256", 28, 51, 16, LIGHTGRAY);
            DrawRectangle(24, 81, 368, 155, DARKGRAY);
            DrawText(TextFormat("VALUE: %03i", registerValue), 38, 94, 32, WHITE);
            for (int bit = 7; bit >= 0; --bit) {
                int bx = 38 + (7 - bit) * 42;
                bool set = (registerValue & (1 << bit)) != 0;
                DrawRectangle(bx, 140, 34, 35, set ? GOLD : GRAY);
                DrawText(set ? "1" : "0", bx + 11, 147, 21, BLACK);
            }
            DrawText(TextFormat("Reader: %i / %i executed", (int)registerPC,
                (int)registerReader.cards.size()), 38, 183, 17, LIGHTGRAY);
            if (registerPC < registerReader.cards.size()) {
                const auto& next = registerReader.cards[registerPC];
                DrawText(TextFormat("Next: %s", FormatCardCommand(next.command, next.argument)), 38, 206, 17, GOLD);
            } else DrawText(registerReader.cards.empty() ? "Reader empty" : "End of program", 38, 206, 17, GOLD);
            auto drawRegisterButton = [](Rectangle r, const char* label, bool enabled) {
                DrawRectangleRec(r, enabled ? DARKBLUE : GRAY);
                DrawRectangleLinesEx(r, 1, LIGHTGRAY);
                DrawText(label, (int)r.x + 9, (int)r.y + 8, 16, WHITE);
            };
            drawRegisterButton(registerStepButton, "STEP", !registerRunning && registerPC < registerReader.cards.size());
            drawRegisterButton(registerRunButton, registerRunning ? "PAUSE" : "RUN", registerPC < registerReader.cards.size());
            drawRegisterButton(registerRewindButton, "REWIND", true);
            drawRegisterButton(registerResetButton, "ZERO REGISTER", !registerRunning);
            drawRegisterButton(registerEjectButton, "EJECT PROGRAM", !registerRunning && registerReader.amount > 0);
            DrawText("INVENTORY - click a punched card", 420, 72, 16, GOLD);
            for (int i = 0; i < INVENTORY_SIZE; ++i) {
                Rectangle r = registerInventoryRect(i);
                DrawRectangleRec(r, DARKGRAY);
                DrawRectangleLinesEx(r, 1, inventory[i].type == ItemType::PunchedCard ? GOLD : GRAY);
                drawStoredStack(inventory[i], (int)r.x, (int)r.y);
                if (inventory[i].type == ItemType::PunchedCard && inventory[i].cards.size() > 1)
                    DrawText(TextFormat("%i cards", (int)inventory[i].cards.size()), (int)r.x+3, (int)r.y+43, 12, GOLD);
            }
            DrawText(registerNotice.c_str(), 28, 375, 17, YELLOW);
            DrawText("K / ESC - CLOSE | Running programs continue in other locations", 28, 420, 16, LIGHTGRAY);
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
