//
//  GameServer.cpp
//  BiggestServer
//
//  Created by Zilo on 2/16/2558 BE.
//  Copyright (c) 2558 THANAKARN LORLERTSAKUL. All rights reserved.
//

#include "GameServer.h"
#include "GameDatabase.h"

#define HEADER_DATA                 u32
#define HEADER_OFFSET               1000

#define GET_HEADER(header_data)     header / HEADER_OFFSET
#define GET_CODE(header_data)       header % HEADER_OFFSET

// MARK: TYPE
typedef u32     GameId;
typedef s16     RoomId;
typedef s8      SeatId;

// MARK: CONST
#define BIG_TOKEN_LENGTH                32
#define BIG_UUID_LENGTH                 64
#define BIG_USERNAME_LENGTH             32

#define BIG_ROOMNAME_LENGTH             32
#define BIG_ROOMPASS_LENGTH             8

#define BIG_MAX_EQUIP_SLOT              4
#define BIG_MAX_REWARD_POINT            1000

#define BIG_MAX_TEMP_RAND               64
#define BIG_MAX_ROOM                    64
#define BIG_MAX_SEAT                    4
#define BIG_MAX_USER                    (2 * BIG_MAX_ROOM * BIG_MAX_SEAT)

#define BIG_MAX_MATCH                   13
#define BIG_MAX_CARD                    52

#define BIG_OFFLINE                     0
#define BIG_ONLINE                      1

#define BIG_NO_GAME                     0
#define BIG_NO_ROOM                     -1
#define BIG_NO_SEAT                     -1
#define BIG_NO_CARD                     -1
#define BIG_NO_USER                     0
#define BIG_NO_EQUIP                    0
#define BIG_NO_CHIP                     0
#define BIG_NO_BET                      0

#define BIG_DEF_RANK                    1
#define BIG_DEF_AVATAR                  0
#define BIG_DEF_VERSION                 0
#define BIG_DEF_GAMESTATE               0
#define BIG_DEF_NEXTCARD                0
#define BIG_DEF_NEXTRAND                0
#define BIG_DEF_CURRENTMATCH            0
#define BIG_DEF_MINCHIP                 0
#define BIG_DEF_POT                     0
#define BIG_DEF_HIGHBET                 0
#define BIG_DEF_ACTION                  0

#define BIG_DEF_REWARD_LEVEL            0
#define BIG_DEF_REWARD_POINT            0
#define BIG_DEF_GOLD                    0
#define BIG_DEF_WIN                     0
#define BIG_DEF_LOSE                    0
#define BIG_DEF_REMAIN_HOURLY           3600

// MARK: HEADER
#define LOGIN_HEADER                    1000
#define LOGIN_CODE_OK                   1100
#define LOGIN_CODE_REJOIN_OK            1101     // REJOIN GAME OK
#define LOGIN_CODE_REJOIN_END           1102     // REJOIN GAME WAS ENDED
#define LOGIN_CODE_NEW_USER             1103     // NEW USER (GO TO REGISTER)
// -- FAIL
#define LOGIN_CODE_BANNED               1300     // BANNED USER
#define LOGIN_CODE_ALREADY_LOGIN        1301     // ALREADY
// -- ERROR
#define LOGIN_CODE_ERROR                1404     // ERROR UNKNOWN CASE


// MARK: UTILLITY FUNCTION
static void GenerateRoomToken(char *token, RoomId roomId) {
    static timeval tv;
    gettimeofday(&tv,NULL);
    sprintf(token,"%03d%ld%d",roomId, tv.tv_sec, tv.tv_usec);
}

// MARK: DATA STRUCT
typedef struct {
    GameId gameId;
    
    u32 winnerId;
    u32 lose1Id;
    u32 lose2Id;
    u32 lose3Id;
    
    u32 winnerBet;
    u32 lose1Bet;
    u32 lose2Bet;
    u32 lose3Bet;
    
    s8  winnerAction;
    s8  lose1Action;
    s8  lose2Action;
    s8  lose3Action;
    
    s8  winnerCard;
    s8  lose1Card;
    s8  lose2Card;
    s8  lose3Card;
    
    u32 pot;
}MatchesHistory;
typedef struct {
    u32                 id;
    u16                 forAvatarId;
    u8                  slot;
    u8                  rarity;
    u32                 price;
}Equipment;
typedef struct {
    u32                 id;
    char                username[BIG_USERNAME_LENGTH];
    u32                 rank;
    u32                 avatarId;
    Equipment           equipments[BIG_MAX_EQUIP_SLOT];
}BigUser;
typedef struct {
    BigUser             user;
    
    u16                 rewardLevel; // more level more rare
    u16                 rewardPoint; // give reward when full (MAX_REWARD_POINT)
    
    u32                 gold;
    u32                 win;
    u32                 lose;
    u32                 remainHourly;
    
    GameId              lastGameId;
    RoomId              lastRoomId;
    SeatId              lastSeatId;
}BigUserData;
typedef struct {
    BigUser             user;
    
    s8                  online; // 0 = offline
    s8                  ignore;
    
    s8                  hand;
    s8                  action;
    
    u32                 chip;
    u32                 bet;
}BigPlayer;
typedef struct {
    const RoomId        id;
    
    GameId              gameId;
    u32                 tokenKey;
    u32                 roomVersion;
    
    char                roomname[BIG_ROOMNAME_LENGTH];
    char                roompass[BIG_ROOMPASS_LENGTH];
    
    s8                  cards[BIG_MAX_CARD];
    u32                 tempRand[BIG_MAX_TEMP_RAND];
    MatchesHistory      matchesHistory[BIG_MAX_MATCH];
    
    u8                  nextCardIndex;
    u8                  nextRandIndex;
    u8                  currentMatch;
    
    u8                  onlineCount;
    u8                  minChipId;
    
    SeatId              winnerSeatId;
    s8                  gameState;
    s8                  currentPlayer;
    
    u32                 pot;
    u32                 highBet;
    
    BigPlayer           seats[BIG_MAX_SEAT];
    
    pthread_mutex_t     room_mutex;
    pthread_mutex_t     pull_mutex;
}BigRoom;


// MARK: REQUEST STRUCT
typedef struct {
    HEADER_DATA         header;
    char                uuid[BIG_UUID_LENGTH];
}BigLoginRequest;
typedef struct {
    
}BigRegisterRequest;

// MARK: RESPONSE STRUCT
typedef struct {
    HEADER_DATA         header;
    RoomId              lastRoomId;
    SeatId              lastSeatId;
}BigLoginResponse;
typedef struct {
    
}BigRegisterResponse;

// MARK: VARIABLE
static s8               DEFAULT_DECK[BIG_MAX_CARD] = {
    0 , 1 , 2 , 3 , 4 , 5 , 6 , 7 , 8 , 9 , 10, 11, 12,
    13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
    26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38,
    39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51
};

static u32              dbTotalEquipment;
static Equipment        *dbEquipments;

static BigRoom          *bigRoom;
static BigUserData      *bigUserData;
static pthread_mutex_t  register_mutex;

// MARK: HELPER FUNCTION
static void SetEquipment(Equipment *dest, u32 id, u16 forAvatarId, u8 slot, u8 rarity, u32 price) {
    dest->id            = id;
    dest->forAvatarId   = forAvatarId;
    dest->slot          = slot;
    dest->rarity        = rarity;
    dest->price         = price;
}
static void ResetEquipment(Equipment *dest) {
    SetEquipment(dest, 0, 0, 0, 0, 0);
}
static s8 InitEquipmentDb(int dbIndex) {
    if (STSV_DBQuery(dbIndex, "SELECT * FROM equipments")) {
        
        dbTotalEquipment    = STSV_DBNumRow(dbIndex);
        dbEquipments        = (Equipment*)malloc(dbTotalEquipment * sizeof(Equipment));
        
        SetEquipment(&dbEquipments[BIG_NO_EQUIP], 0, 0, 0, 0, 0); //
        
        for (u32 i = 1; i < dbTotalEquipment; i++) {
            STSV_DBFetchRow(dbIndex);
            // TODO: implement later
            SetEquipment(&dbEquipments[i], 0, 0, 0, 0, 0);
        }
        return 0;
    }
    else {
        return -1;
    }
}

static void SetBigUser(BigUser *dest, u32 id, const char *username, u16 rank, u16 avatarId, Equipment *equipments) {
    dest->id = id;
    strcpy(dest->username, username);
    dest->rank = rank;
    dest->avatarId = avatarId;
    memcpy(dest->equipments, equipments, sizeof(Equipment) * BIG_MAX_EQUIP_SLOT);
}
static void ResetBigUser(BigUser* dest) {
    Equipment equips[BIG_MAX_EQUIP_SLOT];
    for (u8 slot = 0; slot < BIG_MAX_EQUIP_SLOT; slot++) {
        memcpy(&equips[slot], &dbEquipments[BIG_NO_EQUIP], sizeof(Equipment));
    }
    SetBigUser(dest, BIG_NO_USER, "", BIG_DEF_RANK, BIG_DEF_AVATAR, equips);
}

static void SetBigUserData(BigUserData *dest, BigUser *user, u16 rewardLevel, u16 rewardPoint, u32 gold, u32 win, u32 lose, u32 remainHourly, GameId gameId, RoomId roomId, SeatId seatId) {
    memcpy(&dest->user, user, sizeof(BigUser));
    
    dest->rewardLevel   = rewardLevel;
    dest->rewardPoint   = rewardPoint;
    dest->gold          = gold;
    dest->win           = win;
    dest->lose          = lose;
    dest->remainHourly  = remainHourly;
    dest->lastGameId    = gameId;
    dest->lastRoomId    = roomId;
    dest->lastSeatId    = seatId;
}
static void ResetBigUserData(BigUserData *dest) {
    BigUser user;
    ResetBigUser(&user);
    SetBigUserData(dest, &user, BIG_DEF_REWARD_LEVEL, BIG_DEF_REWARD_POINT, BIG_DEF_GOLD, BIG_DEF_WIN, BIG_DEF_LOSE, BIG_DEF_REMAIN_HOURLY, BIG_NO_GAME, BIG_NO_ROOM, BIG_NO_SEAT);
}
static void InitBigUserData() {
    bigUserData     = (BigUserData*)malloc(BIG_MAX_USER * sizeof(BigUserData));
    for (int i = 0; i < BIG_MAX_USER; i++) {
        ResetBigUserData(&bigUserData[i]);
    }
}

static void SetBigPlayer(BigPlayer *dest, BigUser *user, s8 online, s8 hand, s8 action, u32 chip, u32 bet) {
    memcpy(&dest->user, user, sizeof(BigUser));
    dest->online    = online;
    dest->hand      = hand;
    dest->action    = action;
    dest->chip      = chip;
    dest->bet       = bet;
}
static void ResetBigPlayer(BigPlayer *dest) {
    BigUser user;
    ResetBigUser(&user);
    SetBigPlayer(dest, &user, BIG_OFFLINE, BIG_NO_CARD, BIG_DEF_ACTION, BIG_NO_CHIP, BIG_NO_BET);
}

static void SetBigRoom(BigRoom *dest, GameId gameId, u32 tokenKey, u32 roomVersion, const char* roomname,
                       const char* roompass, s8 *cards, u32 *tempRand, MatchesHistory *matchesHistory, u8 nextCardIndex,
                       u8 nextRandIndex, u8 currentMatch, u8 onlineCount, u8 minChipId, SeatId winnerSeatId,
                       s8 gameState, s8 currentPlayer, u32 pot, u32 highBet, BigPlayer* seats) {
    dest->gameId        = gameId;
    dest->tokenKey      = tokenKey;
    dest->roomVersion   = roomVersion;
    
    strcpy(dest->roomname, roomname);
    strcpy(dest->roompass, roompass);
    
    memcpy(dest->cards, cards, sizeof(s8) * BIG_MAX_CARD);
    memcpy(dest->tempRand, tempRand, sizeof(u32) * BIG_MAX_TEMP_RAND);
    memcpy(dest->matchesHistory, matchesHistory, sizeof(MatchesHistory) * BIG_MAX_MATCH);
    
    dest->nextCardIndex = nextCardIndex;
    dest->nextRandIndex = nextRandIndex;
    dest->currentMatch  = currentMatch;
    
    dest->onlineCount   = onlineCount;
    dest->minChipId     = minChipId;
    
    dest->winnerSeatId  = winnerSeatId;
    dest->gameState     = gameState;
    dest->currentPlayer = currentPlayer;
    
    dest->pot           = pot;
    dest->highBet       = highBet;
    
    memcpy(dest->seats, seats, sizeof(BigPlayer) * BIG_MAX_SEAT);
}
static void ResetBigRoom(BigRoom *dest) {
    char            token[BIG_TOKEN_LENGTH];
    u32             tempRand[BIG_MAX_TEMP_RAND];
    BigPlayer       seats[BIG_MAX_SEAT];
    MatchesHistory  history[BIG_MAX_MATCH];
    
    GenerateRoomToken(token, dest->id);
    for (SeatId seat = 0; seat < BIG_MAX_SEAT; seat++) {
        ResetBigPlayer(&seats[seat]);
    }
    
    srand(atoi(token));
    for (s8 index = 0; index < BIG_MAX_TEMP_RAND; index++) {
        tempRand[index] = rand();
    }
    
    for (s8 i = 0; i < BIG_MAX_MATCH; i++) {
        memset(&history, 0, sizeof(MatchesHistory));
    }
    
    SetBigRoom(dest, BIG_NO_GAME, atoi(token), BIG_DEF_VERSION, "", "", DEFAULT_DECK, tempRand, history,
               BIG_DEF_NEXTCARD, BIG_DEF_NEXTRAND, BIG_DEF_CURRENTMATCH, BIG_MAX_SEAT, BIG_DEF_MINCHIP,
               BIG_NO_SEAT, BIG_DEF_GAMESTATE, BIG_NO_SEAT, BIG_DEF_POT, BIG_DEF_HIGHBET, seats);
}
static void InitBigRoom() {
    bigRoom     = (BigRoom*)malloc(BIG_MAX_ROOM * sizeof(BigRoom));
    for (RoomId i = 0; i < BIG_MAX_ROOM; i++) {
        BigRoom room = { .id = i };
        ResetBigRoom(&room);
        memcpy(&bigRoom, &room, sizeof(BigRoom));
    }
}


static void SetUserDataFromDB(BigUserData *dest, int dbIndex) {
    Equipment equips[] {
        dbEquipments[atoi(STSV_DBGetRowStringValue(dbIndex, 5))],
        dbEquipments[atoi(STSV_DBGetRowStringValue(dbIndex, 6))],
        dbEquipments[atoi(STSV_DBGetRowStringValue(dbIndex, 7))],
        dbEquipments[atoi(STSV_DBGetRowStringValue(dbIndex, 8))]
    };
    
    BigUser user;
    SetBigUser(&user,
               atoi(STSV_DBGetRowStringValue(dbIndex, 0)),
               STSV_DBGetRowStringValue(dbIndex, 2),
               atoi(STSV_DBGetRowStringValue(dbIndex, 3)),
               atoi(STSV_DBGetRowStringValue(dbIndex, 4)),
               equips
               );
    
    u32 remain = BIG_DEF_REMAIN_HOURLY - (GetTimeMillisec() - atol(STSV_DBGetRowStringValue(dbIndex, 14)));
    
    SetBigUserData(dest, &user,
                   atoi(STSV_DBGetRowStringValue(dbIndex, 9)),
                   atoi(STSV_DBGetRowStringValue(dbIndex, 10)),
                   atoi(STSV_DBGetRowStringValue(dbIndex, 11)),
                   atoi(STSV_DBGetRowStringValue(dbIndex, 12)),
                   atoi(STSV_DBGetRowStringValue(dbIndex, 13)),
                   remain,
                   atoi(STSV_DBGetRowStringValue(dbIndex, 15)),
                   atoi(STSV_DBGetRowStringValue(dbIndex, 16)),
                   atoi(STSV_DBGetRowStringValue(dbIndex, 17)));
}


// MARK: PROCESS REQUEST
static void LoginRequestHandler(Client_t *client, BigLoginRequest *req) {
    BigLoginResponse res = { .header = LOGIN_HEADER, .lastRoomId = BIG_NO_ROOM, .lastSeatId = BIG_NO_SEAT };
    BigUserData *pUserData = &bigUserData[client->idx];
    
    int dbIndex = STSV_DBFindAndLockResource();
    
    if (STSV_DBQuery(dbIndex,
        "SELECT * FROM users WHERE uuid = '%s' LIMIT 1", req->uuid)) {
        if (STSV_DBNumRow(dbIndex) == 1) {
            STSV_DBFetchRow(dbIndex);
            
            if (atoi(STSV_DBGetRowStringValue(dbIndex, 18)) == BIG_OFFLINE) {
                SetUserDataFromDB(pUserData, dbIndex);
            }
            else {
                
            }
        }
    }
    
    STSV_DBUnlockResource(dbIndex);
}



// MARK: CORE SERVER FUNCTION
s8 InitGameServer() {
    s8 ret = 0;
    
    InitBigUserData();
    InitBigRoom();
    
    pthread_mutex_init(&register_mutex, NULL);
    
    int dbIndex = STSV_DBFindAndLockResource();
    
    ret = InitEquipmentDb(dbIndex);
    if (ret != 0) {
        SLOG("[ERROR] InitGameServer: InitEquipmentDb fail (%d)\n", ret);
        return ret;
    }
    
    
    
    STSV_DBUnlockResource(dbIndex);
    
    SLOG("[INFO] InitGameServer: server initialized");
    return ret;
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
    
    HEADER_DATA header = *(HEADER_DATA*)buffer;
    switch (GET_HEADER(header)) {
        case LOGIN_HEADER:
            LoginRequestHandler(client, (BigLoginRequest*)buffer);
            break;
            
        default:
            break;
    }
    
    SLOG("[INFO] ProcessRequest: client idx(%d), buffer size(%d)\n", client->idx, buffer_size);
}