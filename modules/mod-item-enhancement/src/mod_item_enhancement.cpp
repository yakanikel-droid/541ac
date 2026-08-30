#include "Chat.h"
#include "CommandScript.h"
#include "Config.h"
#include "Creature.h"
#include "CreatureScript.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "GameTime.h"
#include "Mail.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Random.h"
#include "ScriptMgr.h"
#include "ScriptedGossip.h"
#include "Spell.h"
#include "StringFormat.h"
#include "TradeData.h"

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

struct Selection
{
    ObjectGuid Guid;
};

struct Session
{
    ObjectGuid NpcGuid;
    Selection Item;
    Selection Material;
    Selection TransferSource;
    Selection TransferTarget;
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
std::unordered_map<ObjectGuid, Session> Sessions;

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
    Config.MaxLevel = uint8(std::clamp<uint32>(
        sConfigMgr->GetOption<uint32>("ItemEnhancement.MaxLevel", 10), 1, 10));
    Config.EnchantIdBase = sConfigMgr->GetOption<uint32>("ItemEnhancement.EnchantIdBase", 50000);
    if (Config.EnchantIdBase != 50000)
        Config.EnchantIdBase = 50000;
    Config.MaximumChance = uint8(std::min<uint32>(
        sConfigMgr->GetOption<uint32>("ItemEnhancement.MaximumSuccessChance", 100), 100));
    Config.BlockHeirlooms = sConfigMgr->GetOption<bool>("ItemEnhancement.BlockHeirlooms", true);
    Config.ScaleWeaponDamage = sConfigMgr->GetOption<bool>("ItemEnhancement.ScaleWeaponDamage", true);
    Config.ProtectedLostLevels = uint8(std::min<uint32>(sConfigMgr->GetOption<uint32>(
        "ItemEnhancement.ProtectedFailureLostLevels", 1), Config.MaxLevel));
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
        { "MANA", ITEM_MOD_MANA }, { "HEALTH", ITEM_MOD_HEALTH },
        { "STRENGTH", ITEM_MOD_STRENGTH }, { "AGILITY", ITEM_MOD_AGILITY },
        { "STAMINA", ITEM_MOD_STAMINA }, { "INTELLECT", ITEM_MOD_INTELLECT },
        { "SPIRIT", ITEM_MOD_SPIRIT }, { "ATTACK_POWER", ITEM_MOD_ATTACK_POWER },
        { "RANGED_ATTACK_POWER", ITEM_MOD_RANGED_ATTACK_POWER }, { "SPELL_POWER", ITEM_MOD_SPELL_POWER },
        { "HIT_MELEE_RATING", ITEM_MOD_HIT_MELEE_RATING },
        { "HIT_RANGED_RATING", ITEM_MOD_HIT_RANGED_RATING },
        { "HIT_SPELL_RATING", ITEM_MOD_HIT_SPELL_RATING },
        { "CRIT_MELEE_RATING", ITEM_MOD_CRIT_MELEE_RATING },
        { "CRIT_RANGED_RATING", ITEM_MOD_CRIT_RANGED_RATING },
        { "CRIT_SPELL_RATING", ITEM_MOD_CRIT_SPELL_RATING },
        { "HIT_TAKEN_MELEE_RATING", ITEM_MOD_HIT_TAKEN_MELEE_RATING },
        { "HIT_TAKEN_RANGED_RATING", ITEM_MOD_HIT_TAKEN_RANGED_RATING },
        { "HIT_TAKEN_SPELL_RATING", ITEM_MOD_HIT_TAKEN_SPELL_RATING },
        { "CRIT_TAKEN_MELEE_RATING", ITEM_MOD_CRIT_TAKEN_MELEE_RATING },
        { "CRIT_TAKEN_RANGED_RATING", ITEM_MOD_CRIT_TAKEN_RANGED_RATING },
        { "CRIT_TAKEN_SPELL_RATING", ITEM_MOD_CRIT_TAKEN_SPELL_RATING },
        { "HASTE_MELEE_RATING", ITEM_MOD_HASTE_MELEE_RATING },
        { "HASTE_RANGED_RATING", ITEM_MOD_HASTE_RANGED_RATING },
        { "HASTE_SPELL_RATING", ITEM_MOD_HASTE_SPELL_RATING },
        { "HIT_RATING", ITEM_MOD_HIT_RATING }, { "CRIT_RATING", ITEM_MOD_CRIT_RATING },
        { "HIT_TAKEN_RATING", ITEM_MOD_HIT_TAKEN_RATING },
        { "CRIT_TAKEN_RATING", ITEM_MOD_CRIT_TAKEN_RATING },
        { "HASTE_RATING", ITEM_MOD_HASTE_RATING }, { "EXPERTISE_RATING", ITEM_MOD_EXPERTISE_RATING },
        { "ARMOR_PENETRATION_RATING", ITEM_MOD_ARMOR_PENETRATION_RATING },
        { "DEFENSE_SKILL_RATING", ITEM_MOD_DEFENSE_SKILL_RATING },
        { "DEFENSE_RATING", ITEM_MOD_DEFENSE_SKILL_RATING }, { "DODGE_RATING", ITEM_MOD_DODGE_RATING },
        { "PARRY_RATING", ITEM_MOD_PARRY_RATING }, { "BLOCK_RATING", ITEM_MOD_BLOCK_RATING },
        { "RESILIENCE_RATING", ITEM_MOD_RESILIENCE_RATING }, { "BLOCK_VALUE", ITEM_MOD_BLOCK_VALUE },
        { "SPELL_PENETRATION", ITEM_MOD_SPELL_PENETRATION }, { "MANA_REGEN", ITEM_MOD_MANA_REGENERATION },
        { "HEALTH_REGEN", ITEM_MOD_HEALTH_REGEN },
        { "SPELL_HEALING_DONE", ITEM_MOD_SPELL_HEALING_DONE },
        { "SPELL_DAMAGE_DONE", ITEM_MOD_SPELL_DAMAGE_DONE }
    };
    Config.AllowedStats.clear();
    for (std::string const& name : Split(sConfigMgr->GetOption<std::string>("ItemEnhancement.AllowedStats", ""), ','))
        if (auto itr = statNames.find(name); itr != statNames.end())
            Config.AllowedStats.insert(itr->second);

    // The supplied client DBC and the reserved enchantment range contain +1..+10.
    for (auto& [level, chance] : Config.Chances)
        chance = std::isfinite(chance) ? std::clamp(chance, 0.0f, float(Config.MaximumChance)) : 0.0f;
    for (auto& [level, bonus] : Config.Bonuses)
        bonus = std::isfinite(bonus) ? std::max(bonus, 0.0f) : 0.0f;
    for (auto& [level, gold] : Config.GoldPerItemLevel)
        gold = std::isfinite(gold) ? std::max(gold, 0.0f) : 0.0f;
    for (auto& [entry, chance] : Config.ChanceItems)
        chance = std::isfinite(chance) ? std::clamp(chance, 0.0f, 100.0f) : 0.0f;
    for (auto& [entry, orb] : Config.Orbs)
        orb.MaxLevel = std::min<uint8>(orb.MaxLevel, Config.MaxLevel);
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
    item->SendUpdateToPlayer(player);
}

void Send(Player* player, std::string const& message)
{
    ChatHandler(player->GetSession()).SendSysMessage(Acore::StringFormat("ENH\t{}", message), false);
}

bool IsCarriedUnequipped(Item const* item)
{
    if (!item || item->IsInTrade() || item->IsEquipped())
        return false;

    uint8 bag = item->GetBagSlot();
    uint8 slot = item->GetSlot();
    return (bag == INVENTORY_SLOT_BAG_0 && slot >= INVENTORY_SLOT_ITEM_START && slot < INVENTORY_SLOT_ITEM_END) ||
        (bag >= INVENTORY_SLOT_BAG_START && bag < INVENTORY_SLOT_BAG_END);
}

Item* Resolve(Player* player, Selection const& selection)
{
    if (selection.Guid.IsEmpty())
        return nullptr;
    Item* item = player->GetItemByGuid(selection.Guid);
    return IsCarriedUnequipped(item) ? item : nullptr;
}

bool ConvertClientPosition(uint8 clientBag, uint8 clientSlot, uint8& serverBag, uint8& serverSlot)
{
    if (!clientSlot)
        return false;

    if (clientBag == 0)
    {
        if (clientSlot > INVENTORY_SLOT_ITEM_END - INVENTORY_SLOT_ITEM_START)
            return false;
        serverBag = INVENTORY_SLOT_BAG_0;
        serverSlot = INVENTORY_SLOT_ITEM_START + clientSlot - 1;
        return true;
    }

    if (clientBag >= 1 && clientBag <= INVENTORY_SLOT_BAG_END - INVENTORY_SLOT_BAG_START)
    {
        serverBag = INVENTORY_SLOT_BAG_START + clientBag - 1;
        serverSlot = clientSlot - 1;
        return true;
    }
    return false;
}

bool SelectClientItem(Player* player, uint8 clientBag, uint8 clientSlot, Selection& selection,
    std::string& error)
{
    // Never retain an older valid selection after a rejected client request.
    selection = {};
    uint8 serverBag = 0;
    uint8 serverSlot = 0;
    if (!ConvertClientPosition(clientBag, clientSlot, serverBag, serverSlot))
    {
        error = "Недопустимый слот инвентаря.";
        return false;
    }

    Item* item = player->GetItemByPos(serverBag, serverSlot);
    if (!IsCarriedUnequipped(item))
    {
        error = "Предмет не найден в рюкзаке или сумках.";
        return false;
    }

    selection.Guid = item->GetGUID();
    return true;
}

bool NearNpc(Player* player, Session const& session)
{
    Creature* npc = ObjectAccessor::GetCreature(*player, session.NpcGuid);
    return Config.Enable && npc && npc->GetEntry() == Config.NpcEntry && npc->IsAlive() &&
        player->IsWithinDistInMap(npc, 6.0f);
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
        error = "Служебный слот предмета уже занят "
            "дополнительным сокетом "
            "или другим эффектом.";
        return false;
    }
    return true;
}

uint64 GetCost(ItemTemplate const* proto, uint8 targetLevel)
{
    auto itr = Config.GoldPerItemLevel.find(targetLevel);
    if (itr == Config.GoldPerItemLevel.end())
        return 0;
    double rawCost = double(proto->ItemLevel) * double(itr->second) * static_cast<double>(GOLD);
    if (!std::isfinite(rawCost) || rawCost > double(MAX_MONEY_AMOUNT))
        return uint64(MAX_MONEY_AMOUNT) + 1;
    if (rawCost <= 0.0)
        return 0;
    return uint64(std::llround(rawCost));
}

std::string BuildStatPreview(ItemTemplate const* proto, uint8 level)
{
    std::ostringstream result;
    float bonus = GetBonus(level);
    bool first = true;
    for (uint32 index = 0; index < std::min<uint32>(proto->StatsCount, MAX_ITEM_PROTO_STATS); ++index)
    {
        uint32 type = proto->ItemStat[index].ItemStatType;
        int32 baseValue = proto->ItemStat[index].ItemStatValue;
        if (!Config.AllowedStats.contains(type) || !baseValue)
            continue;

        int32 enhancedValue = baseValue + int32(std::lround(baseValue * bonus / 100.0f));
        if (!first)
            result << ';';
        result << type << ':' << baseValue << ':' << enhancedValue;
        first = false;
    }
    return result.str();
}

std::string BuildDamagePreview(ItemTemplate const* proto, uint8 level)
{
    if (!Config.ScaleWeaponDamage || proto->Class != ITEM_CLASS_WEAPON)
        return {};

    float multiplier = 1.0f + GetBonus(level) / 100.0f;
    std::ostringstream result;
    for (uint32 index = 0; index < MAX_ITEM_PROTO_DAMAGES; ++index)
    {
        if (proto->Damage[index].DamageMax <= 0.0f)
            continue;
        if (result.tellp() > 0)
            result << ';';
        result << index << ':' << proto->Damage[index].DamageMin << ':' << proto->Damage[index].DamageMax << ':'
            << proto->Damage[index].DamageMin * multiplier << ':' << proto->Damage[index].DamageMax * multiplier;
    }
    return result.str();
}

void SendInspect(Player* player, char const* kind, uint8 first, uint8 second, Item* item)
{
    uint8 level = Config.Enable ? GetLevel(item) : 0;
    if (!item || !level)
    {
        Send(player, Acore::StringFormat("INSPECT\t{}\t{}\t{}\t0\t0\t\t", kind, first, second));
        return;
    }

    Send(player, Acore::StringFormat("INSPECT\t{}\t{}\t{}\t{}\t{:.2f}\t{}\t{}", kind, first, second,
        level, GetBonus(level), BuildStatPreview(item->GetTemplate(), level),
        BuildDamagePreview(item->GetTemplate(), level)));
}

void SendPreview(Player* player)
{
    Session& session = Sessions[player->GetGUID()];
    Item* item = Resolve(player, session.Item);
    Item* material = Resolve(player, session.Material);
    if (!item)
    {
        Send(player, "PREVIEW\tEMPTY");
        return;
    }
    if (!session.Material.Guid.IsEmpty() && !material)
    {
        Send(player, "ERROR\tВыбранный материал перемещён или недоступен.");
        return;
    }
    if (material == item)
    {
        Send(player,
            "ERROR\tПредмет и материал должны находиться "
            "в разных ячейках.");
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
            if (current >= itr->second.MaxLevel)
            {
                Send(player,
                    "ERROR\tРанг сферы недостаточен для следующего уровня.");
                return;
            }
            if (itr->second.MaxItemLevel && item->GetTemplate()->ItemLevel > itr->second.MaxItemLevel)
            {
                Send(player, "ERROR\tItemLevel предмета превышает ограничение сферы.");
                return;
            }
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

    Send(player, Acore::StringFormat(
        "PREVIEW\t{}\t{}\t{}\t{}\t{:.2f}\t{}\t{:.2f}\t{:.2f}\t{}\t{:.2f}\t{}\t{}\t{}\t{}",
        item->GetEntry(), current, target, item->GetTemplate()->ItemLevel, chance, cost, GetBonus(current),
        GetBonus(target), materialType, materialValue, BuildStatPreview(item->GetTemplate(), current),
        BuildStatPreview(item->GetTemplate(), target), BuildDamagePreview(item->GetTemplate(), current),
        BuildDamagePreview(item->GetTemplate(), target)));
}

void ConsumeOne(Player* player, Selection& selection)
{
    Item* item = Resolve(player, selection);
    if (!item)
    {
        selection = {};
        return;
    }
    uint32 count = 1;
    player->DestroyItemCount(item, count, true);
    selection = {};
}

bool Attempt(Player* player)
{
    Session& session = Sessions[player->GetGUID()];
    if (!NearNpc(player, session))
    {
        Send(player, "ERROR\tПодойдите к мастеру улучшения.");
        return true;
    }
    Item* item = Resolve(player, session.Item);
    Item* material = Resolve(player, session.Material);
    std::string error;
    if (!IsEligible(item, error))
    {
        Send(player, Acore::StringFormat("ERROR\t{}", error));
        return true;
    }
    if (!session.Material.Guid.IsEmpty() && !material)
    {
        Send(player, "ERROR\tВыбранный материал перемещён или недоступен.");
        return true;
    }
    if (material == item)
    {
        Send(player,
            "ERROR\tПредмет и материал должны находиться "
            "в разных ячейках.");
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
            ConsumeOne(player, session.Material);
        Send(player, Acore::StringFormat("RESULT\tSUCCESS\t{}\t{}\t{}",
            target, GetBonus(target), consumeMaterial ? 1 : 0));
    }
    else
    {
        uint8 resultLevel = protection && current > Config.ProtectedLostLevels
            ? current - Config.ProtectedLostLevels : 0;
        SetLevel(player, item, resultLevel);
        if (consumeMaterial)
            ConsumeOne(player, session.Material);
        Send(player, Acore::StringFormat("RESULT\tFAIL\t{}\t{}\t{}",
            current, resultLevel, consumeMaterial ? 1 : 0));
    }
    SendPreview(player);
    return true;
}

void SendTransferPreview(Player* player)
{
    Session& session = Sessions[player->GetGUID()];
    Item* source = Resolve(player, session.TransferSource);
    Item* target = Resolve(player, session.TransferTarget);
    uint8 sourceLevel = source ? GetLevel(source) : 0;
    Send(player, Acore::StringFormat("TRANSFER_PREVIEW\t{}\t{}\t{}\t{}\t{}\t{:.2f}\t{}\t{}",
        source ? source->GetEntry() : 0, sourceLevel, target ? target->GetEntry() : 0,
        Config.TransferMaterialId, Config.TransferMaterialCount, GetBonus(sourceLevel),
        source && sourceLevel ? BuildStatPreview(source->GetTemplate(), sourceLevel) : std::string{},
        source && sourceLevel ? BuildDamagePreview(source->GetTemplate(), sourceLevel) : std::string{}));
}

bool Transfer(Player* player)
{
    Session& session = Sessions[player->GetGUID()];
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
    if (source->GetEntry() == Config.TransferMaterialId || target->GetEntry() == Config.TransferMaterialId)
    {
        Send(player,
            "ERROR\tРесурс переноса нельзя использовать как исходный "
            "или целевой предмет.");
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
    // Consume first, after every validation, so the resource operation cannot
    // accidentally remove either selected item.
    player->DestroyItemCount(Config.TransferMaterialId, Config.TransferMaterialCount, true);
    SetLevel(player, source, 0);
    SetLevel(player, target, level);
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
        ItemEnhancement::Session& session = ItemEnhancement::Sessions[player->GetGUID()];
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
        ItemEnhancement::Sessions.erase(player->GetGUID());
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
            { "transfer", HandleTransfer, SEC_PLAYER, Console::No },
            { "inspect_bag", HandleInspectBag, SEC_PLAYER, Console::No },
            { "inspect_inventory", HandleInspectInventory, SEC_PLAYER, Console::No },
            { "inspect_target", HandleInspectTarget, SEC_PLAYER, Console::No },
            { "inspect_trade", HandleInspectTrade, SEC_PLAYER, Console::No },
            { "inspect_mail", HandleInspectMail, SEC_PLAYER, Console::No },
            { "inspect_buyback", HandleInspectBuyback, SEC_PLAYER, Console::No },
            { "inspect_chat", HandleInspectChat, SEC_PLAYER, Console::No }
        };
        static ChatCommandTable commands = { { "enhanceui", subcommands } };
        return commands;
    }

    static bool HandleItem(ChatHandler* handler, uint8 bag, uint8 slot)
    {
        Player* player = handler->GetPlayer();
        ItemEnhancement::Session& session = ItemEnhancement::Sessions[player->GetGUID()];
        if (!ItemEnhancement::NearNpc(player, session))
            return true;
        std::string error;
        if (!ItemEnhancement::SelectClientItem(player, bag, slot, session.Item, error))
        {
            ItemEnhancement::Send(player, Acore::StringFormat("ERROR\t{}", error));
            return true;
        }
        ItemEnhancement::SendPreview(player);
        return true;
    }

    static bool HandleMaterial(ChatHandler* handler, uint8 bag, uint8 slot)
    {
        Player* player = handler->GetPlayer();
        ItemEnhancement::Session& session = ItemEnhancement::Sessions[player->GetGUID()];
        if (!ItemEnhancement::NearNpc(player, session))
            return true;
        std::string error;
        if (!ItemEnhancement::SelectClientItem(player, bag, slot, session.Material, error))
        {
            ItemEnhancement::Send(player, Acore::StringFormat("ERROR\t{}", error));
            return true;
        }
        ItemEnhancement::SendPreview(player);
        return true;
    }

    static bool HandleClear(ChatHandler* handler, std::string const& target)
    {
        Player* player = handler->GetPlayer();
        ItemEnhancement::Session& session = ItemEnhancement::Sessions[player->GetGUID()];
        if (!ItemEnhancement::NearNpc(player, session))
            return true;
        if (target == "item")
            session.Item = {};
        else if (target == "material")
            session.Material = {};
        else
        {
            ItemEnhancement::Send(player, "ERROR\tНеизвестная команда очистки.");
            return true;
        }
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
        ItemEnhancement::Session& session = ItemEnhancement::Sessions[player->GetGUID()];
        if (!ItemEnhancement::NearNpc(player, session))
            return true;
        if (bag == 255 && slot == 255)
        {
            session.TransferSource = {};
            ItemEnhancement::SendTransferPreview(player);
            return true;
        }
        std::string error;
        if (!ItemEnhancement::SelectClientItem(player, bag, slot, session.TransferSource, error))
        {
            ItemEnhancement::Send(player, Acore::StringFormat("ERROR\t{}", error));
            return true;
        }
        ItemEnhancement::SendTransferPreview(player);
        return true;
    }

    static bool HandleTransferTarget(ChatHandler* handler, uint8 bag, uint8 slot)
    {
        Player* player = handler->GetPlayer();
        ItemEnhancement::Session& session = ItemEnhancement::Sessions[player->GetGUID()];
        if (!ItemEnhancement::NearNpc(player, session))
            return true;
        if (bag == 255 && slot == 255)
        {
            session.TransferTarget = {};
            ItemEnhancement::SendTransferPreview(player);
            return true;
        }
        std::string error;
        if (!ItemEnhancement::SelectClientItem(player, bag, slot, session.TransferTarget, error))
        {
            ItemEnhancement::Send(player, Acore::StringFormat("ERROR\t{}", error));
            return true;
        }
        ItemEnhancement::SendTransferPreview(player);
        return true;
    }

    static bool HandleTransfer(ChatHandler* handler)
    {
        return ItemEnhancement::Transfer(handler->GetPlayer());
    }

    static bool HandleInspectBag(ChatHandler* handler, uint8 bag, uint8 slot)
    {
        Player* player = handler->GetPlayer();
        uint8 serverBag = 0;
        uint8 serverSlot = 0;
        Item* item = nullptr;
        if (ItemEnhancement::ConvertClientPosition(bag, slot, serverBag, serverSlot))
            item = player->GetItemByPos(serverBag, serverSlot);
        ItemEnhancement::SendInspect(player, "B", bag, slot, item);
        return true;
    }

    static bool HandleInspectInventory(ChatHandler* handler, uint8 slot)
    {
        Player* player = handler->GetPlayer();
        Item* item = nullptr;
        if (slot >= 1 && slot <= EQUIPMENT_SLOT_END)
            item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot - 1);
        ItemEnhancement::SendInspect(player, "I", slot, 0, item);
        return true;
    }

    static bool HandleInspectTarget(ChatHandler* handler, uint8 slot)
    {
        Player* player = handler->GetPlayer();
        Player* target = ObjectAccessor::GetPlayer(*player, player->GetTarget());
        Item* item = nullptr;
        if (target && slot >= 1 && slot <= EQUIPMENT_SLOT_END &&
            player->IsWithinDistInMap(target, INSPECT_DISTANCE, false) &&
            !player->IsValidAttackTarget(target))
            item = target->GetItemByPos(INVENTORY_SLOT_BAG_0, slot - 1);
        ItemEnhancement::SendInspect(player, "T", slot, 0, item);
        return true;
    }

    static bool HandleInspectTrade(ChatHandler* handler, uint8 side, uint8 slot)
    {
        Player* player = handler->GetPlayer();
        Item* item = nullptr;
        if (side <= 1 && slot >= 1 && slot <= TRADE_SLOT_COUNT)
        {
            if (TradeData* trade = player->GetTradeData())
            {
                TradeData* viewedTrade = side == 0 ? trade : trade->GetTraderData();
                if (viewedTrade)
                    item = viewedTrade->GetItem(TradeSlots(slot - 1));
            }
        }
        ItemEnhancement::SendInspect(player, "X", side, slot, item);
        return true;
    }

    static bool HandleInspectBuyback(ChatHandler* handler, uint8 slot)
    {
        Player* player = handler->GetPlayer();
        Item* item = nullptr;
        if (slot >= 1 && slot <= BUYBACK_SLOT_END - BUYBACK_SLOT_START)
            item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, BUYBACK_SLOT_START + slot - 1);
        ItemEnhancement::SendInspect(player, "Y", slot, 0, item);
        return true;
    }

    static bool HandleInspectMail(ChatHandler* handler, uint8 messageIndex, uint8 attachmentIndex)
    {
        Player* player = handler->GetPlayer();
        Item* item = nullptr;
        uint8 visibleIndex = 0;
        time_t now = GameTime::GetGameTime().count();

        if (messageIndex >= 1 && attachmentIndex >= 1 && attachmentIndex <= MAX_MAIL_ITEMS)
        {
            for (Mail const* mail : player->GetMails())
            {
                if (!mail || mail->state == MAIL_STATE_DELETED || now < mail->deliver_time || now > mail->expire_time)
                    continue;
                if (++visibleIndex != messageIndex)
                    continue;
                if (attachmentIndex <= mail->items.size())
                    item = player->GetMItem(mail->items[attachmentIndex - 1].item_guid);
                break;
            }
        }

        ItemEnhancement::SendInspect(player, "M", messageIndex, attachmentIndex, item);
        return true;
    }

    static bool HandleInspectChat(ChatHandler* handler, uint32 entry, uint8 level)
    {
        Player* player = handler->GetPlayer();
        ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(entry);
        if (!ItemEnhancement::Config.Enable || !itemTemplate || !level ||
            level > ItemEnhancement::Config.MaxLevel)
        {
            ItemEnhancement::Send(player, Acore::StringFormat("CHAT_INSPECT\t{}\t0\t0\t\t", entry));
            return true;
        }

        ItemEnhancement::Send(player, Acore::StringFormat("CHAT_INSPECT\t{}\t{}\t{:.2f}\t{}\t{}", entry,
            level, ItemEnhancement::GetBonus(level), ItemEnhancement::BuildStatPreview(itemTemplate, level),
            ItemEnhancement::BuildDamagePreview(itemTemplate, level)));
        return true;
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
