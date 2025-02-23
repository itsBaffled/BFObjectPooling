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
#include "Misc/EngineVersionComparison.h"
#include "BFObjectPool.generated.h"

struct FBFPooledObjectInfo;
class UBFPoolContainer;


template<typename T, ESPMode Mode = ESPMode::NotThreadSafe>
using TBFObjectPoolPtr = TSharedPtr<TBFObjectPool<T, Mode>, Mode>;


/** TBFObjectPool:
 * This pool aims to be a generic object pool that can be used to pool any Actor, Component, UserWidget or UObject.
 * 
 * The Pool Is Primarily Composed Of 3 Main Parts
 *
 * 1)- The Templated Struct Pool itself: `TBFObjectPool`
 *		- The pool itself is responsible for maintaining the pooled objects while they are in an inactive state as well as providing an API for being interacted with.
 *		- The pool should be stored only as a shared pointer to the pool itself, this means either TSharedPtr<TBFObjectPool<T, Mode>, Mode> (which is verbose) or use the using alias `TFBObjectPoolPtr<T, Mode>` 
 *		- The only time you will need to referer to the pool as TBFObjectPool instead of TBFObjectPoolPtr is when first setting your pointer and creating the pool with either of these factory methods:
 *			`MyPool = TBFObjectPool<MyType>::CreatePool()`
 *			`MyPool = TBFObjectPool<MyType>::CreateAndInitPool(MyInitParams)` 
 *
 *		
 * 2)- The Pool Container: `UBFPoolContainer`
 *		- UObject that is used to wrap pooled objects storage in a GC friendly way, handle ticking the pool and stores other meta data about the pooled objects.
 *		- Should never be interacted with directly.
 *
 *
 * 3)- The Pooled Object Handle: `TBFPooledObjectHandle` which is primarily interacted with via shared pointer alias `TBFPooledObjectHandlePtr` for the same convenience reasons.
 *		- When attempting to un-pool an object it is returned to you via a shared pointer to the handle `TBFPooledObjectHandle` as `TBFPooledObjectHandlePtr`.
 *		- The ONLY time a returned pointer handle will be null is when the pool is already at capacity and all objects are in use or cannot be force recalimed (more on this later).
 *		- If you let all shared pointers to the handle go out of scope, the pooled object will be returned to the pool automatically. Otherwise you
 *			can manually return the object to the pool via the handle or pool itself.
 *		- BP side wraps shared pointers via an exposed struct that can be copied around and stored, it has an easy to use function library API for interacting.
 *		- Due to sharing and copying of these handle pointers you may have a handle that goes stale. This occurs when a different copy has returned the object to the pool or
 *			when the object has been stolen via `StealObject()`. To avoid this you should always check the validity of the handle via `Handle->IsHandleValid` but even if you don't the
 *			`Handle->GetObject()` function will still not return the object if it has gone stale.
 *			This can however be forced by calling `Handle->GetObject(true)` which will return the object even if it has gone stale (Should be avoided whenever possible).
 *
 *
 *
 * The pool also supports regularly querying the inactive objects and removing them if they exceed the
 * specified MaxObjectInactiveOccupancySeconds in the pool init function.
 * If you do not want this behaviour, you can leave ticking disabled on the pool which is the default behaviour.
 * You also can optionally set the tick interval to a higher value to reduce the overhead of checking the pools occupancy.
 * 
 * NOTE: The pools debug console variables `BF.OP.PrintPoolOccupancy` and `BF.OP.EnableLogging` can help when debugging issues,
 *		the `PrintPoolOccupancy` requires your pool to enable ticking otherwise it will not log the pools occupancy.
 *
 *
 *
 *
 * Basic usage example:
 *
 * // .h
 * // Store the pool pointer in a class header or wherever you need it, but this should be a long scope because otherwise once all shared pointers to the pool are destroyed the pool will be destroyed.
 * TBFObjectPoolPtr<AMyFoo, ESPMode::NotThreadSafe> MyPool; 
 *
 *
 *
 * // .cpp
 * // Use static factory Create method to create the pool and make sure to Init it before ANYTHING else.
 * MyPool = TBFObjectPoolPtr<AMyFoo, ESPMode::NotThreadSafe>::CreatePool();
 *
 *
 *
 * // After creation the next step is populate the init params, the only *required* params are
 * //	- Owner (you also need to populate the World pointer if the owner has no way to access the world via GetWorld())
 * //	- PoolClass
 *		
 * FBFObjectPoolInitParams Params;
 * Params.Owner = this;
 * Params.PoolClass = AMyFoo::StaticClass() or MyEditorPopulatedTSubclassOf<AMyFoo>();
 * Params.PoolLimit = 100;
 * Params.InitialCount = 5;
 *
 *
 * // Initialize the pool and done, its ready to use!
 * MyPool->InitPool(Params);
 * 
 * // or for a combined creation and initialization use the factory method
 * MyPool = TBFObjectPoolPtr<AMyFoo, ESPMode::NotThreadSafe>::CreateAndInitPool(Params);
 *
 *
 *
 *
 * // IN GAMEPLAY now you can just access pooled objects like so
 *
 * 	if(TBFPooledObjectHandlePtr<AMyFoo> Handle = ObjectPool->UnpoolObject(bAutoActivate))
 *	{
 *		// Do whatever you want with the object, my preferred method is to init the object and hand it
 *		// back its own handle so it can do what it needs to do and return when needed.
 *		// Like for example a sound would set the params and then return on sound finished by itself.
 *		
 *		AMyFoo* Obj = Handle->GetObject();
 *		Obj->SomeObjectFunctionToSetThingsUp()
 *		Obj->SetPoolHandle(Handle);
 *	}
 */


// ----------------------

/** Reclaim policy is a way to describe how important an unpooled object is to us.
 * It is only taken into account when the object pool is full and all objects are currently in use,
 * the policy is used to determine which objects can be returned to the pool in those cases.
 * Can be useful for unpooled objects that aren't super important to us but we want to ensure others can take it from us if needed.
 * It's like a last resort when all other options are exhausted. NOT USED WHEN WE HAVE A COOLDOWN TIME SET as
 * that would break a concept of a cooldown if we could just return it and use it instantly.
 */
UENUM(BlueprintType)
enum struct EBFPooledObjectReclaimPolicy : uint8
{
	// This object is ours and ours only, the pool cannot force it to be returned until we return it or all shared pointers/handles to the handle go out of scope or are invalidate.
	NonReclaimable = 0,

	/** This object isn't too important to us and if the pool we belong to is full and all objects are in use then
	 * we can be forced to return to the pool and given to the next requester. Useful for objects that are just for convenience and not critical such as
	 * certain VFX or decals like a bullet hole or muzzle flash etc.*/
	Reclaimable,
};


// Used to define how we determine which active reclaimable object to return.
UENUM(BlueprintType)
enum struct EBFPooledObjectReclaimStrategy  : uint8
{
	// Returns the oldest Reclaimable in use object ("Slowest" as It's O(N) but the elements fit nicely into the cache line and are spatially close to one another so its still quick, most reliable and preferred.)
	Oldest,
	
	// Returns the First Reclaimable object found within our Reclaimable Objects array (elements are not sorted and as we return new elements the order swaps constantly).
	FirstFound,
	
	// Returns the Last Reclaimable object found within our Reclaimable Objects array (elements are not sorted and as we return new elements the order swaps constantly).
	LastFound,
	
	// Returns a random Reclaimable object.
	Random,
};


/** Each pool can optionally tick, defines params related to that.
 * A tickable pool has 2 use cases:
 * - A) Used to periodically poll the pools inactive objects and remove any that have been inactive for longer than the MaxObjectInactiveOccupancySeconds.
 * - B) Used in combination with `BF.OP.PrintPoolOccupancy` to print the pools occupancy to the screen, great for debugging. */
USTRUCT(Blueprintable)
struct FBFObjectPoolInitTickParams
{
	GENERATED_BODY()
public:
	UPROPERTY(Transient, BlueprintReadWrite)
	float TickInterval = 1.f;

	/** If set above 0 then every tick the pool will evaluate its inactive objects and remove any that have been inactive for longer than this.
	 *  Should severely reduce tick rate as this isn't something needs to be done often at all. */
	UPROPERTY(Transient, BlueprintReadWrite)
	float MaxObjectInactiveOccupancySeconds = -1.f;

	UPROPERTY(Transient, BlueprintReadWrite)
	bool bEnableTicking = false;

	UPROPERTY(Transient, BlueprintReadWrite)
	TEnumAsByte<ETickingGroup> TickGroup = TG_DuringPhysics;
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
		World = nullptr;
		PoolClass = nullptr;
		ForceReturnReclaimStrategy = EBFPooledObjectReclaimStrategy::Oldest;
		PoolLimit = 50;
		InitialCount = 0;
		CooldownTimeSeconds = -1.f;
		PoolTickInfo = FBFObjectPoolInitTickParams();
		bDisableActivationDeactivationLogic = false;
		bAdoptionOnlyPool = false;
		ObjectFlags = RF_NoFlags;
	}
	
public:
	/** When activating and deactivating the pooled objects, if you dont like the default behaviour or want something more custom at the Global level then that's what these exist for,
	 * If bound we call the delegate with the object being unpooled or pooled at the correct time and let you handle the rest, nice for networking pools that use dormancy or some other optimizations.
	 * A few use cases could be for static/skeletal meshes, after spawning those actors and the curfew elapses ready to be returned to the pool you may choose
	 * to leave the actor not hidden but instead visible but inactive. Then on subsequent unpooling attempts it would just randomly take from the pool and activate it in the newer location.
	 * YOU SHOULD ONLY BIND IF YOU WANT THIS BEHAVIOUR. */
	UPROPERTY(BlueprintReadWrite)
	FActivatePooledObjectOverride ActivateObjectOverride;
	UPROPERTY(BlueprintReadWrite)
	FDeactivatePooledObjectOverride DeactivateObjectOverride;
	
	// The owner of the pool, this is the object that will own the pooled objects as well. SHOULD ALWAYS OUTLIVE THE POOL.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TObjectPtr<UObject> Owner = nullptr;

	// Can be left null but the owner must have a way to retrieve the world via GetWorld(), otherwise you can populate this instead.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TObjectPtr<UWorld> World = nullptr;
	
	// Defines the specific class for the pool when we are creating new objects.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TObjectPtr<UClass> PoolClass = nullptr;
	
	// Max num of objects this pool is allowed to create, can be increased/decreased via the pool at runtime.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	int32 PoolLimit = 50;
	
	// How many pooled object to create upfront, otherwise we lazy create and pool objects when needed.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	int32 InitialCount = 0;

	/**If greater than 0, when trying to take an object from the pool it must have been in the pool for at least as long as this (NOT APPLIED FIRST TIME A NEW OBJECT IS CREATED).
	 * (I've noticed ribbon VFX if being pooled and reused too quickly can cause the ribbon trail to span from its last position, it's just a nice option to have.)*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float CooldownTimeSeconds = -1.f;
	
	/** Defines the tick behaviour of the pool, ticking can ignored and it will be disabled by default.
	 * If disabled you lose the ability to clear out inactive objects that exceed the MaxObjectInactiveOccupancySeconds,
	 * tbh this is the desired behaviour most of the time anyway since the whole point of a pool is to pre-spawn and keep but who knows. */ 
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FBFObjectPoolInitTickParams PoolTickInfo;

	// Defines how we reclaim objects when the pool is at capacity and we need to force return an object.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	EBFPooledObjectReclaimStrategy ForceReturnReclaimStrategy = EBFPooledObjectReclaimStrategy::Oldest;
	
	/** If true when taking and returning objects to/from the pool we will NEVER attempt to manually activate/deactivate them (even if the function param bAutoActivate is set,
	 * its ignored. This is important if you want to un-pool objects and *completely* handle activating and deactivating them yourself.) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	uint8 bDisableActivationDeactivationLogic : 1 = false;

	/** If true the pool will not try to create new objects at all, it only can be given them via the AdoptObject function. Useful when you have a pool of
	 * specific objects and you want to control what goes in. UUserWidgets is one case where you would many different widget types and then lean on the special unpool
	 * function such as UnpoolObjectByTag or UnpoolObjectByPredicate . */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	uint8 bAdoptionOnlyPool : 1 = false;

	/** Flags for each new spawned object in the pool, If left to default then we will apply the flag RF_Transient and remove the default flag of RF_Transactional
	 * (which makes spawning a lot cheaper and pooled objects should be transient anyway)
	 * otherwise just add your own flags and no default flags will be applied.  */
	EObjectFlags ObjectFlags = RF_NoFlags;
};


template<typename T, ESPMode Mode = ESPMode::NotThreadSafe> requires BF::OP::CIs_UObject<T>
struct TBFObjectPool : public TSharedFromThis<TBFObjectPool<T, Mode>, Mode> , FGCObject
{
	using FOnObjectAddedToPool =		TMulticastDelegate<void(UObject* Object, int64 ID)>;
	using FOnObjectRemovedFromPool =	TMulticastDelegate<void(int64 ID, int32 CheckoutID)>;
	using FOnObjectPooled =				TMulticastDelegate<void(UObject* Object, bool bIsBeingReturnedToPool, int64 ID, int32 CheckoutID)>;
	
protected:
	template <typename ObjectType, ESPMode PtrMode>
	friend class SharedPointerInternals::TIntrusiveReferenceController;
	friend struct TBFPooledObjectHandle<T, Mode>;
	friend class UBFGameplayFXSubsystem;

	/** Widgets have compile checks of CreateWidget functions so I am forced to use a specific owner type for widget pools,
	 * I store the owner type here to make creation faster for pooled widget objects.*/
	enum struct EWidgetPoolOwnerType : uint8
	{
		Invalid,
		PlayerController,
		Widget,
		GameInstance,
		World
	};

	// Determines the type of activation/deactivation logic we should use for the pool as well as how they are created.
	enum struct EBFPoolType : uint8
	{
		Invalid = 0,
		Actor,			
		Component,		
		UserWidget,
		Object			
	};

	// Book-keeping struct for objects that have been un-pooled and are now in use but can reclaimed at any time.
	struct FReclaimableUnpooledObjectInfo
	{
		int64 PoolID = -1;
		int32 CheckoutID = -1;
		float TimeUnpooled = 0.f;
	};
	
	// Protected to force factory function for creation of pools.
	TBFObjectPool() = default;
	virtual bool ReturnToPool_Internal(int64 PoolID, int32 ObjectCheckoutID, bool bSkipAddingToInactivePool = false);

public:
	// No copy ctors, pools should always be unique and created via the factory method but we do allow moving if needed,
	// just be careful because the owner will still be the Initial owner, I might add rename functionality later for changing outers.
	TBFObjectPool(const TBFObjectPool& Rhs) = delete;
	TBFObjectPool& operator=(const TBFObjectPool& Rhs) = delete;

	// Supports move semantics.
	TBFObjectPool(TBFObjectPool&& Rhs) noexcept;
	TBFObjectPool& operator=(TBFObjectPool&& Rhs) noexcept;

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		Collector.AddReferencedObject(PoolContainer);
		Collector.AddReferencedObject(PoolInitInfo.Owner);
		Collector.AddReferencedObject(PoolInitInfo.PoolClass);
	}
	
	virtual FString GetReferencerName() const override {return TEXT("TBFObjectPool");}

	
	// Owner should ALWAYS outlive the pool.
	template<typename Ty>
	Ty* GetOwner() const { return CastChecked<Ty>(PoolInitInfo.Owner.Get()); }
	UObject* GetOwner() const { return PoolInitInfo.Owner.Get(); }
	
	// Static factory method for returning a shared pointer to a new pool.
	static TBFObjectPoolPtr<T, Mode> CreatePool() { return MakeShared<TBFObjectPool, Mode>(); }
	
	// Static factory method for creating and returning a shared pointer to a new pool as well as initializing it.
	static TBFObjectPoolPtr<T, Mode> CreateAndInitPool(const FBFObjectPoolInitParams& Info)
	{
		TBFObjectPoolPtr<T, Mode> P = MakeShared<TBFObjectPool, Mode>();
		P->InitPool(Info);
		return P;
	}

	// Every pool must first call this init function before trying to use it.
	virtual void InitPool(const FBFObjectPoolInitParams& Info);

	/** Returns a valid pooled object unless at capacity. Should always check the returned handle pointer for `IsValid` in case the pool was all used and at capacity or couldn't reclaim an object.
	 * "IsHandleValid" will always be true at this stage if the returned ptr was valid so need to check for that.
	 * If bAutoActivate is false, the user is responsible for activating the object (depending on the type this may include things like un-hiding or enable collision etc).
	 * Check the ActivateObject() for a general idea. */
	virtual TBFPooledObjectHandlePtr<T, Mode> UnpoolObject(bool bAutoActivate, EBFPooledObjectReclaimPolicy Policy = EBFPooledObjectReclaimPolicy::NonReclaimable);

	/** Iterates the inactive and reclaimable object pools searching all objects calling the interface function GetObjectGameplayTag(), this requires you to have implemented
	 * the function on the object otherwise you won't get anything back. Useful if you pool is of specific objects and not generic reusable ones, otherwise check "UnpoolObjectByPredicate()". */
	virtual TBFPooledObjectHandlePtr<T, Mode> UnpoolObjectByTag(FGameplayTag Tag, bool bAutoActivate, EBFPooledObjectReclaimPolicy Policy = EBFPooledObjectReclaimPolicy::NonReclaimable);

	/** Attempts to unpool the first object that matches the predicate, checks the inactive objects pool to begin with
	 * and then checks the reclaimable pool to see if one of those active objects passes our predicate.*/
	virtual TBFPooledObjectHandlePtr<T, Mode> UnpoolObjectByPredicate(TFunction<bool(const T* Object)> Pred, bool bAutoActivate, EBFPooledObjectReclaimPolicy Policy = EBFPooledObjectReclaimPolicy::NonReclaimable);

	/** If we have room, will attempt to take in the object in as our own. The object must be of the pool type or a child of it.
	 * Useful in combination with UnpoolObjectByTag() or UnpoolObjectByPredicate() to later try fetch these objects. A use case could be
	 * for widgets, we store them here as a generic UUserWidget and you can later fetch your specific widget by whatever tag or predicate you want.
	 * Also see bAdoptionOnlyPool in the init params if we want this pool TO ONLY accept objects via this function and not create our own.
	 * Changing the outer is dangerous if you don't understand what it's for or are using the pool in a network context, just leave it false. */
	bool AdoptObject(T* Object, bool bChangeOuter = false);

	/** When a pooled object is being used, you have the ability to keep it and steal it from the pool,
	 * this will invalidate any handles and return the object ready to be managed by you, depending on the pool type
	 * the returned object may have an outer that is not desired, consider "Renaming" your returned object to change it's outer. */
	T* StealObject(TBFPooledObjectHandlePtr<T, Mode>& Handle);
	T* StealObject(int64 PoolID, int32 ObjectCheckoutID);

	// Returns the object to the pool for later use `MyPool->ReturnToPool(Handle)` or alternatively `Handle->ReturnToPool()` if the handle is still valid.
	virtual bool ReturnToPool(TBFPooledObjectHandlePtr<T, Mode>& Handle);

	// Attempts to remove the specified amount of inactive objects from the pool, will not remove any if the pool is already empty or if we can't remove the specified amount.
	virtual bool RemoveInactiveNumFromPool(int64 NumToRemove);

	// Clears all inactive objects from the pool if there are any.
	virtual bool ClearInactiveObjectsPool();
	
	// Checks not only if the pools contains an Object with the given ID, but also if our specific handles checkout ID is the same as the pooled objects current checkout ID.
	virtual bool IsObjectIDValid(int64 PoolID, int32 ObjectCheckoutID) const;
	virtual bool IsHandleValid(TBFPooledObjectHandlePtr<T, Mode>& Handle) const { return Handle.IsValid() && IsObjectIDValid(Handle->GetPoolID(), Handle->GetCheckoutID()); }
	
	// Checks if the given object is inactive in the pool and not currently in use, also returns false if we can't find the object.
	virtual bool IsObjectInactive(int64 PoolID, int32 ObjectCheckoutID) const;

	int32 GetPoolNum() const { return PoolContainer->ObjectPool.Num(); }
	int32 GetPoolLimit() const { return PoolInitInfo.PoolLimit; }
	int32 GetActivePoolNum() const { return GetPoolNum() - GetInactivePoolNum(); }
	int32 GetInactivePoolNum() const { return PoolContainer->InactiveObjectIDPool.Num(); }
	int32 GetReclaimableObjectNum() const { return ReclaimableUnpooledObjects.Num(); }
	bool IsFull() const { return GetPoolNum() >= PoolInitInfo.PoolLimit; }
	EBFPoolType GetPoolType() const { return PoolType; }
	UWorld* GetWorld() const { return IsValid(PoolInitInfo.World) ? PoolInitInfo.World.Get() : PoolInitInfo.Owner->GetWorld(); }
	float GetMaxObjectInactiveOccupancySeconds() const { return PoolInitInfo.PoolTickInfo.MaxObjectInactiveOccupancySeconds; }

	/** Called after creating a new object and adding it to the pool or when adopting an object, the passed object should only be
	 * used within the callback for any special custom setup logic, do not store the object.*/
	FOnObjectAddedToPool& GetOnObjectAddedToPool() { return OnObjectAddedToPool; }

	// Called after removing an object from the pool.
	FOnObjectRemovedFromPool& GetOnObjectRemovedFromPool() { return OnObjectRemovedFromPool; }
	
	// Called when an object is being either unpooled or returned into the pool.
	FOnObjectPooled& GetOnObjectPooled() { return OnObjectPooled; }
	
	
	/** Can only decrease pool limit if we are able to remove enough inactive objects to meet the new limit, we do not remove active ones,
	 * so if the new limit is below the current active count then we can't decrease the limit and false is returned.*/
	virtual bool SetPoolLimit(int32 InPoolLimit);

	// How long object may remain inactive before removed (Requires Tick enabled). Less than 0 for indefinite occupancy.
	virtual void SetMaxObjectInactiveOccupancySeconds(float MaxObjectInactiveOccupancySeconds);

	// Can be manually called to evaluate the pool occupancy and remove inactive objects if they exceed the MaxObjectInactiveOccupancySeconds.
	virtual bool EvaluatePoolOccupancy();
	
	/** When tick is disabled, there is no clearing out of inactive objects. If you want tick you also can
	 * optionally set its Interval for better performance since clearing inactive pool objects isn't really that important.*/
	virtual void SetTickEnabled(bool bEnable);
	virtual void SetTickGroup(ETickingGroup InTickGroup) { PoolContainer->SetTickGroup(InTickGroup); }
	virtual void SetTickInterval(float InTickInterval);
	float GetTickInterval() const {return PoolInitInfo.PoolTickInfo.TickInterval;}
	bool GetTickEnabled()const {return PoolContainer->GetTickEnabled();}
	const FBFObjectPoolInitParams& GetPoolInitInfo() const {return PoolInitInfo;}
	bool HasBeenInitialized() const {return bHasBeenInitialized;}
protected:
	virtual FBFPooledObjectInfo* CreateNewPoolEntry();
	virtual void ActivateObject(T* Obj, bool bAutoActivate);
	virtual void DeactivateObject(T* Obj);
	virtual void Reset();
	virtual void Tick(UWorld* World, float Dt);
	virtual int64 TryReclaimUnpooledObject();

protected:

	// Called upon a new object being created or deleted from the pool.
	FOnObjectAddedToPool OnObjectAddedToPool;
	FOnObjectRemovedFromPool OnObjectRemovedFromPool;
	// Called when an object is being used or returned from the pool
	FOnObjectPooled OnObjectPooled;

	TObjectPtr<UBFPoolContainer> PoolContainer = nullptr;
	TArray<FReclaimableUnpooledObjectInfo> ReclaimableUnpooledObjects;
	FBFObjectPoolInitParams PoolInitInfo;

	// Ever incrementing ID that is statically assigned to each new object.
	int64 CurrentPoolIDIndex = -1;
	EWidgetPoolOwnerType WidgetPoolOwnerType = EWidgetPoolOwnerType::Invalid;
	EBFPoolType PoolType = EBFPoolType::Invalid;
	uint8 bIsActivateObjectOverridden : 1 = false;
	uint8 bIsDeactivateObjectOverridden : 1 = false;
	uint8 bHasBeenInitialized : 1 = false;
};



template <typename T, ESPMode Mode> 
requires BF::OP::CIs_UObject<T>
TBFObjectPool<T, Mode>::TBFObjectPool(TBFObjectPool&& Rhs) noexcept
{
	OnObjectAddedToPool = std::move(Rhs.OnObjectAddedToPool);
	OnObjectRemovedFromPool = std::move(Rhs.OnObjectRemovedFromPool);
	OnObjectPooled = std::move(Rhs.OnObjectPooled);
	
	PoolContainer = std::move(Rhs.PoolContainer);
	ReclaimableUnpooledObjects = std::move(Rhs.ReclaimableUnpooledObjects);
	PoolInitInfo = std::move(Rhs.PoolInitInfo);
	
	CurrentPoolIDIndex = Rhs.CurrentPoolIDIndex;
	WidgetPoolOwnerType = Rhs.WidgetPoolOwnerType;
	PoolType = Rhs.PoolType;
	bIsActivateObjectOverridden = Rhs.bIsActivateObjectOverridden;
	bIsDeactivateObjectOverridden = Rhs.bIsDeactivateObjectOverridden;
	bHasBeenInitialized = Rhs.bHasBeenInitialized;
	
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
	OnObjectPooled = std::move(Rhs.OnObjectPooled);
	
	PoolContainer = std::move(Rhs.PoolContainer);
	ReclaimableUnpooledObjects = std::move(Rhs.ReclaimableUnpooledObjects);
	PoolInitInfo = std::move(Rhs.PoolInitInfo);
	
	CurrentPoolIDIndex = Rhs.CurrentPoolIDIndex;
	WidgetPoolOwnerType = Rhs.WidgetPoolOwnerType;
	PoolType = Rhs.PoolType;
	bIsActivateObjectOverridden = Rhs.bIsActivateObjectOverridden;
	bIsDeactivateObjectOverridden = Rhs.bIsDeactivateObjectOverridden;
	bHasBeenInitialized = Rhs.bHasBeenInitialized;
	
	Rhs.Reset();
	return *this;
}


template <typename T, ESPMode Mode> 
requires BF::OP::CIs_UObject<T>
void TBFObjectPool<T, Mode>::Reset()
{
	OnObjectAddedToPool.Clear();
	OnObjectRemovedFromPool.Clear();
	OnObjectPooled.Clear();
	
	PoolContainer = nullptr;
	ReclaimableUnpooledObjects.Reset();
	PoolInitInfo.Reset();
	
	CurrentPoolIDIndex = -1;
	WidgetPoolOwnerType = EWidgetPoolOwnerType::Invalid;
	PoolType = EBFPoolType::Invalid;
	bIsActivateObjectOverridden = false;
	bIsDeactivateObjectOverridden = false;
	bHasBeenInitialized = false;
}


template<typename T, ESPMode Mode>
requires BF::OP::CIs_UObject<T>
void TBFObjectPool<T, Mode>::InitPool(const FBFObjectPoolInitParams& Info)
{
	
	if(	Info.Owner == nullptr
		|| (!IsValid(Info.Owner->GetWorld()) && !IsValid(Info.World))
		|| (IsValid(PoolContainer) && PoolContainer->ObjectPool.Num() > 0)
		|| !IsValid(Info.PoolClass)
		|| Info.InitialCount < 0
		|| Info.PoolLimit < 1
		|| Info.InitialCount > Info.PoolLimit
		)
	{
#if !UE_BUILD_SHIPPING
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Red, TEXT("Failed to initialize object pool, check console output log."));
		}
		
		// Ensure we understand why the pool is failing for devs.
		if(Info.Owner == nullptr)
			UE_LOGFMT(LogTemp, Error, "Error: Pool owner must be valid.");
		if (!IsValid(Info.Owner->GetWorld()) && !IsValid(Info.World))
			UE_LOGFMT(LogTemp, Error, "Error: Pool must be able to access the world, either via the Owner or The explicit World pointer in the init settings.");
		if (IsValid(PoolContainer) && PoolContainer->ObjectPool.Num() > 0) 
			UE_LOGFMT(LogTemp, Error, "Error: Pool must be cleared before re-initializing, pool contains {0} objects.", PoolContainer->ObjectPool.Num());
		if (!IsValid(Info.PoolClass))
			UE_LOGFMT(LogTemp, Error, "Error: Pool class must be set.");
		if (Info.InitialCount < 0) 
			UE_LOGFMT(LogTemp, Error, "Error: Initial count must be greater than or equal to 0.");
		if (Info.PoolLimit < 1) 
			UE_LOGFMT(LogTemp, Error, "Error: Pool limit must be greater than or equal to 1.");
		if (Info.InitialCount > Info.PoolLimit) 
			UE_LOGFMT(LogTemp, Error, "Error: Initial count must be less than or equal to the pool limit.");
#endif
		return;
	}

	if (Info.PoolClass->IsChildOf<AActor>())				PoolType = EBFPoolType::Actor;
	else if (Info.PoolClass->IsChildOf<UActorComponent>())	PoolType = EBFPoolType::Component;
	else if (Info.PoolClass->IsChildOf<UUserWidget>())		PoolType = EBFPoolType::UserWidget;
	else if (Info.PoolClass->IsChildOf<UObject>())			PoolType = EBFPoolType::Object;

	if (PoolType == EBFPoolType::Invalid)
	{
		UE_LOG(LogTemp, Error, TEXT("Error: The pool class must be a child of AActor, UActorComponent, UUserWidget or UObject."));
		return;
	}
	

	if (PoolType == EBFPoolType::UserWidget)
	{
		WidgetPoolOwnerType = EWidgetPoolOwnerType::Invalid;
		if (Cast<APlayerController>(Info.Owner))	WidgetPoolOwnerType = EWidgetPoolOwnerType::PlayerController;
		else if (Cast<UUserWidget>(Info.Owner))		WidgetPoolOwnerType = EWidgetPoolOwnerType::Widget;
		else if (Cast<UGameInstance>(Info.Owner))	WidgetPoolOwnerType = EWidgetPoolOwnerType::GameInstance;
		else if (Cast<UWorld>(Info.Owner))			WidgetPoolOwnerType = EWidgetPoolOwnerType::World;
		
		if (WidgetPoolOwnerType == EWidgetPoolOwnerType::Invalid)
		{
			UE_LOG(LogTemp, Error, TEXT("Error: The widget pool owner must be either a - PlayerController, UserWidget, GameInstance or World."));
			return;
		}
	}
	
	PoolInitInfo = Info;
	bIsActivateObjectOverridden = PoolInitInfo.ActivateObjectOverride.IsBound();
	bIsDeactivateObjectOverridden = PoolInitInfo.DeactivateObjectOverride.IsBound();

	if(!IsValid(PoolContainer)) // Reuse if we are re-initializing the pool.
	{
		PoolContainer = NewObject<UBFPoolContainer>(Info.Owner.Get());
	}

	PoolContainer->Init([WeakThis = this->AsWeak()](UWorld* World, float Dt)
	{
		if(WeakThis.IsValid())
			WeakThis.Pin()->Tick(World, Dt);
	},  GetWorld(), PoolInitInfo.PoolTickInfo.TickInterval);

	PoolContainer->SetTickEnabled(PoolInitInfo.PoolTickInfo.bEnableTicking);
	PoolContainer->SetTickGroup(PoolInitInfo.PoolTickInfo.TickGroup);

	int32 Count = Info.InitialCount;
	while(Count--)
	{
		CreateNewPoolEntry();
	}
	
	bHasBeenInitialized = true;
}


template <typename T, ESPMode Mode> 
requires BF::OP::CIs_UObject<T>
void TBFObjectPool<T, Mode>::Tick(UWorld* World, float Dt)
{
#if !UE_BUILD_SHIPPING
	// We register tick through our Init function so we can never NOT be sure the owner and pool class is valid because we ensure/early out there 
	if(BF::OP::CVarObjectPoolPrintPoolOccupancy.GetValueOnGameThread())
	{
		bfEnsure(IsValid(PoolContainer));
		FString FormatString = FString::Format(
			TEXT("Object Pool {0} {1} \n- Total Size:{2}/{3}\n- Active Objects: {4}\n- Inactive Objects: {5}\n- Max Inactive Occupancy: {6}\n- Cooldown Time: {7}\n- Is Adoption only pool: {8}"),
			{
				GetNameSafe(PoolInitInfo.PoolClass),
				GetNameSafe(PoolInitInfo.Owner),
				PoolContainer->ObjectPool.Num(),
				PoolInitInfo.PoolLimit,
				PoolContainer->ObjectPool.Num() - PoolContainer->InactiveObjectIDPool.Num(),
				PoolContainer->InactiveObjectIDPool.Num(), GetMaxObjectInactiveOccupancySeconds(),
				PoolInitInfo.CooldownTimeSeconds,
				PoolInitInfo.bAdoptionOnlyPool
			});

		// Each log is based on the pools unique memory address and the GPlayInEditorID to ensure we can differentiate between pools even in the same PIE session.
#if UE_VERSION_OLDER_THAN(5, 5, 0)
		uint64 UniqueKey = GetTypeHash(reinterpret_cast<uint64>(this)) + GPlayInEditorID;
#else
		uint64 UniqueKey = GetTypeHash(reinterpret_cast<uint64>(this)) + UE::GetPlayInEditorID();
#endif

		if(!GEngine || World->GetNetMode() == NM_DedicatedServer)
			UE_LOGFMT(LogTemp, Warning, "{0}", FormatString);
		else
			GEngine->AddOnScreenDebugMessage(UniqueKey, 8.f, FColor::Green, FormatString);
	}
#endif

	if(GetMaxObjectInactiveOccupancySeconds() > 0.f)
		EvaluatePoolOccupancy();
}


template<typename T, ESPMode Mode>
requires BF::OP::CIs_UObject<T>
FBFPooledObjectInfo* TBFObjectPool<T, Mode>::CreateNewPoolEntry()
{
	bfEnsure(IsValid(PoolContainer) && IsValid(PoolInitInfo.Owner)); // this should never be hit, if it is then something went wrong.	
	SCOPED_NAMED_EVENT(TBFObjectPool_CreateNewPoolEntry, FColor::Green);
	
	if(GetPoolNum() >= PoolInitInfo.PoolLimit || PoolInitInfo.bAdoptionOnlyPool)
		return nullptr;

	EObjectFlags Flags = PoolInitInfo.ObjectFlags == RF_NoFlags ? RF_Transient : PoolInitInfo.ObjectFlags;

	UObject* Object = nullptr;
	switch (GetPoolType())
	{
		case EBFPoolType::Actor:
		{
			AActor* NewPoolObject = nullptr;
			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = Cast<AActor>(PoolInitInfo.Owner.Get()); // Perfectly valid if the pool isn't owned by an AActor
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			SpawnParams.ObjectFlags = Flags;
			NewPoolObject = GetWorld()->SpawnActor<AActor>(PoolInitInfo.PoolClass, SpawnParams);

			if (!NewPoolObject)
			{
				UE_LOGFMT(LogTemp, Error, "Error: Failed to create new object of type {0} for pool {1}.", GetNameSafe(PoolInitInfo.PoolClass), GetNameSafe(PoolInitInfo.Owner));
				return nullptr;				
			}
				
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
			if (!NewPoolObject)
			{
				UE_LOGFMT(LogTemp, Error, "Error: Failed to create new object of type {0} for pool {1}.",  GetNameSafe(PoolInitInfo.PoolClass), GetNameSafe(PoolInitInfo.Owner));
				return nullptr;				
			}
				
			NewPoolObject->bAutoActivate = false;

			if(AActor* Actor = Cast<AActor>(PoolInitInfo.Owner))
			{
				Actor->AddInstanceComponent(NewPoolObject);
				NewPoolObject->RegisterComponent();
			}
				
			Object = NewPoolObject;
			break;
		}
		case EBFPoolType::UserWidget:
		{
			UUserWidget* NewPoolObject = nullptr;

			switch (WidgetPoolOwnerType)
			{
			case EWidgetPoolOwnerType::Widget:				NewPoolObject = CreateWidget<UUserWidget>(CastChecked<UWidget>(PoolInitInfo.Owner.Get()), PoolInitInfo.PoolClass); break;
			case EWidgetPoolOwnerType::PlayerController:	NewPoolObject = CreateWidget<UUserWidget>(CastChecked<APlayerController>(PoolInitInfo.Owner.Get()), PoolInitInfo.PoolClass); break;
			case EWidgetPoolOwnerType::World:				NewPoolObject = CreateWidget<UUserWidget>(CastChecked<UWorld>(PoolInitInfo.Owner.Get()), PoolInitInfo.PoolClass); break;
			case EWidgetPoolOwnerType::GameInstance:		NewPoolObject = CreateWidget<UUserWidget>(CastChecked<UGameInstance>(PoolInitInfo.Owner.Get()), PoolInitInfo.PoolClass); break;

			// Should never be hit.
			case EWidgetPoolOwnerType::Invalid:
				default: bfEnsure(false); return nullptr;
			}
				
			if (!NewPoolObject)
			{
				UE_LOGFMT(LogTemp, Error, "Error: Failed to create new object of type {0} for pool {1}.",  GetNameSafe(PoolInitInfo.PoolClass), GetNameSafe(PoolInitInfo.Owner));
				return nullptr;				
			}

			NewPoolObject->SetVisibility(ESlateVisibility::Collapsed);
			Object = NewPoolObject;
			break;
		}
		case EBFPoolType::Object:
		{
			Object = NewObject<UObject>(PoolInitInfo.Owner.Get(), PoolInitInfo.PoolClass, NAME_None, Flags);

			if (!Object)
			{
				UE_LOGFMT(LogTemp, Error, "Error: Failed to create new object of type {0} for pool {1}.",  GetNameSafe(PoolInitInfo.PoolClass), GetNameSafe(PoolInitInfo.Owner));
				return nullptr;				
			}		
			break;
		}

		// Should never be hit.
		case EBFPoolType::Invalid:
		default:
			bfEnsure(false); return nullptr;
	}
	
	bfValid(Object); // Should be valid always, if not then something went wrong.
	
	// Add a small initial offset to the LastActiveTime in the case when using a cooldown time, this ensures the first pooling doesn't block instant access.
	float CooldownOffset = PoolInitInfo.CooldownTimeSeconds > 0.f ? PoolInitInfo.CooldownTimeSeconds + KINDA_SMALL_NUMBER : 0.f;
	float TimeNow = GetWorld()->GetTimeSeconds();
	
	FBFPooledObjectInfo Info;
	Info.ObjectPoolID = ++CurrentPoolIDIndex; // -1 Is considered an invalid Index
	Info.CreationTime = TimeNow;
	Info.LastTimeActive = TimeNow - CooldownOffset;
	Info.PooledObject = Object;
	++Info.ObjectCheckoutID; // Don't need to but just feel better starting above -1

	FBFPooledObjectInfo* NewInfo = &PoolContainer->ObjectPool.Add(Info.ObjectPoolID, Info);
	PoolContainer->InactiveObjectIDPool.Add(Info.ObjectPoolID);

	if(Object->template Implements< UBFPooledObjectInterface>())
		IBFPooledObjectInterface::Execute_OnObjectCreated(Object);

	OnObjectAddedToPool.Broadcast(Info.PooledObject, Info.ObjectPoolID);
	return NewInfo;
}


template<typename T, ESPMode Mode>
requires BF::OP::CIs_UObject<T>
TBFPooledObjectHandlePtr<T, Mode> TBFObjectPool<T, Mode>::UnpoolObject(bool bAutoActivate, EBFPooledObjectReclaimPolicy Policy)
{
	if (!HasBeenInitialized())
	{
		UE_LOGFMT(LogTemp, Error, "Error: Pool has not been initialized.");
		return nullptr;
	}
	
	float TimeNow = GetWorld()->GetTimeSeconds();
	
	// Might not have any free but we have space to make more.
	if(PoolContainer->InactiveObjectIDPool.Num() == 0)
	{
		// First if we haven't reached capacity then we can just create a new object.
		if(GetPoolNum() < PoolInitInfo.PoolLimit)
		{
			FBFPooledObjectInfo* Entry = CreateNewPoolEntry();
			if (!Entry)
			{
#if !UE_BUILD_SHIPPING
				if(BF::OP::CVarObjectPoolEnableLogging.GetValueOnAnyThread())
					UE_LOGFMT(LogTemp, Warning, "[BFObjectPool] Trying to get a pooled object for {0} but but failed to create a new entry.", GetOwner()->GetName());
#endif
				return nullptr;
			}
		}
		else
		{
			// Otherwise we are at capacity, lets see if we can force a return.
			if (ReclaimableUnpooledObjects.Num() > 0)
			{
				int64 ObjectID = TryReclaimUnpooledObject();
				if (ObjectID != -1)
				{
					FBFPooledObjectInfo& Info = PoolContainer->ObjectPool[ObjectID];
					++Info.ObjectCheckoutID;
					ActivateObject(CastChecked<T>(Info.PooledObject), bAutoActivate);
					Info.bActive = true;
					Info.LastTimeActive = TimeNow;

					if (Policy == EBFPooledObjectReclaimPolicy::Reclaimable)
					{
						Info.bIsReclaimable = true;
						ReclaimableUnpooledObjects.Add({Info.ObjectPoolID, Info.ObjectCheckoutID, TimeNow});
					}
					
					OnObjectPooled.Broadcast(Info.PooledObject, false, Info.ObjectPoolID, Info.ObjectCheckoutID);
					return MakeShared<TBFPooledObjectHandle<T, Mode>, Mode>(&Info, TWeakPtr<TBFObjectPool, Mode>(this->AsWeak()));
				}
			}

			// No force return available, we can't do anything, return null.
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
		Info.LastTimeActive = TimeNow;
		
		if (Policy == EBFPooledObjectReclaimPolicy::Reclaimable)
		{
			Info.bIsReclaimable = true;
			ReclaimableUnpooledObjects.Add({Info.ObjectPoolID, Info.ObjectCheckoutID, TimeNow});
		}
		
		OnObjectPooled.Broadcast(Info.PooledObject, false, Info.ObjectPoolID, Info.ObjectCheckoutID);

		return MakeShared<TBFPooledObjectHandle<T, Mode>, Mode>(&Info, TWeakPtr<TBFObjectPool, Mode>(this->AsWeak()));
	}

	// Iterate inactive pool looking for the first object that meets the cooldown threshold.
	for(int64 ID : PoolContainer->InactiveObjectIDPool)
	{
		FBFPooledObjectInfo& Info = PoolContainer->ObjectPool.FindChecked(ID);
		if(TimeNow - Info.LastTimeActive < Cooldown)
			continue;

		++Info.ObjectCheckoutID;
		ActivateObject(CastChecked<T>(Info.PooledObject), bAutoActivate);
		Info.bActive = true;
		Info.LastTimeActive = TimeNow;
		PoolContainer->InactiveObjectIDPool.Remove(ID);
		
		if (Policy == EBFPooledObjectReclaimPolicy::Reclaimable)
		{
			Info.bIsReclaimable = true;
			ReclaimableUnpooledObjects.Add({Info.ObjectPoolID, Info.ObjectCheckoutID, TimeNow});
		}
		
		OnObjectPooled.Broadcast(Info.PooledObject, false, Info.ObjectPoolID, Info.ObjectCheckoutID);
		return MakeShared<TBFPooledObjectHandle<T, Mode>, Mode>(&Info, TWeakPtr<TBFObjectPool, Mode>(this->AsWeak()));
	}

	
	// This means nothing in the inactive pool met our time threshold, check one last time if we can make one now.
	if(GetPoolNum() < PoolInitInfo.PoolLimit && !PoolInitInfo.bAdoptionOnlyPool)
	{
		FBFPooledObjectInfo& Info = *CreateNewPoolEntry();

		++Info.ObjectCheckoutID;
		ActivateObject(CastChecked<T>(Info.PooledObject), bAutoActivate);
		Info.bActive = true;
		Info.LastTimeActive = TimeNow;
		PoolContainer->InactiveObjectIDPool.Remove(Info.ObjectPoolID);

		if (Policy == EBFPooledObjectReclaimPolicy::Reclaimable)
		{
			Info.bIsReclaimable = true;
			ReclaimableUnpooledObjects.Add({Info.ObjectPoolID, Info.ObjectCheckoutID, TimeNow});
		}
		
		OnObjectPooled.Broadcast(Info.PooledObject, false, Info.ObjectPoolID, Info.ObjectCheckoutID);
		return MakeShared<TBFPooledObjectHandle<T, Mode>, Mode>(&Info, TWeakPtr<TBFObjectPool, Mode>(this->AsWeak()));
	}

	// No point forcing something back here because it would break our concept of a threshold of being inactive
	// we just accept that no object is available right now.
	return nullptr;
}


template <typename T, ESPMode Mode> requires BF::OP::CIs_UObject<T>
TBFPooledObjectHandlePtr<T, Mode> TBFObjectPool<T, Mode>::UnpoolObjectByTag(FGameplayTag Tag,
																			bool bAutoActivate,
																			EBFPooledObjectReclaimPolicy Policy)
{
	if (!HasBeenInitialized())
	{
		UE_LOGFMT(LogTemp, Error, "Error: Pool has not been initialized.");
		return nullptr;
	}
	
	if(!Tag.IsValid())
		return nullptr;

	float TimeNow = GetWorld()->GetTimeSeconds();
	
	// Check inactive pool first.
	for(int64 ID : PoolContainer->InactiveObjectIDPool)
	{
		FBFPooledObjectInfo& Info = PoolContainer->ObjectPool.FindChecked(ID);
		if(!Info.PooledObject->template Implements< UBFPooledObjectInterface>())
			continue;
		
		if(IBFPooledObjectInterface::Execute_GetObjectGameplayTag(Info.PooledObject) == Tag)
		{
			++Info.ObjectCheckoutID;
			ActivateObject(CastChecked<T>(Info.PooledObject), bAutoActivate);
			Info.bActive = true;
			Info.LastTimeActive = TimeNow;
			PoolContainer->InactiveObjectIDPool.Remove(ID);

			if (Policy == EBFPooledObjectReclaimPolicy::Reclaimable)
			{
				Info.bIsReclaimable = true;
				ReclaimableUnpooledObjects.Add({ID, Info.ObjectCheckoutID, TimeNow});
			}

			OnObjectPooled.Broadcast(Info.PooledObject, false, Info.ObjectPoolID, Info.ObjectCheckoutID);
			return MakeShared<TBFPooledObjectHandle<T, Mode>, Mode>(&Info, TSharedPtr<TBFObjectPool, Mode>(this->AsShared()));
		}
	}

	// If we aren't using cooldowns then its worth checking to force return otherwise same reasoning as UnpoolObject
	// where if we force the return we break the concept of a cooldown.
	if (PoolInitInfo.CooldownTimeSeconds <= KINDA_SMALL_NUMBER)
	{
		// Check the reclaimable objects in case one of those meet our criteria.
		for(const FReclaimableUnpooledObjectInfo& UnpooledInfo : ReclaimableUnpooledObjects)
		{
			FBFPooledObjectInfo& Info = PoolContainer->ObjectPool.FindChecked(UnpooledInfo.PoolID);
			if(!Info.PooledObject->template Implements< UBFPooledObjectInterface>())
				continue;
			
			if(IBFPooledObjectInterface::Execute_GetObjectGameplayTag(Info.PooledObject) == Tag)
			{
				if(ReturnToPool_Internal(UnpooledInfo.PoolID, UnpooledInfo.CheckoutID, true))
				{
					++Info.ObjectCheckoutID;
					ActivateObject(CastChecked<T>(Info.PooledObject), bAutoActivate);
					Info.bActive = true;
					Info.LastTimeActive = TimeNow;
					PoolContainer->InactiveObjectIDPool.Remove(UnpooledInfo.PoolID);

					if (Policy == EBFPooledObjectReclaimPolicy::Reclaimable)
					{
						Info.bIsReclaimable = true;
						ReclaimableUnpooledObjects.Add({UnpooledInfo.PoolID, Info.ObjectCheckoutID, TimeNow});
					}
					

					OnObjectPooled.Broadcast(Info.PooledObject, false, Info.ObjectPoolID, Info.ObjectCheckoutID);
					return MakeShared<TBFPooledObjectHandle<T, Mode>, Mode>(&Info, TSharedPtr<TBFObjectPool, Mode>(this->AsShared()));
				}
			}
		}
	}

	return nullptr;
}


template <typename T, ESPMode Mode> requires BF::OP::CIs_UObject<T>
TBFPooledObjectHandlePtr<T, Mode> TBFObjectPool<T, Mode>::UnpoolObjectByPredicate(	TFunction<bool(const T* Object)> Pred,
																					bool bAutoActivate,
																					EBFPooledObjectReclaimPolicy Policy )
{
	if (!HasBeenInitialized())
	{
		UE_LOGFMT(LogTemp, Error, "Error: Pool has not been initialized.");
		return nullptr;
	}
	
	if(PoolContainer->InactiveObjectIDPool.Num() == 0 || !Pred)
		return nullptr;

	float TimeNow = GetWorld()->GetTimeSeconds();
	
	for(int64 ID : PoolContainer->InactiveObjectIDPool)
	{
		FBFPooledObjectInfo& Info = PoolContainer->ObjectPool.FindChecked(ID);
		if(Pred(CastChecked<T>(Info.PooledObject)))
		{
			++Info.ObjectCheckoutID;
			ActivateObject(CastChecked<T>(Info.PooledObject), bAutoActivate);
			Info.bActive = true;
			Info.LastTimeActive = TimeNow;
			PoolContainer->InactiveObjectIDPool.Remove(ID);

			if (Policy == EBFPooledObjectReclaimPolicy::Reclaimable)
			{
				Info.bIsReclaimable = true;
				ReclaimableUnpooledObjects.Add({ID, Info.ObjectCheckoutID, TimeNow});
			}

			OnObjectPooled.Broadcast(Info.PooledObject, false, Info.ObjectPoolID, Info.ObjectCheckoutID);
			return MakeShared<TBFPooledObjectHandle<T, Mode>, Mode>(&Info, TSharedPtr<TBFObjectPool, Mode>(this->AsShared()));
		}
	}

	// If we aren't using cooldowns then its worth checking to force return otherwise same reasoning as UnpoolObject
	// where if we force the return we break the concept of a cooldown.
	if (PoolInitInfo.CooldownTimeSeconds <= KINDA_SMALL_NUMBER)
	{
		// Check the reclaimable objects in case one of those meet our criteria.
		for(const FReclaimableUnpooledObjectInfo& UnpooledInfo : ReclaimableUnpooledObjects)
		{
			FBFPooledObjectInfo& Info = PoolContainer->ObjectPool.FindChecked(UnpooledInfo.PoolID);
			if(Pred(CastChecked<T>(Info.PooledObject)))
			{
				if(ReturnToPool_Internal(UnpooledInfo.PoolID, UnpooledInfo.CheckoutID, true))
				{
					++Info.ObjectCheckoutID;
					ActivateObject(CastChecked<T>(Info.PooledObject), bAutoActivate);
					Info.bActive = true;
					Info.LastTimeActive = TimeNow;
					PoolContainer->InactiveObjectIDPool.Remove(UnpooledInfo.PoolID);

					if (Policy == EBFPooledObjectReclaimPolicy::Reclaimable)
					{
						Info.bIsReclaimable = true;
						ReclaimableUnpooledObjects.Add({UnpooledInfo.PoolID, Info.ObjectCheckoutID, TimeNow});
					}

					OnObjectPooled.Broadcast(Info.PooledObject, false, Info.ObjectPoolID, Info.ObjectCheckoutID);
					return MakeShared<TBFPooledObjectHandle<T, Mode>, Mode>(&Info, TSharedPtr<TBFObjectPool, Mode>(this->AsShared()));
				}
			}
		}
	}
	
	return nullptr;
}


template <typename T, ESPMode Mode> requires BF::OP::CIs_UObject<T>
bool TBFObjectPool<T, Mode>::AdoptObject(T* Object, bool bChangeOuter)
{
	// Not initialized, The object was invalid, We're full or the object isn't of the correct type.
	if (!HasBeenInitialized() || !IsValid(Object) || GetPoolNum() >= PoolInitInfo.PoolLimit || !Object->GetClass()->IsChildOf(PoolInitInfo.PoolClass.Get()))
	{
#if !UE_BUILD_SHIPPING
		if (!HasBeenInitialized())
		{
			UE_LOGFMT(LogTemp, Warning, "Error: Pool has not been initialized.");
		}
		else  if (!IsValid(Object))
		{
			UE_LOGFMT(LogTemp, Warning, "Error: Object trying to be adopted into pool is invalid.");
		}
		else if (GetPoolNum() >= PoolInitInfo.PoolLimit)
		{
			UE_LOGFMT(LogTemp, Warning, "Error: Object {0} trying to be adopted into Pool {1} failed because the pool is full.", GetNameSafe(Object), GetNameSafe(PoolInitInfo.PoolClass));
		}
		else if (!Object->GetClass()->IsChildOf(PoolInitInfo.PoolClass.Get()))
		{
			UE_LOGFMT(LogTemp, Warning, "Error: Object {0} trying to be adopted into Pool {1} failed because the object is not of the same class or a child of it.", GetNameSafe(Object), GetNameSafe(PoolInitInfo.PoolClass));
		}
#endif
		return false;
	}

	if (bChangeOuter)
	{
		Object->Rename(nullptr, PoolInitInfo.Owner.Get());
	}

	
	if (AActor* Actor = Cast<AActor>(Object))
	{
		Actor->SetActorHiddenInGame(true);
		Actor->SetActorTickEnabled(false);
		Actor->SetActorEnableCollision(false);
	}
	else if (UUserWidget* Widget = Cast<UUserWidget>(Object))
	{
		Widget->SetVisibility(ESlateVisibility::Collapsed);
	}

	// Add a small initial offset to the LastActiveTime in the case when using a cooldown time, this ensures the first pooling doesn't block instant access.
	float CooldownOffset = PoolInitInfo.CooldownTimeSeconds > 0.f ? PoolInitInfo.CooldownTimeSeconds + KINDA_SMALL_NUMBER : 0.f;
	float TimeNow = GetWorld()->GetTimeSeconds();
	
	FBFPooledObjectInfo Info;
	Info.ObjectPoolID = ++CurrentPoolIDIndex; // -1 Is considered an invalid Index
	Info.CreationTime = TimeNow;
	Info.LastTimeActive = TimeNow - CooldownOffset;
	Info.PooledObject = Object;
	++Info.ObjectCheckoutID; // Don't need to but just feel better starting above -1

	PoolContainer->ObjectPool.Add(Info.ObjectPoolID, Info);
	PoolContainer->InactiveObjectIDPool.Add(Info.ObjectPoolID);

	if(Object->template Implements< UBFPooledObjectInterface>())
		IBFPooledObjectInterface::Execute_OnObjectCreated(Object);

	OnObjectAddedToPool.Broadcast(Info.PooledObject, Info.ObjectPoolID);
	return true;
}


template <typename T, ESPMode Mode> requires BF::OP::CIs_UObject<T>
T* TBFObjectPool<T, Mode>::StealObject(TBFPooledObjectHandlePtr<T, Mode>& Handle)
{
	if (!HasBeenInitialized())
	{
		UE_LOGFMT(LogTemp, Error, "Error: Pool has not been initialized.");
		return nullptr;
	}

	T* Result = StealObject(Handle->GetPoolID(), Handle->GetCheckoutID());
	Handle = nullptr;
	return Result;
}


template <typename T, ESPMode Mode> 
requires BF::OP::CIs_UObject<T>
T* TBFObjectPool<T, Mode>::StealObject(int64 PoolID, int32 ObjectCheckoutID)
{
	if (!HasBeenInitialized())
	{
		UE_LOGFMT(LogTemp, Error, "Error: Pool has not been initialized.");
		return nullptr;
	}
	
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
		{
			PoolContainer->InactiveObjectIDPool.RemoveSingle(PoolID);
		}
		else if (Info->bIsReclaimable) // if it is active, it might also be in our reclaimable list.
		{
			for (int32 Index = 0; Index < ReclaimableUnpooledObjects.Num(); ++Index)
			{
				if (ReclaimableUnpooledObjects[Index].PoolID == PoolID)
				{
					ReclaimableUnpooledObjects.RemoveAtSwap(Index);
					break;
				}
			}
		}

		PoolContainer->ObjectPool.Remove(PoolID);
		OnObjectRemovedFromPool.Broadcast(PoolID, Info->ObjectCheckoutID);
		return CastChecked<T>(Info->PooledObject);
	}
	return nullptr;
}


template<typename T, ESPMode Mode>
requires BF::OP::CIs_UObject<T>
bool TBFObjectPool<T, Mode>::ReturnToPool(TBFPooledObjectHandlePtr<T, Mode>& Handle)
{
	if (!HasBeenInitialized())
	{
		UE_LOGFMT(LogTemp, Error, "Error: Pool has not been initialized.");
		return false;
	}
	
	if(!Handle.IsValid() && !Handle->IsHandleValid())
	{
		Handle = nullptr;
		return false;
	}

	FBFPooledObjectInfo& ObjectInfo = PoolContainer->ObjectPool.FindChecked(Handle->GetPoolID());
	++ObjectInfo.ObjectCheckoutID; 
	DeactivateObject(Handle->GetObject());
	ObjectInfo.bActive = false;
	PoolContainer->InactiveObjectIDPool.Add(ObjectInfo.ObjectPoolID);

	// Check if the object was a reclaimable one, if so remove them from the list.
	int32 Num = ReclaimableUnpooledObjects.Num();
	if (Num > 0 && ObjectInfo.bIsReclaimable)
	{
		for (int32 Index = 0; Index < Num; ++Index)
		{
			if (ReclaimableUnpooledObjects[Index].PoolID == ObjectInfo.ObjectPoolID)
			{
				ObjectInfo.bIsReclaimable = false;
				ReclaimableUnpooledObjects.RemoveAtSwap(Index);
				break;
			}
		}
	}

	// Broadcast with our original checkout ID
	OnObjectPooled.Broadcast(ObjectInfo.PooledObject, true, ObjectInfo.ObjectPoolID, ObjectInfo.ObjectCheckoutID - 1);
	Handle = nullptr;
	return true;
}


template <typename T, ESPMode Mode> requires BF::OP::CIs_UObject<T>
bool TBFObjectPool<T, Mode>::ReturnToPool_Internal(int64 PoolID, int32 ObjectCheckoutID, bool bSkipAddingToInactivePool)
{
	if(FBFPooledObjectInfo* ObjectInfo = PoolContainer->ObjectPool.Find(PoolID))
	{
		if(ObjectInfo->ObjectCheckoutID == ObjectCheckoutID)
		{
			++ObjectInfo->ObjectCheckoutID;
			DeactivateObject(CastChecked<T>(ObjectInfo->PooledObject.Get()));
			ObjectInfo->bActive = false;

			// Used to skip adding to the inactive pool when we are returning to the pool from the reclaimable list,
			// otherwise we would need to then iterate this again to just remove it.
			if (!bSkipAddingToInactivePool)
				PoolContainer->InactiveObjectIDPool.Add(PoolID);

			// Check if the object was a reclaimable one, if so remove them from the list.
			int32 Num = ReclaimableUnpooledObjects.Num();
			if (Num > 0 && ObjectInfo->bIsReclaimable)
			{
				for (int32 Index = 0; Index < Num; ++Index)
				{
					if (ReclaimableUnpooledObjects[Index].PoolID == ObjectInfo->ObjectPoolID)
					{
						ObjectInfo->bIsReclaimable = false;
						ReclaimableUnpooledObjects.RemoveAtSwap(Index);
						break;
					}
				}
			}

			// Broadcast with our original checkout ID
			OnObjectPooled.Broadcast(ObjectInfo->PooledObject, true, PoolID, ObjectCheckoutID);
			return true;
		}
	}
	return false;
}


template <typename T, ESPMode Mode>
requires BF::OP::CIs_UObject<T>
bool TBFObjectPool<T, Mode>::ClearInactiveObjectsPool()
{
	if (!HasBeenInitialized())
    {
    	UE_LOGFMT(LogTemp, Error, "Error: Pool has not been initialized.");
    	return false;
    }
    
	if(PoolContainer->InactiveObjectIDPool.Num() == 0)
		return false;

	TArray<int64>& InactivePool = PoolContainer->InactiveObjectIDPool;
	TMap<int64, FBFPooledObjectInfo>& ObjectPool = PoolContainer->ObjectPool;
	
	for(int64 ID : InactivePool)
	{
		FBFPooledObjectInfo& Info = PoolContainer->ObjectPool.FindChecked(ID);
		
		if(Info.PooledObject->template Implements< UBFPooledObjectInterface>())
			IBFPooledObjectInterface::Execute_OnObjectDestroyed(Info.PooledObject);

		switch (GetPoolType())
		{
		case EBFPoolType::Actor:		CastChecked<AActor>(Info.PooledObject)->Destroy(); break;
		case EBFPoolType::Component:	CastChecked<UActorComponent>(Info.PooledObject)->DestroyComponent(); break;
		case EBFPoolType::UserWidget:	CastChecked<UUserWidget>(Info.PooledObject)->RemoveFromParent(); CastChecked<UUserWidget>(Info.PooledObject)->MarkAsGarbage(); break;
		case EBFPoolType::Object:		Info.PooledObject->MarkAsGarbage(); break;

			// Should never be hit.
		case EBFPoolType::Invalid:
		default:
			bfEnsure(false); return false;
		}
		
		ObjectPool.Remove(ID);
		OnObjectRemovedFromPool.Broadcast(ID, Info.ObjectCheckoutID);
	}
	
	// Since we are clearing them all then we should just empty the array instead of removing each iteration.
	InactivePool.Empty();	
	return true;
}


template <typename T, ESPMode Mode>
requires BF::OP::CIs_UObject<T>
bool TBFObjectPool<T, Mode>::RemoveInactiveNumFromPool(int64 NumToRemove)
{
	if (!HasBeenInitialized())
	{
		UE_LOGFMT(LogTemp, Error, "Error: Pool has not been initialized.");
		return false;
	}
	
	if(NumToRemove > GetInactivePoolNum())	
		return false;

	for(int i = 0; i < NumToRemove; ++i)
	{
		// Popping from inactive so no need to remove below
		FBFPooledObjectInfo& Info = PoolContainer->ObjectPool.FindChecked(PoolContainer->InactiveObjectIDPool.Pop());
		
		if(Info.PooledObject->template Implements<UBFPooledObjectInterface>())
			IBFPooledObjectInterface::Execute_OnObjectDestroyed(Info.PooledObject);

		
		switch(GetPoolType())
		{
			case EBFPoolType::Actor:		CastChecked<AActor>(Info.PooledObject)->Destroy(); break;
			case EBFPoolType::Component:	CastChecked<UActorComponent>(Info.PooledObject)->DestroyComponent(); break;
			case EBFPoolType::UserWidget:	CastChecked<UUserWidget>(Info.PooledObject)->RemoveFromParent(); CastChecked<UUserWidget>(Info.PooledObject)->MarkAsGarbage(); break;
			case EBFPoolType::Object:		Info.PooledObject->MarkAsGarbage(); break;
			
			// Should never be hit.
			case EBFPoolType::Invalid:
			default:
				bfEnsure(false); return false;
		}

		int64 ID = Info.ObjectPoolID;
		int16 CheckoutID = Info.ObjectCheckoutID;
		PoolContainer->ObjectPool.Remove(ID);
		OnObjectRemovedFromPool.Broadcast(ID, CheckoutID);
	}
	return true;
}


template <typename T, ESPMode Mode> requires BF::OP::CIs_UObject<T>
bool TBFObjectPool<T, Mode>::IsObjectIDValid(int64 PoolID, int32 ObjectCheckoutID) const
{
	if (!HasBeenInitialized())
	{
		UE_LOGFMT(LogTemp, Error, "Error: Pool has not been initialized.");
		return false;
	}
	
	if(FBFPooledObjectInfo* Info = PoolContainer->ObjectPool.Find(PoolID))
		return Info->ObjectCheckoutID == ObjectCheckoutID;
	return false;
}


template <typename T, ESPMode Mode> requires BF::OP::CIs_UObject<T>
bool TBFObjectPool<T, Mode>::IsObjectInactive(int64 PoolID, int32 ObjectCheckoutID) const
{
	if (!HasBeenInitialized())
	{
		UE_LOGFMT(LogTemp, Error, "Error: Pool has not been initialized.");
		return false;
	}
	
	if(FBFPooledObjectInfo* Info = PoolContainer->ObjectPool.Find(PoolID))
		return Info->ObjectCheckoutID == ObjectCheckoutID && !Info->bActive;
	return false;
}



template <typename T, ESPMode Mode> requires BF::OP::CIs_UObject<T>
bool TBFObjectPool<T, Mode>::EvaluatePoolOccupancy()
{
	if (!HasBeenInitialized())
		return false;
	
	TArray<int64>& InactiveObjectIDs = PoolContainer->InactiveObjectIDPool;
	TMap<int64, FBFPooledObjectInfo>& ObjectPool = PoolContainer->ObjectPool;
	
	float SecondsNow = GetWorld()->GetTimeSeconds();
	int NumRemoved = 0;
	for(auto IT = InactiveObjectIDs.CreateIterator(); IT; ++IT)
	{
		int64 ID = *IT;
		FBFPooledObjectInfo& Info = PoolContainer->ObjectPool.FindChecked(ID);
		
		if(SecondsNow - Info.LastTimeActive >= GetMaxObjectInactiveOccupancySeconds())
		{
			if(Info.PooledObject->template Implements< UBFPooledObjectInterface>())
				IBFPooledObjectInterface::Execute_OnObjectDestroyed(Info.PooledObject);

			switch (GetPoolType())
			{
			case EBFPoolType::Actor:		CastChecked<AActor>(Info.PooledObject)->Destroy(); break;
			case EBFPoolType::Component:	CastChecked<UActorComponent>(Info.PooledObject)->DestroyComponent(); break;
			case EBFPoolType::UserWidget:	CastChecked<UUserWidget>(Info.PooledObject)->RemoveFromParent(); CastChecked<UUserWidget>(Info.PooledObject)->MarkAsGarbage(); break;
			case EBFPoolType::Object:		Info.PooledObject->MarkAsGarbage(); break;

				// Should never be hit.
			case EBFPoolType::Invalid:
			default:
				bfEnsure(false); return false;
			}

			int16 CheckoutID = Info.ObjectCheckoutID;
			ObjectPool.Remove(ID);
			IT.RemoveCurrentSwap();
			
			OnObjectRemovedFromPool.Broadcast(ID, CheckoutID);
			++NumRemoved;
		}
	}

#if !UE_BUILD_SHIPPING
	if(BF::OP::CVarObjectPoolEnableLogging.GetValueOnAnyThread())
		UE_LOGFMT(LogTemp, Warning,
			"Removed {0} objects from the pool due to exceeding the MaxObjectInactiveOccupancySeconds", NumRemoved);
#endif
	return NumRemoved > 0;
}



template <typename T, ESPMode Mode>
requires BF::OP::CIs_UObject<T>
bool TBFObjectPool<T, Mode>::SetPoolLimit(int32 PoolLimit)
{
	if (!HasBeenInitialized())
		return false;
	
	if(PoolLimit == PoolInitInfo.PoolLimit)
		return true;

	if(PoolLimit > PoolInitInfo.PoolLimit)
	{
		PoolInitInfo.PoolLimit = PoolLimit;
		return true;
	}

	int32 CurrentPoolSize = GetPoolNum();
	int32 InactivePoolSize = GetInactivePoolNum();

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
	if (!HasBeenInitialized())
	{
		UE_LOGFMT(LogTemp, Error, "Error: Pool has not been initialized.");
		return;
	}
	
	
	// Ensure that we are ticking if we have a value that's useful.
	if(MaxObjectInactiveOccupancySeconds > 0.f)
	{
		PoolContainer->SetTickEnabled(true);
	}
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
	if (!HasBeenInitialized())
	{
		UE_LOGFMT(LogTemp, Error, "Error: Pool has not been initialized.");
		return;
	}
	
	PoolContainer->SetTickInterval(InTickInterval);
	PoolInitInfo.PoolTickInfo.TickInterval = InTickInterval;
}


template <typename T, ESPMode Mode> 
requires BF::OP::CIs_UObject<T>
void TBFObjectPool<T, Mode>::SetTickEnabled(bool bEnable)
{
	if (!HasBeenInitialized())
	{
		UE_LOGFMT(LogTemp, Error, "Error: Pool has not been initialized.");
		return;
	}

	PoolContainer->SetTickEnabled(bEnable);
}


template<typename T, ESPMode Mode>
requires BF::OP::CIs_UObject<T>
void TBFObjectPool<T, Mode>::ActivateObject(T* Obj, bool bAutoActivate)
{
#if !UE_BUILD_SHIPPING
	bfValid(Obj);
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
				AActor* Actor = reinterpret_cast<AActor*>(Obj);
				Actor->SetActorTickEnabled(true);
				Actor->SetActorHiddenInGame(false);
				Actor->SetActorEnableCollision(true);
				break;
			}
			case EBFPoolType::Component:
			{
				UActorComponent* Component = reinterpret_cast<UActorComponent*>(Obj);
				Component->Activate(true);
				break;
			}
			case EBFPoolType::UserWidget:
			{
				UUserWidget* Widget = reinterpret_cast<UUserWidget*>(Obj);
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
void TBFObjectPool<T, Mode>::DeactivateObject(T* Obj)
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
				AActor* Actor = reinterpret_cast<AActor*>(Obj);
				Actor->SetActorTickEnabled(false);
				Actor->SetActorHiddenInGame(true);
				Actor->SetActorEnableCollision(false);
				break;
			}
			case EBFPoolType::Component:
			{
				UActorComponent* Component = reinterpret_cast<UActorComponent*>(Obj);
				Component->Deactivate();
				break;
			}
			case EBFPoolType::UserWidget:
			{
				UUserWidget* Widget = reinterpret_cast<UUserWidget*>(Obj);
				Widget->SetVisibility(ESlateVisibility::Collapsed);
				Widget->SetIsEnabled(false);
				Widget->RemoveFromParent();
				break;
			}
			case EBFPoolType::Object: break; // Not sure what behaviour a uobject needs, ill leave it up to the user with the interface call below.
			case EBFPoolType::Invalid: bfEnsure(false); break;
		}
	}

	if(Obj->template Implements<UBFPooledObjectInterface>())
		IBFPooledObjectInterface::Execute_OnObjectPooled(Obj);
}


template <typename T, ESPMode Mode> requires BF::OP::CIs_UObject<T>
int64 TBFObjectPool<T, Mode>::TryReclaimUnpooledObject()
{
	SCOPED_NAMED_EVENT(TBFObjectPool_TryReclaimUnpooledObject, FColor::Green);

	int64 ReturnedObjectID = -1;
	if (!HasBeenInitialized())
		return ReturnedObjectID;

	int32 Num = ReclaimableUnpooledObjects.Num();
	switch (PoolInitInfo.ForceReturnReclaimStrategy)
	{
	case EBFPooledObjectReclaimStrategy::Oldest:
		{
			float OldestTime = std::numeric_limits<float>::max();
			int32 OldestIndex = 0;
			int32 I = 0;
			
			const FReclaimableUnpooledObjectInfo* RESTRICT Start = ReclaimableUnpooledObjects.GetData();
			for (const FReclaimableUnpooledObjectInfo* RESTRICT Data = Start, *RESTRICT DataEnd = Data + Num; Data != DataEnd; ++Data)
			{
				float ItemTime = Data->TimeUnpooled;
				if (ItemTime < OldestTime)
				{
					OldestIndex = I;
					OldestTime = ItemTime;
				}
				++I;
			}
				
			FReclaimableUnpooledObjectInfo& UnpooledInfo = ReclaimableUnpooledObjects[OldestIndex];
			int64 ID = UnpooledInfo.PoolID;
			if (ReturnToPool_Internal(ID, UnpooledInfo.CheckoutID, true))
			{
				ReturnedObjectID = ID;
			}
			break;
		}
	case EBFPooledObjectReclaimStrategy::FirstFound:
		{
			FReclaimableUnpooledObjectInfo& UnpooledInfo = ReclaimableUnpooledObjects[0];
			int64 ID = UnpooledInfo.PoolID;
			if(ReturnToPool_Internal(ID, UnpooledInfo.CheckoutID, true))
			{
				ReturnedObjectID = ID;
			}
			break;
		}
	case EBFPooledObjectReclaimStrategy::LastFound:
		{
			FReclaimableUnpooledObjectInfo& UnpooledInfo = ReclaimableUnpooledObjects[Num - 1];
			int64 ID = UnpooledInfo.PoolID;
			if (ReturnToPool_Internal(ID, UnpooledInfo.CheckoutID, true))
			{
				ReturnedObjectID = ID;
			}
			break;
		}
	case EBFPooledObjectReclaimStrategy::Random:
		{
			int32 I = FMath::RandRange(0, Num - 1);
			FReclaimableUnpooledObjectInfo& UnpooledInfo = ReclaimableUnpooledObjects[I];
			int64 ID = UnpooledInfo.PoolID;
			if (ReturnToPool_Internal(ID, UnpooledInfo.CheckoutID, true))
			{
				ReturnedObjectID = ID;
			}
			break;
		}
	}
	return ReturnedObjectID;
}


























