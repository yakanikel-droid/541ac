local ENH = {}
local lastBag, lastSlot

local originalPickupContainerItem = PickupContainerItem
PickupContainerItem = function(bag, slot)
    lastBag, lastSlot = bag, slot
    return originalPickupContainerItem(bag, slot)
end

local function SendCommand(command)
    SendChatMessage(".enhanceui " .. command, "SAY")
end

local function MakeSlot(parent, x, y, label)
    local button = CreateFrame("Button", nil, parent, "ItemButtonTemplate")
    button:SetPoint("TOPLEFT", x, y)
    button:SetWidth(40)
    button:SetHeight(40)
    button.label = parent:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    button.label:SetPoint("TOP", button, "BOTTOM", 0, -4)
    button.label:SetText(label)
    return button
end

local frame = CreateFrame("Frame", "ItemEnhancementFrame", UIParent)
frame:SetWidth(420)
frame:SetHeight(485)
frame:SetPoint("CENTER")
frame:SetFrameStrata("DIALOG")
frame:SetMovable(true)
frame:EnableMouse(true)
frame:RegisterForDrag("LeftButton")
frame:SetScript("OnDragStart", frame.StartMoving)
frame:SetScript("OnDragStop", frame.StopMovingOrSizing)
frame:SetBackdrop({
    bgFile = "Interface\\DialogFrame\\UI-DialogBox-Background",
    edgeFile = "Interface\\DialogFrame\\UI-DialogBox-Border",
    tile = true, tileSize = 32, edgeSize = 32,
    insets = { left = 11, right = 12, top = 12, bottom = 11 }
})
frame:Hide()

frame.title = frame:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
frame.title:SetPoint("TOP", 0, -18)
frame.title:SetText("Улучшение снаряжения")

frame.enhanceTab = CreateFrame("Button", nil, frame, "UIPanelButtonTemplate")
frame.enhanceTab:SetWidth(105)
frame.enhanceTab:SetHeight(22)
frame.enhanceTab:SetPoint("TOP", -55, -43)
frame.enhanceTab:SetText("Заточка")
frame.transferTab = CreateFrame("Button", nil, frame, "UIPanelButtonTemplate")
frame.transferTab:SetWidth(105)
frame.transferTab:SetHeight(22)
frame.transferTab:SetPoint("TOP", 55, -43)
frame.transferTab:SetText("Перенос")

frame.close = CreateFrame("Button", nil, frame, "UIPanelCloseButton")
frame.close:SetPoint("TOPRIGHT", -5, -5)

frame.item = MakeSlot(frame, 92, -72, "Предмет")
frame.result = MakeSlot(frame, 286, -72, "Результат")
frame.result:Disable()

frame.arrow = frame:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
frame.arrow:SetPoint("CENTER", frame, "TOP", 0, -92)
frame.arrow:SetText("|cff20ff20>>>|r")

frame.bonus = frame:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
frame.bonus:SetPoint("TOP", 0, -140)
frame.bonus:SetText("Усиление: —")

frame.chance = frame:CreateFontString(nil, "OVERLAY", "GameFontNormal")
frame.chance:SetPoint("TOP", 0, -165)
frame.chance:SetText("Шанс: —")

frame.gold = frame:CreateFontString(nil, "OVERLAY", "GameFontNormal")
frame.gold:SetPoint("TOP", 0, -188)
frame.gold:SetText("Золото: —")

frame.materialText = frame:CreateFontString(nil, "OVERLAY", "GameFontNormal")
frame.materialText:SetPoint("TOPLEFT", 32, -225)
frame.materialText:SetText("Особый материал для улучшения:")

frame.material = MakeSlot(frame, 268, -212, "необязательно")

frame.warning = frame:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
frame.warning:SetPoint("TOPLEFT", 32, -278)
frame.warning:SetPoint("RIGHT", -32, 0)
frame.warning:SetJustifyH("LEFT")
frame.warning:SetText("|cffff4040При неудаче заточка будет сброшена до +0.|r")

frame.historyTitle = frame:CreateFontString(nil, "OVERLAY", "GameFontNormal")
frame.historyTitle:SetPoint("TOPLEFT", 32, -306)
frame.historyTitle:SetText("История:")

frame.historyBox = CreateFrame("Frame", nil, frame)
frame.historyBox:SetPoint("TOPLEFT", 30, -326)
frame.historyBox:SetPoint("BOTTOMRIGHT", -30, 55)
frame.historyBox:SetBackdrop({ bgFile = "Interface\\Tooltips\\UI-Tooltip-Background" })
frame.historyBox:SetBackdropColor(0.03, 0.03, 0.03, 0.85)
frame.history = frame.historyBox:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
frame.history:SetPoint("TOPLEFT", 8, -7)
frame.history:SetPoint("RIGHT", -8, 0)
frame.history:SetJustifyH("LEFT")
frame.history:SetJustifyV("TOP")
frame.history:SetText("")

frame.enhance = CreateFrame("Button", nil, frame, "UIPanelButtonTemplate")
frame.enhance:SetWidth(110)
frame.enhance:SetHeight(24)
frame.enhance:SetPoint("BOTTOM", 0, 22)
frame.enhance:SetText("Улучшить")
frame.enhance:Disable()
frame.enhance:SetScript("OnClick", function() SendCommand("attempt") end)

frame.transferSource = MakeSlot(frame, 92, -92, "Исходный предмет")
frame.transferTarget = MakeSlot(frame, 286, -92, "Новый предмет")
frame.transferArrow = frame:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
frame.transferArrow:SetPoint("CENTER", frame, "TOP", 0, -112)
frame.transferArrow:SetText("|cff20ff20>>>|r")
frame.transferLevel = frame:CreateFontString(nil, "OVERLAY", "GameFontNormal")
frame.transferLevel:SetPoint("TOP", 0, -165)
frame.transferLevel:SetText("Переносимый уровень: —")
frame.transferResource = frame:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
frame.transferResource:SetPoint("TOP", 0, -200)
frame.transferResource:SetText("Необходимый ресурс: —")
frame.transferInfo = frame:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
frame.transferInfo:SetPoint("TOPLEFT", 38, -235)
frame.transferInfo:SetPoint("RIGHT", -38, 0)
frame.transferInfo:SetJustifyH("LEFT")
frame.transferInfo:SetText("Исходный предмет станет +0. Новый предмет получит тот же уровень заточки.")
frame.transferButton = CreateFrame("Button", nil, frame, "UIPanelButtonTemplate")
frame.transferButton:SetWidth(130)
frame.transferButton:SetHeight(24)
frame.transferButton:SetPoint("BOTTOM", 0, 22)
frame.transferButton:SetText("Перенести")
frame.transferButton:SetScript("OnClick", function() SendCommand("transfer") end)
frame.transferButton:Disable()

local enhancementWidgets = {
    frame.item, frame.result, frame.arrow, frame.bonus, frame.chance, frame.gold, frame.materialText,
    frame.material, frame.warning, frame.enhance
}
local transferWidgets = {
    frame.transferSource, frame.transferTarget, frame.transferArrow, frame.transferLevel, frame.transferResource,
    frame.transferInfo, frame.transferButton
}

local function ShowTab(name)
    local transfer = name == "transfer"
    for _, widget in ipairs(enhancementWidgets) do if transfer then widget:Hide() else widget:Show() end end
    for _, widget in ipairs(transferWidgets) do if transfer then widget:Show() else widget:Hide() end end
end

frame.enhanceTab:SetScript("OnClick", function() ShowTab("enhance") end)
frame.transferTab:SetScript("OnClick", function() ShowTab("transfer") end)
ShowTab("enhance")

local history = {}
local function AddHistory(text)
    table.insert(history, 1, text)
    while #history > 6 do table.remove(history) end
    frame.history:SetText(table.concat(history, "\n"))
end

local function ClearSlot(button)
    SetItemButtonTexture(button, nil)
    button.link = nil
end

local function SetSlot(button, bag, slot)
    local texture = GetContainerItemInfo(bag, slot)
    local link = GetContainerItemLink(bag, slot)
    if not texture or not link then return false end
    SetItemButtonTexture(button, texture)
    SetItemButtonCount(button, 0)
    button.link = link
    return true
end

local function SlotEnter(self)
    if self.link then
        GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
        GameTooltip:SetHyperlink(self.link)
    end
end

local function SlotLeave() GameTooltip:Hide() end

frame.item:RegisterForClicks("LeftButtonUp", "RightButtonUp")
frame.item:SetScript("OnClick", function(self, button)
    if button == "RightButton" then
        ClearSlot(self)
        ClearSlot(frame.result)
        frame.enhance:Disable()
        SendCommand("clear item")
    elseif CursorHasItem() and lastBag and SetSlot(self, lastBag, lastSlot) then
        ClearCursor()
        SendCommand("item " .. lastBag .. " " .. lastSlot)
    end
end)
frame.item:SetScript("OnEnter", SlotEnter)
frame.item:SetScript("OnLeave", SlotLeave)
frame.result:SetScript("OnEnter", SlotEnter)
frame.result:SetScript("OnLeave", SlotLeave)

frame.material:RegisterForClicks("LeftButtonUp", "RightButtonUp")
frame.material:SetScript("OnClick", function(self, button)
    if button == "RightButton" then
        ClearSlot(self)
        SendCommand("clear material")
    elseif CursorHasItem() and lastBag and SetSlot(self, lastBag, lastSlot) then
        ClearCursor()
        SendCommand("material " .. lastBag .. " " .. lastSlot)
    end
end)
frame.material:SetScript("OnEnter", SlotEnter)
frame.material:SetScript("OnLeave", SlotLeave)

local function ConfigureTransferSlot(button, command)
    button:RegisterForClicks("LeftButtonUp", "RightButtonUp")
    button:SetScript("OnClick", function(self, mouseButton)
        if mouseButton == "RightButton" then
            ClearSlot(self)
            frame.transferButton:Disable()
            SendCommand(command .. " 255 255")
        elseif CursorHasItem() and lastBag and SetSlot(self, lastBag, lastSlot) then
            ClearCursor()
            SendCommand(command .. " " .. lastBag .. " " .. lastSlot)
        end
    end)
    button:SetScript("OnEnter", SlotEnter)
    button:SetScript("OnLeave", SlotLeave)
end
ConfigureTransferSlot(frame.transferSource, "transfer_source")
ConfigureTransferSlot(frame.transferTarget, "transfer_target")

local function HandlePayload(payload)
    local fields = { strsplit("\t", payload) }
    local command = fields[1]
    if command == "OPEN" then
        frame:Show()
        return
    end
    if command == "ERROR" then
        AddHistory("|cffff4040" .. (fields[2] or "Ошибка") .. "|r")
        return
    end
    if command == "PREVIEW" then
        if fields[2] == "EMPTY" then
            frame.bonus:SetText("Усиление: —")
            frame.chance:SetText("Шанс: —")
            frame.gold:SetText("Золото: —")
            frame.enhance:Disable()
            return
        end
        local current, target = tonumber(fields[3]) or 0, tonumber(fields[4]) or 0
        local chance, copper = tonumber(fields[6]) or 0, tonumber(fields[7]) or 0
        local oldBonus, targetBonus = tonumber(fields[8]) or 0, tonumber(fields[9]) or 0
        local materialType = fields[10] or "NONE"
        local materialValue = tonumber(fields[11]) or 0
        frame.bonus:SetText(string.format("Усиление: %.2f%%  →  %.2f%%", oldBonus, targetBonus))
        frame.chance:SetText(string.format("Шанс: %.2f%%", chance))
        frame.gold:SetText(copper == 0 and "Золото: бесплатно" or GetCoinTextureString(copper))
        frame.result.link = frame.item.link
        SetItemButtonTexture(frame.result, GetItemIcon(frame.item.link or ""))
        if materialType == "PROTECTION" then
            frame.warning:SetText("|cffffff40При неудаче заточка снизится только на 1 уровень.|r")
        elseif materialType == "ORB" then
            frame.warning:SetText("|cff40c0ffСфера гарантирует успешное улучшение без золота.|r")
        else
            frame.warning:SetText("|cffff4040При неудаче заточка будет сброшена до +0.|r")
        end
        frame.enhance:Enable()
        return
    end
    if command == "RESULT" then
        if fields[2] == "SUCCESS" then
            AddHistory("|cff20ff20Улучшение прошло успешно. Новый уровень: +" .. (fields[3] or "?") .. ".|r")
        else
            AddHistory("|cffff4040Не удалось улучшить. Уровень изменён с +" ..
                (fields[3] or "?") .. " на +" .. (fields[4] or "0") .. ".|r")
        end
        return
    end
    if command == "TRANSFER_PREVIEW" then
        local sourceEntry = tonumber(fields[2]) or 0
        local level = tonumber(fields[3]) or 0
        local targetEntry = tonumber(fields[4]) or 0
        local materialId = tonumber(fields[5]) or 0
        local materialCount = tonumber(fields[6]) or 0
        frame.transferLevel:SetText(level > 0 and ("Переносимый уровень: +" .. level) or "Переносимый уровень: —")
        local materialName, materialLink = GetItemInfo(materialId)
        frame.transferResource:SetText("Необходимый ресурс: " ..
            (materialLink or materialName or ("предмет " .. materialId)) .. " x" .. materialCount)
        if sourceEntry > 0 and targetEntry > 0 and level > 0 then
            frame.transferButton:Enable()
        else
            frame.transferButton:Disable()
        end
        return
    end
    if command == "TRANSFER_RESULT" then
        AddHistory("|cff20ff20Заточка +" .. (fields[3] or "?") .. " успешно перенесена.|r")
    end
end

local events = CreateFrame("Frame")
events:RegisterEvent("CHAT_MSG_SYSTEM")
events:SetScript("OnEvent", function(_, _, message)
    if type(message) == "string" and string.sub(message, 1, 4) == "ENH\t" then
        HandlePayload(string.sub(message, 5))
    end
end)

if ChatFrame_AddMessageEventFilter then
    ChatFrame_AddMessageEventFilter("CHAT_MSG_SYSTEM", function(_, _, message)
        if type(message) == "string" and string.sub(message, 1, 4) == "ENH\t" then return true end
        return false
    end)
end

local function EnhanceTooltipName(tooltip)
    local _, link = tooltip:GetItem()
    if not link then return end
    local level
    for index = 2, tooltip:NumLines() do
        local line = _G[tooltip:GetName() .. "TextLeft" .. index]
        local text = line and line:GetText()
        local found = text and string.match(text, "Улучшение %+([0-9]+)")
        if found then level = tonumber(found) break end
    end
    if level then
        local first = _G[tooltip:GetName() .. "TextLeft1"]
        local text = first and first:GetText()
        if text and not string.match(text, "%+" .. level .. "$") then first:SetText(text .. " +" .. level) end
    end
end

GameTooltip:HookScript("OnTooltipSetItem", EnhanceTooltipName)
ItemRefTooltip:HookScript("OnTooltipSetItem", EnhanceTooltipName)
