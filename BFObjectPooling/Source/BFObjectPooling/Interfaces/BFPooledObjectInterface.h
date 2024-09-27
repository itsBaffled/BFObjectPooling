// Copyright (c) 2024 Jack Holland 
// Licensed under the MIT License. See LICENSE.md file in repo root for full license information.

#pragma once
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "BFPooledObjectInterface.generated.h"


UINTERFACE()
class BFOBJECTPOOLING_API UBFPooledObjectInterface : public UInterface
{
	GENERATED_BODY()
};


// If you cannot implement this interface for whatever reason, such as if the pool is of an unreal base class then your next best option is the generic delegate events on the Pool itself.
class BFOBJECTPOOLING_API IBFPooledObjectInterface
{
	GENERATED_BODY()

protected:
	// Called upon creation of the object, should not be used for any kind of activation logic, only for setting up the object, see OnObjectUnpooled for that.
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "BF|Pooling")
	void OnObjectCreated();
	
	// Called just before removing the object from the world and pool (NOT called when stealing an object but evicting inactive).
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "BF|Pooling")
	void OnObjectDestroyed();
	
	// Called upon an object being returned to the pooled. Will not be called the first initial time being pooled after creation, see OnObjectCreated for that.
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "BF|Pooling")
	void OnObjectPooled();
	
	// Called just before returning the object back out from the pool.
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "BF|Pooling")
	void OnObjectUnPooled();

	/* Completely optional but REQUIRED if you want to have a gameplay tag associated with the object for easy identification such as for TryGetPooledObjectByTag, useful when you have
	 * a pool of specific objects (maybe a bunch a widgets where each objects data matters unlike other pooled actors) and you want to quickly grab the object via its tag. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "BF|Pooling")
	FGameplayTag GetObjectGameplayTag();
};













