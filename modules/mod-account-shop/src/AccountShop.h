#ifndef MOD_ACCOUNT_SHOP_H
#define MOD_ACCOUNT_SHOP_H

#include "Define.h"

#include <string>
#include <utility>
#include <vector>

class ChatHandler;
class Player;

namespace AccountShop
{
constexpr uint32 CommandPermission = 1998;
constexpr uint32 AdminPermission = 1999;
constexpr uint32 PageSize = 8;             // все страницы
constexpr uint32 CollectionPageSize = 6;   // только Коллекция
constexpr uint32 HardMaxQuantity = 1000;
constexpr uint32 MaxMailStacks = 120;

enum class CurrencyType : uint8
{
    Donate = 1,
    Vote = 2
};

enum class ProductType : uint8
{
    Item = 1,
    ItemBundle = 2,
    Spell = 3,
    Mount = 4,
    Pet = 5,
    Gold = 6,
    Level = 7,
    Rename = 8,
    Customize = 9,
    RaceChange = 10,
    FactionChange = 11,
    Teleport = 12,
    Vip = 13,
    Custom = 255
};

struct Product
{
    uint32 Id = 0;
    uint32 CategoryId = 0;
    ProductType Type = ProductType::Item;
    uint32 ReferenceId = 0;
    uint64 RewardAmount = 1;
    CurrencyType Currency = CurrencyType::Donate;
    uint64 Price = 0;
    bool AllowQuantity = false;
    uint32 MaxQuantity = 1;
    uint32 AccountLimit = 0;
    uint32 CharacterLimit = 0;
    std::string Name;
    uint32 MapId = 0;
    float PositionX = 0.0f;
    float PositionY = 0.0f;
    float PositionZ = 0.0f;
    float Orientation = 0.0f;
    uint64 CustomValue = 0;
};

struct Purchase
{
    uint64 Id = 0;
    ProductType Type = ProductType::Item;
    uint32 ReferenceId = 0;
    uint64 RewardAmount = 1;
    uint32 Quantity = 1;
    CurrencyType Currency = CurrencyType::Donate;
    uint64 TotalPrice = 0;
    uint32 MapId = 0;
    float PositionX = 0.0f;
    float PositionY = 0.0f;
    float PositionZ = 0.0f;
    float Orientation = 0.0f;
    uint64 CustomValue = 0;
    std::string Status;
};

using ItemReward = std::pair<uint32, uint64>;
using ItemRewards = std::vector<ItemReward>;

class ShopService
{
public:
    static ShopService& Instance();

    void SendOpen(ChatHandler* handler) const;
    void SendSync(ChatHandler* handler) const;
    void SendProductPage(
        ChatHandler* handler, uint32 categoryId, uint32 page, std::string search, uint8 equipmentSlot = 0) const;
    void Buy(ChatHandler* handler, uint32 productId, uint32 quantity) const;

private:
    ShopService() = default;

    void SendBalance(ChatHandler* handler, uint32 accountId) const;
    void SendPayload(ChatHandler* handler, std::string const& payload) const;
    void SendResult(ChatHandler* handler, bool success, std::string const& message) const;

    bool LoadProduct(uint32 productId, Product& product) const;
    bool ValidateProduct(
        Player* player, Product const& product, uint32 quantity, ItemRewards& rewards, std::string& error) const;
    bool LoadItemRewards(Product const& product, uint32 quantity, ItemRewards& rewards, std::string& error) const;
    bool CreateAndDebitPurchase(Player* player,
                                Product const& product,
                                uint32 quantity,
                                std::string const& token,
                                Purchase& purchase,
                                std::string& error) const;
    bool Deliver(Player* player, Purchase const& purchase, ItemRewards const& rewards, std::string& error) const;
    bool DeliverItemsByMail(Player* player, ItemRewards const& rewards, std::string& error) const;
    bool DeliverCustom(Player* player, Purchase const& purchase, std::string& error) const;

    void MarkDelivered(uint64 purchaseId) const;
    void Refund(uint64 purchaseId, std::string error) const;
};
} // namespace AccountShop

#endif
