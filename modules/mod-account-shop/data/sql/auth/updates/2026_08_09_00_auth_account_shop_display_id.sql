-- Run once on acore_auth when upgrading an existing Account Shop installation.
ALTER TABLE `account_shop_product`
    ADD COLUMN `display_id` INT UNSIGNED NOT NULL DEFAULT 0
    COMMENT 'CreatureDisplayInfo ID used by the client 3D preview'
    AFTER `reference_id`;

-- Example only. Replace product/display IDs with values used by your shop.
-- UPDATE `account_shop_product` SET `display_id` = 31007 WHERE `id` = 2;
