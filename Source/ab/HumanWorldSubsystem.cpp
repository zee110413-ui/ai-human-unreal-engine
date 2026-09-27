// HumanWorldSubsystem.cpp

#include "HumanWorldSubsystem.h"
#include "CompleteHumanAI.h"
#include "AffordanceComponent.h"
#include "Engine/World.h"
#include "Algo/Reverse.h"
#include "Misc/Parse.h"

void UHumanWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // Мир просыпается в 8 утра первого дня.
    WorldSeconds = 8.0f * 3600.0f;
    float RequestedSpeed = GameMinutesPerRealSecond;
    if (FParse::Value(FCommandLine::Get(), TEXT("HumanSpeed="), RequestedSpeed))
    {
        GameMinutesPerRealSecond = FMath::Clamp(RequestedSpeed, 0.1f, 120.0f);
    }
    RecalculateCalendar();

    Weather = FMath::FRandRange(0.0f, 0.4f);
    IdCounter = 1;
}

void UHumanWorldSubsystem::Deinitialize()
{
    Humans.Reset();
    ClaimedNames.Reset();
    RecentEvents.Reset();
    PublicPlaces.Reset();
    AffordanceSources.Reset();
    Super::Deinitialize();
}

bool UHumanWorldSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
    // Подсистема нужна только в игре и в PIE, но не в редакторе-превью.
    return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UHumanWorldSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UHumanWorldSubsystem, STATGROUP_Tickables);
}

void UHumanWorldSubsystem::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // --- часы идут ---
    WorldSeconds += RealToGameSeconds(DeltaTime);
    RecalculateCalendar();

    // --- подчищаем реестр от умерших ---
    for (int32 i = Humans.Num() - 1; i >= 0; --i)
    {
        if (!Humans[i].IsValid())
        {
            Humans.RemoveAtSwap(i);
        }
    }
}

void UHumanWorldSubsystem::RecalculateCalendar()
{
    const float SecondsInDay = 86400.0f;

    const int32 DayIndex = FMath::FloorToInt(WorldSeconds / SecondsInDay);
    const float DaySeconds = WorldSeconds - DayIndex * SecondsInDay;

    Now.Day = DayIndex + 1;
    Now.HourFloat = DaySeconds / 3600.0f;
    Now.Hour = FMath::Clamp(FMath::FloorToInt(Now.HourFloat), 0, 23);
    Now.Minute = FMath::Clamp(FMath::FloorToInt((Now.HourFloat - Now.Hour) * 60.0f), 0, 59);
    Now.DayOfWeek = DayIndex % 7;
    Now.bIsNight = (Now.HourFloat < 6.0f) || (Now.HourFloat >= 22.0f);
    Now.bIsWeekend = (Now.DayOfWeek >= 5);
}

int32 UHumanWorldSubsystem::NextUniqueId()
{
    return IdCounter++;
}

// ---------------------------------------------------------------------------
//  Реестр людей
// ---------------------------------------------------------------------------

void UHumanWorldSubsystem::RegisterHuman(ACompleteHumanNPC* Human)
{
    if (!Human)
    {
        return;
    }
    Humans.AddUnique(Human);
}

void UHumanWorldSubsystem::UnregisterHuman(ACompleteHumanNPC* Human)
{
    if (!Human)
    {
        return;
    }
    Humans.RemoveAllSwap([Human](const TWeakObjectPtr<ACompleteHumanNPC>& Ptr)
    {
        return !Ptr.IsValid() || Ptr.Get() == Human;
    });
}

TArray<ACompleteHumanNPC*> UHumanWorldSubsystem::GetAllHumans() const
{
    TArray<ACompleteHumanNPC*> Result;
    Result.Reserve(Humans.Num());
    for (const TWeakObjectPtr<ACompleteHumanNPC>& Ptr : Humans)
    {
        if (ACompleteHumanNPC* H = Ptr.Get())
        {
            Result.Add(H);
        }
    }
    return Result;
}

TArray<ACompleteHumanNPC*> UHumanWorldSubsystem::GetHumansNear(const FVector& Origin, float Radius, const AActor* Exclude) const
{
    struct FCandidate
    {
        ACompleteHumanNPC* Human;
        float DistSq;
    };

    const float RadiusSq = Radius * Radius;
    TArray<FCandidate> Candidates;

    for (const TWeakObjectPtr<ACompleteHumanNPC>& Ptr : Humans)
    {
        ACompleteHumanNPC* H = Ptr.Get();
        if (!H || H == Exclude)
        {
            continue;
        }
        const float DistSq = FVector::DistSquared(Origin, H->GetActorLocation());
        if (DistSq <= RadiusSq)
        {
            Candidates.Add({ H, DistSq });
        }
    }

    Candidates.Sort([](const FCandidate& A, const FCandidate& B) { return A.DistSq < B.DistSq; });

    TArray<ACompleteHumanNPC*> Result;
    Result.Reserve(Candidates.Num());
    for (const FCandidate& C : Candidates)
    {
        Result.Add(C.Human);
    }
    return Result;
}

ACompleteHumanNPC* UHumanWorldSubsystem::GetNearestHuman(const FVector& Origin, float MaxRadius, const AActor* Exclude) const
{
    ACompleteHumanNPC* Best = nullptr;
    float BestDistSq = MaxRadius * MaxRadius;

    for (const TWeakObjectPtr<ACompleteHumanNPC>& Ptr : Humans)
    {
        ACompleteHumanNPC* H = Ptr.Get();
        if (!H || H == Exclude)
        {
            continue;
        }
        const float DistSq = FVector::DistSquared(Origin, H->GetActorLocation());
        if (DistSq < BestDistSq)
        {
            BestDistSq = DistSq;
            Best = H;
        }
    }
    return Best;
}

bool UHumanWorldSubsystem::TryClaimName(const FString& FullName)
{
    if (ClaimedNames.Contains(FullName))
    {
        return false;
    }
    ClaimedNames.Add(FullName);
    return true;
}

// ---------------------------------------------------------------------------
//  Места города
// ---------------------------------------------------------------------------

void UHumanWorldSubsystem::RegisterPlace(EPlaceKind Kind, const FVector& Location, const FString& Label)
{
    // Слишком близкие точки одного типа считаем одним местом.
    for (const FKnownLocation& P : PublicPlaces)
    {
        if (P.Kind == Kind && FVector::DistSquared(P.Location, Location) < 1000000.0f) // ~10 м
        {
            return;
        }
    }

    FKnownLocation New;
    New.Kind = Kind;
    New.Location = Location;
    New.Label = Label.IsEmpty() ? HumanText::Place(Kind) : Label;
    New.Familiarity = 1.0f;
    New.bFirsthand = true;
    PublicPlaces.Add(New);
}

TArray<FKnownLocation> UHumanWorldSubsystem::GetPlacesNear(const FVector& Origin, float Radius) const
{
    TArray<FKnownLocation> Result;
    const float RadiusSq = Radius * Radius;

    for (const FKnownLocation& P : PublicPlaces)
    {
        if (FVector::DistSquared(P.Location, Origin) <= RadiusSq)
        {
            Result.Add(P);
        }
    }
    return Result;
}

// ---------------------------------------------------------------------------
//  Аффордансы
// ---------------------------------------------------------------------------

void UHumanWorldSubsystem::RegisterAffordanceSource(UAffordanceComponent* Source)
{
    if (Source)
    {
        AffordanceSources.AddUnique(Source);
    }
}

void UHumanWorldSubsystem::UnregisterAffordanceSource(UAffordanceComponent* Source)
{
    if (!Source)
    {
        return;
    }
    AffordanceSources.RemoveAllSwap([Source](const TWeakObjectPtr<UAffordanceComponent>& Ptr)
    {
        return !Ptr.IsValid() || Ptr.Get() == Source;
    });
}

void UHumanWorldSubsystem::GatherAffordancesNear(const FVector& Origin, float Radius, TArray<FAffordance>& Out) const
{
    for (const TWeakObjectPtr<UAffordanceComponent>& Ptr : AffordanceSources)
    {
        UAffordanceComponent* Source = Ptr.Get();
        if (!Source || !Source->GetOwner())
        {
            continue;
        }

        const float Reach = FMath::Min(Radius, Source->NoticeRadius);
        if (FVector::DistSquared(Origin, Source->GetOwner()->GetActorLocation()) > Reach * Reach)
        {
            continue;
        }

        const int32 Before = Out.Num();
        Source->CollectOffers(Out);

        // Закрытое сейчас не предлагается вовсе. Человеку не нужно знать
        // «правило рабочего дня» — в три часа ночи такой возможности
        // просто нет, как нет её и в выключенном свете.
        for (int32 i = Out.Num() - 1; i >= Before; --i)
        {
            if (!IsAffordanceOpenNow(Out[i]))
            {
                Out.RemoveAt(i);
            }
        }
    }
}

bool UHumanWorldSubsystem::IsAffordanceOpenNow(const FAffordance& A) const
{
    if (FMath::IsNearlyEqual(A.AvailableFromHour, A.AvailableToHour))
    {
        return true; // круглосуточно
    }

    const float H = Now.HourFloat;

    if (A.AvailableFromHour < A.AvailableToHour)
    {
        return H >= A.AvailableFromHour && H < A.AvailableToHour;
    }
    // Промежуток через полночь.
    return H >= A.AvailableFromHour || H < A.AvailableToHour;
}

void UHumanWorldSubsystem::CollectOffersNear(const FVector& Location, float Radius, TArray<FAffordance>& Out) const
{
    const float RadiusSq = Radius * Radius;

    for (const TWeakObjectPtr<UAffordanceComponent>& Ptr : AffordanceSources)
    {
        UAffordanceComponent* Source = Ptr.Get();
        if (!Source || !Source->GetOwner())
        {
            continue;
        }
        if (FVector::DistSquared(Location, Source->GetOwner()->GetActorLocation()) <= RadiusSq)
        {
            Source->CollectOffers(Out);
        }
    }
}

UAffordanceComponent* UHumanWorldSubsystem::FindSourceAt(const FVector& Location, float Tolerance) const
{
    const float ToleranceSq = Tolerance * Tolerance;

    for (const TWeakObjectPtr<UAffordanceComponent>& Ptr : AffordanceSources)
    {
        UAffordanceComponent* Source = Ptr.Get();
        if (!Source || !Source->GetOwner())
        {
            continue;
        }
        if (FVector::DistSquared(Location, Source->GetOwner()->GetActorLocation()) <= ToleranceSq)
        {
            return Source;
        }
    }
    return nullptr;
}


TArray<FKnownLocation> UHumanWorldSubsystem::GetPlacesOfKind(EPlaceKind Kind) const
{
    TArray<FKnownLocation> Result;
    for (const FKnownLocation& P : PublicPlaces)
    {
        if (P.Kind == Kind)
        {
            Result.Add(P);
        }
    }
    return Result;
}

// ---------------------------------------------------------------------------
//  События мира
// ---------------------------------------------------------------------------

void UHumanWorldSubsystem::BroadcastEvent(const FWorldEvent& Event)
{
    FWorldEvent Stamped = Event;
    Stamped.Time = WorldSeconds;

    RecentEvents.Add(Stamped);
    while (RecentEvents.Num() > MaxRecentEvents)
    {
        RecentEvents.RemoveAt(0);
    }

    // Доставляем всем, кто достаточно близко, чтобы заметить.
    const float RadiusSq = Stamped.Radius * Stamped.Radius;
    for (const TWeakObjectPtr<ACompleteHumanNPC>& Ptr : Humans)
    {
        ACompleteHumanNPC* H = Ptr.Get();
        if (!H)
        {
            continue;
        }
        if (FVector::DistSquared(Stamped.Location, H->GetActorLocation()) <= RadiusSq)
        {
            H->PerceiveWorldEvent(Stamped);
        }
    }
}
