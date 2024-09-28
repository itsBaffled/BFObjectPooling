// Copyright (c) 2024 Jack Holland 
// Licensed under the MIT License. See LICENSE.md file in repo root for full license information.

#include "BFPoolableSkeletalMeshActor.h"
#include "BFObjectPooling/Pool/Private/BFPoolContainer.h"
#include "BFObjectPooling/PoolBP/BFPooledObjectHandleBP.h"


ABFPoolableSkeletalMeshActor::ABFPoolableSkeletalMeshActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer) 
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>("RootComponent");
	RootComponent->SetMobility(EComponentMobility::Movable);
	
	SkeletalMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>("SkeletalMeshComponent");
	SkeletalMeshComponent->SetupAttachment(RootComponent);
}


void ABFPoolableSkeletalMeshActor::FireAndForgetBP(FBFPooledObjectHandleBP& Handle,
	const FBFPoolableSkeletalMeshActorDescription& ActivationParams, const FTransform& ActorTransform)
{
	bfEnsure(Handle.Handle.IsValid()); // You must have a valid handle.
	bfEnsure(Handle.Handle.IsValid() && Handle.Handle->IsHandleValid()); // You must have a valid handle.

	// Either dont use a delay or activate before returning to the pool.
	bfEnsure(ActivationParams.PhysicsBodySleepDelay < 0.f || ActivationParams.ActorCurfew < 0.f ||
		ActivationParams.ActorCurfew > ActivationParams.PhysicsBodySleepDelay);

	SetPoolHandleBP(Handle);
	SetPoolableActorParams(ActivationParams);
	
	if(ActivationInfo.ActorCurfew > 0)
		SetCurfew(ActivationInfo.ActorCurfew);

	// Assumes we are responsible for activation so just go ahead and make sure.
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	
	// Enable ticking only when needed
	bool bNeedsTick = ActivationInfo.bSimulatePhysics || ActivationInfo.AnimationInstance || ActivationInfo.AnimSequence;
	SkeletalMeshComponent->SetComponentTickEnabled(bNeedsTick);
	SkeletalMeshComponent->SetComponentTickInterval(ActivationParams.MeshTickInterval);

	SetActorTransform(ActorTransform, false, nullptr, ETeleportType::ResetPhysics);
	ActivatePoolableActor(ActivationInfo.bSimulatePhysics);
}


void ABFPoolableSkeletalMeshActor::FireAndForget(TBFPooledObjectHandlePtr<ABFPoolableSkeletalMeshActor, ESPMode::NotThreadSafe>& Handle,
	const FBFPoolableSkeletalMeshActorDescription& ActivationParams, const FTransform& ActorTransform)
{
	bfEnsure(Handle.IsValid()); // You must have a valid handle.
	bfEnsure(Handle.IsValid() && Handle->IsHandleValid()); // You must have a valid handle.
	
	// Either dont use a delay or activate before returning to the pool.
	bfEnsure(ActivationParams.PhysicsBodySleepDelay < 0.f || ActivationParams.ActorCurfew < 0.f || ActivationParams.ActorCurfew > ActivationParams.PhysicsBodySleepDelay);
	
	SetPoolHandle(Handle);
	SetPoolableActorParams(ActivationParams);

	if(ActivationInfo.ActorCurfew > 0)
		SetCurfew(ActivationInfo.ActorCurfew);

	// Assumes we are responsible for activation so just go ahead and make sure.
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	
	// Enable ticking only when needed
	bool bNeedsTick = ActivationInfo.bSimulatePhysics || ActivationInfo.AnimationInstance || ActivationInfo.AnimSequence;
	SkeletalMeshComponent->SetComponentTickEnabled(bNeedsTick);	
	SkeletalMeshComponent->SetComponentTickInterval(ActivationParams.MeshTickInterval);

	SetActorTransform(ActorTransform, false, nullptr, ETeleportType::ResetPhysics);
	ActivatePoolableActor(ActivationInfo.bSimulatePhysics);
}



void ABFPoolableSkeletalMeshActor::SetPoolableActorParams( const FBFPoolableSkeletalMeshActorDescription& ActivationParams)
{
	ActivationInfo = ActivationParams; 
}

void ABFPoolableSkeletalMeshActor::ActivatePoolableActor(bool bSimulatePhysics)
{
	SetupObjectState(bSimulatePhysics);

	// Should be overridden if not desired.
	if(bSimulatePhysics && ActivationInfo.PhysicsBodySleepDelay > 0)
	{
		RemovePhysicsSleepDelay();
		FTimerDelegate TimerDel;
		TimerDel.BindWeakLambda(this, [this]
		{
			SkeletalMeshComponent->PutAllRigidBodiesToSleep();
			SkeletalMeshComponent->SetCollisionProfileName(MeshSleepPhysicsProfile.Name);
		});
		GetWorld()->GetTimerManager().SetTimer(SleepPhysicsTimerHandle, TimerDel, ActivationInfo.PhysicsBodySleepDelay, false);
	}
}


void ABFPoolableSkeletalMeshActor::SetupObjectState(bool bSimulatePhysics)
{
	bfValid(ActivationInfo.Mesh);
	SkeletalMeshComponent->SetSkeletalMesh(ActivationInfo.Mesh);

	for(const auto& [Material, Slot] : ActivationInfo.Materials)
		SkeletalMeshComponent->SetMaterial(Slot, Material);
	
	SkeletalMeshComponent->SetRelativeTransform(ActivationInfo.RelativeTransform);

	SkeletalMeshComponent->SetCollisionProfileName(ActivationInfo.CollisionProfile.Name);
	SkeletalMeshComponent->SetCollisionEnabled(ActivationInfo.CollisionEnabled);

	// Apply the anim before simulating.
	if(IsValid(ActivationInfo.AnimationInstance) || IsValid(ActivationInfo.AnimSequence))
	{
		if(IsValid(ActivationInfo.AnimationInstance))
			SkeletalMeshComponent->SetAnimInstanceClass(ActivationInfo.AnimationInstance);
		else 
			SkeletalMeshComponent->PlayAnimation(ActivationInfo.AnimSequence, ActivationInfo.bLoopAnimSequence); 
	}

	SkeletalMeshComponent->SetSimulatePhysics(bSimulatePhysics);
}


// If successful this leads to the interface call OnObjectPooled.
bool ABFPoolableSkeletalMeshActor::ReturnToPool()
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


void ABFPoolableSkeletalMeshActor::OnObjectPooled_Implementation()
{
	RemoveCurfew();
	RemovePhysicsSleepDelay();

	if(SkeletalMeshComponent->IsSimulatingPhysics())
	{
		// I noticed due to disabling SKM tick it causes the bones
		// to resume from where they were last simulated if we don't refresh them.
		SkeletalMeshComponent->SetSimulatePhysics(false);
		SkeletalMeshComponent->RefreshBoneTransforms();
	}
	
	SkeletalMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SkeletalMeshComponent->SetComponentTickEnabled(false);
	ObjectHandle = nullptr;
	BPObjectHandle = nullptr;
	ActivationInfo = {};
}


void ABFPoolableSkeletalMeshActor::SetPoolHandleBP(FBFPooledObjectHandleBP& Handle)
{
	bfEnsure(ObjectHandle == nullptr); // You can't have both handles set.
	bfEnsure(Handle.Handle.IsValid() && Handle.Handle->IsHandleValid()); // You must have a valid handle.
	bIsUsingBPHandle = true;
	BPObjectHandle = Handle.Handle;
	Handle.Reset(); // Clear the handle so it can't be used again.
}


void ABFPoolableSkeletalMeshActor::SetPoolHandle(TBFPooledObjectHandlePtr<ABFPoolableSkeletalMeshActor, ESPMode::NotThreadSafe>& Handle)
{
	bfEnsure(BPObjectHandle == nullptr); // You can't have both handles set.
	bfEnsure(Handle.IsValid() && Handle->IsHandleValid()); // You must have a valid handle.
	bIsUsingBPHandle = false;
	ObjectHandle = Handle;
	Handle.Reset(); // Clear the handle so it can't be used again.
}


void ABFPoolableSkeletalMeshActor::FellOutOfWorld(const UDamageType& DmgType)
{
	// Super::FellOutOfWorld(DmgType); do not want default behaviour here.
#if !UE_BUILD_SHIPPING
	if(BF::OP::CVarObjectPoolEnableLogging.GetValueOnGameThread() == true)
		UE_LOGFMT(LogTemp, Warning, "{0} Fell out of map, auto returning to pool.", GetName());
#endif
	ReturnToPool();
}


void ABFPoolableSkeletalMeshActor::RemovePhysicsSleepDelay()
{
	if(GetWorld()->GetTimerManager().IsTimerActive(SleepPhysicsTimerHandle))
		GetWorld()->GetTimerManager().ClearTimer(SleepPhysicsTimerHandle);
}


USkeletalMesh* ABFPoolableSkeletalMeshActor::GetSkeletalMesh() const
{
	return SkeletalMeshComponent->GetSkeletalMeshAsset();
}


void ABFPoolableSkeletalMeshActor::SetCurfew(float SecondsUntilReturn)
{
	bfEnsure(SecondsUntilReturn > 0);
	RemoveCurfew();
	GetWorld()->GetTimerManager().SetTimer(CurfewTimerHandle, this, &ABFPoolableSkeletalMeshActor::OnCurfewExpired, SecondsUntilReturn, false);
}


void ABFPoolableSkeletalMeshActor::RemoveCurfew()
{
	if(GetWorld()->GetTimerManager().IsTimerActive(CurfewTimerHandle))
		GetWorld()->GetTimerManager().ClearTimer(CurfewTimerHandle);
	
	CurfewTimerHandle.Invalidate();
}


void ABFPoolableSkeletalMeshActor::OnCurfewExpired()
{
	ReturnToPool();
}



