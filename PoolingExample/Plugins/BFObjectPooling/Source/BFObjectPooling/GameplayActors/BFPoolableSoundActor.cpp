// Copyright (c) 2024 Jack Holland 
// Licensed under the MIT License. See LICENSE.md file in repo root for full license information.

#include "BFPoolableSoundActor.h"
#include "BFObjectPooling/Pool/Private/BFObjectPoolHelpers.h"
#include "BFObjectPooling/PoolBP/BFPooledObjectHandleBP.h"


ABFPoolableSoundActor::ABFPoolableSoundActor(const FObjectInitializer& ObjectInitializer)
	: Super( ObjectInitializer )
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	static FName RootComponentName{"RootComponent"};
	RootComponent = CreateDefaultSubobject<USceneComponent>(RootComponentName);

	static FName AudioComponentName{"AudioComponent"};
	AudioComponent = CreateDefaultSubobject<UAudioComponent>(AudioComponentName);
	AudioComponent->SetupAttachment( RootComponent);
} 


void ABFPoolableSoundActor::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (GetWorld()->IsGameWorld())
	{
		AudioComponent->OnAudioFinished.AddDynamic(this, &ABFPoolableSoundActor::OnSoundFinished);
	}
}


void ABFPoolableSoundActor::FireAndForgetBP(FBFPooledObjectHandleBP& Handle,
	const FBFPoolableSoundActorDescription& ActivationParams, const FTransform& ActorTransform)
{
	// You must pass a valid handle otherwise it defeats the purpose of this function.
	check(Handle.Handle.IsValid() && Handle.Handle->IsHandleValid());
	if (ActivationParams.Sound == nullptr || ActivationParams.StartingTimeOffset >= ActivationParams.Sound->GetDuration())
	{
		if (ActivationParams.Sound == nullptr)
			UE_LOGFMT(LogTemp, Warning, "PoolableSoundActor was handed a null sound asset to play, was this intentional?");
		else
			UE_LOGFMT(LogTemp, Warning, "PoolableSoundActor was handed a starting time offset greater than the sound duration, was this intentional?");
		
		Handle.Handle->ReturnToPool();
		return;
	}

#if !UE_BUILD_SHIPPING
	if (ActivationParams.Sound->IsLooping() && ActivationInfo.ActorCurfew <= 0)
	{
		UE_LOGFMT(LogTemp, Warning, "PoolableSoundActor was handed a looping sound {0} but no curfew was set, was this intentional, because it will never return unless you explicitly force it from the handle.",
			ActivationParams.Sound->GetName());
	}
#endif

	SetPoolHandleBP(Handle);
	SetPoolableActorParams(ActivationParams);
	SetActorTransform(ActorTransform);

	
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
	

	// Ensure the curfew accounts for the delayed activation time if set.
	if(ActivationInfo.ActorCurfew > 0)
	{
		if(ActivationInfo.DelayedActivationTimeSeconds > 0)
			ActivationInfo.ActorCurfew += ActivationInfo.DelayedActivationTimeSeconds;
		
		SetCurfew(ActivationInfo.ActorCurfew);
	}

	if(ActivationInfo.DelayedActivationTimeSeconds > 0)
	{
		FTimerHandle TimerHandle;
		FTimerDelegate TimerDel;
		TimerDel.BindUObject(this, &ABFPoolableSoundActor::ActivatePoolableActor);
		GetWorld()->GetTimerManager().SetTimer(TimerHandle, TimerDel, ActivationInfo.DelayedActivationTimeSeconds, false);
	}
	else // Start it now
	{
		ActivatePoolableActor();
	}
}



void ABFPoolableSoundActor::FireAndForget(
	TBFPooledObjectHandlePtr<ABFPoolableSoundActor, ESPMode::NotThreadSafe>& Handle,
	const FBFPoolableSoundActorDescription& ActivationParams, const FTransform& ActorTransform)
{
	// You must pass a valid handle otherwise it defeats the purpose of this function.
	check(Handle.IsValid() && Handle->IsHandleValid());
	if (ActivationParams.Sound == nullptr || ActivationParams.StartingTimeOffset >= ActivationParams.Sound->GetDuration())
	{
		if (ActivationParams.Sound == nullptr)
			UE_LOGFMT(LogTemp, Warning, "PoolableSoundActor was handed a null sound asset to play, was this intentional?");
		else
			UE_LOGFMT(LogTemp, Warning, "PoolableSoundActor was handed a starting time offset greater than the sound duration, was this intentional?");
		
		Handle->ReturnToPool();
		return;
	}

#if !UE_BUILD_SHIPPING
	if (ActivationParams.Sound->IsLooping() && ActivationInfo.ActorCurfew <= 0)
	{
		UE_LOGFMT(LogTemp, Warning, "PoolableSoundActor was handed a looping sound {0} but no curfew was set, was this intentional, because it will never return unless you explicitly force it from the handle.",
			ActivationParams.Sound->GetName());
	}
#endif

	SetPoolHandle(Handle);
	SetPoolableActorParams(ActivationParams);
	SetActorTransform(ActorTransform);

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
	
	if(ActivationInfo.DelayedActivationTimeSeconds > KINDA_SMALL_NUMBER)
	{
		FTimerHandle TimerHandle;
		FTimerDelegate TimerDel = FTimerDelegate::CreateUObject(this, &ABFPoolableSoundActor::ActivatePoolableActor);
		GetWorld()->GetTimerManager().SetTimer(TimerHandle, TimerDel, ActivationInfo.DelayedActivationTimeSeconds, false);
	}
	else // Start it now
	{
		ActivatePoolableActor();
	}
}


void ABFPoolableSoundActor::SetPoolableActorParams(const FBFPoolableSoundActorDescription& ActivationParams)
{
	ActivationInfo = ActivationParams;
}


void ABFPoolableSoundActor::ActivatePoolableActor()
{
	SetupObjectState();
	bHasSoundFinished = false;
	StartTime = GetWorld()->GetTimeSeconds();
	
	if(ActivationInfo.FadeInTime <= 0.f)
	{
		AudioComponent->Play(ActivationInfo.StartingTimeOffset); // SetupObjectState will trigger start via FadeIn if we are using that so don't play here.

		// Sometimes if we have too many sound instances in the world, this can cause us to not play the sound,
		// we handle that here otherwise it will never return to the pool.
		if (!AudioComponent->IsPlaying())
		{
			UE_LOGFMT(LogTemp, Warning, "Failed to play sound, my be due to having too many instances of it in the world, returning to pool.");
			OnSoundFinished();
		}
	}
}


void ABFPoolableSoundActor::SetupObjectState()
{
	bfEnsure(ActivationInfo.Sound); // You must set the sound manually or if you intend to use this method you must set the sound via SetPoolableActorParams.
	AudioComponent->SetSound( ActivationInfo.Sound );
	AudioComponent->SetVolumeMultiplier(ActivationInfo.VolumeMultiplier);
	AudioComponent->SetPitchMultiplier(ActivationInfo.PitchMultiplier);
	AudioComponent->AdjustAttenuation(ActivationInfo.AttenuationSettings);
	AudioComponent->bReverb = ActivationInfo.bReverb;
	AudioComponent->SetUISound(ActivationInfo.bUISound);

	if(ActivationInfo.FadeInTime > 0.f)
		AudioComponent->FadeIn(ActivationInfo.FadeInTime, ActivationInfo.VolumeMultiplier, ActivationInfo.StartingTimeOffset, ActivationInfo.FadeInCurve);
}


bool ABFPoolableSoundActor::ReturnToPool()
{
	bHasSoundFinished = true;
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


void ABFPoolableSoundActor::OnObjectPooled_Implementation()
{
	// Reset everything.
	GetWorld()->GetTimerManager().ClearTimer(DelayedActivationTimerHandle);
	RemoveCurfew();

	if (bIsAttached)
	{
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		bIsAttached = false;
	}

	ObjectHandle.Reset();
	BPObjectHandle.Reset();
	bHasSoundFinished = false;
	bWaitForSoundFinishBeforeCurfew = false;

	StartTime = 0;
	ActivationInfo = {};

	if(AudioComponent)
		AudioComponent->Stop();
}

void ABFPoolableSoundActor::SetPoolHandleBP(FBFPooledObjectHandleBP& Handle)
{
	bfEnsure(ObjectHandle == nullptr); // You can't have both handles set.
	bfEnsure(Handle.Handle.IsValid() && Handle.Handle->IsHandleValid()); // You must have a valid handle.
	
	bIsUsingBPHandle = true;
	BPObjectHandle = Handle.Handle;
	Handle.Invalidate(); // Clear the handle so it can't be used again.
}


void ABFPoolableSoundActor::SetPoolHandle( TBFPooledObjectHandlePtr<ABFPoolableSoundActor, ESPMode::NotThreadSafe>& Handle)
{
	bfEnsure(BPObjectHandle == nullptr); // You can't have both handles set.
	bfEnsure(Handle.IsValid() && Handle->IsHandleValid()); // You must have a valid handle.
	
	bIsUsingBPHandle = false;
	ObjectHandle = Handle;
	Handle.Reset();
}


void ABFPoolableSoundActor::FellOutOfWorld(const UDamageType& DmgType)
{
	// Super::FellOutOfWorld(DmgType); do not want default behaviour here.
#if !UE_BUILD_SHIPPING
	if(BF::OP::CVarObjectPoolEnableLogging.GetValueOnGameThread() == true)
		UE_LOGFMT(LogTemp, Warning, "{0} Fell out of map, auto returning to pool.", GetName());
#endif
	// Very unlikely a sound would ever fall out of the world but I want to make sure we handle
	// that case because the alternative is the default behaviour which is to destroy the actor.
	ReturnToPool();
}


void ABFPoolableSoundActor::SetCurfew(float SecondsUntilReturn, bool bShouldWaitForSoundFinishBeforeCurfew)
{
	if(SecondsUntilReturn > 0)
	{
		// Update flag internally so OnCurfewExpired knows to wait for sound finish.
		bWaitForSoundFinishBeforeCurfew = bShouldWaitForSoundFinishBeforeCurfew;
		
		RemoveCurfew();
		GetWorld()->GetTimerManager().SetTimer( CurfewTimerHandle, this, &ABFPoolableSoundActor::OnCurfewExpired, SecondsUntilReturn, false );
	}
}


void ABFPoolableSoundActor::RemoveCurfew()
{
	if(GetWorld()->GetTimerManager().IsTimerActive(CurfewTimerHandle))
		GetWorld()->GetTimerManager().ClearTimer(CurfewTimerHandle);

	CurfewTimerHandle.Invalidate();
}


void ABFPoolableSoundActor::OnSoundFinished()
{
	bHasSoundFinished = true;
	ActivationInfo.OnSoundFinishedDelegate.ExecuteIfBound();

	// When fading out and using a curfew its implied we should return when the sound finishes, this is helpful because inside OnCurfewExpired
	// if using the fade out we starting fading and return which relies on this OnSoundFinished callback to now let us know to return. Tiny offset just in case.
	// Takes only Pause time into account not real time.
	if(GetAutoReturnOnSoundFinished() || (ActivationInfo.ActorCurfew > 0 && GetWorld()->GetTimeSeconds() > StartTime + ActivationInfo.ActorCurfew - 0.05))
		ReturnToPool();
	else
	{
		if(ActivationInfo.FadeOutTime > 0.f)
			AudioComponent->FadeOut(ActivationInfo.FadeOutTime, 0.f, ActivationInfo.FadeInCurve);
	}
}


USoundBase* ABFPoolableSoundActor::GetSound() const
{
	return ActivationInfo.Sound;
}


void ABFPoolableSoundActor::RestartSound()
{
	bHasSoundFinished = false;
	AudioComponent->Activate(true);
}


bool ABFPoolableSoundActor::IsSoundLooping() const
{
	return AudioComponent->GetSound()->IsLooping();
}


void ABFPoolableSoundActor::CancelDelayedActivationAndReturnToPool()
{
	// OnObjectPooled handles removing timers.
	ReturnToPool();
}


void ABFPoolableSoundActor::CancelDelayedActivation()
{
	if(IsActivationCurrentlyDelayed())
		GetWorld()->GetTimerManager().ClearTimer(DelayedActivationTimerHandle);
}


void ABFPoolableSoundActor::OnCurfewExpired()
{
	if(AudioComponent->IsPlaying())
	{
		if(ActivationInfo.FadeOutTime > 0.f)
		{
			AudioComponent->FadeOut(ActivationInfo.FadeOutTime, 0.f, ActivationInfo.FadeInCurve);
			return;
		}

		if (bWaitForSoundFinishBeforeCurfew)
		{
			float RemainingDuration = GetWorld()->GetTimeSeconds() - StartTime + AudioComponent->GetSound()->GetDuration();
			if(RemainingDuration > 0.05f)
			{
				FTimerDelegate TimerDelegate;
				TimerDelegate.BindUObject(this, &ABFPoolableSoundActor::OnCurfewExpired);
				GetWorld()->GetTimerManager().SetTimer(CurfewTimerHandle, TimerDelegate, RemainingDuration, false);
				return;	
			}
		}
	}
	ReturnToPool();
}


void ABFPoolableSoundActor::BeginDestroy()
{
	Super::BeginDestroy();
	if (AudioComponent)
		AudioComponent->OnAudioFinished.RemoveAll(this);
}













