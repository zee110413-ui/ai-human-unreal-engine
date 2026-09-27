// HumanSandboxGameMode.cpp

#include "HumanSandboxGameMode.h"
#include "Textbook.h"
#include "Crafts.h"
#include "Elements.h"
#include "BookActor.h"
#include "ResourceActor.h"
#include "Matter.h"
#include "MatterComponent.h"
#include "MatterSubsystem.h"
#include "MatterStructure.h"
#include "TerrainGrid.h"
#include "CityGenerator.h"
#include "CompleteHumanAI.h"
#include "HumanWorldSubsystem.h"
#include "MindComponent.h"
#include "IdentityComponent.h"
#include "EmotionComponent.h"
#include "SocialComponent.h"
#include "SpeechComponent.h"
#include "Dialogue.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpectatorPawn.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "EngineUtils.h"
#include "UnrealClient.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "HAL/FileManager.h"
#include "Components/InputComponent.h"
#include "InputCoreTypes.h"
#include "GameFramework/Pawn.h"
#include "BodyMotorComponent.h"
#include "VideoRecorder.h"

void AHumanSandboxGameMode::Speed(float Factor)
{
    GameSpeed = FMath::Clamp(Factor, 0.25f, 16.0f);
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }
    UGameplayStatics::SetGlobalTimeDilation(World, GameSpeed);
    if (APlayerController* PC = World->GetFirstPlayerController())
    {
        if (APawn* Pawn = PC->GetPawn())
        {
            Pawn->CustomTimeDilation = 1.0f / GameSpeed;
        }
    }
    UE_LOG(LogHumanCity, Warning, TEXT("Скорость игры: x%.2f"), GameSpeed);
}

void AHumanSandboxGameMode::SpeedUp()
{
    Speed(GameSpeed * 2.0f);
}

void AHumanSandboxGameMode::SlowDown()
{
    Speed(GameSpeed * 0.5f);
}

void AHumanSandboxGameMode::SpeedNormal()
{
    Speed(1.0f);
}

bool AHumanSandboxGameMode::TraceView(FHitResult& OutHit, FVector& OutFrom, FVector& OutDirection) const
{
    UWorld* World = GetWorld();
    APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
    if (!PC)
    {
        return false;
    }
    FRotator View;
    PC->GetPlayerViewPoint(OutFrom, View);
    OutDirection = View.Vector();
    FCollisionQueryParams Params(SCENE_QUERY_STAT(SandboxView), true, PC->GetPawn());
    return World->LineTraceSingleByChannel(OutHit, OutFrom, OutFrom + OutDirection * 6000.0f, ECC_WorldDynamic, Params);
}

void AHumanSandboxGameMode::Rain(float MillimetresPerHour, float Hours)
{
    if (UMatterSubsystem* Matter = UMatterSubsystem::Get(this))
    {
        Matter->ForceRain(MillimetresPerHour, Hours);
        UE_LOG(LogHumanCity, Warning, TEXT("Погода: %s"), *Matter->DescribeClimate());
    }
}

void AHumanSandboxGameMode::Frost(float Celsius, float Hours)
{
    if (UMatterSubsystem* Matter = UMatterSubsystem::Get(this))
    {
        Matter->ForceTemperature(Celsius, Hours);
        UE_LOG(LogHumanCity, Warning, TEXT("Погода: %s"), *Matter->DescribeClimate());
    }
}

void AHumanSandboxGameMode::Season(int32 DayOfYear)
{
    if (UMatterSubsystem* Matter = UMatterSubsystem::Get(this))
    {
        Matter->ForceSeason(DayOfYear);
        UE_LOG(LogHumanCity, Warning, TEXT("Погода: %s"), *Matter->DescribeClimate());
    }
}

void AHumanSandboxGameMode::WeatherNow()
{
    if (UMatterSubsystem* Matter = UMatterSubsystem::Get(this))
    {
        const FString Text = Matter->DescribeClimate();
        UE_LOG(LogHumanCity, Warning, TEXT("Погода: %s"), *Text);
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(1005, 8.0f, FColor(180, 220, 255), Text);
        }
    }
}

void AHumanSandboxGameMode::WhatIsThis()
{
    FHitResult Hit;
    FVector From;
    FVector Direction;
    UMatterSubsystem* Matter = UMatterSubsystem::Get(this);
    if (!Matter || !TraceView(Hit, From, Direction))
    {
        UE_LOG(LogHumanCity, Warning, TEXT("Перед глазами ничего нет"));
        return;
    }
    FString Text = Matter->DescribeAt(Hit.ImpactPoint, Hit.GetComponent(), Hit.Item);
    if (Text.IsEmpty())
    {
        Text = FString::Printf(TEXT("%s: вещество не задано"), *GetNameSafe(Hit.GetActor()));
    }
    UE_LOG(LogHumanCity, Warning, TEXT("Это %s"), *Text);
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(1005, 8.0f, FColor(255, 230, 160), Text);
    }
}

void AHumanSandboxGameMode::Strike(float Joules, int32 Edge)
{
    FHitResult Hit;
    FVector From;
    FVector Direction;
    if (!TraceView(Hit, From, Direction))
    {
        return;
    }
    AActor* Target = Hit.GetActor();
    if (AMatterStructure* House = Cast<AMatterStructure>(Target))
    {
        const int32 Piece = House->FindPiece(Hit.GetComponent(), Hit.Item);
        const FString Before = House->DescribePiece(Piece);
        House->ImpactPiece(Piece, FMath::Max(0.0f, Joules), Hit.ImpactPoint, Direction, nullptr, Edge != 0);
        UE_LOG(LogHumanCity, Warning, TEXT("Удар %.0f Дж%s по постройке. Было: %s. Стало: %s"), Joules,
            Edge != 0 ? TEXT(" лезвием") : TEXT(""), *Before, *House->DescribePiece(Piece));
        return;
    }
    if (ATerrainGrid* Ground = Cast<ATerrainGrid>(Target))
    {
        Ground->Impact(Hit.ImpactPoint, FMath::Max(0.0f, Joules));
        UE_LOG(LogHumanCity, Warning, TEXT("Удар %.0f Дж по земле: %s"), Joules, *Ground->DescribeAt(Hit.ImpactPoint));
        return;
    }
    UMatterComponent* Matter = Target ? Target->FindComponentByClass<UMatterComponent>() : nullptr;
    if (!Matter)
    {
        UE_LOG(LogHumanCity, Warning, TEXT("Удар пришёлся не по веществу: %s"), *GetNameSafe(Target));
        return;
    }
    const FString Before = Matter->Describe();
    Matter->ReceiveImpact(FMath::Max(0.0f, Joules), Hit.ImpactPoint, Direction, nullptr, Edge != 0);
    const bool bWhole = IsValid(Target) && !Target->IsActorBeingDestroyed();
    const FString After = bWhole ? Matter->Describe() : FString(TEXT("раскололось"));
    UE_LOG(LogHumanCity, Warning, TEXT("Удар %.0f Дж%s. Было: %s. Стало: %s"), Joules,
        Edge != 0 ? TEXT(" лезвием") : TEXT(""), *Before, *After);
}

AActor* AHumanSandboxGameMode::SpawnSample(EResourceKind Kind, float WetShare, const FVector& At, const FVector& Size, const FRotator& Turn)
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return nullptr;
    }
    AResourceActor* Item = nullptr;
    if (Size.IsNearlyZero())
    {
        Item = AResourceActor::Spawn(World, Kind, 1.0f, At);
    }
    else
    {
        EMatterShape Shape = EMatterShape::Box;
        FVector Base = FVector::OneVector;
        const TCHAR* Fallback = nullptr;
        AResourceActor::BaseLook(Kind, Shape, Base, Fallback);
        const float Amount = UMatterComponent::VolumeOf(Shape, Size) / FMath::Max(1.0e-6f, AResourceActor::UnitVolume(Kind));
        Item = AResourceActor::SpawnPiece(World, Kind, Amount, FTransform(Turn, At), Size, Shape);
    }
    if (!Item)
    {
        return nullptr;
    }
    if (Item->Matter && WetShare >= 0.0f)
    {
        const FSubstance& S = FMatter::Of(Kind);
        const float Full = S.IsSoil() ? S.LiquidLimit : FMath::Max(S.SaturatedMoisture(), 0.003f);
        Item->Matter->Moisture = Full * WetShare;
        Item->Setup(Kind, Item->Amount);
    }
    MatterSamples.Add(Item);
    return Item;
}

void AHumanSandboxGameMode::Throw(const FString& Kind, float MetresPerSecond, float Wetness)
{
    UWorld* World = GetWorld();
    APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
    if (!PC)
    {
        return;
    }
    const EResourceKind What = AResourceActor::KindFromWord(Kind);
    if (What == EResourceKind::None)
    {
        UE_LOG(LogHumanCity, Warning, TEXT("Не знаю такого вещества: %s"), *Kind);
        return;
    }
    FVector Eye;
    FRotator View;
    PC->GetPlayerViewPoint(Eye, View);
    AResourceActor* Item = Cast<AResourceActor>(SpawnSample(What, Wetness, Eye + View.Vector() * 120.0f, FVector::ZeroVector, View));
    if (!Item)
    {
        return;
    }
    if (Item->Body)
    {
        Item->Body->SetPhysicsLinearVelocity(View.Vector() * MetresPerSecond * 100.0f);
    }
    UE_LOG(LogHumanCity, Warning, TEXT("Брошено со скоростью %.1f м/с: %s"), MetresPerSecond,
        Item->Matter ? *Item->Matter->Describe() : TEXT(""));
}

void AHumanSandboxGameMode::Ignite()
{
    FHitResult Hit;
    FVector From;
    FVector Direction;
    if (!TraceView(Hit, From, Direction))
    {
        return;
    }
    AActor* Target = Hit.GetActor();
    UMatterComponent* Matter = Target ? Target->FindComponentByClass<UMatterComponent>() : nullptr;
    if (!Matter)
    {
        AMatterStructure* House = Cast<AMatterStructure>(Target);
        const int32 Piece = House ? House->FindPiece(Hit.GetComponent(), Hit.Item) : INDEX_NONE;
        if (House && Piece != INDEX_NONE)
        {
            const bool bLit = House->IgnitePiece(Piece);
            UE_LOG(LogHumanCity, Warning, TEXT("%s: %s"), bLit ? TEXT("Загорелось") : TEXT("Не горит"), *House->DescribePiece(Piece));
            return;
        }
        UE_LOG(LogHumanCity, Warning, TEXT("Поджечь нечего"));
        return;
    }
    const bool bLit = Matter->Ignite();
    UE_LOG(LogHumanCity, Warning, TEXT("%s: %s"), bLit ? TEXT("Загорелось") : TEXT("Не горит"), *Matter->Describe());
}

void AHumanSandboxGameMode::Soak(float Kilograms)
{
    FHitResult Hit;
    FVector From;
    FVector Direction;
    if (!TraceView(Hit, From, Direction))
    {
        return;
    }
    AActor* Target = Hit.GetActor();
    if (ATerrainGrid* Ground = Cast<ATerrainGrid>(Target))
    {
        Ground->AddWater(Hit.ImpactPoint, FMath::Max(0.0f, Kilograms), 50.0f);
        Ground->RebuildDirty();
        UE_LOG(LogHumanCity, Warning, TEXT("Полито водой: %s"), *Ground->DescribeAt(Hit.ImpactPoint));
        return;
    }
    UMatterComponent* Matter = Target ? Target->FindComponentByClass<UMatterComponent>() : nullptr;
    if (!Matter)
    {
        AMatterStructure* House = Cast<AMatterStructure>(Target);
        const int32 Piece = House ? House->FindPiece(Hit.GetComponent(), Hit.Item) : INDEX_NONE;
        if (House && Piece != INDEX_NONE)
        {
            House->SoakPiece(Piece, FMath::Max(0.0f, Kilograms));
            UE_LOG(LogHumanCity, Warning, TEXT("Полито водой: %s"), *House->DescribePiece(Piece));
            return;
        }
        UE_LOG(LogHumanCity, Warning, TEXT("Мочить нечего"));
        return;
    }
    Matter->ReceiveWater(FMath::Max(0.0f, Kilograms));
    UE_LOG(LogHumanCity, Warning, TEXT("Полито водой: %s"), *Matter->Describe());
}

void AHumanSandboxGameMode::Repair(int32 Times)
{
    FHitResult Hit;
    FVector From;
    FVector Direction;
    if (!TraceView(Hit, From, Direction))
    {
        return;
    }
    AMatterStructure* House = Cast<AMatterStructure>(Hit.GetActor());
    if (!House)
    {
        UE_LOG(LogHumanCity, Warning, TEXT("Чинить нечего: это не постройка"));
        return;
    }
    int32 Done = 0;
    for (int32 i = 0; i < FMath::Clamp(Times, 1, 500); ++i)
    {
        if (!House->RepairFromOffer(1.0f))
        {
            break;
        }
        ++Done;
    }
    UE_LOG(LogHumanCity, Warning, TEXT("Починено частей: %d. %s цела на %.0f%%, недостаёт %d"),
        Done, *House->Title, House->Condition() * 100.0f, House->MissingCount());
}

void AHumanSandboxGameMode::Dig(int32 Times)
{
    FHitResult Hit;
    FVector From;
    FVector Direction;
    if (!TraceView(Hit, From, Direction))
    {
        return;
    }
    ATerrainGrid* Ground = Cast<ATerrainGrid>(Hit.GetActor());
    if (!Ground)
    {
        UE_LOG(LogHumanCity, Warning, TEXT("Копать нечего: под взглядом не земля"));
        return;
    }
    int32 Heaps = 0;
    for (int32 i = 0; i < FMath::Clamp(Times, 1, 40); ++i)
    {
        float CubicMetres = 0.0f;
        const EResourceKind Taken = Ground->Dig(Hit.ImpactPoint, 4.0f, CubicMetres);
        if (Taken == EResourceKind::None || CubicMetres <= 0.0f)
        {
            break;
        }
        const float Edge = FMath::Pow(CubicMetres, 1.0f / 3.0f) * 100.0f;
        const FVector Size(Edge * 1.3f, Edge * 1.3f, Edge * 0.6f);
        const float Amount = CubicMetres / FMath::Max(1.0e-6f, AResourceActor::UnitVolume(Taken));
        const FVector Aside = FVector::CrossProduct(Direction, FVector::UpVector).GetSafeNormal() * (90.0f + 25.0f * i);
        const FVector At = Hit.ImpactPoint + Aside + FVector(0.0f, 0.0f, 40.0f + Size.Z);
        if (AResourceActor::SpawnPiece(GetWorld(), Taken, Amount, FTransform(At), Size, EMatterShape::Sphere))
        {
            ++Heaps;
        }
        UE_LOG(LogHumanCity, Warning, TEXT("Выкопано %.0f л: %s"), CubicMetres * 1000.0f, FMatter::Of(Taken).Name);
    }
    Ground->RebuildDirty();
    UE_LOG(LogHumanCity, Warning, TEXT("Куч выкопанного: %d. %s"), Heaps, *Ground->DescribeAt(Hit.ImpactPoint));
}

void AHumanSandboxGameMode::UpdateMatterTest(float RealDelta)
{
    UWorld* World = GetWorld();
    APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
    APawn* Camera = PC ? PC->GetPawn() : nullptr;
    UMatterSubsystem* Matter = UMatterSubsystem::Get(this);
    if (!Camera || !Matter)
    {
        return;
    }

    MatterTestClock += RealDelta;

    if (MatterTestStage == 0)
    {
        if (MatterTestClock < 3.0f)
        {
            return;
        }
        const FVector Probe(1200.0f, 3200.0f, 8000.0f);
        FCollisionQueryParams Params(SCENE_QUERY_STAT(MatterSite), false, Camera);
        FHitResult Ground;
        MatterSite = World->LineTraceSingleByChannel(Ground, Probe, Probe - FVector(0.0f, 0.0f, 16000.0f), ECC_WorldStatic, Params)
            ? FVector(Ground.ImpactPoint) : FVector(1200.0f, 3200.0f, 0.0f);

        const float Drop = 250.0f;
        const FRotator Flat(90.0f, 0.0f, 0.0f);
        SpawnSample(EResourceKind::Clay, 0.2f, MatterSite + FVector(0.0f, 0.0f, Drop), FVector(26.0f, 26.0f, 22.0f), FRotator::ZeroRotator);
        SpawnSample(EResourceKind::Clay, 0.72f, MatterSite + FVector(90.0f, 0.0f, Drop), FVector(26.0f, 26.0f, 22.0f), FRotator::ZeroRotator);
        SpawnSample(EResourceKind::Soil, 1.15f, MatterSite + FVector(180.0f, 0.0f, Drop), FVector(30.0f, 30.0f, 24.0f), FRotator::ZeroRotator);
        SpawnSample(EResourceKind::Glass, 0.0f, MatterSite + FVector(270.0f, 0.0f, Drop), FVector(50.0f, 40.0f, 1.0f), FRotator(0.0f, 0.0f, 12.0f));
        SpawnSample(EResourceKind::Wood, -1.0f, MatterSite + FVector(360.0f, 0.0f, Drop), FVector(22.0f, 22.0f, 140.0f), Flat);
        SpawnSample(EResourceKind::Granite, -1.0f, MatterSite + FVector(450.0f, 0.0f, Drop), FVector(32.0f, 28.0f, 24.0f), FRotator::ZeroRotator);
        SpawnSample(EResourceKind::Straw, -1.0f, MatterSite + FVector(560.0f, 0.0f, 45.0f), FVector(60.0f, 60.0f, 70.0f), FRotator::ZeroRotator);
        SpawnSample(EResourceKind::Wood, -1.0f, MatterSite + FVector(650.0f, 55.0f, 25.0f), FVector(22.0f, 22.0f, 140.0f), Flat);
        SpawnSample(EResourceKind::Brick, -1.0f, MatterSite + FVector(740.0f, 0.0f, Drop), FVector::ZeroVector, FRotator::ZeroRotator);
        MatterTestStage = 1;
        MatterShotTimer = 2.0f;
        UE_LOG(LogHumanCity, Warning, TEXT("MATTER площадка %s, погода: %s"), *MatterSite.ToString(), *Matter->DescribeClimate());
    }

    FVector Focus = MatterSite + FVector(370.0f, 0.0f, 10.0f);
    FVector Eye = MatterSite + FVector(370.0f, -520.0f, 230.0f);
    if (TestHouse)
    {
        const FVector Front = TestHouse->GetActorForwardVector();
        const FVector Side = TestHouse->GetActorRightVector();
        Focus = TestHouse->GetActorLocation() + FVector(0.0f, 0.0f, 180.0f);
        Eye = Focus + Front * 1100.0f + Side * 650.0f + FVector(0.0f, 0.0f, 260.0f);
    }
    Camera->SetActorLocation(Eye, false, nullptr, ETeleportType::TeleportPhysics);
    PC->SetControlRotation((Focus - Eye).Rotation());

    if (MatterTestStage == 1 && MatterTestClock > 7.0f)
    {
        if (MatterSamples.IsValidIndex(6) && IsValid(MatterSamples[6]))
        {
            if (UMatterComponent* Straw = MatterSamples[6]->FindComponentByClass<UMatterComponent>())
            {
                const bool bLit = Straw->Ignite();
                UE_LOG(LogHumanCity, Warning, TEXT("MATTER поджог соломы: %s"), bLit ? TEXT("горит") : TEXT("не занялась"));
            }
        }
        MatterTestStage = 2;
    }

    if (MatterTestStage == 2 && MatterTestClock > 10.0f)
    {
        for (int32 Blow = 0; Blow < 4; ++Blow)
        {
            AActor* Log = MatterSamples.IsValidIndex(4) ? MatterSamples[4].Get() : nullptr;
            if (!IsValid(Log) || Log->IsActorBeingDestroyed())
            {
                break;
            }
            if (UMatterComponent* Wood = Log->FindComponentByClass<UMatterComponent>())
            {
                Wood->ReceiveImpact(90.0f, Log->GetActorLocation(), FVector::DownVector, nullptr, true);
                UE_LOG(LogHumanCity, Warning, TEXT("MATTER удар топором %d"), Blow + 1);
            }
        }
        MatterTestStage = 3;
    }

    if (MatterTestStage == 3 && MatterTestClock > 12.0f)
    {
        float Best = BIG_NUMBER;
        for (TActorIterator<AMatterStructure> It(World); It; ++It)
        {
            if (It->bPrivate)
            {
                const float Distance = FVector::Dist(It->GetActorLocation(), MatterSite);
                if (Distance < Best)
                {
                    Best = Distance;
                    TestHouse = *It;
                }
            }
        }
        if (TestHouse)
        {
            const FVector Base = TestHouse->GetActorLocation();
            int32 Struck = 0;
            const int32 Total = TestHouse->GetPieces().Num();
            for (int32 i = 0; i < Total && Struck < 3; ++i)
            {
                const FStructurePiece Piece = TestHouse->GetPieces()[i];
                const float Height = Piece.Frame.GetLocation().Z - Base.Z;
                if (Piece.Role != FName(TEXT("венец")) || Piece.bRemoved || Height < 90.0f || Height > 130.0f)
                {
                    continue;
                }
                for (int32 Blow = 0; Blow < 12 && !TestHouse->GetPieces()[i].bRemoved; ++Blow)
                {
                    TestHouse->ImpactPiece(i, 400.0f, Piece.Frame.GetLocation(), -TestHouse->GetActorForwardVector(), nullptr, true);
                }
                ++Struck;
            }
            UE_LOG(LogHumanCity, Warning, TEXT("MATTER изба: срублено венцов %d; цела на %.0f%%, недостаёт %d"),
                Struck, TestHouse->Condition() * 100.0f, TestHouse->MissingCount());
        }
        MatterTestStage = 4;
    }

    if (MatterTestStage == 4 && MatterTestClock > 16.0f && TestHouse)
    {
        int32 Lit = 0;
        const int32 Total = TestHouse->GetPieces().Num();
        for (int32 i = 0; i < Total && Lit < 2; ++i)
        {
            if (TestHouse->GetPieces()[i].Role == FName(TEXT("кровля")) && TestHouse->IgnitePiece(i))
            {
                ++Lit;
            }
        }
        UE_LOG(LogHumanCity, Warning, TEXT("MATTER подожжена кровля: %d"), Lit);
        MatterTestStage = 5;
    }

    if (MatterTestStage == 5 && MatterTestClock > 40.0f)
    {
        Matter->ForceRain(14.0f, 6.0f);
        UE_LOG(LogHumanCity, Warning, TEXT("MATTER пошёл дождь: %s"), *Matter->DescribeClimate());
        MatterTestStage = 6;
    }

    if (MatterTestStage == 6 && MatterTestClock > 48.0f && TestHouse)
    {
        int32 Done = 0;
        while (Done < 400 && TestHouse->RepairFromOffer(0.9f))
        {
            ++Done;
        }
        UE_LOG(LogHumanCity, Warning, TEXT("MATTER изба починена: частей %d; цела на %.0f%%, недостаёт %d"),
            Done, TestHouse->Condition() * 100.0f, TestHouse->MissingCount());
        MatterTestStage = 7;
    }

    MatterShotTimer -= RealDelta;
    if (MatterShotTimer <= 0.0f && MatterTestShots < WatchShotsTotal)
    {
        MatterShotTimer = WatchEvery;
        const FString File = WatchFolder / FString::Printf(TEXT("matter_%03d.png"), MatterTestShots);
        FScreenshotRequest::RequestScreenshot(File, true, false);
        UE_LOG(LogHumanCity, Warning, TEXT("MATTER shot %s | %s | вещей %d"),
            *FPaths::GetCleanFilename(File), *Matter->DescribeClimate(), Matter->CountItems());
        for (int32 i = 0; i < MatterSamples.Num(); ++i)
        {
            AActor* Sample = MatterSamples[i].Get();
            const UMatterComponent* State = IsValid(Sample) ? Sample->FindComponentByClass<UMatterComponent>() : nullptr;
            if (!State || Sample->IsActorBeingDestroyed())
            {
                UE_LOG(LogHumanCity, Warning, TEXT("   %d: цельным больше нет"), i);
                continue;
            }
            UE_LOG(LogHumanCity, Warning, TEXT("   %d: %s"), i, *State->Describe());
        }
        if (TestHouse)
        {
            int32 Burning = 0;
            float Hottest = -100.0f;
            for (const FStructurePiece& Piece : TestHouse->GetPieces())
            {
                if (!Piece.bRemoved)
                {
                    Burning += Piece.bBurning ? 1 : 0;
                    Hottest = FMath::Max(Hottest, Piece.Temperature);
                }
            }
            UE_LOG(LogHumanCity, Warning, TEXT("   изба: цела на %.0f%%, недостаёт %d из %d, горит частей %d, самое горячее %.0f °C"),
                TestHouse->Condition() * 100.0f, TestHouse->MissingCount(), TestHouse->PieceCount(), Burning, Hottest);
        }
        ++MatterTestShots;
    }

    if (MatterTestShots >= WatchShotsTotal)
    {
        WatchExitTimer += RealDelta;
        if (WatchExitTimer > 3.0f)
        {
            FGenericPlatformMisc::RequestExit(false);
        }
    }
}

FVector AHumanSandboxGameMode::GroundPoint(const FVector& Near) const
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return Near;
    }
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(GroundPoint), false);
    const FVector From(Near.X, Near.Y, Near.Z + 5000.0f);
    const FVector To(Near.X, Near.Y, Near.Z - 20000.0f);
    if (World->LineTraceSingleByChannel(Hit, From, To, ECC_WorldStatic, Params))
    {
        return Hit.ImpactPoint;
    }
    return Near;
}

void AHumanSandboxGameMode::UpdateRecording(float RealDelta)
{
    if (!bRecording)
    {
        return;
    }
    if (!Recorder.IsValid())
    {
        RecordTimer -= RealDelta;
        if (RecordTimer > 0.0f)
        {
            return;
        }
        Recorder = MakeShared<FClipRecorder>();
        if (!Recorder->Start(RecordFolder))
        {
            UE_LOG(LogHumanCity, Warning, TEXT("VIDEO не удалось начать запись"));
            Recorder.Reset();
            bRecording = false;
            return;
        }
        RecordStep = 0.0f;
    }

    RecordClock += RealDelta;
    RecordStep += RealDelta;
    const float Interval = 1.0f / FMath::Max(1.0f, RecordFps);
    if (RecordStep >= Interval)
    {
        RecordStep = FMath::Min(RecordStep - Interval, Interval);
        Recorder->Capture();
        ++RecordFrame;
    }
    Recorder->Flush(false);

    if (RecordClock >= RecordSeconds)
    {
        Recorder->Stop();
        UE_LOG(LogHumanCity, Warning, TEXT("VIDEO записано кадров: %d (запрошено %d) за %.0f с в %s"),
            Recorder->Captured(), RecordFrame, RecordClock, *RecordFolder);
        Recorder.Reset();
        bRecording = false;
        FGenericPlatformMisc::RequestExit(false);
    }
}

void AHumanSandboxGameMode::UpdateGraspTest(float RealDelta)
{
    UWorld* World = GetWorld();
    APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
    APawn* Camera = PC ? PC->GetPawn() : nullptr;
    UMatterSubsystem* Matter = UMatterSubsystem::Get(this);
    UHumanWorldSubsystem* WorldMind = World ? World->GetSubsystem<UHumanWorldSubsystem>() : nullptr;
    if (!Camera || !Matter || !WorldMind)
    {
        return;
    }

    GraspClock += RealDelta;
    GraspStageClock += RealDelta;
    ACompleteHumanNPC* Human = GraspSubject.Get();

    if (GraspStage == 0)
    {
        if (GraspClock < 4.0f || WorldMind->GetAllHumans().Num() == 0)
        {
            return;
        }
        for (ACompleteHumanNPC* Candidate : WorldMind->GetAllHumans())
        {
            if (Candidate && Candidate->IsAlive() && Candidate->IdentityComponent && Candidate->IdentityComponent->Age > 18.0f)
            {
                Human = Candidate;
                break;
            }
        }
        if (!Human)
        {
            return;
        }
        GraspSubject = Human;
        GraspSite = GroundPoint(FVector(3600.0f, -1700.0f, 0.0f));
        GraspForward = FVector(1.0f, 0.0f, 0.0f);
        const FVector Right(0.0f, 1.0f, 0.0f);
        if (Human->Mind)
        {
            Human->Mind->bPolicyControlled = true;
            Human->Mind->PolicyStop();
        }
        Human->StopMoving();
        Human->SetActorLocationAndRotation(GraspSite + FVector(0.0f, 0.0f, 95.0f), GraspForward.Rotation(), false, nullptr, ETeleportType::TeleportPhysics);

        if (ATerrainGrid* Ground = Matter->GetTerrain())
        {
            Ground->SoakArea(GraspSite + Right * 520.0f, 260.0f, 0.95f, true);
            Ground->SoakArea(GraspSite + Right * 520.0f + GraspForward * 520.0f, 200.0f, 0.55f, true);
            Ground->RebuildDirty();
        }

        auto Place = [this](EResourceKind Kind, float Wet, const FVector& At, const FVector& Size, const FRotator& Turn) -> AActor*
        {
            const FVector Ground = GroundPoint(At);
            return SpawnSample(Kind, Wet, Ground + FVector(0.0f, 0.0f, Size.Z * 0.5f + 2.0f), Size, Turn);
        };
        GraspItems.Reset();
        GraspItems.Add(Place(EResourceKind::Granite, -1.0f, GraspSite + GraspForward * 95.0f + Right * 12.0f, FVector(26.0f, 22.0f, 18.0f), FRotator::ZeroRotator));
        GraspItems.Add(Place(EResourceKind::Clay, 0.6f, GraspSite + GraspForward * 95.0f - Right * 40.0f, FVector(18.0f, 18.0f, 14.0f), FRotator::ZeroRotator));
        GraspItems.Add(Place(EResourceKind::Wood, -1.0f, GraspSite + GraspForward * 110.0f + Right * 70.0f, FVector(16.0f, 16.0f, 80.0f), FRotator(90.0f, 90.0f, 0.0f)));
        if (ABookActor* Book = World->SpawnActor<ABookActor>(ABookActor::StaticClass(), GroundPoint(GraspSite + GraspForward * 90.0f - Right * 90.0f) + FVector(0.0f, 0.0f, 2.5f), FRotator(0.0f, 30.0f, 0.0f)))
        {
            Book->Setup(TEXT("Reader"));
            Book->ShelfLocation = Book->GetActorLocation();
            GraspItems.Add(Book);
        }
        GraspItem = 0;
        GraspStage = 1;
        GraspStageClock = 0.0f;
        UE_LOG(LogHumanCity, Warning, TEXT("GRASP место %s, житель %s, вещей %d"), *GraspSite.ToString(), *Human->GetPersonName(), GraspItems.Num());
    }

    if (!Human || !IsValid(Human))
    {
        return;
    }

    const FVector HumanAt = Human->GetActorLocation();
    const FVector Fwd = Human->GetActorForwardVector();
    const FVector Side = Human->GetActorRightVector();
    FVector Desired = HumanAt + Side * -230.0f + Fwd * 150.0f + FVector(0.0f, 0.0f, 35.0f);
    FVector LookAt = HumanAt + Fwd * 40.0f - FVector(0.0f, 0.0f, 25.0f);
    if (GraspStage >= 20)
    {
        Desired = HumanAt - Side * 330.0f + Fwd * 60.0f + FVector(0.0f, 0.0f, 60.0f);
        LookAt = HumanAt - FVector(0.0f, 0.0f, 60.0f);
    }
    CameraSmooth = bCameraSmoothValid ? FMath::VInterpTo(CameraSmooth, Desired, RealDelta, 2.0f) : Desired;
    bCameraSmoothValid = true;
    Camera->SetActorLocation(CameraSmooth, false, nullptr, ETeleportType::TeleportPhysics);
    PC->SetControlRotation((LookAt - CameraSmooth).Rotation());

    GraspLogTimer -= RealDelta;
    if (GraspLogTimer <= 0.0f)
    {
        GraspLogTimer = 0.5f;
        FString Items;
        for (const TObjectPtr<AActor>& Item : GraspItems)
        {
            if (IsValid(Item))
            {
                Items += FString::Printf(TEXT(" [%s z%.0f]"), *Item->GetName(), Item->GetActorLocation().Z - GraspSite.Z);
            }
        }
        UE_LOG(LogHumanCity, Warning, TEXT("GRASP t%.1f этап %d | %s | в руках %s | скорость %.0f |%s"),
            GraspClock, GraspStage, Human->Motor ? *Human->Motor->DescribeHands() : TEXT("-"),
            Human->CarriedItem ? *Human->CarriedItem->GetName() : TEXT("-"), Human->GetVelocity().Size2D(), *Items);
    }

    const bool bBusy = Human->AreHandsBusy();
    if (GraspStage == 1)
    {
        if (GraspStageClock < 1.5f)
        {
            return;
        }
        if (!GraspItems.IsValidIndex(GraspItem))
        {
            GraspStage = 20;
            GraspStageClock = 0.0f;
            Human->RequestMoveTo(GraspSite + FVector(0.0f, 1.0f, 0.0f) * 520.0f + FVector(1.0f, 0.0f, 0.0f) * 700.0f);
            return;
        }
        AActor* Item = GraspItems[GraspItem].Get();
        if (!IsValid(Item))
        {
            ++GraspItem;
            return;
        }
        const bool bStarted = Human->BeginTake(Item);
        UE_LOG(LogHumanCity, Warning, TEXT("GRASP берёт %s: %s"), *Item->GetName(), bStarted ? TEXT("начал") : TEXT("не может"));
        GraspStage = bStarted ? 2 : 1;
        if (!bStarted)
        {
            ++GraspItem;
        }
        GraspStageClock = 0.0f;
    }
    else if (GraspStage == 2)
    {
        if (!bBusy && GraspStageClock > 0.3f)
        {
            GraspStage = 3;
            GraspStageClock = 0.0f;
        }
        else if (GraspStageClock > 25.0f)
        {
            UE_LOG(LogHumanCity, Warning, TEXT("GRASP не дождался: %s"), Human->Motor ? *Human->Motor->DescribeHands() : TEXT("-"));
            ++GraspItem;
            GraspStage = 1;
            GraspStageClock = 0.0f;
        }
    }
    else if (GraspStage == 3)
    {
        if (GraspStageClock > 2.0f)
        {
            const bool bBook = Cast<ABookActor>(Human->CarriedItem.Get()) != nullptr;
            Human->ReleaseFromHands(bBook);
            GraspStage = 4;
            GraspStageClock = 0.0f;
        }
    }
    else if (GraspStage == 4)
    {
        if (!Human->AreHandsBusy() && GraspStageClock > 0.3f)
        {
            ++GraspItem;
            GraspStage = 1;
            GraspStageClock = 0.0f;
        }
    }
    else if (GraspStage == 20)
    {
        if (GraspStageClock > 14.0f)
        {
            Human->RequestMoveTo(GraspSite + FVector(0.0f, 1.0f, 0.0f) * 520.0f - FVector(1.0f, 0.0f, 0.0f) * 300.0f);
            GraspStage = 21;
            GraspStageClock = 0.0f;
        }
    }
    else if (GraspStage == 21)
    {
        if (GraspStageClock > 14.0f)
        {
            GraspStage = 22;
            GraspStageClock = 0.0f;
            Human->StopMoving();
            UE_LOG(LogHumanCity, Warning, TEXT("GRASP конец проверки"));
        }
    }
}

void AHumanSandboxGameMode::UpdateWalkTest(float RealDelta)
{
    UWorld* World = GetWorld();
    APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
    APawn* Camera = PC ? PC->GetPawn() : nullptr;
    UMatterSubsystem* Matter = UMatterSubsystem::Get(this);
    UHumanWorldSubsystem* WorldMind = World ? World->GetSubsystem<UHumanWorldSubsystem>() : nullptr;
    if (!Camera || !Matter || !WorldMind)
    {
        return;
    }

    GraspClock += RealDelta;
    GraspStageClock += RealDelta;
    ACompleteHumanNPC* Human = GraspSubject.Get();

    if (GraspStage == 0)
    {
        if (GraspClock < 4.0f)
        {
            return;
        }
        for (ACompleteHumanNPC* Candidate : WorldMind->GetAllHumans())
        {
            if (Candidate && Candidate->IsAlive() && Candidate->IdentityComponent && Candidate->IdentityComponent->Age > 18.0f
                && Candidate->IdentityComponent->Age < 50.0f)
            {
                Human = Candidate;
                break;
            }
        }
        if (!Human)
        {
            return;
        }
        GraspSubject = Human;
        GraspSite = GroundPoint(FVector(3600.0f, -1700.0f, 0.0f));
        const FVector Fwd(1.0f, 0.0f, 0.0f);
        const FVector Right(0.0f, 1.0f, 0.0f);
        if (Human->Mind)
        {
            Human->Mind->bPolicyControlled = true;
            Human->Mind->PolicyStop();
        }
        Human->StopMoving();
        Human->SetActorLocationAndRotation(GraspSite + FVector(0.0f, 0.0f, 95.0f), Fwd.Rotation(), false, nullptr, ETeleportType::TeleportPhysics);
        if (ATerrainGrid* Ground = Matter->GetTerrain())
        {
            Ground->SoakArea(GraspSite + Right * 520.0f + Fwd * 250.0f, 280.0f, 0.95f, true);
            Ground->RebuildDirty();
        }
        WalkRoute = {
            GraspSite + Fwd * 900.0f,
            GraspSite + Fwd * 900.0f + Right * 520.0f,
            GraspSite - Fwd * 250.0f + Right * 520.0f,
            GraspSite };
        WalkLeg = 0;
        GraspStage = 1;
        GraspStageClock = 0.0f;
        UE_LOG(LogHumanCity, Warning, TEXT("WALK место %s, житель %s, %s"), *GraspSite.ToString(), *Human->GetPersonName(),
            Human->Motor ? *Human->Motor->DescribeWalking() : TEXT("-"));
    }

    if (!Human || !IsValid(Human))
    {
        return;
    }

    const FVector HumanAt = Human->GetActorLocation();
    FVector Heading = Human->GetVelocity().GetSafeNormal2D();
    if (Heading.IsNearlyZero())
    {
        Heading = Human->GetActorForwardVector();
    }
    const FVector Across = FVector::CrossProduct(FVector::UpVector, Heading);
    const FVector Desired = HumanAt - Across * 360.0f + Heading * 40.0f + FVector(0.0f, 0.0f, 10.0f);
    const FVector LookAt = HumanAt - FVector(0.0f, 0.0f, 45.0f);
    CameraSmooth = bCameraSmoothValid ? FMath::VInterpTo(CameraSmooth, Desired, RealDelta, 1.6f) : Desired;
    bCameraSmoothValid = true;
    Camera->SetActorLocation(CameraSmooth, false, nullptr, ETeleportType::TeleportPhysics);
    PC->SetControlRotation((LookAt - CameraSmooth).Rotation());

    GraspLogTimer -= RealDelta;
    if (GraspLogTimer <= 0.0f)
    {
        GraspLogTimer = 0.5f;
        UE_LOG(LogHumanCity, Warning, TEXT("WALK t%.1f этап %d, отрезок %d | место (%.0f, %.0f) | скорость %.0f см/с | поза %d | %s"),
            GraspClock, GraspStage, WalkLeg, HumanAt.X - GraspSite.X, HumanAt.Y - GraspSite.Y, Human->GetVelocity().Size2D(),
            static_cast<int32>(Human->Posture), Human->Motor ? *Human->Motor->DescribeWalking() : TEXT("-"));
    }

    if (GraspStage == 1)
    {
        if (GraspStageClock > 3.0f)
        {
            Human->RequestMoveTo(WalkRoute[WalkLeg]);
            GraspStage = 2;
            GraspStageClock = 0.0f;
        }
    }
    else if (GraspStage == 2)
    {
        const bool bArrived = !Human->IsMoving() && GraspStageClock > 1.0f;
        if (bArrived || GraspStageClock > 30.0f)
        {
            UE_LOG(LogHumanCity, Warning, TEXT("WALK отрезок %d %s за %.1f с"), WalkLeg, bArrived ? TEXT("пройден") : TEXT("не пройден"), GraspStageClock);
            Human->StopMoving();
            ++WalkLeg;
            GraspStage = WalkRoute.IsValidIndex(WalkLeg) ? 1 : 3;
            GraspStageClock = 0.0f;
        }
    }
    else if (GraspStage == 3 && GraspStageClock > 3.0f)
    {
        GraspStage = 4;
        UE_LOG(LogHumanCity, Warning, TEXT("WALK конец проверки: %s"), Human->Motor ? *Human->Motor->DescribeWalking() : TEXT("-"));
    }
}

void AHumanSandboxGameMode::UpdateTourTest(float RealDelta)
{
    UWorld* World = GetWorld();
    APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
    APawn* Camera = PC ? PC->GetPawn() : nullptr;
    if (!Camera)
    {
        return;
    }
    GraspClock += RealDelta;
    struct FStop
    {
        FVector Eye;
        FVector Look;
    };
    static const FStop Stops[] = {
        { FVector(-9000.0f, -6000.0f, 2600.0f), FVector(0.0f, 0.0f, 0.0f) },
        { FVector(-2400.0f, -3200.0f, 900.0f), FVector(-2400.0f, 0.0f, 300.0f) },
        { FVector(1500.0f, -2600.0f, 350.0f), FVector(4000.0f, -900.0f, 150.0f) },
        { FVector(5200.0f, 200.0f, 260.0f), FVector(8000.0f, -900.0f, 150.0f) },
        { FVector(-6000.0f, -1500.0f, 260.0f), FVector(-6600.0f, -2600.0f, 300.0f) },
        { FVector(9000.0f, 3000.0f, 1400.0f), FVector(0.0f, 0.0f, 0.0f) } };
    const int32 Count = UE_ARRAY_COUNT(Stops);
    const float Leg = 8.0f;
    const float Position = FMath::Fmod(GraspClock / Leg, static_cast<float>(Count));
    const int32 A = FMath::FloorToInt(Position) % Count;
    const int32 B = (A + 1) % Count;
    const float T = FMath::SmoothStep(0.0f, 1.0f, Position - FMath::FloorToFloat(Position));
    FVector Eye = FMath::Lerp(Stops[A].Eye, Stops[B].Eye, T);
    FVector Look = FMath::Lerp(Stops[A].Look, Stops[B].Look, T);
    const float GroundZ = GroundPoint(Eye).Z;
    Eye.Z += GroundZ;
    Look.Z += GroundPoint(Look).Z;
    Camera->SetActorLocation(Eye, false, nullptr, ETeleportType::TeleportPhysics);
    PC->SetControlRotation((Look - Eye).Rotation());
}

void AHumanSandboxGameMode::BindHotkeys()
{
    if (bHotkeysBound)
    {
        return;
    }
    UWorld* World = GetWorld();
    APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
    if (!PC)
    {
        return;
    }
    EnableInput(PC);
    if (!InputComponent)
    {
        return;
    }
    InputComponent->BindKey(EKeys::Add, IE_Pressed, this, &AHumanSandboxGameMode::SpeedUp);
    InputComponent->BindKey(EKeys::Equals, IE_Pressed, this, &AHumanSandboxGameMode::SpeedUp);
    InputComponent->BindKey(EKeys::Subtract, IE_Pressed, this, &AHumanSandboxGameMode::SlowDown);
    InputComponent->BindKey(EKeys::Hyphen, IE_Pressed, this, &AHumanSandboxGameMode::SlowDown);
    InputComponent->BindKey(EKeys::Zero, IE_Pressed, this, &AHumanSandboxGameMode::SpeedNormal);
    InputComponent->BindKey(EKeys::NumPadZero, IE_Pressed, this, &AHumanSandboxGameMode::SpeedNormal);
    bHotkeysBound = true;
}

ACompleteHumanNPC* AHumanSandboxGameMode::PickInteresting() const
{
    UWorld* World = GetWorld();
    UHumanWorldSubsystem* WorldMind = World ? World->GetSubsystem<UHumanWorldSubsystem>() : nullptr;
    if (!WorldMind)
    {
        return nullptr;
    }

    ACompleteHumanNPC* Best = nullptr;
    float BestScore = -BIG_NUMBER;
    for (ACompleteHumanNPC* Human : WorldMind->GetAllHumans())
    {
        if (!Human || !Human->Mind)
        {
            continue;
        }
        float Score = FMath::FRand();
        if (Human->Mind->IsInConversation()) Score += 3.0f;
        if (Human->Posture != EPosture::Standing) Score += 2.5f;
        if (Human->CarriedItem) Score += 2.0f;
        if (Human->Mind->IsPerformingAction()) Score += 1.5f;
        if (Human->GetVelocity().Size2D() > 60.0f) Score += 1.0f;
        if (Human == LastWatchSubject) Score -= 4.0f;
        if (Score > BestScore)
        {
            BestScore = Score;
            Best = Human;
        }
    }
    return Best;
}

void AHumanSandboxGameMode::UpdateWatchTest(float RealDelta)
{
    UWorld* World = GetWorld();
    APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
    APawn* Camera = PC ? PC->GetPawn() : nullptr;
    if (!Camera)
    {
        return;
    }

    WatchSubjectTimer -= RealDelta;
    if (!WatchSubject || !IsValid(WatchSubject) || WatchSubjectTimer <= 0.0f)
    {
        LastWatchSubject = WatchSubject;
        WatchSubject = PickInteresting();
        WatchSubjectTimer = WatchEvery * 2.0f;
        bWatchCameraPlaced = false;
    }
    if (!WatchSubject)
    {
        return;
    }

    static bool bRenderTuned = false;
    if (!bRenderTuned && GEngine)
    {
        bRenderTuned = true;
        GEngine->Exec(World, TEXT("r.MotionBlurQuality 0"));
    }

    const FVector Target = WatchSubject->GetActorLocation();
    const FVector Eye = Target + FVector(0.0f, 0.0f, 50.0f);
    const FVector Fwd = WatchSubject->GetActorForwardVector();
    const FVector Side = WatchSubject->GetActorRightVector();
    const FVector Offsets[] = {
        Fwd * 190.0f + Side * 210.0f + FVector(0.0f, 0.0f, 20.0f),
        Fwd * 190.0f - Side * 210.0f + FVector(0.0f, 0.0f, 20.0f),
        Side * 270.0f + FVector(0.0f, 0.0f, 40.0f),
        -Side * 270.0f + FVector(0.0f, 0.0f, 40.0f),
        Fwd * 290.0f + FVector(0.0f, 0.0f, 50.0f),
        -Fwd * 250.0f + FVector(0.0f, 0.0f, 70.0f) };

    FCollisionQueryParams Params(SCENE_QUERY_STAT(WatchCamera), false, WatchSubject);
    Params.AddIgnoredActor(Camera);

    FVector Desired = Eye + Offsets[0];
    float BestClear = -BIG_NUMBER;
    for (const FVector& Offset : Offsets)
    {
        const FVector Wanted = Eye + Offset;
        const float Full = Offset.Size();
        float Clear = Full;
        FVector Spot = Wanted;
        FHitResult Hit;
        if (World->LineTraceSingleByChannel(Hit, Eye, Wanted, ECC_Visibility, Params))
        {
            Clear = Hit.Distance - 25.0f;
            Spot = Eye + Offset.GetSafeNormal() * FMath::Max(Clear, 10.0f);
        }
        if (Clear > BestClear)
        {
            BestClear = Clear;
            Desired = Spot;
        }
        if (Clear >= Full - 1.0f)
        {
            break;
        }
    }

    WatchCamera = bWatchCameraPlaced ? FMath::VInterpTo(WatchCamera, Desired, RealDelta, 2.5f) : Desired;
    bWatchCameraPlaced = true;
    Camera->SetActorLocation(WatchCamera, false, nullptr, ETeleportType::TeleportPhysics);
    PC->SetControlRotation((Target + FVector(0.0f, 0.0f, 15.0f) - WatchCamera).Rotation());

    if (WatchShotsTaken >= WatchShotsTotal)
    {
        WatchExitTimer += RealDelta;
        if (WatchExitTimer > 3.0f)
        {
            FGenericPlatformMisc::RequestExit(false);
        }
        return;
    }

    WatchShotTimer -= RealDelta;
    if (WatchShotTimer > 0.0f)
    {
        return;
    }
    WatchShotTimer = WatchEvery;

    const FString File = WatchFolder / FString::Printf(TEXT("shot_%03d.png"), WatchShotsTaken);
    FScreenshotRequest::RequestScreenshot(File, true, false);

    static const TCHAR* Postures[] = { TEXT("стоит"), TEXT("сидит"), TEXT("лежит") };
    const UMindComponent* Mind = WatchSubject->Mind;
    UE_LOG(LogHumanCity, Warning, TEXT("WATCH %s | %s | %s | %s | скорость %.0f | в руках: %s | сказал: %s"),
        *FPaths::GetCleanFilename(File),
        WatchSubject->IdentityComponent ? *WatchSubject->IdentityComponent->FirstName : TEXT("?"),
        Mind ? *Mind->CurrentActionLabel : TEXT("-"),
        Postures[FMath::Clamp(static_cast<int32>(WatchSubject->Posture), 0, 2)],
        WatchSubject->GetVelocity().Size2D(),
        WatchSubject->CarriedItem ? *WatchSubject->CarriedItem->GetName() : TEXT("-"),
        WatchSubject->SpeechComponent ? *WatchSubject->SpeechComponent->LastSaid : TEXT("-"));
    ++WatchShotsTaken;
}

// Категория летописи объявлена в HumanTypes.h — в неё пишут и сами люди.

namespace
{
    /** Слова речи, которых нет ни в одной книге: жителям их взять неоткуда. */
    void LogWordsMissingFromBooks()
    {
        TSet<FString> InBooks;
        for (const FTextbook& Book : FLibrary::All())
        {
            for (const FTextbookPage& Page : Book.Pages)
            {
                TArray<FString> Words;
                Page.Text.ParseIntoArrayWS(Words);
                for (const FString& W : Words)
                {
                    const FName Key = USpeechComponent::NormalizeWord(W);
                    if (!Key.IsNone())
                    {
                        InBooks.Add(Key.ToString().ToLower());
                    }
                }
            }
        }

        TSet<FString> InSpeech;
        FDialogueLines::CollectSpeechWords(InSpeech);

        TArray<FString> Missing;
        for (const FString& W : InSpeech)
        {
            if (!InBooks.Contains(W))
            {
                Missing.Add(W);
            }
        }
        Missing.Sort();

        UE_LOG(LogHumanCity, Warning, TEXT("Слов в книгах: %d, в речи: %d, в речи без книги: %d"),
               InBooks.Num(), InSpeech.Num(), Missing.Num());
        if (Missing.Num() > 0)
        {
            UE_LOG(LogHumanCity, Warning, TEXT("Нет ни в одной книге: %s"), *FString::Join(Missing, TEXT(" ")));
        }
    }
}

AHumanSandboxGameMode::AHumanSandboxGameMode()
{
    PrimaryActorTick.bCanEverTick = true;

    // Свободная камера: за людьми смотрят со стороны, а не играют за них.
    DefaultPawnClass = ASpectatorPawn::StaticClass();
}

void AHumanSandboxGameMode::BeginPlay()
{
    Super::BeginPlay();

    // Без экрана единственный способ что-то увидеть — лог.
    if (!FApp::CanEverRender())
    {
        bLogChronicle = true;
        bShowOverlay = false;
    }

    if (bEnsureLighting && FApp::CanEverRender())
    {
        EnsureLighting();
    }

    if (bAutoSpawnCity)
    {
        SpawnCityIfNeeded();
    }

    UE_LOG(LogHumanCity, Log, TEXT("=== Город проснулся ==="));
    LogWordsMissingFromBooks();

    {
        UE_LOG(LogHumanCity, Warning, TEXT("В мире: веществ %d, пород %d, законов природы %d, правил счёта %d, книг %d"),
            FElements::All().Num(), FElements::Minerals().Num(), FElements::Physics().Num(),
            FElements::Mathematics().Num(), FLibrary::All().Num());

        const TArray<FCraft>& Deeds = FCraftBook::All();
        UE_LOG(LogHumanCity, Warning, TEXT("Из книг вычитано дел: %d"), Deeds.Num());
        for (const FCraft& Deed : Deeds)
        {
            FString From;
            for (const FCraftPart& Need : Deed.Inputs)
            {
                From += FString::Printf(TEXT("%s x%.0f "), *FCraftBook::NameOfKind(Need.Kind), Need.Amount);
            }
            UE_LOG(LogHumanCity, Log, TEXT("  %s | из: %s| чем: %s | где: %d%s"),
                *Deed.Label, From.IsEmpty() ? TEXT("ничего ") : *From,
                *FCraftBook::NameOfKind(Deed.Tool), static_cast<int32>(Deed.Station),
                Deed.bAnywhere ? TEXT(" (где угодно)") : TEXT(""));
        }
    }
}

// ---------------------------------------------------------------------------
//  Город
// ---------------------------------------------------------------------------

void AHumanSandboxGameMode::SpawnCityIfNeeded()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    // Если генератор уже стоит на уровне — не мешаем ему.
    for (TActorIterator<ACityGenerator> It(World); It; ++It)
    {
        City = *It;
        return;
    }

    // Если люди уже расставлены вручную — тоже не вмешиваемся.
    for (TActorIterator<ACompleteHumanNPC> It(World); It; ++It)
    {
        return;
    }

    // Откладываем BeginPlay генератора, чтобы успеть задать размеры города:
    // иначе он построится по умолчанию, а потом построится ещё раз.
    City = World->SpawnActorDeferred<ACityGenerator>(
        ACityGenerator::StaticClass(),
        FTransform::Identity,
        nullptr, nullptr,
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

    if (!City)
    {
        return;
    }

    City->BlocksX = FMath::Max(2, CityBlocks);
    City->BlocksY = FMath::Max(2, CityBlocks);
    City->NPCsCount = FMath::Max(1, Population);
    City->bGenerateOnBeginPlay = true;

    // Здесь генератор и оживает: строит дома, отмечает места, селит людей.
    City->FinishSpawning(FTransform::Identity);
}

// ---------------------------------------------------------------------------
//  Свет
// ---------------------------------------------------------------------------

void AHumanSandboxGameMode::EnsureLighting()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    // Если на уровне уже есть солнце — берём его и не плодим второе.
    for (TActorIterator<ADirectionalLight> It(World); It; ++It)
    {
        Sun = *It;
        break;
    }

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    if (!Sun)
    {
        Sun = World->SpawnActor<ADirectionalLight>(
            ADirectionalLight::StaticClass(),
            FVector(0.0f, 0.0f, 5000.0f),
            FRotator(-45.0f, -30.0f, 0.0f), Params);

        if (Sun)
        {
            Sun->SetMobility(EComponentMobility::Movable);
            if (UDirectionalLightComponent* Light = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
            {
                Light->SetIntensity(6.0f);
            }
        }
    }
    else
    {
        // Чужое солнце тоже придётся двигать — иначе ночь не наступит.
        Sun->SetMobility(EComponentMobility::Movable);
    }

    // Небо, чтобы тени не были угольно-чёрными.
    bool bHasSky = false;
    for (TActorIterator<ASkyLight> It(World); It; ++It)
    {
        bHasSky = true;
        break;
    }

    if (!bHasSky)
    {
        if (ASkyLight* SkyLight = World->SpawnActor<ASkyLight>(
                ASkyLight::StaticClass(), FVector(0.0f, 0.0f, 3000.0f), FRotator::ZeroRotator, Params))
        {
            if (USkyLightComponent* Component = SkyLight->GetLightComponent())
            {
                Component->SetMobility(EComponentMobility::Movable);
                Component->SetIntensity(1.2f);
                Component->SetLightColor(FLinearColor(0.55f, 0.62f, 0.8f));
            }
        }
    }
}

void AHumanSandboxGameMode::UpdateSun()
{
    if (!Sun || !bDriveSunByClock)
    {
        return;
    }

    UWorld* World = GetWorld();
    UHumanWorldSubsystem* WorldMind = World ? World->GetSubsystem<UHumanWorldSubsystem>() : nullptr;
    if (!WorldMind)
    {
        return;
    }

    // Полдень — солнце в зените, полночь — глубоко под горизонтом.
    const float Hour = WorldMind->Now.HourFloat;
    const float Pitch = -90.0f * FMath::Sin((Hour - 6.0f) / 24.0f * 2.0f * PI);
    Sun->SetActorRotation(FRotator(Pitch, Hour / 24.0f * 360.0f, 0.0f));

    if (UDirectionalLightComponent* Light = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
    {
        // Ночью свет не выключается совсем — иначе ничего не разглядеть.
        const float DayFactor = FMath::Clamp(FMath::Sin((Hour - 6.0f) / 24.0f * 2.0f * PI), -1.0f, 1.0f);
        Light->SetIntensity(FMath::Lerp(0.35f, 6.0f, FMath::Clamp(DayFactor * 0.5f + 0.5f, 0.0f, 1.0f)));

        // И теплеет к закату.
        const FLinearColor Warm(1.0f, 0.72f, 0.45f);
        const FLinearColor Noon(1.0f, 0.96f, 0.92f);
        Light->SetLightColor(FMath::Lerp(Warm, Noon, FMath::Clamp(DayFactor, 0.0f, 1.0f)));
    }
}

// ---------------------------------------------------------------------------
//  Наблюдение
// ---------------------------------------------------------------------------

void AHumanSandboxGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!bStartupDone)
    {
        bStartupDone = true;
        const TCHAR* Cmd = FCommandLine::Get();
        bWatchTest = FParse::Param(Cmd, TEXT("WatchTest"));
        bMatterTest = FParse::Param(Cmd, TEXT("MatterTest"));
        bGraspTest = FParse::Param(Cmd, TEXT("GraspTest"));
        bTourTest = FParse::Param(Cmd, TEXT("TourTest"));
        bWalkTest = FParse::Param(Cmd, TEXT("WalkTest"));
        if (FParse::Value(Cmd, TEXT("Record="), RecordSeconds))
        {
            bRecording = true;
            FParse::Value(Cmd, TEXT("RecordFps="), RecordFps);
            FString Name = TEXT("clip");
            FParse::Value(Cmd, TEXT("RecordName="), Name);
            RecordFolder = FPaths::ProjectSavedDir() / TEXT("Video") / Name;
            IFileManager::Get().DeleteDirectory(*RecordFolder, false, true);
            IFileManager::Get().MakeDirectory(*RecordFolder, true);
            RecordTimer = 3.0f;
            bShowOverlay = false;
        }
        if (bGraspTest || bTourTest || bWalkTest)
        {
            bShowOverlay = false;
            bLogChronicle = false;
        }
        if (bWatchTest || bMatterTest)
        {
            FParse::Value(Cmd, TEXT("WatchShots="), WatchShotsTotal);
            FParse::Value(Cmd, TEXT("WatchEvery="), WatchEvery);
            WatchEvery = FMath::Max(1.0f, WatchEvery);
            bShowOverlay = false;
            bLogChronicle = true;
            WatchFolder = FPaths::ProjectSavedDir() / TEXT("WatchTest");
            IFileManager::Get().DeleteDirectory(*WatchFolder, false, true);
            IFileManager::Get().MakeDirectory(*WatchFolder, true);
        }
        if (FApp::CanEverRender() && GEngine)
        {
            GEngine->Exec(GetWorld(), TEXT("r.Shadow.Virtual.Enable 0"));
        }
        if (!FParse::Param(Cmd, TEXT("OldMind")) && (FParse::Param(Cmd, TEXT("AgentsMind")) || !FPaths::FileExists(FLifeSchool::CorePath(4))))
        {
            if (UClass* DirectorClass = StaticLoadClass(AActor::StaticClass(), nullptr, TEXT("/Script/ab.MindLearningDirector")))
            {
                GetWorld()->SpawnActor<AActor>(DirectorClass, FTransform::Identity);
            }
        }
        float StartSpeed = 1.0f;
        if (FParse::Value(Cmd, TEXT("Speed="), StartSpeed))
        {
            Speed(StartSpeed);
        }
    }

    BindHotkeys();

    if (bMatterTest)
    {
        UpdateMatterTest(FApp::GetDeltaTime());
    }
    else if (bGraspTest)
    {
        UpdateGraspTest(FApp::GetDeltaTime());
    }
    else if (bTourTest)
    {
        UpdateTourTest(FApp::GetDeltaTime());
    }
    else if (bWalkTest)
    {
        UpdateWalkTest(FApp::GetDeltaTime());
    }
    else if (bWatchTest)
    {
        UpdateWatchTest(FApp::GetDeltaTime());
    }
    UpdateRecording(FApp::GetDeltaTime());

    UpdateSun();

    if (bShowOverlay)
    {
        OverlayAccumulator += DeltaSeconds;
        if (OverlayAccumulator >= 0.25f)
        {
            OverlayAccumulator = 0.0f;
            UpdateOverlay();
        }
    }

    if (bLogChronicle)
    {
        ChronicleAccumulator += DeltaSeconds;
        if (ChronicleAccumulator >= 1.0f)
        {
            ChronicleAccumulator = 0.0f;
            LogChronicle();
        }

        // Время от времени — подробный разбор одного человека: из чего
        // сложилось его решение и что он успел понять о жизни.
        PortraitAccumulator += DeltaSeconds;
        if (PortraitAccumulator >= PortraitInterval)
        {
            PortraitAccumulator = 0.0f;
            LogPortrait();
        }
    }
}

void AHumanSandboxGameMode::LogPortrait()
{
    UWorld* World = GetWorld();
    UHumanWorldSubsystem* WorldMind = World ? World->GetSubsystem<UHumanWorldSubsystem>() : nullptr;
    if (!WorldMind)
    {
        return;
    }

    const TArray<ACompleteHumanNPC*> All = WorldMind->GetAllHumans();
    if (All.Num() == 0)
    {
        return;
    }

    ACompleteHumanNPC* Subject = All[FMath::RandRange(0, All.Num() - 1)];
    if (!Subject || !Subject->Mind)
    {
        return;
    }

    const FHumanTime& T = WorldMind->Now;
    UE_LOG(LogHumanCity, Warning, TEXT("--- [%02d:%02d] ПОРТРЕТ ---\n%s"),
        T.Hour, T.Minute, *Subject->Mind->GetStatusReport());
}

void AHumanSandboxGameMode::WatchNext()
{
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

    const TArray<ACompleteHumanNPC*> All = WorldMind->GetAllHumans();
    if (All.Num() == 0)
    {
        Watched = nullptr;
        return;
    }

    WatchIndex = (WatchIndex + 1) % All.Num();
    Watched = All[WatchIndex];
}

void AHumanSandboxGameMode::UpdateOverlay()
{
    UWorld* World = GetWorld();
    if (!World || !GEngine)
    {
        return;
    }

    UHumanWorldSubsystem* WorldMind = World->GetSubsystem<UHumanWorldSubsystem>();
    if (!WorldMind)
    {
        return;
    }

    // --- Часы мира ----------------------------------------------------------
    static const TCHAR* WeekDays[] = { TEXT("пн"), TEXT("вт"), TEXT("ср"), TEXT("чт"), TEXT("пт"), TEXT("сб"), TEXT("вс") };
    const FHumanTime& T = WorldMind->Now;

    const FString Clock = FString::Printf(
        TEXT("День %d (%s)  %02d:%02d   %s   погода %.0f%%   людей: %d"),
        T.Day,
        WeekDays[FMath::Clamp(T.DayOfWeek, 0, 6)],
        T.Hour, T.Minute,
        T.bIsNight ? TEXT("ночь") : TEXT("день"),
        WorldMind->Weather * 100.0f,
        WorldMind->GetPopulation());

    GEngine->AddOnScreenDebugMessage(1001, 0.3f, FColor::Cyan,
        FString::Printf(TEXT("%s   скорость x%.2f  (+ быстрее, - медленнее, 0 обычная)"), *Clock, GameSpeed));

    if (UMatterSubsystem* Matter = UMatterSubsystem::Get(this))
    {
        GEngine->AddOnScreenDebugMessage(1004, 0.3f, FColor(180, 220, 255), Matter->DescribeClimate());
    }

    // --- За кем смотрим -----------------------------------------------------
    if (!Watched || !IsValid(Watched))
    {
        // Берём того, кто ближе всего к камере наблюдателя.
        FVector ViewLocation = FVector::ZeroVector;
        if (APlayerController* PC = World->GetFirstPlayerController())
        {
            FRotator ViewRotation;
            PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
        }
        Watched = WorldMind->GetNearestHuman(ViewLocation, 1000000.0f, nullptr);
    }

    if (Watched && IsValid(Watched))
    {
        GEngine->AddOnScreenDebugMessage(1002, 0.3f, FColor::White, Watched->GetStatusReport());
    }

    // --- Последнее заметное событие -----------------------------------------
    if (WorldMind->RecentEvents.Num() > 0)
    {
        const FWorldEvent& Last = WorldMind->RecentEvents.Last();
        GEngine->AddOnScreenDebugMessage(1003, 0.3f, FColor::Orange,
            FString::Printf(TEXT("В городе: %s"), *Last.Description));
    }
}

// ---------------------------------------------------------------------------
//  Летопись
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
//  КОМАНДЫ
//
//  Всё, что создаётся этими командами, — обычные вещи города. Жители
//  замечают их сами: еда, положенная посреди улицы, будет подобрана и
//  съедена тем, кто окажется рядом и проголодается.
// ---------------------------------------------------------------------------

FVector AHumanSandboxGameMode::PointInFront(float Distance) const
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return FVector::ZeroVector;
    }

    if (APlayerController* PC = World->GetFirstPlayerController())
    {
        FVector Location;
        FRotator Rotation;
        PC->GetPlayerViewPoint(Location, Rotation);

        FVector Spot = Location + Rotation.Vector() * Distance;

        // Кладём на землю, а не в воздух.
        FHitResult Hit;
        const FVector Down = Spot - FVector(0.0f, 0.0f, 2000.0f);
        if (World->LineTraceSingleByChannel(Hit, Spot + FVector(0, 0, 200.0f), Down, ECC_WorldStatic))
        {
            Spot = Hit.ImpactPoint + FVector(0.0f, 0.0f, 14.0f);
        }
        return Spot;
    }

    return FVector::ZeroVector;
}

void AHumanSandboxGameMode::SpawnFood(int32 Portions)
{
    SpawnStuff(TEXT("CookedFood"), FMath::Max(1, Portions));
}

void AHumanSandboxGameMode::SpawnWater(int32 Portions)
{
    SpawnStuff(TEXT("Water"), FMath::Max(1, Portions));
}

void AHumanSandboxGameMode::SpawnCoins(int32 Amount)
{
    SpawnStuff(TEXT("Coin"), FMath::Max(1, Amount));
}

void AHumanSandboxGameMode::SpawnStuff(const FString& Kind, int32 Amount)
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    const FString K = Kind.ToLower();
    EResourceKind What = EResourceKind::CookedFood;

    if (K.Contains(TEXT("raw")) || K.Contains(TEXT("продукт")))        What = EResourceKind::RawFood;
    else if (K.Contains(TEXT("cook")) || K.Contains(TEXT("еда")))      What = EResourceKind::CookedFood;
    else if (K.Contains(TEXT("water")) || K.Contains(TEXT("вод")))     What = EResourceKind::Water;
    else if (K.Contains(TEXT("coin")) || K.Contains(TEXT("деньг")))    What = EResourceKind::Coin;
    else if (K.Contains(TEXT("wood")) || K.Contains(TEXT("дров")))     What = EResourceKind::Firewood;
    else if (K.Contains(TEXT("herb")) || K.Contains(TEXT("трав")))     What = EResourceKind::Herb;
    else if (K.Contains(TEXT("seed")) || K.Contains(TEXT("семе")))     What = EResourceKind::Seed;
    else if (K.Contains(TEXT("cloth")) || K.Contains(TEXT("ткан")))    What = EResourceKind::Cloth;

    const FVector Where = PointInFront();
    if (AResourceActor* Made = AResourceActor::Spawn(World, What, float(FMath::Max(1, Amount)), Where))
    {
        UE_LOG(LogHumanCity, Warning, TEXT("Положено: %s (%d) — вон там, на земле"),
               *AResourceActor::KindName(What), Amount);
    }
}

// ---------------------------------------------------------------------------
//  Дать все знания одному человеку — сделать учителя.
//
//  Он читал все книги города и помнит всё, что в них написано. Остальные
//  учатся у него: смотрят, как он работает, спрашивают и повторяют.
// ---------------------------------------------------------------------------
void AHumanSandboxGameMode::MakeScholar()
{
    UWorld* World = GetWorld();
    UHumanWorldSubsystem* WorldMind = World ? World->GetSubsystem<UHumanWorldSubsystem>() : nullptr;
    if (!WorldMind)
    {
        return;
    }

    ACompleteHumanNPC* Best = nullptr;
    float BestScore = -1.0f;
    for (ACompleteHumanNPC* Human : WorldMind->GetAllHumans())
    {
        if (!Human || !Human->Mind || !Human->IdentityComponent || !Human->IsAlive())
        {
            continue;
        }
        if (Human->IdentityComponent->Age < 14.0f)
        {
            continue;   // учителем становится взрослый
        }
        const float Literacy = Human->SpeechComponent ? Human->SpeechComponent->Literacy() : 0.0f;
        const float Score = Human->IdentityComponent->Age + Literacy * 40.0f;
        if (Score > BestScore)
        {
            BestScore = Score;
            Best = Human;
        }
    }

    if (!Best)
    {
        UE_LOG(LogHumanCity, Warning, TEXT("Учить некого: взрослых в городе нет."));
        return;
    }

    Best->Mind->PreloadKnowledge();
    Best->IdentityComponent->bMasterTeacher = true;

    const FString Line = FString::Printf(TEXT("Учитель: %s — он прочёл все книги и покажет остальным."),
        *Best->IdentityComponent->GetFullName());
    UE_LOG(LogHumanCity, Warning, TEXT("%s"), *Line);
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Cyan, Line);
    }
    if (Best->Mind)
    {
        Best->Mind->Think(TEXT("теперь я знаю, как делается всё, о чём здесь написано"),
            EThoughtKind::Judgement, 1.0f);
    }
}

void AHumanSandboxGameMode::SpawnBook(const FString& Subject)
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    FName Key(*Subject);
    if (!FLibrary::Find(Key))
    {
        UE_LOG(LogHumanCity, Warning, TEXT("Нет такой книги: %s. Смотри ListStuff."), *Subject);
        return;
    }

    const FVector Where = PointInFront();
    const FTransform T(FRotator::ZeroRotator, Where);

    ABookActor* Book = World->SpawnActorDeferred<ABookActor>(
        ABookActor::StaticClass(), T, nullptr, nullptr,
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

    if (Book)
    {
        Book->Subject = Key;
        Book->ShelfLocation = Where;
        Book->FinishSpawning(T);
        Book->Setup(Key);

        UE_LOG(LogHumanCity, Warning, TEXT("Положена книга: %s"),
               *FLibrary::Find(Key)->Title);
    }
}

void AHumanSandboxGameMode::ListStuff()
{
    UE_LOG(LogHumanCity, Warning, TEXT("=== ЧТО МОЖНО СОЗДАТЬ ==="));
    UE_LOG(LogHumanCity, Warning, TEXT("SpawnFood 3        — готовая еда"));
    UE_LOG(LogHumanCity, Warning, TEXT("SpawnWater 2       — вода"));
    UE_LOG(LogHumanCity, Warning, TEXT("SpawnCoins 50      — деньги"));
    UE_LOG(LogHumanCity, Warning, TEXT("SpawnStuff RawFood 5 — продукты (ещё: Water, Coin, Firewood, Herb, Seed, Cloth)"));
    UE_LOG(LogHumanCity, Warning, TEXT("ShowTags 0 / 1     — надписи над людьми"));
    UE_LOG(LogHumanCity, Warning, TEXT("--- книги (SpawnBook <ключ>) ---"));

    for (const FTextbook& Book : FLibrary::All())
    {
        UE_LOG(LogHumanCity, Warning, TEXT("SpawnBook %-12s — %s"),
               *Book.Subject.ToString(), *Book.Title);
    }
}

void AHumanSandboxGameMode::ShowTags(int32 bOn)
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    if (UHumanWorldSubsystem* WorldMind = World->GetSubsystem<UHumanWorldSubsystem>())
    {
        for (ACompleteHumanNPC* Human : WorldMind->GetAllHumans())
        {
            if (Human)
            {
                Human->bShowTags = (bOn != 0);
            }
        }
    }
    UE_LOG(LogHumanCity, Warning, TEXT("Надписи над людьми: %s"), bOn ? TEXT("видны") : TEXT("скрыты"));
}

void AHumanSandboxGameMode::LogChronicle()
{
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

    const FHumanTime& T = WorldMind->Now;

    // Мысли всех, кто сейчас о чём-то думает.
    for (ACompleteHumanNPC* Human : WorldMind->GetAllHumans())
    {
        if (!Human || !Human->Mind || !Human->IdentityComponent)
        {
            continue;
        }

        UE_LOG(LogHumanCity, Log, TEXT("[%02d:%02d] %-10s | %-26s | %s"),
            T.Hour, T.Minute,
            *Human->IdentityComponent->FirstName,
            *Human->Mind->CurrentActionLabel,
            *Human->Mind->CurrentThought);
    }

    // Раз в игровой час — с кем в городе считаются больше всех.
    static int32 LastStandingHour = -1;
    if (T.Hour != LastStandingHour)
    {
        LastStandingHour = T.Hour;

        TArray<ACompleteHumanNPC*> Ranked = WorldMind->GetAllHumans();
        Ranked.RemoveAll([](const ACompleteHumanNPC* H) { return !H || !H->IdentityComponent; });
        Ranked.Sort([](const ACompleteHumanNPC& A, const ACompleteHumanNPC& B)
        {
            return A.IdentityComponent->Standing > B.IdentityComponent->Standing;
        });

        FString Line;
        for (int32 i = 0; i < FMath::Min(3, Ranked.Num()); ++i)
        {
            ACompleteHumanNPC* Human = Ranked[i];
            int32 Respecting = 0;
            for (ACompleteHumanNPC* Other : WorldMind->GetAllHumans())
            {
                const FRelationship* R = (Other && Other != Human && Other->SocialComponent)
                    ? Other->SocialComponent->Find(Human) : nullptr;
                if (R && R->Respect >= 0.62f)
                {
                    ++Respecting;
                }
            }
            Line += FString::Printf(TEXT("%s %.0f%% (уважают %d, советовались %d); "),
                *Human->IdentityComponent->FirstName, Human->IdentityComponent->Standing * 100.0f,
                Respecting, Human->IdentityComponent->TimesAskedForAdvice);
        }
        UE_LOG(LogHumanCity, Warning, TEXT("[%02d:00] С кем считаются: %s"), T.Hour, *Line);
    }

    // Новые события города.
    const int32 Total = WorldMind->RecentEvents.Num();
    for (int32 i = FMath::Max(0, LastEventCount); i < Total; ++i)
    {
        UE_LOG(LogHumanCity, Warning, TEXT("[%02d:%02d] СОБЫТИЕ: %s"),
            T.Hour, T.Minute, *WorldMind->RecentEvents[i].Description);
    }
    LastEventCount = Total;
}

FString AHumanSandboxGameMode::GetCityDigest() const
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return FString();
    }

    UHumanWorldSubsystem* WorldMind = World->GetSubsystem<UHumanWorldSubsystem>();
    if (!WorldMind)
    {
        return FString();
    }

    FString Digest;
    for (ACompleteHumanNPC* Human : WorldMind->GetAllHumans())
    {
        if (!Human || !Human->IdentityComponent || !Human->Mind)
        {
            continue;
        }

        const UEmotionComponent* Emo = Human->EmotionComponent;
        Digest += FString::Printf(TEXT("%-10s %2d  %-14s  %-22s  %s\n"),
            *Human->IdentityComponent->FirstName,
            FMath::FloorToInt(Human->IdentityComponent->Age),
            *Human->IdentityComponent->Occupation,
            Emo ? *HumanText::Emotion(Emo->GetDominant()) : TEXT("—"),
            *Human->Mind->CurrentThought);
    }
    return Digest;
}
