//
//  GameServer.cpp
//  NoNameGame
//
//  Created by Zilo on 2/16/2558 BE.
//  Copyright (c) 2558 THANAKARN LORLERTSAKUL. All rights reserved.
//

#include "GameServer.h"
#include "GameDatabase.h"

#define HEADER_DATA                 u32
#define HEADER_OFFSET               1000

#define GET_HEADER(header_data)     header / HEADER_OFFSET
#define GET_COMMAND(header_data)    header % HEADER_OFFSET

// MARK: TYPE
typedef s16     RoomId;
typedef s8      SeatId;

// MARK: CONST
#define TKEN_LENGTH                 32
#define UUID_LENGTH                 64

#define NAME_LENGTH                 32

#define MAX_EQUIP                   4

#define MAX_ROOM                    64
#define MAX_SEAT                    4
#define MAX_USER                    (2 * MAX_ROOM * MAX_SEAT)

#define MAX_CARD                    52

#define NO_ROOM                     -1
#define NO_SEAT                     -1
#define NO_USER                     0

#define DEF_RANK                    1

// MARK: SERVER STRUCT
typedef struct {
    u32                 id;
    char                name[NAME_LENGTH];
    
    u32                 avatarId;
    u32                 equipmentId[MAX_EQUIP];
    
    u16                 rank;
    u16                 exp;
    
    u32                 gold;
    u32                 win;
    u32                 lose;
}BigUser;
typedef struct {
    
}BigPlayer;
typedef struct {
    const RoomId        id;
    
    BigPlayer           seats[MAX_SEAT];
}BigRoom;

// MARK: REQUEST STRUCT
typedef struct {
    HEADER_DATA         header;
    char                uuid[UUID_LENGTH];
}BigLoginRequest;

// MARK: RESPONSE STRUCT
typedef struct {
    HEADER_DATA         header;
}BigLoginResponse;

//MARK: CORE SERVER FUNCTION
void InitGameServer() {
    SLOG("%ld\n", sizeof(BigUser));
    SLOG("[INFO] InitGameServer: server initialized");
}
void ClientConnect(Client_t *client) {
    
    SLOG("[INFO] ClientConnect: client idx (%d) connected\n", client->idx);
}
void ClientRequest(Client_t *client) {
    int data_count = STSV_GetDataCount(client->idx);
    u32 buffer_size;
    for (int i = 0; i < data_count; i++) {
        char* buffer = STSV_GetData(client->idx, i, &buffer_size);
        ProcessRequest(client, buffer, buffer_size);
    }
}
void ClientDisconnect(Client_t *client) {
    
    SLOG("[INFO] ClientDisonnect: client idx (%d) disconnected\n", client->idx);
}
void DestroyGameServer() {
    
    SLOG("[INFO] DestroyGameServer: server destroyed bye~\n");
}

void ProcessRequest(Client_t *client, char *buffer, u32 buffer_size) {
    
    SLOG("[INFO] ProcessRequest: client idx(%d), buffer size(%d)\n", client->idx, buffer_size);
}