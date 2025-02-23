// Copyright (c) 2024 Jack Holland 
// Licensed under the MIT License. See LICENSE.md file in repo root for full license information.

#pragma once
#include "Modules/ModuleManager.h"



namespace BF::OP
{
    BFOBJECTPOOLING_API extern TAutoConsoleVariable<bool> CVarObjectPoolPrintPoolOccupancy;    
    BFOBJECTPOOLING_API extern TAutoConsoleVariable<bool> CVarObjectPoolEnableLogging;    
}


class FBFObjectPoolingModule : public IModuleInterface
{
    
};
