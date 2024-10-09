// Copyright (c) 2024 Jack Holland 
// Licensed under the MIT License. See LICENSE.md file in repo root for full license information.

#include "BFPoolContainer.h"
#include "BFObjectPooling/Pool/Private/BFObjectPoolHelpers.h"
  

void FBFPoolContainerTickFunction::ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionEventGraph)
{
	if (Tickable && IsValid(Tickable) && TickType != LEVELTICK_ViewportsOnly)
		Tickable->Tick(DeltaTime);
}


UBFPoolContainer::UBFPoolContainer()
{
	PrimaryContainerTick.Tickable = this;
	PrimaryContainerTick.bCanEverTick = true;
	PrimaryContainerTick.bTickEvenWhenPaused = false;
	PrimaryContainerTick.TickGroup = ETickingGroup::TG_DuringPhysics;
	PrimaryContainerTick.TickInterval = TickInterval;
}


void UBFPoolContainer::Tick(float Dt)
{
	if(OwningPoolTickFunc)
		OwningPoolTickFunc(OwningWorld.Get(), Dt);
}


void UBFPoolContainer::Init(TFunction<void(UWorld*, float)>&& TickFunc, UWorld* World, float InTickInterval)
{
	bfValid(World);
	OwningWorld = World;
	PrimaryContainerTick.RegisterTickFunction(World->PersistentLevel);
	TickInterval = InTickInterval;
	OwningPoolTickFunc = std::move(TickFunc);
}


void UBFPoolContainer::SetTickEnabled(bool bEnable)
{
	PrimaryContainerTick.SetTickFunctionEnable(bEnable);
}

void UBFPoolContainer::SetTickInterval(float InTickInterval)
{
	TickInterval = InTickInterval;
	PrimaryContainerTick.TickInterval = InTickInterval;
}


UClass* UBFPoolContainer::TryGetPoolType() const
{
	if(ObjectPool.Num() > 0)
		return  ObjectPool[0].PooledObject->GetClass();
	
	return nullptr;
}
