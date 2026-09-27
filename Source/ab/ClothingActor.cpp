#include "ClothingActor.h"
#include "CompleteHumanAI.h"
#include "AffordanceComponent.h"
#include "Matter.h"
#include "MatterSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

AClothingActor::AClothingActor()
{
    PrimaryActorTick.bCanEverTick = false;

    Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
    RootComponent = Body;
    Body->SetMobility(EComponentMobility::Movable);
    Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Body->SetCollisionObjectType(ECC_PhysicsBody);
    Body->SetCollisionResponseToAllChannels(ECR_Block);
    Body->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
    Body->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
    Body->SetCanEverAffectNavigation(false);

    Affordances = CreateDefaultSubobject<UAffordanceComponent>(TEXT("Affordances"));
}

void AClothingActor::BeginPlay()
{
    Super::BeginPlay();
    if (Name.IsEmpty())
    {
        Setup(Kind, FString());
    }
}

FString AClothingActor::NameOf(EClothingKind Kind)
{
    switch (Kind)
    {
    case EClothingKind::Shirt:    return TEXT("рубаха");
    case EClothingKind::Trousers: return TEXT("штаны");
    case EClothingKind::Dress:    return TEXT("платье");
    case EClothingKind::Coat:     return TEXT("кафтан");
    case EClothingKind::Boots:    return TEXT("сапоги");
    case EClothingKind::Hat:      return TEXT("шапка");
    case EClothingKind::Apron:    return TEXT("передник");
    case EClothingKind::Belt:     return TEXT("пояс");
    case EClothingKind::BastShoes:return TEXT("лапти");
    case EClothingKind::Kerchief: return TEXT("платок");
    case EClothingKind::Crown:    return TEXT("венец");
    case EClothingKind::Helmet:   return TEXT("шлем");
    case EClothingKind::Swaddle:  return TEXT("пелёнка");
    default:                      return TEXT("одежда");
    }
}

float AClothingActor::WarmthOf(EClothingKind Kind)
{
    switch (Kind)
    {
    case EClothingKind::Shirt:    return 0.18f;
    case EClothingKind::Trousers: return 0.2f;
    case EClothingKind::Dress:    return 0.22f;
    case EClothingKind::Coat:     return 0.4f;
    case EClothingKind::Boots:    return 0.15f;
    case EClothingKind::Hat:      return 0.12f;
    case EClothingKind::Apron:    return 0.05f;
    case EClothingKind::Belt:     return 0.02f;
    case EClothingKind::BastShoes:return 0.1f;
    case EClothingKind::Kerchief: return 0.06f;
    case EClothingKind::Crown:    return 0.0f;
    case EClothingKind::Helmet:   return 0.03f;
    case EClothingKind::Swaddle:  return 0.4f;
    default:                      return 0.1f;
    }
}

EClothingKind AClothingActor::KindFromWord(const FString& Word)
{
    static const TCHAR* Stems[] = { TEXT("рубах"), TEXT("штан"), TEXT("плать"), TEXT("кафтан"),
        TEXT("сапог"), TEXT("шапк"), TEXT("передник"), TEXT("пояс"), TEXT("лапт"), TEXT("плат"),
        TEXT("венц"), TEXT("шлем"), TEXT("пелён"), TEXT("сарафан"), TEXT("порт") };
    static const EClothingKind Kinds[] = { EClothingKind::Shirt, EClothingKind::Trousers,
        EClothingKind::Dress, EClothingKind::Coat, EClothingKind::Boots, EClothingKind::Hat,
        EClothingKind::Apron, EClothingKind::Belt, EClothingKind::BastShoes, EClothingKind::Kerchief,
        EClothingKind::Crown, EClothingKind::Helmet, EClothingKind::Swaddle, EClothingKind::Dress,
        EClothingKind::Trousers };

    for (int32 i = 0; i < UE_ARRAY_COUNT(Stems); ++i)
    {
        if (Word.StartsWith(Stems[i], ESearchCase::IgnoreCase))
        {
            return Kinds[i];
        }
    }
    return EClothingKind::None;
}

void AClothingActor::Setup(EClothingKind InKind, const FString& OfWhat)
{
    Kind = InKind;
    Name = NameOf(Kind);
    if (!OfWhat.IsEmpty())
    {
        Name += TEXT(" из ") + OfWhat;
    }
    Warmth = WarmthOf(Kind);

    const TCHAR* Shape = TEXT("Cube");
    FVector Size(46.0f, 34.0f, 46.0f);
    const TCHAR* Material = TEXT("M_Basic_Wall");

    switch (Kind)
    {
    case EClothingKind::Shirt:    Size = FVector(34.0f, 26.0f, 5.0f); break;
    case EClothingKind::Trousers: Size = FVector(30.0f, 24.0f, 5.0f); break;
    case EClothingKind::Dress:    Size = FVector(38.0f, 28.0f, 7.0f); break;
    case EClothingKind::Coat:     Size = FVector(40.0f, 30.0f, 9.0f); break;
    case EClothingKind::Boots:    Size = FVector(28.0f, 24.0f, 22.0f); break;
    case EClothingKind::BastShoes:Size = FVector(26.0f, 22.0f, 10.0f); break;
    case EClothingKind::Hat:      Shape = TEXT("Cylinder"); Size = FVector(22.0f, 22.0f, 12.0f); break;
    case EClothingKind::Crown:    Shape = TEXT("Cylinder"); Size = FVector(19.0f, 19.0f, 7.0f); break;
    case EClothingKind::Helmet:   Shape = TEXT("Cone"); Size = FVector(22.0f, 22.0f, 22.0f); break;
    case EClothingKind::Apron:    Size = FVector(26.0f, 20.0f, 2.0f); break;
    case EClothingKind::Belt:     Size = FVector(20.0f, 10.0f, 2.0f); break;
    case EClothingKind::Kerchief: Size = FVector(24.0f, 24.0f, 1.5f); break;
    case EClothingKind::Swaddle:  Size = FVector(30.0f, 22.0f, 3.0f); break;
    default: break;
    }

    if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr,
            *FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), Shape, Shape)))
    {
        Body->SetStaticMesh(Mesh);
        Body->SetRelativeScale3D(Size / 100.0f);
    }
    if (UMaterialInterface* Look = Cloth())
    {
        Body->SetMaterial(0, Look);
    }
    else if (UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr,
            *FString::Printf(TEXT("/Game/StarterContent/Materials/%s.%s"), Material, Material)))
    {
        Body->SetMaterial(0, Mat);
    }

    Body->SetMassOverrideInKg(NAME_None, MassKg(), true);

    if (Affordances)
    {
        Affordances->DisplayName = Name;
        Affordances->Category = TEXT("одежда");
        Affordances->NoticeRadius = 600.0f;
        Affordances->Capacity = 1;
        Affordances->Offers.Reset();
        Affordances->AddOffer(EActionType::Observe, FString::Printf(TEXT("надеть: %s"), *Name), 60.0f);
        Affordances->AddPromise(ENeedType::Comfort, 0.15f);
        Affordances->Offers.Last().bNeedsInHand = true;
        Affordances->Offers.Last().EffortCost = 0.03f;
    }
}

bool AClothingActor::PutOn(ACompleteHumanNPC* Who)
{
    if (!Who || WornBy.IsValid())
    {
        return false;
    }

    WornBy = Who;
    Body->SetSimulatePhysics(false);
    Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    AttachToComponent(Who->GetRootComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
    Tailor(Who);
    Body->SetVisibility(Pieces.Num() == 0);
    return true;
}

void AClothingActor::TakeOff()
{
    Unstitch();
    if (WornBy.IsValid())
    {
        DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    }
    WornBy = nullptr;

    Body->SetVisibility(true);
    Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Body->SetSimulatePhysics(true);
}

void AClothingActor::Colour(const FLinearColor& InDye, EResourceKind InStuff, float InLength)
{
    Dye = InDye;
    Stuff = InStuff;
    Length = InLength;
    const FSubstance& Matter = FMatter::Of(Stuff);
    Name = FString::Printf(TEXT("%s (%s)"), *NameOf(Kind), Matter.Name);
    if (Affordances)
    {
        Affordances->DisplayName = Name;
        if (Affordances->Offers.Num() > 0)
        {
            Affordances->Offers.Last().Label = FString::Printf(TEXT("надеть: %s"), *Name);
        }
    }
    Warmth = WarmthOf(Kind) * (Stuff == EResourceKind::Wool ? 1.35f : (Stuff == EResourceKind::Leather ? 1.15f : (Matter.bMetal ? 0.0f : 1.0f)));
    Body->SetMassOverrideInKg(NAME_None, MassKg(), true);
    if (UMaterialInterface* Look = Cloth())
    {
        Body->SetMaterial(0, Look);
    }
    if (ACompleteHumanNPC* Who = WornBy.Get())
    {
        Unstitch();
        Tailor(Who);
        Body->SetVisibility(Pieces.Num() == 0);
    }
}

float AClothingActor::MassKg() const
{
    const FSubstance& Matter = FMatter::Of(Stuff);
    if (Kind == EClothingKind::Crown)
    {
        return Matter.Density * 36.0e-6f;
    }
    if (Kind == EClothingKind::Helmet)
    {
        return Matter.Density * 190.0e-6f;
    }
    float Square = 0.2f;
    switch (Kind)
    {
    case EClothingKind::Shirt:     Square = 1.6f; break;
    case EClothingKind::Trousers:  Square = 1.4f; break;
    case EClothingKind::Dress:     Square = 2.6f * Length; break;
    case EClothingKind::Coat:      Square = 2.8f * Length; break;
    case EClothingKind::Boots:     Square = 0.5f; break;
    case EClothingKind::BastShoes: Square = 0.45f; break;
    case EClothingKind::Hat:       Square = 0.18f; break;
    case EClothingKind::Apron:     Square = 0.4f; break;
    case EClothingKind::Belt:      Square = 0.12f; break;
    case EClothingKind::Kerchief:  Square = 0.6f; break;
    case EClothingKind::Swaddle:   Square = 0.5f; break;
    default: break;
    }
    const float Thick = Stuff == EResourceKind::Leather ? 0.003f : (Stuff == EResourceKind::Wool ? 0.004f : (Stuff == EResourceKind::Straw ? 0.006f : 0.0006f));
    return FMath::Max(0.02f, Square * Thick * Matter.Density);
}

UMaterialInterface* AClothingActor::Cloth() const
{
    UMatterSubsystem* Matter = UMatterSubsystem::Get(this);
    if (!Matter)
    {
        return nullptr;
    }
    const FSubstance& Substance = FMatter::Of(Stuff);
    const float Bright = Substance.Look == EMatterLook::Cloth ? 1.8f : 1.0f;
    return Matter->Look(Substance.Look, FLinearColor(Dye.R * Bright, Dye.G * Bright, Dye.B * Bright, 1.0f));
}

void AClothingActor::Unstitch()
{
    for (UStaticMeshComponent* Piece : Pieces)
    {
        if (Piece)
        {
            Piece->DestroyComponent();
        }
    }
    Pieces.Reset();
}

UStaticMeshComponent* AClothingActor::Stitch(ACompleteHumanNPC* Who, const TCHAR* Shape, const FVector& From, const FVector& To, float Radius, float Depth,
    const FVector& Side, FName Bone, UMaterialInterface* Material)
{
    USkinnedMeshComponent* Skin = Who->PoseBody && Who->PoseBody->IsVisible() ? static_cast<USkinnedMeshComponent*>(Who->PoseBody) : static_cast<USkinnedMeshComponent*>(Who->GetMesh());
    UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), Shape, Shape));
    if (!Skin || !Mesh || Skin->GetBoneIndex(Bone) == INDEX_NONE)
    {
        return nullptr;
    }
    UStaticMeshComponent* Piece = NewObject<UStaticMeshComponent>(this);
    Piece->SetStaticMesh(Mesh);
    Piece->SetMobility(EComponentMobility::Movable);
    Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Piece->SetCanEverAffectNavigation(false);
    if (Material)
    {
        Piece->SetMaterial(0, Material);
    }
    Piece->RegisterComponent();
    const FVector Axis = To - From;
    const float Span = FMath::Max(0.5f, static_cast<float>(Axis.Size()));
    const FVector Z = Axis.SizeSquared() > 1.0e-4f ? Axis.GetSafeNormal() : FVector::UpVector;
    FVector X = Side - Z * FVector::DotProduct(Side, Z);
    if (X.SizeSquared() < 1.0e-4f)
    {
        X = FVector::CrossProduct(Z, FMath::Abs(Z.Z) < 0.9f ? FVector::UpVector : FVector::ForwardVector);
    }
    X.Normalize();
    const FRotator Turn = FRotationMatrix::MakeFromZX(Z, X).Rotator();
    Piece->SetWorldTransform(FTransform(Turn, (From + To) * 0.5f, FVector(Radius * 0.02f, Radius * Depth * 0.02f, Span * 0.01f)));
    Piece->AttachToComponent(Skin, FAttachmentTransformRules::KeepWorldTransform, Bone);
    Pieces.Add(Piece);
    return Piece;
}

void AClothingActor::Tailor(ACompleteHumanNPC* Who)
{
    Unstitch();
    USkinnedMeshComponent* Skin = Who && Who->PoseBody && Who->PoseBody->IsVisible() ? static_cast<USkinnedMeshComponent*>(Who->PoseBody)
        : (Who ? static_cast<USkinnedMeshComponent*>(Who->GetMesh()) : nullptr);
    if (!Skin || !Skin->GetSkinnedAsset() || Skin->GetBoneIndex(TEXT("pelvis")) == INDEX_NONE)
    {
        return;
    }
    auto At = [Skin](const TCHAR* Name) { return Skin->GetBoneLocation(FName(Name), EBoneSpaces::WorldSpace); };
    const float S = FMath::Max(0.25f, static_cast<float>(Who->GetActorScale3D().Z));
    const FVector Pelvis = At(TEXT("pelvis"));
    const FVector Spine2 = At(TEXT("spine_02"));
    const FVector Neck = At(TEXT("neck_01"));
    const FVector Head = At(TEXT("head"));
    const FVector Up = (Neck - Pelvis).GetSafeNormal();
    const FVector HeadUp = (Head - Neck).IsNearlyZero() ? Up : (Head - Neck).GetSafeNormal();
    const FVector Side = (At(TEXT("upperarm_r")) - At(TEXT("upperarm_l"))).GetSafeNormal();
    FVector Front = Who->GetActorForwardVector();
    Front = (Front - Up * FVector::DotProduct(Front, Up)).GetSafeNormal();
    const float Floor = static_cast<float>(FMath::Min(At(TEXT("foot_l")).Z, At(TEXT("foot_r")).Z));
    const float Hang = FMath::Max(10.0f * S, static_cast<float>(Pelvis.Z) - Floor);
    UMaterialInterface* Look = Cloth();

    auto Limb = [&](const TCHAR* From, const TCHAR* To, float Radius, float Begin, float End)
    {
        const FVector A = At(From);
        const FVector B = At(To);
        const FVector Dir = B - A;
        Stitch(Who, TEXT("Cylinder"), A + Dir * Begin, A + Dir * End, Radius * S, 1.0f, Side, FName(From), Look);
    };
    auto Torso = [&](float Lower, float Upper, float Bottom, float Top, float Depth)
    {
        Stitch(Who, TEXT("Cylinder"), Pelvis - Up * Bottom * S, Spine2, Lower * S, Depth, Side, TEXT("spine_01"), Look);
        Stitch(Who, TEXT("Cylinder"), Spine2 - Up * 2.0f * S, Neck - Up * Top * S, Upper * S, Depth, Side, TEXT("spine_03"), Look);
    };
    auto Skirt = [&](float Radius, float Drop, float Apex, float Depth)
    {
        Stitch(Who, TEXT("Cone"), Pelvis - Up * Drop, Pelvis + Up * Apex * S, Radius * S, Depth, Side, TEXT("pelvis"), Look);
    };
    auto Arms = [&](float Upper, float Lower, float Cuff)
    {
        for (const TCHAR* Shoulder : { TEXT("upperarm_l"), TEXT("upperarm_r") })
        {
            const FVector Joint = At(Shoulder);
            Stitch(Who, TEXT("Sphere"), Joint - Up * Upper * S, Joint + Up * Upper * S, Upper * S, 1.0f, Side, FName(Shoulder), Look);
        }
        Limb(TEXT("upperarm_l"), TEXT("lowerarm_l"), Upper, -0.05f, 1.02f);
        Limb(TEXT("upperarm_r"), TEXT("lowerarm_r"), Upper, -0.05f, 1.02f);
        Limb(TEXT("lowerarm_l"), TEXT("hand_l"), Lower, 0.0f, Cuff);
        Limb(TEXT("lowerarm_r"), TEXT("hand_r"), Lower, 0.0f, Cuff);
    };
    auto Feet = [&](float Radius, float Height, float Shaft, float ShaftRadius)
    {
        for (const TCHAR* Side2 : { TEXT("l"), TEXT("r") })
        {
            const FString FootName = FString::Printf(TEXT("foot_%s"), Side2);
            const FString BallName = FString::Printf(TEXT("ball_%s"), Side2);
            const FString CalfName = FString::Printf(TEXT("calf_%s"), Side2);
            const FVector Foot = At(*FootName);
            const FVector Ball = At(*BallName);
            const FVector Calf = At(*CalfName);
            const FVector Along = (Ball - Foot).GetSafeNormal2D();
            const FVector Heel = FVector(Foot.X, Foot.Y, Floor + Height * 0.5f * S) - Along * 6.0f * S;
            const FVector Toe = FVector(Ball.X, Ball.Y, Floor + Height * 0.45f * S) + Along * 7.0f * S;
            Stitch(Who, TEXT("Sphere"), Heel, Toe, Radius * S, Height / FMath::Max(1.0f, Radius * 2.0f), Side, FName(*FootName), Look);
            if (Shaft > 0.0f)
            {
                const FVector Down = Foot - Calf;
                Stitch(Who, TEXT("Cylinder"), Calf + Down * (1.0f - Shaft), Foot + Down * 0.05f, ShaftRadius * S, 1.0f, Side, FName(*CalfName), Look);
            }
        }
    };

    switch (Kind)
    {
    case EClothingKind::Shirt:
        Torso(16.0f, 16.8f, 8.0f, 0.5f, 0.72f);
        Arms(6.4f, 5.3f, 0.93f);
        Skirt(21.0f, Hang * 0.33f * Length, 34.0f, 0.82f);
        break;
    case EClothingKind::Trousers:
        Stitch(Who, TEXT("Cylinder"), Pelvis - Up * 16.0f * S, Pelvis + Up * 5.0f * S, 15.5f * S, 0.8f, Side, TEXT("pelvis"), Look);
        Limb(TEXT("thigh_l"), TEXT("calf_l"), 8.4f, -0.08f, 1.03f);
        Limb(TEXT("thigh_r"), TEXT("calf_r"), 8.4f, -0.08f, 1.03f);
        Limb(TEXT("calf_l"), TEXT("foot_l"), 6.2f, 0.0f, 0.97f);
        Limb(TEXT("calf_r"), TEXT("foot_r"), 6.2f, 0.0f, 0.97f);
        break;
    case EClothingKind::Dress:
        Stitch(Who, TEXT("Cylinder"), Spine2 - Up * 6.0f * S, Neck - Up * 7.0f * S, 17.6f * S, 0.73f, Side, TEXT("spine_03"), Look);
        Stitch(Who, TEXT("Cylinder"), Pelvis + Up * 2.0f * S, Spine2, 16.4f * S, 0.74f, Side, TEXT("spine_01"), Look);
        Skirt(34.0f, (Hang - 7.0f * S) * Length, 46.0f, 0.86f);
        break;
    case EClothingKind::Coat:
        Torso(18.0f, 18.6f, 10.0f, -0.5f, 0.76f);
        Arms(7.4f, 6.4f, 0.96f);
        Skirt(27.0f, Hang * 0.62f * Length, 40.0f, 0.86f);
        break;
    case EClothingKind::Belt:
        Stitch(Who, TEXT("Cylinder"), Pelvis + Up * 6.5f * S, Pelvis + Up * 10.5f * S, (Who->Clothes.ContainsByPredicate([](const AClothingActor* C) { return C && C->Kind == EClothingKind::Coat; }) ? 18.8f : 16.9f) * S,
            0.76f, Side, TEXT("spine_01"), Look);
        break;
    case EClothingKind::BastShoes:
        Feet(5.6f, 7.5f, 0.45f, 6.6f);
        break;
    case EClothingKind::Boots:
        Feet(5.8f, 8.5f, 0.6f, 7.0f);
        break;
    case EClothingKind::Hat:
        Stitch(Who, TEXT("Cylinder"), Head + HeadUp * 9.0f * S, Head + HeadUp * 20.0f * S, 10.8f * S, 1.0f, Side, TEXT("head"), Look);
        Stitch(Who, TEXT("Cylinder"), Head + HeadUp * 8.5f * S, Head + HeadUp * 11.5f * S, 12.2f * S, 1.0f, Side, TEXT("head"), Look);
        break;
    case EClothingKind::Crown:
        Stitch(Who, TEXT("Cylinder"), Head + HeadUp * 14.0f * S, Head + HeadUp * 20.0f * S, 9.6f * S, 1.0f, Side, TEXT("head"), Look);
        break;
    case EClothingKind::Helmet:
        Stitch(Who, TEXT("Cone"), Head + HeadUp * 9.0f * S, Head + HeadUp * 30.0f * S, 11.2f * S, 1.0f, Side, TEXT("head"), Look);
        break;
    case EClothingKind::Kerchief:
        Stitch(Who, TEXT("Sphere"), Head - Front * 4.5f * S, Head + HeadUp * 22.0f * S - Front * 4.5f * S, 11.8f * S, 1.0f, Side, TEXT("head"), Look);
        Stitch(Who, TEXT("Cone"), Head - HeadUp * 14.0f * S - Front * 9.0f * S, Head + HeadUp * 4.0f * S - Front * 7.0f * S, 9.0f * S, 0.35f, Side, TEXT("head"), Look);
        break;
    case EClothingKind::Apron:
        Stitch(Who, TEXT("Cube"), Pelvis + Front * 15.5f * S + Up * 8.0f * S, Pelvis + Front * 19.0f * S - Up * Hang * 0.6f, 14.0f * S, 0.06f, Side, TEXT("pelvis"), Look);
        break;
    case EClothingKind::Swaddle:
        Stitch(Who, TEXT("Cylinder"), (At(TEXT("foot_l")) + At(TEXT("foot_r"))) * 0.5f - Up * 4.0f * S, Neck, 15.0f * S, 0.85f, Side, TEXT("pelvis"), Look);
        break;
    default:
        break;
    }
}

void AClothingActor::Advance(float GameDelta, bool bWorking)
{
    const float Days = GameDelta / 86400.0f;
    Worn = FMath::Clamp(Worn + Days * (bWorking ? 0.05f : 0.02f), 0.0f, 1.0f);
    Dirt = FMath::Clamp(Dirt + Days * (bWorking ? 0.35f : 0.15f), 0.0f, 1.0f);
    Warmth = WarmthOf(Kind) * (1.0f - Worn * 0.6f);
}

AClothingActor* AClothingActor::Spawn(UWorld* World, EClothingKind Kind, const FString& OfWhat, const FVector& At)
{
    return Spawn(World, Kind, OfWhat, At, EResourceKind::Cloth, FLinearColor(0.95f, 0.93f, 0.86f));
}

AClothingActor* AClothingActor::Spawn(UWorld* World, EClothingKind Kind, const FString& OfWhat, const FVector& At, EResourceKind Stuff, const FLinearColor& Dye,
    float Length)
{
    if (!World || Kind == EClothingKind::None)
    {
        return nullptr;
    }

    const FTransform T(FRotator::ZeroRotator, At);
    AClothingActor* Made = World->SpawnActorDeferred<AClothingActor>(
        AClothingActor::StaticClass(), T, nullptr, nullptr,
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

    if (!Made)
    {
        return nullptr;
    }

    Made->Kind = Kind;
    Made->Stuff = Stuff;
    Made->Dye = Dye;
    Made->Length = Length;
    Made->FinishSpawning(T);
    Made->Setup(Kind, OfWhat);
    Made->Colour(Dye, Stuff, Length);
    Made->Body->SetSimulatePhysics(true);
    return Made;
}
