﻿// Copyright (c) 2024 Jack Holland 
// Licensed under the MIT License. See LICENSE.md file in repo root for full license information.

#pragma once
#include "BFObjectPooling/GameplayActors/BFPoolableActorHelpers.h"
#include "BFObjectPooling/PoolBP/BFObjectPoolBP.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BFObjectPooling/PoolBP/BFPooledObjectHandleBP.h"
#include "BFObjectPoolingBlueprintFunctionLibrary.generated.h"

struct FBFPooledObjectHandleBP;

DECLARE_DYNAMIC_DELEGATE_RetVal_OneParam(bool, FUnpoolPredicate, const UObject*, Object);

// Blueprint Function library for interfacing with pooled objects from blueprint.
UCLASS(DisplayName = "BF Object Pooling Library")
class BFOBJECTPOOLING_API UBFObjectPoolingBlueprintFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	// SUPER important and required by any pool before being able to use it.
	UFUNCTION(BlueprintCallable, Category = "BF Object Pooling")
	static void InitializeObjectPool(UPARAM(ref)FBFObjectPoolBP& Pool, const FBFObjectPoolInitParams& PoolInfo);
	
	/** Attempts to un-pool an object, can only fail if the pool is at capacity and all objects are in use. The return OutHandle should
	 * be stored somewhere otherwise as soon as it exits scope the object will be returned,  the ReturnObject is for any setup logic within
	 * scope but should NOT be stored, you should ALWAYS attempt to freshly get a hold of the object via its handle when you arent sure. */
	UFUNCTION(BlueprintCallable, Category = "BF Object Pooling", meta=(ExpandEnumAsExecs="Result"))
	static void UnpoolObject(UPARAM(ref)FBFObjectPoolBP& Pool, UPARAM(ref)FBFPooledObjectHandleBP& OutHandle,
								EBFPooledObjectReclaimPolicy Policy, EBFSuccess& Result,
								UObject*& ReturnObject, bool bAutoActivate = true);

	
	/** Attempts to un-pool the first matching object to the tag and return it via handle, if unable to locate a matching object
	 * or have no free objects function may fail. The handle is your responsibility to manage, once destroyed the handle will automatically
	 * return the object to the pool if it has not already been returned. You should not store the return object and always try
	 * get it from the handle when using it as it may may be invalid due to another copy already returning the object or it being stolen. */
	UFUNCTION(BlueprintCallable, Category = "BF Object Pooling", meta=(ExpandEnumAsExecs="Result"))
	static void UnpoolObjectByTag(UPARAM(ref)FBFObjectPoolBP& Pool, FGameplayTag Tag, UPARAM(ref)FBFPooledObjectHandleBP& OutHandle,
								EBFPooledObjectReclaimPolicy Policy, EBFSuccess& Result,
								UObject*& ReturnObject,  bool bAutoActivate = true);

	
	/** Unpools the first inactive pooled object to pass the Predicate (Predicates are just functions that return true or false, in
	 * this case if it returns true then that object it returned true for will be the object you get back as a handle).
	 * If no results return true then the handle will be invalid. */
	UFUNCTION(BlueprintCallable, Category = "BF Object Pooling", meta=(ExpandEnumAsExecs="Result"))
	static void UnpoolObjectByPredicate(UPARAM(ref)FBFObjectPoolBP& Pool, UPARAM(ref)FBFPooledObjectHandleBP& OutHandle, FUnpoolPredicate Predicate,
										EBFPooledObjectReclaimPolicy Policy, EBFSuccess& Result,
										UObject*& ReturnObject, bool bAutoActivate = true);

	
	// Returns null if the handle is invalid due to another copy already returning the object or the object was stolen from the pool.
	UFUNCTION(BlueprintCallable, Category = "BF Object Pooling", meta=(ExpandEnumAsExecs="Result"))
	static void GetObjectFromHandle(UPARAM(ref)FBFPooledObjectHandleBP& Handle, UObject*& PooledObject, EBFSuccess& Result);

	/**Takes the provided object and attempts to "adopt" it into the pool, we can fail this if
	 * - The Object is null or not a matching or child class of the provided pools class type.
	 * - If the pool is already at capacity and we can't fit it. */
	UFUNCTION(BlueprintCallable, Category = "BF Object Pooling")
	static bool AdoptObject(UPARAM(ref)FBFObjectPoolBP& Pool, UObject* Object);
	
	/**Stealing a pooled object means to take ownership of the object, this means
	 * the pool frees any references to the object and its as if it never existed to it.
	 * This is why it is important not to cache or store pooled objects directly and instead access them via their handle struct since it will always query for validity before trying to return the object.*/
	UFUNCTION(BlueprintCallable, Category = "BF Object Pooling", meta=(ExpandEnumAsExecs="Result"))
	static void StealPooledObjectFromHandle(UPARAM(ref)FBFPooledObjectHandleBP& Handle, UObject*& PooledObject, EBFSuccess& Result);
	
	// Attempts to return the object back to the pool via its handle, if the handle is invalid or stale this will return false.
	UFUNCTION(BlueprintCallable, Category = "BF Object Pooling", meta=(Keywords="Invalidate, Clear, Return, Empty, Reset"))
	static bool ReturnPooledObject(UPARAM(ref)FBFPooledObjectHandleBP& Handle);

	// Invalidates this copy of the handle, if this is the last copy of the handle the object will be returned to the pool automatically.
	UFUNCTION(BlueprintCallable, Category = "BF Object Pooling", meta=(Keywords="Invalidate, Clear, Return, Empty, Reset"))
	static void InvalidateHandle(UPARAM(ref)FBFPooledObjectHandleBP& Handle);
	
	// Checks not only object validity but ensures IDs match in-case someone else was to return or steal the object and this handle is now invalid.
	UFUNCTION(BlueprintCallable, Category = "BF Object Pooling",  meta=(ExpandEnumAsExecs="Result"))
	static void IsPooledObjectHandleValid(UPARAM(ref)FBFPooledObjectHandleBP& Handle, EBFSuccess& Result, bool& bIsValid);

	// Checks if the pool has been initialized.
	UFUNCTION(BlueprintCallable, Category = "BF Object Pooling",  meta=(ExpandEnumAsExecs="Result"))
	static void IsPoolValid(UPARAM(ref)FBFObjectPoolBP& Pool, EBFSuccess& Result, bool& bIsValid);


	/** Returns the handles internally stored ObjectID and CheckoutID, even if the handle itself has gone invalid.
	 * ObjectIDs are assigned at creation of the object and persist for that pooled objects lifetime, CheckoutIDs are continuously incremented
	 * every time the object is returned or taken from the pool. This allows two handles one old and stale and another new to be compared.
	 * It also lets multiple handles exist at the same time and when one handle is returned all other handles become "invalid" (but still have access to their old ID's). */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "BF Object Pooling")
	static void GetUnpooledObjectID(UPARAM(ref)FBFPooledObjectHandleBP& Handle, int64& ObjectID, int32& CheckoutID);

	
	// If ticking is enabled on the pool, each tick all inactive objects are evaluated and if they exceed the time they are removed from the pool.
	UFUNCTION(BlueprintCallable, Category = "BF Object Pooling")
	static void SetMaxObjectInactiveOccupancySeconds(UPARAM(ref)FBFObjectPoolBP& Pool, float MaxInactiveObjectOccupancySeconds);
	
	// Time in seconds that inactive objects can exist in the pool before they are removed (if the pool is ticking).
	UFUNCTION(BlueprintCallable, Category = "BF Object Pooling")
	static float GetMaxObjectInactiveOccupancySeconds(UPARAM(ref)FBFObjectPoolBP& Pool);

	
	// If the object pool has tick enabled it will check for inactive objects and return them to the pool if that behavior is enabled, otherwise not much use to having a ticking pool.
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "BF Object Pooling")
	static bool IsObjectPoolTickEnabled(UPARAM(ref)FBFObjectPoolBP& Pool);

	
	// Checks if the pool matches the class type.
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "BF Object Pooling")
	static bool IsObjectPoolOfType(UPARAM(ref)FBFObjectPoolBP& Pool, UClass* ClassType);

	
	// If the object pool has tick enabled it will periodically check for inactive objects and return them to the pool if that behavior is enabled, otherwise not much use to having a ticking pool.
	UFUNCTION(BlueprintCallable, Category = "BF Object Pooling")
	static void SetObjectPoolTickEnabled(UPARAM(ref)FBFObjectPoolBP& Pool, bool bEnabled);

	
	// How often the pool will tick and check for inactive objects to clear if that behavior is enabled.
	UFUNCTION(BlueprintCallable, Category = "BF Object Pooling")
	static void SetObjectPoolTickInterval(UPARAM(ref)FBFObjectPoolBP& Pool, float TickRate);

	
	// Returns the limit of the object pool, this is the maximum number of objects that can be in the pool at any given time.
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "BF Object Pooling")
	static int32 GetObjectPoolLimit(UPARAM(ref)FBFObjectPoolBP& Pool);

	
	/**Sets the limit of the object pool, this can return false if we do not have enough inactive objects to meet the new limit if we are reducing the limit.
	 * Only inactive objects can be removed from the pool, if you want to remove an active object from the pool then use TryStealPooledObjectFromHandle.*/
	UFUNCTION(BlueprintCallable, Category = "BF Object Pooling")
	static bool SetObjectPoolLimit(UPARAM(ref)FBFObjectPoolBP& Pool, int32 NewLimit);

	
	// Returns the number of objects within the pool, this is the **total** (inactive and active) number of objects that are currently in the pool.
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "BF Object Pooling", DisplayName=GetObjectPoolNum)
	static int32 GetObjectPoolSize(UPARAM(ref)FBFObjectPoolBP& Pool);

	
	// Returns the number of active objects in the pool
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "BF Object Pooling", DisplayName=GetObjectPoolActiveObjectsNum)
	static int32 GetObjectPoolActiveObjectsSize(UPARAM(ref)FBFObjectPoolBP& Pool);

	
	// Returns the number of inactive objects in the pool
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "BF Object Pooling", DisplayName=GetObjectPoolInactiveObjectsNum)
	static int32 GetObjectPoolInactiveObjectsSize(UPARAM(ref)FBFObjectPoolBP& Pool);

	
	// Attempts to clear any inactive objects that exceed our MaxInactiveOccupancySeconds
	UFUNCTION(BlueprintCallable, Category = "BF Object Pooling")
	static bool ClearObjectPoolInactiveObjects(UPARAM(ref)FBFObjectPoolBP& Pool);


		
	/** Utility for quickly attempting to un-pool a "ABFPoolableStaticMeshActor" from the pool and call FireAndForget on it allowing it to return itself back to the pool when ready,
	 * the ReturnObject will be null if the function fails, you should NOT store the object but only use it for any setup logic you need after its been un-pooled, can be ignored. */
	UFUNCTION(BlueprintCallable, Category = "BF Object Pooling", meta=(ExpandEnumAsExecs="Result"))
	static void QuickUnpoolStaticMeshActor(UPARAM(ref)FBFObjectPoolBP& Pool, const FBFPoolableStaticMeshActorDescription& InitParams,
											EBFPooledObjectReclaimPolicy Policy, const FTransform& ActorTransform,
											EBFSuccess& Result, UObject*& ReturnObject);

		
	/** Utility for quickly attempting to un-pool a "ABFPoolableSkeletalMeshActor" from the pool and call FireAndForget on it allowing it to return itself back to the pool when ready,
	 * the ReturnObject will be null if the function fails, you should NOT store the object but only use it for any setup logic you need after its been un-pooled, can be ignored. */
	UFUNCTION(BlueprintCallable, Category = "BF Object Pooling", meta=(ExpandEnumAsExecs="Result"))
	static void QuickUnpoolSkeletalMeshActor(UPARAM(ref)FBFObjectPoolBP& Pool, const FBFPoolableSkeletalMeshActorDescription& InitParams, 
											EBFPooledObjectReclaimPolicy Policy, const FTransform& ActorTransform, 
											EBFSuccess& Result, UObject*& ReturnObject);

	
	/** Utility for quickly attempting to un-pool a "ABFPoolableProjectileActor" from the pool and call FireAndForget on it allowing it to return itself back to the pool when ready,
	 * the ReturnObject will be null if the function fails, you should NOT store the object but only use it for any setup logic you need after its been un-pooled, can be ignored. */
	UFUNCTION(BlueprintCallable, Category = "BF Object Pooling", meta=(ExpandEnumAsExecs="Result"))
	static void QuickUnpoolProjectileActor(UPARAM(ref)FBFObjectPoolBP& Pool, const FBFPoolableProjectileActorDescription& InitParams, 
											EBFPooledObjectReclaimPolicy Policy, const FTransform& ActorTransform, 
											EBFSuccess& Result, UObject*& ReturnObject);


	
	/** Utility for quickly attempting to un-pool a "ABFPoolableNiagaraActor" from the pool and call FireAndForget on it allowing it to return itself back to the pool when ready,
	 * the ReturnObject will be null if the function fails, you should NOT store the object but only use it for any setup logic you need after its been un-pooled, can be ignored. */
	UFUNCTION(BlueprintCallable, Category = "BF Object Pooling", meta=(ExpandEnumAsExecs="Result"))
	static void QuickUnpoolNiagaraActor(UPARAM(ref)FBFObjectPoolBP& Pool, const FBFPoolableNiagaraActorDescription& InitParams, 
											EBFPooledObjectReclaimPolicy Policy, const FTransform& ActorTransform, 
											EBFSuccess& Result, UObject*& ReturnObject);


	
	/** Utility for quickly attempting to un-pool a "ABFPoolableSoundActor" from the pool and call FireAndForget on it allowing it to return itself back to the pool when ready,
	 * the ReturnObject will be null if the function fails, you should NOT store the object but only use it for any setup logic you need after its been un-pooled, can be ignored. */
	UFUNCTION(BlueprintCallable, Category = "BF Object Pooling", meta=(ExpandEnumAsExecs="Result"))
	static void QuickUnpoolSoundActor(UPARAM(ref)FBFObjectPoolBP& Pool, const FBFPoolableSoundActorDescription& InitParams, 
											EBFPooledObjectReclaimPolicy Policy, const FTransform& ActorTransform, 
											EBFSuccess& Result, UObject*& ReturnObject);


	
	/** Utility for quickly attempting to un-pool a "ABFPoolableDecalActor" from the pool and call FireAndForget on it allowing it to return itself back to the pool when ready,
	 * the ReturnObject will be null if the function fails, you should NOT store the object but only use it for any setup logic you need after its been un-pooled, can be ignored. */
	UFUNCTION(BlueprintCallable, Category = "BF Object Pooling", meta=(ExpandEnumAsExecs="Result"))
	static void QuickUnpoolDecalActor(UPARAM(ref)FBFObjectPoolBP& Pool, const FBFPoolableDecalActorDescription& InitParams, 
											EBFPooledObjectReclaimPolicy Policy, const FTransform& ActorTransform, 
											EBFSuccess& Result, UObject*& ReturnObject);



	
	/** Utility for quickly attempting to un-pool a "ABFPoolable3DWidgetActor" from the pool and call FireAndForget on it allowing it to return itself back to the pool when ready,
	 * the ReturnObject will be null if the function fails, you should NOT store the object but only use it for any setup logic you need after its been un-pooled, can be ignored. */
	UFUNCTION(BlueprintCallable, Category = "BF Object Pooling", meta=(ExpandEnumAsExecs="Result"))
	static void QuickUnpool3DWidgetActor(UPARAM(ref)FBFObjectPoolBP& Pool, const FBFPoolable3DWidgetActorDescription& InitParams, 
											EBFPooledObjectReclaimPolicy Policy, const FTransform& ActorTransform, 
											EBFSuccess& Result, UObject*& ReturnObject);



	
	// Internal use only.
	UFUNCTION(BlueprintPure, BlueprintInternalUseOnly)
	static FBFCollisionShapeDescription MakeCollisionStruct(EBFCollisionShapeType CollisionShape, FCollisionProfileName CollisionProfile, float XParam, float YParam, float ZParam)
	{
		return { CollisionShape, CollisionProfile, FVector{XParam, YParam, ZParam} };
	}
	
};























