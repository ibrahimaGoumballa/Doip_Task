//====================================================================
// File             : Interrupt.h
// Description      : Déclaration des Interruptions
// Created          : 17/08/06
// Author           : KP
//
// Copyright ATEQ 
// 15 Rue des Dames 
// 78340 Les Clayes sous bois (FRANCE) 
// Tel : +33 1 30 80 10 20
//
//====================================================================
// Date		Version 	History	 
// 06/12/07	1.10b BETA	KP - Mise en place des niveaux d'interruption	
//====================================================================
#ifndef _INTERRUPT_H
#define _INTERRUPT_H


//--------------- NON MODIFIABLE -------------

//#define IT_TIMER_1_LEVEL			//Voir Kernel.h
//--------------------------------------------

#define IT_TIMER_2_LEVEL	4		//interruption timer 2 (Uart obd)
#define IT_CAN_1_LEVEL		4		//interruption module can (Communication calculateur)
#define IT_INTO_LEVEL		3		//interruption reception obdin
#define IT_TIMER_4_LEVEL	2		//interruption detection silence de ligne sur rs232
#define IT_UART_RX_LEVEL	2		//interruption reception rs232	
#define IT_UART_TX_LEVEL	1		//interruption transmission rs232

//---------------- NON UTILISES --------------
#define IT_TIMER_3_LEVEL			
#define IT_TIMER_5_LEVEL	
#define IT_TIMER_6_LEVEL			
#define IT_TIMER_7_LEVEL
#define IT_TIMER_8_LEVEL	
#define IT_TIMER_9_LEVEL
#define	IT_BUS_PARALLELE
//--------------------------------------------

#define IT_FULL_SET_LEVEL_1		0x1111

#endif
