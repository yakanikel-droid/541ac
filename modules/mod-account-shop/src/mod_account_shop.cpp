#include "AccountShop.h"

#include "Chat.h"
#include "CommandScript.h"
#include "DatabaseEnv.h"
#include "ScriptMgr.h"
#include "StringConvert.h"
#include "Tokenize.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

using namespace Acore::ChatCommands;

namespace
{
std::optional<uint8> HexValue(char value)
{
    if (value >= '0' && value <= '9')
        return static_cast<uint8>(value - '0');
    if (value >= 'A' && value <= 'F')
        return static_cast<uint8>(value - 'A' + 10);
    if (value >= 'a' && value <= 'f')
        return static_cast<uint8>(value - 'a' + 10);
    return std::nullopt;
}

std::optional<std::string> DecodeHex(std::string_view value)
{
    if (value == "-")
        return std::string();
    if (value.size() % 2 != 0 || value.size() > 128)
        return std::nullopt;

    std::string result;
    result.reserve(value.size() / 2);
    for (std::size_t index = 0; index < value.size(); index += 2)
    {
        std::optional<uint8> high = HexValue(value[index]);
        std::optional<uint8> low = HexValue(value[index + 1]);
        if (!high || !low)
            return std::nullopt;
        result.push_back(static_cast<char>((*high << 4) | *low));
    }

    return result;
}

class AccountShopCommandScript : public CommandScript
{
public:
    AccountShopCommandScript() : CommandScript("AccountShopCommandScript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable lifetimeCommands =
        {
            { "add", HandleLifetimeAdd, AccountShop::AdminPermission, Console::Yes },
            { "set", HandleLifetimeSet, AccountShop::AdminPermission, Console::Yes }
        };

        static ChatCommandTable accountCommands =
        {
            { "add", HandleAccountAdd, AccountShop::AdminPermission, Console::Yes },
            { "set", HandleAccountSet, AccountShop::AdminPermission, Console::Yes },
            { "get", HandleAccountGet, AccountShop::AdminPermission, Console::Yes },
            { "lifetime", lifetimeCommands }
        };

        static ChatCommandTable shopCommands =
        {
            { "account", accountCommands },
            { "sync", HandleSync, AccountShop::CommandPermission, Console::No },
            { "list", HandleList, AccountShop::CommandPermission, Console::No },
            { "buy", HandleBuy, AccountShop::CommandPermission, Console::No },
            { "", HandleOpen, AccountShop::CommandPermission, Console::No }
        };

        static ChatCommandTable commandTable =
        {
            { "shop", shopCommands }
        };

        return commandTable;
    }

    static bool HandleOpen(ChatHandler* handler, char const* /*args*/)
    {
        AccountShop::ShopService::Instance().SendOpen(handler);
        return true;
    }

    static bool HandleSync(ChatHandler* handler, char const* /*args*/)
    {
        AccountShop::ShopService::Instance().SendSync(handler);
        return true;
    }

    static bool HandleList(ChatHandler* handler, char const* args)
    {
        auto tokens = Acore::Tokenize(args ? std::string_view(args) : std::string_view(), ' ', false);
        if (tokens.size() < 2 || tokens.size() > 4)
        {
            handler->SendErrorMessage("ASHOP_BAD_LIST_REQUEST");
            return false;
        }

        auto categoryId = Acore::StringTo<uint32>(tokens[0]);
        auto page = Acore::StringTo<uint32>(tokens[1]);
        std::optional<std::string> search = tokens.size() >= 3 ? DecodeHex(tokens[2]) : std::string();
        auto equipmentSlot = tokens.size() == 4 ? Acore::StringTo<uint8>(tokens[3]) : std::optional<uint8>(0);
        if (!categoryId || !page || !search || !equipmentSlot || *equipmentSlot > 15)
        {
            handler->SendErrorMessage("ASHOP_BAD_LIST_REQUEST");
            return false;
        }

        AccountShop::ShopService::Instance().SendProductPage(
            handler, *categoryId, *page, std::move(*search), *equipmentSlot);
        return true;
    }

    static bool HandleBuy(ChatHandler* handler, char const* args)
    {
        auto tokens = Acore::Tokenize(args ? std::string_view(args) : std::string_view(), ' ', false);
        if (tokens.size() != 2)
        {
            handler->SendErrorMessage("ASHOP_BAD_BUY_REQUEST");
            return false;
        }

        auto productId = Acore::StringTo<uint32>(tokens[0]);
        auto quantity = Acore::StringTo<uint32>(tokens[1]);
        if (!productId || !quantity)
        {
            handler->SendErrorMessage("ASHOP_BAD_BUY_REQUEST");
            return false;
        }

        AccountShop::ShopService::Instance().Buy(handler, *productId, *quantity);
        return true;
    }

    static bool ParseAccountAmount(ChatHandler* handler, char const* args, uint32& accountId, uint64& amount)
    {
        auto tokens = Acore::Tokenize(args ? std::string_view(args) : std::string_view(), ' ', false);
        if (tokens.size() != 2)
        {
            handler->SendErrorMessage("Syntax: <account_id> <amount>");
            return false;
        }

        auto parsedAccountId = Acore::StringTo<uint32>(tokens[0]);
        auto parsedAmount = Acore::StringTo<uint64>(tokens[1]);
        if (!parsedAccountId || !parsedAmount)
        {
            handler->SendErrorMessage("Invalid account ID or amount.");
            return false;
        }

        accountId = *parsedAccountId;
        amount = *parsedAmount;
        return true;
    }

    static bool HandleAccountAdd(ChatHandler* handler, char const* args)
    {
        auto tokens = Acore::Tokenize(args ? std::string_view(args) : std::string_view(), ' ', false);
        if (tokens.size() != 3)
        {
            handler->SendErrorMessage("Syntax: .shop account add <account_id> <donate|vote> <amount>");
            return false;
        }

        auto accountId = Acore::StringTo<uint32>(tokens[0]);
        auto amount = Acore::StringTo<uint64>(tokens[2]);
        if (!accountId || !amount || !*amount)
        {
            handler->SendErrorMessage("Invalid account ID or amount.");
            return false;
        }

        if (tokens[1] == "donate")
        {
            LoginDatabase.DirectExecute(
                "INSERT INTO `account_currency` (`account_id`, `donate_balance`, `total_donated`) "
                "VALUES ({}, {}, {}) ON DUPLICATE KEY UPDATE "
                "`donate_balance` = `donate_balance` + VALUES(`donate_balance`), "
                "`total_donated` = `total_donated` + VALUES(`total_donated`)",
                *accountId,
                *amount,
                *amount);
        }
        else if (tokens[1] == "vote")
        {
            LoginDatabase.DirectExecute(
                "INSERT INTO `account_currency` (`account_id`, `vote_balance`) VALUES ({}, {}) "
                "ON DUPLICATE KEY UPDATE `vote_balance` = `vote_balance` + VALUES(`vote_balance`)",
                *accountId,
                *amount);
        }
        else
        {
            handler->SendErrorMessage("Currency must be donate or vote.");
            return false;
        }

        handler->PSendSysMessage("Account {} credited with {} {} currency.", *accountId, *amount, tokens[1]);
        return true;
    }

    static bool HandleAccountSet(ChatHandler* handler, char const* args)
    {
        auto tokens = Acore::Tokenize(args ? std::string_view(args) : std::string_view(), ' ', false);
        if (tokens.size() != 3)
        {
            handler->SendErrorMessage("Syntax: .shop account set <account_id> <donate|vote> <amount>");
            return false;
        }

        auto accountId = Acore::StringTo<uint32>(tokens[0]);
        auto amount = Acore::StringTo<uint64>(tokens[2]);
        if (!accountId || !amount)
        {
            handler->SendErrorMessage("Invalid account ID or amount.");
            return false;
        }

        LoginDatabase.DirectExecute("INSERT IGNORE INTO `account_currency` (`account_id`) VALUES ({})", *accountId);
        if (tokens[1] == "donate")
            LoginDatabase.DirectExecute(
                "UPDATE `account_currency` SET `donate_balance` = {} WHERE `account_id` = {}", *amount, *accountId);
        else if (tokens[1] == "vote")
            LoginDatabase.DirectExecute(
                "UPDATE `account_currency` SET `vote_balance` = {} WHERE `account_id` = {}", *amount, *accountId);
        else
        {
            handler->SendErrorMessage("Currency must be donate or vote.");
            return false;
        }

        handler->PSendSysMessage("Account {} {} balance set to {}.", *accountId, tokens[1], *amount);
        return true;
    }

    static bool HandleAccountGet(ChatHandler* handler, char const* args)
    {
        auto accountId = Acore::StringTo<uint32>(args ? std::string_view(args) : std::string_view());
        if (!accountId)
        {
            handler->SendErrorMessage("Syntax: .shop account get <account_id>");
            return false;
        }

        QueryResult result = LoginDatabase.Query("SELECT `donate_balance`, `vote_balance`, `total_donated` "
                                                 "FROM `account_currency` WHERE `account_id` = {}",
                                                 *accountId);
        if (!result)
        {
            handler->SendErrorMessage("Account currency row not found.");
            return false;
        }

        Field* fields = result->Fetch();
        handler->PSendSysMessage("Account {}: donate={}, vote={}, lifetime={}",
                                 *accountId,
                                 fields[0].Get<uint64>(),
                                 fields[1].Get<uint64>(),
                                 fields[2].Get<uint64>());
        return true;
    }

    static bool HandleLifetimeAdd(ChatHandler* handler, char const* args)
    {
        uint32 accountId = 0;
        uint64 amount = 0;
        if (!ParseAccountAmount(handler, args, accountId, amount) || !amount)
            return false;

        LoginDatabase.DirectExecute(
            "INSERT INTO `account_currency` (`account_id`, `total_donated`) VALUES ({}, {}) "
            "ON DUPLICATE KEY UPDATE `total_donated` = `total_donated` + VALUES(`total_donated`)",
            accountId,
            amount);
        handler->PSendSysMessage("Account {} lifetime donation increased by {}.", accountId, amount);
        return true;
    }

    static bool HandleLifetimeSet(ChatHandler* handler, char const* args)
    {
        uint32 accountId = 0;
        uint64 amount = 0;
        if (!ParseAccountAmount(handler, args, accountId, amount))
            return false;

        LoginDatabase.DirectExecute("INSERT INTO `account_currency` (`account_id`, `total_donated`) VALUES ({}, {}) "
                                    "ON DUPLICATE KEY UPDATE `total_donated` = VALUES(`total_donated`)",
                                    accountId,
                                    amount);
        handler->PSendSysMessage("Account {} lifetime donation set to {}.", accountId, amount);
        return true;
    }
};
} // namespace

void AddAccountShopScripts()
{
    new AccountShopCommandScript();
}
