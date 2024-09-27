// Copyright (c) 2024 Jack Holland 
// Licensed under the MIT License. See LICENSE.md file in repo root for full license information.

#pragma once
#include "BFObjectPooling/Interfaces/BFPooledObjectInterface.h"
#include "BFObjectPooling/Pool/BFObjectPool.h"
#include "BFObjectPooling/Pool/BFPooledObjectHandle.h"
#include "GameFramework/Actor.h"
#include "BFPoolableActorHelpers.h"
#include "BFPoolableProjectileActor.generated.h"


class UNiagaraComponent;
class UProjectileMovementComponent;



/** All built in poolable actors have 3 main paths pre planned out for them and is typically how I expect most poolable actors to be used.
 *
 * - QuickUnpoolProjectileActor: This is the fastest and simplest way for BP to unpool these actors, the pool must be of this type and the actor must be of this type and thats more or less it, just init the pool and call QuickUnpoolProjectileActor.
 *
 * - FireAndForget: This is the simplest way to use a poolable actor from c++ (BP also has access), you just init the pool, get a pooled actor and cast it to this type and then call FireAndForget passing it the
 * 		activation params and a transform and it will handle the rest by returning after the conditional return options are met.
 *		(This is your ActorCurfew value or any other kind of auto return logic the poolable actor supplies).
 *
 * - Manual Control: You can manually control the poolable actor, you are responsible for acquiring the actors handle (like normal but you hang on to it until it's ready to return) and setting its params via SetPoolableActorParams() 
 * 		or any other way you see fit to setup your object. */


/** A generic poolable projectile actor that already implements the IBFPooledObjectInterface and can be used for various situations involving projectiles in the world.
 * Collision shape, collision type, mesh, materials, VFX system etc can all be customized per pooled Projectile actor, even within the same pool.
 * You also don't pay for transform updates or collision checks for non used components. This comes with a *tiny* runtime
 * cost in order to re create the components *ONLY* when needed or changed from previous pool usages but it is very minimal and only happens when the actor is activated, the trade off is far worth it and can be easily mitigated by choosing to pre-spawn the
 * actors from the pool init struct. */
UCLASS(meta = (DisplayName = "BF Poolable Projectile Actor"))
class BFOBJECTPOOLING_API ABFPoolableProjectileActor : public AActor, public IBFPooledObjectInterface 
{
	GENERATED_BODY()

public:
	ABFPoolableProjectileActor(const FObjectInitializer& ObjectInitializer);
	virtual void OnObjectPooled_Implementation() override;
	virtual void PostInitializeComponents() override;

	
	// For easy fire and forget usage, will invalidate the Handle as the PoolActor now takes responsibility for returning based on our poolable actor params.
	UFUNCTION(BlueprintCallable, Category="BF|Poolable Static Mesh Actor", meta=(DisplayName="Fire And Forget Initialize"))
	virtual void FireAndForgetBP(UPARAM(ref)FBFPooledObjectHandleBP& Handle, const FBFPoolableProjectileActorDescription& ActivationParams, const FTransform& ActorTransform);

	void FireAndForget(TBFPooledObjectHandlePtr<ABFPoolableProjectileActor, ESPMode::NotThreadSafe>& Handle, 
		const FBFPoolableProjectileActorDescription& ActivationParams, const FVector& ActorLocation, const FRotator& ActorRotation) {FireAndForget(Handle, ActivationParams, FTransform{ActorRotation, ActorLocation});}

	void FireAndForget(TBFPooledObjectHandlePtr<ABFPoolableProjectileActor, ESPMode::NotThreadSafe>& Handle, 
		const FBFPoolableProjectileActorDescription& ActivationParams, const FVector& ActorLocation) {FireAndForget(Handle, ActivationParams, FTransform{ActorLocation});}

	// For easy fire and forget usage, will invalidate the Handle as the PoolActor now takes responsibility for returning based on our poolable actor params.
	virtual void FireAndForget(TBFPooledObjectHandlePtr<ABFPoolableProjectileActor, ESPMode::NotThreadSafe>& Handle, 
		const FBFPoolableProjectileActorDescription& ActivationParams, const FTransform& ActorTransform);



	/** Used when manually controlling this pooled actor, otherwise you should use FireAndForget. This is typically handed to the pooled actor because you are now ready to let the pooled actor handle its own
	 * state and return itself to the pool whenever it finishes or curfew elapses. */
	UFUNCTION(BlueprintCallable, Category="BF| Poolable Projectile Actor", meta = (DisplayName = "Set Pool Handle"))
	virtual void SetPoolHandleBP(UPARAM(ref)FBFPooledObjectHandleBP& Handle);

	/** Used when manually controlling this pooled actor, otherwise you should use FireAndForget. This is typically handed to the pooled actor because you are now ready to let the pooled actor handle its own
	 * state and return itself to the pool whenever it finishes or curfew elapses. */
	virtual void SetPoolHandle(TBFPooledObjectHandlePtr<ABFPoolableProjectileActor, ESPMode::NotThreadSafe>& Handle);

	// If you are manually wanting to control the system then you can set its params here and call ActivatePoolableActor yourself, if you want to just simply activate it and let it return when done see FireAndForget.
	UFUNCTION(BlueprintCallable, Category="BF| Poolable Projectile Actor")
	virtual void SetPoolableActorParams(const FBFPoolableProjectileActorDescription& ActivationParams);

	 /*	Activates the pooled actor, requires you to have already set the pooled actors ActivationInfo, ideally you would have set
	  *	everything for the actors state before calling this function such as its transforms and everything else needed. */ 
	UFUNCTION(BlueprintCallable, Category="BF|Poolable Static Mesh Actor")
	virtual void ActivatePoolableActor();
	
	/** Attempts to return our handle to the pool and returns true if it was successful, it may fail due to another copy already returning prior to us.
	 * REQUIRES: You to have set the handle via SetPoolHandle or FireAndForget. */
	UFUNCTION(BlueprintCallable, Category="BF| Poolable Projectile Actor")
	virtual bool ReturnToPool();

	// Sets the pooled actors curfew on when it should return itself to the pool. Requires you to give the handle to the pooled actor otherwise when the elapsed curfew hits there isn't much it can do. 
	UFUNCTION(BlueprintCallable, Category="BF| Poolable Projectile Actor")
	virtual void SetCurfew(float SecondsUntilReturn);
	
	// Removes the current curfew if any set.
	UFUNCTION(BlueprintCallable, Category="BF| Poolable Projectile Actor")
	virtual void RemoveCurfew();
	
	UFUNCTION(BlueprintCallable, Category="BF| Poolable Projectile Actor")
	UProjectileMovementComponent* GetProjectileMovementComponent() const { return ProjectileMovementComponent; }
	
	UFUNCTION(BlueprintCallable, Category="BF| Poolable Projectile Actor")
	UStaticMeshComponent* GetStaticMeshComponent() const { return OptionalStaticMeshComponent; }
	
protected:
	UFUNCTION(BlueprintNativeEvent, Category="BF| Poolable Projectile Actor")
	void OnProjectileActorHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	UFUNCTION(BlueprintNativeEvent, Category="BF| Poolable Projectile Actor")
	void OnProjectileActorOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION(BlueprintNativeEvent, Category="BF| Poolable Projectile Actor")
	void OnProjectileStopped(const FHitResult& HitResult);
	
	virtual void OnCurfewExpired();

	/* Any pooled projectile actor can be able to define a completely different collision component and mesh/material from any other within the same pool.
	 * It's not cheap to go changing it every frame but there is support for dynamic changes if needed, otherwise there really shouldn't be any perf impact
	 * at all since all components are only created as needed. */
	virtual bool HandleComponentCreation();

	// Called just prior to being activated in the world.
	virtual void SetupObjectState(); 
protected:
	/* BP pools store UObject handles for convenience and I cant template member functions (:
	 * So I have decided for everyone that we non ThreadSafe for performance benefits, you are using Multithreading with BP typically. You can always implement your own classes anyway.  */
	TBFPooledObjectHandlePtr<UObject, ESPMode::NotThreadSafe> BPObjectHandle = nullptr; 
	TBFPooledObjectHandlePtr<ABFPoolableProjectileActor, ESPMode::NotThreadSafe> ObjectHandle = nullptr;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BF| Poolable Projectile Actor")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovementComponent = nullptr;

	// Optional component pointer that will be invalid if you are not actually defining an asset for it to use in the Init struct.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BF| Poolable Projectile Actor")
	TObjectPtr<UStaticMeshComponent> OptionalStaticMeshComponent = nullptr;
	
	// Optional component pointer that will be invalid if you are not actually defining an asset for it to use in the Init struct.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BF| Poolable Projectile Actor")
	TObjectPtr<UNiagaraComponent> OptionalNiagaraComponent = nullptr;	

	FTimerHandle CurfewTimerHandle;
	FBFPoolableProjectileActorDescription ActivationInfo;

	UPROPERTY(Transient)
	uint32 bIsUsingBPHandle:1 = false;
};
