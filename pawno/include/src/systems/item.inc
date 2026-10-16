/*
	Project: GodFather
	Author: MisterMagik, steeZ
	File name: item.inc
	Date: 12.03.2026
	Modified: 12.03.2026
	Desc: Items system.
*/

stock InitItemData() {
	printf("[PRZEDMIOT] Zaczynam ladowac przedmioty...");

	new Id;
	new PosStr[32];
	new bool:bUsed = false;

	queryStr[0] = EOS;
	format(queryStr, sizeof(queryStr), "SELECT `ID`, `OwnerID`, `OwnerType`, `Name`, `Type`, `SubType`, `Amount`, `Value1`, `Value2`, `Pos`, `Used`, `Vw`, `Int` FROM `Items`");
	mysql_query(queryStr);	

	mysql_store_result();

	if(mysql_result_stored()) {
		while(mysql_fetch_row(queryStr, "|")) {
			sscanf(queryStr, "p<|>iiis[64]iiiiis[32]iii",
				ItemInfo[Id][iID],
				ItemInfo[Id][iOwnerID],
				ItemInfo[Id][iOwnerType],
				ItemInfo[Id][iName],
				ItemInfo[Id][iType],
				ItemInfo[Id][iSubType],
				ItemInfo[Id][iAmount],
				ItemInfo[Id][iValue1],
				ItemInfo[Id][iValue2],
				PosStr,
				bUsed,
				ItemInfo[Id][iVw],
				ItemInfo[Id][iInt]
			);

			ItemInfo[Id][iUsed] = !!bUsed; // Gdyby item by³ u¿ywany to domyœlnie wy³¹czy to

			sscanf(PosStr, "p<,>fff", ItemInfo[Id][iPos][0], ItemInfo[Id][iPos][1], ItemInfo[Id][iPos][2]);
			Iter_Add(ItemItter, Id);

			printf("[PRZEDMIOT] ID: %d, OwnerID: %d, OwnerType: %d, Name: %s, Type: %d, SubType: %d, Amount: %d, Value1: %d, Value2: %d, Pos: %s, Used: %d, Vw: %d, Int: %d\n",
				ItemInfo[Id][iID],
				ItemInfo[Id][iOwnerID],
				ItemInfo[Id][iOwnerType],
				ItemInfo[Id][iName],
				ItemInfo[Id][iType],
				ItemInfo[Id][iSubType],
				ItemInfo[Id][iAmount],
				ItemInfo[Id][iValue1],
				ItemInfo[Id][iValue2],
				PosStr,
				ItemInfo[Id][iUsed],
				ItemInfo[Id][iVw],
				ItemInfo[Id][iInt]
			);

			Id++;
		}
	}

	printf("[PRZEDMIOT] Wczytano %d przedmiotow.", Id);

	mysql_free_result();
	return 1;
}

stock GetItemFreeIndex() {
	for(new i = 0; i < MAX_ITEMS; i++) {
		if(!Iter_Contains(ItemItter, i)) {
			return i;
		}
	}
	return INVALID_ID;
}

stock IsItemInInventory(ItemId) {
	if(ItemInfo[ItemId][iPos][0] != 0.0 || ItemInfo[ItemId][iPos][1] != 0.0 || ItemInfo[ItemId][iPos][2] != 0.0) {
		return 0;
	}
	return 1;
}

stock IsItemOwnedByPlayer(playerid, ItemId) {
	if(!Iter_Contains(ItemItter, ItemId)) {
		return 0;
	}

	if(ItemInfo[ItemId][iOwnerType] != ITEM_OWNER_PLAYER) {
		return 0;
	}

	if(ItemInfo[ItemId][iOwnerID] != PlayerInfo[playerid][pID]) {
		return 0;
	}
	return 1;
}

stock FindStackableItem(OwnerId, OwnerType, const Name[], Type, SubType, Value1, Value2) {
	foreach(new i : ItemItter) {
		if(ItemInfo[i][iOwnerID] == OwnerId && ItemInfo[i][iOwnerType] == OwnerType && ItemInfo[i][iType] == Type && ItemInfo[i][iSubType] == SubType &&
		   ItemInfo[i][iValue1] == Value1 && ItemInfo[i][iValue2] == Value2 && ItemInfo[i][iAmount] > 0 && !ItemInfo[i][iUsed] && IsItemInInventory(i) &&
		   !strcmp(ItemInfo[ItemId][iName], Name, false)) {
			return i;
		}
	}
	return INVALID_ID;
}

stock AddItem(OwnerId, OwnerType, const Name[], Type, SubType, Amount, Value1, Value2, Float:PosX, Float:PosY, Float:PosZ, Vw, Int) {
	if(Amount > 0 && PosX == 0.0 && PosY == 0.0 && PosZ == 0.0) {
		new Stack = FindStackableItem(OwnerId, OwnerType, Name, Type, SubType, Value1, Value2);

		if(Stack != INVALID_ID) {
			ItemInfo[Stack][iAmount] += Amount;

			queryStr[0] = EOS;
			format(queryStr, sizeof(queryStr), "UPDATE `Items` SET `Amount` = '%d' WHERE `ID` = '%d'", ItemInfo[Stack][iAmount], ItemInfo[Stack][iID]);
			mysql_query(queryStr);
			return Stack;
		}
	}

	new Index = GetItemFreeIndex();

	if(Index == INVALID_ID) {
		return 1;
	}

	ItemInfo[Index][iID] = Index + 1;
	ItemInfo[Index][iOwnerID] = OwnerId;
	ItemInfo[Index][iOwnerType] = OwnerType;
	format(ItemInfo[Index][iName], 64, "%s", Name);
	ItemInfo[Index][iType] = Type;
	ItemInfo[Index][iSubType] = SubType;
	ItemInfo[Index][iAmount] = Amount;
	ItemInfo[Index][iValue1] = Value1;
	ItemInfo[Index][iValue2] = Value2;
	ItemInfo[Index][iPos][0] = PosX;
	ItemInfo[Index][iPos][1] = PosY;
	ItemInfo[Index][iPos][2] = PosZ;
	ItemInfo[Index][iUsed] = false;
	ItemInfo[Index][iVw] = Vw;
	ItemInfo[Index][iInt] = Int;

	Iter_Add(ItemItter, Index);

	new EscName[64];
	mysql_real_escape_string(Name, EscName);

	queryStr[0] = EOS;
	format(queryStr, sizeof(queryStr), "INSERT INTO `Items` (`OwnerID`, `OwnerType`, `Name`, `Type`, `SubType`, `Amount`, `Value1`, `Value2`, `Pos`, `Vw`, `Int`) VALUES ('%d', '%d', '%s', '%d', '%d', '%d', '%f,%f,%f', '%d', '%d')",
		OwnerId,
		OwnerType,
		EscName,
		Type,
		SubType,
		Amount,
		Value1,
		Value2,
		PosX,
		PosY,
		PosZ,
		Vw,
		Int
	);
	mysql_query(queryStr);

	printf("[PRZEDMIOT] Dodano przedmiot %s o ID: %d.", Name, Index);
	return 1;
}

stock SaveItem(ItemId) {
	if(!Iter_Contains(ItemItter, ItemId)) {
		return 1;
	}

	new EscName[64];

	mysql_real_escape_string(ItemInfo[ItemId][iName], EscName);

	queryStr[0] = EOS;
	format(queryStr, sizeof(queryStr), "UPDATE `Items` SET `OwnerID` = '%d', `OwnerType` = '%d', `Name` = '%s', `Type` = '%d', `SubType` = '%d', `Amount` = '%d', `Value1` = '%d', `Value2` = '%d', `Pos` = '%f,%f,%f', `Vw` = '%d', `Int` = '%d' WHERE `ID` = '%d'",
		ItemInfo[ItemId][iOwnerId],
		ItemInfo[ItemId][iOwnerType],
		EscName,
		ItemInfo[ItemId][iType],
		ItemInfo[ItemId][iSubType],
		ItemInfo[ItemId][iAmount],
		ItemInfo[ItemId][iValue1],
		ItemInfo[ItemId][iValue2],
		ItemInfo[ItemId][iPos][0],
		ItemInfo[ItemId][iPos][1],
		ItemInfo[ItemId][iPos][2],
		ItemInfo[ItemId][iVw],
		ItemInfo[ItemId][iInt],
		ItemInfo[ItemId][iID]
	);

	mysql_query(queryStr);
	return 1;
}

stock RemoveItem(ItemId, Ammount) {
	if(!Iter_Contains(ItemItter, ItemId)) {
		return 1;
	}

	if(Ammount <= 0) {
		return 1;
	}

	if(ItemInfo[ItemId][iAmount] <= 0) {
		return DestroyItem(ItemId);
	}

	ItemInfo[ItemId][iAmount] -= ammount;

	queryStr[0] = EOS;
	format(queryStr, sizeof(queryStr), "UPDATE `Items` SET `Amount` = '%d' WHERE `ID` = '%d'", ItemInfo[ItemId][iAmount], ItemInfo[ItemId][iID]);
	mysql_query(queryStr);
	return 1;
}

stock DestroyItem(ItemId) {
	if(ItemId == INVALID_ID) {
		return 1;
	}

	if(!Iter_Contains(ItemItter, ItemId)) {
		return 1;
	}

	queryStr[0] = EOS;
	format(queryStr, sizeof(queryStr), "DELETE FROM `Items` WHERE `ID` = '%d'", ItemInfo[ItemId][iID]);
	mysql_query(queryStr);

	ItemInfo[ItemId][iID] = 0;
	ItemInfo[ItemId][iOwnerID] = 0;
	ItemInfo[ItemId][iOwnerType] = ITEM_OWNER_NONE;
	ItemInfo[ItemId][iName] = EOS;
	ItemInfo[ItemId][iType] = ITEM_TYPE_NONE;
	ItemInfo[ItemId][iSubType] = ITEM_SUB_TYPE_NONE;
	ItemInfo[ItemId][iAmount] = 0;
	ItemInfo[ItemId][iValue1] = 0;
	ItemInfo[ItemId][iValue2] = 0;
	ItemInfo[ItemId][iPos][0] = 0.0;
	ItemInfo[ItemId][iPos][1] = 0.0;
	ItemInfo[ItemId][iPos][2] = 0.0;
	ItemInfo[ItemId][iUsed] = false;
	ItemInfo[ItemId][iVw] = 0;
	ItemInfo[ItemId][iInt] = 0;

	Iter_Remove(ItemItter, ItemId);

	printf("[PRZEDMIOT] Usunieto przedmiot o ID: %d", ItemId);
	return 1;
}

stock GetItemTypeFromID(ItemId) {
	return ItemInfo[ItemId][iType];
}

stock GetItemSubTypeFromID(ItemId) {
	return ItemInfo[ItemId][iSubType];
}

stock GetItemDescription(ItemId, Desc[], Size) {
	Desc[0] = EOS;

	switch(GetItemTypeFromID(ItemId)) {
		case ITEM_TYPE_GUN: {
			if(GetItemSubTypeFromID(ItemId) == ITEM_SUB_TYPE_GUN_WEAP) {
				format(Desc, Size, " (NS: %d Stan: %d%%)", ItemInfo[ItemId][iValue1], ItemInfo[ItemId][iValue2]);
				return 1;
			}
		}
		case ITEM_TYPE_ELECTRONIC: {
			if(GetItemSubTypeFromID(ItemId) == ITEM_SUB_TYPE_ELECTRONIC_PHONE) {
				format(Desc, Size, " (Numer: %d)", ItemInfo[ItemId][iValue1]);

				if(ItemInfo[ItemId][iUsed]) {
					strcat(Desc, " {33CC33}(W³¹czony)", Size);
				} else {
					strcat(Desc, " {33CC33}(Wy³¹czony)", Size);
				}
				return 1;
			}
		}
	}

	if(ItemInfo[ItemId][iAmount] > 0) {
		format(Desc, Size, " (Iloœæ: %d)", ItemInfo[ItemId][iAmount]);
	}

	if(ItemInfo[ItemId][iUsed] && (GetItemTypeFromID(ItemId) == ITEM_TYPE_ACCESSORY || ItemNameHas(ItemId, "kawiczki"))) {
		strcat(Desc, " {FFAA00}(Za³o¿one)", Size);
	}
	return 1;
}

stock AddItemListRow(viewerid, const Line[], ItemId) {
	new Row = pItemListRows[viewerid];

	if(Row >= MAX_ITEM_LIST_ROWS) {
		return 0;
	}

	if(strlen(ItemListStr) + strlen(Line) + 2 >= sizeof(ItemListStr)) {
		return 0;
	}

	if(Row) {
		strcat(ItemListStr, "\n", sizeof(ItemListStr));
	}

	strcat(ItemListStr, Line, sizeof(ItemListStr));

	pItemListRow[viewerid][Row] = ItemId;
	pItemListRows[viewerid]++;
	return 1;
}

stock TakeItem(itemid, ammount) {
	
}

stock PutItem(itemid, ammount) {
	
}

stock ShowPlayerItemToSomeOne(playerid, someoneid, dialogid) {
	if(playerid == INVALID_PLAYER_ID || someoneid == INVALID_PLAYER_ID) {
		return 1;
	}

	if(!IsPlayerConnected(playerid) || !IsPlayerConnected(someoneid)) {
		return 1;
	}

	new String[128];
	new Desc[96];
	new bool:bFull = false;

	ItemListStr[0] = EOS;
	pItemListRows[someoneid] = 0;
	pItemListOwner[someoneid] = playerid;

	for(new i = 1; i < MAX_ITEM_TYPES; i++) {
		if(bFull) {
			break;
		}

		new Count = 0;

		foreach(new j : ItemItter) {
			if(ItemInfo[j][iType] == i && IsItemOwnedByPlayer(playerid, j) && IsItemInInventory(j)) {
				Count++;
			}
		}

		if(i == ITEM_TYPE_OTHER) {
			Count++;
		}

		if(!Count) {
			continue;
		}

		format(String, sizeof(String), "%s••• %s ••• (%d)", ItemCategoryColors[i], ItemCategoryNames[i], Count);

		if(!AddItemListRow(someoneid, String, ITEM_LIST_ROW_HEADER)) {
			bFull = true;
			break;
		}

		foreach(new j : ItemItter) {
			if(ItemInfo[j][iType] != i || !IsItemOwnedByPlayer(playerid, j) || !IsItemInInventory(j)) {
				continue;
			}

			GetItemDescription(j, Desc, sizeof(Desc));
			format(String, sizeof(String), "{FFFFFF}%s%s", ItemInfo[j][iName], Desc);

			if(!AddItemListRow(someoneid, String, j)) {
				bFull = true;
				break;
			}
		}

		if(!bFull && i == ITEM_TYPE_OTHER) {
			format(String, sizeof(String), "{FFFFFF}Pieni¹dze (Iloœæ: {33CC33}$%d{FFFFFF})", PlayerInfo[playerid][pCash]);

			if(!AddItemListRow(someoneid, String, ITEM_LIST_ROW_MONEY)) {
				bFull = true;
			}
		}
	}

	new Title[64];

	if(playerid == someoneid) {
		strcat(Title, "Twoje przedmioty:");
	} else {
		format(Title, sizeof(Title), "Przedmioty: %s", PlayerInfo[playerid][pName]);
	}

	osrp_ShowPlayerDialog(someoneid, dialogid, DIALOG_STYLE_LIST, Title, ItemListStr, "Wybierz", "Cofnij");
	return 1;
}

stock ResetPlayerItemList(playerid) {
	pItemListRows[playerid] = 0;
	pItemListOwner[playerid] = INVALID_PLAYER_ID;
	pItemSelected[playerid] = -1;
	pItemAction[playerid] = ITEM_ACTION_NONE;
	pItemTarget[playerid] = -1;
	pItemPutOption[playerid] = 0;
	return 1;
}

stock ShowItemMenuForPlayer(playerid) {
	osrp_ShowPlayerDialog(playerid, DIALOG_ITEM_SHOW, DIALOG_STYLE_LIST, "Przedmioty:", "{FFFFFF}Zobacz swoje przedmioty", "Dalej", "Anuluj");
	return 1;
}

stock ShowItemActionMenu(playerid) {
	osrp_ShowPlayerDialog(playerid, DIALOG_ITEM_ACTION, DIALOG_STYLE_LIST, "Co chcesz zrobiæ z tym przedmiotem?", "{FFFFFF}U¿yj\nOddaj\nOd³ó¿\nZniszcz", "Wybierz", "Cofnij");
	return 1;
}

stock UseItemForPlayer(playerid, itemid, typeid, subtypeid) {
	switch(typeid) {
		case ITEM_TYPE_GUN: {
			switch(subtypeid) {
				case ITEM_SUB_TYPE_GUN_AMMO: {
					
				}
				case ITEM_SUB_TYPE_GUN_WEAP: {
					
				}
			}
		}
		case ITEM_TYPE_FOOD: {
			switch(subtypeid) {
				case ITEM_SUB_TYPE_FOOD_MEAL: {
					
				}
				case ITEM_SUB_TYPE_FOOD_DRINK: {
					
				}
			}
		}
	}
	return 1;
}

stock ItemNameHas(ItemId, const Name[]) {
	if(strfind(ItemInfo[ItemId][iName], Name, true) != -1) {
		return 1;
	}
	return 0;
}
