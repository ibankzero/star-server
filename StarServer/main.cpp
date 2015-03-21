//
//  main.cpp
//  StarServer
//
//  Created by SAMRET WAJANASATHIAN on 6/11/2556.
//  Copyright (c) พ.ศ. 2556 SAMRET WAJANASATHIAN. All rights reserved.
//

#include "GameServer.h"

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
