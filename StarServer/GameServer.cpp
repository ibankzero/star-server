//
//  GameServer.cpp
//  NoNameGame
//
//  Created by Zilo on 2/16/2558 BE.
//  Copyright (c) 2558 THANAKARN LORLERTSAKUL. All rights reserved.
//

#include "GameServer.h"

// TEST

void InitGameServer() {
    
    SLOG("[INFO] InitGameServer: server initialized");
}
void ClientConnect(Client_t *client) {
    
    SLOG("[INFO] ClientConnect: client idx (%d) connected\n", client->idx);
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
