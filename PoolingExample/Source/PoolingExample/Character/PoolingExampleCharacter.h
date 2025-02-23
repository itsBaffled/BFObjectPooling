// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "BFObjectPooling/GameplayActors/BFPoolableStaticMeshActor.h"
#include "BFObjectPooling/Pool/BFObjectPool.h"

#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "PoolingExampleCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

UCLASS(config=Game)
class APoolingExampleCharacter : public ACharacter
{
	GENERATED_BODY()
public:
	APoolingExampleCharacter();

protected:
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
			
	USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	UCameraComponent* GetFollowCamera() const { return FollowCamera; }

protected:
	virtual void NotifyControllerChanged() override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;



	// POOL EXAMPLE STUFF:
	
	// Because the show case uses the BP side of the pool (for quick iteration) I am only going
	// to demonstrate the pool usage here but nothing calls this by default, but this is still how it would be done.
	void MyExamplePoolInit();
	void MyExamplePoolUsage();
	void MyExamplePoolGlobalSubsystemUsage();

	TBFObjectPoolPtr<ABFPoolableStaticMeshActor> ExampleStaticMeshPool;
	
	UPROPERTY(EditAnywhere)
	TSubclassOf<ABFPoolableStaticMeshActor> ExamplePoolClass;

	UPROPERTY(EditAnywhere)
	TObjectPtr<UStaticMesh> ExampleMesh;
	
	UPROPERTY(EditAnywhere)
	TObjectPtr<UMaterialInstance> ExampleMaterial;






	
private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> FollowCamera;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> IA_Jump;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> IA_Move;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> IA_Look;

	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> IA_LeftClick;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> IA_One;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> IA_Two;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> IA_Three;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> IA_Four;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> IA_Five;
};

