// Copyright (c) 2024 Jack Holland 
// Licensed under the MIT License. See LICENSE.md file in repo root for full license information.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "BFObjectPooling/GameplayActors/BFPoolableSoundActor.h"
#include "BFObjectPooling/GameplayActors/BFPoolableNiagaraActor.h"
#include "BFObjectPooling/GameplayActors/BFPoolableDecalActor.h"
#include "BFObjectPooling/GameplayActors/BFPoolableProjectileActor.h"
#include "BFObjectPooling/GameplayActors/BFPoolable3DWidgetActor.h"
#include "BFObjectPooling/GameplayActors/BFPoolableSkeletalMeshActor.h"
#include "BFObjectPooling/GameplayActors/BFPoolableStaticMeshActor.h"
#include "BFObjectPooling/Pool/BFObjectPool.h"
#include "BFGameplayFXSubsystem.generated.h"


// Inner struct for the GameplayFXSubsystem pools to define repetitive settings.
USTRUCT(BlueprintType)
struct BFOBJECTPOOLING_API FBFGameplayFXSubsystemPoolInitParams
{
	GENERATED_BODY()
public:
	// The maximum number of actors that can be pooled at any one time.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 PoolActorLimit = 0;

	// The initial number of actors to spawn when the pool is created, otherwise as we need more we will spawn them until we reach the limit.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 PoolActorInitialCount = 0;

	// Time after being returned to the pool that we must wait until we can consider un-pooling again.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float PoolActorCooldownTimeSeconds = -1.f;

	/** Defines the tick behaviour of the pool, ticking can ignored and it will be disabled by default.
	 * If disabled you lose the ability to clear out inactive objects that exceed the MaxObjectInactiveOccupancySeconds,
	 * tbh this is the desired behaviour most of the time anyway since the whole point of a pool is to pre-spawn and keep but who knows. */ 
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FBFObjectPoolInitTickParams PoolTickInfo;

	/** When this pool is at capacity and a new object is requested we try to see if there are any reclaimable objects we can force return,
	 * this defines how we determine which object to force return. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	EBFPooledObjectReclaimStrategy ForceReturnReclaimStrategy = EBFPooledObjectReclaimStrategy::Oldest;
};


USTRUCT(BlueprintType)
struct BFOBJECTPOOLING_API FBFGameplayFXSubsystemInitParams
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound")
	FBFGameplayFXSubsystemPoolInitParams SoundPoolParams;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Niagara")
	FBFGameplayFXSubsystemPoolInitParams NiagaraPoolParams;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Decal")
	FBFGameplayFXSubsystemPoolInitParams DecalPoolParams;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Projectile")
	FBFGameplayFXSubsystemPoolInitParams ProjectilePoolParams;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Widget3D")
	FBFGameplayFXSubsystemPoolInitParams Widget3DPoolParams;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="StaticMesh")
	FBFGameplayFXSubsystemPoolInitParams StaticMeshPoolParams;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SkeletalMesh")
	FBFGameplayFXSubsystemPoolInitParams SkeletalMeshPoolParams;
};

UENUM(BlueprintType)
enum struct EBFFXSubsystemPoolType : uint8
{
	Sound=0,
	Niagara,
	Decal,
	Projectile,
	Widget3D,
	SkeletalMesh,
	StaticMesh,
};


/** A Globally accessible FX Subsystem for various gameplay needs such as Sounds, Decals, Niagara Systems, etc.
 * The subsystems pools must first be initialized before any unpooling can occur.
 *
 * Initialization is a one time thing per world and should be done from some main framework class such as
 * the GameMode (if not in a network context) or the GameState, or even a child class of this subsystem could do it
 * inside of its OnWorldBeginPlay function, this subsystem is able to be extended and we won't instantiate this parent if a valid child exists.
 *
 * Each unpooled object has their own functionality for returning to the pool once completed, such as when the curfew elapses
 * or the object has more specific conditions in the provided description at unpool time. */
UCLASS()
class BFOBJECTPOOLING_API UBFGameplayFXSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
	
public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	static UBFGameplayFXSubsystem* Get(const UObject* ContextObject)
	{
		UWorld* World = GEngine->GetWorldFromContextObjectChecked(ContextObject);
		return World->GetSubsystem<UBFGameplayFXSubsystem>();
	}

	// Must be called before ANY other functions are called within this subsystem.
	UFUNCTION(BlueprintCallable, Category = "Effects")
	virtual void InitializePools(const FBFGameplayFXSubsystemInitParams& InInitParams);
	
	UFUNCTION(BlueprintCallable, Category = "Effects")
	virtual bool HasPoolsBeenInitialized() const { return bPoolsInitialized; }
	
	UFUNCTION(BlueprintCallable, Category = "Effects")
	virtual bool IsPoolInitialized(EBFFXSubsystemPoolType PoolType) const;


	/** Attempts to unpool and spawn a sound actor at the desired location, returns the unpooled actor if successful, the returned
	 * actor should *NOT* be stored and is only returned for special use cases where you need to set some params on the actor. */
	UFUNCTION(BlueprintCallable, Category = "Effects")
	virtual ABFPoolableSoundActor* SpawnSoundActor(const FBFPoolableSoundActorDescription& Description,
													EBFPooledObjectReclaimPolicy Policy,
													const FVector& Location,
													const FRotator& Rotation = FRotator::ZeroRotator);


	/** Attempts to unpool and spawn a niagara actor at the desired location, returns the unpooled actor if successful, the returned
	 * actor should *NOT* be stored and is only returned for special use cases where you need to set some params on the actor. */
	UFUNCTION(BlueprintCallable, Category = "Effects")
	virtual ABFPoolableNiagaraActor* SpawnNiagaraActor(const FBFPoolableNiagaraActorDescription& Description,
													EBFPooledObjectReclaimPolicy Policy,
													const FTransform& Transform);
	

	/** Attempts to unpool and spawn a decal actor at the desired location, returns the unpooled actor if successful, the returned
	 * actor should *NOT* be stored and is only returned for special use cases where you need to set some params on the actor. */
	UFUNCTION(BlueprintCallable, Category = "Effects")
	virtual ABFPoolableDecalActor* SpawnDecalActor(const FBFPoolableDecalActorDescription& Description,
													EBFPooledObjectReclaimPolicy Policy,
													const FTransform& Transform);


	/** Attempts to unpool and spawn a projectile actor at the desired location, returns the unpooled actor if successful, the returned
	 * actor should *NOT* be stored and is only returned for special use cases where you need to set some params on the actor. */
	UFUNCTION(BlueprintCallable, Category = "Effects")
	virtual ABFPoolableProjectileActor* SpawnProjectileActor(const FBFPoolableProjectileActorDescription& Description,
													EBFPooledObjectReclaimPolicy Policy,
													const FTransform& Transform);


	/** Attempts to unpool and spawn a 3D Widget actor at the desired location, returns the unpooled actor if successful, the returned
	 * actor should *NOT* be stored and is only returned for special use cases where you need to set some params on the actor. */
	UFUNCTION(BlueprintCallable, Category = "Effects")
	virtual ABFPoolable3DWidgetActor* Spawn3DWidgetActor(const FBFPoolable3DWidgetActorDescription& Description, 
														EBFPooledObjectReclaimPolicy Policy, 
														const FTransform& Transform);


	/** Attempts to unpool and spawn a Skeletal Mesh actor at the desired location, returns the unpooled actor if successful, the returned
	 * actor should *NOT* be stored and is only returned for special use cases where you need to set some params on the actor. */
	UFUNCTION(BlueprintCallable, Category = "Effects")
	virtual ABFPoolableSkeletalMeshActor* SpawnSkeletalMeshActor(const FBFPoolableSkeletalMeshActorDescription& Description, 
														EBFPooledObjectReclaimPolicy Policy, 
														const FTransform& Transform);


	/** Attempts to unpool and spawn a Static Mesh actor at the desired location, returns the unpooled actor if successful, the returned
	 * actor should *NOT* be stored and is only returned for special use cases where you need to set some params on the actor. */
	UFUNCTION(BlueprintCallable, Category = "Effects")
	virtual ABFPoolableStaticMeshActor* SpawnStaticMeshActor(const FBFPoolableStaticMeshActorDescription& Description, 
														EBFPooledObjectReclaimPolicy Policy, 
														const FTransform& Transform);
	



	UFUNCTION(BlueprintCallable, Category = "Effects")
	int32 GetSoundActorPoolNum() const { return SoundActorPool ? SoundActorPool->GetPoolLimit() : -1; }
	UFUNCTION(BlueprintCallable, Category = "Effects")
	int32 GetSoundActorPoolLimit() const { return SoundActorPool ? SoundActorPool->GetPoolLimit() : -1; }
	UFUNCTION(BlueprintCallable, Category = "Effects")
	int32 GetSoundActorPoolActiveNum() const { return SoundActorPool ? SoundActorPool->GetActivePoolNum() : -1; }
	UFUNCTION(BlueprintCallable, Category = "Effects")
	int32 GetSoundActorPoolInactiveNum() const { return SoundActorPool ? SoundActorPool->GetInactivePoolNum() : -1; }

	UFUNCTION(BlueprintCallable, Category = "Effects")
	int32 GetNiagaraActorPoolNum() const { return NiagaraActorPool ? NiagaraActorPool->GetPoolLimit() : -1; }
	UFUNCTION(BlueprintCallable, Category = "Effects")
	int32 GetNiagaraActorPoolLimit() const { return NiagaraActorPool ? NiagaraActorPool->GetPoolLimit() : -1; }
	UFUNCTION(BlueprintCallable, Category = "Effects")
	int32 GetNiagaraActorPoolActiveNum() const { return NiagaraActorPool ? NiagaraActorPool->GetActivePoolNum() : -1; }
	UFUNCTION(BlueprintCallable, Category = "Effects")
	int32 GetNiagaraActorPoolInactiveNum() const { return NiagaraActorPool ? NiagaraActorPool->GetInactivePoolNum() : -1; }

	UFUNCTION(BlueprintCallable, Category = "Effects")
	int32 GetDecalActorPoolNum() const { return DecalActorPool ? DecalActorPool->GetPoolLimit() : -1; }
	UFUNCTION(BlueprintCallable, Category = "Effects")
	int32 GetDecalActorPoolLimit() const { return DecalActorPool ? DecalActorPool->GetPoolLimit() : -1; }
	UFUNCTION(BlueprintCallable, Category = "Effects")
	int32 GetDecalActorPoolActiveNum() const { return DecalActorPool ? DecalActorPool->GetActivePoolNum() : -1; }
	UFUNCTION(BlueprintCallable, Category = "Effects")
	int32 GetDecalActorPoolInactiveNum() const { return DecalActorPool ? DecalActorPool->GetInactivePoolNum() : -1; }
	
	UFUNCTION(BlueprintCallable, Category = "Effects")
	int32 GetProjectileActorPoolNum() const { return ProjectileActorPool ? ProjectileActorPool->GetPoolLimit() : -1; }
	UFUNCTION(BlueprintCallable, Category = "Effects")
	int32 GetProjectileActorPoolLimit() const { return ProjectileActorPool ? ProjectileActorPool->GetPoolLimit() : -1; }
	UFUNCTION(BlueprintCallable, Category = "Effects")
	int32 GetProjectileActorPoolActiveNum() const { return ProjectileActorPool ? ProjectileActorPool->GetActivePoolNum() : -1; }
	UFUNCTION(BlueprintCallable, Category = "Effects")
	int32 GetProjectileActorPoolInactiveNum() const { return ProjectileActorPool ? ProjectileActorPool->GetInactivePoolNum() : -1; }
	
	UFUNCTION(BlueprintCallable, Category = "Effects")
	int32 Get3DWidgetActorPoolNum() const { return Widget3DActorPool ? Widget3DActorPool->GetPoolLimit() : -1; }
	UFUNCTION(BlueprintCallable, Category = "Effects")
	int32 Get3DWidgetActorPoolLimit() const { return Widget3DActorPool ? Widget3DActorPool->GetPoolLimit() : -1; }
	UFUNCTION(BlueprintCallable, Category = "Effects")
	int32 Get3DWidgetActorPoolActiveNum() const { return Widget3DActorPool ? Widget3DActorPool->GetActivePoolNum() : -1; }
	UFUNCTION(BlueprintCallable, Category = "Effects")
	int32 Get3DWidgetActorPoolInactiveNum() const { return Widget3DActorPool ? Widget3DActorPool->GetInactivePoolNum() : -1; }
	
	UFUNCTION(BlueprintCallable, Category = "Effects")
	int32 GetSkeletalMeshActorPoolNum() const { return SkeletalMeshActorPool ? SkeletalMeshActorPool->GetPoolLimit() : -1; }
	UFUNCTION(BlueprintCallable, Category = "Effects")
	int32 GetSkeletalMeshActorPoolLimit() const { return SkeletalMeshActorPool ? SkeletalMeshActorPool->GetPoolLimit() : -1; }
	UFUNCTION(BlueprintCallable, Category = "Effects")
	int32 GetSkeletalMeshActorPoolActiveNum() const { return SkeletalMeshActorPool ? SkeletalMeshActorPool->GetActivePoolNum() : -1; }
	UFUNCTION(BlueprintCallable, Category = "Effects")
	int32 GetSkeletalMeshActorPoolInactiveNum() const { return SkeletalMeshActorPool ? SkeletalMeshActorPool->GetInactivePoolNum() : -1; }

	UFUNCTION(BlueprintCallable, Category = "Effects")
	int32 GetStaticMeshActorPoolNum() const { return StaticMeshActorPool ? StaticMeshActorPool->GetPoolLimit() : -1; }
	UFUNCTION(BlueprintCallable, Category = "Effects")
	int32 GetStaticMeshActorPoolLimit() const { return StaticMeshActorPool ? StaticMeshActorPool->GetPoolLimit() : -1; }
	UFUNCTION(BlueprintCallable, Category = "Effects")
	int32 GetStaticMeshActorPoolActiveNum() const { return StaticMeshActorPool ? StaticMeshActorPool->GetActivePoolNum() : -1; }
	UFUNCTION(BlueprintCallable, Category = "Effects")
	int32 GetStaticMeshActorPoolInactiveNum() const { return StaticMeshActorPool ? StaticMeshActorPool->GetInactivePoolNum() : -1; }
	
protected:
	FBFGameplayFXSubsystemInitParams InitParams;
	
	TBFObjectPoolPtr<ABFPoolableSoundActor> SoundActorPool = nullptr;
	TBFObjectPoolPtr<ABFPoolableNiagaraActor> NiagaraActorPool = nullptr;
	TBFObjectPoolPtr<ABFPoolableDecalActor> DecalActorPool = nullptr;
	TBFObjectPoolPtr<ABFPoolableProjectileActor> ProjectileActorPool = nullptr;
	TBFObjectPoolPtr<ABFPoolable3DWidgetActor> Widget3DActorPool = nullptr;
	TBFObjectPoolPtr<ABFPoolableSkeletalMeshActor> SkeletalMeshActorPool = nullptr;
	TBFObjectPoolPtr<ABFPoolableStaticMeshActor> StaticMeshActorPool = nullptr;

	bool bPoolsInitialized = false;
};










