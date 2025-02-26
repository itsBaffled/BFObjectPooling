﻿// Copyright (c) 2024 Jack Holland 
// Licensed under the MIT License. See LICENSE.md file in repo root for full license information.


#include "BFPoolableProjectileActor.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"

#include "BFObjectPooling/Pool/Private/BFObjectPoolHelpers.h"
#include "BFObjectPooling/PoolBP/BFPooledObjectHandleBP.h"
#include "GameFramework/ProjectileMovementComponent.h"



ABFPoolableProjectileActor::ABFPoolableProjectileActor(const FObjectInitializer& ObjectInitializer)
	: Super( ObjectInitializer )
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// Start with a SceneComponent in case we don't need a shape collision component.
	static FName RootComponentName{"RootComponent"};
	RootComponent = CreateDefaultSubobject<USceneComponent>(RootComponentName);
	RootComponent->SetMobility(EComponentMobility::Movable);
	
	static FName ProjectileMovementComponentName{"ProjectileMovementComponent"};
	ProjectileMovementComponent = CreateDefaultSubobject<UProjectileMovementComponent>(ProjectileMovementComponentName);
}


void ABFPoolableProjectileActor::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (GetWorld()->IsGameWorld())
	{
		ProjectileMovementComponent->OnProjectileStop.AddDynamic(this, &ABFPoolableProjectileActor::OnProjectileStopped);
	}
}


void ABFPoolableProjectileActor::FireAndForgetBP(FBFPooledObjectHandleBP& Handle, const FBFPoolableProjectileActorDescription& ActivationParams, const FTransform& ActorTransform)
{
	// You must pass a valid handle otherwise it defeats the purpose of this function.
	check(Handle.Handle.IsValid() && Handle.Handle->IsHandleValid());
	
	SetPoolHandleBP(Handle);
	SetPoolableActorParams(ActivationParams);

	if(ActivationInfo.ActorCurfew > 0.f)
		SetCurfew(ActivationInfo.ActorCurfew);

	SetActorEnableCollision(true);
	
	SetActorTransform(ActorTransform);
	ActivatePoolableActor();
}



void ABFPoolableProjectileActor::FireAndForget(TBFPooledObjectHandlePtr<ABFPoolableProjectileActor, ESPMode::NotThreadSafe>& Handle,
                                               const FBFPoolableProjectileActorDescription& ActivationParams, const FTransform& ActorTransform)
{
	// You must pass a valid handle otherwise it defeats the purpose of this function.
	check(Handle.IsValid() && Handle->IsHandleValid());
	
	SetPoolHandle(Handle);
	SetPoolableActorParams(ActivationParams);

	if(ActivationInfo.ActorCurfew > 0.f)
		SetCurfew(ActivationInfo.ActorCurfew);

	SetActorEnableCollision(true);
	
	SetActorTransform(ActorTransform);
	ActivatePoolableActor();
}


void ABFPoolableProjectileActor::SetPoolableActorParams( const FBFPoolableProjectileActorDescription& ActivationParams)
{
	ActivationInfo = ActivationParams;
}


void ABFPoolableProjectileActor::ActivatePoolableActor()
{
	HandleComponentCreation();
	SetupObjectState();
	
	if((ActivationInfo.ProjectileMesh.Mesh || ActivationInfo.NiagaraSystem) && IsHidden())
		SetActorHiddenInGame(false); // make sure we are visible when it matters
}


void ABFPoolableProjectileActor::SetupObjectState()
{
	bfValid(ProjectileMovementComponent);

	if(ActivationInfo.bIsVelocityInLocalSpace)
		ActivationInfo.Velocity = GetActorTransform().TransformVector(ActivationInfo.Velocity);
	
	ProjectileMovementComponent->bSweepCollision = ActivationInfo.bSweepCollision;
	ProjectileMovementComponent->bShouldBounce = ActivationInfo.bShouldBounce;
	ProjectileMovementComponent->bRotationFollowsVelocity = ActivationInfo.bRotationFollowsVelocity;
	ProjectileMovementComponent->bRotationRemainsVertical = ActivationInfo.bRotationRemainsVertical;

	ProjectileMovementComponent->Velocity = ActivationInfo.Velocity;
	ProjectileMovementComponent->MaxSpeed = ActivationInfo.MaxSpeed;
	ProjectileMovementComponent->Bounciness = ActivationInfo.Bounciness;
	ProjectileMovementComponent->ProjectileGravityScale = ActivationInfo.ProjectileGravityScale;
	ProjectileMovementComponent->Friction = ActivationInfo.Friction;
	
	ProjectileMovementComponent->bIsHomingProjectile = ActivationInfo.HomingTargetComponent != nullptr;
	ProjectileMovementComponent->HomingTargetComponent = ActivationInfo.HomingTargetComponent;
	ProjectileMovementComponent->HomingAccelerationMagnitude = ActivationInfo.HomingAccelerationSpeed;
	
	ProjectileMovementComponent->SetUpdatedComponent(RootComponent);
	ProjectileMovementComponent->SetComponentTickEnabled(true);
}


bool ABFPoolableProjectileActor::ReturnToPool()
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


void ABFPoolableProjectileActor::OnObjectPooled_Implementation()
{
	RemoveCurfew();
	
	ObjectHandle = nullptr;
	BPObjectHandle = nullptr;
	bHandledOverlapOrCollision = false;

	ActivationInfo = {};
	ProjectileMovementComponent->SetComponentTickEnabled(false);

	if(OptionalNiagaraComponent)
	{
		OptionalNiagaraComponent->Deactivate();
		OptionalNiagaraComponent->SetComponentTickEnabled(false);
	}

	if(OptionalStaticMeshComponent)
	{
		OptionalStaticMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		OptionalStaticMeshComponent->SetSimulatePhysics(false);
	}
	
	if(UShapeComponent* CollisionComponent = Cast<UShapeComponent>(RootComponent))
	{
		// We may be changing collision components between re-use so just clear all bindings and not worry about it.
		CollisionComponent->OnComponentHit.RemoveDynamic(this, &ABFPoolableProjectileActor::OnProjectileActorHit);
		CollisionComponent->OnComponentBeginOverlap.RemoveDynamic(this, &ABFPoolableProjectileActor::OnProjectileActorOverlap);
		CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}


void ABFPoolableProjectileActor::SetPoolHandleBP(FBFPooledObjectHandleBP& Handle)
{
	bfEnsure(ObjectHandle == nullptr); // You can't have both handles set.
	bfEnsure(Handle.Handle.IsValid() && Handle.Handle->IsHandleValid()); // You must have a valid handle.
	
	bIsUsingBPHandle = true;
	BPObjectHandle = Handle.Handle;
	Handle.Invalidate();
}


void ABFPoolableProjectileActor::SetPoolHandle(
	TBFPooledObjectHandlePtr<ABFPoolableProjectileActor, ESPMode::NotThreadSafe>& Handle)
{
	bfEnsure(BPObjectHandle == nullptr); // You can't have both handles set.
	bfEnsure(Handle.IsValid() && Handle->IsHandleValid()); // You must have a valid handle.
	
	bIsUsingBPHandle = false;
	ObjectHandle = Handle;
	Handle.Reset();
}


void ABFPoolableProjectileActor::FellOutOfWorld(const UDamageType& DmgType)
{
	// Super::FellOutOfWorld(DmgType); do not want default behaviour here.
#if !UE_BUILD_SHIPPING
	if(BF::OP::CVarObjectPoolEnableLogging.GetValueOnGameThread() == true)
		UE_LOGFMT(LogTemp, Warning, "{0} Fell out of map, auto returning to pool.", GetName());
#endif
	ReturnToPool();
}


bool ABFPoolableProjectileActor::HandleComponentCreation()
{
	// Basically if we have a collision type we ensure that we also have valid data for that type
	bfEnsure(ActivationInfo.ProjectileCollisionShape.CollisionShapeType == EBFCollisionShapeType::NoCollisionShape ||
		(ActivationInfo.ProjectileCollisionShape.CollisionProfile.Name != NAME_None &&
		!ActivationInfo.ProjectileCollisionShape.ShapeParams.IsNearlyZero()));

	bool bUpdatedRoot = false;
	
	/** The main purpose here is to reuse the existing component when possible and
	 * when its not because this time around being activated we have a different collision
	 * shape or none at all, then I need to handle setting up this projectile. */
	switch(ActivationInfo.ProjectileCollisionShape.CollisionShapeType)
	{
		case EBFCollisionShapeType::Box:
		{
			UBoxComponent* Comp = nullptr;
			if(!IsValid(RootComponent) || !RootComponent->IsA<UBoxComponent>())
			{
				Comp = BF::OP::NewComponent<UBoxComponent>(this, nullptr, "BoxCollisionComponent");
				bUpdatedRoot = true;
			}
			else
				Comp = CastChecked<UBoxComponent>(RootComponent);	

			Comp->SetBoxExtent(ActivationInfo.ProjectileCollisionShape.ShapeParams);
			Comp->SetCollisionProfileName(ActivationInfo.ProjectileCollisionShape.CollisionProfile.Name);
			Comp->SetGenerateOverlapEvents(true);
			Comp->OnComponentHit.AddDynamic(this, &ABFPoolableProjectileActor::OnProjectileActorHit);
			Comp->OnComponentBeginOverlap.AddDynamic(this, &ABFPoolableProjectileActor::OnProjectileActorOverlap);

			if(bUpdatedRoot)
			{
				Comp->SetWorldTransform(GetActorTransform());

				UActorComponent* OldComp = RootComponent;
				SetRootComponent(Comp);
				if(OldComp)
					OldComp->DestroyComponent();

				ProjectileMovementComponent->SetUpdatedComponent(Comp);
			}
			break;
		}
		case EBFCollisionShapeType::Sphere:
		{
			USphereComponent* Comp = nullptr;
			if(!IsValid(RootComponent) || !RootComponent->IsA<USphereComponent>())
			{
				Comp = BF::OP::NewComponent<USphereComponent>(this, nullptr, "SphereCollisionComponent");
				bUpdatedRoot = true;
			}
			else
				Comp = CastChecked<USphereComponent>(RootComponent);
				

			Comp->SetSphereRadius(ActivationInfo.ProjectileCollisionShape.ShapeParams.X);
			Comp->SetCollisionProfileName(ActivationInfo.ProjectileCollisionShape.CollisionProfile.Name);
			Comp->SetGenerateOverlapEvents(true);
			Comp->OnComponentHit.AddDynamic(this, &ABFPoolableProjectileActor::OnProjectileActorHit);
			Comp->OnComponentBeginOverlap.AddDynamic(this, &ABFPoolableProjectileActor::OnProjectileActorOverlap);

			if(bUpdatedRoot)
			{
				Comp->SetWorldTransform(GetActorTransform());
				UActorComponent* OldComp = RootComponent;
				SetRootComponent(Comp);
				if(OldComp)
					OldComp->DestroyComponent();
				ProjectileMovementComponent->SetUpdatedComponent(Comp);
			}
			break;
		}
		case EBFCollisionShapeType::Capsule:
		{
			UCapsuleComponent* Comp = nullptr;
			if(!IsValid(RootComponent) || !RootComponent->IsA<UCapsuleComponent>())
			{
				bUpdatedRoot = true;
				Comp = BF::OP::NewComponent<UCapsuleComponent>(this, nullptr, "CapsuleCollisionComponent");
			}
			else
				Comp = CastChecked<UCapsuleComponent>(RootComponent);

			Comp->SetCapsuleSize(ActivationInfo.ProjectileCollisionShape.ShapeParams.X, ActivationInfo.ProjectileCollisionShape.ShapeParams.Y);
			Comp->SetCollisionProfileName(ActivationInfo.ProjectileCollisionShape.CollisionProfile.Name);
			Comp->SetGenerateOverlapEvents(true);
			Comp->OnComponentHit.AddDynamic(this, &ABFPoolableProjectileActor::OnProjectileActorHit);
			Comp->OnComponentBeginOverlap.AddDynamic(this, &ABFPoolableProjectileActor::OnProjectileActorOverlap);

			if(bUpdatedRoot)
			{
				Comp->SetWorldTransform(GetActorTransform());
				UActorComponent* OldComp = RootComponent;
				SetRootComponent(Comp);
				if(OldComp)
					OldComp->DestroyComponent();
				ProjectileMovementComponent->SetUpdatedComponent(Comp);
			}
			break;
		}
		case EBFCollisionShapeType::NoCollisionShape:
			{
				if(!IsValid(RootComponent) || RootComponent->IsA<USphereComponent>() || RootComponent->IsA<UBoxComponent>() || RootComponent->IsA<UCapsuleComponent>())
				{
					USceneComponent* Comp = BF::OP::NewComponent<USceneComponent>(this, nullptr, "SceneRootComponent");
					Comp->SetWorldTransform(GetActorTransform());
					UActorComponent* OldComp = RootComponent;
					SetRootComponent(Comp);
					if(OldComp)
						OldComp->DestroyComponent();
					ProjectileMovementComponent->SetUpdatedComponent(Comp);
					bUpdatedRoot = true;
				}
				break;
			}
	}

	
	// Handle Component attachment, no need to pay transform updates if we aren't using the component.
	if(IsValid(ActivationInfo.ProjectileMesh.Mesh))
	{
		if(!OptionalStaticMeshComponent)
			OptionalStaticMeshComponent = BF::OP::NewComponent<UStaticMeshComponent>(this, nullptr, "StaticMeshComponent");

		OptionalStaticMeshComponent->SetSimulatePhysics(false);
		OptionalStaticMeshComponent->SetVisibility(true);
		OptionalStaticMeshComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::SnapToTargetIncludingScale);
		OptionalStaticMeshComponent->SetStaticMesh(ActivationInfo.ProjectileMesh.Mesh);
		
		for(const auto& [Material, Slot] : ActivationInfo.ProjectileMesh.Materials)
			OptionalStaticMeshComponent->SetMaterial(Slot, Material);

		OptionalStaticMeshComponent->SetRelativeTransform(ActivationInfo.ProjectileMesh.RelativeTransform);
	}
	else if(OptionalStaticMeshComponent) // We did need the component at one stage but no more lets hide it
	{
		OptionalStaticMeshComponent->SetVisibility(false);
		OptionalStaticMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		OptionalStaticMeshComponent->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform); 
	}

	
	// Same thought process as above.
	if(IsValid(ActivationInfo.NiagaraSystem))
	{
		if(!OptionalNiagaraComponent) // We never create one unless we need it, after that point we just toggle it on and off basically.
			OptionalNiagaraComponent = BF::OP::NewComponent<UNiagaraComponent>(this, nullptr, "NiagaraComponent");

		// Internally does nothing if we are already attached so no need to check.
		if(ActivationInfo.NiagaraSystemAttachmentSocketName != NAME_None && OptionalStaticMeshComponent)
			OptionalNiagaraComponent->AttachToComponent(OptionalStaticMeshComponent, FAttachmentTransformRules::SnapToTargetIncludingScale, ActivationInfo.NiagaraSystemAttachmentSocketName);
		else
			OptionalNiagaraComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::SnapToTargetIncludingScale);
		
		OptionalNiagaraComponent->SetComponentTickEnabled(true);
		OptionalNiagaraComponent->SetRelativeTransform(ActivationInfo.NiagaraSystemRelativeTransform);
		OptionalNiagaraComponent->SetAsset(ActivationInfo.NiagaraSystem);
		OptionalNiagaraComponent->Activate();
	}
	else if(OptionalNiagaraComponent)
	{
		OptionalNiagaraComponent->Deactivate();
		OptionalNiagaraComponent->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
	}

	return bUpdatedRoot;
}

void ABFPoolableProjectileActor::SetCurfew(float SecondsUntilReturn)
{
	if(SecondsUntilReturn > 0.f)
	{
		RemoveCurfew();
		GetWorld()->GetTimerManager().SetTimer(CurfewTimerHandle, this, &ABFPoolableProjectileActor::OnCurfewExpired, SecondsUntilReturn, false);
	}
}


void ABFPoolableProjectileActor::RemoveCurfew()
{
	if(GetWorld()->GetTimerManager().IsTimerActive(CurfewTimerHandle))
		GetWorld()->GetTimerManager().ClearTimer(CurfewTimerHandle);

	CurfewTimerHandle.Invalidate();
}


void ABFPoolableProjectileActor::OnCurfewExpired()
{
	ReturnToPool();
}


void ABFPoolableProjectileActor::OnProjectileStopped_Implementation(const FHitResult& HitResult)
{
	ActivationInfo.OnProjectileStoppedDelegate.ExecuteIfBound(HitResult);

	if(ActivationInfo.bShouldReturnOnStop)
	{
		ReturnToPool();
	}
	else if (ActivationInfo.bShouldDisableCollisionOnStop)
	{
		// Otherwise we have invisible collision blocking things.
		if (auto* Shape = Cast<UShapeComponent>(RootComponent))
		{
			Shape->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}

		if (!ActivationInfo.bShouldMeshSimulatePhysicsOnImpact && OptionalStaticMeshComponent)
		{
			OptionalStaticMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
}


void ABFPoolableProjectileActor::OnProjectileActorHit_Implementation(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (bHandledOverlapOrCollision || (ActivationInfo.bIgnoreCollisionWithOtherProjectiles && OtherActor->IsA<ABFPoolableProjectileActor>()))
		return;
	
	ActivationInfo.OnProjectileHitOrOverlapDelegate.ExecuteIfBound(Hit, false);

	if(ActivationInfo.bShouldReturnOnImpact)
	{
		ReturnToPool();
	}
	else if(ActivationInfo.bShouldMeshSimulatePhysicsOnImpact && OptionalStaticMeshComponent)
	{
		// Use the scene comps collision when wanting to simulate physics.
		OptionalStaticMeshComponent->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
		OptionalStaticMeshComponent->SetCollisionProfileName(ActivationInfo.ProjectileMesh.CollisionProfile.Name);
		OptionalStaticMeshComponent->SetSimulatePhysics(true);

		// Disable collision for the shape component otherwise we have an invisible shape blocking things.
		if(auto* Shape = Cast<UShapeComponent>(RootComponent))
		{
			Shape->SetCollisionEnabled(ECollisionEnabled::NoCollision); 
		}

		ProjectileMovementComponent->StopMovementImmediately();
	}
	
	bHandledOverlapOrCollision = true;
}


void ABFPoolableProjectileActor::OnProjectileActorOverlap_Implementation(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bHandledOverlapOrCollision || (ActivationInfo.bIgnoreCollisionWithOtherProjectiles && OtherActor->IsA<ABFPoolableProjectileActor>()))
		return;
	
	ActivationInfo.OnProjectileHitOrOverlapDelegate.ExecuteIfBound(SweepResult, true);
	
	if(ActivationInfo.bShouldReturnOnImpact)
	{
		ReturnToPool();
	}
	else if(ActivationInfo.bShouldMeshSimulatePhysicsOnImpact && OptionalStaticMeshComponent)
	{
		// Use the scene comps collision when wanting to simulate physics.
		OptionalStaticMeshComponent->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
		OptionalStaticMeshComponent->SetCollisionProfileName(ActivationInfo.ProjectileMesh.CollisionProfile.Name);
		OptionalStaticMeshComponent->SetSimulatePhysics(true);

		// Disable collision for the shape component otherwise we have an invisible shape blocking things.
		if(auto* Shape = Cast<UShapeComponent>(RootComponent))
		{
			Shape->SetCollisionEnabled(ECollisionEnabled::NoCollision); 
		}
		
		ProjectileMovementComponent->StopMovementImmediately();
	}
	bHandledOverlapOrCollision = true;
}
