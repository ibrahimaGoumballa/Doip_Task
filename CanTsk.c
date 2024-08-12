/*______________________________________________________________________ 
|   Project      :Interface comunication CAN carte T520_88              |
|   Module       :Tache dediee a l'interface CAN ( emission	data + 		|
|   Read write Identifiant valve calculateur CHERRY	M11					|
|   Module       :Tache dediee a l'interface CAN 						|
|   File         :CanTask.c                                         	|
|_______________________________________________________________________|
|   Description  :interface avec driver can + segmentation trame can	|
|   Date 2/05/2008 - 											        |
|   Auteur : David BARON										        |
|   History : porting to T52088 by wolfgang since 2021/06/29	        |
|_______________________________________________________________________|*/
#include <xc.h>
#include "../Common_Header/NewType.h"
#include "Kernel.h"
#include <stdio.h>
#include <string.h>
#include "rs232tsk.h"
#include "EcanDrv.h"
#include "Cantsk.h"
#include "Map.h"
#include "interrupt.h"
#include "AdcDrv.h"
#include "obddrv.h"
//#include "obdtsk.h"

/*______________________________________________________________________ 
|                        Elements externes                              |
|_______________________________________________________________________|*/

/*______________________________________________________________________ 
|                        declaration des constantes                     |
|_______________________________________________________________________|*/

//Automate pour ecriture et lecture identifiants
enum
{
//CALCULATEUR CAN
	CAN_REPOS,
	CAN_INIT_CONF,

	//segmentation de la trame can
	CAN_WAIT_FLOW_CONTROL,	
	CAN_SEND_CONSECUTIVE_FRAME,						
	//Reception d'une trame de donnee segmentee					
	CAN_WAIT_REP_ONDIAG,						
	CAN_WAIT_CONSECUTIVE_FRAME,					

//start emission trame segmentee
	CAN_SEND_DATA,
//fin de reception d'une trame segmentee
	CAN_END_RX_DATA
};

//Segmentation trame can
#define SINGLEFRAME			0x0
#define FIRSTFRAME			0x10
#define CONSECUTIVEFRAME	0x20
#define FLOWCONTROL			0x30
#define	GET_NB_CAR_RESTANT	0x40
#define	GET_NUM_MESSAGE		0x41
//QD1_44 ajout protocole can direct sans protocole
#define	DIRECTFRAME			0x42
//QD1_44

#define ERREURFRAME			0xFF

//V1.04 Flow control configurable
//Definition d'une trame flow control
#define BS	0
#define STmin 1
#define SIZE_FLOWCONTROL	2

//V1.04

//#define	DEBUGCAN			//activation debug Can

//Flow control en cas de transmission du calculateur
const UCHAR CanRepFlowControl[1] = {0x30};


/*______________________________________________________________________ 
|                        Elements publics                               |
|_______________________________________________________________________|*/
/*______________________________________________________________________ 
|                        Elements locaux                                |
|_______________________________________________________________________|*/
//V2.08 :bug: la taille du buffer de recption est insuffisante pour Suzuki 
#define	_MAX_BUF_SIZE	256
static UCHAR uObdBufRx[_MAX_BUF_SIZE];			//buffer de reception
static UCHAR uObdBufTx[_MAX_BUF_SIZE];			//buffer de transmission 
static UCHAR uObdBufData[_MAX_BUF_SIZE];		//buffer de donnees
//V2.08

SEMAPHORE CanSem ={0,0,0};
SEMAPHORE CanTmrSem ={0,0,0};

static EVENT_DEF *pMsgCan;					//pointeur sur message tache Can
static WORD wCanTimeOut;					//time out tache Can
static WORD	wCaraTxOnDiag = 0;				//nombre d'octets a transmettre sur la ligne k_line ou can
static PF pFoncTimeOut = NULLPROC;		//pointeur sur la fonction a appeler en cas de timeout
static UCHAR cEvtCan;						//evenement tache Can
static UCHAR cMemCanEtat = CAN_INIT_CONF;	//memorisation etat automate apres transmission de donnnee
static UCHAR cCanEtat = CAN_INIT_CONF;		//variable automate read write identifier ecu

static UCHAR *pTxData=NULLPTR;				//pointeur sur la zone data a transmettre
static UCHAR uBlockSize = 8;				//nombre de consecutive frames avant reception d'un Flow control

static long lIdTx = 0;						//Id en transmission (ecu target)
static long lIdRx = 0;						//Id en reception (ecu source)
static UCHAR ucNbBitId = _11BITS;			//Nombre de bits identifiants Can 

static long lIdMask = 0x1FFFFFFF;			//par defaut on ne masque rien
//static long lIdMask = 0x0;					//on recoit tout

//V1.04 Ajout vitesse de communication
static long lBaudRate = _500KBITS;			//vitesse par defaut
static long lDataBaudrate = _500KBITS;
//V1.04

static ULONG ulIdRx = 0;					//Id can recu


//V1.04 flow control configurable
static UCHAR TabCanFlowControl[SIZE_FLOWCONTROL] = { 0xFF, 50};		//255 block et 50 ms inter trame

static UCHAR Tab00CanFlowControl[SIZE_FLOWCONTROL] = { 0x00, 0x00};		//Special case 0x00 0x00 after 0x30 flow control

//CA1_42_7 Ajout extended format can 
static UCHAR ucN_TA = 0;					//Network Target Address pour le protocole can extended format
static UCHAR ucN_TA_rx = 0;					//Network Target Address recue avec le protocole can extended format

/*______________________________________________________________________ 
|   Procedure    : SetCanExtended_N_TA                             	    |
|_______________________________________________________________________|
|   Description  : Mise a jour N_TA Network Target Address utilise  	|
|   dans les protocole												 	|
|																		|
|   CAN_11BIT_FRAME_EXT ou												|
|	CAN_11BIT_8BYTE_FRAME_EXT ou										|
|	CAN_29BIT_FRAME_EXT		  ou										|
|	CAN_29BIT_8BYTE_FRAME_EXT											|
|   En sortie: rien									                    |
|_______________________________________________________________________|*/
void SetCanExtended_N_TA(UCHAR ucVal)
{
	ucN_TA = ucVal;					//init network target address
}

/*______________________________________________________________________ 
|   Procedure    : GetLastN_TA_CanRx                             	    |
|_______________________________________________________________________|
|   Description  : Recuperation de la derniere Network Target Address 	|
|   recue valable pour les protocoles														 	|
|   CAN_11BIT_FRAME_EXT ou												|
|	CAN_11BIT_8BYTE_FRAME_EXT ou										|
|	CAN_29BIT_FRAME_EXT		  ou										|
|	CAN_29BIT_8BYTE_FRAME_EXT											|
|   En sortie: ucN_TA_rx							                    |
|_______________________________________________________________________|*/
UCHAR GetLastN_TA_CanRx(void)
{
	return(ucN_TA_rx);
}

/*______________________________________________________________________ 
|   Procedure    : GetOffsetDataCan                                	    |
|_______________________________________________________________________|
|   Description  : retourne l'offset de la 1ere data dans une trame can	|
|   Si 	CAN_11BIT_FRAME ou												|
|		CAN_11BIT_8BYTE_FRAME ou 										|
|		CAN_29BIT_FRAME		  ou										|
|		CAN_29BIT_8BYTE_FRAME  	--> data sur 1er octet					|
|   Si 	CAN_11BIT_FRAME_EXT ou											|
|		CAN_11BIT_8BYTE_FRAME_EXT ou									|
|		CAN_29BIT_FRAME_EXT		  ou									|
|		CAN_29BIT_8BYTE_FRAME_ext --> data sur 2eme octet				|
|   En sortie: 0 ou 1								                    |
|_______________________________________________________________________|*/
UCHAR GetOffsetDataCan(void)
{
UCHAR ucRet = 0;					//data sur octet 1
UCHAR ucTmp;
		
	ucTmp = GetProtocoleObd();
	if((ucTmp == CAN_11BIT_FRAME_EXT) ||
		(ucTmp == CAN_11BIT_8BYTE_FRAME_EXT) ||
		(ucTmp == CAN_29BIT_FRAME_EXT) ||
		(ucTmp == CAN_29BIT_8BYTE_FRAME_EXT)||
		(ucTmp == CAN_11BIT_8BYTE_FRAME_7BYTE))
	{
		ucRet = 1;						//data sur octet 2
	}
	return(ucRet);
}
//CA1_42_7


/*______________________________________________________________________ 
|   Procedure    : SetFlowControlCan                                    |
|_______________________________________________________________________|
|   Description  : Definition des donnees de la trame flow control a  	|
|   emettre apres une trame first frame								 	|
|   En entree:	ucBS 	--> Block Size (Nombre de trame avant emission  |
|   d'une trame flow control)											|
|  				ucSTmin --> Temps inter trame en ms	                    |
|   En sortie: Rien									                    |
|_______________________________________________________________________|*/
void SetFlowControlCan(UCHAR ucBS, UCHAR ucSTmin)
{
	TabCanFlowControl[BS] = ucBS;			//init block size
	TabCanFlowControl[STmin] = ucSTmin;		//init temps inter trame
}
//V1.04

/*______________________________________________________________________ 
|   Procedure    : GetLastIdCanRx                                       |
|_______________________________________________________________________|
|   Description  : Recuperation du dernier Id can recu				 	|
|   En entree: Rien									                    |
|   En sortie: Id can									                    |
|_______________________________________________________________________|*/
ULONG GetLastIdCanRx(void)
{
	return(ulIdRx);
}

/*______________________________________________________________________ 
|   Procedure    : ReceiveCan	                                        |
|_______________________________________________________________________|
|   Description  : Fin de reception 								 	|
|   En entree: Rien									                    |
|   En sortie: Rien									                    |
|_______________________________________________________________________|*/
void ReceiveCan(void)
{
	OsSendEvent(&CanSem,EVT_CAN_UNLOCK,NULL);
}

/*_______________________________________________________________________ 
|   procedure:  SetNbBitIdCan	       		                             |
|________________________________________________________________________|
|   Description  : configuration nb de bits de l'identififiant CAN  	 |
|   En entree: 										                  	 |
|   	wVal	  = Nombre de bit de l'identifiant		                 |
|   				_11BITS ou							                 |
|   				_29BITS 							                 |
|   En sortie: Rien										                 |
|________________________________________________________________________|*/
void SetNbBitIdCan(UCHAR cVal)
{
	if((cVal != _11BITS) &&
			(cVal != _29BITS))
	{
		cVal = _11BITS;					//en cas d'erreur --> 11Bits d'identifiants
	}
	ucNbBitId = cVal;
}

/*_______________________________________________________________________ 
|   procedure:  SetCanTargetId	                                         |
|________________________________________________________________________|
|   Description  : configuration Id(adresse) calculateur cible		  	 |
|   En entree: 										                  	 |
|   	lVal	  = Adresse ecu cible						                 |
|   En sortie: Rien										                 |
|________________________________________________________________________|*/
void SetEcuTargetId(long lVal)
{
		lIdTx = lVal;
}

/*_______________________________________________________________________ 
|   procedure:  SetEcuSrcId	                                	         |
|________________________________________________________________________|
|   Description  : configuration Id(adresse) Id utilise pour une reponse |
|   En entree: 										                  	 |
|   	lVal	  = Adresse ecu source ou a lire			             |
|   En sortie: Rien										                 |
|________________________________________________________________________|*/
void SetEcuSrcId(long lVal)
{
	lIdRx = lVal;
	OsSendEvent(&CanSem,EVT_INIT_CONF_CAN,NULL);				//emission evenement
}

/*_______________________________________________________________________ 
|   procedure:  SetBaudRateCan                                	         |
|________________________________________________________________________|
|   Description  : Init vitesse communication can						 |
|   En entree: 										                  	 |
|   	lVal	  = Vitesse en baud du canal Can			             |
|   En sortie: Vitesse reelle de communication			                 |
|________________________________________________________________________|*/
long SetBaudRateCan(long lVal, long lValData)
{
long lRet =0L;
long lRetData = 0L;
	if(lVal == _500KBITS)
	{
		lRet = _500KBITS;
	}
	else if(lVal == _250KBITS)
	{
		lRet = _250KBITS;
	}
//T520_84 ajout can low speed
	else if(lVal <= _125KBITS)					
	{
		lRet = lVal;
	}

	lRetData = lRet;

	if(lValData == _5MBITS)
	{
		lRetData = _5MBITS;
	}
	else if(lValData == _2MBITS)
	{
		lRetData = _2MBITS;
	}
	else if(lValData == _1MBITS)
	{
		lRetData = _1MBITS;
	}
	else if(lValData == _500KBITS)
	{
		lRetData = _500KBITS;
	}
	
	if(lRetData <= lRet)
	{// Data baudrate can't be less than nominal rate
		lRetData = lRet;
	}	

//T520_84
//V1.04 ajout vitesse de communication 
	if((lRet != 0L) && ((lBaudRate != lRet) || (lDataBaudrate != lRetData)))
	{
		lBaudRate = lRet;				//affecter la vitesse de communication
		lDataBaudrate = lRetData;
		OsSendEvent(&CanSem,EVT_INIT_CONF_CAN,NULL);				//demande de configuration
	}
	return(lRet);
}

long SetBaudRateCanFd(UCHAR ucBaudRateIdx)
{
long lRet =0L;
	
	switch(ucBaudRateIdx)
	{
		default:
		break;
			
		case CAN_500K_1M:
			SetBaudRateCan(_500KBITS,_1MBITS);
		break;
					
		case CAN_500K_2M:
			SetBaudRateCan(_500KBITS,_2MBITS);
		break;
			
		case CAN_500K_5M:
			SetBaudRateCan(_500KBITS,_5MBITS);
		break;	
		
		case CAN_500K_8M:
			SetBaudRateCan(_500KBITS,_8MBITS);
		break;
					
		case CAN_250K_500K:
			SetBaudRateCan(_250KBITS,_500KBITS);
		break;	
			
		case CAN_250K_1M:
			SetBaudRateCan(_250KBITS,_1MBITS);
		break;
					
		case CAN_250K_2M:
			SetBaudRateCan(_250KBITS,_2MBITS);
		break;
			
		case CAN_125K_500K:
			SetBaudRateCan(_125KBITS,_500KBITS);
		break;
					
		case CAN_33333:
			SetBaudRateCan(33333,33333);
		break;
			
		case CAN_50K:
			SetBaudRateCan(50000,50000);
		break;
					
		case CAN_125K:
			SetBaudRateCan(_125KBITS,_125KBITS);
		break;
			
		case CAN_250K:
			SetBaudRateCan(_250KBITS,_250KBITS);
		break;
					
		case CAN_500K:
			SetBaudRateCan(_500KBITS,_500KBITS);
		break;
	}
	return(lRet);
}

//CA1_42_7 Ajout extended format can
//QD1_44 Ajout protocole can direct sans protocole
/*_______________________________________________________________________ 
|   procedure:  GetTypeFrame 	                                         |
|________________________________________________________________________|
|   Description  : recupere le type de trame recue du calculateur	  	 |
|   En entree: 										                  	 |
|   	pcSrc  = Adresse des donnees de la trame		                 |
|   En sortie: Type de trame recu						                 |
|     					SINGLEFRAME					                	 |
|   				 ou	FIRSTFRAME 					            	     |
|   				 ou	CONSECUTIVEFRAME				    	         |
|   				 ou	FLOWCONTROL					    	             |
|   				 ou	DIRECTFRAME					    	             |
|   				 ou	ERREURFRAME si trame inconnue	                 |
|________________________________________________________________________|*/
UCHAR GetTypeFrame(UCHAR *pcSrc)
{
	UCHAR ucRetCode;
	if(pcSrc)
	{
		if(GetProtocoleObd() == CAN_11BIT_8BYTE_DIRECT)
		{
			ucRetCode = DIRECTFRAME; 	
		}
		else
		{
			ucRetCode = pcSrc[GetOffsetDataCan()] & 0xF0;			//recuperer le type de trame
			if((ucRetCode != SINGLEFRAME) &&
		   (ucRetCode != FIRSTFRAME) &&
		   (ucRetCode != CONSECUTIVEFRAME) &&
		   (ucRetCode != FLOWCONTROL))
				ucRetCode = ERREURFRAME;
		}
	}
	return( ucRetCode );
}
//QD1_44

//QD1-42 :FIX:Bug Taille First frame sur 12bits
/*______________________________________________________________________
|   procedure:  GetNbDataFirstFrame                                     |
|_______________________________________________________________________|
|   Description  : recupere le nombre d'octets qui vont etre tranferes  |
|   sur une first frame												    |
|   En entree: 										                    |
|   	pcSrc  = Adresse des donnees de la first frame	                |
|   En sortie: Nb d'octets a recevoir					                |
|  				 ou	ERREURFRAME si la trame n'est pas une first frame	|
|_______________________________________________________________________|*/
WORD GetNbDataFirstFrame(UCHAR *pcSrc)
{	
	WORD wRetCode;
	UCHAR ucTmp;
	UCHAR ucOffsetData;
	ucOffsetData = GetOffsetDataCan();
	wRetCode = pcSrc[ucOffsetData] & 0xF0;			//recuperer le type de trame
	if(wRetCode != FIRSTFRAME)
	{
		wRetCode = ERREURFRAME;
	}
	else
	{
		//How many data byte in the long frame
		ucTmp = pcSrc[ucOffsetData] & 0xF;			//ne garder que les bits de poid faible
		wRetCode = ucTmp * 0x100; 
		ucTmp = pcSrc[ucOffsetData+1];
		wRetCode += ucTmp;							//Nombre d'octets utiles
	}
	return (wRetCode);
}
//QD1-42:FIX:Bug

/*______________________________________________________________________ 
|   procedure:  GetNumMessageConsFrame                                 	 |
|________________________________________________________________________|
|   Description  : recupere le numero de message d'une consecutive frame |
|   En entree: 										                     |
|   	pcSrc  = Adresse de la consecutive frame		                 |
|   En sortie: Numero de message 						                 |
|   			ou	ERREURFRAME si la trame n'est pas une consecutive	 |
|frame																	 |
|________________________________________________________________________|*/
UCHAR GetNumMessageConsFrame(UCHAR *pcSrc)
{
	UCHAR ucRetCode;
	UCHAR ucOffsetData;
	ucOffsetData = GetOffsetDataCan();
	ucRetCode = pcSrc[ucOffsetData] & 0xF0;
	if(ucRetCode != CONSECUTIVEFRAME)
	{
		ucRetCode = ERREURFRAME;
	}
	else
	{
		ucRetCode = pcSrc[ucOffsetData] & 0x0F;				//Numero de message
	}
	return (ucRetCode);
}

/*______________________________________________________________________
|   procedure:  CpyDataFrame		                                    |
|_______________________________________________________________________|
|   Description  : recupere les donnees utiles d'une trame recue	    |
|   En entree: 										                    |
|   	pcDest = Adresse de rangement des donnees utiles	            |
|   	pcSrc  = Adresse buffer trame recue				                |
|   En sortie: Nb d'octets utiles						                |
|_______________________________________________________________________|*/
UCHAR CpyDataFrame(UCHAR *pcDest , UCHAR *pcSrc)
{
	UCHAR uNbData = 0;
	UCHAR *pdata = 0;
	UCHAR uType;
	UCHAR ucOffsetData;

	ucOffsetData = GetOffsetDataCan();
//QD1_44 Ajout protocole can direct sans protocole
//	uType = pcSrc[ucOffsetData] & 0xF0;
	uType = GetTypeFrame(pcSrc);
	switch(uType)
	{
		case SINGLEFRAME:
			uNbData = pcSrc[ucOffsetData] & 0xF;
			if(uNbData != 0)
			{
				pdata = &pcSrc[1+ucOffsetData];
			}
			else //canFD
			{
				uNbData = pcSrc[1+ucOffsetData];
				pdata = &pcSrc[2+ucOffsetData];
			}
		break;

		case FIRSTFRAME:
			pdata = &pcSrc[2+ucOffsetData];
			uNbData = 6-ucOffsetData;
		break;

		case CONSECUTIVEFRAME:
			pdata = &pcSrc[1+ucOffsetData];
			uNbData = 7-ucOffsetData;		
		break;

		case DIRECTFRAME:
			pdata = pcSrc;
			uNbData = 8;		
		break;

	}			
	if ((uNbData) && pdata)
	{
		memcpy(pcDest,pdata,uNbData);		//recopier les donnees utiles
	}
	return (uNbData);
}
//QD1_44

/*___________________________________________________________________ 
|   procedure:  BuildFrameCan                                         |
|_____________________________________________________________________|
|   Description  : Construction d'une trame can pour le calculateur   |
|   En entree: 										                  |
|   uCmd = Commande pour le type de trame a construire	              |
|     					SINGLEFRAME					              	  |
|   				 ou	FIRSTFRAME 					              	  |
|   				 ou	CONSECUTIVEFRAME				              |
|   pcDest = Adresse de rangement de la trame			              |
|   pcSrc  = Adresse des donnees de la trame 			              |
|   NbData = Nombre de donnees ou Numero de message	                  |
|   En sortie: Nombre de donnees copiees dans le buffer	              |
|_____________________________________________________________________|*/
short BuildFrameCan(short sCmd,UCHAR *pcDest, UCHAR *pcSrc, short sNbData)
{
static short sNumMessage;              //Numero de message  long message divided into 8 byte frame each
static short sCntData;					//compteur de donnees a transmette
static short sDataTransmit;				//nombre de donnees transmises
short	sRetCode = 0;
UCHAR	cTmp;
UCHAR ucOffsetData;

	ucOffsetData = GetOffsetDataCan();
	switch(sCmd)
	{
		case	GET_NB_CAR_RESTANT:
			sRetCode = sCntData;
		break;

		case	GET_NUM_MESSAGE: 		//numero de message
			sRetCode = sNumMessage;
		break;

		case	FLOWCONTROL: //like started as 0x30 
			cTmp = GetProtocoleObd();
			if(pcDest)
			{
				memset(pcDest,0xFF,8);				//La trame doit faire 8 octets les octets non utilises sont initialises a FF
//CA1_42_7 tester si extended format
				if(ucOffsetData)
				{
					if(cTmp == CAN_11BIT_8BYTE_FRAME_7BYTE)
					{
						*pcDest =(UCHAR)(lIdRx&0xFF); //the lower byte of the CAN ID

					}
					else
					{
						*pcDest = ucN_TA;
					}
					pcDest++;
				}
//CA1_42_7
				*pcDest = FLOWCONTROL;
				pcDest++;
				memcpy(pcDest,pcSrc,sNbData);
				//Tester le protocole utilise. Il est possible que toutes les trames fassent 8 bytes de long
				if((CAN_11BIT_8BYTE_FRAME == cTmp) ||
					 (CAN_29BIT_8BYTE_FRAME == cTmp) ||
						(CAN_11BIT_8BYTE_FRAME_EXT == cTmp) ||
							(CAN_29BIT_8BYTE_FRAME_EXT == cTmp)||
								(CAN_11BIT_8BYTE_FRAME_00 == cTmp)||
									(CAN_11BIT_8BYTE_FRAME_7BYTE == cTmp))
				{
					sRetCode = 8;						//forcer le nombre de data a 8
				}
				else
				{
					sRetCode = sNbData + (short)ucOffsetData + 1; 			//Nombre d'octet de la trame
				}
			}
		break;
	
		case	SINGLEFRAME:
			cTmp = GetProtocoleObd();
			if((sNbData < (8-ucOffsetData)) && (pcDest))
			{
				memset(pcDest,0xFF,8);				//La trame doit faire 8 octets les octets non utilises sont initialises a FF
//CA1_42_7 tester si extended format
				if(ucOffsetData)
				{
					if(cTmp == CAN_11BIT_8BYTE_FRAME_7BYTE)
					{
						*pcDest = (UCHAR)(lIdRx&0xFF);
					}
					else
					{
						*pcDest = ucN_TA;
					}
					pcDest++;
				}
//CA1_42_7
				*pcDest = (UCHAR)sNbData;
				pcDest++;
				memcpy(pcDest,pcSrc,sNbData);

//Tester le protocole utilise. Il est possible que toutes les trames fassent 8 bytes de long
				if((CAN_11BIT_8BYTE_FRAME == cTmp) ||
					 (CAN_29BIT_8BYTE_FRAME == cTmp) ||
						(CAN_11BIT_8BYTE_FRAME_EXT == cTmp) ||
							(CAN_29BIT_8BYTE_FRAME_EXT == cTmp) || 
								(CAN_11BIT_8BYTE_FRAME_00 == cTmp)||
									(CAN_11BIT_8BYTE_FRAME_7BYTE == cTmp))
				{
					sRetCode = 8;				//forcer le nombre de data a 8
				}
				else
				{
					sRetCode = sNbData + (short)ucOffsetData + 1;		//Nombre d'octet de la trame
				}
			}
		break;

		case	FIRSTFRAME:
			cTmp = GetProtocoleObd();
			if(pcDest)
			{
				sNumMessage = 1;			
				sCntData = sNbData - 6 + (short)ucOffsetData;			//nombre de donnees restant a transmettre	
//CA1_42_7 tester si extended format
				if(ucOffsetData)
				{
					if(cTmp == CAN_11BIT_8BYTE_FRAME_7BYTE)
					{
						*pcDest = (UCHAR)(lIdRx&0xFF);
					}
					else
					{
						*pcDest = ucN_TA;
					}
					pcDest++;
				}
//CA1_42_7
	   			*pcDest = FIRSTFRAME;
				pcDest++;
				*pcDest = (UCHAR)sNbData; 		//recopier le nombre d'octets total
				pcDest++;
				memcpy(pcDest,pcSrc,(6-ucOffsetData));
				sRetCode = 8;					//Nombre d'octet de la trame
				sDataTransmit = 6-(short)ucOffsetData;
			}

		break;
		
		case	CONSECUTIVEFRAME:
			cTmp = GetProtocoleObd();
			if((sCntData) && (pcDest))
 			{
				memset(pcDest,0xFF,8);				//La trame doit faire 8 octets les octets non utilises sont initialises a FF
//V1.41 tester si extended format
				if(ucOffsetData)
				{
					if(cTmp == CAN_11BIT_8BYTE_FRAME_7BYTE)
					{
						*pcDest = (UCHAR)(lIdRx&0xFF);
					}
					else
					{
						*pcDest = ucN_TA;
					}
					pcDest++;
				}
//V1.41
  			   	*pcDest = CONSECUTIVEFRAME + sNumMessage;
				sNumMessage++;
	   			pcDest++;
	   			if(sCntData > (7-(short)ucOffsetData)) //One frame not enough
	   			{
	   				memcpy(pcDest,&pcSrc[sDataTransmit],(7-(short)ucOffsetData));
	   				sCntData = sCntData - (7-(short)ucOffsetData);
					sDataTransmit = sDataTransmit + (7-(short)ucOffsetData) ;		  		//incrementer le nombre d'octets transmis
	   				sRetCode = 8;					//Nombre d'octet de la trame
	   			}
	   			else
	   			{
	   				memcpy(pcDest,&pcSrc[sDataTransmit],sCntData);
	   	   			sRetCode = sCntData +(short)ucOffsetData + 1;			//sCntData octets copies
					sDataTransmit += sCntData;		//incrementer le nombre d'octets transmis 
		   			sCntData = 0;
		   		}
			//Tester le protocole utilise. Il est possible que toutes les trames fassent 8 bytes de long
				if((CAN_11BIT_8BYTE_FRAME == cTmp) ||
					 (CAN_29BIT_8BYTE_FRAME == cTmp) ||
						(CAN_11BIT_8BYTE_FRAME_EXT == cTmp) ||
							(CAN_29BIT_8BYTE_FRAME_EXT == cTmp)||
								(CAN_11BIT_8BYTE_FRAME_00 == cTmp)||
									(CAN_11BIT_8BYTE_FRAME_7BYTE == cTmp))
				{
					sRetCode = 8;					//forcer le nombre de data a 8
				}
			}
		break;

//QD1_44 Ajout protocole can direct sans protocole
		case	DIRECTFRAME:
			if(sNbData && pcDest)
			{
				memset(pcDest,0x00,8);				//La trame doit faire 8 octets les octets non utilises sont initialises a 00
				memcpy(pcDest,pcSrc,sNbData);

//Tester le protocole utilise. Il est possible que toutes les trames fassent 8 bytes de long
				cTmp = GetProtocoleObd();
				if(CAN_11BIT_8BYTE_DIRECT == cTmp)
				{
					sRetCode = 8;				//forcer le nombre de data a 8
				}
			}
		break;
//QD1_44
	}
	return (sRetCode);								//retourne le nombre d'octet dans le buffer
}
/*_______________________________________________________________________ 
|   procedure:  CanDebugSendFrame		                                  |
|_________________________________________________________________________|
|   Description  : Impression d'une trame recue ou a tramettre			  |
|   En entree: 											                  |
|   pcSrc = Adresse des donnees de la trame 			                  |
|   sLen  = Longueur de la trame a transmettre 			                  |
|   si sLen = 0 --> la longueur est dans la trame a emettre	              |
|   En sortie: Rien											              |
|_________________________________________________________________________|*/
void CanDebugSendFrame(UCHAR *pcSrc, short sLen)
{
#ifdef DEBUGCAN

UCHAR cTypeFrame;
short sNbData = 0;
short i;

UCHAR ucOffsetData;

	ucOffsetData = GetOffsetDataCan();

	if(sLen == 0)
	{
		printf("Rx: ");
	}
	else
	{
		printf("Tx: ");
	}
//QD1_44 Ajout protocole can direct sans protocole
//	cTypeFrame = pcSrc[ucOffsetData];	//4 bits de poids fort type de trame
//	cTypeFrame = cTypeFrame & 0xF0;		//ne garder que le type de trame
	cTypeFrame = GetTypeFrame(pcSrc);

	switch(cTypeFrame)
	{
		case	SINGLEFRAME:						//single frame
			sNbData = pcSrc[ucOffsetData] & 0xF;	//ne garder que le nombre de donnee
			sNbData = sNbData + (short)ucOffsetData + 1;		//pour imprimer l'octet de controle
		break;
   	
		case	DIRECT_FRAME:					//pas de protocole
		case	FIRSTFRAME:						//first frame
			sNbData = 8;
		break;

		case	CONSECUTIVEFRAME:						//consecutive frame
			if(sLen)
			{
				sNbData = sLen;
			}
			else
			{
				sNbData = 8;
			}
		break;

		case	FLOWCONTROL:						//flow control
			if(sLen)
			{
				sNbData = sLen;
			}
			else
			{
				sNbData = 3 + (short)ucOffsetData;				//3 ou 4 octets de donnees
			}
		break;
 	}

	//impression des donnees
	for(i=0; i < sNbData; i++)
	{
		printf("%02x ",*pcSrc);
		pcSrc++;				//caracetre suivant
	}
	printf("\n");				//nouvelle ligne
#endif
#ifndef DEBUGCAN 
	if(pcSrc && sLen)
	{
		Nop();
	}
#endif
}

/*______________________________________________________________________
|   procedure:  GetDataFromCanDiag		                                    |
|_______________________________________________________________________|
|   Description  : recupere les donnees utiles d'une trame OnDiag	    |
|   En entree: 										                    |
|   	pcDest = Adresse de rangement des donnees utiles	            |
|   	pcSrc  = Adresse buffer trame recue				                |
|   	uSize  = Taille max de la trame de donnee		                |
|   En sortie: taille de la trame de donnee ou -1 si erreur             |
|_______________________________________________________________________|*/
short GetDataFromCanDiag(UCHAR *pcDest , UCHAR *pcSrc, short sSize)
{
short sCodeRet =0;					//par defaut il y a une erreur
short i;
short sNbCar;
short sNbCarMax;
short sNumMessage;
UCHAR ucOffsetData;

	if(pcDest && pcSrc && sSize)
	{
		memset(pcDest,0,sSize);	
		ucOffsetData = GetOffsetDataCan();
		i = GetTypeFrame(uObdBufRx);
//QD1_44 ajout protocole can direct san protocole
		if(i == DIRECTFRAME)
		{
			sNbCar = 8;				//longueur fixe
			if(sSize >= sNbCar)
			{
				memcpy(pcDest,pcSrc,sNbCar);	//recopier les data utiles
				sCodeRet = sNbCar;				//Tout va bien recuperation trame reussi
			}
		}
//QD1_44
		else
		if(i == SINGLEFRAME)
		{
			UCHAR *pdata = 0;
			sNbCar = pcSrc[ucOffsetData] & 0xF;		//ne garder que la longueur
			if(sNbCar != 0)
			{
				pdata = &pcSrc[1+ucOffsetData];
			}
			else
			{//canFD
				sNbCar = pcSrc[1+ucOffsetData];
				pdata = &pcSrc[2+ucOffsetData];
			}
			if(sSize >= sNbCar)
			{
				memcpy(pcDest,pdata,sNbCar);	//recopier les data utiles
				sCodeRet = sNbCar;								//Tout va bien recuperation trame reussi
			}
		}
		else
		{
			if(i == FIRSTFRAME)
			{
				sNbCarMax = GetNbDataFirstFrame(pcSrc);	//nombre d'octets utiles 
				if(sSize >= sNbCarMax)
				{
					i=0;
	//debug
	CanDebugSendFrame(pcSrc,0);
	//debug
					sNbCar = CpyDataFrame(pcDest, pcSrc);
					do
					{
						i++;
	//debug
	CanDebugSendFrame(&pcSrc[i*8],0);
	//debug
	
						sNbCar += CpyDataFrame(&pcDest[sNbCar] , &pcSrc[i*8]);
						sNumMessage = GetNumMessageConsFrame(&pcSrc[i*8]);
					}
					while((sNumMessage != ERREURFRAME) 
										&& (sNumMessage == i)  
													&& (sNbCar < sNbCarMax));					
					if(sNumMessage != ERREURFRAME)
					{
						//V1.41 mise ajour de la derniere N_TA recue
						if(ucOffsetData)
						{
							ucN_TA_rx =*pcSrc;
						}
						//V1.41
						sCodeRet = sNbCarMax;			//Tout va bien recuperation trame reussi
					}
				}
			}
		}	
	}
	return(sCodeRet);
}

/*_________________________________________________________________________
|   procedure:  SendDataCanOnDiag		                                   |
|__________________________________________________________________________|
|   Description  : Transmission d'un buffer vers le calculateur			   |
|   Cette procedure permet de construire et d'envoyer une trame sur un 	   |
|   calculateur	en respectant le protocole OnDiag (Single Frame, FirstFrame|
|   ConsecutivFrame)													   |
|   En entree: pData = pointeur sur la zone data			               |
|   		   sNbByte = Nombre de donnees a transmettre	               |
|   En sortie: Rien										                   |
|__________________________________________________________________________|*/
void SendDataCanOnDiag(UCHAR *pData, short sNbByte)
{
short sCmd=SINGLEFRAME;
short sNbData;							//nombre de donnees de la trame
	if(pData && sNbByte)
  	{
		cMemCanEtat = cCanEtat;			//memorisation etat automate apres transmission de donnnee
  		memset(uObdBufRx, 0, sizeof(uObdBufRx));		//remplir le buffer reception
		CanReceive( uObdBufRx, 1, ReceiveCan);			//reception d'une trame Can

//QD1_44 Ajout protocole can direct sans protocole
		if(GetProtocoleObd() == CAN_11BIT_8BYTE_DIRECT)
		{
			sCmd = DIRECTFRAME;					//pas de protocole 
			cCanEtat = CAN_WAIT_REP_ONDIAG;			//attendre la reponse du calculateur
		}
		else
		{
	//V1.01 :bug: pas d'emission si protocole CAN avec adresse etendue et Nombre de data a emettre egal a 7 (CAN_11BIT_FRAME_EXT,CAN_11BIT_8BYTE_FRAME_EXT,CAN_29BIT_FRAME_EXT,CAN_29BIT_8BYTE_FRAME_EXT)
	//Probleme observe avec le protocole Bmw_CAN)
	//		if(sNbByte >= 8)
			if(sNbByte >= (8-GetOffsetDataCan()))
	//V1.01
			{
				sCmd = FIRSTFRAME;
				cCanEtat = CAN_WAIT_FLOW_CONTROL;
			}
			else
			{
				cCanEtat = CAN_WAIT_REP_ONDIAG;			//attendre la reponse du calculateur
			}
		}
		sNbData = BuildFrameCan(sCmd,uObdBufTx, pData, sNbByte);		//construction buffer a transmettre
		pTxData = pData;						//init pointeur sur buffer data
		pFoncTimeOut = NULLPROC;				//Pas de gestion du time out
		wCanTimeOut = NO_TIMEOUT;				//Pas de time out reponse calculateur
//debug
CanDebugSendFrame(uObdBufTx,sNbData);
//debug

		CanTransmit(lIdTx, ucNbBitId, (UCHAR*)uObdBufTx, sNbData);	//transmission buffer sur can

	}		
	else			//si aucun octet a transmettre il faut passer en reception
	{
		cMemCanEtat = cCanEtat;			//memorisation etat automate apres transmission de donnnee
  		memset(uObdBufRx, 0, sizeof(uObdBufRx));		//remplir le buffer reception
		CanReceive( uObdBufRx, 1, ReceiveCan);			//reception d'une trame Can
		cCanEtat = CAN_WAIT_REP_ONDIAG;			//attendre la reponse du calculateur
	}
}

/*_________________________________________________________________________
|   procedure:  OnDiagSetBlockSize		                                   |
|__________________________________________________________________________|
|   Description  : Initialisation du nombre de consecutives frames avant   |
|   reception d'une nouvelle trame flow control	   						   |	
|   En entree: cBlockSize = nombre max de consecutives frames              |
|   En sortie: Rien										                   |
|__________________________________________________________________________|*/
void OnDiagSetBlockSize( UCHAR uBSize)
{
	uBlockSize = uBSize;
}

//CHERRY_00
/*_________________________________________________________________________
|   procedure:  OnDiagGetBlockSize		                                   |
|__________________________________________________________________________|
|   Description  : Recuperation du nombre de consecutives frames avant     |
|   reception d'une nouvelle trame flow control	   						   |	
|   En entree: Rien														   |
|   En sortie: Nombre de consecutives frames avant reception Flow controle |
|__________________________________________________________________________|*/
UCHAR OnDiagGetBlockSize( void)
{
UCHAR uRet = 255;
	if(uBlockSize != 0)
	{
		uRet = uBlockSize;
	}
	return(uRet);
}

/*_______________________________________________________________________ 
|   procedure:  ObdCmpFrame		                                  		 |
|________________________________________________________________________|
|   Description  : Comparaison de 2 trames obd							 |
|   En entree: 										                  	 |
|   pcSrc  = Adresse trame obd No 1 				                  	 |
|   pcSrc2 = Adresse trame obd No 2 			                 		 |
|   NbData = Nombre de donnees a comparer				                 |
|   En sortie: 0 --> Les 2 trames sont identiques			             |
|   			1 --> Les 2 trames sont differentes			             |
|________________________________________________________________________|*/
short ObdCmpFrame(UCHAR *pcSrc, UCHAR *pcSrc2, short sNbData)
{
short sRetCode = 0;		//par defaut les 2 trames sont identiques
short i;
	for(i=0;((i<sNbData)&&(!sRetCode));i++)
	{
		if((*pcSrc++) != (*pcSrc2++))
		{
			sRetCode = 1;	//caractere different
		}			
	}
	return (sRetCode);
}

/*________________________________________________________________________
|   procedure:  CanDiag                                               	  |
|_________________________________________________________________________|
|   Description  : Transmission reception trame can + segmentation trame  |
| KWP2000																  |
|   En entree: Rien				   							              |
|   En sortie: Rien										                  |
|_________________________________________________________________________|*/
void CanDiag(void)
{
WORD sNbCar;
//UCHAR cNtq;
WORD sTmp;
UCHAR ucOffsetData = 0;

	switch(cCanEtat)
	{
		case CAN_REPOS:
		break;

		case CAN_INIT_CONF:       		//init communication Id Can reception
		
		/* Request Configuration Mode */
			ecanSetMode(ECAN_CONFIG_MODE);
			
//init Masque id
//			ecanWriteRxAcptMask(0,lIdMask,(WORD)ucNbBitId);			// Masque 0, les bit a zero sont masques , masque sur Id 11ou29bits Ext
			
//Init Rx id (id can attendu en reception)
//			ecanWriteRxAcptFilter(0, lIdRx, (WORD)ucNbBitId, 1, 0);	// Filtre 0, 708, 11ou29bits, buf, Masque 0
            ecanSetRxAcptFilterAndMask(lIdRx, lIdMask, (WORD)ucNbBitId);

//CA1_42_7 Ajout baud rate configurable
            ecanSetBitTimingAndBaudRate(lBaudRate);
			
			if(lDataBaudrate > lBaudRate)
			{//data baudrate must be higher than nominal one and different to be in FD mode.
				ecanSetBitTimingAndBaudRateFDDataphase(lDataBaudrate);
			}
			/*if(lBaudRate <= _250KBITS)								//T520_84 il faut etre compatible avec les vitesses <= 250kbits
			{
				//250Kbits/s 20 quantums par bits  1 Sync + 8 Propagation + 7 phase1 + 4 phase2 + 1 SJW)
				cNtq = ecanSetBitTiming(_8TQ, _7TQ, _4TQ, _1TQ);	
			}
			else
			{
				//500Kbits/s 20 quantums par bits  1 Sync + 8 Propagation + 7 phase1 + 4 phase2 + 3 SJW)
				cNtq = ecanSetBitTiming(_8TQ, _7TQ, _4TQ, _3TQ);	
			}
			if(cNtq)
			{
				lBaudRate = ecanSetBaudrate(lBaudRate, _FREQ_CAN, cNtq);
			}*/
//CA1_42_7

		/* Request Normal Mode */
			ecanSetMode(ECAN_NORMAL_FD_MODE);

			//C1RXFUL1 = C1RXFUL2 = C1RXOVF1 = C1RXOVF2 = 0x0000;   //clear CAN FIFO flags, should implement, not finish, 20210630
			//C2RXFUL1 = C2RXFUL2 = C2RXOVF1 = C2RXOVF2 = 0x0000;   //clear CAN FIFO flags, should implement, not finish, 20210630

			cCanEtat = CAN_REPOS;
		break;

//Automate de transmission --> attente d'une trame flow control avant transimmission des autres paquets
		case	CAN_WAIT_FLOW_CONTROL:
//debug
CanDebugSendFrame(uObdBufRx,0);
//debug

//CA1_42_7 ajout can extended format
			ucOffsetData = GetOffsetDataCan();

			sTmp = ObdCmpFrame(&uObdBufRx[ucOffsetData],
						(UCHAR*)CanRepFlowControl,
								sizeof(CanRepFlowControl));
			if(sTmp == 0)	
			{
				pFoncTimeOut = NULLPROC;		//gestion du time out
				wCanTimeOut = uObdBufRx[2+ucOffsetData];		//transmission avec le time out fournit par le calculateur
//le temps entre 2 trames peut etre nul
				if(wCanTimeOut == 0)
				{
					wCanTimeOut = 10;			//forcer 10ms					
				}
				OnDiagSetBlockSize(uObdBufRx[1+ucOffsetData]);	//Init Block size
				cCanEtat = CAN_SEND_CONSECUTIVE_FRAME;
			}
			else
			{
				CanReceive( uObdBufRx, 1, ReceiveCan);		//erreur de reception dans la trame du calculateur
			}
		break;

//automate de transmission des data --> transmission CONSECONTIVFRAME sur timeout
		case	CAN_SEND_CONSECUTIVE_FRAME:
//tester si attente flow controle a la prochaine trame
			sTmp = BuildFrameCan(GET_NUM_MESSAGE,0,0,0);//Recuperer le numero de message a envoyer
			sNbCar = OnDiagGetBlockSize();				//Recuperer le block size
			if((sTmp % sNbCar) == 0)					//Au prochain block il faut attendre un flow control
			{
 		 		memset(uObdBufRx, 0, sizeof(uObdBufRx));		//remplir le buffer reception
				CanReceive( uObdBufRx, 1, ReceiveCan);			//reception d'une trame Can
				cCanEtat = CAN_WAIT_FLOW_CONTROL;
			}

			sNbCar = BuildFrameCan(CONSECUTIVEFRAME,uObdBufTx, pTxData, 0);		//construction buffer a transmettre
//tester si derniere trame
			sTmp = BuildFrameCan(GET_NB_CAR_RESTANT,0,0,0);	//Recuperer le nombre d'octet restant
			if(!sTmp)			
			{						
				pFoncTimeOut = NULLPROC;		//gestion du time out
				wCanTimeOut = NO_TIMEOUT;		//time out reponse calculateur
				uObdBufRx[0] = 0xFF;				//introduire une erreur dans le buffer de reception
				//attente d'une reponse du calculateur 
				CanReceive( uObdBufRx, 1, ReceiveCan);			//reception d'une trame Can
				//Attente reponse
				cCanEtat = CAN_WAIT_REP_ONDIAG;
			}
//debug
CanDebugSendFrame(uObdBufTx,sNbCar);
//debug
			CanTransmit(lIdTx, ucNbBitId, (UCHAR*)uObdBufTx, sNbCar);	//transmission buffer sur can
		break;

//Reception d'une trame de donnee segmentee apres une transmission segmentee
		case	CAN_WAIT_REP_ONDIAG:
//debug
CanDebugSendFrame(uObdBufRx,0);
//debug
				ucOffsetData = GetOffsetDataCan();
				sTmp = GetTypeFrame(uObdBufRx);
//QD1_44 Ajout protocole can direct sans protocole
				if(sTmp == DIRECTFRAME)
				{
					cCanEtat = cMemCanEtat;			//fin la la reception
					OsSendEvent(&CanSem,EVT_CAN_UNLOCK,NULL);
				}
//QD1_44
				else
				{
					if(sTmp == SINGLEFRAME)
					{
						UCHAR ucDatalenght = uObdBufRx[ucOffsetData] & 0xf;
						if(ucDatalenght == 0)
						{
							ucDatalenght = uObdBufRx[1+ucOffsetData];
						}
						
						if(ucDatalenght)			//single frame avec une longueur
						{
							cCanEtat = cMemCanEtat;			//fin la la reception
							OsSendEvent(&CanSem,EVT_CAN_UNLOCK,NULL);
						}
						else
						{
							//Erreur relancer la reception d'une trame
							CanReceive( uObdBufRx, 1, ReceiveCan);			//reception d'une trame Can
						}
					}
					else
					{
						if(sTmp == FIRSTFRAME)
						{
							UCHAR	cTmp;
							sNbCar = GetNbDataFirstFrame(uObdBufRx);	//nombre d'octets utiles 
							sNbCar = sNbCar/(7-ucOffsetData);			//nombre de trame a recevoir
							//lancer la reception sur le reste de la trame
							CanReceive(&uObdBufRx[8], sNbCar, ReceiveCan);			//reception d'une ou plusieures trames Can
							cCanEtat = CAN_WAIT_CONSECUTIVE_FRAME;
		
		//V1.04 flow control configuarable
							cTmp = GetProtocoleObd();
							if(CAN_11BIT_8BYTE_FRAME_00 == cTmp)
							{
								sNbCar = BuildFrameCan(FLOWCONTROL,uObdBufTx, Tab00CanFlowControl,SIZE_FLOWCONTROL);		//construction buffer a transmettre
							}
							else
							{
								sNbCar = BuildFrameCan(FLOWCONTROL,uObdBufTx, TabCanFlowControl,SIZE_FLOWCONTROL);		//construction buffer a transmettre
							}
		//V1.04
		
		//debug
		CanDebugSendFrame((UCHAR*)uObdBufTx,sNbCar);
		//debug
							CanTransmit(lIdTx, ucNbBitId, (UCHAR*)uObdBufTx, sNbCar);	//transmission flow control
		
						}
						else
						{
							CanReceive( uObdBufRx, 1, ReceiveCan);				//erreur de reception dans la trame du calculateur
						}
					}
				}
			break;

 //fin de reception de trame segmentee
		case	CAN_WAIT_CONSECUTIVE_FRAME:			
			cCanEtat = cMemCanEtat;			//fin la la reception
			OsSendEvent(&CanSem,EVT_CAN_UNLOCK,NULL);
		break;

//start emission buffer sur ligne CAN
		case	CAN_SEND_DATA:
				wCanTimeOut = NO_TIMEOUT;		//pas de time out
				pFoncTimeOut = NULLPROC;		//pas de fonction de timeout

				cCanEtat = CAN_END_RX_DATA;			//Etat en fin de transmission
				SendDataCanOnDiag(uObdBufData,wCaraTxOnDiag);
		break;

//Reception d'une trame complete --> transmission des donnees au vT55	
		case	CAN_END_RX_DATA:
			sNbCar = (WORD)GetDataFromCanDiag(uObdBufData,uObdBufRx, sizeof(uObdBufData));
			if(sNbCar != 0)
			{
				Rs232SendDataObd(uObdBufData, (UCHAR)sNbCar);				//transmission des donnees recue au vt55
			}
			//repasser en reception quoiqu'il arrive
			SendDataCanOnDiag(uObdBufData,0);
		break;
	}
}

/*______________________________________________________________________ 
|   Procedure   : EcuConfCan	                         	            |
|_______________________________________________________________________|
|   Description  : Init configuration port can							|
|   En entree: Rien														|
|   En sortie: Rien											        	|
|_______________________________________________________________________|*/
void EcuConfCan(void)
{
	OsSendEvent(&CanSem,EVT_INIT_CONF_CAN,NULL);			//emission evenement
}

/*______________________________________________________________________ 
|   Procedure   : EcuSendDataOnDiag	                         	        |
|_______________________________________________________________________|
|   Description  : Lancer la transmission des donnees sur la ligne 		|
|K_Line ou Can															|
|   En entree: pData --> pointeur sur les donnees a transmettre			|
|   		   wSize --> longueur des donnees a transmettre				|
|   		   cEvt  --> evenement a transmettre						|
|   En sortie: Rien											        	|
|_______________________________________________________________________|*/
void EcuSendDataOndiag(UCHAR *pData, WORD wSize, UCHAR cEvt)
{
	if(pData && wSize)
	{
		memcpy(uObdBufData,pData,wSize);			//copier le buffer a transmettre
		wCaraTxOnDiag = wSize;						//init nombre de caractere a transmettre sur K_Line ou Can
		OsSendEvent(&CanSem,cEvt,NULL);				//emission evenement
	}
}

/*______________________________________________________________________ 
|   Procedure   : EcuSendWordOnDiag	                         	        |
|_______________________________________________________________________|
|   Description  : Lancer la transmission des donnees sur la ligne 		|
|K_Line ou Can															|
|	Chaque data est codee sur un word			 						|
|   En entree: pData --> pointeur sur les donnees a transmettre			|
|   		   wSize --> longueur des donnees a transmettre				|
|   		   cEvt  --> evenement a transmettre						|
|   En sortie: Rien											        	|
|_______________________________________________________________________|*/
void EcuSendWordOndiag(WORD *pwData, WORD wSize, UCHAR cEvt)
{
WORD i;
	if(pwData && wSize)
	{
		for(i=0; i<wSize; i++)
		{
			uObdBufData[i] = (UCHAR) *pwData;			//copier le buffer a transmettre
			pwData++;									//data suivante
		}
		wCaraTxOnDiag = wSize;							//init nombre de caractere a transmettre sur K_Line ou Can
		OsSendEvent(&CanSem,cEvt,NULL);					//emission evenement
	}
}

/*______________________________________________________________________ 
|   Tache    : CanTask			                         	            |
|_______________________________________________________________________|
|   Description  : Tache gestion protocole Can + segmentation des trames|
|   can																	|
|   En entree: Rien									                 	|
|   En sortie: Rien									                 	|
|_______________________________________________________________________|*/
void __attribute((far))CanTask(void)
{
	ecanInit();
	wCanTimeOut = NO_TIMEOUT;

	while(1)
	{
		pMsgCan = OsWaitEvent(&CanSem,wCanTimeOut,T_STOP_ON_EVT);
		cEvtCan = pMsgCan->ucEvt;		//recuperer l'evenement
		switch(cEvtCan)
		{
			case EVT_TIMER:						//executer la fonction liee au timeout si elle existe
				if(pFoncTimeOut != NULLPROC)
				{
					pFoncTimeOut();
				}
			break;
			
			case	EVT_INIT_CONF_CAN:			//init Vitesse, Id can Rx et Tx
				cCanEtat = CAN_INIT_CONF;
			break;

			//transmission de x data sur la ligne Can
			//les donnees sont uObdBufData et la taille dans wCaraTxOnDiag
			case	EVT_SEND_DATA_CAN:
				cCanEtat = CAN_SEND_DATA;
			break;

			default:
				wCanTimeOut = NO_TIMEOUT;
			break;
		}
		//automate gestion can
		CanDiag();
	}
}


