// EmotionComponent.h
// ---------------------------------------------------------------------------
// Чувства.
//
// Главная идея: эмоция не «прибавляется к переменной». Эмоция РОЖДАЕТСЯ из
// оценки события (appraisal). Одно и то же событие даст разным людям разные
// чувства — потому что они по-разному его оценят.
//
//   «Он меня толкнул»
//     ...это было нарочно? → гнев
//     ...случайно?         → ничего
//     ...а если он сильнее? → страх
//     ...а если я сам виноват? → стыд
//
// Плюс три слоя: темперамент (вечный) → настроение (часы) → эмоция (секунды).
// ---------------------------------------------------------------------------

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HumanTypes.h"
#include "EmotionComponent.generated.h"

class UPersonalityComponent;

UCLASS(ClassGroup = (Human), meta = (BlueprintSpawnableComponent))
class AB_API UEmotionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UEmotionComponent();

    // --- СОСТОЯНИЕ ----------------------------------------------------------

    /** Живые сейчас эмоции. Их может быть несколько — люди чувствуют смешанно. */
    UPROPERTY(BlueprintReadOnly, Category = "Emotion")
    TArray<FEmotionInstance> Active;

    /** Настроение — медленный фон. */
    UPROPERTY(BlueprintReadOnly, Category = "Emotion")
    FMoodState Mood;

    /** Насколько человек вообще показывает чувства наружу 0..1. */
    UPROPERTY(BlueprintReadOnly, Category = "Emotion")
    float Expressiveness = 0.5f;

    /** Сколько подавленного накопилось — однажды прорвётся. */
    UPROPERTY(BlueprintReadOnly, Category = "Emotion")
    float SuppressedLoad = 0.0f;

    /** Последняя применённая стратегия совладания (для мыслей и отладки). */
    UPROPERTY(BlueprintReadOnly, Category = "Emotion")
    ECopingStyle LastCoping = ECopingStyle::ProblemFocused;

    /**
     * Характер этого человека. Проставляется в Setup и участвует ВО ВСЕХ
     * рождающихся чувствах — включая те, что зажигаются напрямую.
     * Без этого невротик и флегматик переживали бы одинаково, а они не должны.
     */
    UPROPERTY(BlueprintReadOnly, Category = "Emotion")
    TObjectPtr<const UPersonalityComponent> LinkedPersonality = nullptr;

    // --- ЖИЗНЕННЫЙ ЦИКЛ -----------------------------------------------------

    /** Привязать темперамент к личности. */
    void Setup(const UPersonalityComponent* Personality);

    /** Шаг: затухание эмоций, дрейф настроения, стресс, оцепенение. */
    void Advance(float GameDelta, float BodilyDistress, float CurrentWorldTime);

    // --- ГЛАВНОЕ: ОЦЕНКА СОБЫТИЯ -------------------------------------------

    /**
     * Оценить событие и породить из него эмоции.
     * Возвращает самую сильную из порождённых (None, если ничего не задело).
     */
    EEmotionType Appraise(const FAppraisedEvent& Event, const UPersonalityComponent* Personality);

    /** Зажечь эмоцию напрямую (когда оценка уже проведена где-то ещё). */
    void Trigger(EEmotionType Type, float Intensity, AActor* Cause = nullptr, const FString& Reason = FString());

    /** Приглушить конкретное чувство (развлечение гасит скуку, объятие — страх). */
    void Soothe(EEmotionType Type, float Amount);

    // --- РЕГУЛЯЦИЯ ----------------------------------------------------------

    /**
     * Попытаться справиться с чувствами.
     * Effort — сколько воли вложено 0..1. Разные стратегии стоят по-разному
     * и по-разному аукаются.
     */
    void Regulate(ECopingStyle Style, float Effort, const UPersonalityComponent* Personality);

    /** Заразиться чужим состоянием. Closeness 0..1 — насколько человек «свой». */
    void CatchFrom(const UEmotionComponent* Other, float Empathy, float Closeness);

    // --- ЗАПРОСЫ ------------------------------------------------------------

    /** Сила конкретной эмоции 0..1 (то, что человек ЧУВСТВУЕТ). */
    UFUNCTION(BlueprintCallable, Category = "Emotion")
    float GetIntensity(EEmotionType Type) const;

    /** Сила, которая ВИДНА снаружи (с учётом сдержанности). */
    UFUNCTION(BlueprintCallable, Category = "Emotion")
    float GetExpressedIntensity(EEmotionType Type) const;

    /** Самая сильная эмоция. */
    UFUNCTION(BlueprintCallable, Category = "Emotion")
    EEmotionType GetDominant() const;

    /** Самая сильная эмоция и её сила. */
    void GetDominantWithIntensity(EEmotionType& OutType, float& OutIntensity) const;

    /** Итоговое аффективное состояние: настроение + все активные эмоции. */
    UFUNCTION(BlueprintCallable, Category = "Emotion")
    FAffectPAD GetAffect() const;

    /** Текущий стресс 0..1. */
    UFUNCTION(BlueprintCallable, Category = "Emotion")
    float GetStress() const { return Mood.StressLoad; }

    /** Общий эмоциональный накал 0..1 — насколько «штормит». */
    UFUNCTION(BlueprintCallable, Category = "Emotion")
    float GetTurmoil() const;

    /** Словами: «мне тревожно и немного стыдно». */
    UFUNCTION(BlueprintCallable, Category = "Emotion")
    FString DescribeFeeling() const;

    /** Куда сместилось настроение относительно нормы: -1..+1. */
    UFUNCTION(BlueprintCallable, Category = "Emotion")
    float GetMoodShift() const;

    /** Положение эмоции в пространстве PAD — таблица «что значит чувствовать X». */
    static FAffectPAD EmotionToPAD(EEmotionType Type);

    /** Насколько быстро эта эмоция гаснет сама по себе (в секунду). */
    static float BaseDecayRate(EEmotionType Type);

    /** Приятная ли эмоция. */
    static bool IsPositive(EEmotionType Type);

private:
    /** Добавить эмоцию с учётом личности; уже горящая — усиливается. */
    void Ignite(EEmotionType Type, float Intensity, AActor* Cause, const FString& Reason, const UPersonalityComponent* Personality);

    /** Насколько личность усиливает или глушит данную эмоцию. */
    float PersonalityGain(EEmotionType Type, const UPersonalityComponent* Personality) const;

    /** Текущее игровое время (обновляется в Advance). */
    float WorldTime = 0.0f;

    /** Порог, ниже которого эмоция считается погасшей. */
    static constexpr float ExtinctionThreshold = 0.02f;
};
