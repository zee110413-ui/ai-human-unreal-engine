#include "MatterStructure.h"
#include "MatterSubsystem.h"
#include "ImportedModels.h"
#include "ResourceActor.h"
#include "AffordanceComponent.h"
#include "HumanWorldSubsystem.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"

namespace
{
    constexpr int32 LookSlots = 5;

    UStaticMesh* ShapeMesh(EMatterShape Shape)
    {
        const TCHAR* Name = TEXT("Cube");
        switch (Shape)
        {
        case EMatterShape::Cylinder: Name = TEXT("Cylinder"); break;
        case EMatterShape::Sphere:   Name = TEXT("Sphere"); break;
        case EMatterShape::Cone:     Name = TEXT("Cone"); break;
        default:                     break;
        }
        return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), Name, Name));
    }

    void Ordered(const FVector& Size, float& Small, float& Middle, float& Large)
    {
        float A = Size.X;
        float B = Size.Y;
        float C = Size.Z;
        if (A > B)
        {
            Swap(A, B);
        }
        if (B > C)
        {
            Swap(B, C);
        }
        if (A > B)
        {
            Swap(A, B);
        }
        Small = A;
        Middle = B;
        Large = C;
    }

    float AreaOf(EMatterShape Shape, const FVector& Size)
    {
        const float Share = Shape == EMatterShape::Box ? 1.0f : (Shape == EMatterShape::Cylinder ? 0.85f : 0.55f);
        return FMath::Max(1.0e-4f, static_cast<float>(2.0 * (Size.X * Size.Y + Size.Y * Size.Z + Size.X * Size.Z)) * 1.0e-4f * Share);
    }

    float TopOf(EMatterShape Shape, const FVector& Size, const FQuat& Turn)
    {
        const float AreaX = Size.Y * Size.Z;
        const float AreaY = Size.X * Size.Z;
        const float AreaZ = Size.X * Size.Y;
        const float Share = Shape == EMatterShape::Box ? 1.0f : 0.8f;
        const float Projected = FMath::Abs(Turn.GetAxisX().Z) * AreaX + FMath::Abs(Turn.GetAxisY().Z) * AreaY + FMath::Abs(Turn.GetAxisZ().Z) * AreaZ;
        return FMath::Max(1.0e-4f, Projected * 1.0e-4f * Share);
    }

    float SectionOf(EMatterShape Shape, const FVector& Size)
    {
        float Small = 0.0f;
        float Middle = 0.0f;
        float Large = 0.0f;
        Ordered(Size, Small, Middle, Large);
        return FMath::Max(1.0e-6f, Small * Middle * 1.0e-4f * (Shape == EMatterShape::Box ? 1.0f : UE_PI / 4.0f));
    }

    float LengthwiseOf(const FVector& Size)
    {
        float Small = 0.0f;
        float Middle = 0.0f;
        float Large = 0.0f;
        Ordered(Size, Small, Middle, Large);
        return FMath::Max(1.0e-6f, Large * Middle * 1.0e-4f);
    }

    float SectionModulus(EMatterShape Shape, const FVector& Size)
    {
        float Small = 0.0f;
        float Middle = 0.0f;
        float Large = 0.0f;
        Ordered(Size, Small, Middle, Large);
        const float S = Small / 100.0f;
        const float M = Middle / 100.0f;
        if (Shape == EMatterShape::Cylinder)
        {
            return UE_PI * S * S * S / 32.0f;
        }
        return M * S * S / 6.0f;
    }

    float BaseCapacity(EJointKind Kind)
    {
        switch (Kind)
        {
        case EJointKind::Ground:  return 30000.0f;
        case EJointKind::Rest:    return 200000.0f;
        case EJointKind::Notch:   return 60000.0f;
        case EJointKind::Peg:     return 8000.0f;
        case EJointKind::Nail:    return 1500.0f;
        case EJointKind::Mortar:  return 5000.0f;
        default:                  return 4000.0f;
        }
    }

    bool HoldsInTension(EJointKind Kind)
    {
        return Kind == EJointKind::Notch || Kind == EJointKind::Peg || Kind == EJointKind::Nail
            || Kind == EJointKind::Mortar || Kind == EJointKind::Lashing;
    }

    EResourceKind MaterialFor(EResourceKind Kind)
    {
        return Kind == EResourceKind::Granite ? EResourceKind::Stone : Kind;
    }
}

AMatterStructure::AMatterStructure()
{
    PrimaryActorTick.bCanEverTick = false;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent = Root;
    Root->SetMobility(EComponentMobility::Movable);

    Affordances = CreateDefaultSubobject<UAffordanceComponent>(TEXT("Affordances"));
}

void AMatterStructure::BeginPlay()
{
    Super::BeginPlay();
    if (UMatterSubsystem* Matter = UMatterSubsystem::Get(this))
    {
        Matter->RegisterStructure(this);
    }
}

void AMatterStructure::EndPlay(const EEndPlayReason::Type Reason)
{
    if (UMatterSubsystem* Matter = UMatterSubsystem::Get(this))
    {
        Matter->UnregisterStructure(this);
    }
    Super::EndPlay(Reason);
}

int32 AMatterStructure::RendererFor(EResourceKind Substance, EMatterShape Shape, FName Model)
{
    for (int32 i = 0; i < Renderers.Num(); ++i)
    {
        if (Renderers[i].Substance == Substance && Renderers[i].Shape == Shape && Renderers[i].Model == Model)
        {
            return i;
        }
    }

    UStaticMesh* Look3D = nullptr;
    FBox Bounds(ForceInit);
    if (!Model.IsNone())
    {
        FString Folder;
        FString Filter;
        Model.ToString().Split(TEXT("|"), &Folder, &Filter);
        TArray<UStaticMesh*> Found;
        FImportedModels::Gather(TEXT("/Game/Imported/") + Folder, Filter, Found);
        if (Found.Num() > 0)
        {
            Look3D = Found[0];
            Bounds = Look3D->GetBoundingBox();
        }
    }

    UInstancedStaticMeshComponent* Mesh = NewObject<UInstancedStaticMeshComponent>(this);
    Mesh->SetMobility(EComponentMobility::Movable);
    Mesh->SetupAttachment(Root);
    Mesh->SetStaticMesh(Look3D ? Look3D : ShapeMesh(Shape));
    Mesh->SetNumCustomDataFloats(LookSlots);
    Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Mesh->SetCollisionProfileName(TEXT("BlockAll"));
    Mesh->SetCanEverAffectNavigation(false);
    if (!Look3D)
    {
        if (UMatterSubsystem* Matter = UMatterSubsystem::Get(this))
        {
            if (UMaterialInterface* Look = Matter->LookOf(Substance))
            {
                Mesh->SetMaterial(0, Look);
            }
        }
    }
    Mesh->RegisterComponent();

    FStructureRenderer Made;
    Made.Mesh = Mesh;
    Made.Substance = Substance;
    Made.Shape = Shape;
    Made.Model = Look3D ? Model : NAME_None;
    if (Look3D && Bounds.IsValid)
    {
        Made.ModelMin = Bounds.Min;
        Made.ModelSize = Bounds.GetSize().ComponentMax(FVector(0.1f));
    }
    return Renderers.Add(Made);
}

int32 AMatterStructure::AddModelled(FName Model, const FVector& Centre, const FVector& Size, const FQuat& Turn,
                                    EResourceKind Substance, FName PieceRole, bool bSpan, float Exposure)
{
    return AddShape(EMatterShape::Box, Centre, Size, Turn, Substance, PieceRole, bSpan, Exposure, Model);
}

int32 AMatterStructure::AddShape(EMatterShape Shape, const FVector& Centre, const FVector& Size, const FQuat& Turn,
                                 EResourceKind Substance, FName PieceRole, bool bSpan, float Exposure, FName Model)
{
    FStructurePiece Piece;
    Piece.Substance = Substance;
    Piece.Shape = Shape;
    Piece.Frame = FTransform(Turn, Centre);
    Piece.Size = Size.ComponentMax(FVector(0.5f));
    Piece.Role = PieceRole;
    Piece.bSpan = bSpan;
    Piece.Exposure = FMath::Clamp(Exposure, 0.0f, 1.0f);

    const FSubstance& S = FMatter::Of(Substance);
    Piece.DryMass = FMath::Max(0.01f, S.Density * UMatterComponent::VolumeOf(Shape, Piece.Size));
    Piece.OriginalMass = Piece.DryMass;

    float Humidity = 0.7f;
    float Celsius = 12.0f;
    if (UMatterSubsystem* Matter = UMatterSubsystem::Get(this))
    {
        Humidity = Matter->Climate().Humidity;
        Celsius = Matter->Climate().Celsius;
    }
    Piece.Moisture = FMatter::AirDryMoisture(S, Humidity);
    Piece.Temperature = Celsius;
    Piece.Model = Model;
    Piece.Renderer = RendererFor(Substance, Shape, Model);

    const int32 Index = Pieces.Add(Piece);
    for (int32 k = 0; k < LookSlots; ++k)
    {
        Shown.Add(-1.0f);
    }
    Show(Index);
    return Index;
}

int32 AMatterStructure::AddLog(const FVector& From, const FVector& To, float Diameter, EResourceKind Substance, FName PieceRole, bool bSpan, float Exposure)
{
    const FVector Axis = To - From;
    const float Length = Axis.Size();
    if (Length < 1.0f)
    {
        return INDEX_NONE;
    }
    const FQuat Turn = FRotationMatrix::MakeFromZ(Axis / Length).ToQuat();
    return AddShape(EMatterShape::Cylinder, (From + To) * 0.5f, FVector(Diameter, Diameter, Length), Turn, Substance, PieceRole, bSpan, Exposure);
}

int32 AMatterStructure::AddBlock(const FVector& Centre, const FVector& Size, const FQuat& Turn, EResourceKind Substance, FName PieceRole, bool bSpan, float Exposure)
{
    return AddShape(EMatterShape::Box, Centre, Size, Turn, Substance, PieceRole, bSpan, Exposure);
}

void AMatterStructure::Join(int32 Upper, int32 Lower, EJointKind Kind)
{
    if (!Pieces.IsValidIndex(Upper) || Upper == Lower)
    {
        return;
    }
    if (Kind != EJointKind::Ground && !Pieces.IsValidIndex(Lower))
    {
        return;
    }
    for (const FStructureJoint& Existing : Joints)
    {
        if (Existing.Upper == Upper && Existing.Lower == Lower)
        {
            return;
        }
    }
    FStructureJoint Joint;
    Joint.Kind = Kind;
    Joint.Upper = Upper;
    Joint.Lower = Kind == EJointKind::Ground ? INDEX_NONE : Lower;
    Joints.Add(Joint);
}

void AMatterStructure::Finish()
{
    bFinished = true;
    CheckStability();
    RefreshOffers();
    FlushLooks();
    float Heaviest = 0.0f;
    for (const FStructureJoint& Joint : Joints)
    {
        Heaviest = FMath::Max(Heaviest, Joint.Load);
    }
    UE_LOG(LogHumanCity, Log, TEXT("Постройка %s: частей %d, соединений %d, после проверки недостаёт %d, самая нагруженная опора %.1f кН"),
        *Title, Pieces.Num(), Joints.Num(), MissingCount(), Heaviest / 1000.0f);
}

void AMatterStructure::Show(int32 Index)
{
    FStructurePiece& Piece = Pieces[Index];
    if (Piece.Instance != INDEX_NONE || !Renderers.IsValidIndex(Piece.Renderer))
    {
        return;
    }
    FStructureRenderer& Renderer = Renderers[Piece.Renderer];
    FTransform Placed = Piece.Frame;
    if (Renderer.Model.IsNone())
    {
        Placed.SetScale3D(Piece.Size / 100.0f);
    }
    else
    {
        const FVector Scale = Piece.Size / Renderer.ModelSize;
        const FVector Middle = (Renderer.ModelMin + Renderer.ModelSize * 0.5f) * Scale;
        Placed.SetScale3D(Scale);
        Placed.SetLocation(Piece.Frame.GetLocation() - Piece.Frame.GetRotation().RotateVector(Middle));
    }
    Piece.Instance = Renderer.Mesh->AddInstance(Placed, true);
    if (Piece.Instance != Renderer.Pieces.Num())
    {
        UE_LOG(LogHumanCity, Error, TEXT("Постройка: порядок частей нарушен (%d вместо %d)"), Piece.Instance, Renderer.Pieces.Num());
    }
    Renderer.Pieces.Add(Index);
    for (int32 k = 0; k < LookSlots; ++k)
    {
        Shown[Index * LookSlots + k] = -1.0f;
    }
    PushLook(Index, true);
}

void AMatterStructure::Hide(int32 Index)
{
    FStructurePiece& Piece = Pieces[Index];
    if (Piece.Instance == INDEX_NONE || !Renderers.IsValidIndex(Piece.Renderer))
    {
        return;
    }
    FStructureRenderer& Renderer = Renderers[Piece.Renderer];
    const int32 Slot = Piece.Instance;
    const bool bSwap = Renderer.Mesh->SupportsRemoveSwap();
    Renderer.Mesh->RemoveInstance(Slot);
    if (bSwap)
    {
        const int32 Last = Renderer.Pieces.Num() - 1;
        if (Slot != Last)
        {
            Renderer.Pieces[Slot] = Renderer.Pieces[Last];
            Pieces[Renderer.Pieces[Slot]].Instance = Slot;
        }
        Renderer.Pieces.Pop();
    }
    else
    {
        Renderer.Pieces.RemoveAt(Slot);
        for (int32 k = Slot; k < Renderer.Pieces.Num(); ++k)
        {
            Pieces[Renderer.Pieces[k]].Instance = k;
        }
    }
    Piece.Instance = INDEX_NONE;
}

int32 AMatterStructure::FindPiece(const UPrimitiveComponent* Component, int32 Item) const
{
    for (const FStructureRenderer& Renderer : Renderers)
    {
        if (Renderer.Mesh == Component)
        {
            return Renderer.Pieces.IsValidIndex(Item) ? Renderer.Pieces[Item] : INDEX_NONE;
        }
    }
    return INDEX_NONE;
}

void AMatterStructure::PushLook(int32 Index, bool bForce)
{
    const FStructurePiece& Piece = Pieces[Index];
    if (Piece.Instance == INDEX_NONE)
    {
        return;
    }
    const FSubstance& S = FMatter::Of(Piece.Substance);
    const float Wet = FMatter::WetFraction(S, Piece.Moisture);
    const float Values[LookSlots] = {
        Wet,
        FMath::Clamp(Piece.Damage, 0.0f, 1.0f),
        FMath::Clamp(Piece.Char, 0.0f, 1.0f),
        FMath::Clamp(FMath::Max(Piece.Snow, Piece.Ice * Wet), 0.0f, 1.0f),
        Piece.bBurning ? 1.0f : FMath::Clamp((Piece.Temperature - 450.0f) / 400.0f, 0.0f, 1.0f) };

    FStructureRenderer& Renderer = Renderers[Piece.Renderer];
    for (int32 k = 0; k < LookSlots; ++k)
    {
        float& Was = Shown[Index * LookSlots + k];
        if (bForce || FMath::Abs(Values[k] - Was) > 0.03f)
        {
            Renderer.Mesh->SetCustomDataValue(Piece.Instance, k, Values[k], false);
            Was = Values[k];
            Renderer.bDirty = true;
        }
    }
}

void AMatterStructure::FlushLooks()
{
    for (FStructureRenderer& Renderer : Renderers)
    {
        if (Renderer.bDirty && Renderer.Mesh)
        {
            Renderer.Mesh->MarkRenderStateDirty();
            Renderer.bDirty = false;
        }
    }
}

void AMatterStructure::Announce(const FString& What, float Valence, float Radius) const
{
    UWorld* World = GetWorld();
    UHumanWorldSubsystem* WorldMind = World ? World->GetSubsystem<UHumanWorldSubsystem>() : nullptr;
    if (!WorldMind)
    {
        return;
    }
    FWorldEvent Event;
    Event.Tag = TEXT("House");
    Event.Description = What;
    Event.Location = GetActorLocation();
    Event.Radius = Radius;
    Event.Valence = Valence;
    Event.Significance = FMath::Clamp(FMath::Abs(Valence), 0.2f, 0.8f);
    WorldMind->BroadcastEvent(Event);
}

void AMatterStructure::AdvanceMatter(float Seconds, UMatterSubsystem* Matter)
{
    if (!bFinished || !Matter || Seconds <= 0.0f)
    {
        return;
    }
    SnowLoad = Matter->Climate().SnowOnGround;

    bool bLit = false;
    for (int32 i = 0; i < Pieces.Num(); ++i)
    {
        FStructurePiece& Piece = Pieces[i];
        if (Piece.bRemoved)
        {
            continue;
        }
        const FSubstance& S = FMatter::Of(Piece.Substance);
        const FVector Where = Piece.Frame.GetLocation();
        FMatterAir Air = Matter->AirAt(Where, Piece.Exposure < 0.2f);
        Air.Rain *= Piece.Exposure;
        Air.Snow *= Piece.Exposure;
        Air.Sun *= FMath::Max(0.05f, Piece.Exposure);

        FMatterBody Body;
        Body.Moisture = Piece.Moisture;
        Body.Temperature = Piece.Temperature;
        Body.Ice = Piece.Ice;
        Body.Damage = Piece.Damage;
        Body.Char = Piece.Char;
        Body.Rot = Piece.Rot;
        Body.Snow = Piece.Snow;
        Body.DryMass = Piece.DryMass;
        Body.OriginalMass = Piece.OriginalMass;
        Body.bBurning = Piece.bBurning;

        const float CharBefore = Piece.Char;
        const float RotBefore = Piece.Rot;
        const FMatterStep Step = FMatter::Step(S, Body, AreaOf(Piece.Shape, Piece.Size),
            TopOf(Piece.Shape, Piece.Size, Piece.Frame.GetRotation()), Air, Seconds);

        Piece.Moisture = Body.Moisture;
        Piece.Temperature = Body.Temperature;
        Piece.Ice = Body.Ice;
        Piece.Damage = Body.Damage;
        Piece.Char = Body.Char;
        Piece.Rot = Body.Rot;
        Piece.Snow = Body.Snow;
        Piece.DryMass = Body.DryMass;
        Piece.bBurning = Body.bBurning;

        if (Step.Released > 0.0f)
        {
            Matter->AddHeat(Where, Step.Released);
        }
        bLit = bLit || Step.bIgnited;

        if (Step.bBurnedOut || Step.bDissolved)
        {
            Hide(i);
            Piece.bRemoved = true;
            Piece.bBurning = false;
            CutJoints(i);
            bStabilityDirty = true;
            if (Step.bBurnedOut && S.Ember != EResourceKind::None)
            {
                AResourceActor::SpawnPiece(GetWorld(), S.Ember, 0.3f, FTransform(Where), Piece.Size * 0.3f, EMatterShape::Cone);
            }
            continue;
        }

        if (Piece.bBurning || FMath::Abs(Piece.Char - CharBefore) > 0.02f || FMath::Abs(Piece.Rot - RotBefore) > 0.02f)
        {
            bStabilityDirty = true;
        }
        PushLook(i, false);
    }
    FlushLooks();

    if (bLit)
    {
        const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
        if (Now - AnnouncedAt > 30.0)
        {
            AnnouncedAt = Now;
            Announce(FString::Printf(TEXT("горит %s"), *Title), -0.8f, 3500.0f);
        }
    }

    StabilityTimer += Seconds;
    if (bStabilityDirty && StabilityTimer >= 20.0f)
    {
        StabilityTimer = 0.0f;
        bStabilityDirty = false;
        CheckStability();
    }

    OfferTimer += Seconds;
    if (OfferTimer > 900.0f)
    {
        OfferTimer = 0.0f;
        RefreshOffers();
    }
}

float AMatterStructure::PieceStrength(int32 Index) const
{
    const FStructurePiece& Piece = Pieces[Index];
    const FSubstance& S = FMatter::Of(Piece.Substance);
    return FMatter::StrengthFactor(S, Piece.Moisture, Piece.Temperature, Piece.Rot, Piece.Char) * (1.0f - 0.5f * FMath::Clamp(Piece.Damage, 0.0f, 1.0f));
}

float AMatterStructure::PieceCompliance(int32 Index) const
{
    if (!Pieces.IsValidIndex(Index))
    {
        return 1.0f / 50.0f;
    }
    const FStructurePiece& Piece = Pieces[Index];
    return FMatter::Compliance(FMatter::Of(Piece.Substance), Piece.Moisture, Piece.Temperature);
}

float AMatterStructure::JointCapacity(const FStructureJoint& Joint) const
{
    float Factor = PieceStrength(Joint.Upper);
    if (Pieces.IsValidIndex(Joint.Lower))
    {
        Factor = FMath::Min(Factor, PieceStrength(Joint.Lower));
    }
    return BaseCapacity(Joint.Kind) * FMath::Max(0.0f, Factor) * (1.0f - FMath::Clamp(Joint.Damage, 0.0f, 1.0f));
}

void AMatterStructure::CutJoints(int32 Index)
{
    for (FStructureJoint& Joint : Joints)
    {
        if (Joint.Upper == Index || Joint.Lower == Index)
        {
            Joint.bBroken = true;
        }
    }
}

AResourceActor* AMatterStructure::DropPiece(int32 Index, const FVector& Kick)
{
    FStructurePiece& Piece = Pieces[Index];
    if (Piece.bRemoved)
    {
        return nullptr;
    }
    Hide(Index);
    Piece.bRemoved = true;
    CutJoints(Index);
    bStabilityDirty = true;

    UWorld* World = GetWorld();
    if (!World)
    {
        return nullptr;
    }
    const float Amount = UMatterComponent::VolumeOf(Piece.Shape, Piece.Size) / FMath::Max(1.0e-6f, AResourceActor::UnitVolume(Piece.Substance));
    AResourceActor* Loose = AResourceActor::SpawnPiece(World, Piece.Substance, Amount, Piece.Frame, Piece.Size, Piece.Shape);
    if (Loose && Loose->Matter)
    {
        Loose->Matter->Moisture = Piece.Moisture;
        Loose->Matter->Temperature = Piece.Temperature;
        Loose->Matter->Char = Piece.Char;
        Loose->Matter->Rot = Piece.Rot;
        Loose->Matter->Damage = Piece.Damage;
        Loose->Matter->DryMassKg = Piece.DryMass;
        Loose->Matter->OriginalMassKg = Piece.OriginalMass;
        Loose->Matter->bBurning = Piece.bBurning;
        if (UPrimitiveComponent* Body = Loose->Matter->GetBody())
        {
            Body->SetPhysicsLinearVelocity(Kick);
        }
    }
    Piece.bBurning = false;
    return Loose;
}

void AMatterStructure::BreakPiece(int32 Index, const FVector& Direction, float Energy, bool bEdge, AActor* Culprit)
{
    AResourceActor* Loose = DropPiece(Index, Direction.GetSafeNormal() * 60.0f);
    if (Loose && Loose->Matter)
    {
        Loose->Matter->Damage = 0.999f;
        Loose->Matter->ReceiveImpact(FMath::Max(Energy, 1.0f), Loose->GetActorLocation(), Direction, Culprit, bEdge);
    }
    CheckStability();
    RefreshOffers();
}

void AMatterStructure::ImpactPiece(int32 Index, float Joules, const FVector& Where, const FVector& Direction, AActor* Culprit, bool bEdge)
{
    if (!Pieces.IsValidIndex(Index) || Pieces[Index].bRemoved || Joules <= 0.0f)
    {
        return;
    }
    FStructurePiece& Piece = Pieces[Index];
    const FSubstance& S = FMatter::Of(Piece.Substance);
    const float Strength = FMath::Max(0.02f, FMatter::StrengthFactor(S, Piece.Moisture, Piece.Temperature, Piece.Rot, Piece.Char));
    const float Across = FMatter::BreakEnergy(S, SectionOf(Piece.Shape, Piece.Size), false) * Strength;
    const float Along = S.bGrain ? FMatter::BreakEnergy(S, LengthwiseOf(Piece.Size), true) * Strength : Across;
    const float Needed = bEdge ? FMath::Min(Across, Along) : Across * (S.bGrain ? 2.0f : 1.0f);

    float Small = 0.0f;
    float Middle = 0.0f;
    float Large = 0.0f;
    Ordered(Piece.Size, Small, Middle, Large);
    const float Efficiency = bEdge ? 1.0f : FMath::Clamp(0.02f * FMath::Square(Large / FMath::Max(0.5f, Small)), 0.02f, 0.5f);
    const float Effective = Joules * Efficiency;

    float Gain = Effective / FMath::Max(0.01f, Needed * (S.bMetal ? 4.0f : 1.0f));
    if (S.bBrittle && Gain < 1.0f)
    {
        Gain *= 0.35f;
    }
    Piece.Damage += Gain;
    PushLook(Index, false);
    FlushLooks();

    if (Piece.Damage >= 1.0f)
    {
        BreakPiece(Index, Direction, Effective, bEdge, Culprit);
        return;
    }
    if (Piece.Damage > 0.4f)
    {
        bStabilityDirty = true;
    }
    RefreshOffers();
}

bool AMatterStructure::IgnitePiece(int32 Index)
{
    if (!Pieces.IsValidIndex(Index) || Pieces[Index].bRemoved)
    {
        return false;
    }
    FStructurePiece& Piece = Pieces[Index];
    const FSubstance& S = FMatter::Of(Piece.Substance);
    if (Piece.bBurning || !S.Burns() || Piece.Moisture >= FMatter::IgnitionMoistureLimit(S))
    {
        return false;
    }
    Piece.bBurning = true;
    Piece.Temperature = FMath::Max(Piece.Temperature, S.Ignites + 50.0f);
    PushLook(Index, true);
    FlushLooks();
    return true;
}

void AMatterStructure::SoakPiece(int32 Index, float Kilograms)
{
    if (!Pieces.IsValidIndex(Index) || Pieces[Index].bRemoved)
    {
        return;
    }
    FStructurePiece& Piece = Pieces[Index];
    const float Limit = FMatter::WaterLimit(FMatter::Of(Piece.Substance));
    Piece.Moisture = FMath::Min(Limit, Piece.Moisture + FMath::Max(0.0f, Kilograms) / FMath::Max(0.01f, Piece.DryMass));
    PushLook(Index, false);
    FlushLooks();
}

void AMatterStructure::CheckStability()
{
    const int32 Count = Pieces.Num();
    if (Count == 0)
    {
        return;
    }

    for (int32 Round = 0; Round < 16; ++Round)
    {
        bool bChanged = false;

        TArray<TArray<int32>> Links;
        Links.SetNum(Count);
        TArray<TArray<int32>> Supports;
        Supports.SetNum(Count);
        TArray<TArray<int32>> Hangers;
        Hangers.SetNum(Count);
        TArray<int32> Queue;
        TArray<uint8> Held;
        Held.Init(0, Count);

        for (int32 j = 0; j < Joints.Num(); ++j)
        {
            FStructureJoint& Joint = Joints[j];
            Joint.Load = 0.0f;
            if (Joint.bBroken || !Pieces.IsValidIndex(Joint.Upper) || Pieces[Joint.Upper].bRemoved)
            {
                continue;
            }
            if (Joint.Kind == EJointKind::Ground)
            {
                Supports[Joint.Upper].Add(j);
                if (!Held[Joint.Upper])
                {
                    Held[Joint.Upper] = 1;
                    Queue.Add(Joint.Upper);
                }
                continue;
            }
            if (!Pieces.IsValidIndex(Joint.Lower) || Pieces[Joint.Lower].bRemoved)
            {
                continue;
            }
            Links[Joint.Upper].Add(Joint.Lower);
            Links[Joint.Lower].Add(Joint.Upper);
            Supports[Joint.Upper].Add(j);
            if (HoldsInTension(Joint.Kind))
            {
                Hangers[Joint.Lower].Add(j);
            }
        }

        for (int32 q = 0; q < Queue.Num(); ++q)
        {
            for (int32 Next : Links[Queue[q]])
            {
                if (!Held[Next])
                {
                    Held[Next] = 1;
                    Queue.Add(Next);
                }
            }
        }

        int32 Unheld = 0;
        FString Which;
        for (int32 i = 0; i < Count; ++i)
        {
            if (!Pieces[i].bRemoved && !Held[i])
            {
                if (Which.Len() < 200)
                {
                    Which += FString::Printf(TEXT(" %s/%s"), *Pieces[i].Role.ToString(), *Pieces[i].Model.ToString());
                }
                DropPiece(i, FVector::ZeroVector);
                ++Unheld;
                bChanged = true;
            }
        }
        if (bChanged)
        {
            if (Reported < 6)
            {
                ++Reported;
                UE_LOG(LogHumanCity, Log, TEXT("Постройка %s: без опоры осталось и упало частей %d:%s"), *Title, Unheld, *Which);
            }
            continue;
        }

        TArray<int32> Order;
        for (int32 i = 0; i < Count; ++i)
        {
            if (!Pieces[i].bRemoved)
            {
                Order.Add(i);
            }
        }
        Order.Sort([this](int32 A, int32 B)
        {
            return Pieces[A].Frame.GetLocation().Z > Pieces[B].Frame.GetLocation().Z;
        });

        TArray<float> Incoming;
        Incoming.Init(0.0f, Count);
        for (int32 i : Order)
        {
            FStructurePiece& Piece = Pieces[i];
            const float SnowKg = Piece.Exposure > 0.5f ? SnowLoad * TopOf(Piece.Shape, Piece.Size, Piece.Frame.GetRotation()) : 0.0f;
            const float Weight = (Piece.DryMass * (1.0f + Piece.Moisture) + SnowKg) * FMatter::Gravity;
            const float Total = Weight + Incoming[i];
            Piece.Load = Total;

            const TArray<int32>& Under = Supports[i];
            if (Under.Num() > 0)
            {
                const float Share = Total / Under.Num();
                for (int32 j : Under)
                {
                    Joints[j].Load += Share;
                    if (Pieces.IsValidIndex(Joints[j].Lower))
                    {
                        Incoming[Joints[j].Lower] += Share;
                    }
                }
            }
            else if (Hangers[i].Num() > 0)
            {
                const float Share = Total / Hangers[i].Num();
                for (int32 j : Hangers[i])
                {
                    Joints[j].Load += Share;
                    Incoming[Joints[j].Upper] += Share;
                }
            }
        }

        for (FStructureJoint& Joint : Joints)
        {
            if (Joint.bBroken || Joint.Kind == EJointKind::Ground || !Pieces.IsValidIndex(Joint.Upper) || Pieces[Joint.Upper].bRemoved)
            {
                continue;
            }
            if (!Pieces.IsValidIndex(Joint.Lower) || Pieces[Joint.Lower].bRemoved)
            {
                continue;
            }
            const float Capacity = JointCapacity(Joint);
            if (Joint.Load > Capacity)
            {
                Joint.bBroken = true;
                bChanged = true;
                if (Reported < 6)
                {
                    ++Reported;
                    UE_LOG(LogHumanCity, Log, TEXT("Постройка %s: не выдержало соединение %s→%s (%d), нагрузка %.1f кН при пределе %.1f кН"),
                        *Title, *Pieces[Joint.Upper].Role.ToString(), *Pieces[Joint.Lower].Role.ToString(),
                        static_cast<int32>(Joint.Kind), Joint.Load / 1000.0f, Capacity / 1000.0f);
                }
            }
        }

        for (int32 i : Order)
        {
            FStructurePiece& Piece = Pieces[i];
            if (Piece.bRemoved || !Piece.bSpan || Supports[i].Num() < 2)
            {
                continue;
            }
            const FSubstance& S = FMatter::Of(Piece.Substance);
            float Small = 0.0f;
            float Middle = 0.0f;
            float Large = 0.0f;
            Ordered(Piece.Size, Small, Middle, Large);
            const float Moment = Piece.Load * (Large / 100.0f) / 8.0f;
            const float Stress = Moment / FMath::Max(1.0e-9f, SectionModulus(Piece.Shape, Piece.Size));
            if (Stress > S.Tensile * 1.0e6f * FMath::Max(0.01f, PieceStrength(i)))
            {
                if (Reported < 6)
                {
                    ++Reported;
                    UE_LOG(LogHumanCity, Log, TEXT("Постройка %s: переломилось %s, изгиб %.1f МПа, нагрузка %.1f кН"),
                        *Title, *Piece.Role.ToString(), Stress / 1.0e6f, Piece.Load / 1000.0f);
                }
                AResourceActor* Loose = DropPiece(i, FVector::ZeroVector);
                if (Loose && Loose->Matter)
                {
                    Loose->Matter->Damage = 0.999f;
                    Loose->Matter->ReceiveImpact(1000.0f, Loose->GetActorLocation(), FVector::DownVector, nullptr, false);
                }
                bChanged = true;
            }
        }

        if (!bChanged)
        {
            break;
        }
    }

    const int32 Missing = MissingCount();
    if (Missing != LastMissing)
    {
        if (LastMissing >= 0 && Missing > LastMissing + 3)
        {
            Announce(FString::Printf(TEXT("обвалилась %s"), *Title), -0.7f, 3000.0f);
        }
        LastMissing = Missing;
        RefreshOffers();
    }
}

int32 AMatterStructure::MissingCount() const
{
    int32 Missing = 0;
    for (const FStructurePiece& Piece : Pieces)
    {
        if (Piece.bRemoved)
        {
            ++Missing;
        }
    }
    return Missing;
}

float AMatterStructure::Condition() const
{
    if (Pieces.Num() == 0)
    {
        return 0.0f;
    }
    float Sum = 0.0f;
    for (const FStructurePiece& Piece : Pieces)
    {
        Sum += Piece.bRemoved ? 0.0f : 1.0f - FMath::Clamp(FMath::Max3(Piece.Damage, Piece.Rot, Piece.Char), 0.0f, 1.0f);
    }
    return Sum / Pieces.Num();
}

int32 AMatterStructure::NeedsRepair(EResourceKind& OutMaterial, float& OutAmount) const
{
    int32 Best = INDEX_NONE;
    float BestScore = 0.0f;
    const float BaseZ = GetActorLocation().Z;
    for (int32 i = 0; i < Pieces.Num(); ++i)
    {
        const FStructurePiece& Piece = Pieces[i];
        float Score = 0.0f;
        if (Piece.bRemoved)
        {
            bool bCanRest = false;
            for (const FStructureJoint& Joint : Joints)
            {
                if (Joint.Upper == i && (Joint.Kind == EJointKind::Ground
                    || (Pieces.IsValidIndex(Joint.Lower) && !Pieces[Joint.Lower].bRemoved)))
                {
                    bCanRest = true;
                    break;
                }
            }
            Score = bCanRest ? 3.0f : 2.0f;
        }
        else if (Piece.Damage > 0.45f || Piece.Rot > 0.5f || Piece.Char > 0.5f)
        {
            Score = 1.0f + FMath::Max3(Piece.Damage, Piece.Rot, Piece.Char) * 0.5f;
        }
        if (Score <= 0.0f)
        {
            continue;
        }
        Score += 300.0f / (300.0f + FMath::Max(0.0f, static_cast<float>(Piece.Frame.GetLocation().Z) - BaseZ));
        if (Score > BestScore)
        {
            BestScore = Score;
            Best = i;
        }
    }
    if (Best != INDEX_NONE)
    {
        const FStructurePiece& Piece = Pieces[Best];
        OutMaterial = MaterialFor(Piece.Substance);
        OutAmount = FMath::Max(0.2f, UMatterComponent::VolumeOf(Piece.Shape, Piece.Size) / FMath::Max(1.0e-6f, AResourceActor::UnitVolume(OutMaterial)));
    }
    return Best;
}

bool AMatterStructure::RepairPiece(int32 Index, float Quality)
{
    if (!Pieces.IsValidIndex(Index))
    {
        return false;
    }
    FStructurePiece& Piece = Pieces[Index];
    const FSubstance& S = FMatter::Of(Piece.Substance);
    const float Good = FMath::Clamp(Quality, 0.2f, 1.0f);

    float Humidity = 0.7f;
    float Celsius = 12.0f;
    if (UMatterSubsystem* Matter = UMatterSubsystem::Get(this))
    {
        Humidity = Matter->Climate().Humidity;
        Celsius = Matter->Climate().Celsius;
    }

    Piece.Damage = (1.0f - Good) * 0.4f;
    Piece.Char = 0.0f;
    Piece.Rot = 0.0f;
    Piece.Ice = 0.0f;
    Piece.bBurning = false;
    Piece.DryMass = Piece.OriginalMass;
    Piece.Moisture = FMatter::AirDryMoisture(S, Humidity);
    Piece.Temperature = Celsius;
    if (Piece.bRemoved)
    {
        Piece.bRemoved = false;
        Show(Index);
    }

    for (FStructureJoint& Joint : Joints)
    {
        if (Joint.Upper != Index && Joint.Lower != Index)
        {
            continue;
        }
        const int32 Other = Joint.Upper == Index ? Joint.Lower : Joint.Upper;
        if (Joint.Kind == EJointKind::Ground || (Pieces.IsValidIndex(Other) && !Pieces[Other].bRemoved))
        {
            Joint.bBroken = false;
            Joint.Damage = (1.0f - Good) * 0.5f;
        }
    }

    PushLook(Index, true);
    FlushLooks();
    Announce(FString::Printf(TEXT("починили: %s"), *Title), 0.3f, 1500.0f);
    CheckStability();
    RefreshOffers();
    return !Piece.bRemoved;
}

bool AMatterStructure::RepairFromOffer(float Quality)
{
    EResourceKind Material = EResourceKind::None;
    float Amount = 0.0f;
    const int32 Worst = NeedsRepair(Material, Amount);
    return Worst != INDEX_NONE && RepairPiece(Worst, Quality);
}

void AMatterStructure::RefreshOffers()
{
    if (!Affordances)
    {
        return;
    }
    Affordances->DisplayName = Title;
    Affordances->Category = TEXT("изба");
    Affordances->NoticeRadius = 1500.0f;
    Affordances->Capacity = 2;
    Affordances->bPrivate = bPrivate;
    Affordances->OwnerAnchor = OwnerAnchor;
    Affordances->Offers.Reset();

    EResourceKind Need = EResourceKind::None;
    float Amount = 0.0f;
    if (NeedsRepair(Need, Amount) == INDEX_NONE)
    {
        return;
    }

    Affordances->AddOffer(EActionType::Work, FString::Printf(TEXT("починить избу: %s"), *AResourceActor::KindName(Need)), 1800.0f);
    Affordances->AddPromise(ENeedType::Shelter, 0.35f);
    Affordances->AddPromise(ENeedType::Safety, 0.15f);
    Affordances->AddPromise(ENeedType::Competence, 0.12f);
    FAffordance& Offer = Affordances->Offers.Last();
    Offer.Requires = Need;
    Offer.RequiresAmount = Amount;
    Offer.RequiredSkill = TEXT("Building");
    Offer.Difficulty = 0.35f;
    Offer.EffortCost = 0.35f;
    Offer.SetupSeconds = 10.0f;
}

FString AMatterStructure::DescribePiece(int32 Index) const
{
    if (!Pieces.IsValidIndex(Index))
    {
        return FString();
    }
    const FStructurePiece& Piece = Pieces[Index];
    const FSubstance& S = FMatter::Of(Piece.Substance);
    int32 Holds = 0;
    for (const FStructureJoint& Joint : Joints)
    {
        if (Joint.Upper == Index && !Joint.bBroken)
        {
            ++Holds;
        }
    }
    return FString::Printf(TEXT("%s, %s: %s; нагрузка %.1f кН; опор %d; изба цела на %.0f%%, недостаёт частей: %d"),
        *Title, *Piece.Role.ToString(),
        *FMatter::Describe(S, Piece.Moisture, Piece.Temperature, Piece.Damage, Piece.Char, Piece.Rot, Piece.bBurning),
        Piece.Load / 1000.0f, Holds, Condition() * 100.0f, MissingCount());
}

AMatterStructure* AMatterStructure::BuildIzba(UWorld* World, const FVector& Centre, float Yaw, const FVector& Footprint,
                                              EResourceKind Roofing, const FString& InTitle, bool bInPrivate)
{
    if (!World)
    {
        return nullptr;
    }

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    const FTransform Base(FRotator(0.0f, Yaw, 0.0f), Centre);
    AMatterStructure* House = World->SpawnActor<AMatterStructure>(AMatterStructure::StaticClass(), Base, Params);
    if (!House)
    {
        return nullptr;
    }
    House->Title = InTitle;
    House->bPrivate = bInPrivate;

    const FQuat Turn = Base.GetRotation();
    auto At = [&Base](float X, float Y, float Z)
    {
        return Base.TransformPosition(FVector(X, Y, Z));
    };

    constexpr int32 Rows = 11;
    const float HalfX = Footprint.X * 0.5f;
    const float HalfY = Footprint.Y * 0.5f;
    const float Log = 24.0f;
    const float Step = Log * 0.86f;
    const float Over = 25.0f;
    const float StoneTop = 36.0f;
    const float XBase = StoneTop + Log * 0.5f;
    const float YBase = XBase + Step * 0.5f;
    const float DoorHalf = 60.0f;
    const float DoorBottom = YBase + Log * 0.5f;
    const float DoorTop = 250.0f;
    const float WindowHalf = 35.0f;
    const float WindowLow = 145.0f;
    const float WindowHigh = 220.0f;
    House->OwnerAnchor = At(HalfX + 190.0f, 0.0f, 0.0f);

    struct FStoneSpot
    {
        float X;
        float Y;
        int32 Piece;
    };
    TArray<FStoneSpot> Stones = {
        { -HalfX, -HalfY, INDEX_NONE }, { HalfX, -HalfY, INDEX_NONE }, { -HalfX, HalfY, INDEX_NONE }, { HalfX, HalfY, INDEX_NONE },
        { 0.0f, -HalfY, INDEX_NONE }, { 0.0f, HalfY, INDEX_NONE }, { -HalfX, 0.0f, INDEX_NONE }, { HalfX, 0.0f, INDEX_NONE } };
    int32 StoneLook = 0;
    for (FStoneSpot& Spot : Stones)
    {
        const FName Rock(*FString::Printf(TEXT("Quaternius/Nature|Pebble_Square_%d"), 1 + (StoneLook++) % 6));
        Spot.Piece = House->AddModelled(Rock, At(Spot.X, Spot.Y, StoneTop * 0.5f), FVector(44.0f, 44.0f, StoneTop), Turn,
                                        EResourceKind::Granite, TEXT("камень"), false, 0.6f);
        House->Join(Spot.Piece, INDEX_NONE, EJointKind::Ground);
    }

    struct FSegment
    {
        int32 Piece;
        float From;
        float To;
    };
    TArray<FSegment> Walls[4][Rows];

    auto Covering = [&Walls](int32 Wall, int32 Row, float Coord) -> int32
    {
        if (Row < 0 || Row >= Rows)
        {
            return INDEX_NONE;
        }
        for (const FSegment& Segment : Walls[Wall][Row])
        {
            if (Coord >= Segment.From - 1.0f && Coord <= Segment.To + 1.0f)
            {
                return Segment.Piece;
            }
        }
        return INDEX_NONE;
    };

    auto LineOf = [HalfX, HalfY](int32 Wall)
    {
        switch (Wall)
        {
        case 0:  return -HalfY;
        case 1:  return HalfY;
        case 2:  return -HalfX;
        default: return HalfX;
        }
    };

    int32 WindowLowRow[4] = { Rows, Rows, Rows, Rows };
    int32 WindowHighRow[4] = { -1, -1, -1, -1 };
    int32 DoorLowRow = Rows;
    int32 DoorHighRow = -1;

    for (int32 Row = 0; Row < Rows; ++Row)
    {
        for (int32 Wall = 0; Wall < 4; ++Wall)
        {
            const bool bAlongX = Wall < 2;
            const float Line = LineOf(Wall);
            const float Z = (bAlongX ? XBase : YBase) + Row * Step;
            const float Half = (bAlongX ? HalfX : HalfY) + Over;

            bool bGap = false;
            float GapHalf = 0.0f;
            if (Wall == 3 && Z > DoorBottom && Z < DoorTop)
            {
                bGap = true;
                GapHalf = DoorHalf;
                DoorLowRow = FMath::Min(DoorLowRow, Row);
                DoorHighRow = FMath::Max(DoorHighRow, Row);
            }
            else if (Wall != 3 && Z >= WindowLow && Z <= WindowHigh)
            {
                bGap = true;
                GapHalf = WindowHalf;
                WindowLowRow[Wall] = FMath::Min(WindowLowRow[Wall], Row);
                WindowHighRow[Wall] = FMath::Max(WindowHighRow[Wall], Row);
            }

            TArray<FVector2D> Spans;
            if (bGap)
            {
                Spans.Add(FVector2D(-Half, -GapHalf));
                Spans.Add(FVector2D(GapHalf, Half));
            }
            else
            {
                Spans.Add(FVector2D(-Half, Half));
            }

            for (const FVector2D& Span : Spans)
            {
                const FVector From = bAlongX ? At(Span.X, Line, Z) : At(Line, Span.X, Z);
                const FVector To = bAlongX ? At(Span.Y, Line, Z) : At(Line, Span.Y, Z);
                const int32 Piece = House->AddLog(From, To, Log, EResourceKind::Wood, TEXT("венец"), false, 0.35f);
                Walls[Wall][Row].Add({ Piece, static_cast<float>(Span.X), static_cast<float>(Span.Y) });
            }
        }
    }

    for (int32 Row = 0; Row < Rows; ++Row)
    {
        for (int32 Wall = 0; Wall < 4; ++Wall)
        {
            const bool bAlongX = Wall < 2;
            for (const FSegment& Segment : Walls[Wall][Row])
            {
                if (bAlongX)
                {
                    const float SideY = LineOf(Wall);
                    if (-HalfX >= Segment.From && -HalfX <= Segment.To)
                    {
                        House->Join(Segment.Piece, Covering(2, Row - 1, SideY), EJointKind::Notch);
                    }
                    if (HalfX >= Segment.From && HalfX <= Segment.To)
                    {
                        House->Join(Segment.Piece, Covering(3, Row - 1, SideY), EJointKind::Notch);
                    }
                    if (Row == 0)
                    {
                        for (const FStoneSpot& Spot : Stones)
                        {
                            if (FMath::IsNearlyEqual(Spot.Y, SideY) && Spot.X >= Segment.From && Spot.X <= Segment.To)
                            {
                                House->Join(Segment.Piece, Spot.Piece, EJointKind::Rest);
                            }
                        }
                    }
                }
                else
                {
                    const float SideX = LineOf(Wall);
                    if (-HalfY >= Segment.From && -HalfY <= Segment.To)
                    {
                        House->Join(Segment.Piece, Covering(0, Row, SideX), EJointKind::Notch);
                    }
                    if (HalfY >= Segment.From && HalfY <= Segment.To)
                    {
                        House->Join(Segment.Piece, Covering(1, Row, SideX), EJointKind::Notch);
                    }
                    if (Row == 0)
                    {
                        for (const FStoneSpot& Spot : Stones)
                        {
                            if (FMath::IsNearlyEqual(Spot.X, SideX) && Spot.Y >= Segment.From && Spot.Y <= Segment.To)
                            {
                                House->Join(Segment.Piece, Spot.Piece, EJointKind::Rest);
                            }
                        }
                    }
                }

                if (Row > 0)
                {
                    for (const FSegment& Under : Walls[Wall][Row - 1])
                    {
                        if (Under.To > Segment.From && Under.From < Segment.To)
                        {
                            House->Join(Segment.Piece, Under.Piece, EJointKind::Rest);
                        }
                    }
                }
            }
        }
    }

    if (DoorHighRow >= DoorLowRow)
    {
        const float PostHeight = DoorTop - DoorBottom;
        for (int32 Side = -1; Side <= 1; Side += 2)
        {
            const int32 Post = House->AddModelled(TEXT("Quaternius/Village|Corner_Exterior_Wood"), At(HalfX, Side * (DoorHalf + 7.0f), DoorBottom + PostHeight * 0.5f),
                FVector(18.0f, 14.0f, PostHeight), Turn, EResourceKind::Plank, TEXT("колода"), false, 0.3f);
            for (int32 Row = DoorLowRow; Row <= DoorHighRow; ++Row)
            {
                House->Join(Post, Covering(3, Row, Side * (DoorHalf + 7.0f)), EJointKind::Peg);
            }
            House->Join(Post, Covering(3, DoorLowRow - 1, 0.0f), EJointKind::Rest);
        }
    }

    for (int32 Wall = 0; Wall < 3; ++Wall)
    {
        if (WindowHighRow[Wall] < WindowLowRow[Wall])
        {
            continue;
        }
        const bool bAlongX = Wall < 2;
        const float Base0 = bAlongX ? XBase : YBase;
        const float Low = Base0 + WindowLowRow[Wall] * Step - Log * 0.5f;
        const float High = Base0 + WindowHighRow[Wall] * Step + Log * 0.5f;
        const float Line = LineOf(Wall);
        const FVector Size(WindowHalf * 2.0f, 16.0f, High - Low);
        const FQuat Facing = bAlongX ? Turn : Turn * FQuat(FVector::UpVector, UE_HALF_PI);
        const int32 Pane = House->AddModelled(TEXT("Quaternius/Village|Window_Thin_Flat1"),
            bAlongX ? At(0.0f, Line, (Low + High) * 0.5f) : At(Line, 0.0f, (Low + High) * 0.5f),
            Size, Facing, EResourceKind::Glass, TEXT("окно"), false, 0.4f);
        House->Join(Pane, Covering(Wall, WindowLowRow[Wall] - 1, 0.0f), EJointKind::Rest);
        House->Join(Pane, Covering(Wall, WindowHighRow[Wall] + 1, 0.0f), EJointKind::Nail);
    }

    TArray<int32> Joists;
    for (int32 Col = -1; Col <= 1; ++Col)
    {
        const float X = Col * HalfX * 0.5f;
        const int32 Joist = House->AddLog(At(X, -HalfY, 47.0f), At(X, HalfY, 47.0f), 16.0f, EResourceKind::Wood, TEXT("лага"), true, 0.0f);
        House->Join(Joist, Covering(0, 0, X), EJointKind::Rest);
        House->Join(Joist, Covering(1, 0, X), EJointKind::Rest);
        Joists.Add(Joist);
    }

    const int32 Boards = FMath::Max(1, FMath::FloorToInt((HalfY * 2.0f - 30.0f) / 25.0f));
    for (int32 Board = 0; Board < Boards; ++Board)
    {
        const float Y = -HalfY + 15.0f + 12.5f + Board * 25.0f;
        const int32 Plank = House->AddModelled(TEXT("Quaternius/Village|Floor_WoodDark"), At(0.0f, Y, 57.5f), FVector(HalfX * 2.0f - 30.0f, 24.5f, 5.0f), Turn,
            EResourceKind::Plank, TEXT("половица"), true, 0.0f);
        for (int32 Joist : Joists)
        {
            House->Join(Plank, Joist, EJointKind::Nail);
        }
    }

    const float EaveZ = XBase + (Rows - 1) * Step;
    const float TopY = YBase + (Rows - 1) * Step;
    const float Run = HalfY + Over;
    const float Pitch = FMath::DegreesToRadians(38.0f);
    const float Rise = Run * FMath::Tan(Pitch);

    struct FGable
    {
        int32 Piece;
        float Half;
        float Z;
    };
    TArray<FGable> Gables[2];
    for (int32 G = 0; G < 2; ++G)
    {
        const int32 Wall = G == 0 ? 2 : 3;
        const float Line = LineOf(Wall);
        int32 Below = INDEX_NONE;
        for (int32 Level = 0; Level < 40; ++Level)
        {
            const float Z = TopY + (Level + 1) * Step;
            const float Height = Z - EaveZ;
            const float Half = Run * (1.0f - Height / Rise);
            if (Half < 30.0f)
            {
                break;
            }
            const int32 Piece = House->AddLog(At(Line, -Half, Z), At(Line, Half, Z), Log, EResourceKind::Wood, TEXT("самец"), false, 0.5f);
            if (Below == INDEX_NONE)
            {
                for (const FSegment& Segment : Walls[Wall][Rows - 1])
                {
                    House->Join(Piece, Segment.Piece, EJointKind::Rest);
                }
            }
            else
            {
                House->Join(Piece, Below, EJointKind::Rest);
            }
            Gables[G].Add({ Piece, Half, Z });
            Below = Piece;
        }
    }

    auto GableAt = [&Gables](int32 G, float AbsY) -> int32
    {
        int32 Found = INDEX_NONE;
        for (const FGable& Level : Gables[G])
        {
            if (Level.Half >= AbsY)
            {
                Found = Level.Piece;
            }
        }
        return Found;
    };

    const float Reach = HalfX + Over + 25.0f;
    TArray<int32> Purlins[2];
    for (int32 Side = 0; Side < 2; ++Side)
    {
        const float Sign = Side == 0 ? -1.0f : 1.0f;
        for (int32 Part = 1; Part <= 2; ++Part)
        {
            const float Fraction = Part / 3.0f;
            const float Y = Sign * Run * (1.0f - Fraction);
            const float Z = EaveZ + Fraction * Rise + Log * 0.6f;
            const int32 Purlin = House->AddLog(At(-Reach, Y, Z), At(Reach, Y, Z), 20.0f, EResourceKind::Wood, TEXT("слега"), true, 0.3f);
            House->Join(Purlin, GableAt(0, FMath::Abs(Y)), EJointKind::Notch);
            House->Join(Purlin, GableAt(1, FMath::Abs(Y)), EJointKind::Notch);
            Purlins[Side].Add(Purlin);
        }
    }

    const float RidgeZ = EaveZ + Rise + 10.0f;
    const int32 Ridge = House->AddLog(At(-Reach - 5.0f, 0.0f, RidgeZ), At(Reach + 5.0f, 0.0f, RidgeZ), 26.0f, EResourceKind::Wood, TEXT("конёк"), true, 0.8f);
    for (int32 G = 0; G < 2; ++G)
    {
        if (Gables[G].Num() > 0)
        {
            House->Join(Ridge, Gables[G].Last().Piece, EJointKind::Rest);
        }
    }

    const bool bThatch = Roofing == EResourceKind::Straw || Roofing == EResourceKind::Reed;
    const float Thick = bThatch ? 18.0f : 5.0f;
    const float Span = Reach * 2.0f;
    const int32 Bays = FMath::Max(1, FMath::CeilToInt(Span / 160.0f));
    const float Width = Span / Bays;
    for (int32 Side = 0; Side < 2; ++Side)
    {
        const float Sign = Side == 0 ? -1.0f : 1.0f;
        const FVector Low(0.0f, Sign * (Run + 35.0f), EaveZ + Log * 0.5f - 35.0f * FMath::Tan(Pitch));
        const FVector High(0.0f, 0.0f, RidgeZ + 13.0f);
        const FVector Along = High - Low;
        const float Length = Along.Size();
        const FVector Down = (-Along).GetSafeNormal();
        const FVector Outward = FVector::CrossProduct(FVector::ForwardVector, Down).GetSafeNormal() * (Sign > 0.0f ? 1.0f : -1.0f);
        const FVector Lift = (Outward.Z < 0.0f ? -Outward : Outward) * (Thick * 0.5f);
        const FQuat Slope = FRotationMatrix::MakeFromXY(FVector::ForwardVector, Down).ToQuat();
        const int32 Wall = Side == 0 ? 0 : 1;
        for (int32 Bay = 0; Bay < Bays; ++Bay)
        {
            const float X = -Reach + (Bay + 0.5f) * Width;
            const FVector Middle = (Low + High) * 0.5f + Lift;
            const int32 Panel = House->AddModelled(TEXT("Quaternius/Village|Roof_Wooden_2x1_Middle"), At(X, Middle.Y, Middle.Z),
                FVector(Width - 2.0f, Length, Thick), Turn * Slope, Roofing, TEXT("кровля"), false, 1.0f);
            const EJointKind Tie = bThatch ? EJointKind::Lashing : EJointKind::Nail;
            House->Join(Panel, Ridge, Tie);
            for (int32 Purlin : Purlins[Side])
            {
                House->Join(Panel, Purlin, Tie);
            }
            House->Join(Panel, Covering(Wall, Rows - 1, X), Tie);
        }
    }

    const int32 LowStep = House->AddModelled(TEXT("Quaternius/Village|Prop_Crate"), At(HalfX + 75.0f, 0.0f, 11.0f), FVector(50.0f, DoorHalf * 2.0f + 40.0f, 22.0f), Turn,
        EResourceKind::Plank, TEXT("крыльцо"), false, 1.0f);
    const int32 HighStep = House->AddModelled(TEXT("Quaternius/Village|Prop_Crate"), At(HalfX + 35.0f, 0.0f, 22.5f), FVector(40.0f, DoorHalf * 2.0f + 40.0f, 45.0f), Turn,
        EResourceKind::Plank, TEXT("крыльцо"), false, 1.0f);
    House->Join(LowStep, INDEX_NONE, EJointKind::Ground);
    House->Join(HighStep, INDEX_NONE, EJointKind::Ground);

    const FVector2D FlueAt(-HalfX + 95.0f, 70.0f);
    const int32 Footing = House->AddModelled(TEXT("Quaternius/Nature|Pebble_Square_2"), At(FlueAt.X, FlueAt.Y, StoneTop * 0.5f),
        FVector(70.0f, 70.0f, StoneTop), Turn, EResourceKind::Granite, TEXT("подпечье"), false, 0.0f);
    House->Join(Footing, INDEX_NONE, EJointKind::Ground);
    const float FlueTop = RidgeZ + 60.0f;
    const int32 Chimney = House->AddModelled(TEXT("Quaternius/Village|Prop_Chimney"), At(FlueAt.X, FlueAt.Y, (StoneTop + FlueTop) * 0.5f),
        FVector(44.0f, 44.0f, FlueTop - StoneTop), Turn, EResourceKind::Brick, TEXT("труба"), false, 0.5f);
    House->Join(Chimney, Footing, EJointKind::Rest);

    House->Finish();
    return House;
}
