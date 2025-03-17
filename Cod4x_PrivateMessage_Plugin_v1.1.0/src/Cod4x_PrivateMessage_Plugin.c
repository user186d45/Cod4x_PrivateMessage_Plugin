#include "../libs/pinc.h"
#include "version.h"


#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sqlite3.h>

pthread_mutex_t mutex;
sqlite3* db;

// TODO: must modify all instances of client->playerid to the actual player id displayed using ministats

__cdecl int initDb();
__cdecl int initThreads();
__cdecl int writePlayerOnConnect(client_t* client);
__cdecl int deletePlayerOnDC(client_t* client);
__cdecl int isPlayerOnline(const uint64_t playerId);
__cdecl const unsigned char* realPlayerName(const char* rawName, uint64_t clientNum);
__cdecl const int64_t realPlayerId(const uint64_t rawId, uint64_t clientNum);
__cdecl const unsigned char* playerNameById(const uint64_t playerId);
__cdecl void invalidUse(int clientNum, char* argv0);
__cdecl void privateMessage();

PCL int OnInit() {
	int rValue = 0;

	rValue = initDb();
	if(rValue == -1) {
		Plugin_PrintError("Private Message Plugin: Error occurred on initializing database.");
		return 1;

	}

	rValue = initThreads();
	if(rValue == -1) {
		Plugin_PrintError("Private Message Plugin: Error occurred on initializing threads.");
		return 1;

	}

	Plugin_AddCommand("prm", privateMessage, 1);

	return 0;

}

PCL void OnClientEnterWorld(client_t* client) {
	if(isPlayerOnline(client->playerid) == 1) {
		Plugin_Printf("Private Message Plugin: NOTICE: Player already exists.\n");
		return;

	}

	writePlayerOnConnect(client);

}

PCL void OnPlayerDC(client_t* client, const char* reason) {
	deletePlayerOnDC(client);

}

PCL void OnTerminate() {
	sqlite3_close(db);
	pthread_mutex_destroy(&mutex);

}

PCL void OnInfoRequest(pluginInfo_t *info){

    info->handlerVersion.major = PLUGIN_HANDLER_VERSION_MAJOR;
    info->handlerVersion.minor = PLUGIN_HANDLER_VERSION_MINOR;

    info->pluginVersion.major = Cod4x_PrivateMessage_Plugin_VERSION_MAJOR;
    info->pluginVersion.minor = Cod4x_PrivateMessage_Plugin_VERSION_MINOR;
    strncpy(info->fullName,"Cod4X Private Message Plugin",sizeof(info->fullName));
    strncpy(info->shortDescription,"Private Message Plugin for changing maps.",sizeof(info->shortDescription));
    strncpy(info->longDescription,"This plugin is used to send private messages between players. Coded my LM40 ( DevilHunter )",sizeof(info->longDescription));
}

__cdecl int initDb() {

	int rc = 0;
	char* zErrMsg = NULL;

	sqlite3_open("PrivateMessageTmp.db", &db);
	const char* sqlInitDb = "CREATE TABLE IF NOT EXISTS joinedPlayers("
							"ID INTEGER PRIMARY KEY AUTOINCREMENT,"
							"PlayerName TEXT NOT NULL,"
							"PlayerId INTEGER NOT NULL);";

	rc = sqlite3_exec(db, sqlInitDb, NULL, 0, &zErrMsg);
	if(rc != SQLITE_OK) {
		Plugin_PrintError("Private Message Plugin: Error occurred on initializing database: %s\n", zErrMsg);
		return -1;

	}

	Plugin_Printf("Private Message Plugin: NOTICE: Database Initialized successfully.\n");

	return 0;

}

__cdecl int initThreads() {
	pthread_mutex_init(&mutex, NULL);

	return 0;

}

__cdecl int writePlayerOnConnect(client_t* client) {
	pthread_mutex_lock(&mutex);

	int rc = 0;
	sqlite3_stmt* stmt;
	const char* sqlWrite = "INSERT INTO joinedPlayers (PlayerName, PlayerId) VALUES (?,?);";

	rc = sqlite3_prepare_v2(db, sqlWrite, -1, &stmt, NULL);
	if(rc != SQLITE_OK) {
		Plugin_PrintError("Private Message Plugin: Error occurred on preparing player info insertion statement to database: %s", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		pthread_mutex_unlock(&mutex);
		return -1;

	}

	sqlite3_bind_text(stmt, 1, Plugin_GetPlayerName(NUMFORCLIENT(client)), -1, SQLITE_STATIC);
	sqlite3_bind_int64(stmt, 2, client->playerid);

	rc = sqlite3_step(stmt);
	if(rc != SQLITE_DONE) {
		Plugin_PrintError("Private Message Plugin: Error occurred on executing player info insertion statement to database: %s", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		pthread_mutex_unlock(&mutex);
		return -1;

	}

	Plugin_Printf("Private Message Plugin: NOTICE: successfully wrote the player info to database.\n");
	sqlite3_finalize(stmt);
	pthread_mutex_unlock(&mutex);
	return 0;

}

__cdecl int deletePlayerOnDC(client_t* client) {
	pthread_mutex_lock(&mutex);

	int rc = 0;
	sqlite3_stmt* stmt;
	const char* sqlDeletePlayer = "DELETE FROM joinedPlayers WHERE PlayerId = ?;";

	rc = sqlite3_prepare_v2(db, sqlDeletePlayer, -1, &stmt, NULL);
	if(rc != SQLITE_OK) {
		Plugin_PrintError("Private Message Plugin: Error occurred on opening database for omitting the disconnected player: %s", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		pthread_mutex_unlock(&mutex);
		return -1;

	}

	sqlite3_bind_int64(stmt, 1, client->playerid);

	rc = sqlite3_step(stmt);
	if(rc != SQLITE_DONE) {
		Plugin_PrintError("Private Message Plugin: Error occurred on executing the ommitance of the disconnected player: %s", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		pthread_mutex_unlock(&mutex);
		return -1;

	}

	Plugin_Printf("Private Message Plugin: NOTICE: Player successfully removed from the database.\n");
	sqlite3_finalize(stmt);
	pthread_mutex_unlock(&mutex);

	return 0;
}

__cdecl int isPlayerOnline(const uint64_t playerId) {
    pthread_mutex_lock(&mutex);

    int rc = 0;
    sqlite3_stmt* stmt = NULL;
    const char* sqlFind = "SELECT COUNT(*) FROM joinedPlayers WHERE PlayerId = ?;";

    rc = sqlite3_prepare_v2(db, sqlFind, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        Plugin_PrintError(
            "Private Message Plugin: Error occurred on preparing "
            "find statement for player search: %s", sqlite3_errmsg(db)
        );
        pthread_mutex_unlock(&mutex);
        return -1;
    }

    rc = sqlite3_bind_int64(stmt, 1, playerId);
    if (rc != SQLITE_OK) {
        Plugin_PrintError(
            "Private Message Plugin: Error occurred on binding "
            "player name: %s", sqlite3_errmsg(db)
        );
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&mutex);
        return -1;

    }

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        Plugin_PrintError(
            "Private Message Plugin: Error occurred on executing "
            "find statement for player search: %s", sqlite3_errmsg(db)
        );
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&mutex);
        return -1;

    }

    int result = sqlite3_column_int(stmt, 0);

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&mutex);
    return result;
}


__cdecl const unsigned char* realPlayerName(const char* rawName, uint64_t clientNum) {
	pthread_mutex_lock(&mutex);

	int rc = 0;
	sqlite3_stmt* stmt;
	const char* sqlCountNames = "SELECT COUNT(*) AS matchedNames FROM joinedPlayers WHERE PlayerName LIKE ?;";
	char pattern[128];
	unsigned int count = 0;

	rc = sqlite3_prepare_v2(db, sqlCountNames, -1, &stmt, 0);
	if(rc != SQLITE_OK) {
		Plugin_PrintError("Private Message Plugin: Error occurred on preparing real name find statement: %s", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		pthread_mutex_unlock(&mutex);
		return NULL;
	}

	memset(pattern, 0, sizeof(pattern));
	snprintf(pattern, sizeof(pattern), "%%%s%%", rawName);

	sqlite3_bind_text(stmt, 1, pattern, -1, SQLITE_STATIC);

	rc = sqlite3_step(stmt);
	if(rc == SQLITE_ROW) {
		count = sqlite3_column_int(stmt, 0);

	} else if (rc == SQLITE_ERROR) {
		Plugin_PrintError("Private Message Plugin: Error occurred on executing real player find statement: %s", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		pthread_mutex_unlock(&mutex);
		return NULL;
	}

	if(count > 1) {
		Plugin_ChatPrintf(clientNum, "^1Error: ^7There are more than one players with the specified name, maybe try using their playerId?.");
		sqlite3_finalize(stmt);
		pthread_mutex_unlock(&mutex);
		return NULL;

	} else if(count == 0) {
		Plugin_ChatPrintf(clientNum, "^1Error: ^7There are no players with the specified name.");
		sqlite3_finalize(stmt);
		pthread_mutex_unlock(&mutex);
		return NULL;

	}

	sqlite3_reset(stmt);

	const char* sqlRealName = "Select PlayerName FROM joinedPlayers WHERE PlayerName LIKE ?;";

	rc = sqlite3_prepare_v2(db, sqlRealName, -1, &stmt, 0);
	if(rc != SQLITE_OK) {
		Plugin_PrintError("Private Message Plugin: Error occurred on preparing database search statement for receiver's real name: %s", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		pthread_mutex_unlock(&mutex);
		return NULL;

	}

	sqlite3_bind_text(stmt, 1, pattern, -1, SQLITE_STATIC);

	rc = sqlite3_step(stmt);
	if(rc != SQLITE_ROW) {
		Plugin_PrintError("Private Message Plugin: Error occurred on reading database for getting receiver's real name: %s", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		pthread_mutex_unlock(&mutex);
		return NULL;
	}

	const unsigned char* realPlayerName = sqlite3_column_text(stmt, 0);

	sqlite3_finalize(stmt);
	pthread_mutex_unlock(&mutex);
	return realPlayerName;

}

__cdecl const int64_t realPlayerId(const uint64_t rawId, uint64_t clientNum) {
	pthread_mutex_lock(&mutex);

	int rc = 0;
	sqlite3_stmt* stmt;
	const char* sqlCountNames = "SELECT COUNT(*) AS matchedIds FROM joinedPlayers WHERE CAST(PlayerId AS TEXT) LIKE ?;";
	char pattern[128];
	unsigned int count = 0;

	rc = sqlite3_prepare_v2(db, sqlCountNames, -1, &stmt, 0);
	if(rc != SQLITE_OK) {
		Plugin_PrintError("Private Message Plugin: Error occurred on preparing real playerId find statement: %s", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		pthread_mutex_unlock(&mutex);
		return -1;
	}

	memset(pattern, 0, sizeof(pattern));
	snprintf(pattern, sizeof(pattern), "%%%llu%%", rawId);

	sqlite3_bind_text(stmt, 1, pattern, -1, SQLITE_STATIC);

	rc = sqlite3_step(stmt);
	if(rc == SQLITE_ROW) {
		if (rc == SQLITE_ERROR) {
			Plugin_PrintError("Private Message Plugin: Error occurred on executing real player find statement: %s", sqlite3_errmsg(db));
			sqlite3_finalize(stmt);
			pthread_mutex_unlock(&mutex);
			return -1;
		}

		count = sqlite3_column_int(stmt, 0);

	}

	if(count > 1) {
		Plugin_ChatPrintf(clientNum, "^1Error: ^7There are more than one players with the specified plaeyrId.");
		sqlite3_finalize(stmt);
		pthread_mutex_unlock(&mutex);
		return -1;

	} else if(count == 0) {
		Plugin_ChatPrintf(clientNum, "^1Error: ^7There are no players with the specified playerId.");
		sqlite3_finalize(stmt);
		pthread_mutex_unlock(&mutex);
		return -1;

	}

	sqlite3_reset(stmt);

	const char* sqlRealName = "Select PlayerId FROM joinedPlayers WHERE CAST(PlayerId AS TEXT) LIKE ?;";

	rc = sqlite3_prepare_v2(db, sqlRealName, -1, &stmt, 0);
	if(rc != SQLITE_OK) {
		Plugin_PrintError("Private Message Plugin: Error occurred on preparing database search statement for receiver's real name: %s", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		pthread_mutex_unlock(&mutex);
		return -1;

	}

	sqlite3_bind_text(stmt, 1, pattern, -1, SQLITE_STATIC);

	rc = sqlite3_step(stmt);
	if(rc != SQLITE_ROW) {
		Plugin_PrintError("Private Message Plugin: Error occurred on reading database for getting receiver's real name: %s", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		pthread_mutex_unlock(&mutex);
		return -1;
	}

	const uint64_t realPlayerId = sqlite3_column_int64(stmt, 0);

	sqlite3_finalize(stmt);


	pthread_mutex_unlock(&mutex);

	return realPlayerId;

}

__cdecl const unsigned char* playerNameById(const uint64_t playerId) {
	pthread_mutex_lock(&mutex);

	int rc = 0;
	const char* sqlGetPlayerName = "SELECT PlayerName FROM joinedPlayers WHERE PlayerId = ?";
	sqlite3_stmt* stmt;

	rc = sqlite3_prepare_v2(db, sqlGetPlayerName, -1, &stmt, NULL);
	if(rc != SQLITE_OK) {
		Plugin_PrintError("Private Message Plugin: Error occurred on preparing sqlGetPlayerName statement: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		pthread_mutex_unlock(&mutex);
		return NULL;

	}

	sqlite3_bind_int64(stmt, 1, playerId);

	rc = sqlite3_step(stmt);
	if(rc != SQLITE_ROW) {
		Plugin_PrintError("Private Message Plugin: Error occurred on executing sqlGetPlayerName statement after binding: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		pthread_mutex_unlock(&mutex);
		return NULL;

	}

	const unsigned char* playerName = sqlite3_column_text(stmt, 0);
	sqlite3_finalize(stmt);
	pthread_mutex_unlock(&mutex);

	Plugin_Printf("Private Message Plugin: NOTICE: Returning player name received from database by id: %s\n", playerName);

	return playerName;

}

__cdecl void invalidUse(int clientNum, char* argv0) {
	Plugin_ChatPrintf(clientNum, "^1Invalid Use^7. Usage: %s <playerName> <message> \n", argv0);

}

__cdecl void privateMessage() {

	int rc = 0;
	int clientNum = Plugin_Cmd_GetInvokerClnum();

	if(Plugin_Cmd_Argc() < 2) {
		invalidUse(clientNum, Plugin_Cmd_Argv(0));
		return;

	}

	rc = isPlayerOnline(Plugin_GetClientForClientNum(clientNum)->playerid);
	if( rc == 0) {
		Plugin_ChatPrintf(clientNum, "^1Error: ^7There is not a invoker player with the specified name online!");
		return;

	} else if(rc == -1) {
		Plugin_PrintError("Private Message Plugin: Error occurred on checking invoker player status.\n");
		return;

	}

	if(strcmp(Plugin_Cmd_Argv(1), "i") == 0) {
		Plugin_Printf("Private Message Plugin: NOTICE: \"i\" command specified, entering id search mode...\n");

		uint64_t pIdArg = atoi(Plugin_Cmd_Argv(2));
		if(pIdArg == 0) {
			Plugin_ChatPrintf(clientNum, "^1Error: ^7Input id is not valid.\n");
			Plugin_PrintError("Private Message Plugin: Error occurred on getting PlayerId: atoi failing...\n");
			return;

		}

		const int64_t playerId = realPlayerId(pIdArg, clientNum);
		if(playerId == -1) {
			Plugin_PrintError("Private Message Plugin: Error occurred on getting real playerId of the user.\n");
			return;

		}

		const unsigned char* PlayerName = playerNameById(playerId);
		if(!PlayerName) {
			Plugin_PrintError("Private Message Plugin: Error occurred on getting playerName.\n");
			return;

		}

		char msg[512];
		size_t usedLen = 0;
		for(int i = 3, j = 0; i < Plugin_Cmd_Argc() && usedLen < sizeof(msg); i++) {
			char* argv = Plugin_Cmd_Argv(i);

			if (j == 0) {
				memset(msg, 0, sizeof(msg));
				sprintf(msg, "Private Message from ^4%s^7: ",Plugin_GetPlayerName(clientNum));
				usedLen = 29;
				j = 1;

			} else {
				strcat(msg, " "); // strncat throws warning.

			}

			strncat(msg, Plugin_Cmd_Argv(i), strlen(argv) + (1 * sizeof(char)));
			usedLen += strlen(argv) + (1 * sizeof(char)); // including the " "

		}

		usedLen = 0;

		client_t* client2send = Plugin_SV_Cmd_GetPlayerClByHandle((const char*)PlayerName);
		Plugin_ChatPrintf(NUMFORCLIENT(client2send), msg);
		Plugin_ChatPrintf(clientNum, "Message sent ^2successfully^7.");
		return;

	}


	const unsigned char* rPlayerName = realPlayerName(Plugin_Cmd_Argv(1), clientNum);
	if (!rPlayerName) {
		Plugin_PrintError("Private Message Plugin: Error occurred on reading realPlayerName.\n");
		return;

	}

	char msg[512];
	size_t usedLen = 0;
	for(int i = 2, j = 0; i < Plugin_Cmd_Argc() && usedLen < sizeof(msg); i++) {
		char* argv = Plugin_Cmd_Argv(i);

		if (j == 0) {
			memset(msg, 0, sizeof(msg));
			sprintf(msg, "Private Message from ^4%s^7: ",Plugin_GetPlayerName(clientNum));
			usedLen = 29;
			j = 1;

		} else {
			strcat(msg, " "); // strncat throws warning.

		}

		strncat(msg, Plugin_Cmd_Argv(i), strlen(argv) + (1 * sizeof(char)));
		usedLen += strlen(argv) + (1 * sizeof(char)); // including the " "

	}

	usedLen = 0;

	client_t* client2send = Plugin_SV_Cmd_GetPlayerClByHandle((const char*)rPlayerName);
	Plugin_ChatPrintf(NUMFORCLIENT(client2send), msg);
	Plugin_ChatPrintf(clientNum, "Message sent ^2successfully^7.");

}
