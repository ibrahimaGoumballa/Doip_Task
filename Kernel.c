//=========================================================
// File             : DspicKernel.c
// Description      : OS TEMPS REEL MICROCHIP
// Created          : 01/08/06
// Author           : MB
//
// Copyright ATEQ 
// 15 Rue des Dames 
// 78340 Les Clayes sous bois (FRANCE) 
// Tel : +33 1 30 80 10 20
//
//=========================================================
// Date		Version	Author	History	 
// 01/09/07	1.00A	KP		- Version originale
// 18/09/07	1.10A	AR		- Passage des msg d'erreurs en code
//							- Mapping des datas
//							- Correction du bug sur les MUTEX
//							- Passage en librairie (renommage du .h)
// 12/12/07 1.10B	KP&SL	- Modification du Niveau d'Interruption du Timer 1 : 7
//							- Modification de la valeur du disi dans la fonction OsIsrOut : 3 -> 19
//							- Modification du pop & push du SR de l'interrutpion
//							- Correction de la fonction OsIsrOut
// 22/04/08	1.10E	KP		- Modification de la division du délai : OsWaitEvent
//							- Correction d'un bug de niveau d'interruption : OsWaitEvent
//							- Bug OsIsrOut
//13/03/2009		MB
//							- Correction bug scheduler, commutation de tâche lors d'une boucle "REPEAT"
// 25/05/2021 2.00  Wolfgang- Porting to dsPIC33CHxxxx
//=========================================================

//=========================================================
//======================== INCLUDE ========================
//=========================================================

#include <xc.h>
#include <String.h>
#include <StdLib.h>
#include "Kernel.h"
#include "Map.h"

//==================================================================
//======================== LOCAL DATA TYPES ========================
//==================================================================

//---------- DEFINITION DE LA STRUCTURE DES TIMERS DES TACHES ----------
typedef struct
{
	WORD wCnt;
	UCHAR cMode;
	UCHAR ucBourrage;
	SEMAPHORE *pSem;
}TSKTMR;

//---------- DEFINITION DE LA STRUCTURE CONTEXTE DES TACHES ----------
 typedef struct
{
	unsigned short	_wFramePtr;
	unsigned short	_wStkPtr;
	unsigned short 	_wStkLim;
}TASK_CTX;

//---------- DEFINITION DE LA STRUCTURE DE DESCRIPTION DES TACHES ----------
typedef struct
{
	unsigned short	_wTaskEntry;
	unsigned short	_wStkLen;
}TASK_DESC;

//---------- DEFINITION DE LA STRUCTURE DE LA PILE DES TACHES ----------
typedef struct
{
	DWB		dwbReturnAddr;
	WORD	_W0_;
	WORD	_W1_;
	WORD	_SR_;
	WORD	_RCOUNT_;
	WORD	_DSRPAG_;
	WORD	_DSWPAG_;
	WORD	_W2_;
	WORD	_W3_;
	WORD	_W4_;
	WORD	_W5_;
	WORD	_W6_;
	WORD	_W7_;
	WORD	_W8_;
	WORD	_W9_;
	WORD	_W10_;
	WORD	_W11_;
	WORD	_W12_;
	WORD	_W13_;
}TASK_STRUCT;

//------------------ TABLE LOCAL ------------------
static const unsigned char ucTskMsk[MAX_TASK + 1] = 
{
	0b00000000,
	0b00000001,
	0b00000010,
	0b00000100,
	0b00001000,
	0b00010000,
	0b00100000,
	0b01000000,
	0b10000000
};

//==================================================================
//======================== LOCAL VARIABLES =========================
//==================================================================

//---------- DEFINITION DES VARIABLES GLOBALES ----------
static WORD				__attribute__((near))	wTmpStk;
static TASK_STRUCT		__attribute__((near))	*pStack;

static unsigned char	__attribute__((near))	g_wPeriod_ms	= 0;	//V01.10e - KP - 30/04/08 - Variable globale contenant la période

static unsigned char	__attribute__((near))	ucEstf			= 0;
static unsigned char	__attribute__((near))	ucAstf			= 0;
static unsigned char	__attribute__((near))	ucNta			= 0;
static unsigned char	__attribute__((near))	ucIsrCnt		= 0;
static unsigned char	__attribute__((near))	ucIsrScheduleRq	= 0;

static TSKTMR			__attribute__((near))	TskTmr[MAX_TASK];
static TASK_CTX			__attribute__((near))	rKernelVAr[MAX_TASK];
static TASK_DESC		__attribute__((near))	rTaskDesc[MAX_TASK];
static TASK_STRUCT      __attribute__((near))	*pStack;
static WORD             __attribute__((near))	wStartupDSRPAG;
static WORD             __attribute__((near))	wStartupDSWPAG;

//static void(*AffichageErreur)(char*,char*) = NULL;

//============================TABLE TEXTES===========================
//Table des Primitives
const char OsClockInitStr[]		= "OsClockInit";
const char OsTimerLinkStr[]		= "OsTimerLink";
const char OsCreateTaskStr[]	= "OsCreateTask";
const char OsStartStr[]			= "OsStart";
const char OsSendEventStr[]		= "OsSendEvent";
const char OsWaitEventStr[]		= "OsWaitEvent";
const char OsMutexRequestStr[]	= "OsMutexRequest";
const char OsMutexReleaseStr[]	= "OsMutexRelease";

//Table des Erreurs de Primitives
const char UnknowEventStr[]		= "Unknow Event";
const char UnknowTimerModeStr[]	= "Unknow Timer Mode";
const char StackOverflowStr[]	= "Stack Overflow";
const char AlreadyLinkedStr[]	= "Fct Already Linked";
const char TooMuchTaskStr[]		= "Too Much Tasks";
const char TooMuchFunctionStr[]	= "Too Much Functions";
const char VerifyPointerStr[]	= "Verify Pointer";
const char IsrInProgressStr[]	= "Isr In Progress";
const char NULLPointerStr[]		= "NULL Pointer";
const char NULLPeriodStr[]		= "NULL Period";
const char NULLFrequencyStr[]	= "NULL Frequency";
const char VerifyRequestStr[]	= "Verify Request";
const char StackTooSmallStr[]	= "Stack Too Small";

//=================================================================================================================================================
//== OS Real time Clock
//=================================================================================================================================================
void __attribute__((near)) (*TmrFuncTbl[MAXTMRFONC])(void);


static UCHAR __attribute__((near)) ucLinkIdx = 0;

//==================================================================
//=========================== PROTOTYPES ===========================
//==================================================================
static WORD IrqDisable(void);
static void IrqEnable(WORD wIrqLevel);
//static void Schedule(void);
static void RealTimeClock(void);
//static void OsClockInit(WORD wFreqMhz,WORD wPeriod_ms);
static void OsError(const char*,const char*);
static unsigned char GetPriority (unsigned char);

//===========================================================
//======================== FUNCTIONS ========================
//===========================================================

//
//=================================================
//=== Gestion des interruptions
//=== Sections critiques du noyau
//================================================
//

//______________________________________________________________________//
//	Procedure	:	IrqDisable											//
//______________________________________________________________________//
//	Role		:	Desactive les interruptions							//
//	Entrées		:	Rien												//
//	Sorties		:	Level d'interruption precedent						//
//______________________________________________________________________//
static WORD IrqDisable(void)
{
WORD wIrqLevel;

	SET_AND_SAVE_CPU_IPL(wIrqLevel,7);
	return(wIrqLevel);
}

//______________________________________________________________________//
//	Procedure	:	IrqEnable											//
//______________________________________________________________________//
//	Role		:	Active une interruption								//
//	Entrées		:	Level d'interruption a activer						//
//	Sorties		:	Rien												//
//______________________________________________________________________//
static void IrqEnable(WORD wIrqLevel)
{

	RESTORE_CPU_IPL(wIrqLevel);
}

//______________________________________________________________________//
//	Procedure	:	IdleTask											//
//______________________________________________________________________//
//	Role		:														//
//	Entrées		:	Rien												//
//	Sorties		:	Rien												//
//______________________________________________________________________//
void IdleTask(void)
{
	while(1);
}

//===================================================================
//==== SCHEDULER DSPIC // only when stack is as an interrupt stack
//===================================================================
void IsrSchedule(void)
{
//----------------------------------------------------------------------//
//				Verification : Interruption en cours					//
//----------------------------------------------------------------------//
	asm ("					ulnk");
	asm ("					disi		#9");						// disable all interrupts
	asm ("					push.d		W0");						// save W0 + W1 
	asm ("					push		SR");
	asm ("					push		RCOUNT");
	asm ("					push		DSRPAG				");
	asm ("					push		DSWPAG				");
	asm ("					bset		SR,#5");					// interrupt level 7
	asm ("					bset		SR,#6");
	asm ("					bset		SR,#7");
	asm ("					mov.b		_ucIsrCnt,WREG");			// No schedule if Isr in service
	asm ("					ze			W0,W0");
	asm ("					cp0			W0");
	asm ("					bra			nz,ISR_NO_SWITCH");			// if ucIsrCnt != 0 => ISR_NO_SWITCH

//----------------------------------------------------------------------//
// 					Look for task switching Id							//
// 			ucNta == 0x0 then Kernel start or wake from Idle			//
//----------------------------------------------------------------------//
	asm ("					mov.b		_ucAstf,WREG");
	asm ("					and.b		_ucEstf,WREG");			//WREG =  taches eligibles
	asm ("					ze			W0,W0");
	asm ("					ff1r		W0,W1");				// W1 = Highest priority task number (0 if none)
	asm ("					mov.w		W1,W0");
	asm ("					cp.b		_ucNta");				// compare ucNTA et WREG
	asm ("					bra			z,NO_TASK_SWITCH");		// if equal same Task (exit from scheduler)
	asm ("					mov.b		_ucNta,WREG");
	asm ("					ze			W0,W0");				// if current task is 0 then Kernel start or exit from IDLE => No context to save
	asm ("					cp0			W0");
	asm ("					bra			z,NEW_TASK");

//----------------------------------------------------------------------//
//							SAVE CONTEXT								//
//----------------------------------------------------------------------//
	asm ("					push.d		W2");					// save W2 + W3
	asm ("					push.d		W4");					// save W4 + W5
	asm ("					push.d		W6");					// save W6 + W7
	asm ("					push.d		W8");					// save W8 + W9
	asm ("					push.d		W10");					// save W10 + W11
	asm ("					push.d		W12");					// save W12 + W13
	asm ("					mov.b		_ucNta,WREG");
	asm ("					ze			W0,W0");				// W0 = current task
	asm ("					dec			W0,W0");				// 0 align
	asm ("					mul.uu		W0,#6,W2");				// Offset = 6*Task number (6 bytes for W15 & W14 & SPLIM (stack pointeur + frame pointer + STKOV limit))
	asm ("					mov.w		#_rKernelVAr,W0");
	asm ("					add			W0,W2,W2");				// W2 = Task Context address
	asm ("					mov.w		W14,[W2++]");			// save W14
	asm ("					mov.w		W15,[W2++]");			// save R15
	asm ("					mov.w		SPLIM,W0");
	asm ("					mov			W0,[W2]");				// save SPLIM
	
//----------------------------------------------------------------------//
//						Restore new task context						//
//----------------------------------------------------------------------//
	asm ("					cp0			W1");					// if W1 == 0 then no task to launch => go to IDLE state
	asm ("					bra			z,GO_TO_IDLE");

//----------------------------------------------------------------------//
//							RESTORE CONTEXT								//
//----------------------------------------------------------------------//
	asm ("NEW_TASK:			mov.w		W1,W0");
	asm ("					mov.b		WREG,_ucNta");
	asm ("					dec			W1,W1");				// 0 align
	asm ("					mul.uu		W1,#6,W2");				// Offset = 6*Task number (6 bytes for W15 & W14 & SPLIM (stack pointeur + frame pointer + STKOV limit))
	asm ("					mov.w		#_rKernelVAr,W0");
	asm ("					add			W0,W2,W2");				// W2 = Task Context address
	asm ("					mov			_wTmpStk,WREG");		// Set SPLIM at the upper ram address available for stacks
	asm ("					mov.w		W0,SPLIM");
	asm ("					mov.w		[W2++],W14");			// restore frame pointer
	asm ("					mov.w		[W2++],W15");			// restore stack pointer
	asm ("					mov.w		[W2],W0");
	asm ("					mov.w		W0,SPLIM");				// restore stack limit address

	asm ("					pop.d		W12");
	asm ("					pop.d		W10");
	asm ("					pop.d		W8");
	asm ("					pop.d		W6");
	asm ("					pop.d		W4");
	asm ("					pop.d		W2");

	asm ("NO_TASK_SWITCH:	clr.b		_ucIsrScheduleRq	");
	asm ("ISR_NO_SWITCH:	disi		#9					");
	asm ("					pop			DSWPAG				");
	asm ("					pop			DSRPAG				");
	asm ("					pop			RCOUNT				");
	asm ("					pop			SR					");
	asm ("					pop.d		W0					");

	asm ("					retfie");

	asm ("GO_TO_IDLE:		clr.b		_ucNta				");
	asm ("					clr.b		_ucIsrScheduleRq	");
	asm ("					disi		#4					");
	asm ("					bclr		SR, #5				");
	asm ("					bclr		SR, #6				");
	asm ("					bclr		SR, #7				");
	asm ("					bra			_IdleTask			");
}
//______________________________________________________________________//
//	Procedure	:	Schedule											//
//______________________________________________________________________//
//	Role		:	Chef d'orchestre									//
//	Entrées		:	Rien												//
//	Sorties		:	Rien												//
//______________________________________________________________________//

void Schedule(void)
{
	asm ("					disi		#0x3FFF");
	asm ("					ulnk");
	asm ("					push.d		W0");					// save W0 + W1 
	asm ("					mov.w		SR,W0");				// Get status Reg
	asm ("					and.w		#0xFF,W0");				// keep only SRL 
	asm ("					swap		W0");					// SRL on upper byte
	asm ("					mov			W15,W1");				// Get Stack pointer
	asm ("					sub.w		W1,#6,W1");				// W1 = Upper Word return address
	asm ("					ior			W0,[W1],[W1]");			//stack is like after an interrupt service
	asm ("					pop.d		W0");					// restore working reg
	asm ("					bra			_IsrSchedule");			// Task Switch
}
//--------------------------------------------------------------------------------------------
//	Function	:	OsIsrIn
//--------------------------------------------------------------------------------------------
//	Purpose		:	Nested interrupt counter
//	Param		:	[void]
//	Return		:	[void]
//--------------------------------------------------------------------------------------------
/*void OsIsrIn(void)
{
 	asm ("					disi		#0x3FFF");
    asm ("                 	ulnk          ");
    asm ("                  pop.d       W0");       //Get return address
	asm ("					push		DSRPAG");	// save DSRPAG + DSWPAG
  	asm ("					push		DSWPAG");	// save DSRPAG + DSWPAG
   	asm ("					push.d		W0");		// save again return address
  	DSRPAG = wStartupDSRPAG;
	DSWPAG = wStartupDSWPAG;
 	ucIsrCnt ++;	// inc nested interrupt count
    asm ("                 bset        SR,#0");     //clear Carry
    asm ("                 subb        W15,#4,W14" );  
 	asm ("					disi		#2");
    asm ("                 return         ");
 }*/

void OsIsrIn(void)
{
	ucIsrCnt ++;	// inc nested interrupt count
	__asm__ volatile ("					disi		#2");
}

//______________________________________________________________________//
//	Procedure	:	OsIsrOut
//______________________________________________________________________//
//	Role		:	Décrémentation d'un compteur d'interruption			//
//					Vérification du nombre d'interruption				//
//	Entrées		:	Rien												//
//	Sorties		:	Rien												//
//______________________________________________________________________//
/*void OsIsrOut(void)
{
//----------------------------------------------------------------------//
//			Verification : Nombre d'interruption en cours				//
//					Interruption => Pas de Schedule						//
//----------------------------------------------------------------------//
    ATOMIC();
	ucIsrCnt --;
    if(ucIsrCnt > 0 || ucIsrScheduleRq == 0)
	{
        asm ("                  ulnk                        ");
        asm ("NO_SWITCH:                                    ");
        asm ("                  pop.d       W0              ");     //Get return address
        asm ("					pop     	DSWPAG          ");     // restore DSWPAG
        asm ("					pop     	DSRPAG          ");     // restore DSRPAG 
        asm ("					push.d		W0              ");		// save again return address
        asm ("                  sub         W15,#4,W14      ");  
        asm ("            		disi		#0x2            ");		// end atomic section while restoring return address
        asm ("                  return                      ");		// !! return to the interrupt caller
    }    
    else
	{
		asm ("					ulnk						");
			//////////////////////////////////////////////////////////////
        asm ("					mov			W15, W0			");			// Stack Ptr copy in W0
        asm ("					sub.w		#34, W0			");			// W3 = (upper return address + SR <7:0>) ptr
        asm ("					mov			#0xE080, W1		");			// W0 = mask for priority bits only	//18/12/07
        asm ("					and			W1, [W0], W1	");			// W0 = priority bits
        asm ("					cp0			W1				");			// if W0 == 0 then no ISR service in process
        asm ("					bra			nz,NO_SWITCH	");

        asm ("                  pop.d       W0              ");     //remove OsIsrOut return address
        asm ("					pop     	DSWPAG          ");     // restore DSWPAG
        asm ("					pop     	DSRPAG          ");     // restore DSRPAG 
        asm ("                  pop         W14             ");
        asm ("					pop			SR				");
        asm ("					pop			W8				");
        asm ("					pop.d		W6				");
        asm ("					pop.d		W4				");
        asm ("					pop.d		W2				");
        asm ("					pop.d		W0				");
        asm ("					pop			RCOUNT			");
        asm ("					bclr		SR, #5			");			// Interrupt level 7
        asm ("					bclr		SR, #6			");
        asm ("					bclr		SR, #7			");
        asm ("					bra			_Schedule		");

	}
}*/


void OsIsrOut(void)
{
//----------------------------------------------------------------------//
//			Verification : Nombre d'interruption en cours				//
//					Interruption => Pas de Schedule						//
//----------------------------------------------------------------------//
	SET_CPU_IPL(7);
	ucIsrCnt --;
    if(ucIsrCnt == 0 && ucIsrScheduleRq != 0)
	{
        Schedule();
	}
	else
	{
        //do nothing
	}
}//end of OsIsrOut(...)


//----------------------------------------------------------------------//
//				Simulate the ISR Epilog to retreive task				//
//					context but returns to SCHEDULER					//
//----------------------------------------------------------------------//
//V1.10B - 13/12/07 - KP - Modification du SR pour le Scheduler

//______________________________________________________________________//
//	Procedure	:	OsCreateTask										//
//______________________________________________________________________//
//	Role		:	Creer une tache										//
//	Entrées		:	Adresse de la tache a creer							//
//					Taille de la pile(en octet) reserve pour la tache	//
//	Sorties		:	Rien												//
//______________________________________________________________________//
void OsCreateTask(void(*pTsk)(void),unsigned short wSize)
{
//---------------------------------------------------------------------------
//	Minimum de pile allouée pour la tâche : OK
//---------------------------------------------------------------------------
	if(ucNta < MAX_TASK)	// Nombre de tâche inférieur au nombre de tâche maximum
	{
		if(NULL != pTsk)		// pTsk est non nul
		{
			if(wSize >(sizeof(TASK_STRUCT)+8))	// Espace mémoire allou?(pile) supérieur ?(sizeof(TASKSTRUCT)+8)	
			{
				wSize &=0xFFFE;				// wsize must be even
				rTaskDesc[ucNta]._wTaskEntry = (WORD)pTsk;
				rTaskDesc[ucNta]._wStkLen = wSize;
				ucNta ++;
				ucAstf |= ucTskMsk[ucNta];
				ucEstf = ucAstf;
			}
//---------------------------------------------------------------------------
//	Mauvais Paramètres : OK
//---------------------------------------------------------------------------
			else
			{
				OsError(OsCreateTaskStr,StackTooSmallStr);
			}
		}
		else
		{
			OsError(OsCreateTaskStr,NULLPointerStr);
		}
	}
	else
	{
		OsError(OsCreateTaskStr,TooMuchTaskStr);
	}
} 

//______________________________________________________________________//
//	Procedure	:	OsStart												//
//______________________________________________________________________//
//	Role		:	Demarrage du noyau									//
//	Entrées		:	Frequence d'horloge en Mhz							//
//				²	Periode en ms										//
//	Sorties		:	Rien												//
//______________________________________________________________________//
void OsStart(WORD wFreqMhz,WORD wPeriod_ms)
//void OsStart()
{
long lStkOV = 0;					//lStkOV => Variable de test de debordement de pile
	g_wPeriod_ms = wPeriod_ms;		//V01.10e - KP - 30/04/08 - Sauvegarde de la période d'horloge

	// Level 1 interrupt for all devices
    IPC0  = IPC1  = IPC2  = IPC3  = IPC4  = IPC5  = IPC6  = IPC7  = IPC8  = IPC9  = 0x1111;
	IPC10 = IPC11 = IPC12 = IPC13 = IPC15 = IPC16 = IPC17 = IPC20 = IPC21 = 0x1111;
	IPC22 = IPC23 = IPC24 = IPC25 = IPC26 = IPC27 = IPC28 = 0x1111;
    IPC29 = IPC30 = IPC31 = IPC32 = IPC33 = IPC34 = IPC35 = IPC36 = IPC37 = IPC38 = 0x1111;
    IPC39 = IPC42 = IPC43 = IPC44 = IPC45 = IPC47 = 0x1111;
    
	
	IPC0bits.T1IP = ISR_TIMER_1_LEVEL;	//Interrupt Level TIMER 1

	//OsClockInit(wFreqMhz,wPeriod_ms);	// Appel de la fonction OsClockInit
	wStartupDSRPAG = DSRPAG;
	wStartupDSWPAG = DSWPAG;
//---------------------------------------------------------------------------------------
//  Find top of stack
//---------------------------------------------------------------------------------------
	asm ("			mov.w		W14,W0");
	asm ("			sub			#6,W0");		// substract return address + 2 for lnk instruction 
	asm ("			mov			WREG,_wTmpStk");
//---------------------------------------------------------------------------------------
// move stack ptr at the end use 32 bytes for startup purpose
//---------------------------------------------------------------------------------------
	asm ("			mov.w		SPLIM,W0");
	asm ("			sub			#0x20,W0");
	asm ("			mov.w		W0,W15");

	for(ucIsrCnt = 0; ucIsrCnt < ucNta ; ucIsrCnt ++)
	{
		lStkOV += rTaskDesc[ucIsrCnt]._wStkLen;
	}
	if(lStkOV<0xFFFF)
	{
		for(ucIsrCnt = 0; ucIsrCnt < ucNta ; ucIsrCnt ++)
		{
			pStack = (TASK_STRUCT *)wTmpStk;					//pStack = toip of stack
			memset((void *)pStack, 0xFF, rTaskDesc[ucIsrCnt]._wStkLen);
			memset((void *)pStack, 0x00, sizeof(TASK_STRUCT));
			pStack->_DSRPAG_ = wStartupDSRPAG;
			pStack->_DSWPAG_ = wStartupDSWPAG;
			pStack->dwbReturnAddr.LSW = rTaskDesc[ucIsrCnt]._wTaskEntry;
			rKernelVAr[ucIsrCnt]._wFramePtr = wTmpStk + sizeof(TASK_STRUCT);
			rKernelVAr[ucIsrCnt]._wStkLim = wTmpStk + (rTaskDesc[ucIsrCnt]._wStkLen- 8);
			rKernelVAr[ucIsrCnt]._wStkPtr = wTmpStk + sizeof(TASK_STRUCT);
			wTmpStk = rKernelVAr[ucIsrCnt]._wStkLim +8;		
		}
		//---------------------------------------------------------------------------------------
		//	wTmpStk = end of all stacks
		//---------------------------------------------------------------------------------------
		ucNta = 0;
		ucIsrCnt = 0;
		IFS0bits.T1IF = 0;
		IEC0bits.T1IE = 1;
		T1CONbits.TON = 1;
		IsrSchedule();
	}
//---------------------------------------------------------------------------
// Débordement de Pile : OK
//---------------------------------------------------------------------------
	else
	{
		OsError(OsStartStr,StackOverflowStr);
	}
}

//______________________________________________________________________//
//	Procedure	:	OsSendEvent											//
//______________________________________________________________________//
//	Role		:	Envoi d'un message a une tache						//
//	Entrées		:	Pointeur sur la semaphore de destination			//
//					Code d'evenement									//
//					Pointeur sur le buffer de message (NULL si pas		//
//					besoin)												//
//	Sorties		:	Rien												//
//______________________________________________________________________//
void OsSendEvent(SEMAPHORE *pSem,UCHAR ucEvt,UCHAR *pMsg)
{
TSKTMR *pTmr;
UCHAR ucTskIdx = pSem->ucWaitingTask;
WORD wIrq;

//---------------------------------------------------------------------------
// Pointeur sur SEMAPHORE nul : OK
//---------------------------------------------------------------------------
	if(NULL != pSem)
 	{
		wIrq = IrqDisable();
	
		pTmr = &TskTmr[ucTskIdx];
		if(T_RUN != pTmr->cMode)
		{
			pTmr->cMode = T_STOP;
			pTmr->wCnt = 0;
		}
		pSem->rEvent[pSem->ucWriteIdx].ucEvt = ucEvt;
		pSem->rEvent[pSem->ucWriteIdx].pMsg = pMsg;
		pSem->ucWriteIdx ++;
		pSem->ucWriteIdx &= 0x7;
		ucEstf |= ucTskMsk[ucTskIdx];
		ucIsrScheduleRq = YES;
		IrqEnable(wIrq);
		if(ucIsrCnt == 0)
			Schedule();
	}
	else
	{
		OsError(OsSendEventStr,NULLPointerStr);
	}
}

//______________________________________________________________________//
//	Procedure	:	OsWaitEvent											//
//______________________________________________________________________//
//	Role		:	Attente d'un message 								//
//	Entrées		:	Pointeur sur la semaphore de destination			//
//					Delai d'attente (zero si pas besoin)				//
//					Mode du timer										//
//	Sorties		:	Code d'evenement									//
//______________________________________________________________________//
EVENT_DEF *OsWaitEvent(SEMAPHORE *pSem,WORD wDelai,UCHAR ucMode)
{
EVENT_DEF  *pEvent = NULL;
WORD wIrq;
	//---------------------------------------------------------------------------
	//	Pointeur sur SEMAPHORE nul : Mode faux
	//---------------------------------------------------------------------------
	if(NULL != pSem)
	{
		if(ucMode < NMAX_OS_TMR_STATE)
		{
			wIrq = IrqDisable();
			if(pSem->ucReadIdx == pSem->ucWriteIdx)
			{
				if(T_NO_INIT != ucMode)
				{
				//---------------------------------------------------------------------------
				//	V01.10e - Modification de la division du délai
				//---------------------------------------------------------------------------
					TskTmr[ucNta].wCnt	= wDelai / g_wPeriod_ms ;		
					TskTmr[ucNta].cMode	= ucMode;
					TskTmr[ucNta].pSem	= pSem;
				}
				pSem->ucWaitingTask = ucNta;
				ucEstf &= (~ucTskMsk[ucNta]);
				IrqEnable(wIrq);
				Schedule();
			//---------------------------------------------------------------------------
			//	V01.10e - 29/04/08 - Déplacement de IrqDisable() après le Scheduler
			//---------------------------------------------------------------------------
				wIrq = IrqDisable();
			}
		//---------------------------------------------------------------------------
		//	Vérifier la réactivation des interruptions
		//---------------------------------------------------------------------------
			pEvent = &pSem->rEvent[pSem->ucReadIdx];
			pSem->ucReadIdx ++;
			pSem->ucReadIdx &= 0x7;
			pSem->ucWaitingTask = 0;
			IrqEnable(wIrq);
		}
		else
		{
			OsError(OsWaitEventStr,UnknowTimerModeStr);
		}
	}
	else
	{
		OsError(OsWaitEventStr,NULLPointerStr);
	}
	return(pEvent);
}

//
//=================================================================================================================================================
//== OS Real time Clock
//=================================================================================================================================================

//______________________________________________________________________//
//	Procedure	:	OsTimerLink											//
//______________________________________________________________________//
//	Role		:	Fonction linker a l'horloge temps reel				//
//	Entrées		:	Pointeur sur fonction a linker						//
//	Sorties		:	Rien												//
//______________________________________________________________________//
void OsTimerLink(void (*pFunc)(void))
{
UCHAR i;

//---------------------------------------------------------------------------
//	Vérification : Pointeur Nul : Trop de fonction : OK
//---------------------------------------------------------------------------
	if(NULL != pFunc)
	{
		if(ucLinkIdx < MAXTMRFONC)
		{
			i = 0;
			while((i < ucLinkIdx) && (TmrFuncTbl[i] != pFunc))
			{
				i ++;
			}
			if(i == ucLinkIdx)
			{
				TmrFuncTbl[ucLinkIdx]= pFunc;
				ucLinkIdx ++;
			}
	        else
			{
				OsError(OsTimerLinkStr,AlreadyLinkedStr);
			}
		}
		else
		{
			OsError(OsTimerLinkStr,TooMuchFunctionStr);
		}
	}
	else
	{
		OsError(OsTimerLinkStr,VerifyPointerStr);
	}
}

//______________________________________________________________________//
//	Procedure	:	RealTimeClock										//
//______________________________________________________________________//
//	Role		:	Gere l'horloge temps reel (reveille par une 		//
//					interruption)										//
//	Entrées		:	Rien												//
//	Sorties		:	Rien												//
//______________________________________________________________________//
static void RealTimeClock(void)
{
UCHAR i;
void (*pTmrFunc)(void);
TSKTMR *pTimer;

	pTimer = TskTmr;
	for(i = 0; i < (MAX_TASK); i ++)
	{
		if(0 != pTimer->wCnt)
		{
			(pTimer->wCnt) --;
			if((0 == pTimer->wCnt) && (T_STOP != pTimer->cMode))
			{
//                EN_CANLS ^= 1;    //toggle
				OsSendEvent(pTimer->pSem,EVT_TIMER,NULL);
			}
		}
		pTimer ++;
	}
	for(i = 0; i < MAXTMRFONC; i ++)
	{
		pTmrFunc = TmrFuncTbl[i];
		if(NULL != pTmrFunc)
		{
			pTmrFunc();
		}
	}
}

//______________________________________________________________________//
//	Procedure	:	OsClockInit											//
//______________________________________________________________________//
//	Role		:	Initialise l'horloge temps reel						//
//	Entrées		:	Frequence d'horloge en Mhz							//
//					Periode en ms										//
//	Sorties		:	Rien												//
//______________________________________________________________________//
/*static void OsClockInit(WORD wFreqMhz,WORD wPeriod_ms)
{
WORD wPrescale = 0;
float fPeriod_ns;
float fTickTime;
float fReload;

	/// paramètrage de PLL pour obtenir 160 MHZ
	/// Clock Diviser Register : CLKDIV 						
							
	CLKDIVbits.ROI  = 0b0;			// Clock reduction select default						
	CLKDIVbits.DOZE = 0b000;		// Clock reduction select default
	CLKDIVbits.DOZEN = 0b0;			// Processor clock/periph 1:1 
	CLKDIVbits.FRCDIV = 0b000;		// FRC DIVIDED BY 1
	CLKDIVbits.PLLPOST = 0b00;		// PLL VCO output divider ->  output/2
	CLKDIVbits.PLLPRE = 0b00000;	// PLL phase detector input divider ->  input/2

/// PLL Feedback  Divider Register : PLLFBD

	PLLFBDbits.PLLDIV = 0b000011110;	//  =0x1E   ; PLL Feed Back Divider -> M = 32
//================= Activation de la PLL ==================
	asm ("push		W0");
	asm ("push		W1");
	asm ("push		W2");
	asm ("push		W3");
	asm ("mov		#OSCCON+1,W1");		//Load W1 with OSCCONH address for indirect access
	asm ("mov		#0x78,W2");			//UNLOCK OSCCONH sequence CODE1
	asm ("mov		#0x9A,W3");			//UNLOCK OSCCONH sequence CODE2
	asm ("mov		#0x3,W0");			//NEW CLOCK MODE ==> XT + PLL
	asm ("mov.b	W2,[W1]");			//Unlock step1
	asm ("mov.b	W3,[W1]");			//Unlock step2
	asm ("mov.b	W0,[W1]");			//Write new clock mode

	asm ("mov		#OSCCON,W1");		//Load W1 with OSCCONL address for indirect access
	asm ("mov		#0x46,W2");			//UNLOCK OSCCONL sequence CODE1
	asm ("mov		#0x57,W3");			//UNLOCK OSCCONL sequence CODE2
	asm ("mov		#0x1,W0");			//CLOCK SWITCH REQUEST command
	asm ("mov.b	W2,[W1]");			//Unlock step1
	asm ("mov.b	W3,[W1]");			//Unlock step2
	asm ("mov.b	W0,[W1]");			//Switch clock mode request
	asm ("pop		W3");
	asm ("pop		W2");
	asm ("pop		W1");
	asm ("pop		W0");
//---------------------------------------------------------------------------
//	Frequence ou Periode nulle : OK
//---------------------------------------------------------------------------
	if(0 != wFreqMhz)
	{
		if(0 != wPeriod_ms)
		{
			fPeriod_ns = (float)wPeriod_ms * 1000000.0f;
			fTickTime = (float)(wFreqMhz) / 4.0f;			//internal Clock frequency in MHz
			fTickTime = 1000.0 / fTickTime;		// Tick length in ns
			fReload = fPeriod_ns / fTickTime;
			while((fReload > 65535.0f) && (wPrescale < 4))
			{
				wPrescale ++;
				switch(wPrescale)
				{
					case	1:
						fTickTime *= 8.0f;			// prescale 8
					break;
	
					case	2:
						fTickTime *= 8.0f;			// prescale 64
					break;
	
					case	3:
							fTickTime *= 4.0f;			// prescale 256
					break;
				}
					fReload = fPeriod_ns / fTickTime;
			}
			T1CONbits.TCKPS = wPrescale;
			PR1 = (WORD)(fReload);
		}
		else
		{
			OsError(OsClockInitStr,NULLPeriodStr);
		}
	}
	else
	{
		OsError(OsClockInitStr,NULLFrequencyStr);
	}
}*/

//______________________________________________________________________//
//	Procedure	:	_T1Interrupt										//
//______________________________________________________________________//
//	Role		:	Interruption de l'horloge temps reel				//
//	Entrées		:	Rien												//
//	Sorties		:	Rien												//
//______________________________________________________________________//
_OS_ISR _T1Interrupt(void)
{//1ms interrupt
	OsIsrIn();        //mark for debug, should recover
	IFS0bits.T1IF = 0;
	RealTimeClock();
	OsIsrOut();       //mark for debug, should recover
}

//______________________________________________________________________//
//	Procedure	:	OsSetErrorHandler									//
//______________________________________________________________________//
//	Role		:	Affiche un message en cas d'erreur					//
//	Entrées		:	Chaine de caractères								//
//	Sorties		:	Rien												//
//______________________________________________________________________//
void OsSetErrorHandler(void(*pAffichageErreur)(char*,char*))
{
	//AffichageErreur = pAffichageErreur;	//Initialisation du pointeur sur fonction
}

//______________________________________________________________________//
//	Procedure	:	OsError												//
//______________________________________________________________________//
//	Role		:	Affiche un message en cas d'erreur					//
//	Entrées		:	Chaine de caractères								//
//	Sorties		:	Rien												//
//______________________________________________________________________//
static void OsError(const char* sPrimitive, const char* sErreur)
{
	/*if(NULL != AffichageErreur)	//Test si le pointeur est non nul
	{
		char	cPrimitive[64];
		char	cErreur[64];
		
		strcpy (cPrimitive, sPrimitive);
		strcpy (cErreur, sErreur);
		
		AffichageErreur(cPrimitive,cErreur);	//Appel de la fonction d'erreur
	}*/
	while(1);	//Sortie du programme : Blocage
}
//______________________________________________________________________________//
//	Procedure	:	OsMutexRequest												//
//______________________________________________________________________________//
//	Role		:	Sémaphore d'exclusion mutuelle : Requête pour une Ressource	//
//	Entrées		:	Pointeur sur structure SMUTEX								//
//	Sorties		:	Rien														//
//______________________________________________________________________________//
void OsMutexRequest(SMUTEX* pSmutex)
{
WORD wIrq;
	if(NULL != pSmutex)		//Test si le pointeur sur SMUTEX est nul
	{
		if(!ucIsrCnt)		//Test si le compteur d'interruption est non nul
		{
			wIrq = IrqDisable();		//Désactivation des interruptions
			if(pSmutex->iJeton == 0)	//Ressource libre
			{
				pSmutex->iJeton--;	//Occupation de la Ressource
			}
			else
			{
				pSmutex->ucTache |= ucTskMsk[ucNta];	//Mise en attente de la tâche
				ucEstf &= (~ucTskMsk[ucNta]);			//Tâche non éligible
				Schedule();								//Lancement du Scheduler
			}
			IrqEnable(wIrq);							//Réactivation des interruptions
		}
		else
		{
			OsError(OsMutexRequestStr,IsrInProgressStr);		
		}
	}
	else
	{
		OsError(OsMutexRequestStr,NULLPointerStr);
	}
}

//______________________________________________________________________________//
//	Procedure	:	OsMutexRelease												//
//______________________________________________________________________________//
//	Role		:	Sémaphore d'exclusion mutuelle : Libération d'une Ressource	//
//	Entrées		:	Pointeur sur structure SMUTEX								//
//	Sorties		:	Rien														//
//______________________________________________________________________________//
void OsMutexRelease(SMUTEX* pSmutex)
{
WORD wIrq;
UCHAR ucNtaAtt;
	if(NULL != pSmutex)				//Test si le pointeur sur SMUTEX est nul
	{
		if(pSmutex->iJeton < 0)			//Test si le jeton est  nul
		{
			wIrq = IrqDisable();	//Désactivation des interruptions
			if(pSmutex->ucTache)	//Test si les tâches en attente son non nul
			{	
				ucNtaAtt=GetPriority(pSmutex->ucTache);		//Récupérer le numéro de la tâche la plus prioritaire
				pSmutex->ucTache &= ~(ucTskMsk[ucNtaAtt]);	//Retire la tâche de la file d'attente
				ucEstf |= ucTskMsk[ucNtaAtt];				//Eligibilit?de la tâche
				Schedule();									//Lancement du Scheduler
			}
			else
			{
				pSmutex->iJeton = 0;		//Libération de la ressource
			}
			IrqEnable(wIrq);				//Réactivation des interruptions
		}
		else
		{
			OsError(OsMutexReleaseStr,VerifyRequestStr);
		}
	}
	else
	{
		OsError(OsMutexReleaseStr,NULLPointerStr);
	}
}

//______________________________________________________________________//
//	Procedure	:	GetPriority											//
//______________________________________________________________________//
//	Role		:	Récupère le numéro de tâche le plus prioritaire		//
//	Entrées		:	Octet representatif des tâches en Attente			//
//	Sorties		:	Numéro de la tâche									//
//______________________________________________________________________//
static unsigned char GetPriority (unsigned char ucWaitTask)
{
	asm ("	ze		W0,W0");	//Met le MSB de W0 ?0
	asm ("	ff1r	W0,W0");	//Cherche le numéro du premier bit ?"1"
	asm ("	ulnk");
	asm ("	return");			//Retourne W0
	return 0;	//Ligne ignor??cause du return précédent : Nécessaire pour la compilation en C
}
