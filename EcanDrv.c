/*______________________________________________________________________ 
|   Project      :Interface comunication OBD carte T52088               |
|   Module       :driver communication can								|
|   File         :EcanDrv.h                                         	|
|_______________________________________________________________________|
|   Description  :Ensemble de procedures communication module Can 1		|
|   Date 23/06/2021 - 											        |
|   Auteur : Wolfgang                                           		|
|_______________________________________________________________________|*/
#include <xc.h>
#include "Kernel.h"
#include <string.h>
#include "Map.h"
#include "../Common_Header/NewType.h"
#include "interrupt.h"
#include "EcanDrv.h"
#include "./mcc_generated_files/can1.h"
#include "./mcc_generated_files/can2.h"

#define _ECAN2			//Ecan 2 present, always present in T52088(dsPIC33CH512)
/*______________________________________________________________________ 
|                        Elements externes                              |
|_______________________________________________________________________|*/
	
/*______________________________________________________________________ 
|                        declaration des constantes                     |
|_______________________________________________________________________|*/
/* CAN Message Buffer Configuration */

//#define ECAN1_MSG_BUF_LENGTH 	4
//typedef unsigned int ECAN1MSGBUF [ECAN1_MSG_BUF_LENGTH][8];

/*______________________________________________________________________ 
|                        Elements publics                               |
|_______________________________________________________________________|*/
/*______________________________________________________________________ 
|                        Elements locaux                                |
|_______________________________________________________________________|*/

static PF pCanRxFonc = NULL;					//procedure a appeler en fin de reception
static UCHAR ucCanRxCar=0;						//Nombre de caractere recus
static UCHAR ucCanNbFrameRxCar=0;				//Nombre de trame a recevoir
static CAN_MSG_OBJ TxCanMsg;
static CAN_MSG_OBJ RxCanMsg;
static UCHAR ucCanChannel = _ECAN1_CHN;			//par defaut channel 1 (can par default)
static UCHAR ucCanFD = FALSE;



/*______________________________________________________________________ 
|   Procedure    : SetEcanChannel	                       	            |
|_______________________________________________________________________|
|   Description  : Selection du port CAN de communication				|
|   de fin de ligne														|
|   En entree: wVal = Etat resistance fin de ligne	                 	|
|		0 --> Deselection resistance fin de ligne		                |
|		1 --> Selection resistance fin de ligne		                 	|
|	En Sortie: Rien											         	|
|_______________________________________________________________________|*/
void SetEcanChannel(UCHAR ucVal)
{
	ucCanChannel = ucVal;							
}

/*______________________________________________________________________ 
|   Procedure    : GetEcanChannel	                       	            |
|_______________________________________________________________________|
|   Description  : Retourne la valeur du channel can selectionne		|
|   En entree: Rien									                 	|
|	En Sortie: 												         	|
|		0 --> _ECAN1_CHN								                |
|		1 --> _ECAN2_CHN							                 	|
|_______________________________________________________________________|*/
UCHAR GetEcanChannel(void)
{
	return(ucCanChannel);
}

CAN_DLC ConvertNdData (UCHAR ucNbData)
{
CAN_DLC ucRet = DLC_0;

	if(ucNbData <=8)
	{
		ucRet = (ucNbData & 0x0F);
	}
	else if (ucNbData <=12)
	{
		ucRet = DLC_12;
	}
	else if (ucNbData <=16)
	{
		ucRet = DLC_16;
	}			
	else if (ucNbData <=20)
	{
		ucRet = DLC_20;
	}		
	else if (ucNbData <=24)
	{
		ucRet = DLC_24;
	}
	else if (ucNbData <=32)
	{
		ucRet = DLC_32;
	}
	else if (ucNbData <=48)
	{
		ucRet = DLC_48;
	}
	else if (ucNbData <=64)
	{
		ucRet = DLC_64;
	}

	return ucRet;
}
/*______________________________________________________________________ 
|   Procedure    : CanTransmit		                       	            |
|_______________________________________________________________________|
|   Description  : Emission d'une trame can (max 8 octets) sur TX0		|
|   En entree: lId = Identifiant Can 				                 	|
|   		   ucExtd = 0--> Id 11 bits				                 	|
|   		   			1--> Id 29 bits				                 	|
|   		   ucpData = ADresse des donnees a transmettre             	|
|   		   ucNbData = Nombre de donnee a transmettre               	|
|   		   pfonc = pointeur sur fonction a appeler en fin         	|
| de transmission											         	|
| En Sortie: TRUE --> la requete c'est bien passee			         	|
| 			 FALSE --> impossible de transmettre				         	|
|_______________________________________________________________________|*/
UCHAR CanTransmit(long lId, UCHAR ucExtd, UCHAR *ucpData, UCHAR ucNbData)
{
	UCHAR	ucRet = FALSE;
    UCHAR ucTmp;
	if(ucpData && ucNbData)
	{
        TxCanMsg.msgId = lId;   //Target ID (EID or SID)
        TxCanMsg.field.frameType = CAN_FRAME_DATA;  //not CAN_FRAME_RTR
        TxCanMsg.field.idType = ucExtd; // _11BITS equals to CAN_FRAME_STD, _29BITS equals to CAN_FRAME_EXT
        TxCanMsg.data = ucpData;    //ex: 8-byte frame {0x02, 0x10, 0x92, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}
                                    // 0x02 for 2-bytes following data, 0x10 and 0x92 are PID and MODE field of OBD2 definition, refer to EcuChrysler.c of VT56/VT41
         
		if(ucCanFD)
		{
			TxCanMsg.field.dlc = ConvertNdData(ucNbData);
			TxCanMsg.field.brs = CAN_BRS_MODE; //bit rate switch only in CAN FD will be set to 1
			TxCanMsg.field.formatType = CAN_FD_FORMAT; //not CAN_FD_FORMAT
		}
		else
		{
			TxCanMsg.field.dlc = (ucNbData & 0x0F); //dlc is 4-bit field
			TxCanMsg.field.brs = CAN_NON_BRS_MODE; //bit rate switch only in CAN FD will be set to 1
			TxCanMsg.field.formatType = CAN_2_0_FORMAT; //not CAN_FD_FORMAT
		}
		
		//ecanSendMessage(&TxCanMessage, GetEcanChannel());	//Tx ECAN1 ou ECAN2 
        if( _ECAN1_CHN == GetEcanChannel() )
        {
            ucTmp = CAN1_Transmit(CAN1_TXQ, &TxCanMsg);
            if(ucTmp == CAN_TX_MSG_REQUEST_SUCCESS)
            {
                ucRet = TRUE;
            }
        }
        else if( _ECAN2_CHN == GetEcanChannel() )
        {
            ucTmp = CAN2_Transmit(CAN2_TXQ, &TxCanMsg);
            if(ucTmp == CAN_TX_MSG_REQUEST_SUCCESS)
            {
                ucRet = TRUE;
            }
        }
	}
	return(ucRet);
}

/*______________________________________________________________________ 
|   Procedure    : CanReceive		                       	            |
|_______________________________________________________________________|
|   Description  : Reception d'une ou plusieures trames can sur RX1		|
|   En entree: ucpData = ADresse de rangement des donnees a recevoir	|
|   		   ucNbFrame = Nombre de trames a recevoir              	|
|   		   pfonc = pointeur sur fonction a appeler en fin         	|
| de reception												         	|
| En Sortie: TRUE --> la requete c'est bien passee			         	|
| 			 FALSE --> impossible de transmettre				         	|
|_______________________________________________________________________|*/
UCHAR CanReceive(UCHAR *ucpData, UCHAR ucNbFrame, PF pfonc)
{
	UCHAR ucRet = FALSE;
	if(ucpData && ucNbFrame)
	{
		pCanRxFonc = pfonc;					//procedure a appeler en fin de reception
		ucCanNbFrameRxCar=ucNbFrame;		//Init Nombre de trame a recevoir
        RxCanMsg.data = ucpData;        //assign buffer begin address
		ucCanRxCar = 0;						//init nombre de caractere recus
	}
	return(ucRet);
}

/*______________________________________________________________________ 
|   Procedure    : ecan1SetMode		                       	            |
|_______________________________________________________________________|
|   Description  : Selection du mode de fonctionnement du module can	|
|   En entree: sMode = Mode de fonctionnement		                 	|
|				00 = Set Normal Operation mode							|
|				01 = Set Disable mode									|
|				02 = Set Loopback mode									|
|				03 = Set Listen Only Mode								|
|				04 = Set Configuration mode								|
|				05 = Reserved – do not use								|
|				06 = Reserved – do not use								|
|				07 = Set Listen All Messages mode						|
|   En sortie: TRUE ou FALSE						                 	|
|_______________________________________________________________________|*/
UCHAR ecanSetMode(short sMode)
{
UCHAR ucRet = FALSE;
WORD wTempo = 4000;
	C1CONHbits.REQOP = sMode &0x7;			//REQOP consist of 3 bits
	while( (C1CONHbits.OPMOD != C1CONHbits.REQOP) && (wTempo > 0) )
	{//wait until request mode == operating mode
		wTempo--;	
	}
	if(wTempo)
	{
		ucRet = TRUE;				//tout va bien
	}

	wTempo = 4000;
	ucRet = FALSE;
    if(ECAN_NORMAL_FD_MODE == sMode)
        C2CONHbits.REQOP = ECAN_NORMAL_2_0_MODE;
    else
        C2CONHbits.REQOP = sMode &0x7;			//REQOP consist of 3 bits
    
	while( (C2CONHbits.OPMOD != C2CONHbits.REQOP) && (wTempo > 0) )
	{//wait until request mode == operating mode
		wTempo--;	
	}
	if(wTempo)
	{
		ucRet = TRUE;				//tout va bien
	}	

	return(ucRet);
}

// ****************************************************************************
// * Fonction    : ecanSetRxAcptFilterAndMask
// * Description : Configuration des filtres (acceptance filter)
// * identifier-> Bit ordering is given below
// * Filter Identifier (29-bits) : 0b000f ffff ffff ffff ffff ffff ffff ffff
// * 								    |____________||____________________|
// * 									  SID10:0           EID17:0
// * 
// * Filter Identifier (11-bits) : 0b0000 0000 0000 0000 0000 0fff ffff ffff
// *                                                           |___________|
// * 															  SID10:0
// * 
// ****************************************************************************
void ecanSetRxAcptFilterAndMask(long identifier, long mask, unsigned int exide)
{//T52088 only uses one filter (filter0), and only has one FIFO (FIFO1)
    
    //29-bit example: identifier = 0x14DAF145, then SID = 0b1_0100_1101_10, EID = 0x2F145
    //11-bit example: identifier = 0x620, then SID = 0x620, EID = 0
    
    //-----CAN 1 section-----
    C1FLTCON0Lbits.FLTEN0 = 0;  // Disable filter 0
    
    C1FLTCON0Lbits.F0BP = 0x01; // message stored in FIFO1
    
    if(exide == _29BITS)
    {
        C1FLTOBJ0Lbits.EID = (UCHAR)(identifier & 0x0000001F);  //EID[4..0]
        C1FLTOBJ0Lbits.SID = (WORD)((identifier & 0x1FFC0000) >> 18);   //SID[10..0]
        C1FLTOBJ0Hbits.EID = (WORD)((identifier & 0x0003FFE0) >> 5); //EID[17..5]
        C1FLTOBJ0Hbits.EXIDE = 1;
        C1MASK0L = 0xFFFF;
        C1MASK0H = 0x1FFF;
    }
    else
    {
        C1FLTOBJ0L = (WORD)(identifier & 0x000007FF); //EID[4..0] and SID[10..0]
        C1FLTOBJ0H = 0x0000; // EID[17..5], SID11 = 0, EXIDE = 0
        C1MASK0L = 0x07FF;
        C1MASK0H = 0;
    }
    C1MASK0Hbits.MIDE = 1;
    
    C1FLTCON0Lbits.FLTEN0 = 1;  // Enable filter 0
    //-----end of CAN 1 section-----
    
    //-----CAN 2 section-----
    C2FLTCON0Lbits.FLTEN0 = 0;  // Disable filter 0
    
    C2FLTCON0Lbits.F0BP = 0x01; // message stored in FIFO1
    
    //all settings are the same in CAN1 and CAN2
    C2FLTOBJ0L = C1FLTOBJ0L;
    C2FLTOBJ0H = C1FLTOBJ0H;
    
    C2MASK0L = C1MASK0L;
    C2MASK0H = C1MASK0H;
    
    C2FLTCON0Lbits.FLTEN0 = 1;  // Enable filter 0
    //-----end of CAN 2 section-----
}//end of ecanSetRxAcptFilterAndMask(...)



/*______________________________________________________________________ 
|   procedure:  ecanSetBitTimingAndBaudRate                    	     	|
|_______________________________________________________________________|
|  sync(1TQ)  |          Seg1(31TQ)              | Seg2(8TQ)            |
|_____________|__________________________________|______________________|
|                   Sample point located between Seg1 and Seg2 (80%)    |
|   TQ is Time Quanta, smaller than 1bit of CAN signal                  |
| example: Fcan = 40MHz, lBaudRate = 500Kbps, cNbTq = 40 = 40TQ         |
| means 1TQ = 2Fcan = 50ns, 40TQ = 2000ns = 2us = 500KHz                |
|                                                                       |
| Total TQ = Sync + Seg1 + seg2                             			|
| Constraint 1: seg1 > seg2												|
| Constraint 2: seg2 >= SJW												|
| SJW is the value that can be adjust while re-synchronization			|
|                                                                       |
|_________________________________ _____________________________________|*/
UCHAR ecanSetBitTimingAndBaudRate(long lBaudRate)
{
    //default value 1TQ(sync) + 63TQ(seg1) + 16TQ(seg2), total 80TQ = 1bit of CAN 500Kbps = 2us for example
	UCHAR ucSeq1 = _63TQ;
	UCHAR ucSeg2 = _16TQ;
	UCHAR ucSJW = _10TQ;
	UCHAR ucNbTq = 3 + ucSeg2 + ucSeq1; //1 TQ sync +1 for enum alignement for each SEG = 3
	UCHAR ucBRP = (_FREQ_CAN / (ucNbTq*lBaudRate)) - 1;
        
   
	//-----CAN 1 section-----
	C1NBTCFGLbits.SJW = (ucSJW & 0x7F);
	C1NBTCFGLbits.TSEG2 = (ucSeg2 & 0x7F);   //default = _8TQ

	C1NBTCFGHbits.TSEG1 = ucSeq1;    //default = _31TQ
	C1NBTCFGHbits.BRP = ucBRP;
	//example: Fcan = 40MHz, lBaudRate = 500Kbps, cNbTq = 40, then BRP = 1
	//means 1TQ = 2Fcan = 50ns, 40TQ = 2000ns = 2us = 500KHz

	//---CAN FD settings---
	//data bit rate, set to the same as nominal bit rate
	C1DBTCFGL = C1NBTCFGL;
	C1DBTCFGH = C1NBTCFGH;
	//C1TDCL = 0x1F00;    // TDCV 0; TDCO 31;
	C1TDCL = 0x0000;    // TDCV 0; TDCO 0;
	C1TDCH = 0x02;  // EDGFLTEN disabled; TDCMOD Auto; SID11EN disabled;
	//---end of CAN FD settings---
	//-----end of CAN 1 section-----

	//-----CAN 2 section-----
	C2NBTCFGL = C1NBTCFGL;
	C2NBTCFGH = C1NBTCFGH;
	//-----end of CAN 2 section-----
	ucCanFD = FALSE;

    
    return ucNbTq;
}

UCHAR ecanSetBitTimingAndBaudRateFDDataphase(long lBaudRate)
{
    //default value 1TQ(sync) + 31TQ(seg1) + 8TQ(seg2), total 40TQ = 1bit of CAN 1Mbps = 2us for example
    UCHAR ucSeq1,ucSeg2,ucSJW,ucNbTq,ucBRP = 0; 
	
	switch(lBaudRate)
	{
		case _500KBITS:
			ucSeq1 = _31TQ;
			ucSeg2 = _8TQ;
			ucSJW = _5TQ;
			ucNbTq = 3 + ucSeg2 + ucSeq1;
			ucBRP = 3;
		break;

		case _1MBITS:
			ucSeq1 = _31TQ;
			ucSeg2 = _8TQ;
			ucSJW = _5TQ;
			ucNbTq = 3 + ucSeg2 + ucSeq1;
			ucBRP = 1;			
		break;
		
		case _2MBITS:
			ucSeq1 = _15TQ;
			ucSeg2 = _4TQ;
			ucSJW = _4TQ;
			ucNbTq = 3 + ucSeg2 + ucSeq1;
			ucBRP = 1;			
		break;
		
		case _4MBITS:
			ucSeq1 = _7TQ;
			ucSeg2 = _2TQ;
			ucSJW = _2TQ;
			ucNbTq = 3 + ucSeg2 + ucSeq1;
			ucBRP = 1;			
		break;
				
		case _5MBITS:
			ucSeq1 = _2TQ;
			ucSeg2 = _1TQ;
			ucSJW = _1TQ;
			ucNbTq = 3 + ucSeg2 + ucSeq1;
			ucBRP = 3;			
		break;
		
		case _8MBITS:
			ucSeq1 = _3TQ;
			ucSeg2 = _1TQ;
			ucSJW = _1TQ;
			ucNbTq = 3 + ucSeg2 + ucSeq1;
			ucBRP = 1;			
		break;
		
		default:
			ucSeq1 = _3TQ;
			ucSeg2 = _1TQ;
			ucSJW = _1TQ;
			ucNbTq = 3 + ucSeg2 + ucSeq1;
			ucBRP = (_FREQ_CAN / (ucNbTq*lBaudRate)) - 1;			
		break;
	}
    
	//-----CAN 1 section-----
	//---CAN FD settings---
	C1DBTCFGLbits.SJW = (ucSJW & 0x0F);
	C1DBTCFGLbits.TSEG2 = (ucSeg2 & 0x0F);   //default = _8TQ

	C1DBTCFGHbits.TSEG1 = (ucSeq1 & 0x1F);    //default = _31TQ
	C1DBTCFGHbits.BRP = ucBRP;

	C1TDCL = 0x1F00;    // TDCV 0; TDCO 31;
	//C1TDCL = 0x0000;    // TDCV 0; TDCO 0;
	C1TDCH = 0x02;  // EDGFLTEN disabled; TDCMOD Auto; SID11EN disabled;
	//---end of CAN FD settings---
	//-----end of CAN 1 section-----

	//-----CAN 2 section-----
	// CAN 2 is low speed, can't be FD
	//-----end of CAN 2 section-----
	ucCanFD = TRUE;
        
    return ucNbTq;
}


void ecanInit (void)
{
    //Request Configuration Mode
	ecanSetMode(ECAN_CONFIG_MODE);

    //default set 500Kbps, 40TQ = 1 sync + 31 seg1 + 8 seg2, SJW = 5TQ
    ecanSetBitTimingAndBaudRate(_500KBITS);
    
    ecanSetRxAcptFilterAndMask(0x708, 0x1FFFFFFF, _11BITS);
		
	ecanSetMode(ECAN_DISABLE_MODE);				//Disable can by default
			

//---CAN1/CAN2 Interrupt Register---
    C1INTHbits.RXIE = 1;
    IPC7bits.C1RXIP = IT_CAN_1_LEVEL;
    IEC1bits.C1RXIE = 1;
    
    C2INTHbits.RXIE = 1;
	IPC8bits.C2RXIP	= IT_CAN_1_LEVEL;
	IEC2bits.C2RXIE	= 1;
//---end of CAN1/CAN2 Interrupt Register---
	ucCanFD = FALSE;
}//end of ecanInit(...)

	
_OS_ISR _C1RXInterrupt()
{
    OsIsrIn ();

    if(IFS1bits.C1RXIF)
    {
        IFS1bits.C1RXIF = 0;
    }
    
    if(C1INTLbits.RXIF)
    { 
        CAN1_Receive(&RxCanMsg);    //read RX FIFO data to RxCanMsg.data[], address is assigned at CanReceive(...)
        if(ucCanNbFrameRxCar)
        {
            ucCanRxCar += RxCanMsg.field.dlc;   //data length code
            ucCanNbFrameRxCar--;					//decrementer le nombre de trame a recevoir

            if((ucCanNbFrameRxCar == 0) && pCanRxFonc)
            {
                pCanRxFonc();						//appeler la fonction de fin de reception
            } 
            else
            {
                RxCanMsg.data += RxCanMsg.field.dlc;   //assign new pointer address in RxCanMsg
            }
        }
        
        C1INTLbits.RXIF = 0;
    }
    
    OsIsrOut ();
}


_OS_ISR _C2RXInterrupt()
{
    OsIsrIn ();
    
    if(IFS2bits.C2RXIF)
    {
        IFS2bits.C2RXIF = 0;
    }
    
    if(C2INTLbits.RXIF)
    { 
        CAN2_Receive(&RxCanMsg);    //read RX FIFO data to RxCanMsg.data[], address is assigned at CanReceive(...)
        if(ucCanNbFrameRxCar)
        {
            ucCanRxCar += RxCanMsg.field.dlc;   //data length code
            ucCanNbFrameRxCar--;					//decrementer le nombre de trame a recevoir

            if((ucCanNbFrameRxCar == 0) && pCanRxFonc)
            {
                pCanRxFonc();						//appeler la fonction de fin de reception
            } 
            else
            {
                RxCanMsg.data += RxCanMsg.field.dlc;   //assign new pointer address in RxCanMsg
            }
        }
        
        C2INTLbits.RXIF = 0;
    }
    
    OsIsrOut ();
}


