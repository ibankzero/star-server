//
//  StarServer.h
//
//  This programe include Server and Database file
//
//
//  Created by SAMRET WAJANASATHIAN on 6/4/2556.
//  Copyright 2013 3nd Studio Co.,Ltd All rights reserved.
//

// Note 20/04/2556
//   Add Hour and Min and Log file

#define STAR_HAVE_DB
#define DEBUG_MODE

#ifndef _STAR_SERVER_H_
#define _STAR_SERVER_H_

#ifdef DEBUG_MODE
    #define SLOG(...) printf(__VA_ARGS__)
#else
    #define SLOG(...) ((void)0)
#endif

#include <memory.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <termios.h>
#include <sys/select.h>
#include <sys/poll.h>
#include "string.h"

#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>

#ifdef STAR_HAVE_DB
#include <mysql.h>
#include <errmsg.h>
#endif

#include<netinet/in.h>

/*
 // version 2.4.8 FIX
 //    ReadFrom  if send from client more then 512 will bug
 // version 2.4.7 Fix UnlockDB use mutex
 // version 2.4.6 02/11/14  USE With 2.82 only
 //  Use new protocal to send in same package not separate
 // version 2.4.5 14/10/14
 // Fix block mutex when package data for package lost
 // version 2.4.4 03/10/14
 //  FIX IMPORTANT BUG  death lock with STSV_DBFindAndLockResource()
 // version 2.4.3 08/09/14
 //  Add Server Full Massage
 //  Add Duplicate Login check Call Function
 //     void STSV_SendServerFULL(Client_t *client)
 //     void STSV_SendDuplicateLogin(Client_t *clietn);
 // version 2.4.2
 //  Add Mutex check เพื่อไม่ให้ package สูญหาย
 // version 2.4.1
 //  Add Callback function processMSGDestroy สำหรับเมื่อ client หลุดออกจากระบบ
 //  void    STSV_SetDestroyFunction(void (*destroyFunc)(struct Client_t *));
 // version 2.4.0
 //  Fix การส่งค่าใน Room ทั้งหมด
 //  เพิ่มระบบ package การสร้างห้องต่าง ๆ
 //  เพิ่ม Recod Package สำหรับ boardcash room เพื่อถ่ายทอดสด
 //  สร้าง Mutex สำหรับทุกอย่างที่ while ค้างไว้
 // version 2.3.9
 // FIX STSV_DBQuery
 //      Add errorNo for query error
 // version 2.3.8
 // Fix for DOS ATTACK reject
 // Fix Check Limit Error Not working
 // version 2.3.7
 //   Fix Recv data more then 1 packet can use with only StarEngine 2.65
 // version 2.3.5
 //  Change ReadFrom จาก NonBlocking เป็น Blocking
 // version 2.3.4  28/MARCH/2014 15:45
 //  Fix config.sv สามารถอ่านค่า NULL ได้โดยใส่ '' และ สตริงจะใส่ '' หรือ ไม่ใส่ก็ได้
 // version 2.3.3  23/MAR/2014 10:18
 //  ปรับแก้การส่งข้อมูลที่ แบนวิทต่ำ
 //  Fix memset 0 เมื่อจองหน่วนความจำให้ Client_t
 //  Fix Sum for memory more then 4GB
 //  Fix printdigit support 64 bit
 //  Fix ถ้าส่ง File ไปแล้วรับได้ไม่หมด แล้ว connection ใหม่จะพังทันที
 // version 2.3.2  10/MAR/2014 11:25
 //  Add
 //     char* STSV_GetFileDataName(int idx);
 //     bool  STSV_HaveFileData(int idx);
 //     void STSV_SendBigData(Client_t *client,int sockID,const void *buffer,u32 size);
 //     void STSV_SendDBFile(Client_t *client,const char *fileName);
 //     bool   STSV_DBSaveFile(char *fileName,void* buffer,int size);
 //     All temp file will stay on TEMP directory
 //     Move all Log file to LOG directory
 //     recieve file will store on FILE directory
 //     db file will store on DBFILE directory
 //
 //  Fix DB Error when initial
 //  FIX  STSV_DBQuery ไม่ได้คำนวน numrow และ numfield
 //       STSV_DBQuery คำสั่ง insert จะไม่สามารถหา numrow และ numfield (segment fail)
 //  Modify Support
 //       Packet Data ที่มีขนาดมากกว่า 1024 byte แต่ต้องไม่มากกว่า 60K
 //       Big Data ที่มีขนาดไม่จำกัด แต้ต้องระวังเรื่องหน่วยความจำให้ดี
 // version 2.3.1  18/FEB/2014 20:38
 //  Change Type and From id from s32 to s16
 //  Add
 //   void    STSV_SetUnlimiteServer(bool unlimit);
 //   u32     STSV_GetMaxMemoryGB();
 // version 2.3.0
 //  Add
 //  int  STSV_ExitRoom(Client_t *client); return จำนวนคนที่เหลืออยู่ในห้อง
 //  ownRoom variable at Client_t เพื่อตรวจว่าเป็นเจ้าของห้องหรือเปล่า
 // version 2.1.2
 //   Fix STSV_SendData(Client_t *client,int sockID,const void *buffer,u32 size); sent to player still send to all player room
 // version 2.1.1
 //   change void  STSV_SendData(int idx,const void *buffer,u32 size); to
 //   void STSV_SendData(Client_t *client,int sockID,const void *buffer,u32 size);
 
 //   void  STSV_SendData(int idx,const void *buffer,u32 size);
 // version 2.1.0
 //  s32  STSV_CreateRoom(Client_t *client); // return room ID if can create room if not return -1
 //   bool STSV_JoinRoom(Client_t *client,int roomID);
 //   int  STSV_GetRoomCount();
 // Version 2.0.0
 //  This version not compotible with older version please don't use it
 //  Add to support StarSocket Protocal
 //  Add to support TCP/UDP Protocal
 //  Add Support Path with more speed donwload
 //  Fix support Database don't call directly
 //  change version to 3 digit
 
 // VERSION 0.07
 //  Add Cur DB Connection
 // VERSION 0.06
 //  Optimize Database function to thread-self
 //
 //
 //
 // VERSION 0.05
 //  Build in Database Class to Client_t
 //
 // VERSION 0.04
 //  Fix
 //  s32     STDB_Connect(const char* user,const char* pass,const char* DBName);
 //  Add IP Address parameter
 // VERSION 0.03
 //  Add Internal Protocal for detect server still running
 //  Add Server Type (Alway on or OneTime Connection
 // VERSION 0.02
 //  Remove Non ERROR LOG
 */

#if defined(__cplusplus)
extern "C"
{
#endif
    
// Server Type
#define STSV_ALWAY_ON_CONNECTION            0x01
#define STSV_ONETIME_CONNECTION             0x02
    
#define SSNET_ACK                           0xFF000000
#define SSNET_NEXT_DATA                     0xFF010000
#define SSNET_END_DATA                      0xFF020000
#define SSNET_ERROR                         0xFF030000
    
#define SSNET_SERVER_CHECK                  0xFF040000  // For Server Checker
#define SSNET_SERVER_OK                     0xFF050000  // For Server Checker
    
#define SSNET_MAX_PACKET_SIZE               512
#define SSNET_MESSAGE_BUFFER                4
#define STSV_IOTIMEOUT                      15  //  second
    
#define STAR_CLIENT_DEFAULT_BUFFER          1024
#define STAR_CLIENT_DEFAULT_MAX_BLOCK       64
    
#define STSOCKID_PLAYER                     -7
#define STSOCKID_ALLPARTY                   -6
#define STSOCKID_ALLFRIEND                  -5
#define STSOCKID_ALLROOM                    -4  // only in same room
#define STSOCKID_UNKNOWS                    -3
#define STSOCKID_ALL                        -2
#define STSOCKID_SERVER                     -1
    
#ifdef __LP64__
    // 64-bit code
    typedef unsigned char   u8;
    typedef char            s8;
    typedef unsigned short  u16;
    typedef short           s16;
    typedef unsigned int    u32;
    typedef int             s32;
    typedef float           f32;
#else
    // 32-bit code
    typedef unsigned char   u8;
    typedef char            s8;
    typedef unsigned short  u16;
    typedef short           s16;
    typedef unsigned long   u32;
    typedef long            s32;
    typedef float           f32;
#endif
    
    
    typedef int SOCKET;
    
    struct StarVersion {
        s32 major;		///< significant changes
        s32 minor;		///< incremental changes
        s32 revision;	///< bug fixes
    };
    
    
#ifndef TRUE
#define TRUE 1
#endif
    
#ifndef FALSE
#define FALSE 0
#endif
    
    typedef int HOSTID;
    //system message
    struct STAR_PACKET_HEADER {
        u32 packNo;
        u32 packType;
        u32 packSize;
        u32 password;
    };
    typedef struct {
        SOCKET                  socket;
        sockaddr_in             inetSockAddr;
        socklen_t               addrLen;
        bool                    online;
        bool                    thread; // Have Thread Before;
        u16                     idx; // clientID;
        
        s16                     roomIdx;// -1 not have
        
        bool                    sendingData;  // flag for tell server to send data
        size_t                  sendDataSize; // All Data include header
        bool                    sendDone; // USE IN ONE TIME CONNECT if SEND DONE
        bool                    sendHeader;
        size_t                  sendSize;
        size_t                  byte_send;
        
        bool                    recieveDone;
        bool                    recieveHeader;
        STAR_PACKET_HEADER      *header;
        size_t                  recv_size;
        size_t                  byte_read;
        
        pthread_t               thread_ID;
        bool                    NonBlocking;
        // Buffer For Read,Write,and SQL
        u8                       *recvBuffer;
        u8                       *sendBuffer;
        //u8                       *sendBufferTemp;
        /*---------------------------------------------------------------------------*
         Star Socket Protocal Variable
         *---------------------------------------------------------------------------*/
        u32                      packRef; // refernet counting
        char*                    streamDataPtr;
        s32                      streamSize;
        // for packet data
        s16                      sendCount; // จำนวน Block ที่ใช้ส่งข้อมูล
        s16                      recvCount;  // จำนวน package ที่ไ้ดรับมาต่อ หนึ่ง การส่ง
        u16                      curPacketNo; // Packet ที่เท่าไหร่ใน pack send หลังส่งเสร็จจะนับ 0 ใหม่
        u8                       sendPacketNo[STAR_CLIENT_DEFAULT_MAX_BLOCK];
        // for big data && fileName Recieve
        char*                    bigDataBuffer; // เอามาจาก buffer pool ไม่ได้จองหน่วยความจำ
        u32                      bigDataReadSize;
        u32                      bigDataReadSizeDone;
        bool                     bigDataReady; // มี BigData หรือเปล่า หลังจากเอาค่าไปแล้วควรลบออกด้วยเพื่อประหยัดหน่วยความจำ
        
        bool                     fileDataReady;
        char                     fileDataName[32];
        // File Sending
        char                     fileSendName[32];
        bool                     fileDataSending;
        //  bool                     bigDataSending;
        
        s32                      bigDataSendSize;
        u32                      bigDataSendSizeDone;
        
        
        
        
        char* sendBufferMSG[STAR_CLIENT_DEFAULT_MAX_BLOCK];// This will pointer to sendBufferTemp
        char* recvBufferMSG[STAR_CLIENT_DEFAULT_MAX_BLOCK];//this will pointer to data address not copy
        
        int   sendBufferMSGSize[STAR_CLIENT_DEFAULT_MAX_BLOCK];
        int   sendMSGType;
        int   recvBufferMSGSize[STAR_CLIENT_DEFAULT_MAX_BLOCK];
        
        pthread_mutex_t        m_Mutex;
    }Client_t;
    struct STAR_ROOM_LIST{
        int               clientIdx;
        STAR_ROOM_LIST    *next;
    };
    
#define STNET_INVALID_PROTOCOL		-2
    
    void    STSV_SetUnlimitServer(bool unlimit); // default = false; call before Init
    s32     STSV_Init();//int port,int protocal,int maxClient);
    void    STSV_SetClientSupport(StarVersion  clientVersion);
    size_t  STSV_GetEstimateRamUse();
    s32     STSV_Stop();
    
    void    STSV_SetClientRequestFunction(void (*clientRequestFunc)(Client_t *));
    void    STSV_SetClientConnectFunction(void (*clientConnectFunc)(Client_t *));
    void    STSV_SetClientDisconnectFunction(void (*clientDisconnectFunc)(Client_t *));
    
    s32     STSV_GetMaxClient();
    size_t  STSV_GetMemoryUse();
    u32     STSV_GetMaxMemoryGB();
    
    u32     STSV_GetDBConnect();
    u32     STSV_GetMaxGameServer();
    u32     STSV_GetCurConnection();
    
    //u32     STSR_GetLoginServerIP();
    //u32     STSR_GetGameServerIP(int gameServerID); // Not implement
    //u32     STSR_GetDBServerIP(); // Not Impelment
    
    //void  STSV_SendData(int idx,const void *buffer,u32 size); // remove on 2.1.1
    void STSV_SendData(Client_t *client,int sockID,const void *buffer,u32 size);
    void STSV_SendDBFile(Client_t *client,const char *fileName);
    void STSV_SendDuplicateLogin(Client_t *clietn);
    void STSV_SendServerFULL(Client_t *client);
    //void STSV_SendBigData(Client_t *client,int sockID,const void *buffer,u32 size); // Can't use now it take more memory
    
    
    void  STSV_SendStreamData(int idx,const void *buffer,u32 size);
    // Packet Data
    char* STSV_GetData(int idx,int dataIdx,u32 *size); // client_t idx
    int   STSV_GetDataCount(int idx); // client_t idx
    
    // BigData
    char* STSV_GetBigData(int idx); // client_t idx
    void  STSV_FreeBigData(int idx);// !!! สำคัญมาก ถ้าเอาไปแล้วควร ฟรี ทิ้งซะ ไม่งั้นตาย
    bool  STSV_HaveBigData(int idx); // client_t idx
    // FileData
    
    char* STSV_GetFileDataName(int idx);
    bool  STSV_HaveFileData(int idx);
    void  STSV_FreeFileData(int idx);
  
#ifdef USE_LOBBY
#pragma mark -+Group Room use for PVP and other that Player play with Player+
    /*---------------------------------------------------------------------------*
     Lobby System Function
     State CREATE->JOIN->Wait for start when join, Start
     
     *---------------------------------------------------------------------------*/
    
    s32  STSV_CreateRoom(Client_t *client); // return room ID if can create room if not return -1
    s32 STSV_JoinFreeRoom(Client_t *client); // return room ID if can create room if not return -1
    bool STSV_JoinRoom(Client_t *client,int roomID);
    int  STSV_GetRoomCount();
    int  STSV_ClearRoom(int roomID);
    int  STSV_ExitRoom(Client_t *client); // return current player in room;
    bool STSV_RoomOwner(Client_t *client);
    int  STSV_GetNumPlayer(int roomID); // return number player join in this room
#endif
    
#ifdef STAR_HAVE_DB
    s32  STSV_DBFindAndLockResource();// After return resource idx it will lock that resource
    
    int    STSV_DBGetMaxConnection();
    int    STSV_DBGetCurConnection();
    
    bool   STSV_DBHaveConnect();
    u32    STSV_DBLastInsert(int idx);
    bool   STSV_DBQuery(int idx,const char* sqlCommand,...);
    bool   STSV_DBRealQuery(int idx,const char* sqlCommand,...);
    bool   STSV_DBSaveFile(char *fileName,void* buffer,int size);
    
    int    STSV_DBNumRow(int idx);
    int    STSV_DBNumColum(int idx);
    bool   STSV_DBFetchRow(int idx);
    
    int    STSV_DBGetRowIntValue(int idx,int rowIdx);
    char*  STSV_DBGetRowStringValue(int idx,int rowIdx);
    
    
    void    STSV_DBReleaseResult(int idx);
    void    STSV_DBUnlockResource(int idx);
    
#endif
    
    u32 STOS_GetTickCount();
    long GetTimeMillisec();
    
    unsigned char  getch();
    int kbhit();
    
    // Class are not thread self
    //class StarDatabase
    //{
    //private:
    //    MYSQL           m_MySql;
    //    char            m_User[16];
    //    char            m_Pass[16];
    //    char            m_DBName[16];
    //    char            m_IP[32];
    //public:
    //    StarDatabase();
    //    ~StarDatabase();
    //    s32  Connect(const char* ip,const char* user,const char* pass,const char* DBName);
    //    MYSQL_RES* Query(const char* sqlCommand);
    //    MYSQL_RES* RealQuery(const char *sqlCommand,int commandSize);
    //    void  ReleaseResult(MYSQL_RES *res);
    //private:
    //    s32 ReConnect();
    //};
    
    
    
    
    
    
    //Error Code
    
#if defined(__cplusplus)
}
#endif
#endif




