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

	RootComponent = CreateDefaultSubobject<USceneComponent>("Root");
	AudioComponent = CreateDefaultSubobject<UAudioComponent>("AudioComponent");
	AudioComponent->SetupAttachment( RootComponent);
} 


void ABFPoolableSoundActor::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	AudioComponent->OnAudioFinished.AddDynamic(this, &ABFPoolableSoundActor::OnSoundFinished);
}


void ABFPoolableSoundActor::FireAndForgetBP(FBFPooledObjectHandleBP& Handle,
	const FBFPoolableSoundActorDescription& ActivationParams, const FTransform& ActorTransform)
{
	bfEnsure(Handle.Handle.IsValid() && Handle.Handle->IsHandleValid()); // You must have a valid handle.
	bfValid(ActivationParams.Sound); // you must set the system

	SetPoolHandleBP(Handle);
	SetPoolableActorParams(ActivationParams);
	SetActorTransform(ActorTransform);

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
	bfEnsure(Handle.IsValid() && Handle->IsHandleValid()); // You must have a valid handle.
	bfValid(ActivationParams.Sound); // you must set the system


	SetPoolHandle(Handle);
	SetPoolableActorParams(ActivationParams);
	SetActorTransform(ActorTransform);
	
	if(ActivationInfo.ActorCurfew > 0)
		SetCurfew(ActivationInfo.ActorCurfew);
	
	if(ActivationInfo.DelayedActivationTimeSeconds > KINDA_SMALL_NUMBER)
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


void ABFPoolableSoundActor::SetPoolableActorParams(const FBFPoolableSoundActorDescription& ActivationParams)
{
	ActivationInfo = ActivationParams;
}


void ABFPoolableSoundActor::ActivatePoolableActor()
{
	SetupObjectState();
	bfEnsure(AudioComponent->Sound); // You must set the sound manually or via SetPoolableActorParams.
	bfEnsure(ActivationInfo.StartingTimeOffset <= AudioComponent->Sound->GetDuration()); // Trying to start the sound with a time greater than the sound duration.
	bHasSoundFinished = false;
	StartTime = GetWorld()->GetTimeSeconds();
	
	if(ActivationInfo.FadeInTime < 0)
		AudioComponent->Play(ActivationInfo.StartingTimeOffset); // SetupObjectState will trigger start via FadeIn if we are using that so don't play here.
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

	if(ActivationInfo.FadeInTime > 0)
		AudioComponent->FadeIn(ActivationInfo.FadeInTime, ActivationInfo.VolumeMultiplier, ActivationInfo.StartingTimeOffset, (EAudioFaderCurve)ActivationInfo.FadeInCurve);
}


// If successful this leads to the interface call OnObjectPooled.
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
	Handle.Reset(); // Clear the handle so it can't be used again.
}


void ABFPoolableSoundActor::SetPoolHandle( TBFPooledObjectHandlePtr<ABFPoolableSoundActor, ESPMode::NotThreadSafe>& Handle)
{
	bfEnsure(BPObjectHandle == nullptr); // You can't have both handles set.
	bfEnsure(Handle.IsValid() && Handle->IsHandleValid()); // You must have a valid handle.
	bIsUsingBPHandle = false;
	ObjectHandle = Handle;
	Handle.Reset(); // Clear the handle so it can't be used again.
}


void ABFPoolableSoundActor::FellOutOfWorld(const UDamageType& DmgType)
{
	// Super::FellOutOfWorld(DmgType); do not want default behaviour here.
#if !UE_BUILD_SHIPPING
	if(BF::OP::CVarObjectPoolEnableLogging.GetValueOnGameThread() == true)
		UE_LOGFMT(LogTemp, Warning, "{0} Fell out of map, auto returning to pool.", GetName());
#endif
	// Very unlikely a sound would ever simulate and fall out of the world but I want to ensure we cant leak.
	ReturnToPool();
}


void ABFPoolableSoundActor::SetCurfew(float SecondsUntilReturn, bool bShouldWaitForSoundFinishBeforeCurfew)
{
	bfEnsure(SecondsUntilReturn > 0);

	// Update flag internally so OnCurfewExpired knows to wait for sound finish.
	bWaitForSoundFinishBeforeCurfew = bShouldWaitForSoundFinishBeforeCurfew;
	
	RemoveCurfew();
	GetWorld()->GetTimerManager().SetTimer( CurfewTimerHandle, this, &ABFPoolableSoundActor::OnCurfewExpired, SecondsUntilReturn, false );
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
			AudioComponent->FadeOut(ActivationInfo.FadeOutTime, 0.f, (EAudioFaderCurve)ActivationInfo.FadeInCurve);
			return;
		}

		if (bWaitForSoundFinishBeforeCurfew)
		{
			float RemainingDuration = GetWorld()->GetTimeSeconds() - StartTime + AudioComponent->GetSound()->GetDuration();
			if(RemainingDuration > 0.05)
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
	AudioComponent->OnAudioFinished.RemoveAll(this);
}













