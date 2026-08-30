#include "AchievementMgr.h"
#include "Chat.h"
#include "CommandScript.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "DBCStores.h"
#include "Player.h"
#include "ScriptMgr.h"

#include <algorithm>
#include <array>
#include <limits>
#include <numeric>
#include <string>
#include <unordered_map>

using namespace Acore::ChatCommands;

namespace
{
enum StatIndex : uint8
{
    NOVA_STRENGTH,
    NOVA_AGILITY,
    NOVA_STAMINA,
    NOVA_INTELLECT,
    NOVA_SPIRIT,
    NOVA_SPELL_POWER,
    NOVA_ATTACK_POWER,
    NOVA_STAT_COUNT
};

using Allocations = std::array<uint32, NOVA_STAT_COUNT>;

bool IsEnabled()
{
    return sConfigMgr->GetOption<bool>("NovaEnhancement.Enable", true);
}

uint32 PointsPerPoint()
{
    return std::max<uint32>(1, sConfigMgr->GetOption<uint32>("NovaEnhancement.AchievementPointsPerPoint", 4));
}
uint32 ResetCostGold()
{
    return sConfigMgr->GetOption<uint32>(
        "NovaEnhancement.ResetCostGold",
        300
    );
}
uint32 AchievementPoints(Player* player)
{
    uint32 points = 0;
    for (auto const& completed : player->GetAchievementMgr()->GetCompletedAchievements())
        if (AchievementEntry const* achievement = sAchievementStore.LookupEntry(completed.first))
            points += achievement->points;
    return points;
}

uint32 EarnedPoints(Player* player)
{
    return AchievementPoints(player) / PointsPerPoint();
}

uint64 SpentPoints(Allocations const& values)
{
    return std::accumulate(values.begin(), values.end(), uint64(0));
}

uint32 EffectiveBonus(StatIndex stat, uint32 allocatedPoints)
{
    uint64 bonus = allocatedPoints;

    switch (stat)
    {
        case NOVA_SPELL_POWER:
            // AzerothCore stores spell power as an integer. 13 / 10 preserves
            // the requested +1.3 ratio without floating-point rounding drift.
            bonus = uint64(allocatedPoints) * 13 / 10;
            break;
        case NOVA_ATTACK_POWER:
            bonus = uint64(allocatedPoints) * 2;
            break;
        default:
            break;
    }

    return uint32(std::min<uint64>(bonus, std::numeric_limits<uint32>::max()));
}

Allocations LoadAllocations(Player* player)
{
    Allocations values{};
    QueryResult result = CharacterDatabase.Query(
        "SELECT `strength`,`agility`,`stamina`,`intellect`,`spirit`,`spell_power`,`attack_power` "
        "FROM `nova_character_enhancements` WHERE `guid`={}",
        player->GetGUID().GetCounter());

    if (!result)
    {
        CharacterDatabase.Execute(
            "INSERT IGNORE INTO `nova_character_enhancements` (`guid`) VALUES ({})",
            player->GetGUID().GetCounter());
        return values;
    }

    Field* fields = result->Fetch();
    for (uint8 i = 0; i < NOVA_STAT_COUNT; ++i)
        values[i] = fields[i].Get<uint32>();
    return values;
}

void SaveAllocations(Player* player, Allocations const& v)
{
    CharacterDatabase.Execute(
        "INSERT INTO `nova_character_enhancements` "
        "(`guid`,`strength`,`agility`,`stamina`,`intellect`,`spirit`,`spell_power`,`attack_power`) "
        "VALUES ({},{},{},{},{},{},{},{}) "
        "ON DUPLICATE KEY UPDATE `strength`=VALUES(`strength`),`agility`=VALUES(`agility`),"
        "`stamina`=VALUES(`stamina`),`intellect`=VALUES(`intellect`),`spirit`=VALUES(`spirit`),"
        "`spell_power`=VALUES(`spell_power`),`attack_power`=VALUES(`attack_power`)",
        player->GetGUID().GetCounter(), v[0], v[1], v[2], v[3], v[4], v[5], v[6]);
}

void ModifyAllocations(Player* player, Allocations const& v, bool apply)
{
    player->HandleStatFlatModifier(UNIT_MOD_STAT_STRENGTH, TOTAL_VALUE, float(EffectiveBonus(NOVA_STRENGTH, v[NOVA_STRENGTH])), apply);
    player->HandleStatFlatModifier(UNIT_MOD_STAT_AGILITY, TOTAL_VALUE, float(EffectiveBonus(NOVA_AGILITY, v[NOVA_AGILITY])), apply);
    player->HandleStatFlatModifier(UNIT_MOD_STAT_STAMINA, TOTAL_VALUE, float(EffectiveBonus(NOVA_STAMINA, v[NOVA_STAMINA])), apply);
    player->HandleStatFlatModifier(UNIT_MOD_STAT_INTELLECT, TOTAL_VALUE, float(EffectiveBonus(NOVA_INTELLECT, v[NOVA_INTELLECT])), apply);
    player->HandleStatFlatModifier(UNIT_MOD_STAT_SPIRIT, TOTAL_VALUE, float(EffectiveBonus(NOVA_SPIRIT, v[NOVA_SPIRIT])), apply);
    player->ApplySpellPowerBonus(int32(EffectiveBonus(NOVA_SPELL_POWER, v[NOVA_SPELL_POWER])), apply);
    player->HandleStatFlatModifier(UNIT_MOD_ATTACK_POWER, TOTAL_VALUE, float(EffectiveBonus(NOVA_ATTACK_POWER, v[NOVA_ATTACK_POWER])), apply);
    player->HandleStatFlatModifier(UNIT_MOD_ATTACK_POWER_RANGED, TOTAL_VALUE, float(EffectiveBonus(NOVA_ATTACK_POWER, v[NOVA_ATTACK_POWER])), apply);
}

std::unordered_map<ObjectGuid::LowType, Allocations> Applied;

Allocations& GetAllocations(Player* player)
{
    ObjectGuid::LowType guid = player->GetGUID().GetCounter();
    auto [itr, inserted] = Applied.try_emplace(guid);
    if (inserted)
        itr->second = LoadAllocations(player);
    return itr->second;
}

void RefreshApplied(Player* player, Allocations const& next)
{
    ObjectGuid::LowType guid = player->GetGUID().GetCounter();
    auto itr = Applied.find(guid);
    if (itr != Applied.end())
        ModifyAllocations(player, itr->second, false);

    ModifyAllocations(player, next, true);
    Applied[guid] = next;
}

uint32 WithoutBonus(uint32 total, uint32 bonus)
{
    return total > bonus ? total - bonus : 0;
}

bool ResolveStat(std::string const& key, StatIndex& result)
{
    static std::unordered_map<std::string, StatIndex> const keys =
    {
        { "strength", NOVA_STRENGTH }, { "agility", NOVA_AGILITY },
        { "stamina", NOVA_STAMINA }, { "intellect", NOVA_INTELLECT },
        { "spirit", NOVA_SPIRIT }, { "spell_power", NOVA_SPELL_POWER },
        { "attack_power", NOVA_ATTACK_POWER }
    };
    auto itr = keys.find(key);
    if (itr == keys.end())
        return false;
    result = itr->second;
    return true;
}

void SendData(ChatHandler* handler, Player* player, Allocations const& v)
{
    uint32 earned = EarnedPoints(player);
    uint32 spent = SpentPoints(v);
    uint32 available = earned > spent ? earned - spent : 0;
    uint32 strengthBonus = EffectiveBonus(NOVA_STRENGTH, v[NOVA_STRENGTH]);
    uint32 agilityBonus = EffectiveBonus(NOVA_AGILITY, v[NOVA_AGILITY]);
    uint32 staminaBonus = EffectiveBonus(NOVA_STAMINA, v[NOVA_STAMINA]);
    uint32 intellectBonus = EffectiveBonus(NOVA_INTELLECT, v[NOVA_INTELLECT]);
    uint32 spiritBonus = EffectiveBonus(NOVA_SPIRIT, v[NOVA_SPIRIT]);
    uint32 spellPowerBonus = EffectiveBonus(NOVA_SPELL_POWER, v[NOVA_SPELL_POWER]);
    uint32 attackPowerBonus = EffectiveBonus(NOVA_ATTACK_POWER, v[NOVA_ATTACK_POWER]);

    // Primary base values exclude the module bonus. Spell/AP values use the
    // currently displayed total minus the applied module bonus.
    handler->PSendSysMessage(
        "NOVA_DATA|{}|{}|{}|{}|{}|{}|{}|{}|{}|{}|{}|{}|{}|{}|{}",
        available,
        WithoutBonus(uint32(std::max(0.0f, player->GetStat(STAT_STRENGTH))), strengthBonus), strengthBonus,
        WithoutBonus(uint32(std::max(0.0f, player->GetStat(STAT_AGILITY))), agilityBonus), agilityBonus,
        WithoutBonus(uint32(std::max(0.0f, player->GetStat(STAT_STAMINA))), staminaBonus), staminaBonus,
        WithoutBonus(uint32(std::max(0.0f, player->GetStat(STAT_INTELLECT))), intellectBonus), intellectBonus,
        WithoutBonus(uint32(std::max(0.0f, player->GetStat(STAT_SPIRIT))), spiritBonus), spiritBonus,
        WithoutBonus(player->GetUInt32Value(PLAYER_FIELD_MOD_HEALING_DONE_POS), spellPowerBonus), spellPowerBonus,
        WithoutBonus(uint32(std::max(0.0f, player->GetTotalAttackPowerValue(BASE_ATTACK))), attackPowerBonus), attackPowerBonus);
}
}

class NovaEnhancementPlayerScript : public PlayerScript
{
public:
    NovaEnhancementPlayerScript() : PlayerScript("NovaEnhancementPlayerScript") { }

    void OnPlayerLogin(Player* player) override
    {
        if (IsEnabled())
            RefreshApplied(player, LoadAllocations(player));
    }

    void OnPlayerLogout(Player* player) override
    {
        Applied.erase(player->GetGUID().GetCounter());
    }

    void OnPlayerDelete(ObjectGuid guid, uint32 /*accountId*/) override
    {
        CharacterDatabase.Execute(
            "DELETE FROM `nova_character_enhancements` WHERE `guid`={}",
            guid.GetCounter());
    }
};

class NovaEnhancementCommandScript : public CommandScript
{
public:
    NovaEnhancementCommandScript() : CommandScript("NovaEnhancementCommandScript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable novaTable =
        {
            { "data", HandleData, SEC_PLAYER, Console::No },
            { "add", HandleAdd, SEC_PLAYER, Console::No },
            { "reset", HandleReset, SEC_PLAYER, Console::No }
        };
        static ChatCommandTable root = { { "nova", novaTable } };
        return root;
    }

    static bool HandleData(ChatHandler* handler)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!IsEnabled())
        {
            handler->SendSysMessage("NOVA_DATA|ERROR|Модуль отключён.");
            return true;
        }

        Allocations const& values = GetAllocations(player);
        SendData(handler, player, values);
        return true;
    }

    static bool HandleAdd(ChatHandler* handler, std::string const& statKey)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!IsEnabled())
        {
            handler->SendSysMessage("NOVA_DATA|ERROR|Модуль отключён.");
            return true;
        }

        StatIndex stat;
        if (!ResolveStat(statKey, stat))
        {
            handler->SendSysMessage("NOVA_DATA|ERROR|Неизвестная характеристика.");
            return true;
        }

        Allocations values = GetAllocations(player);
        if (SpentPoints(values) >= EarnedPoints(player))
        {
            handler->SendSysMessage("NOVA_DATA|ERROR|Нет доступных очков.");
            SendData(handler, player, values);
            return true;
        }

        uint32 maxPerStat = sConfigMgr->GetOption<uint32>("NovaEnhancement.MaxPointsPerStat", 0);
        if (maxPerStat && values[stat] >= maxPerStat)
        {
            handler->SendSysMessage("NOVA_DATA|ERROR|Достигнут предел характеристики.");
            return true;
        }

        ++values[stat];
        RefreshApplied(player, values);
        SaveAllocations(player, values);
        SendData(handler, player, values);
        return true;
    }

    static bool HandleReset(ChatHandler* handler)
    {
        Player* player = handler->GetSession()->GetPlayer();

        if (!IsEnabled())
        {
            handler->SendSysMessage("NOVA_DATA|ERROR|Модуль отключён.");
            return true;
        }

        if (!sConfigMgr->GetOption<bool>("NovaEnhancement.AllowReset", true))
        {
            handler->SendSysMessage("NOVA_DATA|ERROR|Сброс усилений отключён.");
            return true;
        }

        Allocations currentValues = GetAllocations(player);

        if (SpentPoints(currentValues) == 0)
        {
            handler->SendSysMessage("NOVA_DATA|ERROR|Нет распределённых усилений.");
            SendData(handler, player, currentValues);
            return true;
        }

        uint32 resetCostGold = ResetCostGold();
        uint64 resetCostCopper = uint64(resetCostGold) * GOLD;

        if (player->GetMoney() < resetCostCopper)
        {
            handler->PSendSysMessage("NOVA_DATA|ERROR|Для сброса требуется {} золота.", resetCostGold);
            SendData(handler, player, currentValues);
            return true;
        }

        if (resetCostCopper > 0)
            player->ModifyMoney(-int64(resetCostCopper));

        Allocations emptyValues{};
        RefreshApplied(player, emptyValues);
        SaveAllocations(player, emptyValues);
        SendData(handler, player, emptyValues);

        return true;
    }
};

void AddNovaEnhancementScripts()
{
    new NovaEnhancementPlayerScript();
    new NovaEnhancementCommandScript();
}
