#include "HumanLearning.h"
#include "CompleteHumanAI.h"
#include "MindComponent.h"
#include "NeedComponent.h"
#include "PhysiologyComponent.h"
#include "EmotionComponent.h"
#include "HumanWorldSubsystem.h"
#include "FurnitureActor.h"
#include "BookActor.h"
#include "LearningAgentsObservations.h"
#include "LearningAgentsActions.h"
#include "LearningAgentsCompletions.h"
#include "LearningAgentsCommunicator.h"
#include "LearningAgentsNeuralNetwork.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/FileManager.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

namespace HumanSense
{
    constexpr int32 KindCount = 11;
    constexpr int32 SectorCount = 15;
    constexpr int32 BodyCount = 12;
    constexpr int32 DoCount = KindCount + 3;
    constexpr int32 DoTake = KindCount;
    constexpr int32 DoPut = KindCount + 1;
    constexpr int32 DoStop = KindCount + 2;
    constexpr float Range = 2500.0f;
    constexpr float Reach = 230.0f;

    int32 KindOf(const FAffordance& A)
    {
        switch (A.Action)
        {
        case EActionType::Eat:
        case EActionType::Cook:      return 2;
        case EActionType::Drink:     return 3;
        case EActionType::Sleep:     return 4;
        case EActionType::UseToilet: return 5;
        case EActionType::Wash:      return 6;
        case EActionType::Rest:      return 7;
        case EActionType::Read:
        case EActionType::Study:
        case EActionType::Practice:  return 8;
        case EActionType::Work:      return 9;
        default:                     return 10;
        }
    }

    struct FSenseFrame
    {
        TArray<float> Body;
        TArray<float> Motion;
        TArray<float> Time;
        TArray<float> ReachMask;
        float Wall[SectorCount];
        float Thing[SectorCount];
        int32 Kind[SectorCount];
        int32 Posture = 0;
        bool bHolding = false;
        bool bBusy = false;

        FSenseFrame()
        {
            Body.Init(0.5f, BodyCount);
            Motion.Init(0.0f, 3);
            Time.Init(0.0f, 2);
            ReachMask.Init(0.0f, KindCount);
            for (int32 i = 0; i < SectorCount; ++i)
            {
                Wall[i] = 1.0f;
                Thing[i] = 1.0f;
                Kind[i] = 0;
            }
        }
    };

    ACompleteHumanNPC* AgentOf(UHumanAgentsManager* Manager, int32 AgentId)
    {
        return Manager ? Cast<ACompleteHumanNPC>(Manager->GetAgent(AgentId)) : nullptr;
    }

    void CollectOffers(ACompleteHumanNPC* Human, TArray<FAffordance>& Out)
    {
        UWorld* World = Human ? Human->GetWorld() : nullptr;
        UHumanWorldSubsystem* WorldMind = World ? World->GetSubsystem<UHumanWorldSubsystem>() : nullptr;
        if (!WorldMind)
        {
            return;
        }
        WorldMind->GatherAffordancesNear(Human->GetActorLocation(), Range, Out);

        const bool bHasHome = Human->HasHome();
        const FVector Home = Human->GetHomeLocation();
        Out.RemoveAll([WorldMind, bHasHome, Home](const FAffordance& A)
        {
            if (!A.bHasLocation)
            {
                return true;
            }
            if (A.bPrivate && (!bHasHome || FVector::Dist2D(A.OwnerAnchor, Home) > 400.0f))
            {
                return true;
            }
            return !WorldMind->IsAffordanceOpenNow(A);
        });
    }

    void Sense(ACompleteHumanNPC* Human, FSenseFrame& F)
    {
        UWorld* World = Human->GetWorld();
        UHumanWorldSubsystem* WorldMind = World ? World->GetSubsystem<UHumanWorldSubsystem>() : nullptr;
        if (!World || !WorldMind)
        {
            return;
        }

        static const ENeedType Needs[] = {
            ENeedType::Hunger, ENeedType::Thirst, ENeedType::Sleep, ENeedType::Bladder, ENeedType::Hygiene,
            ENeedType::Comfort, ENeedType::Health, ENeedType::SocialContact, ENeedType::Money };
        for (int32 i = 0; i < UE_ARRAY_COUNT(Needs); ++i)
        {
            F.Body[i] = Human->NeedComponent ? Human->NeedComponent->GetSatisfaction(Needs[i]) : 0.5f;
        }
        F.Body[9] = Human->PhysiologyComponent ? Human->PhysiologyComponent->Body.Stamina : 1.0f;
        F.Body[10] = Human->EmotionComponent ? Human->EmotionComponent->GetAffect().Pleasure : 0.0f;
        F.Body[11] = Human->EmotionComponent ? Human->EmotionComponent->GetStress() : 0.0f;

        F.Posture = FMath::Clamp(static_cast<int32>(Human->Posture), 0, 2);
        const FVector Velocity = Human->GetVelocity();
        F.Motion[0] = FVector::DotProduct(Velocity, Human->GetActorForwardVector()) / 300.0f;
        F.Motion[1] = FVector::DotProduct(Velocity, Human->GetActorRightVector()) / 300.0f;
        F.Motion[2] = Human->Motor ? Human->Motor->GetWalkSkill() : 1.0f;

        const float Hour = WorldMind->Now.HourFloat;
        F.Time[0] = FMath::Sin(Hour / 24.0f * UE_TWO_PI);
        F.Time[1] = FMath::Cos(Hour / 24.0f * UE_TWO_PI);
        F.bHolding = Human->CarriedItem != nullptr;
        F.bBusy = Human->Mind && Human->Mind->IsBusyForPolicy();

        const FVector Here = Human->GetActorLocation();
        const FVector Eye = Here + FVector(0.0f, 0.0f, 60.0f);
        const float Yaw = Human->GetActorRotation().Yaw;
        const float Step = 200.0f / static_cast<float>(SectorCount - 1);
        FCollisionQueryParams Params(SCENE_QUERY_STAT(HumanRetina), false, Human);

        for (int32 i = 0; i < SectorCount; ++i)
        {
            const FVector Dir = FRotator(0.0f, Yaw - 100.0f + Step * i, 0.0f).Vector();
            FHitResult Hit;
            F.Wall[i] = World->LineTraceSingleByChannel(Hit, Eye, Eye + Dir * Range, ECC_WorldStatic, Params)
                ? Hit.Distance / Range : 1.0f;
        }

        auto Consider = [&F, Here, Yaw, Step](const FVector& Where, int32 Kind)
        {
            FVector To = Where - Here;
            To.Z = 0.0f;
            const float Dist = To.Size();
            if (Dist > Range || Dist < 1.0f)
            {
                return;
            }
            const float Relative = FMath::FindDeltaAngleDegrees(Yaw, To.Rotation().Yaw);
            if (FMath::Abs(Relative) > 100.0f + Step * 0.5f)
            {
                return;
            }
            const int32 Sector = FMath::Clamp(FMath::RoundToInt((Relative + 100.0f) / Step), 0, SectorCount - 1);
            const float Norm = Dist / Range;
            if (Norm > F.Wall[Sector] + 0.03f || Norm >= F.Thing[Sector])
            {
                return;
            }
            F.Thing[Sector] = Norm;
            F.Kind[Sector] = Kind;
        };

        TArray<FAffordance> Offers;
        CollectOffers(Human, Offers);
        for (const FAffordance& A : Offers)
        {
            const int32 Kind = KindOf(A);
            Consider(A.Location, Kind);
            if (FVector::Dist(A.Location, Here) <= Reach && Human->CanReachPoint(A.Location))
            {
                F.ReachMask[Kind] = 1.0f;
            }
        }

        for (ACompleteHumanNPC* Other : WorldMind->GetHumansNear(Here, Range, Human))
        {
            if (!Other || !Other->IsAlive())
            {
                continue;
            }
            Consider(Other->GetActorLocation(), 1);
            if (FVector::Dist(Other->GetActorLocation(), Here) <= 320.0f && Human->HasLineOfSight(Other))
            {
                F.ReachMask[1] = 1.0f;
            }
        }
    }
}

UHumanAgentsManager::UHumanAgentsManager()
{
    MaxAgentNum = 64;
}

void UHumanInteractor::SpecifyAgentObservation_Implementation(FLearningAgentsObservationSchemaElement& OutObservationSchemaElement, ULearningAgentsObservationSchema* InObservationSchema)
{
    using namespace HumanSense;

    TMap<FName, FLearningAgentsObservationSchemaElement> Sector;
    Sector.Add(TEXT("Wall"), ULearningAgentsObservations::SpecifyFloatObservation(InObservationSchema, 1.0f, TEXT("Wall")));
    Sector.Add(TEXT("Thing"), ULearningAgentsObservations::SpecifyFloatObservation(InObservationSchema, 1.0f, TEXT("Thing")));
    Sector.Add(TEXT("Kind"), ULearningAgentsObservations::SpecifyExclusiveDiscreteObservation(InObservationSchema, KindCount, TEXT("Kind")));
    const FLearningAgentsObservationSchemaElement SectorElement =
        ULearningAgentsObservations::SpecifyStructObservation(InObservationSchema, Sector, TEXT("Sector"));

    TMap<FName, FLearningAgentsObservationSchemaElement> Root;
    Root.Add(TEXT("Body"), ULearningAgentsObservations::SpecifyContinuousObservation(InObservationSchema, BodyCount, 1.0f, TEXT("Body")));
    Root.Add(TEXT("Posture"), ULearningAgentsObservations::SpecifyExclusiveDiscreteObservation(InObservationSchema, 3, TEXT("Posture")));
    Root.Add(TEXT("Motion"), ULearningAgentsObservations::SpecifyContinuousObservation(InObservationSchema, 3, 1.0f, TEXT("Motion")));
    Root.Add(TEXT("Time"), ULearningAgentsObservations::SpecifyContinuousObservation(InObservationSchema, 2, 1.0f, TEXT("Time")));
    Root.Add(TEXT("Holding"), ULearningAgentsObservations::SpecifyBoolObservation(InObservationSchema, TEXT("Holding")));
    Root.Add(TEXT("Busy"), ULearningAgentsObservations::SpecifyBoolObservation(InObservationSchema, TEXT("Busy")));
    Root.Add(TEXT("Reach"), ULearningAgentsObservations::SpecifyContinuousObservation(InObservationSchema, KindCount, 1.0f, TEXT("Reach")));
    Root.Add(TEXT("Vision"), ULearningAgentsObservations::SpecifyStaticArrayObservation(InObservationSchema, SectorElement, SectorCount, TEXT("Vision")));

    OutObservationSchemaElement = ULearningAgentsObservations::SpecifyStructObservation(InObservationSchema, Root, TEXT("Senses"));
}

void UHumanInteractor::GatherAgentObservation_Implementation(FLearningAgentsObservationObjectElement& OutObservationObjectElement, ULearningAgentsObservationObject* InObservationObject, const int32 AgentId)
{
    using namespace HumanSense;

    FSenseFrame Frame;
    if (ACompleteHumanNPC* Human = AgentOf(HumanManager, AgentId))
    {
        Sense(Human, Frame);
    }

    TArray<FLearningAgentsObservationObjectElement> Sectors;
    Sectors.Reserve(SectorCount);
    for (int32 i = 0; i < SectorCount; ++i)
    {
        TMap<FName, FLearningAgentsObservationObjectElement> Sector;
        Sector.Add(TEXT("Wall"), ULearningAgentsObservations::MakeFloatObservation(InObservationObject, Frame.Wall[i], TEXT("Wall")));
        Sector.Add(TEXT("Thing"), ULearningAgentsObservations::MakeFloatObservation(InObservationObject, Frame.Thing[i], TEXT("Thing")));
        Sector.Add(TEXT("Kind"), ULearningAgentsObservations::MakeExclusiveDiscreteObservation(InObservationObject, Frame.Kind[i], KindCount, TEXT("Kind")));
        Sectors.Add(ULearningAgentsObservations::MakeStructObservation(InObservationObject, Sector, TEXT("Sector")));
    }

    TMap<FName, FLearningAgentsObservationObjectElement> Root;
    Root.Add(TEXT("Body"), ULearningAgentsObservations::MakeContinuousObservation(InObservationObject, Frame.Body, TEXT("Body")));
    Root.Add(TEXT("Posture"), ULearningAgentsObservations::MakeExclusiveDiscreteObservation(InObservationObject, Frame.Posture, 3, TEXT("Posture")));
    Root.Add(TEXT("Motion"), ULearningAgentsObservations::MakeContinuousObservation(InObservationObject, Frame.Motion, TEXT("Motion")));
    Root.Add(TEXT("Time"), ULearningAgentsObservations::MakeContinuousObservation(InObservationObject, Frame.Time, TEXT("Time")));
    Root.Add(TEXT("Holding"), ULearningAgentsObservations::MakeBoolObservation(InObservationObject, Frame.bHolding, TEXT("Holding")));
    Root.Add(TEXT("Busy"), ULearningAgentsObservations::MakeBoolObservation(InObservationObject, Frame.bBusy, TEXT("Busy")));
    Root.Add(TEXT("Reach"), ULearningAgentsObservations::MakeContinuousObservation(InObservationObject, Frame.ReachMask, TEXT("Reach")));
    Root.Add(TEXT("Vision"), ULearningAgentsObservations::MakeStaticArrayObservation(InObservationObject, Sectors, TEXT("Vision")));

    OutObservationObjectElement = ULearningAgentsObservations::MakeStructObservation(InObservationObject, Root, TEXT("Senses"));
}

void UHumanInteractor::SpecifyAgentAction_Implementation(FLearningAgentsActionSchemaElement& OutActionSchemaElement, ULearningAgentsActionSchema* InActionSchema)
{
    using namespace HumanSense;

    TArray<float> Priors;
    Priors.Init(0.4f / static_cast<float>(DoCount - 2), DoCount);
    Priors[0] = 0.5f;
    Priors[DoStop] = 0.1f;

    TMap<FName, FLearningAgentsActionSchemaElement> Root;
    Root.Add(TEXT("Move"), ULearningAgentsActions::SpecifyContinuousAction(InActionSchema, 2, 1.0f, TEXT("Move")));
    Root.Add(TEXT("Do"), ULearningAgentsActions::SpecifyExclusiveDiscreteAction(InActionSchema, DoCount, Priors, TEXT("Do")));

    OutActionSchemaElement = ULearningAgentsActions::SpecifyStructAction(InActionSchema, Root, TEXT("Motor"));
}

void UHumanInteractor::PerformAgentAction_Implementation(const ULearningAgentsActionObject* InActionObject, const FLearningAgentsActionObjectElement& InActionObjectElement, const int32 AgentId)
{
    using namespace HumanSense;

    ACompleteHumanNPC* Human = AgentOf(HumanManager, AgentId);
    if (!Human || !Human->Mind)
    {
        return;
    }
    UMindComponent* Mind = Human->Mind;

    FLearningAgentsActionObjectElement MoveElement;
    FLearningAgentsActionObjectElement DoElement;
    if (!ULearningAgentsActions::GetStructActionElement(MoveElement, InActionObject, InActionObjectElement, TEXT("Move"), TEXT("Motor"))
        || !ULearningAgentsActions::GetStructActionElement(DoElement, InActionObject, InActionObjectElement, TEXT("Do"), TEXT("Motor")))
    {
        return;
    }

    TArray<float> Move;
    ULearningAgentsActions::GetContinuousAction(Move, InActionObject, MoveElement, TEXT("Move"));
    int32 Do = 0;
    ULearningAgentsActions::GetExclusiveDiscreteAction(Do, InActionObject, DoElement, TEXT("Do"));

    Mind->PolicyMove = Move.Num() >= 2 ? FVector2D(Move[0], Move[1]) : FVector2D::ZeroVector;

    if (!Human->IsAlive() || Mind->bAsleep)
    {
        return;
    }
    if (Do == DoStop)
    {
        Mind->PolicyStop();
        return;
    }
    if (Human->AreHandsBusy())
    {
        return;
    }
    if (Do == DoPut)
    {
        if (Human->CarriedItem)
        {
            Human->ReleaseFromHands(false);
        }
        return;
    }
    if (Do == DoTake)
    {
        if (!Human->CarriedItem)
        {
            UHumanWorldSubsystem* WorldMind = Human->GetWorld() ? Human->GetWorld()->GetSubsystem<UHumanWorldSubsystem>() : nullptr;
            if (WorldMind)
            {
                TArray<FAffordance> Near;
                WorldMind->CollectOffersNear(Human->GetActorLocation(), 250.0f, Near);
                AActor* Best = nullptr;
                float BestDist = 250.0f;
                for (const FAffordance& A : Near)
                {
                    AActor* Thing = A.Target.Get();
                    if (!Thing || Thing == Best)
                    {
                        continue;
                    }
                    const AFurnitureActor* Furniture = Cast<AFurnitureActor>(Thing);
                    if (!Cast<ABookActor>(Thing) && !(Furniture && Furniture->CanBeLifted()))
                    {
                        continue;
                    }
                    const float Dist = FVector::Dist(Thing->GetActorLocation(), Human->GetActorLocation());
                    if (Dist < BestDist)
                    {
                        BestDist = Dist;
                        Best = Thing;
                    }
                }
                if (Best)
                {
                    Human->BeginTake(Best);
                }
            }
        }
        return;
    }
    if (Do <= 0 || Mind->IsBusyForPolicy())
    {
        return;
    }

    if (Do == 1)
    {
        UHumanWorldSubsystem* WorldMind = Human->GetWorld() ? Human->GetWorld()->GetSubsystem<UHumanWorldSubsystem>() : nullptr;
        ACompleteHumanNPC* Other = WorldMind ? WorldMind->GetNearestHuman(Human->GetActorLocation(), 320.0f, Human) : nullptr;
        if (Other && Human->HasLineOfSight(Other))
        {
            Mind->PolicyTalk(Other);
        }
        return;
    }

    TArray<FAffordance> Offers;
    CollectOffers(Human, Offers);
    const FAffordance* Best = nullptr;
    float BestDist = Reach;
    for (const FAffordance& A : Offers)
    {
        if (KindOf(A) != Do)
        {
            continue;
        }
        const float Dist = FVector::Dist(A.Location, Human->GetActorLocation());
        if (Dist <= BestDist && Human->CanReachPoint(A.Location))
        {
            BestDist = Dist;
            Best = &A;
        }
    }
    if (Best)
    {
        Mind->PolicyUse(*Best);
    }
}

float UHumanTrainingEnvironment::Feeling(const ACompleteHumanNPC* Human)
{
    if (!Human)
    {
        return 0.0f;
    }
    const UPhysiologyComponent* Body = Human->PhysiologyComponent;
    const UEmotionComponent* Emotions = Human->EmotionComponent;
    const FHormones Hormones = Body ? Body->Hormones : FHormones();

    const float Pleasure = Emotions ? Emotions->GetAffect().Pleasure : 0.0f;
    const float Stress = Emotions ? Emotions->GetStress() : 0.0f;
    const float Pain = Body ? Body->Body.Pain : 0.0f;
    const float Distress = Body ? Body->GetBodilyDistress() : 0.0f;

    return Pleasure * 0.5f
        + (Hormones.Dopamine - 0.4f) * 0.6f
        + (Hormones.Endorphin - 0.2f) * 0.3f
        + (Hormones.Oxytocin - 0.3f) * 0.3f
        + (Hormones.Serotonin - 0.5f) * 0.3f
        - Pain
        - Distress * 0.8f
        - Stress * 0.3f
        - (Hormones.Cortisol - 0.15f) * 0.3f;
}

void UHumanTrainingEnvironment::GatherAgentReward_Implementation(float& OutReward, const int32 AgentId)
{
    OutReward = 0.0f;
    ACompleteHumanNPC* Human = HumanSense::AgentOf(HumanManager, AgentId);
    if (!Human)
    {
        return;
    }

    const float Now = Human->IsAlive() ? Feeling(Human) : -3.0f;
    const float* Before = PreviousFeeling.Find(AgentId);
    const float Change = Before ? Now - *Before : 0.0f;
    PreviousFeeling.Add(AgentId, Now);

    OutReward = FMath::Clamp(Now * 0.05f + Change * 2.0f, -5.0f, 5.0f);
    FeelingSum += Now;
    ++FeelingCount;
    RecentSum += Now;
    ++RecentCount;
}

void UHumanTrainingEnvironment::GatherAgentCompletion_Implementation(ELearningAgentsCompletion& OutCompletion, const int32 AgentId)
{
    const ACompleteHumanNPC* Human = HumanSense::AgentOf(HumanManager, AgentId);
    OutCompletion = (Human && !Human->IsAlive()) ? ELearningAgentsCompletion::Termination : ELearningAgentsCompletion::Running;
}

void UHumanTrainingEnvironment::ResetAgentEpisode_Implementation(const int32 AgentId)
{
    PreviousFeeling.Remove(AgentId);
    ACompleteHumanNPC* Human = HumanSense::AgentOf(HumanManager, AgentId);
    if (Human && !Human->IsAlive())
    {
        Human->Revive(Human->HasHome() ? Human->GetHomeLocation() : Human->GetActorLocation());
        ++Rebirths;
    }
}

AMindLearningDirector::AMindLearningDirector()
{
    PrimaryActorTick.bCanEverTick = true;
    Manager = CreateDefaultSubobject<UHumanAgentsManager>(TEXT("Manager"));

    TrainerSettings.MaxEpisodeStepNum = 2048;
    TrainerSettings.MaximumRecordedStepsPerIteration = 8192;

    TrainingSettings.DiscountFactor = 0.999f;
    TrainingSettings.LearningRatePolicy = 0.0003f;
    TrainingSettings.LearningRateCritic = 0.003f;
    TrainingSettings.ActionEntropyWeight = 0.005f;
    TrainingSettings.Device = ELearningAgentsTrainingDevice::GPU;

    GameSettings.bUseFixedTimeStep = true;
    GameSettings.FixedTimeStepFrequency = 30.0f;
    GameSettings.bUseUnlitViewportRendering = false;
}

bool AMindLearningDirector::ReadyToSetup() const
{
    const UWorld* World = GetWorld();
    const UHumanWorldSubsystem* WorldMind = World ? World->GetSubsystem<UHumanWorldSubsystem>() : nullptr;
    return WorldMind && WorldMind->GetAllHumans().Num() > 0;
}

FString AMindLearningDirector::BrainFolder() const
{
    return FPaths::ProjectSavedDir() / TEXT("LearnedMind") / FString::Printf(TEXT("%d_%d_%d_%d"),
        HumanSense::BodyCount, HumanSense::SectorCount, HumanSense::KindCount, HumanSense::DoCount);
}

FString AMindLearningDirector::BrainFile(const TCHAR* Part) const
{
    return BrainFolder() / FString::Printf(TEXT("%s.bin"), Part);
}

void AMindLearningDirector::SaveBrain()
{
    if (!Policy || !Critic)
    {
        return;
    }
    IFileManager::Get().MakeDirectory(*BrainFolder(), true);

    auto Save = [this](ULearningAgentsNeuralNetwork* Network, const TCHAR* Part)
    {
        if (Network)
        {
            FFilePath Path;
            Path.FilePath = BrainFile(Part);
            Network->SaveNetworkToSnapshot(Path);
        }
    };
    Save(Policy->GetEncoderNetworkAsset(), TEXT("encoder"));
    Save(Policy->GetPolicyNetworkAsset(), TEXT("policy"));
    Save(Policy->GetDecoderNetworkAsset(), TEXT("decoder"));
    Save(Critic->GetCriticNetworkAsset(), TEXT("critic"));
}

void AMindLearningDirector::LoadBrain()
{
    if (!Policy || !Critic)
    {
        return;
    }
    const TCHAR* Parts[] = { TEXT("encoder"), TEXT("policy"), TEXT("decoder"), TEXT("critic") };
    for (const TCHAR* Part : Parts)
    {
        if (!FPaths::FileExists(BrainFile(Part)))
        {
            UE_LOG(LogHumanCity, Warning, TEXT("Мозг: сохранённых весов нет, существа начинают с нуля"));
            return;
        }
    }

    auto Load = [this](ULearningAgentsNeuralNetwork* Network, const TCHAR* Part)
    {
        if (Network)
        {
            FFilePath Path;
            Path.FilePath = BrainFile(Part);
            Network->LoadNetworkFromSnapshot(Path);
        }
    };
    Load(Policy->GetEncoderNetworkAsset(), TEXT("encoder"));
    Load(Policy->GetPolicyNetworkAsset(), TEXT("policy"));
    Load(Policy->GetDecoderNetworkAsset(), TEXT("decoder"));
    Load(Critic->GetCriticNetworkAsset(), TEXT("critic"));
    UE_LOG(LogHumanCity, Warning, TEXT("Мозг: загружены выученные веса из %s"), *BrainFolder());
}

void AMindLearningDirector::SetupLearning()
{
    ULearningAgentsManager* BaseManager = Manager;

    ULearningAgentsInteractor* BaseInteractor = ULearningAgentsInteractor::MakeInteractor(
        BaseManager, UHumanInteractor::StaticClass(), TEXT("Senses"));
    if (UHumanInteractor* Senses = Cast<UHumanInteractor>(BaseInteractor))
    {
        Senses->HumanManager = Manager;
    }

    FLearningAgentsPolicySettings PolicySettings;
    PolicySettings.HiddenLayerNum = 2;
    PolicySettings.HiddenLayerSize = 256;
    PolicySettings.bUseMemory = true;
    PolicySettings.MemoryStateSize = 64;
    ULearningAgentsPolicy* BasePolicy = ULearningAgentsPolicy::MakePolicy(
        BaseManager, BaseInteractor, ULearningAgentsPolicy::StaticClass(), TEXT("Brain"),
        nullptr, nullptr, nullptr, true, true, true, PolicySettings, 1234);

    FLearningAgentsCriticSettings CriticSettings;
    CriticSettings.HiddenLayerNum = 2;
    CriticSettings.HiddenLayerSize = 256;
    ULearningAgentsCritic* BaseCritic = ULearningAgentsCritic::MakeCritic(
        BaseManager, BaseInteractor, BasePolicy, ULearningAgentsCritic::StaticClass(), TEXT("Critic"),
        nullptr, true, CriticSettings, 1234);

    ULearningAgentsTrainingEnvironment* BaseEnvironment = ULearningAgentsTrainingEnvironment::MakeTrainingEnvironment(
        BaseManager, UHumanTrainingEnvironment::StaticClass(), TEXT("Life"));
    if (UHumanTrainingEnvironment* Life = Cast<UHumanTrainingEnvironment>(BaseEnvironment))
    {
        Life->HumanManager = Manager;
    }

    Interactor = BaseInteractor;
    Policy = BasePolicy;
    Critic = BaseCritic;
    Environment = BaseEnvironment;

    if (!Interactor || !Policy || !Critic || !Environment)
    {
        UE_LOG(LogHumanCity, Error, TEXT("Мозг: не удалось собрать нейросеть"));
        return;
    }

    LoadBrain();

    const FString Python = FPaths::ProjectIntermediateDir() / TEXT("PipInstall/Scripts/python.exe");
    bTraining = !FParse::Param(FCommandLine::Get(), TEXT("BrainInference")) && FPaths::FileExists(Python);

    if (bTraining)
    {
        FLearningAgentsTrainerProcessSettings ProcessSettings;
        FLearningAgentsSharedMemoryCommunicatorSettings MemorySettings;
        MemorySettings.TaskName = TEXT("CityLife");
        MemorySettings.Timeout = 120.0f;

        TrainerProcess = ULearningAgentsCommunicatorLibrary::SpawnSharedMemoryTrainingProcess(ProcessSettings, MemorySettings);
        if (TrainerProcess.TrainerProcess.IsValid())
        {
            const FLearningAgentsCommunicator Communicator = ULearningAgentsCommunicatorLibrary::MakeSharedMemoryCommunicator(TrainerProcess, MemorySettings);
            Trainer = ULearningAgentsPPOTrainer::MakePPOTrainer(
                BaseManager, BaseInteractor, BaseEnvironment, BasePolicy, BaseCritic, Communicator,
                ULearningAgentsPPOTrainer::StaticClass(), TEXT("Trainer"), TrainerSettings);
        }
        bTraining = Trainer != nullptr;
        UE_LOG(LogHumanCity, Warning, TEXT("Мозг: обучение %s (PPO, %s)"), bTraining ? TEXT("включено") : TEXT("не запустилось"), *Python);
    }
    else
    {
        UE_LOG(LogHumanCity, Warning, TEXT("Мозг: обучение выключено — нет %s. Существа действуют тем, что уже выучили."), *Python);
    }

    bSetup = true;
}

void AMindLearningDirector::RegisterNewHumans()
{
    UWorld* World = GetWorld();
    UHumanWorldSubsystem* WorldMind = World ? World->GetSubsystem<UHumanWorldSubsystem>() : nullptr;
    if (!WorldMind || !Manager)
    {
        return;
    }

    for (ACompleteHumanNPC* Human : WorldMind->GetAllHumans())
    {
        if (!Human || Registered.Contains(Human) || (Human->Mind && Human->Mind->HasLearnedMind()))
        {
            continue;
        }
        if (Manager->GetAgentNum() >= Manager->GetMaxAgentNum())
        {
            break;
        }
        Manager->AddAgent(Human);
        Registered.Add(Human);
        if (Human->Mind)
        {
            Human->Mind->bPolicyControlled = true;
        }
    }
}

void AMindLearningDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!bSetup)
    {
        StartDelay -= DeltaSeconds;
        if (StartDelay > 0.0f || !ReadyToSetup())
        {
            return;
        }
        SetupLearning();
        if (!bSetup)
        {
            SetActorTickEnabled(false);
            return;
        }
    }

    RegisterNewHumans();

    DecisionTimer += DeltaSeconds;
    if (DecisionTimer < DecisionInterval)
    {
        return;
    }
    DecisionTimer = 0.0f;

    if (bTraining && Trainer && !Trainer->HasTrainingFailed())
    {
        Trainer->RunTraining(TrainingSettings, GameSettings, false, false);
    }
    else if (Policy)
    {
        if (bTraining)
        {
            bTraining = false;
            UE_LOG(LogHumanCity, Error, TEXT("Мозг: связь с обучением потеряна, дальше только применение выученного"));
        }
        Policy->RunInference(1.0f);
    }

    const float Real = DecisionInterval;
    SaveTimer += Real;
    if (SaveTimer > 120.0f)
    {
        SaveTimer = 0.0f;
        SaveBrain();
    }

    ReportTimer += Real;
    if (ReportTimer > 30.0f)
    {
        ReportTimer = 0.0f;
        if (UHumanTrainingEnvironment* Life = Cast<UHumanTrainingEnvironment>(Environment))
        {
            if (Life->RecentCount > 0)
            {
                const float Window = static_cast<float>(Life->RecentSum / static_cast<double>(Life->RecentCount));
                if (!Life->bHaveFirst)
                {
                    Life->FirstWindow = Window;
                    Life->bHaveFirst = true;
                }
                UE_LOG(LogHumanCity, Warning,
                    TEXT("Мозг: самочувствие сейчас %.4f, вначале %.4f, сдвиг %+.4f, шагов %lld, возрождений %d, обучение %s"),
                    Window, Life->FirstWindow, Window - Life->FirstWindow,
                    Life->FeelingCount, Life->Rebirths, bTraining ? TEXT("идёт") : TEXT("выключено"));
                Life->RecentSum = 0.0;
                Life->RecentCount = 0;
            }
        }
    }
}

void AMindLearningDirector::EndPlay(const EEndPlayReason::Type Reason)
{
    SaveBrain();
    if (Trainer && Trainer->IsTraining())
    {
        Trainer->EndTraining();
    }
    Trainer = nullptr;
    TrainerProcess.TrainerProcess.Reset();
    Super::EndPlay(Reason);
}
