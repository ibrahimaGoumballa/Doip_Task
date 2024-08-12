/*______________________________________________________________________ 
|   Project      :OBD Module T52088                                     |
|   Module       :driver conversion of analog function  				|
|   File         :AdcDrv.c                                         		|
|_______________________________________________________________________|
|   Description  :Detect the 12V battery from car                       |
|   Date : 2021/06/10 											        |
|   Auteur :Wolfgang    										        |
|_______________________________________________________________________|*/
#ifndef __ADCDRV
#define __ADCDRV


/*______________________________________________________________________ 
|   Definition prototypes fonctions										|
|_______________________________________________________________________|*/
void InitADC(void);						//init convertisseur analogique numerique
UCHAR IsConnectedOnCar(void);			//connecte ou non sur le vehicule
void ServiceBat(void);					//lecture tension batterie + gestion sortie Val_Vccl (activation ligne Obd)
WORD GetAN2Value(void);
#endif
