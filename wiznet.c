#include "wiznet.h"
#include "DoIP/Socket.h"
#include "wizchip_conf.h"
#include "w6100.h"
#include "mcc_generated_files/spi1.h"
void W6100_Init() {

    reg_wizchip_cs_cbfunc(CS_Select, CS_Deselect);

    // Initialiser la puce réseau
    uint8_t memsize[8] = {2,2,2,2,2,2,2,2};
    uint8_t memsizet[8] = {2,2,2,2,2,2,2,2};
    wizchip_init(memsize,memsizet);
        // Initialiser les paramètres réseau
    wiz_NetInfo net_info = {
        .mac = {0x00, 0x08, 0xDC, 0x00, 0x00, 0x00},
        .ip = {192, 168, 1, 50},
        .sn = {255, 255, 255, 0},
        .gw = {192, 168, 1, 1},
        .dns = {8, 8, 8, 8}
    };
    wizchip_setnetinfo(&net_info);
}

void UDP_Connect() {
    uint8_t sn = 0; // Numéro de socket
    uint8_t protocol = Sn_MR_UDP;
    uint16_t port = 13400; // Port local
    uint8_t flag = 0;

    // Créer le socket
    
// Créer le socket
    SOCKET_INFO mSocketInfo;
    
    mSocketInfo.ucProtocol=protocol;
    mSocketInfo.wPort=port;
    mSocketInfo.ucOptions=flag;
    if(Socket_Open(sn,mSocketInfo) != sn) {
        // Gestion des erreurs de création de socket
        return;
    }

    // Envoyer un message UDP
    uint8_t buf[20] = "Hello Ibrahima";
    uint8_t dest_ip[4] = {0, 0, 0, 0}; // IP de destination
    uint16_t dest_port = 13400; // Port de destination
     if(Socket_SendTo(sn, buf, 5, dest_ip,dest_port)){
                 // Gestion des erreurs d'envoi
        Socket_Close(sn);
        return ;
     }
    return ;

}
