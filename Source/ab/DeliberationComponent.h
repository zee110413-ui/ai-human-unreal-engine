// DeliberationComponent.h
// ---------------------------------------------------------------------------
// Как рождается решение.
//
// Здесь нет ни одного правила вида «если голоден — иди есть».
// Есть только оценка: мир предлагает набор возможностей, и каждая из них
// взвешивается через ТЕКУЩЕЕ внутреннее состояние человека:
//
//   что мне это даст      ← моя нехватка прямо сейчас
//   чем это кончалось     ← мой личный выученный опыт, а не таблица
//   чего это будет стоить ← моя усталость, моя воля, мои деньги
//   насколько это страшно ← мой страх и мой характер
//   стоит ли рисковать    ← моя тяга к новому против моей тревоги
//   к чему я вообще иду   ← мои долгосрочные замыслы
//
// Итог — не argmax. Из достойных вариантов один выбирается случайно, с
// вероятностью по весу. Поэтому один и тот же человек в одном и том же
// положении может поступить по-разному, и предсказать конкретный случай
// нельзя, даже зная о нём всё.
//
// Чтобы человек начал делать что-то новое, код этого файла трогать не надо.
// Достаточно, чтобы в мире появилась новая возможность.
// ---------------------------------------------------------------------------

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HumanTypes.h"
#include "DeliberationComponent.generated.h"

class UNeedComponent;
class UMemoryComponent;
class UPersonalityComponent;
class UEmotionComponent;
class UMotivationComponent;
class UMindComponent;
class UIdentityComponent;
class USocialComponent;
class UPhysiologyComponent;

/** Всё, что нужно знать о себе, чтобы взвесить возможность. */
struct FDeliberationContext
{
    UNeedComponent*        Needs = nullptr;
    UMemoryComponent*      Memory = nullptr;
    UPersonalityComponent* Personality = nullptr;
    UEmotionComponent*     Emotions = nullptr;
    UMotivationComponent*  Motivation = nullptr;
    UMindComponent*        Mind = nullptr;
    UIdentityComponent*    Identity = nullptr;
    USocialComponent*      Social = nullptr;
    UPhysiologyComponent*  Body = nullptr;

    FVector SelfLocation = FVector::ZeroVector;
    float   WorldTime = 0.0f;
    float   HourOfDay = 12.0f;
    float   Money = 0.0f;
    /** Насколько мутно в голове 0..1 — делает выбор более случайным. */
    float   Impairment = 0.0f;
    /** Реальная скорость ходьбы (см/с) — чтобы считать цену дороги честно. */
    float   WalkSpeed = 330.0f;

    /**
     * Какие материалы мне нужны под то, что я умею делать (вес = насколько).
     * Заполняется разумом из вычитанных ремёсел: знаю, как варят вино, —
     * и работа, дающая виноград, становится привлекательнее.
     */
    TMap<EResourceKind, float> WantedInputs;
};

/** Что именно мешает взяться за дело прямо сейчас. */
UENUM(BlueprintType)
enum class EBlocker : uint8
{
    None      UMETA(DisplayName = "None"),
    Money     UMETA(DisplayName = "Money"),      // не по карману
    Stamina   UMETA(DisplayName = "Stamina"),    // сил не дойти
    Skill     UMETA(DisplayName = "Skill"),      // не умею
    Closed    UMETA(DisplayName = "Closed")      // сейчас закрыто
};

/** Разбор одной возможности — из чего сложилась её привлекательность. */
USTRUCT(BlueprintType)
struct FValuation
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Decision") float NeedGain = 0.0f;      // что даст
    UPROPERTY(BlueprintReadOnly, Category = "Decision") float Experience = 0.0f;    // чем кончалось
    UPROPERTY(BlueprintReadOnly, Category = "Decision") float Curiosity = 0.0f;     // а вдруг
    UPROPERTY(BlueprintReadOnly, Category = "Decision") float TravelCost = 0.0f;    // далеко
    UPROPERTY(BlueprintReadOnly, Category = "Decision") float EffortCost = 0.0f;    // лень
    UPROPERTY(BlueprintReadOnly, Category = "Decision") float RiskCost = 0.0f;      // страшно
    UPROPERTY(BlueprintReadOnly, Category = "Decision") float MoneyCost = 0.0f;     // дорого
    UPROPERTY(BlueprintReadOnly, Category = "Decision") float SocialPull = 0.0f;    // к людям
    UPROPERTY(BlueprintReadOnly, Category = "Decision") float GoalPull = 0.0f;      // к своему
    UPROPERTY(BlueprintReadOnly, Category = "Decision") float HabitPull = 0.0f;     // по привычке
    UPROPERTY(BlueprintReadOnly, Category = "Decision") float MoodShift = 0.0f;     // настроение
    UPROPERTY(BlueprintReadOnly, Category = "Decision") float Taste = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category = "Decision") float Total = 0.0f;

    /** Главная причина, по которой это вообще рассматривалось. */
    UPROPERTY(BlueprintReadOnly, Category = "Decision") FString DominantReason;

    /** Что мешает взяться прямо сейчас. */
    UPROPERTY(BlueprintReadOnly, Category = "Decision") EBlocker Blocker = EBlocker::None;

    /**
     * Чего бы это стоило, если бы помеха исчезла.
     * Именно ради этого числа человек и затевает что-то заранее:
     * «в кафе хорошо, но нужны деньги» — значит, идём зарабатывать.
     */
    UPROPERTY(BlueprintReadOnly, Category = "Decision") float PotentialValue = 0.0f;
};

/** Принятое намерение. */
USTRUCT(BlueprintType)
struct FIntention
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Decision") bool bValid = false;
    UPROPERTY(BlueprintReadOnly, Category = "Decision") FAffordance Affordance;
    UPROPERTY(BlueprintReadOnly, Category = "Decision") FValuation Valuation;
    /** Чего человек ждёт от этого: -1..+1. С этим сравнится действительность. */
    UPROPERTY(BlueprintReadOnly, Category = "Decision") float Expectation = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category = "Decision") float DecidedAt = 0.0f;
    /** Как человек объяснил бы свой выбор. */
    UPROPERTY(BlueprintReadOnly, Category = "Decision") FString Reason;

    /**
     * Ради чего это делается, если делается не ради себя.
     * Пусто, когда человек просто хочет то, что делает.
     */
    UPROPERTY(BlueprintReadOnly, Category = "Decision") FString ServesGoal;
};

UCLASS(ClassGroup = (Human), meta = (BlueprintSpawnableComponent))
class AB_API UDeliberationComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UDeliberationComponent();

    /** То, что человек сейчас намерен делать. */
    UPROPERTY(BlueprintReadOnly, Category = "Decision")
    FIntention Current;

    /** Что рассматривалось в последний раз — для отладки и самоанализа. */
    UPROPERTY(BlueprintReadOnly, Category = "Decision")
    TArray<FValuation> LastConsidered;

    UPROPERTY(BlueprintReadOnly, Category = "Decision")
    TArray<FString> LastConsideredLabels;

    /** Сколько возможностей человек вообще способен удержать в голове. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Decision")
    int32 ConsiderationLimit = 14;

    /**
     * Взвесить всё предложенное и выбрать.
     * Возвращает false, если ничего не показалось стоящим.
     */
    bool Decide(const TArray<FAffordance>& Available, const FDeliberationContext& Ctx, FIntention& Out);

    /** Оценить одну возможность. Вся психика выбора — здесь. */
    FValuation Evaluate(const FAffordance& A, const FDeliberationContext& Ctx) const;

    /**
     * Насколько сильно новое предложение должно перевесить текущее,
     * чтобы человек бросил начатое. Зависит от характера и от того,
     * не кричит ли тело.
     */
    float SwitchingThreshold(const FDeliberationContext& Ctx) const;

    /** Сбросить намерение. */
    void Clear();

private:
    /** Ключ опыта для намерения (с учётом цели, если это человек). */
    static FName MakeKey(const FAffordance& A);

    /** Собрать человеческое объяснение выбора из разбора оценки. */
    static FString ExplainChoice(const FAffordance& A, const FValuation& V);
};
