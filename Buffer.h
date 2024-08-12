/* 
 * File:   Buffer.h
 * Author: MM2
 *
 * Created on 11 janvier 2022, 18:31
 */

#ifndef BUFFER_H
#define	BUFFER_H

#define MAX_OBDBUF_TIMESTAMP 500

#define NB_OBD_BUF 20

#define TX_BUFFER 0
#define RX_BUFFER 1

typedef enum
{
    BUFFER_TYPE_UNDEF = 0,
    BUFFER_TYPE_VT_TOOL, //multiframe to/from VT
    BUFFER_TYPE_CAN, //long frame for CAN , usefull for SGW
    BUFFER_TYPE_DOIP, //frame for DoIP
    BUFFER_TYPE_WIFI, // for the next wifi chip if one a future day
    BUFFER_TYPE_MAX,
}BUFFER_TYPE;

typedef struct 
{
    DWORD dwTimeStamp;
    WORD wIdx;
    WORD wBufSize;
    struct{
        WORD wUnused:4;
        WORD wBufIdx:5;
        WORD wBufType:4; //BUFFER_TYPE
        WORD wInUsed : 1;
        WORD wType : 1;// RX ou TX
        WORD wFDorNot : 1;
    }bF;
#if defined _DoIP
	IP IpAddres;
#endif	
    UCHAR ucFrameIdx;
    UCHAR *pBuf;
}OBD_BUF;

void ObdBufInit(void);
OBD_BUF *GetBufByType(UCHAR ucBufType, UCHAR ucRxTx);
OBD_BUF *GetBufByIndex(UCHAR ucBufIdx);
OBD_BUF *GetNewBuffer(WORD wBufSize, UCHAR ucTxRx, UCHAR ucBufType);
void UpdateObdBuffer(void);
void FreeObdBufByIdx(UCHAR ucIdx);

//===============================================================
//=== Buffer RAM memory engine
//===============================================================
#define MAX_RAM_BLK 32
#define RAM_SIZE 256
#define LAST_RAM_BLK MAX_RAM_BLK

#define FREE_STATE	0xFF

extern UCHAR * OsAllocMem(WORD wSize);
extern void  OsFreeMem(UCHAR * pBuf);
extern void MemoryPoolInit(void);

#endif	/* BUFFER_H */

