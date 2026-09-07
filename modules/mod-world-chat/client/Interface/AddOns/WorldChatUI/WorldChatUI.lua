local WORLD_CHANNEL_DBC_ID = 27
local WORLD_CHANNEL_NAME = "Мир"

local ALLIANCE_TEXTURE =
    "Interface\\PVPFrame\\PVP-Currency-Alliance"
local HORDE_TEXTURE =
    "Interface\\PVPFrame\\PVP-Currency-Horde"

local function TextureTag(path, size)
    return "|T" .. path .. ":" .. size .. ":" .. size .. ":0:0|t"
end

-- ChatFrame.lua places CHAT_FLAG_* immediately before the player hyperlink.
-- This gives us the requested [channel] [crest] [player] layout without
-- changing either clickable link.
CHAT_FLAG_WORLDCHAT_ALLIANCE = TextureTag(ALLIANCE_TEXTURE, 14) .. " "
CHAT_FLAG_WORLDCHAT_HORDE = TextureTag(HORDE_TEXTURE, 14) .. " "

local function WorldChannelFilter(
    self,
    event,
    message,
    author,
    languageName,
    channelName,
    target,
    flags,
    zoneChannelID,
    channelIndex,
    channelBaseName,
    ...)

    if zoneChannelID ~= WORLD_CHANNEL_DBC_ID
        and channelBaseName ~= WORLD_CHANNEL_NAME then
        return false
    end

    if string.sub(message, 1, 6) == "[WC:A]" then
        message = string.sub(message, 7)
        flags = "WORLDCHAT_ALLIANCE"
    elseif string.sub(message, 1, 6) == "[WC:H]" then
        message = string.sub(message, 7)
        flags = "WORLDCHAT_HORDE"
    end

    return false,
        message,
        author,
        languageName,
        channelName,
        target,
        flags,
        zoneChannelID,
        channelIndex,
        channelBaseName,
        ...
end

ChatFrame_AddMessageEventFilter("CHAT_MSG_CHANNEL", WorldChannelFilter)

-- Version 1.0.0 saved a custom cyan color. Reset it once to the stock
-- CHANNEL color, then leave future color changes entirely to the player.
local resetFrame = CreateFrame("Frame")
resetFrame:RegisterEvent("PLAYER_ENTERING_WORLD")
resetFrame:RegisterEvent("CHANNEL_UI_UPDATE")
resetFrame:SetScript("OnEvent", function(self)
    WorldChatUIDB = WorldChatUIDB or {}
    if WorldChatUIDB.defaultColorRestored102 then
        self:UnregisterAllEvents()
        return
    end

    local channelIndex = GetChannelName(WORLD_CHANNEL_NAME)
    if channelIndex and channelIndex > 0 and ChatTypeInfo.CHANNEL then
        ChangeChatColor(
            "CHANNEL" .. channelIndex,
            1,
            192 / 255,
            192 / 255)

        WorldChatUIDB.defaultColorRestored102 = true
        self:UnregisterAllEvents()
    end
end)
