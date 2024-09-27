// Copyright (c) 2024 Jack Holland 
// Licensed under the MIT License. See LICENSE.md file in repo root for full license information.

#pragma once
#include "BFObjectPooling/Pool/Private/BFObjectPoolHelpers.h"
#include "BFObjectPooling/Pool/Private/BFPoolContainer.h"
#include "BFObjectPooling/Interfaces/BFPooledObjectInterface.h" 
#include "BFObjectPooling/Module/BFObjectPooling.h"
#include "BFPooledObjectHandle.h"
#include "GameplayTags.h"
#include "Blueprint/UserWidget.h"
#include "BFObjectPool.generated.h"

struct FBFPooledObjectInfo;
class UBFPoolContainer;


template<typename T, ESPMode Mode = ESPMode::NotThreadSafe>
using TBFObjectPoolPtr = TSharedPtr<TBFObjectPool<T, Mode>, Mode>;


/** TBFObjectPool:
 * 
 * This pool aims to be a generic object pool that can be used to pool any Actor, Component, UserWidget or UObject.
 * The pool was primarily designed with c++ in mind but has full support for BP and even some QOL features that c++ doesn't. (Mainly the QuickUnpoolX where X is a built in poolable actor type,
 *																												see GameplayActors folder and BFObjectPoolingBlueprintFunctionLibrary for more info).
 *
 * 
 * The Pool Is Primarily Composed Of 3 Main Parts
 *
 * 1)- The Templated Struct Pool itself: `TBFObjectPool`
 *		- This handles interfacing and dealing with the pooled objects.
 *		- Proves easy to use API for getting, returning and stealing objects from the pool.
 *		
 * 2)- The Pool Container: `UBFPoolContainer`
 *		- This is only internally used by the pool and should not be interacted with directly.
 *		- Stores reflected pointers of all pooled objects and their information.
 *
 * 3)- The Pooled Object Handle: `TBFPooledObjectHandle` which is primary interacted with via shared pointer alias `TBFPooledObjectHandlePtr` for convenience.
 *		- when attempting to un-pool an object it is returned to you via shared pointer and wrapped within this handle struct, the returned pointer may only be null due to the pool being at capacity, otherwise it will always be a valid shared pointer to your new handle.
 *		- If you let all shared pointers to the handle go out of scope, the object will be returned to the pool automatically (assuming its able to and the object hasn't already been returned to the pool).
 *		- BP side wraps shared pointers via a struct and has easy to use function library API for interacting with the pool.
 *		- Due to sharing and copying of handle pointers you should always check for IsHandleValid() before using the handle if you are within a new scope where you can't be certain. This is
 *			because it may have already returned itself to the pool or it may have been "Stolen" via `StealObject()`. The IsHandleValid() function not only checks the objects validity but also compares our handle ID to see if
 *			it matches the pools latest ID, if it doesn't then the handle is considered invalid. 
 *			You should never cache the object directly but only ever access it for short scopes if you need to init or do any setup behaviour.
 *
 *
 * The pool is designed to be able to regularly query the inactive objects and remove them if they exceed the specified MaxObjectInactiveOccupancySeconds in the pool init function.
 * If you do not want this behaviour, you can leave ticking disabled on the pool. You also can optionally set the tick interval to a higher value to reduce the overhead of checking the pools occupancy.
 * NOTE: The pools debug console variables `BF.OP.PrintPoolOccupancy` and `BF.OP.EnableLogging` can help when debugging issues, the `PrintPoolOccupancy` requires your pool to enable ticking otherwise it will not log the pools occupancy.
 *
 *
 * Increasing the object pool limit is possible, however, decreasing it is only possible if there are enough inactive objects to remove to reach the new limit.
 * We do not remove active objects unless they are manually stolen via the `StealObject()` method.
 *
 *
 *
 * Creation of the pool must and should be done as a shared pointer, TBFObjectPoolPtr is a using alias to make it a little nicer.
 * Usage goes as follows:
 *
 * // Header file or wherever you want to store the pool.
 * // ESP mode is optional and defaults to NotThreadSafe for perf benefits since most actors/objects are game thread bound.
 * TBFObjectPoolPtr<AMyFoo> MyPool; 
 *
 *
 *
 * // BeginPlay or some other init function where you want to create the pool.
 * MyPool = TBFObjectPoolPtr<AMyFoo>::CreatePool(); // Use static factory Create method to create the pool and make sure to Init it before ANYTHING else.
 * FBFObjectPoolInitParams Params;
 * Params.FillOutDefaults = whatever;
 * MyPool->InitPool(Params);
 *
 *
 *
 * // Now during gameplay whenever you want to retrieve a pooled object you can do so like this:
 * 	if(TBFPooledObjectHandlePtr<AMyFoo> Handle = ObjectPool->UnpoolObject(bAutoActivate))// Returns a pooled object via a shared handle ptr, if the param is false you must handle activating the object yourself, there is built in activation/deactivation logic already though.
 *	{
 *		// Do whatever you want with the object, my preferred method is to init the object and hand it back its own handle so it can do what it needs to do and return when needed. Like a sound would set the params and then return on sound finished
 *		auto* Obj = Handle->GetObject(); // This will templated in c++ and in BP you get the object as a UObject* for you to then cast. Do not store Obj but just use it in scope.
 *		Obj->SomeObjectFunctionToSetThingsUp()
 *		Obj->SetPoolHandle(Handle); // This is a function I typically have on my pooled objects that takes the handle and stores it for later use, also invalidates your handle so you can't use it again.
 *	}
 *	
 *
 * MyPool->UnpoolObject(bAutoActivate); // Attempts to un-pool an object and return it via a shared handle ptr, the result will return a null pointer if unable to un-pool an object due to pool being at capacity and all objects in use.
 * MyPool->UnpoolObjectByTag(Tag, bAutoActivate); // Only super useful if you have a pool of specific objects you want to re access, for example you can have a UUserWidget pool and each widget be different and when wanting a specific widget
 *													 // you can query the pool for that tag, returns false if unable to locate within the inactive pool of objects.
 *
 * 
 * MyPool->ReturnToPool(Handle.ToSharedRef()); // Attempts to return the handle to the pool, can fail if the handle is stale but failing is perfectly valid and expected, especially if multiple handle copies exist.
 * Handle->ReturnToPool(); // Returns the object to the pool via the handle, (Typically for fire and forget pooled objects otherwise you would be holding onto the handle yourself).
 *
 * 
 * MyPool->ClearPool(); // Clears the pool of all inactive objects. We do not clear in use ones.
 * MyPool->RemoveFromPool(PoolID, ObjectCheckoutID); // Removes a specific object from the pool ONLY if it is inactive and matches our ID. Will not remove active objects, use `StealObject)` for that.
 * MyPool->RemoveNumFromPool(NumToRemove); // Removes a specific number of inactive objects from the pool, if unable to remove the exact amount the returns false.
 *
 *
 *
 * Usage of the pools handles are as follows:
 * Handles are created and returned as shared pointers, once all pointers to a particular handle are destroyed the object is returned to the pool automatically if it hasn't already,
 * you may also manually call `ReturnToPool` or `Steal` to invalidate existing copied shared pointers to the same handle, be sure to use `Handle->IsHandleValid()` to check.
 *
 * Code:
 * TBFPooledObjectHandlePtr<AMyFoo> Handle = MyPool->GetPooledObject(); // Again.. Should be stored in a larger scope  
 * Handle.IsValid(); // Checks the SharedPtr if the Handle object is valid, not related to the Handle objects validity. Ideally this shouldn't be needed since the nature of a shared pointer kinda keeps it alive for you.
 * Handle->IsHandleValid(); // This checks if the handle is valid, this is what you'll want most likely, you could also technically do 'Handle.Get()->IsHandleValid()' but that's a bit verbose.
 * TBFPooledObjectHandlePtr<AMyFoo> ACopyOfMyHandlePtr = Handle; // Creates a reference counted copy of the handle, if you return the object to the pool via one of the handles, all other handles to the same object will be invalidated.
 * Handle->GetObject(); // This returns the object that the handle is holding, this is what you'll want to use to get the object but do not store it.
 * Handle->ReturnToPool(); // This returns the object to the pool and invalidates the handle, if there are multiple handles to the same object they will all be invalidated.
 * Handle->StealObject(); // This simultaneously removes the pooled object from its owning pool, returns it and invalidates all handles to that object.
 */


// Used to define what type activation and deactivation behaviour the pool has.
UENUM(BlueprintType)
enum class EBFPoolType : uint8
{
	// You must select a valid pool type as this is what drives how the objects are created and they are activated and deactivated.
	Invalid = 0,
	// Actors have their owner set to the pools owner and are the most straight forward to use.
	Actor,			
	// Components have their outer set to the pools owner as well as the components are registered and added to the owners instance components array.
	Component,		
	// (Not for 3D widgets) UserWidgets have their outer set to the pools owner, this means your pool owner must be compatible with UUserWidget (APlayerController is the expected owner).
	UserWidget,		
	// Objects are the simplest, we simply create the owner with the outer as the pools owner and that's it, no activation or deactivation logic can really be applied but the interface logic is still called.
	Object			
};



// Each pool can optionally tick, defines params related to that.
USTRUCT(Blueprintable)
struct FBFObjectPoolInitTickParams
{
	GENERATED_BODY()
public:
	// How often we should tick the pool, this is used to clear out inactive objects.
	UPROPERTY(Transient, BlueprintReadWrite)
	float TickInterval = 1.f;

	/*  If set above 0 then every tick the pool will evaluate its inactive objects and remove any that have been inactive for longer than this.
	 *  Should severely reduce tick rate as this isn't something needs to be done often at all. */
	UPROPERTY(Transient, BlueprintReadWrite)
	float MaxObjectInactiveOccupancySeconds = -1.f;

	// If you aren't wanting periodic clearing of inactive objects, you can just leave it disabled.
	UPROPERTY(Transient, BlueprintReadWrite)
	bool bEnableTicking = false;
};



DECLARE_DYNAMIC_DELEGATE_OneParam(FActivatePooledObjectOverride, UObject*, Obj);
DECLARE_DYNAMIC_DELEGATE_OneParam(FDeactivatePooledObjectOverride, UObject*, Obj);
// Every pool must be initialized with this struct, it contains all the necessary information to create and manage the pool.
USTRUCT(Blueprintable)
struct FBFObjectPoolInitParams
{
	GENERATED_BODY()
public:
	void Reset()
	{
		ActivateObjectOverride.Clear();
		DeactivateObjectOverride.Clear();
		Owner = nullptr;
		PoolClass = nullptr;
		PoolType = EBFPoolType::Invalid;
		PoolLimit = 50;
		InitialCount = 0;
		CooldownTimeSeconds = -1.f;
		PoolTickInfo = FBFObjectPoolInitTickParams();
		bDisableActivationDeactivationLogic = false;
		ObjectFlags = RF_NoFlags;
	}
	
public:
	/* When activating and deactivating the pooled objects, if you dont like the default behaviour or want something more custom not at the Interface level then thats what these exist for,
	 * if bound we simply call that delegate with the object being un-pooled or pooled at the correct time and let you handle the rest. A few use cases could be for static/skeletal meshes,
	 * After spawning those actors and the curfew elapses ready to be returned to the pool, you may choose to leave the actor not hidden so they can remain there and next time its needed we then
	 * go un-pool and handle that logic, a pooled object is just an object in a certain state (In my cases just see default behaviour in the pool), there is nothing magical about it and can easily be extended
	 * ONLY BIND IF YOU WANT THIS BEHAVIOUR. */
	UPROPERTY(BlueprintReadWrite)
	FActivatePooledObjectOverride ActivateObjectOverride;

	UPROPERTY(BlueprintReadWrite)
	FDeactivatePooledObjectOverride DeactivateObjectOverride;
	
	
	/* The owner of the pool, this is the actor that will own the pooled objects as well. Depending on the pool type this is super important,
	 * For example a widget cannot be owned by an AActor but can be by a APlayerController.
	 * The owner should ALWAYS outlive the pools lifetime.*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TObjectPtr<AActor> Owner = nullptr;

	
	/* When spawning the pooled objects this is used as the class to spawn, C++ side can leave unpopulated if you just want the templated class, BP side MUST set this. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	UClass* PoolClass = nullptr;

	/* Pool type defines Activation/Deactivation logic as well as how new objects are created. This is important to not mix up, for example if you used a PoolableSoundActor it would be easy
	 * to accidentally set it to a component because its main use is the component, same with a poolable Niagara actor but that would result in the wrong behaviour.
	 * This should be the type of the Object being held within the pool, that's it.
	 * for example:
	 * - Actors: have their owner set to the owner of the pool
	 * - Components: have their outer set to the owner of the pool and are registered with the owners component array.
	 * - UserWidgets: have their owner set to the owner of the pool (Note this MUST be compatible with UUserWidget owner types such as APlayerController, UUserWidget, UWorld etc)
	 * - UObjects: just have their outer set to the owner of the pool
	 * stock uobjects dont come with much logic needed for Activating/deactivating see IBFPoooledObjectInterface if you need custom logic */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	EBFPoolType PoolType = EBFPoolType::Invalid;
	
	// Max num of objects this pull is allowed to create, can be increased/decreased via the pool at runtime.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	int32 PoolLimit = 50;
	
	// How many pooled object to create upfront, otherwise we lazy create and pool objects when needed.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	int32 InitialCount = 0;

	/* If greater than 0, when trying to take an object from the pool it must have been in the pool for at least as long as this (NOT APPLIED FIRST USAGE FROM POOL).
	 * (I've noticed ribbon VFX if being pooled and reused too quickly can cause the ribbon trail to span from its last position, it's just a nice option to have.)*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float CooldownTimeSeconds = -1.f;
	
	/* Defines the tick behaviour of the pool, ticking can ignored and it will be disabled by default.
	 * If disabled you lose the ability to clear out inactive objects that exceed the MaxObjectInactiveOccupancySeconds,
	 * tbh this is the desired behaviour most of the time anyway since the whole point of a pool is to pre-spawn and keep but who knows. */ 
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FBFObjectPoolInitTickParams PoolTickInfo;

	/** If true when taking objects from the pool we will NEVER attempt to manually activate them or when pooling never attempt to deactivate them (even if the function param bAutoActivate is set,
	 * its ignored. This is important if you want to un-pool objects and *completely* handle activating and deactivating them yourself.) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	uint8 bDisableActivationDeactivationLogic : 1 = false;

	/* Flags for each new spawned object in the pool, If left to default then we will apply the flag RF_Transient and remove the default flag of RF_Transactional
	 * (which makes spawning a lot cheaper and pooled objects should be transient anyway)
	 * otherwise just add your own flags and no default flags will be applied.  */
	EObjectFlags ObjectFlags = RF_NoFlags;
};


template<typename T, ESPMode Mode = ESPMode::NotThreadSafe> requires BF::OP::CIs_UObject<T>
struct TBFObjectPool : public TSharedFromThis<TBFObjectPool<T, Mode>, Mode>
{
	using FOnObjectPooled = TMulticastDelegate<void(bool bEnteredPool, int64 ID, int32 CheckoutID)>;
	using FOnObjectAddedToPool = TMulticastDelegate<void(int64 ID, int32 CheckoutID)>;
	using FOnObjectRemovedFromPool = TMulticastDelegate<void(int64 ID, int32 CheckoutID)>;
	
protected:
	template <typename ObjectType, ESPMode PtrMode>
	friend class SharedPointerInternals::TIntrusiveReferenceController;
	friend struct TBFPooledObjectHandle<T, Mode>;
	TBFObjectPool() = default; // Force factory function for creation of pools.
	
	// Shared ptr finally destroys handle and we try manually return then we have an issue bc no other shared ptrs exist
	// so we have an internal function for handles to use to return the object to the pool.
	virtual bool ReturnToPool_Internal(int64 PoolID, int32 ObjectCheckoutID);
public:
	// No copy ctors, pools should always be unique and created via the factory method but we do allow moving if needed,
	// just be careful because the owner will still be the Initial owner, I might add rename functionality later for changing outers.
	TBFObjectPool(const TBFObjectPool& Rhs) = delete;
	TBFObjectPool& operator=(const TBFObjectPool& Rhs) = delete;

	TBFObjectPool(TBFObjectPool&& Rhs) noexcept;
	TBFObjectPool& operator=(TBFObjectPool&& Rhs) noexcept;
	virtual ~TBFObjectPool() = default;

	// Owner should ALWAYS outlive the pool.
	template<typename Ty>
	Ty* GetOwner() const { return CastChecked<Ty>(PoolInitInfo.Owner.Get()); }
	AActor* GetOwner() const { return PoolInitInfo.Owner.Get(); }
	
	// The pool MUST be instantiated via this Create method only.
	static TBFObjectPoolPtr<T,  Mode> CreatePool() { return MakeShared<TBFObjectPool, Mode>(); }

	// Crucial function that every pool needs called in order to function properly.
	virtual void InitPool(const FBFObjectPoolInitParams& Info);

	/* Returns a valid pooled object unless at capacity. Should always Check return for IsValid/Pointer validity in case the pool was all used and at capacity to make more,
	 * IsHandleValid will always be true at this stage if the returned ptr was valid so need to check for that.
	 * If bAutoActivate is false, the user is responsible for activating the object.
	 * Check the ActivateObject() for a general idea. */
	virtual TBFPooledObjectHandlePtr<T, Mode> UnpoolObject(bool bAutoActivate);

	/* Iterates the inactive object pool searching all objects calling the interface function GetObjectGameplayTag(), this requires you to have implemented
	 * the function on the object otherwise you won't get anything back. Useful if you pool is of specific objects and not generic reusable ones. */
	virtual TBFPooledObjectHandlePtr<T, Mode> UnpoolObjectByTag(FGameplayTag Tag, bool bAutoActivate);

	/* When a pooled object is being used, you have the ability to keep it and steal it from the pool,
	 * this will invalidate any handles and return the object ready to be managed by you. */
	T* StealObject(int64 PoolID, int32 ObjectCheckoutID);

	// Returns the object to the pool for later use. MyPool->ReturnToPool(Handle); alternatively Handle->ReturnToPool(); if the handle is still valid.
	virtual bool ReturnToPool(TBFPooledObjectHandlePtr<T, Mode>& Handle);

	// Removes a specific object from the pool only if it is inactive, if you want to remove active objects then you might be after StealObject().
	virtual bool RemoveInactiveObjectFromPool(int64 PoolID, int32 ObjectCheckoutID);

	// Attempts to remove the specified amount of inactive objects from the pool, will not remove any if the pool is already empty or if we can't remove the specified amount.
	virtual bool RemoveInactiveNumFromPool(int64 NumToRemove);

	// Clears all inactive objects from the pool if there are any.
	virtual bool ClearInactiveObjectsPool();
	
	// Checks not only if the pools contains an Object with the given ID, but also if our specific handles checkout ID is the same as the pooled objects current checkout ID.
	virtual bool IsObjectIDValid(int64 PoolID, int32 ObjectCheckoutID) const;
	virtual bool IsObjectInactive(int64 PoolID, int32 ObjectCheckoutID) const;

	int32 GetPoolSize() const { return PoolContainer->ObjectPool.Num(); }
	int32 GetActivePoolSize() const { return GetPoolSize() - GetInactivePoolSize(); }
	int32 GetInactivePoolSize() const { return PoolContainer->InactiveObjectIDPool.Num(); }
	int32 GetPoolLimit() const { return PoolInitInfo.PoolLimit; }
	bool IsFull() const { return GetPoolSize() >= PoolInitInfo.PoolLimit; }
	EBFPoolType GetPoolType() const { return PoolInitInfo.PoolType; }
	UWorld* GetWorld() const { return PoolInitInfo.Owner->GetWorld(); }
	float GetMaxObjectInactiveOccupancySeconds() const { return PoolInitInfo.PoolTickInfo.MaxObjectInactiveOccupancySeconds; }

	// Called after creating a new object and adding it to the pool.
	FOnObjectAddedToPool& GetOnObjectAddedToPool() { return OnObjectAddedToPool; }
	// Called after removing an object from the pool.
	FOnObjectRemovedFromPool& GetOnObjectRemovedFromPool() { return OnObjectRemovedFromPool; }
	// Called when an object is being used or returned from the pool.
	FOnObjectPooled& GetOnObjectPooled() { return OnObjectPooled; }
	
	
	// Can only decrease pool limit if we are able to remove enough inactive objects, we do not remove active one.
	virtual bool SetPoolLimit(int32 InPoolLimit);
	// How long object may remain inactive before removed(Requires Tick enabled). Less than 0 for indefinite occupancy.
	virtual void SetMaxObjectInactiveOccupancySeconds(float MaxObjectInactiveOccupancySeconds);

	// Can be manually called to evaluate the pool occupancy and remove inactive objects if they exceed the MaxObjectInactiveOccupancySeconds.
	virtual bool EvaluatePoolOccupancy();
	
	/* When tick is disabled, there is no clearing out of inactive objects. If you want tick you also can
	* optionally set its Interval for better performance since clearing inactive pool objects isn't really that important.*/
	virtual void SetTickEnabled(bool bEnable);
	virtual void SetTickGroup(ETickingGroup InTickGroup) { PoolContainer->SetTickGroup(InTickGroup); }
	virtual void SetTickInterval(float InTickInterval);
	float GetTickInterval() const {return PoolInitInfo.PoolTickInfo.TickInterval;}
	bool GetTickEnabled()const {return PoolContainer->GetTickEnabled();}
	const FBFObjectPoolInitParams& GetPoolInitInfo() const {return PoolInitInfo;}
	
protected:
	virtual FBFPooledObjectInfo* CreateNewPoolEntry();
	virtual void ActivateObject(T* Obj, bool bAutoActivate);
	virtual void DeactivateObject(T* Obj);
	virtual void Reset();
	virtual void Tick(UWorld* World, float Dt);
	
protected:
	// Called upon a new object being created or deleted from the pool.
	FOnObjectAddedToPool OnObjectAddedToPool;
	FOnObjectRemovedFromPool OnObjectRemovedFromPool;
	
	// Called when an object is being used or returned from the pool
	FOnObjectPooled OnObjectPooled;

	TStrongObjectPtr<UBFPoolContainer> PoolContainer = nullptr;
	FBFObjectPoolInitParams PoolInitInfo;

	// Ever incrementing ID that is statically assigned to each new object.
	int64 CurrentPoolIDIndex = -1;
	
	// Cheaper than IsBound() checks every pooling/un-pooling.
	uint8 bIsActivateObjectOverridden : 1 = false;
	uint8 bIsDeactivateObjectOverridden : 1 = false;
};



template <typename T, ESPMode Mode> 
requires BF::OP::CIs_UObject<T>
TBFObjectPool<T, Mode>::TBFObjectPool(TBFObjectPool&& Rhs) noexcept
{
	OnObjectAddedToPool = std::move(Rhs.OnObjectAddedToPool);
	OnObjectRemovedFromPool = std::move(Rhs.OnObjectRemovedFromPool);
	
	PoolContainer = std::move(Rhs.PoolContainer);
	PoolInitInfo = std::move(Rhs.PoolInitInfo);
	CurrentPoolIDIndex = Rhs.CurrentPoolIDIndex;
	bIsActivateObjectOverridden = Rhs.bIsActivateObjectOverridden;
	bIsDeactivateObjectOverridden = Rhs.bIsDeactivateObjectOverridden;
	Rhs.Reset();
}


template <typename T, ESPMode Mode>
requires BF::OP::CIs_UObject<T>
TBFObjectPool<T, Mode>& TBFObjectPool<T, Mode>::operator=(TBFObjectPool&& Rhs) noexcept
{
	if(this == &Rhs)
		return *this;
	
	OnObjectAddedToPool = std::move(Rhs.OnObjectAddedToPool);
	OnObjectRemovedFromPool = std::move(Rhs.OnObjectRemovedFromPool);
	
	PoolContainer = std::move(Rhs.PoolContainer);
	PoolInitInfo = std::move(Rhs.PoolInitInfo);
	CurrentPoolIDIndex = Rhs.CurrentPoolIDIndex;
	bIsActivateObjectOverridden = Rhs.bIsActivateObjectOverridden;
	bIsDeactivateObjectOverridden = Rhs.bIsDeactivateObjectOverridden;
	Rhs.Reset();
	
	return *this;
}

template<typename T, ESPMode Mode>
requires BF::OP::CIs_UObject<T>
void TBFObjectPool<T, Mode>::InitPool(const FBFObjectPoolInitParams& Info)
{
	bfEnsure(Info.InitialCount >= 0 && Info.PoolLimit >= 0); // Please use positive values, unreal forces int over uint.
	bfEnsure(Info.PoolType != EBFPoolType::Invalid); // Must set the pool type. 
	bfEnsure(Info.InitialCount <= Info.PoolLimit); // Self explanatory.
	bfValid(Info.Owner);
	bfValid(Info.Owner->GetWorld());
	bfEnsure(!PoolContainer.IsValid() || PoolContainer->ObjectPool.Num() == 0); // You can't re-init a pool, you must clear it first or just make a new pool.
	if(!Info.Owner || Info.PoolType == EBFPoolType::Invalid ||
		(PoolContainer.IsValid() && PoolContainer->ObjectPool.Num() > 0))
		return;

	
	PoolInitInfo = Info;
	bIsActivateObjectOverridden = PoolInitInfo.ActivateObjectOverride.IsBound();
	bIsDeactivateObjectOverridden = PoolInitInfo.DeactivateObjectOverride.IsBound();

	if(!PoolContainer.IsValid()) // Reuse if we are re-initializing the pool.
		PoolContainer = TStrongObjectPtr(NewObject<UBFPoolContainer>(Info.Owner.Get()));

	PoolContainer->Init([WeakThis = this->AsWeak()](UWorld* World, float Dt)
	{
		if(WeakThis.IsValid())
			WeakThis.Pin()->Tick(World, Dt);
	},  GetWorld(), PoolInitInfo.PoolTickInfo.TickInterval);

	PoolContainer->SetTickEnabled(PoolInitInfo.PoolTickInfo.bEnableTicking);

	if(!PoolInitInfo.PoolClass) // Only applies to c++ land, in BP we ensure before this is even called if the class is not set since its templated on UObject.
		PoolInitInfo.PoolClass = T::StaticClass();

	int32 Count = Info.InitialCount;
	while(Count--)
	{
		CreateNewPoolEntry();
	}
}



template <typename T, ESPMode Mode> 
requires BF::OP::CIs_UObject<T>
void TBFObjectPool<T,  Mode>::Tick(UWorld* World, float Dt)
{
	
#if !UE_BUILD_SHIPPING
	if(BF::OP::CVarObjectPoolPrintPoolOccupancy.GetValueOnGameThread())
	{
		bfEnsure(PoolContainer.IsValid());
		FString FormatString = FString::Format(TEXT("Object Pool {0} {1} \n- Total Size:{2}/{3}\n- Active Objects: {4}\n- Inactive Objects: {5}\n- Max Inactive Occupancy: {6}\n- Cooldown Time: {7}"),
			{ PoolInitInfo.PoolClass->GetName(), GetNameSafe(PoolInitInfo.Owner), PoolContainer->ObjectPool.Num(), PoolInitInfo.PoolLimit ,PoolContainer->ObjectPool.Num() - PoolContainer->InactiveObjectIDPool.Num(),
				PoolContainer->InactiveObjectIDPool.Num(), GetMaxObjectInactiveOccupancySeconds(), PoolInitInfo.CooldownTimeSeconds });

		if(World->GetNetMode() == NM_DedicatedServer || !GEngine)
			UE_LOGFMT(LogTemp, Warning, "{0}", FormatString);
		else
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FormatString);
	}
#endif
	
	EvaluatePoolOccupancy();
}


template <typename T, ESPMode Mode> requires BF::OP::CIs_UObject<T>
bool TBFObjectPool<T, Mode>::EvaluatePoolOccupancy()
{
	if(GetMaxObjectInactiveOccupancySeconds() < 0.f)
		return false;
	
	float SecondsNow = GetWorld()->GetTimeSeconds();
	
	int NumRemoved = 0;
	for(int32 j = PoolContainer->InactiveObjectIDPool.Num() - 1; j >= 0; --j)
	{
		FBFPooledObjectInfo& Info = PoolContainer->ObjectPool.FindChecked(PoolContainer->InactiveObjectIDPool[j]);
		float Delta = SecondsNow - Info.LastTimeActive;
		if(Delta >= GetMaxObjectInactiveOccupancySeconds())
		{
			RemoveInactiveObjectFromPool(Info.ObjectPoolID, Info.ObjectCheckoutID);
			++NumRemoved;
		}
	}

#if !UE_BUILD_SHIPPING
	if(BF::OP::CVarObjectPoolEnableLogging.GetValueOnAnyThread())
		UE_LOGFMT(LogTemp, Warning, "Removed {0} objects from the pool due to exceeding the MaxObjectInactiveOccupancySeconds", NumRemoved);
#endif

	return NumRemoved > 0;
}


template <typename T, ESPMode Mode> 
requires BF::OP::CIs_UObject<T>
void TBFObjectPool<T, Mode>::Reset()
{
	OnObjectAddedToPool.Clear();
	OnObjectRemovedFromPool.Clear();
	PoolContainer = nullptr;
	PoolInitInfo.Reset();
	CurrentPoolIDIndex = -1;
	bIsActivateObjectOverridden = false;
	bIsDeactivateObjectOverridden = false;
}


template <typename T, ESPMode Mode>
requires BF::OP::CIs_UObject<T>
bool TBFObjectPool<T, Mode>::SetPoolLimit(int32 PoolLimit)
{
	if(PoolLimit == PoolInitInfo.PoolLimit)
		return true;

	if(PoolLimit > PoolInitInfo.PoolLimit)
	{
		PoolInitInfo.PoolLimit = PoolLimit;
		return true;
	}

	int32 CurrentPoolSize = GetPoolSize();
	int32 InactivePoolSize = GetInactivePoolSize();

	// If we don't have enough inactive objects to reach the new limit then we can't do anything
	if(CurrentPoolSize - InactivePoolSize <= PoolLimit)
	{
		// Now check to see if we can fit without trimming some objects.
		if(CurrentPoolSize <= PoolLimit)
		{
			PoolInitInfo.PoolLimit = PoolLimit;
			return true;
		}

		RemoveInactiveNumFromPool(CurrentPoolSize - PoolLimit);
		PoolInitInfo.PoolLimit = PoolLimit;
		return true;
	}

	return false;
}


template <typename T, ESPMode Mode> requires BF::OP::CIs_UObject<T>
void TBFObjectPool<T, Mode>::SetMaxObjectInactiveOccupancySeconds(float MaxObjectInactiveOccupancySeconds)
{
	// You must call Init on your pool before trying to use anything on it.
	bfEnsure(PoolContainer.IsValid());
	
	// Ensure that we are ticking if we have a value that's useful.
	if(MaxObjectInactiveOccupancySeconds > 0.f)
		PoolContainer->SetTickEnabled(true);
	else
	{
		PoolContainer->SetTickEnabled(false);
		MaxObjectInactiveOccupancySeconds = -1.f;
	}

	PoolInitInfo.PoolTickInfo.MaxObjectInactiveOccupancySeconds = MaxObjectInactiveOccupancySeconds;
}


template <typename T, ESPMode Mode> 
requires BF::OP::CIs_UObject<T>
void TBFObjectPool<T, Mode>::SetTickInterval(float InTickInterval)
{
	// You must call Init on your pool before trying to use anything on it.
	bfEnsure(PoolContainer.IsValid());
	PoolContainer->SetTickInterval(InTickInterval);
	PoolInitInfo.PoolTickInfo.TickInterval = InTickInterval;
}


template <typename T, ESPMode Mode> 
requires BF::OP::CIs_UObject<T>
void TBFObjectPool<T, Mode>::SetTickEnabled(bool bEnable)
{
	// You must call Init on your pool before trying to use anything on it.
	bfEnsure(PoolContainer.IsValid());
	PoolContainer->SetTickEnabled(bEnable);
}


template<typename T, ESPMode Mode>
requires BF::OP::CIs_UObject<T>
FBFPooledObjectInfo* TBFObjectPool<T, Mode>::CreateNewPoolEntry()
{
	SCOPED_NAMED_EVENT(TBFObjectPool_CreateNewPoolEntry, FColor::Green);
	// You must call Init on your pool before trying to use anything on it.
	bfEnsure(PoolContainer.IsValid());
	
	if(GetPoolSize() >= PoolInitInfo.PoolLimit)
		return nullptr;

	EObjectFlags Flags = PoolInitInfo.ObjectFlags == RF_NoFlags ? RF_Transient : PoolInitInfo.ObjectFlags;
	

	UObject* Object = nullptr;
	switch (GetPoolType())
	{
		case EBFPoolType::Actor:
		{
			AActor* NewPoolObject = nullptr;
			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = PoolInitInfo.Owner.Get();
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			SpawnParams.ObjectFlags = Flags;
			NewPoolObject = PoolInitInfo.Owner->GetWorld()->SpawnActor<AActor>(PoolInitInfo.PoolClass, SpawnParams);
			NewPoolObject->SetActorTickEnabled(false);
			NewPoolObject->SetActorHiddenInGame(true);
			NewPoolObject->SetActorEnableCollision(false);
				
			Object = NewPoolObject;
			break;
		}
		case EBFPoolType::Component:
		{
			UActorComponent* NewPoolObject = nullptr;
			NewPoolObject = NewObject<UActorComponent>(PoolInitInfo.Owner.Get(), PoolInitInfo.PoolClass, NAME_None, Flags);
			NewPoolObject->bAutoActivate = false; 
			NewPoolObject->RegisterComponent();
			PoolInitInfo.Owner->AddInstanceComponent(NewPoolObject);
				
			Object = NewPoolObject;
			break;
		}
		case EBFPoolType::UserWidget:
		{
			UUserWidget* NewPoolObject = nullptr;
			NewPoolObject = CreateWidget<UUserWidget>(CastChecked<APlayerController>(PoolInitInfo.Owner.Get()), PoolInitInfo.PoolClass);
			NewPoolObject->AddToViewport();
			NewPoolObject->SetVisibility(ESlateVisibility::Hidden); // Hide it by default until we un-pool the widget.
				
			Object = NewPoolObject;
			break;
		}
		case EBFPoolType::Object: Object = NewObject<UObject>(PoolInitInfo.Owner.Get(), PoolInitInfo.PoolClass, NAME_None, Flags); break;
		case EBFPoolType::Invalid: bfEnsure(false); return nullptr;
	}
	
	bfValid(Object); // Should be valid always, if not then something went wrong.
	
	// Add a small initial offset to the LastActiveTime in the case when using a cooldown time, this ensures the first pooling doesn't block instant access.
	float CooldownOffset = PoolInitInfo.CooldownTimeSeconds > 0 ? PoolInitInfo.CooldownTimeSeconds + KINDA_SMALL_NUMBER : 0.f;

	FBFPooledObjectInfo Info;
	Info.ObjectPoolID = ++CurrentPoolIDIndex; // -1 Is considered an invalid Index
	Info.CreationTime = GetWorld()->GetTimeSeconds();
	Info.LastTimeActive = GetWorld()->GetTimeSeconds() - CooldownOffset;
	Info.PooledObject = Object;
	++Info.ObjectCheckoutID; // Don't need to but just feel better starting above -1

	FBFPooledObjectInfo* NewInfo = &PoolContainer->ObjectPool.Add(Info.ObjectPoolID, Info);
	PoolContainer->InactiveObjectIDPool.Add(Info.ObjectPoolID);

	if(Object->template Implements< UBFPooledObjectInterface>())
		IBFPooledObjectInterface::Execute_OnObjectPooled(Object);

	
	OnObjectAddedToPool.Broadcast(Info.ObjectPoolID, Info.ObjectCheckoutID);
	return NewInfo;
}


template<typename T, ESPMode Mode>
requires BF::OP::CIs_UObject<T>
TBFPooledObjectHandlePtr<T, Mode> TBFObjectPool<T, Mode>::UnpoolObject(bool bAutoActivate)
{
	bfEnsure(PoolContainer.IsValid() && IsValid(PoolInitInfo.Owner)); // have you initialized the pool?
	// Might not have any free but we have space to make more.
	if(PoolContainer->InactiveObjectIDPool.Num() == 0)
	{
		if(GetPoolSize() < PoolInitInfo.PoolLimit)
		{
			CreateNewPoolEntry();
		}
		else
		{
#if !UE_BUILD_SHIPPING
			if(BF::OP::CVarObjectPoolEnableLogging.GetValueOnAnyThread())
				UE_LOGFMT(LogTemp, Warning, "[BFObjectPool] Trying to get a pooled object for {0} but all current objects are active and pool {1} is at capacity.", GetOwner()->GetName(), PoolInitInfo.PoolClass->GetName());
#endif
			return nullptr;
		}
	}

	// If we aren't using a cooldowns then we can speed up this by just popping from the inactive pool.
	float Cooldown = PoolInitInfo.CooldownTimeSeconds;
	if(Cooldown < KINDA_SMALL_NUMBER)
	{ 
		FBFPooledObjectInfo& Info = PoolContainer->ObjectPool[PoolContainer->InactiveObjectIDPool.Pop()];
		++Info.ObjectCheckoutID;
		ActivateObject(CastChecked<T>(Info.PooledObject), bAutoActivate);
		Info.bActive = true;
		Info.LastTimeActive = GetWorld()->GetTimeSeconds();
		OnObjectPooled.Broadcast(false, Info.ObjectPoolID, Info.ObjectCheckoutID);

		return MakeShared<TBFPooledObjectHandle<T, Mode>, Mode>(&Info, TWeakPtr<TBFObjectPool, Mode>(this->AsWeak()));
	}

	// Iterate oldest to newest and find the first object that has been inactive for longer than the cooldown time.
	float SecondsNow = GetWorld()->GetTimeSeconds();
	for(auto ID : PoolContainer->InactiveObjectIDPool)
	{
		auto& Info = PoolContainer->ObjectPool.FindChecked(ID);
		float Delta = SecondsNow - Info.LastTimeActive;
		if(Delta < Cooldown)
			continue;

		++Info.ObjectCheckoutID;
		ActivateObject(CastChecked<T>(Info.PooledObject), bAutoActivate);
		Info.bActive = true;
		Info.LastTimeActive = GetWorld()->GetTimeSeconds();
		PoolContainer->InactiveObjectIDPool.Remove(ID);
		OnObjectPooled.Broadcast(false, Info.ObjectPoolID, Info.ObjectCheckoutID);
		return MakeShared<TBFPooledObjectHandle<T, Mode>, Mode>(&Info, TWeakPtr<TBFObjectPool, Mode>(this->AsWeak()));
	}

	// This means nothing in the inactive pool met our threshold, check one last time if we can make one.
	if(GetPoolSize() < PoolInitInfo.PoolLimit)
	{
		FBFPooledObjectInfo& Entry = *CreateNewPoolEntry();

		++Entry.ObjectCheckoutID;
		ActivateObject(CastChecked<T>(Entry.PooledObject), bAutoActivate);
		Entry.bActive = true;
		Entry.LastTimeActive = GetWorld()->GetTimeSeconds();
		PoolContainer->InactiveObjectIDPool.Remove(Entry.ObjectPoolID);
		OnObjectPooled.Broadcast(false, Entry.ObjectPoolID, Entry.ObjectCheckoutID);
		return MakeShared<TBFPooledObjectHandle<T, Mode>, Mode>(&Entry, TWeakPtr<TBFObjectPool, Mode>(this->AsWeak()));
	}
	
	return nullptr;
}


template <typename T, ESPMode Mode> requires BF::OP::CIs_UObject<T>
TBFPooledObjectHandlePtr<T, Mode> TBFObjectPool<T, Mode>::UnpoolObjectByTag(FGameplayTag Tag, bool bAutoActivate)
{
	if(PoolContainer->InactiveObjectIDPool.Num() == 0 || !Tag.IsValid())
		return nullptr;

	for(auto ID : PoolContainer->InactiveObjectIDPool)
	{
		auto& Info = PoolContainer->ObjectPool.FindChecked(ID);
		if(!Info.PooledObject->template Implements< UBFPooledObjectInterface>())
			continue;
		
		if(IBFPooledObjectInterface::Execute_GetObjectGameplayTag(Info.PooledObject) == Tag)
		{
			ActivateObject(CastChecked<T>(Info.PooledObject), bAutoActivate);
			Info.bActive = true;
			++Info.ObjectCheckoutID;
			Info.LastTimeActive = GetWorld()->GetTimeSeconds();
			PoolContainer->InactiveObjectIDPool.Remove(ID);
			OnObjectPooled.Broadcast(false, Info.ObjectPoolID, Info.ObjectCheckoutID);
			return MakeShared<TBFPooledObjectHandle<T, Mode>, Mode>(&Info, TSharedPtr<TBFObjectPool, Mode>(this->AsShared()));
		}
	}
	return nullptr;
}


template<typename T, ESPMode Mode>
requires BF::OP::CIs_UObject<T>
bool TBFObjectPool<T, Mode>::ReturnToPool(TBFPooledObjectHandlePtr<T, Mode>& Handle)
{
	if(!Handle.IsValid() && !Handle->IsHandleValid())
	{
		Handle = nullptr;
		return false;
	}

	auto& ObjectInfo = PoolContainer->ObjectPool.FindChecked(Handle->GetPoolID());
	++ObjectInfo.ObjectCheckoutID; 
	DeactivateObject(Handle->GetObject());
	PoolContainer->InactiveObjectIDPool.Add(ObjectInfo.ObjectPoolID);
	ObjectInfo.bActive = false;
	
	OnObjectPooled.Broadcast(true, ObjectInfo.ObjectPoolID, ObjectInfo.ObjectCheckoutID);

	Handle = nullptr;
	return true;
}


template <typename T, ESPMode Mode> requires BF::OP::CIs_UObject<T>
bool TBFObjectPool<T, Mode>::ReturnToPool_Internal(int64 PoolID, int32 ObjectCheckoutID)
{
	if(FBFPooledObjectInfo* PooledObj = PoolContainer->ObjectPool.Find(PoolID))
	{
		if(PooledObj->ObjectCheckoutID == ObjectCheckoutID)
		{
			++PooledObj->ObjectCheckoutID; // Ensure the ID differs in case the object IF does any checks/returns upon Deactivation.
			DeactivateObject(CastChecked<T>(PooledObj->PooledObject.Get()));
			PoolContainer->InactiveObjectIDPool.Add(PoolID);
			PooledObj->bActive = false;
			
			OnObjectPooled.Broadcast(true, PoolID, ObjectCheckoutID);
			return true;
		}
	}
	return false;
}

template <typename T, ESPMode Mode>
requires BF::OP::CIs_UObject<T>
bool TBFObjectPool<T,  Mode>::ClearInactiveObjectsPool()
{
	if(PoolContainer->InactiveObjectIDPool.Num() == 0)
		return false;

	auto& Pool = PoolContainer->InactiveObjectIDPool;
	for(int j = Pool.Num() -1; j >= 0; --j)
	{
		auto& Info = PoolContainer->ObjectPool.FindChecked(Pool[j]);
		RemoveInactiveObjectFromPool(Info.ObjectPoolID, Info.ObjectCheckoutID);
	}
	
	return true;
}


template <typename T, ESPMode Mode>
requires BF::OP::CIs_UObject<T>
bool TBFObjectPool<T, Mode>::RemoveInactiveObjectFromPool(int64 PoolID, int32 ObjectCheckoutID)
{
	int32 Index = PoolContainer->InactiveObjectIDPool.Find(PoolID);
	if(Index != -1)
	{
		auto& Info = PoolContainer->ObjectPool.FindChecked(PoolID);
		if(Info.ObjectCheckoutID != ObjectCheckoutID)
		{
#if !UE_BUILD_SHIPPING
			if(BF::OP::CVarObjectPoolEnableLogging.GetValueOnAnyThread())
				UE_LOGFMT(LogTemp, Warning, "[BFObjectPool] Trying to remove an object with an invalid checkout ID, this is likely due to a stale handle.");
#endif
			return false;
		}

		if(Info.PooledObject->template Implements< UBFPooledObjectInterface>())
			IBFPooledObjectInterface::Execute_OnObjectDestroyed(Info.PooledObject);

		
		switch (GetPoolType())
		{
			case EBFPoolType::Actor: CastChecked<AActor>(Info.PooledObject)->Destroy(); break;
			case EBFPoolType::Component: CastChecked<UActorComponent>(Info.PooledObject)->DestroyComponent(); break;
			case EBFPoolType::UserWidget: CastChecked<UUserWidget>(Info.PooledObject)->RemoveFromParent(); CastChecked<UUserWidget>(Info.PooledObject)->MarkAsGarbage(); break;
			case EBFPoolType::Object: Info.PooledObject->MarkAsGarbage(); break;
			case EBFPoolType::Invalid: bfEnsure(false); return false;
		}
	
		int64 ID = Info.ObjectPoolID;
		PoolContainer->InactiveObjectIDPool.RemoveAtSwap(Index);
		PoolContainer->ObjectPool.Remove(ID);
		OnObjectRemovedFromPool.Broadcast(ID, Info.ObjectCheckoutID);
		return true;
	}
	return false;
}



template <typename T, ESPMode Mode>
requires BF::OP::CIs_UObject<T>
bool TBFObjectPool<T, Mode>::RemoveInactiveNumFromPool(int64 NumToRemove)
{
	if(NumToRemove > GetInactivePoolSize())	
		return false;

	for(int i = 0; i < NumToRemove; ++i)
	{
		// Popping from inactive so no need to remove below
		auto& Info = PoolContainer->ObjectPool.FindChecked(PoolContainer->InactiveObjectIDPool.Pop());
		
		if(Info.PooledObject->template Implements< UBFPooledObjectInterface>())
			IBFPooledObjectInterface::Execute_OnObjectDestroyed(Info.PooledObject);

		
		switch(GetPoolType())
		{
			case EBFPoolType::Actor: CastChecked<AActor>(Info.PooledObject)->Destroy(); break;
			case EBFPoolType::Component: CastChecked<UActorComponent>(Info.PooledObject)->DestroyComponent(); break;
			case EBFPoolType::UserWidget: CastChecked<UUserWidget>(Info.PooledObject)->RemoveFromParent(); CastChecked<UUserWidget>(Info.PooledObject)->MarkAsGarbage(); break;
			case EBFPoolType::Object: Info.PooledObject->MarkAsGarbage(); break;
			case EBFPoolType::Invalid: bfEnsure(false); return false;
		}

		int64 ID = Info.ObjectPoolID;
		PoolContainer->ObjectPool.Remove(ID);
		OnObjectRemovedFromPool.Broadcast(ID, Info.ObjectCheckoutID);
	}
	return true;
}


template <typename T, ESPMode Mode> requires BF::OP::CIs_UObject<T>
bool TBFObjectPool<T, Mode>::IsObjectIDValid(int64 PoolID, int32 ObjectCheckoutID) const
{
	if(auto* Info = PoolContainer->ObjectPool.Find(PoolID))
		return Info->ObjectCheckoutID == ObjectCheckoutID;
	return false;
}


template <typename T, ESPMode Mode> requires BF::OP::CIs_UObject<T>
bool TBFObjectPool<T, Mode>::IsObjectInactive(int64 PoolID, int32 ObjectCheckoutID) const
{
	if(auto* Info = PoolContainer->ObjectPool.Find(PoolID))
		return Info->ObjectCheckoutID == ObjectCheckoutID && !Info->bActive;
	return false;
}


template <typename T, ESPMode Mode> 
requires BF::OP::CIs_UObject<T>
T* TBFObjectPool<T, Mode>::StealObject(int64 PoolID, int32 ObjectCheckoutID)
{
	if(FBFPooledObjectInfo* Info = PoolContainer->ObjectPool.Find(PoolID))
	{
		if(Info->ObjectCheckoutID != ObjectCheckoutID)
		{
#if !UE_BUILD_SHIPPING
			if(BF::OP::CVarObjectPoolEnableLogging.GetValueOnAnyThread())
				UE_LOGFMT(LogTemp, Warning, "[BFObjectPool] Trying to steal an object with an invalid checkout ID, this is likely due to a stale handle.");
#endif
			return nullptr;
		}

		if(!Info->bActive)
			PoolContainer->InactiveObjectIDPool.Remove(PoolID);

		PoolContainer->ObjectPool.Remove(PoolID);
		OnObjectRemovedFromPool.Broadcast(PoolID, Info->ObjectCheckoutID);
		
		return CastChecked<T>(Info->PooledObject);
	}
	return nullptr;
}



template<typename T, ESPMode Mode>
requires BF::OP::CIs_UObject<T>
void TBFObjectPool<T,  Mode>::ActivateObject(T* Obj, bool bAutoActivate)
{
	bfEnsure(IsValid(Obj));
#if !UE_BUILD_SHIPPING
	if(BF::OP::CVarObjectPoolEnableLogging.GetValueOnAnyThread() && bAutoActivate && PoolInitInfo.bDisableActivationDeactivationLogic)
		UE_LOGFMT(LogTemp, Warning, "[BFObjectPool] Trying to get a pooled object with bAutoActivate set to true but the pools init info has activation/deactivation logic disabled, is this intentional?");
#endif	

	if(bIsActivateObjectOverridden)
	{
		PoolInitInfo.ActivateObjectOverride.Execute(Obj);
	}
	else if(bAutoActivate && !PoolInitInfo.bDisableActivationDeactivationLogic)
	{
		switch (GetPoolType())
		{
			case EBFPoolType::Actor:
			{
				AActor* Actor = (AActor*)Obj;
				Actor->SetActorTickEnabled(true);
				Actor->SetActorHiddenInGame(false);
				Actor->SetActorEnableCollision(true);
				break;
			}
			case EBFPoolType::Component:
			{
				UActorComponent* Component = (UActorComponent*)Obj;
				Component->Activate(true);
				break;
			}
			case EBFPoolType::UserWidget:
			{
				UUserWidget* Widget = (UUserWidget*)Obj;
				Widget->SetVisibility(ESlateVisibility::Visible);
				Widget->SetIsEnabled(true);
				break;
			}
			case EBFPoolType::Object: break; // Not sure what behaviour a uobject needs, ill leave it up to the user with the interface call below.
			case EBFPoolType::Invalid: bfEnsure(false); break;
		}
	}
	
	if(Obj->template Implements<UBFPooledObjectInterface>())
		IBFPooledObjectInterface::Execute_OnObjectUnPooled(Obj);
}


template<typename T, ESPMode Mode>
requires BF::OP::CIs_UObject<T>
void TBFObjectPool<T,  Mode>::DeactivateObject(T* Obj)
{
	bfValid(Obj);

	if(bIsDeactivateObjectOverridden)
	{
		PoolInitInfo.DeactivateObjectOverride.Execute(Obj);
	}
	else if(!PoolInitInfo.bDisableActivationDeactivationLogic)
	{
		switch (GetPoolType())
		{
			case EBFPoolType::Actor:
			{
				AActor* Actor = (AActor*)(Obj);
				Actor->SetActorTickEnabled(false);
				Actor->SetActorHiddenInGame(true);
				Actor->SetActorEnableCollision(false);
				break;
			}
			case EBFPoolType::Component:
			{
				UActorComponent* Component = (UActorComponent*)(Obj);
				Component->Deactivate();
				break;
			}
			case EBFPoolType::UserWidget:
			{
				UUserWidget* Widget = (UUserWidget*)Obj;
				Widget->SetVisibility(ESlateVisibility::Collapsed);
				Widget->SetIsEnabled(false);
				break;
			}
			case EBFPoolType::Object: break; // Not sure what behaviour a uobject needs, ill leave it up to the user with the interface call below.
			case EBFPoolType::Invalid: bfEnsure(false); break;
		}
	}

	if(Obj->template Implements<UBFPooledObjectInterface>())
		IBFPooledObjectInterface::Execute_OnObjectPooled(Obj);
}


























