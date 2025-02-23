// Copyright (c) 2024 Jack Holland 
// Licensed under the MIT License. See LICENSE.md file in repo root for full license information.

#pragma once
#include "BFObjectPooling/Pool/Private/BFPoolContainer.h"
#include "BFObjectPooling/Pool/Private/BFObjectPoolHelpers.h"


template<typename T, ESPMode Mode>
struct TBFPooledObjectHandle;


/** TSharedPtr using statement for a pooled object handle when being taken from the pool, if the returned handle
 * is not stored then when out of scoped the handle returns the object to the pool.
 * Typical way to check validity of a handle is to go `if(Handle.IsValid() && Handle->IsHandleValid())` before using the object.
 * The first checks the shared pointer is not null and the second checks if the handle itself has gone stale since the object
 * may have already been returned to the pool from another handle.
 */
template<typename T, ESPMode Mode = ESPMode::NotThreadSafe>
using TBFPooledObjectHandlePtr = TSharedPtr<TBFPooledObjectHandle<T, Mode>, Mode>;


template<typename T, ESPMode Mode> requires BF::OP::CIs_UObject<T>
struct TBFObjectPool;



/** TBFPooledObjectHandle:
 * When using objects from the pool, they are leased in the form of a shared pointer to this handle struct.
 * For convenience there are "using" statements above that I would recommend using (**TBFPooledObjectHandlePtr**).
 *
 * When taking a pooled object it is your responsibility to return it to the pool when you are finished this can be achieved by either nulling all handles or by specifically returning it to the pool,
 * This can be done with the `ReturnToPool()` method on the handle or by calling the pools `ReturnToPool(MyHandle)` method with the handle.
 *
 * Objects are able to be kept via the `StealObject()` method, this will return the object and invalidate your handle, any copies of that handles shared pointer will also be invalidated.
 *
 * For the most convenience with these handles, I suggest you should have some kind of functionality where you hand the newly unpooled object its own handle
 * and it will return itself to the pool when it is done. See my GameplayActors folder for examples of how I use the FireAndForget functions.
 */
template<typename T, ESPMode Mode>
struct TBFPooledObjectHandle : public TSharedFromThis<TBFPooledObjectHandle<T, Mode>, Mode>
{
	friend struct FBFPooledObjectHandleBP;
	// No copying/moving handles, only copying of the shared pointer.
	TBFPooledObjectHandle& operator=(const TBFPooledObjectHandle& Rhs) = delete; 
	TBFPooledObjectHandle(const TBFPooledObjectHandle& Rhs) = delete;
	TBFPooledObjectHandle& operator=(TBFPooledObjectHandle&& Rhs) noexcept = delete;
	TBFPooledObjectHandle(TBFPooledObjectHandle&& Rhs) noexcept = delete;
	TBFPooledObjectHandle() = delete;

	TBFPooledObjectHandle(const FBFPooledObjectInfo* PooledComponentInfo, TWeakPtr<TBFObjectPool<T,Mode>, Mode> OwningPool);
	virtual ~TBFPooledObjectHandle();
	
	bool operator==(const TBFPooledObjectHandle& Rhs);
	T* operator->() {return GetObject();}
	const T* operator->() const {return GetObject();}

	// Queries not only if the handle pointer is valid but also if we are stale or not (meaning we are a copy of a handle to an object that has already been returned to the pool).
	bool IsHandleValid() const;
	
	// Returns the pooled object if valid.
	T* GetObject(bool bEvenIfInvalid = false) const;

	// Attempts to return the object to the pool via this handle and gets invalidated, can fail if object has already been returned or stolen from pool.
	bool ReturnToPool();

	/* When a pooled object is being used, you have the ability to keep it and steal it from the pool,
	 * this will invalidate any handles and return the object ready to be managed by you, depending on the pool type
	 * the returned object may have an outer that is not desired, consider "Renaming" your returned object to change it's outer. */
	T* StealObject();

	// An ID of -1 means invalid, otherwise the ID represents our key into the owning pools map. Use IsHandleValid() to check if the handle is valid this is just the stored ID when first taken from the pool.
	int64 GetPoolID() const {return ObjectPoolID;}
	
	/* An ID of -1 means invalid, otherwise the ID represents unique un-pooled ID(Helps with re using objects and stale handles).
	* Use IsHandleValid() to check if the handle is valid. */
	int32 GetCheckoutID() const {return ObjectCheckoutID;}

protected:
	friend struct TBFObjectPool<T, Mode>;
	virtual void Invalidate();

protected:
	TWeakObjectPtr<T> PooledObject = nullptr;
	TWeakPtr<TBFObjectPool<T, Mode>, Mode> OwningPool = nullptr;
	int64 ObjectPoolID = -1;
	int32 ObjectCheckoutID = -1;
	bool bInvalidated = false;
};




template <typename T, ESPMode Mode>
TBFPooledObjectHandle<T, Mode>::TBFPooledObjectHandle(	const FBFPooledObjectInfo* PooledComponentInfo,
														TWeakPtr<TBFObjectPool<T, Mode>, Mode> OwningPool)
{
	this->OwningPool = OwningPool;
	this->PooledObject = CastChecked<T>(PooledComponentInfo->PooledObject);
	this->ObjectPoolID = PooledComponentInfo->ObjectPoolID;
	this->ObjectCheckoutID = PooledComponentInfo->ObjectCheckoutID;
}



template <typename T, ESPMode Mode>
T* TBFPooledObjectHandle<T, Mode>::StealObject()
{
	if(IsHandleValid())
	{
		T* Obj = OwningPool.Pin()->StealObject(ObjectPoolID, ObjectCheckoutID);
		Invalidate();
		return Obj;
	}
	return nullptr;
}


template <typename T, ESPMode Mode>
T* TBFPooledObjectHandle<T, Mode>::GetObject(bool bEvenIfInvalid) const
{
	if(IsHandleValid() || bEvenIfInvalid)
		return CastChecked<T>(PooledObject.Get(bEvenIfInvalid), ECastCheckedType::NullAllowed);
	return nullptr;
}


template <typename T, ESPMode Mode>
bool TBFPooledObjectHandle<T, Mode>::ReturnToPool()
{
	bool Result = false;
	if(IsHandleValid())
	{
		auto P = OwningPool.Pin();
		Invalidate();
		Result = P->ReturnToPool_Internal(GetPoolID(), GetCheckoutID());
	}
	return Result;
}


template <typename T, ESPMode Mode>
bool TBFPooledObjectHandle<T, Mode>::IsHandleValid() const
{
	return	!bInvalidated
			&& ObjectPoolID > -1
			&& ObjectCheckoutID > -1 &&
			PooledObject.IsValid()
			&& OwningPool.IsValid()
			&& OwningPool.Pin()->IsObjectIDValid(ObjectPoolID, ObjectCheckoutID);
}


template <typename T, ESPMode Mode>
void TBFPooledObjectHandle<T, Mode>::Invalidate()
{
	OwningPool = nullptr;
	bInvalidated = true;

	// Being kept around for Get(bEvenIfInvalid) to work.
	// PooledObject = nullptr;

	// I used to reset these but there is no harm in the handle being able to access it's old unpooled ID and checkout ID,
	// I personally had a use where I had a separate system that responded to things being pooled and they needed to know the old ID and checkout ID.
	//ObjectPoolID = -1;
	//ObjectCheckoutID = -1;
}


template <typename T, ESPMode Mode>
bool TBFPooledObjectHandle<T, Mode>::operator==(const TBFPooledObjectHandle& Rhs)
{
	return	ObjectPoolID == Rhs.ObjectPoolID
			&& ObjectCheckoutID == Rhs.ObjectCheckoutID;
}


template <typename T, ESPMode Mode>
TBFPooledObjectHandle<T, Mode>::~TBFPooledObjectHandle()
{
	if(IsHandleValid())
		ReturnToPool();
}




