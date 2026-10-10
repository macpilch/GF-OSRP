/*
	Project: GodFather
	File name: systems/scrap.inc
	Desc: Praca dorywcza "Zlomiarz" - zbieranie zlomu, torba z limitem wagi, skup z zywymi cenami,
	      poziomy liczone z XP, zlecenie dzienne, ranking. Postep zapisywany w bazie (JobStats).

	Jak to dziala:
	- admin ustawia punkty: strefy zbierania 1-3 i skup (strefa 0) komenda /zlompunkt,
	- gracz zglasza sie w skupie (/zlom start), potem chodzi po strefach i zbiera zlom (tak samo jak
	  w pracach z casual_jobs: stoi blisko obiektu i patrzy na niego, timer co 250 ms),
	- zebrany zlom ladowany jest do torby (limit wagi rosnie z poziomem), sprzedaz w skupie: /zlom sprzedaj,
	- poziom to funkcja XP: XP potrzebne na poziom L = 50 * L * (L - 1),
	- ceny zmieniaja sie co godzine, a sprzedaz obniza cene materialu (odbudowuje sie z czasem).

	Gracz: /zlom [start | stop | sprzedaj | torba | ceny | poziom | zlecenie | top | wyrzuc tak]
	Admin: /zlompunkt [dodaj strefa | usun id | najblizszy | lista]
*/

// ---------------------------------------------------------------------------
// Ustawienia
// ---------------------------------------------------------------------------
#define SCRAP_JOB_ID				1			// ID pracy w tabeli JobStats
#define MAX_SCRAP_SPOTS				200
#define SCRAP_MATS					6
#define SCRAP_ZONES					3
#define SCRAP_MAX_LEVEL				10
#define SCRAP_ADMIN_LEVEL			1			// pAdmin >= 1 = administrator (tylko /zlompunkt)

#define SCRAP_PICK_RADIUS			1.5			// z jakiej odleglosci zbiera sie zlom
#define SCRAP_BUYER_RADIUS			4.0			// z jakiej odleglosci dziala skup
#define SCRAP_RESPAWN_MIN			180			// po ilu sekundach (min.) odnawia sie punkt zbierania
#define SCRAP_RESPAWN_RANGE			240			// + losowo 0..RANGE sekund

#define SCRAP_BASE_CAPACITY			30			// bazowa pojemnosc torby (kg)
#define SCRAP_CAPACITY_PER_LEVEL	10			// +kg za kazdy poziom powyzej 1
#define SCRAP_BASE_TIME				4000		// bazowy czas zbierania (ms)
#define SCRAP_TIME_CUT_PER_LEVEL	3			// -% czasu za kazdy poziom powyzej 1
#define SCRAP_MIN_TIME				1500		// minimalny czas zbierania (ms)
#define SCRAP_PRICE_BONUS_LEVEL		15			// +promile ceny za kazdy poziom powyzej 1 (15 = +1,5%)

#define SCRAP_FULL_XP_PER_HOUR		60			// tyle przedmiotow na godzine daje pelne XP
#define SCRAP_OVER_LIMIT_XP			50			// % XP po przekroczeniu limitu na godzine

#define SCRAP_QUEST_CASH_PER_KG		30			// nagroda zlecenia dziennego: $ za kg
#define SCRAP_QUEST_XP_PER_KG		3			// nagroda zlecenia dziennego: XP za kg

#define DIALOG_SCRAP_TOP			203			// sprawdz, czy to ID jest u Ciebie wolne

// Materialy
new const ScrapMatName[SCRAP_MATS][] = {
	"Blacha",
	"Opony i felgi",
	"Akumulatory",
	"Miedz",
	"Elektronika",
	"Czesci silnika"
};

new const ScrapMatWeight[SCRAP_MATS] = {4, 8, 10, 5, 3, 12};		// kg
new const ScrapMatXP[SCRAP_MATS] = {5, 8, 12, 18, 30, 60};
new const ScrapMatPrice[SCRAP_MATS] = {3, 4, 5, 12, 25, 15};		// $ za kg (stawka bazowa)

// Modele obiektow zlomu. UWAGA: to sa przykladowe ID - sprawdz je w edytorze i podmien na wlasne.
new const ScrapMatModel[SCRAP_MATS] = {1328, 1073, 1220, 1328, 1220, 1328};

// Strefy: wymagany poziom i szanse wylosowania materialu (waga) - indeks strefy 1-3
new const ScrapZoneLevel[SCRAP_ZONES + 1] = {0, 1, 3, 7};

new const ScrapZoneWeights[SCRAP_ZONES + 1][SCRAP_MATS] = {
	{ 0,  0,  0,  0,  0,  0},
	{60, 30, 10,  0,  0,  0},		// strefa 1: blacha, opony, troche akumulatorow
	{30, 20, 25, 15, 10,  0},		// strefa 2: + miedz i elektronika
	{ 0,  0, 20, 30, 30, 20}		// strefa 3: wartosciowe materialy i czesci silnika
};

// ---------------------------------------------------------------------------
// Dane
// ---------------------------------------------------------------------------
enum eScrapSpot {
	sspUsed,
	sspSQL,
	sspZone,			// 0 = skup, 1-3 = strefa zbierania
	Float:sspX,
	Float:sspY,
	Float:sspZ,
	sspMaterial,		// aktualnie lezacy material
	sspObject,
	sspRespawnAt,		// 0 = dostepny, inaczej czas (unix) odnowienia
	sspReserved,		// playerid + 1 gracza, ktory wlasnie zbiera (0 = nikt)
	sspPickup,			// tylko skup
	sspLabel
};

enum eScrapPlayer {
	splChar,			// Characters.ID, dla ktorego wczytano dane (0 = brak)
	splXP,
	splDelivered,
	splEarned,
	splQuestDay,
	splQuestProgress,
	splQuestDone,
	splActive,			// 1 = gracz ma "zmiane" (zglosil sie w skupie)
	splBusy,
	splActionId,
	splHourStart,
	splHourCount,
	splNoticeAt,
	splCargo[SCRAP_MATS]	// liczba sztuk kazdego materialu w torbie
};

new ScrapSpots[MAX_SCRAP_SPOTS][eScrapSpot];
new ScrapPlayer[MAX_PLAYERS][eScrapPlayer];

new ScrapRoll[SCRAP_MATS];			// losowy mnoznik rynku w promilach (700-1300)
new ScrapPressure[SCRAP_MATS];		// ile kg sprzedano (obniza cene, z czasem znika)
new ScrapMarketHour;
new ScrapLastCheck;
new ScrapActionCounter;

// ---------------------------------------------------------------------------
// Dzielenie tekstu komendy na slowa (bez zaleznosci od zachowania sscanf)
// ---------------------------------------------------------------------------
stock Scrap_NextToken(const src[], dest[], size, &pos) {
	new len = strlen(src), start;

	dest[0] = EOS;

	while(pos < len && src[pos] == ' ') pos++;

	start = pos;

	while(pos < len && src[pos] != ' ') pos++;

	if(pos - start > 0) strmid(dest, src, start, pos, size);
	return pos - start;
}

stock Scrap_Rest(const src[], dest[], size, pos) {
	new len = strlen(src);

	dest[0] = EOS;

	while(pos < len && src[pos] == ' ') pos++;

	if(len - pos > 0) strmid(dest, src, pos, len, size);
}

// ---------------------------------------------------------------------------
// Poziomy, ceny, limity
// ---------------------------------------------------------------------------
stock Scrap_IsAdmin(playerid) {
	return (gPlayerLogged[playerid] && PlayerInfo[playerid][pAdmin] >= SCRAP_ADMIN_LEVEL);
}

stock Scrap_XPForLevel(level) {
	return 50 * level * (level - 1);
}

stock Scrap_Level(xp) {
	new level = 1;

	while(level < SCRAP_MAX_LEVEL && xp >= Scrap_XPForLevel(level + 1)) level++;
	return level;
}

stock Scrap_Capacity(level) {
	return SCRAP_BASE_CAPACITY + SCRAP_CAPACITY_PER_LEVEL * (level - 1);
}

stock Scrap_ActionTime(level) {
	new time = SCRAP_BASE_TIME * (100 - SCRAP_TIME_CUT_PER_LEVEL * (level - 1)) / 100;

	if(time < SCRAP_MIN_TIME) time = SCRAP_MIN_TIME;
	return time;
}

stock Scrap_CargoWeight(playerid) {
	new total;

	for(new m = 0; m < SCRAP_MATS; m++) {
		total += ScrapPlayer[playerid][splCargo][m] * ScrapMatWeight[m];
	}
	return total;
}

// Mnoznik rynku w promilach: losowy mnoznik godzinowy pomniejszony o "presje" sprzedazy.
stock Scrap_Factor(mat) {
	new drop = ScrapPressure[mat] / 100 * 20;		// -2% za kazde 100 kg sprzedane przez wszystkich

	if(drop > 500) drop = 500;
	return ScrapRoll[mat] * (1000 - drop) / 1000;
}

// Cena za 1 kg dla gracza o danym poziomie.
stock Float:Scrap_UnitPrice(mat, level) {
	new bonus = SCRAP_PRICE_BONUS_LEVEL * (level - 1);

	return float(ScrapMatPrice[mat]) * float(Scrap_Factor(mat)) / 1000.0 * float(1000 + bonus) / 1000.0;
}

stock Scrap_RollMaterial(zone) {
	new total, roll;

	for(new m = 0; m < SCRAP_MATS; m++) total += ScrapZoneWeights[zone][m];

	if(total <= 0) return 0;

	roll = random(total);

	for(new m = 0; m < SCRAP_MATS; m++) {
		if(roll < ScrapZoneWeights[zone][m]) return m;
		roll -= ScrapZoneWeights[zone][m];
	}
	return 0;
}

// Zlecenie dzienne jest wyliczane z numeru doby, wiec nie wymaga zapisu w bazie.
stock Scrap_QuestGet(&mat, &kg) {
	new day = gettime() / 86400;

	mat = (day * 7 + 3) % 4;					// blacha, opony, akumulatory lub miedz
	kg = 20 + ((day * 13) % 5) * 5;				// 20-40 kg
}

stock Scrap_QuestCheckDay(playerid) {
	new today = gettime() / 86400;

	if(ScrapPlayer[playerid][splQuestDay] != today) {
		ScrapPlayer[playerid][splQuestDay] = today;
		ScrapPlayer[playerid][splQuestProgress] = 0;
		ScrapPlayer[playerid][splQuestDone] = 0;
	}
}

stock Scrap_Notice(playerid, now, const msg[]) {
	if(now - ScrapPlayer[playerid][splNoticeAt] < 5) return 0;

	ScrapPlayer[playerid][splNoticeAt] = now;
	SendClientMessage(playerid, COLOR_GREY, msg);
	return 1;
}

stock Scrap_NearBuyer(playerid) {
	if(GetPlayerVirtualWorld(playerid) != 0 || GetPlayerInterior(playerid) != 0) return 0;

	for(new i = 0; i < MAX_SCRAP_SPOTS; i++) {
		if(!ScrapSpots[i][sspUsed] || ScrapSpots[i][sspZone] != 0) continue;

		if(IsPlayerInRangeOfPoint(playerid, SCRAP_BUYER_RADIUS, ScrapSpots[i][sspX], ScrapSpots[i][sspY], ScrapSpots[i][sspZ])) return 1;
	}
	return 0;
}

stock Scrap_FindFreeSpot() {
	for(new i = 0; i < MAX_SCRAP_SPOTS; i++) {
		if(!ScrapSpots[i][sspUsed]) return i;
	}
	return -1;
}

// ---------------------------------------------------------------------------
// Punkty: zlom i skup
// ---------------------------------------------------------------------------
stock Scrap_SpotSpawn(idx) {
	new mat;

	if(ScrapSpots[idx][sspObject]) DestroyDynamicObject(ScrapSpots[idx][sspObject]);

	mat = Scrap_RollMaterial(ScrapSpots[idx][sspZone]);

	ScrapSpots[idx][sspMaterial] = mat;
	ScrapSpots[idx][sspRespawnAt] = 0;
	ScrapSpots[idx][sspReserved] = 0;
	ScrapSpots[idx][sspObject] = CreateDynamicObject(ScrapMatModel[mat],
		ScrapSpots[idx][sspX], ScrapSpots[idx][sspY], ScrapSpots[idx][sspZ],
		0.0, 0.0, float(random(360)), 0, -1, -1, 200.0);
}

stock Scrap_SpotDeplete(idx) {
	if(ScrapSpots[idx][sspObject]) DestroyDynamicObject(ScrapSpots[idx][sspObject]);

	ScrapSpots[idx][sspObject] = 0;
	ScrapSpots[idx][sspRespawnAt] = gettime() + SCRAP_RESPAWN_MIN + random(SCRAP_RESPAWN_RANGE);
}

stock Scrap_BuyerVisuals(idx) {
	if(ScrapSpots[idx][sspPickup]) DestroyDynamicPickup(ScrapSpots[idx][sspPickup]);
	if(ScrapSpots[idx][sspLabel]) DestroyDynamic3DTextLabel(Text3D:ScrapSpots[idx][sspLabel]);

	ScrapSpots[idx][sspPickup] = CreateDynamicPickup(1239, 1, ScrapSpots[idx][sspX], ScrapSpots[idx][sspY], ScrapSpots[idx][sspZ] - 0.5, 0, 0);
	ScrapSpots[idx][sspLabel] = _:CreateDynamic3DTextLabel("Skup zlomu\n{FFFFFF}/zlom start, /zlom sprzedaj", 0xFFCC00FF, ScrapSpots[idx][sspX], ScrapSpots[idx][sspY], ScrapSpots[idx][sspZ] + 0.5, 12.0, INVALID_PLAYER_ID, INVALID_VEHICLE_ID, 0, 0, 0);
}

stock Scrap_SpotRemove(idx) {
	if(ScrapSpots[idx][sspObject]) DestroyDynamicObject(ScrapSpots[idx][sspObject]);
	if(ScrapSpots[idx][sspPickup]) DestroyDynamicPickup(ScrapSpots[idx][sspPickup]);
	if(ScrapSpots[idx][sspLabel]) DestroyDynamic3DTextLabel(Text3D:ScrapSpots[idx][sspLabel]);

	queryStr[0] = EOS;
	format(queryStr, sizeof(queryStr), "DELETE FROM `ScrapSpots` WHERE `ID` = '%d' LIMIT 1", ScrapSpots[idx][sspSQL]);
	mysql_query(queryStr);

	ScrapSpots[idx][sspUsed] = 0;
	ScrapSpots[idx][sspObject] = 0;
	ScrapSpots[idx][sspPickup] = 0;
	ScrapSpots[idx][sspLabel] = 0;
}

// ---------------------------------------------------------------------------
// Rynek
// ---------------------------------------------------------------------------
stock Scrap_MarketSave(mat) {
	queryStr[0] = EOS;
	format(queryStr, sizeof(queryStr), "UPDATE `ScrapMarket` SET `Roll` = '%d', `Pressure` = '%d', `Updated` = '%d' WHERE `Material` = '%d' LIMIT 1", ScrapRoll[mat], ScrapPressure[mat], gettime(), mat);
	mysql_query(queryStr);
}

// Nowe losowe ceny i stopniowe odbudowanie cen po sprzedazy (hours = ile godzin uplynelo).
stock Scrap_MarketTick(hours) {
	if(hours > 24) hours = 24;

	for(new m = 0; m < SCRAP_MATS; m++) {
		ScrapRoll[m] = 700 + random(601);

		for(new h = 0; h < hours; h++) ScrapPressure[m] = ScrapPressure[m] * 75 / 100;

		Scrap_MarketSave(m);
	}
}

// ---------------------------------------------------------------------------
// Dane gracza
// ---------------------------------------------------------------------------
stock Scrap_Reset(playerid) {
	ScrapPlayer[playerid][splChar] = 0;
	ScrapPlayer[playerid][splXP] = 0;
	ScrapPlayer[playerid][splDelivered] = 0;
	ScrapPlayer[playerid][splEarned] = 0;
	ScrapPlayer[playerid][splQuestDay] = 0;
	ScrapPlayer[playerid][splQuestProgress] = 0;
	ScrapPlayer[playerid][splQuestDone] = 0;
	ScrapPlayer[playerid][splActive] = 0;
	ScrapPlayer[playerid][splBusy] = 0;
	ScrapPlayer[playerid][splActionId] = 0;
	ScrapPlayer[playerid][splHourStart] = 0;
	ScrapPlayer[playerid][splHourCount] = 0;
	ScrapPlayer[playerid][splNoticeAt] = 0;

	for(new m = 0; m < SCRAP_MATS; m++) ScrapPlayer[playerid][splCargo][m] = 0;
}

stock Scrap_Save(playerid) {
	queryStr[0] = EOS;
	format(queryStr, sizeof(queryStr), "UPDATE `JobStats` SET `XP` = '%d', `Delivered` = '%d', `Earned` = '%d', `QuestDay` = '%d', `QuestProgress` = '%d', `QuestDone` = '%d' WHERE `CharID` = '%d' AND `JobID` = '%d' LIMIT 1",
		ScrapPlayer[playerid][splXP], ScrapPlayer[playerid][splDelivered], ScrapPlayer[playerid][splEarned],
		ScrapPlayer[playerid][splQuestDay], ScrapPlayer[playerid][splQuestProgress], ScrapPlayer[playerid][splQuestDone],
		ScrapPlayer[playerid][splChar], SCRAP_JOB_ID);
	mysql_query(queryStr);
}

// Wczytuje dane gracza z bazy przy pierwszym uzyciu dla danej postaci (bez hookow w OnPlayerLogin).
stock Scrap_Sync(playerid) {
	new charid = PlayerInfo[playerid][pID], row[96], found;

	if(charid <= 0) return 0;
	if(ScrapPlayer[playerid][splChar] == charid) return 1;

	Scrap_Reset(playerid);
	ScrapPlayer[playerid][splChar] = charid;

	queryStr[0] = EOS;
	format(queryStr, sizeof(queryStr), "SELECT `XP`, `Delivered`, `Earned`, `QuestDay`, `QuestProgress`, `QuestDone` FROM `JobStats` WHERE `CharID` = '%d' AND `JobID` = '%d' LIMIT 1", charid, SCRAP_JOB_ID);
	mysql_query(queryStr);
	mysql_store_result();

	if(mysql_result_stored()) {
		if(mysql_fetch_row(row, "|")) {
			new xp, delivered, earned, qday, qprogress, qdone;

			sscanf(row, "p<|>iiiiii", xp, delivered, earned, qday, qprogress, qdone);

			ScrapPlayer[playerid][splXP] = xp;
			ScrapPlayer[playerid][splDelivered] = delivered;
			ScrapPlayer[playerid][splEarned] = earned;
			ScrapPlayer[playerid][splQuestDay] = qday;
			ScrapPlayer[playerid][splQuestProgress] = qprogress;
			ScrapPlayer[playerid][splQuestDone] = qdone;
			found = 1;
		}
	}

	mysql_free_result();

	if(!found) {
		queryStr[0] = EOS;
		format(queryStr, sizeof(queryStr), "INSERT IGNORE INTO `JobStats` (`CharID`, `JobID`) VALUES ('%d', '%d')", charid, SCRAP_JOB_ID);
		mysql_query(queryStr);
	}
	return 1;
}

// Dodaje XP, sprawdza awans, zapisuje w bazie.
stock Scrap_AddXP(playerid, amount) {
	new before = Scrap_Level(ScrapPlayer[playerid][splXP]), after, str[144];

	ScrapPlayer[playerid][splXP] += amount;
	after = Scrap_Level(ScrapPlayer[playerid][splXP]);

	Scrap_Save(playerid);

	if(after > before) {
		format(str, sizeof(str), "Zlomiarz: Awansowales na poziom %d! Torba: %d kg, szybsze zbieranie i lepsze ceny.", after, Scrap_Capacity(after));
		SendClientMessage(playerid, COLOR_GREEN, str);
		GameTextForPlayer(playerid, "~n~~g~Nowy poziom zlomiarza!", 4000, 3);

		for(new z = 1; z <= SCRAP_ZONES; z++) {
			if(ScrapZoneLevel[z] > before && ScrapZoneLevel[z] <= after) {
				format(str, sizeof(str), "Zlomiarz: Odblokowales strefe %d.", z);
				SendClientMessage(playerid, COLOR_YELLOW, str);
			}
		}
	}
}

// ---------------------------------------------------------------------------
// Wczytanie z bazy
// ---------------------------------------------------------------------------
// Wywolaj w OnGameModeInit() po ConnectingMySQL().
stock Scrap_Load() {
	new row[128], count, rowsMarket[SCRAP_MATS], now = gettime(), oldest = now, hours;

	for(new i = 0; i < MAX_SCRAP_SPOTS; i++) ScrapSpots[i][sspUsed] = 0;

	for(new m = 0; m < SCRAP_MATS; m++) {
		ScrapRoll[m] = 1000;
		ScrapPressure[m] = 0;
	}

	for(new i = 0; i < MAX_PLAYERS; i++) Scrap_Reset(i);

	// UWAGA: w petlach po wynikach nie wolno wywolywac mysql_query() - nowe zapytanie
	// zastepuje wynik, po ktorym iterujemy.

	// 1) punkty
	mysql_query("SELECT `ID`, `Zone`, `PosX`, `PosY`, `PosZ` FROM `ScrapSpots`");
	mysql_store_result();

	if(mysql_result_stored()) {
		while(mysql_fetch_row(row, "|")) {
			if(count >= MAX_SCRAP_SPOTS) break;

			new id, zone, Float:x, Float:y, Float:z;

			sscanf(row, "p<|>iifff", id, zone, x, y, z);

			if(zone < 0 || zone > SCRAP_ZONES) continue;

			ScrapSpots[count][sspUsed] = 1;
			ScrapSpots[count][sspSQL] = id;
			ScrapSpots[count][sspZone] = zone;
			ScrapSpots[count][sspX] = x;
			ScrapSpots[count][sspY] = y;
			ScrapSpots[count][sspZ] = z;
			ScrapSpots[count][sspObject] = 0;
			ScrapSpots[count][sspPickup] = 0;
			ScrapSpots[count][sspLabel] = 0;
			ScrapSpots[count][sspRespawnAt] = 0;
			ScrapSpots[count][sspReserved] = 0;
			count++;
		}
	}

	mysql_free_result();

	// 2) rynek
	mysql_query("SELECT `Material`, `Roll`, `Pressure`, `Updated` FROM `ScrapMarket`");
	mysql_store_result();

	if(mysql_result_stored()) {
		while(mysql_fetch_row(row, "|")) {
			new mat, roll, pressure, updated;

			sscanf(row, "p<|>iiii", mat, roll, pressure, updated);

			if(mat < 0 || mat >= SCRAP_MATS) continue;

			ScrapRoll[mat] = roll;
			ScrapPressure[mat] = pressure;
			rowsMarket[mat] = 1;

			if(updated < oldest) oldest = updated;
		}
	}

	mysql_free_result();

	for(new m = 0; m < SCRAP_MATS; m++) {
		if(rowsMarket[m]) continue;

		queryStr[0] = EOS;
		format(queryStr, sizeof(queryStr), "INSERT IGNORE INTO `ScrapMarket` (`Material`, `Roll`, `Pressure`, `Updated`) VALUES ('%d', '1000', '0', '%d')", m, now);
		mysql_query(queryStr);
	}

	// ile godzin minelo od ostatniej aktualizacji cen (np. przerwa serwera)
	hours = (now - oldest) / 3600;
	if(hours >= 1) Scrap_MarketTick(hours);

	ScrapMarketHour = now / 3600;
	ScrapLastCheck = now;

	// 3) obiekty i napisy dopiero po zwolnieniu wynikow
	for(new i = 0; i < MAX_SCRAP_SPOTS; i++) {
		if(!ScrapSpots[i][sspUsed]) continue;

		if(ScrapSpots[i][sspZone] == 0) Scrap_BuyerVisuals(i);
		else Scrap_SpotSpawn(i);
	}

	printf("[ZLOM] Wczytano punktow: %d.", count);
}

// ---------------------------------------------------------------------------
// Zbieranie zlomu (timer co 250 ms, jak w casual_jobs)
// ---------------------------------------------------------------------------
forward ScrapActionDone(playerid, spot, actionId);
public ScrapActionDone(playerid, spot, actionId) {
	new mat, level, now = gettime(), xp, str[144];

	if(ScrapSpots[spot][sspReserved] == playerid + 1) ScrapSpots[spot][sspReserved] = 0;

	if(!IsPlayerConnected(playerid)) return 0;

	osrp_TogglePlayerControllable(playerid, 1);
	ClearAnimations(playerid);

	if(ScrapPlayer[playerid][splActionId] != actionId) return 0;

	ScrapPlayer[playerid][splBusy] = 0;
	ScrapPlayer[playerid][splActionId] = 0;

	if(!gPlayerLogged[playerid] || !ScrapPlayer[playerid][splActive]) return 0;

	if(!ScrapSpots[spot][sspUsed] || ScrapSpots[spot][sspRespawnAt] != 0) {
		SendClientMessage(playerid, COLOR_GREY, "Zlomiarz: Ten zlom juz ktos zabral.");
		return 0;
	}

	mat = ScrapSpots[spot][sspMaterial];
	level = Scrap_Level(ScrapPlayer[playerid][splXP]);

	if(Scrap_CargoWeight(playerid) + ScrapMatWeight[mat] > Scrap_Capacity(level)) {
		SendClientMessage(playerid, COLOR_GREY, "Zlomiarz: Torba jest pelna - sprzedaj zlom w skupie (/zlom sprzedaj).");
		return 0;
	}

	Scrap_SpotDeplete(spot);
	ScrapPlayer[playerid][splCargo][mat]++;

	// limit pelnego XP na godzine (ochrona przed farmieniem)
	if(now - ScrapPlayer[playerid][splHourStart] >= 3600) {
		ScrapPlayer[playerid][splHourStart] = now;
		ScrapPlayer[playerid][splHourCount] = 0;
	}

	ScrapPlayer[playerid][splHourCount]++;

	xp = ScrapMatXP[mat];

	if(ScrapPlayer[playerid][splHourCount] > SCRAP_FULL_XP_PER_HOUR) {
		xp = xp * SCRAP_OVER_LIMIT_XP / 100;
		if(xp < 1) xp = 1;
	}

	format(str, sizeof(str), "Zlomiarz: Zebrano: %s (%d kg) | Torba: %d/%d kg | +%d XP", ScrapMatName[mat], ScrapMatWeight[mat], Scrap_CargoWeight(playerid), Scrap_Capacity(level), xp);
	SendClientMessage(playerid, COLOR_WHITE, str);

	Scrap_AddXP(playerid, xp);
	return 1;
}

stock Scrap_TryPick(playerid, now) {
	new level = Scrap_Level(ScrapPlayer[playerid][splXP]), mat, str[128];

	if(IsPlayerInAnyVehicle(playerid) || GetPlayerState(playerid) != PLAYER_STATE_ONFOOT) return 0;
	if(GetPlayerVirtualWorld(playerid) != 0 || GetPlayerInterior(playerid) != 0) return 0;

	for(new i = 0; i < MAX_SCRAP_SPOTS; i++) {
		if(!ScrapSpots[i][sspUsed] || ScrapSpots[i][sspZone] == 0) continue;
		if(ScrapSpots[i][sspRespawnAt] != 0 || ScrapSpots[i][sspReserved] != 0) continue;

		if(!IsPlayerInRangeOfPoint(playerid, SCRAP_PICK_RADIUS, ScrapSpots[i][sspX], ScrapSpots[i][sspY], ScrapSpots[i][sspZ])) continue;
		if(!IsPlayerFacingObject(playerid, ScrapSpots[i][sspObject])) continue;

		if(level < ScrapZoneLevel[ScrapSpots[i][sspZone]]) {
			format(str, sizeof(str), "Zlomiarz: Ta strefa wymaga poziomu %d (Twoj poziom: %d).", ScrapZoneLevel[ScrapSpots[i][sspZone]], level);
			Scrap_Notice(playerid, now, str);
			return 0;
		}

		mat = ScrapSpots[i][sspMaterial];

		if(Scrap_CargoWeight(playerid) + ScrapMatWeight[mat] > Scrap_Capacity(level)) {
			Scrap_Notice(playerid, now, "Zlomiarz: Torba jest pelna - sprzedaj zlom w skupie (/zlom sprzedaj).");
			return 0;
		}

		ScrapActionCounter++;
		ScrapPlayer[playerid][splBusy] = 1;
		ScrapPlayer[playerid][splActionId] = ScrapActionCounter;
		ScrapSpots[i][sspReserved] = playerid + 1;

		osrp_TogglePlayerControllable(playerid, 0);
		ApplyAnimation(playerid, "BOMBER", "BOM_Plant", 4.1, 1, 0, 0, 0, 0, 1);
		GameTextForPlayer(playerid, "~n~~y~Zbieranie zlomu...", Scrap_ActionTime(level), 3);

		SetTimerEx("ScrapActionDone", Scrap_ActionTime(level), false, "iii", playerid, i, ScrapActionCounter);
		return 1;
	}
	return 0;
}

task ScrapTimer[250]() {
	new now = gettime();

	// co godzine nowe ceny rynkowe
	if(now / 3600 != ScrapMarketHour) {
		ScrapMarketHour = now / 3600;
		Scrap_MarketTick(1);
	}

	// raz na sekunde: odnawianie punktow zbierania
	if(now != ScrapLastCheck) {
		ScrapLastCheck = now;

		for(new i = 0; i < MAX_SCRAP_SPOTS; i++) {
			if(!ScrapSpots[i][sspUsed] || ScrapSpots[i][sspZone] == 0) continue;
			if(ScrapSpots[i][sspRespawnAt] == 0 || now < ScrapSpots[i][sspRespawnAt]) continue;
			if(ScrapSpots[i][sspReserved] != 0) continue;

			Scrap_SpotSpawn(i);
		}
	}

	for(new i = 0; i < MAX_PLAYERS; i++) {
		if(!IsPlayerConnected(i) || !gPlayerLogged[i]) {
			if(ScrapPlayer[i][splChar]) Scrap_Reset(i);		// po wyjsciu z serwera torba przepada
			continue;
		}

		if(!Scrap_Sync(i)) continue;

		if(ScrapPlayer[i][splActive] && !ScrapPlayer[i][splBusy]) Scrap_TryPick(i, now);
	}
}

// ---------------------------------------------------------------------------
// Komendy gracza: /zlom
// ---------------------------------------------------------------------------
stock Scrap_Usage(playerid) {
	SendClientMessage(playerid, COLOR_GRAD1, "Uzycie: /zlom [start | stop | sprzedaj | torba | ceny | poziom | zlecenie | top | wyrzuc tak]");
	return 1;
}

stock Scrap_CmdStart(playerid) {
	new level = Scrap_Level(ScrapPlayer[playerid][splXP]), str[144];

	if(!Scrap_NearBuyer(playerid)) {
		SendClientMessage(playerid, COLOR_GREY, "Zlomiarz: Zglos sie u kierownika w skupie zlomu.");
		return 1;
	}

	if(ScrapPlayer[playerid][splActive]) {
		SendClientMessage(playerid, COLOR_GREY, "Zlomiarz: Juz pracujesz. Zbieraj zlom w strefach (patrz na obiekt i stan blisko niego).");
		return 1;
	}

	ScrapPlayer[playerid][splActive] = 1;

	format(str, sizeof(str), "Zlomiarz: Zaczynasz zmiane. Poziom %d, torba %d kg. Zlom zbierasz stojac blisko niego i patrzac na niego.", level, Scrap_Capacity(level));
	SendClientMessage(playerid, COLOR_GREEN, str);
	return 1;
}

stock Scrap_CmdStop(playerid) {
	if(!ScrapPlayer[playerid][splActive]) {
		SendClientMessage(playerid, COLOR_GREY, "Zlomiarz: Nie pracujesz.");
		return 1;
	}

	ScrapPlayer[playerid][splActive] = 0;
	SendClientMessage(playerid, COLOR_YELLOW, "Zlomiarz: Konczysz zmiane. Zlom w torbie mozesz jeszcze sprzedac w skupie.");
	return 1;
}

stock Scrap_CmdSell(playerid) {
	new level = Scrap_Level(ScrapPlayer[playerid][splXP]), total, kg, n, w, qmat, qkg, str[160];
	new Float:unit;

	if(!Scrap_NearBuyer(playerid)) {
		SendClientMessage(playerid, COLOR_GREY, "Zlomiarz: Musisz byc w skupie zlomu.");
		return 1;
	}

	if(Scrap_CargoWeight(playerid) <= 0) {
		SendClientMessage(playerid, COLOR_GREY, "Zlomiarz: Twoja torba jest pusta.");
		return 1;
	}

	Scrap_QuestCheckDay(playerid);
	Scrap_QuestGet(qmat, qkg);

	for(new m = 0; m < SCRAP_MATS; m++) {
		n = ScrapPlayer[playerid][splCargo][m];

		if(n <= 0) continue;

		w = n * ScrapMatWeight[m];
		unit = Scrap_UnitPrice(m, level);
		total += floatround(unit * float(w));
		kg += w;

		ScrapPressure[m] += w;
		Scrap_MarketSave(m);

		if(m == qmat && !ScrapPlayer[playerid][splQuestDone]) ScrapPlayer[playerid][splQuestProgress] += w;

		ScrapPlayer[playerid][splCargo][m] = 0;
	}

	ScrapPlayer[playerid][splDelivered] += kg;
	ScrapPlayer[playerid][splEarned] += total;

	GivePlayerMoney(playerid, total);
	ConsumingMoney[playerid] = 1;

	format(str, sizeof(str), "Zlomiarz: Sprzedales %d kg zlomu za $%d.", kg, total);
	SendClientMessage(playerid, COLOR_GREEN, str);

	// zlecenie dzienne
	if(!ScrapPlayer[playerid][splQuestDone] && ScrapPlayer[playerid][splQuestProgress] >= qkg) {
		ScrapPlayer[playerid][splQuestDone] = 1;

		GivePlayerMoney(playerid, qkg * SCRAP_QUEST_CASH_PER_KG);
		ConsumingMoney[playerid] = 1;

		format(str, sizeof(str), "Zlomiarz: Zlecenie dzienne wykonane! Premia: $%d i %d XP.", qkg * SCRAP_QUEST_CASH_PER_KG, qkg * SCRAP_QUEST_XP_PER_KG);
		SendClientMessage(playerid, COLOR_YELLOW, str);

		Scrap_AddXP(playerid, qkg * SCRAP_QUEST_XP_PER_KG);		// zapisuje tez reszte danych gracza
	} else {
		Scrap_Save(playerid);
	}
	return 1;
}

stock Scrap_CmdBag(playerid) {
	new level = Scrap_Level(ScrapPlayer[playerid][splXP]), str[128];

	format(str, sizeof(str), "------ Torba zlomiarza: %d/%d kg ------", Scrap_CargoWeight(playerid), Scrap_Capacity(level));
	SendClientMessage(playerid, COLOR_YELLOW, str);

	for(new m = 0; m < SCRAP_MATS; m++) {
		if(ScrapPlayer[playerid][splCargo][m] <= 0) continue;

		format(str, sizeof(str), "%s: %d szt. (%d kg)", ScrapMatName[m], ScrapPlayer[playerid][splCargo][m], ScrapPlayer[playerid][splCargo][m] * ScrapMatWeight[m]);
		SendClientMessage(playerid, COLOR_WHITE, str);
	}

	if(Scrap_CargoWeight(playerid) <= 0) SendClientMessage(playerid, COLOR_GREY, "Torba jest pusta.");
	return 1;
}

stock Scrap_CmdPrices(playerid) {
	new level = Scrap_Level(ScrapPlayer[playerid][splXP]), str[128], trend;

	SendClientMessage(playerid, COLOR_YELLOW, "------ Ceny skupu zlomu (za 1 kg, z Twoim bonusem poziomu) ------");

	for(new m = 0; m < SCRAP_MATS; m++) {
		trend = (Scrap_Factor(m) - 1000) / 10;

		format(str, sizeof(str), "%s: $%.2f/kg (rynek: %d%%) | %d kg/szt.", ScrapMatName[m], Scrap_UnitPrice(m, level), trend, ScrapMatWeight[m]);
		SendClientMessage(playerid, COLOR_WHITE, str);
	}
	return 1;
}

stock Scrap_CmdLevel(playerid) {
	new xp = ScrapPlayer[playerid][splXP], level = Scrap_Level(xp), str[160];

	format(str, sizeof(str), "------ Zlomiarz: poziom %d/%d ------", level, SCRAP_MAX_LEVEL);
	SendClientMessage(playerid, COLOR_YELLOW, str);

	if(level < SCRAP_MAX_LEVEL) {
		format(str, sizeof(str), "XP: %d (do poziomu %d brakuje %d)", xp, level + 1, Scrap_XPForLevel(level + 1) - xp);
	} else {
		format(str, sizeof(str), "XP: %d (maksymalny poziom)", xp);
	}
	SendClientMessage(playerid, COLOR_WHITE, str);

	format(str, sizeof(str), "Torba: %d kg | Czas zbierania: %.1f s | Bonus do cen: +%d.%d%%", Scrap_Capacity(level), float(Scrap_ActionTime(level)) / 1000.0, SCRAP_PRICE_BONUS_LEVEL * (level - 1) / 10, SCRAP_PRICE_BONUS_LEVEL * (level - 1) % 10);
	SendClientMessage(playerid, COLOR_WHITE, str);

	format(str, sizeof(str), "Dostarczono lacznie: %d kg | Zarobiono lacznie: $%d", ScrapPlayer[playerid][splDelivered], ScrapPlayer[playerid][splEarned]);
	SendClientMessage(playerid, COLOR_WHITE, str);

	for(new z = 1; z <= SCRAP_ZONES; z++) {
		if(ScrapZoneLevel[z] <= level) format(str, sizeof(str), "Strefa %d: odblokowana", z);
		else format(str, sizeof(str), "Strefa %d: wymaga poziomu %d", z, ScrapZoneLevel[z]);

		SendClientMessage(playerid, COLOR_GRAD1, str);
	}
	return 1;
}

stock Scrap_CmdQuest(playerid) {
	new qmat, qkg, str[160];

	Scrap_QuestCheckDay(playerid);
	Scrap_QuestGet(qmat, qkg);

	format(str, sizeof(str), "Zlecenie dzienne: dostarcz %d kg materialu: %s.", qkg, ScrapMatName[qmat]);
	SendClientMessage(playerid, COLOR_YELLOW, str);

	if(ScrapPlayer[playerid][splQuestDone]) {
		SendClientMessage(playerid, COLOR_GREEN, "Wykonane! Nowe zlecenie pojawi sie jutro.");
		return 1;
	}

	format(str, sizeof(str), "Postep: %d/%d kg | Nagroda: $%d i %d XP.", ScrapPlayer[playerid][splQuestProgress], qkg, qkg * SCRAP_QUEST_CASH_PER_KG, qkg * SCRAP_QUEST_XP_PER_KG);
	SendClientMessage(playerid, COLOR_WHITE, str);
	return 1;
}

stock Scrap_CmdTop(playerid) {
	new list[1024], line[96], row[64], name[24], xp, place;

	strcat(list, "Miejsce\tNick\tPoziom\tXP\n");

	queryStr[0] = EOS;
	format(queryStr, sizeof(queryStr), "SELECT c.`Name`, j.`XP` FROM `JobStats` j JOIN `Characters` c ON c.`ID` = j.`CharID` WHERE j.`JobID` = '%d' ORDER BY j.`XP` DESC LIMIT 10", SCRAP_JOB_ID);
	mysql_query(queryStr);
	mysql_store_result();

	if(mysql_result_stored()) {
		while(mysql_fetch_row(row, "|")) {
			sscanf(row, "p<|>s[24]i", name, xp);

			place++;
			format(line, sizeof(line), "%d.\t%s\t%d\t%d\n", place, name, Scrap_Level(xp), xp);
			strcat(list, line);
		}
	}

	mysql_free_result();

	if(!place) {
		SendClientMessage(playerid, COLOR_GREY, "Zlomiarz: Ranking jest jeszcze pusty.");
		return 1;
	}

	osrp_ShowPlayerDialog(playerid, DIALOG_SCRAP_TOP, DIALOG_STYLE_TABLIST_HEADERS, "Najlepsi zlomiarze", list, "Zamknij", "");
	return 1;
}

stock Scrap_CmdDump(playerid, const rest[]) {
	if(strcmp(rest, "tak", true)) {
		SendClientMessage(playerid, COLOR_GRAD1, "Uzycie: /zlom wyrzuc tak (wyrzuca caly zlom z torby bez zaplaty!)");
		return 1;
	}

	for(new m = 0; m < SCRAP_MATS; m++) ScrapPlayer[playerid][splCargo][m] = 0;

	SendClientMessage(playerid, COLOR_YELLOW, "Zlomiarz: Wyrzuciles caly zlom z torby.");
	return 1;
}

CMD:zlom(playerid, params[]) {
	new sub[16], rest[32], pos;

	if(!gPlayerLogged[playerid]) return 1;

	if(!Scrap_Sync(playerid)) return 1;

	Scrap_NextToken(params, sub, sizeof(sub), pos);
	Scrap_Rest(params, rest, sizeof(rest), pos);

	if(isnull(sub)) return Scrap_Usage(playerid);

	if(!strcmp(sub, "start", true)) return Scrap_CmdStart(playerid);
	if(!strcmp(sub, "stop", true)) return Scrap_CmdStop(playerid);
	if(!strcmp(sub, "sprzedaj", true)) return Scrap_CmdSell(playerid);
	if(!strcmp(sub, "torba", true)) return Scrap_CmdBag(playerid);
	if(!strcmp(sub, "ceny", true)) return Scrap_CmdPrices(playerid);
	if(!strcmp(sub, "poziom", true)) return Scrap_CmdLevel(playerid);
	if(!strcmp(sub, "zlecenie", true)) return Scrap_CmdQuest(playerid);
	if(!strcmp(sub, "top", true)) return Scrap_CmdTop(playerid);
	if(!strcmp(sub, "wyrzuc", true)) return Scrap_CmdDump(playerid, rest);

	return Scrap_Usage(playerid);
}

// ---------------------------------------------------------------------------
// Administracja: /zlompunkt [dodaj strefa | usun id | najblizszy | lista]
// strefa 0 = skup zlomu, 1-3 = strefy zbierania
// ---------------------------------------------------------------------------
CMD:zlompunkt(playerid, params[]) {
	new sub[16], rest[16], pos, str[144];

	if(!Scrap_IsAdmin(playerid)) return 1;

	Scrap_NextToken(params, sub, sizeof(sub), pos);
	Scrap_Rest(params, rest, sizeof(rest), pos);

	if(isnull(sub)) {
		SendClientMessage(playerid, COLOR_GRAD1, "Uzycie: /zlompunkt [dodaj strefa | usun ID | najblizszy | lista]  (strefa 0 = skup, 1-3 = zbieranie)");
		return 1;
	}

	if(!strcmp(sub, "dodaj", true)) {
		new zone, slot, newId, Float:x, Float:y, Float:z;

		if(isnull(rest) || strlen(rest) > 1 || rest[0] < '0' || rest[0] > '9') {
			SendClientMessage(playerid, COLOR_GRAD1, "Uzycie: /zlompunkt dodaj [strefa 0-3]");
			return 1;
		}

		zone = rest[0] - '0';

		if(zone > SCRAP_ZONES) {
			SendClientMessage(playerid, COLOR_GRAD1, "Uzycie: /zlompunkt dodaj [strefa 0-3]");
			return 1;
		}

		if(IsPlayerInAnyVehicle(playerid) || GetPlayerVirtualWorld(playerid) != 0 || GetPlayerInterior(playerid) != 0) {
			SendClientMessage(playerid, COLOR_GREY, "Zlomiarz: Stan na zewnatrz (swiat 0, interior 0), na nogach.");
			return 1;
		}

		slot = Scrap_FindFreeSpot();

		if(slot == -1) {
			SendClientMessage(playerid, COLOR_GREY, "Zlomiarz: Osiagnieto maksymalna liczbe punktow (MAX_SCRAP_SPOTS).");
			return 1;
		}

		GetPlayerPos(playerid, x, y, z);

		if(zone > 0) z -= 0.9;		// obiekt na poziomie ziemi

		queryStr[0] = EOS;
		format(queryStr, sizeof(queryStr), "INSERT INTO `ScrapSpots` (`Zone`, `PosX`, `PosY`, `PosZ`) VALUES ('%d', '%.4f', '%.4f', '%.4f')", zone, x, y, z);
		mysql_query(queryStr);

		newId = mysql_insert_id();

		if(!newId) {
			SendClientMessage(playerid, COLOR_GREY, "Zlomiarz: Nie udalo sie zapisac punktu w bazie.");
			return 1;
		}

		ScrapSpots[slot][sspUsed] = 1;
		ScrapSpots[slot][sspSQL] = newId;
		ScrapSpots[slot][sspZone] = zone;
		ScrapSpots[slot][sspX] = x;
		ScrapSpots[slot][sspY] = y;
		ScrapSpots[slot][sspZ] = z;
		ScrapSpots[slot][sspObject] = 0;
		ScrapSpots[slot][sspPickup] = 0;
		ScrapSpots[slot][sspLabel] = 0;
		ScrapSpots[slot][sspRespawnAt] = 0;
		ScrapSpots[slot][sspReserved] = 0;

		if(zone == 0) Scrap_BuyerVisuals(slot);
		else Scrap_SpotSpawn(slot);

		format(str, sizeof(str), "Zlomiarz: Dodano punkt ID %d (strefa %d).", newId, zone);
		SendClientMessage(playerid, COLOR_GREEN, str);
		return 1;
	}

	if(!strcmp(sub, "usun", true)) {
		new sqlid = strval(rest), found = -1;

		if(isnull(rest)) {
			SendClientMessage(playerid, COLOR_GRAD1, "Uzycie: /zlompunkt usun [ID punktu] (ID: /zlompunkt najblizszy)");
			return 1;
		}

		for(new i = 0; i < MAX_SCRAP_SPOTS; i++) {
			if(ScrapSpots[i][sspUsed] && ScrapSpots[i][sspSQL] == sqlid) found = i;
		}

		if(found == -1) {
			SendClientMessage(playerid, COLOR_GREY, "Zlomiarz: Nie ma punktu o takim ID.");
			return 1;
		}

		Scrap_SpotRemove(found);

		SendClientMessage(playerid, COLOR_GREEN, "Zlomiarz: Usunieto punkt.");
		return 1;
	}

	if(!strcmp(sub, "najblizszy", true)) {
		new best = -1, Float:bestDist = 15.0, Float:dist;

		for(new i = 0; i < MAX_SCRAP_SPOTS; i++) {
			if(!ScrapSpots[i][sspUsed]) continue;

			dist = GetPlayerDistanceFromPoint(playerid, ScrapSpots[i][sspX], ScrapSpots[i][sspY], ScrapSpots[i][sspZ]);

			if(dist < bestDist) {
				bestDist = dist;
				best = i;
			}
		}

		if(best == -1) {
			SendClientMessage(playerid, COLOR_GREY, "Zlomiarz: W promieniu 15 m nie ma zadnego punktu.");
			return 1;
		}

		format(str, sizeof(str), "Zlomiarz: Najblizszy punkt: ID %d, strefa %d, %.1f m.", ScrapSpots[best][sspSQL], ScrapSpots[best][sspZone], bestDist);
		SendClientMessage(playerid, COLOR_YELLOW, str);
		return 1;
	}

	if(!strcmp(sub, "lista", true)) {
		new counts[SCRAP_ZONES + 1];

		for(new i = 0; i < MAX_SCRAP_SPOTS; i++) {
			if(ScrapSpots[i][sspUsed]) counts[ScrapSpots[i][sspZone]]++;
		}

		format(str, sizeof(str), "Zlomiarz: Skupy: %d | strefa 1: %d | strefa 2: %d | strefa 3: %d punktow zbierania.", counts[0], counts[1], counts[2], counts[3]);
		SendClientMessage(playerid, COLOR_YELLOW, str);
		return 1;
	}

	SendClientMessage(playerid, COLOR_GRAD1, "Uzycie: /zlompunkt [dodaj strefa | usun ID | najblizszy | lista]");
	return 1;
}
