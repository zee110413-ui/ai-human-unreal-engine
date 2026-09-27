// PlaceActor.cpp

#include "PlaceActor.h"
#include "AffordanceComponent.h"
#include "HumanWorldSubsystem.h"

#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"

APlaceActor::APlaceActor()
{
    PrimaryActorTick.bCanEverTick = false;

    Marker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Marker"));
    RootComponent = Marker;
    Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Marker->SetCanEverAffectNavigation(false);
    Marker->SetRelativeScale3D(FVector(1.2f, 1.2f, 0.35f));
    {
        static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
        if (CubeMesh.Succeeded())
        {
            Marker->SetStaticMesh(CubeMesh.Object);
        }
    }

    Affordances = CreateDefaultSubobject<UAffordanceComponent>(TEXT("Affordances"));
}

void APlaceActor::BeginPlay()
{
    Super::BeginPlay();

    if (Marker)
    {
        Marker->SetVisibility(bShowMarker);
    }

    if (!Affordances)
    {
        return;
    }

    if (PlaceName.IsEmpty())
    {
        PlaceName = HumanText::Place(Kind);
    }
    Affordances->DisplayName = PlaceName;
    Affordances->Capacity = Capacity;

    // Место само знает, что оно из себя представляет.
    if (bUseTypicalOffers && Affordances->Offers.Num() == 0)
    {
        Affordances->MakeTypicalFor(Kind);
    }

    // Заносим себя на карту мира — но это не значит, что люди уже знают
    // о нас: каждый должен пройти мимо сам.
    if (UWorld* World = GetWorld())
    {
        if (UHumanWorldSubsystem* WorldMind = World->GetSubsystem<UHumanWorldSubsystem>())
        {
            WorldMind->RegisterPlace(Kind, GetActorLocation(), PlaceName);
        }
    }
}

void APlaceActor::Configure(EPlaceKind InKind, const FString& InName, int32 InCapacity)
{
    Kind = InKind;
    PlaceName = InName;
    Capacity = InCapacity;

    if (Affordances)
    {
        Affordances->DisplayName = InName;
        Affordances->Capacity = InCapacity;
        Affordances->Offers.Reset();
        Affordances->MakeTypicalFor(InKind);
    }
}
