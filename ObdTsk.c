/*______________________________________________________________________ 
|   Project      :Interface comunication OBD T52088                     |
|   Module       :Tache dediee a l'interface OBD (statup + emission	data|
|   File         :ObdTsk.c                                          	|
|_______________________________________________________________________|
|   Description  :interface avec driver obd + generation signal startup	|
|   + emission des data sur ligne k_Line + delai inter caractere 		|
|   Date 24/04/2008 - 											        |
|   Auteur : David BARON										        |
|   History : porting to T52088 by wolfgang since 2021/06/29	        |
|_______________________________________________________________________|*/
#include <xc.h>
#include <string.h>
#include "../Common_Header/NewType.h"
#include "../Common_Header/ObdRs232Cmd.h"
#include "Kernel.h"
#include "obdDrv.h"
#include "obdtsk.h"
#include "Map.h"
#include "interrupt.h"

/*______________________________________________________________________ 
|                        Elements externes                              |
|_______________________________________________________________________|*/

/*______________________________________________________________________ 
|                        declaration des constantes                     |
|_______________________________________________________________________|*/
#define _T200ms	200							//200ms par bit

//liste des evenement decodes par la tache communication OBD
enum
{
	EVT_WAKE_UP_OBD = MAX_OS_EVT,
	EVT_TRANSMIT_OBD,
	EVT_RECEIVE_OBD,
	EVT_END_WAKE_UP_OBD,

//Iso9141
	OBD_IDLE_ISO9141,
	OBD_SEND_BIT_ISO9141
//iso9141


};
/*______________________________________________________________________ 
|                        Elements publics                               |
|_______________________________________________________________________|*/
/*______________________________________________________________________ 
|                        Elements locaux                                |
|_______________________________________________________________________|*/
SEMAPHORE ObdSem ={0,0,0};
SEMAPHORE ObdTmrSem ={0,0,0};

static UCHAR cBufObd[NMAX_CAR_OBD];		//buffer transmission ou reception obd

static EVENT_DEF *pMsgObd;				//pointeur sur message tache obd
WORD wTimeoutObd;						//time out tache obd
static UCHAR cEvtObd;					//evenement tache obd

static UCHAR cNbCarTx = 0;				//nombre de caracteres transmis
static UCHAR cNbCarATx = 0;				//nombre de caracteres a transmettre
static UCHAR cEtatObd = OBD_REPOS;		//variable automate emission reception obd
static WORD wTimeTxCarP4 = T5ms;			//temps entre 2 caracteres en emission par defaut 5 ms
static WORD wStartupLow = T25ms;		//Temps etat bas pour start up 
static WORD wStartupHigh = T25ms;		//Temps etat Haut pour start up  
static volatile WORD wWakeUpTmr = 0;

//iso9141
static UCHAR ucTgtAdr = 0x33;				//adresse ecu pour init is9141 a 5 baud
static UCHAR ucIso9141NumBit = 0;			//numero de bit a transmettre pour l'init
static UCHAR ucIso9141Data = 0;				//data a transmettre a 5 baud
//iso9141

//iso9141
/*______________________________________________________________________ 
|   Procedure    : SetConfObd	                         	            |
|_______________________________________________________________________|
|   Description  : Configuration du delais inter caractere				|
|   En entree:															|
|		ucAdr  --> Adresse ecu cible (Iso9141)		            		|
|		wDelaiCar --> Temps entre 2 caracteres en ms            		|
|		wStartupTLow --> Duree impulsion etat bas startup K_line (ms)	|
|		wStartupTHigh --> Duree impulsion etat haut startup K_line (ms)	|
|   En sortie: Rien									                 	|	
|_______________________________________________________________________|*/
void SetConfObd(UCHAR ucAdr,WORD wDelaiCar, WORD wStartupTLow, WORD wStartupTHigh)
{
	ucTgtAdr = ucAdr;				//adresse ecu pour init 5 baud (ISO9141)
	wTimeTxCarP4 = wDelaiCar;	
	wStartupLow = wStartupTLow;		//Temps etat bas pour start up 
	wStartupHigh = wStartupTHigh;	//Temps etat haut pour start up 
}
//iso9141

/*______________________________________________________________________ 
|   Procedure    : WakeUpObd	                         	            |
|_______________________________________________________________________|
|   Description  : Demarrage sequence wake up							|
|   En entree: Rien									                 	|
|   En sortie: Rien									                 	|
|_______________________________________________________________________|*/
void WakeUpObd(void)
{
	OsSendEvent(&ObdSem,EVT_WAKE_UP_OBD,NULLPTR);		//evenement wake up	
}

/*______________________________________________________________________ 
|   Procedure    : SendDataObd	                         	            |
|_______________________________________________________________________|
|   Description  : Emission d'un buffer sur la ligne obdk				|
|   En entree: pBuff --> adresse du buffer a transmettre               	|
|  				cNbCar --> Nombre de caracteres a transmettre          	|
|   En sortie: Rien									                 	|
|_______________________________________________________________________|*/
void SendDataObd(UCHAR *pBuff, UCHAR cNbCar)
{
	if((cNbCar <= NMAX_CAR_OBD) && pBuff)
	{
		cNbCarATx = cNbCar;							//init nombre d'octets a transmettre (le code de controle n'est pas à transmettre)
		cNbCarTx = 0;								//innit nombre de caracteres tranmis
		memcpy(cBufObd,(void*)pBuff,cNbCar);		//recopier dans le buffer destination
		OsSendEvent(&ObdSem,EVT_TRANSMIT_OBD,NULLPTR);		//evenement transmission obd
	}
}

void SetCCT2Period(WORD wPeriod_ms)
{
    float fPeriod_ns = ((float)wPeriod_ms) * 1000000;
    float fTickTime_ns = 14.2857 * 64; //pre-scale is 64 clock, Fy = 70MHz = 14.2857ns, so fTickTime = 914.2848ns
    float fReload = fPeriod_ns / fTickTime_ns;
    DWORD dwReload = (DWORD)fReload;
    if(wPeriod_ms > 0)
    {
        CCP2CON1Lbits.CCPON = 0;
        CCP2PRL = (WORD)(dwReload & 0x0000FFFF);
        CCP2PRH = (WORD)(dwReload >> 16);
    }
}

void DisableCCT2(void)
{
	CCP2CON1Lbits.CCPON = 0;		//turn off timer 2
	IFS1bits.CCT2IF = 0;			//clear interrupt flag		
	IEC1bits.CCT2IE = 0;			//disable interrupt timer 2
}

void EnableCCT2()
{
    CCP2CON1Lbits.CCPON = 0;		//turn off timer 2
	IFS1bits.CCT2IF = 0;			//clear interrupt flag		
    CCP2TMRL = 0;
    CCP2TMRH = 0;
    IEC1bits.CCT2IE = 1;			//disable interrupt timer 2
    CCP2CON1Lbits.CCPON = 1;		//turn on timer 2
}	

_OS_ISR _CCT2Interrupt ( void )
{
    OsIsrIn();
    IFS1bits.CCT2IF = 0;
    if(cEtatObd == OBD_TX_OBD)
    {
        DisableCCT2();
        OsSendEvent(&ObdSem,EVT_TRANSMIT_OBD,NULLPTR);
    }
    
    OsIsrOut();
}

/*______________________________________________________________________ 
|   Procedure    : ReceiveDataObd	                       	            |
|_______________________________________________________________________|
|   Description  : Passage en reception sur la ligne obd sans passer par|
| la transmission														|
|   En entree: Rien									                 	|
|   En sortie: Rien									                 	|
|_______________________________________________________________________|*/
void ReceiveDataObd(void)
{
	OsSendEvent(&ObdSem,EVT_RECEIVE_OBD,NULLPTR);		//evenement Reception obd sans transmission
}

//Iso9141
/*______________________________________________________________________ 
|   Procedure    : BitSendIso9141_5Baud	                       	        |
|_______________________________________________________________________|
|   Description  : Emission adresse (Init 5 baud) Iso9141				|
| la transmission														|
|   En entree: Rien									                 	|
|   En sortie: Rien									                 	|
|_______________________________________________________________________|*/
void BitSendIso9141_5Baud(void)
{
	switch(ucIso9141NumBit)
	{
		case 0:					//start bit
			wWakeUpTmr = _T200ms;
			OBDK = 1;							//forcer a 0 la ligne obd
			//OBDL = 1;							//L-LINE not implemented in T52088
			ucIso9141NumBit++;					//bit suivant
		break;

		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
		case 6:
		case 7:
		case 8:
			if(ucIso9141Data & 0x1)
			{
				OBDK = 0;						//forcer a 1 la ligne obd
				//OBDL = 0;						//L-LINE not implemented in T52088
			}
			else
			{
				OBDK = 1;						//forcer a 0 la ligne obd
				//OBDL = 1;						//L-LINE not implemented in T52088
			}
			ucIso9141Data =ucIso9141Data >> 1;					//decaler a droite pour transmission future
			ucIso9141NumBit++;					//bit suivant
			wWakeUpTmr = _T200ms;
		break;
		
		default:				//fin de l'emission
			OBDK = 0;							//forcer a 1 la ligne obd
			//OBDL = 0;							//L-LINE not implemented in T52088
			wWakeUpTmr = 1;
			cEtatObd = ODB_END_WAKE_UP;						//transmission adresse 5 baud terminee
		break;
	}
}
//iso9141

/*______________________________________________________________________ 
|   Procedure    : IsrWakeUpSequence                       	            |
|_______________________________________________________________________|
|   Description  : Generation du pulse au demarrage de la communication |
| OBD K_LINE															|
|OBD_SHORT_FRAME ou OBD_EXTENDED_FRAME									|
| 300ms (1)		 25ms(0)	25ms(1)										|
|---------------|_________|---------|Data communication					|
|																		|
|OBD_SHORT_FRAME TOYOTA													|
| 300ms (1)		 35ms(0)	15ms(1)										|
|---------------|_________|---------|Data communication					|
|																		| 
|OBD_FRAME_HONDA99														| 
| 300ms (1)		     70ms(0)		135ms(1)							|
|---------------|_____________|----------------|Data communication		|
|																		| 
|OBD_ISO9141_INIT_5BAUD													|
| 300ms (1)		 Adr 5baud	  0x55	   Key1   Key2	 /Key	  /Adr		|
|---------------|_________|--|_____|--|___|--|___|--|___|-----|___|-----|
|   En entree: Rien									                 	|
|   En sortie: Rien									                 	|
|_______________________________________________________________________|*/
void IsrWakeUpSequence( void)
{
	if(wWakeUpTmr)
	{
		wWakeUpTmr --;
		if(wWakeUpTmr == 0)
		{
			switch(cEtatObd)
			{
//Iso9141
				case OBD_IDLE_ISO9141:
					wWakeUpTmr = 1;
					ucIso9141NumBit = 0;				//transmition start bit (bit 0)
					ucIso9141Data = ucTgtAdr;			//init octet a transmettre
					cEtatObd = OBD_SEND_BIT_ISO9141;	
				break;

				case OBD_SEND_BIT_ISO9141:				//emission adresse ecu a 5 baud
					BitSendIso9141_5Baud();
				break;
//Iso9141

				case OBD_WAKE_UP_TINTL:					//25ms etat bas si KW200 sinon 70ms si IS9141
					wWakeUpTmr = wStartupLow;
					OBDK = 0;							//forcer a 0 la ligne obd
					cEtatObd = OBD_WAKE_UP_TINTH;	
				break;					

				case OBD_WAKE_UP_TINTH:					//25ms etat haut
					OBDK = 1;							//forcer a 1 la ligne obd
					wWakeUpTmr = wStartupHigh;
					cEtatObd = ODB_END_WAKE_UP;	
				break;

				case ODB_END_WAKE_UP:
					OsSendEvent(&ObdTmrSem,EVT_END_WAKE_UP_OBD,NULL);
					cEtatObd = OBD_REPOS;	
				break;

				case	OBD_REPOS:
				break;
			}
		}
	}
}

//iso9141
/*______________________________________________________________________ 
|   Procedure    : ObdEndRxWakeUp	                       	            |
|_______________________________________________________________________|
|   Description  : Procedure appelee en fin de reception d'une trame	|
|   determinee															|
|   En entree: Rien									                 	|
|   En sortie: Rien									                 	|
|_______________________________________________________________________|*/
void ObdEndRxWakeUp(void)
{
	OsSendEvent(&ObdTmrSem,EVT_END_WAKE_UP_OBD,NULL);
}

/*______________________________________________________________________ 
|   Procedure    : ObdWaitWakeup	                       	            |
|_______________________________________________________________________|
|   Description  : Attente fin du motif init obd						|
|   En entree: Rien									                 	|
|   En sortie: Rien									                 	|
|_______________________________________________________________________|*/
void ObdWaitWakeup(void)
{
	do
	{
		pMsgObd = OsWaitEvent(&ObdTmrSem,NO_TIMEOUT,T_STOP_ON_EVT);
	}
	while(pMsgObd->ucEvt != EVT_END_WAKE_UP_OBD);		//attente fin Waikup
}
//iso9141

/*______________________________________________________________________ 
|   Tache    : ObdTask			                         	            |
|_______________________________________________________________________|
|   Description  : Tache gestion ligne obd								|
|   En entree: Rien									                 	|
|   En sortie: Rien									                 	|
|_______________________________________________________________________|*/
void __attribute((far))ObdTask(void)
{
UCHAR cTmp;

	InitKLine();
	wTimeoutObd = NO_TIMEOUT;
//
	OsTimerLink(IsrWakeUpSequence);
	cEtatObd = OBD_REPOS;
    SPI1_Initialize();
    W6100_Init();
    UDP_Connect();
//Init reception obd
	SetFoncEndRxObd(NULLPROC);			//pas de procedure de fin pour le moment

	while(1)
	{
		pMsgObd = OsWaitEvent(&ObdSem,wTimeoutObd,T_STOP_ON_EVT);
		cEvtObd = pMsgObd->ucEvt;		//recuperer l'evenement
		switch(cEvtObd)
		{
			case	EVT_WAKE_UP_OBD:			//start wake up
				cEtatObd = OBD_WAKE_UP_TIDLE;
			break;
		
			case	EVT_TRANSMIT_OBD:
				cEtatObd = OBD_TX_OBD;			//start transmission
			break;	

			case	EVT_RECEIVE_OBD:			//passer en reception
				ReceiveKLine();								//reception obd
				cEtatObd = OBD_REPOS;
				wTimeoutObd = NO_TIMEOUT;
			break;	
			
			default:
				wTimeoutObd = NO_TIMEOUT;
			break;
		}
		//automate gestion ligne obd
		switch(cEtatObd)
		{
			case OBD_REPOS:
			break;
			
			//sequence d'initialisation ligne OBDK
			case OBD_WAKE_UP_TIDLE:					//300ms etat haut
				//DisableRxCarObd();
                DisableUART2();     //use U2TX(RC9) as GPIO to make wakeup waveform
				OBDK = 1;							//bus idle
				wWakeUpTmr = T300ms;
//Iso9141 
				cTmp = GetProtocoleObd();
				if(cTmp == OBD_ISO9141_INIT_5BAUD)
				{
					cEtatObd = OBD_IDLE_ISO9141;
					//attente init 5 bauds
					ObdWaitWakeup();
				}
				else
				{
					cEtatObd = OBD_WAKE_UP_TINTL;
					ObdWaitWakeup();
				}
//Iso9141 
                EnableUART2TX();  //enable UART2 after wake up process finished
			break;					

			//Transmission buffer to K-LINE
			case OBD_TX_OBD:
				if(cNbCarATx)
				{
					//TxCarObd(cBufObd[cNbCarTx]);
                    UART2Send1ByteAndWaitLastBitShiftOut(cBufObd[cNbCarTx]);
					cNbCarTx++; 	//index++
					cNbCarATx--;	//remaining number of bytes
					if(cNbCarATx)
					{
						//wTimeoutObd = wTimeTxCarP4;		//delay between 2 characters, not accurate, set 5ms only measured 4.1ms
                        wTimeoutObd = NO_TIMEOUT;
                        SetCCT2Period(wTimeTxCarP4);    //delay between 2 characters
                        EnableCCT2();
					}
					else				//passer en reception
					{
						ReceiveKLine();								//reception obd
						cEtatObd = OBD_REPOS;
						wTimeoutObd = NO_TIMEOUT;
					}
				}
//iso9141 en cas d'init a 5 baud il faut passer en reception sans data
				else
				{
					ReceiveKLine();								//reception obd
					cEtatObd = OBD_REPOS;
					wTimeoutObd = NO_TIMEOUT;	
				}
//iso9141
			break;
		}
	}
}


