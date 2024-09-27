// Copyright (c) 2024 Jack Holland 
// Licensed under the MIT License. See LICENSE.md file in repo root for full license information.

#pragma once
#include "BFPoolableActorHelpers.h"
#include "BFObjectPooling/Pool/BFObjectPool.h"
#include "BFObjectPooling/Interfaces/BFPooledObjectInterface.h"
#include "BFObjectPooling/Pool/BFPooledObjectHandle.h"
#include "GameFramework/Actor.h"
#include "BFPoolableDecalActor.generated.h"



/** All built in poolable actors have 3 main paths pre planned out for them and is typically how I expect most poolable actors to be used.
 *
 * - QuickUnpoolDecalActor: This is the fastest and simplest way for BP to unpool these actors, the pool must be of this type and the actor must be of this type and thats more or less it, just init the pool and call QuickUnpoolDecalActor.
 *
 * - FireAndForget: This is the simplest way to use a poolable actor from c++ (BP also has access), you just init the pool, get a pooled actor and cast it to this type and then call FireAndForget passing it the
 * 		activation params and a transform and it will handle the rest by returning after the conditional return options are met.
 *		(This is your ActorCurfew value or any other kind of auto return logic the poolable actor supplies).
 *
 * - Manual Control: You can manually control the poolable actor, you are responsible for acquiring the actors handle (like normal but you hang on to it until it's ready to return) and setting its params via SetPoolableActorParams() 
 * 		or any other way you see fit to setup your object. */


/** A generic poolable decal actor, mostly for fire and forget decals during gameplay such as bullet impacts, blood splatters, foot steps etc. */
UCLASS(meta = (DisplayName = "BF Poolable Decal Actor"))
class BFOBJECTPOOLING_API ABFPoolableDecalActor : public AActor, public IBFPooledObjectInterface
{
	GENERATED_BODY()

public:
	ABFPoolableDecalActor(const FObjectInitializer& ObjectInitializer);
	virtual void OnObjectPooled_Implementation() override;

	
	// For easy fire and forget usage, will invalidate the Handle as the PoolActor now takes responsibility for returning based on our poolable actor params.
	UFUNCTION(BlueprintCallable, Category="BF|Poolable Decal Actor", meta=(DisplayName="Fire And Forget"))
	virtual void FireAndForgetBP(UPARAM(ref)FBFPooledObjectHandleBP& Handle, const FBFPoolableDecalActorDescription& ActivationParams, const FTransform& ActorTransform);

	void FireAndForget(TBFPooledObjectHandlePtr<ABFPoolableDecalActor, ESPMode::NotThreadSafe>& Handle, 
		const FBFPoolableDecalActorDescription& ActivationParams, const FVector& ActorTransform, const FRotator& SystemRotation) {FireAndForget(Handle, ActivationParams, FTransform{SystemRotation, ActorTransform});}

	void FireAndForget(TBFPooledObjectHandlePtr<ABFPoolableDecalActor, ESPMode::NotThreadSafe>& Handle, 
		const FBFPoolableDecalActorDescription& ActivationParams, const FVector& SystemLocation) {FireAndForget(Handle, ActivationParams, FTransform{SystemLocation});}

	// For easy fire and forget usage, will invalidate the Handle as the PoolActor now takes responsibility for returning based on our poolable actor params.
	virtual void FireAndForget(TBFPooledObjectHandlePtr<ABFPoolableDecalActor, ESPMode::NotThreadSafe>& Handle, 
		const FBFPoolableDecalActorDescription& ActivationParams, const FTransform& ActorTransform);


	

	/** Used when manually controlling this pooled actor, otherwise you should use FireAndForget. This is typically handed to the pooled actor because you are now ready to let the pooled actor handle its own
	 * state and return itself to the pool whenever it finishes or curfew elapses. */
	UFUNCTION(BlueprintCallable, Category="BF| Poolable Decal Actor", meta = (DisplayName = "Set Pool Handle"))
	virtual void SetPoolHandleBP(UPARAM(ref)FBFPooledObjectHandleBP& Handle);

	/** Used when manually controlling this pooled actor, otherwise you should use FireAndForget. This is typically handed to the pooled actor because you are now ready to let the pooled actor handle its own
	 * state and return itself to the pool whenever it finishes or curfew elapses. */
	virtual void SetPoolHandle(TBFPooledObjectHandlePtr<ABFPoolableDecalActor, ESPMode::NotThreadSafe>& Handle);

	// If you are manually wanting to control the system then you can set its params here and call ActivatePoolableActor yourself if you want to just simply activate it and let it return when done see FireAndForget.
	UFUNCTION(BlueprintCallable, Category="BF| Poolable Decal Actor")
	virtual void SetPoolableActorParams(const FBFPoolableDecalActorDescription& ActivationParams);
	
	/*	Activates the pooled actor, requires you to have already set the pooled actors ActivationInfo, ideally you would have set
	 *	everything for the actors state before calling this function such as its transforms and everything else needed. */ 
	UFUNCTION(BlueprintCallable, Category="BF|Poolable Decal Actor")
	virtual void ActivatePoolableActor();

	/** Attempts to return our handle to the pool and returns true if it was successful, it may fail due to another copy already returning prior to us.
	* REQUIRES: You to have set the handle via SetPoolHandle or FireAndForget. */
	UFUNCTION(BlueprintCallable, Category="BF| Poolable Decal Actor")
	virtual bool ReturnToPool();
	
	// Sets the pooled actors curfew on when it should return itself to the pool. Requires you to give the handle to the pooled actor otherwise when the elapsed curfew hits there isn't much it can do. 
	UFUNCTION(BlueprintCallable, Category="BF| Poolable Decal Actor")
	virtual void SetCurfew(float SecondsUntilReturn);
	
	// Removes the current curfew if any set.
	UFUNCTION(BlueprintCallable, Category="BF| Poolable Decal Actor")
	virtual void RemoveCurfew();
	
	UFUNCTION(BlueprintCallable, Category="BF| Poolable Decal Actor")
	UDecalComponent* GetDecalComponent() const { return DecalComponent; }

protected:
	virtual void OnCurfewExpired();
	
	// Called just prior to being activated in the world.
	virtual void SetupObjectState();
protected:
	/* BP pools store UObject handles for convenience and I cant template member functions (:
	 * So I have decided for everyone that we non ThreadSafe for performance benefits, you are using Multithreading with BP typically. You can always implement your own classes anyway.  */
	TBFPooledObjectHandlePtr<UObject, ESPMode::NotThreadSafe> BPObjectHandle = nullptr; 
	TBFPooledObjectHandlePtr<ABFPoolableDecalActor, ESPMode::NotThreadSafe> ObjectHandle = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BF| Poolable Decal Actor")
	TObjectPtr<UDecalComponent> DecalComponent = nullptr;

	FBFPoolableDecalActorDescription ActivationInfo;
	FTimerHandle CurfewTimerHandle;

	UPROPERTY(Transient)
	uint32 bIsUsingBPHandle:1 = false;
};


