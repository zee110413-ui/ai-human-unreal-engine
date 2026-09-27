#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HumanTypes.h"
#include "ClothingActor.generated.h"

class UStaticMeshComponent;
class UAffordanceComponent;
class UMaterialInterface;
class ACompleteHumanNPC;

UENUM(BlueprintType)
enum class EClothingKind : uint8
{
    None, Shirt, Trousers, Dress, Coat, Boots, Hat, Apron, Belt, BastShoes, Kerchief, Crown, Helmet, Swaddle
};

UCLASS()
class AB_API AClothingActor : public AActor
{
    GENERATED_BODY()

public:
    AClothingActor();

    virtual void BeginPlay() override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clothing")
    EClothingKind Kind = EClothingKind::Shirt;

    UPROPERTY(BlueprintReadOnly, Category = "Clothing")
    FString Name;

    /** Сколько тепла держит 0..1. */
    UPROPERTY(BlueprintReadOnly, Category = "Clothing")
    float Warmth = 0.2f;

    /** Насколько износилась 0..1: дырявое греет хуже. */
    UPROPERTY(BlueprintReadOnly, Category = "Clothing")
    float Worn = 0.0f;

    /** Насколько грязная 0..1. */
    UPROPERTY(BlueprintReadOnly, Category = "Clothing")
    float Dirt = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Clothing")
    TWeakObjectPtr<ACompleteHumanNPC> WornBy;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Clothing")
    TObjectPtr<UStaticMeshComponent> Body;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Clothing")
    TObjectPtr<UAffordanceComponent> Affordances;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UStaticMeshComponent>> Pieces;

    FLinearColor Dye = FLinearColor(1.0f, 1.0f, 1.0f);
    EResourceKind Stuff = EResourceKind::Cloth;
    float Length = 1.0f;

    void Setup(EClothingKind InKind, const FString& OfWhat);
    void Colour(const FLinearColor& InDye, EResourceKind InStuff, float InLength = 1.0f);
    float MassKg() const;

    bool PutOn(ACompleteHumanNPC* Who);
    void TakeOff();

    /** Ношение изнашивает и пачкает. */
    void Advance(float GameDelta, bool bWorking);

    static FString NameOf(EClothingKind Kind);
    static float WarmthOf(EClothingKind Kind);
    static EClothingKind KindFromWord(const FString& Word);

    static AClothingActor* Spawn(UWorld* World, EClothingKind Kind, const FString& OfWhat, const FVector& At);
    static AClothingActor* Spawn(UWorld* World, EClothingKind Kind, const FString& OfWhat, const FVector& At, EResourceKind Stuff,
        const FLinearColor& Dye, float Length = 1.0f);

private:
    void Tailor(ACompleteHumanNPC* Who);
    void Unstitch();
    UMaterialInterface* Cloth() const;
    UStaticMeshComponent* Stitch(ACompleteHumanNPC* Who, const TCHAR* Shape, const FVector& From, const FVector& To, float Radius, float Depth,
        const FVector& Side, FName Bone, UMaterialInterface* Material);
};
