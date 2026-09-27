// PhysiologyComponent.h
// ---------------------------------------------------------------------------
// Тело. Не «декорация к эмоциям», а самостоятельная система с собственной
// инерцией: желудок пустеет, мочевой пузырь наполняется, долг сна копится,
// кортизол подтачивает иммунитет, а иммунитет — здоровье.
// Психика на тело влияет, но тело психике не подчиняется.
// ---------------------------------------------------------------------------

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HumanTypes.h"
#include "PhysiologyComponent.generated.h"

class ACompleteHumanNPC;

/** То, что психика и обстоятельства сообщают телу на очередном шаге. */
USTRUCT(BlueprintType)
struct FPhysiologyDrive
{
    GENERATED_BODY()

    /** Час суток 0..24 — задаёт циркадный ритм. */
    UPROPERTY(BlueprintReadWrite, Category = "Physiology") float HourOfDay = 12.0f;
    /** Эмоциональное возбуждение -1..+1. */
    UPROPERTY(BlueprintReadWrite, Category = "Physiology") float Arousal = 0.0f;
    /** Эмоциональная приятность -1..+1. */
    UPROPERTY(BlueprintReadWrite, Category = "Physiology") float Valence = 0.0f;
    /** Хронический стресс 0..1. */
    UPROPERTY(BlueprintReadWrite, Category = "Physiology") float StressLoad = 0.0f;
    /** Текущая физическая нагрузка 0..1. */
    UPROPERTY(BlueprintReadWrite, Category = "Physiology") float Exertion = 0.0f;
    /** Возраст в годах. */
    UPROPERTY(BlueprintReadWrite, Category = "Physiology") float Age = 30.0f;
    /** Психологическая устойчивость 0..1 (из личности) — смягчает кортизол. */
    UPROPERTY(BlueprintReadWrite, Category = "Physiology") float Resilience = 0.5f;
    /** Спит ли сейчас. */
    UPROPERTY(BlueprintReadWrite, Category = "Physiology") bool bAsleep = false;
    UPROPERTY(BlueprintReadWrite, Category = "Physiology") ESleepPhase SleepPhase = ESleepPhase::Awake;
    /** Тепло недавних человеческих контактов 0..1 — кормит окситоцин. */
    UPROPERTY(BlueprintReadWrite, Category = "Physiology") float SocialWarmth = 0.0f;
    /** Сигнал награды 0..1 — только что получил желаемое. */
    UPROPERTY(BlueprintReadWrite, Category = "Physiology") float RewardSignal = 0.0f;
    /** Насколько вокруг холодно/неуютно 0..1. */
    UPROPERTY(BlueprintReadWrite, Category = "Physiology") float EnvironmentHarshness = 0.0f;
};

UCLASS(ClassGroup = (Human), meta = (BlueprintSpawnableComponent))
class AB_API UPhysiologyComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UPhysiologyComponent();

    // --- СОСТОЯНИЕ ----------------------------------------------------------

    UPROPERTY(BlueprintReadOnly, Category = "Physiology")
    FBodyState Body;

    UPROPERTY(BlueprintReadOnly, Category = "Physiology")
    FHormones Hormones;

    UPROPERTY(BlueprintReadOnly, Category = "Physiology")
    FOrgans Organs;

    /** Сводка по телу 0..1: худший из органов и мера лишений. */
    float Condition() const;

    /** Что именно с телом не так — словами. */
    FString Diagnosis() const;

    /** Активные недомогания. */
    UPROPERTY(BlueprintReadOnly, Category = "Physiology")
    TArray<FAilmentInstance> Ailments;

    /** Биологический возраст: от стресса и образа жизни стареют быстрее паспорта. */
    UPROPERTY(BlueprintReadOnly, Category = "Physiology")
    float BiologicalAge = 30.0f;

    /** Жив ли. */
    UPROPERTY(BlueprintReadOnly, Category = "Physiology")
    bool bAlive = true;

    /** Отчего умер (для эпитафии). */
    UPROPERTY(BlueprintReadOnly, Category = "Physiology")
    FString CauseOfDeath;

    // --- СОВМЕСТИМОСТЬ СО СТАРЫМ API ---------------------------------------
    // Эти поля — зеркало Body, оставлены, чтобы не ломать существующие связи.

    UPROPERTY(BlueprintReadOnly, Category = "Physiology|Legacy") float HeartRate = 70.0f;
    UPROPERTY(BlueprintReadOnly, Category = "Physiology|Legacy") float BloodPressureSystolic = 120.0f;
    UPROPERTY(BlueprintReadOnly, Category = "Physiology|Legacy") float BloodPressureDiastolic = 80.0f;
    UPROPERTY(BlueprintReadOnly, Category = "Physiology|Legacy") float RespiratoryRate = 16.0f;
    UPROPERTY(BlueprintReadOnly, Category = "Physiology|Legacy") float OxygenSaturation = 98.0f;
    UPROPERTY(BlueprintReadOnly, Category = "Physiology|Legacy") float BodyTemperature = 36.6f;
    UPROPERTY(BlueprintReadOnly, Category = "Physiology|Legacy") float BloodVolume = 5.0f;
    UPROPERTY(BlueprintReadOnly, Category = "Physiology|Legacy") float GlucoseLevel = 5.0f;
    UPROPERTY(BlueprintReadOnly, Category = "Physiology|Legacy") float AdrenalineLevel = 0.1f;
    UPROPERTY(BlueprintReadOnly, Category = "Physiology|Legacy") float CortisolLevel = 0.2f;

    // --- ГЛАВНЫЙ ШАГ --------------------------------------------------------

    /** Продвинуть тело на GameDelta игровых секунд. */
    void Advance(float GameDelta, const FPhysiologyDrive& Drive);

    /** Совместимость: старый вход, используемый прежним кодом. */
    void UpdatePhysiology(float DeltaSeconds, ACompleteHumanNPC* Owner);

    // --- ДЕЙСТВИЯ НАД ТЕЛОМ -------------------------------------------------

    /** Поесть. Nutrition 0..1 — сытность порции. */
    void Eat(float Nutrition);
    /** Попить. */
    void Drink(float Amount);
    /** Сходить в туалет. */
    void Relieve();
    /** Помыться. */
    void WashUp();
    /** Физическая нагрузка в течение GameDelta. */
    void Exercise(float GameDelta, float Intensity);
    /**
     * Принять телом то, что было получено.
     * Это физика организма, а не правило поведения: еда наполняет желудок,
     * вода — ткани, отдых снимает усталость. Разум этой таблицей не
     * пользуется — он только раздаёт то, что мир пообещал.
     * Возвращает false, если такая потребность к телу отношения не имеет.
     */
    bool ApplyNeedEffect(ENeedType Need, float Amount, float GameDelta);

    /** Получить травму. */
    void TakeInjury(float Severity, const FString& Cause);
    /** Заболеть. */
    void ContractAilment(EAilment Type, float Severity, float DurationGameSeconds);
    /** Есть ли такое недомогание. */
    bool HasAilment(EAilment Type) const;
    /** Тяжесть недомогания (0, если его нет). */
    float AilmentSeverity(EAilment Type) const;
    /** Вылечиться. */
    void CureAilment(EAilment Type);

    // --- ЗАПРОСЫ ------------------------------------------------------------

    /** Давление сна 0..1: комбинация долга сна и циркадного ритма. */
    float GetSleepPressure(float HourOfDay) const;
    /** Множитель скорости передвижения 0.3..1.2. */
    float GetMovementSpeedMultiplier() const;
    /** Насколько тело мешает думать 0..1 (боль, жар, истощение). */
    float GetCognitiveImpairment() const;
    /** Насколько телу сейчас плохо 0..1 — общий «телесный дискомфорт». */
    float GetBodilyDistress() const;
    float GetSatiety() const;
    /** Короткое описание самочувствия: «ломит голову», «хочется лечь». */
    UFUNCTION(BlueprintCallable, Category = "Physiology")
    FString DescribeFeeling() const;

    /** Умереть. */
    void Die(const FString& Reason);

    void Revive();

private:
    /** Циркадная кривая сонливости для часа суток 0..1. */
    static float CircadianSleepiness(float HourOfDay);
    /** Обновить производные сердечно-сосудистые показатели. */
    void UpdateVitals(const FPhysiologyDrive& Drive);
    /** Обновить гормоны. */
    void UpdateHormones(float GameDelta, const FPhysiologyDrive& Drive);
    /** Течение болезней. */
    void UpdateAilments(float GameDelta, const FPhysiologyDrive& Drive);
    /** Старение и износ. */
    void UpdateAging(float GameDelta, const FPhysiologyDrive& Drive);
    /** Скопировать состояние в legacy-поля. */
    void MirrorLegacyFields();

    /** Накопитель для редких проверок (болезни, старение). */
    float SlowTimer = 0.0f;
};
