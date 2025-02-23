// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoolingExampleCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"

#include "BFObjectPooling/Subsystem/BFGameplayFXSubsystem.h"

DEFINE_LOG_CATEGORY(LogTemplateCharacter);

//////////////////////////////////////////////////////////////////////////
// APoolingExampleCharacter

APoolingExampleCharacter::APoolingExampleCharacter()
{
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	static FName SpringArmComponentName{"CameraBoom"};
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(SpringArmComponentName);
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f; 	
	CameraBoom->bUsePawnControlRotation = true;

	static FName FollowCameraComponentName{"FollowCamera"};
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(FollowCameraComponentName);
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
}

void APoolingExampleCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}

void APoolingExampleCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		EnhancedInputComponent->BindAction(IA_Jump, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(IA_Jump, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		EnhancedInputComponent->BindAction(IA_Move, ETriggerEvent::Triggered, this, &APoolingExampleCharacter::Move);
		EnhancedInputComponent->BindAction(IA_Look, ETriggerEvent::Triggered, this, &APoolingExampleCharacter::Look);
	}
	else
	{
		UE_LOG(LogTemplateCharacter, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}






// -------------------------------------- Pooling Example --------------------------------------

// Step 1 - Initialize and create the pool.
void APoolingExampleCharacter::MyExamplePoolInit()
{
	FBFObjectPoolInitParams InitParams;
	InitParams.Owner = this;
	InitParams.PoolClass = ExamplePoolClass; // or if not using a BP subclass just use the static class ABFPoolableStaticMeshActor::StaticClass();
	InitParams.PoolLimit = 100;
	InitParams.InitialCount = 5;

	ExampleStaticMeshPool = TBFObjectPool<ABFPoolableStaticMeshActor>::CreateAndInitPool(InitParams); 
}


// Step 2 - Use the pool.
void APoolingExampleCharacter::MyExamplePoolUsage()
{
	// Non reclaimable means this object is ours and ours only, we the pool is not allowed to force return our object until we return it or the handles all go out of scope.
	if (TBFPooledObjectHandlePtr<ABFPoolableStaticMeshActor> Handle = ExampleStaticMeshPool->UnpoolObject(true, EBFPooledObjectReclaimPolicy::NonReclaimable))
	{
		ABFPoolableStaticMeshActor* Actor = Handle->GetObject();

		FBFPoolableStaticMeshActorDescription Description;
		Description.Mesh = ExampleMesh; // You would set this to a mesh asset.
		Description.Materials.Add(FBFPoolableMeshMaterialDescription{ExampleMaterial, 0}); // Takes the material and the index of the mesh to apply it to.
		Description.ActorCurfew = 20.f; // How long until the actor will auto return to the pool.
		Description.bSimulatePhysics = true;
		Description.PhysicsBodySleepDelay = 5.f; // We stop simulating after 5s but we don't return to the pool until 20s (15s after disabling physics).
		Description.CollisionProfile = FCollisionProfileName("Ragdoll");

		// Now init and tell it where to spawn, the actor takes the handle and returns when its ready, you no longer need to do anything.
		Actor->FireAndForget(Handle, Description, GetActorTransform());
	}
}


// If we wanted to access the world global effects pool this is how it's done.
void APoolingExampleCharacter::MyExamplePoolGlobalSubsystemUsage()
{
	UBFGameplayFXSubsystem* FXSubsystem = UBFGameplayFXSubsystem::Get(this);

	// Before we can use the pool we must initialize the pool, this is would typically be done from the game mode or game state or just some
	// meaningful place that sets up initializations. For this example I will just do it here so it's clear but this only needs to be done ONCE per world.
	// Here the subsystem is only ever able to be used for unpooling decals since that is the only pool we specified but it also supports other types such as
	// static mesh actors, skeletal mesh actors, particle systems, etc.
	if (!FXSubsystem->HasPoolsBeenInitialized())
	{
		FBFGameplayFXSubsystemInitParams InitParams;
		InitParams.DecalPoolParams.PoolActorLimit = 100;
		InitParams.DecalPoolParams.PoolActorInitialCount = 42;
		FXSubsystem->InitializePools(InitParams);
	}

	
	// Now this is it! We can spawn them by just specify the decal system params!
	FBFPoolableDecalActorDescription Description;
	Description.DecalMaterial = ExampleMaterial; // You would set this to a decal material.
	Description.ActorCurfew = 20.f; // How long until the actor will auto return to the pool.

	// Policy here is reclaimable, meaning if we are full and someone else wants a decal we can force this decal to return and give it to them,
	// good for things like bullet hole decals.
	FXSubsystem->SpawnDecalActor(Description, EBFPooledObjectReclaimPolicy::Reclaimable, GetActorTransform());
}



// -------------------------------------- Pooling Example --------------------------------------







void APoolingExampleCharacter::Move(const FInputActionValue& Value)
{
	FVector2D MovementVector = Value.Get<FVector2D>();
	if (Controller != nullptr)
	{
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}


void APoolingExampleCharacter::Look(const FInputActionValue& Value)
{
	FVector2D LookAxisVector = Value.Get<FVector2D>();
	if (Controller != nullptr)
	{
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}
