#pragma once

#include "CoreMinimal.h"
#include "HumanTypes.h"
#include "MotorLearning.h"

struct AB_API FSelfState
{
    float Needs[static_cast<int32>(ENeedType::MAX)];
    float Weights[static_cast<int32>(ENeedType::MAX)];
    float Stamina = 1.0f;
    float Pain = 0.0f;
    float Health = 1.0f;
    float Impairment = 0.0f;
    float Harshness = 0.0f;
    float Pleasure = 0.0f;
    float Arousal = 0.0f;
    float Fear = 0.0f;
    float Anger = 0.0f;
    float Sadness = 0.0f;
    float Joy = 0.0f;
    float Boredom = 0.0f;
    float Money = 0.0f;
    float Hour = 12.0f;
    float Age = 30.0f;
    float Stores = 3.0f;
    float Firewood = 3.0f;
    float Wares = 0.0f;
    int32 DayOfYear = 200;
    int32 PeopleNearby = 0;
    bool bHolding = false;
    bool bAtHome = false;
    bool bEmployed = false;
    bool bFemale = false;

    FSelfState();
};

struct AB_API FOptionView
{
    EActionType Action = EActionType::Idle;
    EAffordanceSource Source = EAffordanceSource::Object;
    uint32 Category = 0;
    uint32 Place = 0;
    float DistanceM = 0.0f;
    float DurationMin = 10.0f;
    float MoneyCost = 0.0f;
    float MoneyPerHour = 0.0f;
    float Effort = 0.0f;
    float Risk = 0.0f;
    float Difficulty = 0.0f;
    float Mastery = 1.0f;
    float Closeness = 0.0f;
    float Liking = 0.0f;
    float Resentment = 0.0f;
    float Fear = 0.0f;
    int32 TimesTried = 0;
    int32 TimesHere = 0;
    int32 RecentRepeats = 0;
    int8 HaveRequired = 0;
    bool bProduces = false;
    bool bInHand = false;
    bool bSeat = false;
    bool bLying = false;
    bool bMine = false;
    bool bCraft = false;
    bool bOpenNow = true;
};

struct AB_API FMindSense
{
    static constexpr int32 NeedCount = static_cast<int32>(ENeedType::MAX);
    static constexpr int32 ActionCount = static_cast<int32>(EActionType::MAX);
    static constexpr int32 Buckets = 24;
    static constexpr int32 StateSize = NeedCount + 25;
    static constexpr int32 OptionSize = ActionCount + 4 + 30 + Buckets;
    static constexpr int32 InputSize = StateSize + OptionSize;

    static constexpr int32 PainAt = NeedCount;
    static constexpr int32 StaminaAt = NeedCount + 1;
    static constexpr int32 HealthAt = NeedCount + 2;
    static constexpr int32 PleasureAt = NeedCount + 3;
    static constexpr int32 MoneyAt = NeedCount + 4;
    static constexpr int32 TimeAt = NeedCount + 5;
    static constexpr int32 SuccessAt = NeedCount + 6;
    static constexpr int32 StoresAt = NeedCount + 7;
    static constexpr int32 FirewoodAt = NeedCount + 8;
    static constexpr int32 WaresAt = NeedCount + 9;
    static constexpr int32 OutcomeSize = NeedCount + 10;

    static void State(const FSelfState& Self, float* Out);
    static void Option(const FOptionView& View, float Money, float* Out);
    static void Outcome(const FSelfState& Before, const FSelfState& After, float Hours, bool bSuccess, float* Out);
    static float Hours(const float* Outcome);
    static FSelfState Imagined(const FSelfState& Self, const float* Outcome);
    static uint32 Word(const FString& Text);
};

struct AB_API FMindChoice
{
    int32 Index = INDEX_NONE;
    float Score = 0.0f;
    float Gain = 0.0f;
    float Future = 0.0f;
    float Wonder = 0.0f;
    float Hours = 0.0f;
    float Success = 1.0f;
    TArray<float> Expect;
};

class AB_API FMindCore
{
public:
    static constexpr int32 Ensemble = 3;
    static constexpr int32 Hidden = 96;
    static constexpr int32 WorthHidden = 64;
    static constexpr float AliveBonus = 0.6f;
    static constexpr float DeathLoss = 30.0f;

    int32 Capacity = 2400;
    float Gamma = 0.9f;
    float Wonder = 0.6f;
    int64 Lived = 0;
    int64 Lessons = 0;
    double ModelError = 1.0;
    double WorthError = 1.0;
    float AverageLiving = 0.0f;

    void Init(uint32 Seed);
    bool IsReady() const { return Consequence.Num() == Ensemble && Consequence[0].Size() > 0; }

    float Wellbeing(const FSelfState& Self) const;
    float Living(const FSelfState& Self) const { return Wellbeing(Self) + AliveBonus; }
    float Worth(const FSelfState& Self) const;
    void Imagine(const FSelfState& Self, const FOptionView& View, float* OutMean, float* OutSpread) const;
    FMindChoice Weigh(const FSelfState& Self, const FOptionView& View, float Openness, float Anxiety) const;
    FMindChoice Choose(const FSelfState& Self, const TArray<FOptionView>& Views, float Openness, float Anxiety, float Impairment,
        FRandomStream& Rng, TArray<FMindChoice>* OutAll) const;

    void Remember(const FSelfState& Before, const FOptionView& View, const FSelfState& After, float Hours, bool bSuccess, float Weight, bool bDied);
    void Practice(int32 Steps, FRandomStream& Rng);
    void Resize(int32 NewCapacity);
    int32 Memories() const { return Stored; }

    void Serialize(FArchive& Ar);
    bool Save(const FString& Path);
    bool Load(const FString& Path);

private:
    TArray<FDeepNet> Consequence;
    FDeepNet WorthNet;
    TArray<TArray<float>> ModelMoments;
    TArray<float> WorthMoments;
    int32 AdamSteps = 0;

    TArray<float> Inputs;
    TArray<float> Targets;
    TArray<float> Weights;
    TArray<float> From;
    TArray<float> To;
    TArray<float> Reward;
    TArray<float> Span;
    TArray<uint8> Ends;
    int32 Stored = 0;
    int32 Next = 0;
};

class AB_API FLifeSchool
{
public:
    static FString Folder();
    static FString CorePath(int32 Kind);
    static void Raise(FMindCore& Core, int32 Kind, int32 Days, int32 People, uint32 Seed, FString* OutReport);
};
