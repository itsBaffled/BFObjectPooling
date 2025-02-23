// Copyright (c) 2024 Jack Holland 
// Licensed under the MIT License. See LICENSE.md file in repo root for full license information.

#pragma once
#include "BFObjectPooling/Pool/BFObjectPool.h"
#include "BFObjectPooling/Pool/BFPooledObjectHandle.h"
#include "UObject/Object.h"
#include "BFPooledObjectHandleBP.generated.h"



/** A generic BP friendly object pool handle. No matter the pool type, this is your key to accessing your un-pooled object.
 * Purposely not exposing access directly to the object as it may be invalid/stale due to a copy of the same handle being returned or the object has already been stolen.
 * Use appropriate functions in BFObjectPoolingBlueprintFunctionLibrary to access the object such as `GetObjectFromHandle`. */
USTRUCT(BlueprintType, meta=(DisplayName="BF Poolable Object Handle"))
struct BFOBJECTPOOLING_API FBFPooledObjectHandleBP 
{
	GENERATED_BODY()
public:
	void Invalidate()
	{
		// Even after a handle has become invalid we allow the old ID's to be queried,
		// keeping behaviour similar with the C++ version.
		//PooledObjectID = -1;
		//ObjectCheckoutID = -1;
		Handle = nullptr;
	}

	void Reset()
	{
		PooledObjectID = -1;
		ObjectCheckoutID = -1;
		Handle = nullptr;
	}
	
public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int64 PooledObjectID = -1;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 ObjectCheckoutID = -1;

	TBFPooledObjectHandlePtr<UObject, ESPMode::NotThreadSafe> Handle = nullptr;
};
