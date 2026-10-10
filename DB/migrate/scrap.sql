-- Wspolna tabela postepu w pracach (SCRAP_JOB_ID = 1 to Zlomiarz). Inne prace moga uzywac innych JobID.
CREATE TABLE IF NOT EXISTS `JobStats` (
  `CharID` int(11) NOT NULL,
  `JobID` int(11) NOT NULL,
  `XP` int(11) NOT NULL DEFAULT 0,
  `Delivered` int(11) NOT NULL DEFAULT 0,
  `Earned` int(11) NOT NULL DEFAULT 0,
  `QuestDay` int(11) NOT NULL DEFAULT 0,
  `QuestProgress` int(11) NOT NULL DEFAULT 0,
  `QuestDone` tinyint(1) NOT NULL DEFAULT 0,
  PRIMARY KEY (`CharID`, `JobID`),
  CONSTRAINT `JobStats_ibfk_1` FOREIGN KEY (`CharID`) REFERENCES `Characters` (`ID`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

CREATE TABLE IF NOT EXISTS `ScrapSpots` (
  `ID` int(11) NOT NULL AUTO_INCREMENT,
  `Zone` tinyint(1) NOT NULL DEFAULT 1,
  `PosX` float NOT NULL,
  `PosY` float NOT NULL,
  `PosZ` float NOT NULL,
  PRIMARY KEY (`ID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

CREATE TABLE IF NOT EXISTS `ScrapMarket` (
  `Material` tinyint(1) NOT NULL,
  `Roll` int(11) NOT NULL DEFAULT 1000,
  `Pressure` int(11) NOT NULL DEFAULT 0,
  `Updated` int(11) NOT NULL DEFAULT 0,
  PRIMARY KEY (`Material`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;
