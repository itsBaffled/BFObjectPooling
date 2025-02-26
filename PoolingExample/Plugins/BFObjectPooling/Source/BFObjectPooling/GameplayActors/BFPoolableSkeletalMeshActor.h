﻿// Copyright (c) 2024 Jack Holland 
// Licensed under the MIT License. See LICENSE.md file in repo root for full license information.

#pragma once
#include "GameFramework/Actor.h"

#include "BFPoolableActorHelpers.h"
#include "BFObjectPooling/Interfaces/BFPooledObjectInterface.h"
#include "BFObjectPooling/Pool/BFPooledObjectHandle.h"
#include "BFPoolableSkeletalMeshActor.generated.h"



/** All built in poolable actors have 3 main paths pre planned out for them and is typically how I expect most poolable actors to be used.
 *
 * - QuickUnpoolSkeletalMeshActor: This is the fastest and simplest way for BP to unpool these actors, the pool must be of this type and the actor must be of this type and thats more or less it, just init the pool and call QuickUnpoolSkeletalMeshActor.
 *
 * - FireAndForget: This is the simplest way to use a poolable actor from c++ (BP also has access), you just init the pool, get a pooled actor and cast it to this type and then call FireAndForget passing it the
 * 		activation params and a transform and it will handle the rest by returning after the conditional return options are met.
 *		(This is your ActorCurfew value or any other kind of auto return logic the poolable actor supplies).
 *
 * - Manual Control: You can manually control the poolable actor, you are responsible for acquiring the actors handle (like normal but you hang on to it until it's ready to return) and setting its params via SetPoolableActorParams() 
 * 		or any other way you see fit to setup your object. */


// --------------

// A generic poolable skeletal mesh actor that already implements the IBFPooledObjectInterface and can be used for various situations involving skeletal mesh spawning/pooling at runtime.
UCLASS(meta=(DisplayName="BF Poolable Skeletal Mesh Actor"))
class BFOBJECTPOOLING_API ABFPoolableSkeletalMeshActor : public AActor, public IBFPooledObjectInterface
{
	GENERATED_BODY() 

public:
	ABFPoolableSkeletalMeshActor(const FObjectInitializer& ObjectInitializer);
	virtual void OnObjectPooled_Implementation() override;
	virtual void FellOutOfWorld(const UDamageType& DmgType) override;
	
	// For easy fire and forget usage, will invalidate the InHandle as the PoolActor now takes responsibility for returning based on our poolable actor params.
	UFUNCTION(BlueprintCallable, Category="BF|Poolable Skeletal Mesh Actor", meta=(DisplayName="Fire And Forget"))
	virtual void FireAndForgetBP(UPARAM(ref)FBFPooledObjectHandleBP& Handle,
								const FBFPoolableSkeletalMeshActorDescription& ActivationParams,
								const FTransform& ActorTransform);

	// For easy fire and forget usage, will invalidate the Handle as the PoolActor now takes responsibility for returning based on our poolable actor params.
	void FireAndForget(TBFPooledObjectHandlePtr<ABFPoolableSkeletalMeshActor, ESPMode::NotThreadSafe>& Handle, 
						const FBFPoolableSkeletalMeshActorDescription& ActivationParams,
						const FVector& ActorTransform,
						const FRotator& SystemRotation);

	void FireAndForget(TBFPooledObjectHandlePtr<ABFPoolableSkeletalMeshActor, ESPMode::NotThreadSafe>& Handle, 
	                   const FBFPoolableSkeletalMeshActorDescription& ActivationParams,
	                   const FVector& SystemLocation);

	virtual void FireAndForget(TBFPooledObjectHandlePtr<ABFPoolableSkeletalMeshActor, ESPMode::NotThreadSafe>& Handle, 
								const FBFPoolableSkeletalMeshActorDescription& ActivationParams,
								const FTransform& ActorTransform);
	

	/** Used when manually controlling this pooled actor, otherwise you should use FireAndForget. This is typically handed to the pooled actor because you are now ready to let the pooled actor handle its own
	 * state and return itself to the pool whenever it finishes or curfew elapses. */
	UFUNCTION(BlueprintCallable, Category="BF| Poolable Skeletal Mesh Actor", meta = (DisplayName = "Set Pool Handle"))
	virtual void SetPoolHandleBP(UPARAM(ref)FBFPooledObjectHandleBP& Handle);

	/** Used when manually controlling this pooled actor, otherwise you should use FireAndForget. This is typically handed to the pooled actor because you are now ready to let the pooled actor handle its own
	 * state and return itself to the pool whenever it finishes or curfew elapses. */
	virtual void SetPoolHandle(TBFPooledObjectHandlePtr<ABFPoolableSkeletalMeshActor, ESPMode::NotThreadSafe>& Handle);

	// If you are manually wanting to control the system then you can set its params here and call ActivatePoolableActor yourself if you want to just simply activate it and let it return when done see FireAndForget.
	UFUNCTION(BlueprintCallable, Category="BF| Poolable Skeletal Mesh Actor")
	virtual void SetPoolableActorParams(const FBFPoolableSkeletalMeshActorDescription& ActivationParams);
	
	/*	Activates the pooled actor, requires you to have already set the pooled actors ActivationInfo, ideally you would have set
	 *	everything for the actors state before calling this function such as its transforms and everything else needed. */ 
	UFUNCTION(BlueprintCallable, Category="BF|Poolable Skeletal Mesh Actor")
	virtual void ActivatePoolableActor(bool bSimulatePhysics);

	/** Attempts to return our handle to the pool and returns true if it was successful, it may fail due to another copy already returning prior to us.
	* REQUIRES: You to have set the handle via SetPoolHandle or FireAndForget. */
	UFUNCTION(BlueprintCallable, Category="BF| Poolable Skeletal Mesh Actor")
	virtual bool ReturnToPool();

	// Sets whether the mesh should simulate physics or not.
	UFUNCTION(BlueprintCallable, Category="BF|Poolable Skeletal Mesh Actor")
	void SetMeshSimulatePhysics(bool bSimulatePhysics) { SkeletalMeshComponent->SetSimulatePhysics(bSimulatePhysics); }
	
	// Sets the pooled actors curfew on when it should return itself to the pool. Requires you to give the handle to the pooled actor otherwise when the elapsed curfew hits there isn't much it can do. 
	UFUNCTION(BlueprintCallable, Category="BF| Poolable Skeletal Mesh Actor")
	virtual void SetCurfew(float SecondsUntilReturn);
	
	// Removes the current curfew if any set.
	UFUNCTION(BlueprintCallable, Category="BF| Poolable Skeletal Mesh Actor")
	virtual void RemoveCurfew();

	UFUNCTION(BlueprintCallable, Category="BF| Poolable Skeletal Mesh Actor")
	void SetPhysicSleepProfile(FCollisionProfileName Profile) { MeshSleepPhysicsProfile = Profile; }
	
	UFUNCTION(BlueprintCallable, Category="BF| Poolable Skeletal Mesh Actor")
	USkeletalMeshComponent* GetSkeletalMeshComponent() const { return SkeletalMeshComponent; }
	
	UFUNCTION(BlueprintCallable, Category="BF| Poolable Skeletal Mesh Actor")
	USkeletalMesh* GetSkeletalMesh() const;

protected:
	virtual void OnCurfewExpired();
	// Called just prior to being activated in the world.
	virtual void SetupObjectState(bool bSimulatePhysics);

	virtual void RemovePhysicsSleepDelay();

protected:
	// Blueprint pools are wrappers around UObject Templated pools.
	TBFPooledObjectHandlePtr<UObject, ESPMode::NotThreadSafe> BPObjectHandle = nullptr; 
	TBFPooledObjectHandlePtr<ABFPoolableSkeletalMeshActor, ESPMode::NotThreadSafe> ObjectHandle = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BF| Poolable Skeletal Mesh Actor")
	FCollisionProfileName MeshSleepPhysicsProfile = FCollisionProfileName{"Ragdoll"};

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BF| Poolable Skeletal Mesh Actor")
	TObjectPtr<USkeletalMeshComponent> SkeletalMeshComponent = nullptr;
	
	FBFPoolableSkeletalMeshActorDescription ActivationInfo;
	FTimerHandle CurfewTimerHandle;
	FTimerHandle SleepPhysicsTimerHandle;

	uint32 bIsUsingBPHandle:1 = false;
};




inline void ABFPoolableSkeletalMeshActor::FireAndForget(TBFPooledObjectHandlePtr<ABFPoolableSkeletalMeshActor, ESPMode::NotThreadSafe>& Handle,
													const FBFPoolableSkeletalMeshActorDescription& ActivationParams,
													const FVector& ActorTransform,
													const FRotator& SystemRotation)
{
	FireAndForget(Handle, ActivationParams, FTransform{SystemRotation, ActorTransform});
}


inline void ABFPoolableSkeletalMeshActor::FireAndForget(TBFPooledObjectHandlePtr<ABFPoolableSkeletalMeshActor, ESPMode::NotThreadSafe>& Handle,
												const FBFPoolableSkeletalMeshActorDescription& ActivationParams,
												const FVector& SystemLocation)
{
	FireAndForget(Handle, ActivationParams, FTransform{SystemLocation});
}