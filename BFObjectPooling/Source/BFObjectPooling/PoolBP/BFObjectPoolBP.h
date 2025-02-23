// Copyright (c) 2024 Jack Holland 
// Licensed under the MIT License. See LICENSE.md file in repo root for full license information.

#pragma once
#include "BFObjectPooling/Pool/BFObjectPool.h"
#include "BFObjectPoolBP.generated.h"


/** Blueprint struct wrapper for an object pool.
* Must use `InitializeObjectPool` before trying to use object pool.
* See the BFObjectPoolingFunctionLibrary for API. */
USTRUCT(BlueprintType, meta = (DisplayName = "BF Object Pool"))
struct BFOBJECTPOOLING_API FBFObjectPoolBP
{
	GENERATED_BODY()
	
public:
	bool HasPoolBeenInitialized() const { return ObjectPool.IsValid() && ObjectPool->HasBeenInitialized(); }
	
public:
	UPROPERTY()
	FBFObjectPoolInitParams InitInfo;
	
	TBFObjectPoolPtr<UObject, ESPMode::NotThreadSafe> ObjectPool;
};
















