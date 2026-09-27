#include "MatterComponent.h"
#include "MatterSubsystem.h"
#include "MatterStructure.h"
#include "TerrainGrid.h"
#include "ResourceActor.h"
#include "HumanWorldSubsystem.h"
#include "GameFramework/Character.h"
#include "Components/PrimitiveComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Engine/World.h"

namespace
{
    float VolumeShare(EMatterShape Shape)
    {
        switch (Shape)
        {
        case EMatterShape::Cylinder: return UE_PI / 4.0f;
        case EMatterShape::Sphere:   return UE_PI / 6.0f;
        case EMatterShape::Cone:     return UE_PI / 12.0f;
        default:                     return 1.0f;
        }
    }

    float AreaShare(EMatterShape Shape)
    {
        switch (Shape)
        {
        case EMatterShape::Cylinder: return 0.85f;
        case EMatterShape::Sphere:   return 0.55f;
        case EMatterShape::Cone:     return 0.6f;
        default:                     return 1.0f;
        }
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

    int32 LongestAxis(const FVector& Size)
    {
        if (Size.X >= Size.Y && Size.X >= Size.Z)
        {
            return 0;
        }
        return Size.Y >= Size.Z ? 1 : 2;
    }

    constexpr float StoneCompliance = 1.0f / 50.0f;
}

UMatterComponent::UMatterComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    PrimaryComponentTick.TickInterval = 0.1f;
    PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UMatterComponent::BeginPlay()
{
    Super::BeginPlay();

    if (UMatterSubsystem* Matter = UMatterSubsystem::Get(this))
    {
        Matter->Register(this);
        if (const AActor* Owner = GetOwner())
        {
            Temperature = Matter->AirAt(Owner->GetActorLocation(), false).Celsius;
        }
        CheckShelter(Matter);
    }
    WatchRest();
}

void UMatterComponent::WatchRest()
{
    if (Body && Body->IsSimulatingPhysics() && !bVisualOnly && HasBegunPlay())
    {
        RestClock = 0.0f;
        SetComponentTickEnabled(true);
    }
}

bool UMatterComponent::IsUpright() const
{
    const AActor* Owner = GetOwner();
    if (!Owner || !Body)
    {
        return false;
    }
    const FVector Axis = Body->GetComponentQuat().GetAxisZ();
    return FMath::Abs(Axis.Z) > 0.7f;
}

float UMatterComponent::ReposeDegrees(const AActor* GroundActor, const FVector& Where) const
{
    const FSubstance& S = Props();
    const EMatterPhase Now = Phase();
    if (Now == EMatterPhase::Plastic || Now == EMatterPhase::Slurry)
    {
        return 75.0f;
    }

    float Soft = 0.0f;
    if (const ATerrainGrid* Ground = Cast<ATerrainGrid>(GroundActor))
    {
        FTerrainFooting Footing;
        if (Ground->FootingAt(Where, TotalMassKg() * 0.35f, Footing))
        {
            Soft = FMath::Clamp(Footing.SinkCm * 3.0f + Footing.Stick * 10.0f, 0.0f, 25.0f);
        }
        if (Ground->IsGrassAt(Where))
        {
            Soft += 7.0f;
        }
    }

    const float Friction = FMatter::FrictionOf(S, Moisture, Temperature, false);
    const float Sliding = FMath::RadiansToDegrees(FMath::Atan(FMath::Max(0.05f, Friction))) + Soft * 0.5f;
    switch (Shape)
    {
    case EMatterShape::Sphere:
        return FMath::Min(Sliding, 24.0f + Soft);
    case EMatterShape::Cylinder:
        return IsUpright() ? Sliding : FMath::Min(Sliding, 3.5f + Soft);
    case EMatterShape::Cone:
        return FMath::Min(Sliding, 34.0f + Soft);
    default:
        return Sliding;
    }
}

void UMatterComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    UWorld* World = GetWorld();
    if (!World || !Body || bVisualOnly || !Body->IsSimulatingPhysics())
    {
        SetComponentTickEnabled(false);
        return;
    }
    if (!Body->IsAnyRigidBodyAwake())
    {
        RestClock = 0.0f;
        SetComponentTickEnabled(false);
        return;
    }

    const float Speed = Body->GetPhysicsLinearVelocity().Size();
    const float Spin = Body->GetPhysicsAngularVelocityInRadians().Size();
    if (Speed > 18.0f || Spin > 2.0f)
    {
        RestClock = 0.0f;
        return;
    }

    const FBox Box = Body->Bounds.GetBox();
    const FVector Centre = Box.GetCenter();
    FCollisionQueryParams Params(SCENE_QUERY_STAT(MatterRest), false, GetOwner());
    FHitResult Ground;
    if (!World->LineTraceSingleByChannel(Ground, Centre, Centre - FVector(0.0f, 0.0f, Box.GetExtent().Z + 6.0f), ECC_WorldStatic, Params))
    {
        RestClock = 0.0f;
        return;
    }

    const float Slope = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(static_cast<float>(Ground.ImpactNormal.Z), -1.0f, 1.0f)));
    if (Slope > ReposeDegrees(Ground.GetActor(), Ground.ImpactPoint))
    {
        RestClock = 0.0f;
        return;
    }

    RestClock += DeltaTime;
    if (RestClock > 0.3f)
    {
        Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
        Body->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
        Body->PutRigidBodyToSleep();
        RestClock = 0.0f;
        SetComponentTickEnabled(false);
    }
}

void UMatterComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    if (UMatterSubsystem* Matter = UMatterSubsystem::Get(this))
    {
        Matter->Unregister(this);
    }
    if (Body)
    {
        Body->OnComponentHit.RemoveDynamic(this, &UMatterComponent::OnBodyHit);
    }
    Super::EndPlay(Reason);
}

float UMatterComponent::VolumeOf(EMatterShape InShape, const FVector& InSizeCm)
{
    return FMath::Max(1.0e-7f, static_cast<float>(InSizeCm.X * InSizeCm.Y * InSizeCm.Z) * 1.0e-6f * VolumeShare(InShape));
}

void UMatterComponent::Bind(UPrimitiveComponent* InBody, EMatterShape InShape, const FVector& InSizeCm, EResourceKind InSubstance)
{
    const bool bSame = bBound && Body == InBody && Substance == InSubstance;

    if (Body && Body != InBody)
    {
        Body->OnComponentHit.RemoveDynamic(this, &UMatterComponent::OnBodyHit);
    }

    Body = InBody;
    Shape = InShape;
    SizeCm = InSizeCm.ComponentMax(FVector(0.5f));
    Substance = InSubstance;

    const FSubstance& S = Props();
    if (bMassFromShape)
    {
        DryMassKg = FMath::Max(0.005f, S.Density * VolumeM3());
    }

    if (!bSame)
    {
        OriginalMassKg = DryMassKg;
        float Humidity = 0.7f;
        if (UMatterSubsystem* Matter = UMatterSubsystem::Get(this))
        {
            Humidity = Matter->Climate().Humidity;
        }
        Moisture = FMatter::AirDryMoisture(S, Humidity);
        Damage = 0.0f;
        Char = 0.0f;
        Rot = 0.0f;
        Ice = 0.0f;
        Transition = 0.0f;
        bBurning = false;
    }
    else
    {
        OriginalMassKg = FMath::Max(OriginalMassKg, DryMassKg);
    }
    bBound = true;

    if (Body)
    {
        Body->SetNotifyRigidBodyCollision(true);
        Body->OnComponentHit.AddUniqueDynamic(this, &UMatterComponent::OnBodyHit);
        Visuals.AddUnique(Body);
    }

    RefreshPhysics(true);
    RefreshLook(true);
}

void UMatterComponent::AddVisual(UPrimitiveComponent* Part)
{
    if (!Part)
    {
        return;
    }
    Visuals.AddUnique(Part);
    ApplyState(Part);
}

void UMatterComponent::Resize(const FVector& InSizeCm)
{
    SizeCm = InSizeCm.ComponentMax(FVector(0.5f));
    if (Body)
    {
        Body->SetWorldScale3D(SizeCm / 100.0f);
    }
}

void UMatterComponent::SetDryMass(float Kilograms)
{
    DryMassKg = FMath::Max(0.005f, Kilograms);
    if (!bBound)
    {
        OriginalMassKg = DryMassKg;
    }
    RefreshPhysics(true);
}

const FSubstance& UMatterComponent::Props() const
{
    return FMatter::Of(Substance);
}

EMatterPhase UMatterComponent::Phase() const
{
    return FMatter::PhaseOf(Props(), Moisture, Temperature, bBurning, Char, Rot);
}

float UMatterComponent::WetFraction() const
{
    return FMatter::WetFraction(Props(), Moisture);
}

float UMatterComponent::VolumeM3() const
{
    return VolumeOf(Shape, SizeCm);
}

float UMatterComponent::TotalMassKg() const
{
    return DryMassKg * (1.0f + FMath::Max(0.0f, Moisture));
}

float UMatterComponent::SurfaceM2() const
{
    const float X = SizeCm.X;
    const float Y = SizeCm.Y;
    const float Z = SizeCm.Z;
    return FMath::Max(1.0e-4f, 2.0f * (X * Y + Y * Z + X * Z) * 1.0e-4f * AreaShare(Shape));
}

float UMatterComponent::TopM2() const
{
    const float Share = Shape == EMatterShape::Box ? 1.0f : UE_PI / 4.0f;
    return FMath::Max(1.0e-4f, static_cast<float>(SizeCm.X * SizeCm.Y) * 1.0e-4f * Share);
}

float UMatterComponent::SmallestSectionM2() const
{
    float Small = 0.0f;
    float Middle = 0.0f;
    float Large = 0.0f;
    Ordered(SizeCm, Small, Middle, Large);
    const float Share = Shape == EMatterShape::Box ? 1.0f : UE_PI / 4.0f;
    return FMath::Max(1.0e-6f, Small * Middle * 1.0e-4f * Share);
}

float UMatterComponent::LengthwiseSectionM2() const
{
    float Small = 0.0f;
    float Middle = 0.0f;
    float Large = 0.0f;
    Ordered(SizeCm, Small, Middle, Large);
    return FMath::Max(1.0e-6f, Large * Middle * 1.0e-4f);
}

void UMatterComponent::ReceiveWater(float Kilograms)
{
    if (Kilograms <= 0.0f)
    {
        return;
    }
    const float Limit = FMatter::WaterLimit(Props());
    if (Limit <= 0.0f)
    {
        return;
    }
    Moisture = FMath::Min(Limit, Moisture + Kilograms / FMath::Max(0.001f, DryMassKg));
    RefreshPhysics(false);
    RefreshLook(false);
}

void UMatterComponent::ReceiveHeat(float Joules)
{
    const FSubstance& S = Props();
    const float Capacity = FMath::Max(1.0f, S.HeatCapacity * DryMassKg + Moisture * DryMassKg * FMatter::WaterHeatCapacity);
    Temperature += Joules / Capacity;
}

void UMatterComponent::Advance(float Seconds, const FMatterAir& Air)
{
    if (Seconds <= 0.0f || bVisualOnly || !bBound)
    {
        return;
    }
    AActor* Owner = GetOwner();
    UMatterSubsystem* Matter = UMatterSubsystem::Get(this);
    if (!Owner || !Matter)
    {
        return;
    }

    const FSubstance& S = Props();
    FMatterBody State;
    State.Moisture = Moisture;
    State.Temperature = Temperature;
    State.Ice = Ice;
    State.Transition = Transition;
    State.Damage = Damage;
    State.Char = Char;
    State.Rot = Rot;
    State.Snow = SnowCover;
    State.DryMass = DryMassKg;
    State.OriginalMass = OriginalMassKg;
    State.bBurning = bBurning;

    const FMatterStep Result = FMatter::Step(S, State, SurfaceM2(), TopM2(), Air, Seconds);

    Moisture = State.Moisture;
    Temperature = State.Temperature;
    Ice = State.Ice;
    Transition = State.Transition;
    Damage = State.Damage;
    Char = State.Char;
    Rot = State.Rot;
    SnowCover = State.Snow;
    DryMassKg = State.DryMass;
    bBurning = State.bBurning;

    if (Result.Released > 0.0f)
    {
        Matter->AddHeat(Owner->GetActorLocation(), Result.Released);
    }
    if (Result.bIgnited)
    {
        bBurning = false;
        Ignite();
    }

    AResourceActor* Pile = Cast<AResourceActor>(Owner);
    if (Result.bBurnedOut)
    {
        BurnOut();
        return;
    }
    if (Pile && Result.bFired)
    {
        Pile->BecomeKind(S.FiresInto, 0.92f);
        return;
    }
    if (Pile && Result.bTransformed)
    {
        Pile->BecomeKind(S.MeltsInto, S.Form == EMatterForm::Liquid ? 1.09f : 0.95f);
        return;
    }
    if (Pile && Result.bDissolved)
    {
        Pile->Destroy();
        return;
    }
    if (Pile && Result.bSpoiled)
    {
        Pile->Setup(Pile->Kind, Pile->Amount);
    }

    RefreshPhysics(false);
    RefreshLook(Result.bExtinguished);
}

void UMatterComponent::OnBodyHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit)
{
    UWorld* World = GetWorld();
    if (!World || !bBound || bFracturing || !HitComponent || !HitComponent->IsSimulatingPhysics())
    {
        return;
    }
    if (!IsComponentTickEnabled())
    {
        WatchRest();
    }

    const double Now = World->GetTimeSeconds();
    if (Now - LastImpactAt < 0.08)
    {
        return;
    }

    const float Mine = FMath::Max(0.01f, TotalMassKg());
    const float Resting = Mine * FMatter::Gravity * FMath::Max(0.005f, World->GetDeltaSeconds()) * 1.5f;
    const float Impulse = static_cast<float>(NormalImpulse.Size()) / 100.0f - Resting;
    if (Impulse < 0.2f)
    {
        return;
    }

    const bool bOtherMoves = OtherComponent && OtherComponent->IsSimulatingPhysics();
    const float Theirs = bOtherMoves ? FMath::Max(0.01f, OtherComponent->GetMass()) : 0.0f;
    const float Reduced = bOtherMoves ? Mine * Theirs / (Mine + Theirs) : Mine;
    const float Bounce = FMatter::BounceOf(Props(), Moisture, Temperature);
    const float Energy = Impulse * Impulse / (2.0f * FMath::Max(0.01f, Reduced)) * (1.0f - Bounce * Bounce);
    if (Energy < 0.5f)
    {
        return;
    }
    LastImpactAt = Now;

    UMatterComponent* Other = OtherActor ? OtherActor->FindComponentByClass<UMatterComponent>() : nullptr;
    const float Soft = FMatter::Compliance(Props(), Moisture, Temperature);
    const float MyShare = Other ? ImpactShare(Other) : Soft / (Soft + StoneCompliance);

    FVector Direction = NormalImpulse.GetSafeNormal();
    if (Direction.IsNearlyZero())
    {
        Direction = Hit.ImpactNormal;
    }

    if (AMatterStructure* House = Cast<AMatterStructure>(OtherActor))
    {
        const int32 Piece = House->FindPiece(OtherComponent, Hit.Item);
        if (Piece != INDEX_NONE)
        {
            const float Share = Soft / FMath::Max(1.0e-6f, Soft + House->PieceCompliance(Piece));
            ReceiveImpact(Energy * Share, Hit.ImpactPoint, Direction, OtherActor, false);
            House->ImpactPiece(Piece, Energy * (1.0f - Share), Hit.ImpactPoint, -Direction, GetOwner(), false);
            return;
        }
    }

    if (ATerrainGrid* Ground = Cast<ATerrainGrid>(OtherActor))
    {
        const float Share = Soft / FMath::Max(1.0e-6f, Soft + Ground->ComplianceAt(Hit.ImpactPoint));
        ReceiveImpact(Energy * Share, Hit.ImpactPoint, Direction, OtherActor, false);
        Ground->Impact(Hit.ImpactPoint, Energy * (1.0f - Share));
        return;
    }

    ReceiveImpact(Energy * MyShare, Hit.ImpactPoint, Direction, OtherActor, false);
    if (Other && Other != this && !bOtherMoves)
    {
        Other->ReceiveImpact(Energy * (1.0f - MyShare), Hit.ImpactPoint, -Direction, GetOwner(), false);
    }
}

float UMatterComponent::ImpactShare(const UMatterComponent* Other) const
{
    const float Mine = FMatter::Compliance(Props(), Moisture, Temperature);
    const float Theirs = Other ? FMatter::Compliance(Other->Props(), Other->Moisture, Other->Temperature) : StoneCompliance;
    return Mine / FMath::Max(1.0e-6f, Mine + Theirs);
}

void UMatterComponent::ReceiveImpact(float Joules, const FVector& Where, const FVector& Direction, AActor* Instigator, bool bEdge)
{
    if (!bBound || bVisualOnly || Joules <= 0.0f || bFracturing)
    {
        return;
    }

    const FSubstance& S = Props();
    const EMatterPhase Now = Phase();
    if (Now == EMatterPhase::Plastic || Now == EMatterPhase::Slurry)
    {
        Squash(Joules, Direction);
        return;
    }
    if (S.Form == EMatterForm::Liquid || !bBreakable)
    {
        return;
    }

    const float Strength = FMath::Max(0.02f, FMatter::StrengthFactor(S, Moisture, Temperature, Rot, Char));
    const float Across = FMatter::BreakEnergy(S, SmallestSectionM2(), false) * Strength;
    const float Along = S.bGrain ? FMatter::BreakEnergy(S, LengthwiseSectionM2(), true) * Strength : Across;
    const float Needed = bEdge ? FMath::Min(Across, Along) : Across * (S.bGrain ? 2.0f : 1.0f);
    const float Toughness = S.bMetal ? 4.0f : 1.0f;

    float Small = 0.0f;
    float Middle = 0.0f;
    float Large = 0.0f;
    Ordered(SizeCm, Small, Middle, Large);
    const float Aspect = Large / FMath::Max(0.5f, Small);
    const float Efficiency = bEdge ? 1.0f : FMath::Clamp(0.02f * Aspect * Aspect, 0.02f, 0.5f);
    const float Effective = Joules * Efficiency;

    float Gain = Effective / FMath::Max(0.01f, Needed * Toughness);
    if (S.bBrittle && Gain < 1.0f)
    {
        Gain *= 0.35f;
    }
    Damage += Gain;

    if (Damage >= 1.0f)
    {
        Fracture(Where, Direction, Effective, Instigator, bEdge);
    }
    else
    {
        RefreshLook(false);
    }
}

void UMatterComponent::Squash(float Joules, const FVector& Direction)
{
    const FSubstance& S = Props();
    const float Yield = FMatter::YieldPressure(S, Moisture, Temperature);
    float Strain = FMath::Clamp(Joules / FMath::Max(1.0f, Yield * VolumeM3()), 0.0f, 0.8f);
    if (Phase() == EMatterPhase::Slurry)
    {
        Strain = FMath::Max(Strain, 0.55f);
    }
    if (Strain < 0.02f)
    {
        return;
    }

    const AActor* Owner = GetOwner();
    FVector Local = Owner ? Owner->GetActorTransform().InverseTransformVectorNoScale(Direction) : Direction;
    Local = Local.GetAbs();
    const int32 Axis = (Local.X >= Local.Y && Local.X >= Local.Z) ? 0 : (Local.Y >= Local.Z ? 1 : 2);

    FVector Next = SizeCm;
    const float Keep = 1.0f - Strain;
    const float Spread = 1.0f / FMath::Sqrt(Keep);
    for (int32 i = 0; i < 3; ++i)
    {
        Next[i] *= (i == Axis) ? Keep : Spread;
    }
    Next[Axis] = FMath::Max(Next[Axis], 1.0);

    Resize(Next);
    if (Body)
    {
        Body->SetLinearDamping(3.0f);
        Body->SetAngularDamping(12.0f);
    }
    if (AResourceActor* Pile = Cast<AResourceActor>(GetOwner()))
    {
        Pile->CustomSize = Next;
    }
    RefreshLook(true);
}

void UMatterComponent::Fracture(const FVector& Where, const FVector& Direction, float Energy, AActor* Instigator, bool bEdge)
{
    AResourceActor* Pile = Cast<AResourceActor>(GetOwner());
    UWorld* World = GetWorld();
    if (!Pile || !World || bFracturing || Pile->IsActorBeingDestroyed())
    {
        Damage = FMath::Min(Damage, 0.98f);
        return;
    }
    bFracturing = true;

    const FSubstance& S = Props();
    const float Strength = FMath::Max(0.02f, FMatter::StrengthFactor(S, Moisture, Temperature, Rot, Char));
    const float Across = FMatter::BreakEnergy(S, SmallestSectionM2(), false) * Strength;
    const float Along = S.bGrain ? FMatter::BreakEnergy(S, LengthwiseSectionM2(), true) * Strength : Across;
    const bool bSplitAlong = S.bGrain && bEdge && Along < Across;
    const float Needed = bSplitAlong ? Along : Across;
    const float Ratio = Energy / FMath::Max(0.01f, Needed);

    int32 Pieces = 2;
    if (S.bBrittle || S.Form == EMatterForm::Grains || S.Form == EMatterForm::Paste)
    {
        Pieces = FMath::Clamp(2 + FMath::FloorToInt((Ratio - 1.0f) * 1.5f), 2, 6);
    }

    struct FChunk
    {
        FVector Centre;
        FVector Size;
    };
    TArray<FChunk> Chunks;
    Chunks.Add({ FVector::ZeroVector, SizeCm });

    const int32 GrainAxis = LongestAxis(SizeCm);
    FRandomStream Luck(static_cast<int32>(GetUniqueID()) ^ FMath::Rand());

    while (Chunks.Num() < Pieces)
    {
        int32 Biggest = 0;
        for (int32 i = 1; i < Chunks.Num(); ++i)
        {
            if (Chunks[i].Size.GetMax() > Chunks[Biggest].Size.GetMax())
            {
                Biggest = i;
            }
        }
        const FChunk Cut = Chunks[Biggest];
        int32 Axis = LongestAxis(Cut.Size);
        if (bSplitAlong && Axis == GrainAxis)
        {
            const int32 A = (GrainAxis + 1) % 3;
            const int32 B = (GrainAxis + 2) % 3;
            Axis = Cut.Size[A] >= Cut.Size[B] ? A : B;
        }
        const float Part = S.bBrittle ? Luck.FRandRange(0.35f, 0.65f) : 0.5f;
        FChunk First = Cut;
        FChunk Second = Cut;
        First.Size[Axis] = Cut.Size[Axis] * Part;
        Second.Size[Axis] = Cut.Size[Axis] * (1.0f - Part);
        First.Centre[Axis] = Cut.Centre[Axis] - Cut.Size[Axis] * 0.5f + First.Size[Axis] * 0.5f;
        Second.Centre[Axis] = Cut.Centre[Axis] + Cut.Size[Axis] * 0.5f - Second.Size[Axis] * 0.5f;
        Chunks[Biggest] = First;
        Chunks.Add(Second);
    }

    const FQuat Rotation = Pile->GetActorQuat();
    const FVector Origin = Pile->GetActorLocation();
    const FVector Velocity = Body ? Body->GetPhysicsLinearVelocity() : FVector::ZeroVector;
    const float Excess = FMath::Max(0.0f, Energy - Needed);
    const float Kick = FMath::Min(600.0f, FMath::Sqrt(2.0f * Excess / FMath::Max(0.1f, TotalMassKg())) * 50.0f);
    const float WholeVolume = FMath::Max(1.0f, static_cast<float>(SizeCm.X * SizeCm.Y * SizeCm.Z));
    const float WholeAmount = Pile->Amount;
    const float KeptMoisture = Moisture;
    const float KeptTemperature = Temperature;
    const float KeptChar = Char;
    const float KeptRot = Rot;
    const EResourceKind KeptKind = Substance;
    const EMatterShape Whole = Shape;
    const FVector WholeSize = SizeCm;

    if (Pile->HeldBy)
    {
        Pile->PutDown();
    }
    Pile->Destroy();

    for (const FChunk& Chunk : Chunks)
    {
        if (Chunk.Size.GetMin() < 1.5f)
        {
            continue;
        }
        EMatterShape PieceShape = Whole;
        if (Whole == EMatterShape::Cylinder && (Chunk.Size.X < WholeSize.X * 0.95f || Chunk.Size.Y < WholeSize.Y * 0.95f))
        {
            PieceShape = EMatterShape::Box;
        }
        if (Whole == EMatterShape::Cone)
        {
            PieceShape = EMatterShape::Sphere;
        }
        const float Share = static_cast<float>(Chunk.Size.X * Chunk.Size.Y * Chunk.Size.Z) / WholeVolume;
        const FVector At = Origin + Rotation.RotateVector(Chunk.Centre);
        AResourceActor* Piece = AResourceActor::SpawnPiece(World, KeptKind, WholeAmount * Share, FTransform(Rotation, At), Chunk.Size, PieceShape);
        if (!Piece || !Piece->Matter)
        {
            continue;
        }
        Piece->Matter->Moisture = KeptMoisture;
        Piece->Matter->Temperature = KeptTemperature;
        Piece->Matter->Char = KeptChar;
        Piece->Matter->Rot = KeptRot;
        Piece->Matter->Damage = 0.15f;
        if (UPrimitiveComponent* PieceBody = Piece->Matter->GetBody())
        {
            const FVector Out = Rotation.RotateVector(Chunk.Centre).GetSafeNormal();
            PieceBody->SetPhysicsLinearVelocity(Velocity + Out * Kick + Direction.GetSafeNormal() * Kick * 0.3f);
        }
    }

    if (UHumanWorldSubsystem* WorldMind = World->GetSubsystem<UHumanWorldSubsystem>())
    {
        FWorldEvent Event;
        Event.Tag = TEXT("Break");
        Event.Description = FString::Printf(TEXT("раскололось: %s"), S.Name);
        Event.Location = Where;
        Event.Radius = 1200.0f;
        Event.Instigator = Instigator;
        Event.Valence = -0.1f;
        Event.Significance = FMath::Clamp(Energy / 2000.0f, 0.1f, 0.6f);
        WorldMind->BroadcastEvent(Event);
    }
}

void UMatterComponent::BurnOut()
{
    bBurning = false;
    AActor* Owner = GetOwner();
    const FSubstance& S = Props();
    if (AResourceActor* Pile = Cast<AResourceActor>(Owner))
    {
        if (S.Ember != EResourceKind::None)
        {
            const float Heat = FMath::Min(Temperature, 300.0f);
            Pile->BecomeKind(S.Ember, 0.35f);
            if (Pile->Matter)
            {
                Pile->Matter->Temperature = Heat;
            }
        }
        else
        {
            Pile->Destroy();
        }
        return;
    }
    if (Owner && !Owner->IsA<ACharacter>())
    {
        if (S.Ember != EResourceKind::None)
        {
            const FVector At = Owner->GetActorLocation() + FVector(0.0f, 0.0f, 10.0f);
            AResourceActor::SpawnPiece(GetWorld(), S.Ember, 1.0f, FTransform(At), SizeCm * 0.3f, EMatterShape::Cone);
        }
        Owner->Destroy();
    }
}

bool UMatterComponent::Ignite()
{
    const FSubstance& S = Props();
    if (bBurning || !S.Burns() || Moisture >= FMatter::IgnitionMoistureLimit(S))
    {
        return false;
    }
    bBurning = true;
    Temperature = FMath::Max(Temperature, S.Ignites + 50.0f);
    RefreshLook(true);

    UWorld* World = GetWorld();
    UHumanWorldSubsystem* WorldMind = World ? World->GetSubsystem<UHumanWorldSubsystem>() : nullptr;
    if (WorldMind && GetOwner())
    {
        FWorldEvent Event;
        Event.Tag = TEXT("Fire");
        Event.Description = FString::Printf(TEXT("загорелось: %s"), S.Name);
        Event.Location = GetOwner()->GetActorLocation();
        Event.Radius = 2500.0f;
        Event.Valence = -0.6f;
        Event.Significance = 0.7f;
        WorldMind->BroadcastEvent(Event);
    }
    return true;
}

void UMatterComponent::Extinguish()
{
    if (!bBurning)
    {
        return;
    }
    bBurning = false;
    Temperature = FMath::Min(Temperature, 120.0f);
    RefreshLook(true);
}

float UMatterComponent::Repair(float Fraction)
{
    Damage = FMath::Max(0.0f, Damage - FMath::Max(0.0f, Fraction));
    RefreshLook(true);
    return Damage;
}

void UMatterComponent::CheckShelter(UMatterSubsystem* Matter)
{
    const AActor* Owner = GetOwner();
    if (Matter && Owner)
    {
        bSheltered = Matter->IsSheltered(Owner->GetActorLocation(), Owner);
    }
}

FString UMatterComponent::Describe() const
{
    FString Out = FMatter::Describe(Props(), Moisture, Temperature, Damage, Char, Rot, bBurning);
    Out += FString::Printf(TEXT("; масса %.1f кг; %.0f×%.0f×%.0f см"), TotalMassKg(), SizeCm.X, SizeCm.Y, SizeCm.Z);
    if (bSheltered)
    {
        Out += TEXT("; под крышей");
    }
    if (Ice > 0.05f)
    {
        Out += FString::Printf(TEXT("; лёд в порах %.0f%%"), Ice * 100.0f);
    }
    if (SnowCover > 0.05f)
    {
        Out += FString::Printf(TEXT("; под снегом %.0f%%"), SnowCover * 100.0f);
    }
    return Out;
}

void UMatterComponent::RefreshPhysics(bool bForce)
{
    if (!Body || bVisualOnly)
    {
        return;
    }
    const float Mass = FMath::Max(0.01f, TotalMassKg());
    if (bForce || FMath::Abs(Mass - AppliedMass) > FMath::Max(0.05f, AppliedMass * 0.03f))
    {
        Body->SetMassOverrideInKg(NAME_None, Mass, true);
        AppliedMass = Mass;
    }
    if (UMatterSubsystem* Matter = UMatterSubsystem::Get(this))
    {
        UPhysicalMaterial* Surface = Matter->SurfaceOf(Substance, Moisture, Temperature);
        if (Surface && (bForce || Surface != AppliedSurface))
        {
            Body->SetPhysMaterialOverride(Surface);
            AppliedSurface = Surface;
        }
    }
}

void UMatterComponent::RefreshLook(bool bForce)
{
    const float Wet = WetFraction();
    const float Crack = FMath::Clamp(Damage, 0.0f, 1.0f);
    const float Burnt = FMath::Clamp(Char, 0.0f, 1.0f);
    const float Frost = FMath::Clamp(FMath::Max(SnowCover, Ice * Wet), 0.0f, 1.0f);
    const float Glow = bBurning ? 1.0f : FMath::Clamp((Temperature - 450.0f) / 400.0f, 0.0f, 1.0f);

    const bool bChanged = bForce
        || FMath::Abs(Wet - ShownWet) > 0.03f
        || FMath::Abs(Crack - ShownCrack) > 0.03f
        || FMath::Abs(Burnt - ShownChar) > 0.03f
        || FMath::Abs(Frost - ShownFrost) > 0.03f
        || FMath::Abs(Glow - ShownGlow) > 0.03f;
    if (!bChanged)
    {
        return;
    }

    ShownWet = Wet;
    ShownCrack = Crack;
    ShownChar = Burnt;
    ShownFrost = Frost;
    ShownGlow = Glow;

    for (UPrimitiveComponent* Part : Visuals)
    {
        if (Part)
        {
            ApplyState(Part);
        }
    }
}

void UMatterComponent::ApplyState(UPrimitiveComponent* Part) const
{
    Part->SetCustomPrimitiveDataFloat(0, FMath::Max(0.0f, ShownWet));
    Part->SetCustomPrimitiveDataFloat(1, FMath::Max(0.0f, ShownCrack));
    Part->SetCustomPrimitiveDataFloat(2, FMath::Max(0.0f, ShownChar));
    Part->SetCustomPrimitiveDataFloat(3, FMath::Max(0.0f, ShownFrost));
    Part->SetCustomPrimitiveDataFloat(4, FMath::Max(0.0f, ShownGlow));
}
