// SocialComponent.h
// ---------------------------------------------------------------------------
// Отношения с людьми.
//
// Отношение — не одно число «симпатия», а связка из семи независимых нитей:
// знакомство, симпатия, доверие, уважение, привязанность, обида, страх.
// Их можно рвать по отдельности. Можно любить и не доверять. Можно уважать
// и ненавидеть. Можно бояться того, кто тебе нравится. Как у людей.
//
// Вторая половина файла — ТЕОРИЯ РАЗУМА: модель того, что творится в голове
// у другого. Она всегда неточна. Именно из её неточности растут обиды,
// недопонимание и возможность соврать.
// ---------------------------------------------------------------------------

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HumanTypes.h"
#include "SocialComponent.generated.h"

class UPersonalityComponent;
class UEmotionComponent;
class UMemoryComponent;

UCLASS(ClassGroup = (Human), meta = (BlueprintSpawnableComponent))
class AB_API USocialComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    USocialComponent();

    /** Все, кого я знаю. */
    UPROPERTY(BlueprintReadOnly, Category = "Social")
    TMap<TObjectPtr<AActor>, FRelationship> Relations;

    /** Моя репутация в городе -1..+1 (то, что обо мне говорят). */
    UPROPERTY(BlueprintReadOnly, Category = "Social")
    float Reputation = 0.0f;

    /** Сколько раз меня ловили на лжи. */
    UPROPERTY(BlueprintReadOnly, Category = "Social")
    int32 TimesCaughtLying = 0;

    /** Сколько раз я соврал (знаю только я). */
    UPROPERTY(BlueprintReadOnly, Category = "Social")
    int32 LiesTold = 0;

    /** Максимум людей, которых можно удержать в голове (число Данбара в миниатюре). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Social")
    int32 RelationCapacity = 150;

    // --- ЖИЗНЬ ОТНОШЕНИЙ ----------------------------------------------------

    /** Шаг: связи без общения слабеют, обиды медленно тают. */
    void Advance(float GameDelta, float WorldTime);

    FRelationship& FindOrAdd(AActor* Other, float WorldTime);
    FRelationship* Find(AActor* Other);
    const FRelationship* Find(AActor* Other) const;

    // --- СОБЫТИЯ МЕЖДУ ЛЮДЬМИ -----------------------------------------------

    /**
     * Обычное взаимодействие.
     * Quality -1..+1 — насколько хорошо прошло.
     */
    void RecordInteraction(AActor* Other, float Quality, float WorldTime, const UPersonalityComponent* Personality);

    /** Мне помогли. */
    void OnHelpedBy(AActor* Other, float Magnitude, float WorldTime, const UPersonalityComponent* Personality);

    /** Мне навредили. bIntentional решает всё: случайность прощают. */
    void OnHarmedBy(AActor* Other, float Magnitude, bool bIntentional, float WorldTime, const UPersonalityComponent* Personality);

    /** Меня предали — самое дорогое разрушение доверия. */
    void OnBetrayedBy(AActor* Other, float Magnitude, float WorldTime, const UPersonalityComponent* Personality);

    /** Я поймал его на лжи. */
    void OnCaughtLying(AActor* Other, float WorldTime);

    /** Он извинился. Простят ли — зависит от характера и глубины обиды. */
    bool ReceiveApology(AActor* Other, float Sincerity, const UPersonalityComponent* Personality, float WorldTime);

    /** Я его давно не видел и соскучился (или наоборот — отвык). */
    void OnReunion(AActor* Other, float WorldTime);

    /** Он умер. */
    void OnDeathOf(AActor* Other, float WorldTime);

    // --- СЛУХИ И РЕПУТАЦИЯ --------------------------------------------------

    /**
     * Мне рассказали о ком-то.
     * Мнение источника влияет тем сильнее, чем больше я ему доверяю.
     */
    void ReceiveGossip(AActor* From, AActor* About, float Valence, float WorldTime, const UPersonalityComponent* Personality);

    /** Изменить репутацию (свою) — от поступков на людях. */
    void AdjustReputation(float Delta);

    // --- ТЕОРИЯ РАЗУМА ------------------------------------------------------

    /**
     * Посмотреть на человека и попробовать понять, что с ним.
     * ObservedMood/ObservedNeed — то, что человек НА САМОМ ДЕЛЕ переживает;
     * насколько точно это будет считано, решают эмпатия, знакомство и
     * выразительность собеседника.
     */
    void ObserveAndInfer(AActor* Other, float ObservedMood, ENeedType ObservedNeed,
                         float TheirExpressiveness, float MyEmpathySkill, float WorldTime);

    /** Как я думаю, он ко мне относится -1..+1. */
    UFUNCTION(BlueprintCallable, Category = "Social")
    float GetBelievedLikingOfMe(AActor* Other) const;

    /** Считаю ли я, что он знает обо мне нечто (основа для решения соврать). */
    UFUNCTION(BlueprintCallable, Category = "Social")
    float GetBelievedKnowledge(AActor* Other) const;

    // --- РЕШЕНИЯ ------------------------------------------------------------

    /**
     * Стану ли я врать этому человеку ради выгоды Benefit (0..1)?
     * Взвешивается: совесть, страх разоблачения, отношение к нему, умение врать.
     */
    bool DecideToLie(AActor* To, float Benefit, float LieSkill,
                     const UPersonalityComponent* Personality, FString& OutReasoning) const;

    /** Помогу ли я ему, если это стоит мне Cost (0..1)? */
    bool DecideToHelp(AActor* Whom, float Cost, float TheirNeed,
                      const UPersonalityComponent* Personality, FString& OutReasoning) const;

    /** Захочу ли я отомстить? */
    bool DecideToRetaliate(AActor* Against, const UPersonalityComponent* Personality) const;

    // --- ЗАПРОСЫ ------------------------------------------------------------

    /** Общая близость 0..1: знакомство + симпатия + привязанность. */
    UFUNCTION(BlueprintCallable, Category = "Social")
    float GetCloseness(AActor* Other) const;

    UFUNCTION(BlueprintCallable, Category = "Social")
    float GetTrust(AActor* Other) const;

    /** Насколько я вообще не один 0..1 — сумма всех близких связей. */
    UFUNCTION(BlueprintCallable, Category = "Social")
    float GetSocialSupport() const;

    /** Самый близкий человек. */
    UFUNCTION(BlueprintCallable, Category = "Social")
    AActor* GetClosestPerson() const;

    /** Тот, на кого больше всего зла. */
    UFUNCTION(BlueprintCallable, Category = "Social")
    AActor* GetGreatestGrudge() const;

    /** Сколько у меня друзей. */
    UFUNCTION(BlueprintCallable, Category = "Social")
    int32 CountFriends() const;

    /** Пересчитать тип отношений из его числовых нитей. */
    static ERelationKind ClassifyRelation(const FRelationship& R);

    /** Как я отношусь к человеку — словами. */
    UFUNCTION(BlueprintCallable, Category = "Social")
    FString DescribeRelation(AActor* Other) const;

private:
    /** Выбросить самое незначительное знакомство при переполнении. */
    void EnforceCapacity();

    float DecayAccumulator = 0.0f;
};
