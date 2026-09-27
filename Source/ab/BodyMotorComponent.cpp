#include "BodyMotorComponent.h"
#include "CompleteHumanAI.h"
#include "MindComponent.h"
#include "IdentityComponent.h"
#include "BookActor.h"
#include "FurnitureActor.h"
#include "ResourceActor.h"
#include "PhysiologyComponent.h"
#include "HumanWorldSubsystem.h"
#include "MatterSubsystem.h"
#include "HumanMovementComponent.h"
#include "TerrainGrid.h"
#include "Async/Async.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "Misc/CommandLine.h"
#include "EngineUtils.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkinnedAsset.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "ReferenceSkeleton.h"

namespace
{
    float MinJerk(float T)
    {
        const float X = FMath::Clamp(T, 0.0f, 1.0f);
        return X * X * X * (10.0f + X * (-15.0f + 6.0f * X));
    }

    bool IsFreeFor(const AActor* Item, const AActor* Who)
    {
        if (!IsValid(Item) || Item->IsActorBeingDestroyed())
        {
            return false;
        }
        if (const ABookActor* Book = Cast<ABookActor>(Item))
        {
            return !Book->HeldBy || Book->HeldBy == Who;
        }
        if (const AResourceActor* Thing = Cast<AResourceActor>(Item))
        {
            return (!Thing->HeldBy || Thing->HeldBy == Who) && Thing->Amount > 0.0f;
        }
        if (const AFurnitureActor* Furniture = Cast<AFurnitureActor>(Item))
        {
            return !Furniture->HeldBy.IsValid() || Furniture->HeldBy.Get() == Who;
        }
        return true;
    }
}

void FWalkerTick::ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
    if (Motor && IsValid(Motor) && TickType != LEVELTICK_ViewportsOnly)
    {
        Motor->WalkerStep(DeltaTime);
    }
}

void UBodyMotorComponent::BeginPlay()
{
    Super::BeginPlay();
    WalkerTick.Motor = this;
    WalkerTick.TickGroup = TG_PrePhysics;
    WalkerTick.bCanEverTick = true;
    WalkerTick.bStartWithTickEnabled = true;
    WalkerTick.RegisterTickFunction(GetComponentLevel());
    if (ACompleteHumanNPC* Human = Cast<ACompleteHumanNPC>(GetOwner()))
    {
        if (UCharacterMovementComponent* Move = Human->GetCharacterMovement())
        {
            Move->PrimaryComponentTick.AddPrerequisite(this, WalkerTick);
        }
    }
}

void UBodyMotorComponent::WalkerStep(float DeltaTime)
{
    ACompleteHumanNPC* Human = Cast<ACompleteHumanNPC>(GetOwner());
    if (!Human)
    {
        return;
    }
    LastFrame = DeltaTime;
    if (bInfant)
    {
        PracticeInfant(DeltaTime, Human);
    }
    AdvanceWalker(FMath::Clamp(DeltaTime, 0.0f, 0.25f), Human);
}

void UBodyMotorComponent::ReadyToWalk(ACompleteHumanNPC* Human)
{
    bWalkerUsable = WalkBrain.IsReady() && WalkBrain.ObservationSize() == FWalkSkill::Observations && WalkBrain.ActionSize() == FWalkSkill::Actions;
    if (!bWalkerUsable)
    {
        return;
    }
    WalkAction.Init(-1.0f, FWalkSkill::Actions);
    WalkAction[0] = 0.0f;
    WalkAction[1] = 0.0f;
    Lived.Begin(FWalkSkill::Observations, FWalkSkill::Actions, 1024);
    WalkBrain.RefreshNoise();
    Noise.Initialize(static_cast<int32>(GetTypeHash(Human->GetFName()) ^ 0x1f123bb5u));
}

void UBodyMotorComponent::PracticeInfant(float DeltaTime, ACompleteHumanNPC* Human)
{
    const bool bFast = FParse::Param(FCommandLine::Get(), TEXT("FastInfants"));
    const float Age = Human->IdentityComponent ? Human->IdentityComponent->Age : 1.0f;
    const bool bFemale = Human->IdentityComponent && Human->IdentityComponent->bFemale;
    if (Practicing.IsValid())
    {
        if (!Practicing.IsReady())
        {
            return;
        }
        FMotorPolicy Learned = Practicing.Get();
        Practicing = TFuture<FMotorPolicy>();
        const bool bWasAble = CanTryWalking();
        WalkBrain = Learned;
        InfantCycles += bFast ? 6 : 2;
        if (InfantCycles % 20 < (bFast ? 6 : 2))
        {
            UE_LOG(LogHumanCity, Log, TEXT("WALKLEARN %s возраст %.2f, упражнений %d, трудность %.2f"),
                *Human->GetName(), Age, InfantCycles, WalkBrain.Difficulty);
        }
        if (!bWasAble && CanTryWalking())
        {
            ReadyToWalk(Human);
            if (Human->Mind)
            {
                Human->Mind->Report(TEXT("впервые попробует встать на ножки"));
            }
        }
        else if (bWalkerUsable)
        {
            WalkBrain.RefreshNoise();
        }
        if (Age >= 1.5f && InfantCycles >= 300)
        {
            bInfant = false;
            WalkBrain.Save(OwnWalkPath(Human));
        }
        return;
    }
    PracticeClock += DeltaTime;
    if (PracticeClock < (bFast ? 0.3f : 3.0f) || Age < 0.15f)
    {
        return;
    }
    PracticeClock = 0.0f;
    FWalkBody Frame;
    FMotorSchool::MakeWalkBody(FMath::Max(Age, 0.6f), bFemale, Frame);
    FMotorPolicy Copy = WalkBrain;
    const uint32 Seed = GetTypeHash(Human->GetFName()) + static_cast<uint32>(InfantCycles) * 7919u;
    const int32 Chunk = bFast ? 6 : 2;
    Practicing = Async(EAsyncExecution::ThreadPool, [Copy, Frame, Seed, Chunk]() mutable
    {
        FMotorSchool::RaiseWalker(Copy, Frame, Chunk, Seed, nullptr);
        return Copy;
    });
}

UBodyMotorComponent::UBodyMotorComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostPhysics;

    for (int32 H = 0; H < 2; ++H)
    {
        for (int32 F = 0; F < 5; ++F)
        {
            for (int32 J = 0; J < 3; ++J)
            {
                FingerBones[H][F][J] = INDEX_NONE;
            }
        }
    }
}

void UBodyMotorComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    if (WalkerTick.IsTickFunctionRegistered())
    {
        WalkerTick.UnRegisterTickFunction();
    }
    if (bWalkerUsable && WalkSeconds > 30.0)
    {
        if (Studying.IsValid() && Studying.IsReady())
        {
            WalkBrain = Studying.Get();
        }
        const ACompleteHumanNPC* Human = Cast<ACompleteHumanNPC>(GetOwner());
        WalkBrain.Save(OwnWalkPath(Human));
    }
    Super::EndPlay(Reason);
}

void UBodyMotorComponent::Bind(UPoseableMeshComponent* InBody)
{
    Body = InBody;
    bReady = false;
    bHasSmoothed = false;
}

void UBodyMotorComponent::SetGround(float SinkCm, float GripValue, float Effort)
{
    FootSink = FMath::Clamp(SinkCm, 0.0f, 45.0f);
    GroundGrip = FMath::Clamp(GripValue, 0.02f, 1.5f);
    GroundEffort = FMath::Max(1.0f, Effort);
}

void UBodyMotorComponent::FK(int32 From)
{
    for (int32 i = FMath::Max(0, From); i < Local.Num(); ++i)
    {
        const int32 P = Parents[i];
        CS[i] = P >= 0 ? Local[i] * CS[P] : Local[i];
    }
}

void UBodyMotorComponent::RotateBone(int32 Bone, const FQuat& Delta)
{
    if (Bone == INDEX_NONE)
    {
        return;
    }
    const FQuat NewRotation = (Delta * CS[Bone].GetRotation()).GetNormalized();
    const int32 P = Parents[Bone];
    Local[Bone].SetRotation(P >= 0 ? (CS[P].GetRotation().Inverse() * NewRotation).GetNormalized() : NewRotation);
    FK(Bone);
}

void UBodyMotorComponent::AimBone(int32 Bone, int32 Child, const FVector& Direction, float Weight)
{
    if (Bone == INDEX_NONE || Child == INDEX_NONE)
    {
        return;
    }
    const FVector Current = (CS[Child].GetLocation() - CS[Bone].GetLocation()).GetSafeNormal();
    const FVector Wanted = Direction.GetSafeNormal();
    if (Current.IsNearlyZero() || Wanted.IsNearlyZero())
    {
        return;
    }
    FQuat Delta = FQuat::FindBetweenNormals(Current, Wanted);
    if (Weight < 1.0f)
    {
        Delta = FQuat::Slerp(FQuat::Identity, Delta, FMath::Clamp(Weight, 0.0f, 1.0f));
    }
    RotateBone(Bone, Delta);
}

FVector UBodyMotorComponent::SolveMiddle(const FVector& Root, const FVector& Target, float L1, float L2, const FVector& Pole)
{
    const FVector ToTarget = Target - Root;
    const float Raw = ToTarget.Size();
    const FVector Dir = Raw > KINDA_SMALL_NUMBER ? ToTarget / Raw : FVector::DownVector;
    const float Dist = FMath::Clamp(Raw, FMath::Abs(L1 - L2) + 0.5f, L1 + L2 - 0.2f);
    const float Along = (L1 * L1 - L2 * L2 + Dist * Dist) / (2.0f * Dist);
    const float Height = FMath::Sqrt(FMath::Max(0.0f, L1 * L1 - Along * Along));

    FVector Side = Pole - Dir * FVector::DotProduct(Pole, Dir);
    if (!Side.Normalize())
    {
        Side = FVector::CrossProduct(Dir, FVector::RightVector).GetSafeNormal();
    }
    return Root + Dir * Along + Side * Height;
}

void UBodyMotorComponent::SolveLimb(const FLimb& Limb, float L1, float L2, const FVector& Target, const FVector& Pole, const FVector& EndDirection)
{
    const FVector Root = CS[Limb.Upper].GetLocation();
    const FVector Middle = SolveMiddle(Root, Target, L1, L2, Pole);
    AimBone(Limb.Upper, Limb.Lower, Middle - Root);
    AimBone(Limb.Lower, Limb.End, Target - CS[Limb.Lower].GetLocation());
    if (!EndDirection.IsNearlyZero() && Limb.Tip != INDEX_NONE)
    {
        AimBone(Limb.End, Limb.Tip, EndDirection);
    }
}

void UBodyMotorComponent::BuildCache()
{
    bReady = false;
    const USkinnedAsset* Asset = Body ? Body->GetSkinnedAsset() : nullptr;
    if (!Asset)
    {
        return;
    }

    const FReferenceSkeleton& Ref = Asset->GetRefSkeleton();
    const int32 Num = Ref.GetNum();
    if (Num < 20 || Body->BoneSpaceTransforms.Num() != Num)
    {
        return;
    }

    RefLocal = Ref.GetRefBonePose();
    Parents.SetNum(Num);
    for (int32 i = 0; i < Num; ++i)
    {
        Parents[i] = Ref.GetParentIndex(i);
    }
    Local = RefLocal;
    CS.SetNum(Num);
    FK(0);
    RefCS = CS;

    auto Bone = [&Ref](const TCHAR* Name) { return Ref.FindBoneIndex(FName(Name)); };
    Pelvis = Bone(TEXT("pelvis"));
    Spine1 = Bone(TEXT("spine_01"));
    Spine2 = Bone(TEXT("spine_02"));
    Spine3 = Bone(TEXT("spine_03"));
    Neck = Bone(TEXT("neck_01"));
    Head = Bone(TEXT("head"));
    LegL = { Bone(TEXT("thigh_l")), Bone(TEXT("calf_l")), Bone(TEXT("foot_l")), Bone(TEXT("ball_l")) };
    LegR = { Bone(TEXT("thigh_r")), Bone(TEXT("calf_r")), Bone(TEXT("foot_r")), Bone(TEXT("ball_r")) };
    ArmL = { Bone(TEXT("upperarm_l")), Bone(TEXT("lowerarm_l")), Bone(TEXT("hand_l")), Bone(TEXT("middle_01_l")) };
    ArmR = { Bone(TEXT("upperarm_r")), Bone(TEXT("lowerarm_r")), Bone(TEXT("hand_r")), Bone(TEXT("middle_01_r")) };

    static const TCHAR* FingerNames[5] = { TEXT("thumb"), TEXT("index"), TEXT("middle"), TEXT("ring"), TEXT("pinky") };
    bHasFingers = true;
    for (int32 H = 0; H < 2; ++H)
    {
        for (int32 F = 0; F < 5; ++F)
        {
            for (int32 J = 0; J < 3; ++J)
            {
                const FString Name = FString::Printf(TEXT("%s_%02d_%s"), FingerNames[F], J + 1, H == 0 ? TEXT("l") : TEXT("r"));
                FingerBones[H][F][J] = Ref.FindBoneIndex(FName(*Name));
                bHasFingers &= FingerBones[H][F][J] != INDEX_NONE;
            }
        }
    }

    const int32 Required[] = {
        Pelvis, Spine1, Spine2, Spine3, Neck, Head,
        LegL.Upper, LegL.Lower, LegL.End, LegR.Upper, LegR.Lower, LegR.End,
        ArmL.Upper, ArmL.Lower, ArmL.End, ArmR.Upper, ArmR.Lower, ArmR.End };
    for (int32 Index : Required)
    {
        if (Index == INDEX_NONE)
        {
            return;
        }
    }

    auto Dist = [this](int32 A, int32 B) { return FVector::Dist(CS[A].GetLocation(), CS[B].GetLocation()); };
    ThighLen = Dist(LegL.Upper, LegL.Lower);
    CalfLen = Dist(LegL.Lower, LegL.End);
    UpperArmLen = Dist(ArmL.Upper, ArmL.Lower);
    ForeArmLen = Dist(ArmL.Lower, ArmL.End);
    HipHalfWidth = Dist(LegL.Upper, LegR.Upper) * 0.5f;
    RefPelvisPos = CS[Pelvis].GetLocation();
    AnkleHeight = FMath::Max(3.0f, CS[LegL.End].GetLocation().Z);
    ShoulderHeight = CS[ArmL.Upper].GetLocation().Z;

    if (LegL.Tip != INDEX_NONE)
    {
        FootPitch = FMath::Clamp((CS[LegL.Tip].GetLocation() - CS[LegL.End].GetLocation()).GetSafeNormal().Z, -0.8f, 0.2f);
    }

    bReady = ThighLen > 1.0f && CalfLen > 1.0f && UpperArmLen > 1.0f && ForeArmLen > 1.0f;
    for (int32 H = 0; H < 2; ++H)
    {
        Grip[H] = 0.15f;
    }
}

void UBodyMotorComponent::BeginLearning()
{
    if (bLearningStarted)
    {
        return;
    }
    bLearningStarted = true;

    const uint32 Seed = GetOwner() ? GetTypeHash(GetOwner()->GetFName()) : 7u;
    Noise.Initialize(static_cast<int32>(Seed ^ 0x5bd1e995u));

    float Age = 30.0f;
    if (const ACompleteHumanNPC* Human = Cast<ACompleteHumanNPC>(GetOwner()))
    {
        if (Human->IdentityComponent)
        {
            Age = Human->IdentityComponent->Age;
        }
    }

    const float Growth = FMath::Clamp((Age - 1.5f) / 11.0f, 0.0f, 1.0f);
    const float Ageing = FMath::Clamp((Age - 62.0f) / 30.0f, 0.0f, 0.6f);
    ReachSkill = FMath::Clamp(0.12f + 0.85f * Growth - Ageing * 0.25f + Noise.FRandRange(-0.02f, 0.02f), 0.05f, 0.985f);
    SitSkill = FMath::Clamp(0.15f + 0.8f * Growth - Ageing * 0.2f, 0.05f, 0.97f);
    WorkSkill = FMath::Clamp(0.05f + 0.7f * Growth * FMath::Clamp((Age - 10.0f) / 15.0f, 0.0f, 1.0f) + Noise.FRandRange(-0.05f, 0.1f), 0.02f, 0.95f);
}

float UBodyMotorComponent::GraspChance(const AActor* Item) const
{
    float Chance = 0.3f + 0.695f * GetReachSkill();
    const ACompleteHumanNPC* Human = Cast<ACompleteHumanNPC>(GetOwner());
    if (const AResourceActor* Thing = Cast<AResourceActor>(Item))
    {
        if (const UMatterComponent* Matter = Thing->Matter)
        {
            const EMatterPhase Now = Matter->Phase();
            const float Wet = Matter->WetFraction();
            if (Now == EMatterPhase::Slurry)
            {
                Chance *= 0.7f;
            }
            else if (Now == EMatterPhase::Frozen && Wet > 0.3f)
            {
                Chance *= 0.9f;
            }
            else if (!Matter->Props().IsSoil() && Wet > 0.4f && Matter->Props().Porosity < 0.1f)
            {
                Chance *= 0.96f;
            }
            if (Matter->Temperature > 55.0f)
            {
                Chance *= 0.5f;
            }
        }
    }
    const float Strength = Human && Human->IdentityComponent && Human->IdentityComponent->Age < 14.0f ? 12.0f : 40.0f;
    if (ItemMass > Strength * 0.6f)
    {
        Chance *= FMath::Clamp(1.0f - (ItemMass / Strength - 0.6f), 0.2f, 1.0f);
    }
    if (FMath::Min3(ItemExtent.X, ItemExtent.Y, ItemExtent.Z) < 0.6f)
    {
        Chance *= 0.9f;
    }
    if (Human && Human->Mind)
    {
        if (const UHumanWorldSubsystem* WorldMind = GetWorld() ? GetWorld()->GetSubsystem<UHumanWorldSubsystem>() : nullptr)
        {
            const float Hour = WorldMind->Now.HourFloat;
            if (Hour < 5.5f || Hour > 21.5f)
            {
                Chance *= 0.85f;
            }
        }
        if (const UPhysiologyComponent* Body0 = Human->PhysiologyComponent)
        {
            Chance *= 1.0f - 0.5f * FMath::Clamp(Body0->GetCognitiveImpairment(), 0.0f, 1.0f);
            if (Body0->Body.Stamina < 0.15f)
            {
                Chance *= 0.9f;
            }
        }
    }
    if (const UMatterSubsystem* Matter = UMatterSubsystem::Get(this))
    {
        if (Matter->Climate().Celsius < -12.0f)
        {
            Chance *= 0.9f;
        }
    }
    return FMath::Clamp(Chance, 0.02f, 0.995f);
}

float UBodyMotorComponent::CarriedKilograms(const ACompleteHumanNPC* Human) const
{
    if (!Human || !Human->CarriedItem)
    {
        return 0.0f;
    }
    if (const AResourceActor* Thing = Cast<AResourceActor>(Human->CarriedItem.Get()))
    {
        return Thing->Matter ? Thing->Matter->TotalMassKg() : 1.0f;
    }
    if (const AFurnitureActor* Furniture = Cast<AFurnitureActor>(Human->CarriedItem.Get()))
    {
        return Furniture->MassKg;
    }
    return 0.5f;
}

FString UBodyMotorComponent::OwnWalkPath(const ACompleteHumanNPC* Human) const
{
    FString Name = Human && Human->IdentityComponent ? Human->IdentityComponent->GetFullName() : GetOwner()->GetName();
    Name = FPaths::MakeValidFileName(Name.Replace(TEXT(" "), TEXT("_")));
    return FMotorSchool::Folder() / TEXT("People") / (Name + TEXT("_Walk5.pol"));
}

void UBodyMotorComponent::BeginWalker(ACompleteHumanNPC* Human)
{
    bWalkerReady = true;
    float Age = 30.0f;
    bool bFemale = false;
    if (Human->IdentityComponent)
    {
        Age = Human->IdentityComponent->Age;
        bFemale = Human->IdentityComponent->bFemale;
    }
    FMotorSchool::MakeWalkBody(Age, bFemale, Walker);
    const float Scale = FMath::Max(0.3f, static_cast<float>(Human->GetActorScale3D().Z));
    if (bReady && ThighLen + CalfLen > 10.0f)
    {
        const float Leg = (ThighLen + CalfLen + AnkleHeight) * Scale * 0.01f;
        const float Ratio = Leg / FMath::Max(0.2f, Walker.Leg);
        Walker.Leg = Leg;
        Walker.FootFront *= Ratio;
        Walker.FootBack *= Ratio;
        Walker.FootSide *= Ratio;
    }
    Walker.Mass = Human->BodyMassKg();
    WalkKind = FMotorSchool::KindOf(Age, bFemale);

    bInfant = Age < 1.5f;
    if (bInfant)
    {
        InfantCycles = 0;
        WalkBrain = FMotorPolicy();
        if (WalkBrain.Load(OwnWalkPath(Human)))
        {
            InfantCycles = 60;
            ReadyToWalk(Human);
        }
        else
        {
            bWalkerUsable = false;
        }
        return;
    }

    bWalkerUsable = WalkBrain.Load(OwnWalkPath(Human));
    int32 Kind = WalkKind;
    while (!bWalkerUsable && Kind >= 0)
    {
        bWalkerUsable = FMotorSchool::Childhood(TEXT("Walk"), Kind, WalkBrain);
        Kind = FMotorSchool::ParentKind(Kind);
    }
    if (!bWalkerUsable)
    {
        bWalkerUsable = FMotorSchool::Childhood(TEXT("Walk"), 4, WalkBrain);
    }
    bWalkerUsable = bWalkerUsable && WalkBrain.ObservationSize() == FWalkSkill::Observations && WalkBrain.ActionSize() == FWalkSkill::Actions;
    WalkAction.Init(-1.0f, FWalkSkill::Actions);
    WalkAction[0] = 0.0f;
    WalkAction[1] = 0.0f;
    if (bWalkerUsable)
    {
        Lived.Begin(FWalkSkill::Observations, FWalkSkill::Actions, 1024);
        WalkBrain.RefreshNoise();
        Noise.Initialize(static_cast<int32>(GetTypeHash(Human->GetFName()) ^ 0x1f123bb5u));
    }
    if (!bWalkerUsable)
    {
        UE_LOG(LogHumanCity, Warning, TEXT("%s: нет выученной ходьбы для тела %d"), *Human->GetName(), WalkKind);
    }
}

ATerrainGrid* UBodyMotorComponent::TerrainUnder(ACompleteHumanNPC* Human, UHumanMovementComponent* Move) const
{
    ATerrainGrid* Terrain = Cast<ATerrainGrid>(Move->CurrentFloor.HitResult.GetActor());
    if (Terrain && Terrain->HasData())
    {
        return Terrain;
    }
    return nullptr;
}

FStepGround UBodyMotorComponent::GroundAt(const ATerrainGrid* Terrain, const FVector2D& At, float Kilograms, float Floor, float& OutZ) const
{
    FStepGround Ground;
    Ground.Grip = 0.75f;
    Ground.Rough = 0.6f;
    OutZ = Floor;
    if (!Terrain)
    {
        return Ground;
    }
    const FVector World(At.X * 100.0f, At.Y * 100.0f, Floor);
    FTerrainFooting Footing;
    if (!Terrain->FootingAt(World, Kilograms, Footing))
    {
        return Ground;
    }
    OutZ = Terrain->HeightAt(World);
    Ground.Grip = Footing.Grip;
    Ground.SinkCm = Footing.SinkCm;
    Ground.Stick = Footing.Stick;
    Ground.WaterCm = Footing.WaterCm;
    Ground.Rough = Terrain->RoughnessAt(World);
    const FVector Normal = Terrain->NormalAt(World);
    if (Normal.Z > 0.2f)
    {
        Ground.Rise = FVector2D(-Normal.X / Normal.Z, -Normal.Y / Normal.Z);
        LastSlope = Ground.Rise.Size();
        const float Felt = FMath::Min(LastSlope, 0.22f);
        if (LastSlope > Felt)
        {
            Ground.Rise *= Felt / LastSlope;
        }
    }
    return Ground;
}

void UBodyMotorComponent::RefreshGrounds(ACompleteHumanNPC* Human, UHumanMovementComponent* Move)
{
    const ATerrainGrid* Terrain = TerrainUnder(Human, Move);
    const float Kilograms = Walker.Mass + CarriedKilograms(Human);
    float Z = 0.0f;
    for (int32 Index = 0; Index < 2; ++Index)
    {
        WalkGrounds.Foot[Index] = GroundAt(Terrain, Walker.Feet[Index].At, Kilograms, FloorZ, Z);
    }
    WalkGrounds.Aim = GroundAt(Terrain, FWalkSkill::AimPoint(Walker), Kilograms, FloorZ, Z);
    const FStepGround& Under = WalkGrounds.Foot[Walker.Feet[0].bDown ? 0 : 1];
    FootSink = Under.SinkCm;
    GroundGrip = Under.Grip;
}

bool UBodyMotorComponent::FacingWish(ACompleteHumanNPC* Human, FVector& OutPoint) const
{
    if (WantsFacing(OutPoint))
    {
        return true;
    }
    if (const UMindComponent* Mind = Human->Mind)
    {
        if (const AActor* With = Mind->Dialogue.With.Get())
        {
            if (With != Human)
            {
                OutPoint = With->GetActorLocation();
                return true;
            }
        }
    }
    return false;
}

FVector UBodyMotorComponent::FootWorld(int32 Index, const ATerrainGrid* Terrain, float Floor) const
{
    const FWalkFoot& Foot = Walker.Feet[Index];
    FVector World(Foot.At.X * 100.0f, Foot.At.Y * 100.0f, Floor);
    if (Terrain)
    {
        World.Z = Terrain->HeightAt(World);
        if (Foot.bDown)
        {
            World.Z -= FMath::Min(Foot.Sink, 30.0f);
        }
    }
    World.Z += Foot.Z * 100.0f;
    return World;
}

void UBodyMotorComponent::FootDown(int32 Index, ACompleteHumanNPC* Human, UHumanMovementComponent* Move)
{
    ++WalkSteps;
    ATerrainGrid* Terrain = TerrainUnder(Human, Move);
    if (!Terrain)
    {
        return;
    }
    const float Kilograms = Walker.Mass + CarriedKilograms(Human);
    const FVector Foot = FootWorld(Index, Terrain, FloorZ);
    Terrain->Trample(Foot, Kilograms);
    FTerrainFooting Footing;
    if (Terrain->FootingAt(Foot, Kilograms, Footing) && Footing.bSoft && Footing.WaterCm < 25.0f)
    {
        Terrain->Footprint(Foot, FMath::RadiansToDegrees(Walker.Feet[Index].Yaw), Footing.SinkCm, Footing.Stick, Index == 0);
    }
}

void UBodyMotorComponent::CloseDecision(bool bFailed)
{
    if (!bStepOpen)
    {
        return;
    }
    bStepOpen = false;
    const FVector2D MeanVel = StepTime > 0.001f ? StepVel / StepTime : FVector2D::ZeroVector;
    const float Reward = FWalkSkill::RewardFrom(Walker.Mass, bFailed, Walker.Yaw, MeanVel, StepWant, StepFace,
        StepEnergy, StepSlip, StepTrips);
    Lived.Add(StepNormalized, StepObservation, WalkAction, StepLogProb, StepValue, Reward, bFailed);
    StepVel = FVector2D::ZeroVector;
    StepEnergy = 0.0f;
    StepSlip = 0.0f;
    StepTime = 0.0f;
    StepTrips = 0;
}

void UBodyMotorComponent::MaybeStudy(ACompleteHumanNPC* Human)
{
    if (Studying.IsValid())
    {
        if (!Studying.IsReady())
        {
            return;
        }
        WalkBrain = Studying.Get();
        WalkBrain.RefreshNoise();
        Studying = TFuture<FMotorPolicy>();
        ++Studies;
        if (Studies % 4 == 0)
        {
            WalkBrain.Save(OwnWalkPath(Human));
        }
        return;
    }
    if (!Lived.IsFull())
    {
        return;
    }
    FMotorPolicy Learner = WalkBrain;
    FMotorBuffer Story = Lived;
    Lived.Forget();
    const float Tail = StepValue;
    Studying = Async(EAsyncExecution::ThreadPool, [Learner, Story, Tail]() mutable
    {
        FLifeLesson How;
        FLifeLesson::Study(Learner, Story, Tail, How);
        return Learner;
    });
}

void UBodyMotorComponent::FallOver(ACompleteHumanNPC* Human, UHumanMovementComponent* Move)
{
    ++WalkFalls;
    {
        const AActor* FloorActor = Move->CurrentFloor.HitResult.GetActor();
        const FVector2D WantLocal = Walker.ToBody(LastWant);
        UE_LOG(LogHumanCity, Log, TEXT("WALKFALL %s kind %d cause %d want %.2f/%.2f face %d vel %.2f bump %.3f blocked %.2f walking %.1f fatigue %.2f load %.2f grip %.2f frame %.3f floor %s"),
            *Human->GetName(), WalkKind, Walker.FallCause, WantLocal.X, WantLocal.Y, LastFace.IsNearlyZero() ? 0 : 1, Walker.Vel.Size(),
            BumpRecent, BlockedFor, WalkingFor, Walker.Fatigue, Walker.Load, GroundGrip, LastFrame,
            FloorActor ? *FloorActor->GetClass()->GetName() : TEXT("none"));
    }
    CloseDecision(true);
    FVector2D Toward = Walker.FallDirection.IsNearlyZero() ? Walker.Forward() : Walker.FallDirection.GetSafeNormal();
    if (Toward.IsNearlyZero())
    {
        Toward = FVector2D(1.0f, 0.0f);
    }
    const float Height = FMath::Max(0.3f, Walker.Height());
    const float Gap = FVector2D::Distance(Walker.Com, Walker.SupportPoint());
    ToppleDir = FVector(Toward.X, Toward.Y, 0.0f);
    ToppleSpeed = Walker.Vel.Size();
    ToppleAngle = FMath::Clamp(FMath::Atan2(Gap, Height), 0.12f, 0.6f);
    ToppleRate = FMath::Clamp(FVector2D::DotProduct(Walker.Vel, Toward) / Height, 0.25f, 3.0f);
    ToppleTime = 0.0f;
    FallCrouch = 0.0f;
    PivotAhead = Walker.FootFront * 100.0f;
    bWalkerActive = false;
    bFallenDown = true;
    bToppling = true;
    bRising = false;
    StumbleTime = 0.0f;
    Move->ReleaseBody();
    Move->StopMovementImmediately();
    Human->BeginFalling();
    if (StartRagdoll(Human))
    {
        bToppling = false;
        bRagdoll = true;
    }
}

bool UBodyMotorComponent::StartRagdoll(ACompleteHumanNPC* Human)
{
    USkeletalMeshComponent* Doll = Human ? Human->GetMesh() : nullptr;
    USkeletalMesh* Asset = Body ? Cast<USkeletalMesh>(Body->GetSkinnedAsset()) : nullptr;
    if (!Doll || !Asset || !Asset->GetPhysicsAsset() || !bReady || !Body->IsVisible())
    {
        return false;
    }
    if (Doll->GetSkeletalMeshAsset() != Asset)
    {
        Doll->SetSkeletalMesh(Asset);
    }
    Doll->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    Doll->SetComponentTickEnabled(true);
    Doll->SetWorldTransform(Body->GetComponentTransform());
    Doll->SetCollisionProfileName(TEXT("Ragdoll"));
    Doll->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Doll->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
    Doll->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
    if (!Doll->GetBodyInstance())
    {
        Doll->RecreatePhysicsState();
    }
    const TArray<FTransform>& Pose = Body->GetComponentSpaceTransforms();
    TArray<FTransform>& Into = Doll->GetEditableComponentSpaceTransforms();
    if (Pose.Num() != Into.Num() || Pose.Num() == 0)
    {
        Doll->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Doll->SetComponentTickEnabled(false);
        return false;
    }
    for (int32 Index = 0; Index < Pose.Num(); ++Index)
    {
        Into[Index] = Pose[Index];
    }
    Doll->FinalizeBoneTransform();
    Doll->UpdateKinematicBonesToAnim(Doll->GetComponentSpaceTransforms(), ETeleportType::TeleportPhysics, true,
        EAllowKinematicDeferral::DisallowDeferral);
    Doll->SetAllBodiesSimulatePhysics(true);
    Doll->SetAllBodiesPhysicsBlendWeight(1.0f);
    Doll->WakeAllRigidBodies();
    Doll->SetAllPhysicsLinearVelocity(FVector(Walker.Vel.X * 100.0f, Walker.Vel.Y * 100.0f, 0.0f));
    if (Spine3 != INDEX_NONE)
    {
        Doll->AddImpulse(ToppleDir * (60.0f + 40.0f * ToppleRate), Body->GetBoneName(Spine3), true);
    }
    Doll->SetVisibility(true, true);
    Body->SetVisibility(false);
    if (UCapsuleComponent* Capsule = Human->GetCapsuleComponent())
    {
        Capsule->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Ignore);
    }
    DollTime = 0.0f;
    DollCalm = 0.0f;
    DollImpact = 0.0f;
    bDollLying = false;
    const float Tall = BodyHeightMetres(Human) * 100.0f;
    const FVector Feet = Body->GetComponentLocation();
    const FVector Side = FVector::CrossProduct(FVector::UpVector, ToppleDir).GetSafeNormal();
    CatchWorld[0] = Feet + ToppleDir * (Tall * 0.72f) - Side * 22.0f;
    CatchWorld[1] = Feet + ToppleDir * (Tall * 0.72f) + Side * 22.0f;
    return true;
}

void UBodyMotorComponent::AdvanceRagdoll(float Dt, ACompleteHumanNPC* Human)
{
    USkeletalMeshComponent* Doll = Human->GetMesh();
    if (!Doll)
    {
        bRagdoll = false;
        return;
    }
    DollTime += Dt;
    const FName Hips = Pelvis != INDEX_NONE ? Body->GetBoneName(Pelvis) : NAME_None;
    if (!bDollLying)
    {
        if (DollTime < 0.75f && DollTime > 0.1f)
        {
            const FLimb* Hands[2] = { &ArmL, &ArmR };
            for (int32 Index = 0; Index < 2; ++Index)
            {
                if (Hands[Index]->End == INDEX_NONE)
                {
                    continue;
                }
                const FName Hand = Body->GetBoneName(Hands[Index]->End);
                const FVector Toward = (CatchWorld[Index] - Doll->GetBoneLocation(Hand)).GetSafeNormal();
                Doll->AddForce(Toward * 1500.0f * Walker.Strength, Hand, true);
            }
        }
        const float Speed = Hips.IsNone() ? 0.0f : static_cast<float>(Doll->GetPhysicsLinearVelocity(Hips).Size());
        DollImpact = FMath::Max(DollImpact, Speed * 0.01f);
        DollCalm = Speed < 25.0f ? DollCalm + Dt : 0.0f;
        if (DollCalm > 0.5f || DollTime > 4.0f)
        {
            bDollLying = true;
            const FVector Hip = Hips.IsNone() ? Human->GetActorLocation() : Doll->GetBoneLocation(Hips);
            FVector Spot = Hip;
            FVector Lay = Hip - Body->GetComponentLocation();
            Lay.Z = 0.0f;
            const FVector Along = Lay.IsNearlyZero() ? ToppleDir : Lay.GetSafeNormal();
            const float Weakness = FMath::Clamp(1.4f - Walker.Strength, 0.4f, 1.2f);
            FallClock = FMath::FRandRange(1.2f, 2.4f) * Weakness + ToppleSpeed * 0.6f;
            const FTransform Keep = Doll->GetComponentTransform();
            Human->FallDown(Along, FMath::Max(ToppleSpeed, DollImpact * 0.5f), &Spot);
            if (UWorld* World = Human->GetWorld())
            {
                FCollisionQueryParams Params(SCENE_QUERY_STAT(DollGround), false, Human);
                FHitResult Hit;
                if (World->LineTraceSingleByChannel(Hit, Hip + FVector(0.0f, 0.0f, 60.0f), Hip - FVector(0.0f, 0.0f, 400.0f), ECC_WorldStatic, Params))
                {
                    const float Scale = FMath::Max(0.3f, static_cast<float>(Human->GetActorScale3D().Z));
                    Human->SettleAt(Hit.ImpactPoint + FVector(0.0f, 0.0f, 41.0f * Scale));
                }
            }
            Doll->SetWorldTransform(Keep);
            Doll->PutAllRigidBodiesToSleep();
        }
        return;
    }
    if (!Human->IsAlive())
    {
        return;
    }
    FallClock -= Dt;
    if (FallClock <= 0.0f)
    {
        EndRagdoll(Human);
    }
}

void UBodyMotorComponent::EndRagdoll(ACompleteHumanNPC* Human)
{
    USkeletalMeshComponent* Doll = Human->GetMesh();
    TArray<FTransform> World;
    if (Doll)
    {
        const TArray<FTransform>& Pose = Doll->GetComponentSpaceTransforms();
        const FTransform DollToWorld = Doll->GetComponentTransform();
        World.SetNum(Pose.Num());
        for (int32 Index = 0; Index < Pose.Num(); ++Index)
        {
            World[Index] = Pose[Index] * DollToWorld;
        }
        Doll->SetAllBodiesSimulatePhysics(false);
        Doll->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Doll->SetVisibility(false, true);
        Doll->SetComponentTickEnabled(false);
    }
    if (UCapsuleComponent* Capsule = Human->GetCapsuleComponent())
    {
        Capsule->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
    }
    bRagdoll = false;
    bDollLying = false;
    bFallenDown = false;
    Human->SetPosture(EPosture::Standing, FVector::ZeroVector);
    Human->SettleAt(Human->PostureGoal());
    if (Body)
    {
        Body->SetVisibility(true);
        const FTransform ToWorld = Body->GetComponentTransform();
        if (World.Num() == Parents.Num() && World.Num() == Smoothed.Num())
        {
            TArray<FTransform> Space;
            Space.SetNum(World.Num());
            for (int32 Index = 0; Index < World.Num(); ++Index)
            {
                Space[Index] = World[Index].GetRelativeTransform(ToWorld);
            }
            for (int32 Index = 0; Index < World.Num(); ++Index)
            {
                const int32 Parent = Parents[Index];
                FTransform LocalPose = Parent >= 0 ? Space[Index].GetRelativeTransform(Space[Parent]) : Space[Index];
                LocalPose.SetScale3D(Smoothed[Index].GetScale3D());
                Smoothed[Index] = LocalPose;
            }
            Body->BoneSpaceTransforms = Smoothed;
            Body->MarkRefreshTransformDirty();
        }
    }
    bRising = true;
    bRiseFromDoll = true;
    RiseTime = 0.0f;
    FallCrouch = 1.0f;
}

float UBodyMotorComponent::BodyHeightMetres(const ACompleteHumanNPC* Human) const
{
    const float Scale = Human ? FMath::Max(0.3f, static_cast<float>(Human->GetActorScale3D().Z)) : 1.0f;
    const float Tall = ShoulderHeight > 20.0f ? ShoulderHeight * 1.2f : 175.0f;
    return FMath::Max(0.6f, Tall * Scale * 0.01f);
}

void UBodyMotorComponent::TiltBody(ACompleteHumanNPC* Human, float Angle, const FVector& Toward, float Ahead)
{
    if (!Body || !Human)
    {
        return;
    }
    if (!bTilted)
    {
        UprightRelative = Body->GetRelativeTransform();
    }
    if (Angle <= 1.0e-3f)
    {
        if (bTilted)
        {
            Body->SetRelativeTransform(UprightRelative);
            bTilted = false;
        }
        return;
    }
    const USceneComponent* Parent = Body->GetAttachParent();
    const FTransform ParentWorld = Parent ? Parent->GetComponentTransform() : Human->GetActorTransform();
    const FTransform Upright = UprightRelative * ParentWorld;
    const FVector Around = Upright.GetLocation() + Toward * Ahead;
    const FVector Axis = FVector::CrossProduct(FVector::UpVector, Toward).GetSafeNormal();
    if (Axis.IsNearlyZero())
    {
        return;
    }
    const FQuat Turn(Axis, Angle);
    FTransform Tilted = Upright;
    Tilted.SetLocation(Around + Turn.RotateVector(Upright.GetLocation() - Around));
    Tilted.SetRotation(Turn * Upright.GetRotation());
    Body->SetWorldTransform(Tilted);
    bTilted = true;
    const FVector Side = FVector::CrossProduct(FVector::UpVector, Toward).GetSafeNormal();
    const float Reach = BodyHeightMetres(Human) * 100.0f * 0.72f;
    const FVector Ground(Around.X, Around.Y, Upright.GetLocation().Z + 3.0f);
    CatchWorld[0] = Ground + Toward * Reach - Side * 22.0f;
    CatchWorld[1] = Ground + Toward * Reach + Side * 22.0f;
}

void UBodyMotorComponent::AdvanceTopple(float Dt, ACompleteHumanNPC* Human)
{
    const float Tall = BodyHeightMetres(Human);
    if (bToppling)
    {
        ToppleTime += Dt;
        const int32 Slices = FMath::Clamp(FMath::CeilToInt(Dt / 0.004f), 1, 60);
        const float Slice = Dt / Slices;
        for (int32 S = 0; S < Slices; ++S)
        {
            const float Pull = 1.5f * 9.81f / Tall * FMath::Sin(ToppleAngle) * (1.0f + 0.5f * FallCrouch);
            ToppleRate += (Pull - 0.6f * ToppleRate) * Slice;
            ToppleAngle += ToppleRate * Slice;
        }
        FallCrouch = FMath::Min(1.0f, FallCrouch + Dt * 1.8f);
        if (ToppleAngle >= FMath::DegreesToRadians(84.0f) || ToppleTime > 2.5f || !Human->IsAlive())
        {
            TiltBody(Human, 0.0f, ToppleDir, PivotAhead);
            bToppling = false;
            const float Scale = FMath::Max(0.3f, static_cast<float>(Human->GetActorScale3D().Z));
            const FVector Here = Human->GetActorLocation();
            const FVector Spot = Here + ToppleDir * (PivotAhead + (RefPelvisPos.Z + 14.0f) * Scale);
            const float Weakness = FMath::Clamp(1.4f - Walker.Strength, 0.4f, 1.2f);
            FallClock = FMath::FRandRange(1.2f, 2.4f) * Weakness + ToppleSpeed * 0.6f;
            FallCrouch = 0.0f;
            Human->FallDown(ToppleDir, ToppleSpeed, &Spot);
            return;
        }
        TiltBody(Human, ToppleAngle, ToppleDir, PivotAhead);
        return;
    }
    if (bRising)
    {
        RiseTime += Dt;
        const float Total = 1.6f;
        const float Up = FMath::SmoothStep(0.0f, 1.0f, FMath::Clamp(RiseTime / 0.85f, 0.0f, 1.0f));
        FallCrouch = 1.0f - FMath::SmoothStep(0.0f, 1.0f, FMath::Clamp((RiseTime - 0.5f) / (Total - 0.5f), 0.0f, 1.0f));
        if (!bRiseFromDoll)
        {
            TiltBody(Human, FMath::DegreesToRadians(60.0f) * (1.0f - Up), ToppleDir, 0.0f);
        }
        if (RiseTime >= Total || !Human->IsAlive())
        {
            TiltBody(Human, 0.0f, ToppleDir, 0.0f);
            bRising = false;
            bRiseFromDoll = false;
            FallCrouch = 0.0f;
        }
    }
}

FString UBodyMotorComponent::DescribeWalking() const
{
    if (!bWalkerUsable)
    {
        return TEXT("ходить не умеет");
    }
    const float Minutes = static_cast<float>(FMath::Max(1.0 / 60.0, WalkSeconds / 60.0));
    FString Feet;
    for (int32 Index = 0; Index < 2; ++Index)
    {
        const FWalkFoot& Foot = Walker.Feet[Index];
        const FVector2D Rel = Walker.ToBody(Foot.At - Walker.Com);
        Feet += FString::Printf(TEXT("%s%s (%+.0f,%+.0f см%s) "), Index == 0 ? TEXT("Л:") : TEXT("П:"),
            Foot.bDown ? TEXT("стоит") : *FString::Printf(TEXT("в воздухе %.0f см"), Foot.Z * 100.0f),
            Rel.X * 100.0f, Rel.Y * 100.0f, Foot.bDown && Foot.Sink > 0.5f ? *FString::Printf(TEXT(", вязнет %.0f"), Foot.Sink) : TEXT(""));
    }
    return FString::Printf(TEXT("%sхочет %.2f м/с, идёт %.2f м/с | опыт %.1f ч, шагов %d, падений %d (%.2f/мин), спотыканий %d, умение %.0f%%%s"),
        *Feet, LastWant.Size(), Walker.Vel.Size(),
        WalkBrain.Experience * FWalkSkill::Interval / 3600.0 + WalkSeconds / 3600.0, WalkSteps, WalkFalls, WalkFalls / Minutes, WalkTrips,
        WalkSkill * 100.0f, bWalkerActive ? TEXT("") : (bFallenDown ? TEXT(", лежит после падения") : TEXT(", ноги не ведёт")));
}

void UBodyMotorComponent::AdvanceWalker(float Dt, ACompleteHumanNPC* Human)
{
    UHumanMovementComponent* Move = Cast<UHumanMovementComponent>(Human->GetCharacterMovement());
    if (!Move || Dt <= 0.0f)
    {
        return;
    }
    if (!bWalkerReady)
    {
        BeginWalker(Human);
    }

    if (bRagdoll)
    {
        AdvanceRagdoll(Dt, Human);
        return;
    }
    if (bToppling || bRising)
    {
        AdvanceTopple(Dt, Human);
        if (bToppling || bRising)
        {
            return;
        }
    }

    if (bFallenDown)
    {
        FallClock -= Dt;
        const bool bStillThere = Human->Posture == EPosture::Lying;
        if (!Human->IsAlive() || !bStillThere)
        {
            bFallenDown = false;
        }
        else if (FallClock <= 0.0f)
        {
            bFallenDown = false;
            Human->SetPosture(EPosture::Standing, FVector::ZeroVector);
            bRising = true;
            RiseTime = 0.0f;
            FallCrouch = 1.0f;
            AdvanceTopple(0.0f, Human);
            return;
        }
    }

    const bool bCan = bWalkerUsable && !bFallenDown && Human->IsAlive() && Human->Posture == EPosture::Standing
        && Move->MovementMode == MOVE_Walking;
    if (!bCan)
    {
        if (bWalkerActive)
        {
            bWalkerActive = false;
            Move->ReleaseBody();
        }
        return;
    }

    const FVector Here = Human->GetActorLocation();
    FloorZ = static_cast<float>(Here.Z) - Human->GetSimpleCollisionHalfHeight();
    const FVector2D At(Here.X * 0.01f, Here.Y * 0.01f);
    BumpRecent *= FMath::Exp(-Dt / 0.6f);
    if (!bWalkerActive || FVector2D::Distance(At, Walker.Com) > 0.6f)
    {
        const float Mass = Walker.Mass;
        Walker.Place(At, FMath::DegreesToRadians(Human->GetActorRotation().Yaw), 0.22f * Walker.Leg);
        Walker.Mass = Mass;
        bWalkerActive = true;
        WalkDecision = 0.0f;
        DrivenVelocity = FVector2D::ZeroVector;
        LastWalkVel = FVector2D::ZeroVector;
        WalkingFor = 0.0f;
        BlockedFor = 0.0f;
    }
    else
    {
        const FVector2D Off = At - Walker.Com;
        const float Gap = Off.Size();
        if (Gap > 0.015f)
        {
            const FVector2D Normal = Off / Gap;
            const float Into = FVector2D::DotProduct(Walker.Vel, Normal);
            if (Into < 0.0f)
            {
                Walker.Vel -= Normal * Into;
            }
            Walker.Com = At;
            BumpRecent = FMath::Max(BumpRecent, Gap);
            BlockedFor += Dt;
        }
        else
        {
            BlockedFor = 0.0f;
        }
        WalkingFor += Dt;
    }

    const float Carried = CarriedKilograms(Human);
    Walker.Load = Carried / FMath::Max(10.0f, Walker.Mass);
    if (const UPhysiologyComponent* Physiology = Human->PhysiologyComponent)
    {
        Walker.Fatigue = FMath::Clamp(1.0f - Physiology->Body.Stamina, 0.0f, 1.0f);
    }

    const FVector Intent = Move->GetIntent();
    FVector2D Want(Intent.X * 0.01f, Intent.Y * 0.01f);
    const float Top = FMath::Min(FWalkSkill::TopSpeed(WalkBrain.Difficulty, Walker.Leg), 1.15f * FMath::Sqrt(FMath::Max(0.3f, Walker.Leg) / 0.9f));
    CatchClock = FMath::Max(0.0f, CatchClock - Dt);
    if (Want.SizeSquared() > Top * Top)
    {
        Want = Want.GetSafeNormal() * Top;
    }
    FVector2D Face = FVector2D::ZeroVector;
    FVector FacePoint;
    if (FacingWish(Human, FacePoint))
    {
        Face = FVector2D(FacePoint.X - Here.X, FacePoint.Y - Here.Y).GetSafeNormal();
    }
    LastWant = Want;
    LastFace = Face;

    float Left = Dt;
    for (int32 Round = 0; Round < 16 && Left > 1.0e-4f; ++Round)
    {
        if (WalkDecision <= 0.0f)
        {
            WalkDecision = FMath::Max(WalkDecision + FWalkSkill::Interval, 0.01f);
            CloseDecision(false);
            MaybeStudy(Human);
            RefreshGrounds(Human, Move);
            FWalkSkill::Observe(Walker, Want, Face, WalkGrounds, WalkObservation);
            if (++NoiseAge > 8 || WalkNoise.Num() != WalkBrain.LogStd.Num())
            {
                NoiseAge = 0;
                WalkBrain.SampleNoise(Noise, WalkNoise);
                for (float& Jitter : WalkNoise)
                {
                    Jitter *= 0.35f;
                }
            }
            WalkBrain.Act(WalkObservation, WalkNormalized, WalkAction, StepLogProb, StepValue, &WalkNoise);
            for (float& Command : WalkAction)
            {
                Command = FMath::Clamp(Command, -1.0f, 1.0f);
            }
            StepObservation = WalkObservation;
            StepNormalized = WalkNormalized;
            StepWant = Want;
            StepFace = Face;
            bStepOpen = true;
        }

        const float Chunk = FMath::Min(Left, WalkDecision);
        WalkDecision -= Chunk;
        Left -= Chunk;
        const bool bWasDown[2] = { Walker.Feet[0].bDown, Walker.Feet[1].bDown };
        FWalkSkill::Simulate(Walker, WalkAction, WalkGrounds, Chunk);
        WalkSeconds += Chunk;
        WalkTrips += Walker.Trips;
        StepVel += Walker.MeanVel * Chunk;
        StepEnergy += Walker.Energy;
        StepSlip += Walker.Slipped;
        StepTrips += Walker.Trips;
        StepTime += Chunk;
        if (Walker.Trips > 0)
        {
            StumbleTime = 0.5f;
        }
        for (int32 Index = 0; Index < 2; ++Index)
        {
            if (!bWasDown[Index] && Walker.Feet[Index].bDown)
            {
                FootDown(Index, Human, Move);
            }
        }
        const float Reward = FWalkSkill::Reward(Walker, Want, Face);
        WalkSkill = FMath::Lerp(WalkSkill, FMath::Clamp((Reward - 0.3f) / 0.7f, 0.0f, 1.0f), FMath::Min(1.0f, Chunk * 0.05f));

        if (Walker.bFallen)
        {
            if (CatchClock <= 0.0f && Walker.FallCause <= 2 && Walker.Vel.Size() < 2.8f && Human->IsAlive())
            {
                CatchClock = 1.5f;
                ++WalkCatches;
                UE_LOG(LogHumanCity, Log, TEXT("WALKCATCH %s kind %d cause %d vel %.2f want %.2f slope %.2f"), *Human->GetName(), WalkKind, Walker.FallCause,
                    Walker.Vel.Size(), LastWant.Size(), LastSlope);
                CloseDecision(true);
                const FVector2D Keep = Walker.Vel * 0.35f;
                const float Mass = Walker.Mass;
                const float Load = Walker.Load;
                const float Tired = Walker.Fatigue;
                Walker.Place(Walker.Com + Keep.GetSafeNormal() * 0.04f * Walker.Leg, Walker.Yaw, 0.22f * Walker.Leg);
                Walker.Mass = Mass;
                Walker.Load = Load;
                Walker.Fatigue = Tired;
                Walker.Vel = Keep;
                WalkDecision = 0.0f;
                StumbleTime = 0.6f;
                continue;
            }
            FallOver(Human, Move);
            return;
        }
    }

    DrivenVelocity = Walker.Vel;
    const float Frame = FMath::Max(LastFrame > 0.0f ? LastFrame : Dt, 1.0e-3f);
    const FVector2D Stride = (Walker.Com - At) / Frame;
    Move->DriveByBody(FVector(Stride.X * 100.0f, Stride.Y * 100.0f, 0.0f));
    Human->SetActorRotation(FRotator(0.0f, FMath::RadiansToDegrees(Walker.Yaw), 0.0f));
}

float UBodyMotorComponent::GestureLevel(FName Gesture) const
{
    const float* Level = Gestures.Find(Gesture);
    return Level ? *Level : 0.0f;
}

void UBodyMotorComponent::ObserveGesture(FName Gesture, float Attention)
{
    float& Level = Gestures.FindOrAdd(Gesture);
    Level = FMath::Clamp(Level + 0.1f * FMath::Clamp(Attention, 0.0f, 1.0f) * (1.0f - Level), 0.0f, 1.0f);
}

void UBodyMotorComponent::PracticeGesture(FName Gesture, float Amount)
{
    float& Level = Gestures.FindOrAdd(Gesture);
    Level = FMath::Clamp(Level + Amount * (1.0f - Level), 0.0f, 1.0f);
}

FString UBodyMotorComponent::DescribeBody() const
{
    return FString::Printf(TEXT("ходьба %.0f%%, руки %.0f%%, посадка %.0f%%, работа руками %.0f%%, машет %.0f%%, жестикулирует %.0f%%"),
        GetWalkSkill() * 100.0f, GetReachSkill() * 100.0f, SitSkill * 100.0f, WorkSkill * 100.0f,
        GestureLevel(TEXT("Wave")) * 100.0f, GestureLevel(TEXT("Explain")) * 100.0f);
}

FString UBodyMotorComponent::DescribeHands() const
{
    static const TCHAR* Names[] = { TEXT("руки свободны"), TEXT("подходит к вещи"), TEXT("берёт"), TEXT("кладёт") };
    const int32 Index = FMath::Clamp(static_cast<int32>(Stage), 0, 3);
    FString Out = Names[Index];
    if (bHandsUsable)
    {
        Out += FString::Printf(TEXT(" [пальцы %.2f/%.2f, хват %.0f/%.0f Н, наклон %.0f°]"),
            Grips[0].Curl, Grips[1].Curl, Grips[0].Squeeze, Grips[1].Squeeze,
            FMath::RadiansToDegrees((Arms[0].Pitch + Arms[1].Pitch) * 0.5f));
    }
    if (const AActor* Item = HandItem.Get())
    {
        Out += FString::Printf(TEXT(" (%s)"), *Item->GetName());
    }
    if (Bend > 0.05f)
    {
        Out += FString::Printf(TEXT(", наклон %.0f%%, присед %.0f%%"), Bend * 100.0f, Squat * 100.0f);
    }
    if (FootSink > 1.0f)
    {
        Out += FString::Printf(TEXT(", ноги вязнут на %.0f см, усилие x%.1f"), FootSink, GroundEffort);
    }
    if (!LastHandNote.IsEmpty())
    {
        Out += TEXT(", ") + LastHandNote;
    }
    return Out;
}

bool UBodyMotorComponent::MeasureItem(const AActor* Item, FVector& OutCentre, FVector& OutExtent, float& OutMass) const
{
    if (!Item)
    {
        return false;
    }
    FBox Box(ForceInit);
    OutMass = 1.0f;
    if (const AResourceActor* Thing = Cast<AResourceActor>(Item))
    {
        if (Thing->Body)
        {
            Box = Thing->Body->Bounds.GetBox();
        }
        OutMass = Thing->Matter ? Thing->Matter->TotalMassKg() : 1.0f;
    }
    else if (const ABookActor* Book = Cast<ABookActor>(Item))
    {
        if (Book->Cover)
        {
            Box = Book->Cover->Bounds.GetBox();
        }
        OutMass = 0.5f;
    }
    else if (const AFurnitureActor* Furniture = Cast<AFurnitureActor>(Item))
    {
        if (Furniture->Body)
        {
            Box = Furniture->Body->Bounds.GetBox();
        }
        OutMass = Furniture->MassKg;
    }
    if (!Box.IsValid)
    {
        Box = FBox::BuildAABB(Item->GetActorLocation(), FVector(6.0f));
    }
    OutCentre = Box.GetCenter();
    OutExtent = Box.GetExtent().ComponentMax(FVector(1.0f));
    return true;
}

FVector UBodyMotorComponent::PlacePointFor(ACompleteHumanNPC* Human, bool bReturnToPlace, bool& bOutShelf) const
{
    bOutShelf = false;
    AActor* Item = Human ? Human->CarriedItem.Get() : nullptr;
    if (!Human || !Item)
    {
        return FVector::ZeroVector;
    }
    if (bReturnToPlace)
    {
        if (const ABookActor* Book = Cast<ABookActor>(Item))
        {
            if (!Book->ShelfLocation.IsZero() && FVector::Dist2D(Book->ShelfLocation, Human->GetActorLocation()) < 900.0f)
            {
                bOutShelf = true;
                return Book->ShelfLocation;
            }
        }
    }

    const FVector Front = Human->GetActorLocation() + Human->GetActorForwardVector() * 52.0f;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(HandsPlace), false, Human);
    Params.AddIgnoredActor(Item);
    FHitResult Hit;
    const float HalfHeight = FMath::Max(1.0f, static_cast<float>(ItemExtent.Z));
    if (UWorld* World = Human->GetWorld())
    {
        if (World->LineTraceSingleByChannel(Hit, Front + FVector(0.0f, 0.0f, 40.0f), Front - FVector(0.0f, 0.0f, 260.0f), ECC_WorldStatic, Params))
        {
            return Hit.ImpactPoint + FVector(0.0f, 0.0f, HalfHeight + 0.5f);
        }
    }
    return Front - FVector(0.0f, 0.0f, 88.0f - HalfHeight);
}

bool UBodyMotorComponent::BeginTake(AActor* Item)
{
    ACompleteHumanNPC* Human = Cast<ACompleteHumanNPC>(GetOwner());
    if (!bReady || !Human || !Item || !Body || !Body->IsVisible())
    {
        return false;
    }
    if (Human->CarriedItem == Item)
    {
        return true;
    }
    if (IsTaking(Item))
    {
        return true;
    }
    if (Stage != EHandStage::Idle)
    {
        return false;
    }
    if (!IsFreeFor(Item, Human))
    {
        FailedItem = Item;
        FailedAt = Clock;
        LastHandNote = TEXT("вещь занята");
        return false;
    }
    if (Human->CarriedItem)
    {
        QueuedTake = Item;
        if (!BeginPut(true))
        {
            QueuedTake = nullptr;
            Human->LetGo(false);
            QueuedTake = Item;
        }
        return true;
    }

    FVector Centre;
    FVector Extent;
    float Mass = 1.0f;
    MeasureItem(Item, Centre, Extent, Mass);

    float Strength = 45.0f;
    if (Human->IdentityComponent)
    {
        const float Age = Human->IdentityComponent->Age;
        Strength = Age < 14.0f ? 8.0f + Age * 1.5f : (Age > 65.0f ? 30.0f : 45.0f);
    }
    if (Mass > Strength)
    {
        FailedItem = Item;
        FailedAt = Clock;
        LastHandNote = FString::Printf(TEXT("не поднять: %.0f кг"), Mass);
        return false;
    }

    HandItem = Item;
    QueuedTake = nullptr;
    bPutting = false;
    bFumbled = false;
    bPutAfterLift = false;
    GraspTries = 0;
    ItemExtent = Extent;
    ItemMass = Mass;
    bTwoHands = Mass > 3.5f || Extent.GetMax() > 20.0f;
    const FVector ToItem = Centre - Human->GetActorLocation();
    HandSide = FVector::DotProduct(ToItem, Human->GetActorRightVector()) < -15.0f ? -1.0f : 1.0f;
    GoalWorld = Centre + FVector(0.0f, 0.0f, bTwoHands ? 0.0f : Extent.Z * 0.35f);
    LastHandNote.Reset();
    EnterStage(EHandStage::Approach);
    return true;
}

bool UBodyMotorComponent::BeginPut(bool bReturnToPlace)
{
    ACompleteHumanNPC* Human = Cast<ACompleteHumanNPC>(GetOwner());
    if (!bReady || !Human || !Human->CarriedItem || !Body || !Body->IsVisible())
    {
        return false;
    }
    if (Stage != EHandStage::Idle)
    {
        return bPutting;
    }

    FVector Centre;
    FVector Extent;
    float Mass = 1.0f;
    MeasureItem(Human->CarriedItem, Centre, Extent, Mass);
    ItemExtent = Extent;
    ItemMass = Mass;

    bPutting = true;
    bReturnPut = bReturnToPlace;
    bFumbled = false;
    HandItem = Human->CarriedItem;
    PlaceWorld = PlacePointFor(Human, bReturnToPlace, bShelfPut);
    GoalWorld = PlaceWorld;

    EnterStage(FVector::Dist2D(PlaceWorld, Human->GetActorLocation()) > 75.0f ? EHandStage::Approach : EHandStage::Putting);
    return true;
}

bool UBodyMotorComponent::IsTaking(const AActor* Item) const
{
    if (!Item)
    {
        return false;
    }
    if (QueuedTake.Get() == Item)
    {
        return true;
    }
    if (bPutting || HandItem.Get() != Item)
    {
        return false;
    }
    return Stage == EHandStage::Approach || Stage == EHandStage::Taking;
}

bool UBodyMotorComponent::TakeFailed(const AActor* Item) const
{
    return Item && FailedItem.Get() == Item && Clock - FailedAt < 6.0;
}

bool UBodyMotorComponent::WantsApproach(FVector& OutPoint, float& OutSpeed) const
{
    if (Stage != EHandStage::Approach)
    {
        return false;
    }
    OutPoint = GoalWorld;
    OutSpeed = bPutting ? 0.8f : 0.75f;
    return true;
}

bool UBodyMotorComponent::WantsFacing(FVector& OutPoint) const
{
    if (Stage == EHandStage::Idle)
    {
        return false;
    }
    OutPoint = GoalWorld;
    return true;
}

void UBodyMotorComponent::ForgetHeld()
{
    OffsetFor = nullptr;
}

void UBodyMotorComponent::CancelTake(bool bReturnIfHeld)
{
    QueuedTake = nullptr;
    if (bPutting)
    {
        return;
    }
    if (Stage == EHandStage::Approach || Stage == EHandStage::Taking)
    {
        FailHands(TEXT("передумал брать"));
        FailedItem = nullptr;
        return;
    }
    if (bReturnIfHeld && Stage == EHandStage::Idle)
    {
        const ACompleteHumanNPC* Human = Cast<ACompleteHumanNPC>(GetOwner());
        if (Human && Human->CarriedItem)
        {
            bPutAfterLift = true;
            bPutAfterLiftReturn = true;
        }
    }
}

void UBodyMotorComponent::EnterStage(EHandStage NewStage)
{
    Stage = NewStage;
    StageClock = 0.0f;
}

void UBodyMotorComponent::FailHands(const TCHAR* Why)
{
    FailedItem = HandItem;
    FailedAt = Clock;
    LastHandNote = Why;
    HandItem = nullptr;
    QueuedTake = nullptr;
    bPutting = false;
    bFumbled = false;
    EnterStage(EHandStage::Idle);
}

void UBodyMotorComponent::BeginHands(ACompleteHumanNPC* Human)
{
    bHandsReady = true;
    float Age = 30.0f;
    bool bFemale = false;
    if (Human->IdentityComponent)
    {
        Age = Human->IdentityComponent->Age;
        bFemale = Human->IdentityComponent->bFemale;
    }
    const int32 Kind = FMotorSchool::KindOf(Age, bFemale);
    for (int32 H = 0; H < 2; ++H)
    {
        FMotorSchool::MakeReachBody(Age, bFemale, H == 0 ? -1.0f : 1.0f, Arms[H]);
        Grips[H] = FGripState();
        Grips[H].Strength = Arms[H].Strength;
        LastHandHeight[H] = Arms[H].HandAt().Z;
    }

    auto Mine = [this, Human](const TCHAR* Skill)
    {
        FString Name = Human && Human->IdentityComponent ? Human->IdentityComponent->GetFullName() : GetOwner()->GetName();
        Name = FPaths::MakeValidFileName(Name.Replace(TEXT(" "), TEXT("_")));
        return FMotorSchool::Folder() / TEXT("People") / (Name + TEXT("_") + Skill + TEXT(".pol"));
    };
    auto Inherit = [Kind](const TCHAR* Skill, FMotorPolicy& Into)
    {
        int32 Try = Kind;
        while (Try >= 0)
        {
            if (FMotorSchool::Childhood(Skill, Try, Into))
            {
                return true;
            }
            Try = FMotorSchool::ParentKind(Try);
        }
        return FMotorSchool::Childhood(Skill, 4, Into);
    };

    const bool bReachKnown = ReachBrain.Load(Mine(TEXT("Reach"))) || Inherit(TEXT("Reach"), ReachBrain);
    const bool bGripKnown = GripBrain.Load(Mine(TEXT("Grip"))) || Inherit(TEXT("Grip"), GripBrain);
    bHandsUsable = bReachKnown && bGripKnown
        && ReachBrain.ObservationSize() == FReachSkill::StateSize && ReachBrain.ActionSize() == FReachSkill::ActionSize
        && GripBrain.ObservationSize() == FGripSkill::StateSize && GripBrain.ActionSize() == FGripSkill::ActionSize;
    if (bHandsUsable)
    {
        ReachBrain.RefreshNoise();
        GripBrain.RefreshNoise();
    }
    if (!bHandsUsable)
    {
        UE_LOG(LogHumanCity, Warning, TEXT("%s: нет выученных рук для тела %d"), *Human->GetName(), Kind);
    }
}

void UBodyMotorComponent::FeelItem(const AActor* Item, int32 Hand, float Thickness)
{
    FGripState& Hold = Grips[Hand];
    Hold.Mass = FMath::Max(0.05f, bTwoHands ? ItemMass * 0.5f : ItemMass);
    Hold.Contact = FMath::Clamp(0.95f - Thickness / 0.14f, 0.15f, 0.92f);
    Hold.Friction = 0.6f;
    Hold.Softness = 0.0f;
    if (const AResourceActor* Thing = Cast<AResourceActor>(Item))
    {
        if (const UMatterComponent* Matter = Thing->Matter)
        {
            const float Wet = Matter->WetFraction();
            Hold.Friction = FMath::Clamp(0.62f - Wet * 0.35f, 0.15f, 0.85f);
            if (Matter->Phase() == EMatterPhase::Slurry)
            {
                Hold.Friction *= 0.6f;
                Hold.Softness = 0.85f;
            }
            else if (Matter->Props().IsSoil())
            {
                Hold.Softness = 0.5f;
            }
            if (Matter->Temperature > 55.0f)
            {
                Hold.Friction *= 0.7f;
            }
        }
    }
    float Strength = Arms[Hand].Strength;
    if (const ACompleteHumanNPC* Human = Cast<ACompleteHumanNPC>(GetOwner()))
    {
        if (const UPhysiologyComponent* Flesh = Human->PhysiologyComponent)
        {
            Strength *= FMath::Clamp(0.45f + Flesh->Body.Stamina * 0.55f, 0.3f, 1.0f);
            Strength *= 1.0f - 0.35f * FMath::Clamp(Flesh->Body.Pain, 0.0f, 1.0f);
        }
    }
    if (const UMatterSubsystem* Weather = UMatterSubsystem::Get(this))
    {
        if (Weather->Climate().Celsius < -8.0f)
        {
            Strength *= 0.8f;
        }
    }
    Hold.Strength = FMath::Max(0.1f, Strength);
}

FVector UBodyMotorComponent::HandGoalWorld(int32 Hand, ACompleteHumanNPC* Human, const FVector& Centre, const FVector& Extent) const
{
    if (!bTwoHands)
    {
        return Centre;
    }
    const FVector Right = Human->GetActorRightVector();
    const float Width = FMath::Abs(FVector::DotProduct(Extent, Right.GetAbs())) + 3.0f;
    return Centre + Right * ((Hand == 0 ? -1.0f : 1.0f) * Width);
}

FVector UBodyMotorComponent::ToBodyMetres(const ACompleteHumanNPC* Human, const FVector& World) const
{
    const FVector Here = Human->GetActorLocation();
    const FVector Delta = World - Here;
    const float FeetZ = static_cast<float>(Here.Z) - Human->GetSimpleCollisionHalfHeight();
    return FVector(FVector::DotProduct(Delta, Human->GetActorForwardVector()),
        FVector::DotProduct(Delta, Human->GetActorRightVector()),
        static_cast<float>(World.Z) - FeetZ) * 0.01f;
}

FVector UBodyMotorComponent::ModelHandInComponent(int32 Hand, const FVector& Fwd, const FVector& Side, float Scale) const
{
    const FVector Reached = Arms[Hand].HandAt();
    return (Fwd * Reached.X + Side * Reached.Y + FVector::UpVector * Reached.Z) * (100.0f / FMath::Max(0.2f, Scale));
}

void UBodyMotorComponent::AdvanceHands(float Dt, ACompleteHumanNPC* Human, const FTransform& ComponentToWorld)
{
    if (!bHandsReady)
    {
        BeginHands(Human);
    }
    StageClock += Dt;
    AActor* Item = HandItem.Get();
    bool bHolding = Human->CarriedItem != nullptr;

    if (Stage == EHandStage::Idle)
    {
        if (bPutAfterLift && bHolding)
        {
            bPutAfterLift = false;
            BeginPut(bPutAfterLiftReturn);
        }
        else if (QueuedTake.IsValid() && !bHolding)
        {
            AActor* Next = QueuedTake.Get();
            QueuedTake = nullptr;
            BeginTake(Next);
        }
        Item = HandItem.Get();
    }

    FVector Centre = GoalWorld;
    if (Stage == EHandStage::Taking && IsValid(Item))
    {
        FVector Measured;
        FVector Extent;
        float Mass = 1.0f;
        MeasureItem(Item, Measured, Extent, Mass);
        ItemExtent = Extent;
        ItemMass = Mass;
        GoalWorld = Measured + FVector(0.0f, 0.0f, bTwoHands ? 0.0f : Extent.Z * 0.35f);
        Centre = GoalWorld;
    }
    else if (Stage == EHandStage::Putting)
    {
        Centre = PlaceWorld;
    }

    if (Stage == EHandStage::Approach)
    {
        FVector To = GoalWorld - Human->GetActorLocation();
        To.Z = 0.0f;
        const float Away = To.Size();
        const float Close = (bTwoHands ? 42.0f : 50.0f) + FMath::Min(ItemExtent.X, ItemExtent.Y) * 0.5f;
        const float Turn = FMath::Abs(FMath::FindDeltaAngleDegrees(Human->GetActorRotation().Yaw, To.Rotation().Yaw));
        if (bPutting && !bHolding)
        {
            bPutting = false;
            HandItem = nullptr;
            EnterStage(EHandStage::Idle);
        }
        else if (!bPutting && !IsFreeFor(Item, Human))
        {
            FailHands(TEXT("вещь уже взяли"));
        }
        else if ((Away <= Close + 12.0f && Turn < 35.0f) || (StageClock > 8.0f && Away < Close + 60.0f))
        {
            EnterStage(bPutting ? EHandStage::Putting : EHandStage::Taking);
        }
        else if (StageClock > (bPutting ? 14.0f : 11.0f))
        {
            if (bPutting)
            {
                PlaceWorld = Human->GetActorLocation() + Human->GetActorForwardVector() * 52.0f
                    - FVector(0.0f, 0.0f, Human->GetSimpleCollisionHalfHeight() - ItemExtent.Z - 0.5f);
                GoalWorld = PlaceWorld;
                bShelfPut = false;
                bReturnPut = false;
                EnterStage(EHandStage::Putting);
            }
            else
            {
                FailHands(TEXT("не подойти к вещи"));
            }
        }
    }
    else if (Stage == EHandStage::Taking)
    {
        if (!IsValid(Item))
        {
            FailHands(TEXT("вещи нет"));
        }
        else if (!bHolding && !IsFreeFor(Item, Human))
        {
            FailHands(TEXT("вещь уже взяли"));
        }
        else if (StageClock > 15.0f)
        {
            FailHands(TEXT("не дотянулся"));
        }
    }
    else if (Stage == EHandStage::Putting)
    {
        if (!bHolding)
        {
            bPutting = false;
            HandItem = nullptr;
            EnterStage(EHandStage::Idle);
        }
        else if (StageClock > 16.0f)
        {
            Human->LetGo(false);
            LastHandNote = TEXT("бросил, где стоял");
            bPutting = false;
            HandItem = nullptr;
            EnterStage(EHandStage::Idle);
        }
    }

    Item = HandItem.Get();
    bHolding = Human->CarriedItem != nullptr;
    const AActor* Held = Human->CarriedItem.Get();
    const float Thickness = 2.0f * FMath::Min3(ItemExtent.X, ItemExtent.Y, ItemExtent.Z) * 0.01f;
    const bool bReaching = Stage == EHandStage::Taking || Stage == EHandStage::Putting;
    const bool bBook = Cast<ABookActor>(Held) != nullptr;

    for (int32 H = 0; H < 2; ++H)
    {
        const bool bUses = bTwoHands || (H == 1) == (HandSide > 0.0f);
        bHandBusy[H] = bUses && (bReaching || bHolding);
        if (!bHandBusy[H])
        {
            HandGoal[H] = FVector::ZeroVector;
            continue;
        }
        if (bReaching)
        {
            HandGoal[H] = HandGoalWorld(H, Human, Centre, ItemExtent);
        }
        else
        {
            const FVector Right = Human->GetActorRightVector();
            const float Width = bTwoHands ? FMath::Max(9.0f, static_cast<float>(FMath::Min(ItemExtent.X, ItemExtent.Y)) + 3.0f) : 0.0f;
            HandGoal[H] = Human->GetActorLocation() + Human->GetActorForwardVector() * (bBook ? 32.0f : 24.0f)
                + FVector(0.0f, 0.0f, bBook ? -6.0f : -26.0f)
                + Right * ((H == 0 ? -1.0f : 1.0f) * Width);
        }
    }

    if (!bHandsUsable)
    {
        for (int32 H = 0; H < 2; ++H)
        {
            Grip[H] = FMath::FInterpTo(Grip[H], bHandBusy[H] && bHolding ? HoldCurl : 0.18f, Dt, 5.0f);
        }
        Bend = FMath::FInterpTo(Bend, 0.0f, Dt, 3.0f);
        Squat = FMath::FInterpTo(Squat, 0.0f, Dt, 3.0f);
        return;
    }

    ReachClock -= Dt;
    if (ReachClock <= 0.0f)
    {
        ReachClock = FMath::Max(ReachClock + FReachSkill::Interval, 0.0f);
        const float Interval = FReachSkill::Interval;
        for (int32 H = 0; H < 2; ++H)
        {
            FReachBody& Arm = Arms[H];
            Arm.Load = bHandBusy[H] && bHolding ? (bTwoHands ? ItemMass * 0.5f : ItemMass) : 0.0f;
            const bool bAimed = bHandBusy[H];
            const FVector Goal = bAimed ? ToBodyMetres(Human, HandGoal[H]) : FVector(0.03f, Arm.Side * 0.25f, Arm.Leg * 0.8f);
            FReachSkill::Observe(Arm, Goal, bAimed, ReachSense);
            if (++ReachNoiseAge > 6 || ReachNoise.Num() != ReachBrain.LogStd.Num())
            {
                ReachNoiseAge = 0;
                ReachBrain.SampleNoise(Noise, ReachNoise);
            }
            float Chance = 0.0f;
            float Worth = 0.0f;
            ReachBrain.Act(ReachSense, ReachNorm, ReachAct, Chance, Worth, &ReachNoise);
            for (float& Command : ReachAct)
            {
                Command = FMath::Clamp(Command, -1.0f, 1.0f);
            }
            FReachSkill::Simulate(Arm, ReachAct, Interval);
            if (Arm.bToppled)
            {
                Arm.bToppled = false;
                Arm.Wobble = 0.0f;
                Arm.Pitch *= 0.25f;
                Arm.Squat *= 0.25f;
                Arm.PitchRate = 0.0f;
                Arm.SquatRate = 0.0f;
                StumbleTime = FMath::Max(StumbleTime, 0.35f);
            }
            const float Height = static_cast<float>(Arm.HandAt().Z);
            const float Rise = (Height - LastHandHeight[H]) / Interval;
            HandLift[H] = FMath::Clamp((Rise - HandRise[H]) / Interval, 0.0f, 8.0f);
            HandRise[H] = Rise;
            LastHandHeight[H] = Height;
        }
        Bend = FMath::Clamp((Arms[0].Pitch + Arms[1].Pitch) * 0.5f / 1.65f, 0.0f, 1.0f);
        Squat = FMath::Clamp((Arms[0].Squat + Arms[1].Squat) * 0.5f, 0.0f, 1.0f);
    }

    GripClock -= Dt;
    if (GripClock <= 0.0f)
    {
        GripClock = FMath::Max(GripClock + FGripSkill::Interval, 0.0f);
        const float Interval = FGripSkill::Interval;
        const float Scale = FMath::Max(0.3f, static_cast<float>(Human->GetActorScale3D().Z));
        const FVector Fwd = Human->GetActorForwardVector();
        const FVector Right = Human->GetActorRightVector();
        const float FeetZ = static_cast<float>(Human->GetActorLocation().Z) - Human->GetSimpleCollisionHalfHeight();
        bool bGrasped = true;
        bool bUsed = false;
        bool bAtPlace = true;
        for (int32 H = 0; H < 2; ++H)
        {
            const bool bUses = bTwoHands || (H == 1) == (HandSide > 0.0f);
            FGripState& Hold = Grips[H];
            bool bWant = false;
            if (bUses && (bReaching || bHolding))
            {
                FeelItem(bHolding ? Held : Item, H, Thickness);
                const FVector Reached = Arms[H].HandAt();
                const FVector HandNow = Human->GetActorLocation() + Fwd * (Reached.X * 100.0f) + Right * (Reached.Y * 100.0f)
                    + FVector(0.0f, 0.0f, FeetZ + Reached.Z * 100.0f - Human->GetActorLocation().Z);
                const float Gap = static_cast<float>(FVector::Dist(HandNow, HandGoal[H])) / Scale;
                const float Touch = FMath::Max(9.0f, Thickness * 50.0f + 6.0f);
                const bool bReached = Gap < Touch;
                bAtPlace &= bReached;
                bUsed = true;
                Hold.Lift = HandLift[H];
                Hold.bTouching = bHolding || (Stage == EHandStage::Taking && bReached);
                bWant = (Stage == EHandStage::Taking && bReached) || (bHolding && !(Stage == EHandStage::Putting && bReached));
            }
            else
            {
                Hold.bTouching = false;
                Hold.bHolding = false;
                Hold.Lift = 0.0f;
            }
            FGripSkill::Observe(Hold, bWant, GripSense);
            if (++GripNoiseAge > 6 || GripNoise.Num() != GripBrain.LogStd.Num())
            {
                GripNoiseAge = 0;
                GripBrain.SampleNoise(Noise, GripNoise);
            }
            float Chance = 0.0f;
            float Worth = 0.0f;
            GripBrain.Act(GripSense, GripNorm, GripAct, Chance, Worth, &GripNoise);
            for (float& Command : GripAct)
            {
                Command = FMath::Clamp(Command, -1.0f, 1.0f);
            }
            FGripSkill::Simulate(Hold, GripAct, Interval);
            Grip[H] = Hold.Curl;
            if (bUses)
            {
                bGrasped &= Hold.bHolding;
            }
            if (Hold.bDropped && bHolding && bUses)
            {
                LastHandNote = TEXT("выронил");
                Human->LetGo(false);
                bHolding = false;
                bPutting = false;
                HandItem = nullptr;
                EnterStage(EHandStage::Idle);
            }
        }

        if (Stage == EHandStage::Taking && bUsed && bGrasped && !bHolding && IsValid(Item))
        {
            if (Human->TakeIntoHands(Item))
            {
                OffsetFor = nullptr;
                HoldCurl = FMath::Max(Grips[0].Curl, Grips[1].Curl);
                LastHandNote = TEXT("взял в руки");
                HandItem = nullptr;
                EnterStage(EHandStage::Idle);
            }
            else
            {
                FailHands(TEXT("не удержал"));
            }
        }
        else if (Stage == EHandStage::Putting && bHolding && bUsed && bAtPlace && !bGrasped)
        {
            if (AActor* Leaving = Human->CarriedItem.Get())
            {
                if (!bShelfPut)
                {
                    FVector Measured;
                    FVector Extent;
                    float Mass = 1.0f;
                    MeasureItem(Leaving, Measured, Extent, Mass);
                    FCollisionQueryParams Params(SCENE_QUERY_STAT(HandsDrop), false, Human);
                    Params.AddIgnoredActor(Leaving);
                    FHitResult Hit;
                    if (Human->GetWorld() && Human->GetWorld()->LineTraceSingleByChannel(Hit, Measured, Measured - FVector(0.0f, 0.0f, 400.0f), ECC_WorldStatic, Params))
                    {
                        const float Space = static_cast<float>(Measured.Z - Extent.Z - Hit.ImpactPoint.Z);
                        if (Space > 0.3f && Space < 60.0f)
                        {
                            Leaving->AddActorWorldOffset(FVector(0.0f, 0.0f, -(Space - 0.3f)));
                        }
                    }
                }
            }
            Human->LetGo(bReturnPut);
            LastHandNote = bShelfPut ? TEXT("поставил на место") : TEXT("положил");
            bPutting = false;
            HandItem = nullptr;
            EnterStage(EHandStage::Idle);
        }
    }
}
FVector UBodyMotorComponent::PalmNormal(const FLimb& Arm, float Side) const
{
    if (!bHasFingers)
    {
        return -FVector::UpVector;
    }
    const int32 H = Side < 0.0f ? 0 : 1;
    const FVector Wrist = CS[Arm.End].GetLocation();
    const FVector Along = (CS[FingerBones[H][2][0]].GetLocation() - Wrist).GetSafeNormal();
    const FVector Across = (CS[FingerBones[H][1][0]].GetLocation() - CS[FingerBones[H][4][0]].GetLocation()).GetSafeNormal();
    const FVector Normal = Side > 0.0f ? FVector::CrossProduct(Along, Across) : FVector::CrossProduct(Across, Along);
    return Normal.GetSafeNormal();
}

void UBodyMotorComponent::OrientHand(const FLimb& Arm, float Side, const FVector& FingerDir, const FVector& PalmDir, float Weight)
{
    if (!bHasFingers || Weight <= 0.01f)
    {
        return;
    }
    const int32 H = Side < 0.0f ? 0 : 1;
    const int32 Knuckle = FingerBones[H][2][0];
    AimBone(Arm.End, Knuckle, FingerDir, Weight);

    const FVector Along = (CS[Knuckle].GetLocation() - CS[Arm.End].GetLocation()).GetSafeNormal();
    FVector Want = PalmDir - Along * FVector::DotProduct(PalmDir, Along);
    FVector Have = PalmNormal(Arm, Side);
    Have -= Along * FVector::DotProduct(Have, Along);
    if (!Want.Normalize() || !Have.Normalize())
    {
        return;
    }
    const float Angle = FMath::Acos(FMath::Clamp(FVector::DotProduct(Have, Want), -1.0f, 1.0f));
    const float Sign = FVector::DotProduct(FVector::CrossProduct(Have, Want), Along) >= 0.0f ? 1.0f : -1.0f;
    RotateBone(Arm.End, FQuat(Along, Sign * FMath::Min(Angle, FMath::DegreesToRadians(150.0f)) * FMath::Clamp(Weight, 0.0f, 1.0f)));
}

void UBodyMotorComponent::PoseFingers(int32 HandIndex, const FLimb& Arm, float Side, float Curl, float Spread)
{
    if (!bHasFingers)
    {
        return;
    }
    const FVector Normal = PalmNormal(Arm, Side);
    static const float MaxAngle[3] = { 72.0f, 95.0f, 62.0f };

    for (int32 F = 1; F < 5; ++F)
    {
        const float FingerCurl = FMath::Clamp(Curl + (F >= 3 ? 0.06f * F / 4.0f : 0.0f), -0.3f, 1.0f);
        if (Spread != 0.0f)
        {
            const float Fan = (F - 2.5f) * Spread * FMath::DegreesToRadians(6.0f);
            RotateBone(FingerBones[HandIndex][F][0], FQuat(Normal, Fan * Side));
        }
        for (int32 J = 0; J < 3; ++J)
        {
            const int32 Bone = FingerBones[HandIndex][F][J];
            FVector Dir;
            if (J < 2)
            {
                Dir = CS[FingerBones[HandIndex][F][J + 1]].GetLocation() - CS[Bone].GetLocation();
            }
            else
            {
                Dir = CS[Bone].GetLocation() - CS[FingerBones[HandIndex][F][J - 1]].GetLocation();
            }
            FVector Axis = FVector::CrossProduct(Dir.GetSafeNormal(), Normal);
            if (!Axis.Normalize())
            {
                continue;
            }
            RotateBone(Bone, FQuat(Axis, FMath::DegreesToRadians(MaxAngle[J] * FingerCurl)));
        }
    }

    const int32 T0 = FingerBones[HandIndex][0][0];
    const int32 T1 = FingerBones[HandIndex][0][1];
    const int32 T2 = FingerBones[HandIndex][0][2];
    const float Close = FMath::Clamp(Curl, 0.0f, 1.0f);
    if (Close > 0.01f)
    {
        const FVector Palm = (CS[FingerBones[HandIndex][2][0]].GetLocation() + CS[Arm.End].GetLocation()) * 0.5f;
        const FVector Meet = FMath::Lerp(Palm, CS[FingerBones[HandIndex][1][1]].GetLocation(), 0.55f) + Normal * 3.0f;
        AimBone(T0, T1, Meet - CS[T0].GetLocation(), Close * 0.55f);
    }
    else if (Curl < 0.0f)
    {
        const FVector Away = (CS[T1].GetLocation() - CS[FingerBones[HandIndex][1][0]].GetLocation()).GetSafeNormal();
        const FVector Now = (CS[T1].GetLocation() - CS[T0].GetLocation()).GetSafeNormal();
        AimBone(T0, T1, (Now + Away * 0.6f).GetSafeNormal(), -Curl);
    }
    for (int32 J = 1; J < 3; ++J)
    {
        const int32 Bone = J == 1 ? T1 : T2;
        const int32 Prev = J == 1 ? T0 : T1;
        const FVector Dir = (CS[Bone].GetLocation() - CS[Prev].GetLocation()).GetSafeNormal();
        FVector Axis = FVector::CrossProduct(Dir, Normal);
        if (Axis.Normalize())
        {
            RotateBone(Bone, FQuat(Axis, FMath::DegreesToRadians((J == 1 ? 22.0f : 30.0f) * Close)));
        }
    }
}

FTransform UBodyMotorComponent::HandWorld(const FLimb& Arm, const FTransform& ComponentToWorld) const
{
    return CS[Arm.End] * ComponentToWorld;
}

FTransform UBodyMotorComponent::HoldFrame(const FTransform& ComponentToWorld) const
{
    const FTransform Right = HandWorld(ArmR, ComponentToWorld);
    const FTransform Left = HandWorld(ArmL, ComponentToWorld);
    FTransform Frame = HandSide > 0.0f ? Right : Left;
    if (bTwoHands)
    {
        const AActor* Owner = GetOwner();
        const float Yaw = Owner ? Owner->GetActorRotation().Yaw : 0.0f;
        Frame.SetRotation(FRotator(0.0f, Yaw, 0.0f).Quaternion());
        Frame.SetLocation((Left.GetLocation() + Right.GetLocation()) * 0.5f);
    }
    Frame.SetScale3D(FVector::OneVector);
    return Frame;
}

FVector UBodyMotorComponent::IdealHold(const AActor* Held, const FTransform& Frame, const FTransform& ComponentToWorld) const
{
    FVector Centre;
    FVector Extent;
    float Mass = 1.0f;
    MeasureItem(Held, Centre, Extent, Mass);
    if (bTwoHands)
    {
        return FVector(0.0f, 0.0f, -FMath::Min(6.0f, static_cast<float>(Extent.Z) * 0.2f));
    }
    const FLimb& Arm = HandSide > 0.0f ? ArmR : ArmL;
    const int32 H = HandSide > 0.0f ? 1 : 0;
    const float Half = FMath::Min3(Extent.X, Extent.Y, Extent.Z);
    const FVector Wrist = CS[Arm.End].GetLocation();
    const FVector Knuckle = bHasFingers ? CS[FingerBones[H][2][0]].GetLocation() : Wrist;
    const FVector Normal = PalmNormal(Arm, HandSide);
    const FVector Palm = FMath::Lerp(Wrist, Knuckle, 0.7f) + Normal * (Half + 1.0f);
    return Frame.InverseTransformPosition(ComponentToWorld.TransformPosition(Palm));
}

void UBodyMotorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    ACompleteHumanNPC* Human = Cast<ACompleteHumanNPC>(GetOwner());
    if (!Human)
    {
        return;
    }
    if (!WalkerTick.IsTickFunctionRegistered())
    {
        WalkerStep(DeltaTime);
    }
    if (!Body || !Body->IsVisible() || !Body->GetSkinnedAsset())
    {
        return;
    }
    if (!bReady)
    {
        BuildCache();
        if (!bReady)
        {
            return;
        }
        BeginLearning();
        if (bWalkerReady && ThighLen + CalfLen > 10.0f)
        {
            const float Scale = FMath::Max(0.3f, static_cast<float>(Human->GetActorScale3D().Z));
            const float Leg = (ThighLen + CalfLen + AnkleHeight) * Scale * 0.01f;
            const float Ratio = Leg / FMath::Max(0.2f, Walker.Leg);
            Walker.Leg = Leg;
            Walker.FootFront *= Ratio;
            Walker.FootBack *= Ratio;
            Walker.FootSide *= Ratio;
        }
    }

    float Dt = FMath::Clamp(DeltaTime, 0.0f, 0.25f);
    if (bHasSmoothed && !Body->WasRecentlyRendered(0.3f))
    {
        HiddenAccumulator += DeltaTime;
        if (HiddenAccumulator < 0.5f)
        {
            return;
        }
        Dt = FMath::Min(HiddenAccumulator, 1.0f);
        HiddenAccumulator = 0.0f;
    }

    Clock += Dt;
    GreetTime = FMath::Max(0.0f, GreetTime - Dt);
    SpeakTime = FMath::Max(0.0f, SpeakTime - Dt);
    StumbleTime = FMath::Max(0.0f, StumbleTime - Dt);

    const FTransform ComponentToWorld = Body->GetComponentTransform();
    const float Scale = FMath::Max(0.3f, ComponentToWorld.GetScale3D().Z);
    const FVector Up = FVector::UpVector;
    FVector F = ComponentToWorld.InverseTransformVectorNoScale(Human->GetActorForwardVector());
    F.Z = 0.0f;
    F = F.GetSafeNormal();
    if (F.IsNearlyZero())
    {
        F = FVector(0.0f, 1.0f, 0.0f);
    }
    const FVector R = FVector::CrossProduct(Up, F);

    UMindComponent* Mind = Human->Mind;
    const bool bAlive = Human->IsAlive();
    const bool bAsleep = Mind && Mind->bAsleep;
    const EPosture Posture = bAlive ? Human->Posture : EPosture::Lying;
    const EActionType Action = Mind ? Mind->CurrentAction : EActionType::Idle;
    const bool bDoing = Mind && Mind->IsPerformingAction();
    const bool bTalking = Mind && Mind->IsInConversation();

    AdvanceHands(Dt, Human, ComponentToWorld);

    AActor* Held = Human->CarriedItem.Get();
    const bool bBook = Cast<ABookActor>(Held) != nullptr;
    const bool bReadingPose = bBook && Stage == EHandStage::Idle;
    const bool bCarry = Held && !bReadingPose;

    AActor* LookTarget = nullptr;
    if (Mind)
    {
        LookTarget = Mind->Dialogue.With ? Mind->Dialogue.With.Get() : Mind->AttentionTarget.Get();
    }
    if (LookTarget == Human)
    {
        LookTarget = nullptr;
    }
    FVector LookPoint = FVector::ZeroVector;
    const bool bLookAtHands = WantsFacing(LookPoint);

    const float Speed = Human->GetVelocity().Size2D() / Scale;
    const bool bStanding = Posture == EPosture::Standing;
    if (bDoing && Action == EActionType::Work)
    {
        WorkSkill = FMath::Clamp(WorkSkill + Dt * 0.004f * (1.0f - WorkSkill), 0.0f, 1.0f);
    }

    const float Move = bStanding ? FMath::Clamp(Speed / 50.0f, 0.0f, 1.0f) : 0.0f;
    const float Run = bStanding ? FMath::Clamp((Speed - 260.0f) / 110.0f, 0.0f, 1.0f) : 0.0f;
    const float LegLen = ThighLen + CalfLen;
    const float ArmLen = UpperArmLen + ForeArmLen;
    const float Stumble = StumbleTime > 0.0f ? FMath::Sin(StumbleTime / 0.5f * UE_PI) * 0.5f : 0.0f;
    const bool bStepping = bStanding && bWalkerActive;
    UHumanMovementComponent* MoveComp = Cast<UHumanMovementComponent>(Human->GetCharacterMovement());
    const ATerrainGrid* Terrain = bStepping && MoveComp ? TerrainUnder(Human, MoveComp) : nullptr;
    FVector FootTarget[2] = { FVector::ZeroVector, FVector::ZeroVector };
    FVector FootAlong[2] = { F, F };
    float FootRoll[2] = { 0.0f, 0.0f };
    if (bStepping)
    {
        for (int32 Index = 0; Index < 2; ++Index)
        {
            const FWalkFoot& Foot = Walker.Feet[Index];
            FootTarget[Index] = ComponentToWorld.InverseTransformPosition(FootWorld(Index, Terrain, FloorZ)) + Up * AnkleHeight;
            FVector Along = ComponentToWorld.InverseTransformVectorNoScale(FVector(FMath::Cos(Foot.Yaw), FMath::Sin(Foot.Yaw), 0.0f));
            Along.Z = 0.0f;
            FootAlong[Index] = Along.GetSafeNormal();
            if (FootAlong[Index].IsNearlyZero())
            {
                FootAlong[Index] = F;
            }
            if (Foot.bDown)
            {
                const FVector Hip = RefCS[Index == 0 ? LegL.Upper : LegR.Upper].GetLocation();
                const float Behind = FMath::Max(0.0f, static_cast<float>(FVector::DotProduct(Hip - FootTarget[Index], F)));
                FootRoll[Index] = FMath::Clamp(Behind / (0.3f * LegLen), 0.0f, 1.0f);
                FootTarget[Index] += Up * (FootRoll[Index] * 0.17f * LegLen);
            }
        }
    }
    const float Clumsy = 1.0f - ReachSkill;
    const float Sink = bStanding ? FootSink : 0.0f;
    const float Slippery = FMath::Clamp((0.35f - GroundGrip) / 0.3f, 0.0f, 1.0f);

    auto Tremor = [this, Clumsy](float Seed)
    {
        return FVector(FMath::Sin(Clock * 1.7f + Seed),
                       FMath::Sin(Clock * 2.3f + Seed * 1.9f),
                       FMath::Sin(Clock * 1.1f + Seed * 0.7f)) * Clumsy * 7.0f;
    };

    Local = RefLocal;
    FK(0);

    if (Posture != LastPosture)
    {
        LastPosture = Posture;
        SitSkill = FMath::Clamp(SitSkill + 0.05f * (1.0f - SitSkill), 0.0f, 1.0f);
        SupportHeight = Posture == EPosture::Sitting ? 46.0f : 0.0f;
        float Nearest = 160.0f;
        for (TActorIterator<AFurnitureActor> It(GetWorld()); It; ++It)
        {
            const float D = FVector::Dist2D(It->GetActorLocation(), Human->GetActorLocation());
            if (D >= Nearest)
            {
                continue;
            }
            switch (It->FurnitureType)
            {
            case EFurnitureType::Bed:        Nearest = D; SupportHeight = 52.0f; break;
            case EFurnitureType::Sofa:       Nearest = D; SupportHeight = 44.0f; break;
            case EFurnitureType::Bath:       Nearest = D; SupportHeight = 20.0f; break;
            case EFurnitureType::Chair:      Nearest = D; SupportHeight = 49.0f; break;
            case EFurnitureType::Toilet:     Nearest = D; SupportHeight = 46.0f; break;
            case EFurnitureType::SchoolDesk: Nearest = D; SupportHeight = 43.0f; break;
            case EFurnitureType::Bench:      Nearest = D; SupportHeight = 45.0f; break;
            default: break;
            }
        }
        if (!bAlive)
        {
            SupportHeight = 0.0f;
        }
    }

    const float BendNow = bStanding ? Bend : Bend * 0.4f;
    const float SquatNow = bStanding ? Squat : 0.0f;

    FVector PelvisPos = RefPelvisPos;
    const FVector Floor(RefPelvisPos.X, RefPelvisPos.Y, 0.0f);
    if (Posture == EPosture::Sitting)
    {
        PelvisPos = Floor - F * 8.0f + Up * (SupportHeight + 6.0f);
    }
    else if (Posture == EPosture::Lying)
    {
        PelvisPos = Floor + F * 14.0f + Up * (SupportHeight + 10.0f);
    }
    else
    {
        float LegDrop = 0.0f;
        if (bStepping)
        {
            for (int32 Index = 0; Index < 2; ++Index)
            {
                const FVector Hip = RefCS[Index == 0 ? LegL.Upper : LegR.Upper].GetLocation();
                const FVector Gap = FootTarget[Index] - Hip;
                const float Flat = FMath::Min(static_cast<float>(Gap.Size2D()), LegLen * 0.98f);
                const float Reach = FMath::Sqrt(FMath::Max(0.0f, FMath::Square(LegLen * 0.995f) - Flat * Flat));
                LegDrop = FMath::Max(LegDrop, static_cast<float>(-Gap.Z) - Reach);
            }
            LegDrop = FMath::Clamp(LegDrop, 0.0f, LegLen * 0.4f);
        }
        PelvisDrop = FMath::FInterpTo(PelvisDrop, LegDrop, Dt, 14.0f);
        PelvisPos.Z = RefPelvisPos.Z - 1.0f - PelvisDrop - Run * 5.0f - Stumble * 8.0f;
        PelvisPos.Z -= BendNow * (5.0f + SquatNow * 58.0f) + (bStepping ? 0.0f : Sink * 0.85f);
        PelvisPos -= F * (BendNow * (6.0f + 14.0f * (1.0f - SquatNow)));
        PelvisPos.Z -= FallCrouch * LegLen * 0.32f;
        PelvisPos -= F * (FallCrouch * 9.0f);
        if (bDoing && Action == EActionType::Exercise && Move < 0.1f)
        {
            PelvisPos.Z -= 10.0f * (0.5f + 0.5f * FMath::Sin(Clock * 3.5f));
        }
    }

    const int32 PelvisParent = Parents[Pelvis];
    Local[Pelvis].SetTranslation(PelvisParent >= 0 ? CS[PelvisParent].InverseTransformPosition(PelvisPos) : PelvisPos);
    FK(Pelvis);

    auto Tilt = [&Up](const FVector& Toward, float Amount)
    {
        return FQuat::FindBetweenNormals(Up, (Up + Toward * Amount).GetSafeNormal());
    };

    if (Posture == EPosture::Lying)
    {
        AimBone(Pelvis, Spine1, -F);
        AimBone(Spine1, Spine2, -F);
        AimBone(Spine2, Spine3, -F);
        AimBone(Spine3, Neck, -F + Up * 0.08f);
        AimBone(Neck, Head, -F + Up * 0.3f);
    }
    else
    {
        float Twist = 0.0f;
        if (bStepping)
        {
            FVector HipLine = FootTarget[1] - FootTarget[0];
            HipLine.Z = 0.0f;
            if (HipLine.Normalize())
            {
                const float Angle = FMath::Atan2(static_cast<float>(FVector::DotProduct(FVector::CrossProduct(R, HipLine), Up)),
                    static_cast<float>(FVector::DotProduct(R, HipLine)));
                Twist = FMath::Clamp(Angle, -0.5f, 0.5f) * 0.35f;
            }
        }
        LastTorsoTwist = TorsoTwist;
        TorsoTwist = FMath::FInterpTo(TorsoTwist, Twist, Dt, 10.0f);
        RotateBone(Pelvis, FQuat(Up, TorsoTwist));

        float Lean = 0.02f + Move * 0.04f + Run * 0.16f + Stumble * 0.35f + Sink * 0.006f;
        if (Posture == EPosture::Sitting)
        {
            Lean = bReadingPose ? 0.16f : 0.06f;
        }
        if (bDoing && (Action == EActionType::Work || Action == EActionType::Cook || Action == EActionType::Wash))
        {
            Lean += 0.14f;
        }
        if (Action == EActionType::Mourn)
        {
            Lean += 0.28f;
        }
        if (bCarry && ItemMass > 8.0f && Stage == EHandStage::Idle)
        {
            Lean -= FMath::Min(0.12f, ItemMass * 0.004f);
        }
        Lean += BendNow * (1.25f - 0.6f * SquatNow);
        Lean += FallCrouch * 0.55f;
        RotateBone(Spine1, Tilt(F, Lean * 0.5f));
        RotateBone(Spine2, Tilt(F, Lean * 0.5f));
        RotateBone(Spine3, FQuat(Up, -TorsoTwist * 1.3f) * Tilt(F, 0.012f * FMath::Sin(Clock * 1.5f)));
    }

    const FVector Toes = (F * FMath::Sqrt(FMath::Max(0.0f, 1.0f - FootPitch * FootPitch)) + Up * FootPitch).GetSafeNormal();

    auto SolveLeg = [&](const FLimb& Leg, float Side)
    {
        const FVector Hip = CS[Leg.Upper].GetLocation();
        if (Posture == EPosture::Lying)
        {
            SolveLimb(Leg, ThighLen, CalfLen, Hip + F * (LegLen - 0.5f) + R * Side * 4.0f, Up, Up);
            return;
        }
        if (Posture == EPosture::Sitting)
        {
            const FVector Knee = Hip + F * ThighLen * 0.97f + Up * 2.0f + R * Side * 3.0f;
            const float Drop = Knee.Z - AnkleHeight;
            const float Reach = FMath::Sqrt(FMath::Max(0.0f, CalfLen * CalfLen - Drop * Drop));
            FVector Foot = Knee + F * (Reach + 2.0f);
            Foot.Z = AnkleHeight;
            SolveLimb(Leg, ThighLen, CalfLen, Foot, F + Up * 0.3f, Toes);
            return;
        }

        if (bStepping)
        {
            const int32 Index = Side < 0.0f ? 0 : 1;
            const FWalkFoot& Step = Walker.Feet[Index];
            const float Raised = FMath::Clamp(Step.Z * 100.0f / 15.0f, 0.0f, 1.0f);
            const FVector Toe = (FootAlong[Index] * FMath::Sqrt(FMath::Max(0.0f, 1.0f - FootPitch * FootPitch)) + Up * FootPitch).GetSafeNormal();
            const FVector FootDir = Step.bDown
                ? (Toe - Up * (0.5f * FootRoll[Index])).GetSafeNormal()
                : (Toe + Up * (0.3f * Raised - 0.1f)).GetSafeNormal();
            SolveLimb(Leg, ThighLen, CalfLen, FootTarget[Index], (F + R * Side * 0.12f).GetSafeNormal(), FootDir);
            return;
        }
        const FVector Ground(Hip.X, Hip.Y, 0.0f);
        const float Lateral = 2.5f + SquatNow * 9.0f;
        const float Back = SquatNow * 4.0f;
        const FVector Foot = Ground + F * (3.0f - Back) + R * Side * Lateral + Up * (AnkleHeight - Sink);
        SolveLimb(Leg, ThighLen, CalfLen, Foot, F + Up * 0.05f, Toes);
    };
    SolveLeg(LegL, -1.0f);
    SolveLeg(LegR, 1.0f);

    if (bStepping)
    {
        const FVector2D Accel = (Walker.Vel - LastWalkVel) / FMath::Max(Dt, 0.005f);
        LastWalkVel = Walker.Vel;
        const float Forward = FVector2D::DotProduct(Accel, Walker.Forward());
        const float Pendulum = FMath::Max(0.2f, ArmLen * Scale * 0.01f * 0.6f);
        const int32 Slices = FMath::Clamp(FMath::CeilToInt(Dt / 0.02f), 1, 20);
        const float Slice = Dt / Slices;
        for (int32 Index = 0; Index < 2; ++Index)
        {
            const FVector Hip = RefCS[Index == 0 ? LegL.Upper : LegR.Upper].GetLocation();
            const float Ahead = static_cast<float>(FVector::DotProduct(FootTarget[Index] - Hip, F)) / FMath::Max(10.0f, LegLen);
            const float Target = FMath::Clamp(-Ahead * 0.9f, -0.6f, 0.6f) * (1.0f - Slippery * 0.6f);
            for (int32 S = 0; S < Slices; ++S)
            {
                const float Push = 36.0f * (Target - ArmAngle[Index]) - 5.0f * ArmRate[Index]
                    - Forward / Pendulum * FMath::Cos(ArmAngle[Index]) - 9.81f / Pendulum * FMath::Sin(ArmAngle[Index]) * 0.3f;
                ArmRate[Index] += Push * Slice;
                ArmAngle[Index] = FMath::Clamp(ArmAngle[Index] + ArmRate[Index] * Slice, -0.9f, 0.9f);
            }
        }
    }
    else
    {
        LastWalkVel = FVector2D::ZeroVector;
        for (int32 Index = 0; Index < 2; ++Index)
        {
            ArmRate[Index] = 0.0f;
            ArmAngle[Index] = FMath::FInterpTo(ArmAngle[Index], 0.0f, Dt, 4.0f);
        }
    }

    const FVector Chest = CS[Spine3].GetLocation();
    const FVector HeadPos = CS[Head].GetLocation();
    const FVector ChestFloor(Chest.X, Chest.Y, 0.0f);
    const float WaveLevel = GestureLevel(TEXT("Wave"));
    const float ExplainLevel = GestureLevel(TEXT("Explain"));
    const float GoalHeight = ComponentToWorld.InverseTransformPosition(GoalWorld).Z;

    auto SolveArm = [&](const FLimb& Arm, float Side)
    {
        const FVector Shoulder = CS[Arm.Upper].GetLocation();
        FVector Hand = Shoulder - Up * ArmLen * 0.95f + R * Side * 7.0f + F * 3.0f;
        FVector Pole = -F + R * Side * 0.25f;

        const float EatCycle = FMath::Fmod(Clock, 3.2f) / 3.2f;
        const float WorkHeight = Posture == EPosture::Sitting ? 76.0f : 92.0f;
        const bool bHoldingHand = Held && (bTwoHands || (Side > 0.0f) == (HandSide > 0.0f));

        if (Posture == EPosture::Lying)
        {
            Hand = bAlive
                ? Shoulder + F * ArmLen * 0.9f + R * Side * 12.0f - Up * 4.0f
                : Shoulder + R * Side * ArmLen * 0.85f + F * 12.0f;
            Pole = -Up;
        }
        else if (Stumble > 0.05f && Move < 0.05f)
        {
            Hand = Shoulder + F * ArmLen * 0.7f + R * Side * 20.0f + Up * 5.0f;
            Pole = -Up + R * Side;
        }
        else if (bReadingPose)
        {
            Hand = Chest + F * 30.0f - Up * 14.0f + R * Side * 11.0f;
            Pole = -Up + R * Side * 0.6f;
        }
        else if (bCarry && bTwoHands)
        {
            const float Width = FMath::Max(10.0f, static_cast<float>(FMath::Min(ItemExtent.X, ItemExtent.Y)) + 3.0f);
            Hand = Chest + F * (26.0f + ItemExtent.GetMax() * 0.2f) - Up * (26.0f + FMath::Min(10.0f, ItemMass * 0.3f)) + R * Side * Width;
            Pole = -Up + R * Side * 0.7f;
        }
        else if (bCarry && bHoldingHand && !(bDoing && Action == EActionType::Eat))
        {
            Hand = Chest + F * 22.0f - Up * 30.0f + R * Side * 16.0f;
            Pole = -Up + R * Side * 0.5f;
        }
        else if (bDoing && Action == EActionType::Eat)
        {
            Hand = (Side > 0.0f && EatCycle < 0.3f)
                ? HeadPos + F * 11.0f - Up * 7.0f + R * 2.0f
                : ChestFloor + F * 36.0f + R * Side * 11.0f + Up * WorkHeight;
            Pole = -Up + R * Side * 0.4f;
        }
        else if (bDoing && Posture == EPosture::Sitting
                 && (Action == EActionType::Practice || Action == EActionType::Study || Action == EActionType::Work
                     || Action == EActionType::Entertain))
        {
            Hand = ChestFloor + F * 40.0f + R * Side * 16.0f
                 + Up * (74.0f + 2.5f * FMath::Sin(Clock * 9.0f + Side));
            Pole = -Up - F * 0.3f + R * Side * 0.3f;
        }
        else if (Posture == EPosture::Sitting)
        {
            const FVector Knee = CS[Side < 0.0f ? LegL.Lower : LegR.Lower].GetLocation();
            Hand = Knee - F * 12.0f + Up * 9.0f;
            Pole = -F - Up * 0.4f + R * Side * 0.3f;
        }
        else if (Move > 0.05f || FMath::Abs(ArmAngle[Side < 0.0f ? 0 : 1]) > 0.03f)
        {
            const float Angle = ArmAngle[Side < 0.0f ? 0 : 1];
            const FVector Hang = Shoulder + (-Up * FMath::Cos(Angle) + F * FMath::Sin(Angle)) * ArmLen * 0.94f
                + R * Side * (6.0f + Slippery * 22.0f) + Up * (Slippery * 16.0f);
            const FVector Bent = Shoulder - Up * UpperArmLen * 0.85f
                + F * (ForeArmLen * 0.75f + Angle * 50.0f) + R * Side * 4.0f + Up * 6.0f;
            Hand = FMath::Lerp(Hang, Bent, Run);
        }
        else if (bDoing && Action == EActionType::Work)
        {
            const float Beat = Clock * (3.0f + 4.0f * WorkSkill) + (1.0f - WorkSkill) * 2.0f * FMath::Sin(Clock * 1.3f);
            Hand = Side > 0.0f
                ? Chest + F * 36.0f + R * 10.0f + Up * (-14.0f + 16.0f * FMath::Max(0.0f, FMath::Sin(Beat)))
                : Chest + F * 34.0f - R * 8.0f - Up * 20.0f;
            Pole = -Up + R * Side * 0.5f;
        }
        else if (bDoing && (Action == EActionType::Cook || Action == EActionType::Wash || Action == EActionType::Drink))
        {
            Hand = ChestFloor + F * 40.0f + R * Side * 13.0f + Up * (WorkHeight + 3.0f * FMath::Sin(Clock * 3.0f + Side * 1.5f));
            Pole = -Up + R * Side * 0.5f;
        }
        else if (bDoing && Action == EActionType::Exercise)
        {
            const float T = 0.5f + 0.5f * FMath::Sin(Clock * 3.5f);
            const FVector Dir = (-Up * (1.0f - T) + (Up + R * Side * 0.7f) * T).GetSafeNormal();
            Hand = Shoulder + Dir * ArmLen * 0.95f;
            Pole = -F;
        }
        else if (Action == EActionType::Mourn)
        {
            Hand = HeadPos + F * 14.0f + R * Side * 6.0f - Up * 4.0f;
            Pole = -Up + R * Side;
        }
        else if (bTalking && GreetTime > 0.0f && Side > 0.0f && WaveLevel > 0.0f)
        {
            Hand = Shoulder + Up * ArmLen * (0.2f + 0.5f * WaveLevel)
                 + R * (10.0f + 9.0f * WaveLevel * FMath::Sin(Clock * (4.0f + 8.0f * WaveLevel))) + F * 6.0f;
            Pole = R - Up * 0.5f;
        }
        else if (bTalking && SpeakTime > 0.0f && ExplainLevel > 0.0f)
        {
            const float Amp = ExplainLevel;
            Hand = Side > 0.0f
                ? Chest + F * (22.0f + 8.0f * Amp + 4.0f * Amp * FMath::Sin(Clock * 4.0f)) + R * 14.0f - Up * (14.0f - 6.0f * Amp * FMath::Sin(Clock * 5.0f))
                : Chest + F * (14.0f + 8.0f * Amp) - R * 12.0f - Up * (22.0f - 5.0f * Amp * FMath::Sin(Clock * 4.3f + 1.0f));
            Pole = -Up - F * 0.3f + R * Side * 0.4f;
        }
        else if (bDoing && (Action == EActionType::Observe || Action == EActionType::Reflect || Action == EActionType::Rest))
        {
            Hand = PelvisPos - F * 16.0f + R * Side * 9.0f + Up * 4.0f;
            Pole = R * Side + F * 0.1f;
        }
        else if (BendNow > 0.2f)
        {
            Hand = Shoulder - Up * ArmLen * 0.7f + F * 18.0f + R * Side * 8.0f;
            Pole = -F + R * Side * 0.5f;
        }

        if (Stumble > 0.05f && Move >= 0.05f)
        {
            const FVector Brace = Shoulder + F * (ArmLen * 0.6f) + R * Side * 18.0f + Up * 6.0f;
            Hand = FMath::Lerp(Hand, Brace, FMath::Min(1.0f, Stumble * 1.2f));
        }
        const int32 CatchHand = Side < 0.0f ? 0 : 1;
        const float Catch = bToppling ? FMath::Clamp((ToppleTime - 0.12f) / 0.15f, 0.0f, 1.0f)
                          : (bRising ? 1.0f - FMath::Clamp((RiseTime - 0.35f) / 0.4f, 0.0f, 1.0f) : 0.0f);
        if (Catch > 0.0f && !CatchWorld[CatchHand].IsZero())
        {
            const FVector Palm = ComponentToWorld.InverseTransformPosition(CatchWorld[CatchHand]);
            Hand = FMath::Lerp(Hand, Palm, Catch);
            Pole = FMath::Lerp(Pole, (-Up + R * Side * 0.6f).GetSafeNormal(), Catch);
        }
        if (Posture != EPosture::Lying)
        {
            Hand += Tremor(Side * 3.0f) * (Stage == EHandStage::Idle ? 1.0f : 0.25f);
        }

        const int32 Index = Side < 0.0f ? 0 : 1;
        float Weight = 0.0f;
        if (bHandsUsable && bHandBusy[Index] && Posture != EPosture::Lying)
        {
            Weight = 1.0f;
            Hand = ModelHandInComponent(Index, F, R, Scale);
            Pole = FMath::Lerp(Pole, (-F * 0.6f + R * Side * 0.8f - Up * 0.2f).GetSafeNormal(), 0.8f);
        }

        SolveLimb(Arm, UpperArmLen, ForeArmLen, Hand, Pole, FVector::ZeroVector);
        (Side < 0.0f ? LastHandL : LastHandR) = CS[Arm.End].GetLocation();

        const bool bProgramHand = Weight > 0.05f && (bTwoHands || (Side > 0.0f) == (HandSide > 0.0f));
        if (bProgramHand)
        {
            FVector FingerDir;
            FVector PalmDir;
            if (bTwoHands)
            {
                FingerDir = (F * 0.75f - Up * 0.35f).GetSafeNormal();
                PalmDir = -R * Side;
            }
            else if (GoalHeight < 70.0f || bPutting)
            {
                FingerDir = (F * 0.55f - Up * 0.85f).GetSafeNormal();
                PalmDir = (-Up * 0.8f - R * Side * 0.3f).GetSafeNormal();
            }
            else
            {
                FingerDir = (F - Up * 0.1f).GetSafeNormal();
                PalmDir = (-R * Side * 0.9f - Up * 0.2f).GetSafeNormal();
            }
            OrientHand(Arm, Side, FingerDir, PalmDir, Weight);
        }
        else if (bCarry && bHoldingHand)
        {
            OrientHand(Arm, Side, (F * 0.8f - R * Side * 0.3f).GetSafeNormal(), bTwoHands ? (-R * Side) : (Up * 0.6f - R * Side * 0.6f).GetSafeNormal(), 0.7f);
        }
        else if (bReadingPose)
        {
            OrientHand(Arm, Side, (F * 0.7f + Up * 0.4f - R * Side * 0.2f).GetSafeNormal(), (Up * 0.5f - R * Side * 0.7f).GetSafeNormal(), 0.6f);
        }
        PoseFingers(Side < 0.0f ? 0 : 1, Arm, Side, Grip[Side < 0.0f ? 0 : 1],
            Stage == EHandStage::Taking && !Grips[Side < 0.0f ? 0 : 1].bTouching ? 0.8f : 0.0f);
    };
    SolveArm(ArmL, -1.0f);
    SolveArm(ArmR, 1.0f);

    if (bAlive && !bAsleep && Posture != EPosture::Lying)
    {
        FQuat Look = FQuat::Identity;
        FVector LookAt = FVector::ZeroVector;
        bool bHasLookAt = false;
        if (bLookAtHands)
        {
            LookAt = LookPoint;
            bHasLookAt = true;
        }
        else if (LookTarget)
        {
            LookAt = LookTarget->GetActorLocation() + FVector(0.0f, 0.0f, 60.0f);
            bHasLookAt = true;
        }
        if (bReadingPose && !bLookAtHands)
        {
            Look = FQuat::FindBetweenNormals(F, (F - Up * 0.6f).GetSafeNormal());
        }
        else if (Action == EActionType::Mourn && !bLookAtHands)
        {
            Look = FQuat::FindBetweenNormals(F, (F - Up * 0.8f).GetSafeNormal());
        }
        else if (bHasLookAt)
        {
            const FVector Eye = CS[Head].GetLocation();
            const FVector At = ComponentToWorld.InverseTransformPosition(LookAt);
            const FVector Wanted = (At - Eye).GetSafeNormal();
            if (!Wanted.IsNearlyZero())
            {
                FVector Body0 = F;
                if (BendNow > 0.05f)
                {
                    Body0 = (F - Up * BendNow * 0.9f).GetSafeNormal();
                }
                Look = FQuat::FindBetweenNormals(Body0, Wanted);
                const float Angle = Look.GetAngle();
                const float Limit = FMath::DegreesToRadians(70.0f);
                if (Angle > Limit)
                {
                    Look = FQuat::Slerp(FQuat::Identity, Look, Limit / Angle);
                }
            }
        }
        else if (Sink > 4.0f && Move > 0.1f)
        {
            Look = FQuat::FindBetweenNormals(F, (F - Up * 0.45f).GetSafeNormal());
        }
        if (bTalking && SpeakTime <= 0.0f)
        {
            Look = FQuat(R, 0.06f * FMath::Sin(Clock * 2.2f)) * Look;
        }
        RotateBone(Neck, FQuat::Slerp(FQuat::Identity, Look, 0.4f));
        RotateBone(Head, FQuat::Slerp(FQuat::Identity, Look, 0.6f));
    }

    const float Rate = bRising ? 2.6f : (bStepping ? 45.0f : (Stage != EHandStage::Idle ? 18.0f : (Move > 0.1f ? 10.0f + Move * 25.0f : 3.0f + 9.0f * SitSkill)));
    if (!bHasSmoothed || Smoothed.Num() != Local.Num())
    {
        Smoothed = Local;
        bHasSmoothed = true;
    }
    else
    {
        const float Alpha = 1.0f - FMath::Exp(-Rate * Dt);
        for (int32 i = 0; i < Local.Num(); ++i)
        {
            Smoothed[i].SetRotation(FQuat::Slerp(Smoothed[i].GetRotation(), Local[i].GetRotation(), Alpha).GetNormalized());
            Smoothed[i].SetTranslation(FMath::Lerp(Smoothed[i].GetTranslation(), Local[i].GetTranslation(), Alpha));
            Smoothed[i].SetScale3D(Local[i].GetScale3D());
        }
    }

    Body->BoneSpaceTransforms = Smoothed;
    Body->MarkRefreshTransformDirty();

    if (Held)
    {
        Local = Smoothed;
        FK(0);
        const FTransform Frame = HoldFrame(ComponentToWorld);
        if (OffsetFor.Get() != Held)
        {
            HeldOffset = Held->GetActorTransform().GetRelativeTransform(Frame);
            HeldOffset.SetScale3D(FVector::OneVector);
            HeldFrom = HeldOffset.GetLocation();
            SettleClock = 0.0f;
            OffsetFor = Held;
        }
        if (SettleClock < 0.5f)
        {
            SettleClock += Dt;
            HeldOffset.SetLocation(FMath::Lerp(HeldFrom, IdealHold(Held, Frame, ComponentToWorld), MinJerk(SettleClock / 0.5f)));
        }
        else if (!bReadingPose)
        {
            HeldOffset.SetLocation(IdealHold(Held, Frame, ComponentToWorld));
        }
        FVector Where = (HeldOffset * Frame).GetLocation();
        FQuat Turn = (HeldOffset * Frame).GetRotation();
        if (bReadingPose)
        {
            const FVector HandL = HandWorld(ArmL, ComponentToWorld).GetLocation();
            const FVector HandR = HandWorld(ArmR, ComponentToWorld).GetLocation();
            const FVector Mid = (HandL + HandR) * 0.5f + Human->GetActorForwardVector() * 5.0f + FVector(0.0f, 0.0f, 3.0f);
            FRotator Open = Human->GetActorRotation();
            Open.Pitch = 55.0f;
            const float Blend = 1.0f - FMath::Exp(-4.0f * Dt);
            Where = FMath::Lerp(Held->GetActorLocation(), Mid, Blend);
            Turn = FQuat::Slerp(Held->GetActorQuat(), Open.Quaternion(), Blend);
            HeldOffset = FTransform(Turn, Where).GetRelativeTransform(Frame);
        }
        Held->SetActorLocationAndRotation(Where, Turn, false, nullptr, ETeleportType::TeleportPhysics);
    }
}
