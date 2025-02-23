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
	if(Pool.HasPoolBeenInitialized() && Pool.ObjectPool->GetPoolNum() > 0)
	{
#if !UE_BUILD_SHIPPING
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Failed to initialize object pool, pool is already valid and has pooled objects."));
#endif
		return;
	}
	
	Pool.InitInfo = PoolInfo;
	Pool.ObjectPool = TBFObjectPool<UObject, ESPMode::NotThreadSafe>::CreateAndInitPool(PoolInfo);
}


void UBFObjectPoolingBlueprintFunctionLibrary::UnpoolObject(FBFObjectPoolBP& Pool,
															FBFPooledObjectHandleBP& OutHandle,
															EBFPooledObjectReclaimPolicy Policy,
															EBFSuccess& Result,
															UObject*& ReturnObject,
															bool bAutoActivate)
{
	Result = BF::OP::ToBPSuccessEnum(false);
	ReturnObject = nullptr;
	OutHandle.Invalidate();
	
	if (!Pool.HasPoolBeenInitialized())
	{
#if !UE_BUILD_SHIPPING
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Failed to unpool object, pool is not valid or has not been initialized."));
#endif
		return;
	}

	
	if(auto Handle = Pool.ObjectPool->UnpoolObject(bAutoActivate, Policy)) // Might be invalid due to pool being full.
	{
		OutHandle.Handle = Handle;
		OutHandle.PooledObjectID = Handle->GetPoolID();
		OutHandle.ObjectCheckoutID = Handle->GetCheckoutID();
		
		Result = BF::OP::ToBPSuccessEnum(true);
		ReturnObject = OutHandle.Handle->GetObject();
	}
}



void UBFObjectPoolingBlueprintFunctionLibrary::UnpoolObjectByTag(	FBFObjectPoolBP& Pool,
																	FGameplayTag Tag,
	 																FBFPooledObjectHandleBP& ObjectHandle,
	 																EBFPooledObjectReclaimPolicy Policy,
	 																EBFSuccess& Result,
	 																UObject*& ReturnObject,
	 																bool bAutoActivate)
{
	Result = BF::OP::ToBPSuccessEnum(false);
	ReturnObject = nullptr;
	ObjectHandle.Invalidate();
	
	if (!Pool.HasPoolBeenInitialized())
	{
#if !UE_BUILD_SHIPPING
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Failed to unpool object by tag, pool is not valid or has not been initialized."));
#endif
		return;
	}
	
	if(auto Handle = Pool.ObjectPool->UnpoolObjectByTag(Tag, bAutoActivate, Policy))
	{
		ObjectHandle.Handle = Handle;
		ObjectHandle.PooledObjectID = Handle->GetPoolID();
		ObjectHandle.ObjectCheckoutID = Handle->GetCheckoutID();
		
		Result = BF::OP::ToBPSuccessEnum(true);
		ReturnObject = Handle->GetObject();
	}

}

void UBFObjectPoolingBlueprintFunctionLibrary::UnpoolObjectByPredicate(	FBFObjectPoolBP& Pool,
																		FBFPooledObjectHandleBP& OutHandle,
																		FUnpoolPredicate Predicate,
																		EBFPooledObjectReclaimPolicy Policy,
																		EBFSuccess& Result,
																		UObject*& ReturnObject,
																		bool bAutoActivate)
{
	Result = BF::OP::ToBPSuccessEnum(false);
	ReturnObject = nullptr;
	OutHandle.Invalidate();
	
	if (!Pool.HasPoolBeenInitialized() || !Predicate.IsBound())
	{
#if !UE_BUILD_SHIPPING
		if (!Pool.HasPoolBeenInitialized())
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Failed to unpool object by predicate, pool is not valid or has not been initialized."));
		else
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Failed to unpool object by predicate, predicate is not bound."));
#endif
		return;
	}
	
	if(auto Handle = Pool.ObjectPool->UnpoolObjectByPredicate([P = Predicate](const UObject* O)-> bool { return P.Execute(O); }, bAutoActivate, Policy))
	{
		OutHandle.Handle = Handle;
		OutHandle.PooledObjectID = Handle->GetPoolID();
		OutHandle.ObjectCheckoutID = Handle->GetCheckoutID();
		
		Result = BF::OP::ToBPSuccessEnum(true);
		ReturnObject = Handle->GetObject();
	}	
}



bool UBFObjectPoolingBlueprintFunctionLibrary::ReturnPooledObject(FBFPooledObjectHandleBP& Handle)
{
	bool bResult = false;
	if(Handle.Handle.IsValid() && Handle.Handle->IsHandleValid())
	{
		bResult = Handle.Handle->ReturnToPool();
	}
	Handle.Invalidate();
	return bResult;
}


void UBFObjectPoolingBlueprintFunctionLibrary::InvalidateHandle(FBFPooledObjectHandleBP& Handle)
{
	Handle.Invalidate();
}


void UBFObjectPoolingBlueprintFunctionLibrary::GetObjectFromHandle(	FBFPooledObjectHandleBP& Handle,
																	UObject*& PooledObject,
																	EBFSuccess& ReturnValue)
{
	PooledObject = nullptr;
	ReturnValue = BF::OP::ToBPSuccessEnum(false);
	
	if(Handle.Handle.IsValid() && Handle.Handle->IsHandleValid())
	{
		PooledObject = Handle.Handle->GetObject();
		ReturnValue = BF::OP::ToBPSuccessEnum(true);
	}
}


bool UBFObjectPoolingBlueprintFunctionLibrary::AdoptObject(FBFObjectPoolBP& Pool, UObject* Object)
{
	if (IsValid(Object) && Pool.HasPoolBeenInitialized())
	{
		return Pool.ObjectPool->AdoptObject(Object);
	}
	return false;
}


void UBFObjectPoolingBlueprintFunctionLibrary::StealPooledObjectFromHandle(FBFPooledObjectHandleBP& Handle,
																			UObject*& PooledObject,
																			EBFSuccess& Result)
{
	PooledObject = nullptr;
	Result = BF::OP::ToBPSuccessEnum(false);
	
	if(Handle.Handle.IsValid() && Handle.Handle->IsHandleValid())
	{
		if(UObject* Obj = Handle.Handle->StealObject())
		{
			Handle.Invalidate();
			PooledObject = Obj;
			Result = BF::OP::ToBPSuccessEnum(true);
		}
	}
}


void UBFObjectPoolingBlueprintFunctionLibrary::GetUnpooledObjectID(	FBFPooledObjectHandleBP& Handle,
																	int64& ObjectID,
																	int32& CheckoutID)
{
	// Regardless of if the handle is valid, we return its ID in-case the user had some
	// system that makes use of old handle IDs and responds in the delegates on the pool itself.
	ObjectID =		Handle.PooledObjectID;
	CheckoutID =	Handle.ObjectCheckoutID;
}


void UBFObjectPoolingBlueprintFunctionLibrary::SetMaxObjectInactiveOccupancySeconds(FBFObjectPoolBP& Pool,
	float MaxObjectInactiveOccupancySeconds)
{
	if(Pool.HasPoolBeenInitialized())
	{
		Pool.ObjectPool->SetMaxObjectInactiveOccupancySeconds(MaxObjectInactiveOccupancySeconds);
	}
}

float UBFObjectPoolingBlueprintFunctionLibrary::GetMaxObjectInactiveOccupancySeconds(FBFObjectPoolBP& Pool)
{
	if(Pool.HasPoolBeenInitialized())
	{
		return Pool.ObjectPool->GetMaxObjectInactiveOccupancySeconds();
	}
	return -1.f;
}

bool UBFObjectPoolingBlueprintFunctionLibrary::IsObjectPoolTickEnabled(FBFObjectPoolBP& Pool)
{
	if(Pool.HasPoolBeenInitialized())
	{
		return Pool.ObjectPool->GetTickEnabled();
	}
	return false;
}

bool UBFObjectPoolingBlueprintFunctionLibrary::IsObjectPoolOfType(FBFObjectPoolBP& Pool, UClass* ClassType)
{
	if(Pool.HasPoolBeenInitialized())
	{
		return ClassType == Pool.ObjectPool->GetPoolInitInfo().PoolClass;
	}
	return false;
}

void UBFObjectPoolingBlueprintFunctionLibrary::SetObjectPoolTickEnabled(FBFObjectPoolBP& Pool, bool bEnabled)
{
	if(Pool.HasPoolBeenInitialized())
	{
		Pool.ObjectPool->SetTickEnabled(bEnabled);
	}
}

void UBFObjectPoolingBlueprintFunctionLibrary::SetObjectPoolTickInterval(FBFObjectPoolBP& Pool, float TickRate)
{
	if(Pool.HasPoolBeenInitialized())
	{
		Pool.ObjectPool->SetTickInterval(TickRate);
	}
}

int32 UBFObjectPoolingBlueprintFunctionLibrary::GetObjectPoolSize(FBFObjectPoolBP& Pool)
{
	if(Pool.HasPoolBeenInitialized())
	{
		return Pool.ObjectPool->GetPoolNum();
	}
	return -1;
}

int32 UBFObjectPoolingBlueprintFunctionLibrary::GetObjectPoolLimit(FBFObjectPoolBP& Pool)
{
	if(Pool.HasPoolBeenInitialized())
	{
		return Pool.ObjectPool->GetPoolLimit();
	}
	return -1;
}

bool UBFObjectPoolingBlueprintFunctionLibrary::SetObjectPoolLimit(FBFObjectPoolBP& Pool, int32 NewLimit)
{
	if(Pool.HasPoolBeenInitialized())
	{
		return Pool.ObjectPool->SetPoolLimit(NewLimit);
	}
	return false;
}

int32 UBFObjectPoolingBlueprintFunctionLibrary::GetObjectPoolActiveObjectsSize(FBFObjectPoolBP& Pool)
{
	if(Pool.HasPoolBeenInitialized())
	{
		return Pool.ObjectPool->GetActivePoolNum();
	}
	return -1;
}

int32 UBFObjectPoolingBlueprintFunctionLibrary::GetObjectPoolInactiveObjectsSize(FBFObjectPoolBP& Pool)
{
	if(Pool.HasPoolBeenInitialized())
	{
		return Pool.ObjectPool->GetInactivePoolNum();
	}
	return -1;
}


bool UBFObjectPoolingBlueprintFunctionLibrary::ClearObjectPoolInactiveObjects(FBFObjectPoolBP& Pool)
{
	if(Pool.HasPoolBeenInitialized())
	{
		return Pool.ObjectPool->ClearInactiveObjectsPool();
	}
	return false;
}


void UBFObjectPoolingBlueprintFunctionLibrary::IsPooledObjectHandleValid(FBFPooledObjectHandleBP& Handle, EBFSuccess& Result, bool& bSuccess)
{
	bSuccess = Handle.Handle.IsValid() && Handle.Handle->IsHandleValid();
	Result = BF::OP::ToBPSuccessEnum(bSuccess);
}


void UBFObjectPoolingBlueprintFunctionLibrary::IsPoolValid(FBFObjectPoolBP& Pool, EBFSuccess& Result,
	bool& bIsValid)
{
	bIsValid = false;
	Result = BF::OP::ToBPSuccessEnum(false);

	if(Pool.HasPoolBeenInitialized())
	{
		bIsValid = true;
		Result = BF::OP::ToBPSuccessEnum(true);
	}
}



void UBFObjectPoolingBlueprintFunctionLibrary::QuickUnpoolStaticMeshActor(FBFObjectPoolBP& Pool,
																		const FBFPoolableStaticMeshActorDescription& InitParams,
																		EBFPooledObjectReclaimPolicy Policy,
																		const FTransform& ActorTransform,
																		EBFSuccess& Result,
																		UObject*& ReturnObject)
{
	ReturnObject = nullptr;
	Result = BF::OP::ToBPSuccessEnum(false);
	
	if(!Pool.HasPoolBeenInitialized() || !Pool.InitInfo.PoolClass->IsChildOf<ABFPoolableStaticMeshActor>())
	{
#if !UE_BUILD_SHIPPING
		
		if (!Pool.InitInfo.PoolClass->IsChildOf<ABFPoolableStaticMeshActor>())
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Failed to unpool object from a pool that is not of a compatible type to use with QuickUnpoolStaticMeshActor."));
		if (!Pool.ObjectPool.IsValid())
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Failed to unpool object from a pool that is not valid."));
		if (!Pool.ObjectPool->HasBeenInitialized())
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Failed to unpool object from a pool that has not been initialized."));
#endif
		return;
	}

	FBFPooledObjectHandleBP BPHandle ;
	UnpoolObject(Pool, BPHandle, Policy, Result, ReturnObject, false);
	if(Result == EBFSuccess::Success)
	{
		auto* Actor = CastChecked<ABFPoolableStaticMeshActor>(BPHandle.Handle->GetObject());
		Actor->FireAndForgetBP(BPHandle, InitParams, ActorTransform);
	}
}


void UBFObjectPoolingBlueprintFunctionLibrary::QuickUnpoolSkeletalMeshActor(FBFObjectPoolBP& Pool,
																			const FBFPoolableSkeletalMeshActorDescription& InitParams,
																			EBFPooledObjectReclaimPolicy Policy,
																			const FTransform& ActorTransform,
																			EBFSuccess& Result,
																			UObject*& ReturnObject)
{
	ReturnObject = nullptr;
	Result = BF::OP::ToBPSuccessEnum(false);
	
	if(!Pool.HasPoolBeenInitialized() || !Pool.InitInfo.PoolClass->IsChildOf<ABFPoolableSkeletalMeshActor>())
	{
#if !UE_BUILD_SHIPPING
		
		if (!Pool.InitInfo.PoolClass->IsChildOf<ABFPoolableSkeletalMeshActor>())
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Failed to unpool object from a pool that is not of a compatible type to use with QuickUnpoolSkeletalMeshActor."));
		if (!Pool.ObjectPool.IsValid())
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Failed to unpool object from a pool that is not valid."));
		if (!Pool.ObjectPool->HasBeenInitialized())
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Failed to unpool object from a pool that has not been initialized."));
#endif
		Result = BF::OP::ToBPSuccessEnum(false);
		return;
	}


	FBFPooledObjectHandleBP BPHandle;
	UnpoolObject(Pool, BPHandle, Policy, Result, ReturnObject, false);
	if(Result == EBFSuccess::Success)
	{
		auto* Actor = CastChecked<ABFPoolableSkeletalMeshActor>(BPHandle.Handle->GetObject());
		Actor->FireAndForgetBP(BPHandle, InitParams, ActorTransform);
	}
}


void UBFObjectPoolingBlueprintFunctionLibrary::QuickUnpoolProjectileActor(FBFObjectPoolBP& Pool,
																		const FBFPoolableProjectileActorDescription& InitParams,
																		EBFPooledObjectReclaimPolicy Policy,
																		const FTransform& ActorTransform,
																		EBFSuccess& Result,
																		UObject*& ReturnObject)
{
	ReturnObject = nullptr;
	Result = BF::OP::ToBPSuccessEnum(false);
	
	if(!Pool.HasPoolBeenInitialized() || !Pool.InitInfo.PoolClass->IsChildOf<ABFPoolableProjectileActor>())
	{
#if !UE_BUILD_SHIPPING
		
		if (!Pool.InitInfo.PoolClass->IsChildOf<ABFPoolableProjectileActor>())
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Failed to unpool object from a pool that is not of a compatible type to use with QuickUnpoolProjectileActor."));
		if (!Pool.ObjectPool.IsValid())
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Failed to unpool object from a pool that is not valid."));
		if (!Pool.ObjectPool->HasBeenInitialized())
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Failed to unpool object from a pool that has not been initialized."));
#endif
		Result = BF::OP::ToBPSuccessEnum(false);
		return;
	}

	FBFPooledObjectHandleBP BPHandle;
	UnpoolObject(Pool, BPHandle, Policy, Result, ReturnObject, false);
	if(Result == EBFSuccess::Success)
	{
		auto* Actor = CastChecked<ABFPoolableProjectileActor>(BPHandle.Handle->GetObject());
		Actor->FireAndForgetBP(BPHandle, InitParams, ActorTransform);
	}
}

void UBFObjectPoolingBlueprintFunctionLibrary::QuickUnpoolNiagaraActor(FBFObjectPoolBP& Pool,
	const FBFPoolableNiagaraActorDescription& InitParams, EBFPooledObjectReclaimPolicy Policy, const FTransform& ActorTransform, EBFSuccess& Result,
	UObject*& ReturnObject)
{
	ReturnObject = nullptr;
	Result = BF::OP::ToBPSuccessEnum(false);
	
	if(!Pool.HasPoolBeenInitialized() || !Pool.InitInfo.PoolClass->IsChildOf<ABFPoolableNiagaraActor>())
	{
#if !UE_BUILD_SHIPPING
		
		if (!Pool.InitInfo.PoolClass->IsChildOf<ABFPoolableNiagaraActor>())
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Failed to unpool object from a pool that is not of a compatible type to use with QuickUnpoolNiagaraActor."));
		if (!Pool.ObjectPool.IsValid())
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Failed to unpool object from a pool that is not valid."));
		if (!Pool.ObjectPool->HasBeenInitialized())
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Failed to unpool object from a pool that has not been initialized."));
#endif
		Result = BF::OP::ToBPSuccessEnum(false);
		return;
	}

	FBFPooledObjectHandleBP BPHandle;
	UnpoolObject(Pool, BPHandle, Policy, Result, ReturnObject, false);
	if(Result == EBFSuccess::Success)
	{
		auto* Actor = CastChecked<ABFPoolableNiagaraActor>(BPHandle.Handle->GetObject());
		Actor->FireAndForgetBP(BPHandle, InitParams, ActorTransform);
	}
}

void UBFObjectPoolingBlueprintFunctionLibrary::QuickUnpoolSoundActor(FBFObjectPoolBP& Pool,
	const FBFPoolableSoundActorDescription& InitParams, EBFPooledObjectReclaimPolicy Policy, const FTransform& ActorTransform, EBFSuccess& Result,
	UObject*& ReturnObject)
{
	ReturnObject = nullptr;
	Result = BF::OP::ToBPSuccessEnum(false);
	
	if(!Pool.HasPoolBeenInitialized() || !Pool.InitInfo.PoolClass->IsChildOf<ABFPoolableSoundActor>())
	{
#if !UE_BUILD_SHIPPING
		
		if (!Pool.InitInfo.PoolClass->IsChildOf<ABFPoolableSoundActor>())
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Failed to unpool object from a pool that is not of a compatible type to use with QuickUnpoolSoundActor."));
		if (!Pool.ObjectPool.IsValid())
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Failed to unpool object from a pool that is not valid."));
		if (!Pool.ObjectPool->HasBeenInitialized())
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Failed to unpool object from a pool that has not been initialized."));
#endif
		Result = BF::OP::ToBPSuccessEnum(false);
		return;
	}

	FBFPooledObjectHandleBP BPHandle;
	UnpoolObject(Pool, BPHandle, Policy, Result, ReturnObject, false);
	if(Result == EBFSuccess::Success)
	{
		auto* Actor = CastChecked<ABFPoolableSoundActor>(BPHandle.Handle->GetObject());
		Actor->FireAndForgetBP(BPHandle, InitParams, ActorTransform);
	}
}

void UBFObjectPoolingBlueprintFunctionLibrary::QuickUnpoolDecalActor(FBFObjectPoolBP& Pool,
																	const FBFPoolableDecalActorDescription& InitParams, 
																	EBFPooledObjectReclaimPolicy Policy,
																	const FTransform& ActorTransform,
																	EBFSuccess& Result,
																	UObject*& ReturnObject)
{
	ReturnObject = nullptr;
	Result = BF::OP::ToBPSuccessEnum(false);
	
	if(!Pool.HasPoolBeenInitialized() || !Pool.InitInfo.PoolClass->IsChildOf<ABFPoolableDecalActor>())
	{
#if !UE_BUILD_SHIPPING
		
		if (!Pool.InitInfo.PoolClass->IsChildOf<ABFPoolableDecalActor>())
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Failed to unpool object from a pool that is not of a compatible type to use with QuickUnpoolDecalActor."));
		if (!Pool.ObjectPool.IsValid())
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Failed to unpool object from a pool that is not valid."));
		if (!Pool.ObjectPool->HasBeenInitialized())
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Failed to unpool object from a pool that has not been initialized."));
#endif
		Result = BF::OP::ToBPSuccessEnum(false);
		return;
	}
	

	FBFPooledObjectHandleBP BPHandle;
	UnpoolObject(Pool, BPHandle, Policy, Result, ReturnObject, false);
	if(Result == EBFSuccess::Success)
	{
		auto* Actor = CastChecked<ABFPoolableDecalActor>(BPHandle.Handle->GetObject());
		Actor->FireAndForgetBP(BPHandle, InitParams, ActorTransform);
	}
}

void UBFObjectPoolingBlueprintFunctionLibrary::QuickUnpool3DWidgetActor(FBFObjectPoolBP& Pool,
																		const FBFPoolable3DWidgetActorDescription& InitParams,
																		EBFPooledObjectReclaimPolicy Policy,
																		const FTransform& ActorTransform, EBFSuccess& Result,
																		UObject*& ReturnObject)
{
	ReturnObject = nullptr;
	Result = BF::OP::ToBPSuccessEnum(false);
	
	if(!Pool.HasPoolBeenInitialized() || !Pool.InitInfo.PoolClass->IsChildOf<ABFPoolable3DWidgetActor>())
	{
#if !UE_BUILD_SHIPPING
		
		if (!Pool.InitInfo.PoolClass->IsChildOf<ABFPoolable3DWidgetActor>())
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Failed to unpool object from a pool that is not of a compatible type to use with QuickUnpool3DWidgetActor."));
		if (!Pool.ObjectPool.IsValid())
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Failed to unpool object from a pool that is not valid."));
		if (!Pool.ObjectPool->HasBeenInitialized())
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Failed to unpool object from a pool that has not been initialized."));
#endif
		Result = BF::OP::ToBPSuccessEnum(false);
		return;
	}

	FBFPooledObjectHandleBP BPHandle;
	UnpoolObject(Pool, BPHandle, Policy, Result, ReturnObject, false);
	if(Result == EBFSuccess::Success)
	{
		auto* Actor = CastChecked<ABFPoolable3DWidgetActor>(BPHandle.Handle->GetObject());
		Actor->FireAndForgetBP(BPHandle, InitParams, ActorTransform);
	}
}

