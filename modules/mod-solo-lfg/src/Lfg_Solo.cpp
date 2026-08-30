/*
** Made by Traesh https://github.com/Traesh
** AzerothCore 2019 http://www.azerothcore.org/
** Conan513 https://github.com/conan513
** Made into a module by Micrah https://github.com/milestorme/
*/

#include "ScriptMgr.h"
#include "Player.h"
#include "Configuration/Config.h"
#include "World.h"
#include "LFGMgr.h"
#include "Chat.h"
#include "Opcodes.h"

class lfg_solo_announce : public PlayerScript
{
public:
    lfg_solo_announce() : PlayerScript("lfg_solo_announce") {}

    bool OnPlayerCanJoinLfg(Player* player, uint8 roles, std::set<uint32>& /*dungeons*/, std::string const& /*comment*/) override
    {
        if (!sConfigMgr->GetOption<bool>("SoloLFG.Enable", true))
            return true;

        uint8 const combatRoles = roles &
            (lfg::PLAYER_ROLE_TANK | lfg::PLAYER_ROLE_HEALER | lfg::PLAYER_ROLE_DAMAGE);

        // Solo mode bypasses the normal five-player role assignment. The
        // proposal packet must therefore start with exactly one combat role;
        // role 0 and combined masks are returned by the client as UNKNOWN.
        bool const hasExactlyOneRole =
            combatRoles == lfg::PLAYER_ROLE_TANK ||
            combatRoles == lfg::PLAYER_ROLE_HEALER ||
            combatRoles == lfg::PLAYER_ROLE_DAMAGE;

        if (!hasExactlyOneRole)
        {
            ChatHandler(player->GetSession()).SendSysMessage(
                "Для одиночного поиска выберите только одну роль: танк, лекарь или боец.");
            return false;
        }

        return true;
    }

    void OnPlayerLogin(Player* player) override
    {
        // Announce Module
        if (sConfigMgr->GetOption<bool>("SoloLFG.Announce", true))
        {
            ChatHandler(player->GetSession()).SendSysMessage("This server is running the |cff4CFF00Solo Dungeon Finder |rmodule.");
        }
    }

    void OnPlayerRewardKillRewarder(Player* /*player*/, KillRewarder* /*rewarder*/, bool isDungeon, float& rate) override
    {
        if (!isDungeon
            || !sConfigMgr->GetOption<bool>("SoloLFG.Enable", true)
            || !sConfigMgr->GetOption<bool>("SoloLFG.FixedXP", true))
        {
            return;
        }

        // Force the rate to FixedXPRate regardless of group size, to encourage group play
        rate = sConfigMgr->GetOption<float>("SoloLFG.FixedXPRate", 0.2);
    }
};

class lfg_solo : public WorldScript
{
public:
    lfg_solo() : WorldScript("lfg_solo") {}

    void OnAfterConfigLoad(bool /*reload*/) override
    {
        // Solo LFG must not use the global LFG testing/debug mode. Testing
        // changes proposal protocol behavior in addition to group-size checks
        // and produces invalid UNKNOWN roles in the stock 3.3.5a client.
        sLFGMgr->SetSoloLfgEnabled(
            sConfigMgr->GetOption<bool>("SoloLFG.Enable", true));
    }
};

void AddLfgSoloScripts()
{
    new lfg_solo_announce();
    new lfg_solo();
}
