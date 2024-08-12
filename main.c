#include "mcc_generated_files/spi1.h"
#include "Kernel.h"
#include <string.h>
#include "Map.h"
#include "../Common_Header/ObdRs232Cmd.h"
#include "../Common_Header/Rs232Cmd.h"
#include "../Common_OBD_Data/ObdSavedData.h"
#include "../Common_OBD_Data/Flash.h"
#include "../Common_OBD_Data/ObdFirmwareVersion.h"
#if defined _DoIP
#include "DoIP/Ethernet.h"
#include "DoIP/TaskWiznet.h"
#include "DoIP/DOIP_Struct.h"
#include "DoIP/DOIP_Server.h"
#endif

void __attribute((far))TaskFond(void);
void __attribute((far))ObdTask(void);
void __attribute((far))Rs232Task(void);
void __attribute((far))CanTask(void);

int main(void) {
    
    
    SYSTEM_Initialize();
    WIFI_BT_EN = 0; //
    nRESET_WIFI_BT = 0;
    
    CCP2CON1Lbits.CCPON = false; //for debug, should remove

    LED_R = 1;
    LED_G = 1;
    EN_CANLS = 1;   //initial value, always enable CAN2(Low Speed) IC
    nSTB_CANLS = 0;


    
    ReadFirmwareData();
    // Initialisation du microcontrôleur
   // SPI_Init();
    SPI1_Initialize();
    W6100_Init();
    
    // Créer une connexion UDP et envoyer un message
    UDP_Connect();
    SetLed(TRUE, GREEN);
    
#if defined _DoIP
    if(strcmp((const char *)rDeviceData.sFirmware.VersionSoft,ObdVtDoIPVersion) != 0)
    {
        strcpy(rDeviceData.sFirmware.VersionSoft,ObdVtDoIPVersion);    
#elif defined _TRUCK
    if(strcmp((const char *)rDeviceData.sFirmware.VersionSoft,ObdVtTruckVersion) != 0)
    {
        strcpy(rDeviceData.sFirmware.VersionSoft,ObdVtTruckVersion);   
#else
    if(strcmp((const char *)rDeviceData.sFirmware.VersionSoft,ObdVt57Version) != 0)
    {
        strcpy(rDeviceData.sFirmware.VersionSoft,ObdVt57Version);
#endif
    SaveFirmwareData();
    }
 #ifdef _DoIP
    UDP_Connect();
#endif
#ifdef _DoIP    
OsCreateTask(WiznetTask, 512);
#endif
OsCreateTask(Rs232Task, 512);
#ifdef _DoIP    
OsCreateTask(DoiPTask, 512);
#endif  


    OsCreateTask(ObdTask,512);
    OsCreateTask(CanTask,512);
    OsCreateTask(TaskFond, 512);
    OsStart(FREQUENCE_140_MHZ, 1);

    while (1) {
            SPI1_Initialize();
            W6100_Init();
            UDP_Connect();
            SetLed(TRUE,YELLOW);
        // Boucle principale
    }
    return 0;
}
