/*______________________________________________________________________ 
|   Project      :LED control task of T52088                            |
|   Module       :T52088            									|
|   File         :TaskIdle.c                                         	|
|_______________________________________________________________________|
|   Description  :background process									|
|   Date 2021/06/07  											        |
|   Auteur : Wolfgang   										        |
|_______________________________________________________________________|*/
#include <xc.h>
#include "Kernel.h"
#include "Map.h"
#include "../Common_Header/NewType.h"
#include "interrupt.h"
#include "uart.h"
#include "Rs232tsk.h"
#include "adcdrv.h"
#ifdef _DoIP
#include "DoIP/Ethernet.h"
#include "DoIP/DOIP_Struct.h"
#include "DoIP/DOIP_Server.h"
#endif

/*______________________________________________________________________ 
|                        Elements externes                              |
|_______________________________________________________________________|*/
/*______________________________________________________________________ 
|                        declaration des constantes                     |
|_______________________________________________________________________|*/
#define T_500ms	500
#define T_1Sec 1000
#define T_2Sec 2000
#define T_10Sec 10000
/*______________________________________________________________________ 
|                        Elements publics                               |
|_______________________________________________________________________|*/
/*______________________________________________________________________ 
|                        Elements locaux                                |
|_______________________________________________________________________|*/
static WORD wTimerLed = T_500ms;						//tempo pour la led de vie
static WORD wTimerCom = T_2Sec;						//tempo pour la led de vie
static WORD wTimerReset = 0;
static WORD wTimerResetInit = T_10Sec;
static WORD g_wLedState = LED_OFF;
static BOOL g_bLedTwinkle = FALSE;

void SetTimerResetInit(WORD wNewTime)
{
	wTimerResetInit = wNewTime;
}

/*______________________________________________________________________ 
|   Procedure    : ClaerWdtCom	                  		       			|
|_______________________________________________________________________|
|   Description  : Fonction de rafraichissement du watchdog Com		 	|
|																	|
|   En entree: Rien					              						|
|   En sortie: Rien					              						|	
|_______________________________________________________________________|*/
void ClrWdtCom( void)
{
	if(wTimerCom == 0)
	{
		ToggleLed(GREEN);
	}
	wTimerCom = T_2Sec;
	wTimerReset = 0;
}


/*______________________________________________________________________ 
|   Procedure    : LedTimer1ms                                          |
|_______________________________________________________________________|
|   Description  : will be triggered every 1ms                       	|
|   																	|
|   En entree: Rien					              						|
|   En sortie: Rien					              						|	
|_______________________________________________________________________|*/
void LedTimer1ms(void)
{
	if(wTimerLed)
	{
		wTimerLed--;
	}
    
	if(wTimerCom)
	{
		wTimerCom --;
        if(wTimerCom == 0)
		{
			ToggleLed(RED);
			wTimerReset = wTimerResetInit;
		}
	}
	
	if(wTimerReset)
	{
		wTimerReset--;
		if(wTimerReset == 0)
		{
			RunBoot();
		}
	}
    
    if(ADCON3Lbits.SWCTRG == 0 )
    {//start ADC convert
        ADCON3Lbits.SWCTRG = 1; //set software common trigger, it will be cleared automatically by HW
    }
        
}

/*______________________________________________________________________ 
|   Procedure    : SetLed	                  		       				|
|_______________________________________________________________________|
|   Description  : Fonction Activation des leds						|
|   En entree: Leds actives			              						|
|   En sortie: Rien					              						|	
|_______________________________________________________________________|*/

void SetLed(BOOL bTwinkle, WORD wState)
{
	g_wLedState = wState;
	g_bLedTwinkle = bTwinkle;
}

/*______________________________________________________________________ 
|   Procedure    : ToggleLed	                  		       			|
|_______________________________________________________________________|
|   Description  : Fonction Activation des leds	Clignotantes				|
|   En entree: Leds actives			              						|
|   En sortie: Rien					              						|	
|_______________________________________________________________________|*/
void ToggleLed( WORD wState)
{
    if(g_wLedState == LED_OFF)
    {
        SetLed(FALSE, wState);
    }
    else    //LED already turned on (RED, GREEN, YELLOW)
    {
        SetLed(FALSE, LED_OFF);
    }
}
//______________________________________________________________________//
//	Procedure	:	ServiceLedVie										//
//______________________________________________________________________//
//	Description	:	Gestion de la led vie appele exclusivement dans		//
//	la tache de fond													//
//	Entrées		:	Rien												//
//	Sorties		:	Rien												//
//______________________________________________________________________//
void ServiceLed(void)
{
static WORD wCurState;
static UCHAR ucLedTwinkleCnt = 0;   //Cnt=0~3:LED ON(400ms), Cnt=4~19:LED_OFF(1600ms)

    //LED control
    if( (wTimerLed == 0) )
    {
        wTimerLed = 100; //100ms
        wCurState = g_wLedState;
        
        if(TRUE == g_bLedTwinkle)
        {
            ucLedTwinkleCnt++;
            if(ucLedTwinkleCnt >= 4)
                wCurState = LED_OFF;
            if(ucLedTwinkleCnt > 19)
            {
                ucLedTwinkleCnt = 0;
                wCurState = g_wLedState;
            }
        }
        
        switch(wCurState)
        {
            case	RED:
                LED_R = 1;
                LED_G = 0;
            break;

            case	YELLOW:
                LED_R = 1;
                LED_G = 1;
            break;

            case	GREEN:
                LED_R = 0;
                LED_G = 1;
            break;

            default:    //LED_OFF
                LED_R = 0;
                LED_G = 0;
            break;
        }
    }//end of LED control
}

//______________________________________________________________________//
//	Procedure	:	TaskFond												//
//______________________________________________________________________//
//	Role		:	Tache de communication								//
//	Entrées		:	Rien												//
//	Sorties		:	Rien												//
//______________________________________________________________________//
void __attribute((far))TaskFond(void)
{
	OsTimerLink(LedTimer1ms);
	//InitLedLight();
	//ToggleLed(GREEN);
//init lecture tension batterie
	InitADC();
   
	while(1)
	{
//led vie
		ServiceLed();			//faire clignoter la led de vie
//lecture tension batterie
		ServiceBat();			//calculate ADC value of car 12V battery
	}
}
