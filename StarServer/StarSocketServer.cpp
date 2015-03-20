#include "StarSocketServer.h"


/*---------------------------------------------------------------------------*
 Packet Type
 *---------------------------------------------------------------------------*/
// สามารถส่งพร้อมกันได้หลาย ๆ ตัว
#define STSOCKMSG_PACKDATASIZE      0x0001
#define STSOCKMSG_CHATDATA          0x0002
#define STSOCKMSG_FILEDATA          0x0004 // for patch or other file use only TCP
#define STSOCKMSG_BIGDATA           0x0008

#define STSOCKMSG_ALLDATA           0x000f

/*---------------------------------------------------------------------------*
 constant definitions
 *---------------------------------------------------------------------------*/
#define STSV_INVALID_SOCKET		-1
//Protocal
#define STSV_TCP_PROTOCOL		 0
#define STSV_UDP_PROTOCOL		 1

#define MAX_GAME_SERVER 1

// system packet จะเป็นค่าติดลบ ทั้งหมด
#define STSOCKMSG_DATASIZE          -1
#define STSOCKMSG_GIVEID            -2
#define STSOCKMSG_KNOCK             -3
#define STSOCKMSG_CLOSE_CONNECTION  -4
#define STSOCKMSG_REGISTER          -5   // first time check
#define STSOCKMSG_RETRASMIT         -6
#define STSOCKMSG_STREAMDATA        -7
#define STSOCKMSG_PING              -9
#define STSOCKMSG_CONFIRM           -10
#define STSOCKMSG_DUPLICATE         -11

//SYSTEM ROOM PACKAGE
#define STSOCKMSG_CREATE_ROOM       -11
#define STSOCKMSG_JOIN_ROOM         -12
#define STSOCKMSG_START_ROOM        -13
#define STSOCKMSG_EXTI_ROOM         -14

#define STSOCKMSG_CONFIRM_CREATE_ROOM       -15
#define STSOCKMSG_CONFIRM_JOIN_ROOM         -16
#define STSOCKMSG_CONFIRM_START_ROOM        -17
#define STSOCKMSG_CONFIRM_EXTI_ROOM         -18


#define STSOCKMSG_BIGDATA_HEADER    0x0001
#define STSOCKMSG_BIGDATA_CONTINUE  0x0002
#define STSOCKMSG_BIGDATA_END       0x0004

// Register Responst
#define STSOCKMSG_REGIS_OK_ALWAY    1
#define STSOCKMSG_REGIS_OK_SECTION  2
#define STSOCKMSG_OLD_VERSION       3
#define STSOCKMSG_SERVER_FULL       4
#define STSOCKMSG_CLIENT_WRONG      5


#define STSOCKMSG_STREAMBLOCKSIZE  (8*1024)

#define STSV_MAX_BUFFER            64*1024  // 56K
#define STSV_MAX_SQL_BUFFER        64*1024
#define STSV_DEFAULT_PORT		   9009
#define STSV_MAX_DB_CONNECTION     10
#define STSV_CLIENT_MAX            50
#define STSV_MAX_ROOM              (STSV_CLIENT_MAX/4)

#define STSV_BIGDATA_BUFFER        1*1024*1024   // 5 MB // ส่งกลับไปบอกด้วยว่าส่งได้มากสุดเท่าไหร่
#define STSV_BIGDATA_POOL_BLOCK    1

#define STSV_MAX_PLAYER_PER_ROOM    20


typedef struct{
    s16   type;
    s16   fromID; // source hots ID
    s32   toNo; // destination host ID
}STSOCKMSG_GENERIC; // 8 byte

//typedef struct{
//    s32 conf;
//}STSOCKMSG_CONF;

typedef struct
{
    s16 type;
    s16 fromID;
    s32 packNo;
    s32 size;
}STSOCKMSGDATA_DATASIZE;


//typedef struct
//{
//    s16 type;
//    s16 fromID;
//    s32 packNo;
//    s32 sizeAll; // All size
//    s16 crc;
//    s16 numData; // HowManyData
//}STSOCKMSGDATA_PACKDATASIZE;


typedef struct
{
    s16 type;
    s16 fromID;
    s32 packNo;
    s32 sizeAll; // All size
    s16 crc;
    s16 numData; // HowManyData
}STSOCKMSGDATA_PACKDATASIZE;
/*---------------------------------------------------------------------------*
 Server Variable
 *---------------------------------------------------------------------------*/
StarVersion     version={2,4,8};
StarVersion     engineSupport={2,86,0};
StarVersion     clientSupport={1,8,0};

static bool     s_bUnlimit = false;
static bool     s_bDebugVersion;
static int      s_nPort;
static int      s_nProtocol;
static int		s_localID;
static int      s_nMaxConnection;
static int      s_nDataBuffer;
static int      s_nSQLBuffer;
static int      s_nDBMaxConnect;
size_t          s_nMemoryUse;
static int      s_nServerMEMG;
static int      s_nMaxRoom;
static int      s_nRoomCount;
static int      s_nMaxStackSize;
static int      s_nMaxFile;


static char		logFileName[128];
static char     startServerTime[64];
static volatile u32      s_nCurLoginCount;
static volatile u32      s_nServerType;

static bool     s_bWriteLog = false;
static size_t   s_EstimateRam;
/*---------------------------------------------------------------------------*
 ROOM Variable
 *---------------------------------------------------------------------------*/
static pthread_mutex_t  s_MutexFreeRoomIdx = PTHREAD_MUTEX_INITIALIZER;

struct SERVER_ROOM_DESC{
    s32 numClient;
    char  roomName[32];
    pthread_mutex_t  mutexRoom;// = PTHREAD_MUTEX_INITIALIZER;
    Client_t *joinRoom[STSV_MAX_PLAYER_PER_ROOM];
};

struct SERVERDESC {
    SOCKET   listenSocket;
    bool     online;
};
pthread_t                   s_AcceptConnectThreadID;
SERVERDESC                  s_ServerDesc;
Client_t                    *s_Client_t;
SERVER_ROOM_DESC            *s_Room_t;


void (*processMSGClientRequest)(Client_t *) = NULL;
void (*processMSGClientDisconnect)(Client_t *) = NULL;
void (*processMSGClientConnect)(Client_t *) = NULL;

/*---------------------------------------------------------------------------*
 Big Data Buffer
 *---------------------------------------------------------------------------*/
struct SERVER_BIG_DATA_BUFFER{
    char*     buffer;
};
static SERVER_BIG_DATA_BUFFER   s_BigDataBuffer[STSV_BIGDATA_POOL_BLOCK];
static bool                     s_BigDataUse[STSV_BIGDATA_POOL_BLOCK];
static bool                     s_bBigDataLock;


#ifdef STAR_HAVE_DB
/*---------------------------------------------------------------------------*
 Database Variable
 *---------------------------------------------------------------------------*/
struct SQLBuffer{
    char *buffer;
};

static MYSQL            *mMySql;//[STNET_MAX_DB_CONNECTION];
static MYSQL*           *pMySql;//[STNET_MAX_DB_CONNECTION];
static MYSQL_RES*       *mMySqlRes;
static MYSQL_ROW        *mMySqlRow;
static pthread_mutex_t  s_MutexFreeDBIdx = PTHREAD_MUTEX_INITIALIZER;      //Faster way for static intilization
static int              *mMySqlNumRow;
static int              *mMySqlNumFields;
SQLBuffer               *mMySqlCommand;


static bool     haveConnectDB = false;
static bool     unlockDB = false;
static bool     *sqlLock;//[STNET_MAX_DB_CONNECTION]; // For Control Connection at same time



#endif
static volatile s32      s_nCurDBConnect;
char            s_szDBUser[16];
char            s_szDBPass[16];
char            s_szDBName[16];
char            s_szIP[32];

#pragma mark -+Group Room use for PVP and other that Player play with Player+
///*---------------------------------------------------------------------------*
//Lobby System Function
//State CREATE->JOIN->Wait for start when join, Start
//
// *---------------------------------------------------------------------------*/
//
//s32  STSV_CreateRoom(Client_t *client); // return room ID if can create room if not return -1
//s32 STSV_JoinFreeRoom(Client_t *client); // return room ID if can create room if not return -1
//bool STSV_JoinRoom(Client_t *client,int roomID);
//int  STSV_GetRoomCount();
//int  STSV_ClearRoom(int roomID);
//int  STSV_ExitRoom(Client_t *client); // return current player in room;
//bool STSV_RoomOwner(Client_t *client);
//int  STSV_GetNumPlayer(int roomID); // return number player join in this room

void STSV_SendConfirmCreateRoom(int idx,int roomID);
void STSV_SendConfirmJoinRoom(int idx,int roomID);
void STSV_SendConfimrExitRoom(int idx,int roomID);

/*---------------------------------------------------------------------------*
 CRC Check Function Interface
 *---------------------------------------------------------------------------*/
#define STAR_CRC8_STANDARD_POLY     0x07
#define STAR_CRC8_STANDARD_INIT     0

#define STAR_CRC16_STANDARD_POLY    0xa001      // Items that carry out bit inversion also invert generator polynomials.
#define STAR_CRC16_STANDARD_INIT    0
#define STAR_CRC16_CCITT_POLY       0x1021
#define STAR_CRC16_CCITT_INIT       0xffff

#define STAR_CRC32_STANDARD_POLY    0xedb88320  // Items that carry out bit inversion also invert generator polynomials.
#define STAR_CRC32_STANDARD_INIT    0xffffffff
#define STAR_CRC32_POSIX_POLY       0x04c11db7
#define STAR_CRC32_POSIX_INIT       0

static u8  s_CRC8Table[256];
static u16 s_CRC16Table[256];
static u32 s_CRC32Table[256];

void _InitCRC8Table(u8 poly);
void _InitCRC16Table(u16 poly);
void _InitCRC32Table(u32 poly);

void    STOS_InitCRCTable(); // Auto Init CRC 8 16 and 32 Table
u8      STOS_CRC8(const void    *data,u32 dataLength);
u16     STOS_CRC16(const void   *data,u16 dataLength);
u32     STOS_CRC32(const void   *data,u32 dataLength);
/*---------------------------------------------------------------------------*
 Server Function Interface
 *---------------------------------------------------------------------------*/
int CreateServer();
int CreateTCPServer(SOCKET listen);
int CreateUDPServer(SOCKET listen);

int GiveLocalIdTCP();


void *STSV_PollAcceptFunc(void  *ptr); // for TCP/TP
void *STSV_KoockAcceptFunc(void *ptr); // for UDP
/*
void *STSV_ClientThreadFuncx(void *ptr);
*/
void *STSV_ClientThreadFuncAlway(void *ptr);

void STSV_TempClientConnectFunc(Client_t *ct);
void STSV_TempClientRequestFunc(Client_t *);

bool CheckLimite();

void AcceptConnection(void);
void AcceptConnectionUDP(void);

int  FindFreeIdxHost();
int  DestroySocket(int sock);
int  DestroyClientRoom(Client_t *);

int  Read(int id,char *buff,size_t count);
int  ReadFrom(SOCKET sock,char *buf,size_t count,struct sockaddr *sockAddr,socklen_t *sockLen,bool NonBlocking);
//int Write(int id,char *buff,size_t count);
int  WriteTo(SOCKET sock,char* buf,size_t count,struct sockaddr *sockAddr,socklen_t sockLen);
void WriteAll(char *buff,size_t count);

/*---------------------------------------------------------------------------*
 Big Data Allocate
 *---------------------------------------------------------------------------*/
int initBigDataMemory();
int GetBigDataBufferIdx();
void FreeBigDataBuffer(int idx);

/*---------------------------------------------------------------------------*
 StarSocket Protocal Function Interface
 *---------------------------------------------------------------------------*/
void SendReTrasmit(int idx);
int  processMessageBuffer(Client_t* client);
void StartPackSendData(Client_t *client);
void EndPackSendData(Client_t *client);
/*---------------------------------------------------------------------------*
 Database Function Interface
 *---------------------------------------------------------------------------*/
#ifdef STAR_HAVE_DB
s32 STDB_Connect();
s32 STDB_DisConnect();
u32 STSV_GetDBConnect(){
    return s_nCurDBConnect;
}
#endif

static void _StartServerLog(void){
    FILE    *logFile;
    time_t	current=time(NULL);
    struct tm  *lotime = localtime(&current);
    
    mkdir("LOG", 0777);
    
    sprintf(logFileName,"LOG/StarSocketServer%d%02d%02d_%02d%02d%02d.log",lotime->tm_year+1900+543,lotime->tm_mon+1,lotime->tm_mday,lotime->tm_hour,lotime->tm_min,lotime->tm_sec);
    
    if((logFile = fopen(logFileName,"w"))!=NULL)
    {
        fprintf(logFile,"Log file Star Server Started %s",ctime(&current));
        fclose(logFile);
    }
    
    sprintf(startServerTime,"%s",ctime(&current));
}
static char* printdigit(size_t digit){
    static char buffer[64];
    
    
#ifdef __LP64__
    if(digit>999999999999)
        sprintf(buffer,"%ld,%03ld,%03ld,%03ld,%03ld",digit/1000000000000,digit%1000000000000/1000000000,digit%1000000000/1000000,digit%1000000/1000,digit%1000);
    else
        if(digit>999999999)
            sprintf(buffer,"%ld,%03ld,%03ld,%03ld",digit/1000000000,digit%1000000000/1000000,digit%1000000/1000,digit%1000);
        else
#endif
            if(digit>999999)
                sprintf(buffer,"%ld,%03ld,%03ld",digit%1000000000/1000000,digit%1000000/1000,digit%1000);
            else if(digit>999)
                sprintf(buffer,"%ld,%03ld",digit/1000,digit%1000);
            else
                sprintf(buffer,"%ld",digit);
    return buffer;
}
static void LogString(const char *string,...){
    
    if(s_bDebugVersion == false) return;
    
    char buff[1024];
    while (s_bWriteLog);
    
    s_bWriteLog = true;
    va_list		ap;
    va_start(ap,string);
    vsprintf(buff,string,ap);
    va_end(ap);
    time_t	current	=	time(NULL);
    
    FILE	*LogFile;
    
    if((LogFile = fopen(logFileName,"a")) !=NULL)
    {
        fprintf(LogFile,"->%s : %s",ctime(&current),buff);
        fprintf(LogFile,"\n");
        fclose(LogFile);
        printf("->%s : %s",ctime(&current),buff);
        printf("\n");
    }
    
    s_bWriteLog = false;
}

/*---------------------------------------------------------------------------*
 Server BigData Implementation
 *---------------------------------------------------------------------------*/
int initBigDataMemory(){
    for (int i=0; i<STSV_BIGDATA_POOL_BLOCK; i++) {
        s_BigDataBuffer[i].buffer = (char*)malloc(STSV_BIGDATA_BUFFER);
        if(s_BigDataBuffer[i].buffer == NULL)
            return -1;
        
        s_BigDataUse[i] = false;
        // s_nMemoryUse += STSV_BIGDATA_BUFFER;
    }
    s_bBigDataLock = false;
    return 0;
}
int GetBigDataBufferIdx(){
    
    while(s_bBigDataLock)
    {
        
    }
    s_bBigDataLock = true;
    for (int i=0; i<STSV_BIGDATA_POOL_BLOCK; i++) {
        if(s_BigDataUse[i] == false)
        {
            s_BigDataUse[i] = true;
            s_bBigDataLock = false;
            return i;
        }
    }
    s_bBigDataLock = false;
    return -1;
}
void FreeBigDataBuffer(int buffidx){
    if(s_BigDataUse[buffidx] == false)
        return;
    
    s_BigDataUse[buffidx] = false;
}

/*---------------------------------------------------------------------------*
 Server Function Implementation
 *---------------------------------------------------------------------------*/
s32     STSV_GetMaxClient(){
    return s_nMaxConnection;
}
size_t     STSV_GetMemoryUse(){
    return s_nMemoryUse;
}

u32     STSV_GetMaxMemoryGB(){
    return s_nServerMEMG;
}
u32     STSV_GetMaxGameServer(){
    return MAX_GAME_SERVER;
}
u32     STSV_GetCurConnection(){
    return s_nCurLoginCount;
}
u32     STSV_GetLoginServerIP(){
    return     htonl(INADDR_ANY);
}
u32     STSV_GetGameServerIP(int gameServerID){
    return htonl(INADDR_ANY);
}
u32     STSV_GetDBServerIP(){
    return htonl(INADDR_ANY);
};

void GetStringValueConfig(const char* configString,const char * tag,char *value){
    const char *firstIdx;
    
    firstIdx = strstr(configString, tag);
    
    if(firstIdx == NULL)
    {
        value[0] ='\0';
        return;
    }
    
    while (*firstIdx != '=') {
        firstIdx++;
        if (*firstIdx =='\0') {
            return;
        }
    }
    firstIdx++; // มาที่ตัวแรกของค่า
    
    bool foundStartIdx = false;
    bool done =false;
    int idxCount =0;
    
    // ถ้าไม่ได้ใส่อะไรเลย
    
    if (*firstIdx == '\n') {
        value[0] = '\0';
        printf("Found \\n return\n");
        return;
    }
    
    
    while (!done) {
        
        if (foundStartIdx) {
            value[idxCount] = *firstIdx;
            if(value[idxCount] == '\'') value[idxCount] = '\0';
            
            idxCount++;
            firstIdx++;
            if (*firstIdx == '\n' || *firstIdx ==' ') {
                value[idxCount] = '\0';
                return;
            }
        }
        else
        {
            if(*firstIdx != ' ' || *firstIdx == '\'')
            {
                foundStartIdx = true;
            }
            else
                firstIdx++;
        }
        
        
    }
    
}

void ReadConfigFile(){
    FILE *file;
    long fileSize;
    char    configBuffer[2048];
    char    valueBuffer[32];
    
    file = fopen("config.sv2", "r");
    if(file)
    {
        // get file size
        fseek(file, 0, SEEK_END);
        fileSize =ftell(file);
        fseek(file, 0, SEEK_SET);
        fread(configBuffer, fileSize, 1, file);
        configBuffer[fileSize] = '\0';
        fclose(file);
        
        GetStringValueConfig(configBuffer, "DEBUG", valueBuffer);
        s_bDebugVersion = atoi(valueBuffer);
        GetStringValueConfig(configBuffer, "PORT", valueBuffer);
        s_nPort = atoi(valueBuffer);
        GetStringValueConfig(configBuffer, "PROTOCOL", valueBuffer);
        s_nProtocol = atoi(valueBuffer);
        GetStringValueConfig(configBuffer, "MAX_CCU", valueBuffer);
        s_nMaxConnection = atoi(valueBuffer);
        GetStringValueConfig(configBuffer, "DATA_BUFFER", valueBuffer);
        s_nDataBuffer = atoi(valueBuffer);
        GetStringValueConfig(configBuffer, "SQL_BUFFER", valueBuffer);
        s_nSQLBuffer = atoi(valueBuffer);
        GetStringValueConfig(configBuffer, "DB_CCU", valueBuffer);
        s_nDBMaxConnect = atoi(valueBuffer);
        GetStringValueConfig(configBuffer, "ALWAYON", valueBuffer);
        s_nServerType = atoi(valueBuffer);
        
        GetStringValueConfig(configBuffer, "SERVERMEM(GB)", valueBuffer);
        s_nServerMEMG = atoi(valueBuffer);
        
        s_nMaxRoom      = 0;//s_nMaxConnection / 2;
        s_nRoomCount    = 0;
        
        // DB Config
        GetStringValueConfig(configBuffer, "DB_USER", valueBuffer);
        strcpy(s_szDBUser,valueBuffer);
        
        GetStringValueConfig(configBuffer, "DB_PASS", valueBuffer);
        strcpy(s_szDBPass,valueBuffer);
        
        
        GetStringValueConfig(configBuffer, "DB_NAME", valueBuffer);
        strcpy(s_szDBName,valueBuffer);
        
        GetStringValueConfig(configBuffer, "DB_IP", valueBuffer);
        strcpy(s_szIP,valueBuffer);
        
    }
    else
    {
        
        // not have config file use default congif
        file = fopen("config.sv2", "w");
        s_bDebugVersion = true;
        s_nPort         = STSV_DEFAULT_PORT;
        s_nProtocol     = STSV_TCP_PROTOCOL;
        s_nMaxConnection= STSV_CLIENT_MAX;
        s_nDataBuffer   = STSV_MAX_BUFFER;
        s_nSQLBuffer    = STSV_MAX_SQL_BUFFER;
        s_nDBMaxConnect = STSV_MAX_DB_CONNECTION;
        s_nServerType   = STSV_ALWAY_ON_CONNECTION;
        s_nServerMEMG   = 2;
        s_nMaxRoom      = 0;//s_nMaxConnection / 2;
        s_nRoomCount    = 0;
        
        strcpy(s_szDBUser,"root");
        strcpy(s_szDBPass,"123456");
        strcpy(s_szDBName,"StarDB");
        strcpy(s_szIP,"127.0.0.1");
        LogString("Nothave \"congif.sv\" Use Default Config");
        
        fprintf(file, "[STAR_SOCKET_SERVER_CONFIG]\n");
        fprintf(file, "DEBUG = 1\n");
        fprintf(file, "PORT  = 9009\n");
        fprintf(file, "PROTOCOL = 0 #TCP = 0,UDP =1\n");
        fprintf(file, "MAX_CCU = 50\n");
        fprintf(file, "DATA_BUFFER = 65536\n");
        fprintf(file, "SQL_BUFFER = 65536\n");
        fprintf(file, "DB_CCU = 10\n");
        fprintf(file, "ALWAYON = 1\n");
        fprintf(file, "SERVERMEM(GB) = 2\n");
        fprintf(file, "[DB_CONFIG]\n");
        fprintf(file, "DB_USER = root\n");
        fprintf(file, "DB_PASS = ''\n");
        fprintf(file, "DB_NAME = StarDB\n");
        fprintf(file, "DB_IP = 127.0.0.1\n");
        
        fclose(file);
    }
}
bool CheckLimite(){
    // Limite will asume you have 2GB for RAM to run this server
    
    char                    logBuffer[1024];
    char                    txBuf[256];
    struct rlimit   limite;
    int limiteLogin;
    
    
    logBuffer[0] = '\0';
    sprintf(txBuf, "\nCheck Limite Hardware Ram %d GB\n",s_nServerMEMG);
    strcat(logBuffer, txBuf);
    
    getrlimit(RLIMIT_STACK,&limite);
    
    if(limite.rlim_cur < (512 * 1024))
    {
        strcat(logBuffer, "StarSocketServer Can't run if stage size < 512K\n");
        LogString(logBuffer);
        return false;
    }
    
    limiteLogin = (s_nServerMEMG * 1024 * 1024 ) / (limite.rlim_cur/1024) ;
    
    if(limiteLogin < s_nMaxConnection)
    {
        sprintf(txBuf, "Stack size %lld Faile please reduce it\n",limite.rlim_cur);
        strcat(logBuffer, txBuf);
        LogString(logBuffer);
        return false;
    }
    else
    {
        //        sprintf(txBuf, "Check Stack size %lldk OK\n",limite.rlim_cur/1024);
        sprintf(txBuf, "Check Stack size %llu k OK\n",limite.rlim_cur/1024);
        strcat(logBuffer, txBuf);
    }
    
    getrlimit(RLIMIT_NOFILE,&limite);
    if (limite.rlim_cur < s_nMaxConnection * 3) {
        sprintf(txBuf, "No File %lld Faile please set to %d it\n",limite.rlim_cur,s_nMaxConnection * 3);
        strcat(logBuffer, txBuf);
        LogString(logBuffer);
        return false;
    }
    else
    {
        //        sprintf(txBuf, "Check File Limite  %lld OK\n",limite.rlim_cur);
        sprintf(txBuf, "Check File Limite  %llu OK\n",limite.rlim_cur);
        strcat(logBuffer, txBuf);
    }
    
    
    LogString(logBuffer);
    return true;
}

void    STSV_SetUnlimitServer(bool unlimit){
    s_bUnlimit = unlimit;
}

s32 InitServerLimit(){
    
    int ret     = 0;
    int i,j;
    
    char                    textBuffer[1024];
    char                    txBuf[256];
    int                     sumMemoryUse;
    
    
    textBuffer[0] = '\0';
    sumMemoryUse = STSV_BIGDATA_POOL_BLOCK * STSV_BIGDATA_BUFFER;
    
    ReadConfigFile();
    LogString("============= StarServer Version %d.%d.%d =============",version.major,version.minor,version.revision);
    if(s_nServerType == STSV_ONETIME_CONNECTION)
        LogString("Server Type Section");
    else
        LogString("Server Type Alway On");
    if(CheckLimite() == false)
        return -1;
    
    
    
    if (s_bDebugVersion) strcat(textBuffer, "\nDEBUG Mode = TRUE\n"); else strcat(textBuffer, "\nDEBUG Mode = TRUE\n");
    // Malloc Client mamory
    s_Client_t = (Client_t *)malloc(sizeof(Client_t) * s_nMaxConnection);
    if(s_Client_t == NULL)
    {
        strcat(textBuffer, "Allocate \"MAX_CONNECT\" Error\n");
        LogString(textBuffer);
        return -1;
    }
    
    memset((char*)s_Client_t, 0, sizeof(Client_t) * s_nMaxConnection);
    
    //Add Version 2.4.2 Mutex init
    for (i=0; i<s_nMaxConnection; i++) {
        pthread_mutex_init(&s_Client_t[i].m_Mutex,NULL);
    }
    
    sprintf(txBuf, "Stack Size = %d\nMAX_CONNECT = %d Ram use %ld OK\n",s_nMaxStackSize,s_nMaxConnection,s_nMaxConnection * sizeof(Client_t));
    sumMemoryUse += s_nMaxConnection * sizeof(Client_t);
    
    strcat(textBuffer, txBuf);
    //Malloc Buffer
    for (i=0; i<s_nMaxConnection; i++) {
        s_Client_t[i].sendBuffer = (u8*)malloc(s_nDataBuffer);
        s_Client_t[i].recvBuffer = (u8*)malloc(s_nDataBuffer);
        // s_Client_t[i].sendBufferTemp = (u8*)malloc(s_nDataBuffer);
        
        if(s_Client_t[i].sendBuffer == NULL || s_Client_t[i].recvBuffer == NULL)
        {
            strcat(textBuffer, "Allocate DATA_BUFFER Error\n");
            LogString(textBuffer);
            return -1;
        }
        
        for (j=0; j<STAR_CLIENT_DEFAULT_MAX_BLOCK-2; j++) {
            s_Client_t[i].sendBufferMSG[j] = (char*)&s_Client_t[i].sendBuffer[(STAR_CLIENT_DEFAULT_BUFFER * 2) + (j*STAR_CLIENT_DEFAULT_BUFFER)];
        }
    }
    
    sprintf(txBuf, "SEND/RECIEVE Buffer Ram use = %d OK\n",s_nMaxConnection*s_nDataBuffer*2);
    strcat(textBuffer, txBuf);
    
    sumMemoryUse += s_nMaxConnection*s_nDataBuffer*2;
    
    s_Room_t = (SERVER_ROOM_DESC *)malloc(sizeof(SERVER_ROOM_DESC) * s_nMaxRoom);
    sprintf(txBuf, "MAX ROOM = %d Ram use %ld OK\n",s_nMaxRoom,s_nMaxRoom * sizeof(SERVER_ROOM_DESC));
    
    for (int i=0; i<s_nMaxRoom;i++) {
        pthread_mutex_init(&s_Room_t[i].mutexRoom,NULL);
    }
    
    sumMemoryUse += s_nMaxConnection * sizeof(Client_t);
    
    
#ifdef STAR_HAVE_DB
    
    
    
    mMySqlCommand = (SQLBuffer*)malloc(s_nDBMaxConnect * sizeof(SQLBuffer));
    if(mMySqlCommand == NULL){
        strcat(textBuffer, "Allocate \"SQL_BUFFER\" Error\n");
        LogString(textBuffer);
        return -1;
    }
    for ( i=0; i<s_nDBMaxConnect; i++) {
        mMySqlCommand[i].buffer = (char*)malloc(s_nSQLBuffer);
        if(mMySqlCommand[i].buffer == NULL){
            strcat(textBuffer, "Allocate \"SQL_BUFFER\" Error\n");
            LogString(textBuffer);
            return -1;
        }
        
    }
    
    sprintf(txBuf, "SQL_COMMAND Buffer Ram use = %lu OK\n",s_nDBMaxConnect*s_nSQLBuffer + (s_nDBMaxConnect * sizeof(SQLBuffer)));
    strcat(textBuffer, txBuf);
    sumMemoryUse += s_nDBMaxConnect*s_nSQLBuffer + (s_nDBMaxConnect * sizeof(SQLBuffer));
    
    mMySql          = (MYSQL *)malloc(sizeof(MYSQL) * s_nDBMaxConnect);
    pMySql          = (MYSQL **)malloc(sizeof(MYSQL*) * s_nDBMaxConnect);
    sqlLock         = (bool *)malloc(sizeof(bool)*s_nDBMaxConnect);
    mMySqlRes       = (MYSQL_RES**) malloc(sizeof(MYSQL_RES*) * s_nDBMaxConnect);
    mMySqlRow       = (MYSQL_ROW*) malloc(sizeof(MYSQL_ROW) * s_nDBMaxConnect);
    mMySqlNumRow    = (int*)malloc(sizeof(int) * s_nDBMaxConnect);
    mMySqlNumFields = (int*)malloc(sizeof(int) * s_nDBMaxConnect);
    
    if(mMySql == NULL || pMySql == NULL || sqlLock == NULL ||
       mMySqlRes == NULL || mMySqlRow == NULL || mMySqlNumRow == NULL || mMySqlNumFields == NULL)
    {
        strcat(textBuffer, "Allocate SQLMEM Error\n");
        LogString(textBuffer);
        return -1;
    }
    
    sprintf(txBuf, "SQL Ram use = %ld OK\n",(sizeof(MYSQL) * s_nDBMaxConnect)+(sizeof(MYSQL*) * s_nDBMaxConnect) + (sizeof(bool)*s_nDBMaxConnect) + (sizeof(MYSQL_RES*) * s_nDBMaxConnect) + (sizeof(MYSQL_ROW) * s_nDBMaxConnect) + (sizeof(int) * s_nDBMaxConnect * 2));
    
    strcat(textBuffer, txBuf);
    
    sumMemoryUse += (sizeof(MYSQL) * s_nDBMaxConnect)+(sizeof(MYSQL*) * s_nDBMaxConnect) + (sizeof(bool)*s_nDBMaxConnect);
    
    for(i=0;i<s_nDBMaxConnect;i++)
    {
        pMySql[i]      = NULL;
        sqlLock[i]     = false;
        mMySqlRes[i]   = NULL;
        mMySqlNumRow[i]= -1;
        mMySqlNumFields[i] = -1;
    }
    
    STDB_Connect();
#endif
    
    s_nMemoryUse += sumMemoryUse;
    for(i=0;i<s_nMaxConnection;i++)
    {
        s_Client_t[i].online = false;
        s_Client_t[i].thread = false;
    }
    
    //========================================================
    // Create Server
    
    ret = CreateServer();
    if(ret != 0 )
    {
        LogString("Create Serve FAIL !!!");
        return -1;
    }
    else
        strcat(textBuffer, "Create Server Done");
    
    
    sprintf(txBuf, "Port %d Server MEM use = %s\n",s_nPort,printdigit(s_nMemoryUse));
    strcat(textBuffer, txBuf);
    LogString(textBuffer);
    
    return 0;
}
s32 InitServerUnlimit(){
    int ret     = 0;
    int i,j;
    
    char                    textBuffer[1024];
    char                    txBuf[256];
    size_t                  sumMemoryUse;
    
    size_t                  memPerUser;
    size_t                  memoryHave;
    
    textBuffer[0] = '\0';
    sumMemoryUse = STSV_BIGDATA_POOL_BLOCK * STSV_BIGDATA_BUFFER;
    
    ReadConfigFile();
    
    
    LogString("============= StarServer Version %d.%d.%d (Unlimit) =============",version.major,version.minor,version.revision);
    
    if(s_nServerType == STSV_ONETIME_CONNECTION)
        LogString("Server Type Section");
    else
        LogString("Server Type Alway On");
    
    //memoryHave = (s_nServerMEMG * 1024 * 1024 * 1024);
    memoryHave = s_nServerMEMG;
    memoryHave = memoryHave * 1024 * 1024 * 1024;
    memPerUser = sizeof(Client_t) + (2 * s_nDataBuffer) + s_nMaxStackSize + (sizeof(SERVER_ROOM_DESC) / 2 );
    
    s_nMaxConnection = (u32)(memoryHave / memPerUser);
    s_nMaxRoom       = s_nMaxConnection / 2;
    //  printf("MemoryHave = %ld MemUser %ld\n",memoryHave,memPerUser);
    //  printf("Max Connect =%ld MaxRoom = %ld\n",s_nMaxConnection,s_nMaxRoom);
    
    if (s_bDebugVersion) strcat(textBuffer, "\nDEBUG Mode = TRUE\n"); else strcat(textBuffer, "\nDEBUG Mode = TRUE\n");
    // Malloc Client mamory
    
    s_Client_t = (Client_t *)malloc(sizeof(Client_t) * s_nMaxConnection);
    
    if(s_Client_t == NULL)
    {
        strcat(textBuffer, "Allocate \"MAX_CONNECT\" Error\n");
        LogString(textBuffer);
        return -1;
    }
    
    memset((char*)s_Client_t, 0, sizeof(Client_t) * s_nMaxConnection);
    
    
    //Add Version 2.4.2 Mutex init
    for (i=0; i<s_nMaxConnection; i++) {
        pthread_mutex_init(&s_Client_t[i].m_Mutex,NULL);
    }
    
    sprintf(txBuf, "Stack Size = %d\nMAX_CONNECT = %d Ram use %ld OK\n",s_nMaxStackSize,s_nMaxConnection,s_nMaxConnection * sizeof(Client_t));
    sumMemoryUse += s_nMaxConnection * sizeof(Client_t);
    
    strcat(textBuffer, txBuf);
    //Malloc Buffer
    for (i=0; i<s_nMaxConnection; i++) {
        s_Client_t[i].sendBuffer = (u8*)malloc(s_nDataBuffer);
        s_Client_t[i].recvBuffer = (u8*)malloc(s_nDataBuffer);
        // s_Client_t[i].sendBufferTemp = (u8*)malloc(s_nDataBuffer);
        
        if(s_Client_t[i].sendBuffer == NULL || s_Client_t[i].recvBuffer == NULL)
        {
            strcat(textBuffer, "Allocate DATA_BUFFER Error\n");
            LogString(textBuffer);
            return -1;
        }
        
        for (j=0; j<STAR_CLIENT_DEFAULT_MAX_BLOCK-2; j++) {
            s_Client_t[i].sendBufferMSG[j] = (char*)&s_Client_t[i].sendBuffer[(STAR_CLIENT_DEFAULT_BUFFER * 2) + (j*STAR_CLIENT_DEFAULT_BUFFER)];
        }
    }
    
    sprintf(txBuf, "SEND/RECIEVE Buffer Ram use = %d OK\n",s_nMaxConnection * s_nDataBuffer* 2);
    strcat(textBuffer, txBuf);
    
    sumMemoryUse += (size_t)s_nMaxConnection* (size_t)s_nDataBuffer*2;
    
    
    s_Room_t = (SERVER_ROOM_DESC *)malloc(sizeof(SERVER_ROOM_DESC) * s_nMaxRoom);
    sprintf(txBuf, "MAX ROOM = %d Ram use %ld OK\n",s_nMaxRoom,s_nMaxRoom * sizeof(SERVER_ROOM_DESC));
    
    for (int i=0; i<s_nMaxRoom;i++) {
        pthread_mutex_init(&s_Room_t[i].mutexRoom,NULL);
    }
    sumMemoryUse += s_nMaxConnection * sizeof(Client_t);
    
    
    
#ifdef STAR_HAVE_DB
    
    
    
    mMySqlCommand = (SQLBuffer*)malloc(s_nDBMaxConnect * sizeof(SQLBuffer));
    if(mMySqlCommand == NULL){
        strcat(textBuffer, "Allocate \"SQL_BUFFER\" Error\n");
        LogString(textBuffer);
        return -1;
    }
    for ( i=0; i<s_nDBMaxConnect; i++) {
        mMySqlCommand[i].buffer = (char*)malloc(s_nSQLBuffer);
        if(mMySqlCommand[i].buffer == NULL){
            strcat(textBuffer, "Allocate \"SQL_BUFFER\" Error\n");
            LogString(textBuffer);
            return -1;
        }
        
    }
    
    sprintf(txBuf, "SQL_COMMAND Buffer Ram use = %lu OK\n",s_nDBMaxConnect*s_nSQLBuffer + (s_nDBMaxConnect * sizeof(SQLBuffer)));
    strcat(textBuffer, txBuf);
    sumMemoryUse += s_nDBMaxConnect*s_nSQLBuffer + (s_nDBMaxConnect * sizeof(SQLBuffer));
    
    mMySql          = (MYSQL *)malloc(sizeof(MYSQL) * s_nDBMaxConnect);
    pMySql          = (MYSQL **)malloc(sizeof(MYSQL*) * s_nDBMaxConnect);
    sqlLock         = (bool *)malloc(sizeof(bool)*s_nDBMaxConnect);
    mMySqlRes       = (MYSQL_RES**) malloc(sizeof(MYSQL_RES*) * s_nDBMaxConnect);
    mMySqlRow       = (MYSQL_ROW*) malloc(sizeof(MYSQL_ROW) * s_nDBMaxConnect);
    mMySqlNumRow    = (int*)malloc(sizeof(int) * s_nDBMaxConnect);
    mMySqlNumFields = (int*)malloc(sizeof(int) * s_nDBMaxConnect);
    
    if(mMySql == NULL || pMySql == NULL || sqlLock == NULL ||
       mMySqlRes == NULL || mMySqlRow == NULL || mMySqlNumRow == NULL || mMySqlNumFields == NULL)
    {
        strcat(textBuffer, "Allocate SQLMEM Error\n");
        LogString(textBuffer);
        return -1;
    }
    
    sprintf(txBuf, "SQL Ram use = %ld OK\n",(sizeof(MYSQL) * s_nDBMaxConnect)+(sizeof(MYSQL*) * s_nDBMaxConnect) + (sizeof(bool)*s_nDBMaxConnect) + (sizeof(MYSQL_RES*) * s_nDBMaxConnect) + (sizeof(MYSQL_ROW) * s_nDBMaxConnect) + (sizeof(int) * s_nDBMaxConnect * 2));
    
    strcat(textBuffer, txBuf);
    
    sumMemoryUse += (sizeof(MYSQL) * s_nDBMaxConnect)+(sizeof(MYSQL*) * s_nDBMaxConnect) + (sizeof(bool)*s_nDBMaxConnect);
    
    for(i=0;i<s_nDBMaxConnect;i++)
    {
        pMySql[i]      = NULL;
        sqlLock[i]     = false;
        mMySqlRes[i]   = NULL;
        mMySqlNumRow[i]= -1;
        mMySqlNumFields[i] = -1;
    }
    
    STDB_Connect();
#endif
    printf("SUM %ld\n",s_nMemoryUse);
    s_nMemoryUse += sumMemoryUse;
    for(i=0;i<s_nMaxConnection;i++)
    {
        s_Client_t[i].online = false;
        s_Client_t[i].thread = false;
    }
    
    //========================================================
    // Create Server
    
    ret = CreateServer();
    if(ret != 0 )
    {
        LogString("Create Serve FAIL !!!");
        return -1;
    }
    else
        strcat(textBuffer, "Create Server Done");
    sprintf(txBuf, "Port %d Server MEM use = %s\n",s_nPort,printdigit(s_nMemoryUse));
    strcat(textBuffer, txBuf);
    LogString(textBuffer);
    
    
    return 0;
}
s32  STSV_Init(){//int port,int protocal,int maxClient)
    
    
    struct rlimit   limite;
    
    s_localID       = STSOCKID_SERVER;
    s_bDebugVersion   = true;
    _StartServerLog();
    STOS_InitCRCTable();
    s_nMemoryUse = 0;
    
    mkdir("TEMP", 0777);
    mkdir("DBFILE", 0777); // ไฟล์ที่ส่งจาก Server หรือ Patch ต่าง ๆ
    mkdir("FILE", 0777);  // ไฟล์ที่รับจาก Client
    
    if(processMSGClientRequest == NULL)
    {
        LogString("STSR_Init Error:Can't Create Server CallbackFunc = NULL\nPlease Call STSV_SetClientRequestFunction\nUse Empty Temp First\n");
        processMSGClientRequest = STSV_TempClientRequestFunc;
    }
    
    if(processMSGClientConnect == NULL)
    {
        LogString("STSR_Init Error:Can't Create Server CallbackFunc = NULL\nPlease Call STSV_SetClientConnectFunction\nUse Empty Temp First\n");
        processMSGClientConnect = STSV_TempClientConnectFunc;
    }
    
    
    //    printf("\n==== Hardware Spacification ====\n");
    //    long numCPU = sysconf(_SC_NPROCESSORS_ONLN);
    //
    //    int mib[2];
    //    long cpuSpeed;
    //    size_t  len = sizeof(cpuSpeed);
    //
    //    mib[0] = CTL_HW;
    //    mib[1] = HW_CPU_FREQ;
    //
    //    sysctl(mib, 2, &cpuSpeed, &len, NULL, 0);
    //
    //    printf("CPU Num = %ld\n",numCPU);
    //    printf("Process Max Child %ld\n",sysconf(_SC_CHILD_MAX));
    //    printf("File Max =%ld\n",sysconf(_SC_OPEN_MAX));
    //    printf("CPU SPeed = %ld\n",sysconf(_SC_CLK_TCK));//cpuSpeed/1000000);
    //    //printf("MEM = %ld  %ld\n",sysconf(_SC_PAGE_SIZE),sysconf(_SC_AVPHYS_PAGES));
    
    
    getrlimit(RLIMIT_STACK,&limite);
    s_nMaxStackSize = (u32)limite.rlim_cur;
    // s_nMaxStackSize *= 1024;
    //  printf("Stack Size = %d fileSize = %d\n",limite.rlim_cur,s_nMaxFile);
    getrlimit(RLIMIT_NOFILE,&limite);
    
    s_nMaxFile = (u32)limite.rlim_cur;
    
    // printf("Stack Size = %d fileSize = %d\n",s_nMaxStackSize,s_nMaxFile);
    
    if(initBigDataMemory() != 0)
    {
        LogString("Server Init Error: Not Enought Memory for BigData\n");
        return -1;
    }
    
    int ret;
    
    if(s_bUnlimit)
        ret = InitServerUnlimit();
    else
        ret = InitServerLimit();
    
    // Calculate All Memory wan to have
    size_t threadRam = s_nMaxStackSize * s_nMaxConnection;
    
    size_t dbRam = (151 * 1024 * 1024) +  (s_nDBMaxConnect * 4); // estimate
    size_t ramUse = threadRam + s_nMemoryUse + dbRam;
    
    s_EstimateRam = ramUse;
    
    LogString("Server Init Success Engine Use %d\nReserved for %d Player = %d\nSum Ram should install %s",
              s_nMemoryUse,s_nMaxConnection,dbRam + threadRam,printdigit(ramUse));
    
    return ret;
}

void     STSV_SetClientSupport(StarVersion  clientVer){
    clientSupport = clientVer;
}

size_t  STSV_GetEstimateRamUse(){
    return s_EstimateRam;
}
int  CreateServer(){
    int ret;
    int error;
    struct sockaddr_in      inetServAddr;
    SOCKET                  sock;
    
    //char                    ipAddrString[32];
    
    switch (s_nProtocol) {
        case STSV_UDP_PROTOCOL:
            sock = socket(AF_INET,SOCK_DGRAM,0);
            break;
        case STSV_TCP_PROTOCOL:
            sock = socket(AF_INET,SOCK_STREAM,0);
            break;
    }
    
    
    memset((char *)&inetServAddr,0,sizeof(inetServAddr));
    inetServAddr.sin_family         =   AF_INET;
    inetServAddr.sin_port           =   htons((u_short)s_nPort);
    inetServAddr.sin_addr.s_addr    =   htonl(INADDR_ANY);
    
    // Get IP Address String;
    //    strcpy(ipAddrString,inet_ntoa(inetServAddr.sin_addr));
    //    sprintf(txBuf,"Socket = %d\nPort = %d\nIP %s\n",sock,s_nPort,ipAddrString);
    //    strcat(textBuffer, txBuf);
    
    // Binding
    error = bind(sock,(struct sockaddr*)&inetServAddr,sizeof(inetServAddr));
    if(error == -1)
    {
        LogString("ERROR : bind() failed at STSV_Init() %d",errno);
        if (errno == EADDRINUSE) {
            LogString("bind() Again");
            int reuse =1;
            setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,(const char*)&reuse,sizeof(int));
            error = bind(sock,(struct sockaddr*)&inetServAddr,sizeof(inetServAddr));
            if(error == -1)
            {
                LogString("ERROR : bind() failed at STSV_Init() %d",errno);
                DestroySocket(sock);
                return -1;
            }
        }
    }
    
    switch (s_nProtocol) {
        case STSV_UDP_PROTOCOL:
            ret = CreateUDPServer(sock);
            break;
        case STSV_TCP_PROTOCOL:
            ret = CreateTCPServer(sock);
            break;
    }
    
    return ret;
}
int  CreateTCPServer(SOCKET listenSock){
    
    int ret;
    
    LogString("CreateUnixTCPServer()");
    
    int error = listen(listenSock,s_nMaxConnection);
    
    if(error ==-1)
    {
        LogString("ERROR : listen() error at CreateUnixTCPServer");
        return 1;
    }
    
    s_ServerDesc.listenSocket = listenSock;
    s_ServerDesc.online       = true;
    // Start Create Thread for Accept
    s_nCurLoginCount          = 0;
    s_nCurDBConnect           = 0;
    
    ret=pthread_create(&s_AcceptConnectThreadID,NULL,
                       STSV_PollAcceptFunc,(void *)&s_ServerDesc);
    
    if(ret!=0)
    {
        LogString("ERROR: pthread_create() fails: %d",ret);
        return -1;
    }
    return 0;
}
int  CreateUDPServer(SOCKET linstenSock){
    int ret;
    s_ServerDesc.listenSocket = linstenSock;
    s_ServerDesc.online       = true;
    s_nCurDBConnect           = 0;
    s_nCurLoginCount          = 0;
    
    ret = pthread_create(&s_AcceptConnectThreadID,NULL, STSV_KoockAcceptFunc,(void *)&s_ServerDesc);
    
    if(ret!=0)
    {
        LogString("ERROR: pthread_create() UDP fails: %d",ret);
        return -1;
    }
    return 0;
}

void STSV_TempClientConnectFunc(Client_t *ct) {
    // need implement
}

void STSV_TempClientRequestFunc(Client_t *ct){
    // Send Empty DataPacket to test
    char *buffer;
    u32 size;
    
    if(STSV_GetDataCount(ct->idx))
    {
        for(int i=0;i<STSV_GetDataCount(ct->idx);i++)
        {
            buffer = STSV_GetData(ct->idx, i, &size);
            //  if(size == 8)
            {
                printf("got Send File Message\n");
                u32 *dataRecv;
                dataRecv = (u32*)buffer;
                printf("Data %d %d %d\n",dataRecv[256],dataRecv[318],dataRecv[128]);
            }
        }
    }
    return;
}

void STSV_SetClientRequestFunction(void (*clientRequestFunc)(Client_t *)) {
    processMSGClientRequest = clientRequestFunc;
}
void STSV_SetClientDisconnectFunction(void (*clientDisconnectFunc)(Client_t *)) {
    processMSGClientDisconnect = clientDisconnectFunc;
}
void STSV_SetClientConnectFunction(void (*clientConnectFunc)(Client_t *)) {
    processMSGClientConnect = clientConnectFunc;
}

int  DestroyClientRoom(Client_t *client){
    if(client->roomIdx >=0)
    {
        int roomIdx = client->roomIdx;
        for (int i=0; i<s_Room_t[roomIdx].numClient; i++) {
            if(s_Room_t[roomIdx].joinRoom[i] == client)
            {
                pthread_mutex_lock(&s_Room_t[roomIdx].mutexRoom);
                s_Room_t[roomIdx].joinRoom[i] = NULL;
                s_Room_t[roomIdx].numClient--;
                pthread_mutex_unlock(&s_Room_t[roomIdx].mutexRoom);
                
                return 0;
            }
        }
    }
    
    return -1;
}
int  DestroySocket(int sock){
    struct linger Ling;
    Ling.l_onoff	=1;
    Ling.l_linger	=0;
    
    setsockopt(sock,SOL_SOCKET,SO_LINGER,(const char*)&Ling,sizeof(struct linger));
    
    shutdown(sock,SHUT_RDWR);
    int ret = -1;
    
    // while(ret ==-1)
    {
        ret =  close(sock);
    }
    LogString("----------Destroy Socket : %d-----%d",sock,ret);
    return 0;
    //    //RemoteHost[NextFreeHostIndex].socket=0;
    //    if (ret==-1)
    
}
s32  STSV_Stop(){
    
    LogString("Shuting down Server");
    int i;
    for(i =0;i<s_nMaxConnection;i++)
    {
        if(s_Client_t[i].online == true)
            DestroySocket(s_Client_t[i].socket);
    }
    
    DestroySocket(s_ServerDesc.listenSocket);
#ifdef STAR_HAVE_DB
    LogString("Shuting down Database");
    STDB_DisConnect();
#endif
    
    //Add Version 2.4.2 Mutex init
    for (i=0; i<s_nMaxConnection; i++) {
        pthread_mutex_destroy(&s_Client_t[i].m_Mutex);
    }
    
    LogString("Shutdown Done");
    
    return 0;
}
int  FindFreeIdxHost(){
    int i;
    for (i=0; i<s_nMaxConnection; i++) {
        if (s_Client_t[i].online == 0) {
            return i;
        }
    }
    
    return -1;
}
int  SetNonBlocking(int idx,u_long setBlocking){
    u_long set = setBlocking;
    s_Client_t[idx].NonBlocking = (bool)setBlocking;
    
    return ioctl(s_Client_t[idx].socket,FIONBIO,&set);
}
void AcceptConnection(void){
    SOCKET				connectSock = STSV_INVALID_SOCKET; // Unix = -1
    struct sockaddr_in	clientAddr;
    socklen_t			clientLen;
    int					freeHostIdx;
    int					ret;
    //    char                ipAddrString[32];
    //Loop as long as the connection has not been accepte
    
    while (connectSock == STSV_INVALID_SOCKET)
    {
        clientLen   = sizeof(clientAddr);
        connectSock = accept(s_ServerDesc.listenSocket,(struct sockaddr*)&clientAddr,&clientLen);
    }
    
    freeHostIdx = FindFreeIdxHost();
    
    if(freeHostIdx < 0)
    {
        LogString("ERROR MAXIMUM CONNECTION > %d",s_nMaxConnection);
        DestroySocket(connectSock);
        return;
    }
    
    if(s_Client_t[freeHostIdx].thread==true)
    {
        // Mean it Re-Use thread
        ret=pthread_join(s_Client_t[freeHostIdx].thread_ID,NULL);
        if(ret!=0)
        {
            LogString("Kill Thread %d Fail %d %d",s_Client_t[freeHostIdx].thread_ID,ret);
        }
        else
        {
            //LogString("Kill Old Thread %d Ok",freeHostIdx);
            s_Client_t[freeHostIdx].thread=false;
        }
    }
    
    //  memset((char*)&s_Client_t[freeHostIdx], 0, sizeof(Client_t));
    
    s_Client_t[freeHostIdx].socket			=	connectSock;
    s_Client_t[freeHostIdx].inetSockAddr	=	clientAddr;
    s_Client_t[freeHostIdx].addrLen         =	clientLen;
    s_Client_t[freeHostIdx].online			=	true;
    s_Client_t[freeHostIdx].idx             =   freeHostIdx;
    s_Client_t[freeHostIdx].packRef         =   0; // reset
    s_Client_t[freeHostIdx].roomIdx         =   -1;
    s_Client_t[freeHostIdx].curPacketNo     =   0;
    s_Client_t[freeHostIdx].fileDataSending =   false;
    s_Client_t[freeHostIdx].bigDataReady    =   false;
    
    SetNonBlocking(freeHostIdx,1);
    char ipAddrString[64];
    strcpy(ipAddrString,inet_ntoa(s_Client_t[freeHostIdx].inetSockAddr.sin_addr));
    
    LogString("Accepted Connection %d on socket %d addrLen %d From IP %s",
              freeHostIdx,s_Client_t[freeHostIdx].socket,s_Client_t[freeHostIdx].addrLen,ipAddrString);
    
    
    STSOCKMSGDATA_DATASIZE   dataSize;
    dataSize.type    = STSOCKMSG_GIVEID;
    dataSize.fromID  = STSOCKID_SERVER;
    dataSize.packNo  = freeHostIdx;
    dataSize.size    = 0;
    
    WriteTo(s_Client_t[freeHostIdx].socket, (char*)&dataSize, sizeof(STSOCKMSGDATA_DATASIZE),(struct sockaddr*)&s_Client_t[freeHostIdx].inetSockAddr,s_Client_t[freeHostIdx].addrLen);
    ret = pthread_create(&s_Client_t[freeHostIdx].thread_ID,NULL,STSV_ClientThreadFuncAlway,(void *)&s_Client_t[freeHostIdx]);
    
    if(ret != 0)
    {
        LogString("Create Thread Fail %d !!!", ret);
        
        s_Client_t[freeHostIdx].online	   = false;
        DestroySocket(s_Client_t[freeHostIdx].socket);
    }
    else
    {
        s_nCurLoginCount++;
        LogString("Create Thread Alway ID=%d",s_Client_t[freeHostIdx].thread_ID);
    }
}
void AcceptConnectionUDP(void){
    int ret;
    
    STSOCKMSGDATA_DATASIZE   dataBuff;
    STSOCKMSG_GENERIC        *Msg;
    int bufferSize  = sizeof(STSOCKMSGDATA_DATASIZE);
    struct sockaddr_in     clientAddr;
    socklen_t clientLen  = sizeof(clientAddr);
    
    
    int ok = 0;
    while (!ok) {
        // Set Non Blocking Server
        u_long set = 0;
        ioctl(s_ServerDesc.listenSocket,FIONBIO,&set);
        
        if(ReadFrom(s_ServerDesc.listenSocket, (char*)&dataBuff, bufferSize, (struct sockaddr*)&clientAddr, &clientLen, 1))
        {
            Msg =  (STSOCKMSG_GENERIC*)&dataBuff;
            
            if (Msg->type == STSOCKMSG_KNOCK) {
                // Accept Connection Here
                
                int freeIdx = FindFreeIdxHost();
                s_Client_t[freeIdx].inetSockAddr = clientAddr;
                s_Client_t[freeIdx].addrLen      = clientLen;
                s_Client_t[freeIdx].online       = true;
                s_Client_t[freeIdx].packRef      = 0; // reset
                
                close(s_ServerDesc.listenSocket);
                
                SOCKET newSocket = socket(AF_INET, SOCK_DGRAM, 0);
                struct sockaddr_in  inetNewServerAddr;
                memset((char*)&inetNewServerAddr, 0, sizeof(inetNewServerAddr));
                inetNewServerAddr.sin_family = AF_INET;
                inetNewServerAddr.sin_port   = htons(0);
                inetNewServerAddr.sin_addr.s_addr = htonl(INADDR_ANY);
                
                int error = bind(newSocket, (struct sockaddr*)&inetNewServerAddr, sizeof(inetNewServerAddr));
                if(error == -1)
                {
                    LogString("Error: bind() UDP fail\n" );
                }
                
                struct sockaddr_in newAddr;
                socklen_t newLen = sizeof(newAddr);
                
                // Get New Local Address
                getsockname(newSocket, (struct sockaddr *)&newAddr, &newLen);
                
                LogString("Creating child to serve socket %d,addrlen %d,ip %s,port %d",newSocket,sizeof(s_Client_t[freeIdx].inetSockAddr),inet_ntoa(s_Client_t[freeIdx].inetSockAddr.sin_addr),ntohs(newAddr.sin_port));
                
                
                s_Client_t[freeIdx].socket = newSocket;
                s_Client_t[freeIdx].idx    = freeIdx;
                ret=pthread_create(&s_Client_t[freeIdx].thread_ID,NULL,STSV_ClientThreadFuncAlway,(void *)&s_Client_t[freeIdx]);
                
                //if cannot create thread destroy socket
                if(ret!=0)
                {
                    LogString("Create Thread Fail UDP %d !!!",ret);
                    //		RemoteHost[freehost].thread    = 0;
                    
                    s_Client_t[freeIdx].online	   = false;
                    DestroySocket(s_Client_t[freeIdx].socket);
                }
                else
                {
                    s_nCurLoginCount++;
                }
                
                ok = 1;
                
            }
            else
                return;
        }
    }
    
    if(ok)
    {
        int error;
        struct sockaddr_in inetServAddr;
        SOCKET sock = socket(AF_INET,SOCK_DGRAM,0);
        
        memset((char *)&inetServAddr,0,sizeof(inetServAddr));
        inetServAddr.sin_family         =   AF_INET;
        inetServAddr.sin_port           =   htons((u_short)s_nPort);
        inetServAddr.sin_addr.s_addr    =   htonl(INADDR_ANY);
        
        // Binding
        error = bind(sock,(struct sockaddr*)&inetServAddr,sizeof(inetServAddr));
        if(error == -1)
        {
            LogString("ERROR : bind() failed at STSV_Init()");
            DestroySocket(sock);
            return;
        }
        
        s_ServerDesc.listenSocket = sock;
    }
}

int	 Read(int id,char *buff,int count){
    int  	bytes_read		=	0;
    int		this_read		=	-1;
    
    //Blocking version
    if(!s_Client_t[id].NonBlocking)
    {
        while(bytes_read<count)
        {
            while(this_read<0)
            {
                
                //check to send use address or socket only
                if(s_Client_t[id].addrLen!=(socklen_t)NULL)
                {
                    this_read =(int) recvfrom(s_Client_t[id].socket,
                                              buff,count - bytes_read,
                                              0,(struct sockaddr *)
                                              &s_Client_t[id].inetSockAddr,
                                              &s_Client_t[id].addrLen);
                }
                else
                {
                    this_read = (int)recv(s_Client_t[id].socket,buff,count-bytes_read,0);
                }
            }
            
            if(this_read == 0)
            {
                return bytes_read;
            }
            
            bytes_read		+=this_read;
            buff			+=this_read;
        }
    }
    else
    {
        if(s_Client_t[id].addrLen !=(socklen_t)NULL)
        {
            this_read=(int)recvfrom(s_Client_t[id].socket,
                                    buff,count-bytes_read, 0,
                                    (struct sockaddr *)&s_Client_t[id].inetSockAddr,
                                    &s_Client_t[id].addrLen);
        }
        else
        {
            this_read = (int)recv(s_Client_t[id].socket,buff,count-bytes_read,0);
        }
        
        if(this_read < 0)
        {
            return	this_read;
        }
        if(this_read == 0)
        {
            return bytes_read;
        }
        
        bytes_read	+= this_read;
        buff		+= this_read;
    }
    return count;
}
int  ReadFrom(SOCKET sock,char *buf,size_t count,struct sockaddr *sockAddr,socklen_t *sockLen,bool NonBlocking){
    int bytes_read   =	 0;
    int	this_read		=	-1;
    
    if(!NonBlocking)
    {
        while(bytes_read < count)
        {
            while(this_read < 0)
            {
                //	this_read = (int)recvfrom(sock,buf,count-bytes_read,0,sockAddr,sockLen); // UDP
                this_read = (int)recv(sock,buf,count-bytes_read,MSG_WAITALL); // TCP
            }
            bytes_read	+= this_read;
            buf		    += this_read;
            
            if(this_read == 0)
            {
                printf("ReadFrom : Discoonect from client\n");
                return 0;
            }
            
            this_read = -1; // reset it
            //printf("this_read = %d %d\n",this_read,count-bytes_read);
        }
        return bytes_read;
    }
    else
    {
        this_read =(int) recvfrom(sock,buf,count - bytes_read, 0,
                                  sockAddr,sockLen);
        
        return this_read;
    }
    
    return count;
}
int  WriteTo(SOCKET sock,char* buf,size_t count,struct sockaddr *sockAddr,socklen_t sockLen){
    int byte_send =  0;
    int this_write   = -1;
    // Send All data
    
    if(s_nProtocol == STSV_TCP_PROTOCOL)
    {
        while (byte_send < count) {
            this_write = (int)send(sock, buf, count-byte_send, 0);
            
            if(this_write == 0)
            {
                // Disconnec
                return this_write;
            }
            else if(this_write >0)
            {
                byte_send += this_write;
                buf       += this_write;
            }
        }
    }
    else
    {
        // UDP Later
        //        while (byte_send < count) {
        //            this_write = (int)sendto(sock, buf, count-byte_send, 0, sockAddr, sockLen);
        //            if(this_write < 0) return this_write;
        //
        //            byte_send += this_write;
        //            buf       += this_write;
        //        }
        
    }
    return byte_send;
}
void WriteAll(char *buff,size_t count){
    size_t		bytes_send	=	0;
    int  		this_write  =   -1;
    
    
    char *temp_buff  = buff;
    
    for(int id=1; id<s_nMaxConnection; id++)
    {
        bytes_send		=	0;
        this_write		=	-1;
        buff = temp_buff;
        
        //if remote host is not online move to next host
        if(!s_Client_t[id].online) continue;
        
        //blocking version
        if(!s_Client_t[id].NonBlocking)
        {
            while(bytes_send < count)
            {
                while(this_write < 0)
                {
                    if(s_Client_t[id].addrLen != (socklen_t)NULL)
                    {
                        this_write =(int) sendto(s_Client_t[id].socket,buff,count-bytes_send , 0,
                                                 (struct sockaddr *)&s_Client_t[id].inetSockAddr,
                                                 s_Client_t[id].addrLen);
                    }
                    else
                    {
                        //this_write = send(RemoteHost[id].socket,buff,count-bytes_send,0);
                        this_write =(int) sendto(s_Client_t[id].socket,buff,count-bytes_send , 0,
                                                 NULL,
                                                 (socklen_t)NULL);
                    }
                    //					this_write = send(RemoteHost[id].socket,buff,count - bytes_send,0);
                }
                
                bytes_send		+= this_write;
                buff			+= this_write;
            }
        }
        else
        {
            if(s_Client_t[id].addrLen != (socklen_t)NULL)
            {
                this_write = (int)sendto(s_Client_t[id].socket,buff,count-bytes_send , 0,
                                         (struct sockaddr *)&s_Client_t[id].inetSockAddr,
                                         s_Client_t[id].addrLen);
            }
            else
            {
                //this_write = send(RemoteHost[id].socket,buff,count-bytes_send,0);
                this_write = (int)sendto(s_Client_t[id].socket,buff,count-bytes_send , 0,
                                         NULL,
                                         (socklen_t)NULL);
            }
            //this_write	=	send(RemoteHost[id].socket,buff,count-bytes_send,0);
            
            bytes_send	+= this_write;
            buff		+= this_write;
        }
    }
};
/*
void *STSV_ClientThreadFunc(void *ptr){
    Client_t    *client;
    int         maxfd;
    
    fd_set      allset;
    
    client = (Client_t *)ptr;
    
    struct timeval tv;
    tv.tv_sec = STSV_IOTIMEOUT;
    tv.tv_usec = 0;
    
    maxfd  = client->socket;
    
    FD_ZERO(&allset);
    FD_SET(client->socket,&allset);
    
    client->thread = true;
    
    client->recieveDone = false;
    client->recieveHeader = false;
    client->sendingData  = false;
    client->sendDone     = false;
    client->sendDataSize = 0;
    //    STAR_PACKET_HEADER   ack;
    //==========================================================================
    // Recieve Data Here
    while (client->recieveDone == false || client->sendDone == false) {
        tv.tv_sec = STSV_IOTIMEOUT; // It want to reset it for linux
        fd_set  reading = allset;
        int	nready		= select(maxfd+1,&reading,NULL,NULL,&tv);
        //        LogString("Recieve Data From %d",client->socket);
        //        printf("nready = %d\n",nready);
        if(nready>0)
        {
            if(FD_ISSET(client->socket, &reading) == true)
            {
                processMSGClientRequest(client);
            }
        }
        else
        {
            break;
        }
        
    }
    
    client->online = false;
    DestroySocket(client->socket);
    s_nCurLoginCount--;
    return (NULL);
    //LogString("- Exiting Thread from Socket: %d -",client->socket);
}
 */
void *STSV_ClientThreadFuncAlway(void *ptr){
    Client_t    *client;
    
    struct pollfd  fds,allset;
    int            nfds;
    int            timeOut;
    
    client = (Client_t *)ptr;
    
    memset(&allset, 0, sizeof(allset));
    nfds = 1;
    allset.fd = client->socket;
    allset.events = POLLIN;
    timeOut  = STSV_IOTIMEOUT * 1000;
    
    client->thread = true;
    
    client->recieveDone = false;
    client->recieveHeader = false;
    client->sendingData  = false;
    client->sendDone     = false;
    client->sendDataSize = 0;
    
    //    STAR_PACKET_HEADER   ack;
    //==========================================================================
    // Recieve Data Here
    
    if (client->online) {
        processMSGClientConnect(client);
    }
    
    while (client->online) {
        timeOut  = STSV_IOTIMEOUT * 1000;
        fds = allset;
        //  printf("Wait Data (%d)\n",client->idx);
        int	nready		= poll(&fds,1,timeOut);
        //  printf("Have Data (%d)\n",client->idx);
        if(nready>0)
        {
            if(processMessageBuffer(client) == 0)
            {
                StartPackSendData(client);
                processMSGClientRequest(client);
                EndPackSendData(client);
            }
        }
        else
        {
            // Discoonet from Server Time out
            printf("Disconnect Time out\n");
            client->online = false;
        }
    }
    
    processMSGClientDisconnect(client);
    DestroySocket(client->socket);
    DestroyClientRoom(client);
    
    s_nCurLoginCount--;
    return (NULL);
}
void *STSV_KoockAcceptFunc(void *ptr){
    
    SERVERDESC	*serverDesc=(SERVERDESC *)ptr;
    
    struct pollfd  fds,allset;
    int            nfds;
    
    
    memset(&allset, 0, sizeof(allset));
    nfds = 1;
    
    
    //Loop while we are online
    while(serverDesc->online)
    {
        allset.fd = serverDesc->listenSocket;
        allset.events = POLLIN;
        fds = allset;
        int	nready		= poll(&fds,1,-1);
        if(nready>0)
        {
            AcceptConnectionUDP();
        }
    }
    return (NULL);
    
}
void *STSV_PollAcceptFunc(void  *ptr){
    int			maxfd;
    fd_set		allset;
    //Cast a pointer
    SERVERDESC	*serverDesc=(SERVERDESC *)ptr;
    
    maxfd	=	serverDesc->listenSocket;
    LogString("TCP Server Thread create Listen on socket %d",maxfd);
    
    //Reset all bits
    FD_ZERO(&allset);
    FD_SET(serverDesc->listenSocket,&allset);
    
    //Loop while we are online
    while(serverDesc->online)
    {
        //LogString("StarServer !!! Wait for Connect !!!");
        fd_set acc		= allset;
        int nready		= select(maxfd+1,&acc,NULL,NULL,NULL);
        
        //check if we should accept
        // On Unix if error it have -1
        // On Windows if error it have SOCKET_ERROR
        if(nready>0)
        {
            // LogString("Have Connect");//"Thread Accept"); // Disable 9/02/2548
            AcceptConnection();
        }
    }
    return (NULL) ;
}

#ifdef USE_LOBBY
#pragma mark -
#pragma mark Join and Create Room
#pragma mark -
/*---------------------------------------------------------------------------*
 StarSocket Protocal Function Implementation Lobby and Join
 *---------------------------------------------------------------------------*/
// Lobby and Join

s32  FindFreeIdxRoom(){
    
    for (int i=0; i<s_nMaxRoom; i++) {
        if(s_Room_t[i].numClient == 0)
            return i;
    }
    
    return  -1;
}

int  STSV_ClearRoom(int roomID){
    memset(&s_Room_t[roomID], 0, sizeof(SERVER_ROOM_DESC));
    return 0;
}

s32  STSV_CreateRoom(Client_t * client){ // return room ID if can create room if not return -1
    if(client->roomIdx >= 0)
    {
        LogString("CREATE ROOM Error client %d already have room %d\n",client->idx,client->roomIdx);
        return -2;
    }
    
    pthread_mutex_lock(&s_MutexFreeRoomIdx);
    int roomFound = FindFreeIdxRoom();
    pthread_mutex_unlock(&s_MutexFreeRoomIdx);
    
    if(roomFound == -1)
    {
        // Full
        return -1;
    }
    
    memset(&s_Room_t[roomFound], 0, sizeof(SERVER_ROOM_DESC));
    
    s_Room_t[roomFound].joinRoom[0] = client;
    s_Room_t[roomFound].numClient = 1;
    client->roomIdx = roomFound;
    s_nRoomCount++;
    return roomFound;
    
}
s32  STSV_JoinFreeRoom(Client_t *client){ // return room ID if can create room if not return -1
    for (int i=0; i<s_nMaxRoom; i++) {
        if(s_Room_t[i].numClient>0 && s_Room_t[i].numClient < STSV_MAX_PLAYER_PER_ROOM)
        {
            s_Room_t[i].joinRoom[s_Room_t[i].numClient] = client;
            s_Room_t[i].numClient++;
            return i;
        }
    }
    
    return -1;
}


bool STSV_JoinRoom(Client_t *client,int roomID){
    if (roomID <0 && roomID >= s_nMaxRoom) {
        LogString("Join Room Error %d",roomID);
        return false;
    }
    
    if(s_Room_t[roomID].numClient == 0)
    {
        // Want to create this room first
        LogString("JOIN ROOM ERROR ROOM EMPTY please create room frist\n");
        return false;
    }
    
    
    if(s_Room_t[roomID].numClient < STSV_MAX_PLAYER_PER_ROOM)
    {
        pthread_mutex_lock(&s_Room_t[roomID].mutexRoom);
        s_Room_t[roomID].joinRoom[s_Room_t[roomID].numClient] = client;
        s_Room_t[roomID].numClient ++;
        client->roomIdx = roomID;
        pthread_mutex_lock(&s_Room_t[roomID].mutexRoom);
        return true;
    }
    else
    {
        LogString("JOIN ROOM ERROR ROOM FULL");
        return false;
    }
}
bool STSV_RoomOwner(Client_t *client){
    if (client->roomIdx <0) {
        return  false;
    }
    
    
    if(s_Room_t[client->roomIdx].joinRoom[0] == client)
        return true;
    
    
    return false;
}
int  STSV_GetNumPlayer(int roomID){ // return number player join in this room
    
    return s_Room_t[roomID].numClient;
    
}
int  STSV_GetRoomCount(){
    return  s_nRoomCount;
}


int  STSV_ExitRoom(Client_t *client){
    int room = client->roomIdx;
    if(room >=0)
    {
        DestroyClientRoom(client);
        return s_Room_t[room].numClient;
    }
    return 0;
}
void STSV_SendConfirmCreateRoom(int idx,int roomID){
    STSOCKMSGDATA_DATASIZE   dataSize;
    dataSize.type   = STSOCKMSG_CONFIRM_CREATE_ROOM;
    dataSize.packNo   = roomID;
    dataSize.fromID = STSOCKID_SERVER;
    dataSize.size   = 0;
    
    if(WriteTo(s_Client_t[idx].socket,(char*)&dataSize,sizeof(STSOCKMSGDATA_DATASIZE),(struct sockaddr *)&s_Client_t[idx].inetSockAddr,s_Client_t[idx].addrLen) == 0)
        s_Client_t[idx].online = false;
}
void STSV_SendConfirmJoinRoom(int idx,int roomID){
    STSOCKMSGDATA_DATASIZE   dataSize;
    dataSize.type   = STSOCKMSG_CONFIRM_JOIN_ROOM;
    dataSize.packNo   = roomID;
    dataSize.fromID = STSOCKID_SERVER;
    dataSize.size   = 0;
    
    if(WriteTo(s_Client_t[idx].socket,(char*)&dataSize,sizeof(STSOCKMSGDATA_DATASIZE),(struct sockaddr *)&s_Client_t[idx].inetSockAddr,s_Client_t[idx].addrLen) == 0)
        s_Client_t[idx].online = false;
}
void STSV_SendConfimrExitRoom(int idx,int roomID){
    STSOCKMSGDATA_DATASIZE   dataSize;
    dataSize.type   = STSOCKMSG_CONFIRM_EXTI_ROOM;
    dataSize.packNo   = roomID;
    dataSize.fromID = STSOCKID_SERVER;
    dataSize.size   = 0;
    
    if(WriteTo(s_Client_t[idx].socket,(char*)&dataSize,sizeof(STSOCKMSGDATA_DATASIZE),(struct sockaddr *)&s_Client_t[idx].inetSockAddr,s_Client_t[idx].addrLen) == 0)
        s_Client_t[idx].online = false;
}
#endif


#pragma mark -
#pragma mark StarSocket Protocol
#pragma mark -
/*---------------------------------------------------------------------------*
 StarSocket Protocal Function Implementation
 *---------------------------------------------------------------------------*/
void SendReTrasmit(int idx,int packetNo){
    
    
    STSOCKMSGDATA_DATASIZE   dataSize;
    dataSize.type   = STSOCKMSG_RETRASMIT;
    dataSize.packNo   = packetNo;
    dataSize.fromID = STSOCKID_SERVER;
    dataSize.size   = 0;
    
    if(WriteTo(s_Client_t[idx].socket,(char*)&dataSize,sizeof(STSOCKMSGDATA_DATASIZE),(struct sockaddr *)&s_Client_t[idx].inetSockAddr,s_Client_t[idx].addrLen) == 0)
        s_Client_t[idx].online = false;
    
}
void SendConfirmMessage(int idx,int packetNo){
    STSOCKMSGDATA_DATASIZE   dataSize;
    dataSize.type   = STSOCKMSG_CONFIRM;
    dataSize.packNo   = packetNo;
    dataSize.fromID = STSOCKID_SERVER;
    dataSize.size   = 0;
    
    if(WriteTo(s_Client_t[idx].socket,(char*)&dataSize,sizeof(STSOCKMSGDATA_DATASIZE),(struct sockaddr *)&s_Client_t[idx].inetSockAddr,s_Client_t[idx].addrLen) == 0)
        s_Client_t[idx].online = false;
}

int _processMessagePacket(Client_t *client){
    
    
    STSOCKMSGDATA_PACKDATASIZE  *PackData;
    u16                         *dataSizeTable;
    char                        *startDataAddr;
    s16                         curPacket = 0;
    u16                         continuePack = 0;
    
    
    PackData = (STSOCKMSGDATA_PACKDATASIZE *)client->recvBuffer;
    
    if (PackData->numData == 0) {
        return 0;
    }
    // Now Data Correct let fill in MSG Buffer
    
    dataSizeTable = (u16 *)&client->recvBuffer[sizeof(STSOCKMSGDATA_PACKDATASIZE)];
    startDataAddr = (char *)&client->recvBuffer[sizeof(STSOCKMSGDATA_PACKDATASIZE) + (2*PackData->numData)];
    
    continuePack = 1;
    curPacket  = -1;
    
    for(int i=0;i<PackData->numData;i++)
    {
        if(continuePack != dataSizeTable[i] >> 11)
        {
            continuePack = 1-continuePack;
            curPacket++;
            client->recvBufferMSG[curPacket] = startDataAddr;
            client->recvBufferMSGSize[curPacket] = 0;
        }
        
        client->recvBufferMSGSize[curPacket] += dataSizeTable[i] & 0x7ff;
        
        continuePack = dataSizeTable[i] >> 11;
        
        startDataAddr+= (dataSizeTable[i] & 0x7ff);
    }
    
    
    client->recvCount = curPacket + 1;//PackData->numData;
    // debug only
    // printf("Recieve NumData = %d Count data = %d\n",PackData->numData,client->recvCount);
    
    return 0;
}
int _ProessMessageBigData(Client_t *client){
    
    STSOCKMSGDATA_PACKDATASIZE  *PackData;
    u16                         packetDataSize = 0;
    size_t                      bigDataPackSize;
    u16                         *dataSizeTable;
    char                        *startDataAddr;
    
    s32                         *headerBigData;
    FILE*                       file = NULL;
    char                        tempFileName[64];
    
    PackData = (STSOCKMSGDATA_PACKDATASIZE *)client->recvBuffer;
    dataSizeTable = (u16 *)&client->recvBuffer[sizeof(STSOCKMSGDATA_PACKDATASIZE)];
    for(int i=0;i<PackData->numData;i++)
    {
        packetDataSize += dataSizeTable[i] & 0x7ff;
    }
    packetDataSize += sizeof(STSOCKMSGDATA_PACKDATASIZE) + (2*PackData->numData);
    startDataAddr = (char*)&client->recvBuffer[packetDataSize];
    headerBigData = (s32*)startDataAddr;
    startDataAddr += 8;
    bigDataPackSize = (PackData->sizeAll -  packetDataSize) + sizeof(STSOCKMSGDATA_DATASIZE) - 8; // delete header 8 byte
    
    sprintf(tempFileName, "TEMP/bigData%08d.tmp",client->idx);
    
    if(headerBigData[0] & STSOCKMSG_BIGDATA_HEADER)
    {
        // write to file first for temp and get back when want to process for save memory
        client->bigDataReady = false;
        client->bigDataReadSize  = headerBigData[1];
        client->bigDataReadSizeDone = 0;
        file = fopen(tempFileName, "wb");
        if(file == NULL)
        {
            LogString("TempFilefor BigData Error can't create file idx %d\n",client->idx);
            return -1;
        }
        else
        {
            
        }
        
    }
    else if( (headerBigData[0] & STSOCKMSG_BIGDATA_CONTINUE) || (headerBigData[0] & STSOCKMSG_BIGDATA_END))
    {
        file = fopen(tempFileName, "ab");
    }
    
    if(file != NULL)
    {
        fwrite(startDataAddr, bigDataPackSize, 1, file);
        client->bigDataReadSizeDone += bigDataPackSize;
        //  printf("Write Big Data = %d %d\n",client->bigDataReadSizeDone,bigDataPackSize);
        
        fclose(file);
    }
    if(headerBigData[0] & STSOCKMSG_BIGDATA_END)
    {
        client->bigDataReady = true;
        // printf("BigData Done size = %d\n",client->bigDataReadSize);
    }
    
    
    return 0;
}
int _ProessMessageFileData(Client_t *client){
    
    STSOCKMSGDATA_PACKDATASIZE  *PackData;
    u16                         packetDataSize = 0;
    size_t                      bigDataPackSize;
    u16                         *dataSizeTable;
    char                        *startDataAddr;
    
    s32                         *headerBigData;
    FILE*                       file = NULL;
    char                        tempFileName[64];
    
    PackData = (STSOCKMSGDATA_PACKDATASIZE *)client->recvBuffer;
    dataSizeTable = (u16 *)&client->recvBuffer[sizeof(STSOCKMSGDATA_PACKDATASIZE)];
    for(int i=0;i<PackData->numData;i++)
    {
        packetDataSize += dataSizeTable[i] & 0x7ff;
    }
    packetDataSize += sizeof(STSOCKMSGDATA_PACKDATASIZE) + (2*PackData->numData);
    startDataAddr = (char*)&client->recvBuffer[packetDataSize];
    headerBigData = (s32*)startDataAddr;
    startDataAddr += 8;
    bigDataPackSize = (PackData->sizeAll -  packetDataSize) + sizeof(STSOCKMSGDATA_DATASIZE) - 8; // delete header 8 byte
    
    
    
    if(headerBigData[0] & STSOCKMSG_BIGDATA_HEADER)
    {
        // write to file first for temp and get back when want to process for save memory
        client->fileDataReady = false;
        client->bigDataReadSize  = headerBigData[1];
        client->bigDataReadSizeDone = 0;
        
        memcpy(client->fileDataName, (char*)&headerBigData[2], 32);
        sprintf(tempFileName, "FILE/file%08d%s",client->idx,client->fileDataName);
        bigDataPackSize -=32;
        startDataAddr += 32;
        file = fopen(tempFileName, "wb");
        if(file == NULL)
        {
            LogString("TempFilefor BigData Error can't create file idx %d\n",client->idx);
            return -1;
        }
        else
        {
            
        }
        
    }
    else if( (headerBigData[0] & STSOCKMSG_BIGDATA_CONTINUE) || ( headerBigData[0] & STSOCKMSG_BIGDATA_END))
    {
        sprintf(tempFileName, "FILE/file%08d%s",client->idx,client->fileDataName);
        file = fopen(tempFileName, "ab");
    }
    
    if(file != NULL)
    {
        fwrite(startDataAddr, bigDataPackSize, 1, file);
        client->bigDataReadSizeDone += bigDataPackSize;
        //   printf("Write File Data = %d %d\n",client->bigDataReadSizeDone,bigDataPackSize);
        
        fclose(file);
    }
    if(headerBigData[0] == STSOCKMSG_BIGDATA_END)
    {
        client->fileDataReady = true;
        //   printf("FileData Done size = %d\n",client->bigDataReadSize);
    }
    
    
    return 0;
}

static void _PackDataWithPacket(Client_t *client){
    
    int i;
    u32 sumSize = 0;
    u16 dataCount = client->sendCount;
    
    // Start Packing Data
    STSOCKMSGDATA_PACKDATASIZE    *packetHeader;
    u16                           *dataSizeTable;
    char                          *startDataAddr;
    u16                           continuePack = 0;
    u8                            curPacket = 0;
    packetHeader            = (STSOCKMSGDATA_PACKDATASIZE  *)client->sendBuffer;  // 16 byte
    dataSizeTable = (u16 *)&client->sendBuffer[sizeof(STSOCKMSGDATA_PACKDATASIZE)]; //
    startDataAddr = (char *)&client->sendBuffer[sizeof(STSOCKMSGDATA_PACKDATASIZE) + (2*dataCount)];
    
    
    packetHeader->type      = STSOCKMSG_PACKDATASIZE;
    packetHeader->packNo    = client->packRef;
    packetHeader->fromID    = STSOCKID_SERVER;
    
    
    for (i=0; i<dataCount; i++) {
        sumSize += client->sendBufferMSGSize[i];
    }
    
    sumSize += (dataCount * 2); // Add Data Size Table
    sumSize += 4; // Add 4 byte extend
    
    packetHeader->sizeAll  = sumSize;
    packetHeader->numData  = dataCount;
    
    //Start Packing Data // will have 2 byter for eachdata
    for(i=0; i<dataCount; i++){
        // นับจากด้านขวามาซ้าย
        //  11 bit แรก เป็นขนาดของ Packet มากสุด 1024
        //  11  bit ถัดมาเป็น การบอกการต่อเนื่องของ Packet เนื่องด้วยมีแค่ 6 bit และ สามารถให้ได้มากสุดแค่ 32 เลยใช่ 1 กับ 0 สลับกันไปเริ่มที่ 0 และสลับไปเป็น  1
        if(curPacket != client->sendPacketNo[i]){
            continuePack = 1 - continuePack; // สลับ 0 กับ 1
        }
        dataSizeTable[i]  = client->sendBufferMSGSize[i] | (continuePack << 11);
        curPacket =  client->sendPacketNo[i];
        
    }
    
    for(i=0; i<dataCount; i++)
    {
        memcpy(startDataAddr,client->sendBufferMSG[i],client->sendBufferMSGSize[i]);
        startDataAddr+=client->sendBufferMSGSize[i];
    }
    
    
}
static void _PackDataWithBigData(Client_t *client){
    STSOCKMSGDATA_PACKDATASIZE    *packetHeader;
    char                          *startDataAddr;
    int                            sizeDataCanSend = 0;
    s32                           *headerAddr;
    packetHeader            = (STSOCKMSGDATA_PACKDATASIZE  *)client->sendBuffer;  // 16 byte
    char      fileName[64];
    FILE        *file;
    
    sprintf(fileName, "DBFILE/%s",client->fileSendName);
    file = fopen(fileName, "rb");
    if(file == NULL)
    {
        return;
    }
    
    //if(client->bigDataSending == true)
    //    packetHeader->type      = packetHeader->type | STSOCKMSG_BIGDATA;
    if(client->fileDataSending == true)
        packetHeader->type      = packetHeader->type | STSOCKMSG_FILEDATA;
    
    startDataAddr           = (char *)&client->sendBuffer[packetHeader->sizeAll + sizeof(STSOCKMSGDATA_DATASIZE)];
    headerAddr              = (s32*)startDataAddr;
    startDataAddr +=8;
    
    
    sizeDataCanSend = (60 << 10) - packetHeader->sizeAll;
    packetHeader->sizeAll += 8;
    
    // have 8 byte header for flag
    if(client->bigDataSendSizeDone == 0){
        // first packet
        printf("Send Header\n");
        headerAddr[0]  = STSOCKMSG_BIGDATA_HEADER;
        headerAddr[1]  = client->bigDataSendSize;
        if(client->fileDataSending == true)
        {
            memcpy((char*)&headerAddr[2],client->fileSendName,32);
            startDataAddr += 32;
            packetHeader->sizeAll += 32;
            //            SLOG("!!! Send File DATA !!! \n");
        }
    }
    else
    {
        headerAddr[0]  = STSOCKMSG_BIGDATA_CONTINUE;
        headerAddr[1]  = client->bigDataSendSize;
    }
    
    if(sizeDataCanSend > client->bigDataSendSize - client->bigDataSendSizeDone)
    {
        fseek(file, client->bigDataSendSizeDone, SEEK_SET);
        fread(startDataAddr, client->bigDataSendSize - client->bigDataSendSizeDone, 1,file);
        fclose(file);
        //        memcpy(startDataAddr, &s_RemoteHost.bigDataBuffer[s_RemoteHost.bigDataSendSize],s_RemoteHost.bigDataSize - s_RemoteHost.bigDataSendSize);
        packetHeader->sizeAll += (client->bigDataSendSize - client->bigDataSendSizeDone);
        client->bigDataSendSizeDone += client->bigDataSendSize - client->bigDataSendSizeDone;
        
        // printf("Big Data Pack End = %d\n",s_RemoteHost.bigDataSize - s_RemoteHost.bigDataSendSize);
    }
    else
    {
        fseek(file, client->bigDataSendSizeDone, SEEK_SET);
        fread(startDataAddr, sizeDataCanSend, 1,file);
        fclose(file);
        // memcpy(startDataAddr, &s_RemoteHost.bigDataBuffer[s_RemoteHost.bigDataSendSize],sizeDataCanSend);
        client->bigDataSendSizeDone += sizeDataCanSend;
        packetHeader->sizeAll += sizeDataCanSend;
        // printf("Big Data Pack = %d\n %d / %d",sizeDataCanSend,s_RemoteHost.bigDataSendSize,s_RemoteHost.bigDataSize);
    }
    
    
    if(client->bigDataSendSizeDone == client->bigDataSendSize)
    {
        //s_RemoteHost.bigDataSending = false;
        client->fileDataSending = false;
        headerAddr[0]  |= STSOCKMSG_BIGDATA_END;
        headerAddr[1]  = client->bigDataSendSize;
        printf("Send End File %d %d\n",client->bigDataSendSizeDone,client->bigDataSendSize);
        
    }
}
static void _PackDataWithCRC16(Client_t *client){
    STSOCKMSGDATA_PACKDATASIZE    *packetHeader;
    packetHeader            = (STSOCKMSGDATA_PACKDATASIZE  *)client->sendBuffer;  // 16 byte
    u16  crc16 = STOS_CRC16(&client->sendBuffer[sizeof(STSOCKMSGDATA_PACKDATASIZE)],packetHeader->sizeAll - 4);
    packetHeader->crc = crc16;
    
    //SLOG("Pack Done CRC = %d size = %d\n",crc16,packetHeader->sizeAll);
}

int   processMessageBuffer(Client_t *client){
    STSOCKMSGDATA_DATASIZE      *DataMsg;
    STSOCKMSG_GENERIC           *Msg;
    
    int                         readSize;
    int                         sendSize;
    int                         ret;
    u16                         crc16,dataCRC16;
    
    client->recvCount = 0;
    
    Msg = (STSOCKMSG_GENERIC*)client->recvBuffer;
    readSize = ReadFrom(client->socket, (char*) client->recvBuffer, sizeof(STSOCKMSGDATA_DATASIZE), (struct sockaddr*)&client->inetSockAddr, &client->addrLen,0);
    
    if(readSize == 0)
    {
        client->online = false;
        return -1;
    }
    
    // Check if not come from true client disconnect immediatly
    
    
    //printf("Process Message %d\n",Msg->type);
    switch (Msg->type) {
        case STSOCKMSG_REGISTER:
            // check version and send back
            DataMsg = (STSOCKMSGDATA_DATASIZE *)client->recvBuffer;
            readSize =ReadFrom(client->socket, (char*) &client->recvBuffer[sizeof(STSOCKMSGDATA_DATASIZE)], DataMsg->size, (struct sockaddr*)&client->inetSockAddr, &client->addrLen,1);
            
            if(readSize != sizeof(StarVersion) * 2)
            {
                client->online = false;
                printf("Read Size error =%d<>%d\n",DataMsg->size,readSize);
                return  -1;
            }
            
            // check Client Version
            StarVersion    *clientVersion;
            StarVersion    *engineVersion;
            
            engineVersion  = (StarVersion*)&client->recvBuffer[sizeof(STSOCKMSGDATA_DATASIZE)];
            clientVersion  = (StarVersion*)&client->recvBuffer[sizeof(STSOCKMSGDATA_DATASIZE)+ sizeof(StarVersion)];
            //printf("\nClient Version = %d %d %d\n",clientVersion->major,clientVersion->minor,clientVersion->revision);
            
            DataMsg = (STSOCKMSGDATA_DATASIZE *)client->sendBuffer;
            
            //#define STSOCKMSG_REGIS_OK          1
            //#define STSOCKMSG_OLD_VERSION       2
            //#define STSOCKMSG_SERVER_FULL       3
            //#define STSOCKMSG_DUPLICATE         4
            
            if(s_nServerType == STSV_ONETIME_CONNECTION)
                DataMsg->fromID = STSOCKMSG_REGIS_OK_SECTION;
            else DataMsg->fromID = STSOCKMSG_REGIS_OK_ALWAY;
            
            
            
            if(engineVersion->major < engineSupport.major)
                DataMsg->fromID = STSOCKMSG_OLD_VERSION;
            else if(engineVersion->minor < engineSupport.minor)
                DataMsg->fromID = STSOCKMSG_OLD_VERSION;
            else if(engineVersion->revision < engineSupport.revision)
                DataMsg->fromID = STSOCKMSG_OLD_VERSION;
            
            
            if(clientVersion->major < clientSupport.major)
                DataMsg->fromID = STSOCKMSG_CLIENT_WRONG;
            else if(clientVersion->minor < clientSupport.minor)
                DataMsg->fromID = STSOCKMSG_CLIENT_WRONG;
            else if(clientVersion->revision < clientSupport.revision)
                DataMsg->fromID = STSOCKMSG_CLIENT_WRONG;
            
            if(s_nCurLoginCount >= s_nMaxConnection - 10)
            {
                DataMsg->fromID = STSOCKMSG_SERVER_FULL;
                printf("Server FULL %d>= %d\n",s_nCurLoginCount,s_nMaxConnection-10);
            }
            
            DataMsg->type = STSOCKMSG_REGISTER;
            
            client->sendDataSize = sizeof(STSOCKMSGDATA_DATASIZE);
            sendSize =  WriteTo(client->socket, (char *)client->sendBuffer, client->sendDataSize, (struct sockaddr *)&client->inetSockAddr, client->addrLen);
            
            if (sendSize == 0) {
                client->online = false;
            }
            
            printf("Register Server Done\n");
            return -1;
            
            break;
        case STSOCKMSG_RETRASMIT:
            // Mean not have Data
            
            sendSize =  WriteTo(client->socket, (char *)client->sendBuffer, client->sendDataSize, (struct sockaddr *)&client->inetSockAddr, client->addrLen);
            
            if (sendSize == 0) {
                client->online = false;
            }
            else
                printf("Client send re trasmit data %d\n",Msg->toNo);
            
            return -1;
            
            
            break;
        case STSOCKMSG_PING:
            DataMsg = DataMsg = (STSOCKMSGDATA_DATASIZE *) client->recvBuffer;
            //  printf("Got Ping MSG\n");
            return -1;
            break;
            
            
        default:// Unknow it should destroy data by recieve all data in socket
            
            // Check for hack
            if(Msg->fromID < 0 || Msg->fromID >= s_nMaxConnection)
            {
                client->online = false;
                return -1;
            }
            
            
            
            if( (Msg->type & STSOCKMSG_ALLDATA) )
            {
                
                DataMsg = (STSOCKMSGDATA_DATASIZE *)client->recvBuffer;
                //  printf("Got ALL DATA SIZE = %d\n",DataMsg->size);
                
                if(DataMsg->size < 0 || DataMsg->size > STSV_MAX_BUFFER)
                {  // Hack System
                    client->online =false;
                    return  -1;
                }
                
                readSize =ReadFrom(client->socket, (char*) &client->recvBuffer[sizeof(STSOCKMSGDATA_DATASIZE)], DataMsg->size, (struct sockaddr*)&client->inetSockAddr, &client->addrLen,0); // Change 1 to 0 on 2.3.5
                
                if (DataMsg->packNo <= client->packRef) {
                    printf(" Old Packet %d ignore it cur = %d\n",DataMsg->packNo,client->packRef);
                    
                    return -1;
                }
                else if(DataMsg->packNo == client->packRef){
                    //printf("Same Packet %d %d it done check only send ack again\n",DataMsg->packNo,client->packRef);
                }
                //            else
                //                printf("Got Packet %d\n",DataMsg->packNo);
                
                // Add Server Type One Time Connect
                if (s_nServerType == STSV_ONETIME_CONNECTION ) {
                    if (client->sendDone &&
                        ( (Msg->type & STSOCKMSG_BIGDATA) == 0) &&
                        ( (Msg->type & STSOCKMSG_FILEDATA) == 0)) {
                        client->online = false;
                        return -1;
                        // Server will disconnect from client automatic
                        // Send Disconnect Message
                    }
                }
                
                
                if(readSize != DataMsg->size)// Lost Data Retrasmit
                {
                    SendReTrasmit(client->idx,DataMsg->packNo);
                    printf("Data Size Not Correct %d<>%d\n",readSize,DataMsg->size);
                    return -1;
                }
                else {
                    // printf("Read Data Size = %d\n",readSize);
                }
                // CRC Check
                // Get All Data Check CRC First if error re-trasnmit again
                // Calculate CRC
                // if(s_nProtocol == STSV_UDP_PROTOCOL) // Alway check CRC for HACK
                
                crc16 = STOS_CRC16(&client->recvBuffer[sizeof(STSOCKMSGDATA_PACKDATASIZE)],DataMsg->size-4);
                
                dataCRC16 = *(u16*)&client->recvBuffer[sizeof(STSOCKMSGDATA_DATASIZE)];
                // printf("CRC %d = %d\n",crc16,dataCRC16);
                
                if(crc16 != dataCRC16)
                {
                    SendReTrasmit(client->idx,DataMsg->packNo);
                    printf("ProcessMSGError CRC not correct %d <> %d size = %d\n",crc16,dataCRC16,DataMsg->size);
                    return -1;
                }
                //=================================================================
                // Start Encode Packet
                
                if(Msg->type  & STSOCKMSG_PACKDATASIZE)
                {
                    ret = _processMessagePacket(client);
                    if(ret != 0)
                        return ret;
                }
                
                if(Msg->type & STSOCKMSG_BIGDATA)
                {
                    printf("Got Big Data (%d) ",DataMsg->packNo);
                    ret = _ProessMessageBigData(client);
                    if (ret != 0) {
                        printf("Big Data Packet Error\n");
                        return  ret;
                    }
                }
                
                if(Msg->type & STSOCKMSG_FILEDATA) {
                    ret = _ProessMessageFileData(client);
                    if (ret != 0) {
                        return  ret;
                    }
                }
                
                // Remove Send Confrim To Merch with Send Data And End Send Package Data
                //SendConfirmMessage(client->idx, DataMsg->packNo);
                
                client->packRef = DataMsg->packNo;
            }
            else {
                // ไม่ทราบที่มาที่ไปของ packet
                // SendReTrasmit(client->idx,client->packRef);
                return -1;
            }
            break;
    }
    
    return 0;
}

void  StartPackSendData(Client_t *client) {
    //client->sendCount = 0; // Remove it and Reset when end Send then Can Send Anytime for this package
    client->streamSize = 0;
    
    // Can send only 63 packet
    
}
void  EndPackSendData(Client_t *client) { // will send data here
    
    STSOCKMSGDATA_PACKDATASIZE  *PackData;
    
    //    u16                           *dataSizeTable;
    //    char                          *startDataAddr;
    //    u16                           continuePack = 0;
    //    u8                            curPacket = 0;
    //
    //    u32  sumSize = 0;
    //    u16  dataCount = client->sendCount;
    //    int  i;
    if(client->sendCount > 0 || client->fileDataSending == true) {
        pthread_mutex_lock(&client->m_Mutex);
        
        PackData = (STSOCKMSGDATA_PACKDATASIZE  *)client->sendBuffer;
        
        //if(client->sendCount > 0)  // FIX 2.4.7
        _PackDataWithPacket(client);
        
        if (client->fileDataSending) {
            //printf("Sending Pack Data %d\n",client->packRef);
            _PackDataWithBigData(client);
        }
        
        _PackDataWithCRC16(client);
        
        // size_t  sendsize = sumSize + sizeof(STSOCKMSGDATA_DATASIZE);
        size_t sendsize = PackData->sizeAll + sizeof(STSOCKMSGDATA_DATASIZE);
        client->sendDataSize  = sendsize;
        
        PackData->packNo = client->packRef;
        
        sendsize = WriteTo(client->socket, (char *)client->sendBuffer, client->sendDataSize , (struct sockaddr *)&client->inetSockAddr, client->addrLen);
        if(sendsize <= 0) {
            client->online = false;
        }
        
        client->recvCount   = 0;
        client->sendCount   = 0;
        client->curPacketNo = 0;
        
        pthread_mutex_unlock(&client->m_Mutex);
    }
    else {
        // ถ้าไม่มี Data จะส่ง confirm ไปเลย แต่ถ้ามีจะ เอา confirm พวงไปด้วย
        SendConfirmMessage(client->idx, client->packRef);
        
        if(s_nServerType == STSV_ONETIME_CONNECTION)
            client->sendDone = true;
    }
}
void  STSV_SendDuplicateLogin(Client_t *client) {
    pthread_mutex_lock(&client->m_Mutex);
    
    STSOCKMSGDATA_DATASIZE   dataSize;
    dataSize.type   = STSOCKMSG_DUPLICATE;
    dataSize.fromID = STSOCKID_SERVER;
    dataSize.size   = 0;
    
    WriteTo(client->socket,(char*)&dataSize,sizeof(STSOCKMSGDATA_DATASIZE),(struct sockaddr *)&client->inetSockAddr,client->addrLen);
    client->online = false;
    
    pthread_mutex_unlock(&client->m_Mutex);
}

void  STSV_SendServerFULL(Client_t *client) {
    pthread_mutex_lock(&client->m_Mutex);
    
    STSOCKMSGDATA_DATASIZE   dataSize;
    dataSize.type   = STSOCKMSG_SERVER_FULL;
    dataSize.fromID = STSOCKID_SERVER;
    dataSize.size   = 0;
    
    WriteTo(client->socket,(char*)&dataSize,sizeof(STSOCKMSGDATA_DATASIZE),(struct sockaddr *)&client->inetSockAddr,client->addrLen);
    client->online = false;
    
    pthread_mutex_unlock(&client->m_Mutex);
}


void  STSV_SendData(Client_t *client,int sockID,const void *buffer,u32 size) {
    
    pthread_mutex_lock(&client->m_Mutex);
    
    if(client->sendCount < STAR_CLIENT_DEFAULT_MAX_BLOCK-2) {
        u32 fillSize = size;
        char*  bufferSend = (char*)buffer;
        while (fillSize) {
            if(fillSize <= STAR_CLIENT_DEFAULT_BUFFER ) {
                memcpy(client->sendBufferMSG[client->sendCount],bufferSend,fillSize);
                client->sendBufferMSGSize[client->sendCount] = fillSize;
                client->sendPacketNo[client->sendCount] = client->curPacketNo;
                client->sendCount++;
                
                fillSize -= fillSize;
                bufferSend += fillSize;
            }
            else {
                memcpy(client->sendBufferMSG[client->sendCount],bufferSend,STAR_CLIENT_DEFAULT_BUFFER);
                client->sendBufferMSGSize[client->sendCount] = STAR_CLIENT_DEFAULT_BUFFER;
                client->sendPacketNo[client->sendCount] = client->curPacketNo;
                client->sendCount++;
                
                fillSize -= STAR_CLIENT_DEFAULT_BUFFER;
                bufferSend += STAR_CLIENT_DEFAULT_BUFFER;
            }
            
            if(client->sendCount >= STAR_CLIENT_DEFAULT_MAX_BLOCK-2) {
                LogString("\nERROR SendSize %d are Over Buffer Full Stop Filling(%d)\n",size,client->sendCount);
                break;
            }
        }
        
        client->curPacketNo++;
    }
    else {
        LogString("STSV_SendData Error:MAXIMUM\n");
    }
    
    pthread_mutex_unlock(&client->m_Mutex);
    
}
void  STSV_SendDBFile(Client_t *client,const char *fileName) {
    
    if(client->fileDataSending == true) {
        printf("File Data Sending ignore it\n");
        return;
    }
    
    sprintf(client->fileSendName,"DBFILE/%s",fileName);
    FILE *file = fopen(client->fileSendName, "rb");
    if(file) {
        fseek(file, 0, SEEK_END);
        client->bigDataSendSize = (s32)ftell(file);
        client->bigDataSendSizeDone = 0;
        fclose(file);
        client->fileDataSending = true;
        sprintf(client->fileSendName,"%s",fileName);
    }
    else {
        LogString("STSV_SendDBFIle Error Can't open %s\n",fileName);
        client->fileDataSending = false;
        return;
    }
    
}

char* STSV_GetData(int idx,int dataIdx,u32 *dataSize) {
    *dataSize = s_Client_t[idx].recvBufferMSGSize[dataIdx];
    return s_Client_t[idx].recvBufferMSG[dataIdx];
}
int   STSV_GetDataCount(int idx) {
    return s_Client_t[idx].recvCount;
}

bool  STSV_HaveBigData(int idx) {
    return s_Client_t[idx].bigDataReady;
}

char* STSV_GetBigData(int idx) {
    if(s_Client_t[idx].bigDataReady == false)
        return NULL;
    FILE*  file;
    char   tempFileName[64];
    
    int buffIdx = GetBigDataBufferIdx();
    if(buffIdx == -1) {
        LogString("STSV_GetBigData Outoff Memory\n");
        return NULL;
    }
    s_Client_t[idx].bigDataBuffer = s_BigDataBuffer[buffIdx].buffer;
    
    sprintf(tempFileName, "TEMP/bigData%08d.tmp",idx);
    file = fopen(tempFileName, "rb");
    
    if (file) {
        fread(s_Client_t[idx].bigDataBuffer, s_Client_t[idx].bigDataReadSize, 1, file);
        fclose(file);
    }
    else {
        LogString("STSV_GetBigData Temp File %d LOST\n",idx);
        return NULL;
    }
    
    
    return s_BigDataBuffer[buffIdx].buffer;
    
}

void  STSV_FreeBigData(int idx){
    if(s_Client_t[idx].bigDataBuffer != NULL) {
        for (int i=0; i<STSV_BIGDATA_POOL_BLOCK; i++) {
            if(s_Client_t[idx].bigDataBuffer == s_BigDataBuffer[i].buffer) {
                FreeBigDataBuffer(i);
                s_Client_t[idx].bigDataBuffer = NULL;
                break;
            }
        }
        
    }
    
    s_Client_t[idx].bigDataBuffer = NULL;
    s_Client_t[idx].bigDataReady = false;
    
}

char* STSV_GetFileDataName(int idx) {
    return s_Client_t[idx].fileDataName;
}
bool  STSV_HaveFileData(int idx) {
    return s_Client_t[idx].fileDataReady;
}
void  STSV_FreeFileData(int idx) {
    s_Client_t[idx].fileDataReady = false;
}


#ifdef STAR_HAVE_DB
#pragma mark -
#pragma mark Database
#pragma mark -
/*---------------------------------------------------------------------------*
 Database Function Implementation
 *---------------------------------------------------------------------------*/
int    STSV_DBGetMaxConnection() {
    return s_nDBMaxConnect;
}
int    STSV_DBGetCurConnection() {
    return s_nCurDBConnect;
}

s32 STDB_Connect() {
    
    s32 cCount = 0;
    
    for(int i=0;i<s_nDBMaxConnect;i++) {
        mysql_init(&mMySql[i]);
        mysql_options(&mMySql[i],MYSQL_SET_CHARSET_NAME,"utf8");
        
        pMySql[i] = mysql_real_connect(&mMySql[i], s_szIP, s_szDBUser, s_szDBPass, s_szDBName, 0, NULL, 0);
        if(pMySql[i] != 0) {
            sqlLock[i] = false;
            cCount++;
            haveConnectDB = true;
        }
        else {
            LogString("Connect Database %s at Connect No. %d  Error",s_szDBName,i+1);
        }
    }
    
    return cCount;
}

s32 STDB_DisConnect() {
    for(int i=0;i<s_nDBMaxConnect;i++) {
        if(mMySqlRes[i] != NULL) {
            STSV_DBReleaseResult(i);
            mMySqlRes[i] = NULL;
        }
        
        if(pMySql[i])
            mysql_close(&mMySql[i]);
    }
    return 0;
}

s32 STDB_ReConnect(int idx) {
    LogString("STDB_Query ReConnect %d\n",idx);
    mysql_init(&mMySql[idx]);
    mysql_options(&mMySql[idx],MYSQL_SET_CHARSET_NAME,"utf8");
    pMySql[idx] = mysql_real_connect(&mMySql[idx], s_szIP, s_szDBUser, s_szDBPass, s_szDBName, 0, NULL, 0);
    
    return (pMySql[idx] > 0) ? 1 : 0;
}

bool STSV_DBHaveConnect() {
    return haveConnectDB;
}

void STSV_DBUnlockResource(int idx) {
    unlockDB = true;
    if(sqlLock[idx] == true) {
        s_nCurDBConnect--;
        if (s_nCurDBConnect < 0) {
            LogString("ERROR CODE UnlockDB CurDB = %d\n", s_nCurDBConnect);
            s_nCurDBConnect = 0;
        }
        sqlLock[idx] = false;
    }
    else {
        LogString("ERROR CODE STSV_UnlockResourec %d Already Unlock",idx);
    }
    
    unlockDB = false;
}

s32 STSV_DBFindAndLockResource() {
    
    int i=0;
    int foundIdx = -1;
    
    pthread_mutex_lock(&s_MutexFreeDBIdx);
    
    while (foundIdx == -1) {
        if(sqlLock[i] == false && pMySql[i] != NULL)
            foundIdx = i;
        else {
            i++;
            if(i >= s_nDBMaxConnect) {
                LogString("Free DB WARRNING FULL Wait Again Idx = %d\n", foundIdx);
                i = 0;
            }
        }
    }
    
    s_nCurDBConnect++;
    sqlLock[foundIdx] = true;
    
    pthread_mutex_unlock(&s_MutexFreeDBIdx);
    return foundIdx;
}

u32 STSV_DBLastInsert(int idx) {
    u32 ret;
    
    if(mMySqlRes[idx] != NULL) {
        STSV_DBReleaseResult(idx);
    }
    
    if(sqlLock[idx] == false) {
        LogString("STSV_LastInsert ERROR: resource %d not lock",idx);
        return false;
    }
    
    ret = mysql_insert_id(&mMySql[idx]);
    
    return ret;
}

bool STSV_DBQuery(int idx,const char* sqlCommand,...){
    int ret;
    int errorNo;
    
    if(mMySqlRes[idx] != NULL)
        STSV_DBReleaseResult(idx);
    
    if(sqlLock[idx] == false) {
        LogString("STSV_Query ERROR: resource %d not lock",idx);
        return false;
    }
    
    va_list		ap;
    va_start(ap,sqlCommand);
    vsprintf(mMySqlCommand[idx].buffer,sqlCommand,ap);
    va_end(ap);
    
    ret = mysql_query(&mMySql[idx],mMySqlCommand[idx].buffer);
    
    if(ret != 0) {
        if(ret == CR_SERVER_GONE_ERROR) {
            LogString("STDB_Query Error %s %s\n",mysql_error(&mMySql[idx]),mMySqlCommand[idx].buffer);
            STDB_ReConnect(idx);
            
            ret = mysql_query(&mMySql[idx],mMySqlCommand[idx].buffer);
            if(ret == 0)
                LogString("Re Connect OK\n");
        }
        else if(ret == CR_OUT_OF_MEMORY) {
            LogString("STDB_Query Error NO MEM %s %s",mysql_error(&mMySql[idx]),mMySqlCommand[idx].buffer);
            return false;
        }
        else {
            LogString("STDB_Query Error Other %d ret = %d %s %s",idx,mysql_errno(&mMySql[idx]),mysql_error(&mMySql[idx]),mMySqlCommand[idx].buffer);
            errorNo = mysql_errno(&mMySql[idx]);
            
            if(errorNo == 1146 ||  // No Data
               errorNo == 1064 ||  // SQL Error
               errorNo == 1062 )   // Duplicate Key
            {
                return false;
            }
            else {
                if(STDB_ReConnect(idx) > 0) {
                    
                    ret = mysql_query(&mMySql[idx], mMySqlCommand[idx].buffer);
                    if(ret != 0) {
                        LogString("Re Connect OK but STDB_Query %d Still Fail",idx);
                        return false;
                    }
                }
                else {
                    LogString("Re Connect Fail %s %s\n", mysql_error(&mMySql[idx]), mMySqlCommand[idx].buffer);
                    return false;
                }
            }
        }
    }
    
    mMySqlRes[idx] = mysql_store_result(&mMySql[idx]);
    if (mMySqlRes[idx] != NULL) {
        mMySqlNumRow[idx] = (int)mysql_num_rows(mMySqlRes[idx]);
        mMySqlNumFields[idx] = mysql_num_fields(mMySqlRes[idx]);
    }
    return true;
}

bool STSV_RealQuery(int idx, const char* sqlCommand, ...){
    int ret;
    
    if(mMySqlRes[idx] != NULL)
        STSV_DBReleaseResult(idx);
    
    if(sqlLock[idx] == false) {
        LogString("STSV_RealQuery ERROR: resource %d not lock",idx);
        return false;
    }
    
    va_list		ap;
    va_start(ap,sqlCommand);
    vsprintf(mMySqlCommand[idx].buffer,sqlCommand,ap);
    va_end(ap);
    
    long commandSize = strlen(mMySqlCommand[idx].buffer);
    ret = mysql_real_query(&mMySql[idx],mMySqlCommand[idx].buffer,commandSize);
    
    if(ret != 0) {
        if(ret == CR_SERVER_GONE_ERROR) {
            STDB_ReConnect(idx);
            ret = mysql_real_query(&mMySql[idx],mMySqlCommand[idx].buffer,commandSize);
            
        }
        else {
            LogString("STDB RealQuery Error %s %s\n",mysql_error(&mMySql[idx]),mMySqlCommand[idx].buffer);
            if(STDB_ReConnect(idx) > 0) {
                ret = mysql_real_query(&mMySql[idx],mMySqlCommand[idx].buffer,commandSize);
                if(ret != 0) {
                    LogString("Re Connect OK but STDB_RealQuery %d Still Fail",idx);
                    return false;
                }
            }
            else {
                LogString("Re Connect ERROR STDB_RealQuery %d",idx);
                return false;
            }
        }
    }
    
    mMySqlRes[idx] = mysql_store_result(&mMySql[idx]);
    if(mMySqlRes[idx]) {
        mMySqlNumRow[idx] = (int)mysql_num_rows(mMySqlRes[idx]);
        mMySqlNumFields[idx] = mysql_num_fields(mMySqlRes[idx]);
    }
    return true;
    
}

bool STSV_DBSaveFile(char *fileName, void* buffer, int size){
    char filePath[128];
    sprintf(filePath, "DBFILE/%s", filePath);
    FILE *file;
    file = fopen(filePath, "wb");
    if(file) {
        fwrite(buffer, size, 1, file);
        fclose(file);
        return  true;
    }
    else
        return false;
    
}

int  STSV_DBNumRow(int idx){
    if(sqlLock[idx] == false) {
        LogString("STSV_DBNumRow ERROR: resource %d not lock", idx);
        return false;
    }
    
    return mMySqlNumRow[idx];
}
int  STSV_DBNumColum(int idx) {
    if(sqlLock[idx] == false) {
        LogString("STSV_DBNumColum ERROR: resource %d not lock", idx);
        return false;
    }
    return mMySqlNumFields[idx];
}
bool STSV_DBFetchRow(int idx) {
    if(sqlLock[idx] == false) {
        LogString("STSV_DBFetchRow ERROR: resource %d not lock", idx);
        return false;
    }
    
    if(mMySqlRes[idx] == NULL)
        return false;
    
    mMySqlRow[idx] = mysql_fetch_row(mMySqlRes[idx]);
    if(mMySqlRow[idx] != NULL)
        return true;
    else
        return false;
}

int STSV_DBGetRowIntValue(int idx, int rowIdx) {
    if(sqlLock[idx] == false) {
        LogString("STSV_DBGetRowIntValue ERROR: resource %d not lock", idx);
        return -1;
    }
    if(rowIdx > mMySqlNumFields[idx]) {
        LogString("STSV_DBGetRowIntValue ERROR :resource %d rowIdx %d > %d", idx,rowIdx, mMySqlNumFields[idx]);
        return -1;
    }
    char *value = mMySqlRow[idx][rowIdx];
    int intValue = atoi(value);
    
    return intValue;
}
char* STSV_DBGetRowStringValue(int idx,int rowIdx) {
    static char charZero[2] = "0";
    
    if(sqlLock[idx] == false) {
        LogString("STSV_DBGetRowStringValue ERROR: resource %d not lock", idx);
        return NULL;
    }
    if(rowIdx > mMySqlNumFields[idx]) {
        LogString("STSV_DBGetRowStringValue ERROR :resource %d rowIdx %d > %d", idx, rowIdx, mMySqlNumFields[idx]);
        return NULL;
    }
    char *value = mMySqlRow[idx][rowIdx];
    if(value == NULL)
        return charZero;
    
    return value;
}
void STSV_DBReleaseResult(int idx) {
    if(mMySqlRes[idx] != NULL) {
        mysql_free_result(mMySqlRes[idx]);
        mMySqlRes[idx] = NULL;
        mMySqlNumFields[idx] = -1;
        mMySqlNumRow[idx] = -1;
    }
    else {
        LogString("ERROR CODE:Can't release mMySqlRes(%d) == NULL", idx);
    }
}

#endif

#pragma mark -
#pragma mark CRC
#pragma mark -

void _InitCRC8Table(u8 poly) {
    u32     r;
    u32     i, j;
    u8     *t = s_CRC8Table;
    
    for (i = 0; i < 256; i++) {
        r = i;
        for (j = 0; j < 8; j++) {
            if (r & 0x80) {
                r = (r << 1) ^ poly;
            }
            else {
                r <<= 1;
            }
        }
        t[i] = (u8)r;
    }
}

void _InitCRC16Table(u16 poly) {
    u32     r;
    u32     i, j;
    u16    *t = s_CRC16Table;
    
    for (i = 0; i < 256; i++) {
        r = i << 8;
        for (j = 0; j < 8; j++) {
            if (r & 0x8000) {
                r = (r << 1) ^ poly;
            }
            else {
                r <<= 1;
            }
        }
        t[i] = (u16)r;
    }
}

void _InitCRC32Table(u32 poly) {
    u32     r;
    u32     i, j;
    u32    *t = s_CRC32Table;
    
    for (i = 0; i < 256; i++) {
        r = i << 24;
        for (j = 0; j < 8; j++) {
            if (r & 0x80000000U) {
                r = (r << 1) ^ poly;
            }
            else {
                r <<= 1;
            }
        }
        t[i] = r;
    }
}

void STOS_InitCRCTable() {
    _InitCRC8Table(STAR_CRC8_STANDARD_POLY);
    _InitCRC16Table(STAR_CRC16_STANDARD_POLY);
    _InitCRC32Table(STAR_CRC32_STANDARD_POLY);
}

u8 STOS_CRC8(const void    *input,u32 dataLength) {
    u8 context = STAR_CRC8_STANDARD_INIT;
    u32     r;
    u32     i;
    const u8 *t = s_CRC8Table;
    u8     *data = (u8 *)input;
    
    r = (u32)context;
    for (i = 0; i < dataLength; i++) {
        r = t[(r ^ *data) & 0xff];
        data++;
    }
    context = (u8)r;
    return context;
}

u16 STOS_CRC16(const void   *input,u16 dataLength) {
    u16     context = STAR_CRC16_STANDARD_INIT;
    u32     r;
    u32     i;
    const u16 *t = s_CRC16Table;
    u8     *data = (u8 *)input;
    
    r = (u32)context;
    for (i = 0; i < dataLength; i++) {
        r = (r << 8) ^ t[((r >> 8) ^ *data) & 0xff];
        data++;
    }
    context = (u16) r;
    
    return context;
}

u32 STOS_CRC32(const void   *input,u32 dataLength){
    u32     context = STAR_CRC32_STANDARD_INIT;
    u32     r;
    u32     i;
    const u32 *t = s_CRC32Table;
    u8     *data = (u8 *)input;
    
    r = (u32)context;
    for (i = 0; i < dataLength; i++) {
        r = (r << 8) ^ t[((r >> 24) ^ *data) & 0xff];
        data++;
    }
    context = (u32) r;
    return context;
}