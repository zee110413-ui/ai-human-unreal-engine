#pragma once

#include "CoreMinimal.h"
#include "HumanTypes.h"

enum class EMatterForm : uint8
{
    Solid,
    Grains,
    Fibres,
    Paste,
    Liquid,
    Living
};

enum class EMatterPhase : uint8
{
    Dry,
    Damp,
    Plastic,
    Slurry,
    Liquid,
    Frozen,
    Burning,
    Charred,
    Molten,
    Rotten
};

enum class EMatterLook : uint8
{
    Oak,
    Pine,
    Walnut,
    Bark,
    Straw,
    Stone,
    Granite,
    Limestone,
    Sandstone,
    Chalk,
    Flint,
    Clay,
    Soil,
    Sand,
    Peat,
    Coal,
    Ash,
    Brick,
    Pottery,
    Glass,
    Iron,
    Steel,
    Copper,
    Bronze,
    Tin,
    Lead,
    Silver,
    Gold,
    Ore,
    Water,
    Ice,
    Snow,
    Bone,
    Leather,
    Cloth,
    Rope,
    Wax,
    Food,
    Greens,
    Paper,
    Salt,
    Plaster,
    Lawn,
    Gravel,
    Asphalt,
    Cobble,
    OldBrick,
    CutStone,
    Panels,
    Tiles,
    Concrete,
    Slate,
    Moss,
    Count
};

struct AB_API FMatterAir
{
    float Celsius = 12.0f;
    float Humidity = 0.7f;
    float Wind = 2.0f;
    float Rain = 0.0f;
    float Snow = 0.0f;
    float Sun = 0.0f;
    float Radiant = 0.0f;
    bool bInWater = false;
};

struct AB_API FMatterBody
{
    float Moisture = 0.0f;
    float Temperature = 12.0f;
    float Ice = 0.0f;
    float Transition = 0.0f;
    float Damage = 0.0f;
    float Char = 0.0f;
    float Rot = 0.0f;
    float Snow = 0.0f;
    float DryMass = 1.0f;
    float OriginalMass = 1.0f;
    bool bBurning = false;
};

struct AB_API FMatterStep
{
    float Released = 0.0f;
    bool bIgnited = false;
    bool bExtinguished = false;
    bool bBurnedOut = false;
    bool bFired = false;
    bool bTransformed = false;
    bool bSpoiled = false;
    bool bDissolved = false;
};

struct AB_API FSubstance
{
    static constexpr float Never = 100000.0f;

    EResourceKind Kind = EResourceKind::None;
    const TCHAR* Name = TEXT("вещество");
    EMatterForm Form = EMatterForm::Solid;
    EMatterLook Look = EMatterLook::Stone;
    FLinearColor Tint = FLinearColor::White;

    float Density = 1000.0f;
    float Porosity = 0.0f;
    float Young = 1.0f;
    float Compressive = 1.0f;
    float Tensile = 1.0f;
    float Fracture = 10.0f;
    float Split = 0.0f;
    float Cohesion = 0.0f;
    float InternalFriction = 0.0f;
    float Hardness = 1.0f;

    float Friction = 0.5f;
    float Sliding = 0.4f;
    float WetGrip = 1.0f;
    float Bounce = 0.3f;

    float Conductivity = 1.0f;
    float HeatCapacity = 1000.0f;
    float Melts = Never;
    float Boils = Never;
    float Ignites = Never;
    float Heat = 0.0f;
    float FiredAt = Never;

    float PlasticLimit = 0.0f;
    float LiquidLimit = 0.0f;
    float Permeability = 1.0e-9f;
    float Food = 0.0f;

    EResourceKind Ember = EResourceKind::None;
    EResourceKind MeltsInto = EResourceKind::None;
    EResourceKind FiresInto = EResourceKind::None;

    bool bBrittle = false;
    bool bGrain = false;
    bool bRots = false;
    bool bRusts = false;
    bool bDissolves = false;
    bool bSpoils = false;
    bool bMetal = false;

    bool Burns() const { return Ignites < Never; }
    bool IsSoil() const { return LiquidLimit > 0.0f; }
    float SaturatedMoisture() const;
};

struct AB_API FMatter
{
    static constexpr float Gravity = 9.81f;
    static constexpr float WaterDensity = 1000.0f;
    static constexpr float LatentMelt = 334000.0f;
    static constexpr float LatentBoil = 2257000.0f;
    static constexpr float WaterHeatCapacity = 4186.0f;

    static const TArray<FSubstance>& All();
    static const FSubstance& Of(EResourceKind Kind);

    static EMatterPhase PhaseOf(const FSubstance& S, float Moisture, float Celsius, bool bBurning, float Char, float Rot);
    static float StrengthFactor(const FSubstance& S, float Moisture, float Celsius, float Rot, float Char);
    static float FrictionOf(const FSubstance& S, float Moisture, float Celsius, bool bSliding);
    static float BounceOf(const FSubstance& S, float Moisture, float Celsius);
    static float BreakEnergy(const FSubstance& S, float CrossSectionM2, bool bAlongGrain);
    static float YieldPressure(const FSubstance& S, float Moisture, float Celsius);
    static float AirDryMoisture(const FSubstance& S, float Humidity);
    static float IgnitionMoistureLimit(const FSubstance& S);
    static float Compliance(const FSubstance& S, float Moisture, float Celsius);
    static float WaterLimit(const FSubstance& S);
    static float WetFraction(const FSubstance& S, float Moisture);
    static float FlameTemperature(const FSubstance& S, float Moisture);

    static float SaturationVapourPressure(float Celsius);
    static float Evaporation(float SurfaceCelsius, float AirCelsius, float Humidity, float Wind, float SunWatts);
    static float ConvectiveCoefficient(float Wind);

    static FMatterStep Step(const FSubstance& S, FMatterBody& Body, float AreaM2, float TopM2, const FMatterAir& Air, float Seconds);

    static FString PhaseWord(const FSubstance& S, EMatterPhase Phase);
    static FString Describe(const FSubstance& S, float Moisture, float Celsius, float Damage, float Char, float Rot, bool bBurning);
};
