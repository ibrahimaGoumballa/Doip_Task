//=========================================================
// File             : DspicKernel.h
// Description      : Déclaration des constantes, des structures 
//						et des prototypes liées au noyau
// Created          : 01/08/06
// Author           : JD
//
// Copyright ATEQ 
// 15 Rue des Dames 
// 78340 Les Clayes sous bois (FRANCE) 
// Tel : +33 1 30 80 10 20
//
//=========================================================
// Date		Version	Author	History	 
// 01/09/07	1.00A	KP	- Version originale
// 18/09/07	1.10A	AR	- Passage des msg d'erreurs en code
//				- Mapping des datas
//				- Correction du bug sur les MUTEX
//				- Passage en librairie (renommage du .h)
// 07/05/08	1.10E	KP	- Déplacement des déclarations de type dans NewType.h
// 25/05/2021 2.00  Wolfgang- Porting to dsPIC33CHxxxx
//=========================================================

#ifndef _DSPICKERNEL_H
#define _DSPICKERNEL_H

//=========================================================
//======================= INCLUDES ========================
//=========================================================

#include "../Common_Header/NewType.h"

//=========================================================
//======================= CONSTANTS =======================
//=========================================================

#define MAX_TASK		8
#define MAXTMRFONC		16

#define ISR_TIMER_1_LEVEL		6

#define NO_TIMEOUT		0

enum OS_EVT
{
	NO_EVT = 0,
	EVT_TIME_OUT,
	EVT_TIMER,
	EVT_REFRESH_CONTEXT,

	MAX_OS_EVT
};

enum OS_TMR_STATE
{
	T_STOP,
	T_STOP_ON_EVT,
	T_RUN,
	T_NO_INIT,
	NMAX_OS_TMR_STATE
};


//=========================================================
//=================== GLOBAL DATA TYPES ===================
//=========================================================
typedef struct
{
	UCHAR volatile ucEvt;
	UCHAR volatile *pMsg;
}EVENT_DEF;

typedef struct
{
	UCHAR 	ucWaitingTask;
	UCHAR 	ucReadIdx;
	UCHAR 	ucWriteIdx;
	EVENT_DEF rEvent[8];
}SEMAPHORE;

typedef struct
{
	int		volatile iJeton;
	BYTE	volatile ucTache;
}SMUTEX;

//=========================================================
//====================== PROTOTYPES =======================
//=========================================================

#define _OS_ISR void __attribute__ ((__interrupt__(__save__(SR),preprologue("disi #0x3FFF")), no_auto_psv))
void OsIsrIn(void);
void OsIsrOut(void);
void OsCreateTask(void(*pTsk)(void),unsigned short wSize);
void OsStart(WORD wFreqMhz,WORD wPeriod_ms);
void OsSendEvent(SEMAPHORE *pSem,UCHAR ucEvt,UCHAR *pMsg);
EVENT_DEF *OsWaitEvent(SEMAPHORE *pSem,WORD wDelai,UCHAR ucMode);
void OsTimerLink(void (*pFunc)(void));
void OsSetErrorHandler(void(*pAfficherErreur)(char*,char*));
void OsMutexRequest(SMUTEX*);
void OsMutexRelease(SMUTEX*);

#endif
