//==================================================================
// File             : Horodating.h
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

#ifndef HORODATING
#define HORODATING

void InitTimeStamp(void);
DWORD GetTimeStamp(void);
void UpdateTimeStamp(void);
DWORD TimeStampDiff(DWORD dwTimeStamp2Compare);

#endif
