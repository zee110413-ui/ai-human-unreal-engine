#include "MindLearning.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/Crc.h"
#include "Serialization/BufferArchive.h"
#include "Serialization/MemoryReader.h"

namespace
{
    constexpr int32 CoreVersion = 3;

    float Priority(int32 Need)
    {
        switch (static_cast<ENeedType>(Need))
        {
        case ENeedType::Hunger:
        case ENeedType::Thirst:
        case ENeedType::Sleep:
        case ENeedType::Bladder:
        case ENeedType::Health:
            return 2.0f;
        case ENeedType::Safety:
        case ENeedType::Comfort:
        case ENeedType::Shelter:
            return 1.4f;
        case ENeedType::Hygiene:
            return 1.0f;
        default:
            return 0.7f;
        }
    }

    void AdamStep(TArray<float>& Params, const TArray<float>& Grad, TArray<float>& Moments, int32 Step, float Rate)
    {
        const int32 Count = Params.Num();
        if (Moments.Num() != Count * 2)
        {
            Moments.Init(0.0f, Count * 2);
        }
        const float C1 = 1.0f - FMath::Pow(0.9f, static_cast<float>(FMath::Min(Step, 2000)));
        const float C2 = 1.0f - FMath::Pow(0.999f, static_cast<float>(FMath::Min(Step, 20000)));
        float* M = Moments.GetData();
        float* V = M + Count;
        for (int32 I = 0; I < Count; ++I)
        {
            const float G = FMath::Clamp(Grad[I], -5.0f, 5.0f);
            M[I] = 0.9f * M[I] + 0.1f * G;
            V[I] = 0.999f * V[I] + 0.001f * G * G;
            Params[I] -= Rate * (M[I] / C1) / (FMath::Sqrt(V[I] / C2) + 1.0e-5f);
        }
    }

    float Sign(bool bValue)
    {
        return bValue ? 1.0f : -1.0f;
    }
}

FSelfState::FSelfState()
{
    for (int32 N = 0; N < FMindSense::NeedCount; ++N)
    {
        Needs[N] = 0.7f;
        Weights[N] = 1.0f;
    }
}

uint32 FMindSense::Word(const FString& Text)
{
    return Text.IsEmpty() ? 0u : FCrc::StrCrc32(*Text);
}

void FMindSense::State(const FSelfState& Self, float* Out)
{
    int32 I = 0;
    for (int32 N = 0; N < NeedCount; ++N)
    {
        Out[I++] = Self.Needs[N] * 2.0f - 1.0f;
    }
    Out[I++] = Self.Stamina * 2.0f - 1.0f;
    Out[I++] = Self.Pain * 2.0f;
    Out[I++] = Self.Health * 2.0f - 1.0f;
    Out[I++] = Self.Impairment * 2.0f;
    Out[I++] = Self.Harshness * 2.0f;
    Out[I++] = Self.Pleasure;
    Out[I++] = Self.Arousal;
    Out[I++] = Self.Fear * 2.0f;
    Out[I++] = Self.Anger * 2.0f;
    Out[I++] = Self.Sadness * 2.0f;
    Out[I++] = Self.Joy * 2.0f;
    Out[I++] = Self.Boredom * 2.0f;
    Out[I++] = FMath::Loge(1.0f + FMath::Max(0.0f, Self.Money)) / 3.0f - 1.0f;
    Out[I++] = FMath::Sin(Self.Hour / 24.0f * UE_TWO_PI);
    Out[I++] = FMath::Cos(Self.Hour / 24.0f * UE_TWO_PI);
    Out[I++] = FMath::Loge(1.0f + FMath::Max(0.0f, Self.Stores)) / 1.5f - 1.0f;
    Out[I++] = FMath::Loge(1.0f + FMath::Max(0.0f, Self.Firewood)) / 1.5f - 1.0f;
    Out[I++] = FMath::Loge(1.0f + FMath::Max(0.0f, Self.Wares)) / 3.0f - 1.0f;
    Out[I++] = FMath::Sin(Self.DayOfYear / 365.0f * UE_TWO_PI);
    Out[I++] = FMath::Cos(Self.DayOfYear / 365.0f * UE_TWO_PI);
    Out[I++] = Self.Age / 40.0f - 1.0f;
    Out[I++] = FMath::Min(Self.PeopleNearby, 6) / 3.0f - 1.0f;
    Out[I++] = Sign(Self.bHolding);
    Out[I++] = Sign(Self.bAtHome);
    Out[I++] = Sign(Self.bEmployed);
    check(I == StateSize);
    for (int32 K = 0; K < StateSize; ++K)
    {
        Out[K] = FMath::Clamp(Out[K], -4.0f, 4.0f);
    }
}

void FMindSense::Option(const FOptionView& View, float Money, float* Out)
{
    FMemory::Memzero(Out, sizeof(float) * OptionSize);
    int32 I = 0;
    Out[I + FMath::Clamp(static_cast<int32>(View.Action), 0, ActionCount - 1)] = 1.0f;
    I += ActionCount;
    Out[I + FMath::Clamp(static_cast<int32>(View.Source), 0, 3)] = 1.0f;
    I += 4;
    const float Walk = View.DistanceM / 1.35f / 3600.0f;
    Out[I++] = FMath::Loge(1.0f + FMath::Max(0.0f, View.DistanceM)) / 4.0f - 1.0f;
    Out[I++] = FMath::Min(Walk * 4.0f, 4.0f);
    Out[I++] = FMath::Loge(1.0f + FMath::Max(0.0f, View.DurationMin)) / 3.0f - 1.0f;
    Out[I++] = FMath::Loge(1.0f + FMath::Max(0.0f, View.MoneyCost)) / 3.0f;
    Out[I++] = View.MoneyCost <= 0.0f || View.MoneyCost <= Money ? 1.0f : -1.0f;
    Out[I++] = FMath::Loge(1.0f + FMath::Max(0.0f, View.MoneyPerHour)) / 3.0f;
    Out[I++] = View.Effort * 2.0f;
    Out[I++] = View.Risk * 2.0f;
    Out[I++] = View.Difficulty * 2.0f;
    Out[I++] = View.Mastery * 2.0f - 1.0f;
    Out[I++] = View.Closeness;
    Out[I++] = View.Liking;
    Out[I++] = View.Resentment;
    Out[I++] = View.Fear;
    Out[I++] = FMath::Loge(1.0f + View.TimesTried) / 3.0f;
    Out[I++] = FMath::Loge(1.0f + View.TimesHere) / 3.0f;
    Out[I++] = FMath::Min(View.RecentRepeats, 6) / 3.0f;
    Out[I++] = static_cast<float>(View.HaveRequired);
    Out[I++] = Sign(View.bProduces);
    Out[I++] = Sign(View.bInHand);
    Out[I++] = Sign(View.bSeat);
    Out[I++] = Sign(View.bLying);
    Out[I++] = Sign(View.bMine);
    Out[I++] = Sign(View.bCraft);
    Out[I++] = Sign(View.bOpenNow);
    Out[I++] = View.Mastery - View.Difficulty;
    Out[I++] = FMath::Clamp(View.MoneyCost / (Money + 1.0f), 0.0f, 2.0f);
    Out[I++] = Sign(View.DistanceM < 3.0f);
    Out[I++] = Sign(View.Place != 0);
    Out[I++] = 1.0f;
    if (View.Category != 0)
    {
        Out[I + View.Category % Buckets] += 1.0f;
        Out[I + (View.Category / Buckets) % Buckets] += (View.Category & 0x100000u) ? 0.5f : -0.5f;
    }
    if (View.Place != 0)
    {
        Out[I + View.Place % Buckets] += 0.35f;
    }
    I += Buckets;
    check(I == OptionSize);
    for (int32 K = 0; K < OptionSize; ++K)
    {
        Out[K] = FMath::Clamp(Out[K], -4.0f, 4.0f);
    }
}

void FMindSense::Outcome(const FSelfState& Before, const FSelfState& After, float Hours, bool bSuccess, float* Out)
{
    for (int32 N = 0; N < NeedCount; ++N)
    {
        Out[N] = FMath::Clamp((After.Needs[N] - Before.Needs[N]) * 3.0f, -3.0f, 3.0f);
    }
    Out[PainAt] = FMath::Clamp((After.Pain - Before.Pain) * 3.0f, -3.0f, 3.0f);
    Out[StaminaAt] = FMath::Clamp((After.Stamina - Before.Stamina) * 3.0f, -3.0f, 3.0f);
    Out[HealthAt] = FMath::Clamp((After.Health - Before.Health) * 3.0f, -3.0f, 3.0f);
    Out[PleasureAt] = FMath::Clamp((After.Pleasure - Before.Pleasure) * 2.0f, -3.0f, 3.0f);
    const float Spent = After.Money - Before.Money;
    Out[MoneyAt] = FMath::Sign(Spent) * FMath::Loge(1.0f + FMath::Abs(Spent)) / 3.0f;
    Out[TimeAt] = FMath::Loge(1.0f + FMath::Max(0.0f, Hours) * 60.0f) / 3.0f;
    Out[SuccessAt] = bSuccess ? 1.0f : 0.0f;
    Out[StoresAt] = FMath::Clamp((After.Stores - Before.Stores) / 2.0f, -3.0f, 3.0f);
    Out[FirewoodAt] = FMath::Clamp((After.Firewood - Before.Firewood) / 2.0f, -3.0f, 3.0f);
    const float Gained = After.Wares - Before.Wares;
    Out[WaresAt] = FMath::Sign(Gained) * FMath::Loge(1.0f + FMath::Abs(Gained)) / 3.0f;
}

float FMindSense::Hours(const float* Outcome)
{
    const float Minutes = FMath::Exp(FMath::Clamp(Outcome[TimeAt], 0.0f, 3.0f) * 3.0f) - 1.0f;
    return FMath::Clamp(Minutes / 60.0f, 1.0f / 60.0f, 14.0f);
}

FSelfState FMindSense::Imagined(const FSelfState& Self, const float* Outcome)
{
    FSelfState After = Self;
    for (int32 N = 0; N < NeedCount; ++N)
    {
        After.Needs[N] = FMath::Clamp(Self.Needs[N] + Outcome[N] / 3.0f, 0.0f, 1.0f);
    }
    After.Pain = FMath::Clamp(Self.Pain + Outcome[PainAt] / 3.0f, 0.0f, 1.0f);
    After.Stamina = FMath::Clamp(Self.Stamina + Outcome[StaminaAt] / 3.0f, 0.0f, 1.0f);
    After.Health = FMath::Clamp(Self.Health + Outcome[HealthAt] / 3.0f, 0.0f, 1.0f);
    After.Pleasure = FMath::Clamp(Self.Pleasure + Outcome[PleasureAt] / 2.0f, -1.0f, 1.0f);
    const float Log = FMath::Abs(Outcome[MoneyAt]) * 3.0f;
    After.Money = FMath::Max(0.0f, Self.Money + FMath::Sign(Outcome[MoneyAt]) * (FMath::Exp(FMath::Min(Log, 12.0f)) - 1.0f));
    After.Hour = FMath::Fmod(Self.Hour + Hours(Outcome), 24.0f);
    After.Stores = FMath::Max(0.0f, Self.Stores + Outcome[StoresAt] * 2.0f);
    After.Firewood = FMath::Max(0.0f, Self.Firewood + Outcome[FirewoodAt] * 2.0f);
    const float WaresLog = FMath::Abs(Outcome[WaresAt]) * 3.0f;
    After.Wares = FMath::Max(0.0f, Self.Wares + FMath::Sign(Outcome[WaresAt]) * (FMath::Exp(FMath::Min(WaresLog, 12.0f)) - 1.0f));
    return After;
}

void FMindCore::Init(uint32 Seed)
{
    FRandomStream Rng(static_cast<int32>(Seed));
    Consequence.SetNum(Ensemble);
    ModelMoments.SetNum(Ensemble);
    for (int32 K = 0; K < Ensemble; ++K)
    {
        FRandomStream Own(static_cast<int32>(Seed + 977u * (K + 1)));
        Consequence[K].Init(FMindSense::InputSize, Hidden, FMindSense::OutcomeSize, Own, 0.5f);
        ModelMoments[K].Reset();
    }
    WorthNet.Init(FMindSense::StateSize, WorthHidden, 1, Rng, 0.3f);
    WorthMoments.Reset();
    AdamSteps = 0;
    Inputs.Init(0.0f, Capacity * FMindSense::InputSize);
    Targets.Init(0.0f, Capacity * FMindSense::OutcomeSize);
    Weights.Init(0.0f, Capacity);
    From.Init(0.0f, Capacity * FMindSense::StateSize);
    To.Init(0.0f, Capacity * FMindSense::StateSize);
    Reward.Init(0.0f, Capacity);
    Span.Init(0.0f, Capacity);
    Ends.Init(0, Capacity);
    Stored = 0;
    Next = 0;
    Lived = 0;
    Lessons = 0;
    ModelError = 1.0;
    WorthError = 1.0;
    AverageLiving = 0.0f;
}

float FMindCore::Wellbeing(const FSelfState& Self) const
{
    float Value = 0.0f;
    for (int32 N = 0; N < FMindSense::NeedCount; ++N)
    {
        const float Deficit = 1.0f - FMath::Clamp(Self.Needs[N], 0.0f, 1.0f);
        Value -= FMath::Clamp(Self.Weights[N], 0.0f, 2.5f) * Priority(N) * Deficit * Deficit;
    }
    Value -= 2.0f * FMath::Clamp(Self.Pain, 0.0f, 1.0f);
    Value -= 1.5f * (1.0f - FMath::Clamp(Self.Health, 0.0f, 1.0f));
    Value -= 0.5f * (1.0f - FMath::Clamp(Self.Stamina, 0.0f, 1.0f));
    Value += 0.8f * FMath::Clamp(Self.Pleasure, -1.0f, 1.0f);
    return Value / 10.0f;
}

float FMindCore::Worth(const FSelfState& Self) const
{
    if (!IsReady())
    {
        return 0.0f;
    }
    float In[FMindSense::StateSize];
    FMindSense::State(Self, In);
    float Cache[2 * WorthHidden];
    float Out = 0.0f;
    WorthNet.Forward(In, &Out, Cache);
    return Out;
}

void FMindCore::Imagine(const FSelfState& Self, const FOptionView& View, float* OutMean, float* OutSpread) const
{
    float In[FMindSense::InputSize];
    FMindSense::State(Self, In);
    FMindSense::Option(View, Self.Money, In + FMindSense::StateSize);
    float Cache[2 * Hidden];
    float Each[Ensemble][FMindSense::OutcomeSize];
    for (int32 K = 0; K < Ensemble; ++K)
    {
        Consequence[K].Forward(In, Each[K], Cache);
    }
    for (int32 O = 0; O < FMindSense::OutcomeSize; ++O)
    {
        float Sum = 0.0f;
        for (int32 K = 0; K < Ensemble; ++K)
        {
            Sum += Each[K][O];
        }
        const float Mean = Sum / Ensemble;
        float Var = 0.0f;
        for (int32 K = 0; K < Ensemble; ++K)
        {
            Var += FMath::Square(Each[K][O] - Mean);
        }
        OutMean[O] = Mean;
        if (OutSpread)
        {
            OutSpread[O] = FMath::Sqrt(Var / Ensemble);
        }
    }
}

FMindChoice FMindCore::Weigh(const FSelfState& Self, const FOptionView& View, float Openness, float Anxiety) const
{
    FMindChoice Choice;
    if (!IsReady())
    {
        return Choice;
    }
    Choice.Expect.SetNumZeroed(FMindSense::OutcomeSize);
    float Spread[FMindSense::OutcomeSize];
    Imagine(Self, View, Choice.Expect.GetData(), Spread);
    Choice.Success = FMath::Clamp(Choice.Expect[FMindSense::SuccessAt], 0.0f, 1.0f);
    Choice.Hours = FMindSense::Hours(Choice.Expect.GetData());
    const FSelfState After = FMindSense::Imagined(Self, Choice.Expect.GetData());
    const float Now = Living(Self);
    const float Then = Living(After);
    Choice.Gain = (0.5f * (Now + Then) - AverageLiving) * Choice.Hours;
    Choice.Future = FMath::Pow(Gamma, Choice.Hours) * Worth(After);
    float Doubt = 0.0f;
    for (int32 N = 0; N < FMindSense::NeedCount; ++N)
    {
        Doubt += Spread[N];
    }
    Doubt += Spread[FMindSense::PleasureAt] + Spread[FMindSense::PainAt] + Spread[FMindSense::SuccessAt];
    Doubt /= FMindSense::NeedCount + 3;
    Choice.Wonder = (Doubt * Wonder * (0.3f + FMath::Clamp(Openness, 0.0f, 1.0f)) - Doubt * 0.3f * FMath::Clamp(Anxiety, 0.0f, 1.0f))
        * (0.25f + 0.75f * Choice.Success);
    Choice.Score = Choice.Gain + Choice.Future + Choice.Wonder;
    return Choice;
}

FMindChoice FMindCore::Choose(const FSelfState& Self, const TArray<FOptionView>& Views, float Openness, float Anxiety, float Impairment,
    FRandomStream& Rng, TArray<FMindChoice>* OutAll) const
{
    TArray<FMindChoice> All;
    All.Reserve(Views.Num());
    float Best = -BIG_NUMBER;
    for (int32 I = 0; I < Views.Num(); ++I)
    {
        FMindChoice Choice = Weigh(Self, Views[I], Openness, Anxiety);
        Choice.Index = I;
        Best = FMath::Max(Best, Choice.Score);
        All.Add(MoveTemp(Choice));
    }
    FMindChoice Picked;
    if (All.Num() == 0)
    {
        return Picked;
    }
    const float Temperature = 0.04f + 0.25f * FMath::Clamp(Impairment, 0.0f, 1.0f);
    double Total = 0.0;
    TArray<double> Chance;
    Chance.SetNum(All.Num());
    for (int32 I = 0; I < All.Num(); ++I)
    {
        Chance[I] = FMath::Exp(FMath::Max(-30.0f, (All[I].Score - Best) / Temperature));
        Total += Chance[I];
    }
    double Draw = Rng.FRand() * Total;
    int32 Index = All.Num() - 1;
    for (int32 I = 0; I < All.Num(); ++I)
    {
        Draw -= Chance[I];
        if (Draw <= 0.0)
        {
            Index = I;
            break;
        }
    }
    Picked = All[Index];
    if (OutAll)
    {
        *OutAll = MoveTemp(All);
    }
    return Picked;
}

void FMindCore::Remember(const FSelfState& Before, const FOptionView& View, const FSelfState& After, float Hours, bool bSuccess, float Weight, bool bDied)
{
    if (!IsReady())
    {
        return;
    }
    const int32 Slot = Next;
    Next = (Next + 1) % Capacity;
    Stored = FMath::Min(Stored + 1, Capacity);
    float* In = Inputs.GetData() + Slot * FMindSense::InputSize;
    FMindSense::State(Before, In);
    FMindSense::Option(View, Before.Money, In + FMindSense::StateSize);
    FMindSense::Outcome(Before, After, Hours, bSuccess, Targets.GetData() + Slot * FMindSense::OutcomeSize);
    Weights[Slot] = FMath::Clamp(Weight, 0.05f, 2.0f);
    FMindSense::State(Before, From.GetData() + Slot * FMindSense::StateSize);
    FMindSense::State(After, To.GetData() + Slot * FMindSense::StateSize);
    const float Lasted = FMath::Clamp(Hours, 1.0f / 60.0f, 14.0f);
    const float Rate = 0.5f * (Living(Before) + Living(After));
    AverageLiving += (1.0f - FMath::Exp(-Lasted / 48.0f)) * (Rate - AverageLiving);
    Reward[Slot] = Rate * Lasted - (bDied ? DeathLoss : 0.0f);
    Span[Slot] = Lasted;
    Ends[Slot] = bDied ? 1 : 0;
    ++Lived;
}

void FMindCore::Practice(int32 Steps, FRandomStream& Rng)
{
    if (!IsReady() || Stored < 16)
    {
        return;
    }
    const int32 Batch = FMath::Min(48, Stored);
    TArray<float> Grad;
    float Cache[2 * Hidden];
    float Scratch[2 * Hidden];
    float Out[FMindSense::OutcomeSize];
    float OutGrad[FMindSense::OutcomeSize];
    float WorthCache[2 * WorthHidden];
    float WorthScratch[2 * WorthHidden];
    for (int32 Step = 0; Step < Steps; ++Step)
    {
        ++AdamSteps;
        double ErrorSum = 0.0;
        for (int32 K = 0; K < Ensemble; ++K)
        {
            FDeepNet& Net = Consequence[K];
            Grad.Init(0.0f, Net.Size());
            for (int32 B = 0; B < Batch; ++B)
            {
                const int32 I = Rng.RandRange(0, Stored - 1);
                const float* In = Inputs.GetData() + I * FMindSense::InputSize;
                const float* Target = Targets.GetData() + I * FMindSense::OutcomeSize;
                Net.Forward(In, Out, Cache);
                const float Scale = Weights[I] * 2.0f / (Batch * FMindSense::OutcomeSize);
                for (int32 O = 0; O < FMindSense::OutcomeSize; ++O)
                {
                    const float Error = Out[O] - Target[O];
                    ErrorSum += Error * Error;
                    OutGrad[O] = Error * Scale;
                }
                Net.Backward(In, Cache, OutGrad, Grad.GetData(), Scratch);
            }
            AdamStep(Net.Weights, Grad, ModelMoments[K], AdamSteps, 1.0e-3f);
        }
        ModelError = 0.98 * ModelError + 0.02 * (ErrorSum / (Ensemble * Batch * FMindSense::OutcomeSize));

        Grad.Init(0.0f, WorthNet.Size());
        double WorthSum = 0.0;
        for (int32 B = 0; B < Batch; ++B)
        {
            const int32 I = Rng.RandRange(0, Stored - 1);
            const float* Start = From.GetData() + I * FMindSense::StateSize;
            const float* Finish = To.GetData() + I * FMindSense::StateSize;
            float Later = 0.0f;
            if (Ends[I] == 0)
            {
                WorthNet.Forward(Finish, &Later, WorthCache);
            }
            const float Target = Reward[I] - AverageLiving * Span[I] + FMath::Pow(Gamma, Span[I]) * Later;
            float Value = 0.0f;
            WorthNet.Forward(Start, &Value, WorthCache);
            const float Error = Value - Target;
            WorthSum += Error * Error;
            const float ValueGrad = Error / Batch;
            WorthNet.Backward(Start, WorthCache, &ValueGrad, Grad.GetData(), WorthScratch);
        }
        AdamStep(WorthNet.Weights, Grad, WorthMoments, AdamSteps, 5.0e-4f);
        WorthError = 0.98 * WorthError + 0.02 * (WorthSum / Batch);
        ++Lessons;
    }
}

void FMindCore::Resize(int32 NewCapacity)
{
    NewCapacity = FMath::Max(64, NewCapacity);
    if (!IsReady() || NewCapacity == Capacity)
    {
        return;
    }
    const int32 Keep = FMath::Min(Stored, NewCapacity);
    const int32 Old = Capacity;
    const int32 Head = Next;
    auto Carry = [&](auto& Data, int32 Width)
    {
        using FItem = typename TRemoveReference<decltype(Data)>::Type::ElementType;
        TArray<FItem> Fresh;
        Fresh.SetNumZeroed(NewCapacity * Width);
        for (int32 K = 0; K < Keep; ++K)
        {
            const int32 Source = (Head - Keep + K + Old * 2) % Old;
            FMemory::Memcpy(Fresh.GetData() + K * Width, Data.GetData() + Source * Width, sizeof(FItem) * Width);
        }
        Data = MoveTemp(Fresh);
    };
    Carry(Inputs, FMindSense::InputSize);
    Carry(Targets, FMindSense::OutcomeSize);
    Carry(Weights, 1);
    Carry(From, FMindSense::StateSize);
    Carry(To, FMindSense::StateSize);
    Carry(Reward, 1);
    Carry(Span, 1);
    Carry(Ends, 1);
    Capacity = NewCapacity;
    Stored = Keep;
    Next = Keep % NewCapacity;
}

void FMindCore::Serialize(FArchive& Ar)
{
    int32 Version = CoreVersion;
    int32 InputSize = FMindSense::InputSize;
    int32 OutcomeSize = FMindSense::OutcomeSize;
    int32 StateSize = FMindSense::StateSize;
    Ar << Version << InputSize << OutcomeSize << StateSize;
    if (Version != CoreVersion || InputSize != FMindSense::InputSize || OutcomeSize != FMindSense::OutcomeSize
        || StateSize != FMindSense::StateSize)
    {
        Ar.SetError();
        return;
    }
    int32 Members = Consequence.Num();
    Ar << Members;
    if (Ar.IsLoading())
    {
        Consequence.SetNum(Members);
        ModelMoments.SetNum(Members);
    }
    for (FDeepNet& Net : Consequence)
    {
        Net.Serialize(Ar);
    }
    WorthNet.Serialize(Ar);
    Ar << Capacity << Gamma << Wonder << Lived << Lessons << ModelError << WorthError << AdamSteps << AverageLiving;
    Ar << Inputs << Targets << Weights << From << To << Reward << Span << Ends << Stored << Next;
    for (TArray<float>& Moments : ModelMoments)
    {
        Ar << Moments;
    }
    Ar << WorthMoments;
}

bool FMindCore::Save(const FString& Path)
{
    FBufferArchive Writer;
    Serialize(Writer);
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
    return FFileHelper::SaveArrayToFile(Writer, *Path);
}

bool FMindCore::Load(const FString& Path)
{
    TArray<uint8> Bytes;
    if (!IFileManager::Get().FileExists(*Path) || !FFileHelper::LoadFileToArray(Bytes, *Path))
    {
        return false;
    }
    FMemoryReader Reader(Bytes);
    FMindCore Loaded;
    Loaded.Serialize(Reader);
    if (Reader.IsError() || !Loaded.IsReady() || Loaded.Inputs.Num() != Loaded.Capacity * FMindSense::InputSize)
    {
        return false;
    }
    *this = MoveTemp(Loaded);
    return true;
}
