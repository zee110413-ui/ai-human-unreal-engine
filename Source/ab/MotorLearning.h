#pragma once

#include "CoreMinimal.h"
#include "Templates/UniquePtr.h"

struct AB_API FSkillStats
{
    int32 Falls = 0;
    int32 Steps = 0;
    int32 Goals = 0;
    int32 Reached = 0;
    int32 Drops = 0;
    int32 Holds = 0;
    int32 Crushes = 0;
    int32 Trips = 0;
    double Track = 0.0;
    double Speed = 0.0;
    double Want = 0.0;
    double Energy = 0.0;
    double Distance = 0.0;
    double Slip = 0.0;
    double Seconds = 0.0;
    double Settled = 0.0;
    int32 Samples = 0;
    int32 SettledSamples = 0;

    void Add(const FSkillStats& Other);
};

using FSkillDescribe = TFunction<FString(const FSkillStats& Stats)>;
using FSkillMastery = TFunction<float(const FSkillStats& Stats)>;

struct AB_API FDeepNet
{
    int32 Inputs = 0;
    int32 Hidden = 0;
    int32 Outputs = 0;
    TArray<float> Weights;

    void Init(int32 InInputs, int32 InHidden, int32 InOutputs, FRandomStream& Rng, float OutputScale);
    void Forward(const float* In, float* Out, float* Cache) const;
    void Backward(const float* In, const float* Cache, const float* OutGrad, float* Grad, float* Scratch) const;
    void Serialize(FArchive& Ar);
    int32 Size() const { return Weights.Num(); }
};

struct AB_API FMotorPolicy
{
    FDeepNet Actor;
    FDeepNet Critic;
    TArray<float> LogStd;
    TArray<float> NoiseVariance;
    TArray<float> ObsMean;
    TArray<float> ObsVar;
    double ObsCount = 0.0;
    int64 Experience = 0;
    float Difficulty = 0.0f;

    void Init(int32 Observations, int32 Actions, int32 HiddenSize, uint32 Seed, float InitialStd);
    bool IsReady() const { return Actor.Size() > 0; }
    int32 ObservationSize() const { return Actor.Inputs; }
    int32 ActionSize() const { return Actor.Outputs; }
    void RefreshNoise();
    void SampleNoise(FRandomStream& Rng, TArray<float>& Out) const;
    float TypicalSpread() const;
    void Normalize(const float* Raw, float* Out) const;
    void Act(const TArray<float>& Raw, TArray<float>& OutNormalized, TArray<float>& OutAction, float& OutLogProb, float& OutValue, const TArray<float>* Noise) const;
    float Evaluate(const TArray<float>& Raw) const;
    void Absorb(const float* Rows, int32 RowCount);
    void Serialize(FArchive& Ar);
    bool Save(const FString& Path);
    bool Load(const FString& Path);
};

class AB_API IMotorTask
{
public:
    virtual ~IMotorTask() = default;
    virtual void Reset(FRandomStream& Rng, float Difficulty) = 0;
    virtual void Observe(TArray<float>& Out) const = 0;
    virtual float Step(const TArray<float>& Action, FRandomStream& Rng, bool& bOutFailed, FSkillStats& Stats) = 0;
    virtual bool IsOver() const = 0;
};

using FMotorTaskMaker = TFunction<TUniquePtr<IMotorTask>()>;

struct AB_API FPracticePlan
{
    int32 Tasks = 48;
    int32 Horizon = 256;
    int32 Iterations = 400;
    int32 Epochs = 6;
    int32 Batch = 2048;
    float Gamma = 0.99f;
    float Lambda = 0.95f;
    float Clip = 0.2f;
    float ActorRate = 3.0e-4f;
    float CriticRate = 1.0e-3f;
    float Entropy = 0.0f;
    float TargetKl = 0.03f;
    float DifficultyStep = 0.02f;
    int32 NoiseEvery = 8;
    int32 Reports = 20;
};

class AB_API FMotorPractice
{
public:
    static void Train(FMotorPolicy& Policy, const FMotorTaskMaker& Maker, const FPracticePlan& Plan, uint32 Seed,
        const FSkillDescribe& Describe, const FSkillMastery& Mastery, FString* OutReport);
};

struct AB_API FMotorBuffer
{
    TArray<float> Obs;
    TArray<float> Raw;
    TArray<float> Act;
    TArray<float> LogP;
    TArray<float> Val;
    TArray<float> Rew;
    TArray<uint8> Ends;
    int32 ObsSize = 0;
    int32 ActSize = 0;
    int32 Room = 0;

    void Begin(int32 InObsSize, int32 InActSize, int32 InRoom);
    void Forget();
    int32 Num() const { return LogP.Num(); }
    bool IsFull() const { return Room > 0 && LogP.Num() >= Room; }
    void Add(const TArray<float>& Normalized, const TArray<float>& RawRow, const TArray<float>& Action, float LogProb, float Value, float Reward, bool bFailed);
};

struct AB_API FLifeLesson
{
    float ActorRate = 5.0e-5f;
    float CriticRate = 3.0e-4f;
    float Gamma = 0.99f;
    float Lambda = 0.95f;
    float Clip = 0.1f;
    float TargetKl = 0.012f;
    int32 Epochs = 2;
    int32 Batch = 256;

    static void Study(FMotorPolicy& Policy, const FMotorBuffer& Lived, float TailValue, const FLifeLesson& How);
};

struct AB_API FStepGround
{
    float Grip = 0.7f;
    float SinkCm = 0.0f;
    float Stick = 0.0f;
    float WaterCm = 0.0f;
    float Rough = 1.0f;
    FVector2D Rise = FVector2D::ZeroVector;
};

struct AB_API FWalkGrounds
{
    FStepGround Foot[2];
    FStepGround Aim;
};

struct AB_API FWalkFoot
{
    FVector2D At = FVector2D::ZeroVector;
    FVector2D Vel = FVector2D::ZeroVector;
    FVector2D From = FVector2D::ZeroVector;
    float Z = 0.0f;
    float Yaw = 0.0f;
    float Clock = 1.0f;
    float Sink = 0.0f;
    float Stuck = 0.0f;
    bool bDown = true;
};

struct AB_API FWalkBody
{
    float Leg = 0.9f;
    float Mass = 70.0f;
    float Strength = 1.0f;
    float FootFront = 0.17f;
    float FootBack = 0.06f;
    float FootSide = 0.045f;
    float Load = 0.0f;
    float Fatigue = 0.0f;

    FVector2D Com = FVector2D::ZeroVector;
    FVector2D Vel = FVector2D::ZeroVector;
    float Yaw = 0.0f;
    float YawRate = 0.0f;
    FWalkFoot Feet[2];
    FVector2D Cop = FVector2D::ZeroVector;
    bool bFallen = false;
    FVector2D FallDirection = FVector2D::ZeroVector;
    uint8 FallCause = 0;

    float Energy = 0.0f;
    float Slipped = 0.0f;
    float Climbed = 0.0f;
    int32 Landings = 0;
    int32 Trips = 0;
    FVector2D MeanVel = FVector2D::ZeroVector;

    void Place(const FVector2D& At, float InYaw, float Width);
    void Shift(const FVector2D& Offset);
    float Height() const;
    float Omega() const;
    FVector2D Capture() const;
    FVector2D Forward() const { return FVector2D(FMath::Cos(Yaw), FMath::Sin(Yaw)); }
    FVector2D Right() const { return FVector2D(-FMath::Sin(Yaw), FMath::Cos(Yaw)); }
    FVector2D ToBody(const FVector2D& World) const;
    int32 Lifted() const;
    int32 DownCount() const;
    FVector2D SupportPoint() const;
};

class AB_API FWalkSkill
{
public:
    static constexpr int32 Observations = 50;
    static constexpr int32 Actions = 8;
    static constexpr float Interval = 0.05f;

    static void Observe(const FWalkBody& Body, const FVector2D& Want, const FVector2D& Face, const FWalkGrounds& Grounds, TArray<float>& Out);
    static FVector2D AimPoint(const FWalkBody& Body);
    static void Simulate(FWalkBody& Body, const TArray<float>& Action, const FWalkGrounds& Grounds, float Seconds);
    static float Reward(const FWalkBody& Body, const FVector2D& Want, const FVector2D& Face);
    static float RewardFrom(float Mass, bool bFallen, float Yaw, const FVector2D& MeanVel, const FVector2D& Want, const FVector2D& Face,
        float Energy, float Slipped, int32 Trips);
    static float TopSpeed(float Difficulty, float Leg);
};

struct AB_API FReachBody
{
    float Trunk = 0.52f;
    float Shoulder = 0.19f;
    float UpperArm = 0.29f;
    float ForeArm = 0.27f;
    float Leg = 0.9f;
    float Mass = 70.0f;
    float Strength = 1.0f;

    float Pitch = 0.0f;
    float Squat = 0.0f;
    float Raise = 0.0f;
    float Spread = 0.1f;
    float Elbow = 0.2f;
    float Side = 1.0f;
    float Load = 0.0f;

    float PitchRate = 0.0f;
    float SquatRate = 0.0f;
    float RaiseRate = 0.0f;
    float SpreadRate = 0.0f;
    float ElbowRate = 0.0f;

    float Energy = 0.0f;
    float Wobble = 0.0f;
    bool bToppled = false;

    FVector ShoulderAt() const;
    FVector ElbowAt() const;
    FVector HandAt() const;
    float PelvisDrop() const;
    float PelvisBack() const;
    float BalanceOffset() const;
};

class AB_API FReachSkill
{
public:
    static constexpr int32 StateSize = 20;
    static constexpr int32 ActionSize = 5;
    static constexpr float Interval = 0.08f;

    static void Observe(const FReachBody& Body, const FVector& Goal, bool bActive, TArray<float>& Out);
    static void Simulate(FReachBody& Body, const TArray<float>& Action, float Seconds);
    static float Reward(const FReachBody& Body, const FVector& Goal, bool bActive, float DistanceBefore, float Seconds);
};

struct AB_API FGripState
{
    float Curl = 0.15f;
    float Contact = 0.6f;
    float Friction = 0.5f;
    float Mass = 1.0f;
    float Strength = 1.0f;
    float Lift = 0.0f;
    float Softness = 0.0f;
    bool bTouching = false;
    bool bHolding = false;
    bool bDropped = false;
    bool bCrushed = false;
    float Squeeze = 0.0f;
    float Energy = 0.0f;
};

class AB_API FGripSkill
{
public:
    static constexpr int32 StateSize = 9;
    static constexpr int32 ActionSize = 1;
    static constexpr float Interval = 0.05f;

    static void Observe(const FGripState& Grip, bool bWantHold, TArray<float>& Out);
    static void Simulate(FGripState& Grip, const TArray<float>& Action, float Seconds);
    static float Reward(const FGripState& Grip, bool bWantHold, float Seconds);
};

class AB_API FMotorSchool
{
public:
    static FString Folder();
    static void RaiseWalker(FMotorPolicy& Policy, const FWalkBody& Shape, int32 Iterations, uint32 Seed, FString* OutReport);
    static void RaiseReacher(FMotorPolicy& Policy, const FReachBody& Shape, int32 Iterations, uint32 Seed, FString* OutReport);
    static void RaiseGripper(FMotorPolicy& Policy, float Strength, int32 Iterations, uint32 Seed, FString* OutReport);
    static FString ExamineWalker(const FMotorPolicy& Policy, const FWalkBody& Shape, float Difficulty, uint32 Seed, TArray<FString>* OutTrace);
    static FString DescribeWalk(const FSkillStats& Stats, float Mass);
    static FString DescribeReach(const FSkillStats& Stats);
    static FString DescribeGrip(const FSkillStats& Stats);
    static bool Childhood(const FString& Skill, int32 Kind, FMotorPolicy& Out);
    static int32 KindOf(float Age, bool bFemale);
    static int32 ParentKind(int32 Kind);
    static void Grow(float Age, bool bFemale, float& OutLeg, float& OutMass, float& OutStrength);
    static void MakeWalkBody(float Age, bool bFemale, FWalkBody& Out);
    static void MakeReachBody(float Age, bool bFemale, float Side, FReachBody& Out);
};
