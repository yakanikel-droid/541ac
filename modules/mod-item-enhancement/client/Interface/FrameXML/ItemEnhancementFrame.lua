local ENH = {}
local lastBag, lastSlot
local enhancementSlotCounter = 0

local originalPickupContainerItem = PickupContainerItem
PickupContainerItem = function(bag, slot)
    lastBag, lastSlot = bag, slot
    return originalPickupContainerItem(bag, slot)
end

local function SendCommand(command)
    SendChatMessage(".enhanceui " .. command, "SAY")
end

local function SendSlotCommand(command, clientBag, clientSlot)
    -- The server validates and converts client inventory coordinates.
    SendCommand(command .. " " .. clientBag .. " " .. clientSlot)
end

local function MakeSlot(parent, x, y, label)
    enhancementSlotCounter = enhancementSlotCounter + 1
    local button = CreateFrame(
        "Button",
        "ItemEnhancementSlot" .. enhancementSlotCounter,
        parent,
        "ItemButtonTemplate"
    )
    button:SetPoint("TOPLEFT", x, y)
    button:SetWidth(40)
    button:SetHeight(40)
    button.label = button:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    button.label:SetPoint("TOP", button, "BOTTOM", 0, -4)
    button.label:SetText(label)
    return button
end

local frame = CreateFrame("Frame", "ItemEnhancementFrame", UIParent)
tinsert(UISpecialFrames, "ItemEnhancementFrame")
frame:SetWidth(420)
frame:SetHeight(485)
frame:SetPoint("CENTER")
frame:SetFrameStrata("DIALOG")
frame:SetMovable(true)
frame:EnableMouse(true)
frame:RegisterForDrag("LeftButton")
frame:SetScript("OnDragStart", frame.StartMoving)
frame:SetScript("OnDragStop", frame.StopMovingOrSizing)
-- One-piece window skin in a 512x512 BLP2 DXT5 texture.
-- The 420x485 frame begins at x=45, y=24. The offset also preserves the
-- emblem protruding beyond the top-left corner without scaling or seams.
frame.skin = frame:CreateTexture(nil, "BACKGROUND")
frame.skin:SetTexture("Interface\\AccountShop\\ram_vk1")
frame.skin:SetWidth(512)
frame.skin:SetHeight(512)
frame.skin:SetPoint("TOPLEFT", frame, "TOPLEFT", -50, 1)
frame:Hide()

frame.title = frame:CreateFontString(nil, "OVERLAY", "GameFontNormal")
frame.title:SetPoint("TOP", 0, -3)
frame.title:SetText("Улучшение снаряжения")

local function SetTabSkin(button, selected)
    if selected then
        button:SetHeight(28)
        button.text:SetTextColor(1.0, 0.82, 0.0)
    else
        button:SetHeight(24)
        button.text:SetTextColor(0.75, 0.75, 0.75)
    end
end

frame.enhanceTab = CreateFrame("Button", "ItemEnhancementFrameTab1", frame)
frame.enhanceTab:SetWidth(84)
frame.enhanceTab:SetHeight(28)
frame.enhanceTab:SetPoint("BOTTOMLEFT", frame, "BOTTOMLEFT", 21, -25)
frame.enhanceTab.text = frame.enhanceTab:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
frame.enhanceTab.text:SetPoint("CENTER", 0, 5)
frame.enhanceTab.text:SetText("Заточка")
frame.transferTab = CreateFrame("Button", "ItemEnhancementFrameTab2", frame)
frame.transferTab:SetWidth(84)
frame.transferTab:SetHeight(24)
frame.transferTab:SetPoint("BOTTOMLEFT", frame, "BOTTOMLEFT", 114, -25)
frame.transferTab.text = frame.transferTab:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
frame.transferTab.text:SetPoint("CENTER", 0, 5)
frame.transferTab.text:SetText("Перенос")

frame.close = CreateFrame("Button", nil, frame, "UIPanelCloseButton")
frame.close:SetPoint("TOPRIGHT", 5, 5)

local function MakeHelpMark(parent, relativeTo, title, lines)
    local mark = CreateFrame("Frame", nil, parent)
    mark:SetWidth(18)
    mark:SetHeight(18)
    mark:EnableMouse(true)
    mark:SetPoint("LEFT", relativeTo, "RIGHT", 7, 0)
    mark.text = mark:CreateFontString(nil, "OVERLAY", "GameFontNormal")
    mark.text:SetPoint("CENTER", 0, 0)
    mark.text:SetText("?")
    mark.text:SetTextColor(1.0, 0.82, 0.0)
    mark:SetScript("OnEnter", function(self)
        GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
        GameTooltip:SetText(title, 1.0, 0.82, 0.0)
        for _, line in ipairs(lines) do
            GameTooltip:AddLine(line, 1.0, 1.0, 1.0, true)
        end
        GameTooltip:Show()
    end)
    mark:SetScript("OnLeave", function()
        GameTooltip:Hide()
    end)
    return mark
end

frame.item = MakeSlot(frame, 92, -72, "Предмет")
frame.result = MakeSlot(frame, 286, -72, "Результат")
frame.result:EnableMouse(true)
frame.failureHelp = MakeHelpMark(frame, frame.result, "Неудачное улучшение", {
    "При неудаче заточка будет сброшена до +0."
})

frame.arrow = frame:CreateTexture(nil, "ARTWORK")
frame.arrow:SetTexture("Interface\\AccountShop\\ram_st")
frame.arrow:SetWidth(75)
frame.arrow:SetHeight(38)
frame.arrow:SetPoint("CENTER", frame, "TOP", 0, -92)
frame.arrow:SetTexCoord(6 / 128, 125 / 128, 34 / 128, 94 / 128)

frame.bonus = frame:CreateFontString(nil, "OVERLAY", "GameFontHighlight")
frame.bonus:Hide()

frame.chance = frame:CreateFontString(nil, "OVERLAY", "GameFontNormal")
frame.chance:SetPoint("TOP", 0, -135)
frame.chance:SetText("Шанс: —")

frame.gold = frame:CreateFontString(nil, "OVERLAY", "GameFontNormal")
frame.gold:SetPoint("TOP", 0, -168)
frame.gold:SetText("Золото: —")

frame.material = MakeSlot(frame, 268, -212, "")
frame.materialHelp = MakeHelpMark(frame, frame.material, "Дополнительный материал", {
    "Сферы дракона гарантируют улучшение без затрат золота в пределах разрешённого уровня предмета.",
    "Камни повышения шанса увеличивают вероятность успешной заточки.",
    "Защитный камень при неудаче снижает заточку только на 1 уровень вместо сброса до +0."
})

frame.warning = frame:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
frame.warning:Hide()
frame.warning:SetText("|cffff4040При неудаче заточка будет сброшена до +0.|r")

frame.historyTitle = frame:CreateFontString(nil, "OVERLAY", "GameFontNormal")
frame.historyTitle:SetPoint("TOPLEFT", 20, -260)
frame.historyTitle:SetText("История:")

frame.historyBox = CreateFrame("Frame", nil, frame)
frame.historyBox:SetPoint("TOPLEFT", 18, -280)
frame.historyBox:SetPoint("BOTTOMRIGHT", -18, 70)
frame.historyBox:SetBackdrop({
    bgFile = "Interface\\Tooltips\\UI-Tooltip-Background",
    edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
    tile = true,
    tileSize = 16,
    edgeSize = 12,
    insets = { left = 3, right = 3, top = 3, bottom = 3 }
})
frame.historyBox:SetBackdropColor(0.03, 0.03, 0.03, 0.85)
frame.historyBox:SetBackdropBorderColor(0.45, 0.38, 0.24, 0.9)

frame.historyScroll = CreateFrame("ScrollFrame", "ItemEnhancementHistoryScroll", frame.historyBox, "UIPanelScrollFrameTemplate")
frame.historyScroll:SetPoint("TOPLEFT", 8, -8)
frame.historyScroll:SetPoint("BOTTOMRIGHT", -28, 8)
frame.historyScroll:EnableMouseWheel(true)

frame.historyContent = CreateFrame("Frame", nil, frame.historyScroll)
frame.historyContent:SetWidth(340)
frame.historyContent:SetHeight(1)
frame.historyScroll:SetScrollChild(frame.historyContent)

frame.history = frame.historyContent:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
frame.history:SetPoint("TOPLEFT", 0, 0)
frame.history:SetPoint("RIGHT", 0, 0)
frame.history:SetJustifyH("LEFT")
frame.history:SetJustifyV("TOP")
frame.history:SetText("")

frame.historyScroll:SetScript("OnMouseWheel", function(self, delta)
    local nextValue = self:GetVerticalScroll() - delta * 24
    nextValue = math.max(0, math.min(nextValue, self:GetVerticalScrollRange()))
    self:SetVerticalScroll(nextValue)
end)

frame.enhance = CreateFrame("Button", nil, frame, "UIPanelButtonTemplate")
frame.enhance:SetWidth(110)
frame.enhance:SetHeight(24)
frame.enhance:SetPoint("BOTTOM", 0, 22)
frame.enhance:SetText("Улучшить")
frame.enhance:Disable()
frame.enhance:SetScript("OnClick", function() SendCommand("attempt") end)

frame.transferSource = MakeSlot(frame, 92, -92, "Исходный предмет")
frame.transferTarget = MakeSlot(frame, 286, -92, "Новый предмет")
frame.transferArrow = frame:CreateTexture(nil, "ARTWORK")
frame.transferArrow:SetTexture("Interface\\AccountShop\\ram_st")
frame.transferArrow:SetWidth(75)
frame.transferArrow:SetHeight(38)
frame.transferArrow:SetPoint("CENTER", frame, "TOP", 0, -112)
frame.transferArrow:SetTexCoord(6 / 128, 125 / 128, 34 / 128, 94 / 128)
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
    frame.item, frame.result, frame.arrow, frame.chance,
    frame.gold, frame.material, frame.failureHelp, frame.materialHelp,
    frame.enhance
}
local transferWidgets = {
    frame.transferSource, frame.transferTarget, frame.transferArrow, frame.transferLevel, frame.transferResource,
    frame.transferInfo, frame.transferButton
}

local histories = { enhance = {}, transfer = {} }
local activeHistory = "enhance"
local RefreshHistory

local function ShowTab(name)
    local transfer = name == "transfer"
    activeHistory = transfer and "transfer" or "enhance"
    frame.skin:SetTexture(transfer and "Interface\\AccountShop\\ram_vk2" or "Interface\\AccountShop\\ram_vk1")
    frame.title:SetText(transfer and "Перенос улучшений" or "Улучшение снаряжения")
    SetTabSkin(frame.enhanceTab, not transfer)
    SetTabSkin(frame.transferTab, transfer)
    for _, widget in ipairs(enhancementWidgets) do if transfer then widget:Hide() else widget:Show() end end
    for _, widget in ipairs(transferWidgets) do if transfer then widget:Show() else widget:Hide() end end
    if RefreshHistory then RefreshHistory() end
end

frame.enhanceTab:SetScript("OnClick", function() ShowTab("enhance") end)
frame.transferTab:SetScript("OnClick", function() ShowTab("transfer") end)
ShowTab("enhance")

local function UpdateHistoryScroll()
    local visibleHeight = frame.historyScroll:GetHeight()
    local textHeight = frame.history:GetStringHeight() + 4
    frame.historyContent:SetWidth(math.max(1, frame.historyScroll:GetWidth()))
    frame.historyContent:SetHeight(math.max(visibleHeight, textHeight))
    frame.historyScroll:UpdateScrollChildRect()
end

RefreshHistory = function()
    frame.history:SetText(table.concat(histories[activeHistory], "\n"))
    UpdateHistoryScroll()
    frame.historyScroll:SetVerticalScroll(0)
end

local function ClearHistory(kind)
    kind = kind or activeHistory
    wipe(histories[kind])
    if kind == activeHistory then RefreshHistory() end
end

local function AddHistory(text, kind)
    kind = kind or activeHistory
    local history = histories[kind]
    table.insert(history, 1, text)
    while #history > 100 do table.remove(history) end
    if kind == activeHistory then RefreshHistory() end
end

local function ClearSlot(button)
    SetItemButtonTexture(button, nil)
    button.link = nil
    button.enhancementLevel = nil
    button.enhancementBonus = nil
    button.isResultPreview = nil
    button.previewStats = nil
    button.previewDamage = nil
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

local statNames = {
    [0] = "Мана", [1] = "Здоровье", [3] = "Ловкость", [4] = "Сила",
    [5] = "Интеллект", [6] = "Дух", [7] = "Выносливость", [12] = "Рейтинг защиты",
    [13] = "Рейтинг уклонения", [14] = "Рейтинг парирования", [15] = "Рейтинг блока",
    [16] = "Меткость ближнего боя", [17] = "Меткость дальнего боя",
    [18] = "Меткость заклинаний", [19] = "Критический удар ближнего боя",
    [20] = "Критический удар дальнего боя", [21] = "Критический эффект заклинаний",
    [22] = "Снижение меткости ближнего боя", [23] = "Снижение меткости дальнего боя",
    [24] = "Снижение меткости заклинаний", [25] = "Снижение крита ближнего боя",
    [26] = "Снижение крита дальнего боя", [27] = "Снижение крита заклинаний",
    [28] = "Скорость ближнего боя", [29] = "Скорость дальнего боя",
    [30] = "Скорость заклинаний", [31] = "Рейтинг меткости", [32] = "Рейтинг критического удара",
    [33] = "Снижение меткости", [34] = "Снижение критического удара",
    [35] = "Рейтинг устойчивости", [36] = "Рейтинг скорости", [37] = "Рейтинг мастерства",
    [38] = "Сила атаки", [39] = "Сила атаки дальнего боя", [41] = "Сила лечения",
    [42] = "Урон заклинаний", [43] = "Восстановление маны", [44] = "Пробивание брони",
    [45] = "Сила заклинаний", [46] = "Восстановление здоровья",
    [47] = "Проникновение заклинаний", [48] = "Величина блока"
}

local statPatterns = {
    [3] = "ловкости", [4] = "к силе", [5] = "интеллекту", [6] = "духу",
    [7] = "выносливости", [12] = "защиты", [13] = "уклонения",
    [14] = "парирования", [15] = "блока", [31] = "меткости",
    [32] = "критического", [35] = "устойчивости", [36] = "скорости",
    [37] = "мастерства", [38] = "Сила атаки", [39] = "Сила атаки дальнего боя",
    [44] = "пробивания брони", [45] = "силу заклинаний", [47] = "проникновение заклинаний",
    [48] = "блокируем"
}

local function ParsePreviewRows(value)
    local rows = {}
    if not value or value == "" then return rows end
    for row in string.gmatch(value, "[^;]+") do
        local kind, oldValue, newValue = string.match(row, "([^:]+):([^:]+):([^:]+)")
        if kind and oldValue and newValue then
            table.insert(rows, {
                kind = tonumber(kind), oldValue = tonumber(oldValue), newValue = tonumber(newValue)
            })
        end
    end
    return rows
end

local function ParseDamageRows(value)
    local rows = {}
    if not value or value == "" then return rows end
    for row in string.gmatch(value, "[^;]+") do
        local _, oldMin, oldMax, newMin, newMax = string.match(
            row, "([^:]+):([^:]+):([^:]+):([^:]+):([^:]+)")
        if oldMin then
            table.insert(rows, {
                oldMin = tonumber(oldMin), oldMax = tonumber(oldMax),
                newMin = tonumber(newMin), newMax = tonumber(newMax)
            })
        end
    end
    return rows
end

local function RewriteTooltipStat(tooltip, stat)
    local pattern = statPatterns[stat.kind]
    if not pattern or not stat.oldValue or not stat.newValue then return false end
    for index = 2, tooltip:NumLines() do
        local line = _G[tooltip:GetName() .. "TextLeft" .. index]
        local text = line and line:GetText()
        local matches = text and string.find(text, pattern, 1, true)
        if stat.kind == 4 and text and string.find(text, "заклинаний", 1, true) then matches = false end
        if matches then
            local changed, count = string.gsub(text, tostring(stat.oldValue), tostring(stat.newValue), 1)
            if count > 0 then
                line:SetText(changed .. " |cff20ff20(было " .. stat.oldValue .. ")|r")
                return true
            end
        end
    end
    return false
end

local function ApplyEnhancementTooltip(tooltip, data, isResultPreview)
    local level = tonumber(data and data.level) or 0
    local bonus = tonumber(data and data.bonus) or 0
    if level <= 0 then return end

    local first = _G[tooltip:GetName() .. "TextLeft1"]
    local name = first and first:GetText()
    if name and not string.match(name, "%+" .. level .. "$") then
        first:SetText(name .. " +" .. level)
    end

    local missingStats = {}
    for _, stat in ipairs(data.stats or {}) do
        if not RewriteTooltipStat(tooltip, stat) then
            table.insert(missingStats, stat)
        end
    end

    tooltip:AddLine(" ")
    if isResultPreview then
        tooltip:AddLine("Результат после успешной заточки", 0.25, 1.0, 0.25)
    else
        tooltip:AddLine("Предмет улучшен", 0.25, 1.0, 0.25)
    end
    tooltip:AddLine("Уровень улучшения: +" .. level, 1.0, 0.82, 0.0)
    tooltip:AddLine(string.format("Разрешённые характеристики и урон оружия: +%.2f%%", bonus),
        0.25, 1.0, 0.25, true)

    for _, stat in ipairs(missingStats) do
        local label = statNames[stat.kind] or ("Характеристика " .. tostring(stat.kind))
        tooltip:AddDoubleLine(label .. ":", tostring(stat.oldValue) .. " → " ..
            "|cff20ff20" .. tostring(stat.newValue) .. "|r", 1, 0.82, 0, 1, 1, 1)
    end
    for _, damage in ipairs(data.damage or {}) do
        tooltip:AddDoubleLine("Урон оружия:", string.format("%.1f–%.1f → |cff20ff20%.1f–%.1f|r",
            damage.oldMin, damage.oldMax, damage.newMin, damage.newMax), 1, 0.82, 0, 1, 1, 1)
    end
    tooltip:Show()
end

local function SlotEnter(self)
    if self.link then
        GameTooltip.itemEnhancementInspectKey = nil
        GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
        GameTooltip.itemEnhancementPendingExact = true
        GameTooltip:SetHyperlink(self.link)
        GameTooltip.itemEnhancementPendingExact = nil
        ApplyEnhancementTooltip(GameTooltip, {
            level = self.enhancementLevel,
            bonus = self.enhancementBonus,
            stats = self.previewStats,
            damage = self.previewDamage
        }, self.isResultPreview)
        GameTooltip:Show()
    end
end

local function SlotLeave()
    GameTooltip.itemEnhancementInspectKey = nil
    GameTooltip:Hide()
end

frame.item:RegisterForClicks("LeftButtonUp", "RightButtonUp")
frame.item:SetScript("OnClick", function(self, button)
    if button == "RightButton" then
        ClearHistory("enhance")
        ClearSlot(self)
        ClearSlot(frame.result)
        frame.enhance:Disable()
        SendCommand("clear item")
    elseif CursorHasItem() and lastBag and SetSlot(self, lastBag, lastSlot) then
        ClearHistory("enhance")
        ClearCursor()
        SendSlotCommand("item", lastBag, lastSlot)
    end
end)
frame.item:SetScript("OnEnter", SlotEnter)
frame.item:SetScript("OnLeave", SlotLeave)
frame.result:SetScript("OnEnter", SlotEnter)
frame.result:SetScript("OnLeave", SlotLeave)

frame.material:RegisterForClicks("LeftButtonUp", "RightButtonUp")
frame.material:SetScript("OnClick", function(self, button)
    if button == "RightButton" then
        ClearHistory("enhance")
        ClearSlot(self)
        SendCommand("clear material")
    elseif CursorHasItem() and lastBag and SetSlot(self, lastBag, lastSlot) then
        ClearHistory("enhance")
        ClearCursor()
        SendSlotCommand("material", lastBag, lastSlot)
    end
end)
frame.material:SetScript("OnEnter", SlotEnter)
frame.material:SetScript("OnLeave", SlotLeave)

local function ConfigureTransferSlot(button, command)
    button:RegisterForClicks("LeftButtonUp", "RightButtonUp")
    button:SetScript("OnClick", function(self, mouseButton)
        if mouseButton == "RightButton" then
            ClearHistory("transfer")
            ClearSlot(self)
            frame.transferButton:Disable()
            SendCommand(command .. " 255 255")
        elseif CursorHasItem() and lastBag and SetSlot(self, lastBag, lastSlot) then
            ClearHistory("transfer")
            ClearCursor()
            SendSlotCommand(command, lastBag, lastSlot)
        end
    end)
    button:SetScript("OnEnter", SlotEnter)
    button:SetScript("OnLeave", SlotLeave)
end
ConfigureTransferSlot(frame.transferSource, "transfer_source")
ConfigureTransferSlot(frame.transferTarget, "transfer_target")

local inspectedItems = {}
local pendingInspects = {}
local suppressTooltipInspect = false
local originalSetBagItem = GameTooltip.SetBagItem
local originalSetInventoryItem = GameTooltip.SetInventoryItem
local originalSetTradePlayerItem = GameTooltip.SetTradePlayerItem
local originalSetTradeTargetItem = GameTooltip.SetTradeTargetItem

local function RequestInspect(key, command)
    if inspectedItems[key] or pendingInspects[key] then return end
    pendingInspects[key] = true
    SendCommand(command)
end

GameTooltip.SetBagItem = function(self, bag, slot)
    self.itemEnhancementPendingExact = true
    local result = originalSetBagItem(self, bag, slot)
    self.itemEnhancementPendingExact = nil
    if suppressTooltipInspect then return result end
    local key = "B:" .. bag .. ":" .. slot
    self.itemEnhancementInspectKey = key
    local data = inspectedItems[key]
    if data then
        ApplyEnhancementTooltip(self, data, false)
    else
        RequestInspect(key, "inspect_bag " .. bag .. " " .. slot)
    end
    return result
end

GameTooltip.SetInventoryItem = function(self, unit, slot)
    self.itemEnhancementPendingExact = true
    local result = originalSetInventoryItem(self, unit, slot)
    self.itemEnhancementPendingExact = nil
    if suppressTooltipInspect then return result end
    local ownItem = unit == "player"
    local targetItem = not ownItem and UnitExists("target") and UnitIsUnit(unit, "target")
    if not ownItem and not targetItem then return result end
    local kind = ownItem and "I" or "T"
    local key = kind .. ":" .. slot
    self.itemEnhancementInspectKey = key
    local data = inspectedItems[key]
    if data then
        ApplyEnhancementTooltip(self, data, false)
    else
        RequestInspect(key, (ownItem and "inspect_inventory " or "inspect_target ") .. slot)
    end
    return result
end

if originalSetTradePlayerItem then
    GameTooltip.SetTradePlayerItem = function(self, slot)
        self.itemEnhancementPendingExact = true
        local result = originalSetTradePlayerItem(self, slot)
        self.itemEnhancementPendingExact = nil
        if suppressTooltipInspect then return result end
        local key = "X:0:" .. slot
        self.itemEnhancementInspectKey = key
        local data = inspectedItems[key]
        if data then
            ApplyEnhancementTooltip(self, data, false)
        else
            RequestInspect(key, "inspect_trade 0 " .. slot)
        end
        return result
    end
end

if originalSetTradeTargetItem then
    GameTooltip.SetTradeTargetItem = function(self, slot)
        self.itemEnhancementPendingExact = true
        local result = originalSetTradeTargetItem(self, slot)
        self.itemEnhancementPendingExact = nil
        if suppressTooltipInspect then return result end
        local key = "X:1:" .. slot
        self.itemEnhancementInspectKey = key
        local data = inspectedItems[key]
        if data then
            ApplyEnhancementTooltip(self, data, false)
        else
            RequestInspect(key, "inspect_trade 1 " .. slot)
        end
        return result
    end
end

local function RefreshInspectedTooltip(data)
    if not GameTooltip:IsShown() or GameTooltip.itemEnhancementInspectKey ~= data.key then return end
    suppressTooltipInspect = true
    GameTooltip.itemEnhancementPendingExact = true
    if data.kind == "B" then
        originalSetBagItem(GameTooltip, data.first, data.second)
    elseif data.kind == "X" then
        if data.first == 0 and originalSetTradePlayerItem then
            originalSetTradePlayerItem(GameTooltip, data.second)
        elseif data.first == 1 and originalSetTradeTargetItem then
            originalSetTradeTargetItem(GameTooltip, data.second)
        end
    else
        originalSetInventoryItem(GameTooltip, data.kind == "I" and "player" or "target", data.first)
    end
    GameTooltip.itemEnhancementPendingExact = nil
    suppressTooltipInspect = false
    ApplyEnhancementTooltip(GameTooltip, data, false)
end

GameTooltip:HookScript("OnHide", function(self)
    self.itemEnhancementInspectKey = nil
end)

local function HandlePayload(payload)
    local fields = { strsplit("\t", payload) }
    local command = fields[1]
    if command == "OPEN" then
        frame:Show()
        return
    end
    if command == "ERROR" then
        AddHistory("|cffff4040" .. (fields[2] or "Ошибка") .. "|r")
        frame.enhance:Disable()
        frame.transferButton:Disable()
        return
    end
    if command == "INSPECT" then
        local kind = fields[2]
        local first = tonumber(fields[3]) or 0
        local second = tonumber(fields[4]) or 0
        local key
        if kind == "B" or kind == "X" then
            key = kind .. ":" .. first .. ":" .. second
        else
            key = kind .. ":" .. first
        end
        local data = {
            key = key,
            kind = kind,
            first = first,
            second = second,
            level = tonumber(fields[5]) or 0,
            bonus = tonumber(fields[6]) or 0,
            stats = ParsePreviewRows(fields[7]),
            damage = ParseDamageRows(fields[8])
        }
        pendingInspects[key] = nil
        inspectedItems[key] = data
        RefreshInspectedTooltip(data)
        return
    end
    if command == "PREVIEW" then
        if fields[2] == "EMPTY" then
            frame.bonus:SetText("Усиление: —")
            frame.chance:SetText("Шанс: —")
            frame.gold:SetText("Золото: —")
            ClearSlot(frame.result)
            frame.item.enhancementLevel = nil
            frame.item.enhancementBonus = nil
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
        frame.item.enhancementLevel = current
        frame.item.enhancementBonus = oldBonus
        frame.item.previewStats = ParsePreviewRows(fields[12])
        frame.item.previewDamage = ParseDamageRows(fields[14])
        frame.result.link = frame.item.link
        frame.result.enhancementLevel = target
        frame.result.enhancementBonus = targetBonus
        frame.result.isResultPreview = true
        frame.result.previewStats = ParsePreviewRows(fields[13])
        frame.result.previewDamage = ParseDamageRows(fields[15])
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
        local materialConsumed = tonumber(fields[5]) == 1
        if fields[2] == "SUCCESS" then
            AddHistory("|cff20ff20Улучшение прошло успешно. Новый уровень: +" .. (fields[3] or "?") .. ".|r", "enhance")
        else
            AddHistory("|cffff4040Не удалось улучшить. Уровень изменён с +" ..
                (fields[3] or "?") .. " на +" .. (fields[4] or "0") .. ".|r", "enhance")
        end
        if materialConsumed then
            ClearSlot(frame.material)
        end
        return
    end
    if command == "TRANSFER_PREVIEW" then
        local sourceEntry = tonumber(fields[2]) or 0
        local level = tonumber(fields[3]) or 0
        local targetEntry = tonumber(fields[4]) or 0
        local materialId = tonumber(fields[5]) or 0
        local materialCount = tonumber(fields[6]) or 0
        local sourceBonus = tonumber(fields[7]) or 0
        frame.transferSource.enhancementLevel = level
        frame.transferSource.enhancementBonus = sourceBonus
        frame.transferSource.previewStats = ParsePreviewRows(fields[8])
        frame.transferSource.previewDamage = ParseDamageRows(fields[9])
        frame.transferSource.isResultPreview = nil
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
        AddHistory("|cff20ff20Заточка +" .. (fields[3] or "?") .. " успешно перенесена.|r", "transfer")
    end
end

local events = CreateFrame("Frame")
events:RegisterEvent("CHAT_MSG_SYSTEM")
events:RegisterEvent("BAG_UPDATE")
events:RegisterEvent("UNIT_INVENTORY_CHANGED")
events:RegisterEvent("PLAYER_TARGET_CHANGED")
events:RegisterEvent("TRADE_UPDATE")
events:RegisterEvent("TRADE_CLOSED")
events:SetScript("OnEvent", function(_, event, message)
    if event == "BAG_UPDATE" or event == "PLAYER_TARGET_CHANGED" or event == "UNIT_INVENTORY_CHANGED" or
        event == "TRADE_UPDATE" or event == "TRADE_CLOSED" then
        inspectedItems = {}
        pendingInspects = {}
        return
    end
    if event == "CHAT_MSG_SYSTEM" and type(message) == "string" and string.sub(message, 1, 4) == "ENH\t" then
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
    local hasEnhancementLine = false
    for index = 2, tooltip:NumLines() do
        local line = _G[tooltip:GetName() .. "TextLeft" .. index]
        local text = line and line:GetText()
        local found = text and (string.match(text, "Улучшение %+([0-9]+)") or
            string.match(text, "Уровень улучшения: %+([0-9]+)"))
        if found then
            level = tonumber(found)
            hasEnhancementLine = true
            break
        end
    end
    if not level then
        local payload = string.match(link, "item:([%-0-9:]+)")
        local field = 0
        for value in string.gmatch(payload or "", "([%-0-9]+)") do
            field = field + 1
            local enchantId = tonumber(value) or 0
            if field >= 2 and field <= 8 and enchantId >= 50001 and enchantId <= 50010 then
                level = enchantId - 50000
                break
            end
        end
    end
    if level then
        local first = _G[tooltip:GetName() .. "TextLeft1"]
        local text = first and first:GetText()
        if text and not string.match(text, "%+" .. level .. "$") then first:SetText(text .. " +" .. level) end
        if not hasEnhancementLine and not tooltip.itemEnhancementPendingExact then
            tooltip:AddLine("Предмет улучшен", 0.25, 1.0, 0.25)
            tooltip:AddLine("Уровень улучшения: +" .. level, 1.0, 0.82, 0.0)
            tooltip:Show()
        end
    end
end

local function HookGlobalItemTooltip(tooltip)
    if tooltip and tooltip.HookScript then
        tooltip:HookScript("OnTooltipSetItem", EnhanceTooltipName)
    end
end

HookGlobalItemTooltip(GameTooltip)
HookGlobalItemTooltip(ItemRefTooltip)
HookGlobalItemTooltip(ShoppingTooltip1)
HookGlobalItemTooltip(ShoppingTooltip2)
HookGlobalItemTooltip(ShoppingTooltip3)
