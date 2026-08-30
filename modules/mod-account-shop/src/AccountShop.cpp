#include "AccountShop.h"

#include "Chat.h"
#include "DBCStores.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "Item.h"
#include "Log.h"
#include "Mail.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "Player.h"
#include "Random.h"
#include "SpellMgr.h"
#include "StringFormat.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <string_view>

namespace
{
std::atomic<uint32> MessageSequence{1};
std::string GetItemIcon(uint32 itemId)
{
    ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId);

    if (!itemTemplate)
        return "Interface\\Icons\\INV_Misc_QuestionMark";

    ItemDisplayInfoEntry const* displayInfo =
        sItemDisplayInfoStore.LookupEntry(itemTemplate->DisplayInfoID);

    if (!displayInfo || !displayInfo->inventoryIcon || !displayInfo->inventoryIcon[0])
        return "Interface\\Icons\\INV_Misc_QuestionMark";

    return std::string("Interface\\Icons\\") + displayInfo->inventoryIcon;
}
void CacheCreatureForShopPreview(Player* player, uint32 creatureEntry)
{
    if (!player || !player->GetSession() || creatureEntry == 0)
        return;

    // PlayerModel:SetCreature expects a creature_template entry already present
    // in the client's creature query cache. Reuse AzerothCore's normal query
    // handler so locale, model IDs and all packet fields remain core-compatible.
    WorldPacket query(CMSG_CREATURE_QUERY, 12);
    query << uint32(creatureEntry);
    query << ObjectGuid::Empty;
    player->GetSession()->HandleCreatureQueryOpcode(query);
}

bool MatchesEquipmentSlot(uint32 inventoryType, uint8 slot)
{
    switch (slot)
    {
        case 1:  return inventoryType == 1;                         // Голова
        case 2:  return inventoryType == 2;                         // Шея
        case 3:  return inventoryType == 3;                         // Плечи
        case 4:  return inventoryType == 16;                        // Спина
        case 5:  return inventoryType == 5 || inventoryType == 20;  // Грудь / роба
        case 6:  return inventoryType == 9;                         // Запястья
        case 7:  return inventoryType == 10;                        // Кисти рук
        case 8:  return inventoryType == 6;                         // Пояс
        case 9:  return inventoryType == 7;                         // Ноги
        case 10: return inventoryType == 8;                         // Ступни
        case 11: return inventoryType == 11;                        // Палец
        case 12: return inventoryType == 12;                        // Аксессуар
        case 13: return inventoryType == 13 || inventoryType == 17 || inventoryType == 21;
        case 14: return inventoryType == 14 || inventoryType == 22 || inventoryType == 23;
        case 15: return inventoryType == 15 || inventoryType == 25 || inventoryType == 26 || inventoryType == 28;
        default: return true;
    }
}

std::string EscapeField(std::string_view value)
{
    std::string result;
    result.reserve(value.size());

    for (unsigned char character : value)
    {
        switch (character)
        {
            case '%':
                result += "%25";
                break;
            case '\t':
                result += "%09";
                break;
            case '\r':
                result += "%0D";
                break;
            case '\n':
                result += "%0A";
                break;
            default:
                result.push_back(static_cast<char>(character));
                break;
        }
    }

    return result;
}

std::string HexEncode(std::string_view value)
{
    static constexpr char Hex[] = "0123456789ABCDEF";
    std::string result;
    result.reserve(value.size() * 2);

    for (unsigned char character : value)
    {
        result.push_back(Hex[(character >> 4) & 0x0F]);
        result.push_back(Hex[character & 0x0F]);
    }

    return result;
}

std::string MakeRequestToken(Player const* player)
{
    uint32 const accountId = player->GetSession()->GetAccountId();
    uint32 const guidLow = player->GetGUID().GetCounter();
    uint32 const now = static_cast<uint32>(GameTime::GetGameTimeMS().count());

    return Acore::StringFormat("{:08X}{:08X}{:08X}{:08X}", accountId, guidLow, now, rand32());
}

bool IsUniqueProduct(AccountShop::ProductType type)
{
    using AccountShop::ProductType;

    switch (type)
    {
        case ProductType::Spell:
        case ProductType::Mount:
        case ProductType::Pet:
        case ProductType::Level:
        case ProductType::Rename:
        case ProductType::Customize:
        case ProductType::RaceChange:
        case ProductType::FactionChange:
        case ProductType::Teleport:
        case ProductType::Vip:
        case ProductType::Custom:
            return true;
        default:
            return false;
    }
}
} // namespace

namespace AccountShop
{
ShopService& ShopService::Instance()
{
    static ShopService instance;
    return instance;
}

void ShopService::SendPayload(ChatHandler* handler, std::string const& payload) const
{
    constexpr std::size_t DirectPayloadLimit = 180;
    constexpr std::size_t ChunkSize = 140;

    if (payload.size() <= DirectPayloadLimit)
    {
        handler->SendSysMessage(Acore::StringFormat("ASHOP\t{}", payload), false);
        return;
    }

    std::string const encoded = HexEncode(payload);
    uint32 const messageId = MessageSequence.fetch_add(1, std::memory_order_relaxed);
    uint32 const total = static_cast<uint32>((encoded.size() + ChunkSize - 1) / ChunkSize);

    for (uint32 index = 0; index < total; ++index)
    {
        std::size_t const offset = static_cast<std::size_t>(index) * ChunkSize;
        std::string const piece = encoded.substr(offset, ChunkSize);
        handler->SendSysMessage(Acore::StringFormat("ASHOP\tCHUNK\t{}\t{}\t{}\t{}", messageId, index + 1, total, piece),
                                false);
    }
}

void ShopService::SendResult(ChatHandler* handler, bool success, std::string const& message) const
{
    SendPayload(handler, Acore::StringFormat("RESULT\t{}\t{}", success ? 1 : 0, EscapeField(message)));
}

void ShopService::SendOpen(ChatHandler* handler) const
{
    handler->SendSysMessage("ASHOP_OPEN", false);
}

void ShopService::SendBalance(ChatHandler* handler, uint32 accountId) const
{
    LoginDatabase.DirectExecute("INSERT IGNORE INTO `account_currency` (`account_id`) VALUES ({})", accountId);

    QueryResult result = LoginDatabase.Query("SELECT `donate_balance`, `vote_balance` FROM "
                                             "`account_currency` WHERE `account_id` = {}",
                                             accountId);

    uint64 donateBalance = 0;
    uint64 voteBalance = 0;
    if (result)
    {
        Field* fields = result->Fetch();
        donateBalance = fields[0].Get<uint64>();
        voteBalance = fields[1].Get<uint64>();
    }

    SendPayload(handler, Acore::StringFormat("BALANCE\t{}\t{}", donateBalance, voteBalance));
}

void ShopService::SendSync(ChatHandler* handler) const
{
    if (!handler->GetSession())
        return;

    uint32 const accountId = handler->GetSession()->GetAccountId();
    SendPayload(handler, "SYNC_BEGIN");
    SendBalance(handler, accountId);

    QueryResult categories =
        LoginDatabase.Query("SELECT `id`, `parent_id`, `sort_order`, `name`, `icon` "
                            "FROM `account_shop_category` WHERE `enabled` = 1 ORDER BY `sort_order`, "
                            "`id`");

    if (categories)
    {
        do
        {
            Field* fields = categories->Fetch();
            SendPayload(handler,
                        Acore::StringFormat("CATEGORY\t{}\t{}\t{}\t{}\t{}",
                                            fields[0].Get<uint32>(),
                                            fields[1].Get<uint32>(),
                                            fields[2].Get<int32>(),
                                            EscapeField(fields[3].Get<std::string>()),
                                            EscapeField(fields[4].Get<std::string>())));
        } while (categories->NextRow());
    }

    SendPayload(handler, "SYNC_END");
}

void ShopService::SendProductPage(
    ChatHandler* handler, uint32 categoryId, uint32 page, std::string search, uint8 equipmentSlot) const
{
    if (!handler->GetSession())
        return;

    if (search.size() > 64)
        search.resize(64);

    std::string where = "`enabled` = 1";
    if (categoryId == 0)
    {
        where += " AND `featured` = 1";
    }
    else
    {
        // Показываем товары самой категории и всех её прямых
        // подкатегорий. Например, Коллекция (2) включает товары
        // из категорий 2, 3 (Транспорт) и 4 (Питомцы).
        where += Acore::StringFormat(
            " AND (`category_id` = {} OR `category_id` IN ("
            "SELECT `id` FROM `account_shop_category` "
            "WHERE `parent_id` = {} AND `enabled` = 1))",
            categoryId,
            categoryId);
    }

    if (!search.empty())
    {
        LoginDatabase.EscapeString(search);
        where += Acore::StringFormat(" AND (`name` LIKE '%{}%' OR `description` LIKE '%{}%')", search, search);
    }

    // Для экрана экипировки фильтруем товары по InventoryType из item_template.
    // Дополнительная SQL-колонка магазину не требуется.
    if (categoryId == 1 && equipmentSlot > 0)
    {
        QueryResult candidates = LoginDatabase.Query(Acore::StringFormat(
            "SELECT `id`, `product_type`, `reference_id` FROM `account_shop_product` WHERE {}",
            where));

        std::string matchingIds;
        if (candidates)
        {
            do
            {
                Field* fields = candidates->Fetch();
                uint32 const productId = fields[0].Get<uint32>();
                uint8 const productType = fields[1].Get<uint8>();
                uint32 const itemId = fields[2].Get<uint32>();

                if (productType != static_cast<uint8>(ProductType::Item))
                    continue;

                ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
                if (!itemTemplate || !MatchesEquipmentSlot(itemTemplate->InventoryType, equipmentSlot))
                    continue;

                if (!matchingIds.empty())
                    matchingIds += ',';
                matchingIds += std::to_string(productId);
            } while (candidates->NextRow());
        }

        if (matchingIds.empty())
            where += " AND 1 = 0";
        else
            where += Acore::StringFormat(" AND `id` IN ({})", matchingIds);
    }

    QueryResult countResult =
        LoginDatabase.Query(Acore::StringFormat("SELECT COUNT(*) FROM `account_shop_product` WHERE {}", where));
    uint64 const productCount = countResult ? countResult->Fetch()[0].Get<uint64>() : 0;
	bool const collectionCategory =
    categoryId == 2 ||
    categoryId == 3 ||
    categoryId == 4;

uint32 const pageSize =
    collectionCategory ? CollectionPageSize : PageSize;
uint32 const pageCount = std::max<uint32>(1, static_cast<uint32>((productCount + pageSize - 1) / pageSize));
page = std::min(page, pageCount - 1);
uint32 const offset = page * pageSize;

    SendPayload(handler, Acore::StringFormat("LIST_BEGIN\t{}\t{}\t{}", categoryId, page, pageCount));

    QueryResult products = LoginDatabase.Query(Acore::StringFormat("SELECT `id`, `category_id`, `product_type`, "
                                                                   "`reference_id`, `price`, `currency_type`, "
                                                                   "`allow_quantity`, `max_quantity`, `name`, "
                                                                   "LEFT(COALESCE(`description`, ''), 800), `icon`, `creature_entry` "
                                                                   "FROM `account_shop_product` WHERE {} ORDER BY "
                                                                   "`sort_order`, `id` LIMIT {} OFFSET {}",
                                                                   where,
                                                                   pageSize,
                                                                   offset));

    if (products)
    {
        do
        {
            Field* fields = products->Fetch();
            uint8 const productType = fields[2].Get<uint8>();
            uint32 const creatureEntry = fields[11].Get<uint32>();

            if ((productType == static_cast<uint8>(ProductType::Mount) ||
                 productType == static_cast<uint8>(ProductType::Pet)) &&
                creatureEntry != 0)
            {
                CacheCreatureForShopPreview(handler->GetPlayer(), creatureEntry);
            }

            SendPayload(handler,
                        Acore::StringFormat("PRODUCT\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}",
                                            fields[0].Get<uint32>(),
                                            fields[1].Get<uint32>(),
                                            fields[2].Get<uint8>(),
                                            fields[3].Get<uint32>(),
                                            fields[4].Get<uint64>(),
                                            fields[5].Get<uint8>(),
                                            fields[6].Get<uint8>(),
                                            fields[7].Get<uint32>(),
                                            EscapeField(fields[8].Get<std::string>()),
                                            EscapeField(fields[9].Get<std::string>()),
                                            EscapeField(
    productType == static_cast<uint8>(ProductType::Item)
    ? GetItemIcon(fields[3].Get<uint32>())
    : fields[10].Get<std::string>()
),
                                            fields[11].Get<uint32>()));
        } while (products->NextRow());
    }

    SendPayload(handler, "LIST_END");
}

bool ShopService::LoadProduct(uint32 productId, Product& product) const
{
    QueryResult result = LoginDatabase.Query("SELECT `id`, `category_id`, `product_type`, `reference_id`, "
                                             "`reward_amount`, `currency_type`, `price`, "
                                             "`allow_quantity`, `max_quantity`, `account_limit`, `character_limit`, "
                                             "`name`, `map_id`, `position_x`, "
                                             "`position_y`, `position_z`, `orientation`, `custom_value` "
                                             "FROM `account_shop_product` WHERE `id` = {} AND `enabled` = 1",
                                             productId);

    if (!result)
        return false;

    Field* fields = result->Fetch();
    product.Id = fields[0].Get<uint32>();
    product.CategoryId = fields[1].Get<uint32>();
    product.Type = static_cast<ProductType>(fields[2].Get<uint8>());
    product.ReferenceId = fields[3].Get<uint32>();
    product.RewardAmount = fields[4].Get<uint64>();
    product.Currency = static_cast<CurrencyType>(fields[5].Get<uint8>());
    product.Price = fields[6].Get<uint64>();
    product.AllowQuantity = fields[7].Get<uint8>() != 0;
    product.MaxQuantity = fields[8].Get<uint32>();
    product.AccountLimit = fields[9].Get<uint32>();
    product.CharacterLimit = fields[10].Get<uint32>();
    product.Name = fields[11].Get<std::string>();
    product.MapId = fields[12].Get<uint32>();
    product.PositionX = fields[13].Get<float>();
    product.PositionY = fields[14].Get<float>();
    product.PositionZ = fields[15].Get<float>();
    product.Orientation = fields[16].Get<float>();
    product.CustomValue = fields[17].Get<uint64>();
    return true;
}

bool ShopService::LoadItemRewards(Product const& product,
                                  uint32 quantity,
                                  ItemRewards& rewards,
                                  std::string& error) const
{
    if (product.Type == ProductType::Item)
    {
        if (!product.ReferenceId || !product.RewardAmount)
        {
            error = "Для товара не настроена награда.";
            return false;
        }

        if (product.RewardAmount > std::numeric_limits<uint64>::max() / quantity)
        {
            error = "Количество награды слишком велико.";
            return false;
        }

        rewards.emplace_back(product.ReferenceId, product.RewardAmount * quantity);
    }
    else if (product.Type == ProductType::ItemBundle)
    {
        QueryResult result = LoginDatabase.Query("SELECT `item_id`, `item_count` FROM `account_shop_product_item` "
                                                 "WHERE `product_id` = {} ORDER BY `sort_order`, `item_id`",
                                                 product.Id);

        if (!result)
        {
            error = "Комплект не содержит предметов.";
            return false;
        }

        do
        {
            Field* fields = result->Fetch();
            uint32 const itemId = fields[0].Get<uint32>();
            uint64 const count = fields[1].Get<uint32>();
            if (!count || count > std::numeric_limits<uint64>::max() / quantity)
            {
                error = "Количество предметов в комплекте некорректно.";
                return false;
            }

            rewards.emplace_back(itemId, count * quantity);
        } while (result->NextRow());
    }

    uint32 stackCount = 0;
    for (auto const& [itemId, count] : rewards)
    {
        ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
        if (!itemTemplate || !count)
        {
            error = Acore::StringFormat("Предмет {} не существует.", itemId);
            return false;
        }

        if (itemTemplate->MaxCount > 0 && count > static_cast<uint64>(itemTemplate->MaxCount))
        {
            error = Acore::StringFormat(
                "Количество предмета {} превышает его серверный лимит.",
                itemId);
            return false;
        }

        uint32 const maxStack = std::max<uint32>(1, itemTemplate->GetMaxStackSize());
        uint64 const stacks = (count + maxStack - 1) / maxStack;
        if (stacks > MaxMailStacks || stackCount + stacks > MaxMailStacks)
        {
            error = "Покупка создаёт слишком много почтовых вложений.";
            return false;
        }

        stackCount += static_cast<uint32>(stacks);
    }

    return true;
}

bool ShopService::ValidateProduct(
    Player* player, Product const& product, uint32 quantity, ItemRewards& rewards, std::string& error) const
{
    if (!quantity || quantity > HardMaxQuantity)
    {
        error = "Недопустимое количество.";
        return false;
    }

    uint32 const configuredMaximum = std::clamp(product.MaxQuantity, 1u, HardMaxQuantity);
    if ((!product.AllowQuantity && quantity != 1) || quantity > configuredMaximum ||
        (IsUniqueProduct(product.Type) && quantity != 1))
    {
        error = "Это количество не разрешено для выбранного товара.";
        return false;
    }

    if (!product.Price || product.Price > std::numeric_limits<uint64>::max() / quantity)
    {
        error = "Цена товара настроена неверно.";
        return false;
    }

    if (product.Currency != CurrencyType::Donate && product.Currency != CurrencyType::Vote)
    {
        error = "Валюта товара настроена неверно.";
        return false;
    }

    uint32 const accountId = player->GetSession()->GetAccountId();
    uint32 const guidLow = player->GetGUID().GetCounter();

    if (product.AccountLimit)
    {
        QueryResult result = LoginDatabase.Query("SELECT COUNT(*) FROM `account_shop_purchase` "
                                                 "WHERE `account_id` = {} AND `product_id` = {} "
                                                 "AND `status` IN ('DEBITED', 'DELIVERED')",
                                                 accountId,
                                                 product.Id);
        if (result && result->Fetch()[0].Get<uint64>() >= product.AccountLimit)
        {
            error = "Лимит покупок этого товара для аккаунта исчерпан.";
            return false;
        }
    }

    if (product.CharacterLimit)
    {
        QueryResult result = LoginDatabase.Query("SELECT COUNT(*) FROM `account_shop_purchase` "
                                                 "WHERE `character_guid` = {} AND `product_id` = {} "
                                                 "AND `status` IN ('DEBITED', 'DELIVERED')",
                                                 guidLow,
                                                 product.Id);
        if (result && result->Fetch()[0].Get<uint64>() >= product.CharacterLimit)
        {
            error = "Лимит покупок этого товара для персонажа исчерпан.";
            return false;
        }
    }

    switch (product.Type)
    {
        case ProductType::Item:
        case ProductType::ItemBundle:
            return LoadItemRewards(product, quantity, rewards, error);
        case ProductType::Spell:
        case ProductType::Mount:
        case ProductType::Pet:
            if (!product.ReferenceId || !sSpellMgr->GetSpellInfo(product.ReferenceId))
            {
                error = "Заклинание товара не существует.";
                return false;
            }
            if (player->HasSpell(product.ReferenceId))
            {
                error = "Это заклинание уже изучено.";
                return false;
            }
            return true;
        case ProductType::Gold:
        {
            if (!product.RewardAmount || product.RewardAmount > std::numeric_limits<uint64>::max() / quantity)
            {
                error = "Количество золота настроено неверно.";
                return false;
            }
            uint64 const money = product.RewardAmount * quantity;
            if (money > MAX_MONEY_AMOUNT || player->GetMoney() > MAX_MONEY_AMOUNT - money)
            {
                error = "Персонаж не может получить столько золота.";
                return false;
            }
            return true;
        }
        case ProductType::Level:
        {
            uint32 const targetLevel = static_cast<uint32>(product.RewardAmount);
            if (!targetLevel || targetLevel > sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL) ||
                player->GetLevel() >= targetLevel)
            {
                error = "Указанный уровень нельзя применить к персонажу.";
                return false;
            }
            return true;
        }
        case ProductType::Rename:
        case ProductType::Customize:
        case ProductType::RaceChange:
        case ProductType::FactionChange:
            return true;
        case ProductType::Teleport:
            if (!player->IsAlive() || player->IsInCombat() || player->IsInFlight() || player->GetTransport())
            {
                error = "Телепортация сейчас недоступна.";
                return false;
            }
            if (!sMapStore.LookupEntry(product.MapId) || !std::isfinite(product.PositionX) ||
                !std::isfinite(product.PositionY) || !std::isfinite(product.PositionZ) ||
                !std::isfinite(product.Orientation))
            {
                error = "Координаты телепорта настроены неверно.";
                return false;
            }
            return true;
        case ProductType::Vip:
            if (!product.RewardAmount || product.RewardAmount > 3650)
            {
                error = "Срок VIP настроен неверно.";
                return false;
            }
            return true;
        case ProductType::Custom:
            return DeliverCustom(player, Purchase{}, error);
        default:
            error = "Неизвестный тип товара.";
            return false;
    }
}

bool ShopService::CreateAndDebitPurchase(Player* player,
                                         Product const& product,
                                         uint32 quantity,
                                         std::string const& token,
                                         Purchase& purchase,
                                         std::string& error) const
{
    uint32 const accountId = player->GetSession()->GetAccountId();
    uint32 const guidLow = player->GetGUID().GetCounter();
    LoginDatabase.DirectExecute("INSERT IGNORE INTO `account_currency` (`account_id`) VALUES ({})", accountId);

    LoginDatabase.DirectExecute("INSERT INTO `account_shop_purchase` "
                                "(`request_token`, `account_id`, `character_guid`, `product_id`, "
                                "`product_type`, `reference_id`, "
                                "`reward_amount`, `quantity`, `currency_type`, `unit_price`, "
                                "`total_price`, `map_id`, `position_x`, "
                                "`position_y`, `position_z`, `orientation`, `custom_value`, `status`) "
                                "SELECT '{}', {}, {}, `id`, `product_type`, `reference_id`, "
                                "`reward_amount`, {}, `currency_type`, `price`, "
                                "`price` * {}, `map_id`, `position_x`, `position_y`, `position_z`, "
                                "`orientation`, `custom_value`, 'CREATED' "
                                "FROM `account_shop_product` WHERE `id` = {} AND `enabled` = 1 AND "
                                "`price` > 0 "
                                "AND `currency_type` IN (1, 2) AND ((`allow_quantity` = 1 AND {} BETWEEN "
                                "1 AND LEAST(`max_quantity`, {})) "
                                "OR (`allow_quantity` = 0 AND {} = 1))",
                                token,
                                accountId,
                                guidLow,
                                quantity,
                                quantity,
                                product.Id,
                                quantity,
                                HardMaxQuantity,
                                quantity);

    QueryResult created =
        LoginDatabase.Query("SELECT `id` FROM `account_shop_purchase` WHERE `request_token` = '{}'", token);
    if (!created)
    {
        error = "Товар изменился или больше недоступен.";
        return false;
    }

    uint64 const purchaseId = created->Fetch()[0].Get<uint64>();
    if (product.Type == ProductType::ItemBundle)
    {
        LoginDatabase.DirectExecute("INSERT INTO `account_shop_purchase_item` (`purchase_id`, `item_id`, "
                                    "`item_count`) "
                                    "SELECT {}, `item_id`, `item_count` * {} FROM "
                                    "`account_shop_product_item` WHERE `product_id` = {}",
                                    purchaseId,
                                    quantity,
                                    product.Id);
    }

    LoginDatabase.DirectExecute("UPDATE `account_currency` AS c "
                                "INNER JOIN `account_shop_purchase` AS p ON p.`account_id` = "
                                "c.`account_id` "
                                "SET c.`donate_balance` = c.`donate_balance` - IF(p.`currency_type` = 1, "
                                "p.`total_price`, 0), "
                                "c.`vote_balance` = c.`vote_balance` - IF(p.`currency_type` = 2, "
                                "p.`total_price`, 0), "
                                "p.`status` = 'DEBITED' "
                                "WHERE p.`id` = {} AND p.`status` = 'CREATED' "
                                "AND ((p.`currency_type` = 1 AND c.`donate_balance` >= p.`total_price`) "
                                "OR (p.`currency_type` = 2 AND c.`vote_balance` >= p.`total_price`))",
                                purchaseId);

    QueryResult result = LoginDatabase.Query("SELECT `id`, `product_type`, `reference_id`, `reward_amount`, "
                                             "`quantity`, `currency_type`, "
                                             "`total_price`, `map_id`, `position_x`, `position_y`, `position_z`, "
                                             "`orientation`, `custom_value`, `status` "
                                             "FROM `account_shop_purchase` WHERE `id` = {}",
                                             purchaseId);

    if (!result)
    {
        error = "Не удалось создать покупку.";
        return false;
    }

    Field* fields = result->Fetch();
    purchase.Id = fields[0].Get<uint64>();
    purchase.Type = static_cast<ProductType>(fields[1].Get<uint8>());
    purchase.ReferenceId = fields[2].Get<uint32>();
    purchase.RewardAmount = fields[3].Get<uint64>();
    purchase.Quantity = fields[4].Get<uint32>();
    purchase.Currency = static_cast<CurrencyType>(fields[5].Get<uint8>());
    purchase.TotalPrice = fields[6].Get<uint64>();
    purchase.MapId = fields[7].Get<uint32>();
    purchase.PositionX = fields[8].Get<float>();
    purchase.PositionY = fields[9].Get<float>();
    purchase.PositionZ = fields[10].Get<float>();
    purchase.Orientation = fields[11].Get<float>();
    purchase.CustomValue = fields[12].Get<uint64>();
    purchase.Status = fields[13].Get<std::string>();

    if (purchase.Status != "DEBITED")
    {
        LoginDatabase.DirectExecute("UPDATE `account_shop_purchase` SET `status` = 'FAILED', `error_text` "
                                    "= 'Insufficient balance', "
                                    "`completed_at` = NOW() WHERE `id` = {} AND `status` = 'CREATED'",
                                    purchaseId);
        error = "Недостаточно валюты для покупки.";
        return false;
    }

    return true;
}

bool ShopService::DeliverItemsByMail(Player* player, ItemRewards const& rewards, std::string& error) const
{
    std::vector<std::pair<uint32, uint32>> stacks;
    stacks.reserve(MaxMailStacks);

    for (auto const& [itemId, totalCount] : rewards)
    {
        ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
        if (!itemTemplate)
        {
            error = Acore::StringFormat("Предмет {} не существует.", itemId);
            return false;
        }

        uint64 remaining = totalCount;
        uint32 const maxStack = std::max<uint32>(1, itemTemplate->GetMaxStackSize());
        while (remaining)
        {
            uint32 const count = static_cast<uint32>(std::min<uint64>(remaining, maxStack));
            stacks.emplace_back(itemId, count);
            remaining -= count;
        }
    }

    if (stacks.empty() || stacks.size() > MaxMailStacks)
    {
        error = "Неверное количество почтовых вложений.";
        return false;
    }

    CharacterDatabaseTransaction transaction = CharacterDatabase.BeginTransaction();
    std::size_t offset = 0;
    while (offset < stacks.size())
    {
        MailDraft draft(
            "Покупка в магазине",
            "Спасибо за покупку. Предметы приложены к письму.");
        uint32 attached = 0;
        while (offset < stacks.size() && attached < MAX_MAIL_ITEMS)
        {
            auto const& [itemId, count] = stacks[offset++];
            Item* item = Item::CreateItem(itemId, count, player);
            if (!item)
            {
                error = Acore::StringFormat("Не удалось создать предмет {}.", itemId);
                return false;
            }

            item->SaveToDB(transaction);
            draft.AddItem(item);
            ++attached;
        }

        draft.SendMailTo(transaction,
                         MailReceiver(player, player->GetGUID().GetCounter()),
                         MailSender(MAIL_NORMAL, player->GetGUID().GetCounter(), MAIL_STATIONERY_GM));
    }

    // Item and mail prepared statements are registered for CONNECTION_ASYNC.
    // DirectCommitTransaction executes on the synchronous connection and
    // asserts when CHAR_REP_ITEM_INSTANCE is requested. Use the same async
    // commit path as AzerothCore's built-in mail commands.
    CharacterDatabase.CommitTransaction(transaction);
    return true;
}

bool ShopService::DeliverCustom(Player* /*player*/, Purchase const& /*purchase*/, std::string& error) const
{
    // Add server-specific custom operations here and key them by
    // Purchase::ReferenceId.
    error =
        "Для пользовательской услуги ещё не назначен C++-обработчик.";
    return false;
}

bool ShopService::Deliver(Player* player,
                          Purchase const& purchase,
                          ItemRewards const& rewards,
                          std::string& error) const
{
    switch (purchase.Type)
    {
        case ProductType::Item:
        case ProductType::ItemBundle:
            return DeliverItemsByMail(player, rewards, error);
        case ProductType::Spell:
        case ProductType::Mount:
        case ProductType::Pet:
            if (player->HasSpell(purchase.ReferenceId))
            {
                error = "Заклинание уже изучено.";
                return false;
            }
            player->learnSpell(purchase.ReferenceId, false);
            player->SaveToDB(false, false);
            return true;
        case ProductType::Gold:
        {
            if (!purchase.RewardAmount ||
                purchase.RewardAmount > std::numeric_limits<uint64>::max() / purchase.Quantity)
            {
                error = "Количество золота настроено неверно.";
                return false;
            }
            uint64 const money = purchase.RewardAmount * purchase.Quantity;
            if (money > MAX_MONEY_AMOUNT || player->GetMoney() > MAX_MONEY_AMOUNT - money)
            {
                error = "Персонаж не может получить столько золота.";
                return false;
            }
            player->ModifyMoney(static_cast<int32>(money));
            player->SaveToDB(false, false);
            return true;
        }
        case ProductType::Level:
        {
            uint32 const targetLevel = static_cast<uint32>(purchase.RewardAmount);
            if (!targetLevel || targetLevel > sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL) ||
                player->GetLevel() >= targetLevel)
            {
                error = "Уровень нельзя применить.";
                return false;
            }
            player->GiveLevel(static_cast<uint8>(targetLevel));
            player->InitTalentForLevel();
            player->SaveToDB(false, false);
            return true;
        }
        case ProductType::Rename:
            player->SetAtLoginFlag(AT_LOGIN_RENAME);
            player->SaveToDB(false, false);
            return true;
        case ProductType::Customize:
            player->SetAtLoginFlag(AT_LOGIN_CUSTOMIZE);
            player->SaveToDB(false, false);
            return true;
        case ProductType::RaceChange:
            player->SetAtLoginFlag(AT_LOGIN_CHANGE_RACE);
            player->SaveToDB(false, false);
            return true;
        case ProductType::FactionChange:
            player->SetAtLoginFlag(AT_LOGIN_CHANGE_FACTION);
            player->SaveToDB(false, false);
            return true;
        case ProductType::Teleport:
            if (!player->TeleportTo(
                    purchase.MapId, purchase.PositionX, purchase.PositionY, purchase.PositionZ, purchase.Orientation))
            {
                error = "Телепортация не выполнена.";
                return false;
            }
            return true;
        case ProductType::Vip:
        {
            uint64 const seconds = purchase.RewardAmount * 86400;
            uint32 const accountId = player->GetSession()->GetAccountId();
            LoginDatabase.DirectExecute("INSERT INTO `account_shop_entitlement` (`account_id`, "
                                        "`entitlement_type`, `expires_at`, `value`) "
                                        "VALUES ({}, 'VIP', FROM_UNIXTIME(UNIX_TIMESTAMP() + {}), {}) "
                                        "ON DUPLICATE KEY UPDATE `expires_at` = "
                                        "FROM_UNIXTIME(GREATEST(UNIX_TIMESTAMP(), "
                                        "UNIX_TIMESTAMP(COALESCE(`expires_at`, NOW()))) + {}), `value` = "
                                        "VALUES(`value`)",
                                        accountId,
                                        seconds,
                                        purchase.CustomValue,
                                        seconds);
            return true;
        }
        case ProductType::Custom:
            return DeliverCustom(player, purchase, error);
        default:
            error = "Неизвестный тип товара.";
            return false;
    }
}

void ShopService::MarkDelivered(uint64 purchaseId) const
{
    LoginDatabase.DirectExecute("UPDATE `account_shop_purchase` SET `status` = 'DELIVERED', "
                                "`completed_at` = NOW(), `error_text` = '' "
                                "WHERE `id` = {} AND `status` = 'DEBITED'",
                                purchaseId);
}

void ShopService::Refund(uint64 purchaseId, std::string error) const
{
    if (error.size() > 240)
        error.resize(240);
    LoginDatabase.EscapeString(error);

    LoginDatabase.DirectExecute("UPDATE `account_currency` AS c "
                                "INNER JOIN `account_shop_purchase` AS p ON "
                                "p.`account_id` = c.`account_id` "
                                "SET c.`donate_balance` = c.`donate_balance` + "
                                "IF(p.`currency_type` = 1, p.`total_price`, 0), "
                                "c.`vote_balance` = c.`vote_balance` + "
                                "IF(p.`currency_type` = 2, p.`total_price`, 0), "
                                "p.`status` = 'REFUNDED', p.`error_text` = '{}', "
                                "p.`completed_at` = NOW() "
                                "WHERE p.`id` = {} AND p.`status` = 'DEBITED'",
                                error,
                                purchaseId);
}

void ShopService::Buy(ChatHandler* handler, uint32 productId, uint32 quantity) const
{
    if (!handler->GetSession() || !handler->GetPlayer())
        return;

    Player* player = handler->GetPlayer();
    Product product;
    ItemRewards rewards;
    std::string error;

    if (!LoadProduct(productId, product))
    {
        SendResult(handler, false, "Товар не найден или отключён.");
        return;
    }

    if (!ValidateProduct(player, product, quantity, rewards, error))
    {
        SendResult(handler, false, error);
        return;
    }

    Purchase purchase;
    std::string const token = MakeRequestToken(player);
    if (!CreateAndDebitPurchase(player, product, quantity, token, purchase, error))
    {
        SendResult(handler, false, error);
        SendBalance(handler, player->GetSession()->GetAccountId());
        return;
    }

    if (!Deliver(player, purchase, rewards, error))
    {
        Refund(purchase.Id, error);
        LOG_ERROR("module.account-shop",
                  "Purchase {} delivery failed for account {}: {}",
                  purchase.Id,
                  player->GetSession()->GetAccountId(),
                  error);
        SendResult(
            handler,
            false,
            Acore::StringFormat(
                "Покупка отменена, валюта возвращена: {}",
                error));
        SendBalance(handler, player->GetSession()->GetAccountId());
        return;
    }

    MarkDelivered(purchase.Id);
    LOG_INFO("module.account-shop",
             "Purchase {} delivered to account {}, character {}",
             purchase.Id,
             player->GetSession()->GetAccountId(),
             player->GetGUID().GetCounter());
    SendResult(handler, true, "Покупка успешно выполнена.");
    SendBalance(handler, player->GetSession()->GetAccountId());
}
} // namespace AccountShop
