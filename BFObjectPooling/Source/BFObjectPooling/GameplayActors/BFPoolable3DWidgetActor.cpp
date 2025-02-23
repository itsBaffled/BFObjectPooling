// Copyright (c) 2024 Jack Holland 
// Licensed under the MIT License. See LICENSE.md file in repo root for full license information.

#include "BFPoolable3DWidgetActor.h"
#include "Components/WidgetComponent.h"

#include "BFObjectPooling/PoolBP/BFPooledObjectHandleBP.h"


ABFPoolable3DWidgetActor::ABFPoolable3DWidgetActor(const FObjectInitializer& ObjectInitializer)
	: Super( ObjectInitializer )
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	static const FName RootComponentName{"RootComponent"};
	RootComponent = CreateDefaultSubobject<USceneComponent>(RootComponentName);

	static const FName WidgetComponentName{"WidgetComponent"};
	WidgetComponent = CreateDefaultSubobject<UWidgetComponent>(WidgetComponentName);
	WidgetComponent->SetupAttachment(RootComponent);
	WidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}


void ABFPoolable3DWidgetActor::Tick(float Dt)
{
	Super::Tick(Dt);
	
	if(IsValid(ActivationInfo.TargetComponent) && ActivationInfo.WidgetSpace != EWidgetSpace::Screen) // SS doesn't require this.
	{
		FRotator Rot = (ActivationInfo.TargetComponent->GetComponentLocation() - GetActorLocation()).Rotation();
		WidgetComponent->SetWorldRotation(Rot);
	}

	float Curfew = ActivationInfo.ActorCurfew;
	UCurveVector4* Curve = ActivationInfo.WidgetLifetimePositionAndSizeCurve;
	
	// Can only do this if know its lifetime so we can normalize and sample.
	if(Curfew > 0.f && IsValid(Curve))
	{
		float NormalizedTimeAlive = FMath::Clamp( ( GetWorld()->GetTimeSeconds() - StartingTime) / Curfew, 0.f, 1.f);
		FVector4 CurveValue = Curve->GetVectorValue(NormalizedTimeAlive);
		FVector PosOffset = FVector{CurveValue.X, CurveValue.Y, CurveValue.Z};
		PosOffset = ActivationInfo.bInvertWidgetCurve ? -PosOffset : PosOffset;
		
		WidgetComponent->SetRelativeLocation(PosOffset);
		WidgetComponent->SetDrawSize(ActivationInfo.DrawSize * FMath::Max(CurveValue.W, 0.05f));
	}
}



void ABFPoolable3DWidgetActor::FireAndForgetBP(FBFPooledObjectHandleBP& Handle,
	const FBFPoolable3DWidgetActorDescription& ActivationParams, const FTransform& ActorTransform)
{
	// You must pass a valid handle otherwise it defeats the purpose of this function.
	check(Handle.Handle.IsValid() && Handle.Handle->IsHandleValid());
#if !UE_BUILD_SHIPPING
	if (ActivationParams.WidgetClass == nullptr)
	{
		UE_LOGFMT(LogTemp, Warning, "PoolableWidgetActor was handed a null Widget class, was this intentional?");
	}
#endif

	
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
	
	if(ActivationInfo.ActorCurfew > 0)
		SetCurfew(ActivationInfo.ActorCurfew);

	SetActorHiddenInGame(false);
	SetActorTickEnabled(true);
	
	SetActorTransform(ActorTransform);
	ActivatePoolableActor();
}


void ABFPoolable3DWidgetActor::FireAndForget(TBFPooledObjectHandlePtr<ABFPoolable3DWidgetActor, ESPMode::NotThreadSafe>& Handle,
                                             const FBFPoolable3DWidgetActorDescription& ActivationParams, const FTransform& ActorTransform)
{
	// You must pass a valid handle otherwise it defeats the purpose of this function.
	check(Handle.IsValid() && Handle->IsHandleValid());
#if !UE_BUILD_SHIPPING
	if (ActivationParams.WidgetClass == nullptr)
	{
		UE_LOGFMT(LogTemp, Warning, "PoolableWidgetActor was handed a null Widget class, was this intentional?");
	}
#endif
	
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
	
	if(ActivationInfo.ActorCurfew > 0.f)
		SetCurfew(ActivationInfo.ActorCurfew);

	SetActorHiddenInGame(false);
	SetActorTickEnabled(true);
	
	SetActorTransform(ActorTransform);
	ActivatePoolableActor();
}


void ABFPoolable3DWidgetActor::SetPoolableActorParams( const FBFPoolable3DWidgetActorDescription& ActivationParams)
{
	ActivationInfo = ActivationParams;
}

void ABFPoolable3DWidgetActor::SetupObjectState()
{
	// Create new only if they differ or if we don't have a widget.
	if(!WidgetComponent->GetWidget() || WidgetComponent->GetWidget()->GetClass() != ActivationInfo.WidgetClass)
		WidgetComponent->SetWidget(CreateWidget<UUserWidget>(GetWorld(), ActivationInfo.WidgetClass));

	WidgetComponent->bCastFarShadow = ActivationInfo.bShouldCastShadow;
	WidgetComponent->SetVisibility(true);
	WidgetComponent->SetTickMode(ETickMode::Enabled);
	WidgetComponent->SetWidgetSpace(ActivationInfo.WidgetSpace);
	
	WidgetComponent->SetTintColorAndOpacity(ActivationInfo.WidgetTintAndOpacity);
	WidgetComponent->SetTickWhenOffscreen(ActivationInfo.bShouldTickWhenOffscreen);
	WidgetComponent->SetTickableWhenPaused(ActivationInfo.bTickableWhenPaused);
	WidgetComponent->SetDrawSize(ActivationInfo.DrawSize);
	WidgetComponent->SetTwoSided(ActivationInfo.bTwoSided);
	
	WidgetComponent->UpdateWidget();
	StartingTime = GetWorld()->GetTimeSeconds();
}


void ABFPoolable3DWidgetActor::ActivatePoolableActor()
{
	SetupObjectState();
	WidgetComponent->SetRelativeLocation(FVector::ZeroVector); // In case using curve to drive position, ensure we snap back.
}


bool ABFPoolable3DWidgetActor::ReturnToPool()
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


void ABFPoolable3DWidgetActor::OnObjectPooled_Implementation()
{
	RemoveCurfew();

	if (bIsAttached)
	{
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		bIsAttached = false;
	}
	
	ObjectHandle = nullptr;
	BPObjectHandle = nullptr;

	WidgetComponent->SetVisibility(false);
	WidgetComponent->SetTickMode(ETickMode::Disabled);
	WidgetComponent->UpdateWidget();
	ActivationInfo = {};
}


void ABFPoolable3DWidgetActor::SetPoolHandleBP(FBFPooledObjectHandleBP& Handle)
{
	bfEnsure(ObjectHandle == nullptr); // You can't have both handles set.
	bfEnsure(Handle.Handle.IsValid() && Handle.Handle->IsHandleValid()); // You must have a valid handle.
	
	bIsUsingBPHandle = true;
	BPObjectHandle = Handle.Handle;
	Handle.Invalidate(); // Invalidate the handle so it can't be used again.
}


void ABFPoolable3DWidgetActor::SetPoolHandle(TBFPooledObjectHandlePtr<ABFPoolable3DWidgetActor, ESPMode::NotThreadSafe>& Handle)
{
	bfEnsure(BPObjectHandle == nullptr); // You can't have both handles set.
	bfEnsure(Handle.IsValid() && Handle->IsHandleValid()); // You must have a valid handle.
	
	bIsUsingBPHandle = false;
	ObjectHandle = Handle;
	Handle.Reset(); // Clear the handle so it can't be used again.
}


void ABFPoolable3DWidgetActor::FellOutOfWorld(const UDamageType& DmgType)
{
	// Super::FellOutOfWorld(DmgType); do not want default behaviour here.
#if !UE_BUILD_SHIPPING
	if(BF::OP::CVarObjectPoolEnableLogging.GetValueOnGameThread() == true)
		UE_LOGFMT(LogTemp, Warning, "{0} Fell out of map, auto returning to pool.", GetName());
#endif
	ReturnToPool();
}


void ABFPoolable3DWidgetActor::SetCurfew(float SecondsUntilReturn)
{
	if(SecondsUntilReturn > 0.f)
	{
		RemoveCurfew();
		GetWorld()->GetTimerManager().SetTimer(CurfewTimerHandle, this, &ABFPoolable3DWidgetActor::OnCurfewExpired, SecondsUntilReturn, false);
	}
}


void ABFPoolable3DWidgetActor::RemoveCurfew()
{
	if(GetWorld()->GetTimerManager().IsTimerActive(CurfewTimerHandle))
		GetWorld()->GetTimerManager().ClearTimer(CurfewTimerHandle);
	
	CurfewTimerHandle.Invalidate();
}


void ABFPoolable3DWidgetActor::OnCurfewExpired()
{
	ReturnToPool();
}


