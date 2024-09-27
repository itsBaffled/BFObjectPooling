#pragma once

#include "Logging/StructuredLog.h"
#include "BFObjectPoolHelpers.generated.h"


UENUM(BlueprintType)
enum class EBFSuccess : uint8
{
	Success = true,
	Failure = false,
};



#if !UE_BUILD_SHIPPING


#define bfValid(Obj) ensure(::IsValid(Obj))
#define bfEnsure(Expr) ensure(Expr)


#else

#define bfValid(Obj)
#define bfEnsure(Expr)

#endif



namespace BF::OP
{
	template<typename T> concept CIs_UObject = std::is_base_of_v<UObject, std::remove_pointer_t<T>>;

	// Used for BP graph, converts a bool to an EBFSuccess enum, which is used for success/failure return pins.
	inline EBFSuccess ToBPSuccessEnum(const bool Value)
	{
		return static_cast<EBFSuccess>(Value);
	}
	
	template<typename T>
	T* NewComponent(AActor* Owner, UClass* Class, FName ComponentName, EObjectFlags ObjectFlags = RF_Transient)
	{
		if(!Class)
			Class = T::StaticClass();

		T* Component = NewObject<T>(Owner, Class, ComponentName, ObjectFlags);
		bfValid(Component);
		
		Component->RegisterComponent();
		Owner->AddInstanceComponent(Component);

		return Component;
	}
}


















