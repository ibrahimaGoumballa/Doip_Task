/*______________________________________________________________________ 
|   Project      :Interface comunication OBD carte 520_84               |
|   Module       :driver communication obd 								|
|   File         :Obddrv.h                                         		|
|_______________________________________________________________________|
|   Description  :Ensemble de procedure pour transmettre et recevoir	|
|   des donnees sur la ligne K_LINE										|
|   Date 21/04/2008 - 											        |
|   Auteur : David BARON										        |
|_______________________________________________________________________|*/
#ifndef __OBDDRV
#define __OBDDRV


enum {
	_NO_BOARD,
	_T520_84,		//nouvelle carte OBD
	_520_84,		//ancienne carte obd
    _T520_88        //3rd generation OBD module
};	

#define	_10400Baud	10400		//periode 1 bit  = 96153 nano s
#define _9600Baud	9600		//periode 1 bit = 104166 nano s	

//definition des octets d'une trame Obd
#define Fmt	0
#define Tgt	1
#define Src	2
#define Len	3
#define Data 4

//definition des octets d'une trame Honda h99
#define Hb		0		//header byte
#define Mlen	1 		//message length
#define IsoData	2

//definition des octets d'une trame Iso9141
#define Iso9141Hb		0		//header byte 1
#define Iso9141Hb1		1 		//header byte 2
#define Iso9141Src		2 		//ecu source
#define Iso9141Data		3 		//Debut data

/*______________________________________________________________________ 
|   Definition prototypes fonctions										|
|_______________________________________________________________________|*/
void InitKLine(void);
void UART2Send1ByteAndWaitLastBitShiftOut(UCHAR ucData);
void ReceiveKLine();			//begin to receive K-LINE frame
void TxBitObd(void);
void RxCarObd(void);
void DisableRxCarObd(void);
void DisableUART2();
UCHAR SetProtocoleObd(UCHAR cType);			//selection du type de protocole
UCHAR GetProtocoleObd(void);
UCHAR GetNbCarRxObd(void);					//recuperation nombre de caracteres recus sur OBD
UCHAR ObdCrc(UCHAR *pBuf);					//calcul crc Obd
UCHAR ObdIsoCrc(UCHAR *pBuf);				//calcul crc Iso9141
//UCHAR *GetRxBufObd(void);					//recuperation pointeur sur le buffer de reception
void SetKLineRxBuffer(UCHAR *pBuf);
void SetFoncEndRxObd(PF pFonf);				//parametrage fonction de fin reception
void SetBaudRateKLine(long lBaudRate);
void SetBaudRateIndexKLine(UCHAR ucBaudRateIdx);
void ServiceEspion(void);
UCHAR SetKlineOutputObd(UCHAR cNumPin, UCHAR cState);	//selection pin de sortie ligne k_line
//T520_84 nouvelle carte
UCHAR GetTypeCarteObd(void);
void SetPinObd( WORD wVal);
//T520_84
void EnableUART2TX();

#endif

