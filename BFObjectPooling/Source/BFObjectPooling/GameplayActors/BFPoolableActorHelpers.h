// Copyright (c) 2024 Jack Holland 
// Licensed under the MIT License. See LICENSE.md file in repo root for full license information.

#pragma once
#include "Components/AudioComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/HitResult.h"
#include "BFPoolableActorHelpers.generated.h"

class UNiagaraSystem;
class UCurveVector4;






// Defines a collision shape type.
UENUM(BlueprintType)
enum class EBFCollisionShapeType : uint8
{
	NoCollisionShape = 0,
	Sphere,
	Capsule,
	Box,
};



// Describes a material and its index for a mesh.
USTRUCT(BlueprintType, meta=(DisplayName="BF Poolable Mesh Material Description"))
struct FBFPoolableMeshMaterialDescription
{
	GENERATED_BODY();
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TObjectPtr<UMaterialInterface> Material = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	int32 MaterialIndex = 0;
};



// Used to describe a scene components collision shape and its params.
USTRUCT(BlueprintType, meta=(DisplayName="BF Collision Shape Description", HasNativeMake=""))
struct FBFCollisionShapeDescription
{
	GENERATED_BODY();
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	EBFCollisionShapeType CollisionShapeType = EBFCollisionShapeType::NoCollisionShape;
	
	// Important: if your projectiles are overlapping immediately try using a custom collision channel that ignores whatever is being overlapped.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FCollisionProfileName CollisionProfile = FCollisionProfileName("WorldDynamic");
	
	/** Describes the collision shapes dimensions. If Collision Shape is None then these are not applied and there is no collision, otherwise:
	 * - Sphere:	X = Radius
	 * - Capsule:	X = Radius,		Y = Half Height
	 * - Box:		X = Half Width, Y = Half Height, Z = Half Depth */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, meta=(SplitStruct))
	FVector ShapeParams = FVector::ZeroVector;
};


// Optional struct that if set will be used to define how this poolable actor attaches to the provided actor/component
// The component takes precedence over the actor if both are set since a component is more explicit.
USTRUCT(BlueprintType, meta=(DisplayName="BF Poolable Actor Attachment Description"))
struct FBFPoolableActorAttachmentDescription
{
	GENERATED_BODY();
public:
	bool IsSet() const { return IsValid(AttachmentActor) || IsValid(AttachmentComponent); }
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<AActor> AttachmentActor = nullptr;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<USceneComponent> AttachmentComponent = nullptr;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName SocketName = NAME_None;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EAttachmentRule LocationRule = EAttachmentRule::KeepWorld;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EAttachmentRule RotationRule = EAttachmentRule::KeepWorld;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EAttachmentRule ScaleRule = EAttachmentRule::KeepWorld;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bWeldSimulatedBodies = false;
};



// Used to describe a static mesh and its params for a poolable actor.
USTRUCT(BlueprintType, meta=(DisplayName="BF Poolable Static Mesh Description"))
struct FBFPoolableStaticMeshDescription
{
	GENERATED_BODY();
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TObjectPtr<UStaticMesh> Mesh = nullptr;

	// Defines the collision profile to use for the mesh, only used when simulating physics like inside of FBFPoolableProjectileActor with bShouldMeshSimulatePhysicsOnImpact.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FCollisionProfileName CollisionProfile = FCollisionProfileName("WorldDynamic");

	// Defines a meshes relative transform to its parent.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FTransform RelativeTransform = FTransform::Identity;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TArray<FBFPoolableMeshMaterialDescription> Materials;
};





DECLARE_DYNAMIC_DELEGATE_OneParam(FOnProjectileStopped, FHitResult, ComponentHitResult);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnProjectileHitOrOverlap, FHitResult, ComponentHitResult, bool, bOverlap);

// Used to describe a projectile actors params for a poolable actor.
USTRUCT(BlueprintType, meta=(DisplayName="BF Poolable Projectile Actor Description"))
struct FBFPoolableProjectileActorDescription
{
	GENERATED_BODY();
public:
	// Optional static mesh to be displayed along with the projectile.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FBFPoolableStaticMeshDescription ProjectileMesh;

	// Optional collision shape to be used for the projectile. If none is provided the projectile will be represented via a scene component.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FBFCollisionShapeDescription ProjectileCollisionShape;

	// Optional Niagara system to be played along with the projectile.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TObjectPtr<UNiagaraSystem> NiagaraSystem = nullptr;

	// Optional attachment socket name for the niagara system, should be relative to the static mesh along with this actor, if not using a static mesh this isn't used.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FName NiagaraSystemAttachmentSocketName = NAME_None;

	// Optional relative transform for the niagara system, if using a valid socket this will be relative to that, otherwise it's relative to the scene component.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FTransform NiagaraSystemRelativeTransform = FTransform::Identity;

	/* Optional target for the projectile to home in on, if using then you should also set HomingAccelerationSpeed.
	 * bIsHomingProjectile on the component is inferred true if this is valid.*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<USceneComponent> HomingTargetComponent = nullptr;
	
	// Is called for both overlap and hit events passing through a bool to signify which one.
	UPROPERTY(BlueprintReadWrite)
	FOnProjectileHitOrOverlap OnProjectileHitOrOverlapDelegate;
	
	// Called when projectile has come to a stop (velocity is below simulation threshold, bounces are disabled, or it is forcibly stopped).
	UPROPERTY(BlueprintReadWrite)
	FOnProjectileStopped OnProjectileStoppedDelegate;

	// Initial Starting velocity of the projectile, can be in local or world space, just ensure the bIsVelocityInLocalSpace flag is set.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FVector Velocity = {1200.f, 0.f, 0.f};

	/** Values above 0 drive how long until the pooled actor will attempt to auto return to the pool,
	* requires you to have given the handle to the pooled actor either via SetPoolHandle or FireAndForget.
	* (Note that some other properties may also trigger a return such as bShouldReturnOnImpact).*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float ActorCurfew = 5.0f;

	// Defines the max speed of the projectile.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float MaxSpeed = 4000.0f;
	
	// When homing on a target this defines the acceleration speed, limited by the max speed.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float HomingAccelerationSpeed = 2000.0f;

	// When impacting with a surface this defines the bounciness of the projectile.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float Bounciness = 0.5f;

	// Defines the friction of the projectile when in contact with surfaces.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float Friction = 0.2f;

	// Set to 0 to ignore gravity.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float ProjectileGravityScale = 1.f;

	// If true the velocity will be interpreted from local to world space.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	uint8 bIsVelocityInLocalSpace:1 = true;
	
	// If disabled the projectile will teleport between updates rather than try detect a collision. When disabled ONLY overlap hits can be generated.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	uint8 bSweepCollision:1 = true;

	// Whether or not the projectile should follow the velocity vector.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	uint8 bRotationFollowsVelocity:1 = true;
	
	// Requires bRotationFollowsVelocity to be true. Will lock the rotation to the vertical axis only being able to tilt up and down.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	uint8 bRotationRemainsVertical:1 = false;

	// If the projectile should bounce upon impact.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	uint8 bShouldBounce:1 = true;

	// If true the projectile will return to the pool immediately after ANY overlap or blocking hit (Is triggered AFTER we callback the delegates above in-case you needed any logic just prior).
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	uint8 bShouldReturnOnImpact:1 = false;
	
	// If true the mesh will start simulating physics upon impact, note that if you return on impact this will be redundant and ignored.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	uint8 bShouldMeshSimulatePhysicsOnImpact:1 = false;
	
	// If true the projectile will return to the pool immediately after stopping.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	uint8 bShouldReturnOnStop:1 = false;

	// If we are not going to return when we stop should we disable collision at least so it doesn't block other projectiles or actors.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	uint8 bShouldDisableCollisionOnStop:1 = false;

	// If we spawn too quickly we can collide with our same type of projectile, this will ignore collision with other projectiles of the same type.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	uint8 bIgnoreCollisionWithOtherProjectiles:1 = true;

};


/** Implementation of a Curve for Vec4 types. */
UCLASS(BlueprintType, MinimalAPI)
class UCurveVector4 : public UCurveBase
{
	GENERATED_BODY()

public:
	UCurveVector4(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {}
	virtual TArray<FRichCurveEditInfoConst> GetCurves() const override;
	virtual TArray<FRichCurveEditInfo> GetCurves() override;
	
	// Evaluate this float curve at the specified time
	UFUNCTION(BlueprintCallable, Category="Math|Curves")
	BFOBJECTPOOLING_API FVector4 GetVectorValue(float InTime) const;
	BFOBJECTPOOLING_API bool operator == (const UCurveVector4& Curve) const;
	virtual bool IsValidCurve( FRichCurveEditInfo CurveInfo ) override;

public:
	/** Keyframe data, one curve for X, Y, Z W */
	UPROPERTY()
	FRichCurve FloatCurves[4];
};




USTRUCT(BlueprintType, meta=(DisplayName="BF Poolable Widget Actor Description"))
struct FBFPoolable3DWidgetActorDescription
{
	GENERATED_BODY();
public:
	// Widget to display
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TSubclassOf<UUserWidget> WidgetClass = nullptr;

	// If set will face the widget towards the target component (Only if Screen Space is disabled.)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TObjectPtr<USceneComponent> TargetComponent = nullptr;

	// Defines how want this 3D widget to attach to the given actor or component in the world.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FBFPoolableActorAttachmentDescription OptionalAttachmentParams;

	/** Curve SHOULD be a normalized 0-1 range where XYZ define a relative offset to the widget after time and W defines its size,
	 * this curve is sampled over the lifetime of the widget from start to curfew and applied in world space on the widget. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	UCurveVector4* WidgetLifetimePositionAndSizeCurve = nullptr;

	// Ranges from 0-255 and is used to tint your widgets color.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FColor WidgetTintAndOpacity = FColor::White;

	/** When defines the widgets size on screen. If bDrawAtDesiredSize is false and the widget is in world space
	 * then its size will be scaled by the distance from the camera.*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FVector2D DrawSize = FVector2D{256.f, 256.f};

	/** Values above 0 drive how long until the pooled actor will attempt to auto return to the pool,
	* requires you to have given the handle to the pooled actor either via SetPoolHandle or FireAndForget.*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float ActorCurfew = 2.0f;

	// What space to display the widget in.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	EWidgetSpace WidgetSpace = EWidgetSpace::World;

	// Whether or not this widget should be drawn as a two sided.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	uint8 bTwoSided:1 = false;

	// Should the widget cast shadows.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	uint8 bShouldCastShadow:1 = false;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	uint8 bShouldTickWhenOffscreen:1 = false;

	// When the game instance is paused should this widget also pause
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	uint8 bTickableWhenPaused:1 = true;
	
	// Optionally can invert the sampled curve value.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	uint8 bInvertWidgetCurve:1 = false;
};




USTRUCT(BlueprintType, meta=(DisplayName="BF Poolable Decal Actor Description"))
struct FBFPoolableDecalActorDescription
{
	GENERATED_BODY();
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TObjectPtr<UMaterialInterface> DecalMaterial = nullptr;

	// Defines how want this decal to attach to the given actor or component in the world.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FBFPoolableActorAttachmentDescription OptionalAttachmentParams;
	
	// World space extent of the decal.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FVector DecalExtent = FVector{256.f, 256.f, 256.f};

	/** Values above 0 drive how long until the pooled actor will attempt to auto return to the pool,
	* requires you to have given the handle to the pooled actor either via SetPoolHandle or FireAndForget.*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float ActorCurfew = 2.0f;

	/* Controls how quickly the decal fades in if above 0. Requires your material to use
	 * the DecalLifetimeOpacity node in materials for you to have any effect.*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float FadeInTime = .1f;

	/* Controls how quickly the decal fades out if above 0. Requires your material to use
	 * the DecalLifetimeOpacity node in materials for you to have any effect.*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float FadeOutTime = .1f;

	// Screen size of the decal before it starts to fade out.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float FadeScreenSize = .001f;
	
	// Higher values draw over lower values.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	int32 SortOrder = 0;
};

 

USTRUCT(BlueprintType, meta=(DisplayName="BF Poolable Niagara Actor Description"))
struct FBFPoolableNiagaraActorDescription
{
	GENERATED_BODY();
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TObjectPtr<UNiagaraSystem> NiagaraSystem = nullptr;

	// Defines how want this niagara system to attach to the given actor or component in the world.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FBFPoolableActorAttachmentDescription OptionalAttachmentParams;

	
	// If curfew is set then this delayed time is already accounted for and we will return at curfew + delay time.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float DelayedActivationTimeSeconds = -1.0f; 

	/** Values above 0 drive how long until the pooled actor will attempt to auto return to the pool,
	* requires you to have given the handle to the pooled actor either via SetPoolHandle or FireAndForget.
	* Should opt for bAutoReturnOnSystemFinish when possible over this option.*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float ActorCurfew = -1.0f;

	// Requires the handle to have been given to us which is the expected behavior. If disabled you need to make sure you have some way of returning the actor to the pool still.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	uint8 bAutoReturnOnSystemFinish:1 = true;
};






DECLARE_DYNAMIC_DELEGATE(FOnPooledSoundFinished);

USTRUCT(BlueprintType, meta=(DisplayName="BF Poolable Sound Actor Description"))
struct FBFPoolableSoundActorDescription
{
	GENERATED_BODY();
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TObjectPtr<USoundBase> Sound = nullptr;
	
	// Defines how want this sound to attach to the given actor or component in the world.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FBFPoolableActorAttachmentDescription OptionalAttachmentParams;
	
	// Optional sound attenuation settings.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FSoundAttenuationSettings AttenuationSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FOnPooledSoundFinished OnSoundFinishedDelegate;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float VolumeMultiplier = 1.0f;
	
	// Can skip ahead where a sound starts
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float StartingTimeOffset = 0.0f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float PitchMultiplier = 1.0f;
	
	// If curfew is set then this delayed time is already accounted for and we will return at curfew + delay time.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float DelayedActivationTimeSeconds = -1.0f;

	// If above 0 when activating the sound you can specify a fade in time
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float FadeInTime = -1.0f;
	
	// If above 0 when the curfew elapses and the the sound is still playing will use this value to fade out audio and then return.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float FadeOutTime = -1.0f;
	
	/** Values above 0 drive how long until the pooled actor will attempt to auto return to the pool,
	* requires you to have given the handle to the pooled actor either via SetPoolHandle or FireAndForget.
	* (Should be used when unable to take advantage of bAutoReturnOnSoundFinish). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float ActorCurfew = -1.0f;

	// Sound curve to be applied when using audio fading.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	EAudioFaderCurve FadeInCurve = EAudioFaderCurve::Linear;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	uint8 bReverb:1 = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	uint8 bUISound:1 = false;

	// Requires the handle to have been given to us which is the expected behavior. If disabled you need to make sure you have some way of returning the actor to the pool still.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	uint8 bAutoReturnOnSoundFinish:1 = true;
};




USTRUCT(BlueprintType, meta=(DisplayName="BF Poolable Skeletal Mesh Actor Description"))
struct FBFPoolableSkeletalMeshActorDescription
{
	GENERATED_BODY();
	
public:
	// Collision Profile to use for the mesh.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FCollisionProfileName CollisionProfile = FCollisionProfileName(TEXT("Ragdoll"));

	
	// If using collision on the mesh, what type of collision to use, most of the time NoCollision or CollisionEnabled is enough, ragdolls require collision.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TEnumAsByte<ECollisionEnabled::Type> CollisionEnabled = ECollisionEnabled::NoCollision;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TObjectPtr<USkeletalMesh> Mesh = nullptr;

	// Defines the meshes transform relative to the parent root.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FTransform RelativeTransform = FTransform::Identity;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TArray<FBFPoolableMeshMaterialDescription> Materials;
	
	// Animation instance to use for the mesh if any.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TSubclassOf<UAnimInstance> AnimationInstance = nullptr;

	// Optional animation sequence to play on the mesh, will play only if there is no animation instance set above.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TObjectPtr<UAnimSequence> AnimSequence = nullptr;
	
	/** Values above 0 drive how long until the pooled actor will attempt to auto return to the pool,
	* requires you to have given the handle to the pooled actor either via SetPoolHandle or FireAndForget.*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float ActorCurfew = 10.0f;

	/* If simulating physics and this is above 0 then this defines how long after activating this poolable actor
	* should we waiting until we put all the physics bodies to sleep. By default the sleep collision profile is Ragdoll so we no longer interact.
	* Should be overridden at the actor level if you do not want this behaviour by implementing something similar yourself. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float PhysicsBodySleepDelay = -1.f;

	/** When playing animations or simulating physics we need to enable ticking otherwise it just won't work, you can however
	 * optionally set a tick interval for the mesh component here, anything above 0 will be used otherwise we stick to default tick interval of every frame.
	 * Only really want to lower the tick interval if the mesh is far away or you deem it acceptable to save on performance. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float MeshTickInterval = -1.f;
	
	// If true the mesh will simulate physics and no animations will be played.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	uint8 bSimulatePhysics:1 = false;

	// Only applies to AnimSequence.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	uint8 bLoopAnimSequence:1 = false;
};



USTRUCT(BlueprintType, meta=(DisplayName="BF Poolable Static Mesh Actor Description"))
struct FBFPoolableStaticMeshActorDescription
{
	GENERATED_BODY();
public:
	// Collision Profile to use for the mesh.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FCollisionProfileName CollisionProfile = FCollisionProfileName(TEXT("NoCollision"));
	
	// If using collision on the mesh, what type of collision to use, most of the time NoCollision or CollisionEnabled is enough.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TEnumAsByte<ECollisionEnabled::Type> CollisionEnabled = ECollisionEnabled::NoCollision;
	
	// Mesh to display
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TObjectPtr<UStaticMesh> Mesh = nullptr;

	// Defines the meshes transform relative to the parent root.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FTransform RelativeTransform = FTransform::Identity;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TArray<FBFPoolableMeshMaterialDescription> Materials;

	/** Values above 0 drive how long until the pooled actor will attempt to auto return to the pool,
	* requires you to have given the handle to the pooled actor either via SetPoolHandle or FireAndForget.*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float ActorCurfew = 5.0f;

	/* If simulating physics and this is above 0 then this defines how long after activating this poolable actor
	 * should we waiting until we the physics body to sleep. By default the sleep collision profile is Ragdoll so we no longer interact.
	 * Should be overridden at the actor level if you do not want this behaviour by implementing something similar yourself. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float PhysicsBodySleepDelay = -1.f;

	// If true the mesh will simulate physics.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	uint8 bSimulatePhysics:1 = false;
	
};






// Curve Vec4 function implementations.
inline FVector4 UCurveVector4::GetVectorValue( float InTime ) const
{
	FVector4 Result;
	Result.X = FloatCurves[0].Eval(InTime);
	Result.Y = FloatCurves[1].Eval(InTime);
	Result.Z = FloatCurves[2].Eval(InTime);
	Result.W = FloatCurves[3].Eval(InTime);
	return Result;
}


namespace BF::OP
{
	static const FName XCurveName(TEXT("X"));
	static const FName YCurveName(TEXT("Y"));
	static const FName ZCurveName(TEXT("Z"));
	static const FName WCurveName(TEXT("W"));
}


inline TArray<FRichCurveEditInfoConst> UCurveVector4::GetCurves() const 
{
	TArray<FRichCurveEditInfoConst> Curves;
	Curves.Add(FRichCurveEditInfoConst(&FloatCurves[0], BF::OP::XCurveName));
	Curves.Add(FRichCurveEditInfoConst(&FloatCurves[1], BF::OP::YCurveName));
	Curves.Add(FRichCurveEditInfoConst(&FloatCurves[2], BF::OP::ZCurveName));
	Curves.Add(FRichCurveEditInfoConst(&FloatCurves[3], BF::OP::WCurveName));
	return Curves;
}


inline TArray<FRichCurveEditInfo> UCurveVector4::GetCurves() 
{
	TArray<FRichCurveEditInfo> Curves;
	Curves.Add(FRichCurveEditInfo(&FloatCurves[0], BF::OP::XCurveName));
	Curves.Add(FRichCurveEditInfo(&FloatCurves[1], BF::OP::YCurveName));
	Curves.Add(FRichCurveEditInfo(&FloatCurves[2], BF::OP::ZCurveName));
	Curves.Add(FRichCurveEditInfo(&FloatCurves[3], BF::OP::WCurveName));
	return Curves;
}


inline bool UCurveVector4::operator==( const UCurveVector4& Curve ) const
{
	return (FloatCurves[0] == Curve.FloatCurves[0]) && (FloatCurves[1] == Curve.FloatCurves[1]) && (FloatCurves[2] == Curve.FloatCurves[2]) && (FloatCurves[3] == Curve.FloatCurves[3]);
}


inline bool UCurveVector4::IsValidCurve( FRichCurveEditInfo CurveInfo )
{
	return	CurveInfo.CurveToEdit == &FloatCurves[0] ||
			CurveInfo.CurveToEdit == &FloatCurves[1] ||
			CurveInfo.CurveToEdit == &FloatCurves[2] ||
			CurveInfo.CurveToEdit == &FloatCurves[3];
}











