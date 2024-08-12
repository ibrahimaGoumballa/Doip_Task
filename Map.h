//=========================================================
// File             : Map.h
// Description      : GPIO pin alias definition
// Created          : 2021/04/21
// Author           : Wolfgang
//
// Copyright ATEQ 
// 15 Rue des Dames 
// 78340 Les Clayes sous bois (FRANCE) 
// Tel : +33 1 30 80 10 20
//
//=========================================================
// Date		Version 	History	 
// XX/XX/XX	1.XX	-	
//=========================================================

#ifndef __MAP_H
#define __MAP_H

//=========================================================
//======================== INCLUDE ========================
//=========================================================


//=========================================================
//======================= CONSTANTS =======================
//=========================================================

//============= PORTA =============
#define TRIS_PORTA  0b0000000000000100				// Out: N/A, Out: N/A, Out: N/A, Out: N/A, Out: N/A, Out: N/A, Out: N/A, Out: N/A, Out: N/A, Out: N/A, Out: N/A, Out: unused, Out: RST_WIFI_BT, In: Vobd_AN2, Out: unused, Out: MUX_HC
#define IDLE_PORTA  0b0000000000001000

#define OBD_MUX_HC      LATAbits.LATA0
#define nRESET_WIFI_BT  LATAbits.LATA3
#define nCs_ETH_RX      LATAbits.LATA4

//============= PORTB =============
#define TRIS_PORTB  0b0000000000001011				// Out: unused, Out: unused, Out: unused, Out: unused, Out: unused, Out: unused, Out: unused, Out: unused, Out: unused, Out: EN_LIN, Out: STB_CANLS, Out: U1TX, In: U1RX, Out: unused, In: OSCO, In: OSCI
#define IDLE_PORTB  0b0000000000110000

#define INT_WIZNET		PORTBbits.RB2
#define nSTB_CANLS		LATBbits.LATB5
#define	EN_LIN          LATBbits.LATB6
#define OBD_MUX_ETH_RX  LATBbits.LATB14
#define nCs_ETH_TX      LATBbits.LATB15

//============= PORTC =============
#define TRIS_PORTC  0b0100110100000000				// Out: SDO2, In: SDI2, Out: SCLK2, Out: MUX_LC, In: IT_CANLS, In: ERR_CANLS, Out: U2TX, In: U2RX, Out: LED_G, Out: MUX_HB, Out: unused, Out: EN_CANLS, Out: CS_CAN_HS, Out: CS_K_LINE, Out: unused, Out: CS_CAN_LS
#define IDLE_PORTC  0b0000001000011101


#define nCS_CAN_LS		LATCbits.LATC0
#define LINK			PORTCbits.RC1
#define nCS_K_LINE		LATCbits.LATC2
#define nCS_CAN_HS      LATCbits.LATC3
#define EN_CANLS        LATCbits.LATC4
#define OBD_MUX_HB      LATCbits.LATC6
#define LED_G           LATCbits.LATC7

#define	OBDK			LATCbits.LATC9  //U2TX sometimes become GPIO in K-LINE initial stage
#define nERR_CANLS      PORTCbits.RC10
#define nIT_CANLS       PORTCbits.RC11

#define OBD_MUX_LC      LATCbits.LATC12


//============= PORTD =============
#define TRIS_PORTD  0b0000000010001100				// Out: CS2_WIFI_BT, Out: unused, Out: EN_WIFI, Out: MUX_LB, Out: RST_K_LINE, Out: LED_R, Out: MUX_LA, Out: MUX_HA, In: INT_WIFI_BT, Out: unused, Out: unused, Out: CAN2_LS_TX, In: CAN2_LS_RX, In: CAN1_FD_RX, Out: CAN1_FD_TX, Out: unused
#define IDLE_PORTD  0b1000000000010010


#define DOIP_EN         LATDbits.LATD0
#define CS_WIZNET       LATDbits.LATD6
#define OBD_MUX_HA      LATDbits.LATD8
#define OBD_MUX_LA		LATDbits.LATD9
#define LED_R           LATDbits.LATD10
#define RST_K_LINE      LATDbits.LATD11
#define OBD_MUX_LB      LATDbits.LATD12
#define WIFI_BT_EN      LATDbits.LATD13
#define OBD_MUX_ETH_TX  LATDbits.LATD14


//============= OBD MACRO =============
#if defined (_TRUCK) || defined (_DoIP)

#define OBD_MUX_ON 1
#define OBD_MUX_OFF 0

#else

#define OBD_MUX_ON 0
#define OBD_MUX_OFF 1

#endif

#define OBDOff()				EN_LIN = 0; \
                                DOIP_EN = 0; \
                                nCs_ETH_RX = 1; \
                                nCs_ETH_TX = 1; \
								nCS_K_LINE = OBD_MUX_OFF; \
								nCS_CAN_HS = OBD_MUX_OFF; \
								nCS_CAN_LS = OBD_MUX_OFF

#define Kline_WakeupReset()		EN_LIN = 0; \
								Nop(); \
								EN_LIN = 1;

#define Kline_1()				OBDOff(); \
								OBD_MUX_HA = 0; \
								OBD_MUX_HB = 0; \
								OBD_MUX_HC = 0; \
								EN_LIN = 1; \
								nCS_K_LINE = OBD_MUX_ON

#define Kline_7()				OBDOff(); \
								OBD_MUX_HA = 0; \
								OBD_MUX_HB = 1; \
								OBD_MUX_HC = 0; \
								EN_LIN = 1; \
								nCS_K_LINE = OBD_MUX_ON

#define Kline_8()				OBDOff(); \
								OBD_MUX_HA = 1; \
								OBD_MUX_HB = 1; \
								OBD_MUX_HC = 0; \
								EN_LIN = 1; \
								nCS_K_LINE = OBD_MUX_ON

#define Kline_9()				OBDOff(); \
								OBD_MUX_HA = 0; \
								OBD_MUX_HB = 0; \
								OBD_MUX_HC = 1; \
								EN_LIN = 1; \
								nCS_K_LINE = OBD_MUX_ON

#define Kline_12()				OBDOff(); \
								OBD_MUX_HA = 0; \
								OBD_MUX_HB = 1; \
								OBD_MUX_HC = 1; \
								EN_LIN = 1; \
								nCS_K_LINE = OBD_MUX_ON

#define CAN_HS_6_14()			OBDOff(); \
								OBD_MUX_HA = 0; \
								OBD_MUX_HB = 1; \
								OBD_MUX_HC = 0; \
								OBD_MUX_LA = 1; \
								OBD_MUX_LB = 1; \
								OBD_MUX_LC = 1; \
								nCS_CAN_HS = OBD_MUX_ON

#define CAN_HS_3_8()			OBDOff(); \
								OBD_MUX_HA = 1; \
								OBD_MUX_HB = 0; \
								OBD_MUX_HC = 0; \
								OBD_MUX_LA = 0; \
								OBD_MUX_LB = 1; \
								OBD_MUX_LC = 0; \
								nCS_CAN_HS = OBD_MUX_ON

#define CAN_LS_1_SW()			OBDOff(); \
								OBD_MUX_HA = 0; \
								OBD_MUX_HB = 0; \
								OBD_MUX_HC = 0; \
								OBD_MUX_LA = 1; \
								OBD_MUX_LB = 1; \
								OBD_MUX_LC = 1; \
								nSTB_CANLS = 1; \
								nCS_CAN_LS = OBD_MUX_ON

#define CAN_HS_3_11()			OBDOff(); \
								OBD_MUX_HA = 1; \
								OBD_MUX_HB = 0; \
								OBD_MUX_HC = 0; \
								OBD_MUX_LA = 0; \
								OBD_MUX_LB = 0; \
								OBD_MUX_LC = 1; \
								nCS_CAN_HS = OBD_MUX_ON

#define CAN_HS_11_3()			OBDOff(); \
								OBD_MUX_HA = 1; \
								OBD_MUX_HB = 0; \
								OBD_MUX_HC = 1; \
								OBD_MUX_LA = 1; \
								OBD_MUX_LB = 0; \
								OBD_MUX_LC = 0; \
								nCS_CAN_HS = OBD_MUX_ON

#define CAN_LS_3_11()			OBDOff(); \
								OBD_MUX_HA = 1; \
								OBD_MUX_HB = 0; \
								OBD_MUX_HC = 0; \
								OBD_MUX_LA = 0; \
								OBD_MUX_LB = 0; \
								OBD_MUX_LC = 1; \
								nSTB_CANLS = 1; \
								nCS_CAN_LS = OBD_MUX_ON

#define CAN_HS_12_13()			OBDOff(); \
								OBD_MUX_HA = 0; \
								OBD_MUX_HB = 1; \
								OBD_MUX_HC = 1; \
								OBD_MUX_LA = 0; \
								OBD_MUX_LB = 1; \
								OBD_MUX_LC = 1; \
								nCS_CAN_HS = OBD_MUX_ON
								
#define CAN_LS_1_9()			OBDOff(); \
								OBD_MUX_HA = 0; \
								OBD_MUX_HB = 0; \
								OBD_MUX_HC = 0; \
								OBD_MUX_LA = 1; \
								OBD_MUX_LB = 1; \
								OBD_MUX_LC = 0; \
								nSTB_CANLS = 1; \
								nCS_CAN_LS = OBD_MUX_ON

#define CAN_HS_1_9()			OBDOff(); \
								OBD_MUX_HA = 0; \
								OBD_MUX_HB = 0; \
								OBD_MUX_HC = 0; \
								OBD_MUX_LA = 1; \
								OBD_MUX_LB = 1; \
								OBD_MUX_LC = 0; \
								nCS_CAN_HS = OBD_MUX_ON

#define ETHERNET_3_11()			OBDOff(); \
                                OBD_MUX_ETH_RX = 0; \
                                OBD_MUX_ETH_TX = 1; \
                                DOIP_EN = 1; \
                                nCs_ETH_RX = 1; \
                                nCs_ETH_TX = 1                              
                                
#define ETHERNET_1_9()			OBDOff(); \
                                OBD_MUX_ETH_RX = 0; \
                                OBD_MUX_ETH_TX = 0; \
                                DOIP_EN = 1; \
                                nCs_ETH_RX = 1; \
                                nCs_ETH_TX = 1                              
                                
                                 
enum LED_COLOR
{
	LED_OFF,
	GREEN,
	RED,
	YELLOW
};


//#define FREQUENCE_160_MHZ	160
#define FREQUENCE_140_MHZ	140



void ClrWdtCom( void);
//void SetLed( WORD wSate);
void SetLed(BOOL bTwinkle, WORD wState);
void ToggleLed( WORD wSate);

#ifdef _BOOTLOADER
#define	RunAppli()	__asm__(" goto 0x3000 ")
#endif
#define	RunBoot()	__asm__(" goto 0x1000 ")

#endif
