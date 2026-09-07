CREATE TABLE IF NOT EXISTS `dungeonrespawn_playerinfo` (
  `guid` BIGINT UNSIGNED NOT NULL,
  `map` INT NOT NULL,
  `x` FLOAT NOT NULL,
  `y` FLOAT NOT NULL,
  `z` FLOAT NOT NULL,
  `o` FLOAT NOT NULL,
  PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
