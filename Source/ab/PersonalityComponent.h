// PersonalityComponent.h
// ---------------------------------------------------------------------------
// Личность: то, что остаётся, когда меняются настроение и обстоятельства.
// Черты почти постоянны, но НЕ вечны: жизнь их медленно точит.
// Травма поднимает нейротизм, успех — экстраверсию, возраст — добросовестность.
// ---------------------------------------------------------------------------

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HumanTypes.h"
#include "PersonalityComponent.generated.h"

UCLASS(ClassGroup = (Human), meta = (BlueprintSpawnableComponent))
class AB_API UPersonalityComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UPersonalityComponent();

    // --- ЧЕРТЫ --------------------------------------------------------------

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Personality")
    FBigFive Traits;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Personality")
    FPersonalityFacets Facets;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Personality")
    FValueSystem Values;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Personality")
    FMoralFoundations Morals;

    /** Врождённый эмоциональный «нуль», к которому всегда возвращается настроение. */
    UPROPERTY(BlueprintReadOnly, Category = "Personality")
    FAffectPAD TemperamentBaseline;

    /** Насколько сильно этот человек вообще способен меняться 0..1. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Personality")
    float Plasticity = 0.5f;

    // --- СОЗДАНИЕ -----------------------------------------------------------

    /** Сгенерировать личность. Черты не независимы — они коррелируют, как у людей. */
    UFUNCTION(BlueprintCallable, Category = "Personality")
    void GenerateRandom();

    /** Пересчитать фасеты, ценности и темперамент из Большой пятёрки. */
    UFUNCTION(BlueprintCallable, Category = "Personality")
    void DeriveFromTraits();

    // --- ИЗМЕНЕНИЕ ----------------------------------------------------------

    /**
     * Сдвинуть черту под влиянием пережитого.
     * Amount — желаемый сдвиг; реально применится лишь часть, зависящая от
     * пластичности и возраста: в двадцать лет человек меняется, в семьдесят — нет.
     */
    void NudgeTrait(FName TraitName, float Amount, float Age);

    /** Естественное взросление: раз в игровой год характер чуть «созревает». */
    void ApplyMaturation(float Age);

    /** Пережитая травма: поднимает тревожность, сбивает доверие. */
    void ApplyTrauma(float Severity, float Age);

    /** Пережитый успех: поднимает уверенность и открытость. */
    void ApplyTriumph(float Magnitude, float Age);

    // --- ВЫВОДЫ -------------------------------------------------------------

    /** Предпочитаемый способ справляться со стрессом при данном уровне стресса. */
    ECopingStyle PreferredCoping(float StressLevel) const;
    void CopingLeanings(float StressLevel, float Out[7]) const;

    /**
     * Насколько тяжело мне далось бы такое действие с моральной точки зрения.
     * Возвращает 0..1: 0 — ничего страшного, 1 — переступить через себя.
     * HarmToOthers/Unfairness/Betrayal — характеристики поступка, 0..1.
     */
    float MoralCost(float HarmToOthers, float Unfairness, float Betrayal) const;

    /** Насколько я склонен рискнуть ради выигрыша Reward при вероятности успеха P. */
    float RiskAppetite(float Reward, float SuccessChance) const;

    /** Базовая сила воли (без учёта усталости) 0..1. */
    float BaseWillpower() const;

    /** Насколько я терпелив к отложенному вознаграждению 0..1. */
    float Patience() const;

    /** Насколько мне нужно общение 0..1 (интроверт «наедается» быстро). */
    float SocialAppetite() const;

    /** Короткая словесная характеристика — для отладки и для самоописания. */
    UFUNCTION(BlueprintCallable, Category = "Personality")
    FString DescribeSelf() const;

    /** Одно-два определяющих слова: «вспыльчивый», «замкнутый». */
    UFUNCTION(BlueprintCallable, Category = "Personality")
    FString DominantTraitWord() const;

private:
    /** Ограничить все значения диапазоном 0..1. */
    void ClampAll();
};
