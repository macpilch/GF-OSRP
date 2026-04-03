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

	new id;
	new posStr[32];

	queryStr[0] = EOS;
	format(queryStr, sizeof(queryStr), "SELECT `ID`, `OwnerID`, `OwnerType`, `Name`, `Type`, `SubType`, `Amount`, `Pos`, `Used`, `Vw`, `Int` FROM `Items`");
	mysql_query(queryStr);	

	mysql_store_result();

	if(mysql_result_stored()) {
		while(mysql_fetch_row(queryStr, "|")) {
			sscanf(queryStr, "p<|>iiis[64]iiis[32]iii",
				ItemInfo[id][iID],
				ItemInfo[id][iOwnerID],
				ItemInfo[id][iOwnerType],
				ItemInfo[id][iName],
				ItemInfo[id][iType],
				ItemInfo[id][iSubType],
				ItemInfo[id][iAmount],
				posStr,
				ItemInfo[id][iUsed],
				ItemInfo[id][iVw],
				ItemInfo[id][iInt]
			);

			sscanf(posStr, "p<,>", ItemInfo[id][iPos][0], ItemInfo[id][iPos][1], ItemInfo[id][iPos][2]);

			Iter_Add(ItemItter, id);

			printf("[PRZEDMIOT] ID: %d, OwnerID: %d, OwnerType: %d, Name: %s, Type: %d, SubType: %d, Amount: %d, Pos: %s, Used: %d, Vw: %d, Int: %d'n",
				ItemInfo[id][iID],
				ItemInfo[id][iOwnerID],
				ItemInfo[id][iOwnerType],
				ItemInfo[id][iName],
				ItemInfo[id][iType],
				ItemInfo[id][iSubType],
				ItemInfo[id][iAmount],
				posStr,
				ItemInfo[id][iUsed],
				ItemInfo[id][iVw],
				ItemInfo[id][iInt]
			);

			id++;
		}
	}

	printf("[PRZEDMIOT] Wczytano %d przedmiotow.", id + 1);

	mysql_free_result();
	return 1;
}

stock AddItem(ownerid, ownertype, name[], type, subtype, amount, Float:PosX, Float:PosY, Float:PosZ) {
	
}

stock RemoveItem(itemid, ammount) {
	if(ItemInfo[itemid][iAmount] <= 0) {
		DestroyItem(itemid);
		return 1;
	}

	ItemInfo[itemid][iAmount] += ammount;

	queryStr[0] = EOS;
	format(queryStr, sizeof(queryStr), "UPDATE `Items` SET `Amount` = '%d' WHERE `ID` = '%d'", ItemInfo[itemid][iAmount], ItemInfo[itemid][iID]);
	mysql_query(queryStr);
	return 1;
}

stock DestroyItem(itemid) {
	if(itemid == -1) {
		return 1;
	}

	if(!Iter_Count(ItemItter)) {
		return 1;
	}

	queryStr[0] = EOS;
	format(queryStr, sizeof(queryStr), "DELETE FROM `Items` WHERE `ID` = '%d'", ItemInfo[itemid][iID]);
	mysql_query(queryStr);

	ItemInfo[itemid][iID] = 0;
	ItemInfo[itemid][iOwnerID] = 0;
	ItemInfo[itemid][iOwnerType] = ITEM_OWNER_NONE;
	ItemInfo[itemid][iName] = " ";
	ItemInfo[itemid][iType] = ITEM_TYPE_NONE;
	ItemInfo[itemid][iSubType] = ITEM_SUB_TYPE_NONE;
	ItemInfo[itemid][iAmount] = 0;
	ItemInfo[itemid][iPos][0] = 0.0;
	ItemInfo[itemid][iPos][1] = 0.0;
	ItemInfo[itemid][iPos][2] = 0.0;
	ItemInfo[itemid][iUsed] = false;
	ItemInfo[itemid][iVw] = 0;
	ItemInfo[itemid][iInt] = 0;

	Iter_Remove(ItemItter, itemid);

	printf("[PRZEDMIOT] Usunieto przedmiot o ID: %d", itemid);
	return 1;
}

stock TakeItem(itemid, ammount) {
	
}

stock PutItem(itemid, ammount) {
	
}

stock GetItemTypeFromID(itemid) {
	return ItemInfo[itemid][iType];
}

stock GetItemSubTypeFromID(itemid) {
	return ItemInfo[itemid][iSubType];
}

stock ShowPlayerItemToSomeOne(playerid, someoneid, dialogid) {
	if(playerid == INVALID_PLAYER_ID) {
		return 1;
	}

	if(someoneid == INVALID_PLAYER_ID) {
		return 1;
	}

	if(!IsPlayerConnected(someoneid)) {
		return 1;
	}

	mainStr[0] = EOS;

	if(playerid == someoneid) {
		foreach(new i : ItemItter) {
			if(ItemInfo[i][iOwnerID] == PlayerInfo[playerid][pID]) {
				if(ItemInfo[i][iOwnerType] == ITEM_OWNER_PLAYER && ItemInfo[i][iPos][0] == 0.0 && ItemInfo[i][iPos][1] == 0.0 && ItemInfo[i][iPos][2] == 0.0) {
					if(ItemInfo[i][iAmount] > 0) {
						format(mainStr, sizeof(mainStr), "%s\n{FFFFFF}%s (Iloœæ: %d)", mainStr, ItemInfo[i][iName], ItemInfo[i][iAmount]);
					} else {
						format(mainStr, sizeof(mainStr), "%s\n{FFFFFF}%s");
					}
				}
			}
		}

		osrp_ShowPlayerDialog(someoneid, dialogid, DIALOG_STYLE_LIST, "Twoje przedmioty:", mainStr, "Wybierz", "Cofnij");
	}
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
