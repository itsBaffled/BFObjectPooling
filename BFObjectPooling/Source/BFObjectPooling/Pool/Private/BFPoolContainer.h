// Copyright (c) 2024 Jack Holland 
// Licensed under the MIT License. See LICENSE.md file in repo root for full license information.

#pragma once
#include "BFPoolContainer.generated.h"




// Stores information about a pooled object, for internal book-keeping.
USTRUCT(meta = (Hidden))
struct BFOBJECTPOOLING_API FBFPooledObjectInfo
{
	GENERATED_BODY()
public:
	UPROPERTY(Transient)
	TObjectPtr<UObject> PooledObject;
	
	int64 ObjectPoolID = 0; // Statically assigned ID for each new object.
	float CreationTime = 0.0f; // Time the object was created in game world seconds.
	float LastTimeActive = 0.0f; // Last time this object was used in game world seconds, for culling inactive pool objects if that behaviour is enabled.
	int16 ObjectCheckoutID = -1; // Every time an object is used from the pool and returned we increment this ID to ensure that other stale handles dont think they are the same just because their pool ID is the same.
	uint8 bActive:1 = false; // Flag to determine if this object is currently in use or not.
	uint8 bIsReclaimable:1 = false; // Flag to determine if its worth trying to locate it in the reclaimable object list.
};


USTRUCT()
struct FBFPoolContainerTickFunction : public FTickFunction
{
	GENERATED_BODY()
public:
	TObjectPtr<class UBFPoolContainer> Tickable;
	virtual void ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionEventGraph) override;
	virtual FString DiagnosticMessage() override {return TEXT("BFObjectPoolingTickFunction");}
	virtual FName DiagnosticContext(bool bDetailed) override {return FName(TEXT("BFObjectPoolingTickFunction"));}
};

template<>
struct TStructOpsTypeTraits<FBFPoolContainerTickFunction> : public TStructOpsTypeTraitsBase2<FBFPoolContainerTickFunction>
{
	enum
	{
		WithCopy = false
	};
};



// Internal use only, it was this or I add the pooled object to the RootSet or use TStrongObjectPtr. One extra object per pool is not a big deal at all.
UCLASS(meta = (Hidden))
class BFOBJECTPOOLING_API UBFPoolContainer : public UObject
{
	GENERATED_BODY()
public:
	UBFPoolContainer();
	virtual void Tick(float Dt);
	void Init(TFunction<void(UWorld*, float)>&& TickFunc, UWorld* World, float TickInterval);
	void SetTickGroup(ETickingGroup InTickGroup) {PrimaryContainerTick.TickGroup = InTickGroup;}
	void SetTickEnabled(bool bEnable);
	void SetTickInterval(float InTickInterval);
	bool GetTickEnabled() const {return PrimaryContainerTick.IsTickFunctionEnabled();}
	UClass* TryGetPoolType() const;

public:
	// Reflected container that stores each allocated object some info about them.
	UPROPERTY()
	TMap<int64, FBFPooledObjectInfo> ObjectPool;

	// Storing an array of inactive pool object IDs for quick querying.
	TArray<int64> InactiveObjectIDPool;
	
protected:
	float TickInterval = 1.f;
	TWeakObjectPtr<UWorld> OwningWorld;
	TFunction<void(UWorld*, float)> OwningPoolTickFunc;
	FBFPoolContainerTickFunction PrimaryContainerTick;
};



























