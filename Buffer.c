

#include <xc.h>
#include <string.h>
#include "../Common_Header/NewType.h"
#if defined _DoIP
#include "DoIP/Ethernet.h"
#endif
#include "Buffer.h"
#include "Horodating.h"

static OBD_BUF ObdBuffTab[NB_OBD_BUF];
static UCHAR *NullPoint = NULL;


//==================================================================
//=========================== FUNCTIONS ============================
//==================================================================

void FreeObdBufByIdx(UCHAR ucIdx)
{
    ObdBuffTab[ucIdx].bF.wInUsed = 0;
    ObdBuffTab[ucIdx].dwTimeStamp = 0;

    if(ObdBuffTab[ucIdx].pBuf != NULL)
    {
        OsFreeMem(ObdBuffTab[ucIdx].pBuf);
		ObdBuffTab[ucIdx].pBuf = NullPoint;
    }
}

void ObdBufInit(void)
{
UCHAR i = 0;

    MemoryPoolInit();

    for(i=0;i<NB_OBD_BUF;i++)
    {
        ObdBuffTab[i].pBuf = NullPoint;
        ObdBuffTab[i].bF.wBufIdx = i;
        FreeObdBufByIdx(i);
    }
}

OBD_BUF *GetBufByType(UCHAR ucBufType, UCHAR ucRxTx)
{
OBD_BUF *pBuf = NULL;
UCHAR i = 0;
UCHAR ucfound = 0;

    while(ucfound ==0 && i<= NB_OBD_BUF)
    {   
        if((ObdBuffTab[i].bF.wBufType == ucBufType) && (ObdBuffTab[i].bF.wType == ucRxTx))
        {
            pBuf = &ObdBuffTab[i];
            ucfound = 1;
        }
        else
        {
            i++;
        }
    }

return pBuf;
}


OBD_BUF *GetBufByIndex(UCHAR ucBufIdx)
{
OBD_BUF *pBuf = NULL;
UCHAR i = 0;
UCHAR ucfound = 0;

    while(ucfound ==0 && i<= NB_OBD_BUF)
    {   
        if(ObdBuffTab[i].bF.wBufIdx == ucBufIdx)
        {
            pBuf = &ObdBuffTab[i];
            ucfound = 1;
        }
        else
        {
            i++;
        }
    }

return pBuf;
}

OBD_BUF *GetNewBuffer(WORD wBufSize, UCHAR ucTxRx, UCHAR ucBufType)
{
OBD_BUF *pNewBuf = NULL;
UCHAR i = 0;
UCHAR ucfound = 0;

    if(pNewBuf == NULL)
    {
        while(ucfound ==0 && i<= NB_OBD_BUF)
        {
            if(ObdBuffTab[i].bF.wInUsed == 0)
            {
                ucfound = 1;
                pNewBuf = &ObdBuffTab[i];
            }
            else
            {
                if(ObdBuffTab[i].bF.wInUsed == 1 && TimeStampDiff(ObdBuffTab[i].dwTimeStamp) > MAX_OBDBUF_TIMESTAMP)
                {
                    ucfound = 1;
                    pNewBuf = &ObdBuffTab[i];
                }
                i++;
            }

        }        
    }
    
    pNewBuf->bF.wInUsed = 1;
    pNewBuf->wBufSize = wBufSize;
    pNewBuf->ucFrameIdx = 0;
    pNewBuf->wIdx = 0;
    if(pNewBuf->pBuf != NULL)
    {
        OsFreeMem(pNewBuf->pBuf);
    }
    pNewBuf->pBuf = OsAllocMem(wBufSize);
    pNewBuf->bF.wType = ucTxRx;
    pNewBuf->bF.wBufType = ucBufType;
    pNewBuf->dwTimeStamp = GetTimeStamp();

return pNewBuf;
}

void UpdateObdBuffer(void)
{
    
UCHAR i = 0;
    
    for(i=0;i<NB_OBD_BUF;i++)
    {
        if(ObdBuffTab[i].bF.wInUsed == 1 && TimeStampDiff(ObdBuffTab[i].dwTimeStamp) > MAX_OBDBUF_TIMESTAMP)
        {
            FreeObdBufByIdx(i);
        }
    }
}


//==========================================================================================
//================== EDS memory pool management engine =====================================
//==========================================================================================
static UCHAR	arAllocTable[MAX_RAM_BLK] __attribute__((page));
static UCHAR	arMemoryPool[MAX_RAM_BLK * RAM_SIZE] __attribute__((page));

//==========================================================================================
//==========================================================================================
//==========================================================================================
void MemoryPoolInit(void)
{
	memset(arAllocTable,FREE_STATE,MAX_RAM_BLK);
}

//==========================================================================================
//==========================================================================================
UCHAR * OsAllocMem(WORD wSize)
{
WORD wNblk = (wSize +(RAM_SIZE -1))/ RAM_SIZE;
WORD i;
WORD wFound = 0;
WORD wFirst = 0;
UCHAR * pBuf = NullPoint;

    memset((BYTE *)&pBuf,0,sizeof(pBuf));

	for (i = 0; (i != MAX_RAM_BLK) && (wFound < wNblk); i++)
	{
		if(arAllocTable[i] == FREE_STATE)
		{
			if (wFound == 0) wFirst = i;
			wFound ++;
		}
		else
			wFound = 0;
	}

	if(wFound == wNblk)
	{
		pBuf = &arMemoryPool[wFirst*RAM_SIZE];
		for(i = wFirst; i < (wFirst + wNblk) ; i ++)
		{
			arAllocTable[i]= LAST_RAM_BLK;
			if( i > wFirst)
				arAllocTable[i-1] = i;
		}
	}
	return(pBuf);	
}

//=======================================================
void  OsFreeMem(UCHAR * pBuf)
{
WORD wBlkIndex;
WORD wState;
UCHAR bDone;

	if(pBuf != NULL)
	{
		wBlkIndex = (pBuf-arMemoryPool) / RAM_SIZE;
		do
		{
			bDone = TRUE;
			wState = arAllocTable[wBlkIndex];
			if(wState != FREE_STATE)
			{
				if(wState != LAST_RAM_BLK) bDone = FALSE;
				arAllocTable[wBlkIndex ++] = FREE_STATE;
			}
		}while(bDone == FALSE);
	}
}
