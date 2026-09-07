#include "DungeonRespawn.h"

#include "Config.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "Map.h"
#include "Player.h"

#include <algorithm>
#include <vector>

namespace
{
struct DungeonPosition
{
    int32 MapId = -1;
    float X = 0.0f;
    float Y = 0.0f;
    float Z = 0.0f;
    float Orientation = 0.0f;
};

struct PlayerRespawnData
{
    ObjectGuid Guid;
    DungeonPosition Position;
    bool IsChangingMap = false;
    bool IsInsideInstance = false;
};

std::vector<PlayerRespawnData> RespawnData;
std::vector<ObjectGuid> PendingGhosts;
bool DungeonRespawnEnabled = false;
float RespawnHealthPct = 50.0f;

bool IsInsideDungeonOrRaid(Player const* player)
{
    if (!player)
        return false;

    Map const* map = player->GetMap();
    return map && (map->IsDungeon() || map->IsRaid());
}

PlayerRespawnData& GetOrCreateRespawnData(Player* player)
{
    auto itr = std::find_if(RespawnData.begin(), RespawnData.end(), [player](PlayerRespawnData const& data)
    {
        return data.Guid == player->GetGUID();
    });

    if (itr != RespawnData.end())
        return *itr;

    RespawnData.push_back({ player->GetGUID() });
    return RespawnData.back();
}

void SaveRespawnData()
{
    for (PlayerRespawnData const& data : RespawnData)
    {
        if (data.IsInsideInstance && data.Position.MapId >= 0)
        {
            CharacterDatabase.Execute(
                "INSERT INTO `dungeonrespawn_playerinfo` (`guid`, `map`, `x`, `y`, `z`, `o`) "
                "VALUES ({}, {}, {}, {}, {}, {}) ON DUPLICATE KEY UPDATE `map` = VALUES(`map`), "
                "`x` = VALUES(`x`), `y` = VALUES(`y`), `z` = VALUES(`z`), `o` = VALUES(`o`)",
                data.Guid.GetRawValue(), data.Position.MapId, data.Position.X, data.Position.Y,
                data.Position.Z, data.Position.Orientation);
        }
        else
            CharacterDatabase.Execute(
                "DELETE FROM `dungeonrespawn_playerinfo` WHERE `guid` = {}", data.Guid.GetRawValue());
    }
}

void LoadRespawnData()
{
    RespawnData.clear();

    QueryResult result = CharacterDatabase.Query(
        "SELECT `guid`, `map`, `x`, `y`, `z`, `o` FROM `dungeonrespawn_playerinfo`");

    if (!result)
    {
        LOG_INFO("module.dungeon-respawn", "Loaded 0 dungeon respawn positions.");
        return;
    }

    do
    {
        Field* fields = result->Fetch();
        PlayerRespawnData data;
        data.Guid = ObjectGuid(fields[0].Get<uint64>());
        data.Position.MapId = fields[1].Get<int32>();
        data.Position.X = fields[2].Get<float>();
        data.Position.Y = fields[3].Get<float>();
        data.Position.Z = fields[4].Get<float>();
        data.Position.Orientation = fields[5].Get<float>();
        RespawnData.push_back(data);
    } while (result->NextRow());

    LOG_INFO("module.dungeon-respawn", "Loaded {} dungeon respawn positions.", RespawnData.size());
}

class DungeonRespawnPlayerScript : public PlayerScript
{
public:
    DungeonRespawnPlayerScript()
        : PlayerScript("DungeonRespawnPlayerScript")
    {
    }

    void OnPlayerReleasedGhost(Player* player) override
    {
        if (!DungeonRespawnEnabled || !IsInsideDungeonOrRaid(player))
            return;

        if (std::find(PendingGhosts.begin(), PendingGhosts.end(), player->GetGUID()) == PendingGhosts.end())
            PendingGhosts.push_back(player->GetGUID());
    }

    bool OnPlayerBeforeTeleport(Player* player, uint32 mapId, float, float, float, float, uint32, Unit*) override
    {
        if (!DungeonRespawnEnabled || !player)
            return true;

        PlayerRespawnData& data = GetOrCreateRespawnData(player);
        if (player->GetMapId() != mapId)
            data.IsChangingMap = true;

        if (!IsInsideDungeonOrRaid(player) || !player->isDead())
            return true;

        auto pendingItr = std::find(PendingGhosts.begin(), PendingGhosts.end(), player->GetGUID());
        if (pendingItr == PendingGhosts.end())
            return true;

        PendingGhosts.erase(pendingItr);

        if (data.Position.MapId < 0 || data.Position.MapId != int32(player->GetMapId()))
            return true;

        data.IsChangingMap = false;
        player->TeleportTo(data.Position.MapId, data.Position.X, data.Position.Y, data.Position.Z,
            data.Position.Orientation);
        player->ResurrectPlayer(RespawnHealthPct / 100.0f, false);
        player->SpawnCorpseBones();
        return false;
    }

    void OnPlayerMapChanged(Player* player) override
    {
        if (!player)
            return;

        PlayerRespawnData& data = GetOrCreateRespawnData(player);
        data.IsInsideInstance = IsInsideDungeonOrRaid(player);

        if (!data.IsInsideInstance)
        {
            data.IsChangingMap = false;
            return;
        }

        if (!data.IsChangingMap)
            return;

        data.Position.MapId = player->GetMapId();
        data.Position.X = player->GetPositionX();
        data.Position.Y = player->GetPositionY();
        data.Position.Z = player->GetPositionZ();
        data.Position.Orientation = player->GetOrientation();
        data.IsChangingMap = false;
    }

    void OnPlayerLogin(Player* player) override
    {
        if (!player)
            return;

        PlayerRespawnData& data = GetOrCreateRespawnData(player);
        data.IsInsideInstance = IsInsideDungeonOrRaid(player);
    }

    void OnPlayerLogout(Player* player) override
    {
        if (!player)
            return;

        std::erase(PendingGhosts, player->GetGUID());
        PlayerRespawnData& data = GetOrCreateRespawnData(player);
        data.IsInsideInstance = IsInsideDungeonOrRaid(player);
    }
};

class DungeonRespawnWorldScript : public WorldScript
{
public:
    DungeonRespawnWorldScript()
        : WorldScript("DungeonRespawnWorldScript")
    {
    }

    void OnAfterConfigLoad(bool reload) override
    {
        if (reload)
            SaveRespawnData();

        DungeonRespawnEnabled = sConfigMgr->GetOption<bool>("DungeonRespawn.Enable", false);
        RespawnHealthPct = std::clamp(
            sConfigMgr->GetOption<float>("DungeonRespawn.RespawnHealthPct", 50.0f), 0.0f, 100.0f);
        LoadRespawnData();
    }

    void OnShutdown() override
    {
        SaveRespawnData();
    }
};
}

void AddDungeonRespawnScripts()
{
    new DungeonRespawnWorldScript();
    new DungeonRespawnPlayerScript();
}
