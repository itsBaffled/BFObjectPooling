// Copyright (c) 2024 Jack Holland 
// Licensed under the MIT License. See LICENSE.md file in repo root for full license information.

#pragma once
#include "BFObjectPooling/Interfaces/BFPooledObjectInterface.h"
#include "BFObjectPooling/Pool/BFObjectPool.h"
#include "BFObjectPooling/Pool/BFPooledObjectHandle.h"
#include "Components/AudioComponent.h"
#include "GameFramework/Actor.h"
#include "BFPoolableActorHelpers.h"
#include "BFPoolableSoundActor.generated.h"



/** All built in poolable actors have 3 main paths pre planned out for them and is typically how I expect most poolable actors to be used.
 *
 * - QuickUnpoolSoundActor: This is the fastest and simplest way for BP to unpool these actors, the pool must be of this type and the actor must be of this type and thats more or less it, just init the pool and call QuickUnpoolSoundActor.
 *
 * - FireAndForget: This is the simplest way to use a poolable actor from c++ (BP also has access), you just init the pool, get a pooled actor and cast it to this type and then call FireAndForget passing it the
 * 		activation params and a transform and it will handle the rest by returning after the conditional return options are met.
 *		(This is your ActorCurfew value or any other kind of auto return logic the poolable actor supplies).
 *
 * - Manual Control: You can manually control the poolable actor, you are responsible for acquiring the actors handle (like normal but you hang on to it until it's ready to return) and setting its params via SetPoolableActorParams() 
 * 		or any other way you see fit to setup your object. */


/** A generic poolable sound actor that already implements the IBFPooledObjectInterface and can be used for various situations involving sounds. */
UCLASS(meta=(DisplayName="BF Poolable Sound Actor"))
class BFOBJECTPOOLING_API ABFPoolableSoundActor : public AActor, public IBFPooledObjectInterface
{
	GENERATED_BODY()

public:
	ABFPoolableSoundActor(const FObjectInitializer& ObjectInitializer); 
	virtual void PostInitializeComponents() override;
	virtual void BeginDestroy() override;
	virtual void FellOutOfWorld(const UDamageType& DmgType) override;
	virtual void OnObjectPooled_Implementation() override;

	
	// For easy fire and forget usage, will invalidate the Handle as the PoolActor now takes responsibility for returning based on our poolable actor params.
	UFUNCTION(BlueprintCallable, Category="BF|Poolable Sound Actor", meta=(DisplayName="Fire And Forget"))
	virtual void FireAndForgetBP(UPARAM(ref)FBFPooledObjectHandleBP& Handle, const FBFPoolableSoundActorDescription& ActivationParams, const FTransform& ActorTransform);

	void FireAndForget(TBFPooledObjectHandlePtr<ABFPoolableSoundActor, ESPMode::NotThreadSafe>& Handle, 
		const FBFPoolableSoundActorDescription& ActivationParams, const FVector& ActorLocation, const FRotator& ActorRotation) {FireAndForget(Handle, ActivationParams, FTransform{ActorRotation, ActorLocation});}

	void FireAndForget(TBFPooledObjectHandlePtr<ABFPoolableSoundActor, ESPMode::NotThreadSafe>& Handle, 
		const FBFPoolableSoundActorDescription& ActivationParams, const FVector& SystemLocation) {FireAndForget(Handle, ActivationParams, FTransform{SystemLocation});}

	// For easy fire and forget usage, will invalidate the Handle as the PoolActor now takes responsibility for returning based on our poolable actor params.
	virtual void FireAndForget(TBFPooledObjectHandlePtr<ABFPoolableSoundActor, ESPMode::NotThreadSafe>& Handle, 
		const FBFPoolableSoundActorDescription& ActivationParams, const FTransform& ActorTransform);



	
	/** Used when manually controlling this pooled actor, otherwise you should use FireAndForget. This is typically handed to the pooled actor because you are now ready to let the pooled actor handle its own
	 * state and return itself to the pool whenever it finishes or curfew elapses. */
	UFUNCTION(BlueprintCallable, Category="BF| Poolable Sound Actor", meta = (DisplayName = "Set Pool Handle"))
	virtual void SetPoolHandleBP(UPARAM(ref)FBFPooledObjectHandleBP& Handle);

	/** Used when manually controlling this pooled actor, otherwise you should use FireAndForget. This is typically handed to the pooled actor because you are now ready to let the pooled actor handle its own
	 * state and return itself to the pool whenever it finishes or curfew elapses. */
	virtual void SetPoolHandle(TBFPooledObjectHandlePtr<ABFPoolableSoundActor, ESPMode::NotThreadSafe>& Handle);

	// If you are manually wanting to control the system then you can set its params here and call ActivatePoolableActor yourself if you want to just simply activate it and let it return when done see FireAndForget.
	UFUNCTION(BlueprintCallable, Category="BF| Poolable Sound Actor")
	virtual void SetPoolableActorParams(const FBFPoolableSoundActorDescription& ActivationParams);

	/*	Activates the pooled actor, requires you to have already set the pooled actors ActivationInfo, ideally you would have set
	 *	everything for the actors state before calling this function such as its transforms and everything else needed. */ 
	UFUNCTION(BlueprintCallable, Category="BF|Poolable Sound Actor")
	virtual void ActivatePoolableActor();
	
	/** Attempts to return our handle to the pool and returns true if it was successful, it may fail due to another copy already returning prior to us.
	* REQUIRES: You to have set the handle via SetPoolHandle or FireAndForget. */
	UFUNCTION(BlueprintCallable, Category="BF| Poolable Sound Actor")
	virtual bool ReturnToPool();
	
	/* Sets the pooled actors curfew on when it should return itself to the pool. Requires you to give the handle to the pooled actor otherwise when the elapsed curfew hits there isn't much it can do.
	 * If bWaitForSoundFinishBeforeCurfew is true then when the curfew expires we check to see if the sound is currently still playing, if it is we go back into waiting for as long as it has left. */
	UFUNCTION(BlueprintCallable, Category="BF| Poolable Sound Actor")
	virtual void SetCurfew(float SecondsUntilReturn, bool bWaitForSoundFinishBeforeCurfew = false);
	
	// Removes the current curfew if any set.
	UFUNCTION(BlueprintCallable, Category="BF| Poolable Sound Actor")
	virtual void RemoveCurfew();

	// Restarts the current sound even if it wasn't finished or playing, does require you have set the sound already though.
	UFUNCTION(BlueprintCallable, Category="BF| Poolable Sound Actor")
	virtual void RestartSound();

	UFUNCTION(BlueprintCallable, Category="BF| Poolable Sound Actor")
	void SetAutoReturnOnSoundFinished(bool bShouldAutoReturnOnSoundFinish) { ActivationInfo.bAutoReturnOnSoundFinish = bShouldAutoReturnOnSoundFinish; }

	// Is this system set to automatically return to the pool when the system finishes.
	UFUNCTION(BlueprintCallable, Category="BF| Poolable Sound Actor")
	bool GetAutoReturnOnSoundFinished() const { return ActivationInfo.bAutoReturnOnSoundFinish; }

	// Returns true if we have finished playing the sound, note if you delay the playing this still will be false until that time elapses and the sound finishes.
	UFUNCTION(BlueprintCallable, Category="BF| Poolable Sound Actor")
	bool HasSoundFinished() const { return bHasSoundFinished; }
	
	UFUNCTION(BlueprintCallable, Category="BF| Poolable Sound Actor")
	virtual bool IsActivationCurrentlyDelayed() const { return !HasSoundFinished() && GetWorld()->GetTimerManager().IsTimerActive(DelayedActivationTimerHandle); }
	
	UFUNCTION(BlueprintCallable, Category="BF| Poolable Sound Actor")
	bool IsSoundLooping() const;

	// If you previously delayed a sounds activation but now need to stop that
	UFUNCTION(BlueprintCallable, Category="BF| Poolable Sound Actor")
	void CancelDelayedActivation();

	// If you previously delayed the actor and now need to cancel and return, relies on you to have already passed the handle via SetPoolHandle. 
	UFUNCTION(BlueprintCallable, Category="BF| Poolable Sound Actor")
	void CancelDelayedActivationAndReturnToPool();

	template<typename T = UAudioComponent>
	T* GetAudioComponent() const { return CastChecked<T>(AudioComponent); }

	UFUNCTION(BlueprintCallable, Category="BF| Poolable Sound Actor")
	UAudioComponent* GetAudioComponent() const { return AudioComponent; }

	UFUNCTION(BlueprintCallable, Category="BF| Poolable Sound Actor")
	USoundBase* GetSound() const;

	UFUNCTION(BlueprintCallable, Category="BF| Poolable Sound Actor")
	float GetStartTime() const { return StartTime; }

protected:
	UFUNCTION()
	virtual void OnSoundFinished();
	virtual void OnCurfewExpired();
	
	// Called just prior to being activated in the world.
	virtual void SetupObjectState(); 
protected:
	/* BP pools store UObject handles for convenience and I cant template member functions (:
	 * So I have decided for everyone that we non ThreadSafe for performance benefits, you are using Multithreading with BP typically. You can always implement your own classes anyway.  */
	TBFPooledObjectHandlePtr<UObject, ESPMode::NotThreadSafe> BPObjectHandle = nullptr; 
	TBFPooledObjectHandlePtr<ABFPoolableSoundActor, ESPMode::NotThreadSafe> ObjectHandle = nullptr;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BF| Poolable Sound Actor")
	TObjectPtr<UAudioComponent> AudioComponent = nullptr;
	
	FBFPoolableSoundActorDescription ActivationInfo;
	FTimerHandle DelayedActivationTimerHandle;
	FTimerHandle CurfewTimerHandle;

	// Used to work out how long we have left.
	float StartTime = 0.0;
	
	UPROPERTY(Transient, BlueprintReadOnly)
	uint32 bHasSoundFinished:1 = false;
	uint32 bIsUsingBPHandle:1 = false;
	uint32 bWaitForSoundFinishBeforeCurfew:1 = false;
};








