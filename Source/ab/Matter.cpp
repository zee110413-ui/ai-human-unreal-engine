#include "Matter.h"

namespace
{
    constexpr float Never = FSubstance::Never;

    struct FMake
    {
        FSubstance S;

        FMake(EResourceKind Kind, const TCHAR* Name, EMatterForm Form, EMatterLook Look)
        {
            S.Kind = Kind;
            S.Name = Name;
            S.Form = Form;
            S.Look = Look;
        }

        FMake& Mass(float Density, float Porosity)
        {
            S.Density = Density;
            S.Porosity = Porosity;
            return *this;
        }

        FMake& Strength(float YoungGPa, float CompressiveMPa, float TensileMPa)
        {
            S.Young = YoungGPa;
            S.Compressive = CompressiveMPa;
            S.Tensile = TensileMPa;
            return *this;
        }

        FMake& Toughness(float Across, float Along = 0.0f)
        {
            S.Fracture = Across;
            S.Split = Along;
            return *this;
        }

        FMake& Grip(float Static, float Kinetic, float Wet, float Bounce)
        {
            S.Friction = Static;
            S.Sliding = Kinetic;
            S.WetGrip = Wet;
            S.Bounce = Bounce;
            return *this;
        }

        FMake& Warmth(float Conductivity, float Capacity)
        {
            S.Conductivity = Conductivity;
            S.HeatCapacity = Capacity;
            return *this;
        }

        FMake& Hard(float Mohs)
        {
            S.Hardness = Mohs;
            return *this;
        }

        FMake& Melt(float At, EResourceKind Into = EResourceKind::None, float Boil = Never)
        {
            S.Melts = At;
            S.MeltsInto = Into;
            S.Boils = Boil;
            return *this;
        }

        FMake& Burn(float At, float MegaJoules, EResourceKind Into = EResourceKind::Ash)
        {
            S.Ignites = At;
            S.Heat = MegaJoules;
            S.Ember = Into;
            return *this;
        }

        FMake& Fire(float At, EResourceKind Into)
        {
            S.FiredAt = At;
            S.FiresInto = Into;
            return *this;
        }

        FMake& Earth(float Plastic, float Liquid, float Permeability, float CohesionKPa, float Angle)
        {
            S.PlasticLimit = Plastic;
            S.LiquidLimit = Liquid;
            S.Permeability = Permeability;
            S.Cohesion = CohesionKPa;
            S.InternalFriction = Angle;
            return *this;
        }

        FMake& Loose(float Angle, float Permeability)
        {
            S.InternalFriction = Angle;
            S.Permeability = Permeability;
            return *this;
        }

        FMake& Edible(float Kcal, bool bSpoils)
        {
            S.Food = Kcal;
            S.bSpoils = bSpoils;
            return *this;
        }

        FMake& Colour(float R, float G, float B)
        {
            S.Tint = FLinearColor(R, G, B);
            return *this;
        }

        FMake& Brittle() { S.bBrittle = true; return *this; }
        FMake& Grain() { S.bGrain = true; return *this; }
        FMake& Rots() { S.bRots = true; return *this; }
        FMake& Rusts() { S.bRusts = true; return *this; }
        FMake& Dissolves() { S.bDissolves = true; return *this; }
        FMake& Metal() { S.bMetal = true; return *this; }
    };

    void AddWoods(TArray<FSubstance>& T)
    {
        using K = EResourceKind;
        using F = EMatterForm;
        using L = EMatterLook;

        T.Add(FMake(K::Wood, TEXT("бревно"), F::Solid, L::Oak)
            .Mass(520.0f, 0.65f).Strength(10.0f, 45.0f, 80.0f).Toughness(12000.0f, 250.0f)
            .Grip(0.5f, 0.38f, 0.75f, 0.45f).Warmth(0.12f, 1700.0f).Hard(2.0f)
            .Burn(300.0f, 18.5f).Grain().Rots().S);

        T.Add(FMake(K::Plank, TEXT("доска"), F::Solid, L::Pine)
            .Mass(480.0f, 0.65f).Strength(10.0f, 40.0f, 75.0f).Toughness(9000.0f, 200.0f)
            .Grip(0.45f, 0.35f, 0.75f, 0.45f).Warmth(0.12f, 1700.0f).Hard(2.0f)
            .Burn(290.0f, 18.5f).Grain().Rots().S);

        T.Add(FMake(K::Firewood, TEXT("дрова"), F::Solid, L::Walnut)
            .Mass(520.0f, 0.65f).Strength(10.0f, 45.0f, 80.0f).Toughness(9000.0f, 200.0f)
            .Grip(0.5f, 0.38f, 0.75f, 0.4f).Warmth(0.12f, 1700.0f).Hard(2.0f)
            .Burn(290.0f, 18.5f).Grain().Rots().S);

        T.Add(FMake(K::Bark, TEXT("кора"), F::Fibres, L::Bark)
            .Mass(450.0f, 0.6f).Strength(1.0f, 5.0f, 4.0f).Toughness(800.0f, 300.0f)
            .Grip(0.6f, 0.45f, 0.8f, 0.2f).Warmth(0.09f, 1500.0f).Hard(1.0f)
            .Burn(260.0f, 19.0f).Grain().Rots().S);

        T.Add(FMake(K::Straw, TEXT("солома"), F::Fibres, L::Straw)
            .Mass(110.0f, 0.9f).Strength(0.005f, 0.02f, 0.3f).Toughness(600.0f)
            .Grip(0.45f, 0.35f, 0.9f, 0.1f).Warmth(0.06f, 1600.0f).Hard(0.5f)
            .Burn(240.0f, 14.5f).Rots().S);

        T.Add(FMake(K::Reed, TEXT("камыш"), F::Fibres, L::Straw)
            .Mass(200.0f, 0.85f).Strength(0.02f, 0.05f, 1.0f).Toughness(800.0f)
            .Grip(0.45f, 0.35f, 0.9f, 0.15f).Warmth(0.07f, 1600.0f).Hard(0.5f)
            .Burn(260.0f, 16.0f).Colour(0.85f, 0.9f, 0.7f).Rots().S);


        T.Add(FMake(K::BirchWood, TEXT("берёза"), F::Solid, L::Chalk)
            .Mass(510.0f, 0.6f).Strength(8.0f, 38.0f, 70.0f).Toughness(11000.0f, 220.0f)
            .Grip(0.5f, 0.38f, 0.75f, 0.45f).Warmth(0.11f, 1650.0f).Hard(1.8f)
            .Burn(290.0f, 18.0f).Colour(0.92f, 0.9f, 0.85f).Grain().Rots().S);

        T.Add(FMake(K::PineWood, TEXT("сосна"), F::Solid, L::Pine)
            .Mass(500.0f, 0.62f).Strength(9.0f, 40.0f, 75.0f).Toughness(11500.0f, 230.0f)
            .Grip(0.5f, 0.38f, 0.75f, 0.45f).Warmth(0.12f, 1680.0f).Hard(1.9f)
            .Burn(295.0f, 18.8f).Colour(0.85f, 0.65f, 0.4f).Grain().Rots().S);

        T.Add(FMake(K::BirchBark, TEXT("береста"), F::Fibres, L::Bark)
            .Mass(300.0f, 0.7f).Strength(0.5f, 1.0f, 20.0f).Toughness(3000.0f, 400.0f)
            .Grip(0.5f, 0.4f, 0.8f, 0.1f).Warmth(0.08f, 1400.0f).Hard(1.0f)
            .Burn(250.0f, 18.0f).Colour(0.95f, 0.92f, 0.82f).Rots().S);

        T.Add(FMake(K::Stump, TEXT("пень"), F::Solid, L::Oak)
            .Mass(600.0f, 0.5f).Strength(8.0f, 40.0f, 60.0f).Toughness(9000.0f, 200.0f)
            .Grip(0.5f, 0.4f, 0.7f, 0.3f).Warmth(0.12f, 1700.0f).Hard(2.0f)
            .Burn(300.0f, 17.0f).Colour(0.6f, 0.45f, 0.3f).Grain().Rots().S);

        T.Add(FMake(K::Brushwood, TEXT("хворост"), F::Fibres, L::Bark)
            .Mass(250.0f, 0.8f).Strength(0.1f, 1.0f, 5.0f).Toughness(200.0f)
            .Grip(0.5f, 0.4f, 0.8f, 0.1f).Warmth(0.08f, 1600.0f).Hard(1.0f)
            .Burn(240.0f, 15.0f).Colour(0.6f, 0.5f, 0.35f).Rots().S);

        T.Add(FMake(K::Vine, TEXT("лоза"), F::Fibres, L::Bark)
            .Mass(400.0f, 0.7f).Strength(0.2f, 1.0f, 15.0f).Toughness(4000.0f, 500.0f)
            .Grip(0.5f, 0.4f, 0.8f, 0.15f).Warmth(0.1f, 1500.0f).Hard(1.0f)
            .Burn(280.0f, 16.0f).Colour(0.6f, 0.5f, 0.35f).Rots().S);

        T.Add(FMake(K::Cone, TEXT("шишки"), F::Grains, L::Bark)
            .Mass(400.0f, 0.6f).Strength(0.5f, 2.0f, 1.0f).Toughness(50.0f)
            .Grip(0.5f, 0.4f, 0.8f, 0.2f).Warmth(0.1f, 1600.0f).Hard(1.5f)
            .Loose(30.0f, 1.0e-5f).Burn(300.0f, 16.0f).Colour(0.5f, 0.35f, 0.2f).S);

        T.Add(FMake(K::Acorn, TEXT("желуди"), F::Grains, L::Food)
            .Mass(800.0f, 0.3f).Strength(1.0f, 3.0f, 2.0f).Toughness(5.0f)
            .Grip(0.5f, 0.4f, 0.8f, 0.2f).Warmth(0.15f, 1700.0f).Hard(2.0f)
            .Loose(28.0f, 1.0e-5f).Burn(380.0f, 17.0f).Colour(0.55f, 0.4f, 0.2f).Edible(2500.0f, true).S);

        T.Add(FMake(K::Rod, TEXT("удочка"), F::Solid, L::Pine)
            .Mass(500.0f, 0.6f).Strength(10.0f, 40.0f, 90.0f).Toughness(12000.0f, 250.0f)
            .Grip(0.45f, 0.35f, 0.75f, 0.4f).Warmth(0.12f, 1700.0f).Hard(2.0f)
            .Burn(300.0f, 18.5f).Grain().Rots().S);
    }

    void AddStones(TArray<FSubstance>& T)
    {
        using K = EResourceKind;
        using F = EMatterForm;
        using L = EMatterLook;

        T.Add(FMake(K::Stone, TEXT("камень"), F::Solid, L::Stone)
            .Mass(2650.0f, 0.01f).Strength(50.0f, 150.0f, 10.0f).Toughness(60.0f)
            .Grip(0.65f, 0.5f, 0.85f, 0.55f).Warmth(2.8f, 790.0f).Hard(6.0f)
            .Melt(1250.0f).Brittle().S);

        T.Add(FMake(K::Granite, TEXT("гранит"), F::Solid, L::Granite)
            .Mass(2700.0f, 0.01f).Strength(55.0f, 180.0f, 12.0f).Toughness(70.0f)
            .Grip(0.6f, 0.5f, 0.85f, 0.55f).Warmth(3.0f, 790.0f).Hard(6.5f)
            .Melt(1250.0f).Brittle().S);

        T.Add(FMake(K::Limestone, TEXT("известняк"), F::Solid, L::Limestone)
            .Mass(2550.0f, 0.12f).Strength(35.0f, 60.0f, 5.0f).Toughness(25.0f)
            .Grip(0.6f, 0.45f, 0.8f, 0.4f).Warmth(1.3f, 910.0f).Hard(3.0f)
            .Fire(900.0f, K::Lime).Brittle().S);

        T.Add(FMake(K::Sandstone, TEXT("песчаник"), F::Solid, L::Sandstone)
            .Mass(2300.0f, 0.18f).Strength(15.0f, 55.0f, 3.5f).Toughness(30.0f)
            .Grip(0.7f, 0.55f, 0.85f, 0.4f).Warmth(2.3f, 900.0f).Hard(6.0f)
            .Melt(1650.0f).Brittle().S);

        T.Add(FMake(K::Chalk, TEXT("мел"), F::Solid, L::Chalk)
            .Mass(1900.0f, 0.4f).Strength(3.0f, 8.0f, 0.8f).Toughness(8.0f)
            .Grip(0.5f, 0.4f, 0.6f, 0.25f).Warmth(0.9f, 900.0f).Hard(1.5f)
            .Fire(900.0f, K::Lime).Brittle().S);

        T.Add(FMake(K::Flint, TEXT("кремень"), F::Solid, L::Flint)
            .Mass(2600.0f, 0.005f).Strength(70.0f, 400.0f, 45.0f).Toughness(25.0f)
            .Grip(0.5f, 0.4f, 0.9f, 0.6f).Warmth(2.0f, 740.0f).Hard(7.0f)
            .Melt(1700.0f).Brittle().S);

        T.Add(FMake(K::Quartz, TEXT("кварц"), F::Solid, L::Salt)
            .Mass(2650.0f, 0.0f).Strength(72.0f, 1100.0f, 48.0f).Toughness(9.0f)
            .Grip(0.5f, 0.4f, 0.9f, 0.6f).Warmth(6.5f, 740.0f).Hard(7.0f)
            .Melt(1670.0f, K::Glass).Colour(0.95f, 0.95f, 1.0f).Brittle().S);

        T.Add(FMake(K::Coal, TEXT("каменный уголь"), F::Solid, L::Coal)
            .Mass(1350.0f, 0.08f).Strength(4.0f, 20.0f, 1.5f).Toughness(25.0f)
            .Grip(0.55f, 0.45f, 0.9f, 0.3f).Warmth(0.26f, 1320.0f).Hard(2.5f)
            .Burn(450.0f, 27.0f).Brittle().S);

        T.Add(FMake(K::Charcoal, TEXT("древесный уголь"), F::Solid, L::Coal)
            .Mass(350.0f, 0.8f).Strength(0.5f, 3.0f, 0.5f).Toughness(8.0f)
            .Grip(0.5f, 0.4f, 0.9f, 0.2f).Warmth(0.09f, 1000.0f).Hard(1.5f)
            .Burn(350.0f, 29.5f).Brittle().S);

        T.Add(FMake(K::Salt, TEXT("соль"), F::Solid, L::Salt)
            .Mass(2170.0f, 0.02f).Strength(30.0f, 20.0f, 1.5f).Toughness(1.5f)
            .Grip(0.6f, 0.5f, 0.4f, 0.3f).Warmth(6.0f, 880.0f).Hard(2.5f)
            .Melt(801.0f).Dissolves().Brittle().S);

        T.Add(FMake(K::Sulphur, TEXT("сера"), F::Solid, L::Salt)
            .Mass(2070.0f, 0.02f).Strength(14.0f, 20.0f, 1.0f).Toughness(2.0f)
            .Grip(0.5f, 0.4f, 0.9f, 0.3f).Warmth(0.27f, 710.0f).Hard(2.0f)
            .Melt(115.0f).Burn(232.0f, 9.3f, K::None).Colour(1.0f, 0.9f, 0.25f).Brittle().S);

        T.Add(FMake(K::Saltpetre, TEXT("селитра"), F::Solid, L::Salt)
            .Mass(2110.0f, 0.02f).Strength(20.0f, 10.0f, 1.0f).Toughness(2.0f)
            .Grip(0.6f, 0.5f, 0.5f, 0.3f).Warmth(0.5f, 950.0f).Hard(2.0f)
            .Melt(334.0f).Dissolves().Brittle().S);

        T.Add(FMake(K::Lime, TEXT("известь"), F::Grains, L::Plaster)
            .Mass(1000.0f, 0.5f).Strength(1.0f, 5.0f, 0.5f).Toughness(5.0f)
            .Grip(0.6f, 0.5f, 0.6f, 0.1f).Warmth(0.5f, 750.0f).Hard(3.5f)
            .Melt(2572.0f).Loose(40.0f, 1.0e-6f).Brittle().S);

        T.Add(FMake(K::Ash, TEXT("зола"), F::Grains, L::Ash)
            .Mass(650.0f, 0.7f).Strength(0.001f, 0.001f, 0.0001f).Toughness(0.1f)
            .Grip(0.6f, 0.5f, 0.6f, 0.0f).Warmth(0.1f, 900.0f).Hard(1.0f)
            .Melt(1200.0f).Loose(40.0f, 1.0e-6f).S);
    }


    void AddStonesMore(TArray<FSubstance>& T)
    {
        using K = EResourceKind;
        using F = EMatterForm;
        using L = EMatterLook;

        T.Add(FMake(K::Marble, TEXT("мрамор"), F::Solid, L::CutStone)
            .Mass(2700.0f, 0.005f).Strength(50.0f, 120.0f, 8.0f).Toughness(50.0f)
            .Grip(0.6f, 0.5f, 0.85f, 0.5f).Warmth(2.5f, 880.0f).Hard(3.5f)
            .Colour(0.92f, 0.92f, 0.9f).Brittle().S);

        T.Add(FMake(K::Basalt, TEXT("базальт"), F::Solid, L::Cobble)
            .Mass(2900.0f, 0.02f).Strength(70.0f, 200.0f, 15.0f).Toughness(80.0f)
            .Grip(0.65f, 0.55f, 0.85f, 0.5f).Warmth(1.8f, 840.0f).Hard(6.0f)
            .Colour(0.25f, 0.26f, 0.28f).Brittle().S);

        T.Add(FMake(K::Slate, TEXT("сланец"), F::Solid, L::Slate)
            .Mass(2800.0f, 0.02f).Strength(40.0f, 100.0f, 8.0f).Toughness(20.0f, 500.0f)
            .Grip(0.6f, 0.5f, 0.85f, 0.5f).Warmth(2.0f, 860.0f).Hard(3.5f)
            .Colour(0.35f, 0.38f, 0.42f).Brittle().S);

        T.Add(FMake(K::Obsidian, TEXT("обсидиан"), F::Solid, L::Glass)
            .Mass(2400.0f, 0.0f).Strength(70.0f, 80.0f, 12.0f).Toughness(10.0f)
            .Grip(0.5f, 0.4f, 0.7f, 0.4f).Warmth(1.4f, 800.0f).Hard(5.5f)
            .Melt(1200.0f, K::Glass).Colour(0.08f, 0.08f, 0.1f).Brittle().S);

        T.Add(FMake(K::Mica, TEXT("слюда"), F::Solid, L::Glass)
            .Mass(2800.0f, 0.0f).Strength(20.0f, 60.0f, 5.0f).Toughness(5.0f, 800.0f)
            .Grip(0.5f, 0.4f, 0.7f, 0.2f).Warmth(0.5f, 850.0f).Hard(2.5f)
            .Colour(0.9f, 0.88f, 0.8f).Brittle().S);

        T.Add(FMake(K::Graphite, TEXT("графит"), F::Solid, L::Coal)
            .Mass(2200.0f, 0.05f).Strength(15.0f, 65.0f, 20.0f).Toughness(20.0f)
            .Grip(0.4f, 0.2f, 0.5f, 0.2f).Warmth(120.0f, 710.0f).Hard(1.5f)
            .Colour(0.2f, 0.2f, 0.22f).Brittle().S);

        T.Add(FMake(K::Gravel, TEXT("гравий"), F::Grains, L::Gravel)
            .Mass(1700.0f, 0.35f).Strength(0.05f, 0.01f, 0.001f).Toughness(1.0f)
            .Grip(0.7f, 0.6f, 0.9f, 0.2f).Warmth(0.8f, 850.0f).Hard(6.0f)
            .Loose(35.0f, 1.0e-3f).Colour(0.6f, 0.58f, 0.55f).S);

        T.Add(FMake(K::Mud, TEXT("грязь"), F::Paste, L::Soil)
            .Mass(1500.0f, 0.55f).Strength(0.005f, 0.02f, 0.005f).Toughness(2.0f)
            .Grip(0.5f, 0.3f, 0.3f, 0.0f).Warmth(1.0f, 1500.0f).Hard(0.5f)
            .Earth(0.35f, 0.6f, 1.0e-8f, 8.0f, 18.0f).Colour(0.35f, 0.28f, 0.2f).S);

        T.Add(FMake(K::Compost, TEXT("перегной"), F::Grains, L::Soil)
            .Mass(900.0f, 0.6f).Strength(0.005f, 0.01f, 0.005f).Toughness(1.0f)
            .Grip(0.5f, 0.4f, 0.5f, 0.02f).Warmth(0.6f, 1400.0f).Hard(0.5f)
            .Earth(0.3f, 0.55f, 1.0e-6f, 6.0f, 22.0f).Burn(260.0f, 8.0f)
            .Colour(0.25f, 0.2f, 0.15f).Rots().S);

        T.Add(FMake(K::ZincOre, TEXT("цинковая руда"), F::Solid, L::Ore)
            .Mass(4100.0f, 0.03f).Strength(60.0f, 90.0f, 8.0f).Toughness(25.0f)
            .Grip(0.6f, 0.5f, 0.85f, 0.5f).Warmth(2.5f, 500.0f).Hard(3.5f)
            .Colour(0.75f, 0.7f, 0.55f).Brittle().S);

        T.Add(FMake(K::Gem, TEXT("самоцвет"), F::Solid, L::Glass)
            .Mass(3200.0f, 0.0f).Strength(60.0f, 110.0f, 20.0f).Toughness(30.0f)
            .Grip(0.4f, 0.3f, 0.6f, 0.4f).Warmth(2.0f, 700.0f).Hard(7.0f)
            .Colour(0.4f, 0.8f, 0.9f).Brittle().S);

        T.Add(FMake(K::Diamond, TEXT("алмаз"), F::Solid, L::Glass)
            .Mass(3510.0f, 0.0f).Strength(100.0f, 150.0f, 60.0f).Toughness(80.0f)
            .Grip(0.4f, 0.3f, 0.6f, 0.4f).Warmth(1000.0f, 520.0f).Hard(10.0f)
            .Burn(850.0f, 33.0f, K::None).Colour(0.95f, 0.98f, 1.0f).Brittle().S);

        T.Add(FMake(K::Ruby, TEXT("рубин"), F::Solid, L::Glass)
            .Mass(3980.0f, 0.0f).Strength(60.0f, 130.0f, 30.0f).Toughness(40.0f)
            .Grip(0.4f, 0.3f, 0.6f, 0.4f).Warmth(40.0f, 760.0f).Hard(9.0f)
            .Colour(0.85f, 0.1f, 0.15f).Brittle().S);

        T.Add(FMake(K::Emerald, TEXT("изумруд"), F::Solid, L::Glass)
            .Mass(2700.0f, 0.0f).Strength(50.0f, 90.0f, 15.0f).Toughness(25.0f)
            .Grip(0.4f, 0.3f, 0.6f, 0.4f).Warmth(3.0f, 800.0f).Hard(7.5f)
            .Colour(0.15f, 0.75f, 0.35f).Brittle().S);

        T.Add(FMake(K::Sapphire, TEXT("сапфир"), F::Solid, L::Glass)
            .Mass(3980.0f, 0.0f).Strength(60.0f, 130.0f, 30.0f).Toughness(40.0f)
            .Grip(0.4f, 0.3f, 0.6f, 0.4f).Warmth(40.0f, 760.0f).Hard(9.0f)
            .Colour(0.15f, 0.3f, 0.85f).Brittle().S);

        T.Add(FMake(K::Pearl, TEXT("жемчуг"), F::Solid, L::Glass)
            .Mass(2700.0f, 0.0f).Strength(30.0f, 50.0f, 10.0f).Toughness(30.0f)
            .Grip(0.4f, 0.3f, 0.6f, 0.3f).Warmth(1.5f, 900.0f).Hard(3.0f)
            .Colour(0.95f, 0.93f, 0.88f).Brittle().S);

        T.Add(FMake(K::CharcoalDust, TEXT("угольная пыль"), F::Grains, L::Coal)
            .Mass(500.0f, 0.5f).Strength(0.001f, 0.001f, 0.0f).Toughness(0.1f)
            .Grip(0.4f, 0.3f, 0.4f, 0.0f).Warmth(1.0f, 800.0f).Hard(1.0f)
            .Burn(400.0f, 28.0f).Loose(30.0f, 1.0e-6f).Colour(0.1f, 0.1f, 0.1f).S);

        T.Add(FMake(K::Zinc, TEXT("цинк"), F::Solid, L::Steel)
            .Mass(7130.0f, 0.0f).Strength(90.0f, 80.0f, 110.0f).Toughness(20000.0f)
            .Grip(0.5f, 0.4f, 0.9f, 0.3f).Warmth(116.0f, 390.0f).Hard(2.5f)
            .Melt(419.0f, K::None, 907.0f).Colour(0.75f, 0.78f, 0.82f).Metal().S);

        T.Add(FMake(K::Brass, TEXT("латунь"), F::Solid, L::Bronze)
            .Mass(8500.0f, 0.0f).Strength(100.0f, 200.0f, 300.0f).Toughness(40000.0f)
            .Grip(0.5f, 0.4f, 0.9f, 0.4f).Warmth(110.0f, 380.0f).Hard(3.0f)
            .Melt(930.0f).Colour(0.85f, 0.7f, 0.3f).Metal().S);

        T.Add(FMake(K::Electrum, TEXT("электрум"), F::Solid, L::Gold)
            .Mass(15000.0f, 0.0f).Strength(80.0f, 100.0f, 130.0f).Toughness(50000.0f)
            .Grip(0.5f, 0.4f, 0.9f, 0.4f).Warmth(250.0f, 180.0f).Hard(2.5f)
            .Melt(1000.0f).Colour(0.9f, 0.85f, 0.6f).Metal().S);
    }

    void AddEarths(TArray<FSubstance>& T)
    {
        using K = EResourceKind;
        using F = EMatterForm;
        using L = EMatterLook;

        T.Add(FMake(K::Soil, TEXT("земля"), F::Grains, L::Soil)
            .Mass(1300.0f, 0.5f).Strength(0.02f, 0.2f, 0.02f).Toughness(3.0f)
            .Grip(0.6f, 0.5f, 0.55f, 0.02f).Warmth(0.9f, 1100.0f).Hard(1.0f)
            .Earth(0.20f, 0.34f, 1.0e-6f, 12.0f, 28.0f).S);

        T.Add(FMake(K::Clay, TEXT("глина"), F::Paste, L::Clay)
            .Mass(1700.0f, 0.4f).Strength(0.1f, 3.0f, 0.4f).Toughness(15.0f)
            .Grip(0.6f, 0.45f, 0.35f, 0.05f).Warmth(1.3f, 900.0f).Hard(2.0f)
            .Earth(0.24f, 0.48f, 1.0e-9f, 60.0f, 20.0f).Fire(950.0f, K::Brick).S);

        T.Add(FMake(K::Sand, TEXT("песок"), F::Grains, L::Sand)
            .Mass(1600.0f, 0.38f).Strength(0.05f, 0.01f, 0.001f).Toughness(0.5f)
            .Grip(0.6f, 0.5f, 0.9f, 0.02f).Warmth(0.3f, 830.0f).Hard(7.0f)
            .Melt(1700.0f, K::Glass).Loose(33.0f, 1.0e-4f).S);

        T.Add(FMake(K::Peat, TEXT("торф"), F::Grains, L::Peat)
            .Mass(250.0f, 0.9f).Strength(0.001f, 0.02f, 0.01f).Toughness(2.0f)
            .Grip(0.5f, 0.4f, 0.6f, 0.02f).Warmth(0.07f, 1900.0f).Hard(1.0f)
            .Earth(2.0f, 4.0f, 1.0e-6f, 10.0f, 25.0f).Burn(250.0f, 15.0f).S);

        T.Add(FMake(K::Snow, TEXT("снег"), F::Grains, L::Snow)
            .Mass(300.0f, 0.7f).Strength(0.001f, 0.02f, 0.005f).Toughness(0.5f)
            .Grip(0.2f, 0.05f, 0.6f, 0.02f).Warmth(0.15f, 2090.0f).Hard(1.0f)
            .Melt(0.0f, K::Water).S);

        T.Add(FMake(K::Ice, TEXT("лёд"), F::Solid, L::Ice)
            .Mass(917.0f, 0.0f).Strength(9.0f, 5.0f, 1.0f).Toughness(1.0f)
            .Grip(0.1f, 0.03f, 0.5f, 0.5f).Warmth(2.2f, 2090.0f).Hard(1.5f)
            .Melt(0.0f, K::Water).Brittle().S);

        T.Add(FMake(K::Water, TEXT("вода"), F::Liquid, L::Water)
            .Mass(1000.0f, 0.0f).Strength(2.2f, 0.0f, 0.0f).Toughness(0.0f)
            .Grip(0.05f, 0.03f, 1.0f, 0.0f).Warmth(0.6f, 4186.0f).Hard(0.0f)
            .Melt(0.0f, K::Ice, 100.0f).S);

        T.Add(FMake(K::IronOre, TEXT("железная руда"), F::Solid, L::Ore)
            .Mass(5000.0f, 0.05f).Strength(150.0f, 150.0f, 10.0f).Toughness(40.0f)
            .Grip(0.6f, 0.5f, 0.85f, 0.5f).Warmth(5.0f, 650.0f).Hard(5.5f)
            .Colour(0.75f, 0.45f, 0.3f).Brittle().S);

        T.Add(FMake(K::CopperOre, TEXT("медная руда"), F::Solid, L::Ore)
            .Mass(4200.0f, 0.03f).Strength(70.0f, 100.0f, 8.0f).Toughness(30.0f)
            .Grip(0.6f, 0.5f, 0.85f, 0.5f).Warmth(3.0f, 540.0f).Hard(3.5f)
            .Colour(0.55f, 0.75f, 0.6f).Brittle().S);

        T.Add(FMake(K::TinOre, TEXT("оловянная руда"), F::Solid, L::Ore)
            .Mass(6900.0f, 0.02f).Strength(200.0f, 200.0f, 12.0f).Toughness(30.0f)
            .Grip(0.6f, 0.5f, 0.85f, 0.5f).Warmth(4.0f, 350.0f).Hard(6.5f)
            .Colour(0.45f, 0.4f, 0.38f).Brittle().S);

        T.Add(FMake(K::LeadOre, TEXT("свинцовая руда"), F::Solid, L::Ore)
            .Mass(7500.0f, 0.02f).Strength(80.0f, 50.0f, 5.0f).Toughness(5.0f)
            .Grip(0.6f, 0.5f, 0.85f, 0.4f).Warmth(2.3f, 210.0f).Hard(2.5f)
            .Melt(1114.0f).Colour(0.6f, 0.62f, 0.66f).Brittle().S);

        T.Add(FMake(K::SilverOre, TEXT("серебряная руда"), F::Solid, L::Ore)
            .Mass(7200.0f, 0.02f).Strength(60.0f, 40.0f, 4.0f).Toughness(5.0f)
            .Grip(0.6f, 0.5f, 0.85f, 0.4f).Warmth(2.0f, 300.0f).Hard(2.5f)
            .Colour(0.7f, 0.72f, 0.75f).Brittle().S);

        T.Add(FMake(K::GoldOre, TEXT("золотая руда"), F::Solid, L::Ore)
            .Mass(3000.0f, 0.01f).Strength(70.0f, 200.0f, 12.0f).Toughness(12.0f)
            .Grip(0.5f, 0.4f, 0.9f, 0.55f).Warmth(6.0f, 700.0f).Hard(6.5f)
            .Colour(0.95f, 0.85f, 0.55f).Brittle().S);
    }

    void AddMetals(TArray<FSubstance>& T)
    {
        using K = EResourceKind;
        using F = EMatterForm;
        using L = EMatterLook;

        T.Add(FMake(K::Iron, TEXT("железо"), F::Solid, L::Iron)
            .Mass(7700.0f, 0.0f).Strength(200.0f, 250.0f, 350.0f).Toughness(80000.0f)
            .Grip(0.6f, 0.45f, 0.9f, 0.55f).Warmth(60.0f, 450.0f).Hard(4.5f)
            .Melt(1538.0f, K::None, 2862.0f).Rusts().Metal().S);

        T.Add(FMake(K::Steel, TEXT("сталь"), F::Solid, L::Steel)
            .Mass(7850.0f, 0.0f).Strength(205.0f, 450.0f, 550.0f).Toughness(50000.0f)
            .Grip(0.6f, 0.42f, 0.9f, 0.6f).Warmth(45.0f, 490.0f).Hard(5.5f)
            .Melt(1450.0f, K::None, 2862.0f).Rusts().Metal().S);

        T.Add(FMake(K::Copper, TEXT("медь"), F::Solid, L::Copper)
            .Mass(8960.0f, 0.0f).Strength(117.0f, 70.0f, 220.0f).Toughness(100000.0f)
            .Grip(0.6f, 0.45f, 0.9f, 0.5f).Warmth(400.0f, 385.0f).Hard(3.0f)
            .Melt(1085.0f, K::None, 2562.0f).Metal().S);

        T.Add(FMake(K::Bronze, TEXT("бронза"), F::Solid, L::Bronze)
            .Mass(8800.0f, 0.0f).Strength(110.0f, 250.0f, 350.0f).Toughness(30000.0f)
            .Grip(0.5f, 0.35f, 0.9f, 0.5f).Warmth(50.0f, 380.0f).Hard(3.5f)
            .Melt(950.0f).Metal().S);

        T.Add(FMake(K::Tin, TEXT("олово"), F::Solid, L::Tin)
            .Mass(7290.0f, 0.0f).Strength(50.0f, 10.0f, 15.0f).Toughness(40000.0f)
            .Grip(0.6f, 0.5f, 0.9f, 0.3f).Warmth(67.0f, 228.0f).Hard(1.5f)
            .Melt(232.0f, K::None, 2602.0f).Metal().S);

        T.Add(FMake(K::Lead, TEXT("свинец"), F::Solid, L::Lead)
            .Mass(11340.0f, 0.0f).Strength(16.0f, 12.0f, 18.0f).Toughness(30000.0f)
            .Grip(0.7f, 0.5f, 0.9f, 0.1f).Warmth(35.0f, 130.0f).Hard(1.5f)
            .Melt(327.0f, K::None, 1749.0f).Metal().S);

        T.Add(FMake(K::Silver, TEXT("серебро"), F::Solid, L::Silver)
            .Mass(10490.0f, 0.0f).Strength(83.0f, 100.0f, 140.0f).Toughness(60000.0f)
            .Grip(0.5f, 0.4f, 0.9f, 0.5f).Warmth(429.0f, 235.0f).Hard(2.5f)
            .Melt(962.0f, K::None, 2162.0f).Metal().S);

        T.Add(FMake(K::Gold, TEXT("золото"), F::Solid, L::Gold)
            .Mass(19320.0f, 0.0f).Strength(79.0f, 80.0f, 120.0f).Toughness(60000.0f)
            .Grip(0.5f, 0.4f, 0.9f, 0.4f).Warmth(318.0f, 129.0f).Hard(2.5f)
            .Melt(1064.0f, K::None, 2856.0f).Metal().S);

        T.Add(FMake(K::Coin, TEXT("монета"), F::Solid, L::Copper)
            .Mass(8960.0f, 0.0f).Strength(117.0f, 70.0f, 220.0f).Toughness(100000.0f)
            .Grip(0.6f, 0.45f, 0.9f, 0.5f).Warmth(400.0f, 385.0f).Hard(3.0f)
            .Melt(1085.0f).Metal().S);

        T.Add(FMake(K::Axe, TEXT("топор"), F::Solid, L::Steel)
            .Mass(7850.0f, 0.0f).Strength(205.0f, 450.0f, 550.0f).Toughness(50000.0f)
            .Grip(0.6f, 0.42f, 0.9f, 0.6f).Warmth(45.0f, 490.0f).Hard(5.5f)
            .Melt(1450.0f).Rusts().Metal().S);

        T.Add(FMake(K::Shovel, TEXT("лопата"), F::Solid, L::Iron)
            .Mass(7850.0f, 0.0f).Strength(205.0f, 450.0f, 550.0f).Toughness(50000.0f)
            .Grip(0.6f, 0.42f, 0.9f, 0.6f).Warmth(45.0f, 490.0f).Hard(5.0f)
            .Melt(1450.0f).Rusts().Metal().S);

        T.Add(FMake(K::Saw, TEXT("пила"), F::Solid, L::Steel)
            .Mass(7850.0f, 0.0f).Strength(205.0f, 450.0f, 700.0f).Toughness(30000.0f)
            .Grip(0.6f, 0.42f, 0.9f, 0.6f).Warmth(45.0f, 490.0f).Hard(6.0f)
            .Melt(1450.0f).Rusts().Metal().S);

        T.Add(FMake(K::Hammer, TEXT("молот"), F::Solid, L::Iron)
            .Mass(7850.0f, 0.0f).Strength(205.0f, 450.0f, 550.0f).Toughness(50000.0f)
            .Grip(0.6f, 0.42f, 0.9f, 0.6f).Warmth(45.0f, 490.0f).Hard(5.5f)
            .Melt(1450.0f).Rusts().Metal().S);

        T.Add(FMake(K::Needle, TEXT("игла"), F::Solid, L::Steel)
            .Mass(7850.0f, 0.0f).Strength(205.0f, 450.0f, 900.0f).Toughness(20000.0f)
            .Grip(0.5f, 0.4f, 0.9f, 0.6f).Warmth(45.0f, 490.0f).Hard(6.5f)
            .Melt(1450.0f).Rusts().Metal().S);

        T.Add(FMake(K::Tool, TEXT("гвоздь"), F::Solid, L::Iron)
            .Mass(7700.0f, 0.0f).Strength(200.0f, 250.0f, 350.0f).Toughness(80000.0f)
            .Grip(0.6f, 0.45f, 0.9f, 0.55f).Warmth(60.0f, 450.0f).Hard(4.5f)
            .Melt(1538.0f).Rusts().Metal().S);
    }

    void AddMade(TArray<FSubstance>& T)
    {
        using K = EResourceKind;
        using F = EMatterForm;
        using L = EMatterLook;

        T.Add(FMake(K::Brick, TEXT("кирпич"), F::Solid, L::Brick)
            .Mass(1800.0f, 0.25f).Strength(12.0f, 20.0f, 2.0f).Toughness(20.0f)
            .Grip(0.7f, 0.55f, 0.85f, 0.3f).Warmth(0.7f, 840.0f).Hard(5.5f)
            .Melt(1500.0f).Brittle().S);

        T.Add(FMake(K::Pot, TEXT("горшок"), F::Solid, L::Pottery)
            .Mass(1900.0f, 0.15f).Strength(20.0f, 30.0f, 3.0f).Toughness(12.0f)
            .Grip(0.6f, 0.5f, 0.85f, 0.3f).Warmth(0.8f, 850.0f).Hard(5.0f)
            .Melt(1500.0f).Brittle().S);

        T.Add(FMake(K::Glass, TEXT("стекло"), F::Solid, L::Glass)
            .Mass(2500.0f, 0.0f).Strength(70.0f, 1000.0f, 40.0f).Toughness(8.0f)
            .Grip(0.5f, 0.4f, 0.6f, 0.65f).Warmth(1.0f, 840.0f).Hard(5.5f)
            .Melt(1000.0f).Brittle().S);

        T.Add(FMake(K::Bone, TEXT("кость"), F::Solid, L::Bone)
            .Mass(1900.0f, 0.1f).Strength(17.0f, 170.0f, 130.0f).Toughness(1500.0f)
            .Grip(0.5f, 0.4f, 0.85f, 0.5f).Warmth(0.5f, 1300.0f).Hard(3.5f)
            .Burn(450.0f, 7.0f).Brittle().Rots().S);

        T.Add(FMake(K::Leather, TEXT("кожа"), F::Fibres, L::Leather)
            .Mass(900.0f, 0.5f).Strength(0.15f, 5.0f, 25.0f).Toughness(25000.0f)
            .Grip(0.6f, 0.45f, 0.8f, 0.15f).Warmth(0.15f, 1500.0f).Hard(1.0f)
            .Burn(350.0f, 18.0f).Rots().S);

        T.Add(FMake(K::Hide, TEXT("шкура"), F::Fibres, L::Leather)
            .Mass(1050.0f, 0.6f).Strength(0.05f, 2.0f, 15.0f).Toughness(20000.0f)
            .Grip(0.6f, 0.45f, 0.8f, 0.1f).Warmth(0.2f, 3000.0f).Hard(1.0f)
            .Burn(400.0f, 15.0f).Colour(0.8f, 0.7f, 0.6f).Rots().Edible(0.0f, true).S);

        T.Add(FMake(K::Cloth, TEXT("ткань"), F::Fibres, L::Cloth)
            .Mass(450.0f, 0.6f).Strength(0.5f, 0.1f, 40.0f).Toughness(20000.0f)
            .Grip(0.5f, 0.4f, 0.9f, 0.05f).Warmth(0.05f, 1300.0f).Hard(1.0f)
            .Burn(400.0f, 16.0f).Rots().S);

        T.Add(FMake(K::Thread, TEXT("нить"), F::Fibres, L::Cloth)
            .Mass(1200.0f, 0.3f).Strength(5.0f, 1.0f, 300.0f).Toughness(10000.0f)
            .Grip(0.5f, 0.4f, 0.9f, 0.05f).Warmth(0.1f, 1300.0f).Hard(1.0f)
            .Burn(400.0f, 16.0f).Rots().S);

        T.Add(FMake(K::Rope, TEXT("верёвка"), F::Fibres, L::Rope)
            .Mass(800.0f, 0.3f).Strength(1.0f, 1.0f, 50.0f).Toughness(30000.0f)
            .Grip(0.7f, 0.5f, 0.9f, 0.1f).Warmth(0.2f, 1300.0f).Hard(1.0f)
            .Burn(380.0f, 17.0f).Rots().S);

        T.Add(FMake(K::Flax, TEXT("лён"), F::Fibres, L::Straw)
            .Mass(250.0f, 0.8f).Strength(1.0f, 0.05f, 100.0f).Toughness(5000.0f)
            .Grip(0.5f, 0.4f, 0.9f, 0.05f).Warmth(0.05f, 1300.0f).Hard(0.5f)
            .Burn(380.0f, 16.0f).Colour(0.8f, 0.8f, 0.65f).Rots().S);

        T.Add(FMake(K::Wool, TEXT("шерсть"), F::Fibres, L::Cloth)
            .Mass(200.0f, 0.85f).Strength(0.1f, 0.01f, 20.0f).Toughness(5000.0f)
            .Grip(0.6f, 0.45f, 0.9f, 0.05f).Warmth(0.04f, 1360.0f).Hard(0.5f)
            .Burn(570.0f, 20.0f).Colour(0.9f, 0.88f, 0.82f).Rots().S);

        T.Add(FMake(K::Wax, TEXT("воск"), F::Solid, L::Wax)
            .Mass(960.0f, 0.0f).Strength(0.2f, 2.0f, 1.0f).Toughness(50.0f)
            .Grip(0.3f, 0.2f, 1.0f, 0.1f).Warmth(0.25f, 3400.0f).Hard(0.5f)
            .Melt(63.0f, K::None, 350.0f).Burn(250.0f, 42.0f, K::None).S);

        T.Add(FMake(K::Resin, TEXT("смола"), F::Solid, L::Wax)
            .Mass(1070.0f, 0.0f).Strength(3.0f, 10.0f, 2.0f).Toughness(5.0f)
            .Grip(0.8f, 0.6f, 1.0f, 0.1f).Warmth(0.15f, 1300.0f).Hard(2.0f)
            .Melt(80.0f).Burn(250.0f, 38.0f, K::None).Colour(0.9f, 0.6f, 0.2f).Brittle().S);

        T.Add(FMake(K::Tar, TEXT("дёготь"), F::Paste, L::Coal)
            .Mass(1050.0f, 0.0f).Strength(0.001f, 0.0f, 0.0f).Toughness(1.0f)
            .Grip(0.9f, 0.8f, 1.0f, 0.0f).Warmth(0.2f, 1700.0f).Hard(0.0f)
            .Melt(30.0f).Burn(300.0f, 36.0f, K::None).S);

        T.Add(FMake(K::Oil, TEXT("масло"), F::Liquid, L::Wax)
            .Mass(920.0f, 0.0f).Strength(1.5f, 0.0f, 0.0f).Toughness(0.0f)
            .Grip(0.08f, 0.05f, 1.0f, 0.0f).Warmth(0.17f, 1970.0f).Hard(0.0f)
            .Melt(-10.0f, K::None, 300.0f).Burn(320.0f, 37.0f, K::None).Colour(1.0f, 0.9f, 0.4f).S);

        T.Add(FMake(K::Soap, TEXT("мыло"), F::Solid, L::Plaster)
            .Mass(1100.0f, 0.05f).Strength(0.1f, 0.5f, 0.2f).Toughness(20.0f)
            .Grip(0.3f, 0.1f, 0.2f, 0.1f).Warmth(0.3f, 2000.0f).Hard(1.0f)
            .Melt(60.0f).Colour(0.95f, 0.9f, 0.8f).Dissolves().S);

        T.Add(FMake(K::Paper, TEXT("бумага"), F::Fibres, L::Paper)
            .Mass(800.0f, 0.5f).Strength(3.0f, 1.0f, 40.0f).Toughness(5000.0f)
            .Grip(0.4f, 0.3f, 0.9f, 0.05f).Warmth(0.05f, 1340.0f).Hard(1.0f)
            .Burn(233.0f, 16.0f).Rots().S);

        T.Add(FMake(K::Ink, TEXT("чернила"), F::Liquid, L::Coal)
            .Mass(1000.0f, 0.0f).Strength(2.0f, 0.0f, 0.0f).Toughness(0.0f)
            .Grip(0.05f, 0.03f, 1.0f, 0.0f).Warmth(0.6f, 4000.0f).Hard(0.0f)
            .Melt(-2.0f, K::None, 100.0f).S);

        T.Add(FMake(K::Medicine, TEXT("отвар"), F::Liquid, L::Water)
            .Mass(1010.0f, 0.0f).Strength(2.2f, 0.0f, 0.0f).Toughness(0.0f)
            .Grip(0.05f, 0.03f, 1.0f, 0.0f).Warmth(0.6f, 4100.0f).Hard(0.0f)
            .Melt(-1.0f, K::None, 100.0f).Colour(0.6f, 0.45f, 0.2f).Edible(10.0f, true).S);
    }


    void AddFoodMore(TArray<FSubstance>& T)
    {
        using K = EResourceKind;
        using F = EMatterForm;
        using L = EMatterLook;

        auto Veg = [&](EResourceKind Kind, const TCHAR* Name, float Density, float Kcal,
                       float R, float G, float B)
        {
            T.Add(FMake(Kind, Name, F::Living, L::Greens)
                .Mass(Density, 0.1f).Strength(0.005f, 0.3f, 0.2f).Toughness(200.0f)
                .Grip(0.5f, 0.4f, 0.7f, 0.2f).Warmth(0.5f, 3800.0f).Hard(0.5f)
                .Colour(R, G, B).Edible(Kcal, true).S);
        };

        auto Cereal = [&](EResourceKind Kind, const TCHAR* Name, float R, float G, float B)
        {
            T.Add(FMake(Kind, Name, F::Grains, L::Food)
                .Mass(750.0f, 0.4f).Strength(3.0f, 10.0f, 5.0f).Toughness(5.0f)
                .Grip(0.45f, 0.35f, 0.8f, 0.05f).Warmth(0.15f, 1700.0f).Hard(1.0f)
                .Loose(25.0f, 1.0e-5f).Burn(400.0f, 16.0f).Colour(R, G, B).Edible(3400.0f, true).S);
        };

        auto Fruit = [&](EResourceKind Kind, const TCHAR* Name, float Kcal, float R, float G, float B)
        {
            T.Add(FMake(Kind, Name, F::Living, L::Greens)
                .Mass(850.0f, 0.08f).Strength(0.002f, 0.1f, 0.05f).Toughness(50.0f)
                .Grip(0.5f, 0.4f, 0.7f, 0.2f).Warmth(0.5f, 3800.0f).Hard(0.3f)
                .Colour(R, G, B).Edible(Kcal, true).S);
        };

        Veg(K::Turnip,      TEXT("репа"),       960.0f, 280.0f,  0.9f,  0.85f, 0.7f);
        Veg(K::Cabbage,     TEXT("капуста"),    950.0f, 250.0f,  0.7f,  0.85f, 0.55f);
        Veg(K::Carrot,      TEXT("морковь"),    980.0f, 410.0f,  0.95f, 0.5f,  0.15f);
        Veg(K::Onion,       TEXT("лук"),        920.0f, 400.0f,  0.85f, 0.75f, 0.5f);
        Veg(K::Garlic,      TEXT("чеснок"),     900.0f, 1490.0f, 0.95f, 0.92f, 0.85f);
        Veg(K::Beet,        TEXT("свёкла"),     970.0f, 430.0f,  0.5f,  0.1f,  0.15f);
        Veg(K::Cucumber,    TEXT("огурец"),     940.0f, 150.0f,  0.4f,  0.65f, 0.25f);
        Veg(K::Pumpkin,     TEXT("тыква"),      900.0f, 260.0f,  0.95f, 0.6f,  0.15f);
        Veg(K::Melon,       TEXT("дыня"),       900.0f, 340.0f,  0.95f, 0.85f, 0.45f);
        Veg(K::Watermelon,  TEXT("арбуз"),      940.0f, 300.0f,  0.3f,  0.6f,  0.3f);
        Veg(K::Potato,      TEXT("картофель"),  1020.0f, 770.0f, 0.8f,  0.7f,  0.5f);
        Veg(K::Sunflower,   TEXT("подсолнух"),  700.0f, 580.0f,  0.95f, 0.8f,  0.1f);

        Cereal(K::Wheat, TEXT("пшеница"), 1.0f, 0.9f, 0.55f);
        Cereal(K::Rye,   TEXT("рожь"),    0.85f, 0.8f, 0.55f);
        Cereal(K::Barley, TEXT("ячмень"), 0.9f, 0.85f, 0.6f);
        Cereal(K::Oats,  TEXT("овёс"),    0.85f, 0.75f, 0.5f);

        T.Add(FMake(K::Hemp, TEXT("конопля"), F::Fibres, L::Straw)
            .Mass(300.0f, 0.8f).Strength(0.01f, 0.05f, 0.6f).Toughness(700.0f)
            .Grip(0.45f, 0.35f, 0.9f, 0.1f).Warmth(0.07f, 1600.0f).Hard(0.5f)
            .Burn(260.0f, 15.5f).Colour(0.6f, 0.7f, 0.4f).Rots().S);

        T.Add(FMake(K::Hay, TEXT("сено"), F::Fibres, L::Straw)
            .Mass(90.0f, 0.92f).Strength(0.005f, 0.02f, 0.3f).Toughness(500.0f)
            .Grip(0.45f, 0.35f, 0.9f, 0.1f).Warmth(0.06f, 1600.0f).Hard(0.3f)
            .Burn(230.0f, 14.0f).Colour(0.85f, 0.8f, 0.5f).Rots().S);

        Fruit(K::Apple,  TEXT("яблоко"),   520.0f, 0.85f, 0.2f, 0.2f);
        Fruit(K::Pear,   TEXT("груша"),    570.0f, 0.85f, 0.8f, 0.3f);
        Fruit(K::Plum,   TEXT("слива"),    460.0f, 0.35f, 0.2f, 0.55f);
        Fruit(K::Cherry, TEXT("вишня"),    500.0f, 0.6f,  0.05f, 0.1f);
        Fruit(K::Grape,  TEXT("виноград"), 670.0f, 0.4f,  0.2f, 0.5f);

        T.Add(FMake(K::Nut, TEXT("орех"), F::Grains, L::Food)
            .Mass(650.0f, 0.3f).Strength(2.0f, 8.0f, 4.0f).Toughness(30.0f)
            .Grip(0.5f, 0.4f, 0.8f, 0.15f).Warmth(0.15f, 1700.0f).Hard(2.5f)
            .Loose(30.0f, 1.0e-5f).Burn(400.0f, 20.0f).Colour(0.6f, 0.45f, 0.25f).Edible(6500.0f, true).S);

        T.Add(FMake(K::Feather, TEXT("пух и перья"), F::Fibres, L::Plaster)
            .Mass(30.0f, 0.95f).Strength(0.001f, 0.01f, 0.1f).Toughness(100.0f)
            .Grip(0.5f, 0.4f, 0.9f, 0.0f).Warmth(0.04f, 1500.0f).Hard(0.1f)
            .Burn(300.0f, 18.0f).Colour(0.9f, 0.88f, 0.85f).Rots().S);

        T.Add(FMake(K::Fur, TEXT("мех"), F::Fibres, L::Leather)
            .Mass(600.0f, 0.7f).Strength(0.5f, 2.0f, 20.0f).Toughness(8000.0f)
            .Grip(0.6f, 0.5f, 0.9f, 0.1f).Warmth(0.05f, 1400.0f).Hard(0.5f)
            .Burn(320.0f, 16.0f).Colour(0.55f, 0.4f, 0.3f).Rots().S);

        T.Add(FMake(K::Antler, TEXT("рога"), F::Solid, L::Bone)
            .Mass(1900.0f, 0.1f).Strength(10.0f, 100.0f, 30.0f).Toughness(3000.0f)
            .Grip(0.5f, 0.4f, 0.7f, 0.3f).Warmth(0.3f, 1500.0f).Hard(2.5f)
            .Burn(500.0f, 12.0f).Colour(0.8f, 0.75f, 0.65f).S);

        T.Add(FMake(K::Tallow, TEXT("сало"), F::Solid, L::Wax)
            .Mass(950.0f, 0.0f).Strength(0.1f, 0.5f, 0.2f).Toughness(50.0f)
            .Grip(0.2f, 0.1f, 0.3f, 0.05f).Warmth(0.2f, 2000.0f).Hard(0.3f)
            .Melt(45.0f, K::None, 200.0f).Burn(300.0f, 38.0f, K::None).Colour(0.95f, 0.92f, 0.85f).Edible(9000.0f, true).S);

        T.Add(FMake(K::CurdledMilk, TEXT("скисшее молоко"), F::Liquid, L::Plaster)
            .Mass(1030.0f, 0.0f).Strength(2.2f, 0.0f, 0.0f).Toughness(0.0f)
            .Grip(0.05f, 0.03f, 1.0f, 0.0f).Warmth(0.55f, 3900.0f).Hard(0.0f)
            .Melt(-0.5f, K::None, 100.0f).Colour(0.92f, 0.92f, 0.85f).Edible(600.0f, true).S);

        T.Add(FMake(K::Butter, TEXT("сливочное масло"), F::Solid, L::Wax)
            .Mass(910.0f, 0.0f).Strength(0.05f, 0.2f, 0.1f).Toughness(20.0f)
            .Grip(0.15f, 0.08f, 0.2f, 0.02f).Warmth(0.2f, 2200.0f).Hard(0.2f)
            .Melt(32.0f, K::None, 180.0f).Burn(350.0f, 37.0f, K::None).Colour(0.98f, 0.92f, 0.6f).Edible(7200.0f, true).S);

        T.Add(FMake(K::Cheese, TEXT("сыр"), F::Solid, L::Food)
            .Mass(1050.0f, 0.05f).Strength(0.1f, 0.5f, 0.3f).Toughness(500.0f)
            .Grip(0.5f, 0.4f, 0.6f, 0.1f).Warmth(0.3f, 3000.0f).Hard(1.0f)
            .Colour(0.95f, 0.85f, 0.5f).Edible(3500.0f, true).S);

        T.Add(FMake(K::CottageCheese, TEXT("творог"), F::Paste, L::Plaster)
            .Mass(1000.0f, 0.2f).Strength(0.02f, 0.1f, 0.05f).Toughness(50.0f)
            .Grip(0.4f, 0.3f, 0.5f, 0.02f).Warmth(0.4f, 3400.0f).Hard(0.1f)
            .Colour(0.95f, 0.95f, 0.9f).Edible(1500.0f, true).S);

        T.Add(FMake(K::SmokedFish, TEXT("копчёная рыба"), F::Solid, L::Food)
            .Mass(900.0f, 0.05f).Strength(0.001f, 0.2f, 0.2f).Toughness(2000.0f)
            .Grip(0.5f, 0.4f, 0.6f, 0.05f).Warmth(0.4f, 3200.0f).Hard(0.2f)
            .Colour(0.5f, 0.35f, 0.2f).Edible(1800.0f, true).S);

        T.Add(FMake(K::SmokedMeat, TEXT("копчёная грудинка"), F::Solid, L::Food)
            .Mass(950.0f, 0.05f).Strength(0.001f, 0.4f, 0.4f).Toughness(4000.0f)
            .Grip(0.6f, 0.5f, 0.7f, 0.05f).Warmth(0.4f, 3000.0f).Hard(0.3f)
            .Colour(0.55f, 0.3f, 0.2f).Edible(2800.0f, true).S);

        T.Add(FMake(K::DriedFish, TEXT("вяленая рыба"), F::Solid, L::Food)
            .Mass(800.0f, 0.1f).Strength(0.001f, 0.3f, 0.3f).Toughness(2500.0f)
            .Grip(0.5f, 0.4f, 0.6f, 0.05f).Warmth(0.4f, 3200.0f).Hard(0.5f)
            .Colour(0.6f, 0.55f, 0.45f).Edible(2000.0f, true).S);

        T.Add(FMake(K::Hops, TEXT("хмель"), F::Fibres, L::Greens)
            .Mass(400.0f, 0.8f).Strength(0.01f, 0.05f, 0.5f).Toughness(300.0f)
            .Grip(0.5f, 0.4f, 0.8f, 0.05f).Warmth(0.15f, 3400.0f).Hard(0.3f)
            .Burn(280.0f, 14.0f).Colour(0.5f, 0.7f, 0.4f).Rots().S);

        T.Add(FMake(K::Madder, TEXT("марена"), F::Living, L::Greens)
            .Mass(500.0f, 0.6f).Strength(0.01f, 0.02f, 0.5f).Toughness(100.0f)
            .Grip(0.5f, 0.4f, 0.8f, 0.05f).Warmth(0.2f, 3400.0f).Hard(0.3f)
            .Burn(300.0f, 15.0f).Colour(0.7f, 0.2f, 0.15f).Rots().S);

        T.Add(FMake(K::Woad, TEXT("вайда"), F::Living, L::Greens)
            .Mass(500.0f, 0.6f).Strength(0.01f, 0.02f, 0.5f).Toughness(100.0f)
            .Grip(0.5f, 0.4f, 0.8f, 0.05f).Warmth(0.2f, 3400.0f).Hard(0.3f)
            .Burn(300.0f, 15.0f).Colour(0.3f, 0.5f, 0.6f).Rots().S);

        auto Drink = [&](EResourceKind Kind, const TCHAR* Name, float Kcal, float R, float G, float B)
        {
            T.Add(FMake(Kind, Name, F::Liquid, L::Water)
                .Mass(1005.0f, 0.0f).Strength(2.2f, 0.0f, 0.0f).Toughness(0.0f)
                .Grip(0.05f, 0.03f, 1.0f, 0.0f).Warmth(0.6f, 4000.0f).Hard(0.0f)
                .Melt(-2.0f, K::None, 95.0f).Colour(R, G, B).Edible(Kcal, true).S);
        };

        T.Add(FMake(K::GrapeMust, TEXT("виноградное сусло"), F::Liquid, L::Water)
            .Mass(1080.0f, 0.0f).Strength(2.2f, 0.0f, 0.0f).Toughness(0.0f)
            .Grip(0.1f, 0.06f, 1.0f, 0.0f).Warmth(0.6f, 3900.0f).Hard(0.0f)
            .Melt(-2.0f, K::None, 100.0f).Colour(0.45f, 0.2f, 0.35f).Edible(600.0f, true).S);

        Drink(K::Kvass, TEXT("квас"),   300.0f, 0.7f, 0.5f, 0.2f);
        Drink(K::Beer,  TEXT("пиво"),   450.0f, 0.85f, 0.65f, 0.2f);
        Drink(K::Wine,  TEXT("вино"),   700.0f, 0.5f, 0.1f, 0.15f);
        Drink(K::Mead,  TEXT("медовуха"), 500.0f, 0.95f, 0.8f, 0.3f);

        T.Add(FMake(K::Vinegar, TEXT("уксус"), F::Liquid, L::Water)
            .Mass(1010.0f, 0.0f).Strength(2.0f, 0.0f, 0.0f).Toughness(0.0f)
            .Grip(0.05f, 0.03f, 1.0f, 0.0f).Warmth(0.6f, 4100.0f).Hard(0.0f)
            .Melt(-2.0f, K::None, 100.0f).Colour(0.95f, 0.95f, 0.9f).Edible(10.0f, false).S);

        T.Add(FMake(K::Dye, TEXT("краска"), F::Paste, L::Plaster)
            .Mass(1100.0f, 0.1f).Strength(0.02f, 0.1f, 0.05f).Toughness(10.0f)
            .Grip(0.4f, 0.3f, 0.5f, 0.02f).Warmth(0.3f, 3000.0f).Hard(0.2f)
            .Colour(0.7f, 0.15f, 0.2f).Dissolves().S);

        T.Add(FMake(K::Candle, TEXT("свеча"), F::Solid, L::Wax)
            .Mass(950.0f, 0.0f).Strength(0.2f, 2.0f, 1.0f).Toughness(50.0f)
            .Grip(0.3f, 0.2f, 0.9f, 0.1f).Warmth(0.25f, 3400.0f).Hard(0.5f)
            .Melt(60.0f, K::None, 300.0f).Burn(240.0f, 42.0f, K::None).Colour(0.95f, 0.9f, 0.75f).S);

        T.Add(FMake(K::OliveOil, TEXT("постное масло"), F::Liquid, L::Wax)
            .Mass(910.0f, 0.0f).Strength(1.5f, 0.0f, 0.0f).Toughness(0.0f)
            .Grip(0.08f, 0.05f, 1.0f, 0.0f).Warmth(0.17f, 1970.0f).Hard(0.0f)
            .Melt(-6.0f, K::None, 300.0f).Burn(320.0f, 37.0f, K::None).Colour(0.9f, 0.85f, 0.4f).Edible(8800.0f, true).S);

        T.Add(FMake(K::Soda, TEXT("сода"), F::Grains, L::Plaster)
            .Mass(2200.0f, 0.3f).Strength(0.5f, 1.0f, 0.5f).Toughness(5.0f)
            .Grip(0.5f, 0.4f, 0.7f, 0.05f).Warmth(0.9f, 1000.0f).Hard(2.5f)
            .Loose(30.0f, 1.0e-6f).Colour(0.95f, 0.95f, 0.98f).Dissolves().S);

        T.Add(FMake(K::Basket, TEXT("корзина"), F::Solid, L::Straw)
            .Mass(300.0f, 0.9f).Strength(0.2f, 1.0f, 5.0f).Toughness(2000.0f)
            .Grip(0.5f, 0.4f, 0.85f, 0.15f).Warmth(0.1f, 1500.0f).Hard(1.0f)
            .Burn(260.0f, 15.0f).Colour(0.8f, 0.7f, 0.5f).Rots().S);

        T.Add(FMake(K::Cork, TEXT("пробка"), F::Solid, L::Bark)
            .Mass(240.0f, 0.7f).Strength(0.02f, 1.5f, 1.0f).Toughness(300.0f)
            .Grip(0.6f, 0.5f, 0.8f, 0.2f).Warmth(0.05f, 1200.0f).Hard(1.0f)
            .Burn(280.0f, 18.0f).Colour(0.85f, 0.75f, 0.6f).Rots().S);

        T.Add(FMake(K::Gunpowder, TEXT("порох"), F::Grains, L::Coal)
            .Mass(1700.0f, 0.4f).Strength(0.001f, 0.001f, 0.0f).Toughness(0.1f)
            .Grip(0.4f, 0.3f, 0.4f, 0.0f).Warmth(1.0f, 800.0f).Hard(0.5f)
            .Burn(300.0f, 3.0f, K::None).Loose(25.0f, 1.0e-6f).Colour(0.12f, 0.12f, 0.12f).S);

        T.Add(FMake(K::Mortar, TEXT("известковый раствор"), F::Paste, L::Plaster)
            .Mass(1800.0f, 0.4f).Strength(0.05f, 0.5f, 0.1f).Toughness(10.0f)
            .Grip(0.5f, 0.4f, 0.5f, 0.02f).Warmth(0.9f, 900.0f).Hard(1.0f)
            .Earth(0.25f, 0.5f, 1.0e-9f, 30.0f, 25.0f).Colour(0.85f, 0.83f, 0.78f).S);

        T.Add(FMake(K::CharcoalPencil, TEXT("угольный карандаш"), F::Solid, L::Coal)
            .Mass(600.0f, 0.2f).Strength(0.5f, 5.0f, 1.0f).Toughness(20.0f)
            .Grip(0.5f, 0.4f, 0.7f, 0.1f).Warmth(0.5f, 700.0f).Hard(1.5f)
            .Burn(400.0f, 30.0f).Colour(0.15f, 0.15f, 0.15f).S);
    }
    void AddFood(TArray<FSubstance>& T)
    {
        using K = EResourceKind;
        using F = EMatterForm;
        using L = EMatterLook;

        T.Add(FMake(K::RawFood, TEXT("овощи"), F::Living, L::Greens)
            .Mass(950.0f, 0.1f).Strength(0.005f, 0.3f, 0.2f).Toughness(200.0f)
            .Grip(0.5f, 0.4f, 0.7f, 0.2f).Warmth(0.5f, 3800.0f).Hard(0.5f)
            .Edible(400.0f, true).S);

        T.Add(FMake(K::CookedFood, TEXT("хлеб"), F::Living, L::Food)
            .Mass(250.0f, 0.75f).Strength(0.0005f, 0.05f, 0.02f).Toughness(100.0f)
            .Grip(0.6f, 0.5f, 0.8f, 0.1f).Warmth(0.2f, 2800.0f).Hard(0.3f)
            .Burn(300.0f, 10.0f, K::Charcoal).Edible(2600.0f, true).S);

        T.Add(FMake(K::Grain, TEXT("зерно"), F::Grains, L::Food)
            .Mass(750.0f, 0.4f).Strength(3.0f, 10.0f, 5.0f).Toughness(5.0f)
            .Grip(0.45f, 0.35f, 0.8f, 0.05f).Warmth(0.15f, 1700.0f).Hard(1.0f)
            .Loose(25.0f, 1.0e-5f).Burn(400.0f, 16.0f).Colour(1.0f, 0.9f, 0.55f).Edible(3400.0f, true).S);

        T.Add(FMake(K::Flour, TEXT("мука"), F::Grains, L::Plaster)
            .Mass(590.0f, 0.6f).Strength(0.001f, 0.001f, 0.0f).Toughness(0.1f)
            .Grip(0.6f, 0.5f, 0.5f, 0.0f).Warmth(0.1f, 1800.0f).Hard(0.5f)
            .Loose(45.0f, 1.0e-7f).Burn(380.0f, 16.0f).Colour(0.98f, 0.96f, 0.9f).Edible(3640.0f, true).S);

        T.Add(FMake(K::Seed, TEXT("семена"), F::Grains, L::Food)
            .Mass(700.0f, 0.4f).Strength(2.0f, 5.0f, 2.0f).Toughness(5.0f)
            .Grip(0.45f, 0.35f, 0.8f, 0.05f).Warmth(0.15f, 1700.0f).Hard(1.0f)
            .Loose(30.0f, 1.0e-5f).Burn(400.0f, 20.0f).Colour(0.6f, 0.5f, 0.35f).Edible(3500.0f, false).S);

        T.Add(FMake(K::Herb, TEXT("травы"), F::Living, L::Greens)
            .Mass(300.0f, 0.7f).Strength(0.01f, 0.01f, 1.0f).Toughness(200.0f)
            .Grip(0.5f, 0.4f, 0.8f, 0.05f).Warmth(0.2f, 3500.0f).Hard(0.2f)
            .Burn(300.0f, 15.0f).Edible(250.0f, true).S);

        T.Add(FMake(K::Berry, TEXT("ягоды"), F::Living, L::Greens)
            .Mass(1050.0f, 0.05f).Strength(0.001f, 0.02f, 0.01f).Toughness(20.0f)
            .Grip(0.5f, 0.4f, 0.6f, 0.05f).Warmth(0.5f, 3700.0f).Hard(0.1f)
            .Colour(0.75f, 0.2f, 0.3f).Edible(500.0f, true).S);

        T.Add(FMake(K::Mushroom, TEXT("грибы"), F::Living, L::Food)
            .Mass(900.0f, 0.3f).Strength(0.001f, 0.05f, 0.02f).Toughness(50.0f)
            .Grip(0.5f, 0.4f, 0.6f, 0.05f).Warmth(0.4f, 3700.0f).Hard(0.1f)
            .Colour(0.75f, 0.6f, 0.45f).Edible(300.0f, true).S);

        T.Add(FMake(K::Egg, TEXT("яйцо"), F::Living, L::Plaster)
            .Mass(1030.0f, 0.0f).Strength(30.0f, 10.0f, 1.0f).Toughness(10.0f)
            .Grip(0.4f, 0.3f, 0.6f, 0.1f).Warmth(0.5f, 3200.0f).Hard(3.0f)
            .Colour(0.95f, 0.88f, 0.75f).Edible(1430.0f, true).Brittle().S);

        T.Add(FMake(K::Meat, TEXT("мясо"), F::Living, L::Food)
            .Mass(1060.0f, 0.0f).Strength(0.001f, 0.5f, 0.5f).Toughness(5000.0f)
            .Grip(0.6f, 0.4f, 0.6f, 0.05f).Warmth(0.45f, 3500.0f).Hard(0.1f)
            .Burn(350.0f, 12.0f, K::Charcoal).Colour(0.7f, 0.25f, 0.25f).Edible(2500.0f, true).S);

        T.Add(FMake(K::Fish, TEXT("рыба"), F::Living, L::Food)
            .Mass(1050.0f, 0.0f).Strength(0.001f, 0.3f, 0.3f).Toughness(3000.0f)
            .Grip(0.3f, 0.2f, 0.4f, 0.05f).Warmth(0.45f, 3600.0f).Hard(0.1f)
            .Colour(0.65f, 0.7f, 0.75f).Edible(1400.0f, true).S);

        T.Add(FMake(K::Milk, TEXT("молоко"), F::Liquid, L::Plaster)
            .Mass(1030.0f, 0.0f).Strength(2.2f, 0.0f, 0.0f).Toughness(0.0f)
            .Grip(0.05f, 0.03f, 1.0f, 0.0f).Warmth(0.55f, 3900.0f).Hard(0.0f)
            .Melt(-0.5f, K::None, 100.0f).Edible(640.0f, true).S);

        T.Add(FMake(K::Honey, TEXT("мёд"), F::Liquid, L::Wax)
            .Mass(1420.0f, 0.0f).Strength(2.0f, 0.0f, 0.0f).Toughness(0.0f)
            .Grip(0.9f, 0.8f, 1.0f, 0.0f).Warmth(0.5f, 2500.0f).Hard(0.0f)
            .Melt(-20.0f).Colour(1.0f, 0.7f, 0.2f).Edible(3040.0f, false).S);

        T.Add(FMake(K::Maize, TEXT("кукуруза"), F::Grains, L::Food)
            .Mass(720.0f, 0.4f).Strength(3.0f, 12.0f, 5.0f).Toughness(8.0f)
            .Grip(0.45f, 0.35f, 0.8f, 0.05f).Warmth(0.15f, 1700.0f).Hard(1.5f)
            .Loose(27.0f, 1.0e-5f).Burn(400.0f, 16.0f).Colour(1.0f, 0.8f, 0.2f).Edible(3650.0f, true).S);

        T.Add(FMake(K::Bread, TEXT("хлеб"), F::Living, L::Food)
            .Mass(260.0f, 0.75f).Strength(0.0005f, 0.05f, 0.02f).Toughness(100.0f)
            .Grip(0.6f, 0.5f, 0.8f, 0.1f).Warmth(0.2f, 2800.0f).Hard(0.3f)
            .Burn(300.0f, 10.0f, K::Charcoal).Colour(0.75f, 0.5f, 0.25f).Edible(2500.0f, true).S);

        T.Add(FMake(K::Sickle, TEXT("серп"), F::Solid, L::Iron)
            .Mass(7700.0f, 0.0f).Strength(200.0f, 250.0f, 350.0f).Toughness(60000.0f)
            .Grip(0.6f, 0.45f, 0.9f, 0.55f).Warmth(60.0f, 450.0f).Hard(5.0f)
            .Melt(1538.0f).Rusts().Metal().S);

        T.Add(FMake(K::Shirt, TEXT("рубаха"), F::Fibres, L::Cloth)
            .Mass(450.0f, 0.6f).Strength(0.5f, 0.1f, 40.0f).Toughness(20000.0f)
            .Grip(0.5f, 0.4f, 0.9f, 0.05f).Warmth(0.05f, 1300.0f).Hard(1.0f)
            .Burn(400.0f, 16.0f).Colour(0.92f, 0.9f, 0.82f).Rots().S);
    }

    TArray<FSubstance> Build()
    {
        TArray<FSubstance> Table;
        AddWoods(Table);
        AddStones(Table);
        AddStonesMore(Table);
        AddEarths(Table);
        AddMetals(Table);
        AddMade(Table);
        AddFood(Table);
        AddFoodMore(Table);
        return Table;
    }

    float Smooth01(float X)
    {
        const float T = FMath::Clamp(X, 0.0f, 1.0f);
        return T * T * (3.0f - 2.0f * T);
    }

    bool Freezes(const FSubstance& S, float Moisture, float Celsius)
    {
        return Celsius < -0.5f && Moisture > 0.02f && (S.Porosity > 0.03f || S.IsSoil() || S.Form == EMatterForm::Grains);
    }
}

float FSubstance::SaturatedMoisture() const
{
    if (Form == EMatterForm::Liquid || bMetal || Porosity <= 0.0f)
    {
        return 0.0f;
    }
    const float ByPores = Porosity * FMatter::WaterDensity / FMath::Max(1.0f, Density);
    if (bGrain)
    {
        return FMath::Min(ByPores, 1.6f);
    }
    return ByPores;
}

const TArray<FSubstance>& FMatter::All()
{
    static const TArray<FSubstance> Table = Build();
    return Table;
}

const FSubstance& FMatter::Of(EResourceKind Kind)
{
    static TMap<EResourceKind, int32> Index;
    const TArray<FSubstance>& Table = All();
    if (Index.Num() == 0)
    {
        for (int32 i = 0; i < Table.Num(); ++i)
        {
            Index.Add(Table[i].Kind, i);
        }
    }
    if (const int32* Found = Index.Find(Kind))
    {
        return Table[*Found];
    }
    static const FSubstance Stone = Table[Index.FindRef(EResourceKind::Stone)];
    return Stone;
}

EMatterPhase FMatter::PhaseOf(const FSubstance& S, float Moisture, float Celsius, bool bBurning, float Char, float Rot)
{
    if (bBurning)
    {
        return EMatterPhase::Burning;
    }
    if (S.Form == EMatterForm::Liquid)
    {
        return Celsius <= S.Melts ? EMatterPhase::Frozen : EMatterPhase::Liquid;
    }
    if (Celsius >= S.Melts && S.Melts < FSubstance::Never && S.MeltsInto != EResourceKind::Water)
    {
        return EMatterPhase::Molten;
    }
    if (Char > 0.6f)
    {
        return EMatterPhase::Charred;
    }
    if (Rot > 0.6f)
    {
        return EMatterPhase::Rotten;
    }
    if (Freezes(S, Moisture, Celsius))
    {
        return EMatterPhase::Frozen;
    }
    if (S.IsSoil())
    {
        if (Moisture >= S.LiquidLimit)
        {
            return EMatterPhase::Slurry;
        }
        if (Moisture >= S.PlasticLimit)
        {
            return EMatterPhase::Plastic;
        }
        return Moisture > S.PlasticLimit * 0.5f ? EMatterPhase::Damp : EMatterPhase::Dry;
    }
    const float Full = S.SaturatedMoisture();
    if (S.Form == EMatterForm::Grains)
    {
        if (Full > 0.0f && Moisture > Full * 0.92f)
        {
            return EMatterPhase::Slurry;
        }
        return Moisture > 0.03f ? EMatterPhase::Damp : EMatterPhase::Dry;
    }
    if (S.bGrain)
    {
        return Moisture > 0.28f ? EMatterPhase::Damp : EMatterPhase::Dry;
    }
    return (Full > 0.0f && Moisture > Full * 0.3f) ? EMatterPhase::Damp : EMatterPhase::Dry;
}

float FMatter::StrengthFactor(const FSubstance& S, float Moisture, float Celsius, float Rot, float Char)
{
    float Factor = 1.0f;

    if (S.bGrain || S.Form == EMatterForm::Fibres)
    {
        Factor *= 1.0f - 0.45f * Smooth01((Moisture - 0.12f) / 0.16f);
    }
    else if (S.IsSoil())
    {
        if (Freezes(S, Moisture, Celsius))
        {
            Factor *= FMath::Clamp(3.0f * (1.0f + Moisture * 4.0f), 1.0f, 8.0f);
        }
        else if (Moisture >= S.LiquidLimit)
        {
            Factor *= 0.01f;
        }
        else if (Moisture >= S.PlasticLimit)
        {
            const float Along = (Moisture - S.PlasticLimit) / FMath::Max(0.01f, S.LiquidLimit - S.PlasticLimit);
            Factor *= FMath::Lerp(0.15f, 0.01f, Along);
        }
        else
        {
            const float Along = Moisture / FMath::Max(0.01f, S.PlasticLimit);
            Factor *= FMath::Lerp(1.0f, 0.15f, Smooth01((Along - 0.4f) / 0.6f));
        }
    }
    else if (S.Form == EMatterForm::Grains)
    {
        const float Capillary = FMath::Exp(-FMath::Square((Moisture - 0.08f) / 0.06f));
        Factor *= 0.2f + 4.0f * Capillary;
        if (Freezes(S, Moisture, Celsius))
        {
            Factor *= 1.0f + 20.0f * FMath::Min(Moisture, 0.3f);
        }
    }
    else
    {
        const float Full = S.SaturatedMoisture();
        if (Full > 0.0f)
        {
            Factor *= 1.0f - 0.25f * FMath::Clamp(Moisture / Full, 0.0f, 1.0f);
        }
    }

    if (S.bMetal && S.Melts < FSubstance::Never)
    {
        const float Kelvin = Celsius + 273.15f;
        const float MeltKelvin = S.Melts + 273.15f;
        const float Homologous = Kelvin / MeltKelvin;
        if (Homologous > 0.35f)
        {
            Factor *= FMath::Pow(FMath::Clamp(1.0f - (Homologous - 0.35f) / 0.65f, 0.0f, 1.0f), 1.5f);
        }
    }

    if (S.Kind == EResourceKind::Ice || S.Kind == EResourceKind::Snow)
    {
        Factor *= FMath::Clamp(-Celsius / 4.0f, 0.1f, 1.0f);
    }

    Factor *= 1.0f - 0.95f * FMath::Clamp(Rot, 0.0f, 1.0f);
    Factor *= 1.0f - 0.9f * FMath::Clamp(Char, 0.0f, 1.0f);
    return FMath::Max(0.0f, Factor);
}

float FMatter::FrictionOf(const FSubstance& S, float Moisture, float Celsius, bool bSliding)
{
    float Mu = bSliding ? S.Sliding : S.Friction;
    const float Full = S.SaturatedMoisture();

    if (S.IsSoil())
    {
        if (Freezes(S, Moisture, Celsius))
        {
            return Moisture >= S.LiquidLimit ? (bSliding ? 0.04f : 0.12f) : Mu;
        }
        if (Moisture > S.PlasticLimit * 0.6f)
        {
            const float Along = (Moisture - S.PlasticLimit * 0.6f) / FMath::Max(0.01f, S.LiquidLimit - S.PlasticLimit * 0.6f);
            Mu *= FMath::Lerp(1.0f, S.WetGrip * 0.6f, FMath::Clamp(Along, 0.0f, 1.0f));
        }
        return FMath::Clamp(Mu, 0.02f, 1.5f);
    }

    const float Wetness = Full > 0.0f ? FMath::Clamp(Moisture / Full, 0.0f, 1.0f) : FMath::Clamp(Moisture * 20.0f, 0.0f, 1.0f);
    if (Celsius < -0.5f && Wetness > 0.3f)
    {
        return bSliding ? 0.03f : 0.1f;
    }
    Mu *= FMath::Lerp(1.0f, S.WetGrip, Wetness);
    return FMath::Clamp(Mu, 0.01f, 1.5f);
}

float FMatter::BounceOf(const FSubstance& S, float Moisture, float Celsius)
{
    const EMatterPhase Phase = PhaseOf(S, Moisture, Celsius, false, 0.0f, 0.0f);
    if (Phase == EMatterPhase::Plastic || Phase == EMatterPhase::Slurry || Phase == EMatterPhase::Liquid || Phase == EMatterPhase::Molten)
    {
        return 0.0f;
    }
    const float Full = S.SaturatedMoisture();
    const float Wetness = Full > 0.0f ? FMath::Clamp(Moisture / Full, 0.0f, 1.0f) : 0.0f;
    return FMath::Clamp(S.Bounce * (1.0f - 0.4f * Wetness), 0.0f, 0.95f);
}

float FMatter::BreakEnergy(const FSubstance& S, float CrossSectionM2, bool bAlongGrain)
{
    const float Gc = (bAlongGrain && S.Split > 0.0f) ? S.Split : S.Fracture;
    const float Area = FMath::Max(CrossSectionM2, 1.0e-6f);
    const float SizeEffect = S.bBrittle ? 1.0f + FMath::Sqrt(FMath::Sqrt(Area) / 0.05f) : 1.0f;
    return FMath::Max(0.01f, Gc * Area * 2.0f * SizeEffect);
}

float FMatter::YieldPressure(const FSubstance& S, float Moisture, float Celsius)
{
    const float Factor = StrengthFactor(S, Moisture, Celsius, 0.0f, 0.0f);
    if (S.IsSoil())
    {
        return FMath::Max(200.0f, S.Cohesion * 2000.0f * Factor);
    }
    if (S.Form == EMatterForm::Grains)
    {
        return FMath::Max(100.0f, 5000.0f * Factor);
    }
    return FMath::Max(1000.0f, S.Compressive * 1.0e6f * Factor);
}

float FMatter::AirDryMoisture(const FSubstance& S, float Humidity)
{
    const float Air = FMath::Clamp(Humidity, 0.05f, 1.0f);
    if (S.Form == EMatterForm::Liquid || S.Form == EMatterForm::Living || S.bMetal)
    {
        return 0.0f;
    }
    if (S.IsSoil())
    {
        return S.PlasticLimit * FMath::Lerp(0.35f, 0.8f, Air);
    }
    if (S.bGrain || S.Form == EMatterForm::Fibres)
    {
        return FMath::Clamp(0.12f * Air / 0.65f, 0.04f, 0.26f);
    }
    if (S.Form == EMatterForm::Grains)
    {
        return FMath::Min(S.SaturatedMoisture(), 0.01f + 0.04f * Air);
    }
    return S.SaturatedMoisture() * 0.08f * Air;
}

float FMatter::IgnitionMoistureLimit(const FSubstance& S)
{
    if (S.Kind == EResourceKind::Peat)
    {
        return 0.4f;
    }
    if (S.bGrain || S.Form == EMatterForm::Fibres)
    {
        return 0.25f;
    }
    return 0.3f;
}

float FMatter::Compliance(const FSubstance& S, float Moisture, float Celsius)
{
    const EMatterPhase Phase = PhaseOf(S, Moisture, Celsius, false, 0.0f, 0.0f);
    float Soft = 1.0f / FMath::Max(0.0005f, S.Young);
    if (Phase == EMatterPhase::Plastic || Phase == EMatterPhase::Slurry)
    {
        Soft *= 50.0f;
    }
    return Soft;
}

float FMatter::SaturationVapourPressure(float Celsius)
{
    if (Celsius >= 100.0f)
    {
        return 101.325f;
    }
    if (Celsius >= 0.0f)
    {
        return 0.6108f * FMath::Exp(17.27f * Celsius / (Celsius + 237.3f));
    }
    return 0.6108f * FMath::Exp(21.87f * Celsius / (Celsius + 265.5f));
}

float FMatter::Evaporation(float SurfaceCelsius, float AirCelsius, float Humidity, float Wind, float SunWatts)
{
    const float Surface = SaturationVapourPressure(SurfaceCelsius);
    const float Air = FMath::Clamp(Humidity, 0.0f, 1.0f) * SaturationVapourPressure(AirCelsius);
    const float Deficit = FMath::Max(0.0f, Surface - Air);
    const float ByAir = 3.0e-5f * (1.0f + 0.54f * FMath::Max(0.0f, Wind)) * Deficit;
    const float BySun = 0.5f * FMath::Max(0.0f, SunWatts) / LatentBoil;
    return ByAir + BySun * (Deficit > 0.01f ? 1.0f : 0.2f);
}

float FMatter::ConvectiveCoefficient(float Wind)
{
    return 5.7f + 3.8f * FMath::Max(0.0f, Wind);
}

float FMatter::WaterLimit(const FSubstance& S)
{
    if (S.Form == EMatterForm::Liquid)
    {
        return 0.0f;
    }
    if (S.IsSoil())
    {
        return S.LiquidLimit * 1.3f;
    }
    const float Full = S.SaturatedMoisture();
    return Full > 0.0f ? Full : 0.003f;
}

float FMatter::WetFraction(const FSubstance& S, float Moisture)
{
    if (S.IsSoil())
    {
        return FMath::Clamp(Moisture / FMath::Max(0.01f, S.LiquidLimit), 0.0f, 1.0f);
    }
    const float Full = S.SaturatedMoisture();
    if (Full > 0.001f)
    {
        return FMath::Clamp(Moisture / Full, 0.0f, 1.0f);
    }
    return FMath::Clamp(Moisture * 400.0f, 0.0f, 1.0f);
}

float FMatter::FlameTemperature(const FSubstance& S, float Moisture)
{
    const float Dryness = FMath::Clamp(1.0f - Moisture / 0.3f, 0.0f, 1.0f);
    const float Hot = S.Heat > 25.0f ? 1050.0f : 850.0f;
    return Hot * (0.7f + 0.3f * Dryness);
}

FMatterStep FMatter::Step(const FSubstance& S, FMatterBody& B, float AreaM2, float TopM2, const FMatterAir& Air, float Seconds)
{
    FMatterStep Out;
    if (Seconds <= 0.0f)
    {
        return Out;
    }

    const float Area = FMath::Max(AreaM2, 1.0e-4f);
    const float Top = FMath::Max(TopM2, 1.0e-4f);
    const float Dry = FMath::Max(0.001f, B.DryMass);
    const float Full = S.SaturatedMoisture();
    const float Limit = WaterLimit(S);
    const bool bLiquid = S.Form == EMatterForm::Liquid;
    const float WasTemperature = B.Temperature;

    if (!bLiquid)
    {
        if (Air.Rain > 0.0f)
        {
            const float Soak = FMath::Clamp(0.3f + 0.1f * (FMath::LogX(10.0f, FMath::Max(S.Permeability, 1.0e-12f)) + 9.0f), 0.05f, 0.9f);
            B.Moisture = FMath::Min(Limit, B.Moisture + Air.Rain * Top * Seconds / 3600.0f * Soak / Dry);
        }
        if (Air.bInWater)
        {
            const float Uptake = FMath::Clamp(FMath::Max(S.Permeability, 1.0e-10f) * 2.0e5f, 0.0005f, 0.05f);
            B.Moisture = FMath::Min(Limit, B.Moisture + (Limit - B.Moisture) * FMath::Min(1.0f, Uptake * Seconds / 60.0f));
        }
    }
    if (Air.Snow > 0.0f)
    {
        B.Snow = FMath::Min(1.0f, B.Snow + Air.Snow * Seconds / 3600.0f / 15.0f);
    }

    float Evaporating = 0.0f;
    if (!bLiquid && B.Moisture > 0.0f && B.Ice < 0.99f && !Air.bInWater)
    {
        float Open = 1.0f;
        if (S.IsSoil())
        {
            Open = FMath::Clamp(B.Moisture / FMath::Max(0.01f, S.PlasticLimit), 0.05f, 1.0f);
        }
        else if (Full > 0.0f)
        {
            Open = FMath::Clamp(B.Moisture / Full * 3.0f, 0.02f, 1.0f);
        }
        const float Rate = Evaporation(B.Temperature, Air.Celsius, Air.Humidity, Air.Wind, Air.Sun) * Area * Open * (1.0f - B.Ice);
        const float Lost = FMath::Min(Rate * Seconds, B.Moisture * Dry);
        B.Moisture -= Lost / Dry;
        Evaporating = Lost / Seconds;
    }

    const float Water = FMath::Max(0.0f, B.Moisture) * Dry;
    const float Capacity = FMath::Max(1.0f, S.HeatCapacity * Dry + Water * (WaterHeatCapacity * (1.0f - B.Ice) + 2090.0f * B.Ice));

    if (B.bBurning)
    {
        const float Dryness = FMath::Clamp(1.0f - B.Moisture / 0.3f, 0.0f, 1.0f);
        const float Rate = 0.012f * Area * (0.25f + 0.75f * Dryness);
        const float Burned = FMath::Min(Rate * Seconds, FMath::Max(0.0f, B.DryMass - B.OriginalMass * 0.03f));
        B.DryMass -= Burned;
        Out.Released = Burned * S.Heat * 1.0e6f / Seconds;
        B.Char = FMath::Clamp((1.0f - B.DryMass / FMath::Max(0.001f, B.OriginalMass)) * 1.4f + 0.15f, 0.0f, 1.0f);
        B.Temperature = FMath::Lerp(B.Temperature, FlameTemperature(S, B.Moisture), 1.0f - FMath::Exp(-Seconds / 20.0f));

        if (Air.Rain > 6.0f || B.Moisture > 0.45f || Air.bInWater)
        {
            B.bBurning = false;
            B.Temperature = FMath::Min(B.Temperature, 120.0f);
            Out.bExtinguished = true;
        }
        else if (B.DryMass <= B.OriginalMass * 0.04f)
        {
            B.bBurning = false;
            Out.bBurnedOut = true;
            return Out;
        }
    }
    else
    {
        const float Exchange = (ConvectiveCoefficient(Air.Wind) + 4.0f) * Area * (Air.bInWater ? 20.0f : 1.0f);
        const float Absorb = S.bMetal ? 0.4f : 0.75f;
        const float Gain = Absorb * Air.Sun * Top + 0.35f * Air.Radiant * Area - Evaporating * LatentBoil;
        const float Settle = Air.bInWater ? FMath::Max(Air.Celsius, 0.5f) : Air.Celsius;
        const float Equilibrium = Settle + Gain / Exchange;
        const float Tau = Capacity / Exchange;
        B.Temperature = Equilibrium + (B.Temperature - Equilibrium) * FMath::Exp(-Seconds / FMath::Max(1.0f, Tau));
    }
    B.Temperature = FMath::Clamp(B.Temperature, -60.0f, 3000.0f);

    if (!bLiquid && Water > 1.0e-5f)
    {
        if (B.Temperature < 0.0f && B.Ice < 1.0f)
        {
            const float Frozen = FMath::Min(-B.Temperature * Capacity / LatentMelt, Water * (1.0f - B.Ice));
            B.Ice += Frozen / Water;
            B.Temperature += Frozen * LatentMelt / Capacity;
            if (S.bBrittle && Full > 0.0f && B.Moisture > Full * 0.8f && WasTemperature >= 0.0f)
            {
                B.Damage = FMath::Min(1.0f, B.Damage + 0.004f);
            }
        }
        else if (B.Temperature > 0.0f && B.Ice > 0.0f)
        {
            const float Melted = FMath::Min(B.Temperature * Capacity / LatentMelt, Water * B.Ice);
            B.Ice -= Melted / Water;
            B.Temperature -= Melted * LatentMelt / Capacity;
        }
        B.Ice = FMath::Clamp(B.Ice, 0.0f, 1.0f);
    }
    else if (!bLiquid)
    {
        B.Ice = 0.0f;
    }

    if (B.Snow > 0.0f && (B.Temperature > 0.5f || Air.Celsius > 1.0f))
    {
        B.Snow = FMath::Max(0.0f, B.Snow - Seconds / 3600.0f * FMath::Max(B.Temperature, Air.Celsius) * 0.08f);
    }

    if (!B.bBurning && !Out.bExtinguished && S.Burns() && B.Temperature >= S.Ignites
        && B.Moisture < IgnitionMoistureLimit(S) && !Air.bInWater)
    {
        B.bBurning = true;
        B.Temperature = FMath::Max(B.Temperature, S.Ignites + 50.0f);
        Out.bIgnited = true;
    }

    if (S.FiresInto != EResourceKind::None && B.Temperature >= S.FiredAt)
    {
        Out.bFired = true;
    }

    if (S.MeltsInto != EResourceKind::None)
    {
        const float Beyond = bLiquid ? S.Melts - B.Temperature : B.Temperature - S.Melts;
        if (Beyond > 0.0f)
        {
            const float Changed = FMath::Min(Beyond * Capacity / LatentMelt, Dry * (1.0f - B.Transition));
            B.Transition += Changed / Dry;
            B.Temperature += (bLiquid ? 1.0f : -1.0f) * Changed * LatentMelt / Capacity;
            if (B.Transition >= 0.999f)
            {
                B.Transition = 0.0f;
                Out.bTransformed = true;
            }
        }
        else
        {
            B.Transition = FMath::Max(0.0f, B.Transition - Seconds / 600.0f);
        }
    }

    if (S.bSpoils && B.Temperature > -1.0f)
    {
        const float Days = 5.0f * FMath::Pow(2.0f, (20.0f - B.Temperature) / 10.0f);
        const float Before = B.Rot;
        B.Rot = FMath::Min(1.0f, B.Rot + Seconds / (Days * 86400.0f));
        Out.bSpoiled = Before < 0.5f && B.Rot >= 0.5f;
    }
    else if (S.bRots && B.Moisture > 0.25f && B.Temperature > 3.0f && B.Temperature < 40.0f)
    {
        B.Rot = FMath::Min(1.0f, B.Rot + Seconds / (2.0f * 365.0f * 86400.0f));
    }

    if (S.bRusts && WetFraction(S, B.Moisture) > 0.3f)
    {
        B.Damage = FMath::Min(0.99f, B.Damage + Seconds / (10.0f * 365.0f * 86400.0f));
    }

    if (S.bDissolves && B.Moisture > 0.02f)
    {
        const float Gone = FMath::Min(B.DryMass, 0.0004f * Area * Seconds * FMath::Clamp(B.Moisture * 20.0f, 0.0f, 1.0f));
        B.DryMass -= Gone;
        Out.bDissolved = B.DryMass <= B.OriginalMass * 0.05f;
    }

    return Out;
}

FString FMatter::PhaseWord(const FSubstance& S, EMatterPhase Phase)
{
    switch (Phase)
    {
    case EMatterPhase::Dry:     return TEXT("сухо");
    case EMatterPhase::Damp:    return TEXT("сыро");
    case EMatterPhase::Plastic: return TEXT("мнётся, как тесто");
    case EMatterPhase::Slurry:  return S.IsSoil() ? TEXT("расползается грязью") : TEXT("плывёт");
    case EMatterPhase::Liquid:  return TEXT("течёт");
    case EMatterPhase::Frozen:  return TEXT("скованно морозом");
    case EMatterPhase::Burning: return TEXT("горит");
    case EMatterPhase::Charred: return TEXT("в угле");
    case EMatterPhase::Molten:  return TEXT("плавится");
    case EMatterPhase::Rotten:  return TEXT("в гнили");
    default:                    return TEXT("");
    }
}

FString FMatter::Describe(const FSubstance& S, float Moisture, float Celsius, float Damage, float Char, float Rot, bool bBurning)
{
    const EMatterPhase Phase = PhaseOf(S, Moisture, Celsius, bBurning, Char, Rot);
    FString Out = FString::Printf(TEXT("%s: %s; влага %.0f%%; %.0f °C"), S.Name, *PhaseWord(S, Phase), Moisture * 100.0f, Celsius);
    if (Damage > 0.02f)
    {
        Out += FString::Printf(TEXT("; трещины %.0f%%"), Damage * 100.0f);
    }
    if (Char > 0.02f)
    {
        Out += FString::Printf(TEXT("; обуглено %.0f%%"), Char * 100.0f);
    }
    if (Rot > 0.02f)
    {
        Out += FString::Printf(TEXT("; гниль %.0f%%"), Rot * 100.0f);
    }
    Out += FString::Printf(TEXT("; трение %.2f; прочность %.0f%%"),
        FrictionOf(S, Moisture, Celsius, false), StrengthFactor(S, Moisture, Celsius, Rot, Char) * 100.0f);
    return Out;
}
