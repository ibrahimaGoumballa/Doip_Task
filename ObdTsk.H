/*______________________________________________________________________ 
|   Project      :Interface comunication OBD carte T520_88              |
|   Module       :Tache de communication obd							|
|   File         :ObdTsk.h                                         		|
|_______________________________________________________________________|
|   Description  :Tache d'interface pour la communication OBD			|
|   Date 21/04/2008 - 											        |
|   Auteur : David BARON										        |
|   Modify : Wolfgang porting to T52088, 2021/07/05				        |
|_______________________________________________________________________|*/
#ifndef _OBDTSK_H
#define _OBDTSK_H

#define NMAX_CAR_OBD	128	//nombre de caracteres en reception et transmission obd

#define T300ms	300			//300ms
#define	T5ms	5			//5ms
#define T25ms	25			//25ms
#define	T12ms	12			//12ms
#define	T15ms	15			//15ms
#define T35ms	35			//35ms
#define T70ms	70			//70ms
#define T135ms	135			//135ms

//etat automate gestion obd
enum {
		OBD_REPOS,
		//Wake up
		OBD_WAKE_UP_TIDLE,			//300ms etat haut
		OBD_WAKE_UP_TINTL,			//etat bas
		OBD_WAKE_UP_TINTH,			//etat haut
		ODB_END_WAKE_UP,			//fin sequence init
		//Trransmission OBD
		OBD_TX_OBD
	 };

/*______________________________________________________________________ 
|   Definition prototypes fonctions										|
|_______________________________________________________________________|*/
void WakeUpObd(void);			//demande wake up sequence
//iso9141
void SetConfObd(UCHAR ucAdr,WORD wDelaiCar, WORD wStartupTLow, WORD wStartupTHigh);	//configuration du delais inter caractere + tempo startup 
//iso9141
void SendDataObd(UCHAR *pBuff, UCHAR cNbCar);	//demande de transmission d'un buffer sur la ligne obdk
void ReceiveDataObd(void);		// place le module en reception OBD sans emission préalable

#endif  
