#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Matter.h"
#include "MatterComponent.generated.h"

class UPrimitiveComponent;
class UPhysicalMaterial;
class UMatterSubsystem;

UENUM()
enum class EMatterShape : uint8
{
    Box,
    Cylinder,
    Sphere,
    Cone
};

UCLASS(ClassGroup = (Matter), meta = (BlueprintSpawnableComponent))
class AB_API UMatterComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UMatterComponent();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    void WatchRest();
    float ReposeDegrees(const AActor* GroundActor, const FVector& Where) const;
    bool IsUpright() const;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Matter")
    EResourceKind Substance = EResourceKind::Stone;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Matter")
    float Moisture = 0.08f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Matter")
    float Temperature = 12.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Matter")
    float Ice = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Matter")
    float Transition = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Matter")
    float Damage = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Matter")
    float Char = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Matter")
    float Rot = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Matter")
    float SnowCover = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Matter")
    bool bBurning = false;

    UPROPERTY(BlueprintReadOnly, Category = "Matter")
    bool bSheltered = false;

    UPROPERTY(BlueprintReadOnly, Category = "Matter")
    float DryMassKg = 1.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Matter")
    float OriginalMassKg = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Matter")
    bool bBreakable = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Matter")
    bool bVisualOnly = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Matter")
    bool bMassFromShape = true;

    void Bind(UPrimitiveComponent* InBody, EMatterShape InShape, const FVector& InSizeCm, EResourceKind InSubstance);
    void AddVisual(UPrimitiveComponent* Part);
    void Resize(const FVector& InSizeCm);
    void SetDryMass(float Kilograms);

    const FSubstance& Props() const;
    EMatterPhase Phase() const;
    float WetFraction() const;
    float VolumeM3() const;
    float TotalMassKg() const;
    float SurfaceM2() const;
    float TopM2() const;
    float SmallestSectionM2() const;
    float LengthwiseSectionM2() const;
    FVector GetSizeCm() const { return SizeCm; }
    EMatterShape GetShape() const { return Shape; }
    UPrimitiveComponent* GetBody() const { return Body; }
    bool IsBound() const { return bBound; }

    static float VolumeOf(EMatterShape InShape, const FVector& InSizeCm);

    void Advance(float Seconds, const FMatterAir& Air);
    void ReceiveImpact(float Joules, const FVector& Where, const FVector& Direction, AActor* Instigator, bool bEdge = false);
    void ReceiveHeat(float Joules);
    void ReceiveWater(float Kilograms);
    bool Ignite();
    void Extinguish();
    float Repair(float Fraction);
    void CheckShelter(UMatterSubsystem* Matter);
    FString Describe() const;
    float ImpactShare(const UMatterComponent* Other) const;

private:
    UFUNCTION()
    void OnBodyHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit);

    void Fracture(const FVector& Where, const FVector& Direction, float Energy, AActor* Instigator, bool bEdge);
    void Squash(float Joules, const FVector& Direction);
    void BurnOut();
    void RefreshPhysics(bool bForce);
    void RefreshLook(bool bForce);
    void ApplyState(UPrimitiveComponent* Part) const;

    UPROPERTY()
    TObjectPtr<UPrimitiveComponent> Body;

    UPROPERTY()
    TArray<TObjectPtr<UPrimitiveComponent>> Visuals;

    UPROPERTY()
    TObjectPtr<UPhysicalMaterial> AppliedSurface;

    EMatterShape Shape = EMatterShape::Box;
    FVector SizeCm = FVector(20.0f, 20.0f, 20.0f);
    float ShownWet = -1.0f;
    float ShownCrack = -1.0f;
    float ShownChar = -1.0f;
    float ShownFrost = -1.0f;
    float ShownGlow = -1.0f;
    double LastImpactAt = -1.0;
    float AppliedMass = 0.0f;
    float RestClock = 0.0f;
    bool bFracturing = false;
    bool bBound = false;
};
