-- Run once on acore_auth when upgrading the previous 3D preview build.
ALTER TABLE `account_shop_product`
    CHANGE COLUMN `display_id` `creature_entry` INT UNSIGNED NOT NULL DEFAULT 0
    COMMENT 'creature_template.Entry used by the client 3D preview';

-- Values previously stored as CreatureDisplayID must be replaced with
-- creature_template.Entry. Example: Swift Spectral Tiger uses entry 24004.
-- UPDATE `account_shop_product` SET `creature_entry` = 24004 WHERE `id` = 2;
