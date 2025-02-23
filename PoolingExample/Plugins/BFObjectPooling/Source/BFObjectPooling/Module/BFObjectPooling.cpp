// Copyright (c) 2024 Jack Holland 
// Licensed under the MIT License. See LICENSE.md file in repo root for full license information.

#include "BFObjectPooling.h"


namespace BF::OP
{
	TAutoConsoleVariable<bool> CVarObjectPoolPrintPoolOccupancy(TEXT("BF.OP.PrintPoolOccupancy"),
		false,
		TEXT("If enabled each pool that IS TICKING will print its occupancy stats."),
		ECVF_Cheat);

	TAutoConsoleVariable<bool> CVarObjectPoolEnableLogging(TEXT("BF.OP.EnableLogging"),
		false,
		TEXT("Good for catching some early warnings or knowing if you are hitting pool capacity."),
		ECVF_Cheat);
}

    
IMPLEMENT_MODULE(FBFObjectPoolingModule, BFObjectPooling)