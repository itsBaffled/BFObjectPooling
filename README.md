

# BF Object Pooling
BF Object Pooling aims to be a simple to use yet powerful generic object pooling solution.


<img src="Resources/BFObjectPoolingCoverImage.png" alt="BF Object Pooling Cover Image" width="1500"/>

- Side Note: Some screenshot examples contain renamed functions but all functionality is the same.


---

### Overview
- Supports C++ first class with templates in mind and full operability with blueprint

- Various Customization options so each pool can be totally different from the last
	- Custom override delegates for activation/deactivation at the pool level (Allows you to implement your own special functionality for that behaviour and bypass the pools)
	- Custom activation/deactivation interface support at the pooled object level (Allows for any object logic to be performed at those stages, see `UBFPooledObjectInterface`)
	- Callbacks for object add/removal as well as when objects are leased and returned to the pool, lots of hooks.
	- Runtime changing of pool type (Assuming its of same template class or a child type, see how the BP handle does it by just using UObject).

- Supports 4 class types (and child derived types of course) which practically covers almost everything needed:
	- `AActor`
	- `UActorComponent`
	- `UUserWidget`
	- `UObject`

- Supports UObject ownership of the pool so no more reliance on Actors which is great for subsystems (Unless the pool is UserWidget type, you must set the owner as a player controller.)

- Comes with **7** built in generic classes that are ready for use with lots of easy examples for implementing your own U/A unreal classes
	- Generic Projectile Actor
		- Supports Static Mesh, Niagara VFX system and Different collision shape types (Sphere, Box, Capsule) with dynamic runtime changing of the mentioned
	- Generic Decal Actor
	- Generic Sound Actor
	- Generic Skeletal Mesh Actor
	- Generic Static Mesh Actor
	- Generic 3D Widget Actor
	- Generic Niagara Actor


---


### The Key Main Components Of The Object Pool Itself Can Be Broken Into 3 Parts
 
  1)- The Templated Struct Pool itself: `TBFObjectPool`
- The pool itself is responsible for maintaining the pooled objects while they are in an inactive state as well as providing an API for being interacted with.
- The pool should be stored only as a shared pointer to the pool itself, this means either `TSharedPtr<TBFObjectPool<T, Mode>, Mode>` (which is verbose) or use the using alias `TFBObjectPoolPtr<T, Mode>` 
- The only time you will need to referer to the pool as `TBFObjectPool` instead of `TBFObjectPoolPtr` is when first setting your pointer and creating the pool like so `MyPool = TBFObjectPool<MyType>::CreatePool()`

 <br>
 		
2)- The Pool Container: `UBFPoolContainer`
- UObject that is used to wrap pooled objects storage in a GC friendly way, handle ticking the pool and stores other meta data about the pooled objects.
- Should never be interacted with directly.

<br>
 
3)- The Pooled Object Handle: `TBFPooledObjectHandle` which is primary interacted with via shared pointer alias `TBFPooledObjectHandlePtr` for the same convenience reasons.
- When attempting to un-pool an object it is returned to you via shared pointer to the handle (`TBFPooledObjectHandle`), if unsuccessful like due to being at pool capacity the handle is a nullptr.
- If you let all shared pointers to the handle go out of scope, the pooled object will be returned to the pool automatically (assuming its able to and the object hasn't already been returned to the pool).
- BP side wraps shared pointers via a value struct that copied around and stored, it has easy to use function library API for interacting with the pooled object.
- Due to sharing and copying of handle pointers you should always check for `IsHandleValid()` before using the handle if you are within a new scope where you can't be certain. This is
because it may have already returned itself to the pool or it may have been "Stolen" via `StealObject()`. The `IsHandleValid()` function not only checks the objects validity but also compares our handle ID to see if
it matches the pools latest ID, if it doesn't then the handle is considered invalid. 
You should never cache the object directly but only ever access it for short scopes if you need to init or do any setup behaviour.

<br>

---

Tips:
- For custom pooled actors (that arent my default provided ones) I recommend mimicking how I have functionality to hand the unpooled actor back its handle so it can return itself to the pool when it's done. See ABFPoolableSoundActor::FireAndForget for an example.
- Use the core header `BFObjectPooling/BFObjectPoolingCore.h` which handles keeping everything conveniently in one place.
- If you have linking issues building the plugin then please ensure whatever module is using it, also has the dependecies it has like UMG for example (See BFObjectPooling.Build.cs).

C++ Code examples


```cpp
 Usage goes as follows:

 // Header file or wherever you want to store the pool.
 // ESP mode is optional and defaults to NotThreadSafe for perf benefits since most actors/objects are game thread bound.
 TBFObjectPoolPtr<AMyFoo, ESPMode::ThreadSafe> MyPool; 



 // Note that if you set the ESP mode you also need to reflect that when creating the pool, so if pool ptr was `TBFObjectPoolPtr<AMyFoo, ESPMode::ThreadSafe> MyPool;` the pool creation would be `MyPool = TBFObjectPool<AMyFoo, ESPMode::ThreadSafe>::CreatePool()`
 // BeginPlay or some other init function where you want to create and init the pool.
 MyPool = TBFObjectPoolPtr<AMyFoo, ESPMode::ThreadSafe>::CreatePool(); // Use static factory Create method to create the pool and make sure to Init it before ANYTHING else.

 // The only NEEDED params to be filled out are these,
 // - the owner can be UObject or Actor **unless** the pool is of UserWidget type, then you MUST lock in APlayerController or child classes to be the pool owner.
 // - the pool type dictates how we create, activate and deactivate the pooled objects.
 // - the pool class **if its a BP pool**, otherwise the class is inferred by the template type.
 FBFObjectPoolInitParams Params;
 Params.Owner = this;
 Params.PoolType = EBFPoolType::Actor;

 // Fill out any other info like pool limit or pool size
 Params.FillOutDefaults = whatever;

 // Initialize the pool and done, its ready to use!
 MyPool->InitPool(Params);




 // IN GAMEPLAY now you can just access pooled objects like so, this is literally all thats needed after initialization to use your pool.

// Returns a pooled object via a shared handle ptr, if the param is false you must handle activating the object yourself, there is built in activation/deactivation logic already though.
if(TBFPooledObjectHandlePtr<AMyFoo> Handle = ObjectPool->UnpoolObject(bAutoActivate))
{
	// Do whatever you want with the object, my preferred method is to init the object and hand it back its own handle so it can do what it needs to do and return when needed. 
	// Like a sound would set the params and then return on sound finished
	auto* Obj = Handle->GetObject(); // This is templated in c++ and in BP you get the object as a UObject* for you to then cast. Do not store Obj but just use it in scope.
	Obj->SomeObjectFunctionToSetThingsUp()
	Obj->SetPoolHandle(Handle); // This is a function I typically have on my pooled objects that takes the handle and stores it for later use, also invalidates your handle so you can't use it again.
}






// Some other functionality includes:

 MyPool->UnpoolObject(bAutoActivate); // Attempts to un-pool an object and return it via a shared handle ptr, the result will return a null pointer if unable to un-pool an object due to pool being at capacity and all objects in use.
 MyPool->UnpoolObjectByTag(Tag, bAutoActivate); // Only super useful if you have a pool of specific objects you want to re access, for example you can have a UUserWidget pool and each widget be different and when wanting a specific widget
													 // you can query the pool for that tag, returns false if unable to locate within the inactive pool of objects.

 
 MyPool->ReturnToPool(Handle); // Attempts to return the handle to the pool, can fail if the handle is stale but failing is perfectly valid and expected, especially if multiple handle copies exist.
 Handle->ReturnToPool(); // Returns the object to the pool via the handle, (Typically for fire and forget pooled objects otherwise you would be holding onto the handle yourself).

 
 MyPool->ClearInactiveObjectsPool(); // Clears the pool of all inactive objects. We do not clear in use ones.
 MyPool->RemoveInactiveObjectFromPool(PoolID, ObjectCheckoutID); // Removes a specific object from the pool ONLY if it is inactive and matches our ID. Will not remove active objects, use `StealObject)` for that.
 MyPool->RemoveInactiveNumFromPool(NumToRemove); // Removes a specific number of inactive objects from the pool, if unable to remove the exact amount the returns false.



 // Usage of the pools handles are as follows:
 // Handles are created and returned as shared pointers, once all pointers to a particular handle are destroyed the object is returned to the pool automatically if it hasn't already,
 // you may also manually call `ReturnToPool()` or `StealObject()` to invalidate existing copied shared pointers to the same handle, be sure to use `Handle->IsHandleValid()` to check.

 TBFPooledObjectHandlePtr<AMyFoo> Handle = MyPool->UnpoolObject(); // Again.. Should be stored in a larger scope an if your pool was a different ESPMode you would have to reflect that here after AMFoo.
 Handle.IsValid(); // Checks the SharedPtr if the Handle object is valid, not related to the Handle objects validity. Ideally this shouldn't be needed after you know the returned handle was valid since the nature of a shared pointer kinda keeps it alive for you.
 Handle->IsHandleValid(); // This checks if the handle is valid, this is what you'll want most likely, you could also technically do 'Handle.Get()->IsHandleValid()' but that's a bit verbose.
 TBFPooledObjectHandlePtr<AMyFoo> ACopyOfMyHandlePtr = Handle; // Creates a reference counted copy of the handle, if you return the object to the pool via one of the handles, all other handles to the same object will be invalidated.
 Handle->GetObject(); // This returns the object that the handle is holding, this is what you'll want to use to get the object but do not store it.
 Handle->ReturnToPool(); // This returns the object to the pool and invalidates the handle, if there are multiple handles to the same object they will all be invalidated.
 Handle->StealObject(); // This simultaneously removes the pooled object from its owning pool, returns it and invalidates all handles to that object.
```

<br>

Please make sure to report any issues and I'll try my best to get back to you.
- [Twitter](https://twitter.com/itsBaffled)























