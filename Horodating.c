//==================================================================
// File             : Horodating.c
// Description      : Gestion of horodating
// Created          : 2021/8/11
// Author           : MM2
//
// Copyright ATEQ 
// 15 Rue des Dames 
// 78340 Les Clayes sous bois (FRANCE) 
// Tel : +33 1 30 80 10 20
//
//==================================================================
#include "../Common_Header/NewType.h"
#include "../Common_Header/Kernel.h"

void UpdateTimeStamp(void);

static DWORD dwTimeStamp;

void InitTimeStamp(void)
{
    dwTimeStamp = 0;
    OsTimerLink(UpdateTimeStamp);
}

DWORD GetTimeStamp(void)
{
    return dwTimeStamp;
}

void UpdateTimeStamp(void)
{
    dwTimeStamp++;
}

DWORD TimeStampDiff(DWORD dwTimeStamp2Compare)
{
DWORD dwReturn;

    if(dwTimeStamp2Compare > dwTimeStamp)
    {//dwTimeStamp has overflow
        dwReturn = (0xFFFFFFFF - dwTimeStamp2Compare)+ dwTimeStamp;
    }
    else
    {//dwTimeStamp hasn't overflow
        dwReturn = dwTimeStamp - dwTimeStamp2Compare;
    }

return dwReturn;
}

