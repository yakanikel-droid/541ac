-- Examples for the site/SOAP integration.
-- Replace 123 with the real AzerothCore account.id.

-- Credit 100 donate tokens. Both spendable balance and lifetime total increase.
INSERT INTO `account_currency` (`account_id`, `donate_balance`, `total_donated`)
VALUES (123, 100, 100)
ON DUPLICATE KEY UPDATE
    `donate_balance` = `donate_balance` + VALUES(`donate_balance`),
    `total_donated` = `total_donated` + VALUES(`total_donated`);

-- Credit 25 vote tokens. Lifetime donated is intentionally not changed.
INSERT INTO `account_currency` (`account_id`, `vote_balance`)
VALUES (123, 25)
ON DUPLICATE KEY UPDATE
    `vote_balance` = `vote_balance` + VALUES(`vote_balance`);

-- Read values for an account. The in-game shop displays only the first two balances.
SELECT `donate_balance`, `vote_balance`, `total_donated`
FROM `account_currency`
WHERE `account_id` = 123;

