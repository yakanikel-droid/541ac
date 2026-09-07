/*
 * Neutral starting campaign for Classic Mount Hyjal.
 */

#include "Chat.h"
#include "Config.h"
#include "Creature.h"
#include "DBCStructure.h"
#include "Player.h"
#include "QuestDef.h"
#include "ReputationMgr.h"
#include "ScriptMgr.h"
#include "ScriptedGossip.h"

namespace
{
enum NeutralState : uint8
{
    STATE_NEUTRAL = 0,
    STATE_ALLIANCE = 1,
    STATE_HORDE = 2
};

enum GossipActions : uint32
{
    ACTION_CHOOSE_ALLIANCE = GOSSIP_ACTION_INFO_DEF + 1,
    ACTION_CHOOSE_HORDE = GOSSIP_ACTION_INFO_DEF + 2
};

bool IsEnabled()
{
    return sConfigMgr->GetOption<bool>("NeutralStart.Enable", true);
}

uint32 GetStartMap()
{
    return sConfigMgr->GetOption<uint32>("NeutralStart.Map", 1);
}

uint32 GetStartArea()
{
    return sConfigMgr->GetOption<uint32>("NeutralStart.Area", 616);
}

void TeleportToStart(Player* player)
{
    player->TeleportTo(
        GetStartMap(),
        sConfigMgr->GetOption<float>("NeutralStart.X", 4975.579f),
        sConfigMgr->GetOption<float>("NeutralStart.Y", -1736.777f),
        sConfigMgr->GetOption<float>("NeutralStart.Z", 1342.3079f),
        sConfigMgr->GetOption<float>("NeutralStart.Orientation", 1.4023175f));
}

void SetStartHomebind(Player* player)
{
    WorldLocation location(
        GetStartMap(),
        sConfigMgr->GetOption<float>("NeutralStart.X", 4975.579f),
        sConfigMgr->GetOption<float>("NeutralStart.Y", -1736.777f),
        sConfigMgr->GetOption<float>("NeutralStart.Z", 1342.3079f),
        sConfigMgr->GetOption<float>("NeutralStart.Orientation", 1.4023175f));
    player->SetHomebind(location, GetStartArea());
}

bool HasCompletedRequiredQuest(Player* player)
{
    uint32 questId = sConfigMgr->GetOption<uint32>("NeutralStart.RequiredQuestId", 0);
    return questId == 0 || player->GetQuestRewardStatus(questId);
}

void ChooseFaction(Player* player, NeutralState state)
{
    if (player->GetCustomState() != STATE_NEUTRAL)
        return;

    player->SetCustomState(state);
    player->GetReputationMgr().ReinitializeForRaceMask(player->GetCustomReputationRaceMask());
    player->GetReputationMgr().SendInitialReputations();

    if (state == STATE_ALLIANCE)
    {
        ChatHandler(player->GetSession()).SendSysMessage(
            "Вы выбрали Альянс. Добро пожаловать в Штормград!");
        WorldLocation location(
            sConfigMgr->GetOption<uint32>("NeutralStart.Alliance.Map", 0),
            sConfigMgr->GetOption<float>("NeutralStart.Alliance.X", -8833.38f),
            sConfigMgr->GetOption<float>("NeutralStart.Alliance.Y", 628.62f),
            sConfigMgr->GetOption<float>("NeutralStart.Alliance.Z", 94.00f),
            sConfigMgr->GetOption<float>("NeutralStart.Alliance.Orientation", 0.70f));
        player->SetHomebind(location, sConfigMgr->GetOption<uint32>("NeutralStart.Alliance.Area", 1519));
        player->TeleportTo(location);
    }
    else
    {
        ChatHandler(player->GetSession()).SendSysMessage(
            "Вы выбрали Орду. Добро пожаловать в Оргриммар!");
        WorldLocation location(
            sConfigMgr->GetOption<uint32>("NeutralStart.Horde.Map", 1),
            sConfigMgr->GetOption<float>("NeutralStart.Horde.X", 1569.59f),
            sConfigMgr->GetOption<float>("NeutralStart.Horde.Y", -4397.63f),
            sConfigMgr->GetOption<float>("NeutralStart.Horde.Z", 16.06f),
            sConfigMgr->GetOption<float>("NeutralStart.Horde.Orientation", 0.54f));
        player->SetHomebind(location, sConfigMgr->GetOption<uint32>("NeutralStart.Horde.Area", 1637));
        player->TeleportTo(location);
    }

    player->SaveToDB(false, false);
}
}

class neutral_start_player_script : public PlayerScript
{
public:
    neutral_start_player_script() : PlayerScript("neutral_start_player_script") { }

    void OnPlayerFirstLogin(Player* player) override
    {
        if (!IsEnabled())
            return;

        player->SetCustomState(STATE_NEUTRAL);
        player->GetReputationMgr().ReinitializeForRaceMask(0);
        player->GetReputationMgr().SendInitialReputations();
        SetStartHomebind(player);
        TeleportToStart(player);
    }

    void OnPlayerLogin(Player* player) override
    {
        if (!IsEnabled())
            return;

        player->SetCustomState(player->GetCustomState());

        if (player->GetCustomState() == STATE_NEUTRAL &&
            (player->GetMapId() != GetStartMap() || player->GetAreaId() != GetStartArea()))
            TeleportToStart(player);
    }

    void OnPlayerUpdateArea(Player* player, uint32 /*oldArea*/, uint32 newArea) override
    {
        if (IsEnabled() && !player->IsGameMaster() && player->GetCustomState() == STATE_NEUTRAL &&
            newArea != GetStartArea() && !player->IsBeingTeleported())
            TeleportToStart(player);
    }

    bool OnPlayerBeforeTeleport(Player* player, uint32 mapId, float /*x*/, float /*y*/, float /*z*/,
        float /*orientation*/, uint32 /*options*/, Unit* /*target*/) override
    {
        return !IsEnabled() || player->IsGameMaster() || player->GetCustomState() != STATE_NEUTRAL ||
            mapId == GetStartMap();
    }

    bool OnPlayerCanEnterMap(Player* player, MapEntry const* entry, InstanceTemplate const* /*instance*/,
        MapDifficulty const* /*mapDiff*/, bool /*loginCheck*/) override
    {
        return !IsEnabled() || player->IsGameMaster() || player->GetCustomState() != STATE_NEUTRAL ||
            entry->MapID == GetStartMap();
    }

    bool OnPlayerCanJoinLfg(Player* player, uint8 /*roles*/, std::set<uint32>& /*dungeons*/,
        std::string const& /*comment*/) override
    {
        return !IsEnabled() || player->GetCustomState() != STATE_NEUTRAL;
    }

    bool OnPlayerCanInitTrade(Player* player, Player* /*target*/) override
    {
        return !IsEnabled() || player->GetCustomState() != STATE_NEUTRAL;
    }

    void OnPlayerIsPvP(Player* player, bool& result) override
    {
        if (IsEnabled() && player->GetCustomState() == STATE_NEUTRAL)
            result = false;
    }

    void OnPlayerIsFFAPvP(Player* player, bool& result) override
    {
        if (IsEnabled() && player->GetCustomState() == STATE_NEUTRAL)
            result = false;
    }
};

class npc_neutral_faction_guide : public CreatureScript
{
public:
    npc_neutral_faction_guide() : CreatureScript("npc_neutral_faction_guide") { }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        ClearGossipMenuFor(player);

        if (!IsEnabled())
            return true;

        if (player->GetCustomState() != STATE_NEUTRAL)
        {
            ChatHandler(player->GetSession()).SendSysMessage("Вы уже выбрали свою фракцию.");
            CloseGossipMenuFor(player);
            return true;
        }

        if (!HasCompletedRequiredQuest(player))
        {
            ChatHandler(player->GetSession()).SendSysMessage(
                "Сначала завершите общую цепочку заданий Хиджала.");
            CloseGossipMenuFor(player);
            return true;
        }

        AddGossipItemFor(
            player, GOSSIP_ICON_CHAT, "Я выбираю Альянс.", GOSSIP_SENDER_MAIN,
            ACTION_CHOOSE_ALLIANCE,
            "Выбор необратим. Присоединиться к Альянсу?", 0, false);
        AddGossipItemFor(
            player, GOSSIP_ICON_CHAT, "Я выбираю Орду.", GOSSIP_SENDER_MAIN,
            ACTION_CHOOSE_HORDE,
            "Выбор необратим. Присоединиться к Орде?", 0, false);
        SendGossipMenuFor(player, player->GetGossipTextId(creature), creature->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* /*creature*/, uint32 sender, uint32 action) override
    {
        if (sender != GOSSIP_SENDER_MAIN || !HasCompletedRequiredQuest(player))
            return false;

        ClearGossipMenuFor(player);
        CloseGossipMenuFor(player);

        if (action == ACTION_CHOOSE_ALLIANCE)
            ChooseFaction(player, STATE_ALLIANCE);
        else if (action == ACTION_CHOOSE_HORDE)
            ChooseFaction(player, STATE_HORDE);

        return true;
    }
};

void AddNeutralStartScripts()
{
    new neutral_start_player_script();
    new npc_neutral_faction_guide();
}
