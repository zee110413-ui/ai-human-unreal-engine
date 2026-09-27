#include "MotorLearning.h"
#include "HumanTypes.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Serialization/Archive.h"
#include "Serialization/BufferArchive.h"
#include "Serialization/MemoryReader.h"
#include "Misc/FileHelper.h"
#include "Async/ParallelFor.h"

namespace
{
    constexpr int32 FileVersion = 5;
    constexpr int32 PolicyVersion = 2;
    constexpr float Gravity = 9.81f;

    float Gauss(FRandomStream& Rng)
    {
        const float U1 = FMath::Max(Rng.FRand(), 1.0e-6f);
        const float U2 = Rng.FRand();
        return FMath::Sqrt(-2.0f * FMath::Loge(U1)) * FMath::Cos(UE_TWO_PI * U2);
    }

    FVector2D ClosestOnSegment(const FVector2D& P, const FVector2D& A, const FVector2D& B)
    {
        const FVector2D AB = B - A;
        const float Len2 = AB.SizeSquared();
        if (Len2 < 1.0e-8f)
        {
            return A;
        }
        const float T = FMath::Clamp(FVector2D::DotProduct(P - A, AB) / Len2, 0.0f, 1.0f);
        return A + AB * T;
    }

    FVector Turn(const FVector& V, float Angle)
    {
        const float C = FMath::Cos(Angle);
        const float S = FMath::Sin(Angle);
        return FVector(V.X * C + V.Z * S, V.Y, -V.X * S + V.Z * C);
    }

    struct FAdam
    {
        TArray<float> M;
        TArray<float> V;
        int32 Steps = 0;

        void Step(TArray<float>& Params, const TArray<float>& Grad, float Rate)
        {
            if (M.Num() != Params.Num())
            {
                M.Init(0.0f, Params.Num());
                V.Init(0.0f, Params.Num());
                Steps = 0;
            }
            ++Steps;
            const float C1 = 1.0f - FMath::Pow(0.9f, static_cast<float>(FMath::Min(Steps, 2000)));
            const float C2 = 1.0f - FMath::Pow(0.999f, static_cast<float>(FMath::Min(Steps, 20000)));
            for (int32 I = 0; I < Params.Num(); ++I)
            {
                M[I] = 0.9f * M[I] + 0.1f * Grad[I];
                V[I] = 0.999f * V[I] + 0.001f * Grad[I] * Grad[I];
                Params[I] -= Rate * (M[I] / C1) / (FMath::Sqrt(V[I] / C2) + 1.0e-5f);
            }
        }
    };

    void ClipNorm(TArray<float>& Grad, float Limit)
    {
        double Sum = 0.0;
        for (const float G : Grad)
        {
            Sum += static_cast<double>(G) * G;
        }
        const double Norm = FMath::Sqrt(Sum);
        if (Norm > Limit)
        {
            const float Scale = static_cast<float>(Limit / Norm);
            for (float& G : Grad)
            {
                G *= Scale;
            }
        }
    }

    float PaceLevel(float Difficulty)
    {
        return FMath::Clamp(Difficulty * 2.5f, 0.0f, 1.0f);
    }

    float GroundLevel(float Difficulty)
    {
        return FMath::Clamp((Difficulty - 0.3f) / 0.7f, 0.0f, 1.0f);
    }

    FStepGround RandomGround(FRandomStream& Rng, float Difficulty)
    {
        FStepGround Patch;
        const float Harsh = GroundLevel(Difficulty);
        Patch.Rise = FVector2D(Rng.FRandRange(-1.0f, 1.0f), Rng.FRandRange(-1.0f, 1.0f)) * 0.15f * Harsh;
        if (Rng.FRand() >= Harsh * 0.7f)
        {
            Patch.Grip = Rng.FRandRange(0.6f, 0.85f);
            Patch.SinkCm = Rng.FRandRange(0.0f, 0.6f);
            Patch.Rough = Rng.FRandRange(0.5f, 2.0f);
            return Patch;
        }
        switch (Rng.RandRange(0, 6))
        {
        case 0: Patch.Grip = Rng.FRandRange(0.25f, 0.45f); Patch.SinkCm = Rng.FRandRange(3.0f, 15.0f); Patch.Stick = Rng.FRandRange(0.3f, 0.9f); break;
        case 1: Patch.Grip = Rng.FRandRange(0.12f, 0.25f); Patch.SinkCm = Rng.FRandRange(0.5f, 2.0f); Patch.Stick = 0.5f; break;
        case 2: Patch.Grip = 0.55f; Patch.SinkCm = Rng.FRandRange(1.0f, 4.0f); break;
        case 3: Patch.Grip = 0.3f; Patch.SinkCm = Rng.FRandRange(5.0f, 25.0f); Patch.Rough = 0.5f; break;
        case 4: Patch.Grip = Rng.FRandRange(0.06f, 0.12f); Patch.Rough = 0.2f; break;
        case 5: Patch.Grip = 0.4f; Patch.WaterCm = Rng.FRandRange(10.0f, 60.0f); Patch.SinkCm = 3.0f; break;
        default: Patch.Grip = 0.6f; Patch.Rough = Rng.FRandRange(2.0f, 5.0f); break;
        }
        Patch.Rise = FVector2D(Rng.FRandRange(-1.0f, 1.0f), Rng.FRandRange(-1.0f, 1.0f)) * 0.5f * Harsh;
        return Patch;
    }

    FVector2D ClampToFoot(const FWalkBody& Body, const FWalkFoot& Foot, const FVector2D& Point)
    {
        const FVector2D Along(FMath::Cos(Foot.Yaw), FMath::Sin(Foot.Yaw));
        const FVector2D Across(-Along.Y, Along.X);
        const FVector2D Rel = Point - Foot.At;
        const float X = FMath::Clamp(FVector2D::DotProduct(Rel, Along), -Body.FootBack, Body.FootFront);
        const float Y = FMath::Clamp(FVector2D::DotProduct(Rel, Across), -Body.FootSide, Body.FootSide);
        return Foot.At + Along * X + Across * Y;
    }

    bool Bears(const FWalkBody& Body, int32 Index)
    {
        const FWalkFoot& Foot = Body.Feet[Index];
        return Foot.bDown && FVector2D::DistSquared(Foot.At, Body.Com) <= FMath::Square(0.55f * Body.Leg);
    }

    FVector2D SupportClamp(const FWalkBody& Body, const FVector2D& Point)
    {
        const bool bLeft = Bears(Body, 0);
        const bool bRight = Bears(Body, 1);
        if (bLeft && bRight)
        {
            const FVector2D Line = ClosestOnSegment(Point, Body.Feet[0].At, Body.Feet[1].At);
            const FVector2D Fwd = Body.Forward();
            const float Off = FMath::Clamp(FVector2D::DotProduct(Point - Line, Fwd), -Body.FootBack, Body.FootFront);
            return Line + Fwd * Off;
        }
        if (bLeft || bRight)
        {
            return ClampToFoot(Body, Body.Feet[bLeft ? 0 : 1], Point);
        }
        return Body.Com;
    }

    class FWalkTask final : public IMotorTask
    {
    public:
        explicit FWalkTask(const FWalkBody& InShape)
            : Shape(InShape)
        {
        }

        virtual void Reset(FRandomStream& Rng, float InDifficulty) override
        {
            Difficulty = InDifficulty;
            const float Harsh = GroundLevel(Difficulty);
            Body = Shape;
            Body.Load = Rng.FRand() < 0.25f * Harsh ? Rng.FRandRange(0.05f, 0.45f) : 0.0f;
            Body.Fatigue = Rng.FRandRange(0.0f, 0.2f + 0.5f * Harsh);
            Body.Place(FVector2D::ZeroVector, Rng.FRandRange(-PI, PI), Rng.FRandRange(0.12f, 0.34f) * Shape.Leg);
            if (Rng.FRand() < 0.5f)
            {
                const float Angle = Rng.FRandRange(-PI, PI);
                Body.Vel = FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Rng.FRandRange(0.0f, FMath::Lerp(0.5f, 1.6f, PaceLevel(Difficulty)));
            }
            Want = FVector2D::ZeroVector;
            Face = FVector2D::ZeroVector;
            Drift = 0.0f;
            Steady = 0.0f;
            WantClock = Rng.FRandRange(0.3f, 2.0f);
            FieldSeed = Rng.GetUnsignedInt();
            FieldOffset = FVector2D(Rng.FRandRange(-50.0f, 50.0f), Rng.FRandRange(-50.0f, 50.0f));
            const float Steep = Rng.FRandRange(0.0f, 0.5f) * FMath::Clamp(Difficulty * 1.4f, 0.0f, 1.0f);
            const float Share = Rng.FRandRange(0.2f, 0.8f);
            for (int32 Wave = 0; Wave < 2; ++Wave)
            {
                const float Angle = Rng.FRandRange(-PI, PI);
                HillDir[Wave] = FVector2D(FMath::Cos(Angle), FMath::Sin(Angle));
                HillK[Wave] = 2.0f * PI / Rng.FRandRange(18.0f, 70.0f);
                HillPhase[Wave] = Rng.FRandRange(-PI, PI);
                HillSlope[Wave] = Steep * (Wave == 0 ? Share : 1.0f - Share);
            }
            bTilt = Rng.FRand() < 0.3f;
            const float TiltAngle = Rng.FRandRange(-PI, PI);
            Tilt = FVector2D(FMath::Cos(TiltAngle), FMath::Sin(TiltAngle)) * Steep;
            Clock = 0.0f;
            Limit = Rng.FRandRange(20.0f, 40.0f);
            Refresh();
        }

        virtual void Observe(TArray<float>& Out) const override
        {
            FWalkSkill::Observe(Body, Want, Face, Grounds, Out);
        }

        virtual float Step(const TArray<float>& Action, FRandomStream& Rng, bool& bOutFailed, FSkillStats& Stats) override
        {
            const float Pace = PaceLevel(Difficulty);
            WantClock -= FWalkSkill::Interval;
            Steady += FWalkSkill::Interval;
            if (WantClock <= 0.0f)
            {
                WantClock = Rng.FRandRange(1.0f, 6.0f);
                Steady = 0.0f;
                Face = FVector2D::ZeroVector;
                Drift = Rng.FRand() < 0.4f ? Rng.FRandRange(-0.5f, 0.5f) * Pace : 0.0f;
                if (Rng.FRand() < 0.25f)
                {
                    Want = FVector2D::ZeroVector;
                    Drift = 0.0f;
                    if (Rng.FRand() < 0.6f)
                    {
                        const float Look = Body.Yaw + Rng.FRandRange(-1.0f, 1.0f) * FMath::Lerp(0.8f, PI, Pace);
                        Face = FVector2D(FMath::Cos(Look), FMath::Sin(Look));
                    }
                }
                else
                {
                    const float Heading = Body.Yaw + Rng.FRandRange(-1.0f, 1.0f) * FMath::Lerp(0.6f, PI, Pace);
                    const float Speed = Rng.FRandRange(0.2f, FWalkSkill::TopSpeed(Difficulty, Shape.Leg));
                    Want = FVector2D(FMath::Cos(Heading), FMath::Sin(Heading)) * Speed;
                    if (Rng.FRand() < 0.15f * Pace)
                    {
                        const float Look = Heading + Rng.FRandRange(-1.0f, 1.0f);
                        Face = FVector2D(FMath::Cos(Look), FMath::Sin(Look));
                    }
                }
            }
            if (Drift != 0.0f && !Want.IsNearlyZero())
            {
                const float C = FMath::Cos(Drift * FWalkSkill::Interval);
                const float S = FMath::Sin(Drift * FWalkSkill::Interval);
                Want = FVector2D(Want.X * C - Want.Y * S, Want.X * S + Want.Y * C);
            }
            if (Rng.FRand() < FWalkSkill::Interval * FMath::Lerp(0.03f, 0.15f, Difficulty))
            {
                const float Angle = Rng.FRandRange(-PI, PI);
                Body.Vel += FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Rng.FRandRange(0.1f, FMath::Lerp(0.2f, 0.8f, Difficulty));
            }
            Refresh();
            const FVector2D Before = Body.Com;
            FWalkSkill::Simulate(Body, Action, Grounds, FWalkSkill::Interval);
            const float Reward = FWalkSkill::Reward(Body, Want, Face);
            Clock += FWalkSkill::Interval;
            bOutFailed = Body.bFallen;
            Stats.Falls += Body.bFallen ? 1 : 0;
            Stats.Steps += Body.Landings;
            Stats.Trips += Body.Trips;
            Stats.Track += (Body.MeanVel - Want).SizeSquared();
            if (Steady > 1.5f)
            {
                Stats.Settled += (Body.MeanVel - Want).SizeSquared();
                ++Stats.SettledSamples;
            }
            Stats.Speed += Body.MeanVel.Size();
            Stats.Want += Want.Size();
            Stats.Energy += Body.Energy;
            Stats.Distance += (Body.Com - Before).Size();
            Stats.Slip += Body.Slipped;
            Stats.Seconds += FWalkSkill::Interval;
            ++Stats.Samples;
            if (Body.Com.SizeSquared() > 400.0f)
            {
                const FVector2D Shift = Body.Com;
                Body.Shift(-Shift);
                FieldOffset += Shift;
            }
            return Reward;
        }

        virtual bool IsOver() const override
        {
            return Clock >= Limit;
        }

        const FWalkBody& GetBody() const { return Body; }
        const FVector2D& GetWant() const { return Want; }
        const FVector2D& GetFace() const { return Face; }

    private:
        FStepGround GroundAt(const FVector2D& Where) const
        {
            const FVector2D P = Where + FieldOffset;
            const int32 CX = FMath::FloorToInt(P.X / 1.2f);
            const int32 CY = FMath::FloorToInt(P.Y / 1.2f);
            FRandomStream Cell(static_cast<int32>(HashCombine(HashCombine(FieldSeed, GetTypeHash(CX)), GetTypeHash(CY))));
            FStepGround Ground = RandomGround(Cell, Difficulty);
            if (bTilt)
            {
                Ground.Rise += Tilt;
            }
            else
            {
                for (int32 Wave = 0; Wave < 2; ++Wave)
                {
                    Ground.Rise += HillDir[Wave] * (HillSlope[Wave] * FMath::Cos(HillK[Wave] * FVector2D::DotProduct(HillDir[Wave], P) + HillPhase[Wave]));
                }
            }
            return Ground;
        }

        void Refresh()
        {
            Grounds.Foot[0] = GroundAt(Body.Feet[0].At);
            Grounds.Foot[1] = GroundAt(Body.Feet[1].At);
            Grounds.Aim = GroundAt(FWalkSkill::AimPoint(Body));
        }

        FWalkBody Shape;
        FWalkBody Body;
        FWalkGrounds Grounds;
        FVector2D Want = FVector2D::ZeroVector;
        FVector2D Face = FVector2D::ZeroVector;
        FVector2D FieldOffset = FVector2D::ZeroVector;
        uint32 FieldSeed = 0;
        FVector2D HillDir[2] = { FVector2D(1.0f, 0.0f), FVector2D(0.0f, 1.0f) };
        float HillK[2] = { 0.1f, 0.1f };
        float HillPhase[2] = { 0.0f, 0.0f };
        float HillSlope[2] = { 0.0f, 0.0f };
        FVector2D Tilt = FVector2D::ZeroVector;
        bool bTilt = false;
        float WantClock = 0.0f;
        float Drift = 0.0f;
        float Steady = 0.0f;
        float Difficulty = 0.0f;
        float Clock = 0.0f;
        float Limit = 30.0f;
    };

    float WalkMastery(const FSkillStats& Stats)
    {
        const float Minutes = static_cast<float>(Stats.Seconds / 60.0);
        const float FallRate = Stats.Falls / FMath::Max(0.01f, Minutes);
        const float Track = Stats.SettledSamples > 0
            ? FMath::Sqrt(static_cast<float>(Stats.Settled / Stats.SettledSamples))
            : FMath::Sqrt(static_cast<float>(Stats.Track / FMath::Max(1, Stats.Samples)));
        if (FallRate < 2.0f && Track < 0.25f)
        {
            return 1.0f;
        }
        if (FallRate > 8.0f || Track > 0.5f)
        {
            return 0.0f;
        }
        return 0.5f;
    }
    class FReachTask final : public IMotorTask
    {
    public:
        explicit FReachTask(const FReachBody& InShape)
            : Shape(InShape)
        {
        }

        virtual void Reset(FRandomStream& Rng, float InDifficulty) override
        {
            Difficulty = InDifficulty;
            Body = Shape;
            Body.Side = Rng.FRand() < 0.5f ? -1.0f : 1.0f;
            Goal = Body.HandAt();
            bActive = false;
            GoalClock = Rng.FRandRange(0.3f, 1.2f);
            Clock = 0.0f;
            Limit = Rng.FRandRange(14.0f, 24.0f);
            bCounted = false;
        }

        virtual void Observe(TArray<float>& Out) const override
        {
            FReachSkill::Observe(Body, Goal, bActive, Out);
        }

        virtual float Step(const TArray<float>& Action, FRandomStream& Rng, bool& bOutFailed, FSkillStats& Stats) override
        {
            GoalClock -= FReachSkill::Interval;
            if (GoalClock <= 0.0f)
            {
                GoalClock = Rng.FRandRange(1.5f, 3.2f);
                bActive = Rng.FRand() < 0.85f;
                bCounted = false;
                if (bActive)
                {
                    const float Scale = Body.Leg / 0.93f;
                    const bool bLow = Rng.FRand() < 0.25f + 0.3f * Difficulty;
                    const float Height = bLow ? Rng.FRandRange(0.02f, 0.45f) * Scale : Rng.FRandRange(0.45f, 1.85f) * Scale;
                    Goal = FVector(Rng.FRandRange(0.1f, FMath::Lerp(0.45f, 0.78f, Difficulty)) * Scale,
                        Body.Side * Rng.FRandRange(-0.2f, 0.5f) * Scale, Height);
                    Body.Load = Rng.FRand() < 0.35f * Difficulty ? Rng.FRandRange(0.5f, 16.0f) * Body.Strength : 0.0f;
                    ++Stats.Goals;
                }
                else
                {
                    Goal = FVector(0.03f, Body.Side * 0.25f, Body.Leg * 0.8f);
                    Body.Load = 0.0f;
                }
            }
            const float Before = FVector::Dist(Body.HandAt(), Goal);
            FReachSkill::Simulate(Body, Action, FReachSkill::Interval);
            const float Reward = FReachSkill::Reward(Body, Goal, bActive, Before, FReachSkill::Interval);
            const float After = FVector::Dist(Body.HandAt(), Goal);
            Clock += FReachSkill::Interval;
            bOutFailed = Body.bToppled;
            Stats.Seconds += FReachSkill::Interval;
            Stats.Energy += Body.Energy;
            Stats.Falls += Body.bToppled ? 1 : 0;
            if (bActive)
            {
                if (GoalClock < 0.5f)
                {
                    Stats.Distance += After;
                    ++Stats.Samples;
                }
                if (!bCounted && After < 0.05f)
                {
                    bCounted = true;
                    ++Stats.Reached;
                }
            }
            if (Body.bToppled)
            {
                const float Side = Body.Side;
                Body = Shape;
                Body.Side = Side;
            }
            return Reward;
        }

        virtual bool IsOver() const override
        {
            return Clock >= Limit;
        }

    private:
        FReachBody Shape;
        FReachBody Body;
        FVector Goal = FVector::ZeroVector;
        float GoalClock = 0.0f;
        float Difficulty = 0.0f;
        float Clock = 0.0f;
        float Limit = 20.0f;
        bool bActive = false;
        bool bCounted = false;
    };

    float ReachMastery(const FSkillStats& Stats)
    {
        const float Near = static_cast<float>(Stats.Distance / FMath::Max(1, Stats.Samples));
        const float Hit = Stats.Goals > 0 ? static_cast<float>(Stats.Reached) / Stats.Goals : 0.0f;
        if (Near < 0.05f && Hit > 0.8f && Stats.Falls * 50 < Stats.Goals)
        {
            return 1.0f;
        }
        if (Near > 0.25f || Stats.Falls * 5 > Stats.Goals)
        {
            return 0.0f;
        }
        return 0.5f;
    }

    class FGripTask final : public IMotorTask
    {
    public:
        explicit FGripTask(float InStrength)
            : Strength(InStrength)
        {
        }

        virtual void Reset(FRandomStream& Rng, float InDifficulty) override
        {
            Difficulty = InDifficulty;
            Grip = FGripState();
            Grip.Strength = Strength;
            bWant = false;
            Phase = 4;
            Clock = 0.0f;
            Lived = 0.0f;
            Limit = Rng.FRandRange(14.0f, 22.0f);
        }

        virtual void Observe(TArray<float>& Out) const override
        {
            FGripSkill::Observe(Grip, bWant, Out);
        }

        virtual float Step(const TArray<float>& Action, FRandomStream& Rng, bool& bOutFailed, FSkillStats& Stats) override
        {
            Clock -= FGripSkill::Interval;
            if (Clock <= 0.0f)
            {
                Phase = (Phase + 1) % 5;
                switch (Phase)
                {
                case 0:
                    Grip.Contact = Rng.FRandRange(0.2f, 0.9f);
                    Grip.Friction = Rng.FRandRange(FMath::Lerp(0.5f, 0.12f, Difficulty), 0.9f);
                    Grip.Mass = Rng.FRand() < 0.5f ? Rng.FRandRange(0.05f, 2.0f) : Rng.FRandRange(2.0f, FMath::Lerp(4.0f, 18.0f, Difficulty));
                    Grip.Softness = Rng.FRand() < 0.3f * Difficulty ? Rng.FRandRange(0.3f, 0.9f) : 0.0f;
                    Grip.bTouching = false;
                    Grip.bHolding = false;
                    Grip.Lift = 0.0f;
                    bWant = false;
                    Clock = Rng.FRandRange(0.3f, 0.7f);
                    break;
                case 1:
                    Grip.bTouching = true;
                    bWant = true;
                    Clock = Rng.FRandRange(0.3f, 0.8f);
                    break;
                case 2:
                    Grip.Lift = Rng.FRandRange(0.5f, 5.0f);
                    Clock = Rng.FRandRange(0.4f, 1.4f);
                    ++Stats.Goals;
                    break;
                case 3:
                    if (Grip.bHolding)
                    {
                        ++Stats.Holds;
                    }
                    Grip.Lift = Rng.FRandRange(-0.5f, 0.5f);
                    Clock = Rng.FRandRange(0.5f, 1.6f);
                    break;
                default:
                    Grip.Lift = 0.0f;
                    bWant = false;
                    Clock = Rng.FRandRange(0.3f, 0.7f);
                    break;
                }
            }
            FGripSkill::Simulate(Grip, Action, FGripSkill::Interval);
            const float Reward = FGripSkill::Reward(Grip, bWant, FGripSkill::Interval);
            Lived += FGripSkill::Interval;
            Stats.Seconds += FGripSkill::Interval;
            Stats.Energy += Grip.Energy;
            Stats.Drops += Grip.bDropped ? 1 : 0;
            Stats.Crushes += Grip.bCrushed ? 1 : 0;
            ++Stats.Samples;
            bOutFailed = false;
            if (Grip.bDropped)
            {
                Grip.bTouching = false;
                Grip.bHolding = false;
                Phase = 4;
                Clock = 0.3f;
            }
            return Reward;
        }

        virtual bool IsOver() const override
        {
            return Lived >= Limit;
        }

    private:
        FGripState Grip;
        float Strength = 1.0f;
        float Difficulty = 0.0f;
        float Clock = 0.0f;
        float Lived = 0.0f;
        float Limit = 18.0f;
        int32 Phase = 4;
        bool bWant = false;
    };

    float GripMastery(const FSkillStats& Stats)
    {
        const float Kept = Stats.Goals > 0 ? static_cast<float>(Stats.Holds) / Stats.Goals : 0.0f;
        if (Kept > 0.9f && Stats.Drops * 20 < Stats.Goals)
        {
            return 1.0f;
        }
        if (Kept < 0.35f)
        {
            return 0.0f;
        }
        return 0.5f;
    }
}

void FSkillStats::Add(const FSkillStats& Other)
{
    Falls += Other.Falls;
    Steps += Other.Steps;
    Goals += Other.Goals;
    Reached += Other.Reached;
    Drops += Other.Drops;
    Holds += Other.Holds;
    Crushes += Other.Crushes;
    Trips += Other.Trips;
    Track += Other.Track;
    Speed += Other.Speed;
    Want += Other.Want;
    Energy += Other.Energy;
    Distance += Other.Distance;
    Slip += Other.Slip;
    Seconds += Other.Seconds;
    Samples += Other.Samples;
    Settled += Other.Settled;
    SettledSamples += Other.SettledSamples;
}


void FDeepNet::Init(int32 InInputs, int32 InHidden, int32 InOutputs, FRandomStream& Rng, float OutputScale)
{
    Inputs = InInputs;
    Hidden = InHidden;
    Outputs = InOutputs;
    Weights.Init(0.0f, Hidden * (Inputs + 1) + Hidden * (Hidden + 1) + Outputs * (Hidden + 1));
    int32 W = 0;
    auto Fill = [this, &W, &Rng](int32 Rows, int32 Cols, float Range)
    {
        for (int32 R = 0; R < Rows; ++R)
        {
            for (int32 C = 0; C < Cols; ++C)
            {
                Weights[W++] = Rng.FRandRange(-Range, Range);
            }
            Weights[W++] = 0.0f;
        }
    };
    Fill(Hidden, Inputs, FMath::Sqrt(6.0f / (Inputs + Hidden)));
    Fill(Hidden, Hidden, FMath::Sqrt(3.0f / Hidden));
    Fill(Outputs, Hidden, FMath::Sqrt(6.0f / (Hidden + Outputs)) * OutputScale);
}

void FDeepNet::Forward(const float* In, float* Out, float* Cache) const
{
    const float* W = Weights.GetData();
    float* First = Cache;
    float* Second = Cache + Hidden;
    for (int32 J = 0; J < Hidden; ++J)
    {
        float Sum = W[Inputs];
        for (int32 I = 0; I < Inputs; ++I)
        {
            Sum += W[I] * In[I];
        }
        First[J] = FMath::Tanh(Sum);
        W += Inputs + 1;
    }
    for (int32 K = 0; K < Hidden; ++K)
    {
        float Sum = W[Hidden];
        for (int32 J = 0; J < Hidden; ++J)
        {
            Sum += W[J] * First[J];
        }
        Second[K] = FMath::Tanh(Sum);
        W += Hidden + 1;
    }
    for (int32 O = 0; O < Outputs; ++O)
    {
        float Sum = W[Hidden];
        for (int32 K = 0; K < Hidden; ++K)
        {
            Sum += W[K] * Second[K];
        }
        Out[O] = Sum;
        W += Hidden + 1;
    }
}

void FDeepNet::Backward(const float* In, const float* Cache, const float* OutGrad, float* Grad, float* Scratch) const
{
    const float* First = Cache;
    const float* Second = Cache + Hidden;
    float* DSecond = Scratch;
    float* DFirst = Scratch + Hidden;
    const float* W = Weights.GetData();
    const int32 SecondStart = Hidden * (Inputs + 1);
    const int32 ThirdStart = SecondStart + Hidden * (Hidden + 1);

    for (int32 K = 0; K < Hidden; ++K)
    {
        DSecond[K] = 0.0f;
    }
    for (int32 O = 0; O < Outputs; ++O)
    {
        const float G = OutGrad[O];
        const int32 Row = ThirdStart + O * (Hidden + 1);
        for (int32 K = 0; K < Hidden; ++K)
        {
            Grad[Row + K] += G * Second[K];
            DSecond[K] += G * W[Row + K];
        }
        Grad[Row + Hidden] += G;
    }
    for (int32 J = 0; J < Hidden; ++J)
    {
        DFirst[J] = 0.0f;
    }
    for (int32 K = 0; K < Hidden; ++K)
    {
        const float G = DSecond[K] * (1.0f - Second[K] * Second[K]);
        const int32 Row = SecondStart + K * (Hidden + 1);
        for (int32 J = 0; J < Hidden; ++J)
        {
            Grad[Row + J] += G * First[J];
            DFirst[J] += G * W[Row + J];
        }
        Grad[Row + Hidden] += G;
    }
    for (int32 J = 0; J < Hidden; ++J)
    {
        const float G = DFirst[J] * (1.0f - First[J] * First[J]);
        const int32 Row = J * (Inputs + 1);
        for (int32 I = 0; I < Inputs; ++I)
        {
            Grad[Row + I] += G * In[I];
        }
        Grad[Row + Inputs] += G;
    }
}

void FDeepNet::Serialize(FArchive& Ar)
{
    Ar << Inputs << Hidden << Outputs << Weights;
}

void FMotorPolicy::Init(int32 Observations, int32 Actions, int32 HiddenSize, uint32 Seed, float InitialStd)
{
    FRandomStream Rng(static_cast<int32>(Seed));
    Actor.Init(Observations, HiddenSize, Actions, Rng, 0.1f);
    Critic.Init(Observations, HiddenSize, 1, Rng, 1.0f);
    LogStd.Init(FMath::Loge(InitialStd / FMath::Sqrt(HiddenSize * 0.35f)), Actions * HiddenSize);
    ObsMean.Init(0.0f, Observations);
    ObsVar.Init(1.0f, Observations);
    ObsCount = 1.0e-4;
    Experience = 0;
    Difficulty = 0.0f;
    RefreshNoise();
}

void FMotorPolicy::RefreshNoise()
{
    NoiseVariance.SetNum(LogStd.Num());
    for (int32 I = 0; I < LogStd.Num(); ++I)
    {
        NoiseVariance[I] = FMath::Exp(2.0f * LogStd[I]);
    }
}

void FMotorPolicy::SampleNoise(FRandomStream& Rng, TArray<float>& Out) const
{
    Out.SetNum(LogStd.Num());
    for (int32 I = 0; I < LogStd.Num(); ++I)
    {
        Out[I] = Gauss(Rng) * FMath::Sqrt(NoiseVariance[I]);
    }
}

float FMotorPolicy::TypicalSpread() const
{
    double Sum = 0.0;
    for (const float V : NoiseVariance)
    {
        Sum += V;
    }
    const int32 Actions = FMath::Max(1, Actor.Outputs);
    return static_cast<float>(FMath::Sqrt(Sum / Actions * 0.35));
}

void FMotorPolicy::Normalize(const float* Raw, float* Out) const
{
    for (int32 I = 0; I < ObsMean.Num(); ++I)
    {
        Out[I] = FMath::Clamp((Raw[I] - ObsMean[I]) / FMath::Sqrt(FMath::Max(ObsVar[I], 0.01f)), -6.0f, 6.0f);
    }
}

void FMotorPolicy::Act(const TArray<float>& Raw, TArray<float>& OutNormalized, TArray<float>& OutAction, float& OutLogProb, float& OutValue, const TArray<float>* Noise) const
{
    OutNormalized.SetNum(Actor.Inputs);
    Normalize(Raw.GetData(), OutNormalized.GetData());
    OutAction.SetNum(Actor.Outputs);
    TArray<float, TInlineAllocator<256>> Cache;
    Cache.SetNum(2 * FMath::Max(Actor.Hidden, Critic.Hidden));
    Actor.Forward(OutNormalized.GetData(), OutAction.GetData(), Cache.GetData());
    OutLogProb = 0.0f;
    if (Noise && Noise->Num() == NoiseVariance.Num())
    {
        const int32 Hidden = Actor.Hidden;
        const float* Features = Cache.GetData() + Hidden;
        for (int32 J = 0; J < OutAction.Num(); ++J)
        {
            const float* Row = Noise->GetData() + J * Hidden;
            const float* Var = NoiseVariance.GetData() + J * Hidden;
            float Shift = 0.0f;
            float Variance = 1.0e-6f;
            for (int32 K = 0; K < Hidden; ++K)
            {
                const float F = Features[K];
                Shift += Row[K] * F;
                Variance += Var[K] * F * F;
            }
            OutAction[J] += Shift;
            const float Z2 = Shift * Shift / Variance;
            OutLogProb += -0.5f * Z2 - 0.5f * FMath::Loge(Variance) - 0.9189385f;
        }
    }
    OutValue = 0.0f;
    Critic.Forward(OutNormalized.GetData(), &OutValue, Cache.GetData());
}

float FMotorPolicy::Evaluate(const TArray<float>& Raw) const
{
    TArray<float, TInlineAllocator<128>> Normalized;
    Normalized.SetNum(Critic.Inputs);
    Normalize(Raw.GetData(), Normalized.GetData());
    TArray<float, TInlineAllocator<256>> Cache;
    Cache.SetNum(2 * Critic.Hidden);
    float Value = 0.0f;
    Critic.Forward(Normalized.GetData(), &Value, Cache.GetData());
    return Value;
}

void FMotorPolicy::Absorb(const float* Rows, int32 RowCount)
{
    if (RowCount <= 0)
    {
        return;
    }
    const int32 Size = ObsMean.Num();
    for (int32 D = 0; D < Size; ++D)
    {
        double Sum = 0.0;
        double Sq = 0.0;
        for (int32 R = 0; R < RowCount; ++R)
        {
            const double V = Rows[R * Size + D];
            Sum += V;
            Sq += V * V;
        }
        const double BatchMean = Sum / RowCount;
        const double BatchVar = FMath::Max(0.0, Sq / RowCount - BatchMean * BatchMean);
        const double Total = ObsCount + RowCount;
        const double Delta = BatchMean - ObsMean[D];
        ObsMean[D] = static_cast<float>(ObsMean[D] + Delta * RowCount / Total);
        ObsVar[D] = static_cast<float>((ObsVar[D] * ObsCount + BatchVar * RowCount + Delta * Delta * ObsCount * RowCount / Total) / Total);
    }
    ObsCount = FMath::Min(ObsCount + RowCount, 2.0e6);
}

void FMotorPolicy::Serialize(FArchive& Ar)
{
    int32 Version = PolicyVersion;
    Ar << Version;
    if (Version != PolicyVersion)
    {
        Ar.SetError();
        return;
    }
    Actor.Serialize(Ar);
    Critic.Serialize(Ar);
    Ar << LogStd << ObsMean << ObsVar << ObsCount << Experience << Difficulty;
}

bool FMotorPolicy::Save(const FString& Path)
{
    FBufferArchive Writer;
    Serialize(Writer);
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
    return FFileHelper::SaveArrayToFile(Writer, *Path);
}

bool FMotorPolicy::Load(const FString& Path)
{
    TArray<uint8> Bytes;
    if (!IFileManager::Get().FileExists(*Path) || !FFileHelper::LoadFileToArray(Bytes, *Path))
    {
        return false;
    }
    FMemoryReader Reader(Bytes);
    FMotorPolicy Loaded;
    Loaded.Serialize(Reader);
    if (Reader.IsError() || !Loaded.IsReady() || Loaded.LogStd.Num() != Loaded.Actor.Outputs * Loaded.Actor.Hidden
        || Loaded.ObsMean.Num() != Loaded.Actor.Inputs)
    {
        return false;
    }
    *this = MoveTemp(Loaded);
    RefreshNoise();
    return true;
}

void FMotorPractice::Train(FMotorPolicy& Policy, const FMotorTaskMaker& Maker, const FPracticePlan& Plan, uint32 Seed,
    const FSkillDescribe& Describe, const FSkillMastery& Mastery, FString* OutReport)
{
    const int32 ObsSize = Policy.ObservationSize();
    const int32 ActSize = Policy.ActionSize();
    const int32 Count = Plan.Tasks * Plan.Horizon;
    const int32 Chunks = 12;

    TArray<TUniquePtr<IMotorTask>> Tasks;
    TArray<FRandomStream> Streams;
    for (int32 T = 0; T < Plan.Tasks; ++T)
    {
        Tasks.Add(Maker());
        Streams.Emplace(static_cast<int32>(Seed + 7919u * static_cast<uint32>(T + 1)));
        Tasks[T]->Reset(Streams[T], Policy.Difficulty);
    }

    TArray<float> Obs;
    TArray<float> Raw;
    TArray<float> Act;
    Obs.SetNumZeroed(Count * ObsSize);
    Raw.SetNumZeroed(Count * ObsSize);
    Act.SetNumZeroed(Count * ActSize);
    TArray<float> LogP;
    TArray<float> Val;
    TArray<float> Rew;
    TArray<float> Boot;
    TArray<float> Adv;
    TArray<float> Ret;
    LogP.SetNumZeroed(Count);
    Val.SetNumZeroed(Count);
    Rew.SetNumZeroed(Count);
    Boot.SetNumZeroed(Count);
    Adv.SetNumZeroed(Count);
    Ret.SetNumZeroed(Count);
    TArray<uint8> Ends;
    Ends.SetNumZeroed(Count);
    TArray<float> LastValue;
    LastValue.SetNumZeroed(Plan.Tasks);
    TArray<FSkillStats> TaskStats;
    TaskStats.SetNum(Plan.Tasks);
    TArray<TArray<float>> TaskNoise;
    TArray<int32> NoiseAge;
    TaskNoise.SetNum(Plan.Tasks);
    NoiseAge.Init(1 << 20, Plan.Tasks);
    const int32 Hidden = Policy.Actor.Hidden;
    const int32 NoiseSize = ActSize * Hidden;
    Policy.RefreshNoise();

    FAdam ActorAdam;
    FAdam StdAdam;
    FAdam CriticAdam;
    FRandomStream Shuffle(static_cast<int32>(Seed ^ 0x68e31da4u));
    TArray<int32> Order;
    Order.SetNum(Count);
    TArray<TArray<float>> ActorParts;
    TArray<TArray<float>> StdParts;
    TArray<TArray<float>> CriticParts;
    ActorParts.SetNum(Chunks);
    StdParts.SetNum(Chunks);
    CriticParts.SetNum(Chunks);
    TArray<double> KlParts;
    TArray<double> ClipParts;
    TArray<float> ActorGrad;
    TArray<float> StdGrad;
    TArray<float> CriticGrad;

    FSkillStats Window;
    double WindowReward = 0.0;
    double WindowKl = 0.0;
    int32 WindowIterations = 0;
    const int32 ReportEvery = FMath::Max(1, Plan.Iterations / FMath::Max(1, Plan.Reports));
    double Lived = 0.0;

    for (int32 Iteration = 0; Iteration < Plan.Iterations; ++Iteration)
    {
        const float Difficulty = Policy.Difficulty;
        const FMotorPolicy& Frozen = Policy;
        ParallelFor(Plan.Tasks, [&](int32 T)
        {
            FRandomStream& Rng = Streams[T];
            IMotorTask& Task = *Tasks[T];
            FSkillStats& Stats = TaskStats[T];
            Stats = FSkillStats();
            TArray<float> Observation;
            TArray<float> Normalized;
            TArray<float> Action;
            TArray<float> Clipped;
            TArray<float>& Noise = TaskNoise[T];
            for (int32 Step = 0; Step < Plan.Horizon; ++Step)
            {
                const int32 Index = T * Plan.Horizon + Step;
                if (NoiseAge[T] >= Plan.NoiseEvery || Noise.Num() != NoiseSize)
                {
                    Frozen.SampleNoise(Rng, Noise);
                    NoiseAge[T] = 0;
                }
                ++NoiseAge[T];
                Task.Observe(Observation);
                float LogProb = 0.0f;
                float Value = 0.0f;
                Frozen.Act(Observation, Normalized, Action, LogProb, Value, &Noise);
                FMemory::Memcpy(&Raw[Index * ObsSize], Observation.GetData(), sizeof(float) * ObsSize);
                FMemory::Memcpy(&Obs[Index * ObsSize], Normalized.GetData(), sizeof(float) * ObsSize);
                FMemory::Memcpy(&Act[Index * ActSize], Action.GetData(), sizeof(float) * ActSize);
                LogP[Index] = LogProb;
                Val[Index] = Value;
                Clipped = Action;
                for (float& V : Clipped)
                {
                    V = FMath::Clamp(V, -1.0f, 1.0f);
                }
                bool bFailed = false;
                Rew[Index] = Task.Step(Clipped, Rng, bFailed, Stats);
                Ends[Index] = 0;
                Boot[Index] = 0.0f;
                if (bFailed)
                {
                    Ends[Index] = 1;
                    Task.Reset(Rng, Difficulty);
                    NoiseAge[T] = Plan.NoiseEvery;
                }
                else if (Task.IsOver())
                {
                    Ends[Index] = 2;
                    Task.Observe(Observation);
                    Boot[Index] = Frozen.Evaluate(Observation);
                    Task.Reset(Rng, Difficulty);
                    NoiseAge[T] = Plan.NoiseEvery;
                }
            }
            Task.Observe(Observation);
            LastValue[T] = Frozen.Evaluate(Observation);
        });

        for (int32 T = 0; T < Plan.Tasks; ++T)
        {
            float NextAdv = 0.0f;
            float NextValue = LastValue[T];
            for (int32 Step = Plan.Horizon - 1; Step >= 0; --Step)
            {
                const int32 Index = T * Plan.Horizon + Step;
                if (Ends[Index] == 1)
                {
                    NextValue = 0.0f;
                    NextAdv = 0.0f;
                }
                else if (Ends[Index] == 2)
                {
                    NextValue = Boot[Index];
                    NextAdv = 0.0f;
                }
                const float Delta = Rew[Index] + Plan.Gamma * NextValue - Val[Index];
                NextAdv = Delta + Plan.Gamma * Plan.Lambda * NextAdv;
                Adv[Index] = NextAdv;
                Ret[Index] = NextAdv + Val[Index];
                NextValue = Val[Index];
            }
        }

        double AdvMean = 0.0;
        double AdvSq = 0.0;
        for (const float A : Adv)
        {
            AdvMean += A;
            AdvSq += static_cast<double>(A) * A;
        }
        AdvMean /= Count;
        const double AdvStd = FMath::Sqrt(FMath::Max(1.0e-8, AdvSq / Count - AdvMean * AdvMean));
        for (float& A : Adv)
        {
            A = static_cast<float>((A - AdvMean) / AdvStd);
        }

        for (int32 I = 0; I < Count; ++I)
        {
            Order[I] = I;
        }
        const int32 ActorSize = Policy.Actor.Size();
        const int32 CriticSize = Policy.Critic.Size();
        double IterationKl = 0.0;
        int32 IterationBatches = 0;
        bool bStop = false;
        for (int32 Epoch = 0; Epoch < Plan.Epochs && !bStop; ++Epoch)
        {
            for (int32 I = Count - 1; I > 0; --I)
            {
                Order.Swap(I, Shuffle.RandRange(0, I));
            }
            double EpochKl = 0.0;
            int32 EpochBatches = 0;
            for (int32 Start = 0; Start + Plan.Batch <= Count; Start += Plan.Batch)
            {
                KlParts.Init(0.0, Chunks);
                ClipParts.Init(0.0, Chunks);
                ParallelFor(Chunks, [&](int32 C)
                {
                    TArray<float>& AG = ActorParts[C];
                    TArray<float>& SG = StdParts[C];
                    TArray<float>& CG = CriticParts[C];
                    AG.Init(0.0f, ActorSize);
                    SG.Init(0.0f, NoiseSize);
                    CG.Init(0.0f, CriticSize);
                    TArray<float, TInlineAllocator<256>> Cache;
                    TArray<float, TInlineAllocator<256>> Scratch;
                    TArray<float, TInlineAllocator<256>> CriticCache;
                    TArray<float, TInlineAllocator<256>> CriticScratch;
                    TArray<float, TInlineAllocator<16>> Mean;
                    TArray<float, TInlineAllocator<16>> OutGrad;
                    TArray<float, TInlineAllocator<16>> Var;
                    TArray<float, TInlineAllocator<16>> Zs;
                    Cache.SetNum(2 * Hidden);
                    Scratch.SetNum(2 * Hidden);
                    CriticCache.SetNum(2 * Policy.Critic.Hidden);
                    CriticScratch.SetNum(2 * Policy.Critic.Hidden);
                    Mean.SetNum(ActSize);
                    OutGrad.SetNum(ActSize);
                    Var.SetNum(ActSize);
                    Zs.SetNum(ActSize);
                    const float* Sigma2 = Policy.NoiseVariance.GetData();
                    const float InvBatch = 1.0f / Plan.Batch;
                    const int32 From = Start + Plan.Batch * C / Chunks;
                    const int32 To = Start + Plan.Batch * (C + 1) / Chunks;
                    for (int32 K = From; K < To; ++K)
                    {
                        const int32 I = Order[K];
                        const float* X = &Obs[I * ObsSize];
                        const float* A = &Act[I * ActSize];
                        Policy.Actor.Forward(X, Mean.GetData(), Cache.GetData());
                        const float* Features = Cache.GetData() + Hidden;
                        float NewLogP = 0.0f;
                        for (int32 J = 0; J < ActSize; ++J)
                        {
                            float V = 1.0e-6f;
                            const float* Row = Sigma2 + J * Hidden;
                            for (int32 H = 0; H < Hidden; ++H)
                            {
                                V += Row[H] * Features[H] * Features[H];
                            }
                            Var[J] = V;
                            const float Z = (A[J] - Mean[J]) / FMath::Sqrt(V);
                            Zs[J] = Z;
                            NewLogP += -0.5f * Z * Z - 0.5f * FMath::Loge(V) - 0.9189385f;
                        }
                        const float LogRatio = FMath::Clamp(NewLogP - LogP[I], -20.0f, 20.0f);
                        const float Ratio = FMath::Exp(LogRatio);
                        KlParts[C] += (Ratio - 1.0f) - LogRatio;
                        const float Advantage = Adv[I];
                        const bool bActive = Advantage >= 0.0f ? Ratio < 1.0f + Plan.Clip : Ratio > 1.0f - Plan.Clip;
                        if (bActive)
                        {
                            const float Coef = -Advantage * Ratio * InvBatch;
                            for (int32 J = 0; J < ActSize; ++J)
                            {
                                const float Z = Zs[J];
                                OutGrad[J] = Coef * Z / FMath::Sqrt(Var[J]);
                                const float Scale = Coef * (Z * Z - 1.0f) / Var[J];
                                const float* Row = Sigma2 + J * Hidden;
                                float* Out = SG.GetData() + J * Hidden;
                                for (int32 H = 0; H < Hidden; ++H)
                                {
                                    Out[H] += Scale * Row[H] * Features[H] * Features[H];
                                }
                            }
                            Policy.Actor.Backward(X, Cache.GetData(), OutGrad.GetData(), AG.GetData(), Scratch.GetData());
                        }
                        else
                        {
                            ClipParts[C] += 1.0;
                        }
                        float Value = 0.0f;
                        Policy.Critic.Forward(X, &Value, CriticCache.GetData());
                        const float ValueGrad = (Value - Ret[I]) * InvBatch;
                        Policy.Critic.Backward(X, CriticCache.GetData(), &ValueGrad, CG.GetData(), CriticScratch.GetData());
                    }
                });

                ActorGrad.Init(0.0f, ActorSize);
                StdGrad.Init(0.0f, NoiseSize);
                CriticGrad.Init(0.0f, CriticSize);
                double BatchKl = 0.0;
                for (int32 C = 0; C < Chunks; ++C)
                {
                    for (int32 W = 0; W < ActorSize; ++W)
                    {
                        ActorGrad[W] += ActorParts[C][W];
                    }
                    for (int32 W = 0; W < NoiseSize; ++W)
                    {
                        StdGrad[W] += StdParts[C][W];
                    }
                    for (int32 W = 0; W < CriticSize; ++W)
                    {
                        CriticGrad[W] += CriticParts[C][W];
                    }
                    BatchKl += KlParts[C];
                }
                for (float& G : StdGrad)
                {
                    G -= Plan.Entropy;
                }
                ClipNorm(ActorGrad, 0.5f);
                ClipNorm(StdGrad, 0.5f);
                ClipNorm(CriticGrad, 5.0f);
                ActorAdam.Step(Policy.Actor.Weights, ActorGrad, Plan.ActorRate);
                StdAdam.Step(Policy.LogStd, StdGrad, Plan.ActorRate);
                for (float& S : Policy.LogStd)
                {
                    S = FMath::Clamp(S, -5.0f, -0.5f);
                }
                Policy.RefreshNoise();
                CriticAdam.Step(Policy.Critic.Weights, CriticGrad, Plan.CriticRate);
                EpochKl += BatchKl / Plan.Batch;
                ++EpochBatches;
            }
            if (EpochBatches > 0)
            {
                IterationKl = EpochKl / EpochBatches;
                ++IterationBatches;
                if (IterationKl > Plan.TargetKl)
                {
                    bStop = true;
                }
            }
        }

        Policy.Absorb(Raw.GetData(), Count);
        Policy.Experience += Count;

        FSkillStats Total;
        for (const FSkillStats& Stats : TaskStats)
        {
            Total.Add(Stats);
        }
        Lived += Total.Seconds;
        double RewardSum = 0.0;
        for (const float R : Rew)
        {
            RewardSum += R;
        }
        Window.Add(Total);
        WindowReward += RewardSum / Count;
        WindowKl += IterationKl;
        ++WindowIterations;

        const float Score = Mastery ? Mastery(Total) : 0.5f;
        if (Score >= 0.8f)
        {
            Policy.Difficulty = FMath::Min(1.0f, Policy.Difficulty + Plan.DifficultyStep);
        }
        else if (Score <= 0.2f)
        {
            Policy.Difficulty = FMath::Max(0.0f, Policy.Difficulty - Plan.DifficultyStep);
        }

        if (Iteration == 0 || (Iteration + 1) % ReportEvery == 0 || Iteration + 1 == Plan.Iterations)
        {
            const float Spread = Policy.TypicalSpread();
            const FString Line = FString::Printf(TEXT("[%4d] трудность %.2f, прожито %.2f ч, награда %.3f, разброс %.2f, KL %.4f | %s"),
                Iteration + 1, Policy.Difficulty, Lived / 3600.0, WindowReward / WindowIterations, Spread,
                WindowKl / WindowIterations, Describe ? *Describe(Window) : TEXT(""));
            UE_LOG(LogHumanCity, Display, TEXT("MOTOR %s"), *Line);
            if (OutReport)
            {
                *OutReport += Line + TEXT("\n");
            }
            Window = FSkillStats();
            WindowReward = 0.0;
            WindowKl = 0.0;
            WindowIterations = 0;
        }
    }
}

void FMotorBuffer::Begin(int32 InObsSize, int32 InActSize, int32 InRoom)
{
    ObsSize = InObsSize;
    ActSize = InActSize;
    Room = InRoom;
    Forget();
    Obs.Reserve(Room * ObsSize);
    Raw.Reserve(Room * ObsSize);
    Act.Reserve(Room * ActSize);
    LogP.Reserve(Room);
    Val.Reserve(Room);
    Rew.Reserve(Room);
    Ends.Reserve(Room);
}

void FMotorBuffer::Forget()
{
    Obs.Reset();
    Raw.Reset();
    Act.Reset();
    LogP.Reset();
    Val.Reset();
    Rew.Reset();
    Ends.Reset();
}

void FMotorBuffer::Add(const TArray<float>& Normalized, const TArray<float>& RawRow, const TArray<float>& Action, float LogProb, float Value, float Reward, bool bFailed)
{
    if (Normalized.Num() != ObsSize || RawRow.Num() != ObsSize || Action.Num() != ActSize)
    {
        return;
    }
    Obs.Append(Normalized);
    Raw.Append(RawRow);
    Act.Append(Action);
    LogP.Add(LogProb);
    Val.Add(Value);
    Rew.Add(Reward);
    Ends.Add(bFailed ? 1 : 0);
}

void FLifeLesson::Study(FMotorPolicy& Policy, const FMotorBuffer& Lived, float TailValue, const FLifeLesson& How)
{
    const int32 Count = Lived.Num();
    const int32 ObsSize = Policy.ObservationSize();
    const int32 ActSize = Policy.ActionSize();
    const int32 Hidden = Policy.Actor.Hidden;
    if (Count < How.Batch || Lived.ObsSize != ObsSize || Lived.ActSize != ActSize)
    {
        return;
    }

    TArray<float> Adv;
    TArray<float> Ret;
    Adv.SetNumZeroed(Count);
    Ret.SetNumZeroed(Count);
    float NextAdv = 0.0f;
    float NextValue = TailValue;
    for (int32 Step = Count - 1; Step >= 0; --Step)
    {
        if (Lived.Ends[Step] == 1)
        {
            NextValue = 0.0f;
            NextAdv = 0.0f;
        }
        const float Delta = Lived.Rew[Step] + How.Gamma * NextValue - Lived.Val[Step];
        NextAdv = Delta + How.Gamma * How.Lambda * NextAdv;
        Adv[Step] = NextAdv;
        Ret[Step] = NextAdv + Lived.Val[Step];
        NextValue = Lived.Val[Step];
    }
    double Mean = 0.0;
    double Square = 0.0;
    for (const float A : Adv)
    {
        Mean += A;
        Square += static_cast<double>(A) * A;
    }
    Mean /= Count;
    const double Spread = FMath::Sqrt(FMath::Max(1.0e-8, Square / Count - Mean * Mean));
    for (float& A : Adv)
    {
        A = static_cast<float>((A - Mean) / Spread);
    }

    FAdam ActorAdam;
    FAdam StdAdam;
    FAdam CriticAdam;
    FRandomStream Shuffle(static_cast<int32>(Policy.Experience & 0x7fffffff));
    TArray<int32> Order;
    Order.SetNum(Count);
    TArray<float> ActorGrad;
    TArray<float> StdGrad;
    TArray<float> CriticGrad;
    TArray<float> Cache;
    TArray<float> Scratch;
    TArray<float> CriticCache;
    TArray<float> CriticScratch;
    TArray<float> MeanOut;
    TArray<float> OutGrad;
    TArray<float> Var;
    TArray<float> Zs;
    Cache.SetNumZeroed(2 * Hidden);
    Scratch.SetNumZeroed(2 * Hidden);
    CriticCache.SetNumZeroed(2 * Policy.Critic.Hidden);
    CriticScratch.SetNumZeroed(2 * Policy.Critic.Hidden);
    MeanOut.SetNumZeroed(ActSize);
    OutGrad.SetNumZeroed(ActSize);
    Var.SetNumZeroed(ActSize);
    Zs.SetNumZeroed(ActSize);

    for (int32 Epoch = 0; Epoch < How.Epochs; ++Epoch)
    {
        for (int32 I = 0; I < Count; ++I)
        {
            Order[I] = I;
        }
        for (int32 I = Count - 1; I > 0; --I)
        {
            Order.Swap(I, Shuffle.RandRange(0, I));
        }
        double EpochKl = 0.0;
        int32 Batches = 0;
        for (int32 Start = 0; Start + How.Batch <= Count; Start += How.Batch)
        {
            ActorGrad.Init(0.0f, Policy.Actor.Size());
            StdGrad.Init(0.0f, Policy.LogStd.Num());
            CriticGrad.Init(0.0f, Policy.Critic.Size());
            const float InvBatch = 1.0f / How.Batch;
            const float* Sigma2 = Policy.NoiseVariance.GetData();
            double BatchKl = 0.0;
            for (int32 K = Start; K < Start + How.Batch; ++K)
            {
                const int32 I = Order[K];
                const float* X = &Lived.Obs[I * ObsSize];
                const float* A = &Lived.Act[I * ActSize];
                Policy.Actor.Forward(X, MeanOut.GetData(), Cache.GetData());
                const float* Features = Cache.GetData() + Hidden;
                float NewLogP = 0.0f;
                for (int32 J = 0; J < ActSize; ++J)
                {
                    float V = 1.0e-6f;
                    const float* Row = Sigma2 + J * Hidden;
                    for (int32 H = 0; H < Hidden; ++H)
                    {
                        V += Row[H] * Features[H] * Features[H];
                    }
                    Var[J] = V;
                    Zs[J] = (A[J] - MeanOut[J]) / FMath::Sqrt(V);
                    NewLogP += -0.5f * Zs[J] * Zs[J] - 0.5f * FMath::Loge(V) - 0.9189385f;
                }
                const float LogRatio = FMath::Clamp(NewLogP - Lived.LogP[I], -20.0f, 20.0f);
                const float Ratio = FMath::Exp(LogRatio);
                BatchKl += (Ratio - 1.0f) - LogRatio;
                const float Advantage = Adv[I];
                const bool bActive = Advantage >= 0.0f ? Ratio < 1.0f + How.Clip : Ratio > 1.0f - How.Clip;
                if (bActive)
                {
                    const float Coef = -Advantage * Ratio * InvBatch;
                    for (int32 J = 0; J < ActSize; ++J)
                    {
                        OutGrad[J] = Coef * Zs[J] / FMath::Sqrt(Var[J]);
                        const float Scale = Coef * (Zs[J] * Zs[J] - 1.0f) / Var[J];
                        const float* Row = Sigma2 + J * Hidden;
                        float* Out = StdGrad.GetData() + J * Hidden;
                        for (int32 H = 0; H < Hidden; ++H)
                        {
                            Out[H] += Scale * Row[H] * Features[H] * Features[H];
                        }
                    }
                    Policy.Actor.Backward(X, Cache.GetData(), OutGrad.GetData(), ActorGrad.GetData(), Scratch.GetData());
                }
                float Value = 0.0f;
                Policy.Critic.Forward(X, &Value, CriticCache.GetData());
                const float ValueGrad = (Value - Ret[I]) * InvBatch;
                Policy.Critic.Backward(X, CriticCache.GetData(), &ValueGrad, CriticGrad.GetData(), CriticScratch.GetData());
            }
            ClipNorm(ActorGrad, 0.3f);
            ClipNorm(StdGrad, 0.3f);
            ClipNorm(CriticGrad, 3.0f);
            ActorAdam.Step(Policy.Actor.Weights, ActorGrad, How.ActorRate);
            StdAdam.Step(Policy.LogStd, StdGrad, How.ActorRate);
            for (float& S : Policy.LogStd)
            {
                S = FMath::Clamp(S, -5.0f, -0.5f);
            }
            Policy.RefreshNoise();
            CriticAdam.Step(Policy.Critic.Weights, CriticGrad, How.CriticRate);
            EpochKl += BatchKl / How.Batch;
            ++Batches;
        }
        if (Batches > 0 && EpochKl / Batches > How.TargetKl)
        {
            break;
        }
    }
    Policy.Absorb(Lived.Raw.GetData(), Count);
    Policy.Experience += Count;
}

void FWalkBody::Place(const FVector2D& At, float InYaw, float Width)
{
    Com = At;
    Vel = FVector2D::ZeroVector;
    Yaw = InYaw;
    YawRate = 0.0f;
    const FVector2D Side = Right() * (Width * 0.5f);
    for (int32 Index = 0; Index < 2; ++Index)
    {
        FWalkFoot& Foot = Feet[Index];
        Foot = FWalkFoot();
        Foot.At = Index == 0 ? At - Side : At + Side;
        Foot.From = Foot.At;
        Foot.Yaw = InYaw;
    }
    Cop = At;
    bFallen = false;
    FallDirection = FVector2D::ZeroVector;
    FallCause = 0;
    Energy = 0.0f;
    Slipped = 0.0f;
    Climbed = 0.0f;
    Landings = 0;
    Trips = 0;
    MeanVel = FVector2D::ZeroVector;
}

void FWalkBody::Shift(const FVector2D& Offset)
{
    Com += Offset;
    Cop += Offset;
    for (FWalkFoot& Foot : Feet)
    {
        Foot.At += Offset;
        Foot.From += Offset;
    }
}

float FWalkBody::Height() const
{
    float Sink = 0.0f;
    int32 Count = 0;
    for (const FWalkFoot& Foot : Feet)
    {
        if (Foot.bDown)
        {
            Sink += Foot.Sink;
            ++Count;
        }
    }
    if (Count > 0)
    {
        Sink /= Count;
    }
    return FMath::Max(0.3f * Leg, 0.92f * Leg - Sink * 0.01f);
}

float FWalkBody::Omega() const
{
    return FMath::Sqrt(Gravity / FMath::Max(0.1f, Height()));
}

FVector2D FWalkBody::Capture() const
{
    return Com + Vel / Omega();
}

FVector2D FWalkBody::ToBody(const FVector2D& World) const
{
    return FVector2D(FVector2D::DotProduct(World, Forward()), FVector2D::DotProduct(World, Right()));
}

int32 FWalkBody::Lifted() const
{
    if (!Feet[0].bDown)
    {
        return 0;
    }
    if (!Feet[1].bDown)
    {
        return 1;
    }
    return -1;
}

int32 FWalkBody::DownCount() const
{
    return (Feet[0].bDown ? 1 : 0) + (Feet[1].bDown ? 1 : 0);
}

FVector2D FWalkBody::SupportPoint() const
{
    return SupportClamp(*this, Com);
}

void FWalkSkill::Observe(const FWalkBody& Body, const FVector2D& Want, const FVector2D& Face, const FWalkGrounds& Grounds, TArray<float>& Out)
{
    Out.SetNum(Observations);
    int32 I = 0;
    auto Put = [&Out, &I](const FVector2D& V, float Scale)
    {
        Out[I++] = V.X / Scale;
        Out[I++] = V.Y / Scale;
    };
    const FVector2D Mid = (Body.Feet[0].At + Body.Feet[1].At) * 0.5f;
    Put(Body.ToBody(Body.Vel), 1.5f);
    Put(Body.ToBody(Want), 1.5f);
    Put(Body.ToBody(Body.Com - Body.Feet[0].At), 0.5f);
    Put(Body.ToBody(Body.Com - Body.Feet[1].At), 0.5f);
    Put(Body.ToBody(Body.Capture() - Mid), 0.5f);
    Put(Body.ToBody(Body.Cop - Body.Com), 0.3f);
    for (int32 F = 0; F < 2; ++F)
    {
        const FWalkFoot& Foot = Body.Feet[F];
        const FStepGround& Ground = Grounds.Foot[F];
        Out[I++] = Foot.bDown ? 1.0f : -1.0f;
        Out[I++] = Foot.Z / 0.15f;
        Out[I++] = FMath::Min(Foot.Clock, 1.0f);
        Out[I++] = FMath::Min(Foot.Stuck, 1.0f) * 2.0f;
        Put(Body.ToBody(Foot.Vel), 2.0f);
        Out[I++] = FMath::FindDeltaAngleRadians(Body.Yaw, Foot.Yaw);
        Out[I++] = Foot.Sink / 10.0f;
        Out[I++] = Ground.Grip;
        Out[I++] = Ground.Stick;
        Out[I++] = Ground.WaterCm / 40.0f;
    }
    const FStepGround& Aim = Grounds.Aim;
    Out[I++] = Aim.Grip;
    Out[I++] = Aim.SinkCm / 10.0f;
    Out[I++] = Aim.Stick;
    Out[I++] = Aim.Rough / 3.0f;
    Out[I++] = Aim.WaterCm / 40.0f;
    Put(Body.ToBody((Grounds.Foot[0].Rise + Grounds.Foot[1].Rise) * 0.5f), 0.2f);
    Out[I++] = Body.YawRate / 2.0f;
    Out[I++] = FMath::Clamp(Body.Fatigue, 0.0f, 1.0f);
    Out[I++] = FMath::Clamp(Body.Load, 0.0f, 1.5f);
    Out[I++] = Body.Leg / 0.9f;
    Out[I++] = Body.Strength;
    Out[I++] = Body.Mass / 70.0f;
    const bool bFace = !Face.IsNearlyZero();
    const float Turn = bFace ? FMath::FindDeltaAngleRadians(Body.Yaw, FMath::Atan2(Face.Y, Face.X)) : 0.0f;
    Out[I++] = bFace ? 1.0f : -1.0f;
    Out[I++] = bFace ? FMath::Sin(Turn) : 0.0f;
    Out[I++] = bFace ? FMath::Cos(Turn) : 0.0f;
    check(I == Observations);
    for (float& Value : Out)
    {
        Value = FMath::Clamp(Value, -5.0f, 5.0f);
    }
}

FVector2D FWalkSkill::AimPoint(const FWalkBody& Body)
{
    const int32 Up = Body.Lifted();
    if (Up >= 0)
    {
        return Body.Feet[Up].At + Body.Feet[Up].Vel * 0.12f;
    }
    return Body.Capture();
}

void FWalkSkill::Simulate(FWalkBody& Body, const TArray<float>& Action, const FWalkGrounds& Grounds, float Seconds)
{
    Body.Energy = 0.0f;
    Body.Slipped = 0.0f;
    Body.Climbed = 0.0f;
    Body.Landings = 0;
    Body.Trips = 0;
    Body.MeanVel = FVector2D::ZeroVector;
    if (Body.bFallen || Seconds <= 0.0f || Action.Num() < Actions)
    {
        return;
    }

    const int32 Substeps = FMath::Max(1, FMath::CeilToInt(Seconds * 120.0f));
    const float Dt = Seconds / Substeps;
    const float Legs = FMath::Max(0.2f, Body.Leg);
    const float Heavy = 1.0f + FMath::Max(0.0f, Body.Load);
    const float Strong = FMath::Max(0.15f, Body.Strength * (1.0f - 0.4f * FMath::Clamp(Body.Fatigue, 0.0f, 1.0f))) / Heavy;
    const float Weight = Body.Mass * Heavy;
    const float SwingMass = Body.Mass * 0.05f;
    const float CopX = FMath::Clamp(Action[0], -1.0f, 1.0f);
    const float CopY = FMath::Clamp(Action[1], -1.0f, 1.0f);
    const float Lift[2] = { FMath::Clamp(Action[2], -1.0f, 1.0f), FMath::Clamp(Action[3], -1.0f, 1.0f) };
    const FVector2D AimLocal(FMath::Clamp(Action[4], -1.0f, 1.0f) * 0.55f * Legs, FMath::Clamp(Action[5], -1.0f, 1.0f) * 0.55f * Legs);
    const float Push = FMath::Max(0.0f, FMath::Clamp(Action[6], -1.0f, 1.0f));
    const float TurnCommand = FMath::Clamp(Action[7], -1.0f, 1.0f);

    auto Touchdown = [&Body, &Grounds, Weight, Legs, SwingMass](int32 Index, bool bTrip)
    {
        FWalkFoot& Foot = Body.Feet[Index];
        const FStepGround& Ground = Grounds.Aim;
        const float FootSpeed = Foot.Vel.Size();
        Body.Energy += 0.5f * SwingMass * FootSpeed * FootSpeed;
        Foot.bDown = true;
        Foot.Clock = 0.0f;
        Foot.Z = 0.0f;
        Foot.Yaw = Body.Yaw;
        Foot.Stuck = 0.0f;
        Foot.Sink = Ground.SinkCm * (0.7f + 0.3f * Weight / 70.0f);
        Foot.Vel = FVector2D::ZeroVector;
        const float Speed = Body.Vel.Size();
        if (Speed > 0.02f)
        {
            const FVector2D Dir = Body.Vel / Speed;
            const float Ahead = FMath::Max(0.0f, FVector2D::DotProduct(Foot.At - Body.Com, Dir));
            const float Angle = FMath::Atan2(Ahead, Body.Height());
            const float Loss = FMath::Square(FMath::Sin(2.0f * Angle)) * 0.3f;
            Body.Energy += 0.5f * Weight * Speed * Speed * Loss * 0.5f;
            Body.Vel *= FMath::Sqrt(FMath::Max(0.0f, 1.0f - Loss));
            const float Need = FMath::Tan(Angle) * 0.55f + FMath::Min(FootSpeed, 3.0f) * 0.05f;
            if (Need > Ground.Grip)
            {
                const float Slide = FMath::Min(0.35f * Legs, (Need - Ground.Grip) * 0.4f * Legs);
                Foot.At += Dir * Slide;
                Body.Slipped += Slide;
            }
        }
        ++Body.Landings;
        if (bTrip)
        {
            ++Body.Trips;
        }
    };

    for (int32 Sub = 0; Sub < Substeps && !Body.bFallen; ++Sub)
    {
        const int32 DownBefore = Body.DownCount();
        const float TurnLimit = DownBefore == 2 ? 1.2f : 3.0f;
        Body.YawRate += (TurnCommand * TurnLimit - Body.YawRate) * FMath::Min(1.0f, Dt / 0.08f);
        float Yaw = Body.Yaw + Body.YawRate * Dt;
        for (const FWalkFoot& Foot : Body.Feet)
        {
            if (Foot.bDown)
            {
                const float Twist = FMath::FindDeltaAngleRadians(Foot.Yaw, Yaw);
                if (FMath::Abs(Twist) > 1.0f)
                {
                    Yaw = Foot.Yaw + FMath::Sign(Twist);
                    Body.YawRate = 0.0f;
                }
            }
        }
        Body.Yaw = FMath::UnwindRadians(Yaw);
        Body.Energy += Body.Mass * 0.03f * FMath::Abs(Body.YawRate) * Dt * (DownBefore == 2 ? 2.0f : 1.0f);

        const FVector2D Fwd = Body.Forward();
        const FVector2D Side = Body.Right();
        const FVector2D Capture = Body.Capture();

        for (int32 Index = 0; Index < 2; ++Index)
        {
            FWalkFoot& Foot = Body.Feet[Index];
            const FWalkFoot& Other = Body.Feet[1 - Index];
            const FStepGround& Here = Grounds.Foot[Index];
            Foot.Clock += Dt;
            if (Foot.bDown)
            {
                if (Lift[Index] > 0.0f && Other.bDown && Foot.Clock > 0.1f)
                {
                    Foot.bDown = false;
                    Foot.Clock = 0.0f;
                    Foot.Vel = FVector2D::ZeroVector;
                    Foot.Z = 0.0f;
                    Foot.From = Foot.At;
                    Foot.Stuck = Foot.Sink > 0.3f ? (Foot.Sink * 0.012f + Here.Stick * Foot.Sink * 0.025f) / Strong : 0.0f;
                    Body.Energy += Weight * Gravity * (Foot.Sink * 0.01f) * (0.15f + Here.Stick * 0.6f) + Weight * 1.2f;
                }
                continue;
            }
            if (Foot.Stuck > 0.0f)
            {
                if (Lift[Index] <= 0.0f)
                {
                    Foot.bDown = true;
                    Foot.Clock = 0.0f;
                    Foot.Stuck = 0.0f;
                }
                else
                {
                    Foot.Stuck -= Dt;
                    Body.Energy += Weight * 0.4f * Dt;
                }
                continue;
            }

            const float TargetZ = Lift[Index] > 0.0f ? Lift[Index] * 0.3f * Legs + 0.01f : -0.02f;
            const float ZSpeed = FMath::Clamp((TargetZ - Foot.Z) / 0.05f, -1.4f, 1.4f) * FMath::Sqrt(Strong);
            const float NewZ = FMath::Max(0.0f, Foot.Z + ZSpeed * Dt);
            if (NewZ > Foot.Z)
            {
                Body.Energy += SwingMass * Gravity * (NewZ - Foot.Z) * 2.0f;
            }
            Foot.Z = NewZ;

            FVector2D Aim = Capture + Fwd * AimLocal.X + Side * AimLocal.Y;
            FVector2D Rel(FVector2D::DotProduct(Aim - Body.Com, Fwd), FVector2D::DotProduct(Aim - Body.Com, Side));
            const float Outward = Index == 0 ? -1.0f : 1.0f;
            Rel.X = FMath::Clamp(Rel.X, -0.42f * Legs, 0.5f * Legs);
            Rel.Y = Outward * FMath::Clamp(Rel.Y * Outward, -0.12f * Legs, 0.4f * Legs);
            const float Far = Rel.Size();
            if (Far > 0.52f * Legs)
            {
                Rel *= 0.52f * Legs / Far;
            }
            Aim = Body.Com + Fwd * Rel.X + Side * Rel.Y;
            const FVector2D Gap = Aim - Other.At;
            const float Room = 0.1f * Legs;
            if (Gap.SizeSquared() < Room * Room)
            {
                Aim = Other.At + (Gap.IsNearlyZero() ? Side * Outward : Gap.GetSafeNormal()) * Room;
            }
            const float Wet = FMath::Max(Grounds.Aim.WaterCm, Here.WaterCm) * 0.01f;
            const FVector2D Pull = Lift[Index] > 0.0f ? (Aim - Foot.At) * 190.0f - Foot.Vel * 27.6f : Foot.Vel * -27.6f;
            Foot.Vel += Pull * Dt;
            Foot.Vel *= FMath::Max(0.0f, 1.0f - Wet * 8.0f * Dt);
            const float Fastest = 5.0f * FMath::Sqrt(Strong);
            if (Foot.Vel.SizeSquared() > Fastest * Fastest)
            {
                Foot.Vel = Foot.Vel.GetSafeNormal() * Fastest;
            }
            Foot.At += Foot.Vel * Dt;
            const float Speed = Foot.Vel.Size();
            const float Sideways = Speed > 0.1f ? FMath::Abs(FVector2D::DotProduct(Foot.Vel, Side)) / Speed : 0.0f;
            Body.Energy += SwingMass * (FMath::Max(0.0f, FVector2D::DotProduct(Pull, Foot.Vel)) * 1.5f + Foot.Vel.SizeSquared() * Wet * 20.0f) * Dt * (1.0f + Sideways);

            const float Hole = FVector2D::Distance(Foot.At, Foot.From) < 0.15f ? Foot.Sink * 0.01f : 0.0f;
            const float Clear = Hole + FMath::Max(0.0f, Grounds.Aim.Rough - 0.8f) * 0.012f;
            if (Lift[Index] > 0.0f && Foot.Clock > 0.05f && Foot.Z < Clear && Speed > 0.45f)
            {
                Touchdown(Index, true);
                Foot.Clock = -0.15f;
                Body.Energy += Weight * 0.5f;
            }
            else if (Lift[Index] <= 0.0f && Foot.Z <= 0.0f && Foot.Clock > 0.04f)
            {
                Touchdown(Index, false);
            }
        }

        const bool bLeft = Bears(Body, 0);
        const bool bRight = Bears(Body, 1);
        const bool bSupported = bLeft || bRight;
        const float Along = CopX > 0.0f ? CopX * Body.FootFront : CopX * Body.FootBack;
        if (bSupported)
        {
            FVector2D Wanted;
            if (bLeft && bRight)
            {
                Wanted = FMath::Lerp(Body.Feet[0].At, Body.Feet[1].At, (CopY + 1.0f) * 0.5f) + Fwd * Along;
            }
            else
            {
                const FWalkFoot& Stand = Body.Feet[bLeft ? 0 : 1];
                const FVector2D FootFwd(FMath::Cos(Stand.Yaw), FMath::Sin(Stand.Yaw));
                const FVector2D FootSide(-FootFwd.Y, FootFwd.X);
                Wanted = Stand.At + FootFwd * Along + FootSide * (CopY * Body.FootSide);
            }
            Body.Cop += (Wanted - Body.Cop) * FMath::Min(1.0f, Dt / 0.035f);
            Body.Cop = SupportClamp(Body, Body.Cop);
        }
        else
        {
            Body.Cop = Body.Com;
        }

        float SinkNow = 0.0f;
        float Grip = 0.0f;
        float Water = 0.0f;
        float Mire = 0.0f;
        FVector2D Rise = FVector2D::ZeroVector;
        int32 Loaded = 0;
        for (int32 Index = 0; Index < 2; ++Index)
        {
            const FWalkFoot& Foot = Body.Feet[Index];
            const FStepGround& Ground = Grounds.Foot[Index];
            if (Foot.bDown)
            {
                Mire = FMath::Max(Mire, Foot.Sink * 0.01f * (2.0f + 5.0f * Ground.Stick));
            }
            if (Index == 0 ? bLeft : bRight)
            {
                SinkNow += Foot.Sink;
                Grip += Ground.Grip * (Foot.Sink > 5.0f ? 1.3f : 1.0f);
                Water = FMath::Max(Water, Ground.WaterCm * 0.01f);
                Rise += Ground.Rise;
                ++Loaded;
            }
        }
        if (Loaded > 0)
        {
            SinkNow /= Loaded;
            Grip /= Loaded;
            Rise /= static_cast<float>(Loaded);
        }
        const float Height = FMath::Max(0.3f * Legs, 0.92f * Legs - SinkNow * 0.01f);
        const float Omega2 = Gravity / Height;
        FVector2D Support = bSupported ? (Body.Com - Body.Cop) * Omega2 : FVector2D::ZeroVector;

        if (bSupported && Push > 0.0f)
        {
            int32 Behind = -1;
            float Trail = 0.0f;
            const FVector2D Heading = Body.Vel.SizeSquared() > 0.01f ? Body.Vel.GetSafeNormal() : Fwd;
            for (int32 Index = 0; Index < 2; ++Index)
            {
                if (Index == 0 ? bLeft : bRight)
                {
                    const float Lag = FVector2D::DotProduct(Body.Com - Body.Feet[Index].At, Heading);
                    if (Behind < 0 || Lag > Trail)
                    {
                        Behind = Index;
                        Trail = Lag;
                    }
                }
            }
            const FVector2D Lever = Body.Com - Body.Feet[Behind].At;
            const float Reach = Lever.Size();
            if (Reach > 0.02f && Reach < 0.6f * Legs)
            {
                const FVector2D Thrust = Lever / Reach * (Push * 3.0f * Strong);
                Support += Thrust;
                Body.Energy += Weight * (FMath::Max(0.0f, FVector2D::DotProduct(Thrust, Body.Vel)) * 4.0f + Push * 0.3f) * Dt;
            }
        }

        const float Brace = bSupported ? FMath::Abs(Along) + (Loaded == 1 ? FMath::Abs(CopY) * Body.FootSide : 0.0f) : 0.0f;
        Body.Energy += Weight * Gravity * Brace * (Body.Vel.Size() / Height * 0.75f + 0.05f) * Dt;

        FVector2D Acc = Support;
        Acc -= Rise * (Gravity * 0.3f * FMath::Clamp(Body.Vel.Size(), 0.0f, 1.0f));
        const float Wade = Water * 2.0f + Mire;
        Acc -= Body.Vel * Wade;
        Body.Energy += Weight * Body.Vel.SizeSquared() * Wade * 4.0f * Dt;
        if (bSupported)
        {
            const float Need = Support.Size() / Gravity;
            const float Hold = FMath::Max(0.03f, Grip);
            if (Need > Hold)
            {
                const FVector2D Away = -Support / Support.Size();
                const float SlipSpeed = (Need - Hold) * Gravity * 0.3f;
                for (int32 Index = 0; Index < 2; ++Index)
                {
                    if (Index == 0 ? bLeft : bRight)
                    {
                        Body.Feet[Index].At += Away * (SlipSpeed * Dt);
                    }
                }
                Body.Cop += Away * (SlipSpeed * Dt);
                Body.Slipped += SlipSpeed * Dt;
                Acc -= Support * (1.0f - Hold / Need);
            }
        }

        Body.Vel += Acc * Dt;
        const FVector2D Moved = Body.Vel * Dt;
        Body.Com += Moved;
        const float Up = FVector2D::DotProduct(Rise, Moved);
        Body.Climbed += Up;
        Body.Energy += Weight * Gravity * (Up > 0.0f ? Up * 4.0f : -Up * 0.6f);
        Body.MeanVel += Body.Vel * (Dt / Seconds);

        const FVector2D Base = bSupported ? SupportClamp(Body, Body.Com) : Body.Com;
        const float GapToSupport = (Body.Com - Base).Size();
        uint8 Cause = !bSupported ? 1 : (GapToSupport > 0.45f * Legs ? 2 : (Body.Vel.Size() > 3.6f ? 3 : 0));
        if (bLeft && bRight && FVector2D::Distance(Body.Feet[0].At, Body.Feet[1].At) > 1.2f * Legs)
        {
            Cause = 4;
        }
        for (const FWalkFoot& Foot : Body.Feet)
        {
            if (Foot.bDown && FVector2D::Distance(Foot.At, Body.Com) > 0.8f * Legs)
            {
                Cause = 5;
            }
        }
        if (Cause != 0)
        {
            Body.bFallen = true;
            Body.FallCause = Cause;
            Body.FallDirection = (Body.Com - Base).GetSafeNormal();
            if (Body.FallDirection.IsNearlyZero())
            {
                Body.FallDirection = Body.Vel.GetSafeNormal();
            }
        }
    }
}

float FWalkSkill::RewardFrom(float Mass, bool bFallen, float Yaw, const FVector2D& MeanVel, const FVector2D& Want, const FVector2D& Face,
    float Energy, float Slipped, int32 Trips)
{
    if (bFallen)
    {
        return -3.0f;
    }
    const float Miss = (MeanVel - Want).SizeSquared();
    float Value = 0.3f + 0.7f * FMath::Exp(-Miss / 0.2f);
    if (!Face.IsNearlyZero())
    {
        const float Turn = FMath::FindDeltaAngleRadians(Yaw, FMath::Atan2(Face.Y, Face.X));
        Value += 0.25f * (FMath::Cos(Turn) - 1.0f);
    }
    Value -= 0.4f * Energy / FMath::Max(5.0f, Mass);
    Value -= 4.0f * Slipped;
    Value -= 0.8f * Trips;
    return Value;
}

float FWalkSkill::Reward(const FWalkBody& Body, const FVector2D& Want, const FVector2D& Face)
{
    return RewardFrom(Body.Mass, Body.bFallen, Body.Yaw, Body.MeanVel, Want, Face, Body.Energy, Body.Slipped, Body.Trips);
}

float FWalkSkill::TopSpeed(float Difficulty, float Leg)
{
    return FMath::Lerp(1.3f, 2.05f, PaceLevel(Difficulty)) * FMath::Sqrt(FMath::Max(0.3f, Leg) / 0.9f);
}

float FReachBody::PelvisDrop() const
{
    return Squat * Leg * 0.55f;
}

float FReachBody::PelvisBack() const
{
    return Squat * Leg * 0.22f;
}

FVector FReachBody::ShoulderAt() const
{
    const FVector Pelvis(-PelvisBack(), 0.0f, Leg - PelvisDrop());
    return Pelvis + Turn(FVector(0.0f, Side * Shoulder, Trunk), Pitch);
}

FVector FReachBody::ElbowAt() const
{
    const FVector Down(0.0f, Side * FMath::Sin(Spread), -FMath::Cos(Spread));
    const FVector Arm = Turn(Turn(Down, -Raise), Pitch);
    return ShoulderAt() + Arm * UpperArm;
}

FVector FReachBody::HandAt() const
{
    const FVector Down(0.0f, Side * FMath::Sin(Spread), -FMath::Cos(Spread));
    const FVector Fore = Turn(Turn(Down, -(Raise + Elbow)), Pitch);
    return ElbowAt() + Fore * ForeArm;
}

float FReachBody::BalanceOffset() const
{
    const float Back = PelvisBack();
    const float LegX = -Back * 0.5f;
    const float TrunkX = -Back + FMath::Sin(Pitch) * Trunk * 0.55f;
    const float ArmX = (ShoulderAt().X + HandAt().X) * 0.5f;
    const float RestArmX = -Back + FMath::Sin(Pitch) * Trunk;
    const float Carried = Load / FMath::Max(10.0f, Mass);
    const float Total = 0.32f + 0.5f + 0.09f + 0.09f + Carried;
    const float Sum = 0.32f * LegX + 0.5f * TrunkX + 0.09f * ArmX + 0.09f * RestArmX + Carried * HandAt().X;
    return Sum / Total;
}

void FReachSkill::Observe(const FReachBody& Body, const FVector& Goal, bool bActive, TArray<float>& Out)
{
    const FVector Hand = Body.HandAt();
    const FVector Shoulder = Body.ShoulderAt();
    const FVector ToGoal = (Goal - Hand) * FVector(1.0f, Body.Side, 1.0f);
    const FVector FromShoulder = (Goal - Shoulder) * FVector(1.0f, Body.Side, 1.0f);
    Out.SetNum(StateSize);
    int32 I = 0;
    Out[I++] = ToGoal.X / 0.6f;
    Out[I++] = ToGoal.Y / 0.6f;
    Out[I++] = ToGoal.Z / 0.6f;
    Out[I++] = FromShoulder.X;
    Out[I++] = FromShoulder.Y;
    Out[I++] = FromShoulder.Z;
    Out[I++] = Body.Pitch / 1.65f;
    Out[I++] = Body.Squat;
    Out[I++] = Body.Raise / 3.0f;
    Out[I++] = Body.Spread / 1.6f;
    Out[I++] = Body.Elbow / 2.6f;
    Out[I++] = Body.PitchRate / 2.2f;
    Out[I++] = Body.SquatRate / 1.8f;
    Out[I++] = Body.RaiseRate / 4.0f;
    Out[I++] = Body.SpreadRate / 3.0f;
    Out[I++] = Body.ElbowRate / 5.0f;
    Out[I++] = Body.BalanceOffset() / 0.1f;
    Out[I++] = Body.Load / 20.0f;
    Out[I++] = bActive ? 1.0f : -1.0f;
    Out[I++] = Hand.Z / 1.8f;
    for (float& Value : Out)
    {
        Value = FMath::Clamp(Value, -4.0f, 4.0f);
    }
}

void FReachSkill::Simulate(FReachBody& Body, const TArray<float>& Action, float Seconds)
{
    Body.Energy = 0.0f;
    if (Body.bToppled || Seconds <= 0.0f)
    {
        return;
    }
    const float Strong = FMath::Max(0.2f, Body.Strength);
    const float Lag = 1.0f - FMath::Exp(-Seconds / 0.06f);
    auto Drive = [Lag, Strong](float& Rate, float Command, float MaxRate)
    {
        Rate = FMath::Lerp(Rate, FMath::Clamp(Command, -1.0f, 1.0f) * MaxRate * FMath::Sqrt(Strong), Lag);
    };
    Drive(Body.PitchRate, Action[0], 2.2f);
    Drive(Body.SquatRate, Action[1], 1.8f);
    Drive(Body.RaiseRate, Action[2], 4.0f);
    Drive(Body.SpreadRate, Action[3], 3.0f);
    Drive(Body.ElbowRate, Action[4], 5.0f);

    auto Move = [Seconds](float& Value, float& Rate, float Low, float High)
    {
        Value += Rate * Seconds;
        if (Value < Low)
        {
            Value = Low;
            Rate = 0.0f;
        }
        else if (Value > High)
        {
            Value = High;
            Rate = 0.0f;
        }
    };
    Move(Body.Pitch, Body.PitchRate, -0.2f, 1.65f);
    Move(Body.Squat, Body.SquatRate, 0.0f, 1.0f);
    Move(Body.Raise, Body.RaiseRate, -0.8f, 3.0f);
    Move(Body.Spread, Body.SpreadRate, -0.3f, 1.6f);
    Move(Body.Elbow, Body.ElbowRate, 0.0f, 2.6f);

    const float Upper = Body.Mass * 0.6f;
    const float Hold = Upper * Gravity * FMath::Sin(FMath::Max(0.0f, Body.Pitch)) * Body.Trunk * 0.5f
        + Body.Mass * Gravity * Body.Squat * 0.12f
        + (Body.Mass * 0.05f * Gravity * 0.3f + Body.Load * Gravity) * FMath::Abs(Body.HandAt().X - Body.ShoulderAt().X);
    const float Motion = Upper * 0.02f * FMath::Square(Body.PitchRate) + Body.Mass * 0.03f * FMath::Square(Body.SquatRate)
        + (Body.Mass * 0.004f + Body.Load * 0.02f) * (FMath::Square(Body.RaiseRate) + FMath::Square(Body.SpreadRate) + 0.4f * FMath::Square(Body.ElbowRate));
    Body.Energy = (Hold * 0.04f + Motion) * Seconds / Strong;

    const float Offset = Body.BalanceOffset();
    const float Beyond = FMath::Max(0.0f, Offset - 0.17f) + FMath::Max(0.0f, -0.06f - Offset);
    if (Beyond > 0.0f)
    {
        Body.Wobble += Seconds * (1.0f + Beyond * 40.0f);
    }
    else
    {
        Body.Wobble = FMath::Max(0.0f, Body.Wobble - Seconds * 2.0f);
    }
    if (Body.Wobble > 0.45f)
    {
        Body.bToppled = true;
    }
}

float FReachSkill::Reward(const FReachBody& Body, const FVector& Goal, bool bActive, float DistanceBefore, float Seconds)
{
    if (Body.bToppled)
    {
        return -4.0f;
    }
    const float Distance = FVector::Dist(Body.HandAt(), Goal);
    float Value = 0.0f;
    Value -= Body.Energy / FMath::Max(10.0f, Body.Mass) * 2.0f;
    const float Offset = Body.BalanceOffset();
    Value -= (FMath::Max(0.0f, Offset - 0.17f) + FMath::Max(0.0f, -0.06f - Offset)) * 6.0f;
    if (bActive)
    {
        Value += (DistanceBefore - Distance) * 5.0f;
        Value -= Distance * Seconds * 6.0f;
        const float HandSpeed = FMath::Abs(Body.RaiseRate) * Body.UpperArm + FMath::Abs(Body.ElbowRate) * Body.ForeArm
            + FMath::Abs(Body.PitchRate) * Body.Trunk + FMath::Abs(Body.SquatRate) * Body.Leg * 0.5f;
        if (Distance < 0.04f && HandSpeed < 0.25f)
        {
            Value += 0.4f;
        }
    }
    else
    {
        Value -= (FMath::Square(Body.Pitch) + FMath::Square(Body.Squat) + FMath::Square(Body.Raise) * 0.3f
            + FMath::Square(Body.Spread - 0.1f) + FMath::Square(Body.Elbow - 0.2f) * 0.2f) * Seconds * 2.0f;
    }
    return Value;
}

void FGripSkill::Observe(const FGripState& Grip, bool bWantHold, TArray<float>& Out)
{
    Out.SetNum(StateSize);
    int32 I = 0;
    Out[I++] = Grip.Curl;
    Out[I++] = Grip.Contact;
    Out[I++] = Grip.bTouching ? FMath::Clamp((Grip.Curl - Grip.Contact) * 5.0f, -1.0f, 1.0f) : -1.0f;
    Out[I++] = Grip.Friction;
    Out[I++] = FMath::Min(Grip.Mass, 30.0f) / 20.0f;
    Out[I++] = Grip.Lift / 5.0f;
    Out[I++] = Grip.Softness;
    Out[I++] = Grip.bHolding ? 1.0f : -1.0f;
    Out[I++] = bWantHold ? 1.0f : -1.0f;
}

void FGripSkill::Simulate(FGripState& Grip, const TArray<float>& Action, float Seconds)
{
    Grip.Energy = 0.0f;
    Grip.bDropped = false;
    Grip.bCrushed = false;
    const float Rate = FMath::Clamp(Action[0], -1.0f, 1.0f) * 3.0f;
    Grip.Curl = FMath::Clamp(Grip.Curl + Rate * Seconds, -0.3f, 1.0f);
    Grip.Squeeze = Grip.bTouching ? FMath::Max(0.0f, Grip.Curl - Grip.Contact) * 400.0f * Grip.Strength : 0.0f;
    const float Capacity = 2.0f * Grip.Friction * Grip.Squeeze;
    const float Demand = Grip.Mass * (Gravity + FMath::Max(0.0f, Grip.Lift));
    const bool bWasHolding = Grip.bHolding;
    Grip.bHolding = Grip.bTouching && Capacity >= Demand && Grip.Squeeze > 1.0f;
    if (bWasHolding && !Grip.bHolding && Grip.bTouching && Grip.Lift > 0.2f)
    {
        Grip.bDropped = true;
    }
    if (Grip.Softness > 0.05f && Grip.Squeeze > 20.0f + 80.0f * (1.0f - Grip.Softness))
    {
        Grip.bCrushed = true;
    }
    Grip.Energy = (Grip.Squeeze * 0.002f + FMath::Abs(Rate) * 0.02f) * Seconds;
}

float FGripSkill::Reward(const FGripState& Grip, bool bWantHold, float Seconds)
{
    float Value = -Grip.Energy * 2.0f;
    if (Grip.bDropped)
    {
        return -2.0f;
    }
    if (Grip.bCrushed)
    {
        Value -= 0.3f;
    }
    if (bWantHold)
    {
        Value += Grip.bHolding ? 0.1f : -0.05f;
    }
    else
    {
        Value += Grip.Curl < Grip.Contact - 0.05f ? 0.05f : -0.08f;
    }
    return Value;
}

FString FMotorSchool::Folder()
{
    return FPaths::ProjectSavedDir() / TEXT("LearnedMind") / TEXT("Motor");
}

void FMotorSchool::Grow(float Age, bool bFemale, float& OutLeg, float& OutMass, float& OutStrength)
{
    const float Adult = FMath::Clamp((Age - 1.0f) / 16.0f, 0.0f, 1.0f);
    OutLeg = FMath::Lerp(0.32f, bFemale ? 0.86f : 0.93f, FMath::Pow(Adult, 0.8f));
    OutMass = Age < 18.0f ? FMath::Clamp(3.5f + Age * 3.4f, 9.0f, 65.0f) : (bFemale ? 62.0f : 76.0f);
    OutStrength = FMath::Lerp(0.35f, bFemale ? 0.85f : 1.0f, Adult);
    if (Age > 60.0f)
    {
        OutStrength *= FMath::Clamp(1.0f - (Age - 60.0f) * 0.015f, 0.45f, 1.0f);
    }
}

void FMotorSchool::MakeWalkBody(float Age, bool bFemale, FWalkBody& Out)
{
    Grow(Age, bFemale, Out.Leg, Out.Mass, Out.Strength);
    const float Scale = Out.Leg / 0.93f;
    Out.FootFront = 0.17f * Scale;
    Out.FootBack = 0.06f * Scale;
    Out.FootSide = 0.045f * Scale;
}

void FMotorSchool::MakeReachBody(float Age, bool bFemale, float Side, FReachBody& Out)
{
    Grow(Age, bFemale, Out.Leg, Out.Mass, Out.Strength);
    const float Scale = Out.Leg / 0.93f;
    Out.Trunk = 0.52f * Scale;
    Out.Shoulder = 0.19f * Scale;
    Out.UpperArm = 0.29f * Scale;
    Out.ForeArm = 0.27f * Scale;
    Out.Side = Side;
}

int32 FMotorSchool::KindOf(float Age, bool bFemale)
{
    if (Age < 3.0f)
    {
        return 0;
    }
    if (Age < 9.0f)
    {
        return 1;
    }
    if (Age < 15.0f)
    {
        return 2;
    }
    if (Age < 60.0f)
    {
        return bFemale ? 3 : 4;
    }
    return bFemale ? 5 : 6;
}

int32 FMotorSchool::ParentKind(int32 Kind)
{
    switch (Kind)
    {
    case 0: return 1;
    case 1: return 2;
    case 2: return 4;
    case 3: return 4;
    case 5: return 3;
    case 6: return 4;
    default: return -1;
    }
}

FString FMotorSchool::DescribeWalk(const FSkillStats& S, float Mass)
{
    const float Minutes = static_cast<float>(S.Seconds / 60.0);
    return FString::Printf(TEXT("падений в минуту %.2f, спотыканий в минуту %.1f, ошибка скорости %.2f м/с, скорость %.2f из %.2f м/с, шагов в секунду %.2f, скольжение %.2f м/мин, расход %.1f Дж/кг/м"),
        S.Falls / FMath::Max(0.01f, Minutes), S.Trips / FMath::Max(0.01f, Minutes),
        FMath::Sqrt(S.Track / FMath::Max(1, S.Samples)), S.Speed / FMath::Max(1, S.Samples), S.Want / FMath::Max(1, S.Samples),
        S.Steps / FMath::Max(0.01, S.Seconds), S.Slip / FMath::Max(0.01f, Minutes),
        S.Energy / FMath::Max(1.0, Mass * S.Distance));
}

void FMotorSchool::RaiseWalker(FMotorPolicy& Policy, const FWalkBody& Shape, int32 Iterations, uint32 Seed, FString* OutReport)
{
    if (!Policy.IsReady() || Policy.ObservationSize() != FWalkSkill::Observations || Policy.ActionSize() != FWalkSkill::Actions)
    {
        Policy.Init(FWalkSkill::Observations, FWalkSkill::Actions, 64, Seed, 0.6f);
    }
    FPracticePlan Plan;
    Plan.Iterations = Iterations;
    const float Mass = Shape.Mass;
    FMotorPractice::Train(Policy,
        [Shape]() { return TUniquePtr<IMotorTask>(new FWalkTask(Shape)); },
        Plan, Seed,
        [Mass](const FSkillStats& Stats) { return DescribeWalk(Stats, Mass); },
        [](const FSkillStats& Stats) { return WalkMastery(Stats); },
        OutReport);
}

FString FMotorSchool::ExamineWalker(const FMotorPolicy& Policy, const FWalkBody& Shape, float Difficulty, uint32 Seed, TArray<FString>* OutTrace)
{
    FSkillStats Stats;
    TArray<float> Observation;
    TArray<float> Normalized;
    TArray<float> Action;
    for (int32 Episode = 0; Episode < 8; ++Episode)
    {
        FRandomStream Rng(static_cast<int32>(Seed + Episode * 101u));
        FWalkTask Task(Shape);
        Task.Reset(Rng, Difficulty);
        for (int32 Step = 0; Step < 600; ++Step)
        {
            Task.Observe(Observation);
            float LogProb = 0.0f;
            float Value = 0.0f;
            Policy.Act(Observation, Normalized, Action, LogProb, Value, nullptr);
            for (float& V : Action)
            {
                V = FMath::Clamp(V, -1.0f, 1.0f);
            }
            bool bFailed = false;
            Task.Step(Action, Rng, bFailed, Stats);
            const FWalkBody& Body = Task.GetBody();
            if (OutTrace && Episode == 0 && (Step % 2 == 0 || bFailed))
            {
                const FVector2D Want = Task.GetWant();
                const FVector2D Face = Task.GetFace();
                OutTrace->Add(FString::Printf(TEXT("t=%5.2f want=(%5.2f,%5.2f) face=%s com=(%6.2f,%6.2f) v=(%5.2f,%5.2f) yaw=%5.2f L=(%6.2f,%6.2f)%s z=%.2f R=(%6.2f,%6.2f)%s z=%.2f cop=(%6.2f,%6.2f) act=[%s]%s"),
                    Step * FWalkSkill::Interval, Want.X, Want.Y,
                    Face.IsNearlyZero() ? TEXT("-") : *FString::Printf(TEXT("%.2f"), FMath::Atan2(Face.Y, Face.X)),
                    Body.Com.X, Body.Com.Y, Body.Vel.X, Body.Vel.Y, Body.Yaw,
                    Body.Feet[0].At.X, Body.Feet[0].At.Y, Body.Feet[0].bDown ? TEXT("v") : TEXT("^"), Body.Feet[0].Z,
                    Body.Feet[1].At.X, Body.Feet[1].At.Y, Body.Feet[1].bDown ? TEXT("v") : TEXT("^"), Body.Feet[1].Z,
                    Body.Cop.X, Body.Cop.Y,
                    *FString::JoinBy(Action, TEXT(" "), [](float V) { return FString::Printf(TEXT("%+.2f"), V); }),
                    bFailed ? TEXT(" УПАЛ") : TEXT("")));
            }
            if (bFailed)
            {
                Task.Reset(Rng, Difficulty);
            }
        }
    }
    return DescribeWalk(Stats, Shape.Mass);
}

FString FMotorSchool::DescribeReach(const FSkillStats& S)
{
    return FString::Printf(TEXT("дотянулся %d из %d, промах в конце %.3f м, опрокидывался %d"),
        S.Reached, S.Goals, S.Distance / FMath::Max(1, S.Samples), S.Falls);
}

FString FMotorSchool::DescribeGrip(const FSkillStats& S)
{
    return FString::Printf(TEXT("удержал при подъёме %d из %d, уронил %d, смял %d"),
        S.Holds, S.Goals, S.Drops, S.Crushes);
}
void FMotorSchool::RaiseReacher(FMotorPolicy& Policy, const FReachBody& Shape, int32 Iterations, uint32 Seed, FString* OutReport)
{
    if (!Policy.IsReady() || Policy.ObservationSize() != FReachSkill::StateSize || Policy.ActionSize() != FReachSkill::ActionSize)
    {
        Policy.Init(FReachSkill::StateSize, FReachSkill::ActionSize, 64, Seed, 0.5f);
    }
    FPracticePlan Plan;
    Plan.Iterations = Iterations;
    Plan.Tasks = 32;
    Plan.Horizon = 192;
    Plan.Batch = 1536;
    Plan.Gamma = 0.97f;
    FMotorPractice::Train(Policy,
        [Shape]() { return TUniquePtr<IMotorTask>(new FReachTask(Shape)); },
        Plan, Seed,
        [](const FSkillStats& Stats) { return DescribeReach(Stats); },
        [](const FSkillStats& Stats) { return ReachMastery(Stats); },
        OutReport);
}

void FMotorSchool::RaiseGripper(FMotorPolicy& Policy, float Strength, int32 Iterations, uint32 Seed, FString* OutReport)
{
    if (!Policy.IsReady() || Policy.ObservationSize() != FGripSkill::StateSize || Policy.ActionSize() != FGripSkill::ActionSize)
    {
        Policy.Init(FGripSkill::StateSize, FGripSkill::ActionSize, 32, Seed, 0.5f);
    }
    FPracticePlan Plan;
    Plan.Iterations = Iterations;
    Plan.Tasks = 24;
    Plan.Horizon = 192;
    Plan.Batch = 1152;
    Plan.Gamma = 0.95f;
    FMotorPractice::Train(Policy,
        [Strength]() { return TUniquePtr<IMotorTask>(new FGripTask(Strength)); },
        Plan, Seed,
        [](const FSkillStats& Stats) { return DescribeGrip(Stats); },
        [](const FSkillStats& Stats) { return GripMastery(Stats); },
        OutReport);
}

bool FMotorSchool::Childhood(const FString& Skill, int32 Kind, FMotorPolicy& Out)
{
    const FString Path = Folder() / FString::Printf(TEXT("%s_%d.pol"), *Skill, Kind);
    return Out.Load(Path);
}

