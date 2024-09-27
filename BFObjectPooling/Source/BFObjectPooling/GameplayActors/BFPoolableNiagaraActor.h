// Copyright (c) 2024 Jack Holland 
// Licensed under the MIT License. See LICENSE.md file in repo root for full license information.

#pragma once
#include "NiagaraSystem.h"
#include "BFObjectPooling/Interfaces/BFPooledObjectInterface.h"
#include "BFObjectPooling/Pool/BFObjectPool.h"
#include "BFObjectPooling/Pool/BFPooledObjectHandle.h"
#include "BFPoolableActorHelpers.h"
#include "GameFramework/Actor.h"
#include "BFPoolableNiagaraActor.generated.h"


class UNiagaraComponent;


DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPoolableNiagaraSystemFinished);


/** All built in poolable actors have 2 main paths pre planned out for them and is typically how I expect most poolable actors to be used.
 *
 * - BP side QuickUnpoolNiagaraActor : This is the fastest and simplest way for BP to unpool these actors, the pool must be of this type and the actor must be of this type.
 *
 * - FireAndForget: This is the simplest way to use a poolable actor, you give it the initial params and it will handle the rest by returning to the pool after the conditional return options are met.
 *		(This is your ActorCurfew value or any other kind of auto return logic the poolable actor supplies).
 *
 * - Manual Control: You can manually control the poolable actor, you are responsible for acquiring the actors handle, setting its params via SetPoolableActorParams() or any other way you see fit and then activating the actor yourself,
 *		You can then manually tell the handle when to return or pass the object its handle via SetPoolHandle and let the actor handle its own return logic. You must ensure it has a way to manually return when it finishes.
 */


/** A generic poolable niagara actor that already implements the IBFPooledObjectInterface and can be used for various situations involving vfx spawning/pooling at runtime, such as muzzle flashes, impact smoke, footsteps dust, etc. */
UCLASS(meta=(DisplayName="BF Poolable Niagara Actor"))
class BFOBJECTPOOLING_API ABFPoolableNiagaraActor : public AActor, public IBFPooledObjectInterface
{
	GENERATED_BODY()

public:
	ABFPoolableNiagaraActor(const FObjectInitializer& ObjectInitializer);
	virtual void PostInitializeComponents() override;
	virtual void BeginDestroy() override;
	virtual void OnObjectPooled_Implementation() override;

	
	// For easy fire and forget usage, will invalidate the Handle as the PoolActor now takes responsibility for returning based on our poolable actor params.
	UFUNCTION(BlueprintCallable, Category="BF|Poolable Sound Actor", meta=(DisplayName="Fire And Forget Initialize"))
	virtual void FireAndForgetBP(UPARAM(ref)FBFPooledObjectHandleBP& Handle, const FBFPoolableNiagaraActorDescription& ActivationParams, const FTransform& SystemTransform);

	void FireAndForget(TBFPooledObjectHandlePtr<ABFPoolableNiagaraActor, ESPMode::NotThreadSafe>& Handle, 
		const FBFPoolableNiagaraActorDescription& ActivationParams, const FVector& ActorLocation, const FRotator& ActorRotation) {FireAndForget(Handle, ActivationParams, FTransform{ActorRotation, ActorLocation});}

	void FireAndForget(TBFPooledObjectHandlePtr<ABFPoolableNiagaraActor, ESPMode::NotThreadSafe>& Handle, 
		const FBFPoolableNiagaraActorDescription& ActivationParams, const FVector& ActorLocation) {FireAndForget(Handle, ActivationParams, FTransform{ActorLocation});}
	
	// For easy fire and forget usage, will invalidate the Handle as the PoolActor now takes responsibility for returning based on our poolable actor params.
	virtual void FireAndForget(TBFPooledObjectHandlePtr<ABFPoolableNiagaraActor, ESPMode::NotThreadSafe>& Handle, 
		const FBFPoolableNiagaraActorDescription& ActivationParams, const FTransform& ActorTransform);
	

	
	/** Used when manually controlling this pooled actor, otherwise you should use FireAndForget. This is typically handed to the pooled actor because you are now ready to let the pooled actor handle its own
	 * state and return itself to the pool whenever it finishes or curfew elapses. */
	UFUNCTION(BlueprintCallable, Category="BF| Poolable Niagara Actor", meta = (DisplayName = "Set Pool Handle"))
	virtual void SetPoolHandleBP(UPARAM(ref)FBFPooledObjectHandleBP& Handle);

	/** Used when manually controlling this pooled actor, otherwise you should use FireAndForget. This is typically handed to the pooled actor because you are now ready to let the pooled actor handle its own
	 * state and return itself to the pool whenever it finishes or curfew elapses. */
	virtual void SetPoolHandle(TBFPooledObjectHandlePtr<ABFPoolableNiagaraActor, ESPMode::NotThreadSafe>& Handle);

	// If you are manually wanting to control the system then you can set its params here and call ActivatePoolableActor yourself if you want to just simply activate it and let it return when done see FireAndForget.
	UFUNCTION(BlueprintCallable, Category="BF| Poolable Niagara Actor")
	virtual void SetPoolableActorParams(const FBFPoolableNiagaraActorDescription& ActivationParams);

	/*	Activates the pooled actor, requires you to have already set the pooled actors ActivationInfo, ideally you would have set
	 *	everything for the actors state before calling this function such as its transforms and everything else needed. */ 
	UFUNCTION(BlueprintCallable, Category="BF| Poolable Niagara Actor")
	virtual void ActivatePoolableActor();

	/** Attempts to return our handle to the pool and returns true if it was successful, it may fail due to another copy already returning prior to us.
	* REQUIRES: You to have set the handle via SetPoolHandle or FireAndForget. */
	UFUNCTION(BlueprintCallable, Category="BF| Poolable Static Mesh Actor")
	virtual bool ReturnToPool();

	// Sets the pooled actors curfew on when it should return itself to the pool. Requires you to give the handle to the pooled actor otherwise when the elapsed curfew hits there isn't much it can do. 
	UFUNCTION(BlueprintCallable, Category="BF| Poolable Niagara Actor")
	virtual void SetCurfew(float SecondsUntilReturn);
	
	// Removes the current curfew if any set.
	UFUNCTION(BlueprintCallable, Category="BF| Poolable Niagara Actor")
	virtual void RemoveCurfew();

	// Resets the VFX System to its initial state.
	UFUNCTION(BlueprintCallable, Category="BF| Poolable Niagara Actor")
	virtual void ResetSystem();

	// Enables or disables the auto return on system finish. If enabled the system will attempt to return to the pool when it finishes but it requires you have to set the handle via SetPoolHandle or FireAndForget.
	UFUNCTION(BlueprintCallable, Category="BF| Poolable Niagara Actor")
	void SetAutoReturnOnSystemFinish(bool bShouldAutoReturnOnSoundFinish) { ActivationInfo.bAutoReturnOnSystemFinish = bShouldAutoReturnOnSoundFinish; }

	// Is this system set to automatically return to the pool when the system finishes.
	UFUNCTION(BlueprintCallable, Category="BF| Poolable Niagara Actor")
	bool GetAutoReturnOnSystemFinish() const { return ActivationInfo.bAutoReturnOnSystemFinish; }
	
	UFUNCTION(BlueprintCallable, Category="BF| Poolable Niagara Actor")
	bool HasSystemFinished() const { return bHasFinished; }
	
	UFUNCTION(BlueprintCallable, Category="BF| Poolable Niagara Actor")
	bool IsSystemLooping() const;
		
	template<typename T = UNiagaraComponent>
	T* GetNiagaraComponent() const { return CastChecked<T>(NiagaraComponent); }

	UFUNCTION(BlueprintCallable, Category="BF| Poolable Niagara Actor", meta = (DisplayName = "Get Niagara Component"))
	UNiagaraComponent* GetNiagaraComponentBP() const { return NiagaraComponent; }
	UNiagaraComponent* GetNiagaraComponent() const { return NiagaraComponent; }
		
	UFUNCTION(BlueprintCallable, Category="BF| Poolable Niagara Actor")
	UNiagaraSystem* GetNiagaraSystem() const;
	
protected:
	UFUNCTION(CallInEditor)
	virtual void OnNiagaraSystemFinished(UNiagaraComponent* FinishedComponent);

	virtual void OnCurfewExpired();
public:
	UPROPERTY(BlueprintAssignable, Category="BF| Poolable Niagara Actor")
	FOnPoolableNiagaraSystemFinished OnNiagaraSystemFinishedDelegate;
protected:
	/* BP pools store UObject handles for convenience and I cant template member functions (:
	 * So I have decided for everyone that we non ThreadSafe for performance benefits, you are using Multithreading with BP typically. You can always implement your own classes anyway.  */
	TBFPooledObjectHandlePtr<UObject, ESPMode::NotThreadSafe> BPObjectHandle = nullptr; 
	TBFPooledObjectHandlePtr<ABFPoolableNiagaraActor, ESPMode::NotThreadSafe> ObjectHandle = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BF| Poolable Niagara Actor")
	TObjectPtr<UNiagaraComponent> NiagaraComponent = nullptr;
	
	FBFPoolableNiagaraActorDescription ActivationInfo; 
	FTimerHandle CurfewTimerHandle;
	FTimerHandle DelayedActivationTimerHandle;

	UPROPERTY(Transient, BlueprintReadOnly)
	uint32 bHasFinished:1 = false;
	uint32 bIsUsingBPHandle:1 = false;
};






















