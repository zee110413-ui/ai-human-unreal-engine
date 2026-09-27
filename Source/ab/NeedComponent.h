// NeedComponent.h
// ---------------------------------------------------------------------------
// Потребности — источник всех желаний.
// Телесные читаются прямо из тела, психологические живут своей жизнью:
// общение «наедается» и снова пустеет, смысл требует не еды, а поступков.
//
// Важно: иерархии Маслоу в жёстком виде нет. Голодный человек может бросить
// всё ради уважения, а сытый — страдать от бессмысленности. Веса решают всё.
// ---------------------------------------------------------------------------

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HumanTypes.h"
#include "NeedComponent.generated.h"

class UPhysiologyComponent;
class UPersonalityComponent;

/** Внешние обстоятельства, влияющие на психологические потребности. */
USTRUCT(BlueprintType)
struct FNeedContext
{
    GENERATED_BODY()

    /** Есть ли дом и в нём ли я сейчас. */
    UPROPERTY(BlueprintReadWrite, Category = "Needs") bool bHasHome = false;
    UPROPERTY(BlueprintReadWrite, Category = "Needs") bool bAtHome = false;
    /** Сколько людей рядом. */
    UPROPERTY(BlueprintReadWrite, Category = "Needs") int32 PeopleNearby = 0;
    /** Насколько близкие люди рядом 0..1. */
    UPROPERTY(BlueprintReadWrite, Category = "Needs") float ClosenessNearby = 0.0f;
    /** Воспринимаемая опасность 0..1. */
    UPROPERTY(BlueprintReadWrite, Category = "Needs") float PerceivedDanger = 0.0f;
    /** Деньги в кармане. */
    UPROPERTY(BlueprintReadWrite, Category = "Needs") float Money = 0.0f;
    /** Есть ли работа. */
    UPROPERTY(BlueprintReadWrite, Category = "Needs") bool bEmployed = false;
    /** Насколько предсказуем день 0..1 (рутина успокаивает). */
    UPROPERTY(BlueprintReadWrite, Category = "Needs") float RoutinePredictability = 0.5f;
    /** Спит ли — во сне потребности почти замирают. */
    UPROPERTY(BlueprintReadWrite, Category = "Needs") bool bAsleep = false;
    /** Красота окружения 0..1. */
    UPROPERTY(BlueprintReadWrite, Category = "Needs") float SurroundingBeauty = 0.3f;
    /** Час суток 0..24 — от него зависит, насколько давит сон. */
    UPROPERTY(BlueprintReadWrite, Category = "Needs") float HourOfDay = 12.0f;
    UPROPERTY(BlueprintReadWrite, Category = "Needs") float Provision = 1.0f;
};

UCLASS(ClassGroup = (Human), meta = (BlueprintSpawnableComponent))
class AB_API UNeedComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UNeedComponent();

    /** Все потребности. Индекс = значение ENeedType. */
    UPROPERTY(BlueprintReadOnly, Category = "Needs")
    TArray<FNeedState> Needs;

    /** Сколько игровых секунд потребность держится ниже порога. Копит отчаяние. */
    UPROPERTY(BlueprintReadOnly, Category = "Needs")
    TArray<float> DeprivationTime;

    /** Настроить веса потребностей под конкретную личность. */
    void Setup(const UPersonalityComponent* Personality);

    /** Шаг. */
    void Advance(float GameDelta, const UPhysiologyComponent* Physiology, const FNeedContext& Context);
    void ReadBody(const UPhysiologyComponent* Physiology, float HourOfDay);

    // --- Доступ -------------------------------------------------------------

    /** Удовлетворённость 0..1. */
    UFUNCTION(BlueprintCallable, Category = "Needs")
    float GetSatisfaction(ENeedType Type) const;

    /** Насколько эта потребность сейчас кричит 0..1 (с учётом веса и депривации). */
    UFUNCTION(BlueprintCallable, Category = "Needs")
    float GetUrgency(ENeedType Type) const;

    /**
     * Какой эта потребность станет через столько-то игровых секунд.
     *
     * Живое существо действует не по тому, что с ним сейчас, а по тому,
     * что с ним будет. Сытый идёт добывать еду, потому что знает: через
     * три часа он будет голоден, а к тому времени будет поздно.
     * Без этого человек ждал, пока приспичит, и только тогда спохватывался.
     */
    UFUNCTION(BlueprintCallable, Category = "Needs")
    float ProjectSatisfaction(ENeedType Type, float SecondsAhead) const;

    /** Насколько эта потребность будет кричать через столько-то секунд. */
    UFUNCTION(BlueprintCallable, Category = "Needs")
    float ProjectUrgency(ENeedType Type, float SecondsAhead) const;

    /** Самая громкая потребность. */
    UFUNCTION(BlueprintCallable, Category = "Needs")
    ENeedType GetMostUrgent() const;

    /** Несколько самых громких, по убыванию. */
    TArray<ENeedType> GetTopUrgent(int32 Count) const;

    /** Удовлетворить потребность. */
    UFUNCTION(BlueprintCallable, Category = "Needs")
    void Satisfy(ENeedType Type, float Amount);

    /** Отнять удовлетворённость (например, унизили — падает Esteem). */
    UFUNCTION(BlueprintCallable, Category = "Needs")
    void Deprive(ENeedType Type, float Amount);

    /** Суммарное «мне плохо» от всех неудовлетворённых потребностей 0..1. */
    UFUNCTION(BlueprintCallable, Category = "Needs")
    float GetTotalDistress() const;

    /** Сколько игровых секунд эта потребность в депривации. */
    float GetDeprivationTime(ENeedType Type) const;

    /** Изменить личную важность потребности (жизнь меняет приоритеты). */
    void SetWeight(ENeedType Type, float Weight);

    /** Ссылка на состояние (может вернуть nullptr). */
    FNeedState* Find(ENeedType Type);
    const FNeedState* Find(ENeedType Type) const;

private:
    void BuildDefaults();

    /** Текущее игровое время — обновляется в Advance. */
    float CurrentTime = 0.0f;
};
