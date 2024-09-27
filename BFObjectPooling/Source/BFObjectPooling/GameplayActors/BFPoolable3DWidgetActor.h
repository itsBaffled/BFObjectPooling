// Copyright (c) 2024 Jack Holland 
// Licensed under the MIT License. See LICENSE.md file in repo root for full license information.

#pragma once
#include "CoreMinimal.h"
#include "BFPoolableActorHelpers.h"
#include "GameFramework/Actor.h"
#include "BFObjectPooling/Pool/BFObjectPool.h"
#include "BFPoolable3DWidgetActor.generated.h"


/** All built in poolable actors have 3 main paths pre planned out for them and is typically how I expect most poolable actors to be used.
 *
 * - QuickUnpoolWidgetActor: This is the fastest and simplest way for BP to unpool these actors, the pool must be of this type and the actor must be of this type and thats more or less it, just init the pool and call QuickUnpoolWidgetActor.
 *
 * - FireAndForget: This is the simplest way to use a poolable actor from c++ (BP also has access), you just init the pool, get a pooled actor and cast it to this type and then call FireAndForget passing it the
 * 		activation params and a transform and it will handle the rest by returning after the conditional return options are met.
 *		(This is your ActorCurfew value or any other kind of auto return logic the poolable actor supplies).
 *
 * - Manual Control: You can manually control the poolable actor, you are responsible for acquiring the actors handle (like normal but you hang on to it until it's ready to return) and setting its params via SetPoolableActorParams() 
 * 		or any other way you see fit to setup your object. */


class UWidgetComponent;

/** A generic 3D World Widget actor (opposed to a screen space UI widget) that already implements the IBFPooledObjectInterface and can be used for various situations
 * involving world widget spawning/pooling at runtime, such as interaction world space UI popups, hit damage numbers and more. Pooled widgets actors hold onto their widgets in case we
 * re use the same one when unpooled, this means we only pay for spawning when the widget class differs or we have no widget to begin with. */
UCLASS(meta = (DisplayName = "BF Poolable 3D Widget Actor"))
class BFOBJECTPOOLING_API ABFPoolable3DWidgetActor : public AActor, public IBFPooledObjectInterface
{
	GENERATED_BODY()

public:
	ABFPoolable3DWidgetActor(const FObjectInitializer& ObjectInitializer);
	virtual void OnObjectPooled_Implementation() override;
	virtual void Tick(float Dt) override;
	
	// For easy fire and forget usage, will invalidate the Handle as the PoolActor now takes responsibility for returning based on our poolable actor params.
	UFUNCTION(BlueprintCallable, Category="BF|Poolable 3D Widget Actor", meta=(DisplayName="Fire And Forget"))
	virtual void FireAndForgetBP(UPARAM(ref)FBFPooledObjectHandleBP& Handle, const FBFPoolable3DWidgetActorDescription& ActivationParams, const FTransform& ActorTransform);

	void FireAndForget(TBFPooledObjectHandlePtr<ABFPoolable3DWidgetActor, ESPMode::NotThreadSafe>& Handle, 
		const FBFPoolable3DWidgetActorDescription& ActivationParams, const FVector& ActorTransform, const FRotator& SystemRotation) {FireAndForget(Handle, ActivationParams, FTransform{SystemRotation, ActorTransform});}

	void FireAndForget(TBFPooledObjectHandlePtr<ABFPoolable3DWidgetActor, ESPMode::NotThreadSafe>& Handle, 
		const FBFPoolable3DWidgetActorDescription& ActivationParams, const FVector& SystemLocation) {FireAndForget(Handle, ActivationParams, FTransform{SystemLocation});}

	// For easy fire and forget usage, will invalidate the Handle as the PoolActor now takes responsibility for returning based on our poolable actor params.
	virtual void FireAndForget(TBFPooledObjectHandlePtr<ABFPoolable3DWidgetActor, ESPMode::NotThreadSafe>& Handle, 
		const FBFPoolable3DWidgetActorDescription& ActivationParams, const FTransform& ActorTransform);
	


	
	/** Used when manually controlling this pooled actor, otherwise you should use FireAndForget. This is typically handed to the pooled actor because you are now ready to let the pooled actor handle its own
	 * state and return itself to the pool whenever it finishes or curfew elapses. */
	UFUNCTION(BlueprintCallable, Category="BF| Poolable 3D Widget Actor", meta = (DisplayName = "Set Pool Handle"))
	virtual void SetPoolHandleBP(UPARAM(ref)FBFPooledObjectHandleBP& Handle);

	/** Used when manually controlling this pooled actor, otherwise you should use FireAndForget. This is typically handed to the pooled actor because you are now ready to let the pooled actor handle its own
	 * state and return itself to the pool whenever it finishes or curfew elapses. */
	virtual void SetPoolHandle(TBFPooledObjectHandlePtr<ABFPoolable3DWidgetActor, ESPMode::NotThreadSafe>& Handle);

	// If you are manually wanting to control the system then you can set its params here and call ActivatePoolableActor yourself if you want to just simply activate it and let it return when done see FireAndForget.
	UFUNCTION(BlueprintCallable, Category="BF| Poolable 3D Widget Actor")
	virtual void SetPoolableActorParams(const FBFPoolable3DWidgetActorDescription& ActivationParams);
	
	/*	Activates the pooled actor, requires you to have already set the pooled actors ActivationInfo, ideally you would have set
	 *	everything for the actors state before calling this function such as its transforms and everything else needed. */ 
	UFUNCTION(BlueprintCallable, Category="BF|Poolable 3D Widget Actor")
	virtual void ActivatePoolableActor();

	/** Attempts to return our handle to the pool and returns true if it was successful, it may fail due to another copy already returning prior to us.
	* REQUIRES: You to have set the handle via SetPoolHandle or FireAndForget. */
	UFUNCTION(BlueprintCallable, Category="BF| Poolable 3D Widget Actor")
	virtual bool ReturnToPool();
	
	// Sets the pooled actors curfew on when it should return itself to the pool. Requires you to give the handle to the pooled actor otherwise when the elapsed curfew hits there isn't much it can do. 
	UFUNCTION(BlueprintCallable, Category="BF| Poolable 3D Widget Actor")
	virtual void SetCurfew(float SecondsUntilReturn);
	
	// Removes the current curfew if any set.
	UFUNCTION(BlueprintCallable, Category="BF| Poolable 3D Widget Actor")
	virtual void RemoveCurfew();
	
	UFUNCTION(BlueprintCallable, Category="BF| Poolable 3D Widget Actor")
	UWidgetComponent* GetWidgetComponent() const { return WidgetComponent; }

protected:
	virtual void OnCurfewExpired();
	
	// Called just prior to being activated in the world.
	virtual void SetupObjectState(); 
protected:
	/* BP pools store UObject handles for convenience and I cant template member functions (:
	 * So I have decided for everyone that we non ThreadSafe for performance benefits, you are using Multithreading with BP typically. You can always implement your own classes anyway.  */
	TBFPooledObjectHandlePtr<UObject, ESPMode::NotThreadSafe> BPObjectHandle = nullptr; 
	TBFPooledObjectHandlePtr<ABFPoolable3DWidgetActor, ESPMode::NotThreadSafe> ObjectHandle = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BF| Poolable 3D Widget Actor")
	TObjectPtr<UWidgetComponent> WidgetComponent = nullptr;

	// Used to track our lifetime so we can normalize it and use it for the ActivationInfo.WidgetPositionAndScaleCurve
	float StartingTime = 0.f;
	FVector AbsoluteStartLocation = FVector::ZeroVector;

	FBFPoolable3DWidgetActorDescription ActivationInfo;
	FTimerHandle CurfewTimerHandle;

	UPROPERTY(Transient)
	uint32 bIsUsingBPHandle:1 = false;
};

