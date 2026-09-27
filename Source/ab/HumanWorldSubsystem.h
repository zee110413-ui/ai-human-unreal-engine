// HumanWorldSubsystem.h
// ---------------------------------------------------------------------------
// Общая реальность, в которой живут люди:
//   - игровые часы (сутки, дни недели, ночь/день) — от них зависит вся жизнь;
//   - реестр всех людей — чтобы не искать их перебором актёров каждый тик;
//   - шина событий: то, что случилось рядом, видят все, кто рядом.
// ---------------------------------------------------------------------------

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "HumanTypes.h"
#include "HumanWorldSubsystem.generated.h"

class ACompleteHumanNPC;

/** Публичное событие мира — то, что могли заметить окружающие. */
USTRUCT(BlueprintType)
struct FWorldEvent
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "World") FName Tag;
    UPROPERTY(BlueprintReadWrite, Category = "World") FString Description;
    UPROPERTY(BlueprintReadWrite, Category = "World") FVector Location = FVector::ZeroVector;
    /** Радиус, в котором событие заметно (см). */
    UPROPERTY(BlueprintReadWrite, Category = "World") float Radius = 1500.0f;
    /** Кто это сделал. */
    UPROPERTY(BlueprintReadWrite, Category = "World") TObjectPtr<AActor> Instigator = nullptr;
    /** С кем это произошло. */
    UPROPERTY(BlueprintReadWrite, Category = "World") TObjectPtr<AActor> Target = nullptr;
    /** Насколько это хорошо «объективно» -1..+1 (каждый оценит по-своему). */
    UPROPERTY(BlueprintReadWrite, Category = "World") float Valence = 0.0f;
    /** Значимость 0..1. */
    UPROPERTY(BlueprintReadWrite, Category = "World") float Significance = 0.5f;
    /** Нарушение общественной нормы -1..+1. */
    UPROPERTY(BlueprintReadWrite, Category = "World") float NormViolation = 0.0f;
    UPROPERTY(BlueprintReadWrite, Category = "World") float Time = 0.0f;
};

UCLASS()
class AB_API UHumanWorldSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()

public:
    // --- жизненный цикл подсистемы ---
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;
    virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

    // --- ВРЕМЯ --------------------------------------------------------------

    /**
     * Сколько игровых минут проходит за одну реальную секунду.
     * 1.0 — сутки за 24 реальные минуты. Это компромисс: успеть увидеть
     * целый день жизни, но не гнать время так, чтобы человек не успевал
     * дойти до другого конца города.
     */
    UPROPERTY(BlueprintReadWrite, Category = "World|Time")
    float GameMinutesPerRealSecond = 1.0f;

    /** Игровое время в секундах от начала симуляции. Все метки времени — в нём. */
    UPROPERTY(BlueprintReadOnly, Category = "World|Time")
    float WorldSeconds = 0.0f;

    /** Текущая дата и час. */
    UPROPERTY(BlueprintReadOnly, Category = "World|Time")
    FHumanTime Now;

    UFUNCTION(BlueprintCallable, Category = "World|Time")
    float GetWorldSeconds() const { return WorldSeconds; }

    UFUNCTION(BlueprintCallable, Category = "World|Time")
    FHumanTime GetTime() const { return Now; }

    /** Сколько игровых секунд в одном игровом часе. */
    UFUNCTION(BlueprintCallable, Category = "World|Time")
    float SecondsPerGameHour() const { return 3600.0f; }

    /** Перевод реальных секунд кадра в игровые. */
    UFUNCTION(BlueprintCallable, Category = "World|Time")
    float RealToGameSeconds(float RealDelta) const { return RealDelta * GameMinutesPerRealSecond * 60.0f; }

    // --- РЕЕСТР ЛЮДЕЙ -------------------------------------------------------

    void RegisterHuman(ACompleteHumanNPC* Human);
    void UnregisterHuman(ACompleteHumanNPC* Human);

    /** Все живые зарегистрированные люди. */
    UFUNCTION(BlueprintCallable, Category = "World|People")
    TArray<ACompleteHumanNPC*> GetAllHumans() const;

    /** Люди в радиусе от точки, кроме исключённого. Отсортированы по расстоянию. */
    TArray<ACompleteHumanNPC*> GetHumansNear(const FVector& Origin, float Radius, const AActor* Exclude = nullptr) const;

    /** Ближайший человек к точке. */
    ACompleteHumanNPC* GetNearestHuman(const FVector& Origin, float MaxRadius, const AActor* Exclude = nullptr) const;

    /** Сколько всего людей живо. */
    UFUNCTION(BlueprintCallable, Category = "World|People")
    int32 GetPopulation() const { return Humans.Num(); }

    /** Занять уникальное имя (чтобы в городе не было пяти Игорей Волковых). */
    bool TryClaimName(const FString& FullName);

    // --- МЕСТА ГОРОДА -------------------------------------------------------
    // Объективно существующие места. Человек узнаёт о них, только оказавшись
    // рядом или услышав от другого, — сам по себе список ему недоступен.

    /** Отметить на карте мира место (кафе, работу, парк). */
    UFUNCTION(BlueprintCallable, Category = "World|Places")
    void RegisterPlace(EPlaceKind Kind, const FVector& Location, const FString& Label);

    /** Места в радиусе — то, что человек может заметить, проходя мимо. */
    TArray<FKnownLocation> GetPlacesNear(const FVector& Origin, float Radius) const;

    /** Все места данного типа. */
    TArray<FKnownLocation> GetPlacesOfKind(EPlaceKind Kind) const;

    UPROPERTY(BlueprintReadOnly, Category = "World|Places")
    TArray<FKnownLocation> PublicPlaces;

    // --- АФФОРДАНСЫ ---------------------------------------------------------
    // Всё, что мир предлагает сделать. Источники сами встают на учёт.

    void RegisterAffordanceSource(class UAffordanceComponent* Source);
    void UnregisterAffordanceSource(class UAffordanceComponent* Source);

    /** Собрать всё, что предлагается в радиусе от точки. */
    void GatherAffordancesNear(const FVector& Origin, float Radius, TArray<FAffordance>& Out) const;

    /** Найти источник предложений, стоящий в этой точке. */
    class UAffordanceComponent* FindSourceAt(const FVector& Location, float Tolerance = 600.0f) const;

    /**
     * Собрать всё, что предлагается вокруг точки, — от ВСЕХ источников,
     * а не от первого попавшегося.
     *
     * Дом — это не только подъезд: это и кровать, и раковина, и плита
     * внутри. Раньше запоминался один случайный предмет, и человек помнил
     * о собственном доме, что там «можно полежать», и ничего больше.
     */
    void CollectOffersNear(const FVector& Location, float Radius, TArray<FAffordance>& Out) const;

    /** Открыто ли это сейчас по часам. */
    bool IsAffordanceOpenNow(const FAffordance& A) const;


    // --- СОБЫТИЯ ------------------------------------------------------------

    /** Объявить событие миру: все, кто в радиусе, его воспримут. */
    UFUNCTION(BlueprintCallable, Category = "World|Events")
    void BroadcastEvent(const FWorldEvent& Event);

    /** Последние события — для отладки и для «новостей» в разговорах. */
    UPROPERTY(BlueprintReadOnly, Category = "World|Events")
    TArray<FWorldEvent> RecentEvents;

    /** Сколько событий хранить. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World|Events")
    int32 MaxRecentEvents = 64;

    // --- ОБЩЕСТВЕННАЯ ЖИЗНЬ -------------------------------------------------

    /** Погода: 0 — ясно, 1 — мерзко. Влияет на настроение всего города. */
    UPROPERTY(BlueprintReadOnly, Category = "World|Weather")
    float Weather = 0.2f;

    /** Насколько город вообще опасен 0..1 — фон для тревоги. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World|Weather")
    float AmbientDanger = 0.1f;

    /** Глобальный счётчик — выдаёт уникальные идентификаторы воспоминаниям и целям. */
    int32 NextUniqueId();

private:
    /** Все люди мира (слабые ссылки: умершие/уничтоженные отсеиваются сами). */
    UPROPERTY()
    TArray<TWeakObjectPtr<ACompleteHumanNPC>> Humans;

    /** Всё, что мир предлагает сделать. */
    UPROPERTY()
    TArray<TWeakObjectPtr<class UAffordanceComponent>> AffordanceSources;

    /** Занятые имена. */
    TSet<FString> ClaimedNames;

    /** Счётчик уникальных идентификаторов. */
    int32 IdCounter = 1;

    /** Накопитель для смены погоды. */
    float WeatherTimer = 0.0f;

    /** Пересчитать календарь из WorldSeconds. */
    void RecalculateCalendar();
};
