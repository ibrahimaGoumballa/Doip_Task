//=========================================================
// File        		:	Uart.h
// Description      : 	Déclaration des constantes, des structures 
//						et des prototypes liées a l'uart du Dspic
// Created          :	01/08/06
// Author           :	JD
//
// Copyright ATEQ 
// 15 Rue des Dames 
// 78340 Les Clayes sous bois (FRANCE) 
// Tel : +33 1 30 80 10 20
//
//=========================================================
// Date		Version 	History	 
// 2021/04/26           Wolfgang porting to T52088, different register definition from PIC24HJxxxx
//=========================================================

#ifndef _UART_H
#define _UART_H


//=========================================================
//======================== INCLUDE ========================
//=========================================================
typedef void  (*pUARTFONC)( unsigned char ucState);

//=========================================================
//======================= CONSTANTS =======================
//=========================================================

enum
{
	UART_1,
	UART_2,
	N_MAX_UART
};

enum
{
	WAIT_STX,
	GET_NBRE,
	GET_DATA,
	CHECK_CRC
};

enum
{
	RS232_STX,			// Code de synchro début de trame
	RS232_NBDATA,		//nb de donnees transmises code de commande compris
	RS232_CMD,			//Commande
	RS232_DATA			//Data 
};

enum
{
	RS232_RXNBDATA,			//nb de donnees transmises code de commande compris
	RS232_RXCMD,			//Commande
	RS232_RXDATA			//Data 
};

enum
{
	FAIL,
	PASS
};


//#define OSC_FREQUENCY 10000000
//#define PLL 16
//#define FCY (OSC_FREQUENCY*PLL)/4

//decode timeout set to 10ms
#define RS232_BREAK_DETECT	10
#ifdef	_DEBUG_PC 
	#define THE_BAUD_RATE	57600
#else
	#define THE_BAUD_RATE  60900
#endif

//=========================================================
//====================== PROTOTYPES =======================
//=========================================================

void Rs232Init(void);
#ifdef _BOOTLOADER
	void Rs232Send(UCHAR *pbuf, UCHAR ucNbCar);
	UCHAR *Rs232Receive(void);
	void Rs232Ack(UCHAR ucState);
	void Rs232SendInfo(UCHAR ucCmd,UCHAR *pData, UCHAR NbCar);
#else
	void Rs232Send(UCHAR *pbuf, UCHAR ucNbCar, pUARTFONC pfonc);
	void Rs232Receive(UCHAR *pbuf, pUARTFONC pfonc);
#endif
UCHAR Rs232Ready(void);
#endif



