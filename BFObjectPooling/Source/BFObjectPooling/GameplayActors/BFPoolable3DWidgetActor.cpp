// Copyright (c) 2024 Jack Holland 
// Licensed under the MIT License. See LICENSE.md file in repo root for full license information.

#include "BFPoolable3DWidgetActor.h"
#include "BFObjectPooling/PoolBP/BFPooledObjectHandleBP.h"
#include "Components/WidgetComponent.h"


ABFPoolable3DWidgetActor::ABFPoolable3DWidgetActor(const FObjectInitializer& ObjectInitializer)
	: Super( ObjectInitializer )
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>("RootComponent");
	WidgetComponent = CreateDefaultSubobject<UWidgetComponent>("WidgetComponent");
	WidgetComponent->SetupAttachment(RootComponent);
}


void ABFPoolable3DWidgetActor::OnObjectPooled_Implementation()
{
	RemoveCurfew();
	
	ObjectHandle = nullptr;
	BPObjectHandle = nullptr;

	WidgetComponent->SetTickMode(ETickMode::Disabled);
	WidgetComponent->SetWidgetSpace(EWidgetSpace::World);
	WidgetComponent->SetVisibility(false);
	WidgetComponent->UpdateWidget();
	ActivationInfo = {};
}


void ABFPoolable3DWidgetActor::Tick(float Dt)
{
	Super::Tick(Dt);
	
	if(IsValid(ActivationInfo.TargetComponent) && ActivationInfo.WidgetSpace != EBFWidgetSpace::Screen) // SS doesnt require this.
	{
		FRotator Rot = (ActivationInfo.TargetComponent->GetComponentLocation() - GetActorLocation()).Rotation();
		WidgetComponent->SetWorldRotation(Rot);
	}

	float Curfew = ActivationInfo.ActorCurfew;
	UCurveVector4* Curve = ActivationInfo.WidgetLifetimePositionAndSizeCurve;
	if(Curfew > 0 && Curve) // Can only do this if know its lifetime so we can normalize and sample.
	{
		float NormalizedTimeAlive = FMath::Clamp( ( GetWorld()->GetTimeSeconds() - StartingTime) / Curfew, 0.f, 1.f);
		FVector4 CurveValue = Curve->GetVectorValue(NormalizedTimeAlive);
		FVector PosOffset = FVector{CurveValue.X, CurveValue.Y, CurveValue.Z};
		PosOffset = ActivationInfo.bInvertWidgetCurve ? -PosOffset : PosOffset; 
		WidgetComponent->SetWorldLocation(AbsoluteStartLocation + WidgetComponent->GetComponentTransform().TransformVector(PosOffset));// Ensure our local space defined curve offset is sampled in WS
		WidgetComponent->SetDrawSize(ActivationInfo.DrawSize * CurveValue.W);
	}
}


void ABFPoolable3DWidgetActor::FireAndForgetBP(FBFPooledObjectHandleBP& Handle,
	const FBFPoolable3DWidgetActorDescription& ActivationParams, const FTransform& ActorTransform)
{
	bfEnsure(Handle.Handle.IsValid()); // You must have a valid handle.
	bfEnsure(Handle.Handle.IsValid() && Handle.Handle->IsHandleValid()); // You must have a valid handle.
	
	SetPoolHandleBP(Handle);
	ActivationInfo = ActivationParams;
	
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
	bfEnsure(Handle.IsValid()); // You must have a valid handle.
	bfEnsure(Handle.IsValid() && Handle->IsHandleValid()); // You must have a valid handle.
	
	SetPoolHandle(Handle);
	ActivationInfo = ActivationParams;
	
	if(ActivationInfo.ActorCurfew > 0)
		SetCurfew(ActivationInfo.ActorCurfew);

	SetActorHiddenInGame(false);
	SetActorTickEnabled(true);
	
	SetActorTransform(ActorTransform);
	ActivatePoolableActor();
}


void ABFPoolable3DWidgetActor::SetPoolHandleBP(FBFPooledObjectHandleBP& Handle)
{
	bfEnsure(ObjectHandle == nullptr); // You can't have both handles set.
	bfEnsure(Handle.Handle.IsValid() && Handle.Handle->IsHandleValid()); // You must have a valid handle.
	bIsUsingBPHandle = true;
	BPObjectHandle = Handle.Handle;
	Handle.Reset(); // Clear the handle so it can't be used again.
}


void ABFPoolable3DWidgetActor::SetPoolHandle(TBFPooledObjectHandlePtr<ABFPoolable3DWidgetActor, ESPMode::NotThreadSafe>& Handle)
{
	bfEnsure(BPObjectHandle == nullptr); // You can't have both handles set.
	bfEnsure(Handle.IsValid() && Handle->IsHandleValid()); // You must have a valid handle.
	bIsUsingBPHandle = false;
	ObjectHandle = Handle;
	Handle.Reset(); // Clear the handle so it can't be used again.
}


void ABFPoolable3DWidgetActor::SetPoolableActorParams( const FBFPoolable3DWidgetActorDescription& ActivationParams)
{
	ActivationInfo = ActivationParams;
}


void ABFPoolable3DWidgetActor::ActivatePoolableActor()
{
	SetupObjectState();
	AbsoluteStartLocation = GetActorLocation();
	WidgetComponent->SetWorldLocation(AbsoluteStartLocation); // In case using curve to drive position, ensure we snap back.
	bfValid(WidgetComponent->GetWidget());
}



void ABFPoolable3DWidgetActor::SetCurfew(float SecondsUntilReturn)
{
	bfEnsure(SecondsUntilReturn > 0);
	RemoveCurfew();
	GetWorld()->GetTimerManager().SetTimer(CurfewTimerHandle, this, &ABFPoolable3DWidgetActor::OnCurfewExpired, SecondsUntilReturn, false);
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


void ABFPoolable3DWidgetActor::SetupObjectState()
{
	// Create new only if they differ or if we don't have a widget.
	if(!WidgetComponent->GetWidget() || WidgetComponent->GetWidget()->GetClass() != ActivationInfo.WidgetClass)
		WidgetComponent->SetWidget(CreateWidget<UUserWidget>(GetWorld(), ActivationInfo.WidgetClass));

	WidgetComponent->bCastFarShadow = ActivationInfo.bShouldCastShadow;
	WidgetComponent->SetVisibility(true);
	WidgetComponent->SetTickMode(ETickMode::Enabled);
	WidgetComponent->SetWidgetSpace((EWidgetSpace)ActivationInfo.WidgetSpace);
	
	WidgetComponent->SetTintColorAndOpacity(ActivationInfo.WidgetTintAndOpacity);
	WidgetComponent->SetTickWhenOffscreen(ActivationInfo.bShouldTickWhenOffscreen);
	WidgetComponent->SetTickableWhenPaused(ActivationInfo.bTickableWhenPaused);
	WidgetComponent->SetDrawSize(ActivationInfo.DrawSize);
	WidgetComponent->SetTwoSided(ActivationInfo.bTwoSided);
	
	WidgetComponent->UpdateWidget();

	StartingTime = GetWorld()->GetTimeSeconds();
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
