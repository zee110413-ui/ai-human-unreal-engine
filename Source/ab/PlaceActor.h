// PlaceActor.h
// ---------------------------------------------------------------------------
// Место в городе: кафе, контора, сквер, подъезд.
//
// Ничего не решает и ни за кем не следит. Просто стоит и предлагает то,
// что у него есть. Человек, проходя мимо, это замечает и запоминает.
//
// Поставьте такой актор на уровень, выберите вид места — и люди начнут
// туда ходить. Код интеллекта при этом не меняется ни на строчку.
// ---------------------------------------------------------------------------

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HumanTypes.h"
#include "PlaceActor.generated.h"

class UAffordanceComponent;

UCLASS()
class AB_API APlaceActor : public AActor
{
    GENERATED_BODY()

public:
    APlaceActor();

    virtual void BeginPlay() override;

    /** Что это за место. Определяет набор предложений по умолчанию. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Place")
    EPlaceKind Kind = EPlaceKind::Food;

    /** Как это место называют. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Place")
    FString PlaceName;

    /** Сколько человек помещается одновременно. 0 — сколько угодно. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Place")
    int32 Capacity = 0;

    /** Заполнить предложения по виду места (если список пуст). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Place")
    bool bUseTypicalOffers = true;

    /** Показывать метку места. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Place")
    bool bShowMarker = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Place")
    TObjectPtr<UAffordanceComponent> Affordances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Place")
    TObjectPtr<class UStaticMeshComponent> Marker;

    /** Настроить место под конкретный вид и имя. */
    UFUNCTION(BlueprintCallable, Category = "Place")
    void Configure(EPlaceKind InKind, const FString& InName, int32 InCapacity);
};
