//
//  main.cpp
//  StarServer
//
//  Created by SAMRET WAJANASATHIAN on 6/11/2556.
//  Copyright (c) พ.ศ. 2556 SAMRET WAJANASATHIAN. All rights reserved.
//

#include "GameServer.h"

void ClientRequest(Client_t *client) {
    int data_count = STSV_GetDataCount(client->idx);
    u32 buffer_size;
    for (int i = 0; i < data_count; i++) {
        char* buffer = STSV_GetData(client->idx, i, &buffer_size);
        ProcessRequest(client, buffer, buffer_size);
    }
}

int main(int argc, const char* argv[]) {
    
    STSV_SetUnlimitServer(true);
    STSV_SetClientRequestFunction(ClientRequest);
    STSV_SetClientConnectFunction(ClientConnect);
    STSV_SetClientDisconnectFunction(ClientDisconnect);
    
    int ret = STSV_Init();
    
    InitGameServer();
    
    while (ret == 0) {
        if(kbhit()) {
            if(getch() == 'q') {
                ret = -1;
            }
        }
        usleep(1000);
    }
    STSV_Stop();
    
    DestroyGameServer();
    
    return 0;
}
