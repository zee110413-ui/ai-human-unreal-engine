// CompleteHumanAI.cpp

#include "CompleteHumanAI.h"
#include "FurnitureActor.h"
#include "BookActor.h"
#include "Components/BoxComponent.h"
#include "ClothingActor.h"
#include "EngineUtils.h"

#include "PersonalityComponent.h"
#include "PhysiologyComponent.h"
#include "NeedComponent.h"
#include "EmotionComponent.h"
#include "MemoryComponent.h"
#include "IdentityComponent.h"
#include "SocialComponent.h"
#include "MotivationComponent.h"
#include "SpeechComponent.h"
#include "DeliberationComponent.h"
#include "MindComponent.h"

#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Components/CapsuleComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimInstance.h"
#include "AffordanceComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/PoseableMeshComponent.h"
#include "BodyMotorComponent.h"
#include "MatterSubsystem.h"
#include "TerrainGrid.h"
#include "ResourceActor.h"
#include "HumanMovementComponent.h"
#include "Village.h"
#include "Misc/App.h"

ACompleteHumanNPC::ACompleteHumanNPC(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer.SetDefaultSubobjectClass<UHumanMovementComponent>(ACharacter::CharacterMovementComponentName))
{
    PrimaryActorTick.bCanEverTick = true;

    AIControllerClass = AAIController::StaticClass();
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

    PerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("PerceptionComponent"));

    if (UCapsuleComponent* Capsule = GetCapsuleComponent())
    {
        Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        Capsule->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
        Capsule->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
        Capsule->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
        Capsule->SetNotifyRigidBodyCollision(true);
    }

    if (UCharacterMovementComponent* Move = GetCharacterMovement())
    {
        Move->bEnablePhysicsInteraction = true;
        Move->bPushForceUsingZOffset = false;
        Move->PushForceFactor = 200000.0f;
        Move->InitialPushForceFactor = 1500.0f;
        Move->MaxStepHeight = 45.0f;
    }

    PoseBody = CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("PoseBody"));
    PoseBody->SetupAttachment(RootComponent);
    PoseBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PoseBody->SetCanEverAffectNavigation(false);
    PoseBody->SetVisibility(false);

    Motor = CreateDefaultSubobject<UBodyMotorComponent>(TEXT("Motor"));

    // Простое тело: без него человека попросту не видно на сцене.
    // Когда появится MetaHuman — этот компонент можно просто скрыть.
    PlaceholderBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderBody"));
    PlaceholderBody->SetupAttachment(RootComponent);
    // Куб — 100 см и центрирован, поэтому масштаб задаёт габариты напрямую:
    // 45 x 32 x 176 см. Несимметричное сечение позволяет видеть, куда человек
    // повёрнут, без всяких стрелок.
    PlaceholderBody->SetRelativeLocation(FVector::ZeroVector);
    PlaceholderBody->SetRelativeScale3D(FVector(0.45f, 0.32f, 1.76f));
    PlaceholderBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PlaceholderBody->SetCanEverAffectNavigation(false);
    {
        static ConstructorHelpers::FObjectFinder<UStaticMesh> BodyMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
        if (BodyMesh.Succeeded())
        {
            PlaceholderBody->SetStaticMesh(BodyMesh.Object);
        }
    }

    // --- Подсистемы разума ---
    PersonalityComponent = CreateDefaultSubobject<UPersonalityComponent>(TEXT("Personality"));
    PhysiologyComponent  = CreateDefaultSubobject<UPhysiologyComponent>(TEXT("Physiology"));
    NeedComponent        = CreateDefaultSubobject<UNeedComponent>(TEXT("Needs"));
    EmotionComponent     = CreateDefaultSubobject<UEmotionComponent>(TEXT("Emotions"));
    MemoryComponent      = CreateDefaultSubobject<UMemoryComponent>(TEXT("Memory"));
    IdentityComponent    = CreateDefaultSubobject<UIdentityComponent>(TEXT("Identity"));
    SocialComponent      = CreateDefaultSubobject<USocialComponent>(TEXT("Social"));
    MotivationComponent  = CreateDefaultSubobject<UMotivationComponent>(TEXT("Motivation"));
    SpeechComponent      = CreateDefaultSubobject<USpeechComponent>(TEXT("Speech"));
    DeliberationComponent = CreateDefaultSubobject<UDeliberationComponent>(TEXT("Deliberation"));
    Mind                 = CreateDefaultSubobject<UMindComponent>(TEXT("Mind"));

    if (UCharacterMovementComponent* Move = GetCharacterMovement())
    {
        Move->MaxWalkSpeed = BaseWalkSpeed;
        Move->bOrientRotationToMovement = true;
        Move->RotationRate = FRotator(0.0f, 360.0f, 0.0f);
        Move->bUseControllerDesiredRotation = false;

        // Люди обходят друг друга, а не проходят насквозь. Мелочь, без
        // которой толпа сразу выглядит неживой.
        Move->bUseRVOAvoidance = true;
        Move->AvoidanceConsiderationRadius = 260.0f;
        Move->AvoidanceWeight = 0.5f;
    }
    bUseControllerRotationYaw = false;
}

// ---------------------------------------------------------------------------
//  Начало жизни
// ---------------------------------------------------------------------------

void ACompleteHumanNPC::BeginPlay()
{
    Super::BeginPlay();

    SetupPerception();
    SetupAppearance();
    GenerateRandomAppearance();

    // Разум собирает себя: находит подсистемы и раздаёт им личность.
    if (Mind)
    {
        Mind->Awaken();
    }

    // Регистрируемся в мире.
    if (UWorld* World = GetWorld())
    {
        if (UHumanWorldSubsystem* WorldMind = World->GetSubsystem<UHumanWorldSubsystem>())
        {
            WorldMind->RegisterHuman(this);

            // Имя должно быть уникальным — в городе не может быть пяти
            // одинаковых Игорей Волковых.
            if (IdentityComponent)
            {
                int32 Attempts = 0;
                while (!WorldMind->TryClaimName(IdentityComponent->GetFullName()) && Attempts++ < 12)
                {
                    IdentityComponent->LastName = HumanText::RandomLastName(IdentityComponent->bFemale);
                    if (Attempts > 6)
                    {
                        IdentityComponent->FirstName = HumanText::RandomFirstName(IdentityComponent->bFemale);
                    }
                }
            }

            // Дом на карте не появляется сам собой. У человека есть жильё,
            // но где оно — он должен узнать так же, как всё остальное:
            // дойти и увидеть. Иначе получается, что он родился со знанием
            // города, а этого ни с кем не бывает.
        }
    }

    if (IdentityComponent)
    {
        CurrentJob = IdentityComponent->Occupation;
    }

    // Старые бессознательные желания сохраняем — они больше ни на что не
    // влияют напрямую, но пусть остаются для совместимости.
    if (UnconsciousDesires.Num() == 0)
    {
        UnconsciousDesires = { TEXT("Богатство"), TEXT("Власть"), TEXT("Любовь") };
    }

    SyncLegacyMirror();
}

void ACompleteHumanNPC::EndPlay(const EEndPlayReason::Type Reason)
{
    if (UWorld* World = GetWorld())
    {
        if (UHumanWorldSubsystem* WorldMind = World->GetSubsystem<UHumanWorldSubsystem>())
        {
            WorldMind->UnregisterHuman(this);
        }
    }
    Super::EndPlay(Reason);
}

// ---------------------------------------------------------------------------
//  Жизнь
// ---------------------------------------------------------------------------

void ACompleteHumanNPC::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!IsAlive())
    {
        if (Mind)
        {
            Mind->Advance(DeltaSeconds);
        }
        return;
    }

    // --- Скорость зависит от состояния тела --------------------------------
    if (UCharacterMovementComponent* Move = GetCharacterMovement())
    {
        const float Multiplier = PhysiologyComponent ? PhysiologyComponent->GetMovementSpeedMultiplier() : 1.0f;
        const float Hurry = Mind ? Mind->GetHurry() : 0.0f;
        Move->MaxWalkSpeed = BaseWalkSpeed * Multiplier * (1.0f + Hurry * 0.45f);
        if (IdentityComponent && IdentityComponent->Age < 16.0f)
        {
            Move->MaxWalkSpeed *= FMath::Clamp(static_cast<float>(GetActorScale3D().Z), 0.3f, 1.0f);
        }
        if (Motor && Motor->IsInfant())
        {
            Move->MaxWalkSpeed = Motor->CanTryWalking() ? Move->MaxWalkSpeed * 0.5f : 0.0f;
        }
        UpdateFooting(DeltaSeconds);
    }

    const bool bHandsBusy = AreHandsBusy();
    if (bDirectSteering && !bHandsBusy)
    {
        UpdateDirectSteering(DeltaSeconds);
    }
    UpdateHandsMovement(DeltaSeconds);

    if (Mind && Mind->bPolicyControlled && !bMoveRequested && Posture == EPosture::Standing && !bHandsBusy)
    {
        const float Turn = FMath::Clamp(Mind->PolicyMove.Y, -1.0f, 1.0f);
        const float Forward = FMath::Clamp(Mind->PolicyMove.X, 0.0f, 1.0f);
        if (BodyLeadsFeet())
        {
            if (Forward > 0.02f)
            {
                AddMovementInput(GetActorForwardVector().RotateAngleAxis(Turn * 60.0f, FVector::UpVector), Forward);
            }
        }
        else
        {
            AddActorWorldRotation(FRotator(0.0f, Turn * 140.0f * DeltaSeconds, 0.0f));
            if (Forward > 0.02f)
            {
                AddMovementInput(GetActorForwardVector(), Forward);
            }
        }
    }

    // --- Разум --------------------------------------------------------------
    if (Mind)
    {
        Mind->Advance(DeltaSeconds);
    }

    // --- Осмотр окрестностей ------------------------------------------------
    ScanAccumulator += DeltaSeconds;
    if (ScanAccumulator >= 0.5f)
    {
        ScanAccumulator = 0.0f;
        if (IdentityComponent && IdentityComponent->Age < 18.0f)
        {
            UpdateGrowth();
        }
        ScanSurroundings();
        DiscoverPlacesNearby();
        UpdateBodyTint();
        UpdateAnimation();
    }

    MovePosture(DeltaSeconds);

    GroundAccumulator += DeltaSeconds;
    if (GroundAccumulator >= 2.0f)
    {
        GroundAccumulator = 0.0f;
        NoticeThingsAround();
    }

    // --- Что видно со стороны ------------------------------------------------
    UpdateTags(DeltaSeconds);

    // --- Зеркало старых полей -----------------------------------------------
    MirrorAccumulator += DeltaSeconds;
    if (MirrorAccumulator >= 1.0f)
    {
        MirrorAccumulator = 0.0f;
        SyncLegacyMirror();
    }
}

// ---------------------------------------------------------------------------
//  Таблички над головой
//
//  Всё, что человек делает и говорит, до сих пор уходило в лог, и со стороны
//  город выглядел молчаливым: люди просто ходили. Теперь над каждым видно,
//  кто он, чем занят и что сейчас сказал.
// ---------------------------------------------------------------------------

void ACompleteHumanNPC::ShowSpeech(const FString& Text)
{
    const FString Spoken = Text.TrimStartAndEnd();
    if (Spoken.IsEmpty() || Spoken.StartsWith(TEXT("(")) || Spoken.StartsWith(TEXT("«(")))
    {
        return;
    }

    LastSpokenLine = Text.Left(120);

    // Реплика висит тем дольше, чем она длиннее: её же надо прочесть.
    SpeechTagTimeLeft = FMath::Clamp(2.5f + Text.Len() * 0.045f, 3.5f, 10.0f);
}

void ACompleteHumanNPC::UpdateTags(float DeltaSeconds)
{
    // Шрифт готовых текстовых компонентов кириллицы не знает — вместо букв
    // выходили белые квадраты. Поэтому надписи рисуются средствами движка:
    // они берут шрифт редактора, а он русский текст показывает исправно.
    UWorld* World = GetWorld();
    if (!bShowTags || !World || !FApp::CanEverRender())
    {
        return;
    }

    if (SpeechTagTimeLeft > 0.0f)
    {
        SpeechTagTimeLeft -= static_cast<float>(FApp::GetDeltaTime());
    }

    // Смотреть на весь город сразу незачем: подписываем тех, кто рядом.
    const APlayerController* PC = World->GetFirstPlayerController();
    if (!PC)
    {
        return;
    }

    FVector ViewPoint;
    FRotator ViewRotation;
    PC->GetPlayerViewPoint(ViewPoint, ViewRotation);

    const float Distance = FVector::Dist(ViewPoint, GetActorLocation());
    if (Distance > 4500.0f)
    {
        return;
    }

    // Видно только сказанное вслух. Мысли и занятия человека со стороны
    // не читаются — как и у настоящих людей; их место в летописи.
    if (SpeechTagTimeLeft > 0.0f && !LastSpokenLine.IsEmpty())
    {
        const FVector Head = GetActorLocation() + FVector(0.0f, 0.0f, 128.0f);
        DrawDebugString(World, Head,
                        FString::Printf(TEXT("%s: «%s»"),
                            IdentityComponent ? *IdentityComponent->FirstName : TEXT(""),
                            *LastSpokenLine),
                        nullptr, FColor(235, 240, 245), 0.0f, true, 1.25f);
    }
}

// ---------------------------------------------------------------------------
//  Движение
// ---------------------------------------------------------------------------

void ACompleteHumanNPC::RequestMoveTo(const FVector& Destination)
{
    // =======================================================================
    //  У человека нет маршрута.
    //
    //  Он знает только, КУДА ему нужно — направление и примерное место.
    //  Дороги туда он не знает: он пойдёт и будет смотреть перед собой,
    //  обходя то, что окажется на пути. Дверь он не «знает» — он найдёт
    //  её, идя вдоль стены, как это делает всякий, кто впервые пришёл
    //  к незнакомому дому.
    // =======================================================================
    MoveTarget = Destination;
    bMoveRequested = true;
    bDirectSteering = true;

    Heading = GetActorForwardVector();
    WallSide = 0;
    WallFollowTime = 0.0f;
    StuckTime = 0.0f;
    LastStuckCheckPos = GetActorLocation();
    TrailSinceLastCrumb = 0.0f;

    // --- Начинаем запоминать дорогу ----------------------------------------
    TravelStart = GetActorLocation();
    TravelBeganAt = 0.0f;
    if (UWorld* World = GetWorld())
    {
        if (UHumanWorldSubsystem* WorldMind = World->GetSubsystem<UHumanWorldSubsystem>())
        {
            TravelBeganAt = WorldMind->WorldSeconds;
        }
    }
    Trail.Reset();
    Trail.Add(TravelStart);

    // --- А не ходил ли я уже туда? -----------------------------------------
    // Подсказка памяти — не рельсы: она говорит, куда держать, а идти
    // всё равно придётся глазами.
    RecalledPath.Reset();
    PathIndex = 0;

    if (MemoryComponent)
    {
        float WorldTime = TravelBeganAt;
        MemoryComponent->RecallRoute(TravelStart, Destination, WorldTime, RecalledPath);

        // Первые точки, которые уже позади, пропускаем: возвращаться
        // к началу знакомой дороги ради «правильного» её начала — глупо.
        while (RecalledPath.IsValidIndex(PathIndex)
               && FVector::Dist2D(TravelStart, RecalledPath[PathIndex]) < 300.0f)
        {
            ++PathIndex;
        }
    }
}

void ACompleteHumanNPC::StopMoving()
{
    bMoveRequested = false;
    bDirectSteering = false;
    WallSide = 0;
    WallFollowTime = 0.0f;

    if (AAIController* AI = Cast<AAIController>(GetController()))
    {
        AI->StopMovement();
    }
}

// ---------------------------------------------------------------------------
//  Зрение: насколько далеко видно в эту сторону
// ---------------------------------------------------------------------------

float ACompleteHumanNPC::LookAhead(const FVector& Direction, float Distance, FVector& OutNormal) const
{
    OutNormal = FVector::ZeroVector;

    UWorld* World = GetWorld();
    if (!World)
    {
        return Distance;
    }

    const float Radius = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleRadius() : 34.0f;

    // Смотрим на уровне груди: то, что под ногами (бордюры, ступени),
    // человека не останавливает.
    const FVector Start = GetActorLocation() + FVector(0.0f, 0.0f, 20.0f);
    const FVector End = Start + Direction * Distance;

    FCollisionQueryParams Params(SCENE_QUERY_STAT(HumanLook), false, this);
    FHitResult Hit;

    const bool bBlocked = World->SweepSingleByChannel(
        Hit, Start, End, FQuat::Identity, ECC_WorldStatic,
        FCollisionShape::MakeSphere(Radius * 0.9f), Params);

    if (!bBlocked)
    {
        return Distance;
    }

    OutNormal = Hit.ImpactNormal;
    OutNormal.Z = 0.0f;
    OutNormal = OutNormal.GetSafeNormal();

    return Hit.Distance;
}

void ACompleteHumanNPC::UpdateDirectSteering(float DeltaSeconds)
{
    if (!bMoveRequested)
    {
        return;
    }

    const FVector Here = GetActorLocation();

    // --- Хлебные крошки: дорога запоминается, пока по ней идут --------------
    TrailSinceLastCrumb += FVector::Dist2D(Here, Trail.Num() > 0 ? Trail.Last() : Here);
    if (TrailSinceLastCrumb > 350.0f && Trail.Num() < 48)
    {
        Trail.Add(Here);
        TrailSinceLastCrumb = 0.0f;
    }

    FVector ToGoal = MoveTarget - Here;
    ToGoal.Z = 0.0f;
    const float DistToGoal = ToGoal.Size();

    // Стоять вплотную к стене, за которой цель, — это не «дошёл».
    if (DistToGoal < 110.0f && CanReachPoint(MoveTarget))
    {
        // Дошёл — значит, теперь знает, как сюда ходить.
        if (MemoryComponent && Trail.Num() >= 2)
        {
            float WorldTime = TravelBeganAt;
            if (UWorld* World = GetWorld())
            {
                if (UHumanWorldSubsystem* WorldMind = World->GetSubsystem<UHumanWorldSubsystem>())
                {
                    WorldTime = WorldMind->WorldSeconds;
                }
            }
            Trail.Add(Here);
            MemoryComponent->LearnRoute(TravelStart, MoveTarget, Trail,
                                        FMath::Max(1.0f, WorldTime - TravelBeganAt), WorldTime);
        }

        bDirectSteering = false;
        bMoveRequested = false;
        WallSide = 0;
        RecalledPath.Reset();
        return;
    }

    // --- Куда держать: к подсказке памяти или прямо к цели ------------------
    FVector Aim = MoveTarget;
    if (RecalledPath.IsValidIndex(PathIndex))
    {
        Aim = RecalledPath[PathIndex];

        // Поворот пройден — держим на следующий.
        if (FVector::Dist2D(Here, Aim) < 260.0f)
        {
            ++PathIndex;
            if (RecalledPath.IsValidIndex(PathIndex))
            {
                Aim = RecalledPath[PathIndex];
            }
            else
            {
                Aim = MoveTarget;
            }
        }

        // Если цель уже ближе, чем подсказка, — память больше не нужна.
        if (FVector::Dist2D(Here, MoveTarget) < FVector::Dist2D(Here, Aim))
        {
            RecalledPath.Reset();
            Aim = MoveTarget;
        }
    }

    FVector ToAim = Aim - Here;
    ToAim.Z = 0.0f;
    const FVector GoalDir = ToAim.GetSafeNormal(0.001f).IsNearlyZero()
        ? (ToGoal / DistToGoal)
        : ToAim.GetSafeNormal();

    // =======================================================================
    //  ЧЕЛОВЕК СМОТРИТ ПЕРЕД СОБОЙ
    //
    //  Никакого маршрута нет. Есть направление, в котором он хочет идти,
    //  и есть то, что он видит. Он прикидывает несколько направлений,
    //  смотрит, насколько далеко в каждую сторону свободно, и выбирает
    //  то, что и ведёт куда надо, и не упирается в стену.
    //
    //  Отсюда само собой получается человеческое: он обходит углы, жмётся
    //  к свободному, притормаживает у препятствия и ищет проход.
    // =======================================================================
    const float Probe = FMath::Min(650.0f, DistToGoal + 120.0f);

    // Веер: прямо, и всё шире в обе стороны.
    static const float Fan[] = { 0.0f, -22.0f, 22.0f, -45.0f, 45.0f, -70.0f, 70.0f, -100.0f, 100.0f };

    float BestScore = -BIG_NUMBER;
    FVector BestDir = GoalDir;
    float BestClear = 0.0f;
    FVector BlockingNormal = FVector::ZeroVector;
    float StraightClear = Probe;

    for (float Angle : Fan)
    {
        const FVector Dir = GoalDir.RotateAngleAxis(Angle, FVector::UpVector);

        FVector Normal;
        const float Clear = LookAhead(Dir, Probe, Normal);

        if (FMath::IsNearlyZero(Angle))
        {
            StraightClear = Clear;
            BlockingNormal = Normal;
        }

        // Чем дальше видно и чем ближе к нужной стороне — тем лучше.
        float Score = (Clear / Probe) * 0.62f + FVector::DotProduct(Dir, GoalDir) * 0.30f;

        // Курса держатся: без этого человек дёргается на месте, без конца
        // передумывая, с какой стороны обойти.
        Score += FVector::DotProduct(Dir, Heading) * 0.14f;

        // Если уже решил обходить с одной стороны — не мечется.
        if (WallSide != 0)
        {
            Score += (FMath::Sign(Angle) == WallSide) ? 0.18f : -0.10f;
        }

        if (Score > BestScore)
        {
            BestScore = Score;
            BestDir = Dir;
            BestClear = Clear;
        }
    }

    // --- Впереди стена: идём вдоль неё, пока не найдётся проход -------------
    // Так человек и находит дверь в незнакомом доме: не «знает, где она»,
    // а ведёт рукой по стене, пока стена не кончится.
    if (StraightClear < 160.0f && !BlockingNormal.IsNearlyZero())
    {
        if (WallSide == 0)
        {
            // В какую сторону обходить — решаем один раз, по тому, куда
            // ближе к цели, и дальше держимся этого решения.
            const FVector Along = FVector::CrossProduct(FVector::UpVector, BlockingNormal);
            WallSide = (FVector::DotProduct(Along, GoalDir) >= 0.0f) ? 1 : -1;
        }

        WallFollowTime += DeltaSeconds;

        const FVector Along = FVector::CrossProduct(FVector::UpVector, BlockingNormal) * static_cast<float>(WallSide);
        // Немного «прижимаемся» к стене, иначе отходим от неё и теряем её.
        BestDir = (Along * 0.85f - BlockingNormal * 0.15f).GetSafeNormal();

        // Долго не удаётся обойти — значит, выбрали не ту сторону.
        if (WallFollowTime > 6.0f)
        {
            WallSide = -WallSide;
            WallFollowTime = 0.0f;
        }
    }
    else if (StraightClear > 320.0f)
    {
        // Путь открылся — перестаём жаться к стене.
        WallSide = 0;
        WallFollowTime = 0.0f;
    }

    // --- Совсем некуда: разворачиваемся -------------------------------------
    if (BestClear < 70.0f)
    {
        BestDir = -Heading;
        WallSide = 0;
    }

    Heading = FMath::VInterpNormalRotationTo(Heading, BestDir, DeltaSeconds, 260.0f);

    // --- Замечает, что стоит на месте ---------------------------------------
    if (FVector::DistSquared2D(Here, LastStuckCheckPos) < 30.0f * 30.0f)
    {
        StuckTime += DeltaSeconds;
    }
    else
    {
        StuckTime = 0.0f;
        LastStuckCheckPos = Here;
    }

    if (StuckTime > 2.5f)
    {
        // Уткнулся и не продвигается — пробует обойти с другой стороны.
        WallSide = (WallSide == 0) ? (FMath::RandBool() ? 1 : -1) : -WallSide;
        WallFollowTime = 0.0f;
        StuckTime = 0.0f;
    }

    // Перед препятствием человек замедляется, а не влетает в него.
    const float Speed = FMath::Clamp(StraightClear / 260.0f, 0.35f, 1.0f);
    AddMovementInput(Heading, Speed);

    if (!BodyLeadsFeet())
    {
        const FRotator Target = Heading.Rotation();
        const FRotator Smooth = FMath::RInterpTo(GetActorRotation(), FRotator(0.0f, Target.Yaw, 0.0f), DeltaSeconds, 7.0f);
        SetActorRotation(Smooth);
    }
}

bool ACompleteHumanNPC::BodyLeadsFeet() const
{
    return Motor && Motor->IsWalkerActive();
}

void ACompleteHumanNPC::SettleAt(const FVector& Where)
{
    bPostureMoving = false;
    PostureTarget = Where;
    SetActorLocation(Where, true, nullptr, ETeleportType::TeleportPhysics);
}

void ACompleteHumanNPC::BeginFalling()
{
    StopMoving();
    if (CarriedItem)
    {
        LetGo(false);
    }
}

void ACompleteHumanNPC::FallDown(const FVector& Direction, float Speed, const FVector* LandAt)
{
    StopMoving();
    if (CarriedItem)
    {
        LetGo(false);
    }
    FVector Along = Direction.GetSafeNormal2D();
    if (Along.IsNearlyZero())
    {
        Along = GetActorForwardVector();
    }
    const FVector Spot = LandAt ? *LandAt : GetActorLocation() + Along * 70.0f * GetActorScale3D().Z;
    SetActorRotation(FRotator(0.0f, (-Along).Rotation().Yaw, 0.0f));
    SetPosture(EPosture::Lying, Spot);

    float Age = 30.0f;
    if (IdentityComponent)
    {
        Age = IdentityComponent->Age;
    }
    const float Brittle = Age > 60.0f ? 1.0f + (Age - 60.0f) * 0.05f : (Age < 10.0f ? 0.5f : 1.0f);
    const float Impact = (0.04f + Speed * 0.05f) * Brittle;
    if (PhysiologyComponent)
    {
        if (Impact > 0.12f)
        {
            PhysiologyComponent->TakeInjury(FMath::Min(Impact * 0.5f, 0.6f), TEXT("падение"));
        }
        else
        {
            PhysiologyComponent->Body.Pain = FMath::Clamp(PhysiologyComponent->Body.Pain + Impact, 0.0f, 1.0f);
        }
    }
    if (Mind)
    {
        Mind->FeelFall(Speed, Impact, GetActorLocation());
    }
}

// ---------------------------------------------------------------------------
//  Дом
// ---------------------------------------------------------------------------

bool ACompleteHumanNPC::HasHome() const
{
    return !Home.Location.IsNearlyZero();
}

bool ACompleteHumanNPC::IsAtHome() const
{
    return HasHome() && FVector::Dist2D(GetActorLocation(), Home.Location) < 900.0f;
}

void ACompleteHumanNPC::SetHome(const FVector& Location)
{
    Home.Location = Location;
    Home.bOwned = true;

    if (MemoryComponent)
    {
        float WorldTime = 0.0f;
        if (UWorld* World = GetWorld())
        {
            if (UHumanWorldSubsystem* WorldMind = World->GetSubsystem<UHumanWorldSubsystem>())
            {
                WorldTime = WorldMind->WorldSeconds;
            }
        }
        MemoryComponent->LearnPlace(EPlaceKind::Home, Location, TEXT("мой дом"), true, nullptr, WorldTime);
    }
}

// ---------------------------------------------------------------------------
//  Восприятие
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
//  Тело: модель и анимации
// ---------------------------------------------------------------------------

void ACompleteHumanNPC::SetupAppearance()
{
    USkeletalMeshComponent* SkelMesh = GetMesh();
    if (!SkelMesh)
    {
        return;
    }

    // Своя модель важнее встроенной.
    USkeletalMesh* Chosen = CharacterMesh;

    if (!Chosen)
    {
        // Гуманоид, который идёт в комплекте с движком. Ничего скачивать
        // не нужно: он есть всегда, у него есть скелет и анимации.
        Chosen = LoadObject<USkeletalMesh>(nullptr,
            TEXT("/Engine/Tutorial/SubEditors/TutorialAssets/Character/TutorialTPP.TutorialTPP"));
    }

    if (!Chosen)
    {
        // Модели нет — остаёмся кубом. Разум от этого не страдает.
        return;
    }

    if (PlaceholderBody)
    {
        PlaceholderBody->SetVisibility(false);
    }

    if (!AnimationBlueprint && PoseBody && Motor)
    {
        SkelMesh->SetVisibility(false);
        SkelMesh->SetComponentTickEnabled(false);
        PoseBody->SetSkinnedAssetAndUpdate(Chosen);
        PoseBody->SetRelativeLocationAndRotation(MeshOffset, MeshRotation);
        PoseBody->SetVisibility(true);
        Motor->Bind(PoseBody);
        return;
    }

    SkelMesh->SetSkeletalMesh(Chosen);
    SkelMesh->SetRelativeLocationAndRotation(MeshOffset, MeshRotation);
    SkelMesh->SetVisibility(true);

    if (AnimationBlueprint)
    {
        // Схема анимации берёт управление на себя — это лучший вариант,
        // когда она есть: там и переходы, и смешивание.
        SkelMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
        SkelMesh->SetAnimInstanceClass(AnimationBlueprint);
        return;
    }

    // Схемы нет — проигрываем одиночные анимации и сами переключаем их
    // по состоянию. Без смешивания, зато работает сразу и без правок.
    if (!IdleAnimation)
    {
        IdleAnimation = LoadObject<UAnimSequence>(nullptr,
            TEXT("/Engine/Tutorial/SubEditors/TutorialAssets/Character/Tutorial_Idle.Tutorial_Idle"));
    }
    if (!WalkAnimation)
    {
        WalkAnimation = LoadObject<UAnimSequence>(nullptr,
            TEXT("/Engine/Tutorial/SubEditors/TutorialAssets/Character/Tutorial_Walk_Fwd.Tutorial_Walk_Fwd"));
    }

    SkelMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    CurrentAnimation = nullptr;
    UpdateAnimation();
}

void ACompleteHumanNPC::UpdateAnimation()
{
    USkeletalMeshComponent* SkelMesh = GetMesh();
    if (!SkelMesh || AnimationBlueprint || (PoseBody && PoseBody->IsVisible()))
    {
        return;
    }

    // Какое состояние сейчас — это знает разум, а не таблица поз.
    UAnimSequence* Wanted = IdleAnimation;

    const bool bSleeping = (Mind && Mind->bAsleep);
    const float Speed = GetVelocity().Size2D();

    if (bSleeping && SleepAnimation)
    {
        Wanted = SleepAnimation;
    }
    else if (!bSleeping && Speed > RunThreshold && RunAnimation)
    {
        Wanted = RunAnimation;
    }
    else if (!bSleeping && Speed > 10.0f && WalkAnimation)
    {
        Wanted = WalkAnimation;
    }
    else if (!bSleeping && Mind && Mind->ConversationPartner && TalkAnimation)
    {
        Wanted = TalkAnimation;
    }
    else if (!bSleeping && Mind && SitAnimation
             && (Mind->CurrentAction == EActionType::Rest || Mind->CurrentAction == EActionType::Eat))
    {
        Wanted = SitAnimation;
    }

    if (!Wanted || Wanted == CurrentAnimation)
    {
        return;
    }

    CurrentAnimation = Wanted;
    SkelMesh->PlayAnimation(Wanted, true);

    // Шаги подгоняются под настоящую скорость: иначе человек «скользит».
    if (Wanted == WalkAnimation || Wanted == RunAnimation)
    {
        const float Reference = (Wanted == RunAnimation) ? 450.0f : 150.0f;
        SkelMesh->SetPlayRate(FMath::Clamp(Speed / Reference, 0.4f, 2.2f));
    }
    else
    {
        SkelMesh->SetPlayRate(1.0f);
    }
}

void ACompleteHumanNPC::SetupPerception()
{
    if (!PerceptionComponent)
    {
        return;
    }

    UAISenseConfig_Sight* SightConfig = NewObject<UAISenseConfig_Sight>(this, TEXT("SightConfig"));
    SightConfig->SightRadius = SightRadius;
    SightConfig->LoseSightRadius = SightRadius * 1.2f;
    SightConfig->PeripheralVisionAngleDegrees = SightHalfAngle;
    SightConfig->SetMaxAge(5.0f);
    SightConfig->DetectionByAffiliation.bDetectEnemies = true;
    SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
    SightConfig->DetectionByAffiliation.bDetectFriendlies = true;

    UAISenseConfig_Hearing* HearingConfig = NewObject<UAISenseConfig_Hearing>(this, TEXT("HearingConfig"));
    HearingConfig->HearingRange = 4000.0f;
    HearingConfig->SetMaxAge(4.0f);
    HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
    HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;
    HearingConfig->DetectionByAffiliation.bDetectFriendlies = true;

    PerceptionComponent->ConfigureSense(*SightConfig);
    PerceptionComponent->ConfigureSense(*HearingConfig);
    PerceptionComponent->SetDominantSense(SightConfig->GetSenseImplementation());
    PerceptionComponent->OnTargetPerceptionUpdated.AddDynamic(this, &ACompleteHumanNPC::OnPerceptionUpdated);
}

void ACompleteHumanNPC::OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
    if (!Actor || Actor == this || !Mind || !Stimulus.WasSuccessfullySensed())
    {
        return;
    }

    const bool bSight = (Stimulus.Type == UAISense::GetSenseID<UAISense_Sight>());
    const FName Kind = bSight ? FName(TEXT("Sight")) : FName(TEXT("Sound"));

    // Звук настораживает сильнее, чем вид: источник не виден.
    const float Salience = bSight ? 0.35f : 0.55f;
    Mind->Notice(Actor, Kind, Stimulus.StimulusLocation, Salience);
}

void ACompleteHumanNPC::NoticeThingsAround()
{
    if (!SpeechComponent || !IsAlive())
    {
        return;
    }

    UWorld* World = GetWorld();
    UHumanWorldSubsystem* WorldMind = World ? World->GetSubsystem<UHumanWorldSubsystem>() : nullptr;
    if (!WorldMind)
    {
        return;
    }

    const FVector Here = GetActorLocation();
    const FVector Forward = GetActorForwardVector();
    const float CosHalf = FMath::Cos(FMath::DegreesToRadians(SightHalfAngle));

    TArray<FAffordance> Offers;
    WorldMind->CollectOffersNear(Here, 900.0f, Offers);

    TSet<AActor*> Already;
    int32 Looked = 0;
    for (const FAffordance& A : Offers)
    {
        AActor* Thing = A.Target.Get();
        if (!Thing || Already.Contains(Thing))
        {
            continue;
        }
        Already.Add(Thing);

        FVector To = Thing->GetActorLocation() - Here;
        To.Z = 0.0f;
        const float Distance = To.Size();
        if (Distance < 1.0f || Distance > 900.0f)
        {
            continue;
        }
        if (FVector::DotProduct(To / Distance, Forward) < CosHalf)
        {
            continue;
        }
        if (++Looked > 6)
        {
            break;
        }
        if (!HasLineOfSight(Thing))
        {
            continue;
        }

        if (const AFurnitureActor* Furniture = Cast<AFurnitureActor>(Thing))
        {
            SpeechComponent->NoticeThing(Furniture->Name);
        }
        else if (Cast<ABookActor>(Thing))
        {
            SpeechComponent->NoticeThing(TEXT("книга"));
        }
    }

    if (WorldMind->GetHumansNear(Here, SightRadius, this).Num() > 0)
    {
        SpeechComponent->NoticeThing(TEXT("человек"));
    }

    const FVector Eye = Here + FVector(0.0f, 0.0f, 62.0f);
    FCollisionQueryParams Overhead(SCENE_QUERY_STAT(HumanSky), false, this);
    FHitResult Roof;
    if (!World->LineTraceSingleByChannel(Roof, Eye, Eye + FVector(0.0f, 0.0f, 1800.0f), ECC_Visibility, Overhead))
    {
        SpeechComponent->NoticeThing(TEXT("небо"));
        SpeechComponent->NoticeThing(TEXT("улица"));
        const float Hour = WorldMind->Now.HourFloat;
        if (Hour > 7.0f && Hour < 19.0f)
        {
            SpeechComponent->NoticeThing(TEXT("солнце"));
        }
    }
}

void ACompleteHumanNPC::ScanSurroundings()
{
    if (!Mind)
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    UHumanWorldSubsystem* WorldMind = World->GetSubsystem<UHumanWorldSubsystem>();
    if (!WorldMind)
    {
        return;
    }

    const FVector MyLocation = GetActorLocation();
    const FVector Forward = GetActorForwardVector();
    const float CosHalfAngle = FMath::Cos(FMath::DegreesToRadians(SightHalfAngle));

    const TArray<ACompleteHumanNPC*> Nearby = WorldMind->GetHumansNear(MyLocation, SightRadius, this);

    for (ACompleteHumanNPC* Other : Nearby)
    {
        if (!Other || !Other->IsAlive())
        {
            continue;
        }

        const FVector ToOther = Other->GetActorLocation() - MyLocation;
        const float Distance = ToOther.Size();
        if (Distance < KINDA_SMALL_NUMBER)
        {
            continue;
        }

        float Salience = 0.0f;
        FName Sense;
        if (!CanPerceive(Other, Salience, Sense))
        {
            continue;
        }

        Mind->Notice(Other, Sense, Other->GetActorLocation(), Salience);
        if (SpeechComponent)
        {
            SpeechComponent->NoticeThing(TEXT("человек"));
        }
    }

    if (!SpeechComponent || FMath::FRand() > 0.3f)
    {
        return;
    }

    const FVector Eye = MyLocation + FVector(0.0f, 0.0f, 62.0f);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(HumanLook), false, this);

    TArray<FAffordance> Offers;
    WorldMind->CollectOffersNear(MyLocation, 900.0f, Offers);
    TSet<const AActor*> Looked;
    for (const FAffordance& Offer : Offers)
    {
        const AActor* Thing = Offer.Target.Get();
        if (!Thing || Looked.Contains(Thing))
        {
            continue;
        }
        Looked.Add(Thing);

        const AFurnitureActor* Furniture = Cast<AFurnitureActor>(Thing);
        const FString Name = Furniture ? Furniture->Name : (Cast<ABookActor>(Thing) ? FString(TEXT("книга")) : FString());
        if (Name.IsEmpty() || SpeechComponent->HasSeen(Name))
        {
            continue;
        }

        const FVector To = Thing->GetActorLocation() - MyLocation;
        if (FVector::DotProduct(To.GetSafeNormal2D(), Forward.GetSafeNormal2D()) < CosHalfAngle)
        {
            continue;
        }

        FCollisionQueryParams ThingParams = Params;
        ThingParams.AddIgnoredActor(Thing);
        FHitResult Hit;
        if (World->LineTraceSingleByChannel(Hit, Eye, Thing->GetActorLocation() + FVector(0.0f, 0.0f, 50.0f), ECC_Visibility, ThingParams))
        {
            continue;
        }

        SpeechComponent->NoticeThing(Name);
        if (Furniture && (Furniture->FurnitureType == EFurnitureType::TV || Furniture->FurnitureType == EFurnitureType::Computer))
        {
            SpeechComponent->NoticeThing(TEXT("экран"));
        }
        else if (Furniture && Furniture->FurnitureType == EFurnitureType::GardenBed && Furniture->Ripeness > 0.3f)
        {
            SpeechComponent->NoticeThing(TEXT("овощи"));
        }
    }

    if (!SpeechComponent->HasSeen(TEXT("небо")) || !SpeechComponent->HasSeen(TEXT("солнце")))
    {
        FHitResult Roof;
        if (!World->LineTraceSingleByChannel(Roof, Eye, Eye + FVector(0.0f, 0.0f, 5000.0f), ECC_Visibility, Params))
        {
            SpeechComponent->NoticeThing(TEXT("небо"));
            SpeechComponent->NoticeThing(TEXT("улица"));
            SpeechComponent->NoticeThing(TEXT("трава"));
            const float Hour = WorldMind->Now.HourFloat;
            if (Hour > 7.0f && Hour < 19.0f)
            {
                SpeechComponent->NoticeThing(TEXT("солнце"));
            }
        }
    }
}

// ---------------------------------------------------------------------------
//  Восприятие
//
//  Человек видит только то, что перед ним, и только если между ними нет
//  стены. Прежде достаточно было оказаться в радиусе — и люди замечали
//  друг друга через дом, затылком, из соседнего квартала.
// ---------------------------------------------------------------------------

bool ACompleteHumanNPC::HasLineOfSight(const AActor* Other) const
{
    UWorld* World = GetWorld();
    if (!World || !Other)
    {
        return false;
    }

    // Смотрим с высоты глаз в корпус собеседника, а не из пяток в пятки:
    // иначе обзор перекрывает каждый бордюр.
    const FVector Eye = GetActorLocation() + FVector(0.0f, 0.0f, 62.0f);
    const FVector Target = Other->GetActorLocation() + FVector(0.0f, 0.0f, 40.0f);

    FCollisionQueryParams Params(SCENE_QUERY_STAT(HumanSight), false, this);
    Params.AddIgnoredActor(Other);

    FHitResult Hit;
    const bool bBlocked = World->LineTraceSingleByChannel(Hit, Eye, Target, ECC_Visibility, Params);

    return !bBlocked;
}

// ---------------------------------------------------------------------------
//  Тело: руки и поза
//
//  До сих пор человек проделывал всё стоя и с пустыми руками: книга
//  читалась сама собой, еда съедалась на ходу. Теперь у него есть руки,
//  которые чем-то заняты, и тело, которое сидит или лежит.
// ---------------------------------------------------------------------------

void ACompleteHumanNPC::SetPosture(EPosture NewPosture, const FVector& At)
{
    if (Posture == NewPosture)
    {
        return;
    }

    UCapsuleComponent* Capsule = GetCapsuleComponent();
    if (!Capsule)
    {
        return;
    }

    const float HeightScale = GetActorScale3D().Z;
    auto PlaceBody = [this](float Half)
    {
        const FVector Offset(MeshOffset.X, MeshOffset.Y, -Half);
        if (USkeletalMeshComponent* Skel = GetMesh())
        {
            Skel->SetRelativeLocationAndRotation(Offset, MeshRotation);
        }
        if (PoseBody)
        {
            PoseBody->SetRelativeLocationAndRotation(Offset, MeshRotation);
        }
    };

    if (NewPosture == EPosture::Standing)
    {
        const float Rise = 88.0f - Capsule->GetUnscaledCapsuleHalfHeight();
        Capsule->SetCapsuleHalfHeight(88.0f);
        PlaceBody(88.0f);

        if (Rise > 0.0f)
        {
            PostureTarget = GetActorLocation() + FVector(0.0f, 0.0f, Rise * HeightScale);
            bPostureMoving = true;
        }

        LeaveSeat();

        if (UCharacterMovementComponent* Move = GetCharacterMovement())
        {
            Move->SetMovementMode(MOVE_Walking);
        }
        Posture = NewPosture;
        return;
    }

    PostureOrigin = GetActorLocation();

    const float Half = (NewPosture == EPosture::Sitting) ? 60.0f : 40.0f;
    const float Drop = Capsule->GetUnscaledCapsuleHalfHeight() - Half;
    Capsule->SetCapsuleHalfHeight(Half);
    PlaceBody(Half);

    TakeSeat();

    FVector Spot = At.IsZero() ? GetActorLocation() : At;
    Spot.Z = GetActorLocation().Z - Drop * HeightScale;
    PostureTarget = Spot;
    bPostureMoving = true;

    StopMoving();
    if (UCharacterMovementComponent* Move = GetCharacterMovement())
    {
        Move->StopMovementImmediately();
        Move->DisableMovement();
    }

    Posture = NewPosture;
}

bool ACompleteHumanNPC::Wear(AClothingActor* Garment)
{
    if (!Garment || Garment->WornBy.IsValid())
    {
        return false;
    }

    for (AClothingActor* Already : Clothes)
    {
        if (Already && Already->Kind == Garment->Kind)
        {
            return false;
        }
    }

    if (!Garment->PutOn(this))
    {
        return false;
    }

    Clothes.Add(Garment);
    return true;
}

void ACompleteHumanNPC::Undress(AClothingActor* Garment)
{
    if (!Garment)
    {
        return;
    }
    Garment->TakeOff();
    Clothes.Remove(Garment);
}

float ACompleteHumanNPC::ClothingWarmth() const
{
    float Total = 0.0f;
    for (const AClothingActor* Garment : Clothes)
    {
        if (Garment)
        {
            Total += Garment->Warmth;
        }
    }
    return FMath::Clamp(Total, 0.0f, 1.0f);
}

float ACompleteHumanNPC::ClothingDirt() const
{
    if (Clothes.Num() == 0)
    {
        return 0.0f;
    }

    float Total = 0.0f;
    int32 Count = 0;
    for (const AClothingActor* Garment : Clothes)
    {
        if (Garment)
        {
            Total += Garment->Dirt;
            ++Count;
        }
    }
    return Count > 0 ? Total / Count : 0.0f;
}

void ACompleteHumanNPC::AdvanceClothes(float GameDelta, bool bWorking)
{
    for (int32 i = Clothes.Num() - 1; i >= 0; --i)
    {
        AClothingActor* Garment = Clothes[i];
        if (!Garment)
        {
            Clothes.RemoveAt(i);
            continue;
        }
        Garment->Advance(GameDelta, bWorking);
    }
}

void ACompleteHumanNPC::TakeSeat()
{
    UWorld* World = GetWorld();
    if (!World || SeatedOn.IsValid())
    {
        return;
    }

    AFurnitureActor* Nearest = nullptr;
    float Best = 220.0f;
    for (TActorIterator<AFurnitureActor> It(World); It; ++It)
    {
        AFurnitureActor* Thing = *It;
        if (!Thing)
        {
            continue;
        }
        const float Distance = FVector::Dist2D(Thing->GetActorLocation(), GetActorLocation());
        if (Distance < Best)
        {
            Best = Distance;
            Nearest = Thing;
        }
    }

    if (!Nearest)
    {
        return;
    }

    SeatedOn = Nearest;
    if (UCapsuleComponent* Capsule = GetCapsuleComponent())
    {
        Capsule->IgnoreActorWhenMoving(Nearest, true);
    }
    if (Nearest->Body)
    {
        Nearest->Body->IgnoreActorWhenMoving(this, true);
    }
}

void ACompleteHumanNPC::LeaveSeat()
{
    AFurnitureActor* Thing = SeatedOn.Get();
    SeatedOn = nullptr;
    if (!Thing)
    {
        return;
    }

    if (UCapsuleComponent* Capsule = GetCapsuleComponent())
    {
        Capsule->IgnoreActorWhenMoving(Thing, false);
    }
    if (Thing->Body)
    {
        Thing->Body->IgnoreActorWhenMoving(this, false);
    }
}

void ACompleteHumanNPC::MovePosture(float DeltaSeconds)
{
    if (!bPostureMoving)
    {
        return;
    }

    const FVector Here = GetActorLocation();
    FVector Step = PostureTarget - Here;
    const float Distance = Step.Size();

    if (Distance < 1.5f || DeltaSeconds <= 0.0f)
    {
        bPostureMoving = false;
        return;
    }

    const float Speed = FMath::Max(45.0f, Distance * 4.0f);
    const float Travel = FMath::Min(Distance, Speed * DeltaSeconds);
    SetActorLocation(Here + Step.GetSafeNormal() * Travel, true);
}

bool ACompleteHumanNPC::TakeIntoHands(AActor* Item)
{
    if (!Item)
    {
        return false;
    }
    if (CarriedItem == Item)
    {
        return true;
    }
    if (CarriedItem)
    {
        LetGo(false);
    }

    if (ABookActor* Book = Cast<ABookActor>(Item))
    {
        if (!Book->PickUp(this))
        {
            return false;
        }
    }
    else if (AFurnitureActor* Thing = Cast<AFurnitureActor>(Item))
    {
        if (!Thing->PickedUp(this))
        {
            return false;
        }
    }
    else if (AResourceActor* Resource = Cast<AResourceActor>(Item))
    {
        if (!Resource->PickUp(this))
        {
            return false;
        }
    }
    else
    {
        Item->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
    }

    CarriedItem = Item;
    return true;
}

bool ACompleteHumanNPC::BeginTake(AActor* Item)
{
    if (!Item)
    {
        return false;
    }
    if (CarriedItem == Item)
    {
        return true;
    }
    if (Motor && Motor->IsReady() && PoseBody && PoseBody->IsVisible() && IsAlive())
    {
        return Motor->BeginTake(Item);
    }
    return TakeIntoHands(Item);
}

bool ACompleteHumanNPC::AreHandsBusy() const
{
    return Motor && Motor->IsHandling();
}

float ACompleteHumanNPC::BodyMassKg() const
{
    if (!IdentityComponent)
    {
        return 70.0f;
    }
    const float Age = IdentityComponent->Age;
    if (Age < 18.0f)
    {
        return FMath::Clamp(3.5f + Age * 3.4f, 3.5f, 65.0f);
    }
    return IdentityComponent->bFemale ? 62.0f : 76.0f;
}

void ACompleteHumanNPC::ReleaseFromHands(bool bReturnToPlace)
{
    if (!CarriedItem)
    {
        return;
    }
    if (Motor && Motor->IsReady() && PoseBody && PoseBody->IsVisible() && IsAlive())
    {
        if (Motor->BeginPut(bReturnToPlace))
        {
            return;
        }
    }
    LetGo(bReturnToPlace);
}

void ACompleteHumanNPC::LetGo(bool bReturnToPlace)
{
    if (!CarriedItem)
    {
        return;
    }

    if (ABookActor* Book = Cast<ABookActor>(CarriedItem.Get()))
    {
        if (bReturnToPlace)
        {
            Book->ReturnToShelf();
        }
        else
        {
            Book->PutDown();
        }
    }
    else if (AFurnitureActor* Thing = Cast<AFurnitureActor>(CarriedItem.Get()))
    {
        Thing->PutDown();
    }
    else if (AResourceActor* Resource = Cast<AResourceActor>(CarriedItem.Get()))
    {
        Resource->PutDown();
    }
    else
    {
        CarriedItem->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    }

    CarriedItem = nullptr;
    if (Motor)
    {
        Motor->ForgetHeld();
    }
}

void ACompleteHumanNPC::UpdateHandsMovement(float DeltaSeconds)
{
    if (!Motor || Posture != EPosture::Standing || !IsAlive())
    {
        return;
    }
    FVector Point;
    float Pace = 0.0f;
    const bool bApproach = Motor->WantsApproach(Point, Pace);
    if (!bApproach && !Motor->WantsFacing(Point))
    {
        return;
    }
    FVector To = Point - GetActorLocation();
    To.Z = 0.0f;
    const float Dist = To.Size();
    if (Dist < 1.0f)
    {
        return;
    }
    if (bApproach && Dist > 48.0f)
    {
        AddMovementInput(To / Dist, Pace * FMath::Clamp((Dist - 30.0f) / 55.0f, 0.3f, 1.0f));
    }
    if (!BodyLeadsFeet())
    {
        const FRotator Face(0.0f, To.Rotation().Yaw, 0.0f);
        SetActorRotation(FMath::RInterpTo(GetActorRotation(), Face, DeltaSeconds, 5.0f));
    }
}

void ACompleteHumanNPC::UpdateFooting(float DeltaSeconds)
{
    UCharacterMovementComponent* Move = GetCharacterMovement();
    if (!Move)
    {
        return;
    }
    UMatterSubsystem* MatterWorld = UMatterSubsystem::Get(this);
    ATerrainGrid* Ground = MatterWorld ? MatterWorld->GetTerrain() : nullptr;
    FTerrainFooting Footing;
    const bool bOnGround = Ground && Move->IsMovingOnGround() && Move->CurrentFloor.HitResult.GetActor() == Ground;
    float Carried = 0.0f;
    if (const AResourceActor* Held = Cast<AResourceActor>(CarriedItem.Get()))
    {
        Carried = Held->Matter ? Held->Matter->TotalMassKg() : 1.0f;
    }
    else if (const AFurnitureActor* Thing = Cast<AFurnitureActor>(CarriedItem.Get()))
    {
        Carried = Thing->MassKg;
    }
    const float Mass = BodyMassKg();

    if (!bOnGround || !Ground->FootingAt(GetActorLocation(), Mass + Carried, Footing))
    {
        Move->GroundFriction = 8.0f;
        Move->BrakingDecelerationWalking = 2048.0f;
        Move->MaxAcceleration = 2048.0f;
        GroundEffort = 1.0f + Carried / FMath::Max(10.0f, Mass) * 0.8f;
        if (Motor)
        {
            Motor->SetGround(0.0f, 0.8f, GroundEffort);
        }
        bWasInMud = false;
        return;
    }

    const float Load = 1.0f + Carried / FMath::Max(10.0f, Mass) * 0.8f;
    GroundEffort = Footing.Effort * Load;
    const bool bFeetFeelIt = BodyLeadsFeet();
    if (!bFeetFeelIt)
    {
        const float Hold = FMath::Clamp(Footing.Grip / 0.6f, 0.05f, 1.5f);
        Move->MaxWalkSpeed *= FMath::Pow(1.0f / GroundEffort, 0.8f);
        if (Footing.WaterCm > 60.0f)
        {
            Move->MaxWalkSpeed *= FMath::Clamp(1.0f - (Footing.WaterCm - 60.0f) / 60.0f, 0.15f, 1.0f);
        }
        Move->GroundFriction = 8.0f * Hold;
        Move->BrakingDecelerationWalking = 2048.0f * Hold;
        Move->MaxAcceleration = 2048.0f * FMath::Clamp(Hold, 0.2f, 1.0f) / (1.0f + Footing.Stick * 1.5f + Footing.SinkCm * 0.04f);
        if (Motor)
        {
            Motor->SetGround(Footing.SinkCm, Footing.Grip, GroundEffort);
        }
    }

    const float Speed = GetVelocity().Size2D();
    if (Speed < 20.0f)
    {
        return;
    }

    if (!bFeetFeelIt)
    {
        StrideTravel += Speed * DeltaSeconds;
        const float Stride = FMath::Lerp(72.0f, 48.0f, FMath::Clamp(Footing.SinkCm / 15.0f + Footing.Stick * 0.4f, 0.0f, 1.0f));
        if (StrideTravel >= Stride)
        {
            StrideTravel = 0.0f;
            bLeftFootNext = !bLeftFootNext;
            const float Half = GetSimpleCollisionHalfHeight();
            const FVector Foot = GetActorLocation() - FVector(0.0f, 0.0f, Half) + GetActorRightVector() * (bLeftFootNext ? -11.0f : 11.0f);
            Ground->Trample(Foot, Mass + Carried);
            if (Footing.bSoft && Footing.WaterCm < 25.0f)
            {
                Ground->Footprint(Foot, GetActorRotation().Yaw, Footing.SinkCm, Footing.Stick, bLeftFootNext);
            }
        }
    }

    FootingFeelTimer -= DeltaSeconds;
    const bool bHard = Footing.SinkCm > 3.0f || Footing.Stick > 0.45f || Footing.Grip < 0.25f || Footing.WaterCm > 30.0f;
    if (bHard && (!bWasInMud || FootingFeelTimer <= 0.0f) && Mind)
    {
        FootingFeelTimer = 25.0f;
        Mind->FeelFooting(Footing.SinkCm, Footing.Stick, Footing.Grip, GroundEffort, Footing.WaterCm, GetActorLocation());
    }
    bWasInMud = bHard;
}

bool ACompleteHumanNPC::CanReachPoint(const FVector& Point) const
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return true;
    }

    // Луч на уровне пояса: порог и ступеньку человек переступит, стену — нет.
    const FVector From = GetActorLocation() + FVector(0.0f, 0.0f, 30.0f);
    const FVector To = FVector(Point.X, Point.Y, From.Z);

    FCollisionQueryParams Params(SCENE_QUERY_STAT(HumanReach), false, this);
    FHitResult Hit;

    if (!World->LineTraceSingleByChannel(Hit, From, To, ECC_WorldStatic, Params))
    {
        return true;
    }

    return FVector::Dist2D(Hit.ImpactPoint, To) < 170.0f;
}

void ACompleteHumanNPC::MoveBlockedBy(const FHitResult& Impact)
{
    Super::MoveBlockedBy(Impact);

    AFurnitureActor* Thing = Cast<AFurnitureActor>(Impact.GetActor());
    if (!Thing || Thing->bFixture)
    {
        return;
    }

    FVector Push = GetVelocity();
    Push.Z = 0.0f;
    if (Push.IsNearlyZero())
    {
        Push = GetActorForwardVector();
    }

    const float Strength = PhysiologyComponent ? FMath::Clamp(PhysiologyComponent->Body.Stamina, 0.2f, 1.0f) : 1.0f;
    if (Thing->Shove(Push, 2.2f * Strength) && Mind)
    {
        Mind->Practised(TEXT("Strength"), 0.004f);
    }
}

bool ACompleteHumanNPC::CanPerceive(const AActor* Other, float& OutSalience, FName& OutSense) const
{
    OutSalience = 0.0f;
    OutSense = TEXT("Sight");

    if (!Other || Other == this)
    {
        return false;
    }

    const FVector ToOther = Other->GetActorLocation() - GetActorLocation();
    const float Distance = ToOther.Size();
    if (Distance < KINDA_SMALL_NUMBER || Distance > SightRadius)
    {
        return false;
    }

    // --- Слух: за стеной человека не видно, но слышно ----------------------
    // Звук огибает препятствия и не требует смотреть в нужную сторону.
    // Поэтому в соседней комнате чужое присутствие всё-таки замечаешь —
    // но смутно, и подробностей не разглядишь.
    const bool bWithinEarshot = (Distance < HearingRadius);

    // --- Зрение -------------------------------------------------------------
    const float Dot = FVector::DotProduct(ToOther / Distance, GetActorForwardVector());
    const float CosHalfAngle = FMath::Cos(FMath::DegreesToRadians(SightHalfAngle));

    // Боковым зрением замечают движение, но только вблизи.
    const bool bInView = (Dot >= CosHalfAngle) || (Distance < 300.0f && Dot > -0.2f);

    if (bInView && HasLineOfSight(Other))
    {
        OutSense = TEXT("Sight");
        // Близкое заметнее далёкого; то, что прямо перед носом, — заметнее всего.
        const float Nearness = FMath::Clamp(1.0f - Distance / SightRadius, 0.05f, 1.0f);
        const float Directness = FMath::Clamp((Dot - CosHalfAngle) / FMath::Max(0.01f, 1.0f - CosHalfAngle), 0.0f, 1.0f);
        OutSalience = FMath::Clamp(Nearness * (0.45f + Directness * 0.35f), 0.03f, 1.0f);
        return true;
    }

    if (bWithinEarshot)
    {
        OutSense = TEXT("Sound");
        // Услышанное занимает внимание слабее увиденного.
        OutSalience = FMath::Clamp((1.0f - Distance / HearingRadius) * 0.30f, 0.02f, 0.4f);
        return true;
    }

    return false;
}

void ACompleteHumanNPC::DiscoverPlacesNearby()
{
    if (!MemoryComponent)
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    UHumanWorldSubsystem* WorldMind = World->GetSubsystem<UHumanWorldSubsystem>();
    if (!WorldMind)
    {
        return;
    }

    // Город не выдаёт человеку карту — он замечает только то, мимо чего прошёл.
    const TArray<FKnownLocation> Seen = WorldMind->GetPlacesNear(GetActorLocation(), 1600.0f);
    for (const FKnownLocation& P : Seen)
    {
        const bool bWasUnknown = !MemoryComponent->KnowsPlace(P.Kind, P.Location);

        if (SpeechComponent)
        {
            switch (P.Kind)
            {
            case EPlaceKind::Home:
                SpeechComponent->NoticeThing(TEXT("дом"));
                SpeechComponent->NoticeThing(TEXT("подъезд"));
                SpeechComponent->NoticeThing(TEXT("дверь"));
                break;
            case EPlaceKind::Study:     SpeechComponent->NoticeThing(TEXT("школа")); break;
            case EPlaceKind::Library:   SpeechComponent->NoticeThing(TEXT("библиотека")); break;
            case EPlaceKind::Market:    SpeechComponent->NoticeThing(TEXT("рынок")); break;
            case EPlaceKind::Bathhouse: SpeechComponent->NoticeThing(TEXT("баня")); break;
            case EPlaceKind::Bakery:    SpeechComponent->NoticeThing(TEXT("пекарня")); break;
            case EPlaceKind::Food:      SpeechComponent->NoticeThing(TEXT("кафе")); break;
            case EPlaceKind::Rest:
            case EPlaceKind::Beautiful:
                SpeechComponent->NoticeThing(TEXT("сквер"));
                SpeechComponent->NoticeThing(TEXT("дерево"));
                SpeechComponent->NoticeThing(TEXT("трава"));
                break;
            default: break;
            }
        }

        // Заодно разглядываем, что там вообще можно делать — всё,
        // а не первую попавшуюся вещь.
        TArray<FAffordance> Offers;
        WorldMind->CollectOffersNear(P.Location, 1500.0f, Offers);

        MemoryComponent->LearnPlace(P.Kind, P.Location, P.Label, true, nullptr,
                                    WorldMind->WorldSeconds, Offers.Num() > 0 ? &Offers : nullptr);

        // Новое место — маленькое открытие.
        if (bWasUnknown && Mind && EmotionComponent)
        {
            EmotionComponent->Trigger(EEmotionType::Curiosity, 0.15f, nullptr,
                FString::Printf(TEXT("нашёл(ла): %s"), *P.Label));
            Mind->Think(FString::Printf(TEXT("а, тут %s. Запомню."), *P.Label),
                EThoughtKind::Observation, 0.45f);
            if (NeedComponent)
            {
                NeedComponent->Satisfy(ENeedType::Novelty, 0.12f);
            }
        }
    }
}

void ACompleteHumanNPC::UpdateBodyTint()
{
    if (!bTintBodyByMood || !PlaceholderBody || !EmotionComponent)
    {
        return;
    }

    if (!BodyMaterial)
    {
        if (UMaterialInterface* Base = PlaceholderBody->GetMaterial(0))
        {
            BodyMaterial = UMaterialInstanceDynamic::Create(Base, this);
            PlaceholderBody->SetMaterial(0, BodyMaterial);
        }
        if (!BodyMaterial)
        {
            return;
        }
    }

    const FAffectPAD Affect = EmotionComponent->GetAffect();
    const bool bSleeping = Mind && Mind->bAsleep;

    // Тёплый — хорошо, холодный — плохо, яркий — на взводе.
    FLinearColor Color(
        FMath::Clamp(0.45f - Affect.Pleasure * 0.35f + Affect.Arousal * 0.25f, 0.05f, 1.0f),
        FMath::Clamp(0.45f + Affect.Pleasure * 0.40f, 0.05f, 1.0f),
        FMath::Clamp(0.50f + Affect.Pleasure * 0.10f - Affect.Arousal * 0.25f, 0.05f, 1.0f));

    if (bSleeping)
    {
        Color *= 0.35f;
    }
    if (!IsAlive())
    {
        Color = FLinearColor(0.08f, 0.08f, 0.08f);
    }

    // Имена параметров различаются между материалами — задаём оба.
    BodyMaterial->SetVectorParameterValue(TEXT("Color"), Color);
    BodyMaterial->SetVectorParameterValue(TEXT("BaseColor"), Color);
}

void ACompleteHumanNPC::PerceiveWorldEvent(const FWorldEvent& Event)
{
    if (Mind)
    {
        Mind->OnWorldEvent(Event);
    }
}

// ---------------------------------------------------------------------------
//  Внешние воздействия
// ---------------------------------------------------------------------------

void ACompleteHumanNPC::Hurt(AActor* By, float Severity, bool bIntentional)
{
    if (!IsAlive())
    {
        return;
    }

    if (Mind)
    {
        Mind->OnHurt(By, Severity, bIntentional);
    }

    // Такое видят все вокруг — и делают свои выводы.
    if (UWorld* World = GetWorld())
    {
        if (UHumanWorldSubsystem* WorldMind = World->GetSubsystem<UHumanWorldSubsystem>())
        {
            FWorldEvent Event;
            Event.Tag = TEXT("Violence");
            Event.Description = FString::Printf(TEXT("на %s напали"), *GetPersonName());
            Event.Location = GetActorLocation();
            Event.Radius = 2500.0f;
            Event.Instigator = By;
            Event.Target = this;
            Event.Valence = -0.8f;
            Event.Significance = FMath::Clamp(0.5f + Severity, 0.0f, 1.0f);
            Event.NormViolation = bIntentional ? -0.9f : -0.1f;
            WorldMind->BroadcastEvent(Event);
        }
    }
}

void ACompleteHumanNPC::Help(AActor* By, float Magnitude)
{
    if (Mind)
    {
        Mind->OnHelped(By, Magnitude);
    }
}

void ACompleteHumanNPC::Revive(const FVector& At)
{
    if (PhysiologyComponent)
    {
        PhysiologyComponent->Revive();
    }
    if (NeedComponent)
    {
        for (FNeedState& Need : NeedComponent->Needs)
        {
            Need.Satisfaction = FMath::Max(Need.Satisfaction, 0.7f);
        }
        for (float& Time : NeedComponent->DeprivationTime)
        {
            Time = 0.0f;
        }
    }
    bDeathHandled = false;
    if (CarriedItem)
    {
        LetGo(true);
    }
    SetPosture(EPosture::Standing, FVector::ZeroVector);
    if (UCharacterMovementComponent* Move = GetCharacterMovement())
    {
        Move->SetMovementMode(MOVE_Walking);
        Move->StopMovementImmediately();
    }
    if (!At.IsZero())
    {
        SetActorLocation(At + FVector(0.0f, 0.0f, 100.0f), false, nullptr, ETeleportType::TeleportPhysics);
    }
    if (Mind)
    {
        Mind->Reborn();
    }
}

void ACompleteHumanNPC::OnDied()
{
    if (bDeathHandled)
    {
        return;
    }
    bDeathHandled = true;

    StopMoving();
    if (CarriedItem)
    {
        LetGo(false);
    }

    if (UCharacterMovementComponent* Move = GetCharacterMovement())
    {
        Move->DisableMovement();
    }

    // Мир узнаёт о смерти. Тех, кто был близок, это разрушает.
    if (UWorld* World = GetWorld())
    {
        if (UHumanWorldSubsystem* WorldMind = World->GetSubsystem<UHumanWorldSubsystem>())
        {
            FWorldEvent Event;
            Event.Tag = TEXT("Death");
            Event.Description = FString::Printf(TEXT("%s умер(ла): %s"),
                *GetPersonName(),
                PhysiologyComponent ? *PhysiologyComponent->CauseOfDeath : TEXT("неизвестно"));
            Event.Location = GetActorLocation();
            Event.Radius = 4000.0f;
            Event.Target = this;
            Event.Valence = -1.0f;
            Event.Significance = 1.0f;
            WorldMind->BroadcastEvent(Event);

            // Близкие теряют часть себя — независимо от того, были ли рядом.
            for (ACompleteHumanNPC* Other : WorldMind->GetAllHumans())
            {
                if (!Other || Other == this)
                {
                    continue;
                }
                USocialComponent* TheirSocial = Other->SocialComponent;
                if (!TheirSocial)
                {
                    continue;
                }

                const float Closeness = TheirSocial->GetCloseness(this);
                if (Closeness < 0.25f)
                {
                    continue;
                }

                const FRelationship* R = TheirSocial->Find(this);
                const float Attachment = R ? R->Attachment : 0.0f;
                const float Liking = R ? R->Liking : 0.0f;
                const float Resentment = R ? R->Resentment : 0.0f;

                TheirSocial->OnDeathOf(this, WorldMind->WorldSeconds);

                // Горе НЕ присваивается. Событие оценивается — и чувства
                // рождаются из того, чем этот человек для меня был.
                // Если я его не любил, скорби не будет. Если был должен —
                // придёт вина. Если ненавидел — может прийти стыд за
                // облегчение. Всё это следует из отношений, а не из правила.
                if (UEmotionComponent* TheirEmotions = Other->EmotionComponent)
                {
                    FAppraisedEvent Loss;
                    Loss.Tag = TEXT("Loss");
                    Loss.Description = FString::Printf(TEXT("%s больше нет"), *GetPersonName());
                    Loss.Subject = this;
                    Loss.Location = GetActorLocation();

                    // Насколько это плохо ДЛЯ НЕГО — ровно настолько,
                    // насколько он был к этому человеку привязан.
                    Loss.Desirability = -FMath::Clamp(Attachment * 0.7f + FMath::Max(0.0f, Liking) * 0.5f, 0.0f, 1.0f);

                    // Смерть всегда неожиданна и всегда неподвластна.
                    Loss.Unexpectedness = 0.9f;
                    Loss.Controllability = 0.0f;
                    Loss.Certainty = 1.0f;

                    // Никто не виноват — и это делает горе горем, а не гневом.
                    Loss.SelfAgency = 0.0f;
                    Loss.OtherAgency = 0.0f;

                    Loss.Significance = FMath::Clamp(0.3f + Attachment, 0.0f, 1.0f);

                    TheirEmotions->Appraise(Loss, Other->PersonalityComponent);

                    // Невысказанная обида на мёртвого превращается в вину:
                    // теперь уже ничего не исправить.
                    if (Resentment > 0.3f)
                    {
                        FAppraisedEvent TooLate;
                        TooLate.Tag = TEXT("ExpectationBroken");
                        TooLate.Description = FString::Printf(TEXT("мы так и не поговорили — %s"), *GetPersonName());
                        TooLate.Subject = this;
                        TooLate.Desirability = -Resentment * 0.6f;
                        TooLate.SelfAgency = 0.7f;
                        TooLate.Controllability = 0.0f;
                        TooLate.Significance = Resentment;
                        TheirEmotions->Appraise(TooLate, Other->PersonalityComponent);
                    }
                }
                if (UIdentityComponent* TheirIdentity = Other->IdentityComponent)
                {
                    TheirIdentity->RecordLifeEvent(
                        FString::Printf(TEXT("потерял(а) близкого человека — %s"), *GetPersonName()),
                        -1.0f, Closeness, WorldMind->WorldSeconds);
                    TheirIdentity->DeathAwareness = FMath::Clamp(TheirIdentity->DeathAwareness + 0.2f, 0.0f, 1.0f);
                }
                if (UMemoryComponent* TheirMemory = Other->MemoryComponent)
                {
                    TArray<AActor*> Participants = { this };
                    TheirMemory->Encode(FString::Printf(TEXT("%s умер(ла)"), *GetPersonName()),
                        TEXT("Death"), -1.0f, 1.0f, Participants,
                        GetActorLocation(), WorldMind->WorldSeconds, 1.0f, EMemoryKind::Flashbulb);
                }
                if (UPersonalityComponent* TheirPersonality = Other->PersonalityComponent)
                {
                    TheirPersonality->ApplyTrauma(Closeness * 0.8f,
                        Other->IdentityComponent ? Other->IdentityComponent->Age : 30.0f);
                }
                // И теперь им предстоит это как-то пережить.
                if (Other->Mind)
                {
                    Other->Mind->Grieve(this, Closeness);
                }
            }

            WorldMind->UnregisterHuman(this);
        }
    }
}

bool ACompleteHumanNPC::IsAlive() const
{
    return !PhysiologyComponent || PhysiologyComponent->bAlive;
}

FString ACompleteHumanNPC::GetPersonName() const
{
    return IdentityComponent ? IdentityComponent->GetFullName() : GetName();
}

FString ACompleteHumanNPC::GetStatusReport() const
{
    return Mind ? Mind->GetStatusReport() : FString();
}

FString ACompleteHumanNPC::GetCurrentThought() const
{
    return Mind ? Mind->CurrentThought : FString();
}

// ---------------------------------------------------------------------------
//  Внешность
// ---------------------------------------------------------------------------

void ACompleteHumanNPC::UpdateGrowth()
{
    const float Age = IdentityComponent ? IdentityComponent->Age : 30.0f;
    const float ForAge = FVillage::IsMedieval(this) ? FVillage::HeightScaleForAge(Age)
        : FMath::Lerp(0.8f, 1.0f, FMath::Clamp(Age / 18.0f, 0.0f, 1.0f));
    const float HeightScale = StatureFactor * ForAge;
    if (GrownScale > 0.0f && FMath::Abs(HeightScale - GrownScale) < 0.004f)
    {
        return;
    }
    GrownScale = HeightScale;
    SetActorScale3D(FVector(1.0f, 1.0f, HeightScale));
    if (PoseBody)
    {
        PoseBody->SetRelativeScale3D(FVector(HeightScale, HeightScale, 1.0f));
    }
}

void ACompleteHumanNPC::GenerateRandomAppearance()
{
    StatureFactor = FMath::FRandRange(0.92f, 1.08f);
    if (IdentityComponent)
    {
        if (IdentityComponent->bFemale)
        {
            StatureFactor *= 0.96f;
        }
        if (IdentityComponent->Age > 70.0f)
        {
            StatureFactor *= 0.97f;
        }
    }
    GrownScale = -1.0f;
    UpdateGrowth();

    UMeshComponent* Visible = (PoseBody && PoseBody->IsVisible())
        ? static_cast<UMeshComponent*>(PoseBody)
        : static_cast<UMeshComponent*>(GetMesh());

    if (Visible && Visible->GetMaterial(0))
    {
        SkinMaterial = UMaterialInstanceDynamic::Create(Visible->GetMaterial(0), this);
        Visible->SetMaterial(0, SkinMaterial);

        const FLinearColor SkinColor(
            FMath::FRandRange(0.55f, 1.0f),
            FMath::FRandRange(0.38f, 0.78f),
            FMath::FRandRange(0.30f, 0.68f));
        SkinMaterial->SetVectorParameterValue(TEXT("BaseColor"), SkinColor);
    }
}

// ===========================================================================
//  Совместимость со старым интерфейсом
// ===========================================================================

void ACompleteHumanNPC::SyncLegacyMirror()
{
    // --- Эмоции -------------------------------------------------------------
    if (EmotionComponent)
    {
        Emotions.Joy      = EmotionComponent->GetIntensity(EEmotionType::Joy);
        Emotions.Fear     = EmotionComponent->GetIntensity(EEmotionType::Fear);
        Emotions.Sadness  = EmotionComponent->GetIntensity(EEmotionType::Sadness);
        Emotions.Anger    = EmotionComponent->GetIntensity(EEmotionType::Anger);
        Emotions.Boredom  = EmotionComponent->GetIntensity(EEmotionType::Boredom);
    }

    // --- Личность -----------------------------------------------------------
    if (PersonalityComponent)
    {
        Personality.Openness          = PersonalityComponent->Traits.Openness;
        Personality.Conscientiousness = PersonalityComponent->Traits.Conscientiousness;
        Personality.Extraversion      = PersonalityComponent->Traits.Extraversion;
        Personality.Agreeableness     = PersonalityComponent->Traits.Agreeableness;
        Personality.Neuroticism       = PersonalityComponent->Traits.Neuroticism;
    }

    // --- Тело ---------------------------------------------------------------
    if (PhysiologyComponent)
    {
        Hunger = (1.0f - PhysiologyComponent->Body.StomachFullness) * 100.0f;
        Energy = PhysiologyComponent->Body.Stamina * 100.0f;
        Emotions.Tiredness = 1.0f - PhysiologyComponent->Body.Stamina;
    }

    if (Mind)
    {
        bIsSleeping = Mind->bAsleep;
    }

    if (IdentityComponent)
    {
        CurrentJob = IdentityComponent->Occupation;
    }

    // --- Память -------------------------------------------------------------
    if (MemoryComponent)
    {
        Memories.Reset();
        const int32 Count = FMath::Min(20, MemoryComponent->Episodes.Num());
        for (int32 i = MemoryComponent->Episodes.Num() - Count; i < MemoryComponent->Episodes.Num(); ++i)
        {
            const FEpisodicMemory& M = MemoryComponent->Episodes[i];
            FMemoryEntry Entry;
            Entry.Event = M.Summary;
            Entry.EmotionalImpact = M.Valence;
            Entry.Timestamp = FDateTime::Now();
            Memories.Add(Entry);
        }

        KnownPlaces.Reset();
        for (const FKnownLocation& P : MemoryComponent->Places)
        {
            FKnownPlace Legacy;
            Legacy.Type = HumanText::Place(P.Kind);
            Legacy.Location = P.Location;
            Legacy.Familiarity = P.Familiarity;
            Legacy.Source = P.bFirsthand ? TEXT("Visited") : TEXT("Told");
            KnownPlaces.Add(Legacy);
        }
    }

    // --- Отношения ----------------------------------------------------------
    if (SocialComponent)
    {
        SocialRelationList.Reset();
        for (const TPair<TObjectPtr<AActor>, FRelationship>& Pair : SocialComponent->Relations)
        {
            if (!Pair.Key)
            {
                continue;
            }
            FSocialRelation Legacy;
            Legacy.Other = Pair.Key;
            Legacy.Opinion = Pair.Value.Liking * 100.0f;
            Legacy.Trust = Pair.Value.Trust;
            SocialRelationList.Add(Legacy);
        }
    }
}

void ACompleteHumanNPC::ChooseJob()
{
    // Профессию теперь выбирает личность, а не случай. Но если кто-то
    // вызывает это снаружи — подтверждаем и синхронизируем.
    if (IdentityComponent)
    {
        CurrentJob = IdentityComponent->Occupation;
    }
}

void ACompleteHumanNPC::AddMemory(const FString& Event, float Impact)
{
    if (!MemoryComponent)
    {
        return;
    }

    float WorldTime = 0.0f;
    if (UWorld* World = GetWorld())
    {
        if (UHumanWorldSubsystem* WorldMind = World->GetSubsystem<UHumanWorldSubsystem>())
        {
            WorldTime = WorldMind->WorldSeconds;
        }
    }

    MemoryComponent->EncodeSimple(Event, Impact, WorldTime);

    if (EmotionComponent && FMath::Abs(Impact) > 0.1f)
    {
        EmotionComponent->Trigger(Impact > 0.0f ? EEmotionType::Joy : EEmotionType::Sadness,
            FMath::Abs(Impact) * 0.4f, nullptr, Event);
    }
}

void ACompleteHumanNPC::UpdateSocialRelation(AActor* Other, float DeltaOpinion)
{
    if (!SocialComponent || !Other)
    {
        return;
    }

    float WorldTime = 0.0f;
    if (UWorld* World = GetWorld())
    {
        if (UHumanWorldSubsystem* WorldMind = World->GetSubsystem<UHumanWorldSubsystem>())
        {
            WorldTime = WorldMind->WorldSeconds;
        }
    }

    SocialComponent->RecordInteraction(Other, FMath::Clamp(DeltaOpinion / 20.0f, -1.0f, 1.0f), WorldTime, PersonalityComponent);
}

void ACompleteHumanNPC::AddKnownPlace(const FString& Type, const FVector& Location, const FString& Source)
{
    if (!MemoryComponent)
    {
        return;
    }

    // Переводим старые строковые типы в новые.
    EPlaceKind Kind = EPlaceKind::Landmark;
    if (Type.Contains(TEXT("Cafe")) || Type.Contains(TEXT("Food")))  Kind = EPlaceKind::Food;
    else if (Type.Contains(TEXT("Home")))                            Kind = EPlaceKind::Home;
    else if (Type.Contains(TEXT("Work")) || Type.Contains(TEXT("Office"))) Kind = EPlaceKind::Work;
    else if (Type.Contains(TEXT("Shop")) || Type.Contains(TEXT("Commercial"))) Kind = EPlaceKind::Shop;
    else if (Type.Contains(TEXT("Danger")))                          Kind = EPlaceKind::Danger;

    float WorldTime = 0.0f;
    if (UWorld* World = GetWorld())
    {
        if (UHumanWorldSubsystem* WorldMind = World->GetSubsystem<UHumanWorldSubsystem>())
        {
            WorldTime = WorldMind->WorldSeconds;
        }
    }

    MemoryComponent->LearnPlace(Kind, Location, Type, Source != TEXT("Told"), nullptr, WorldTime);
}

FVector ACompleteHumanNPC::FindNearestKnownPlace(const FString& Type)
{
    if (!MemoryComponent)
    {
        return GetActorLocation();
    }

    EPlaceKind Kind = EPlaceKind::Landmark;
    if (Type.Contains(TEXT("Cafe")) || Type.Contains(TEXT("Food")))  Kind = EPlaceKind::Food;
    else if (Type.Contains(TEXT("Home")))                            Kind = EPlaceKind::Home;
    else if (Type.Contains(TEXT("Work")) || Type.Contains(TEXT("Office"))) Kind = EPlaceKind::Work;
    else if (Type.Contains(TEXT("Shop")) || Type.Contains(TEXT("Commercial"))) Kind = EPlaceKind::Shop;

    FVector Found;
    if (MemoryComponent->FindNearestPlace(Kind, GetActorLocation(), Found))
    {
        return Found;
    }
    return GetActorLocation();
}

void ACompleteHumanNPC::GenerateQualia(const FString& Type, float Intensity, const FString& Context)
{
    FQualia Q;
    Q.Type = Type;
    Q.Intensity = Intensity;
    Q.Description = Type + TEXT(": ") + Context;
    QualiaList.Add(Q);
    while (QualiaList.Num() > 32)
    {
        QualiaList.RemoveAt(0);
    }

    if (!EmotionComponent)
    {
        return;
    }

    if (Type.Contains(TEXT("Боль")))
    {
        if (PhysiologyComponent)
        {
            PhysiologyComponent->Body.Pain = FMath::Clamp(PhysiologyComponent->Body.Pain + Intensity * 0.5f, 0.0f, 1.0f);
        }
        EmotionComponent->Trigger(EEmotionType::Fear, Intensity * 0.3f, nullptr, Context);
    }
    else if (Type.Contains(TEXT("Радость")))
    {
        EmotionComponent->Trigger(EEmotionType::Joy, Intensity * 0.5f, nullptr, Context);
    }
}

void ACompleteHumanNPC::Sleep()
{
    if (Mind)
    {
        Mind->ForceSleep();
    }
}

void ACompleteHumanNPC::WakeUp()
{
    if (Mind)
    {
        Mind->ForceWake();
    }
}

void ACompleteHumanNPC::GoHome()
{
    if (HasHome())
    {
        RequestMoveTo(Home.Location);
    }
}
