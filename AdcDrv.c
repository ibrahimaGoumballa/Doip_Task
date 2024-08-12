/*______________________________________________________________________ 
|   Project      :OBD Module T52088                                     |
|   Module       :driver conversion of analog function  				|
|   File         :AdcDrv.c                                         		|
|_______________________________________________________________________|
|   Description  :Detect the 12V battery from car                       |
|   Date : 2021/06/10 											        |
|   Auteur :Wolfgang    										        |
|_______________________________________________________________________|*/
#include <xc.h>
#include "../Common_Header/NewType.h"
//#include "interrupt.h"
#include "adcDrv.h"
#include "Map.h"

/*______________________________________________________________________ 
|                        Elements externes                              |
|_______________________________________________________________________|*/
	
/*______________________________________________________________________ 
|                        declaration des constantes                     |
|_______________________________________________________________________|*/
#define MAX_CONV_BAT	10				//10 points moving average
#define SEUIL_MIN_BAT	1200			//VBAT_12V over 8.7 volts to be valid voltage  (30 Volts --> 4096pt)
					
/*______________________________________________________________________ 
|                        Elements publics                               |
|_______________________________________________________________________|*/

/*______________________________________________________________________ 
|                        Elements locaux                                |
|_______________________________________________________________________|*/
static WORD wBat=0;			//tension batterie

/*______________________________________________________________________ 
|   Procedure    : InitADC                          		            |
|_______________________________________________________________________|
|   Description  : Initialisation module conversion analogique digitale	|
|   En entree: Rien									                 	|
|   En sortie: Rien									                 	|	
|_______________________________________________________________________|*/
void InitADC(void)
{
    //all settings are inside ADC1_Initialize()
}

WORD GetAN2Value(void)
{
    return wBat;
}


/*______________________________________________________________________ 
|   Procedure    : IsConnectedOnCar                    		            |
|_______________________________________________________________________|
|   Description  : Initialisation module conversion analogique digitale	|
|   En entree: Rien									                 	|
|   En sortie: FALSE --> Le vehicule n'est pas connecte                	|	
|  			   TRUE --> Le vehicule est pas connecte                	|	
|_______________________________________________________________________|*/
UCHAR IsConnectedOnCar(void)
{
	UCHAR ucRet = FALSE;
	if(wBat > SEUIL_MIN_BAT)
	{
		ucRet = TRUE;					//la voiture est connectee
	}
	else
	{
		Nop();
	}
	return(ucRet);
}

/*______________________________________________________________________ 
|   Procedure    : ServiceBat	                    		            |
|_______________________________________________________________________|
|   Description  : Lecture de la tension de batterie					|
|   Cette fonction doit etre appelee en boucle de fond ou a intervales	|
|   reguliers															|
|   En entree: Rien									                 	|
|   En sortie: Rien									                	|	
|_______________________________________________________________________|*/
void ServiceBat(void)
{
static WORD wIndex = 0;
static WORD wSumConv = 0;
WORD wTmp;

    if(ADSTATLbits.AN2RDY)
	{//ADC convert done
        //EN_CANLS ^= 1;    //toggle, for debug
		wTmp = ADCBUF2;				//get ADC value of AN2
		if(wIndex >= MAX_CONV_BAT)
		{
			//le filtre est full
			wSumConv = wSumConv -(wSumConv/MAX_CONV_BAT);
			wSumConv += wTmp; 
		}
		else
		{
			//le filtre est vide
			wIndex++;
			wSumConv += wTmp;
		}
		wBat = wSumConv/wIndex;						//mise a jour tension batterie
		
	}
}
