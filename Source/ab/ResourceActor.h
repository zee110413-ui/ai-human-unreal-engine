#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HumanTypes.h"
#include "MatterComponent.h"
#include "ResourceActor.generated.h"

class UAffordanceComponent;
class UStaticMeshComponent;

UCLASS()
class AB_API AResourceActor : public AActor
{
    GENERATED_BODY()

public:
    AResourceActor();

    virtual void BeginPlay() override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource")
    EResourceKind Kind = EResourceKind::RawFood;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource")
    float Amount = 1.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Resource")
    TObjectPtr<UStaticMeshComponent> Body;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Resource")
    TObjectPtr<UAffordanceComponent> Affordances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Resource")
    TObjectPtr<class UTextRenderComponent> Label;

    UPROPERTY(Transient)
    TObjectPtr<UStaticMeshComponent> Shape3D;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Resource")
    TObjectPtr<UMatterComponent> Matter;

    UPROPERTY(BlueprintReadOnly, Category = "Resource")
    TObjectPtr<AActor> HeldBy;

    UPROPERTY(BlueprintReadOnly, Category = "Resource")
    float SpoilsAt = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource")
    FVector CustomSize = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource")
    bool bKeepShape = false;

    UPROPERTY(BlueprintReadWrite, Category = "Resource")
    bool bCargo = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Resource")
    EMatterShape KeptShape = EMatterShape::Box;

    void Setup(EResourceKind InKind, float InAmount);
    void BecomeKind(EResourceKind NewKind, float SizeScale);
    FVector CurrentSize() const;

    UFUNCTION(BlueprintCallable, Category = "Resource")
    bool PickUp(AActor* Who);

    UFUNCTION(BlueprintCallable, Category = "Resource")
    void PutDown();

    UFUNCTION(BlueprintCallable, Category = "Resource")
    float Consume(float Portions);

    UFUNCTION(BlueprintCallable, Category = "Resource")
    bool IsAvailable() const { return HeldBy == nullptr && Amount > 0.0f; }

    UFUNCTION(BlueprintCallable, Category = "Resource")
    static FString KindName(EResourceKind Which);

    static AResourceActor* Spawn(UWorld* World, EResourceKind Kind, float Amount, const FVector& At);
    static AResourceActor* SpawnPiece(UWorld* World, EResourceKind Kind, float Amount, const FTransform& Where, const FVector& SizeCm, EMatterShape Shape);
    static void BaseLook(EResourceKind Kind, EMatterShape& OutShape, FVector& OutSize, const TCHAR*& OutFallback);
    static float UnitVolume(EResourceKind Kind);
    static EResourceKind KindFromWord(const FString& Word);
};
