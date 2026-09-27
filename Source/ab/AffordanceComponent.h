// AffordanceComponent.h
// ---------------------------------------------------------------------------
// «Что со мной можно сделать».
//
// Вешается на что угодно: на кровать, на плиту, на здание, на дерево.
// Объект сам объявляет, что он предлагает и что это даёт. Это свойство
// ВЕЩИ, а не правило поведения человека.
//
// Благодаря этому внутри NPC нет ни одной строчки вида «если голоден — иди
// в кафе». Он видит предложение «+0.8 сытости» и решает сам.
//
// Чтобы добавить в мир новую возможность — новую еду, новое развлечение,
// новую работу, — не нужно трогать код интеллекта. Достаточно повесить
// этот компонент и заполнить список предложений.
// ---------------------------------------------------------------------------

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HumanTypes.h"
#include "AffordanceComponent.generated.h"

UCLASS(ClassGroup = (Human), meta = (BlueprintSpawnableComponent))
class AB_API UAffordanceComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UAffordanceComponent();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

    /** Что этот объект предлагает. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Affordance")
    TArray<FAffordance> Offers;

    /** Как называется это место/вещь — попадёт в ключ опыта и в мысли. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Affordance")
    FString DisplayName;

    /**
     * Вид, к которому относится эта вещь: «кафе», «плита», «кровать».
     * По нему опыт переносится с одного предмета на все ему подобные.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Affordance")
    FString Category;

    /** С какого расстояния предложение вообще заметно (см). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Affordance")
    float NoticeRadius = 1800.0f;

    /** Сколько человек могут пользоваться одновременно. 0 — сколько угодно. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Affordance")
    int32 Capacity = 0;

    /** Сколько занято сейчас. */
    UPROPERTY(BlueprintReadOnly, Category = "Affordance")
    int32 InUse = 0;

    /** Это чужая собственность — пользоваться может только хозяин. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Affordance")
    bool bPrivate = false;

    /** Чьё: точка, по которой хозяин узнаёт своё (обычно вход в его дом). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Affordance")
    FVector OwnerAnchor = FVector::ZeroVector;

    /** Свободно ли. */
    UFUNCTION(BlueprintCallable, Category = "Affordance")
    bool IsAvailable() const { return Capacity <= 0 || InUse < Capacity; }

    UFUNCTION(BlueprintCallable, Category = "Affordance")
    void Occupy() { ++InUse; }

    UFUNCTION(BlueprintCallable, Category = "Affordance")
    void Release() { InUse = FMath::Max(0, InUse - 1); }

    /** Готовые предложения с проставленными ключами и координатами. */
    void CollectOffers(TArray<FAffordance>& Out) const;

    // --- Сборка типовых предложений -----------------------------------------
    // Это знание МИРА о самом себе: что такое кровать, что такое кафе.
    // Психика этим не пользуется — она только читает результат.

    /** Добавить предложение вручную. */
    UFUNCTION(BlueprintCallable, Category = "Affordance")
    void AddOffer(EActionType Action, const FString& Label, float Duration);

    /** Добавить обещание к последнему предложению. */
    UFUNCTION(BlueprintCallable, Category = "Affordance")
    void AddPromise(ENeedType Need, float Amount);

    /** Заполнить набор предложений, типичный для места такого рода. */
    UFUNCTION(BlueprintCallable, Category = "Affordance")
    void MakeTypicalFor(EPlaceKind Kind);
    void MakeVillageTypical(EPlaceKind Kind);
};
