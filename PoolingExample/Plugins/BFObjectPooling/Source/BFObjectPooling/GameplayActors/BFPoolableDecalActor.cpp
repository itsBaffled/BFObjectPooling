// Copyright (c) 2024 Jack Holland 
// Licensed under the MIT License. See LICENSE.md file in repo root for full license information.


#include "BFPoolableDecalActor.h"
#include "Components/DecalComponent.h"

#include "BFObjectPooling/PoolBP/BFPooledObjectHandleBP.h"
#include "BFObjectPooling/Pool/Private/BFObjectPoolHelpers.h"


namespace BF::OP
{
	static auto* DecalFadeDurationCVar = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.Decal.FadeDurationScale"));
}


ABFPoolableDecalActor::ABFPoolableDecalActor(const FObjectInitializer& ObjectInitializer)
: Super( ObjectInitializer )
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	static FName RootComponentName{"RootComponent"};
	RootComponent = CreateDefaultSubobject<USceneComponent>(RootComponentName);

	static FName DecalComponentName{"DecalMeshComponent"};
	DecalComponent = CreateDefaultSubobject<UDecalComponent>(DecalComponentName);
	DecalComponent->SetupAttachment(RootComponent);
}


void ABFPoolableDecalActor::FireAndForgetBP(FBFPooledObjectHandleBP& Handle,
	const FBFPoolableDecalActorDescription& ActivationParams, const FTransform& ActorTransform)
{
	// You must pass a valid handle otherwise it defeats the purpose of this function.
	check(Handle.Handle.IsValid() && Handle.Handle->IsHandleValid());
	if (ActivationParams.DecalMaterial == nullptr)
	{
		UE_LOGFMT(LogTemp, Warning, "PoolableDecalActor was handed a null material to display, was this intentional?");
		Handle.Handle->ReturnToPool();
		return;
	}	
	
	SetPoolHandleBP(Handle);
	SetPoolableActorParams(ActivationParams);

	if (ActivationInfo.OptionalAttachmentParams.IsSet())
	{
		FBFPoolableActorAttachmentDescription& P = ActivationInfo.OptionalAttachmentParams;
		if (P.AttachmentComponent != nullptr)
		{
			AttachToComponent(ActivationInfo.OptionalAttachmentParams.AttachmentComponent,
				FAttachmentTransformRules(P.LocationRule, P.RotationRule, P.ScaleRule, P.bWeldSimulatedBodies), P.SocketName);
			
			bIsAttached = true;
		}
		else
		{
			AttachToActor(ActivationInfo.OptionalAttachmentParams.AttachmentActor,
				FAttachmentTransformRules(P.LocationRule, P.RotationRule, P.ScaleRule, P.bWeldSimulatedBodies), P.SocketName);

			bIsAttached = true;
		}
	}
	
	if(ActivationInfo.ActorCurfew > 0.f)
		SetCurfew(ActivationInfo.ActorCurfew);

	SetActorHiddenInGame(false);
	SetActorTickEnabled(true);
	
	SetActorTransform(ActorTransform);
	ActivatePoolableActor();
}


void ABFPoolableDecalActor::FireAndForget(TBFPooledObjectHandlePtr<ABFPoolableDecalActor, ESPMode::NotThreadSafe>& Handle,
	const FBFPoolableDecalActorDescription& ActivationParams, const FTransform& ActorTransform)
{
	// You must pass a valid handle otherwise it defeats the purpose of this function.
	check(Handle.IsValid() && Handle->IsHandleValid());
	if (ActivationParams.DecalMaterial == nullptr)
	{
		UE_LOGFMT(LogTemp, Warning, "PoolableDecalActor was handed a null material to display, was this intentional?");
		Handle->ReturnToPool();
		return;
	}
	
	SetPoolHandle(Handle);
	SetPoolableActorParams(ActivationParams);

	if (ActivationInfo.OptionalAttachmentParams.IsSet())
	{
		FBFPoolableActorAttachmentDescription& P = ActivationInfo.OptionalAttachmentParams;
		if (P.AttachmentComponent != nullptr)
		{
			AttachToComponent(ActivationInfo.OptionalAttachmentParams.AttachmentComponent,
				FAttachmentTransformRules(P.LocationRule, P.RotationRule, P.ScaleRule, P.bWeldSimulatedBodies), P.SocketName);
			
			bIsAttached = true;
		}
		else
		{
			AttachToActor(ActivationInfo.OptionalAttachmentParams.AttachmentActor,
				FAttachmentTransformRules(P.LocationRule, P.RotationRule, P.ScaleRule, P.bWeldSimulatedBodies), P.SocketName);

			bIsAttached = true;
		}
	}
	
	
	if(ActivationInfo.ActorCurfew > 0)
		SetCurfew(ActivationInfo.ActorCurfew);
	
	SetActorHiddenInGame(false);
	
	SetActorTransform(ActorTransform);
	ActivatePoolableActor();
}


void ABFPoolableDecalActor::SetPoolableActorParams( const FBFPoolableDecalActorDescription& ActivationParams)
{
	ActivationInfo = ActivationParams;
}


void ABFPoolableDecalActor::ActivatePoolableActor()
{
	SetupObjectState();
}


void ABFPoolableDecalActor::SetupObjectState()
{
	DecalComponent->SetMaterial(0, ActivationInfo.DecalMaterial);
	DecalComponent->DecalSize = ActivationInfo.DecalExtent;
	DecalComponent->SortOrder = ActivationInfo.SortOrder;
	DecalComponent->FadeScreenSize = ActivationInfo.FadeScreenSize;
	
	if(ActivationInfo.FadeInTime > 0)
	{
		float FadeDurationScale = BF::OP::DecalFadeDurationCVar->GetValueOnGameThread();
		DecalComponent->FadeInStartDelay = 0.f;
		DecalComponent->FadeInDuration = FadeDurationScale > KINDA_SMALL_NUMBER ? ActivationInfo.FadeInTime / FadeDurationScale : KINDA_SMALL_NUMBER;
		
		if(DecalComponent->SceneProxy)
			GetWorld()->Scene->UpdateDecalFadeOutTime(DecalComponent);
		else
			DecalComponent->MarkRenderStateDirty();
	}
}


bool ABFPoolableDecalActor::ReturnToPool()
{
	// If successful this leads to the interface call OnObjectPooled.
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


void ABFPoolableDecalActor::OnObjectPooled_Implementation()
{
	RemoveCurfew();
	GetWorld()->GetTimerManager().ClearTimer(FadeOutTimerHandle);

	if (bIsAttached)
	{
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		bIsAttached = false;
	}

	DecalComponent->FadeInDuration = 0.f;
	DecalComponent->FadeInStartDelay = 0.f;
	DecalComponent->FadeDuration = 0.f;
	DecalComponent->FadeStartDelay = 0.f;

	ObjectHandle = nullptr;
	BPObjectHandle = nullptr;
	ActivationInfo = {};
}


void ABFPoolableDecalActor::SetPoolHandleBP(FBFPooledObjectHandleBP& Handle)
{
	bfEnsure(ObjectHandle == nullptr); // You can't have both handles set.
	bfEnsure(Handle.Handle.IsValid() && Handle.Handle->IsHandleValid()); // You must have a valid handle.
	
	bIsUsingBPHandle = true;
	BPObjectHandle = Handle.Handle;
	Handle.Invalidate(); // Invalidate the handle so it can't be used again.
}


void ABFPoolableDecalActor::SetPoolHandle(TBFPooledObjectHandlePtr<ABFPoolableDecalActor, ESPMode::NotThreadSafe>& Handle)
{
	bfEnsure(BPObjectHandle == nullptr); // You can't have both handles set.
	bfEnsure(Handle.IsValid() && Handle->IsHandleValid()); // You must have a valid handle.
	
	bIsUsingBPHandle = false;
	ObjectHandle = Handle;
	Handle.Reset(); // Invalidate the handle so it can't be used again.
}


void ABFPoolableDecalActor::FellOutOfWorld(const UDamageType& DmgType)
{
	// Super::FellOutOfWorld(DmgType); do not want default behaviour here.
#if !UE_BUILD_SHIPPING
	if(BF::OP::CVarObjectPoolEnableLogging.GetValueOnGameThread() == true)
		UE_LOGFMT(LogTemp, Warning, "{0} Fell out of map, auto returning to pool.", GetName());
#endif
	ReturnToPool();
}


void ABFPoolableDecalActor::SetCurfew(float SecondsUntilReturn)
{
	if(SecondsUntilReturn > 0)
	{
		RemoveCurfew();
		GetWorld()->GetTimerManager().SetTimer(CurfewTimerHandle, this, &ABFPoolableDecalActor::OnCurfewExpired, SecondsUntilReturn, false);
	}
}


void ABFPoolableDecalActor::RemoveCurfew()
{
	if(GetWorld()->GetTimerManager().IsTimerActive(CurfewTimerHandle))
		GetWorld()->GetTimerManager().ClearTimer(CurfewTimerHandle);
	
	CurfewTimerHandle.Invalidate();
}


void ABFPoolableDecalActor::OnCurfewExpired()
{
	if(ActivationInfo.FadeOutTime > 0)
	{
		float FadeDurationScale = BF::OP::DecalFadeDurationCVar->GetValueOnGameThread();
		DecalComponent->FadeStartDelay = 0.f;
		DecalComponent->FadeDuration = FadeDurationScale > KINDA_SMALL_NUMBER ? ActivationInfo.FadeOutTime / FadeDurationScale : KINDA_SMALL_NUMBER;
		
		if(DecalComponent->SceneProxy)
			GetWorld()->Scene->UpdateDecalFadeOutTime(DecalComponent);
		else
			DecalComponent->MarkRenderStateDirty();

		FTimerDelegate TimerDel = FTimerDelegate::CreateWeakLambda(this,
			[this]()
			{
				ReturnToPool();
			}
		);
		GetWorld()->GetTimerManager().SetTimer(FadeOutTimerHandle, TimerDel, DecalComponent->FadeDuration, false);
		return;
	}
	ReturnToPool();
}


