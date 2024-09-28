// Copyright (c) 2024 Jack Holland 
// Licensed under the MIT License. See LICENSE.md file in repo root for full license information.

#include "BFPoolableNiagaraActor.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "BFObjectPooling/Pool/Private/BFObjectPoolHelpers.h"
#include "BFObjectPooling/PoolBP/BFPooledObjectHandleBP.h"



ABFPoolableNiagaraActor::ABFPoolableNiagaraActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>("RootComponent");
	NiagaraComponent = CreateDefaultSubobject<UNiagaraComponent>("NiagaraComponent");
	NiagaraComponent->SetupAttachment(RootComponent);
}


void ABFPoolableNiagaraActor::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	GetNiagaraComponent()->OnSystemFinished.AddUniqueDynamic(this, &ABFPoolableNiagaraActor::OnNiagaraSystemFinished);
}


void ABFPoolableNiagaraActor::FireAndForgetBP(FBFPooledObjectHandleBP& Handle,
		const FBFPoolableNiagaraActorDescription& ActivationParams, const FTransform& ActorTransform)
{
	bfEnsure(Handle.Handle.IsValid() && Handle.Handle->IsHandleValid()); // You must have a valid handle.
	bfValid(ActivationParams.NiagaraSystem); // you must set the system


	SetPoolHandleBP(Handle);
	SetActorTransform(ActorTransform);
	SetPoolableActorParams(ActivationParams);
	
	if(ActivationInfo.ActorCurfew > 0)
	{
		if(ActivationInfo.DelayedActivationTimeSeconds > 0)
			ActivationInfo.ActorCurfew += ActivationInfo.DelayedActivationTimeSeconds;
		
		SetCurfew(ActivationInfo.ActorCurfew);
	}

	if(ActivationInfo.DelayedActivationTimeSeconds > 0)
	{
		FTimerDelegate TimerDel;
		TimerDel.BindWeakLambda(this, [this]()
		{
			SetActorHiddenInGame(false);
			ActivatePoolableActor();
		});
		GetWorld()->GetTimerManager().SetTimer(DelayedActivationTimerHandle, TimerDel, ActivationInfo.DelayedActivationTimeSeconds, false);
	}
	else // Start it now
	{
		SetActorHiddenInGame(false);
		ActivatePoolableActor();
	}
}


void ABFPoolableNiagaraActor::FireAndForget(TBFPooledObjectHandlePtr<ABFPoolableNiagaraActor, ESPMode::NotThreadSafe>& Handle,
		const FBFPoolableNiagaraActorDescription& ActivationParams, const FTransform& ActorTransform)
{
	bfEnsure(Handle.IsValid() && Handle->IsHandleValid()); // You must have a valid handle.
	bfValid(ActivationParams.NiagaraSystem); // you must set the system

	// Even if delayed we set the transform and stay waiting hidden until the delayed activation time.
	SetPoolHandle(Handle);
	SetActorTransform(ActorTransform);
	SetPoolableActorParams(ActivationParams);
	
	if(ActivationInfo.ActorCurfew > 0)
		SetCurfew(ActivationInfo.ActorCurfew);


	if(ActivationInfo.DelayedActivationTimeSeconds > 0)
	{
		FTimerHandle TimerHandle;
		FTimerDelegate TimerDel;
		TimerDel.BindWeakLambda(this, [this]()
		{
			SetActorHiddenInGame(false);
			ActivatePoolableActor();
		});
		GetWorld()->GetTimerManager().SetTimer(TimerHandle, TimerDel, ActivationInfo.DelayedActivationTimeSeconds, false);
	}
	else // Start it now
	{
		SetActorHiddenInGame(false);
		ActivatePoolableActor();
	}
}


void ABFPoolableNiagaraActor::SetPoolableActorParams(const FBFPoolableNiagaraActorDescription& ActivationParams)
{
	bfValid(ActivationParams.NiagaraSystem); // You must set the system for a niagara actor otherwise what is the point.
	ActivationInfo = ActivationParams;
}


void ABFPoolableNiagaraActor::ActivatePoolableActor()
{
	bfValid(ActivationInfo.NiagaraSystem); // You must set the system
	GetNiagaraComponent()->SetAsset(ActivationInfo.NiagaraSystem);
	ResetSystem();
}


// If successful this leads to the interface call OnObjectPooled.
bool ABFPoolableNiagaraActor::ReturnToPool()
{
	bHasFinished = true;
	OnNiagaraSystemFinishedDelegate.Broadcast();

	if(bIsUsingBPHandle)
	{
		if(BPObjectHandle.IsValid() && BPObjectHandle->IsHandleValid())
			return BPObjectHandle->ReturnToPool();
	}
	else
	{
		if(ObjectHandle.IsValid() && ObjectHandle->IsHandleValid())
			return ObjectHandle->ReturnToPool();
	}
	return false;
}


void ABFPoolableNiagaraActor::OnObjectPooled_Implementation()
{
	// Reset everything.
	GetWorld()->GetTimerManager().ClearTimer(DelayedActivationTimerHandle);
	RemoveCurfew();

	ObjectHandle = nullptr;
	BPObjectHandle = nullptr;
	bHasFinished = false; 
	OnNiagaraSystemFinishedDelegate.Clear();
	ActivationInfo = {};
	
	if(GetNiagaraComponent())
		GetNiagaraComponent()->DeactivateImmediate();
}


void ABFPoolableNiagaraActor::SetPoolHandle(TBFPooledObjectHandlePtr<ABFPoolableNiagaraActor, ESPMode::NotThreadSafe>& Handle)
{
	bfEnsure(ObjectHandle == nullptr); // You can't have both handles set.
	bfEnsure(Handle.IsValid() && Handle->IsHandleValid()); // You must have a valid handle.
	bIsUsingBPHandle = false;
	ObjectHandle = Handle;
	Handle.Reset(); // Clear the handle so it can't be used again.
}


void ABFPoolableNiagaraActor::SetPoolHandleBP(FBFPooledObjectHandleBP& Handle)
{
	bfEnsure(BPObjectHandle == nullptr); // You can't have both handles set.
	bfEnsure(Handle.Handle.IsValid() && Handle.Handle->IsHandleValid()); // You must have a valid handle.
	bIsUsingBPHandle = true;
	BPObjectHandle = Handle.Handle;
	Handle.Reset(); // Clear the handle so it can't be used again.	
}


void ABFPoolableNiagaraActor::FellOutOfWorld(const UDamageType& DmgType)
{
	// Super::FellOutOfWorld(DmgType); do not want default behaviour here.
#if !UE_BUILD_SHIPPING
	if(BF::OP::CVarObjectPoolEnableLogging.GetValueOnGameThread() == true)
		UE_LOGFMT(LogTemp, Warning, "{0} Fell out of map, auto returning to pool.", GetName());
#endif
	ReturnToPool();
}


void ABFPoolableNiagaraActor::OnNiagaraSystemFinished(UNiagaraComponent* FinishedComponent)
{
	if(GetAutoReturnOnSystemFinish())
		ReturnToPool();
}


void ABFPoolableNiagaraActor::OnCurfewExpired()
{
	ReturnToPool();
}


void ABFPoolableNiagaraActor::ResetSystem()
{
	bHasFinished = false;
	GetNiagaraComponent()->ResetSystem();
}


bool ABFPoolableNiagaraActor::IsSystemLooping() const
{
	if(GetNiagaraComponent() && !GetNiagaraComponent()->IsComplete() && GetNiagaraComponent()->GetAsset())
		return GetNiagaraComponent()->GetAsset()->IsLooping();
	return false;
}


void ABFPoolableNiagaraActor::SetCurfew(float SecondsUntilReturn)
{
	bfEnsure(SecondsUntilReturn > 0);
	RemoveCurfew();
	GetWorld()->GetTimerManager().SetTimer(CurfewTimerHandle, this,  &ABFPoolableNiagaraActor::OnCurfewExpired, SecondsUntilReturn);
}


void ABFPoolableNiagaraActor::RemoveCurfew()
{
	if(GetWorld()->GetTimerManager().IsTimerActive(CurfewTimerHandle))
		GetWorld()->GetTimerManager().ClearTimer(CurfewTimerHandle);

	CurfewTimerHandle.Invalidate();
}


UNiagaraSystem* ABFPoolableNiagaraActor::GetNiagaraSystem() const
{
	return GetNiagaraComponent()->GetAsset();
}


void ABFPoolableNiagaraActor::BeginDestroy()
{
	Super::BeginDestroy();

	if(GetNiagaraComponent())
		GetNiagaraComponent()->OnSystemFinished.RemoveAll(this);
}










