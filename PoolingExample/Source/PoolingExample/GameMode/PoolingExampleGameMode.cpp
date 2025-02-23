// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoolingExampleGameMode.h"
#include "PoolingExample/Character/PoolingExampleCharacter.h"
#include "UObject/ConstructorHelpers.h"

APoolingExampleGameMode::APoolingExampleGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/PoolShowcase/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
