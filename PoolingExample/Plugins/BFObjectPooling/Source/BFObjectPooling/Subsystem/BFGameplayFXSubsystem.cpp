// Copyright (c) 2024 Jack Holland 
// Licensed under the MIT License. See LICENSE.md file in repo root for full license information.


#include "BFGameplayFXSubsystem.h"

bool UBFGameplayFXSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (GetClass() == UBFGameplayFXSubsystem::StaticClass())
	{
		TArray<UClass*> ChildClasses;
		GetDerivedClasses(UBFGameplayFXSubsystem::StaticClass(), ChildClasses, true);
		for (UClass* Child : ChildClasses)
		{
			if (Child->GetDefaultObject<UBFGameplayFXSubsystem>()->ShouldCreateSubsystem(Outer))
			{
				// Do not create this class because one of our child classes wants to be created
				return false;
			}
		}
	}
	return true;
}


void UBFGameplayFXSubsystem::InitializePools(const FBFGameplayFXSubsystemInitParams& InInitParams)
{
	if (HasPoolsBeenInitialized())
	{
#if !UE_BUILD_SHIPPING
		GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Red,
			TEXT("Failed to initialize pools because they have already been initialized."));
#endif
		return;
	}

	static auto InitPool = []<typename T>(	const FBFGameplayFXSubsystemPoolInitParams& PoolParams,
											FBFObjectPoolInitParams& PoolInitParams,
											TBFObjectPoolPtr<T, ESPMode::NotThreadSafe>& Pool)
	{
		if (PoolParams.PoolActorLimit == 0)
			return;
		
		PoolInitParams.PoolClass = T::StaticClass();
		PoolInitParams.PoolLimit = PoolParams.PoolActorLimit;
		PoolInitParams.InitialCount = PoolParams.PoolActorInitialCount;
		PoolInitParams.CooldownTimeSeconds = PoolParams.PoolActorCooldownTimeSeconds;
		PoolInitParams.ForceReturnReclaimStrategy = PoolParams.ForceReturnReclaimStrategy;
		PoolInitParams.PoolTickInfo = PoolParams.PoolTickInfo;
		
		Pool = TBFObjectPool<T>::CreateAndInitPool(PoolInitParams);
	};

	InitParams = InInitParams;
	
	FBFObjectPoolInitParams PoolInitParams;
	PoolInitParams.Owner = this;
	PoolInitParams.World = GetWorld();

	// Initialize any pool that has been given a limit.
	InitPool(InitParams.SoundPoolParams,		PoolInitParams, SoundActorPool);
	InitPool(InitParams.NiagaraPoolParams,		PoolInitParams, NiagaraActorPool);
	InitPool(InitParams.DecalPoolParams,		PoolInitParams, DecalActorPool);
	InitPool(InitParams.ProjectilePoolParams,	PoolInitParams, ProjectileActorPool);
	InitPool(InitParams.Widget3DPoolParams,		PoolInitParams, Widget3DActorPool);
	InitPool(InitParams.SkeletalMeshPoolParams, PoolInitParams, SkeletalMeshActorPool);
	InitPool(InitParams.StaticMeshPoolParams,	PoolInitParams, StaticMeshActorPool);
	
	bPoolsInitialized = true;
}

bool UBFGameplayFXSubsystem::IsPoolInitialized(EBFFXSubsystemPoolType PoolType) const
{
	switch (PoolType)
	{
	case EBFFXSubsystemPoolType::Sound:			return SoundActorPool != nullptr;
	case EBFFXSubsystemPoolType::Niagara:		return NiagaraActorPool != nullptr;
	case EBFFXSubsystemPoolType::Decal:			return DecalActorPool != nullptr;
	case EBFFXSubsystemPoolType::Projectile: 	return ProjectileActorPool != nullptr;
	case EBFFXSubsystemPoolType::Widget3D:		return Widget3DActorPool != nullptr;
	case EBFFXSubsystemPoolType::SkeletalMesh:	return SkeletalMeshActorPool != nullptr;
	case EBFFXSubsystemPoolType::StaticMesh:		return StaticMeshActorPool != nullptr;
	}
	return false;
}


ABFPoolableSoundActor* UBFGameplayFXSubsystem::SpawnSoundActor(const FBFPoolableSoundActorDescription& Description,
																EBFPooledObjectReclaimPolicy Policy,
																const FVector& Location,
																const FRotator& Rotation)
{
	SCOPED_NAMED_EVENT(UBFGameplayFXSubsystem_SpawnSoundActor, FColor::Orange);
	if (!HasPoolsBeenInitialized() || SoundActorPool == nullptr)
	{
#if !UE_BUILD_SHIPPING
		if (!HasPoolsBeenInitialized())
		{
			GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Red, 
				TEXT("Failed to spawn sound actor because pools have not been initialized, ensure you first call InitializePools on the subsystem."));
		}
		else
		{
			GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Red, 
				TEXT("Failed to spawn sound actor because that pool type was not given an initial count in the init params."));
		}
#endif
		return nullptr;
	}

	
	if (TBFPooledObjectHandlePtr<ABFPoolableSoundActor> Handle = SoundActorPool->UnpoolObject(true, Policy))
	{
		ABFPoolableSoundActor* Actor = Handle->GetObject();
		Actor->FireAndForget(Handle, Description, FTransform{Rotation, Location});
		
		// Sound components some times are denied from playing if there are too many instances of it already playing. It is unreals functionality, not mine.
		if (Actor->GetAudioComponent()->IsPlaying())
			return Actor;
	}
	return nullptr;
}


ABFPoolableNiagaraActor* UBFGameplayFXSubsystem::SpawnNiagaraActor(const FBFPoolableNiagaraActorDescription& Description,
																	EBFPooledObjectReclaimPolicy Policy,
																	const FTransform& Transform)
{
	SCOPED_NAMED_EVENT(UBFGameplayFXSubsystem_SpawnNiagaraActor, FColor::Orange);
	if (!HasPoolsBeenInitialized() || NiagaraActorPool == nullptr)
	{
#if !UE_BUILD_SHIPPING
		if (!HasPoolsBeenInitialized())
		{
			GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Red, 
				TEXT("Failed to spawn niagara actor because pools have not been initialized, ensure you first call InitializePools on the subsystem."));
		}
		else
		{
			GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Red, 
				TEXT("Failed to spawn niagara actor because that pool type was not given an initial count in the init params."));
		}
#endif
		return nullptr;
	}
	
	
	if (TBFPooledObjectHandlePtr<ABFPoolableNiagaraActor> Handle = NiagaraActorPool->UnpoolObject(true, Policy))
	{
		ABFPoolableNiagaraActor* Actor = Handle->GetObject();
		Actor->FireAndForget(Handle, Description, Transform);
		return Actor;
	}
	return nullptr;
}


ABFPoolableDecalActor* UBFGameplayFXSubsystem::SpawnDecalActor(	const FBFPoolableDecalActorDescription& Description,
																EBFPooledObjectReclaimPolicy Policy,
																const FTransform& Transform)
{
	SCOPED_NAMED_EVENT(UBFGameplayFXSubsystem_SpawnDecalActor, FColor::Orange);
	if (!HasPoolsBeenInitialized() || DecalActorPool == nullptr)
	{
#if !UE_BUILD_SHIPPING
		if (!HasPoolsBeenInitialized())
		{
			GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Red, 
				TEXT("Failed to spawn decal actor because pools have not been initialized, ensure you first call InitializePools on the subsystem."));
		}
		else
		{
			GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Red, 
				TEXT("Failed to spawn decal actor because that pool type was not given an initial count in the init params."));
		}
#endif
		return nullptr;
	}
	
	
	if (TBFPooledObjectHandlePtr<ABFPoolableDecalActor> Handle = DecalActorPool->UnpoolObject(true, Policy))
	{
		ABFPoolableDecalActor* Actor = Handle->GetObject();
		Actor->FireAndForget(Handle, Description, Transform);
		return Actor;
	}
	return nullptr;	
}

ABFPoolableProjectileActor* UBFGameplayFXSubsystem::SpawnProjectileActor(	const FBFPoolableProjectileActorDescription& Description,
																			EBFPooledObjectReclaimPolicy Policy,
																			const FTransform& Transform)
{
	SCOPED_NAMED_EVENT(UBFGameplayFXSubsystem_SpawnProjectileActor, FColor::Orange);
	if (!HasPoolsBeenInitialized() || ProjectileActorPool == nullptr)
	{
#if !UE_BUILD_SHIPPING
		if (!HasPoolsBeenInitialized())
		{
			GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Red, 
				TEXT("Failed to spawn projectile actor because pools have not been initialized, ensure you first call InitializePools on the subsystem."));
		}
		else
		{
			GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Red, 
				TEXT("Failed to spawn projectile actor because that pool type was not given an initial count in the init params."));
		}
#endif
		return nullptr;
	}
	
	
	if (TBFPooledObjectHandlePtr<ABFPoolableProjectileActor> Handle = ProjectileActorPool->UnpoolObject(true, Policy)	)
	{
		ABFPoolableProjectileActor* Actor = Handle->GetObject();
		Actor->FireAndForget(Handle, Description, Transform);
		return Actor;
	}
	return nullptr;	
}

ABFPoolable3DWidgetActor* UBFGameplayFXSubsystem::Spawn3DWidgetActor(	const FBFPoolable3DWidgetActorDescription& Description,
																		EBFPooledObjectReclaimPolicy Policy,
																		const FTransform& Transform)
{
	SCOPED_NAMED_EVENT(UBFGameplayFXSubsystem_Spawn3DWidgetActor, FColor::Orange);
	if (!HasPoolsBeenInitialized() || Widget3DActorPool == nullptr)
	{
#if !UE_BUILD_SHIPPING
		if (!HasPoolsBeenInitialized())
		{
			GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Red, 
				TEXT("Failed to spawn 3D widget actor because pools have not been initialized, ensure you first call InitializePools on the subsystem."));
		}
		else
		{
			GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Red, 
				TEXT("Failed to spawn 3D widget actor because that pool type was not given an initial count in the init params."));
		}
#endif
		return nullptr;
	}
	
	
	if (TBFPooledObjectHandlePtr<ABFPoolable3DWidgetActor> Handle = Widget3DActorPool->UnpoolObject(true, Policy))
	{
		ABFPoolable3DWidgetActor* Actor = Handle->GetObject();
		Actor->FireAndForget(Handle, Description, Transform);
		return Actor;
	}
	return nullptr;
}


ABFPoolableSkeletalMeshActor* UBFGameplayFXSubsystem::SpawnSkeletalMeshActor(	const FBFPoolableSkeletalMeshActorDescription& Description,
																				EBFPooledObjectReclaimPolicy Policy,
																				const FTransform& Transform)
{
	SCOPED_NAMED_EVENT(UBFGameplayFXSubsystem_SpawnSkeletalActor, FColor::Orange);
	if (!HasPoolsBeenInitialized() || SkeletalMeshActorPool == nullptr)
	{
#if !UE_BUILD_SHIPPING
		if (!HasPoolsBeenInitialized())
		{
			GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Red, 
				TEXT("Failed to spawn skeletal mesh actor because pools have not been initialized, ensure you first call InitializePools on the subsystem."));
		}
		else
		{
			GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Red, 
				TEXT("Failed to spawn skeletal mesh actor because that pool type was not given an initial count in the init params."));
		}
#endif
		return nullptr;
	}
	
	if (TBFPooledObjectHandlePtr<ABFPoolableSkeletalMeshActor> Handle = SkeletalMeshActorPool->UnpoolObject(true, Policy))
	{
		ABFPoolableSkeletalMeshActor* Actor = Handle->GetObject();
		Actor->FireAndForget(Handle, Description, Transform);
		return Actor;
	}
	return nullptr;
}


ABFPoolableStaticMeshActor* UBFGameplayFXSubsystem::SpawnStaticMeshActor(	const FBFPoolableStaticMeshActorDescription& Description,
																			EBFPooledObjectReclaimPolicy Policy,
																			const FTransform& Transform)
{
	SCOPED_NAMED_EVENT(UBFGameplayFXSubsystem_SpawnStaticMeshActor, FColor::Orange);
	if (!HasPoolsBeenInitialized() || StaticMeshActorPool == nullptr)
	{
#if !UE_BUILD_SHIPPING
		if (!HasPoolsBeenInitialized())
		{
			GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Red, 
				TEXT("Failed to spawn static mesh actor because pools have not been initialized, ensure you first call InitializePools on the subsystem."));
		}
		else
		{
			GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Red, 
				TEXT("Failed to spawn static mesh actor because that pool type was not given an initial count in the init params."));
		}
#endif
		return nullptr;
	}

	
	if (TBFPooledObjectHandlePtr<ABFPoolableStaticMeshActor> Handle = StaticMeshActorPool->UnpoolObject(true, Policy))
	{
		ABFPoolableStaticMeshActor* Actor = Handle->GetObject();
		Actor->FireAndForget(Handle, Description, Transform);
		return Actor;
	}
	return nullptr;
}



