CREATE TABLE IF NOT EXISTS `Orgs` (

  `ID` int(11) NOT NULL AUTO_INCREMENT,

  `Name` varchar(32) NOT NULL,

  `Color` int(11) NOT NULL DEFAULT 0,

  `Money` int(11) NOT NULL DEFAULT 0,

  `Motd` varchar(96) NOT NULL DEFAULT 'Brak',

  `Created` int(11) NOT NULL DEFAULT 0,

  `HQSet` tinyint(1) NOT NULL DEFAULT 0,

  `HQX` float NOT NULL DEFAULT 0,

  `HQY` float NOT NULL DEFAULT 0,

  `HQZ` float NOT NULL DEFAULT 0,

  `HQA` float NOT NULL DEFAULT 0,

  PRIMARY KEY (`ID`),

  UNIQUE KEY `Name` (`Name`)

) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;



CREATE TABLE IF NOT EXISTS `OrgMembers` (

  `CharID` int(11) NOT NULL,

  `OrgID` int(11) NOT NULL,

  `Rank` tinyint(1) NOT NULL DEFAULT 1,

  `Joined` int(11) NOT NULL DEFAULT 0,

  `InsideHQ` tinyint(1) NOT NULL DEFAULT 0,

  PRIMARY KEY (`CharID`),

  KEY `OrgID` (`OrgID`),

  CONSTRAINT `OrgMembers_ibfk_1` FOREIGN KEY (`CharID`) REFERENCES `Characters` (`ID`) ON DELETE CASCADE,

  CONSTRAINT `OrgMembers_ibfk_2` FOREIGN KEY (`OrgID`) REFERENCES `Orgs` (`ID`) ON DELETE CASCADE

) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;


CREATE TABLE IF NOT EXISTS `OrgRanks` (
  `OrgID` int(11) NOT NULL,
  `Rank` tinyint(1) NOT NULL,
  `Name` varchar(24) NOT NULL,
  `Perms` int(11) NOT NULL DEFAULT 0,
  PRIMARY KEY (`OrgID`, `Rank`),
  CONSTRAINT `OrgRanks_ibfk_1` FOREIGN KEY (`OrgID`) REFERENCES `Orgs` (`ID`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;
