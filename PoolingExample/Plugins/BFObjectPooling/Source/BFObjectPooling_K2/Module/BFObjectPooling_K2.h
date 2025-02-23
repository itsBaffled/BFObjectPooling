// Copyright (c) 2024 Jack Holland 
// Licensed under the MIT License. See LICENSE.md file in repo root for full license information.



#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FBFObjectPooling_K2Module : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};
