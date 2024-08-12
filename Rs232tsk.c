/*______________________________________________________________________ 
|   Project      :Interface comunication OBD carte 520_88               |
|   Module       :Tache communication rs232								|
|   File         :Rs232Tsk.c                                         	|
|_______________________________________________________________________|
|   Description  :Decodage trame rs232 venant du VT56           		|
|   Date 25/05/2021 - 											        |
|   Auteur : Wolfgang   										        |
|_______________________________________________________________________|*/
#include <xc.h>
#include "../Common_Header/NewType.h"
#include "Kernel.h"
#include <string.h>
#include "rs232tsk.h"
#include "uart.h"
#include "obddrv.h"       //mark for debug, should recover
#include "obdtsk.h"       //mark for debug, should recover
#include "cantsk.h"       //mark for debug, should recover
#include "Map.h"
#include "interrupt.h"
#include "adcdrv.h"       
#include "EcanDrv.h"       //mark for debug, should recover
#if defined _DoIP
#include "DoIP/Ethernet.h"
#endif
#include "Buffer.h"
#include "../Common_OBD_Data/ObdSavedData.h"


/*______________________________________________________________________ 
|                        Elements externes                              |
|_______________________________________________________________________|*/
void SetTimerResetInit(WORD wNewTime);
/*______________________________________________________________________ 
|                        declaration des constantes                     |
|_______________________________________________________________________|*/
//liste des evenement decodes par la tache communication OBD
enum
{
	EVT_DECODE_TRAME = MAX_OS_EVT,
	EVT_TX_ECU_REPONSE,
	EVT_STARTUP
};

#define	_T100ms	100
#define _T4s	4000
/*______________________________________________________________________ 
|                        Elements publics                               |
|_______________________________________________________________________|*/
/*______________________________________________________________________ 
|                        Elements locaux                                |
|_______________________________________________________________________|*/
static SEMAPHORE Rs232TimerSem ={0,0,0};			//semaphore attente sur timer 
static SEMAPHORE Rs232Sem ={0,0,0};
static EVENT_DEF *pMsgRs232;				//pointeur sur message tache rs232
static UCHAR cEvtRs232;						//evenement tache rs232

//static UCHAR cBufTxRs232[MAX_UART_BUFF];			//buffer emission 
//static UCHAR cBufRxRs232[MAX_UART_BUFF];			//buffer reception
static RS232_BUFFER sBufTxRs232;			//buffer emission 
static RS232_BUFFER sBufRxRs232;			//buffer reception
static WORD wTimeoutRs232;
static UCHAR cNbCarRs232 = 0;						//nombre de caractere emis ou recus sur la rs232
void Rs232DecodeFrame(unsigned char ucState);

static UCHAR uObdBufRx[MAX_UART_BUFF];			//buffer de reception

static OBD_BUF *pRs232Buf = NULL;

void GoToBoot (void)
{
	//wait ~4 second to simulate end of connection for VT
	do
	{
		pMsgRs232 = OsWaitEvent(&Rs232TimerSem,_T4s,T_STOP_ON_EVT);
	}
	while(pMsgRs232->ucEvt != EVT_TIMER);
	RunBoot();
}


/*______________________________________________________________________ 
|   Procedure    : Rs232EndTx		                       	            |
|_______________________________________________________________________|
|   Description  : Procedure appelee en fin transmission				|
|   En entree: Rien									                 	|
|   En sortie: Rien									                 	|
|_______________________________________________________________________|*/
void Rs232EndTx(unsigned char ucState)
{
	if(ucState == PASS)
	{
		Rs232Receive(sBufRxRs232.ucBuf, Rs232DecodeFrame);			//attente reception trame rs232
	}
}
/*______________________________________________________________________ 
|   Procedure    : Rs232Ack			                       	            |
|_______________________________________________________________________|
|   Description  : Preparer la transmission d'un ACK					|
|   En entree: Rien									                 	|
|   En sortie: Rien									                 	|
|_______________________________________________________________________|*/
void Rs232Ack(void)
{
	sBufTxRs232.ucBuf[RS232_STX] = ECU_STX;
	sBufTxRs232.ucBuf[RS232_NBDATA] = 1;
	sBufTxRs232.ucBuf[RS232_CMD] = ECU_ACK;
	sBufTxRs232.ucBuf[RS232_DATA] = ECU_ACK ^ 0x1;
	Rs232Send(sBufTxRs232.ucBuf,4, Rs232EndTx);

}

/*______________________________________________________________________ 
|   Procedure    : Rs232NAck		                       	            |
|_______________________________________________________________________|
|   Description  : Preparer la transmission d'un NACK					|
|   En entree: Rien									                 	|
|   En sortie: Rien									                 	|
|_______________________________________________________________________|*/
void Rs232NAck(void)
{
	sBufTxRs232.ucBuf[RS232_STX] = ECU_STX;
	sBufTxRs232.ucBuf[RS232_CMD] = ECU_NAK;
	sBufTxRs232.ucBuf[RS232_NBDATA] = 1;
	sBufTxRs232.ucBuf[RS232_DATA] = ECU_NAK ^ 0x1;
	Rs232Send(sBufTxRs232.ucBuf,4, Rs232EndTx);
}


/*______________________________________________________________________ 
|   Procedure    : Rs232DecodeFrame	                       	            |
|_______________________________________________________________________|
|   Description  : Demande de decode d'une trame rs232					|
|   En entree: Rien									                 	|
|   En sortie: Rien									                 	|
|_______________________________________________________________________|*/
void Rs232DecodeFrame(unsigned char ucState)
{
	if(ucState == PASS)
	{
		OsSendEvent(&Rs232Sem,EVT_DECODE_TRAME,NULL);		//emission evenement
	}
	else
	{
		Rs232NAck();
	}
}


/*______________________________________________________________________ 
|   Procedure    : Rs232SendDataObd	                       	            |
|_______________________________________________________________________|
|   Description  : Data from K_LIne send to VT56 through RS232      	|
|                   													|
|   En entree: pData --> pointeur sur les donnees a transmettre        	|
|   		   NbCar --> Nombre d'octets a transmettre			       	|
|   En sortie: Rien									                 	|
|_______________________________________________________________________|*/
void Rs232SendDataObd(UCHAR *pData, UCHAR NbCar)
{
unsigned char ucChecksum;
unsigned char i;
	if(pData)
	{
		cNbCarRs232 = 4+NbCar;									// (STX + LONG + CMD + CHECKSUM) + Data ...
		sBufTxRs232.ucBuf[RS232_STX] = RS232CMD_ECU_STX;
		ucChecksum = (NbCar + 1);
		sBufTxRs232.ucBuf[RS232_NBDATA] = ucChecksum ;				//on inclue le code de comande
		sBufTxRs232.ucBuf[RS232_CMD] = RS232CMD_SEND_DATA;
		ucChecksum ^= RS232CMD_SEND_DATA;				//on inclue le code de comande
		for(i = 0; i < NbCar; i ++)
		{
			sBufTxRs232.ucBuf[RS232_DATA+i] = pData[i];
			ucChecksum ^= pData[i];
		}
		sBufTxRs232.ucBuf[RS232_DATA+i] = ucChecksum;
		OsSendEvent(&Rs232Sem,EVT_TX_ECU_REPONSE,NULL);
	}
}

void Rs232SendDataWInfoObd(UCHAR *pData, UCHAR NbCar,UCHAR DataType)
{
unsigned char ucChecksum;
unsigned char i;
	if(pData)
	{
		cNbCarRs232 = 4+NbCar;									// (STX + LONG + CMD + CHECKSUM) + Data ...
		sBufTxRs232.ucBuf[RS232_STX] = RS232CMD_ECU_STX;
		ucChecksum = (NbCar + 1);
		sBufTxRs232.ucBuf[RS232_NBDATA] = ucChecksum ;				//on inclue le code de comande
		sBufTxRs232.ucBuf[RS232_CMD] = RS232CMD_SEND_DATA_W_INFO;
		ucChecksum ^= ECU_SEND_DATA;				//on inclue le code de comande
        sBufTxRs232.ucBuf[RS232_DATA] = DataType;
        ucChecksum ^= DataType;
		for(i = 1; i < NbCar; i ++)
		{
			sBufTxRs232.ucBuf[RS232_DATA+i] = pData[i];
			ucChecksum ^= pData[i];
		}
		sBufTxRs232.ucBuf[RS232_DATA+i] = ucChecksum;
		OsSendEvent(&Rs232Sem,EVT_TX_ECU_REPONSE,NULL);
	}
}

/*______________________________________________________________________ 
|   Procedure    : EndReceiveKLinePacket	                       	            |
|_______________________________________________________________________|
|   Description  : Fonction appelee en fin de reception d'une trame 	|
|   sur la K_Line														|
|   En entree: Rien												       	|
|   En sortie: Rien									                 	|
|_______________________________________________________________________|*/
void EndReceiveKLinePacket(void)
{
short sNbCar;

	sNbCar = GetNbCarRxObd();		//received K-LINE packet size
	if(sNbCar)
	{
		//send received K-LINE packet to VT57 tool
		Rs232SendDataObd(uObdBufRx,sNbCar);
	}
	ReceiveKLine();		//begin to receive next packet
}

//T520_84 Nouveau module Obd gestion des switch
/*______________________________________________________________________ 
|   Procedure    : ConfigCanObd		                       	            |
|_______________________________________________________________________|
|   Description  : Configuration de la communication obd ou can			|
|   En entree: pData --> pointeur sur les donnees de la configuration 	|
|   En sortie:	TRUE  --> config ok					                 	|
|   			FALSE --> la config a echouee							|
|_______________________________________________________________________|*/
UCHAR ConfigCanObd(UCHAR *pData)
{
OBD2_PROTOCOLE_DEF StrConfObd;							//config protocole obd
UCHAR cRet = FALSE;
    //marked for debug, should recover, wolfgang, 20210607
	if(pData)
	{	
		memcpy(&StrConfObd,pData, sizeof(OBD2_PROTOCOLE_DEF));
		//init config Can
        if(StrConfObd.wProtocol >= DOIP_FRAME)
        {//DoIP
            ecanSetMode(ECAN_DISABLE_MODE);	//disable Can module
        }
		else if(StrConfObd.wProtocol >= CAN_11BIT_FRAME)					//Index du protocole sélectionné
		{
//Selection des PIN de sorties OBD + selection resistance CAN 120 ohm 
//L'affectation physique des PIN OBD est faite dans SetProtocoleObd
			if((StrConfObd.wProtocol == CAN_29BIT_8BYTE_FRAME) ||
				(StrConfObd.wProtocol == CAN_29BIT_FRAME_EXT) ||
					(StrConfObd.wProtocol == CAN_29BIT_8BYTE_FRAME_EXT) ||
							(StrConfObd.wProtocol == CAN_29BIT_FRAME))
			{
				SetNbBitIdCan(_29BITS);							//29 bits d'identifiants
			}
			else
			{
				SetNbBitIdCan(_11BITS);							//11 bits d'identifiants
			}

//V1.41 ajout can extended format
			if((StrConfObd.wProtocol == CAN_11BIT_FRAME_EXT) ||
				(StrConfObd.wProtocol == CAN_11BIT_8BYTE_FRAME_EXT) ||
				(StrConfObd.wProtocol == CAN_29BIT_FRAME_EXT) ||
				(StrConfObd.wProtocol == CAN_29BIT_8BYTE_FRAME_EXT))
			{
				SetCanExtended_N_TA((UCHAR)StrConfObd.wExtended_N_TA);	//Init Network Target Adress
			}

//V1.41 
			SetEcuTargetId(StrConfObd.dwTargetAddr);		//init Can target Id
			SetEcuSrcId(StrConfObd.dwSourceAddr);			//init can rx Id
			SetBaudRateCan(StrConfObd.dwBaudRate, StrConfObd.dwBaudRate);			//init vitesse de communication Can
			SetFlowControlCan(StrConfObd.wBlockSize, StrConfObd.wFrameDelay);	//init block size et delay inter trame
		}
		else
		{//init config K_line
//DB2 Modif plante Can Honda
			ecanSetMode(ECAN_DISABLE_MODE);	//disable Can module
//DB2 Modif plante Can Honda

//520_84D

			//ReceiveKLine(uObdBufRx);				//Init pointeur sur buffer reception
            SetKLineRxBuffer(uObdBufRx);
//QD1-42d
			SetFoncEndRxObd(EndReceiveKLinePacket);		//Init fonction de fin reception obd

			SetBaudRateKLine(StrConfObd.dwBaudRate);	//init vitesse de communication uart K_Line
//Iso9141
			SetConfObd((UCHAR)StrConfObd.dwTargetAddr, 
						(WORD)StrConfObd.wCharacterDelay,
								(WORD)StrConfObd.wStartupLowTime, 
										(WORD)StrConfObd.wStartupHighTime);	//Adresse + configuration du delai inter caractere + tempo startup
//Iso9141
		}
		
        SetPinObd(StrConfObd.wSelectKLine);	
		cRet = SetProtocoleObd(StrConfObd.wProtocol);				//selection du protocole
	}
	return(cRet);
}

UCHAR ConfigCanFdObd(UCHAR *pData)
{
OBD2_PROTOCOLE_DEF_FD StrConfObd;							//config protocole obd
UCHAR cRet = FALSE;
    //marked for debug, should recover, wolfgang, 20210607
	if(pData)
	{	
		memcpy(&StrConfObd,pData, sizeof(OBD2_PROTOCOLE_DEF_FD));
		//init config Can
        if(StrConfObd.wProtocol >= DOIP_FRAME)
        {//DoIP
            ecanSetMode(ECAN_DISABLE_MODE);	//disable Can module
        }
		else if(StrConfObd.wProtocol >= CAN_11BIT_FRAME)					//Index du protocole sélectionné
		{
//Selection des PIN de sorties OBD + selection resistance CAN 120 ohm 
//L'affectation physique des PIN OBD est faite dans SetProtocoleObd
			if((StrConfObd.wProtocol == CAN_29BIT_8BYTE_FRAME) ||
				(StrConfObd.wProtocol == CAN_29BIT_FRAME_EXT) ||
					(StrConfObd.wProtocol == CAN_29BIT_8BYTE_FRAME_EXT) ||
							(StrConfObd.wProtocol == CAN_29BIT_FRAME))
			{
				SetNbBitIdCan(_29BITS);							//29 bits d'identifiants
			}
			else
			{
				SetNbBitIdCan(_11BITS);							//11 bits d'identifiants
			}

//V1.41 ajout can extended format
			if((StrConfObd.wProtocol == CAN_11BIT_FRAME_EXT) ||
				(StrConfObd.wProtocol == CAN_11BIT_8BYTE_FRAME_EXT) ||
				(StrConfObd.wProtocol == CAN_29BIT_FRAME_EXT) ||
				(StrConfObd.wProtocol == CAN_29BIT_8BYTE_FRAME_EXT))
			{
				SetCanExtended_N_TA((UCHAR)StrConfObd.wExtended_N_TA);	//Init Network Target Adress
			}

//V1.41 
			SetEcuTargetId(StrConfObd.dwTargetAddr);		//init Can target Id
			SetEcuSrcId(StrConfObd.dwSourceAddr);			//init can rx Id
			SetBaudRateCanFd(StrConfObd.ucBaudrateIndex);			//init vitesse de communication Can
			SetFlowControlCan(StrConfObd.wBlockSize, StrConfObd.wFrameDelay);	//init block size et delay inter trame
		}
		else
		{//init config K_line
//DB2 Modif plante Can Honda
			ecanSetMode(ECAN_DISABLE_MODE);	//disable Can module
//DB2 Modif plante Can Honda

//520_84D

			//ReceiveKLine(uObdBufRx);				//Init pointeur sur buffer reception
            SetKLineRxBuffer(uObdBufRx);
//QD1-42d
			SetFoncEndRxObd(EndReceiveKLinePacket);		//Init fonction de fin reception obd

			SetBaudRateIndexKLine(StrConfObd.ucBaudrateIndex);	//init vitesse de communication uart K_Line
//Iso9141
			SetConfObd((UCHAR)StrConfObd.dwTargetAddr, 
						(WORD)StrConfObd.wCharacterDelay,
								(WORD)StrConfObd.wStartupLowTime, 
										(WORD)StrConfObd.wStartupHighTime);	//Adresse + configuration du delai inter caractere + tempo startup
//Iso9141
		}
		
        SetPinObd(StrConfObd.wSelectKLine);	
		cRet = SetProtocoleObd(StrConfObd.wProtocol);				//selection du protocole
	}
	return(cRet);
}

void SendCurrentOBDVoltageRaw(void)
{
	WORD wAdcData = GetAN2Value();
	Rs232SendDataObd((UCHAR *)&wAdcData,sizeof(wAdcData));
	SetLed(TRUE, YELLOW);
}

void SendCurrentOBDVoltageVolt(void)
{
	float fObdVoltage=0;
	
	fObdVoltage = (float)GetAN2Value() * 3.3 * ((float)124+(float)15)/(float)((float)15*(float)4096);
	// buffer * 3.3/4096 = mesured value
	// mesured value * (124+15)/15 = value OBD
	
	// application calibration correctif
	fObdVoltage = fObdVoltage * rDeviceData.sAdcCalib.fGain + rDeviceData.sAdcCalib.fOffset;
	Rs232SendDataObd((UCHAR *)&fObdVoltage,sizeof(fObdVoltage));
	SetLed(TRUE, YELLOW);
}


//T520_84
/*______________________________________________________________________ 
|   Tache    : Rs232Task			                     	            |
|_______________________________________________________________________|
|   Description  : Tache gestion ligne Rs232							|
|   En entree: Rien									                 	|
|   En sortie: Rien									                 	|
|_______________________________________________________________________|*/
void __attribute((far))Rs232Task(void)
{
	UCHAR cTmp;
//    WORD wTmp;
	Rs232Init();
    MemoryPoolInit();
	wTimeoutRs232 = NO_TIMEOUT;
    //wTimeoutRs232 = 500; //500ms, for debug, should recover
	Rs232Receive(sBufRxRs232.ucBuf, Rs232DecodeFrame);		//attente reception trame rs232

	while(1)
	{
		pMsgRs232 = OsWaitEvent(&Rs232Sem,wTimeoutRs232,T_STOP_ON_EVT);
		cEvtRs232 = pMsgRs232->ucEvt;		//recuperer l'evenement

		ClrWdtCom();
		switch(cEvtRs232)
		{
			case EVT_TIMER:						//pour l'instant pas de time out 

			break;


			case	EVT_DECODE_TRAME:			//Decodage trame rs232
				switch(sBufRxRs232.ucBuf[RS232_RXCMD])
				{

					case	RS232CMD_BOOT_REQUEST:
						Rs232Ack();				//Reponse ACK
						GoToBoot();
					break;
	
					case 	RS232CMD_CHECK_MODULE:
						Rs232Ack();				//Reponse ACK
					break;
					
					case	RS232CMD_ECU_LIGHT_ON:
						Rs232Ack();				//Reponse ACK
					break;

					case	RS232CMD_ECU_CHECK_12V:
						cTmp = IsConnectedOnCar();					//tester si le vehicule est connecte
						if(cTmp == TRUE)
						{
							Rs232Ack();			//reponse ACK
						}
						else
						{
							Rs232NAck();					//Vehicule deconnecte --> reponse NACK
						}
					break;
					
                    //selection du protocole de communication
                    case	RS232CMD_ECU_SELECT_PROTOCOLE:
						cTmp = ConfigCanObd(&sBufRxRs232.ucBuf[RS232_RXDATA]);
						if(cTmp == TRUE)
						{
                            SetLed(TRUE, YELLOW);
							Rs232Ack();			//reponse ACK
						}
						else
						{
							Rs232NAck();					//par defaut on prepare une reponse NACK
						}
					break;
					
                    case	RS232CMD_ECU_SELECT_PROTOCOLE_FD:
						cTmp = ConfigCanFdObd(&sBufRxRs232.ucBuf[RS232_RXDATA]);
						if(cTmp == TRUE)
						{
                            SetLed(TRUE, YELLOW);
							Rs232Ack();			//reponse ACK
						}
						else
						{
							Rs232NAck();					//par defaut on prepare une reponse NACK
						}
					break;	
					
					//start sequence Waik_up
					case 	RS232CMD_ECU_WAKE_UP:
						Rs232Ack();				//reponse ACK
						WakeUpObd();			//wake up en fonction du protocole choisi
					break;
		
					//send data to K-LINE
					case RS232CMD_SEND_DATA:
						Rs232Ack();				//reponse ACK
						cTmp = GetProtocoleObd();		//recuperation du protocole
						if(cTmp < CAN_11BIT_FRAME)
						{ 
							SendDataObd(&sBufRxRs232.ucBuf[RS232_RXDATA],sBufRxRs232.ucBuf[RS232_RXNBDATA]-1);
						}
						else if(cTmp < DOIP_FRAME)
						{
							EcuSendDataOndiag(&sBufRxRs232.ucBuf[RS232_RXDATA], sBufRxRs232.ucBuf[RS232_RXNBDATA]-1,EVT_SEND_DATA_CAN);
						}
                        else
                        {//DoIP
                        
                        }
					break;
					
					//start sequence Waik_up + emission data 
					case RS232CMD_ECU_STARTCOM:
						Rs232Ack();				//reponse ACK
						cTmp = GetProtocoleObd();		//recuperation du protocole
						if(cTmp < CAN_11BIT_FRAME)			
						{ //K-LINE procedure
							WakeUpObd();			//wake up en fonction des timing parametres
							//attente 100ms --> transmission data sur K_Line
							//la transmission des datas ne sera effectuee qu'apres le start up sur la ligne Obd
							do
							{
								pMsgRs232 = OsWaitEvent(&Rs232TimerSem,_T100ms,T_STOP_ON_EVT);
							}
							while(pMsgRs232->ucEvt != EVT_TIMER);		
							//start transmission data
							SendDataObd(&sBufRxRs232.ucBuf[RS232_RXDATA],sBufRxRs232.ucBuf[RS232_RXNBDATA]-1);
						}
						else  if(cTmp < DOIP_FRAME)
						{//CAN bus procedure
							EcuSendDataOndiag(&sBufRxRs232.ucBuf[RS232_RXDATA], sBufRxRs232.ucBuf[RS232_RXNBDATA]-1, EVT_SEND_DATA_CAN);
						}
                        else
                        {//DoIP
                            if(pRs232Buf != NULL)
                            {
                                FreeObdBufByIdx(pRs232Buf->bF.wBufIdx);
                                pRs232Buf = NULL;
                            }           
                            pRs232Buf = GetNewBuffer(sBufRxRs232.ucBuf[RS232_RXNBDATA]-1,RX_BUFFER,BUFFER_TYPE_VT_TOOL);
                            memcpy(pRs232Buf->pBuf,&sBufRxRs232.ucBuf[RS232_RXDATA],sBufRxRs232.ucBuf[RS232_RXNBDATA]-1);
                            //to call the next step
                        }
					break;

					case	RS232CMD_SUCCESS:
						SetLed(FALSE, GREEN);
					break;

					case	RS232CMD_FAIL:
						SetLed(FALSE, RED);
					break;

					case	RS232CMD_ECU_END:
						ToggleLed(GREEN);
					break;	
					
					case RS232CMD_SELECT_TIMEOUT_REBOOT:
					{
						WORD wNewTime;
						
						memcpy(&wNewTime,&sBufRxRs232.ucBuf[RS232_RXDATA],2);
						SetTimerResetInit(wNewTime);
					}
					break;
					
					case RS232CMD_GET_OBD_VOLTAGE_RAW:
					{
						SendCurrentOBDVoltageRaw();
					}
					break;
					
					case RS232CMD_GET_OBD_VOLTAGE_VOLT:
					{
						SendCurrentOBDVoltageVolt();
					}
					break;
					
					case	RS232CMD_GET_SERIAL_NUMBER:
						Rs232SendDataObd((UCHAR *)&rDeviceData.ucAteqSerial,SERIAL_LEN);
					break;

					case	RS232CMD_GET_ARGOS_ID:
						Rs232SendDataObd((UCHAR *)&rDeviceData.ucArgosID,SERIAL_LEN);
					break;
				
					case	RS232CMD_WRITE_OBD_ADC_CONFIG:
						saveAdcConfig(&sBufRxRs232.ucBuf[RS232_RXDATA]);
						Rs232Ack();
					break;
				
					case	RS232CMD_GET_OBD_ADC_CONFIG:
						Rs232SendDataObd((UCHAR *)&rDeviceData.sAdcCalib,sizeof(ADC_CALIBRATION));
					break;
					
                    case    RS232CMD_MULTIFRAME:
                    {
                        RS232_MULTI sMultiData;
                        
                        memcpy(&sMultiData,&sBufRxRs232.ucBuf[RS232_RXDATA],sBufRxRs232.ucBuf[RS232_RXNBDATA]-1);
                        if(sMultiData.ucNbActualFrame == 1)
                        {//first frame
                            if(pRs232Buf != NULL)
                            {
                                FreeObdBufByIdx(pRs232Buf->bF.wBufIdx);
                                pRs232Buf = NULL;
                            }
                            // size of buffer, max of possible char with the max number of frame
                            pRs232Buf = GetNewBuffer((sMultiData.ucNbTotalFrame * MAX_USEFUL_DATA_RS232_MULTIFRAME),RX_BUFFER,BUFFER_TYPE_VT_TOOL);
                            memcpy(pRs232Buf->pBuf,&sMultiData.ucData,sMultiData.ucDataInThisFrame);
                            Rs232Ack();	
                        }
                        else if((pRs232Buf != NULL) &&(sMultiData.ucNbActualFrame == pRs232Buf->ucFrameIdx + 1))//not first frame
                        {
                            
                            pRs232Buf->ucFrameIdx++;
                            memcpy(pRs232Buf->pBuf,&sMultiData.ucData,sMultiData.ucDataInThisFrame);
                            if(sMultiData.ucNbActualFrame == sMultiData.ucNbTotalFrame)
                            {// last frame
                                //to do
                            }
                            Rs232Ack();	
                        }
                        else
                        {
                            if(pRs232Buf != NULL)//Re-init !
                            {
                                FreeObdBufByIdx(pRs232Buf->bF.wBufIdx);
                                pRs232Buf = NULL;
                            }                 
                            Rs232NAck();
                        }
                    }
                    break;
				}//end of switch(sBufRxRs232.ucBuf[RS232_RXCMD])

			break;  //end of EVT_DECODE_TRAME

			case	EVT_TX_ECU_REPONSE:
					Rs232Send(sBufTxRs232.ucBuf,cNbCarRs232, Rs232EndTx);				//Send ECU response to VT57
			break;
		
			default:
				Rs232NAck();		
			break;
		}
	}
}


