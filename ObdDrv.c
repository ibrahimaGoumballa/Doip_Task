/*______________________________________________________________________ 
|   Project      :Interface comunication OBD carte T52088               |
|   Module       :driver communication obd 								|
|   File         :Obddrv.h                                         		|
|_______________________________________________________________________|
|   Description  :Ensemble de procedure pour transmettre et recevoir	|
|   des donnees sur la ligne K_LINE										|
|   Date 21/06/2021 - 											        |
|   Author : Wolfgang                                                   |
|_______________________________________________________________________|*/
#include <xc.h>
#include "Kernel.h"
#include <stdio.h>
#include "../Common_Header/NewType.h"
#include "rs232tsk.h"
#include "obdDrv.h"
#include "Map.h"
#include "interrupt.h"
#include "uart.h"
#include "EcanDrv.h"
#include "./mcc_generated_files/clock.h"



/*______________________________________________________________________ 
|                        Elements externes                              |
|_______________________________________________________________________|*/
	
/*______________________________________________________________________ 
|                        declaration des constantes                     |
|_______________________________________________________________________|*/
/*______________________________________________________________________ 
|                        Elements publics                               |
|_______________________________________________________________________|*/
/*______________________________________________________________________ 
|                        Elements locaux                                |
|_______________________________________________________________________|*/

static UCHAR cCarObd;			//caractere recu ou a emettre sur les lignes OBD
static UCHAR cNbCarRxObd;		//nombre de caracteres recus sur la ligne k line
static UCHAR cNbCarRestant;		//nombre de caracteres restant a recevoir
static UCHAR cCRC;				//crc trame recu

static UCHAR *pBufRxObd = NULL;		//pointeur sur buffer reception
static UCHAR cObdProtocole;		//protocole de commnication utilise (KW2000 ou ISO9141)

static PF pfFoncEndRxObd = NULLPROC;	//fonction appelee en fin de reception d'une trame OBD

//Iso9141
static WORD wObdRxTimeOut=0;			//time en ms pour declencher le decodage d'une trame OBD
//Iso9141

//-------prototypes-------
void DisableUART2();


/*______________________________________________________________________ 
|   Procedure    : SetPinObd		                     	            |
|_______________________________________________________________________|
|   Description  : Selection des pins de sorties OBD					|
|   En entree: Numero des Pin a selectionner							|
|	K_LINE_PIN7															|
|	K_LINE_PIN8															|
|	K_LINE_PIN1															|
|	K_LINE_PIN12														|
|	K_LINE_ALL															|
|	K_LINE_OFF															|
|	K_RESET_ECU															|
|	K_CANHS_PIN6_PIN14													|
|   En sortie: rien									                 	|
|_______________________________________________________________________|*/
void SetPinObd( WORD wVal)
{
    switch(wVal)
    {
        case	K_LINE_PIN7:
            Kline_7();			//K_LINE --> OBD PIN 7
        break;

        case	K_LINE_PIN8:
            Kline_8();			//K_LINE --> PIN 8
        break;

        case	K_LINE_PIN1:
            Kline_1();			//K_LINE --> PIN 1
        break;

        case	K_LINE_PIN12:
            Kline_12();		//K_LINE --> PIN 12
        break;

        case K_LINE_PIN9:
            Kline_9();
        break;

        case	K_RESET_ECU:
            RST_K_LINE = 1; //pull OBD_PIN_13 to GND
        break;

        case K_LINE_OFF:
            RST_K_LINE = 0; //porting from VT41, T52084 did not do this action
            CAN_HS_6_14();			//canh --> PIN 6, canl --> PIN 14
            SetEcanChannel(_ECAN1_CHN);                    
        break;

        //PSA HS 3 et 8 
        case K_CANHS_PIN3_PIN8:
            CAN_HS_3_8();		//canh --> PIN 3, canl --> PIN 8
            SetEcanChannel(_ECAN1_CHN);		//selection can HS
        break;

        //low Spedd single wire low speed
        case K_CANLS_SW_PIN1:
            CAN_LS_1_SW();			//canh --> PIN 1
            SetEcanChannel(_ECAN2_CHN);
        break;

        case	K_CANHS_PIN3_PIN11:
            CAN_HS_3_11();			//canh hs --> PIN 3, canl hs --> PIN 11
            SetEcanChannel(_ECAN1_CHN);		//selection can HS
        break;
		
		case	K_CANHS_PIN11_PIN3:
            CAN_HS_11_3();			//canh hs --> PIN 11, canl hs --> PIN 3
            SetEcanChannel(_ECAN1_CHN);		//selection can HS			
		break;

        case	K_CANLS_PIN1_PIN9:
            CAN_LS_1_9();			//canhls --> PIN 1, canlls --> PIN 9
            SetEcanChannel(_ECAN2_CHN);
        break;

        case	K_CANLS_PIN3_PIN11:
            CAN_LS_3_11();			//canhls --> PIN 3, canlls --> PIN 11
            SetEcanChannel(_ECAN2_CHN);
        break;
		
        case	K_CANHS_PIN12_PIN13:
            CAN_HS_12_13();		//canh hs --> PIN 12, canl hs --> PIN 13
            SetEcanChannel(_ECAN1_CHN);		//selection can HS
        break;

        case	K_CANHS_PIN1_PIN9:
            CAN_HS_1_9();			//canh hs --> PIN 1, canl hs --> PIN 9
            SetEcanChannel(_ECAN1_CHN);		//selection can HS
        break;

        case K_DOIP_PIN3_11_PIN12_13:
            ETHERNET_3_11();
        break;   

        case K_DOIP_PIN1_9_PIN12_13: 
            ETHERNET_1_9();
        break;


        case K_CANLS_PIN6_PIN14: //T52088D0 does not support this configuration, hardware limitation
        case K_LINE_ALL:
        default:
            OBDOff();   
        break;
    }
}

/*______________________________________________________________________ 
|   Procedure    : SetProtocoleObd	                       	            |
|_______________________________________________________________________|
|   Description  : Selection du type de protocole Obd 					|
| et affectation des PIN OBD pour les 2 types de modules OBD 			|
| 520_84 (Ancien) et T520_84 (Nouveau)						 			|	
|   En entree: cType = protocole 					                 	|
|   En sortie: TRUE ou FALSE						                 	|
|_______________________________________________________________________|*/
UCHAR SetProtocoleObd(UCHAR cType)
{
	UCHAR ucRet = FALSE;				//par defaut il y a une erreur
	if(cType < MAX_ECU_PROTOCOLE)
	{
		cObdProtocole = cType;						//init type de protocole
		ucRet = TRUE;								//tout va bien
	}
	return(ucRet);
}


//Iso9141
/*______________________________________________________________________ 
|   Procedure    : SetTimeOutEndRx	                       	            |
|_______________________________________________________________________|
|   Description  : actualisation du time de fin de trame				|
|   En entree: wVal = Valeur en milliseconde du timeout decodage fin de	|
|   trame																|
|   si wVal == 0 --> pas de timeout										|
|_______________________________________________________________________|*/
void SetTimeOutEndRx(WORD wVal)
{
	wObdRxTimeOut = wVal;
}


/*______________________________________________________________________ 
|   Procedure    : IsTimeOutEndRx	                       	            |
|_______________________________________________________________________|
|   Description  : Procedure de test de fin de reception trame OBD sur	|
|   time out															|
|   En entree: wVal = Valeur en milliseconde du timeout decodage fin de	|
|   trame																|
|   si wVal == 0 --> pas de timeout										|
|_______________________________________________________________________|*/
void IsTimeOutEndRx(void)
{
	if(wObdRxTimeOut)
	{
		wObdRxTimeOut--;
		if(wObdRxTimeOut == 0)
		{
			DisableRxCarObd();		//stop reception
			if(pfFoncEndRxObd)
			{
				pfFoncEndRxObd();					//fin de reception
			}
		}
	}
}
//Iso9141

/*______________________________________________________________________ 
|   Procedure    : GetProtocoleObd	                       	            |
|_______________________________________________________________________|
|   Description  : Recuperation du type de protocole KW2000 ou ISO9141	|
|   En entree: Rien									                 	|
|   En sortie: KW2000 ou ISO9141					                 	|
|_______________________________________________________________________|*/
UCHAR GetProtocoleObd(void)
{
	return(cObdProtocole);
}
/*______________________________________________________________________ 
|   Procedure    : GetNbCarRxObd	                       	            |
|_______________________________________________________________________|
|   Description  : Recuperation du nombre de caracteres recus sur la	|
|   ligne K_LINE														|
|   En entree: Rien									                 	|
|   En sortie: Nombre de caracteres recus sur la ligne K_Line         	|
|_______________________________________________________________________|*/
UCHAR GetNbCarRxObd(void)
{
	return(cNbCarRxObd);
}


/*______________________________________________________________________ 
|   Procedure    : SetFoncEndRxObd		                       	            |
|_______________________________________________________________________|
|   Description  : Init pointeur reception Obd							|
|  	En entree:	pFonc --> pointeur sur la fonction a appeler en fin de 	|
|	reception															|
|   En sortie: Rien									                 	|
|_______________________________________________________________________|*/
void SetFoncEndRxObd(PF pFonc)
{
	pfFoncEndRxObd = pFonc;		//fonction appelee en fin de reception d'une trame OBD
}


/*______________________________________________________________________ 
|   Procedure    : InitTimerKLine	                          			    |
|_______________________________________________________________________|
|   Description  : Initialisation du timer emission reception obd	 	|
|   En entree: Rien										             	|
|   En sortie: Rien											          	|	
|_______________________________________________________________________|*/
void InitTimerKLine(void)
{
    CCP2CON1Lbits.CCPON = false;    //stop SCCP2 timer
    IPC6bits.CCT2IP = IT_TIMER_2_LEVEL; //assign interrupt priority
    IFS1bits.CCT2IF = 0;    //reset interrupt flag
    //IEC1bits.CCT2IE = 0;    //disable interruption timer2
}


/*______________________________________________________________________ 
|   Procedure    : InitKLine                          		            |
|_______________________________________________________________________|
|   Description  : Initialize UART2 and Timer for K-LINE and ISO9141	|
|   En entree: Rien									                 	|
|   En sortie: Rien									                 	|	
|_______________________________________________________________________|*/
void InitKLine(void)
{
    //disable UART2, first K-LINE wake up waveform is implemented by GPIO
    DisableUART2();
    IPC6bits.U2RXIP = 1;    //interrupt level 1
    
    SetBaudRateKLine(_10400Baud);		//init K-LINE default baud rate 10400baud (10.4Kbps)
    
	//installation timer obd (reception et transmission)
	InitTimerKLine();

	//Init ligne OBDK
	OBDK = 1;			//bus idle
	//Init ligne OBDL	
	//OBDL = 0;			//L-LINE not implemented in T52088

// l'activation de la ligne obd est faite lorsque la tension batterie est OK 
//	DISABLE_OBD = 1;		//disable ligne Obd
	cObdProtocole = OBD_EXTENDED_FRAME;		//par defaut protocole OBD K_LINE extended frame

	OsTimerLink(IsTimeOutEndRx);			//procedure de fin d'une trame OBD

}


/*______________________________________________________________________ 
|   Procedure    : SetBaudRateKLine                          			    |
|_______________________________________________________________________|
|   Description  : Initialisation de la vitesse de transmission sur la 	|
| 	ligne K_LINE														|
|   En entree: wBaudRate = vitesse de transmission en bit/sec          	|
|   En sortie: La periode en nano sec pour 1 bit			          	|	
|_______________________________________________________________________|*/
void SetBaudRateKLine(long lBaudRate)
{
    unsigned long ulFcy = CLOCK_PeripheralFrequencyGet();   //Fcy = Fp = 70MHz, Fosc = 140MHz
	if(lBaudRate > 0)
	{
		U2BRG = (WORD)(((ulFcy/(unsigned long)lBaudRate)/16) - 1);     // K-LINE speed 1.2Kbps ~ 10.4Kbps
	}
}

void SetBaudRateIndexKLine(UCHAR ucBaudRateIdx)
{
	switch(ucBaudRateIdx)
	{
		default:
		break;
			
		case KLINE_9600:
			SetBaudRateKLine(9600);
		break;
					
		case KLINE_10400:
			SetBaudRateKLine(10400);
		break;
	}
}

/*______________________________________________________________________ 
|   Procedure    : DisableRxCarObd                          		    |
|_______________________________________________________________________|
|   Description  : Interdire la reception des caracteres			 	|
|   En entree: Rien									                	|
|   En sortie: Rien									                 	|	
|_______________________________________________________________________|*/
void DisableRxCarObd(void)
{
	//Stoper la reception caractere
    /*IEC0bits.INT0IE = 0;   					//disable interrupt reception
    IFS0bits.INT0IF = 0;					// clear the interrupt flag 

	//stop timer obd
	DisableTimerObd();*/
}

void DisableUART2()
{
    __builtin_write_RPCON(0x0000); // unlock PPS
    RPOR12bits.RP57R = 0x0000;    //RC9 -> GPIO
    __builtin_write_RPCON(0x0800); // lock PPS
    
    U2MODE = 0x0000;
    
    IEC1bits.U2RXIE = 0;
}

void EnableUART2TX()
{
    __builtin_write_RPCON(0x0000); // unlock PPS
    RPOR12bits.RP57R = 0x0003;      //RC9 -> U2TX
    __builtin_write_RPCON(0x0800); // lock PPS
    
    U2STAH = 0x0022;   //clear TX and RX FIFO
    U2MODEbits.UARTEN = 1;
    U2MODEbits.UTXEN = 1; 

}

void UART2Send1ByteAndWaitLastBitShiftOut(UCHAR ucData)
{//send 1 byte to K-LINE
    
    while(U2STAbits.TRMT == 0);
	U2TXREG = ucData;
    while(U2STAbits.TRMT == 0); //wait until UART2 shift register becomes empty

}


/*__________________________________________________________________________ 
|   Procedure    : ReceiveKLine	                      		            	|
|___________________________________________________________________________|
|   Description  : Begin to receive data from UART2                         |
|   Parameter:  pBuf = Adresse de rangement des caracteres recus			|
| reception																	|
|   Return: void									                 		|	
|___________________________________________________________________________|*/
void ReceiveKLine()
{//begin to receive U2RX data
	cNbCarRxObd = 0;		//clear buffer index to 0	
	U2MODEbits.URXEN = 0;
    
    if(U2STAbits.OERR != 0)
	{
		U2STAbits.OERR = 0;
	}
    U2STAH = 0x0022;   //clear TX and RX FIFO
    U2MODEbits.URXEN = 1;       //Enable U2RX
    IFS1bits.U2RXIF = 0;		//clear RX interrupt flag
    IEC1bits.U2RXIE = 1;		//Enable UART2 RX interrupt
}

void SetKLineRxBuffer(UCHAR *pBuf)
{
    pBufRxObd = pBuf;
}


void RxCarObd(void)
{
    
}

/*__________________________________________________________________________ 
|   Procedure    : EndRxCarObd	                      		            	|
|___________________________________________________________________________|
|   Description  : Un caractere a etait recu sur obdin						|
|   il faut le stocker en buffer et decoder la trame						|
|   En entree:  Rien														|
|   En sortie: Rien									                 		|	
|___________________________________________________________________________|*/
void EndRxCarObd(void)
{
	if(pBufRxObd)
	{
		pBufRxObd[cNbCarRxObd] = cCarObd;		//Ranger le caractere en buffer
	}

	switch(cNbCarRxObd)
	{
		//reception octet Fmt 
		case 0:
			//Detection waik up 
			if(cCarObd & 0xC0)						//si waik up --> rearmer la reception
			{
				cCRC = cCarObd;		//init crc
				cNbCarRestant = cCarObd & 0x3F;				//if it is a short frame, the length must be initialized
				cNbCarRxObd++;								//index++
			}
			//RxCarObd();			//armer la reception d'un caractere
		break;

		//reception octet Tgt
		//reception octet Scr
		case 1:
		case 2:
			cCRC += cCarObd;	//calcule crc
			cNbCarRxObd++;		//passer au caractere suivant		
			//RxCarObd();				//armer la reception d'un caractere
		break;

		//reception octet Len ou data
		case 3:
			if(cNbCarRestant)					//Trame courte sans longueur
			{
				cNbCarRestant--;	//decrementer le nombre d'octet a recevoir
			}
			else							
			{									//Trame longue avec longueur
				cNbCarRestant = cCarObd;		//init nombre de caracteres a recevoir
			}
			cCRC += cCarObd;	//calcule crc
			cNbCarRxObd++;		//passer au caractere suivant		
		break;

		default:
			if(cNbCarRestant && (cNbCarRestant < 0xff))		//reception trame
			{
				cCRC += cCarObd;	//calcule crc
				cNbCarRxObd++;		//passer au caractere suivant		
				cNbCarRestant--;	//decrementer le nombre d'octet a recevoir

			}
			else					//reception crc
			{
				if(cCRC == cCarObd)
				{
					cNbCarRxObd++;			//passer au caractere suivant		
					//DisableRxCarObd();		//stop reception
					//Executer la procedure de fin de reception s'il elle existe
					if(pfFoncEndRxObd)
					{
						pfFoncEndRxObd();		
					}
				}				
			}
		break;
	}
}


/*__________________________________________________________________________ 
|   Procedure    : EndRx0FF0CarObd	                      		            	|
|___________________________________________________________________________|
|   Description  : Un caractere a etait recu sur obdin						|
|   il faut le stocker en buffer et decoder la trame						|
|   En entree:  Rien														|
|   En sortie: Rien									                 		|	
|___________________________________________________________________________|*/
UCHAR ucShift = 0;
void EndRx0FF0CarObd(void)  //wangz need write a new function for 0FF0 firmware 
{
	if(cNbCarRxObd == 0) //Only initialize the ucShift when the start the RX at the beginning
	{
		ucShift =0;
	}

	if(pBufRxObd)
	{
		pBufRxObd[cNbCarRxObd] = cCarObd;		//Ranger le caractere en buffer
	}

	switch(cNbCarRxObd - ucShift*12)
	{
		//reception as 0x0F
		case 0:
			if(cCarObd == 0x0F)//this is the start the frame
			{
				cNbCarRxObd++; //go for next char
			}
			//RxCarObd();			//armer la reception d'un caractere
		break;

		case 1: //data length inlcuded CRC
			cCRC = cCarObd;
			cNbCarRestant = cCarObd;	//not include the F0 at the end
			cNbCarRxObd++;
			//RxCarObd();
		break;

		default:
			if(cNbCarRestant && (cNbCarRestant < 0xff))		//reception trame
			{
				cCRC ^= cCarObd;	//calcule crc
				cNbCarRxObd++;		//passer au caractere suivant		
				cNbCarRestant--;	//decrementer le nombre d'octet a recevoir
				//RxCarObd();				//armer la reception d'un caractere
			}
			else		//we have got all the data			//reception crc
			{
				if(cCarObd == 0xF0) //this indicate the frame is end
				{
					//Check check CRC
					if(cCRC == 0) //it should be zero when data1^data2^....^CS
					{
						cNbCarRxObd++;			//passer au caractere suivant	

						if(pBufRxObd[1+12*ucShift]== 0x09 && pBufRxObd[2+12*ucShift]== 0x14)//There is more than one response 4 for now, need check if all got
						{
							if((pBufRxObd[9+12*ucShift]&0x07)==0x03)//already the last one 0, 1,2,3
							{
								//DisableRxCarObd();		//stop reception
								//Executer la procedure de fin de reception s'il elle existe
								if(pfFoncEndRxObd)
								{
									pfFoncEndRxObd();		
								}
							}
							else
							{
								ucShift = (pBufRxObd[9+12*ucShift]&0x07) +1;
								//RxCarObd();	
							}
						}
						else
						{
							//DisableRxCarObd();		//stop reception
							//Executer la procedure de fin de reception s'il elle existe
							if(pfFoncEndRxObd)
							{
								pfFoncEndRxObd();		
							}
						}
					}
				}				
			}
		break;
	}
}
/*__________________________________________________________________________ 
|   Procedure    : EndRxCarH99	                      		            |
|___________________________________________________________________________|
|   Description  :Traitement reception protocole Honda H99					|
|	Un caractere a etait recu sur obdin										|
|   il faut le stocker en buffer et decoder la trame						|
|   En entree:  Rien														|
|   En sortie: Rien									                 		|	
|___________________________________________________________________________|*/
void EndRxCarH99(void)
{
	if(pBufRxObd)
	{
		pBufRxObd[cNbCarRxObd] = cCarObd;		//copy received character to pBuf
	}

	switch(cNbCarRxObd)
	{
		//reception octet Header byte
		case 0:
			//Detection waik up 
			if(cCarObd)						//si waik up --> rearmer la reception
			{
				cCRC = cCarObd;		//init crc
				cNbCarRxObd++;		//passer au caractere suivant		
			}

		break;

		//reception octet Len
		case 1:
			cNbCarRestant = cCarObd - 3;		//init nombre de caracteres a recevoir
			cCRC += cCarObd;	//calcule crc
			cNbCarRxObd++;		//passer au caractere suivant		

		break;

		//reception data
		default:
			if(cNbCarRestant && (cNbCarRxObd < 0xff))		//reception trame
			{
				cCRC += cCarObd;	//calcule crc
				cNbCarRxObd++;		//passer au caractere suivant		
				cNbCarRestant--;	//decrementer le nombre d'octet a recevoir

			}
			else					//reception crc
			{
				cCRC = ~cCRC;		//not(crc)
				cCRC++;				//Crc + 1
				if(cCRC == cCarObd)
				{
					cNbCarRxObd++;			//passer au caractere suivant		
					//DisableRxCarObd();		//stop reception
					//Executer la procedure de fin de reception s'il elle existe
					if(pfFoncEndRxObd)
					{
						pfFoncEndRxObd();		
					}
				}				
			}
		break;
	}
}

/*________________________________________________________________________
|   procedure:  ObdCrc                                              	  |
|_________________________________________________________________________|
|   Description  : Calcul du crc d'une trame Obd				  		  |
|   En entree: pBuf = adresse debut de trame Obd				   		  |
|   En sortie: crc obd										              |
|_________________________________________________________________________|*/
/*UCHAR ObdCrc(UCHAR *pBuf)
{
	UCHAR cCrc = 0;
	UCHAR cTmp;
	UCHAR i;
	WORD uNbData;
	
	if(pBuf)
	{
		//la longueur est contenu dans l'octet format
//V1.04 bug sur le calcul du crc (Longueur limitee a 3F au lieu de 0xF)
		if((pBuf[Fmt] & 0x3F))
		{
			cTmp = pBuf[Fmt]& 0x3F;
			uNbData = (unsigned short)(cTmp)+ 3;	//il faut tenir compte du header  
		}
//V1.04
		else
		{
			cTmp = pBuf[Len];		//recuperer le nombre d'octets
			uNbData = (unsigned short)(cTmp)+ 4;	//il faut tenir compte du header  
		}
		cCrc = 0;
		i = 0;
		while(uNbData)
		{
			cCrc+= pBuf[i];		//calcul crc
			i++;  
			uNbData--;
		}
	}
	return(cCrc);
}*/

/*________________________________________________________________________
|   procedure:  ObdH99Crc                                              	  |
|_________________________________________________________________________|
|   Description  : Calcul du crc d'une trame Obd Honda H99			  		  |
|   En entree: pBuf = adresse debut de trame Obd			              |
|   Crc iso = NOT(Data0 + Data1 + ... + DataN) + 1		                  |
|   En sortie: crc obd									                  |
|_________________________________________________________________________|*/
/*UCHAR ObdH99Crc(UCHAR *pBuf)
{
	UCHAR cCrc = 0;
	UCHAR cTmp;
	UCHAR i;
	WORD uNbData;

	if(pBuf)
	{
		//la longueur est contenu dans l'octet len
		cTmp = pBuf[Mlen];
		uNbData = (WORD)(cTmp)-1;	//il ne faut pas tenir compte du crc
		cCrc = 0;
		i = 0;
		while(uNbData)
		{
			cCrc+= pBuf[i];			//Somme de toutes les donnees
			i++;  
			uNbData--;
		}
		cCrc = ~cCrc;				//inverser le crc
		cCrc++;						//crc+1
	}
	return(cCrc);
}*/

//Iso9141
/*________________________________________________________________________
|   procedure:  ObdCrcIso9141                                          	  |
|_________________________________________________________________________|
|   Description  : Calcul du crc d'une trame Obd Iso9141		  		  |
|   En entree: pBuf = adresse debut de trame Obd				   		  |
|   		   wNbCar = Nombre de caractere de la trame			   		  |
|   En sortie: crc obd										              |
|_________________________________________________________________________|*/
/*UCHAR ObdCrcIso9141(UCHAR *pBuf, WORD wNbCar)
{
	UCHAR cCrc = 0;
	if(pBuf)
	{
		cCrc = 0;
		while(wNbCar)
		{
			cCrc+= *pBuf;		//calcul crc
			pBuf++;  
			wNbCar--;
		}
	}
	return(cCrc);
}*/


/*__________________________________________________________________________ 
|   Procedure    : EndRxCarIso9141                     		            	|
|___________________________________________________________________________|
|   Description  : Un caractere a etait recu sur obdin avec le protocole 	|
|	iso9141																	|
|   il faut le stocker en buffer et decoder la trame						|
|   En entree:  Rien														|
|   En sortie: Rien									                 		|	
|___________________________________________________________________________|*/
void EndRxCarIso9141(void)
{
	if(pBufRxObd)
	{
		pBufRxObd[cNbCarRxObd] = cCarObd;		//Ranger le caractere en buffer
	}
//tester si 1er caractere coherent
	cNbCarRxObd++;								//caractere suivant

//tester si init
	if((cNbCarRxObd >= 3) &&
			(pBufRxObd[0] == 0) &&	
				(pBufRxObd[1] == 0) &&	
					(pBufRxObd[2] == 0))
	{
		SetTimeOutEndRx(660);						//armer un time out de 460ms pour fin de trame pour repondre
	}
	else
	if(pBufRxObd[0])
	{
		SetTimeOutEndRx(20);						//armer un time out de 20ms pour fin de trame pour repondre (P1)
	}
	RxCarObd();			//armer la reception d'un caractere}
}
//Iso9141



_OS_ISR _U2RXInterrupt( void )
{
    OsIsrIn();
    IFS1bits.U2RXIF = 0;
    
    while(U2STAHbits.URXBE == 0)    // (Rx buffer is not empty)
    {
        cCarObd = U2RXREG;
        if(OBD_FRAME_HONDA99 == cObdProtocole)
        {
            EndRxCarH99();		//received 1 byte, process with protocol Honda 99
        }
        else if(OBD_0FF0_FRAME == cObdProtocole)
        {
            EndRx0FF0CarObd();
        }
        else if((OBD_ISO9141_INIT_5BAUD == cObdProtocole) ||
                (OBD_ISO9141_FRAME == cObdProtocole) )
        {//iso9141
            EndRxCarIso9141();		//traitement du caractere recu iso9141
        }
        else
        {
            EndRxCarObd();			// traitement du caractere recu
        }
    }
    OsIsrOut();
}





