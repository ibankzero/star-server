//
//  GameServer.h
//  NoNameGame
//
//  Created by Zilo on 2/16/2558 BE.
//  Copyright (c) 2558 THANAKARN LORLERTSAKUL. All rights reserved.
//

#ifndef __NoNameGame__GameServer__
#define __NoNameGame__GameServer__

#include "StarSocketServer.h"

void InitGameServer();
void ClientConnect(Client_t *client);
void ClientRequest(Client_t *client);
void ProcessRequest(Client_t *client, char *buffer, u32 buffer_size);
void ClientDisconnect(Client_t *client);
void DestroyGameServer();

#endif /* defined(__NoNameGame__GameServer__) */
