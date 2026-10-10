-- Wykonaj JEDEN RAZ (tabele Orgs i OrgMembers musza juz istniec).
-- Domyslne rangi dla istniejacych organizacji serwer doda sam przy starcie (Org_Load).
CREATE TABLE IF NOT EXISTS `OrgRanks` (
  `OrgID` int(11) NOT NULL,
  `Rank` tinyint(1) NOT NULL,
  `Name` varchar(24) NOT NULL,
  `Perms` int(11) NOT NULL DEFAULT 0,
  PRIMARY KEY (`OrgID`, `Rank`),
  CONSTRAINT `OrgRanks_ibfk_1` FOREIGN KEY (`OrgID`) REFERENCES `Orgs` (`ID`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;
