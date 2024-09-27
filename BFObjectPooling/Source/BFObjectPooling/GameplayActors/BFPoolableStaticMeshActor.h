// Copyright (c) 2024 Jack Holland 
// Licensed under the MIT License. See LICENSE.md file in repo root for full license information.

#pragma once
#include "BFObjectPooling/Interfaces/BFPooledObjectInterface.h"
#include "BFObjectPooling/Pool/BFObjectPool.h"
#include "BFObjectPooling/Pool/BFPooledObjectHandle.h"
#include "GameFramework/Actor.h"
#include "BFPoolableActorHelpers.h"
#include "BFPoolableStaticMeshActor.generated.h"

 
/** All built in poolable actors have 3 main paths pre planned out for them and is typically how I expect most poolable actors to be used.
 *
 * - QuickUnpoolStaticMeshActor: This is the fastest and simplest way for BP to unpool these actors, the pool must be of this type and the actor must be of this type and thats more or less it, just init the pool and call QuickUnpoolStaticMeshActor.
 *
 * - FireAndForget: This is the simplest way to use a poolable actor from c++ (BP also has access), you just init the pool, get a pooled actor and cast it to this type and then call FireAndForget passing it the
 * 		activation params and a transform and it will handle the rest by returning after the conditional return options are met.
 *		(This is your ActorCurfew value or any other kind of auto return logic the poolable actor supplies).
 *
 * - Manual Control: You can manually control the poolable actor, you are responsible for acquiring the actors handle (like normal but you hang on to it until it's ready to return) and setting its params via SetPoolableActorParams() 
 * 		or any other way you see fit to setup your object. */

/** A generic poolable static mesh actor that already implements the IBFPooledObjectInterface and can be used for various situations involving mesh spawning/pooling at runtime, such as gibs or bullet shells. */
UCLASS(meta=(DisplayName="BF Poolable Static Mesh Actor"))
class BFOBJECTPOOLING_API ABFPoolableStaticMeshActor : public AActor, public IBFPooledObjectInterface
{
	GENERATED_BODY()

public:
	ABFPoolableStaticMeshActor(const FObjectInitializer& ObjectInitializer);
	virtual void OnObjectPooled_Implementation() override;
	
	// For easy fire and forget usage, will invalidate the Handle as the PoolActor now takes responsibility for returning based on our poolable actor params.
	UFUNCTION(BlueprintCallable, Category="BF|Poolable Static Mesh Actor", meta=(DisplayName="Fire And Forget"))
	virtual void FireAndForgetBP(UPARAM(ref)FBFPooledObjectHandleBP& Handle, const FBFPoolableStaticMeshActorDescription& ActivationParams, const FTransform& ActorTransform);

	void FireAndForget(TBFPooledObjectHandlePtr<ABFPoolableStaticMeshActor, ESPMode::NotThreadSafe>& Handle, 
		const FBFPoolableStaticMeshActorDescription& ActivationParams, const FVector& ActorLocation, const FRotator& ActorRotation) {FireAndForget(Handle, ActivationParams, FTransform{ActorRotation, ActorLocation});}

	void FireAndForget(TBFPooledObjectHandlePtr<ABFPoolableStaticMeshActor, ESPMode::NotThreadSafe>& Handle, 
		const FBFPoolableStaticMeshActorDescription& ActivationParams, const FVector& ActorLocation) {FireAndForget(Handle, ActivationParams, FTransform{ActorLocation});}

	// For easy fire and forget usage, will invalidate the Handle as the PoolActor now takes responsibility for returning based on our poolable actor params.
	virtual void FireAndForget(TBFPooledObjectHandlePtr<ABFPoolableStaticMeshActor, ESPMode::NotThreadSafe>& Handle, 
		const FBFPoolableStaticMeshActorDescription& ActivationParams, const FTransform& ActorTransform);
	

	
	/** Used when manually controlling this pooled actor, otherwise you should use FireAndForget. This is typically handed to the pooled actor because you are now ready to let the pooled actor handle its own
	 * state and return itself to the pool whenever it finishes or curfew elapses. */
	UFUNCTION(BlueprintCallable, Category="BF| Poolable Static Mesh Actor", meta = (DisplayName = "Set Pool Handle"))
	virtual void SetPoolHandleBP(UPARAM(ref)FBFPooledObjectHandleBP& Handle);

	/** Used when manually controlling this pooled actor, otherwise you should use FireAndForget. This is typically handed to the pooled actor because you are now ready to let the pooled actor handle its own
	 * state and return itself to the pool whenever it finishes or curfew elapses. */
	virtual void SetPoolHandle(TBFPooledObjectHandlePtr<ABFPoolableStaticMeshActor, ESPMode::NotThreadSafe>& Handle);

	// If you are manually wanting to control the system then you can set its params here and call ActivatePoolableActor yourself if you want to just simply activate it and let it return when done see FireAndForget.
	UFUNCTION(BlueprintCallable, Category="BF| Poolable Static Mesh Actor")
	virtual void SetPoolableActorParams(const FBFPoolableStaticMeshActorDescription& ActivationParams);

	/*	Activates the pooled actor, requires you to have already set the pooled actors ActivationInfo, ideally you would have set
	 *	everything for the actors state before calling this function such as its transforms and everything else needed. */ 
	UFUNCTION(BlueprintCallable, Category="BF|Poolable Static Mesh Actor")
	virtual void ActivatePoolableActor(bool bSimulatePhysics);
	
	/** Attempts to return our handle to the pool and returns true if it was successful, it may fail due to another copy already returning prior to us.
	* REQUIRES: You to have set the handle via SetPoolHandle or FireAndForget. */
	UFUNCTION(BlueprintCallable, Category="BF| Poolable Static Mesh Actor")
	virtual bool ReturnToPool();

	// Sets whether the mesh should simulate physics or not.
	UFUNCTION(BlueprintCallable, Category="BF|Poolable Static Mesh Actor")
	void SetMeshSimulatePhysics(bool bSimulatePhysics) { StaticMeshComponent->SetSimulatePhysics(bSimulatePhysics); }
	
	// Sets the pooled actors curfew on when it should return itself to the pool. Requires you to give the handle to the pooled actor otherwise when the elapsed curfew hits there isn't much it can do. 
	UFUNCTION(BlueprintCallable, Category="BF| Poolable Static Mesh Actor")
	virtual void SetCurfew(float SecondsUntilReturn);
	
	// Removes the current curfew if any set.
	UFUNCTION(BlueprintCallable, Category="BF| Poolable Static Mesh Actor")
	virtual void RemoveCurfew();
	
	UFUNCTION(BlueprintCallable, Category="BF| Poolable Static Mesh Actor")
	UStaticMeshComponent* GetStaticMeshComponent() const { return StaticMeshComponent; }
	
	UFUNCTION(BlueprintCallable, Category="BF| Poolable Static Mesh Actor")
	UStaticMesh* GetStaticMesh() const;

protected:
	virtual void OnCurfewExpired();
	
	// Called just prior to being activated in the world.
	virtual void SetupObjectState(bool bSimulatePhysics); 
protected:
	/* BP pools store UObject handles for convenience and I cant template member functions (:
	 * So I have decided for everyone that we non ThreadSafe for performance benefits, you are using Multithreading with BP typically. You can always implement your own classes anyway.  */
	TBFPooledObjectHandlePtr<UObject, ESPMode::NotThreadSafe> BPObjectHandle = nullptr; 
	TBFPooledObjectHandlePtr<ABFPoolableStaticMeshActor, ESPMode::NotThreadSafe> ObjectHandle = nullptr;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BF| Poolable Static Mesh Actor")
	TObjectPtr<UStaticMeshComponent> StaticMeshComponent = nullptr;

	FBFPoolableStaticMeshActorDescription ActivationInfo;
	FTimerHandle CurfewTimerHandle;

	UPROPERTY(Transient)
	uint32 bIsUsingBPHandle:1 = false;
	uint32 bWasSimulating:1 = false;
};


