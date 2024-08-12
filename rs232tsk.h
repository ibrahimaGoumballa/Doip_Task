/*______________________________________________________________________ 
|   Project      :Interface comunication OBD carte 520_84               |
|   Module       :Tache de communication rs232							|
|   File         :Rs232Tsk.h                                       		|
|_______________________________________________________________________|
|   Description  :Tache communication ligne rs232						|
|   Date 24/04/2008 - 											        |
|   Auteur : David BARON										        |
|_______________________________________________________________________|*/
#include "../Common_Header/ObdRs232Cmd.h"
#include "../Common_Header/Rs232Cmd.h"
#ifndef _RS232TSK_H
#define _RS232SK_H


#define MAX_UART_BUFF	128						



/*______________________________________________________________________ 
|   Definition prototypes fonctions										|
|_______________________________________________________________________|*/
void Rs232SendDataObd(UCHAR *pData, UCHAR NbCar);
void Rs232SendDataWInfoObd(UCHAR *pData, UCHAR NbCar,UCHAR DataType);

#endif  
