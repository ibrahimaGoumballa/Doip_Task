//=========================================================
// File             : Uart.c
// Description      : Fonctions de gestion de l'uart du Dspic
//						pour la communication par liaison RS232
// Created          : 01/08/06
// Author           : JD & KP
//
// Copyright ATEQ 
// 15 Rue des Dames 
// 78340 Les Clayes sous bois (FRANCE) 
// Tel : +33 1 30 80 10 20
//=========================================================
// FICHIER COMMUN : F300 & POC Rapide
//=========================================================
// Date		Version 	History	 
// 19/12/07	1.10c4		Modification des Interruptions
//						de Réception pour la compatibilité
//						avec le noyau V1.10B
// 26/05/21 1.20        Wolfgang porting to T52088, big difference
//=========================================================

//=========================================================
//======================== INCLUDE ========================
//=========================================================
#include <xc.h>
#include "Kernel.h"
#include <stdio.h>
#include "interrupt.h"
#include "uart.h"
#include "rs232tsk.h"
#include "Map.h"
#include "../Common_Header/NewType.h"
#include "./mcc_generated_files/clock.h"



//==================================================================
//=========================== EXTERNALS ============================
//==================================================================

//==================================================================
//========================== PROTOTYPES ============================
//==================================================================

//==================================================================
//========================== CONSTANTS =============================
//==================================================================
#define MODE_9_BITS		3

//==================================================================
//======================== LOCAL DATA TYPES ========================
//==================================================================
static pUARTFONC pRxFonc = NULL;					//procedure a appeler en fin de reception
static pUARTFONC pTxFonc = NULL;					//procedure a appeler en fin de transmission
static UCHAR *pRxBuf=NULLPTR;					//Pointeur sur le buffer de reception
static UCHAR *pTxBuf=NULLPTR;					//Pointeur sur le buffer de transmission
static UCHAR ucRxCar=0;							//index sur le caractere a ranger dans le buffer de reception
static UCHAR ucTxCar=0;							//index sur le caractere a transmettre
static UCHAR ucNbCarTx=0;						//Nombre de caractere a transmettre
static UCHAR bTxOn = 0;							//Flag de Communication
static UCHAR ucRxState = WAIT_STX;

//==================================================================
//======================== LOCAL VARIABLES =========================
//==================================================================

//==================================================================
//============================ FUNCTIONS ===========================
//==================================================================


//______________________________________________________________________//
//	Procedure	:	Rs232TimerComInit
//______________________________________________________________________//
//	Role		:	Initialise le timer pour la communication en
//					protocole modbus
//	Entrées		:	Frequence d'horloge en Mhz
//					Periode en ms
//	Sorties		:	Rien
//______________________________________________________________________//
void Rs232TimerComInit(WORD wFreqMhz,WORD wPeriod_ms)
{
/*WORD wPrescale = 0;
float fPeriod_ns;
float fTickTime;
float fReload;

	if((0 != wFreqMhz) && (0 != wPeriod_ms))
	{
		fPeriod_ns = (float)wPeriod_ms * 1000000.0f;
		fTickTime = (float)(wFreqMhz) / 4.0f;			//Internal Clock frequency in MHz
		fTickTime = 1000.0 / fTickTime;					//Tick length in ns
		fReload = fPeriod_ns / fTickTime;
		while((fReload > 65535.0f) && (wPrescale < 4))
		{
			wPrescale ++;
			switch(wPrescale)
			{
				case	1:
					fTickTime *= 8.0f;			//Prescale 8
				break;

				case	2:
					fTickTime *= 8.0f;			//Prescale 64
				break;

				case	3:
						fTickTime *= 4.0f;		//Prescale 256
				break;

				default:
				break;
			}
				fReload = fPeriod_ns / fTickTime;
		}

		T4CONbits.TCKPS = wPrescale;
		PR4 = (WORD)(fReload);
	}*/
}

//______________________________________________________________________//
//	Procedure	:	UartInit
//______________________________________________________________________//
//	Role		:	Fonction de Configuration de l'UART 1
//	Entrées		:	Rien
//					Configuration Figee 57600 bauds 8 bits 1 stop parite paire
//	Sorties		:	Rien
//______________________________________________________________________//
void Rs232Init(void)
{
unsigned long ulFcy = CLOCK_PeripheralFrequencyGet();   //Fcy = Fp = 70MHz, Fosc = 140MHz

	//Configuration : Vitesse, Mode et Etat des UARTs
	U1BRG	=	(WORD)(((ulFcy/THE_BAUD_RATE)/16) - 1);    //set baud rate to 60900
	U1STAH	=	0x0022;     //U1TX interrupt when TX buffer empty, U1RX interrupt when 1 byte received
    U1MODE	=	0x8030;		//Enable UART1, TXEN, RXEN

//Init interruption uart (RX et TX)
    IFS0bits.U1RXIF = 0;					//Effacement du Flag d'IT Rx
    IFS0bits.U1TXIF = 0;					//Effacement du Flag d'IT Tx

    IPC2bits.U1RXIP = 0x07 & IT_UART_RX_LEVEL;		//Configuration de l'IT Rx
    IPC3bits.U1TXIP = 0x07 & IT_UART_TX_LEVEL;		//Configuration de l'IT Tx

    IEC0bits.U1RXIE = 0x01;		//Validation de l'IT Rx
    IEC0bits.U1TXIE = 0x01;		//Validation de l'IT Rx

	//Timer4 utilise pour le timeout reception 
	//IPC9bits.IC4IP = IT_TIMER_4_LEVEL;				//not sure, not finish
	//Rs232TimerComInit(FREQUENCE_160_MHZ,RS232_BREAK_DETECT);			//timer out fin de trame rs232
}



//______________________________________________________________________//
//	Procedure	:	Rs232Receive										//
//______________________________________________________________________//
//	Description	:	Init reception d'une trame sur la rs232				//
//	Entree		:	pbuf --> adresse de rangement des caracteres		//
//					pFonc --> fonction a appeler en fin de reception    //
//	Sortie		:	Rien												//
//______________________________________________________________________//
void Rs232Receive(UCHAR *pbuf, pUARTFONC pFonc)
{
	pRxFonc = pFonc;						//procedure a appeler en fin de reception
	pRxBuf = pbuf;							//Pointeur sur le buffer de reception
	ucRxCar=0;								//Reset index reception caractere
}

//______________________________________________________________________//
//	Procedure	:	Rs232TxCar											//
//______________________________________________________________________//
//	Description	:	Transmission d'un caractere sur le port rs232		//
//	Entree		:	rien												//
//	Sortie		:	Rien												//
//______________________________________________________________________//
void Rs232TxCar(void)
{
WB wTxCar;
	if(ucNbCarTx && pTxBuf)
	{
		wTxCar.LSB = pTxBuf[ucTxCar];			
		wTxCar.MSB = 0;
		ucNbCarTx--;					//decrementer le nombre de caractere a transmettre
		ucTxCar++;						//incrementer le nombre de caractere transmit
		U1TXREG = wTxCar.word;				//init buffer de transmission 		
	}
	else
	{ 
		if(pTxFonc != NULL)
		{
			pTxFonc(PASS);						//appeler la fonction de fin de transmission
		}
		bTxOn = 0;							//fin de transmission
	}	
}

//______________________________________________________________________//
//	Procedure	:	Rs232Send											//
//______________________________________________________________________//
//	Description	:	Init transmission d'une trame sur la rs232			//
//	Entree		:	pbuf --> adresse de rangement des caracteres		//
//					ucNbCar --> nombre de caractere a transmettre		//
//					pFonc --> fonction a appeler en fin de reception    //
//	Sortie		:	Rien												//
//______________________________________________________________________//
void Rs232Send(UCHAR *pbuf, UCHAR ucNbCar, pUARTFONC pfonc)
{
	pTxFonc = pfonc;					//init procedure a appeler en fin de transmission
	pTxBuf = pbuf;						//Pointeur sur le buffer de transmission
	ucTxCar = 0;						//index sur le caractere a transmettre
	ucNbCarTx = ucNbCar;				//Nombre de caractere a transmettre
	bTxOn = 1;							//transmission en cours
	Rs232TxCar();						//start transmission
}

//______________________________________________________________________//
//	Procedure	:	SetComBreakDetect
//______________________________________________________________________//
//	Role		:	Active le timer 4 comme detecteur de silence de
//					ligne
//	Entrées		:	Rien
//	Sorties		:	Rien
//______________________________________________________________________//
static void SetComBreakDetect(void)
{
	/*T4CONbits.TON = 0;
	IFS1bits.T4IF = 0;
	IEC1bits.T4IE = 1;
	TMR4 = 0;
	T4CONbits.TON = 1;*/
}
//______________________________________________________________________//
//	Procedure	:	ResetComBreakDetect
//______________________________________________________________________//
//	Role		:	Desactive le timer 4 comme detecteur de silence de
//					ligne
//	Entrées		:	Rien
//	Sorties		:	Rien
//______________________________________________________________________//
static void ResetComBreakDetect(void)
{
	ucRxState = WAIT_STX;
	/*T4CONbits.TON = 0;
	IFS1bits.T4IF = 0;
	IEC1bits.T4IE = 0;
	TMR4 = 0;*/
}

//==================================================================
//=========================== INTTERUPTION =========================
//==================================================================
//______________________________________________________________________//
//	Procedure	:	_T4Interrupt
//______________________________________________________________________//
//	Role		:	Interruption de l'horloge temps reel
//	Entrées		:	Rien
//	Sorties		:	Rien
//______________________________________________________________________//
/*_OS_ISR _T4Interrupt()
{
	OsIsrIn();
//----------------------------------
	ResetComBreakDetect();
	if(pRxFonc != NULL)
	{
		pRxFonc(FAIL);					//appeler la procedure de fin de reception
	}
//----------------------------------
	OsIsrOut();
}*/

//______________________________________________________________________//
//	Procedure	:	_U1TXInterrupt
//______________________________________________________________________//
//	Role		:	Interruption de transmission UART 1
//	Entrées		:	Rien
//	Sorties		:	Rien
//______________________________________________________________________//
_OS_ISR _U1TXInterrupt()
{
//------------------
	OsIsrIn();
	IFS0bits.U1TXIF = 0;
	Rs232TxCar();				//transmettre un autre caractere
	OsIsrOut();
}
//______________________________________________________________________//
//	Procedure	:	_U1RXInterrupt
//______________________________________________________________________//
//	Role		:	Interruption de reception sur l'UART1
//	Entrées		:	Rien
//	Sorties		:	Rien
//______________________________________________________________________//
_OS_ISR _U1RXInterrupt()
{
WB wRxCar;
static UCHAR ucNbre,ucCheckSum;

	OsIsrIn();    //mark for debug, should recover
//----------------------------------
	IFS0bits.U1RXIF = 0;	
	if(U1STAbits.OERR != 0)
	{
		U1STAbits.OERR = 0;
	}
	else
	{
        //if(U1STAHbits.URXBE == 0)    // (Rx buffer is not empty)
        while(U1STAHbits.URXBE == 0)    //while (Rx buffer is not empty)
		{
			SetComBreakDetect();	//init timer detection fin de trame
			wRxCar.word = U1RXREG;				//Lecture du caractere recu
			ucCheckSum ^= wRxCar.LSB;
			switch( ucRxState)
			{
				case	WAIT_STX:
					if(wRxCar.LSB == ECU_STX)
					{
						ucRxCar = 0;
						ucRxState = GET_NBRE;
						ucCheckSum = 0;
					}
				break;

				case	GET_NBRE:
					pRxBuf[ucRxCar] = wRxCar.LSB;	//ranger le caractere recu
					ucRxCar++;						//pointer le caractere suivant
					ucNbre = wRxCar.LSB;
					ucRxState = GET_DATA;
				break;

				case	GET_DATA:
					if(ucNbre == ucRxCar)
					{
						ucRxState = CHECK_CRC;
					}
					pRxBuf[ucRxCar] = wRxCar.LSB;	//ranger le caractere recu
					ucRxCar++;						//pointer le caractere suivant
				break;

				case	CHECK_CRC:
					if(ucCheckSum == 0)
					{
						ResetComBreakDetect();
						if(pRxFonc != NULL)
						{
							pRxFonc(PASS);
						}
					}
				break;
			}
		}
	}
//----------------------------------
	OsIsrOut();   //mark for debug, should recover
}


