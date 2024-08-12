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
#ifndef __CANTSK
#define __CANTSK

//liste des evenements decodes par la tache communication CAN
enum
{
	EVT_CAN_UNLOCK = MAX_OS_EVT,		//auto reveil
	EVT_INIT_CONF_CAN,					//init Vitesse, Id can Rx et Tx
	EVT_SEND_DATA_CAN,					//transmission donnees sur la ligne can 
};

/*______________________________________________________________________ 
|   Definition prototypes fonctions										|
|_______________________________________________________________________|*/
long SetBaudRateCan(long lVal, long lValData);
long SetBaudRateCanFd(UCHAR ucBaudRateIdx);
void SetEcuTargetId(long lVal);
void SetEcuSrcId(long lVal);
void SetNbBitIdCan(UCHAR cVal);
void SetFlowControlCan(UCHAR ucBS, UCHAR ucSTmin);				//parametrage trame flow control
void EcuSendDataOndiag(UCHAR *pData, WORD wSize, UCHAR cEvt);
void EcuSendWordOndiag(WORD *pwData, WORD wSize, UCHAR cEvt);
void EcuRxDataOndiag(void);					//forcer la ligne can en reception
ULONG GetLastIdCanRx(void);

//CA1_42_7 Ajout can extended format
void SetCanExtended_N_TA(UCHAR ucVal);				//parametrage extended format
//CA1_42_7




#endif
