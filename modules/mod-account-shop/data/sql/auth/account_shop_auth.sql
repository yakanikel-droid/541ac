-- Account Shop for AzerothCore 3.3.5a
-- Apply this file to the AUTH database (normally acore_auth).
-- The web site/SOAP credits balances; the worldserver only reads and spends them.

CREATE TABLE IF NOT EXISTS `account_currency` (
    `account_id` INT UNSIGNED NOT NULL,
    `donate_balance` BIGINT UNSIGNED NOT NULL DEFAULT 0,
    `vote_balance` BIGINT UNSIGNED NOT NULL DEFAULT 0,
    `total_donated` BIGINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Stored for external systems; never shown by the shop',
    `updated_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (`account_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `account_shop_category` (
    `id` INT UNSIGNED NOT NULL AUTO_INCREMENT,
    `parent_id` INT UNSIGNED NOT NULL DEFAULT 0,
    `name` VARCHAR(100) NOT NULL,
    `icon` VARCHAR(255) NOT NULL DEFAULT '',
    `sort_order` INT NOT NULL DEFAULT 0,
    `enabled` TINYINT(1) UNSIGNED NOT NULL DEFAULT 1,
    PRIMARY KEY (`id`),
    KEY `idx_shop_category_parent` (`parent_id`, `enabled`, `sort_order`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- product_type:
-- 1 ITEM, 2 ITEM_BUNDLE, 3 SPELL, 4 MOUNT, 5 PET, 6 GOLD,
-- 7 LEVEL, 8 RENAME, 9 CUSTOMIZE, 10 RACE_CHANGE, 11 FACTION_CHANGE,
-- 12 TELEPORT, 13 VIP, 255 CUSTOM.
-- currency_type: 1 DONATE, 2 VOTE.
CREATE TABLE IF NOT EXISTS `account_shop_product` (
    `id` INT UNSIGNED NOT NULL AUTO_INCREMENT,
    `category_id` INT UNSIGNED NOT NULL,
    `product_type` TINYINT UNSIGNED NOT NULL,
    `reference_id` INT UNSIGNED NOT NULL DEFAULT 0,
    `creature_entry` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'creature_template.Entry used by the client 3D preview',
    `reward_amount` BIGINT UNSIGNED NOT NULL DEFAULT 1,
    `currency_type` TINYINT UNSIGNED NOT NULL,
    `price` BIGINT UNSIGNED NOT NULL,
    `allow_quantity` TINYINT(1) UNSIGNED NOT NULL DEFAULT 0,
    `max_quantity` INT UNSIGNED NOT NULL DEFAULT 1,
    `account_limit` INT UNSIGNED NOT NULL DEFAULT 0,
    `character_limit` INT UNSIGNED NOT NULL DEFAULT 0,
    `name` VARCHAR(150) NOT NULL,
    `description` TEXT NULL,
    `icon` VARCHAR(255) NOT NULL DEFAULT '',
    `featured` TINYINT(1) UNSIGNED NOT NULL DEFAULT 0,
    `map_id` INT UNSIGNED NOT NULL DEFAULT 0,
    `position_x` FLOAT NOT NULL DEFAULT 0,
    `position_y` FLOAT NOT NULL DEFAULT 0,
    `position_z` FLOAT NOT NULL DEFAULT 0,
    `orientation` FLOAT NOT NULL DEFAULT 0,
    `custom_value` BIGINT UNSIGNED NOT NULL DEFAULT 0,
    `sort_order` INT NOT NULL DEFAULT 0,
    `enabled` TINYINT(1) UNSIGNED NOT NULL DEFAULT 1,
    `created_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    `updated_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (`id`),
    KEY `idx_shop_product_list` (`enabled`, `category_id`, `sort_order`, `id`),
    KEY `idx_shop_product_featured` (`enabled`, `featured`, `sort_order`, `id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- Rewards for product_type=2 (ITEM_BUNDLE).
CREATE TABLE IF NOT EXISTS `account_shop_product_item` (
    `product_id` INT UNSIGNED NOT NULL,
    `item_id` INT UNSIGNED NOT NULL,
    `item_count` INT UNSIGNED NOT NULL DEFAULT 1,
    `sort_order` INT NOT NULL DEFAULT 0,
    PRIMARY KEY (`product_id`, `item_id`),
    KEY `idx_shop_bundle_sort` (`product_id`, `sort_order`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- A purchase is first CREATED, then atomically DEBITED, and finally DELIVERED.
-- DEBITED rows are recoverable after a crash and can be refunded without double crediting.
CREATE TABLE IF NOT EXISTS `account_shop_purchase` (
    `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `request_token` CHAR(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    `account_id` INT UNSIGNED NOT NULL,
    `character_guid` BIGINT UNSIGNED NOT NULL,
    `product_id` INT UNSIGNED NOT NULL,
    `product_type` TINYINT UNSIGNED NOT NULL,
    `reference_id` INT UNSIGNED NOT NULL DEFAULT 0,
    `reward_amount` BIGINT UNSIGNED NOT NULL DEFAULT 1,
    `quantity` INT UNSIGNED NOT NULL DEFAULT 1,
    `currency_type` TINYINT UNSIGNED NOT NULL,
    `unit_price` BIGINT UNSIGNED NOT NULL,
    `total_price` BIGINT UNSIGNED NOT NULL,
    `map_id` INT UNSIGNED NOT NULL DEFAULT 0,
    `position_x` FLOAT NOT NULL DEFAULT 0,
    `position_y` FLOAT NOT NULL DEFAULT 0,
    `position_z` FLOAT NOT NULL DEFAULT 0,
    `orientation` FLOAT NOT NULL DEFAULT 0,
    `custom_value` BIGINT UNSIGNED NOT NULL DEFAULT 0,
    `status` ENUM('CREATED', 'DEBITED', 'DELIVERED', 'FAILED', 'REFUNDED') NOT NULL DEFAULT 'CREATED',
    `error_text` VARCHAR(255) NOT NULL DEFAULT '',
    `created_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    `completed_at` TIMESTAMP NULL DEFAULT NULL,
    PRIMARY KEY (`id`),
    UNIQUE KEY `uq_shop_purchase_token` (`request_token`),
    KEY `idx_shop_purchase_account` (`account_id`, `created_at`),
    KEY `idx_shop_purchase_character` (`character_guid`, `created_at`),
    KEY `idx_shop_purchase_recovery` (`status`, `created_at`),
    KEY `idx_shop_purchase_limits` (`account_id`, `product_id`, `status`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `account_shop_purchase_item` (
    `purchase_id` BIGINT UNSIGNED NOT NULL,
    `item_id` INT UNSIGNED NOT NULL,
    `item_count` BIGINT UNSIGNED NOT NULL,
    PRIMARY KEY (`purchase_id`, `item_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- VIP is only stored here. Other server modules/site code may read this entitlement.
CREATE TABLE IF NOT EXISTS `account_shop_entitlement` (
    `account_id` INT UNSIGNED NOT NULL,
    `entitlement_type` VARCHAR(50) NOT NULL,
    `expires_at` TIMESTAMP NULL DEFAULT NULL,
    `value` BIGINT UNSIGNED NOT NULL DEFAULT 0,
    `updated_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (`account_id`, `entitlement_type`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- Permission 1998 is linked to AzerothCore's standard Player role (195).
DELETE FROM `rbac_linked_permissions` WHERE `linkedId` IN (1998, 1999);
DELETE FROM `rbac_permissions` WHERE `id` IN (1998, 1999);
INSERT INTO `rbac_permissions` (`id`, `name`) VALUES
(1998, 'Command: shop'),
(1999, 'Command: shop account administration');
INSERT INTO `rbac_linked_permissions` (`id`, `linkedId`) VALUES
(195, 1998),
(198, 1999);

-- Default categories. Re-running this block is idempotent.
INSERT INTO `account_shop_category` (`id`, `parent_id`, `name`, `icon`, `sort_order`, `enabled`) VALUES
(1, 0, 'Предметы', 'Interface\\Icons\\INV_Misc_Bag_10', 10, 1),
(2, 0, 'Маунты', 'Interface\\Icons\\Ability_Mount_RidingHorse', 20, 1),
(3, 0, 'Питомцы', 'Interface\\Icons\\INV_Box_PetCarrier_01', 30, 1),
(4, 0, 'Услуги', 'Interface\\Icons\\INV_Misc_Note_01', 40, 1),
(5, 0, 'VIP', 'Interface\\Icons\\INV_Crown_01', 50, 1)
ON DUPLICATE KEY UPDATE
    `name` = VALUES(`name`),
    `icon` = VALUES(`icon`),
    `sort_order` = VALUES(`sort_order`);

-- Disabled examples. Review IDs/prices, then set enabled=1 explicitly.
INSERT INTO `account_shop_product`
    (`id`, `category_id`, `product_type`, `reference_id`, `creature_entry`, `reward_amount`, `currency_type`, `price`,
     `allow_quantity`, `max_quantity`, `name`, `description`, `icon`, `featured`, `sort_order`, `enabled`)
VALUES
    (1, 1, 1, 49623, 0, 1, 1, 1500, 1, 5, 'Тёмная Скорбь',
     'Пример предмета. Перед включением проверьте ID и цену.', 'Interface\\Icons\\INV_Axe_113', 1, 10, 0),
    (2, 2, 4, 72286, 38545, 1, 1, 1500, 0, 1, 'Непобедимый',
     'Изучает заклинание транспорта. Количество всегда равно одному.',
     'Interface\\Icons\\Ability_Mount_Undeadhorse', 1, 20, 0),
    (3, 4, 8, 0, 0, 1, 2, 300, 0, 1, 'Смена имени',
     'Устанавливает флаг смены имени. Изменение выполняется после повторного входа.',
     'Interface\\Icons\\INV_Misc_Note_05', 1, 30, 0),
    (4, 5, 13, 0, 0, 30, 1, 800, 0, 1, 'VIP — 30 дней',
     'Добавляет 30 дней к аккаунтному VIP-сроку.', 'Interface\\Icons\\INV_Crown_01', 1, 40, 0)
ON DUPLICATE KEY UPDATE `id` = VALUES(`id`);
