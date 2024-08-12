/*______________________________________________________________________ 
|   Project      :Interface comunication OBD carte T52088               |
|   Module       :driver communication can								|
|   File         :EcanDrv.h                                         	|
|_______________________________________________________________________|
|   Description  :Ensemble de procedures communication module Can 1		|
|   Date 22/06/2021 - 											        |
|   Auteur : Wolfgang                   						        |
|_______________________________________________________________________|*/

#ifndef __ECAN_DRV_H__
#define __ECAN_DRV_H__

//==================================================================
//=========================== CONSTANTS ============================
//==================================================================
#define _FREQ_CAN	80000000				//CAN clock source set to Fvco / 2 = 560MHz, divided by 7 (CANDIV[6..0] value) becomes 80MHz
#define _8MBITS		8000000					//8000000 kbits / sec
#define _5MBITS		5000000					//5000000 kbits / sec
#define _4MBITS		4000000					//4000000 kbits / sec
#define _2MBITS		2000000					//2000000 kbits / sec
#define _1MBITS		1000000					//1000000 kbits / sec
#define _500KBITS	500000					//500000 kbits / sec
#define _250KBITS	250000					//200000 kbits / sec
#define _125KBITS	125000					//125000 kbits / sec

enum{
	_1TQ,
	_2TQ,
	_3TQ,
	_4TQ,
	_5TQ,
	_6TQ,
	_7TQ,
	_8TQ = 7,
	_9TQ,
	_10TQ,
	_15TQ = 14,
	_16TQ = 15,
    _31TQ = 30,
	_63TQ = 62,
	};
//definition mode de fonctionnement module ecan1
enum{
	ECAN_NORMAL_FD_MODE,			//00 = Set Normal Operation mode
	ECAN_DISABLE_MODE,				//01 = Set Disable mode
	ECAN_LOOPBACK_MODE,				//02 = Set Loopback mode
	ECAN_LISTEN_ONLY_MODE,			//03 = Set Listen Only Mode	
	ECAN_CONFIG_MODE,				//04 = Set Configuration mode
	ECAN_RESERVED1_MODE,			//05 = Reserved – do not use
	ECAN_NORMAL_2_0_MODE,			//06 = CAN 2.0 only, FD packet will not be accepted
	ECAN_RESTRICTED_OPERATION_MODE	//07 = Set Listen All Messages mode
	};

enum{
	_11BITS,
	_29BITS
	};


enum{
	_ECAN1_CHN,						//ECAN1
	_ECAN2_CHN,						//ECAN2
	_ECAN_CHN_MAX
};	


//==================================================================
//=========================== CONSTANTS ============================
//==================================================================

typedef struct		// Structure d'un message CAN
{
	ULONG	Address;		// Id du message
	BYTE	Ext:1,			// Mode standard ou etendu
			NoOfBytes:4,	// Nombre d'octets recus
			Remote:1,		// Mode Remote
			NotUse:2;
	BYTE	bAlignement;	// Pour l'alignement
	BYTE	*ucpData;		//pointeur sur les donnees a emmettre ou a recevoir			
} CANMessage;

/*************************/
/** Fonctions publics	**/
/*************************/
void 	ecanInit(void);
UCHAR 	ecanSetMode(short sMode);
void    ecanSetRxAcptFilterAndMask(long identifier, long mask, unsigned int exide);
//void	ecanWriteRxAcptFilter	(int n, long identifier, unsigned int exide,unsigned int bufPnt,unsigned int maskSel);
//void	ecanWriteRxAcptMask	(int m, long identifierMask, unsigned int mide);
UCHAR	CanTransmit(long lId, UCHAR ucExtd, UCHAR *ucpData, UCHAR ucNbData);
UCHAR	CanReceive(UCHAR *ucpData, UCHAR ucNbFrame, PF pfonc);
//void	ecanSendMessage (CANMessage *pMsg, BYTE bNumBuf);

//void SetR120Can(WORD wVal);
void SetEcanChannel(UCHAR ucVal);
UCHAR GetEcanChannel(void);

//WORD GetR120Can(void);

UCHAR ecanSetBitTimingAndBaudRate(long lBaudRate);
UCHAR ecanSetBitTimingAndBaudRateFDDataphase(long lBaudRate);
//UCHAR ecanSetBitTiming (UCHAR cPropSeg, UCHAR cSeg1, UCHAR cSeg2, UCHAR cSJW);
//long ecanSetBaudrate(long lBaudRate, long lFreq , UCHAR cNTq);




#endif
