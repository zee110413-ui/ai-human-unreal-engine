// MotivationComponent.h
// ---------------------------------------------------------------------------
// Хотение.
//
// Цепочка: потребность → желание → цель → план → шаг.
// На каждом стыке всё может сломаться, и в этом главное: планы проваливаются,
// шаги не выходят, цели бросают на полпути, а вместо трудного дела человек
// с чистой совестью идёт делать лёгкое.
//
// Воля — исчерпаемый ресурс. Уставший человек не «менее эффективен» — он
// принимает другие решения: импульсивные, сиюминутные, о которых пожалеет.
// ---------------------------------------------------------------------------

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HumanTypes.h"
#include "MotivationComponent.generated.h"

class UPersonalityComponent;
class UNeedComponent;

/** Что разум знает о мире в момент планирования. */
USTRUCT(BlueprintType)
struct FPlanningContext
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Plan") FVector SelfLocation = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "Plan") bool bHasHome = false;
    UPROPERTY(BlueprintReadWrite, Category = "Plan") FVector HomeLocation = FVector::ZeroVector;
    UPROPERTY(BlueprintReadWrite, Category = "Plan") bool bAtHome = false;

    UPROPERTY(BlueprintReadWrite, Category = "Plan") bool bKnowsFood = false;
    UPROPERTY(BlueprintReadWrite, Category = "Plan") FVector FoodLocation = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "Plan") bool bKnowsWork = false;
    UPROPERTY(BlueprintReadWrite, Category = "Plan") FVector WorkLocation = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "Plan") bool bKnowsBeauty = false;
    UPROPERTY(BlueprintReadWrite, Category = "Plan") FVector BeautyLocation = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "Plan") bool bKnowsSafePlace = false;
    UPROPERTY(BlueprintReadWrite, Category = "Plan") FVector SafeLocation = FVector::ZeroVector;

    /** Ближайший человек вообще. */
    UPROPERTY(BlueprintReadWrite, Category = "Plan") TObjectPtr<AActor> NearestPerson = nullptr;
    /** Самый близкий человек в поле зрения. */
    UPROPERTY(BlueprintReadWrite, Category = "Plan") TObjectPtr<AActor> NearbyFriend = nullptr;
    /** Тот, кому сейчас плохо (можно помочь). */
    UPROPERTY(BlueprintReadWrite, Category = "Plan") TObjectPtr<AActor> SomeoneInNeed = nullptr;
    /** Тот, кто меня пугает. */
    UPROPERTY(BlueprintReadWrite, Category = "Plan") TObjectPtr<AActor> Threat = nullptr;

    UPROPERTY(BlueprintReadWrite, Category = "Plan") float Money = 0.0f;
    UPROPERTY(BlueprintReadWrite, Category = "Plan") bool bEmployed = false;
    UPROPERTY(BlueprintReadWrite, Category = "Plan") bool bWorkingHours = false;
    UPROPERTY(BlueprintReadWrite, Category = "Plan") bool bNight = false;
    /** Умение, в котором человек хочет расти. */
    UPROPERTY(BlueprintReadWrite, Category = "Plan") FName FocusSkill;
};

UCLASS(ClassGroup = (Human), meta = (BlueprintSpawnableComponent))
class AB_API UMotivationComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UMotivationComponent();

    // --- СОСТОЯНИЕ ----------------------------------------------------------

    UPROPERTY(BlueprintReadOnly, Category = "Motivation")
    TArray<FGoal> Goals;

    /** Цель, которой человек занят прямо сейчас. -1, если ничем. */
    UPROPERTY(BlueprintReadOnly, Category = "Motivation")
    int32 ActiveGoalId = -1;

    /** Сила воли 0..1 — исчерпаемый ресурс. */
    UPROPERTY(BlueprintReadOnly, Category = "Motivation")
    float Willpower = 1.0f;

    /** Потолок воли для этого человека. */
    UPROPERTY(BlueprintReadOnly, Category = "Motivation")
    float WillpowerCapacity = 1.0f;

    /** Сколько раз подряд человек откладывал трудное. */
    UPROPERTY(BlueprintReadOnly, Category = "Motivation")
    int32 ProcrastinationStreak = 0;

    /** Последнее объяснение выбора — для потока сознания. */
    UPROPERTY(BlueprintReadOnly, Category = "Motivation")
    FString LastDecisionReason;

    /**
     * Насколько начатое дело защищено от пересмотра.
     * Разум сбрасывает это в 1.0, когда телу становится по-настоящему плохо:
     * никакая увлечённость не должна пересиливать жажду.
     */
    UPROPERTY(BlueprintReadWrite, Category = "Motivation")
    float CommitmentMultiplier = 1.45f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motivation")
    int32 GoalCapacity = 14;

    // --- ЖИЗНЕННЫЙ ЦИКЛ -----------------------------------------------------

    void Setup(const UPersonalityComponent* Personality);

    /** Шаг: восстановление воли, пересчёт срочности, дедлайны, разочарование. */
    void Advance(float GameDelta, float WorldTime, bool bResting, const UPersonalityComponent* Personality);

    // --- ЦЕЛИ ---------------------------------------------------------------

    /** Завести цель. Возвращает Id. */
    int32 CreateGoal(const FString& Name, ENeedType Need, EGoalHorizon Horizon,
                     float Importance, float WorldTime, float Deadline = -1.0f);

    /**
     * Замыслы рождаются не из сиюминутной нехватки, а из ХРОНИЧЕСКОЙ.
     *
     * Голод не делает человека целеустремлённым — он просто идёт есть.
     * А вот многодневная нехватка уважения, близости или смысла однажды
     * складывается в намерение: «я хочу это изменить». Это и есть цель.
     *
     * Поэтому здесь смотрят не на текущее значение потребности,
     * а на то, сколько времени её не хватает.
     */
    void FormLongTermAims(const UNeedComponent* Needs, const UPersonalityComponent* Personality, float WorldTime);

    /** Замысел продвинулся: что-то из сделанного работало на него. */
    void RegisterProgress(ENeedType Need, float Amount, float WorldTime);

    /** Поставить жизненную цель (мечту). */
    int32 SetLifeGoal(const FString& Name, ENeedType Need, float WorldTime);

    /**
     * Завести цель с уже готовым планом.
     * Нужно для поступков, которые не выводятся из потребности, а возникают
     * по случаю: подвернулся человек, которому плохо; попался на глаза тот,
     * кого не простил.
     */
    int32 CreateGoalWithPlan(const FString& Name, ENeedType Need, EGoalHorizon Horizon,
                             float Importance, const TArray<FPlanStep>& Steps, float WorldTime);

    FGoal* FindGoal(int32 Id);
    const FGoal* FindGoal(int32 Id) const;
    FGoal* GetActiveGoal();

    void AchieveGoal(int32 Id, float WorldTime);
    void FailGoal(int32 Id, float WorldTime, const UPersonalityComponent* Personality);
    void AbandonGoal(int32 Id, float WorldTime);

    // --- ВОЛЯ ---------------------------------------------------------------

    /** Потратить волю. Возвращает false, если не хватило — и тогда человек сдался. */
    bool SpendWillpower(float Amount);

    /** Насколько человек сейчас способен на усилие 0..1. */
    UFUNCTION(BlueprintCallable, Category = "Motivation")
    float GetSelfControl() const;

    /** Восстановить волю (сон, отдых, радость). */
    void RestoreWillpower(float Amount);

    // --- ЗАПРОСЫ ------------------------------------------------------------

    /** Насколько замысел сейчас жив — как его ощущает сам человек. */
    float EvaluateGoal(const FGoal& Goal, const UPersonalityComponent* Personality, float Stress) const;

    /** Пересмотреть замыслы: брошенные, безнадёжные, достигнутые. */
    void ReviewAims(float WorldTime, const UPersonalityComponent* Personality);

    /** Есть ли невыполненное обещание самому себе (источник вины). */
    UFUNCTION(BlueprintCallable, Category = "Motivation")
    bool HasNaggingGoal() const;

    /** Словами: чем человек сейчас занят. */
    UFUNCTION(BlueprintCallable, Category = "Motivation")
    FString DescribeIntent() const;

    /** Сколько целей провалено подряд — путь к беспомощности. */
    UPROPERTY(BlueprintReadOnly, Category = "Motivation")
    int32 ConsecutiveFailures = 0;

private:
    /** Убрать завершённые и протухшие цели. */
    void PruneGoals(float WorldTime);

    int32 NextGoalId = 1;
    float RegenAccumulator = 0.0f;
};
