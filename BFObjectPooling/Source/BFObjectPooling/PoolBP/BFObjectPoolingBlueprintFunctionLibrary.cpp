// Copyright (c) 2024 Jack Holland 
// Licensed under the MIT License. See LICENSE.md file in repo root for full license information.

#include "BFObjectPoolingBlueprintFunctionLibrary.h"
#include "BFObjectPooling/Pool/Private/BFObjectPoolHelpers.h"
#include "BFPooledObjectHandleBP.h"
#include "BFObjectPooling/Pool/BFObjectPool.h"
#include "BFObjectPooling/PoolBP/BFObjectPoolBP.h"

#include "BFObjectPooling/GameplayActors/BFPoolable3DWidgetActor.h"
#include "BFObjectPooling/GameplayActors/BFPoolableDecalActor.h"
#include "BFObjectPooling/GameplayActors/BFPoolableNiagaraActor.h"
#include "BFObjectPooling/GameplayActors/BFPoolableSoundActor.h"
#include "BFObjectPooling/GameplayActors/BFPoolableProjectileActor.h"
#include "BFObjectPooling/GameplayActors/BFPoolableSkeletalMeshActor.h"
#include "BFObjectPooling/GameplayActors/BFPoolableStaticMeshActor.h"



void UBFObjectPoolingBlueprintFunctionLibrary::InitializeObjectPool(FBFObjectPoolBP& Pool, const FBFObjectPoolInitParams& PoolInfo)
{
	// Ensure we aren't already valid or if we are then we must have 0 members, you need to clear your pool before re purposing it.
	bfEnsure(!Pool.ObjectPool.IsValid() || Pool.ObjectPool->GetPoolSize() == 0);
	if(Pool.ObjectPool.IsValid() && Pool.ObjectPool->GetPoolSize() > 0)
		return;
	
	Pool.InitInfo = PoolInfo;
	Pool.ObjectPool = TBFObjectPool<UObject, ESPMode::NotThreadSafe>::CreatePool();
	Pool.ObjectPool->InitPool(Pool.InitInfo);
}

void UBFObjectPoolingBlueprintFunctionLibrary::UnpoolObject(FBFObjectPoolBP& Pool, FBFPooledObjectHandleBP& ObjectHandle, EBFSuccess& ReturnValue, UObject*& ReturnObject, bool bAutoActivate)
{
	ReturnValue = BF::OP::ToBPSuccessEnum(false);
	ReturnObject = nullptr;
	FBFPooledObjectHandleBP Handle;
	Handle.Handle = Pool.ObjectPool->UnpoolObject(bAutoActivate);
	if(Handle.Handle.IsValid() && Handle.Handle->IsHandleValid()) // Might be invalid due to pool being full.
	{
		Handle.PooledObjectID = Handle.Handle->GetPoolID();
		Handle.ObjectCheckoutID = Handle.Handle->GetCheckoutID();
		ReturnValue = BF::OP::ToBPSuccessEnum(true);
		ReturnObject = Handle.Handle->GetObject();
	}
	ObjectHandle = Handle;
}


void UBFObjectPoolingBlueprintFunctionLibrary::UnpoolObjectByTag(FBFObjectPoolBP& Pool, FGameplayTag Tag,
	 FBFPooledObjectHandleBP& ObjectHandle, EBFSuccess& ReturnValue, UObject*& ReturnObject,  bool bAutoActivate)
{
	FBFPooledObjectHandleBP BPHandle;

	if(Pool.ObjectPool.IsValid())
	{		
		auto Handle = Pool.ObjectPool->UnpoolObjectByTag(Tag, bAutoActivate);
		if(Handle.IsValid() && Handle->IsHandleValid())
		{
			BPHandle.Handle = Handle;
			BPHandle.PooledObjectID = Handle->GetPoolID();
			BPHandle.ObjectCheckoutID = Handle->GetCheckoutID();
			
			ReturnValue = BF::OP::ToBPSuccessEnum(true);
			ReturnObject = Handle->GetObject();
			return;
		}
	}

	ObjectHandle = BPHandle;
	ReturnValue = BF::OP::ToBPSuccessEnum(false);
	ReturnObject = nullptr;
}


bool UBFObjectPoolingBlueprintFunctionLibrary::ReturnPooledObject(FBFPooledObjectHandleBP& Handle)
{
	if(Handle.Handle.IsValid() && Handle.Handle->IsHandleValid())
	{
		Handle.Handle->ReturnToPool();
		Handle.Reset();
		return true;
	}
	Handle.Reset();
	return false;
}

void UBFObjectPoolingBlueprintFunctionLibrary::GetObjectFromHandle(FBFPooledObjectHandleBP& Handle, UObject*& PooledObject, EBFSuccess& ReturnValue)
{
	if(Handle.Handle.IsValid() && Handle.Handle->IsHandleValid())
	{
		PooledObject = Handle.Handle->GetObject();
		ReturnValue = BF::OP::ToBPSuccessEnum(true);
		return;
	}

	PooledObject = nullptr;
	ReturnValue = BF::OP::ToBPSuccessEnum(false);
}



void UBFObjectPoolingBlueprintFunctionLibrary::StealPooledObjectFromHandle(FBFPooledObjectHandleBP& Handle, UObject*& PooledObject, EBFSuccess& ReturnValue)
{
	if(Handle.Handle.IsValid() && Handle.Handle->IsHandleValid())
	{
		if(UObject* Obj = Handle.Handle->StealObject())
		{
			Handle.Reset();
			PooledObject =  Obj;
			ReturnValue = BF::OP::ToBPSuccessEnum(true);
			return;
		}
	}

	PooledObject = nullptr;
	ReturnValue = BF::OP::ToBPSuccessEnum(false);
}

void UBFObjectPoolingBlueprintFunctionLibrary::GetPooledObjectID(FBFPooledObjectHandleBP& Handle, int64& ObjectID, int32& CheckoutID)
{
	if(Handle.Handle.IsValid() && Handle.Handle->IsHandleValid())
	{
		ObjectID = Handle.Handle->GetPoolID();
		CheckoutID = Handle.Handle->GetCheckoutID();
		return;
	}
	ObjectID = -1;
	CheckoutID = -1;
}


void UBFObjectPoolingBlueprintFunctionLibrary::SetMaxObjectInactiveOccupancySeconds(FBFObjectPoolBP& Pool,
	float MaxObjectInactiveOccupancySeconds)
{
	if(Pool.ObjectPool.IsValid())
		Pool.ObjectPool->SetMaxObjectInactiveOccupancySeconds(MaxObjectInactiveOccupancySeconds);
}

float UBFObjectPoolingBlueprintFunctionLibrary::GetMaxObjectInactiveOccupancySeconds(FBFObjectPoolBP& Pool)
{
	if(Pool.ObjectPool.IsValid())
		return Pool.ObjectPool->GetMaxObjectInactiveOccupancySeconds();
	return -1.f;
}

bool UBFObjectPoolingBlueprintFunctionLibrary::IsObjectPoolTickEnabled(FBFObjectPoolBP& Pool)
{
	if(Pool.ObjectPool.IsValid())
		return Pool.ObjectPool->GetTickEnabled();
	return false;
}

bool UBFObjectPoolingBlueprintFunctionLibrary::IsObjectPoolOfType(FBFObjectPoolBP& Pool, UClass* ClassType)
{
	if(Pool.ObjectPool.IsValid() && Pool.ObjectPool->GetPoolType() != EBFPoolType::Invalid)
		return ClassType == Pool.ObjectPool->GetPoolInitInfo().PoolClass;
	return false;
}

void UBFObjectPoolingBlueprintFunctionLibrary::SetObjectPoolTickEnabled(FBFObjectPoolBP& Pool, bool bEnabled)
{
	if(Pool.ObjectPool.IsValid())
		Pool.ObjectPool->SetTickEnabled(bEnabled);
}

void UBFObjectPoolingBlueprintFunctionLibrary::SetObjectPoolTickInterval(FBFObjectPoolBP& Pool, float TickRate)
{
	if(Pool.ObjectPool.IsValid())
		Pool.ObjectPool->SetTickInterval(TickRate);
}

int32 UBFObjectPoolingBlueprintFunctionLibrary::GetObjectPoolSize(FBFObjectPoolBP& Pool)
{
	if(Pool.ObjectPool.IsValid())
		return Pool.ObjectPool->GetPoolSize();
	return -1;
}

int32 UBFObjectPoolingBlueprintFunctionLibrary::GetObjectPoolLimit(FBFObjectPoolBP& Pool)
{
	if(Pool.ObjectPool.IsValid())
		return Pool.ObjectPool->GetPoolLimit();
	return -1;
}

bool UBFObjectPoolingBlueprintFunctionLibrary::SetObjectPoolLimit(FBFObjectPoolBP& Pool, int32 NewLimit)
{
	if(Pool.ObjectPool.IsValid())
		return Pool.ObjectPool->SetPoolLimit(NewLimit);
	return false;
}

int32 UBFObjectPoolingBlueprintFunctionLibrary::GetObjectPoolActiveObjectsSize(FBFObjectPoolBP& Pool)
{
	if(Pool.ObjectPool.IsValid())
		return Pool.ObjectPool->GetActivePoolSize();
	return -1;
}

int32 UBFObjectPoolingBlueprintFunctionLibrary::GetObjectPoolInactiveObjectsSize(FBFObjectPoolBP& Pool)
{
	if(Pool.ObjectPool.IsValid())
		return Pool.ObjectPool->GetInactivePoolSize();
	return -1;
}


bool UBFObjectPoolingBlueprintFunctionLibrary::ClearObjectPoolInactiveObjects(FBFObjectPoolBP& Pool)
{
	if(Pool.ObjectPool.IsValid())
		return Pool.ObjectPool->ClearInactiveObjectsPool();
	return false;
}


void UBFObjectPoolingBlueprintFunctionLibrary::QuickUnpoolStaticMeshActor(FBFObjectPoolBP& Pool, const FBFPoolableStaticMeshActorDescription& InitParams, const FTransform& ActorTransform, EBFSuccess& ReturnValue,  UObject*& ReturnObject)
{
	// You cannot call this function with a pool that is not for this specific actor.
	bfEnsure(Pool.InitInfo.PoolClass->IsChildOf<ABFPoolableStaticMeshActor>());

	ReturnObject = nullptr;
	ReturnValue = BF::OP::ToBPSuccessEnum(false);
	if(!Pool.ObjectPool.IsValid() || !Pool.InitInfo.PoolClass->IsChildOf<ABFPoolableStaticMeshActor>())
		return;

	FBFPooledObjectHandleBP BPHandle;
	UnpoolObject(Pool,BPHandle, ReturnValue, ReturnObject, false);
	if(BPHandle.Handle.IsValid() && BPHandle.Handle->IsHandleValid())
	{
		auto* Actor = CastChecked<ABFPoolableStaticMeshActor>(BPHandle.Handle->GetObject());
		Actor->FireAndForgetBP(BPHandle, InitParams, ActorTransform);
	}
}


void UBFObjectPoolingBlueprintFunctionLibrary::QuickUnpoolSkeletalMeshActor(FBFObjectPoolBP& Pool,
	const FBFPoolableSkeletalMeshActorDescription& InitParams, const FTransform& ActorTransform, EBFSuccess& ReturnValue, UObject*& ReturnObject)
{
	// You cannot call this function with a pool that is not for this specific actor.
	bfEnsure(Pool.InitInfo.PoolClass->IsChildOf<ABFPoolableSkeletalMeshActor>());
	
	ReturnObject = nullptr;
	ReturnValue = BF::OP::ToBPSuccessEnum(false);
	if(!Pool.ObjectPool.IsValid() || !Pool.InitInfo.PoolClass->IsChildOf<ABFPoolableSkeletalMeshActor>())
		return;


	FBFPooledObjectHandleBP BPHandle;
	UnpoolObject(Pool,BPHandle, ReturnValue, ReturnObject, false);
	if(BPHandle.Handle.IsValid() && BPHandle.Handle->IsHandleValid())
	{
		auto* Actor = CastChecked<ABFPoolableSkeletalMeshActor>(BPHandle.Handle->GetObject());
		Actor->FireAndForgetBP(BPHandle, InitParams, ActorTransform);
	}
}


void UBFObjectPoolingBlueprintFunctionLibrary::QuickUnpoolProjectileActor(FBFObjectPoolBP& Pool,
	const FBFPoolableProjectileActorDescription& InitParams, const FTransform& ActorTransform, EBFSuccess& ReturnValue, UObject*& ReturnObject)
{
	// You cannot call this function with a pool that is not for this specific actor.
	bfEnsure(Pool.InitInfo.PoolClass->IsChildOf<ABFPoolableProjectileActor>());

	ReturnObject = nullptr;
	ReturnValue = BF::OP::ToBPSuccessEnum(false);
	if(!Pool.ObjectPool.IsValid() || !Pool.InitInfo.PoolClass->IsChildOf<ABFPoolableProjectileActor>())
		return;

	FBFPooledObjectHandleBP BPHandle;
	UnpoolObject(Pool,BPHandle, ReturnValue, ReturnObject, false);
	if(BPHandle.Handle.IsValid() && BPHandle.Handle->IsHandleValid())
	{
		auto* Actor = CastChecked<ABFPoolableProjectileActor>(BPHandle.Handle->GetObject());
		Actor->FireAndForgetBP(BPHandle, InitParams, ActorTransform);
	}
}

void UBFObjectPoolingBlueprintFunctionLibrary::QuickUnpoolNiagaraActor(FBFObjectPoolBP& Pool,
	const FBFPoolableNiagaraActorDescription& InitParams, const FTransform& ActorTransform, EBFSuccess& ReturnValue,
	UObject*& ReturnObject)
{
	// You cannot call this function with a pool that is not for this specific actor.
	bfEnsure(Pool.InitInfo.PoolClass->IsChildOf<ABFPoolableNiagaraActor>());

	ReturnObject = nullptr;
	ReturnValue = BF::OP::ToBPSuccessEnum(false);
	if(!Pool.ObjectPool.IsValid() || !Pool.InitInfo.PoolClass->IsChildOf<ABFPoolableNiagaraActor>())
		return;

	FBFPooledObjectHandleBP BPHandle;
	UnpoolObject(Pool,BPHandle, ReturnValue, ReturnObject, false);
	if(BPHandle.Handle.IsValid() && BPHandle.Handle->IsHandleValid())
	{
		auto* Actor = CastChecked<ABFPoolableNiagaraActor>(BPHandle.Handle->GetObject());
		Actor->FireAndForgetBP(BPHandle, InitParams, ActorTransform);
	}
}

void UBFObjectPoolingBlueprintFunctionLibrary::QuickUnpoolSoundActor(FBFObjectPoolBP& Pool,
	const FBFPoolableSoundActorDescription& InitParams, const FTransform& ActorTransform, EBFSuccess& ReturnValue,
	UObject*& ReturnObject)
{
	// You cannot call this function with a pool that is not for this specific actor.
	bfEnsure(Pool.InitInfo.PoolClass->IsChildOf<ABFPoolableSoundActor>());
	
	
	ReturnObject = nullptr;
	ReturnValue = BF::OP::ToBPSuccessEnum(false);
	if(!Pool.ObjectPool.IsValid() || !Pool.InitInfo.PoolClass->IsChildOf<ABFPoolableSoundActor>())
		return;

	FBFPooledObjectHandleBP BPHandle;
	UnpoolObject(Pool,BPHandle, ReturnValue, ReturnObject, false);
	if(BPHandle.Handle.IsValid() && BPHandle.Handle->IsHandleValid())
	{
		auto* Actor = CastChecked<ABFPoolableSoundActor>(BPHandle.Handle->GetObject());
		Actor->FireAndForgetBP(BPHandle, InitParams, ActorTransform);
	}
}

void UBFObjectPoolingBlueprintFunctionLibrary::QuickUnpoolDecalActor(FBFObjectPoolBP& Pool,
	const FBFPoolableDecalActorDescription& InitParams, const FTransform& ActorTransform, EBFSuccess& ReturnValue,
	UObject*& ReturnObject)
{
	// You cannot call this function with a pool that is not for this specific actor.
	bfEnsure(Pool.InitInfo.PoolClass->IsChildOf<ABFPoolableDecalActor>());

	ReturnObject = nullptr;
	ReturnValue = BF::OP::ToBPSuccessEnum(false);
	if(!Pool.ObjectPool.IsValid() || !Pool.InitInfo.PoolClass->IsChildOf<ABFPoolableDecalActor>())
		return;

	FBFPooledObjectHandleBP BPHandle;
	UnpoolObject(Pool,BPHandle, ReturnValue, ReturnObject, false);
	if(BPHandle.Handle.IsValid() && BPHandle.Handle->IsHandleValid())
	{
		auto* Actor = CastChecked<ABFPoolableDecalActor>(BPHandle.Handle->GetObject());
		Actor->FireAndForgetBP(BPHandle, InitParams, ActorTransform);
	}
}

void UBFObjectPoolingBlueprintFunctionLibrary::QuickUnpool3DWidgetActor(FBFObjectPoolBP& Pool,
	const FBFPoolable3DWidgetActorDescription& InitParams, const FTransform& ActorTransform, EBFSuccess& ReturnValue,
	 UObject*& ReturnObject)
{
	// You cannot call this function with a pool that is not for this specific actor.
	bfEnsure(Pool.InitInfo.PoolClass->IsChildOf<ABFPoolable3DWidgetActor>());
	
	ReturnObject = nullptr;
	ReturnValue = BF::OP::ToBPSuccessEnum(false);
	if(!Pool.ObjectPool.IsValid() || !Pool.InitInfo.PoolClass->IsChildOf<ABFPoolable3DWidgetActor>())
		return;

	FBFPooledObjectHandleBP BPHandle;
	UnpoolObject(Pool,BPHandle, ReturnValue, ReturnObject, false);
	if(BPHandle.Handle.IsValid() && BPHandle.Handle->IsHandleValid())
	{
		auto* Actor = CastChecked<ABFPoolable3DWidgetActor>(BPHandle.Handle->GetObject());
		Actor->FireAndForgetBP(BPHandle, InitParams, ActorTransform);
	}
}


void UBFObjectPoolingBlueprintFunctionLibrary::IsPooledObjectHandleValid(FBFPooledObjectHandleBP& Handle, EBFSuccess& ReturnValue, bool& bSuccess)
{
	bSuccess = Handle.Handle.IsValid() && Handle.Handle->IsHandleValid();
	ReturnValue = BF::OP::ToBPSuccessEnum(bSuccess);
}

void UBFObjectPoolingBlueprintFunctionLibrary::IsPoolValid(FBFObjectPoolBP& Pool, EBFSuccess& ReturnValue,
	bool& bIsValid)
{
	if(Pool.ObjectPool.IsValid() && Pool.ObjectPool->GetPoolType() != EBFPoolType::Invalid)
	{
		bIsValid = true;
		ReturnValue = BF::OP::ToBPSuccessEnum(true);
		return;
	}
	bIsValid = false;
	ReturnValue = BF::OP::ToBPSuccessEnum(false);
}
