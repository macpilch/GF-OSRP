/*
	Project: GodFather
	File name: systems/orgs.inc
	Desc: Dynamiczny system organizacji przestepczych (w pelni na bazie danych).

	- organizacje tworzy i usuwa administrator serwera (pAdmin) - tylko /orgstworz, /orgusun, /orglista,
	- wszystkie uprawnienia WEWNATRZ organizacji wynikaja wylacznie z rangi (admin serwera nie ma tam wyjatkow),
	- jedna postac moze nalezec tylko do jednej organizacji,
	- 5 poziomow rang; szef nadaje im wlasne nazwy i ustawia uprawnienia kazdej rangi,
	- wspolny skarbiec, MOTD, czat organizacji, lista czlonkow takze offline.

	Komendy:
	  /org [info | czlonkowie | zapros | akceptuj | wyrzuc | awans | degraduj | lider | wyjdz | motd | wplac | wyplac]
	  /oc [tekst]                         - czat organizacji
	  /org hq [ustaw | usun]              - siedziba organizacji
	  /org rangi, /org ranga ...          - podglad rang i (dla szefa) ich edycja
	  /hq [zapukaj | wpusc ID]            - wejscie / wyjscie z siedziby, pukanie, wpuszczanie gosci
	  /orgstworz, /orgusun, /orglista     - administracja
*/

// ---------------------------------------------------------------------------
// Ustawienia
// ---------------------------------------------------------------------------
#define MAX_ORGS				20
#define ORG_MAX_MEMBERS			30			// limit czlonkow jednej organizacji
#define ORG_MAX_RANK			5			// najwyzsza ranga = szef (ma zawsze wszystkie uprawnienia)
#define ORG_RANK_NAME_LEN		24

// Uprawnienia rang (maska bitowa zapisywana w OrgRanks.Perms)
#define ORG_PERM_INVITE			(1 << 0)	// zapraszanie
#define ORG_PERM_KICK			(1 << 1)	// wyrzucanie
#define ORG_PERM_PROMOTE		(1 << 2)	// awanse i degradacje
#define ORG_PERM_MOTD			(1 << 3)	// zmiana MOTD
#define ORG_PERM_WITHDRAW		(1 << 4)	// wyplaty ze skarbca
#define ORG_PERM_GUEST			(1 << 5)	// wpuszczanie gosci do siedziby
#define ORG_PERM_ALL			63
#define ORG_INVITE_TIME			60			// sekundy na przyjecie zaproszenia
#define ORG_MAX_MONEY			2000000000	// maksymalny stan skarbca
#define ORG_ADMIN_LEVEL			1			// pAdmin >= 1 = administrator serwera (tylko do /orgstworz, /orgusun, /orglista)
// --- Siedziba (HQ) ---
#define ORG_RANK_HQ_SET			5			// ustawienie / usuniecie siedziby: tylko szef (najwyzsza ranga)
#define ORG_HQ_COST				0			// koszt ustawienia siedziby ze skarbca (0 = za darmo)
#define ORG_HQ_MIN_DIST			50.0		// minimalna odleglosc miedzy wejsciami roznych siedzib
#define ORG_HQ_VW_BASE			10000		// swiat wirtualny siedziby = ORG_HQ_VW_BASE + ID organizacji
#define ORG_HQ_GUEST_TIME		60			// sekundy na wejscie po wpuszczeniu
#define ORG_HQ_KNOCK_COOLDOWN	15			// sekundy miedzy pukaniami

// Interior siedziby (wspolny dla wszystkich organizacji, kazda ma swoj swiat wirtualny).
// UWAGA: wspolrzedne przykladowe (dom Sweeta, int 1) - sprawdz w grze i podmien na wlasne.
#define ORG_HQ_INT				1
#define ORG_HQ_INX				2527.6540
#define ORG_HQ_INY				-1679.3883
#define ORG_HQ_INZ				1015.4986
#define ORG_HQ_INA				270.0

#define DIALOG_ORG_MEMBERS		201			// sprawdz, czy to ID jest u Ciebie wolne

new const OrgDefaultRankNames[ORG_MAX_RANK + 1][] = {
	"Brak",
	"Rekrut",
	"Czlonek",
	"Zaufany",
	"Zastepca",
	"Szef"
};

// ---------------------------------------------------------------------------
// Dane
// ---------------------------------------------------------------------------
enum eOrg {
	oUsed,
	oSQL,
	oName[32],
	oColor,				// 24-bit RGB
	oMoney,
	oMotd[96],
	oHQSet,				// 1 = siedziba ustawiona
	Float:oHQX,			// wejscie na zewnatrz
	Float:oHQY,
	Float:oHQZ,
	Float:oHQA,
	oHQPickup,			// dynamiczne pickupy i napisy (0 = brak)
	oHQLabel,
	oHQInPickup,
	oHQInLabel
};

new OrgData[MAX_ORGS][eOrg];
new OrgRankName[MAX_ORGS][ORG_MAX_RANK + 1][ORG_RANK_NAME_LEN];	// wlasne nazwy rang
new OrgRankPerm[MAX_ORGS][ORG_MAX_RANK + 1];					// maski uprawnien rang

enum ePlayerOrg {
	poOrg,				// indeks w OrgData, -1 = brak
	poChar,				// ID postaci, dla ktorej wazne sa dane (zabezpieczenie przed zerami z pamieci)
	poRank,
	poInviteOrg,		// indeks organizacji, ktora zaprosila (-1 = brak)
	poInviteTime,		// do kiedy wazne zaproszenie (unix)
	poInsideHQ,			// 1 = gracz wyszedl z serwera w siedzibie swojej organizacji (z bazy)
	poLoginSpawn,		// ile najblizszych spawnow ma isc na ostatnia pozycje (ustawiane po zalogowaniu)
	poGuestOrg,			// organizacja, do ktorej gracz zostal wpuszczony jako gosc
	poGuestTime			// do kiedy wazne wpuszczenie (unix)
};

new OrgPlayer[MAX_PLAYERS][ePlayerOrg];
new OrgKnockTime[MAX_PLAYERS];

// Indeks organizacji gracza albo -1. Zwraca wartosc tylko, gdy dane zostaly wczytane
// dla aktualnej postaci (Org_OnLogin) - brak inicjalizacji nie daje falszywego czlonkostwa.
stock Org_Idx(playerid) {
	new idx = OrgPlayer[playerid][poOrg];

	if(!gPlayerLogged[playerid]) return -1;
	if(PlayerInfo[playerid][pID] <= 0 || OrgPlayer[playerid][poChar] != PlayerInfo[playerid][pID]) return -1;
	if(idx < 0 || idx >= MAX_ORGS || !OrgData[idx][oUsed]) return -1;
	return idx;
}

// ---------------------------------------------------------------------------
// Funkcje pomocnicze
// ---------------------------------------------------------------------------
stock Org_Color(idx) {
	return (OrgData[idx][oColor] << 8) | 0xAA;
}

stock Org_GetBySQL(sqlid) {
	for(new i = 0; i < MAX_ORGS; i++) {
		if(OrgData[i][oUsed] && OrgData[i][oSQL] == sqlid) return i;
	}
	return -1;
}

stock Org_FindFreeSlot() {
	for(new i = 0; i < MAX_ORGS; i++) {
		if(!OrgData[i][oUsed]) return i;
	}
	return -1;
}

stock Org_ValidNick(const nick[]) {
	new len = strlen(nick);

	if(len < 3 || len > 24) return 0;

	for(new i = 0; i < len; i++) {
		if(nick[i] >= 'a' && nick[i] <= 'z') continue;
		if(nick[i] >= 'A' && nick[i] <= 'Z') continue;
		if(nick[i] >= '0' && nick[i] <= '9') continue;
		if(nick[i] == '_') continue;
		return 0;
	}
	return 1;
}

stock Org_ValidName(const name[]) {
	new len = strlen(name);

	if(len < 3 || len > 31) return 0;

	for(new i = 0; i < len; i++) {
		if(name[i] >= 'a' && name[i] <= 'z') continue;
		if(name[i] >= 'A' && name[i] <= 'Z') continue;
		if(name[i] >= '0' && name[i] <= '9') continue;
		if(name[i] == ' ' || name[i] == '-' || name[i] == '_') continue;
		return 0;
	}
	return 1;
}

stock Org_Send(idx, color, const msg[]) {
	for(new i = 0; i < MAX_PLAYERS; i++) {
		if(IsPlayerConnected(i) && gPlayerLogged[i] && Org_Idx(i) == idx) {
			SendClientMessage(i, color, msg);
		}
	}
}

stock Org_FindOnlineByChar(charid) {
	for(new i = 0; i < MAX_PLAYERS; i++) {
		if(IsPlayerConnected(i) && gPlayerLogged[i] && PlayerInfo[i][pID] == charid) return i;
	}
	return INVALID_PLAYER_ID;
}

stock Org_CountMembers(idx) {
	new row[16], count;

	queryStr[0] = EOS;
	format(queryStr, sizeof(queryStr), "SELECT COUNT(*) FROM `OrgMembers` WHERE `OrgID` = '%d'", OrgData[idx][oSQL]);
	mysql_query(queryStr);
	mysql_store_result();

	if(mysql_result_stored()) {
		if(mysql_fetch_row(row, "|")) count = strval(row);
	}

	mysql_free_result();
	return count;
}

// Sprawdza czlonkostwo i range. Zwraca indeks organizacji albo -1 (z komunikatem).
stock Org_Require(playerid, minRank = 1) {
	new idx = Org_Idx(playerid), str[96];

	if(idx == -1) {
		SendClientMessage(playerid, COLOR_GREY, "Organizacje: Nie nalezysz do zadnej organizacji.");
		return -1;
	}

	if(OrgPlayer[playerid][poRank] < minRank) {
		format(str, sizeof(str), "Organizacje: Wymagana ranga: %s.", OrgRankName[idx][minRank]);
		SendClientMessage(playerid, COLOR_GREY, str);
		return -1;
	}
	return idx;
}

// ---------------------------------------------------------------------------
// Rangi i uprawnienia
// ---------------------------------------------------------------------------
stock Org_DefaultPerm(rank) {
	if(rank >= ORG_MAX_RANK - 1) return ORG_PERM_ALL;	// Zastepca i Szef
	if(rank == 3) return ORG_PERM_GUEST;				// Zaufany
	return 0;
}

stock Org_RanksSetDefaults(idx) {
	for(new r = 1; r <= ORG_MAX_RANK; r++) {
		format(OrgRankName[idx][r], ORG_RANK_NAME_LEN, "%s", OrgDefaultRankNames[r]);
		OrgRankPerm[idx][r] = Org_DefaultPerm(r);
	}
}

// Wstawia do bazy rangi, ktorych tam jeszcze nie ma (wartosci z pamieci). Istniejace wiersze zostaja.
stock Org_RanksInsertMissing(idx) {
	new row[128], escaped[64];

	queryStr[0] = EOS;
	strcat(queryStr, "INSERT IGNORE INTO `OrgRanks` (`OrgID`, `Rank`, `Name`, `Perms`) VALUES ", sizeof(queryStr));

	for(new r = 1; r <= ORG_MAX_RANK; r++) {
		mysql_real_escape_string(OrgRankName[idx][r], escaped);

		if(r > 1) strcat(queryStr, ", ", sizeof(queryStr));

		format(row, sizeof(row), "('%d', '%d', '%s', '%d')", OrgData[idx][oSQL], r, escaped, OrgRankPerm[idx][r]);
		strcat(queryStr, row, sizeof(queryStr));
	}

	mysql_query(queryStr);
}

stock Org_PermText(perms, dest[], size = sizeof(dest)) {
	dest[0] = EOS;

	if(perms & ORG_PERM_INVITE) strcat(dest, "zapros ", size);
	if(perms & ORG_PERM_KICK) strcat(dest, "wyrzuc ", size);
	if(perms & ORG_PERM_PROMOTE) strcat(dest, "awans ", size);
	if(perms & ORG_PERM_MOTD) strcat(dest, "motd ", size);
	if(perms & ORG_PERM_WITHDRAW) strcat(dest, "wyplac ", size);
	if(perms & ORG_PERM_GUEST) strcat(dest, "wpusc ", size);

	if(isnull(dest)) format(dest, size, "brak");
}

// Sprawdza czlonkostwo i konkretne uprawnienie rangi (szef ma zawsze wszystkie).
// Zwraca indeks organizacji albo -1 (z komunikatem).
stock Org_RequirePerm(playerid, perm, const what[]) {
	new idx = Org_Idx(playerid), rank = OrgPlayer[playerid][poRank], str[144];

	if(idx == -1) {
		SendClientMessage(playerid, COLOR_GREY, "Organizacje: Nie nalezysz do zadnej organizacji.");
		return -1;
	}

	if(rank >= ORG_MAX_RANK) return idx;

	if(rank < 1 || !(OrgRankPerm[idx][rank] & perm)) {
		format(str, sizeof(str), "Organizacje: Twoja ranga (%s) nie ma uprawnienia: %s.", OrgRankName[idx][rank], what);
		SendClientMessage(playerid, COLOR_GREY, str);
		return -1;
	}
	return idx;
}

// Szuka czlonka organizacji po nicku (takze offline).
stock Org_LookupMember(idx, const nick[], &charid, &rank) {
	new row[32], found;

	if(!Org_ValidNick(nick)) return 0;

	queryStr[0] = EOS;
	format(queryStr, sizeof(queryStr), "SELECT m.`CharID`, m.`Rank` FROM `OrgMembers` m JOIN `Characters` c ON c.`ID` = m.`CharID` WHERE c.`Name` = '%s' AND m.`OrgID` = '%d' LIMIT 1", nick, OrgData[idx][oSQL]);
	mysql_query(queryStr);
	mysql_store_result();

	if(mysql_result_stored()) {
		if(mysql_fetch_row(row, "|")) {
			sscanf(row, "p<|>ii", charid, rank);
			found = 1;
		}
	}

	mysql_free_result();
	return found;
}

// ---------------------------------------------------------------------------
// Siedziba (HQ) - funkcje pomocnicze
// ---------------------------------------------------------------------------
stock Org_HQExit(playerid, idx) {
	SetPlayerVirtualWorld(playerid, 0);
	SetPlayerInterior(playerid, 0);
	SetPlayerPos(playerid, OrgData[idx][oHQX], OrgData[idx][oHQY], OrgData[idx][oHQZ]);
	SetPlayerFacingAngle(playerid, OrgData[idx][oHQA]);
	SetCameraBehindPlayer(playerid);
	PlayerInfo[playerid][pInt] = 0;
}

stock Org_HQEnter(playerid, idx) {
	SetPlayerVirtualWorld(playerid, ORG_HQ_VW_BASE + OrgData[idx][oSQL]);
	SetPlayerInterior(playerid, ORG_HQ_INT);
	SetPlayerPos(playerid, ORG_HQ_INX, ORG_HQ_INY, ORG_HQ_INZ);
	SetPlayerFacingAngle(playerid, ORG_HQ_INA);
	SetCameraBehindPlayer(playerid);
	PlayerInfo[playerid][pInt] = ORG_HQ_INT;
}

// Indeks organizacji, w ktorej siedzibie stoi gracz (albo -1).
stock Org_HQInsideIdx(playerid) {
	new vw = GetPlayerVirtualWorld(playerid), idx;

	if(vw <= ORG_HQ_VW_BASE || GetPlayerInterior(playerid) != ORG_HQ_INT) return -1;

	idx = Org_GetBySQL(vw - ORG_HQ_VW_BASE);

	if(idx == -1 || !OrgData[idx][oHQSet]) return -1;
	return idx;
}

// Jesli gracz jest w jakiejs siedzibie - wyrzuca go na zewnatrz.
stock Org_HQEjectPlayer(playerid) {
	new idx = Org_HQInsideIdx(playerid);

	if(idx != -1) Org_HQExit(playerid, idx);
}

stock Org_HQEjectAll(idx) {
	new vw = ORG_HQ_VW_BASE + OrgData[idx][oSQL];

	for(new i = 0; i < MAX_PLAYERS; i++) {
		if(IsPlayerConnected(i) && gPlayerLogged[i] && GetPlayerVirtualWorld(i) == vw && GetPlayerInterior(i) == ORG_HQ_INT) {
			Org_HQExit(i, idx);
		}
	}
}

stock Org_HQDestroyVisuals(idx) {
	if(OrgData[idx][oHQPickup]) DestroyDynamicPickup(OrgData[idx][oHQPickup]);
	if(OrgData[idx][oHQLabel]) DestroyDynamic3DTextLabel(Text3D:OrgData[idx][oHQLabel]);
	if(OrgData[idx][oHQInPickup]) DestroyDynamicPickup(OrgData[idx][oHQInPickup]);
	if(OrgData[idx][oHQInLabel]) DestroyDynamic3DTextLabel(Text3D:OrgData[idx][oHQInLabel]);

	OrgData[idx][oHQPickup] = 0;
	OrgData[idx][oHQLabel] = 0;
	OrgData[idx][oHQInPickup] = 0;
	OrgData[idx][oHQInLabel] = 0;
}

stock Org_HQCreateVisuals(idx) {
	new text[96], vw = ORG_HQ_VW_BASE + OrgData[idx][oSQL], color = (OrgData[idx][oColor] << 8) | 0xFF;

	Org_HQDestroyVisuals(idx);

	if(!OrgData[idx][oHQSet]) return 0;

	// wejscie (swiat 0, interior 0)
	OrgData[idx][oHQPickup] = CreateDynamicPickup(1318, 1, OrgData[idx][oHQX], OrgData[idx][oHQY], OrgData[idx][oHQZ], 0, 0);

	format(text, sizeof(text), "Siedziba: %s\n{FFFFFF}/hq - wejscie, /hq zapukaj", OrgData[idx][oName]);
	OrgData[idx][oHQLabel] = _:CreateDynamic3DTextLabel(text, color, OrgData[idx][oHQX], OrgData[idx][oHQY], OrgData[idx][oHQZ] + 0.8, 15.0, INVALID_PLAYER_ID, INVALID_VEHICLE_ID, 0, 0, 0);

	// wyjscie (wlasny swiat wirtualny organizacji)
	OrgData[idx][oHQInPickup] = CreateDynamicPickup(1318, 1, ORG_HQ_INX, ORG_HQ_INY, ORG_HQ_INZ, vw, ORG_HQ_INT);

	format(text, sizeof(text), "%s\n{FFFFFF}/hq - wyjscie", OrgData[idx][oName]);
	OrgData[idx][oHQInLabel] = _:CreateDynamic3DTextLabel(text, color, ORG_HQ_INX, ORG_HQ_INY, ORG_HQ_INZ + 0.8, 10.0, INVALID_PLAYER_ID, INVALID_VEHICLE_ID, 0, vw, ORG_HQ_INT);
	return 1;
}

// Pelne usuniecie siedziby z mapy (gracze w srodku sa wyrzucani przed zmiana danych).
stock Org_HQDestroy(idx) {
	if(OrgData[idx][oHQSet]) Org_HQEjectAll(idx);
	Org_HQDestroyVisuals(idx);
}

// Siedziba, przy ktorej wejsciu stoi gracz (swiat 0, interior 0) albo -1.
stock Org_FindEntranceNear(playerid, Float:dist) {
	if(GetPlayerVirtualWorld(playerid) != 0 || GetPlayerInterior(playerid) != 0) return -1;

	for(new i = 0; i < MAX_ORGS; i++) {
		if(!OrgData[i][oUsed] || !OrgData[i][oHQSet]) continue;

		if(IsPlayerInRangeOfPoint(playerid, dist, OrgData[i][oHQX], OrgData[i][oHQY], OrgData[i][oHQZ])) return i;
	}
	return -1;
}

// ---------------------------------------------------------------------------
// Baza danych
// ---------------------------------------------------------------------------
stock Org_AddMember(idx, charid, rank) {
	new target;

	queryStr[0] = EOS;
	format(queryStr, sizeof(queryStr), "INSERT INTO `OrgMembers` (`CharID`, `OrgID`, `Rank`, `Joined`) VALUES ('%d', '%d', '%d', '%d')", charid, OrgData[idx][oSQL], rank, gettime());
	mysql_query(queryStr);

	target = Org_FindOnlineByChar(charid);

	if(target != INVALID_PLAYER_ID) {
		OrgPlayer[target][poOrg] = idx;
		OrgPlayer[target][poChar] = charid;
		OrgPlayer[target][poInsideHQ] = 0;
		OrgPlayer[target][poRank] = rank;
	}
}

stock Org_RemoveMember(charid) {
	new target;

	queryStr[0] = EOS;
	format(queryStr, sizeof(queryStr), "DELETE FROM `OrgMembers` WHERE `CharID` = '%d' LIMIT 1", charid);
	mysql_query(queryStr);

	target = Org_FindOnlineByChar(charid);

	if(target != INVALID_PLAYER_ID) {
		Org_HQEjectPlayer(target);		// jesli byl w siedzibie - wychodzi na zewnatrz
		OrgPlayer[target][poOrg] = -1;
		OrgPlayer[target][poRank] = 0;
	}
}

stock Org_SetRank(charid, rank) {
	new target;

	queryStr[0] = EOS;
	format(queryStr, sizeof(queryStr), "UPDATE `OrgMembers` SET `Rank` = '%d' WHERE `CharID` = '%d' LIMIT 1", rank, charid);
	mysql_query(queryStr);

	target = Org_FindOnlineByChar(charid);

	if(target != INVALID_PLAYER_ID) {
		OrgPlayer[target][poRank] = rank;
	}
}

stock Org_SaveMoney(idx) {
	queryStr[0] = EOS;
	format(queryStr, sizeof(queryStr), "UPDATE `Orgs` SET `Money` = '%d' WHERE `ID` = '%d' LIMIT 1", OrgData[idx][oMoney], OrgData[idx][oSQL]);
	mysql_query(queryStr);
}

stock Org_Remove(idx) {
	Org_HQDestroy(idx);

	queryStr[0] = EOS;
	format(queryStr, sizeof(queryStr), "DELETE FROM `OrgMembers` WHERE `OrgID` = '%d'", OrgData[idx][oSQL]);
	mysql_query(queryStr);

	queryStr[0] = EOS;
	format(queryStr, sizeof(queryStr), "DELETE FROM `OrgRanks` WHERE `OrgID` = '%d'", OrgData[idx][oSQL]);
	mysql_query(queryStr);

	queryStr[0] = EOS;
	format(queryStr, sizeof(queryStr), "DELETE FROM `Orgs` WHERE `ID` = '%d' LIMIT 1", OrgData[idx][oSQL]);
	mysql_query(queryStr);

	for(new i = 0; i < MAX_PLAYERS; i++) {
		if(Org_Idx(i) == idx) {
			OrgPlayer[i][poOrg] = -1;
			OrgPlayer[i][poRank] = 0;
		}
		if(OrgPlayer[i][poInviteOrg] == idx) OrgPlayer[i][poInviteOrg] = -1;
	}

	OrgData[idx][oUsed] = 0;
}

// Wywolaj w OnGameModeInit() po ConnectingMySQL().
stock Org_Load() {
	new row[256], loaded, rankRows[MAX_ORGS];

	for(new i = 0; i < MAX_ORGS; i++) OrgData[i][oUsed] = 0;

	for(new i = 0; i < MAX_PLAYERS; i++) {
		OrgPlayer[i][poOrg] = -1;
		OrgPlayer[i][poChar] = 0;
		OrgPlayer[i][poRank] = 0;
		OrgPlayer[i][poInviteOrg] = -1;
		OrgPlayer[i][poInviteTime] = 0;
		OrgPlayer[i][poInsideHQ] = 0;
		OrgPlayer[i][poLoginSpawn] = 0;
		OrgPlayer[i][poGuestOrg] = -1;
		OrgPlayer[i][poGuestTime] = 0;
	}

	mysql_query("SELECT `ID`, `Name`, `Color`, `Money`, `Motd`, `HQSet`, `HQX`, `HQY`, `HQZ`, `HQA` FROM `Orgs`");
	mysql_store_result();

	if(mysql_result_stored()) {
		while(mysql_fetch_row(row, "|")) {
			if(loaded >= MAX_ORGS) break;

			new id, name[32], color, money, motd[96], hqset, Float:hx, Float:hy, Float:hz, Float:ha;

			sscanf(row, "p<|>is[32]iis[96]iffff", id, name, color, money, motd, hqset, hx, hy, hz, ha);

			OrgData[loaded][oUsed] = 1;
			OrgData[loaded][oSQL] = id;
			OrgData[loaded][oColor] = color;
			OrgData[loaded][oMoney] = money;
			format(OrgData[loaded][oName], 32, "%s", name);
			format(OrgData[loaded][oMotd], 96, "%s", motd);
			OrgData[loaded][oHQSet] = hqset;
			OrgData[loaded][oHQX] = hx;
			OrgData[loaded][oHQY] = hy;
			OrgData[loaded][oHQZ] = hz;
			OrgData[loaded][oHQA] = ha;
			OrgData[loaded][oHQPickup] = 0;
			OrgData[loaded][oHQLabel] = 0;
			OrgData[loaded][oHQInPickup] = 0;
			OrgData[loaded][oHQInLabel] = 0;

			loaded++;
		}
	}

	mysql_free_result();

	// rangi: najpierw wartosci domyslne, potem nadpisane tym, co jest w bazie
	for(new i = 0; i < loaded; i++) Org_RanksSetDefaults(i);

	mysql_query("SELECT `OrgID`, `Rank`, `Name`, `Perms` FROM `OrgRanks`");
	mysql_store_result();

	if(mysql_result_stored()) {
		while(mysql_fetch_row(row, "|")) {
			new orgSql, rank, rname[24], perms, oi;

			sscanf(row, "p<|>iis[24]i", orgSql, rank, rname, perms);

			oi = Org_GetBySQL(orgSql);

			if(oi == -1 || rank < 1 || rank > ORG_MAX_RANK) continue;

			format(OrgRankName[oi][rank], ORG_RANK_NAME_LEN, "%s", rname);
			OrgRankPerm[oi][rank] = perms;
			rankRows[oi]++;
		}
	}

	mysql_free_result();

	// organizacje bez kompletu rang w bazie (np. stworzone przed ta funkcja) dostaja domyslne
	for(new i = 0; i < loaded; i++) {
		if(rankRows[i] < ORG_MAX_RANK) Org_RanksInsertMissing(i);
	}

	// obiekty siedzib tworzymy dopiero po zwolnieniu wyniku zapytania
	for(new i = 0; i < loaded; i++) {
		if(OrgData[i][oHQSet]) Org_HQCreateVisuals(i);
	}

	printf("[ORG] Wczytano %d organizacji.", loaded);
}

// Wywolaj w OnPlayerLogin() tuz po gPlayerLogged[playerid] = 1;
stock Org_OnLogin(playerid) {
	new row[32], sqlid, rank, idx, str[160];

	OrgPlayer[playerid][poOrg] = -1;
	OrgPlayer[playerid][poChar] = PlayerInfo[playerid][pID];
	OrgPlayer[playerid][poRank] = 0;
	OrgPlayer[playerid][poInviteOrg] = -1;
	OrgPlayer[playerid][poInviteTime] = 0;
	OrgPlayer[playerid][poInsideHQ] = 0;
	OrgPlayer[playerid][poLoginSpawn] = 2;		// pierwszy spawn po logowaniu = ostatnia pozycja (2 pokrywa podwojne SpawnPlayer)
	OrgPlayer[playerid][poGuestOrg] = -1;
	OrgPlayer[playerid][poGuestTime] = 0;

	queryStr[0] = EOS;
	format(queryStr, sizeof(queryStr), "SELECT `OrgID`, `Rank`, `InsideHQ` FROM `OrgMembers` WHERE `CharID` = '%d' LIMIT 1", PlayerInfo[playerid][pID]);
	mysql_query(queryStr);
	mysql_store_result();

	if(mysql_result_stored()) {
		if(mysql_fetch_row(row, "|")) {
			new insideHQ;

			sscanf(row, "p<|>iii", sqlid, rank, insideHQ);
			idx = Org_GetBySQL(sqlid);

			if(idx != -1) {
				OrgPlayer[playerid][poOrg] = idx;
				OrgPlayer[playerid][poRank] = rank;
				OrgPlayer[playerid][poInsideHQ] = insideHQ;

				format(str, sizeof(str), "Organizacje: Jestes w organizacji %s (ranga: %s). MOTD: %s", OrgData[idx][oName], OrgRankName[idx][rank], OrgData[idx][oMotd]);
				SendClientMessage(playerid, Org_Color(idx), str);
			}
		}
	}

	mysql_free_result();
}

// ---------------------------------------------------------------------------
// Podkomendy /org
// ---------------------------------------------------------------------------
stock Org_Usage(playerid) {
	SendClientMessage(playerid, COLOR_GRAD1, "Uzycie: /org [info | czlonkowie | zapros | akceptuj | wyrzuc | awans | degraduj | lider | wyjdz | motd | wplac | wyplac | hq | rangi | ranga]");
	SendClientMessage(playerid, COLOR_GRAD1, "Czat organizacji: /oc [tekst] | Siedziba: /hq [zapukaj | wpusc ID]");
	return 1;
}

stock Org_CmdInfo(playerid) {
	new idx = Org_Require(playerid), str[160];

	if(idx == -1) return 1;

	SendClientMessage(playerid, Org_Color(idx), "------ Organizacja ------");
	format(str, sizeof(str), "Nazwa: %s | Twoja ranga: %s (%d/%d)", OrgData[idx][oName], OrgRankName[idx][OrgPlayer[playerid][poRank]], OrgPlayer[playerid][poRank], ORG_MAX_RANK);
	SendClientMessage(playerid, COLOR_WHITE, str);
	format(str, sizeof(str), "Czlonkowie: %d/%d | Skarbiec: $%d", Org_CountMembers(idx), ORG_MAX_MEMBERS, OrgData[idx][oMoney]);
	SendClientMessage(playerid, COLOR_WHITE, str);
	format(str, sizeof(str), "MOTD: %s", OrgData[idx][oMotd]);
	SendClientMessage(playerid, COLOR_YELLOW2, str);
	if(OrgData[idx][oHQSet]) {
		format(str, sizeof(str), "Siedziba: ustawiona (wejscie: /hq)");
	} else {
		format(str, sizeof(str), "Siedziba: brak (szef ustawia ja komenda /org hq ustaw)");
	}
	SendClientMessage(playerid, COLOR_WHITE, str);
	return 1;
}

stock Org_CmdMembers(playerid) {
	new idx = Org_Require(playerid), list[2048], line[96], row[64], name[24], rank, status[24];

	if(idx == -1) return 1;

	strcat(list, "Nick\tRanga\tStatus\n");

	queryStr[0] = EOS;
	format(queryStr, sizeof(queryStr), "SELECT c.`Name`, m.`Rank` FROM `OrgMembers` m JOIN `Characters` c ON c.`ID` = m.`CharID` WHERE m.`OrgID` = '%d' ORDER BY m.`Rank` DESC, c.`Name` ASC", OrgData[idx][oSQL]);
	mysql_query(queryStr);
	mysql_store_result();

	if(mysql_result_stored()) {
		while(mysql_fetch_row(row, "|")) {
			sscanf(row, "p<|>s[24]i", name, rank);

			format(status, sizeof(status), "{AFAFAF}offline");

			for(new i = 0; i < MAX_PLAYERS; i++) {
				if(IsPlayerConnected(i) && gPlayerLogged[i] && !strcmp(PlayerInfo[i][pName], name, false)) {
					format(status, sizeof(status), "{33AA33}online");
					break;
				}
			}

			format(line, sizeof(line), "%s\t%s\t%s\n", name, OrgRankName[idx][rank], status);
			strcat(list, line);
		}
	}

	mysql_free_result();

	format(line, sizeof(line), "%s - czlonkowie", OrgData[idx][oName]);
	osrp_ShowPlayerDialog(playerid, DIALOG_ORG_MEMBERS, DIALOG_STYLE_TABLIST_HEADERS, line, list, "Zamknij", "");
	return 1;
}

stock Org_CmdInvite(playerid, const rest[]) {
	new idx = Org_RequirePerm(playerid, ORG_PERM_INVITE, "zapraszanie"), targetid, str[144];

	if(idx == -1) return 1;

	if(sscanf(rest, "u", targetid)) {
		SendClientMessage(playerid, COLOR_GRAD1, "Uzycie: /org zapros [ID/czesc nicku]");
		return 1;
	}

	if(targetid == INVALID_PLAYER_ID || !gPlayerLogged[targetid]) {
		SendClientMessage(playerid, COLOR_GREY, "Organizacje: Nie ma takiego gracza.");
		return 1;
	}

	if(targetid == playerid) {
		SendClientMessage(playerid, COLOR_GREY, "Organizacje: Nie mozesz zaprosic samego siebie.");
		return 1;
	}

	if(Org_Idx(targetid) != -1) {
		SendClientMessage(playerid, COLOR_GREY, "Organizacje: Ten gracz nalezy juz do jakiejs organizacji.");
		return 1;
	}

	if(OrgPlayer[targetid][poInviteOrg] != -1 && OrgPlayer[targetid][poInviteTime] > gettime()) {
		SendClientMessage(playerid, COLOR_GREY, "Organizacje: Ten gracz ma juz oczekujace zaproszenie.");
		return 1;
	}

	if(Org_CountMembers(idx) >= ORG_MAX_MEMBERS) {
		SendClientMessage(playerid, COLOR_GREY, "Organizacje: Twoja organizacja osiagnela limit czlonkow.");
		return 1;
	}

	OrgPlayer[targetid][poInviteOrg] = idx;
	OrgPlayer[targetid][poInviteTime] = gettime() + ORG_INVITE_TIME;

	format(str, sizeof(str), "Organizacje: Zaprosiles %s do organizacji.", PlayerInfo[targetid][pName]);
	SendClientMessage(playerid, COLOR_GREEN, str);

	format(str, sizeof(str), "Organizacje: %s zaprasza Cie do organizacji %s. Wpisz /org akceptuj (masz %d s).", PlayerInfo[playerid][pName], OrgData[idx][oName], ORG_INVITE_TIME);
	SendClientMessage(targetid, Org_Color(idx), str);
	return 1;
}

stock Org_CmdAccept(playerid) {
	new idx = OrgPlayer[playerid][poInviteOrg], str[144];

	if(idx == -1 || OrgPlayer[playerid][poInviteTime] < gettime() || !OrgData[idx][oUsed]) {
		OrgPlayer[playerid][poInviteOrg] = -1;
		SendClientMessage(playerid, COLOR_GREY, "Organizacje: Nie masz zadnego waznego zaproszenia.");
		return 1;
	}

	if(Org_Idx(playerid) != -1) {
		OrgPlayer[playerid][poInviteOrg] = -1;
		SendClientMessage(playerid, COLOR_GREY, "Organizacje: Nalezysz juz do organizacji.");
		return 1;
	}

	if(Org_CountMembers(idx) >= ORG_MAX_MEMBERS) {
		OrgPlayer[playerid][poInviteOrg] = -1;
		SendClientMessage(playerid, COLOR_GREY, "Organizacje: Ta organizacja nie ma juz wolnych miejsc.");
		return 1;
	}

	OrgPlayer[playerid][poInviteOrg] = -1;
	Org_AddMember(idx, PlayerInfo[playerid][pID], 1);

	format(str, sizeof(str), "Organizacje: %s dolaczyl do organizacji.", PlayerInfo[playerid][pName]);
	Org_Send(idx, Org_Color(idx), str);
	return 1;
}

stock Org_CmdKick(playerid, const rest[]) {
	new idx = Org_RequirePerm(playerid, ORG_PERM_KICK, "wyrzucanie"), nick[24], charid, rank, str[144];

	if(idx == -1) return 1;

	if(sscanf(rest, "s[24]", nick)) {
		SendClientMessage(playerid, COLOR_GRAD1, "Uzycie: /org wyrzuc [Nick_Postaci]");
		return 1;
	}

	if(!Org_LookupMember(idx, nick, charid, rank)) {
		SendClientMessage(playerid, COLOR_GREY, "Organizacje: Nie ma takiego czlonka w Twojej organizacji.");
		return 1;
	}

	if(charid == PlayerInfo[playerid][pID]) {
		SendClientMessage(playerid, COLOR_GREY, "Organizacje: Aby odejsc, uzyj /org wyjdz.");
		return 1;
	}

	if(rank >= OrgPlayer[playerid][poRank]) {
		SendClientMessage(playerid, COLOR_GREY, "Organizacje: Mozesz wyrzucac tylko osoby o nizszej randze od Twojej.");
		return 1;
	}

	Org_RemoveMember(charid);

	format(str, sizeof(str), "Organizacje: %s zostal wyrzucony z organizacji przez %s.", nick, PlayerInfo[playerid][pName]);
	Org_Send(idx, Org_Color(idx), str);

	new target = Org_FindOnlineByChar(charid);
	if(target != INVALID_PLAYER_ID) {
		SendClientMessage(target, COLOR_LIGHTRED, "Organizacje: Zostales wyrzucony z organizacji.");
	}
	return 1;
}

// direction: 1 = awans, -1 = degradacja
stock Org_CmdRank(playerid, const rest[], direction) {
	new idx = Org_RequirePerm(playerid, ORG_PERM_PROMOTE, "awansowanie i degradowanie"), nick[24], charid, rank, newRank, str[144], verb[16];

	if(idx == -1) return 1;

	if(sscanf(rest, "s[24]", nick)) {
		if(direction > 0) SendClientMessage(playerid, COLOR_GRAD1, "Uzycie: /org awans [Nick_Postaci]");
		else SendClientMessage(playerid, COLOR_GRAD1, "Uzycie: /org degraduj [Nick_Postaci]");
		return 1;
	}

	if(!Org_LookupMember(idx, nick, charid, rank)) {
		SendClientMessage(playerid, COLOR_GREY, "Organizacje: Nie ma takiego czlonka w Twojej organizacji.");
		return 1;
	}

	if(charid == PlayerInfo[playerid][pID]) {
		SendClientMessage(playerid, COLOR_GREY, "Organizacje: Nie mozesz zmienic wlasnej rangi.");
		return 1;
	}

	if(rank >= OrgPlayer[playerid][poRank]) {
		SendClientMessage(playerid, COLOR_GREY, "Organizacje: Mozesz zmieniac range tylko osobom nizszym od siebie.");
		return 1;
	}

	newRank = rank + direction;

	if(newRank < 1) {
		SendClientMessage(playerid, COLOR_GREY, "Organizacje: Ta osoba ma juz najnizsza range.");
		return 1;
	}

	if(newRank >= OrgPlayer[playerid][poRank]) {
		SendClientMessage(playerid, COLOR_GREY, "Organizacje: Nie mozesz nadac rangi rownej lub wyzszej od swojej. (Szefa zmienisz przez /org lider)");
		return 1;
	}

	Org_SetRank(charid, newRank);

	if(direction > 0) format(verb, sizeof(verb), "awansowal");
	else format(verb, sizeof(verb), "zdegradowal");

	format(str, sizeof(str), "Organizacje: %s %s gracza %s: %s -> %s.", PlayerInfo[playerid][pName], verb, nick, OrgRankName[idx][rank], OrgRankName[idx][newRank]);
	Org_Send(idx, Org_Color(idx), str);
	return 1;
}

stock Org_CmdLeader(playerid, const rest[]) {
	new idx = Org_Require(playerid, ORG_MAX_RANK), nick[24], charid, rank, str[144];

	if(idx == -1) return 1;

	if(sscanf(rest, "s[24]", nick)) {
		SendClientMessage(playerid, COLOR_GRAD1, "Uzycie: /org lider [Nick_Postaci] (przekazuje szefowanie, Ty zostajesz Zastepca)");
		return 1;
	}

	if(!Org_LookupMember(idx, nick, charid, rank)) {
		SendClientMessage(playerid, COLOR_GREY, "Organizacje: Nie ma takiego czlonka w Twojej organizacji.");
		return 1;
	}

	if(charid == PlayerInfo[playerid][pID]) {
		SendClientMessage(playerid, COLOR_GREY, "Organizacje: Juz jestes szefem.");
		return 1;
	}

	Org_SetRank(charid, ORG_MAX_RANK);
	Org_SetRank(PlayerInfo[playerid][pID], ORG_MAX_RANK - 1);

	format(str, sizeof(str), "Organizacje: %s przekazal szefowanie dla %s.", PlayerInfo[playerid][pName], nick);
	Org_Send(idx, Org_Color(idx), str);
	return 1;
}

stock Org_CmdLeave(playerid) {
	new idx = Org_Require(playerid), str[144];

	if(idx == -1) return 1;

	if(OrgPlayer[playerid][poRank] >= ORG_MAX_RANK) {
		SendClientMessage(playerid, COLOR_GREY, "Organizacje: Szef nie moze odejsc. Przekaz szefowanie (/org lider) lub popros administratora.");
		return 1;
	}

	Org_RemoveMember(PlayerInfo[playerid][pID]);

	format(str, sizeof(str), "Organizacje: %s opuscil organizacje.", PlayerInfo[playerid][pName]);
	Org_Send(idx, Org_Color(idx), str);
	SendClientMessage(playerid, COLOR_YELLOW, "Organizacje: Opusciles organizacje.");
	return 1;
}

stock Org_CmdMotd(playerid, const rest[]) {
	new idx = Org_RequirePerm(playerid, ORG_PERM_MOTD, "zmiana MOTD"), escaped[200], str[160];

	if(idx == -1) return 1;

	if(isnull(rest) || strlen(rest) > 90) {
		SendClientMessage(playerid, COLOR_GRAD1, "Uzycie: /org motd [tekst, max 90 znakow]");
		return 1;
	}

	mysql_real_escape_string(rest, escaped);

	queryStr[0] = EOS;
	format(queryStr, sizeof(queryStr), "UPDATE `Orgs` SET `Motd` = '%s' WHERE `ID` = '%d' LIMIT 1", escaped, OrgData[idx][oSQL]);
	mysql_query(queryStr);

	format(OrgData[idx][oMotd], 96, "%s", rest);

	format(str, sizeof(str), "Organizacje: %s zmienil MOTD: %s", PlayerInfo[playerid][pName], rest);
	Org_Send(idx, Org_Color(idx), str);
	return 1;
}

stock Org_CmdDeposit(playerid, const rest[]) {
	new idx = Org_Require(playerid), amount, str[144];

	if(idx == -1) return 1;

	if(sscanf(rest, "i", amount) || amount < 1) {
		SendClientMessage(playerid, COLOR_GRAD1, "Uzycie: /org wplac [kwota]");
		return 1;
	}

	if(GetPlayerMoney(playerid) < amount) {
		SendClientMessage(playerid, COLOR_GREY, "Organizacje: Nie masz tyle gotowki.");
		return 1;
	}

	if(OrgData[idx][oMoney] > ORG_MAX_MONEY - amount) {
		SendClientMessage(playerid, COLOR_GREY, "Organizacje: Skarbiec nie pomiesci tej kwoty.");
		return 1;
	}

	GivePlayerMoney(playerid, -amount);
	OrgData[idx][oMoney] += amount;
	Org_SaveMoney(idx);

	format(str, sizeof(str), "Organizacje: %s wplacil $%d do skarbca (stan: $%d).", PlayerInfo[playerid][pName], amount, OrgData[idx][oMoney]);
	Org_Send(idx, Org_Color(idx), str);
	return 1;
}

stock Org_CmdWithdraw(playerid, const rest[]) {
	new idx = Org_RequirePerm(playerid, ORG_PERM_WITHDRAW, "wyplaty ze skarbca"), amount, str[144];

	if(idx == -1) return 1;

	if(sscanf(rest, "i", amount) || amount < 1) {
		SendClientMessage(playerid, COLOR_GRAD1, "Uzycie: /org wyplac [kwota]");
		return 1;
	}

	if(OrgData[idx][oMoney] < amount) {
		SendClientMessage(playerid, COLOR_GREY, "Organizacje: W skarbcu nie ma tyle pieniedzy.");
		return 1;
	}

	OrgData[idx][oMoney] -= amount;
	Org_SaveMoney(idx);
	GivePlayerMoney(playerid, amount);

	format(str, sizeof(str), "Organizacje: %s wyplacil $%d ze skarbca (stan: $%d).", PlayerInfo[playerid][pName], amount, OrgData[idx][oMoney]);
	Org_Send(idx, Org_Color(idx), str);
	return 1;
}

// ---------------------------------------------------------------------------
// Rangi - komendy
// ---------------------------------------------------------------------------
stock Org_CmdRanks(playerid) {
	new idx = Org_Require(playerid), perms[48], str[160];

	if(idx == -1) return 1;

	SendClientMessage(playerid, Org_Color(idx), "------ Rangi organizacji ------");

	for(new r = ORG_MAX_RANK; r >= 1; r--) {
		if(r == ORG_MAX_RANK) format(perms, sizeof(perms), "wszystkie (szef)");
		else Org_PermText(OrgRankPerm[idx][r], perms);

		format(str, sizeof(str), "%d. %s - uprawnienia: %s", r, OrgRankName[idx][r], perms);
		SendClientMessage(playerid, COLOR_WHITE, str);
	}
	return 1;
}

// /org ranga nazwa [nr] [nowa nazwa]
// /org ranga uprawnienie [nr] [zapros|wyrzuc|awans|motd|wyplac|wpusc] [tak|nie]
stock Org_CmdRankEdit(playerid, const rest[]) {
	new idx = Org_Require(playerid, ORG_MAX_RANK), sub[16], arg[64], nr, str[160];

	if(idx == -1) return 1;

	if(sscanf(rest, "s[16]S()[64]", sub, arg)) {
		SendClientMessage(playerid, COLOR_GRAD1, "Uzycie: /org ranga nazwa [nr] [nowa nazwa]");
		SendClientMessage(playerid, COLOR_GRAD1, "Uzycie: /org ranga uprawnienie [nr] [zapros|wyrzuc|awans|motd|wyplac|wpusc] [tak|nie]");
		return 1;
	}

	if(!strcmp(sub, "nazwa", true)) {
		new name[ORG_RANK_NAME_LEN], escaped[64];

		if(sscanf(arg, "is[24]", nr, name) || nr < 1 || nr > ORG_MAX_RANK) {
			SendClientMessage(playerid, COLOR_GRAD1, "Uzycie: /org ranga nazwa [nr 1-5] [nowa nazwa] (zobacz numery: /org rangi)");
			return 1;
		}

		if(!Org_ValidName(name) || strlen(name) >= ORG_RANK_NAME_LEN) {
			SendClientMessage(playerid, COLOR_GREY, "Organizacje: Nazwa rangi: 3-23 znaki, litery bez polskich znakow, cyfry, spacja, - lub _.");
			return 1;
		}

		mysql_real_escape_string(name, escaped);

		queryStr[0] = EOS;
		format(queryStr, sizeof(queryStr), "UPDATE `OrgRanks` SET `Name` = '%s' WHERE `OrgID` = '%d' AND `Rank` = '%d' LIMIT 1", escaped, OrgData[idx][oSQL], nr);
		mysql_query(queryStr);

		format(str, sizeof(str), "Organizacje: %s zmienil nazwe rangi %d: %s -> %s.", PlayerInfo[playerid][pName], nr, OrgRankName[idx][nr], name);
		format(OrgRankName[idx][nr], ORG_RANK_NAME_LEN, "%s", name);

		Org_Send(idx, Org_Color(idx), str);
		return 1;
	}

	if(!strcmp(sub, "uprawnienie", true)) {
		new permName[16], value[8], bit, newPerms;

		if(sscanf(arg, "is[16]s[8]", nr, permName, value)) {
			SendClientMessage(playerid, COLOR_GRAD1, "Uzycie: /org ranga uprawnienie [nr 1-4] [zapros|wyrzuc|awans|motd|wyplac|wpusc] [tak|nie]");
			return 1;
		}

		if(nr < 1 || nr >= ORG_MAX_RANK) {
			SendClientMessage(playerid, COLOR_GREY, "Organizacje: Uprawnienia mozna ustawiac rangom 1-4. Szef ma zawsze wszystkie.");
			return 1;
		}

		if(!strcmp(permName, "zapros", true)) bit = ORG_PERM_INVITE;
		else if(!strcmp(permName, "wyrzuc", true)) bit = ORG_PERM_KICK;
		else if(!strcmp(permName, "awans", true)) bit = ORG_PERM_PROMOTE;
		else if(!strcmp(permName, "motd", true)) bit = ORG_PERM_MOTD;
		else if(!strcmp(permName, "wyplac", true)) bit = ORG_PERM_WITHDRAW;
		else if(!strcmp(permName, "wpusc", true)) bit = ORG_PERM_GUEST;
		else {
			SendClientMessage(playerid, COLOR_GREY, "Organizacje: Uprawnienie: zapros, wyrzuc, awans, motd, wyplac lub wpusc.");
			return 1;
		}

		newPerms = OrgRankPerm[idx][nr];

		if(!strcmp(value, "tak", true)) newPerms |= bit;
		else if(!strcmp(value, "nie", true)) newPerms &= ~bit;
		else {
			SendClientMessage(playerid, COLOR_GREY, "Organizacje: Ostatni argument to 'tak' albo 'nie'.");
			return 1;
		}

		OrgRankPerm[idx][nr] = newPerms;

		queryStr[0] = EOS;
		format(queryStr, sizeof(queryStr), "UPDATE `OrgRanks` SET `Perms` = '%d' WHERE `OrgID` = '%d' AND `Rank` = '%d' LIMIT 1", newPerms, OrgData[idx][oSQL], nr);
		mysql_query(queryStr);

		format(str, sizeof(str), "Organizacje: %s ustawil dla rangi %s uprawnienie '%s': %s.", PlayerInfo[playerid][pName], OrgRankName[idx][nr], permName, value);
		Org_Send(idx, Org_Color(idx), str);
		return 1;
	}

	SendClientMessage(playerid, COLOR_GRAD1, "Uzycie: /org ranga [nazwa | uprawnienie] ...");
	return 1;
}

// ---------------------------------------------------------------------------
// Siedziba (HQ) - komendy
// ---------------------------------------------------------------------------
stock Org_CmdHQ(playerid, const rest[]) {
	new idx = Org_Require(playerid, ORG_RANK_HQ_SET), option[16], str[144];

	if(idx == -1) return 1;

	if(sscanf(rest, "s[16]", option)) {
		SendClientMessage(playerid, COLOR_GRAD1, "Uzycie: /org hq [ustaw | usun]");
		return 1;
	}

	if(!strcmp(option, "ustaw", true)) {
		new Float:x, Float:y, Float:z, Float:a;

		if(IsPlayerInAnyVehicle(playerid) || GetPlayerState(playerid) != PLAYER_STATE_ONFOOT) {
			SendClientMessage(playerid, COLOR_GREY, "Organizacje: Musisz stac na nogach, aby ustawic siedzibe.");
			return 1;
		}

		if(GetPlayerInterior(playerid) != 0 || GetPlayerVirtualWorld(playerid) != 0) {
			SendClientMessage(playerid, COLOR_GREY, "Organizacje: Wejscie do siedziby mozna ustawic tylko na zewnatrz.");
			return 1;
		}

		for(new i = 0; i < MAX_ORGS; i++) {
			if(i == idx || !OrgData[i][oUsed] || !OrgData[i][oHQSet]) continue;

			if(GetPlayerDistanceFromPoint(playerid, OrgData[i][oHQX], OrgData[i][oHQY], OrgData[i][oHQZ]) < ORG_HQ_MIN_DIST) {
				SendClientMessage(playerid, COLOR_GREY, "Organizacje: Zbyt blisko jest wejscie do innej siedziby.");
				return 1;
			}
		}

		#if ORG_HQ_COST > 0
		if(OrgData[idx][oMoney] < ORG_HQ_COST) {
			format(str, sizeof(str), "Organizacje: Ustawienie siedziby kosztuje $%d ze skarbca organizacji.", ORG_HQ_COST);
			SendClientMessage(playerid, COLOR_GREY, str);
			return 1;
		}
		#endif

		GetPlayerPos(playerid, x, y, z);
		GetPlayerFacingAngle(playerid, a);

		// po wyjsciu gracz ma stac tylem do drzwi
		if(a >= 180.0) a -= 180.0;
		else a += 180.0;

		Org_HQDestroy(idx);

		OrgData[idx][oHQSet] = 1;
		OrgData[idx][oHQX] = x;
		OrgData[idx][oHQY] = y;
		OrgData[idx][oHQZ] = z;
		OrgData[idx][oHQA] = a;

		queryStr[0] = EOS;
		format(queryStr, sizeof(queryStr), "UPDATE `Orgs` SET `HQSet` = '1', `HQX` = '%.4f', `HQY` = '%.4f', `HQZ` = '%.4f', `HQA` = '%.2f' WHERE `ID` = '%d' LIMIT 1", x, y, z, a, OrgData[idx][oSQL]);
		mysql_query(queryStr);

		Org_HQCreateVisuals(idx);

		#if ORG_HQ_COST > 0
		OrgData[idx][oMoney] -= ORG_HQ_COST;
		Org_SaveMoney(idx);
		#endif

		format(str, sizeof(str), "Organizacje: %s ustawil siedzibe organizacji. Wejscie: /hq, pukanie: /hq zapukaj.", PlayerInfo[playerid][pName]);
		Org_Send(idx, Org_Color(idx), str);
		return 1;
	}

	if(!strcmp(option, "usun", true)) {
		if(!OrgData[idx][oHQSet]) {
			SendClientMessage(playerid, COLOR_GREY, "Organizacje: Twoja organizacja nie ma siedziby.");
			return 1;
		}

		Org_HQDestroy(idx);
		OrgData[idx][oHQSet] = 0;

		queryStr[0] = EOS;
		format(queryStr, sizeof(queryStr), "UPDATE `Orgs` SET `HQSet` = '0' WHERE `ID` = '%d' LIMIT 1", OrgData[idx][oSQL]);
		mysql_query(queryStr);

		format(str, sizeof(str), "Organizacje: %s usunal siedzibe organizacji.", PlayerInfo[playerid][pName]);
		Org_Send(idx, Org_Color(idx), str);
		return 1;
	}

	SendClientMessage(playerid, COLOR_GRAD1, "Uzycie: /org hq [ustaw | usun]");
	return 1;
}

// Wpuszczenie goscia przez czlonka organizacji.
stock Org_HQLetIn(playerid, const rest[]) {
	new idx = Org_RequirePerm(playerid, ORG_PERM_GUEST, "wpuszczanie gosci"), targetid, str[144];

	if(idx == -1) return 1;

	if(!OrgData[idx][oHQSet]) {
		SendClientMessage(playerid, COLOR_GREY, "Organizacje: Twoja organizacja nie ma siedziby.");
		return 1;
	}

	if(sscanf(rest, "u", targetid)) {
		SendClientMessage(playerid, COLOR_GRAD1, "Uzycie: /hq wpusc [ID/czesc nicku]");
		return 1;
	}

	if(targetid == INVALID_PLAYER_ID || !gPlayerLogged[targetid] || targetid == playerid) {
		SendClientMessage(playerid, COLOR_GREY, "Organizacje: Nie ma takiego gracza.");
		return 1;
	}

	if(Org_HQInsideIdx(playerid) != idx && Org_FindEntranceNear(playerid, 8.0) != idx) {
		SendClientMessage(playerid, COLOR_GREY, "Organizacje: Musisz byc w siedzibie albo przy jej wejsciu.");
		return 1;
	}

	if(Org_FindEntranceNear(targetid, 6.0) != idx) {
		SendClientMessage(playerid, COLOR_GREY, "Organizacje: Ta osoba nie stoi przy wejsciu do siedziby.");
		return 1;
	}

	OrgPlayer[targetid][poGuestOrg] = idx;
	OrgPlayer[targetid][poGuestTime] = gettime() + ORG_HQ_GUEST_TIME;

	format(str, sizeof(str), "Organizacje: Zgodzono sie wpuscic %s do siedziby.", PlayerInfo[targetid][pName]);
	SendClientMessage(playerid, COLOR_GREEN, str);

	format(str, sizeof(str), "Organizacje: Ktos z %s pozwolil Ci wejsc. Wpisz /hq przy drzwiach (masz %d s).", OrgData[idx][oName], ORG_HQ_GUEST_TIME);
	SendClientMessage(targetid, Org_Color(idx), str);
	return 1;
}

// Wywolaj w SetPlayerSpawn() (funcs.inc), po blokach wiezienia, a przed "if(house !=255)":
//     if(Org_OnSpawn(playerid)) return 1;
// Pierwszy spawn po zalogowaniu = dokladnie tam, gdzie gracz wyszedl z serwera
// (takze wewnatrz siedziby swojej organizacji). Respawn po smierci dziala jak dotychczas.
stock Org_OnSpawn(playerid) {
	new idx = Org_Idx(playerid);

	// po smierci w siedzibie swiat wirtualny zostaje - zerujemy go przed dalszym spawnem
	if(GetPlayerVirtualWorld(playerid) >= ORG_HQ_VW_BASE) SetPlayerVirtualWorld(playerid, 0);

	if(OrgPlayer[playerid][poLoginSpawn] <= 0) return 0;

	OrgPlayer[playerid][poLoginSpawn]--;

	SetPlayerToTeamColor(playerid);

	// wyszedl w siedzibie swojej organizacji - wraca do srodka
	if(idx != -1 && OrgData[idx][oHQSet] && OrgPlayer[playerid][poInsideHQ]) {
		Org_HQEnter(playerid, idx);
		return 1;
	}

	SetCameraBehindPlayer(playerid);
	SetPlayerPos(playerid, PlayerInfo[playerid][pPos_x], PlayerInfo[playerid][pPos_y], PlayerInfo[playerid][pPos_z]);
	SetPlayerFacingAngle(playerid, 0.0);
	SetPlayerVirtualWorld(playerid, 0);
	SetPlayerInterior(playerid, PlayerInfo[playerid][pInt]);
	return 1;
}

// Wywolaj w SavePlayerData() (funcs.inc) zaraz po GetPlayerPos(...):
//     Org_FixSavedPos(playerid);
// Gracz, ktory jest w siedzibie, zapisuje sie przy jej wejsciu (na zewnatrz, swiat 0),
// a w bazie zapamietujemy, ze nalezy go po zalogowaniu wpuscic z powrotem do srodka.
stock Org_FixSavedPos(playerid) {
	new vw = GetPlayerVirtualWorld(playerid), idx = -1, member = Org_Idx(playerid), inside, q[112];

	if(vw > ORG_HQ_VW_BASE) idx = Org_GetBySQL(vw - ORG_HQ_VW_BASE);
	if(idx != -1 && !OrgData[idx][oHQSet]) idx = -1;

	if(idx != -1) {
		PlayerInfo[playerid][pPos_x] = OrgData[idx][oHQX];
		PlayerInfo[playerid][pPos_y] = OrgData[idx][oHQY];
		PlayerInfo[playerid][pPos_z] = OrgData[idx][oHQZ];
		PlayerInfo[playerid][pInt] = 0;
	}

	if(member != -1) {
		inside = (idx == member) ? 1 : 0;

		if(inside != OrgPlayer[playerid][poInsideHQ]) {
			OrgPlayer[playerid][poInsideHQ] = inside;

			format(q, sizeof(q), "UPDATE `OrgMembers` SET `InsideHQ` = '%d' WHERE `CharID` = '%d' LIMIT 1", inside, PlayerInfo[playerid][pID]);
			mysql_query(q);
		}
	}

	return (idx != -1);
}

// ---------------------------------------------------------------------------
// Komendy gracza
// ---------------------------------------------------------------------------
CMD:org(playerid, params[]) {
	new sub[16], rest[96];

	if(!gPlayerLogged[playerid]) return 1;

	if(sscanf(params, "s[16]S()[96]", sub, rest)) return Org_Usage(playerid);

	if(!strcmp(sub, "info", true)) return Org_CmdInfo(playerid);
	if(!strcmp(sub, "czlonkowie", true)) return Org_CmdMembers(playerid);
	if(!strcmp(sub, "zapros", true)) return Org_CmdInvite(playerid, rest);
	if(!strcmp(sub, "akceptuj", true)) return Org_CmdAccept(playerid);
	if(!strcmp(sub, "wyrzuc", true)) return Org_CmdKick(playerid, rest);
	if(!strcmp(sub, "awans", true)) return Org_CmdRank(playerid, rest, 1);
	if(!strcmp(sub, "degraduj", true)) return Org_CmdRank(playerid, rest, -1);
	if(!strcmp(sub, "lider", true)) return Org_CmdLeader(playerid, rest);
	if(!strcmp(sub, "wyjdz", true)) return Org_CmdLeave(playerid);
	if(!strcmp(sub, "motd", true)) return Org_CmdMotd(playerid, rest);
	if(!strcmp(sub, "wplac", true)) return Org_CmdDeposit(playerid, rest);
	if(!strcmp(sub, "wyplac", true)) return Org_CmdWithdraw(playerid, rest);
	if(!strcmp(sub, "hq", true)) return Org_CmdHQ(playerid, rest);
	if(!strcmp(sub, "rangi", true)) return Org_CmdRanks(playerid);
	if(!strcmp(sub, "ranga", true)) return Org_CmdRankEdit(playerid, rest);

	return Org_Usage(playerid);
}

CMD:oc(playerid, params[]) {
	new idx, str[192];

	if(!gPlayerLogged[playerid]) return 1;

	idx = Org_Require(playerid);
	if(idx == -1) return 1;

	if(isnull(params)) {
		SendClientMessage(playerid, COLOR_GRAD1, "Uzycie: /oc [tekst]");
		return 1;
	}

	format(str, sizeof(str), "[%s] %s %s: %s", OrgData[idx][oName], OrgRankName[idx][OrgPlayer[playerid][poRank]], PlayerInfo[playerid][pName], params);
	Org_Send(idx, Org_Color(idx), str);
	return 1;
}

CMD:hq(playerid, params[]) {
	new sub[16], rest[32], idx, str[144];

	if(!gPlayerLogged[playerid]) return 1;

	if(IsPlayerInAnyVehicle(playerid)) {
		SendClientMessage(playerid, COLOR_GREY, "Organizacje: Nie mozesz tego zrobic w pojezdzie.");
		return 1;
	}

	sscanf(params, "S()[16]S()[32]", sub, rest);

	if(!strcmp(sub, "wpusc", true)) return Org_HQLetIn(playerid, rest);

	// --- wyjscie z siedziby ---
	idx = Org_HQInsideIdx(playerid);

	if(idx != -1) {
		if(!IsPlayerInRangeOfPoint(playerid, 3.0, ORG_HQ_INX, ORG_HQ_INY, ORG_HQ_INZ)) {
			SendClientMessage(playerid, COLOR_GREY, "Organizacje: Podejdz do wyjscia z siedziby.");
			return 1;
		}

		Org_HQExit(playerid, idx);
		return 1;
	}

	// --- wejscie do siedziby ---
	idx = Org_FindEntranceNear(playerid, 3.0);

	if(idx == -1) {
		SendClientMessage(playerid, COLOR_GREY, "Organizacje: Nie stoisz przy wejsciu do zadnej siedziby.");
		return 1;
	}

	if(!strcmp(sub, "zapukaj", true)) {
		if(gettime() - OrgKnockTime[playerid] < ORG_HQ_KNOCK_COOLDOWN) {
			SendClientMessage(playerid, COLOR_GREY, "Organizacje: Dopiero co pukales, poczekaj chwile.");
			return 1;
		}

		OrgKnockTime[playerid] = gettime();

		format(str, sizeof(str), "Organizacje: Ktos puka do drzwi siedziby %s.", OrgData[idx][oName]);
		Org_Send(idx, Org_Color(idx), str);

		SendClientMessage(playerid, COLOR_YELLOW, "* Pukasz do drzwi siedziby.");
		return 1;
	}

	if(Org_Idx(playerid) == idx || (OrgPlayer[playerid][poGuestOrg] == idx && OrgPlayer[playerid][poGuestTime] > gettime())) {
		OrgPlayer[playerid][poGuestTime] = 0;
		Org_HQEnter(playerid, idx);
		return 1;
	}

	if(Org_Idx(playerid) == -1) {
		SendClientMessage(playerid, COLOR_GREY, "Organizacje: Drzwi sa zamkniete - system nie widzi Cie jako czlonka (sprawdz /org info, w razie potrzeby zaloguj sie ponownie). Mozesz zapukac: /hq zapukaj.");
	} else {
		SendClientMessage(playerid, COLOR_GREY, "Organizacje: To siedziba innej organizacji. Mozesz zapukac: /hq zapukaj.");
	}
	return 1;
}

// TYMCZASOWA komenda diagnostyczna - usun, gdy wejscie do siedziby juz dziala.
CMD:hqdebug(playerid, params[]) {
	new str[160], idx, Float:dist = -1.0;

	if(!gPlayerLogged[playerid]) return 1;

	idx = Org_Idx(playerid);

	format(str, sizeof(str), "HQ-DEBUG 1/3: pID=%d poChar=%d poOrg=%d poRank=%d => Org_Idx=%d", PlayerInfo[playerid][pID], OrgPlayer[playerid][poChar], OrgPlayer[playerid][poOrg], OrgPlayer[playerid][poRank], idx);
	SendClientMessage(playerid, COLOR_YELLOW, str);

	if(idx != -1) {
		if(OrgData[idx][oHQSet]) {
			dist = GetPlayerDistanceFromPoint(playerid, OrgData[idx][oHQX], OrgData[idx][oHQY], OrgData[idx][oHQZ]);
		}

		format(str, sizeof(str), "HQ-DEBUG 2/3: org=%s HQSet=%d wejscie=(%.1f, %.1f, %.1f) odleglosc=%.1f m", OrgData[idx][oName], OrgData[idx][oHQSet], OrgData[idx][oHQX], OrgData[idx][oHQY], OrgData[idx][oHQZ], dist);
		SendClientMessage(playerid, COLOR_YELLOW, str);
	} else {
		SendClientMessage(playerid, COLOR_YELLOW, "HQ-DEBUG 2/3: brak organizacji u tego gracza");
	}

	format(str, sizeof(str), "HQ-DEBUG 3/3: swiat=%d interior=%d (do wejscia musza byc 0 i 0), wejscie w zasiegu 3 m: %d", GetPlayerVirtualWorld(playerid), GetPlayerInterior(playerid), Org_FindEntranceNear(playerid, 3.0));
	SendClientMessage(playerid, COLOR_YELLOW, str);
	return 1;
}

// ---------------------------------------------------------------------------
// Komendy administracyjne (jedyne miejsce, gdzie uzywane jest pAdmin)
// ---------------------------------------------------------------------------
CMD:orgstworz(playerid, params[]) {
	new hex, leaderid, name[32], escaped[80], idx, newId, str[144];

	if(!gPlayerLogged[playerid] || PlayerInfo[playerid][pAdmin] < ORG_ADMIN_LEVEL) return 1;

	if(sscanf(params, "hus[32]", hex, leaderid, name)) {
		SendClientMessage(playerid, COLOR_GRAD1, "Uzycie: /orgstworz [kolor HEX np. FF0000] [ID przyszlego szefa] [nazwa organizacji]");
		return 1;
	}

	if(leaderid == INVALID_PLAYER_ID || !gPlayerLogged[leaderid]) {
		SendClientMessage(playerid, COLOR_GREY, "Organizacje: Nie ma takiego gracza.");
		return 1;
	}

	if(Org_Idx(leaderid) != -1) {
		SendClientMessage(playerid, COLOR_GREY, "Organizacje: Ten gracz nalezy juz do organizacji.");
		return 1;
	}

	if(!Org_ValidName(name)) {
		SendClientMessage(playerid, COLOR_GREY, "Organizacje: Nazwa: 3-31 znakow, litery bez polskich znakow, cyfry, spacja, - lub _.");
		return 1;
	}

	for(new i = 0; i < MAX_ORGS; i++) {
		if(OrgData[i][oUsed] && !strcmp(OrgData[i][oName], name, true)) {
			SendClientMessage(playerid, COLOR_GREY, "Organizacje: Organizacja o takiej nazwie juz istnieje.");
			return 1;
		}
	}

	idx = Org_FindFreeSlot();

	if(idx == -1) {
		SendClientMessage(playerid, COLOR_GREY, "Organizacje: Osiagnieto maksymalna liczbe organizacji (MAX_ORGS).");
		return 1;
	}

	hex &= 0xFFFFFF;
	mysql_real_escape_string(name, escaped);

	queryStr[0] = EOS;
	format(queryStr, sizeof(queryStr), "INSERT INTO `Orgs` (`Name`, `Color`, `Money`, `Motd`, `Created`) VALUES ('%s', '%d', '0', 'Brak', '%d')", escaped, hex, gettime());
	mysql_query(queryStr);

	newId = mysql_insert_id();

	if(!newId) {
		SendClientMessage(playerid, COLOR_GREY, "Organizacje: Nie udalo sie zapisac organizacji w bazie.");
		return 1;
	}

	OrgData[idx][oUsed] = 1;
	OrgData[idx][oSQL] = newId;
	OrgData[idx][oColor] = hex;
	OrgData[idx][oMoney] = 0;
	format(OrgData[idx][oName], 32, "%s", name);
	format(OrgData[idx][oMotd], 96, "Brak");

	Org_RanksSetDefaults(idx);
	Org_RanksInsertMissing(idx);

	Org_AddMember(idx, PlayerInfo[leaderid][pID], ORG_MAX_RANK);

	format(str, sizeof(str), "Organizacje: Utworzono organizacje %s (ID: %d), szef: %s.", name, idx, PlayerInfo[leaderid][pName]);
	SendClientMessage(playerid, COLOR_GREEN, str);

	format(str, sizeof(str), "Organizacje: Zostales szefem organizacji %s. Komendy: /org, /oc.", name);
	SendClientMessage(leaderid, Org_Color(idx), str);
	return 1;
}

CMD:orgusun(playerid, params[]) {
	new idx, str[144];

	if(!gPlayerLogged[playerid] || PlayerInfo[playerid][pAdmin] < ORG_ADMIN_LEVEL) return 1;

	if(sscanf(params, "i", idx) || idx < 0 || idx >= MAX_ORGS || !OrgData[idx][oUsed]) {
		SendClientMessage(playerid, COLOR_GRAD1, "Uzycie: /orgusun [ID organizacji z /orglista]");
		return 1;
	}

	format(str, sizeof(str), "Organizacje: Twoja organizacja (%s) zostala rozwiazana przez administratora.", OrgData[idx][oName]);
	Org_Send(idx, COLOR_LIGHTRED, str);

	format(str, sizeof(str), "Organizacje: Usunieto organizacje %s.", OrgData[idx][oName]);
	SendClientMessage(playerid, COLOR_GREEN, str);

	Org_Remove(idx);
	return 1;
}

CMD:orglista(playerid, params[]) {
	new str[144], count;

	if(!gPlayerLogged[playerid] || PlayerInfo[playerid][pAdmin] < ORG_ADMIN_LEVEL) return 1;

	SendClientMessage(playerid, COLOR_YELLOW, "------ Organizacje ------");

	for(new i = 0; i < MAX_ORGS; i++) {
		if(!OrgData[i][oUsed]) continue;

		format(str, sizeof(str), "ID: %d | %s | skarbiec: $%d", i, OrgData[i][oName], OrgData[i][oMoney]);
		SendClientMessage(playerid, Org_Color(i), str);
		count++;
	}

	if(!count) SendClientMessage(playerid, COLOR_GREY, "Brak organizacji.");
	return 1;
}
