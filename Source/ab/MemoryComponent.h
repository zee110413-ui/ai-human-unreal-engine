// MemoryComponent.h
// ---------------------------------------------------------------------------
// Память.
//
// Не журнал событий, а живая ткань, которая:
//   - забывает по кривой (сильнее всего — в первые часы);
//   - помнит потрясения почти вечно;
//   - ИСКАЖАЕТ воспоминание каждый раз, когда его достают (реконсолидация);
//   - иногда просто не может вспомнить, хотя «где-то там оно есть»;
//   - вытесняет невыносимое — и возвращает его во сне;
//   - во сне склеивает эпизоды в общие выводы о мире.
//
// Три вида: эпизодическая (что было), семантическая (что я знаю),
// процедурная (что я делаю не думая).
// ---------------------------------------------------------------------------

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HumanTypes.h"
#include "MemoryComponent.generated.h"

UCLASS(ClassGroup = (Human), meta = (BlueprintSpawnableComponent))
class AB_API UMemoryComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UMemoryComponent();

    // --- ХРАНИЛИЩА ----------------------------------------------------------

    /** Эпизоды — то, что со мной было. */
    UPROPERTY(BlueprintReadOnly, Category = "Memory")
    TArray<FEpisodicMemory> Episodes;

    /** Убеждения — то, что я считаю правдой о мире. Могут быть ложными. */
    UPROPERTY(BlueprintReadOnly, Category = "Memory")
    TArray<FBelief> Beliefs;

    /** Когнитивная карта — места, которые я знаю. */
    UPROPERTY(BlueprintReadOnly, Category = "Memory")
    TArray<FKnownLocation> Places;

    /** Привычки — действия, которые запускаются сами. */
    UPROPERTY(BlueprintReadOnly, Category = "Memory")
    TArray<FHabit> Habits;

    /**
     * Выученный опыт: чем для меня оборачивается то или иное дело.
     * Ниоткуда не задано. Заполняется только тем, что человек прожил сам.
     * Отсюда берутся «в том кафе вкусно» и «с этим лучше не связываться».
     */
    UPROPERTY(BlueprintReadOnly, Category = "Memory")
    TArray<FOutcomeAssociation> Outcomes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Memory")
    int32 OutcomeCapacity = 500;

    /** Сколько эпизодов вообще помещается в голове. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Memory")
    int32 EpisodeCapacity = 900;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Memory")
    int32 BeliefCapacity = 600;

    /** Качество памяти 0..1 — врождённое, падает с возрастом. */
    UPROPERTY(BlueprintReadOnly, Category = "Memory")
    float MemoryQuality = 1.0f;

    /** Сколько всего эпизодов было забыто безвозвратно (для статистики). */
    UPROPERTY(BlueprintReadOnly, Category = "Memory")
    int32 ForgottenCount = 0;

    // --- ЗАПИСЬ -------------------------------------------------------------

    /**
     * Запомнить событие.
     * Attention 0..1 — насколько я был сосредоточен. Рассеянный не запомнит.
     * Возвращает Id или -1, если не отложилось вовсе.
     */
    int32 Encode(const FString& Summary, FName Tag, float Valence, float Arousal,
                 const TArray<AActor*>& Participants, const FVector& Location,
                 float WorldTime, float Attention, EMemoryKind Kind = EMemoryKind::Episodic);

    /** Упрощённая запись — когда деталей нет. */
    int32 EncodeSimple(const FString& Summary, float Valence, float WorldTime);

    // --- ПРИПОМИНАНИЕ -------------------------------------------------------

    /**
     * Попытаться вспомнить эпизод по теме и/или человеку.
     * Может НЕ найти, даже если след существует — слабый след не достаётся.
     * Успешное припоминание укрепляет след и одновременно его искажает.
     */
    FEpisodicMemory* Recall(FName Tag, AActor* About, float WorldTime);

    /** Несколько подходящих воспоминаний (без искажения — для внутреннего анализа). */
    TArray<FEpisodicMemory> Query(FName Tag, AActor* About, int32 MaxCount) const;

    /** Самое яркое воспоминание — то, что всплывает само. */
    FEpisodicMemory* GetMostVivid();

    /** Случайное воспоминание с весом по яркости — материал для снов. */
    FEpisodicMemory* GetRandomWeighted();

    /** Насколько хорошо я помню этого человека 0..1. */
    float GetFamiliarityWith(AActor* Who) const;

    /** Средняя эмоциональная окраска воспоминаний о человеке -1..+1. */
    float GetAffectiveToneAbout(AActor* Who) const;

    /** Средняя окраска всех недавних воспоминаний — «как жизнь в последнее время». */
    float GetRecentLifeTone(float WorldTime, float WindowSeconds) const;

    // --- ЗАБЫВАНИЕ И СОН ----------------------------------------------------

    /** Шаг: забывание по кривой. */
    void Advance(float GameDelta, float WorldTime, float CognitiveImpairment);

    /**
     * Консолидация во сне: укрепляет важное, выбрасывает мусор,
     * и — самое главное — превращает повторяющиеся эпизоды в убеждения.
     */
    void ConsolidateDuringSleep(float GameDelta, float WorldTime, bool bREM);

    /** Вытеснить невыносимое воспоминание. */
    void Repress(int32 MemoryId);

    /** Вернуть вытесненное в сознание (случается при стрессе и во сне). */
    FEpisodicMemory* SurfaceRepressed();

    // --- УБЕЖДЕНИЯ ----------------------------------------------------------

    /**
     * Узнать что-то новое.
     * SourceCredibility 0..1 — насколько я верю источнику.
     * Учитывается предвзятость подтверждения: то, что совпадает с уже
     * имеющимся мнением, принимается легче.
     */
    void Learn(const FBelief& NewBelief, float SourceCredibility, float Openness, float WorldTime);

    /** Найти убеждение. */
    FBelief* FindBelief(FName Subject, FName Predicate);
    const FBelief* FindBelief(FName Subject, FName Predicate) const;

    /** Значение убеждения (0, если такого мнения нет). */
    UFUNCTION(BlueprintCallable, Category = "Memory")
    float GetBeliefValue(FName Subject, FName Predicate) const;

    /** Уверенность в убеждении 0..1. */
    UFUNCTION(BlueprintCallable, Category = "Memory")
    float GetBeliefConfidence(FName Subject, FName Predicate) const;

    /** Личный опыт: подтвердить или опровергнуть убеждение. Самый сильный довод. */
    void VerifyBelief(FName Subject, FName Predicate, float ObservedValue, float WorldTime);

    /** Убеждение, которым хочется поделиться (интересное и уверенное). */
    const FBelief* PickBeliefToShare() const;

    // --- МЕСТА --------------------------------------------------------------

    /**
     * Запомнить место. Offers — то, что человек, по его мнению, там нашёл.
     * Именно его мнение: оно может расходиться с действительностью и
     * уточняется только следующими посещениями.
     */
    void LearnPlace(EPlaceKind Kind, const FVector& Location, const FString& Label,
                    bool bFirsthand, AActor* ToldBy, float WorldTime,
                    const TArray<FAffordance>* Offers = nullptr);

    /** Ближайшее известное место нужного типа. Возвращает false, если такого не знаю. */
    bool FindNearestPlace(EPlaceKind Kind, const FVector& From, FVector& OutLocation) const;

    /** Знаю ли я уже об этом месте. */
    bool KnowsPlace(EPlaceKind Kind, const FVector& Location) const;

    /** Все места данного типа. */
    TArray<FKnownLocation> GetPlacesOfKind(EPlaceKind Kind) const;

    /** Отметить посещение — растит близость к месту. */
    void VisitPlace(const FVector& Location, float WorldTime, float AffectDelta);

    // --- ДОРОГИ -------------------------------------------------------------

    /** Выученные дороги: куда как ходить. */
    UPROPERTY(BlueprintReadOnly, Category = "Memory")
    TArray<FRouteMemory> Routes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Memory")
    int32 RouteCapacity = 120;

    /** Прошёл дорогу — запомнил её. */
    void LearnRoute(const FVector& From, const FVector& To, const TArray<FVector>& Trail,
                    float TravelTime, float WorldTime);

    /**
     * Вспомнить, как туда идти.
     *
     * Сначала ищется дорога именно туда. Если такой нет, годится и чужая:
     * если какая-то знакомая дорога проходит рядом с нужным местом, человек
     * пойдёт по ней и свернёт в конце. Так и получается срезание пути —
     * признак настоящей карты в голове, а не заученной последовательности.
     */
    bool RecallRoute(const FVector& From, const FVector& To, float WorldTime,
                     TArray<FVector>& OutWaypoints);

    /** Насколько хорошо человек знает дорогу туда 0..1. */
    float GetRouteFamiliarity(const FVector& From, const FVector& To) const;

    // --- ВЫУЧЕННЫЙ ОПЫТ -----------------------------------------------------

    /**
     * Чего я жду от этого дела. 0, если никогда его не делал.
     * OutConfidence — насколько я в этом уверен.
     */
    float GetExpectedOutcome(FName Key, float& OutConfidence) const;

    /**
     * То же, но с переносом опыта на подобное.
     *
     * Если про эту самую вещь ничего не известно, человек судит по виду:
     * в незнакомом кафе он ждёт примерно того же, что и в прочих кафе.
     * По мере того как накапливается опыт именно здесь, общее знание
     * уступает место частному — так и происходит различение.
     *
     * Без этого он подходил к каждой новой вещи так, будто никогда
     * в жизни не видел ничего похожего.
     */
    float GetExpectedOutcomeGeneralised(FName Key, FName CategoryKey, float& OutConfidence) const;

    /**
     * Прожил поступок — уточнил ожидание.
     * ActualValue -1..+1 — чем всё обернулось на самом деле.
     * Возвращает ошибку предсказания: насколько действительность разошлась
     * с ожиданием. Именно из неё, а не из таблицы, рождаются радость
     * и разочарование.
     */
    float LearnOutcome(FName Key, float ActualValue, float WorldTime, float LearningRate = 0.35f);

    /**
     * Прожил поступок — уточнил и частное знание, и общее.
     *
     * Опыт с конкретной вещью откладывается быстро, знание о виде —
     * медленно: чтобы изменить мнение обо всех кафе сразу, одного
     * неудачного обеда мало.
     * Возвращает ошибку предсказания по конкретной вещи.
     */
    float LearnOutcomeGeneralised(FName Key, FName CategoryKey, float ActualValue, float WorldTime);

    /** Сформулировать вывод словами: «там вкусно», «с ним тяжело». */
    void NameConclusion(FName Key, const FString& Text);

    /** Найти запись опыта. */
    const FOutcomeAssociation* FindOutcome(FName Key) const;

    /** Самый горький и самый ценный выученный опыт — для мыслей и разговоров. */
    const FOutcomeAssociation* GetStrongestOutcome(bool bPositive) const;

    // --- ПРИВЫЧКИ -----------------------------------------------------------

    /** Действие повторилось в этом контексте — привычка крепнет. */
    void ReinforceHabit(FName Context, EActionType Action, float WorldTime);

    /** Есть ли привычка, готовая сработать автоматически. */
    bool GetHabitualAction(FName Context, EActionType& OutAction, float& OutStrength) const;

    /** Ослабить привычку (сознательное усилие её переломить). */
    void WeakenHabit(FName Context, float Amount);

    // --- ПРОЧЕЕ -------------------------------------------------------------

    /** Возраст влияет на память: после 60 следы держатся хуже. */
    void ApplyAging(float Age);

    /** Короткий пересказ пары ярких воспоминаний — для разговоров и отладки. */
    UFUNCTION(BlueprintCallable, Category = "Memory")
    FString SummarizeLife() const;

private:
    /** Выбросить самое слабое, если переполнено. */
    void EnforceCapacity();

    /** Исказить воспоминание при доставании его из памяти. */
    void Reconsolidate(FEpisodicMemory& Memory, float WorldTime);

    /** Следующий свободный Id. */
    int32 NextId = 1;

    /** Накопитель для нечастой обработки забывания. */
    float DecayAccumulator = 0.0f;
};
