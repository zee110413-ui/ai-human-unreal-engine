// BookActor.cpp

#include "BookActor.h"
#include "AffordanceComponent.h"
#include "Textbook.h"
#include "HumanWorldSubsystem.h"
#include "ImportedModels.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

ABookActor::ABookActor()
{
    PrimaryActorTick.bCanEverTick = false;

    Cover = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Cover"));
    RootComponent = Cover;
    Cover->SetMobility(EComponentMobility::Movable);

    Pages = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Pages"));
    Pages->SetupAttachment(Cover);
    Pages->SetMobility(EComponentMobility::Movable);

    Affordances = CreateDefaultSubobject<UAffordanceComponent>(TEXT("Affordances"));
}

void ABookActor::BeginPlay()
{
    Super::BeginPlay();

    if (ShelfLocation.IsZero())
    {
        ShelfLocation = GetActorLocation();
    }
    if (Affordances && Affordances->Offers.Num() == 0)
    {
        Setup(Subject);
    }
}

void ABookActor::Setup(FName InSubject)
{
    Subject = InSubject;
    const FTextbook* Book = FLibrary::Find(Subject);
    const FString Title = Book ? Book->Title : TEXT("книга");

    // --- Вид ---------------------------------------------------------------
    TArray<UStaticMesh*> Volumes;
    FImportedModels::Gather(TEXT("/Game/Imported/PolyHaven/book_encyclopedia_set_01"), TEXT("book_encyclopedia_set_01_book01"), Volumes);
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (Volumes.Num() > 0)
    {
        Cover->SetStaticMesh(Volumes[0]);
        Cover->SetRelativeScale3D(FVector::OneVector);
        Pages->SetVisibility(false);
    }
    else if (Cube)
    {
        Cover->SetStaticMesh(Cube);
        Cover->SetRelativeScale3D(FVector(0.22f, 0.16f, 0.04f));   // 22 x 16 x 4 см

        Pages->SetStaticMesh(Cube);
        Pages->SetRelativeScale3D(FVector(0.92f, 0.92f, 0.55f));
        Pages->SetRelativeLocation(FVector(2.0f, 0.0f, 0.6f));
    }

    // Обложки разного цвета по предмету — так книгу узнают, не открывая.
    const TCHAR* CoverMat = TEXT("M_Wood_Walnut");
    if (Book)
    {
        if (Book->Course == TEXT("русский язык"))  CoverMat = TEXT("M_Brick_Clay_Old");
        else if (Book->Course == TEXT("математика")) CoverMat = TEXT("M_Metal_Steel");
    }
    UMaterialInterface* M = Volumes.Num() > 0 ? nullptr : LoadObject<UMaterialInterface>(nullptr,
            *FString::Printf(TEXT("/Game/StarterContent/Materials/%s.%s"), CoverMat, CoverMat));
    if (M)
    {
        Cover->SetMaterial(0, M);
    }
    if (UMaterialInterface* P = LoadObject<UMaterialInterface>(nullptr,
            TEXT("/Game/StarterContent/Materials/M_Basic_Wall.M_Basic_Wall")))
    {
        Pages->SetMaterial(0, P);
    }

    // --- Физика ------------------------------------------------------------
    // Книга — предмет: её можно уронить и сдвинуть. Но пока она стоит на
    // полке, она стоит: у полок нет физической толщины, и книга, начни она
    // падать, провалилась бы сквозь них и укатилась неизвестно куда.
    Cover->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Cover->SetCollisionObjectType(ECC_PhysicsBody);
    Cover->SetCollisionResponseToAllChannels(ECR_Block);
    Cover->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    Cover->SetSimulatePhysics(false);
    Cover->SetMassOverrideInKg(NAME_None, 0.4f);

    Pages->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // --- Что с ней можно сделать -------------------------------------------
    Affordances->DisplayName = Title;
    Affordances->Category = Book ? Book->Course : TEXT("книга");
    Affordances->NoticeRadius = 700.0f;
    Affordances->Capacity = 1;
    Affordances->Offers.Reset();

    Affordances->AddOffer(EActionType::Study,
        FString::Printf(TEXT("читать: %s"), *Title), 1800.0f);
    Affordances->AddPromise(ENeedType::Competence, 0.45f);
    Affordances->AddPromise(ENeedType::Novelty, 0.3f);
    Affordances->AddPromise(ENeedType::Meaning, 0.2f);
    Affordances->Offers.Last().RequiredSkill = TEXT("Reading");
    Affordances->Offers.Last().Difficulty = Book ? Book->RequiredReading : 0.25f;
    Affordances->Offers.Last().EffortCost = 0.1f + (Book ? Book->Difficulty : 0.4f) * 0.2f;
    // Книгу надо взять в руки — само чтение начнётся только после этого.
    Affordances->Offers.Last().bNeedsInHand = true;
    Affordances->Offers.Last().bNeedsSeat = true;
    Affordances->Offers.Last().bNeedsLight = true;

    if (UWorld* World = GetWorld())
    {
        if (UHumanWorldSubsystem* WorldMind = World->GetSubsystem<UHumanWorldSubsystem>())
        {
            WorldMind->RegisterAffordanceSource(Affordances);
        }
    }
}

bool ABookActor::PickUp(AActor* Who)
{
    if (!Who || (HeldBy && HeldBy != Who))
    {
        return false;
    }

    HeldBy = Who;

    // В руках книга физику не симулирует — иначе она вырывается и падает.
    Cover->SetSimulatePhysics(false);
    Cover->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    AttachToActor(Who, FAttachmentTransformRules::KeepWorldTransform);
    return true;
}

void ABookActor::PutDown()
{
    if (!HeldBy)
    {
        return;
    }

    DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    HeldBy = nullptr;

    Cover->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Cover->SetSimulatePhysics(true);
}

void ABookActor::ReturnToShelf()
{
    if (HeldBy)
    {
        DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
        HeldBy = nullptr;
    }

    Cover->SetSimulatePhysics(false);
    SetActorLocation(ShelfLocation);
    SetActorRotation(FRotator::ZeroRotator);
    Cover->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
}
