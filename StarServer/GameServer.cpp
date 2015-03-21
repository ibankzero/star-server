//
//  GameServer.cpp
//  StarServer
//
//  Created by Zilo on 2/16/2558 BE.
//  Copyright (c) 2558 THANAKARN LORLERTSAKUL. All rights reserved.
//

#include "GameServer.h"
#include "GameDatabase.h"

void InitGameServer() {
    
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
