#include "Channel.h"
#include "Player.h"
#include "ScriptMgr.h"

namespace
{
constexpr uint32 WORLD_CHANNEL_ID = 27;
constexpr char ALLIANCE_MARKER[] = "[WC:A]";
constexpr char HORDE_MARKER[] = "[WC:H]";
}

class WorldChatPlayerScript final : public PlayerScript
{
public:
    WorldChatPlayerScript()
        : PlayerScript("WorldChatPlayerScript",
            { PLAYERHOOK_CAN_PLAYER_USE_CHANNEL_CHAT })
    {
    }

    [[nodiscard]] bool OnPlayerCanUseChat(
        Player* player,
        uint32 /*type*/,
        uint32 /*language*/,
        std::string& message,
        Channel* channel) override
    {
        if (!player || !channel || channel->GetChannelId() != WORLD_CHANNEL_ID)
            return true;

        // The client addon removes this marker and uses it to place the
        // correct faction crest immediately before the author's name.
        message.insert(0, player->GetTeamId() == TEAM_ALLIANCE
            ? ALLIANCE_MARKER
            : HORDE_MARKER);

        return true;
    }
};

void AddWorldChatScripts()
{
    new WorldChatPlayerScript();
}
