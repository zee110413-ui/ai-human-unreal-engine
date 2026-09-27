#include "MindComponent.h"
#include "CompleteHumanAI.h"
#include "Crafts.h"
#include "Village.h"
#include "FurnitureActor.h"
#include "ResourceActor.h"
#include "ClothingActor.h"
#include "MemoryComponent.h"
#include "NeedComponent.h"
#include "PhysiologyComponent.h"
#include "HumanWorldSubsystem.h"
#include "EngineUtils.h"

namespace
{
    float StockAround(UWorld* World, const FVector& Here, EResourceKind Kind, float Radius)
    {
        if (!World || Kind == EResourceKind::None)
        {
            return 0.0f;
        }

        float Total = 0.0f;
        for (TActorIterator<AFurnitureActor> It(World); It; ++It)
        {
            AFurnitureActor* Thing = *It;
            if (Thing && FVector::Dist(Thing->GetActorLocation(), Here) <= Radius)
            {
                if (const float* Stock = Thing->Stored.Find(Kind))
                {
                    Total += *Stock;
                }
            }
        }
        for (TActorIterator<AResourceActor> It(World); It; ++It)
        {
            AResourceActor* Pile = *It;
            if (Pile && Pile->Kind == Kind && Pile->Amount > 0.0f
                && FVector::Dist(Pile->GetActorLocation(), Here) <= Radius)
            {
                Total += Pile->Amount;
            }
        }
        return Total;
    }

    float SpendAround(UWorld* World, const FVector& Here, EResourceKind Kind, float HowMuch, float Radius)
    {
        if (!World || Kind == EResourceKind::None || HowMuch <= 0.0f)
        {
            return 0.0f;
        }

        float Left = HowMuch;
        for (TActorIterator<AFurnitureActor> It(World); It && Left > 0.0f; ++It)
        {
            AFurnitureActor* Thing = *It;
            if (Thing && FVector::Dist(Thing->GetActorLocation(), Here) <= Radius)
            {
                Left -= Thing->TakeOut(Kind, Left);
            }
        }
        for (TActorIterator<AResourceActor> It(World); It && Left > 0.0f; ++It)
        {
            AResourceActor* Pile = *It;
            if (Pile && Pile->Kind == Kind && Pile->Amount > 0.0f
                && FVector::Dist(Pile->GetActorLocation(), Here) <= Radius)
            {
                Left -= Pile->Consume(Left);
            }
        }
        return HowMuch - FMath::Max(0.0f, Left);
    }
}

bool UMindComponent::HoldsThing(EResourceKind Kind) const
{
    const ACompleteHumanNPC* Me = GetHuman();
    if (!Me || !Me->CarriedItem)
    {
        return false;
    }
    const AResourceActor* Held = Cast<AResourceActor>(Me->CarriedItem.Get());
    return Held && Held->Kind == Kind;
}

float UMindComponent::RecallOfDeed(FName Deed) const
{
    if (!Memory || Deed.IsNone())
    {
        return 0.0f;
    }
    return Memory->GetBeliefConfidence(Deed, TEXT("HowTo"));
}

float UMindComponent::PerformCraft(const FAffordance& A, float T)
{
    ACompleteHumanNPC* Me = GetHuman();
    UWorld* World = Me ? Me->GetWorld() : nullptr;
    const FCraft* Deed = FCraftBook::Find(A.Craft);
    if (!Me || !World || !Deed)
    {
        return 0.4f;
    }

    const FVector Here = Me->GetActorLocation();
    AFurnitureActor* Station = Cast<AFurnitureActor>(A.Target.Get());
    const FVillageWork* Chore = FVillage::FindWork(Deed->Id);
    const float Reach = Chore ? 1200.0f : 800.0f;

    for (const FCraftPart& Need : Deed->Inputs)
    {
        if (Need.Amount <= 0.0f)
        {
            continue;
        }
        const float Have = Station ? Station->MaterialNearby(Need.Kind, Reach) : StockAround(World, Here, Need.Kind, Reach);
        if (Have < Need.Amount)
        {
            Report(FString::Printf(TEXT("взялся(ась) %s, а %s нет"), *Deed->Label,
                   *FCraftBook::NameOfKind(Need.Kind)));
            Think(FString::Printf(TEXT("без этого никак: %s"), *FCraftBook::NameOfKind(Need.Kind)),
                  EThoughtKind::Judgement, 0.5f);
            return 0.2f;
        }
    }

    const float Recall = Chore && Deed->Skill.IsNone() ? 1.0f : FMath::Max(RecallOfDeed(Deed->Id), Deed->Skill.IsNone() ? 0.0f : Mastery(Deed->Skill));
    const bool bTooled = Deed->Tool == EResourceKind::None || HoldsThing(Deed->Tool) || FVillage::HasTool(Me, Deed->Tool);
    const float Impairment = Body ? Body->GetCognitiveImpairment() : 0.0f;

    const float Chance = Chore
        ? FMath::Clamp(0.3f + Recall * 0.9f + (bTooled ? 0.1f : -0.4f) - Deed->Difficulty * 0.4f - Impairment * 0.3f, 0.02f, 0.97f)
        : FMath::Clamp(0.05f + Recall * 0.8f + (bTooled ? 0.15f : -0.3f) - Deed->Difficulty * 0.35f - Impairment * 0.3f, 0.02f, 0.95f);

    const bool bSuccess = FMath::FRand() < Chance;
    const float Margin = bSuccess ? Chance - 0.5f : 0.0f;

    if (!bSuccess)
    {
        for (const FCraftPart& Need : Deed->Inputs)
        {
            if (Need.Amount > 0.0f)
            {
                const float Wasted = Need.Amount * 0.5f;
                if (Station)
                {
                    Station->SpendNearby(Need.Kind, Wasted, Reach);
                }
                else
                {
                    SpendAround(World, Here, Need.Kind, Wasted, Reach);
                }
            }
        }
        if (Chore && !Deed->Skill.IsNone())
        {
            Practised(Deed->Skill, 0.01f);
        }

        if (Memory)
        {
            FBelief Tried;
            Tried.Subject = Deed->Id;
            Tried.Predicate = TEXT("HowTo");
            Tried.Value = 1.0f;
            Tried.Confidence = FMath::Min(0.9f, Recall + 0.06f);
            Tried.LearnedAt = T;
            Tried.bVerified = true;
            Tried.Text = Deed->Label;
            Memory->Learn(Tried, 1.0f, 0.8f, T);
        }

        if (!bTooled)
        {
            Report(FString::Printf(TEXT("пробовал(а) %s голыми руками — не вышло"), *Deed->Label));
            Think(FString::Printf(TEXT("без инструмента это не берётся: нужен %s"),
                  *FCraftBook::NameOfKind(Deed->Tool)), EThoughtKind::Judgement, 0.7f);
        }
        else
        {
            Report(FString::Printf(TEXT("взялся(ась) %s — испортил(а)"), *Deed->Label));
            Think(Recall > 0.3f ? TEXT("читал(а), а руки ещё не слушаются")
                                : TEXT("делаю наугад, оттого и выходит плохо"),
                  EThoughtKind::SelfTalk, 0.6f);
        }

        return 0.25f;
    }

    for (const FCraftPart& Need : Deed->Inputs)
    {
        if (Need.Amount > 0.0f)
        {
            if (Station)
            {
                Station->SpendNearby(Need.Kind, Need.Amount, Reach);
            }
            else
            {
                SpendAround(World, Here, Need.Kind, Need.Amount, Reach);
            }
        }
    }

    const FVector Spot = Here + Me->GetActorForwardVector() * 120.0f;

    if (Chore)
    {
        EResourceKind Made = Deed->Output;
        if (Station && Deed->Inputs.Num() == 0 && Station->Substance != EResourceKind::None
            && (Chore->Id == TEXT("v_stone") || Chore->Id == TEXT("v_clay")))
        {
            Made = Station->Substance;
        }
        const float Skill = FMath::Clamp(Recall, 0.0f, 1.0f);
        const float Yield = FMath::Max(1.0f, FMath::RoundToFloat(Deed->OutputAmount * FMath::Lerp(0.5f, 1.25f, Skill)));
        if (Station && Station->FurnitureType == EFurnitureType::GardenBed)
        {
            if (Chore->Output == EResourceKind::None)
            {
                Station->Ripeness = 0.03f;   // посев: грядка занята всходами
            }
            else if (Chore->Input == EResourceKind::None)
            {
                Station->Ripeness = 0.0f;    // сбор урожая: грядка пуста
            }
            Station->RefreshLook();
            Station->BuildOffers();
            Station->RefreshCraftOffers();
        }
        if (Made != EResourceKind::None)
        {
            const bool bNearHome = Me->HasHome() && FVector::Dist2D(Here, Me->GetHomeLocation()) < 1100.0f;
            AFurnitureActor* Keep = bNearHome ? FVillage::HomeStore(Me, Made) : nullptr;
            if (Keep)
            {
                Keep->Store(Made, Yield);
                Report(FString::Printf(TEXT("%s: вышло %s ×%d, убрал(а) — %s"), *Deed->Label,
                    *FCraftBook::NameOfKind(Made), FMath::RoundToInt(Yield), *Keep->Name));
            }
            else if (TakeCargo(Made, Yield))
            {
                Report(FString::Printf(TEXT("%s: вышло %s ×%d (цена %s), несёт с собой"), *Deed->Label,
                    *FCraftBook::NameOfKind(Made), FMath::RoundToInt(Yield), *FVillage::Coins(Yield * FVillage::PriceOf(Made))));
            }
        }
        if (!Deed->Skill.IsNone())
        {
            Practised(Deed->Skill, 0.03f);
        }
    }
    else if (Deed->Wears != EClothingKind::None)
    {
        if (AClothingActor* Made = AClothingActor::Spawn(World, Deed->Wears, FString(),
                FVector(Spot.X, Spot.Y, Here.Z - 40.0f)))
        {
            Me->Wear(Made);
        }
    }
    else if (Deed->bBuilds)
    {
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        if (AFurnitureActor* Made = World->SpawnActor<AFurnitureActor>(AFurnitureActor::StaticClass(),
            FVector(Spot.X, Spot.Y, Here.Z - 88.0f), FRotator(0.0f, Me->GetActorRotation().Yaw + 180.0f, 0.0f), Params))
        {
            Made->FurnitureType = Deed->Builds;
            Made->BuildLook();
            Made->BuildOffers();
            Made->MakeSolid();
        }
    }
    else if (Deed->Output != EResourceKind::None)
    {
        AResourceActor::Spawn(World, Deed->Output, Deed->OutputAmount, FVector(Spot.X, Spot.Y, Here.Z - 60.0f));
    }

    if (Memory && !Chore)
    {
        FBelief Done;
        Done.Subject = Deed->Id;
        Done.Predicate = TEXT("HowTo");
        Done.Value = 1.0f;
        Done.Confidence = FMath::Min(1.0f, FMath::Max(Recall + 0.12f, 0.45f));
        Done.LearnedAt = T;
        Done.bVerified = true;
        Done.Text = Deed->Label;
        Memory->Learn(Done, 1.0f, 0.8f, T);
    }

    if (Needs)
    {
        Needs->Satisfy(ENeedType::Competence, 0.2f);
        Needs->Satisfy(ENeedType::Meaning, 0.12f);
    }

    if (!Chore)
    {
        Report(FString::Printf(TEXT("сделал(а): %s"), *Deed->Label));
    }
    Think(Chore ? (Recall > 0.7f ? TEXT("руки сами знают, как это делается") : TEXT("вышло, не зря старался(ась)"))
                : (Recall > 0.3f ? TEXT("вышло так, как написано в книге") : TEXT("вышло. Теперь знаю, как это делается")),
          EThoughtKind::Judgement, 0.8f);

    if (ACompleteHumanNPC* Human = GetHuman())
    {
        UHumanWorldSubsystem* WorldMind = World->GetSubsystem<UHumanWorldSubsystem>();
        if (WorldMind && Chore && !Deed->Skill.IsNone())
        {
            for (ACompleteHumanNPC* Other : WorldMind->GetHumansNear(Here, 700.0f, Human))
            {
                if (Other && Other->Mind && Other->HasLineOfSight(Human))
                {
                    Other->Mind->SawSomeoneDo(Deed->Skill, Recall, 0.5f);
                    Other->Mind->SawSomeoneDo(Deed->Id, Recall, 0.75f);
                    Other->Mind->JudgeByDeeds(Human, Recall > 0.8f ? 0.04f : 0.02f,
                        Recall > 0.8f ? FString::Printf(TEXT("мастер: %s"), *Deed->Label) : FString(TEXT("работящий")));
                }
            }
        }
        else if (WorldMind)
        {
            for (ACompleteHumanNPC* Other : WorldMind->GetHumansNear(Here, 700.0f, Human))
            {
                if (Other && Other->Mind && Other->MemoryComponent && Other->HasLineOfSight(Human))
                {
                    const float Seen = Other->Mind->RecallOfDeed(Deed->Id);

                    FBelief Watched;
                    Watched.Subject = Deed->Id;
                    Watched.Predicate = TEXT("HowTo");
                    Watched.Value = 1.0f;
                    Watched.Confidence = FMath::Min(0.75f, Seen + 0.18f);
                    Watched.Source = Human;
                    Watched.LearnedAt = T;
                    Watched.Text = Deed->Label;
                    Other->MemoryComponent->Learn(Watched, 0.8f, 0.7f, T);

                    Other->Mind->JudgeByDeeds(Human, 0.03f, TEXT("умеет делать вещи"));
                }
            }
        }
    }

    return 1.0f;
}

void UMindComponent::GatherHandCrafts(TArray<FAffordance>& Out) const
{
    const ACompleteHumanNPC* Me = GetHuman();
    UWorld* World = Me ? Me->GetWorld() : nullptr;
    if (!Me || !World)
    {
        return;
    }

    TArray<const FCraft*> Loose;
    FCraftBook::Anywhere(Loose);
    if (Loose.Num() == 0)
    {
        return;
    }

    const FVector Here = Me->GetActorLocation();

    for (const FCraft* Deed : Loose)
    {
        if (Deed->Tool != EResourceKind::None && !HoldsThing(Deed->Tool))
        {
            continue;
        }

        bool bHaveAll = true;
        for (const FCraftPart& Need : Deed->Inputs)
        {
            if (Need.Amount > 0.0f && StockAround(World, Here, Need.Kind, 800.0f) < Need.Amount)
            {
                bHaveAll = false;
                break;
            }
        }
        if (!bHaveAll)
        {
            continue;
        }

        FAffordance Offer;
        Offer.Action = EActionType::Work;
        Offer.Source = EAffordanceSource::Self;
        Offer.Craft = Deed->Id;
        Offer.Label = Deed->Label;
        Offer.Key = Deed->Id;
        Offer.CategoryKey = Deed->Id;
        Offer.Location = Here;
        Offer.bHasLocation = true;
        Offer.Duration = Deed->Duration;
        Offer.EffortCost = Deed->Effort;
        Offer.Difficulty = Deed->Difficulty;

        FNeedPromise Made;
        Made.Need = ENeedType::Competence;
        Made.Amount = 0.18f;
        Offer.Promises.Add(Made);

        Out.Add(Offer);
    }
}
