﻿// Copyright (c) 2024 Jack Holland 
// Licensed under the MIT License. See LICENSE.md file in repo root for full license information.

#include "BFPoolableStaticMeshActor.h"
#include "BFObjectPooling/Pool/Private/BFObjectPoolHelpers.h"
#include "BFObjectPooling/PoolBP/BFPooledObjectHandleBP.h"



ABFPoolableStaticMeshActor::ABFPoolableStaticMeshActor(const FObjectInitializer& ObjectInitializer)
	: Super( ObjectInitializer )
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// Was going to opt for StaticMesh as the root but this is more flexible in case you want offsets or different rotations.
	static const FName RootComponentName{"RootComponent"};
	RootComponent = CreateDefaultSubobject<USceneComponent>(RootComponentName);
	RootComponent->SetMobility(EComponentMobility::Movable);

	static const FName StaticMeshComponentName{"StaticMeshComponent"};
	StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(StaticMeshComponentName);
	StaticMeshComponent->SetupAttachment(RootComponent);
}


void ABFPoolableStaticMeshActor::FireAndForgetBP(FBFPooledObjectHandleBP& Handle,
	const FBFPoolableStaticMeshActorDescription& ActivationParams, const FTransform& ActorTransform)
{
	// You must pass a valid handle otherwise it defeats the purpose of this function.
	check(Handle.Handle.IsValid() && Handle.Handle->IsHandleValid());
	
	if (ActivationParams.Mesh == nullptr || ActivationParams.ActorCurfew < 0.f)
	{
#if !UE_BUILD_SHIPPING
		if(ActivationParams.Mesh == nullptr)
			UE_LOGFMT(LogTemp, Warning, "PoolableSkeletalMeshActor was handed a null Mesh asset to display, was this intentional?");
		else
			UE_LOGFMT(LogTemp, Warning, "PoolableSkeletalMeshActor was handed invalid ActivationParams, ActorCurfew must be greater than 0.");
#endif
		Handle.Handle->ReturnToPool();
		return;
	}
	
	SetPoolHandleBP(Handle);
	SetPoolableActorParams(ActivationParams);
	
	if(ActivationInfo.ActorCurfew > 0.f)
		SetCurfew(ActivationInfo.ActorCurfew);

	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	
	SetActorTransform(ActorTransform, false, nullptr, ETeleportType::ResetPhysics);
	ActivatePoolableActor(ActivationInfo.bSimulatePhysics);
}




void ABFPoolableStaticMeshActor::FireAndForget(TBFPooledObjectHandlePtr<ABFPoolableStaticMeshActor, ESPMode::NotThreadSafe>& Handle,
                                               const FBFPoolableStaticMeshActorDescription& ActivationParams, const FTransform& ActorTransform)
{
	// You must pass a valid handle otherwise it defeats the purpose of this function.
	check(Handle.IsValid() && Handle->IsHandleValid());
	
	if (ActivationParams.Mesh == nullptr || ActivationParams.ActorCurfew < 0.f)
	{
#if !UE_BUILD_SHIPPING
		if(ActivationParams.Mesh == nullptr)
			UE_LOGFMT(LogTemp, Warning, "PoolableSkeletalMeshActor was handed a null Mesh asset to display, was this intentional?");
		else
			UE_LOGFMT(LogTemp, Warning, "PoolableSkeletalMeshActor was handed invalid ActivationParams, ActorCurfew must be greater than 0.");
#endif
		Handle->ReturnToPool();
		return;
	}
	
	SetPoolHandle(Handle);
	SetPoolableActorParams(ActivationParams);
	
	if(ActivationInfo.ActorCurfew > 0.f)
		SetCurfew(ActivationInfo.ActorCurfew);

	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	
	SetActorTransform(ActorTransform, false, nullptr, ETeleportType::ResetPhysics);
	ActivatePoolableActor(ActivationInfo.bSimulatePhysics);
}


void ABFPoolableStaticMeshActor::SetPoolableActorParams( const FBFPoolableStaticMeshActorDescription& ActivationParams)
{
	ActivationInfo = ActivationParams;
}


void ABFPoolableStaticMeshActor::ActivatePoolableActor(bool bSimulatePhysics)
{
	SetupObjectState(bSimulatePhysics);
	bWasSimulating = bSimulatePhysics;
}

void ABFPoolableStaticMeshActor::SetupObjectState(bool bSimulatePhysics)
{
	bfValid(ActivationInfo.Mesh); // you must set the mesh
	StaticMeshComponent->SetStaticMesh(ActivationInfo.Mesh);

	for(const auto& [Material, Slot] : ActivationInfo.Materials)
		StaticMeshComponent->SetMaterial(Slot, Material);

	StaticMeshComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::SnapToTargetIncludingScale);
	StaticMeshComponent->SetRelativeTransform(ActivationInfo.RelativeTransform);
	
	// I've noticed issues with un-pooling and activating a simulating mesh sometimes causes the BodyInstance to not update its transform (even though our actor and mesh scene comp transform does update),
	// not sure why but I suspect visibility has something to do with it when we return to the pool, anyway this just ensures they are in sync.
	if(bSimulatePhysics && bWasSimulating)
		StaticMeshComponent->BodyInstance.SetBodyTransform(StaticMeshComponent->GetComponentTransform(), ETeleportType::ResetPhysics, bSimulatePhysics);
	
	StaticMeshComponent->SetCollisionProfileName(ActivationInfo.CollisionProfile.Name);
	StaticMeshComponent->SetCollisionEnabled(ActivationInfo.CollisionEnabled);

	StaticMeshComponent->SetSimulatePhysics(bSimulatePhysics);

	if (ActivationInfo.PhysicsBodySleepDelay > 0.f)
	{
		FTimerDelegate TimerDel = FTimerDelegate::CreateWeakLambda(this,
		[this]()
		{
			StaticMeshComponent->SetSimulatePhysics(false);
			StaticMeshComponent->SetCollisionProfileName(MeshSleepPhysicsProfile.Name);
		});
		GetWorld()->GetTimerManager().SetTimer(SleepPhysicsTimerHandle, TimerDel, ActivationInfo.PhysicsBodySleepDelay, false);
	}
}


bool ABFPoolableStaticMeshActor::ReturnToPool()
{
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


void ABFPoolableStaticMeshActor::OnObjectPooled_Implementation()
{
	RemoveCurfew(); 
	GetWorld()->GetTimerManager().ClearTimer(SleepPhysicsTimerHandle);
	
	StaticMeshComponent->SetSimulatePhysics(false);
	StaticMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	
	ObjectHandle = nullptr;
	BPObjectHandle = nullptr;

	ActivationInfo = {};
}


void ABFPoolableStaticMeshActor::SetPoolHandleBP(FBFPooledObjectHandleBP& Handle)
{
	bfEnsure(ObjectHandle == nullptr); // You can't have both handles set.
	bfEnsure(Handle.Handle.IsValid() && Handle.Handle->IsHandleValid()); // You must have a valid handle.
	
	bIsUsingBPHandle = true;
	BPObjectHandle = Handle.Handle;
	Handle.Invalidate();
}


void ABFPoolableStaticMeshActor::SetPoolHandle(TBFPooledObjectHandlePtr<ABFPoolableStaticMeshActor, ESPMode::NotThreadSafe>& Handle)
{
	bfEnsure(BPObjectHandle == nullptr); // You can't have both handles set.
	bfEnsure(Handle.IsValid() && Handle->IsHandleValid()); // You must have a valid handle.
	
	bIsUsingBPHandle = false;
	ObjectHandle = Handle;
	Handle.Reset();
}


void ABFPoolableStaticMeshActor::FellOutOfWorld(const UDamageType& DmgType)
{
	// Super::FellOutOfWorld(DmgType); do not want default behaviour here.
#if !UE_BUILD_SHIPPING
	if(BF::OP::CVarObjectPoolEnableLogging.GetValueOnGameThread() == true)
		UE_LOGFMT(LogTemp, Warning, "{0} Fell out of map, auto returning to pool.", GetName());
#endif
	ReturnToPool();
}


UStaticMesh* ABFPoolableStaticMeshActor::GetStaticMesh() const
{
	return StaticMeshComponent->GetStaticMesh();
}


void ABFPoolableStaticMeshActor::SetCurfew(float SecondsUntilReturn)
{
	if(SecondsUntilReturn > 0)
	{
		RemoveCurfew();
		GetWorld()->GetTimerManager().SetTimer(CurfewTimerHandle, this, &ABFPoolableStaticMeshActor::OnCurfewExpired, SecondsUntilReturn, false);
	}
}


void ABFPoolableStaticMeshActor::RemoveCurfew()
{
	if(GetWorld()->GetTimerManager().IsTimerActive(CurfewTimerHandle))
		GetWorld()->GetTimerManager().ClearTimer(CurfewTimerHandle);
	
	CurfewTimerHandle.Invalidate();
}


void ABFPoolableStaticMeshActor::OnCurfewExpired()
{
	ReturnToPool();
}
