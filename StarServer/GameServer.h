//
//  GameServer.h
//  StarServer
//
//  Created by Zilo on 2/16/2558 BE.
//  Copyright (c) 2558 THANAKARN LORLERTSAKUL. All rights reserved.
//  Mod by Maxoja

#ifndef __StarServer__GameServer__
#define __StarServer__GameServer__

#include "StarSocketServer.h"

void InitGameServer     ();
void ClientConnect      (Client_t* client);
void ClientRequest      (Client_t* client);
void ClientDisconnect   (Client_t* client);
void DestroyGameServer  ();

void ProcessRequest     (Client_t* client, char* buffer, u32 buffer_size);

#endif /* defined(__StarServer__GameServer__) */
