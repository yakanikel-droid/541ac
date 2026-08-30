#include "Chat.h"
#include "CommandScript.h"
#include "Config.h"
#include "Creature.h"
#include "CreatureScript.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Random.h"
#include "ScriptMgr.h"
#include "ScriptedGossip.h"
#include "Spell.h"
#include "StringFormat.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

using namespace Acore::ChatCommands;

namespace ItemEnhancement
{
struct Orb
{
    uint8 MaxLevel = 0;
    uint32 MaxItemLevel = 0;
};

struct Location
{
    uint8 Bag = 0;
    uint8 Slot = 0;
    bool Set = false;
};

struct Session
{
    ObjectGuid NpcGuid;
    Location ItemLocation;
    Location MaterialLocation;
    Location TransferSource;
    Location TransferTarget;
};

struct Settings
{
    bool Enable = true;
    uint32 NpcEntry = 900000;
    uint8 MaxLevel = 10;
    uint32 EnchantIdBase = 50000;
    uint8 MaximumChance = 100;
    bool BlockHeirlooms = true;
    bool ScaleWeaponDamage = true;
    uint8 ProtectedLostLevels = 1;
    bool ConsumeProtectionOnFailureOnly = true;
    bool TransferEnable = true;
    uint32 TransferMaterialId = 70096;
    uint32 TransferMaterialCount = 1;
    std::map<uint8, float> Bonuses;
    std::map<uint8, float> Chances;
    std::map<uint8, float> GoldPerItemLevel;
    std::map<uint32, Orb> Orbs;
    std::map<uint32, float> ChanceItems;
    std::set<uint32> ProtectionItems;
    std::set<uint32> BlockedItems;
    std::set<uint32> AllowedItems;
    std::set<uint32> AllowedStats;
    std::set<uint32> AllowedSpellIds;
};

Settings Config;
std::unordered_map<ObjectGuid::LowType, Session> Sessions;

std::vector<std::string> Split(std::string value, char separator)
{
    std::vector<std::string> result;
    std::stringstream stream(value);
    std::string part;
    while (std::getline(stream, part, separator))
    {
        part.erase(std::remove_if(part.begin(), part.end(), ::isspace), part.end());
        if (!part.empty())
            result.push_back(part);
    }
    return result;
}

template <typename T>
T Number(std::string const& value, T fallback = {})
{
    std::stringstream stream(value);
    T result{};
    return stream >> result ? result : fallback;
}

std::map<uint8, float> ParseLevelFloats(std::string const& value)
{
    std::map<uint8, float> result;
    for (std::string const& row : Split(value, ','))
    {
        std::vector<std::string> fields = Split(row, ':');
        if (fields.size() == 2)
            result[uint8(Number<uint32>(fields[0]))] = Number<float>(fields[1]);
    }
    return result;
}

std::set<uint32> ParseIds(std::string const& value)
{
    std::set<uint32> result;
    for (std::string const& part : Split(value, ','))
        result.insert(Number<uint32>(part));
    result.erase(0);
    return result;
}

void LoadConfig()
{
    Config.Enable = sConfigMgr->GetOption<bool>("ItemEnhancement.Enable", true);
    Config.NpcEntry = sConfigMgr->GetOption<uint32>("ItemEnhancement.NpcEntry", 900000);
    Config.MaxLevel = sConfigMgr->GetOption<uint8>("ItemEnhancement.MaxLevel", 10);
    Config.EnchantIdBase = sConfigMgr->GetOption<uint32>("ItemEnhancement.EnchantIdBase", 50000);
    Config.MaximumChance = sConfigMgr->GetOption<uint8>("ItemEnhancement.MaximumSuccessChance", 100);
    Config.BlockHeirlooms = sConfigMgr->GetOption<bool>("ItemEnhancement.BlockHeirlooms", true);
    Config.ScaleWeaponDamage = sConfigMgr->GetOption<bool>("ItemEnhancement.ScaleWeaponDamage", true);
    Config.ProtectedLostLevels = sConfigMgr->GetOption<uint8>("ItemEnhancement.ProtectedFailureLostLevels", 1);
    Config.ConsumeProtectionOnFailureOnly =
        sConfigMgr->GetOption<bool>("ItemEnhancement.ConsumeProtectionOnFailureOnly", true);
    Config.TransferEnable = sConfigMgr->GetOption<bool>("ItemEnhancement.Transfer.Enable", true);
    Config.TransferMaterialId = sConfigMgr->GetOption<uint32>("ItemEnhancement.Transfer.MaterialItemId", 70096);
    Config.TransferMaterialCount = sConfigMgr->GetOption<uint32>("ItemEnhancement.Transfer.MaterialCount", 1);
    Config.Bonuses = ParseLevelFloats(sConfigMgr->GetOption<std::string>("ItemEnhancement.LevelBonuses", ""));
    Config.Chances = ParseLevelFloats(sConfigMgr->GetOption<std::string>("ItemEnhancement.SuccessChances", ""));
    Config.GoldPerItemLevel =
        ParseLevelFloats(sConfigMgr->GetOption<std::string>("ItemEnhancement.GoldPerItemLevel", ""));
    Config.BlockedItems = ParseIds(sConfigMgr->GetOption<std::string>("ItemEnhancement.BlockedItemIds", ""));
    Config.AllowedItems = ParseIds(sConfigMgr->GetOption<std::string>("ItemEnhancement.AllowedItemIds", ""));
    Config.ProtectionItems =
        ParseIds(sConfigMgr->GetOption<std::string>("ItemEnhancement.ProtectionItems", ""));
    Config.AllowedSpellIds =
        ParseIds(sConfigMgr->GetOption<std::string>("ItemEnhancement.AllowedSpellIds", ""));

    Config.Orbs.clear();
    for (std::string const& row : Split(sConfigMgr->GetOption<std::string>("ItemEnhancement.Orbs", ""), ','))
    {
        std::vector<std::string> fields = Split(row, ':');
        if (fields.size() == 3)
            Config.Orbs[Number<uint32>(fields[0])] = { uint8(Number<uint32>(fields[1])), Number<uint32>(fields[2]) };
    }

    Config.ChanceItems.clear();
    for (std::string const& row : Split(sConfigMgr->GetOption<std::string>("ItemEnhancement.ChanceItems", ""), ','))
    {
        std::vector<std::string> fields = Split(row, ':');
        if (fields.size() == 2)
            Config.ChanceItems[Number<uint32>(fields[0])] = Number<float>(fields[1]);
    }

    static std::map<std::string, uint32> const statNames =
    {
        { "STRENGTH", ITEM_MOD_STRENGTH }, { "AGILITY", ITEM_MOD_AGILITY },
        { "STAMINA", ITEM_MOD_STAMINA }, { "INTELLECT", ITEM_MOD_INTELLECT },
        { "SPIRIT", ITEM_MOD_SPIRIT }, { "ATTACK_POWER", ITEM_MOD_ATTACK_POWER },
        { "RANGED_ATTACK_POWER", ITEM_MOD_RANGED_ATTACK_POWER }, { "SPELL_POWER", ITEM_MOD_SPELL_POWER },
        { "HIT_RATING", ITEM_MOD_HIT_RATING }, { "CRIT_RATING", ITEM_MOD_CRIT_RATING },
        { "HASTE_RATING", ITEM_MOD_HASTE_RATING }, { "EXPERTISE_RATING", ITEM_MOD_EXPERTISE_RATING },
        { "ARMOR_PENETRATION_RATING", ITEM_MOD_ARMOR_PENETRATION_RATING },
        { "DEFENSE_RATING", ITEM_MOD_DEFENSE_SKILL_RATING }, { "DODGE_RATING", ITEM_MOD_DODGE_RATING },
        { "PARRY_RATING", ITEM_MOD_PARRY_RATING }, { "BLOCK_RATING", ITEM_MOD_BLOCK_RATING },
        { "RESILIENCE_RATING", ITEM_MOD_RESILIENCE_RATING }, { "BLOCK_VALUE", ITEM_MOD_BLOCK_VALUE },
        { "SPELL_PENETRATION", ITEM_MOD_SPELL_PENETRATION }, { "MANA_REGEN", ITEM_MOD_MANA_REGENERATION },
        { "HEALTH_REGEN", ITEM_MOD_HEALTH_REGEN }
    };
    Config.AllowedStats.clear();
    for (std::string const& name : Split(sConfigMgr->GetOption<std::string>("ItemEnhancement.AllowedStats", ""), ','))
        if (auto itr = statNames.find(name); itr != statNames.end())
            Config.AllowedStats.insert(itr->second);
}

uint8 GetLevel(Item const* item)
{
    if (!item)
        return 0;
    uint32 enchant = item->GetEnchantmentId(PRISMATIC_ENCHANTMENT_SLOT);
    if (enchant <= Config.EnchantIdBase || enchant > Config.EnchantIdBase + Config.MaxLevel)
        return 0;
    return uint8(enchant - Config.EnchantIdBase);
}

float GetBonus(uint8 level)
{
    auto itr = Config.Bonuses.find(level);
    return itr == Config.Bonuses.end() ? 0.0f : itr->second;
}

void SetLevel(Player* player, Item* item, uint8 level)
{
    player->ApplyEnchantment(item, PRISMATIC_ENCHANTMENT_SLOT, false);
    if (level)
        item->SetEnchantment(PRISMATIC_ENCHANTMENT_SLOT, Config.EnchantIdBase + level, 0, 0, player->GetGUID());
    else
        item->ClearEnchantment(PRISMATIC_ENCHANTMENT_SLOT);
    player->ApplyEnchantment(item, PRISMATIC_ENCHANTMENT_SLOT, true);
    item->SetState(ITEM_CHANGED, player);
}

void Send(Player* player, std::string const& message)
{
    ChatHandler(player->GetSession()).SendSysMessage(Acore::StringFormat("ENH\t{}", message), false);
}

Item* Resolve(Player* player, Location const& location)
{
    return location.Set ? player->GetItemByPos(location.Bag, location.Slot) : nullptr;
}

bool NearNpc(Player* player, Session const& session)
{
    Creature* npc = ObjectAccessor::GetCreature(*player, session.NpcGuid);
    return npc && npc->GetEntry() == Config.NpcEntry && player->IsWithinDistInMap(npc, 6.0f);
}

bool IsEligible(Item const* item, std::string& error)
{
    if (!item || !item->GetTemplate())
    {
        error = "Предмет не найден.";
        return false;
    }
    ItemTemplate const* proto = item->GetTemplate();
    if (Config.BlockHeirlooms && proto->Quality == ITEM_QUALITY_HEIRLOOM)
    {
        error = "Наследуемые предметы нельзя улучшать.";
        return false;
    }
    if (Config.BlockedItems.contains(item->GetEntry()))
    {
        error = "Этот предмет запрещено улучшать.";
        return false;
    }
    if (!Config.AllowedItems.empty() && !Config.AllowedItems.contains(item->GetEntry()))
    {
        error = "Этого предмета нет в списке разрешённых.";
        return false;
    }
    if (proto->Class != ITEM_CLASS_WEAPON && proto->Class != ITEM_CLASS_ARMOR)
    {
        error = "Можно улучшать только оружие и экипировку.";
        return false;
    }
    uint32 enchant = item->GetEnchantmentId(PRISMATIC_ENCHANTMENT_SLOT);
    if (enchant && GetLevel(item) == 0)
    {
        error =
            "Служебный слот предмета уже занят дополнительным сокетом или другим эффектом.";
        return false;
    }
    return true;
}

uint64 GetCost(ItemTemplate const* proto, uint8 targetLevel)
{
    auto itr = Config.GoldPerItemLevel.find(targetLevel);
    if (itr == Config.GoldPerItemLevel.end())
        return 0;
    return uint64(std::llround(double(proto->ItemLevel) * itr->second * GOLD));
}

void SendPreview(Player* player)
{
    Session& session = Sessions[player->GetGUID().GetCounter()];
    Item* item = Resolve(player, session.ItemLocation);
    Item* material = Resolve(player, session.MaterialLocation);
    if (!item)
    {
        Send(player, "PREVIEW\tEMPTY");
        return;
    }

    std::string error;
    if (!IsEligible(item, error))
    {
        Send(player, Acore::StringFormat("ERROR\t{}", error));
        return;
    }

    uint8 current = GetLevel(item);
    uint8 target = current + 1;
    if (target > Config.MaxLevel)
    {
        Send(player, "ERROR\tДостигнут максимальный уровень улучшения.");
        return;
    }

    float chance = Config.Chances[target];
    uint64 cost = GetCost(item->GetTemplate(), target);
    std::string materialType = "NONE";
    float materialValue = 0.0f;
    if (material)
    {
        if (auto itr = Config.Orbs.find(material->GetEntry()); itr != Config.Orbs.end())
        {
            materialType = "ORB";
            materialValue = itr->second.MaxItemLevel;
            chance = 100.0f;
            cost = 0;
        }
        else if (auto itr = Config.ChanceItems.find(material->GetEntry()); itr != Config.ChanceItems.end())
        {
            materialType = "CHANCE";
            materialValue = itr->second;
            chance = std::min<float>(Config.MaximumChance, chance + itr->second);
        }
        else if (Config.ProtectionItems.contains(material->GetEntry()))
            materialType = "PROTECTION";
        else
        {
            Send(player,
                "ERROR\tЭтот предмет не является материалом для улучшения.");
            return;
        }
    }

    Send(player, Acore::StringFormat("PREVIEW\t{}\t{}\t{}\t{}\t{:.2f}\t{}\t{:.2f}\t{:.2f}\t{}\t{:.2f}",
        item->GetEntry(), current, target, item->GetTemplate()->ItemLevel, chance, cost, GetBonus(current),
        GetBonus(target), materialType, materialValue));
}

void Consume(Player* player, Location& location)
{
    if (!location.Set)
        return;
    player->DestroyItem(location.Bag, location.Slot, true);
    location = {};
}

bool Attempt(Player* player)
{
    Session& session = Sessions[player->GetGUID().GetCounter()];
    if (!NearNpc(player, session))
    {
        Send(player, "ERROR\tПодойдите к мастеру улучшения.");
        return true;
    }
    Item* item = Resolve(player, session.ItemLocation);
    Item* material = Resolve(player, session.MaterialLocation);
    std::string error;
    if (!IsEligible(item, error))
    {
        Send(player, Acore::StringFormat("ERROR\t{}", error));
        return true;
    }
    uint8 current = GetLevel(item);
    uint8 target = current + 1;
    if (target > Config.MaxLevel)
    {
        Send(player, "ERROR\tДостигнут максимальный уровень улучшения.");
        return true;
    }

    float chance = Config.Chances[target];
    uint64 cost = GetCost(item->GetTemplate(), target);
    bool protection = false;
    if (material)
    {
        if (auto itr = Config.Orbs.find(material->GetEntry()); itr != Config.Orbs.end())
        {
            if (current >= itr->second.MaxLevel)
            {
                Send(player,
                    "ERROR\tРанг сферы недостаточен для следующего уровня.");
                return true;
            }
            if (itr->second.MaxItemLevel && item->GetTemplate()->ItemLevel > itr->second.MaxItemLevel)
            {
                Send(player, "ERROR\tItemLevel предмета превышает ограничение сферы.");
                return true;
            }
            chance = 100.0f;
            cost = 0;
        }
        else if (auto itr = Config.ChanceItems.find(material->GetEntry()); itr != Config.ChanceItems.end())
            chance = std::min<float>(Config.MaximumChance, chance + itr->second);
        else if (Config.ProtectionItems.contains(material->GetEntry()))
            protection = true;
        else
        {
            Send(player, "ERROR\tНедопустимый особый материал.");
            return true;
        }
    }

    if (player->GetMoney() < cost)
    {
        Send(player, "ERROR\tНедостаточно золота.");
        return true;
    }
    if (cost)
        player->ModifyMoney(-int64(cost));

    bool success = roll_chance_f(chance);
    bool consumeMaterial = material && (!protection || !Config.ConsumeProtectionOnFailureOnly || !success);
    if (success)
    {
        SetLevel(player, item, target);
        if (consumeMaterial)
            Consume(player, session.MaterialLocation);
        Send(player, Acore::StringFormat("RESULT\tSUCCESS\t{}\t{}", target, GetBonus(target)));
    }
    else
    {
        uint8 resultLevel = protection && current > Config.ProtectedLostLevels
            ? current - Config.ProtectedLostLevels : 0;
        SetLevel(player, item, resultLevel);
        if (consumeMaterial)
            Consume(player, session.MaterialLocation);
        Send(player, Acore::StringFormat("RESULT\tFAIL\t{}\t{}", current, resultLevel));
    }
    SendPreview(player);
    return true;
}

void SendTransferPreview(Player* player)
{
    Session& session = Sessions[player->GetGUID().GetCounter()];
    Item* source = Resolve(player, session.TransferSource);
    Item* target = Resolve(player, session.TransferTarget);
    Send(player, Acore::StringFormat("TRANSFER_PREVIEW\t{}\t{}\t{}\t{}\t{}",
        source ? source->GetEntry() : 0, source ? GetLevel(source) : 0, target ? target->GetEntry() : 0,
        Config.TransferMaterialId, Config.TransferMaterialCount));
}

bool Transfer(Player* player)
{
    Session& session = Sessions[player->GetGUID().GetCounter()];
    if (!Config.TransferEnable)
    {
        Send(player, "ERROR\tПеренос заточки отключён.");
        return true;
    }
    if (!NearNpc(player, session))
    {
        Send(player, "ERROR\tПодойдите к мастеру улучшения.");
        return true;
    }
    Item* source = Resolve(player, session.TransferSource);
    Item* target = Resolve(player, session.TransferTarget);
    std::string error;
    if (!IsEligible(source, error) || !IsEligible(target, error))
    {
        Send(player, Acore::StringFormat("ERROR\t{}", error));
        return true;
    }
    if (source == target)
    {
        Send(player, "ERROR\tВыберите два разных предмета.");
        return true;
    }
    uint8 level = GetLevel(source);
    if (!level)
    {
        Send(player, "ERROR\tИсходный предмет не имеет заточки.");
        return true;
    }
    if (GetLevel(target))
    {
        Send(player, "ERROR\tЦелевой предмет уже заточен.");
        return true;
    }
    if (!Config.TransferMaterialId || !Config.TransferMaterialCount ||
        !player->HasItemCount(Config.TransferMaterialId, Config.TransferMaterialCount))
    {
        Send(player, "ERROR\tНедостаточно ресурсов для переноса.");
        return true;
    }
    SetLevel(player, source, 0);
    SetLevel(player, target, level);
    player->DestroyItemCount(Config.TransferMaterialId, Config.TransferMaterialCount, true);
    Send(player, Acore::StringFormat("TRANSFER_RESULT\tSUCCESS\t{}", level));
    SendTransferPreview(player);
    return true;
}
} // namespace ItemEnhancement

class ItemEnhancementWorldScript : public WorldScript
{
public:
    ItemEnhancementWorldScript() : WorldScript("ItemEnhancementWorldScript") { }

    void OnAfterConfigLoad(bool /*reload*/) override
    {
        ItemEnhancement::LoadConfig();
    }
};

class ItemEnhancementNpc : public CreatureScript
{
public:
    ItemEnhancementNpc() : CreatureScript("npc_item_enhancement") { }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        if (!ItemEnhancement::Config.Enable || creature->GetEntry() != ItemEnhancement::Config.NpcEntry)
            return false;
        ItemEnhancement::Session& session = ItemEnhancement::Sessions[player->GetGUID().GetCounter()];
        session = {};
        session.NpcGuid = creature->GetGUID();
        ItemEnhancement::Send(player, "OPEN");
        CloseGossipMenuFor(player);
        return true;
    }
};

class ItemEnhancementPlayerScript : public PlayerScript
{
public:
    ItemEnhancementPlayerScript() : PlayerScript("ItemEnhancementPlayerScript") { }

    void OnPlayerLogout(Player* player) override
    {
        ItemEnhancement::Sessions.erase(player->GetGUID().GetCounter());
    }

    void OnPlayerApplyItemModsBefore(Player* player, uint8 slot, bool /*apply*/, uint8 /*statNumber*/,
        uint32 statType, int32& value) override
    {
        if (!ItemEnhancement::Config.Enable || !ItemEnhancement::Config.AllowedStats.contains(statType))
            return;
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        uint8 level = ItemEnhancement::GetLevel(item);
        if (!level)
            return;
        value += int32(std::lround(value * ItemEnhancement::GetBonus(level) / 100.0f));
    }

    void OnPlayerApplyWeaponDamage(Player* player, uint8 slot, ItemTemplate const* /*proto*/, float& minDamage,
        float& maxDamage, uint8 /*damageIndex*/) override
    {
        if (!ItemEnhancement::Config.Enable || !ItemEnhancement::Config.ScaleWeaponDamage)
            return;
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        uint8 level = ItemEnhancement::GetLevel(item);
        if (!level)
            return;
        float multiplier = 1.0f + ItemEnhancement::GetBonus(level) / 100.0f;
        minDamage *= multiplier;
        maxDamage *= multiplier;
    }
};

class ItemEnhancementCommandScript : public CommandScript
{
public:
    ItemEnhancementCommandScript() : CommandScript("ItemEnhancementCommandScript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable subcommands =
        {
            { "item", HandleItem, SEC_PLAYER, Console::No },
            { "material", HandleMaterial, SEC_PLAYER, Console::No },
            { "clear", HandleClear, SEC_PLAYER, Console::No },
            { "attempt", HandleAttempt, SEC_PLAYER, Console::No },
            { "transfer_source", HandleTransferSource, SEC_PLAYER, Console::No },
            { "transfer_target", HandleTransferTarget, SEC_PLAYER, Console::No },
            { "transfer", HandleTransfer, SEC_PLAYER, Console::No }
        };
        static ChatCommandTable commands = { { "enhanceui", subcommands } };
        return commands;
    }

    static bool HandleItem(ChatHandler* handler, uint8 bag, uint8 slot)
    {
        Player* player = handler->GetPlayer();
        if (bag == INVENTORY_SLOT_BAG_0 && slot < EQUIPMENT_SLOT_END)
        {
            ItemEnhancement::Send(player, "ERROR\tПеред улучшением снимите предмет.");
            return true;
        }
        ItemEnhancement::Session& session = ItemEnhancement::Sessions[player->GetGUID().GetCounter()];
        if (!ItemEnhancement::NearNpc(player, session))
            return true;
        session.ItemLocation = { bag, slot, true };
        ItemEnhancement::SendPreview(player);
        return true;
    }

    static bool HandleMaterial(ChatHandler* handler, uint8 bag, uint8 slot)
    {
        Player* player = handler->GetPlayer();
        ItemEnhancement::Session& session = ItemEnhancement::Sessions[player->GetGUID().GetCounter()];
        if (!ItemEnhancement::NearNpc(player, session))
            return true;
        session.MaterialLocation = { bag, slot, true };
        ItemEnhancement::SendPreview(player);
        return true;
    }

    static bool HandleClear(ChatHandler* handler, std::string const& target)
    {
        Player* player = handler->GetPlayer();
        ItemEnhancement::Session& session = ItemEnhancement::Sessions[player->GetGUID().GetCounter()];
        if (target == "item")
            session.ItemLocation = {};
        else if (target == "material")
            session.MaterialLocation = {};
        ItemEnhancement::SendPreview(player);
        return true;
    }

    static bool HandleAttempt(ChatHandler* handler)
    {
        return ItemEnhancement::Attempt(handler->GetPlayer());
    }

    static bool HandleTransferSource(ChatHandler* handler, uint8 bag, uint8 slot)
    {
        Player* player = handler->GetPlayer();
        if (bag == INVENTORY_SLOT_BAG_0 && slot < EQUIPMENT_SLOT_END)
        {
            ItemEnhancement::Send(player, "ERROR\tПеред переносом снимите предмет.");
            return true;
        }
        ItemEnhancement::Session& session = ItemEnhancement::Sessions[player->GetGUID().GetCounter()];
        if (!ItemEnhancement::NearNpc(player, session))
            return true;
        session.TransferSource = { bag, slot, true };
        ItemEnhancement::SendTransferPreview(player);
        return true;
    }

    static bool HandleTransferTarget(ChatHandler* handler, uint8 bag, uint8 slot)
    {
        Player* player = handler->GetPlayer();
        if (bag == INVENTORY_SLOT_BAG_0 && slot < EQUIPMENT_SLOT_END)
        {
            ItemEnhancement::Send(player, "ERROR\tПеред переносом снимите предмет.");
            return true;
        }
        ItemEnhancement::Session& session = ItemEnhancement::Sessions[player->GetGUID().GetCounter()];
        if (!ItemEnhancement::NearNpc(player, session))
            return true;
        session.TransferTarget = { bag, slot, true };
        ItemEnhancement::SendTransferPreview(player);
        return true;
    }

    static bool HandleTransfer(ChatHandler* handler)
    {
        return ItemEnhancement::Transfer(handler->GetPlayer());
    }
};

class ItemEnhancementSpellScript : public AllSpellScript
{
public:
    ItemEnhancementSpellScript() : AllSpellScript("ItemEnhancementSpellScript", { ALLSPELLHOOK_ON_PREPARE }) { }

    void OnSpellPrepare(Spell* spell, Unit* /*caster*/, SpellInfo const* spellInfo) override
    {
        if (!ItemEnhancement::Config.Enable || !spell || !spellInfo ||
            !ItemEnhancement::Config.AllowedSpellIds.contains(spellInfo->Id))
            return;
        Item* castItem = spell->m_CastItem;
        uint8 level = ItemEnhancement::GetLevel(castItem);
        if (!level)
            return;
        float multiplier = 1.0f + ItemEnhancement::GetBonus(level) / 100.0f;
        SpellValue* values = const_cast<SpellValue*>(spell->GetSpellValue());
        for (uint8 effect = 0; effect < MAX_SPELL_EFFECTS; ++effect)
            values->EffectBasePoints[effect] = int32(std::lround(values->EffectBasePoints[effect] * multiplier));
    }
};

void AddItemEnhancementScripts()
{
    new ItemEnhancementWorldScript();
    new ItemEnhancementNpc();
    new ItemEnhancementPlayerScript();
    new ItemEnhancementCommandScript();
    new ItemEnhancementSpellScript();
}
