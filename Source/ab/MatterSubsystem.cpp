#include "MatterSubsystem.h"
#include "MatterComponent.h"
#include "MatterStructure.h"
#include "TerrainGrid.h"
#include "HumanWorldSubsystem.h"
#include "Engine/World.h"
#include "Engine/Texture.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Components/PrimitiveComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/DateTime.h"

namespace
{
    struct FLookPreset
    {
        const TCHAR* Albedo;
        const TCHAR* Bumps;
        FLinearColor Tint;
        float Tile;
        float Rough;
        float Porous;
        float Metal;
        float Bump;
    };

    const FLookPreset& PresetOf(EMatterLook Look)
    {
        static const FLookPreset Presets[] =
        {
            { TEXT("T_Wood_Oak_D"), TEXT("T_Wood_Oak_N"), FLinearColor(0.82f, 0.72f, 0.6f), 90.0f, 0.75f, 0.6f, 0.0f, 0.8f },
            { TEXT("T_Wood_Pine_D"), TEXT("T_Wood_Pine_N"), FLinearColor(1.0f, 0.95f, 0.88f), 90.0f, 0.7f, 0.6f, 0.0f, 0.7f },
            { TEXT("T_Wood_Walnut_D"), TEXT("T_Wood_Walnut_N"), FLinearColor(1.0f, 1.0f, 1.0f), 80.0f, 0.7f, 0.6f, 0.0f, 0.8f },
            { TEXT("T_Wood_Walnut_D"), TEXT("T_Wood_Walnut_N"), FLinearColor(0.55f, 0.45f, 0.38f), 35.0f, 0.92f, 0.7f, 0.0f, 1.5f },
            { TEXT("T_Wood_Pine_D"), TEXT("T_Wood_Pine_N"), FLinearColor(1.35f, 1.12f, 0.55f), 30.0f, 0.88f, 0.8f, 0.0f, 1.0f },
            { TEXT("T_Rock_Basalt_D"), TEXT("T_Rock_Basalt_N"), FLinearColor(0.9f, 0.9f, 0.9f), 110.0f, 0.8f, 0.3f, 0.0f, 1.0f },
            { TEXT("T_Rock_Smooth_Granite_D"), TEXT("T_Rock_Basalt_N"), FLinearColor(1.0f, 1.0f, 1.0f), 100.0f, 0.7f, 0.15f, 0.0f, 0.5f },
            { TEXT("T_Rock_Sandstone_D"), TEXT("T_Rock_Sandstone_N"), FLinearColor(1.05f, 1.05f, 1.0f), 120.0f, 0.85f, 0.5f, 0.0f, 0.8f },
            { TEXT("T_Rock_Sandstone_D"), TEXT("T_Rock_Sandstone_N"), FLinearColor(1.0f, 1.0f, 1.0f), 120.0f, 0.85f, 0.6f, 0.0f, 1.0f },
            { TEXT("T_Concrete_Poured_D"), TEXT("T_Concrete_Poured_N"), FLinearColor(1.3f, 1.3f, 1.25f), 80.0f, 0.95f, 0.8f, 0.0f, 0.4f },
            { TEXT("T_Rock_Basalt_D"), TEXT("T_Rock_Basalt_N"), FLinearColor(0.55f, 0.5f, 0.45f), 60.0f, 0.4f, 0.05f, 0.0f, 0.6f },
            { TEXT("T_Concrete_Grime_D"), TEXT("T_Concrete_Poured_N"), FLinearColor(0.85f, 0.55f, 0.38f), 70.0f, 0.9f, 0.9f, 0.0f, 0.4f },
            { TEXT("T_Ground_Gravel_D"), TEXT("T_Ground_Gravel_N"), FLinearColor(0.55f, 0.45f, 0.35f), 90.0f, 0.95f, 0.9f, 0.0f, 0.8f },
            { TEXT("T_Rock_Sandstone_D"), TEXT("T_Rock_Sandstone_N"), FLinearColor(1.15f, 1.05f, 0.8f), 60.0f, 0.95f, 0.7f, 0.0f, 0.5f },
            { TEXT("T_ground_Moss_D"), TEXT("T_Ground_Moss_N"), FLinearColor(0.4f, 0.3f, 0.22f), 80.0f, 0.95f, 0.9f, 0.0f, 0.8f },
            { TEXT("T_Rock_Basalt_D"), TEXT("T_Rock_Basalt_N"), FLinearColor(0.15f, 0.15f, 0.16f), 60.0f, 0.5f, 0.2f, 0.0f, 1.0f },
            { TEXT("T_Concrete_Poured_D"), TEXT("T_Concrete_Poured_N"), FLinearColor(0.55f, 0.55f, 0.55f), 60.0f, 1.0f, 0.9f, 0.0f, 0.2f },
            { TEXT("T_Brick_Clay_New_D"), TEXT("T_Brick_Clay_New_N"), FLinearColor(1.0f, 1.0f, 1.0f), 100.0f, 0.85f, 0.6f, 0.0f, 1.0f },
            { TEXT("T_Concrete_Grime_D"), TEXT("T_Concrete_Poured_N"), FLinearColor(0.95f, 0.55f, 0.35f), 60.0f, 0.7f, 0.4f, 0.0f, 0.2f },
            { TEXT("T_Concrete_Poured_D"), TEXT("T_Concrete_Poured_N"), FLinearColor(0.1f, 0.12f, 0.13f), 100.0f, 0.05f, 0.0f, 0.0f, 0.0f },
            { TEXT("T_Metal_Steel_D"), TEXT("T_Metal_Steel_N"), FLinearColor(0.45f, 0.45f, 0.47f), 60.0f, 0.55f, 0.0f, 1.0f, 0.5f },
            { TEXT("T_Metal_Steel_D"), TEXT("T_Metal_Steel_N"), FLinearColor(1.0f, 1.0f, 1.0f), 60.0f, 0.35f, 0.0f, 1.0f, 0.4f },
            { TEXT("T_Metal_Copper_D"), TEXT("T_Metal_Steel_N"), FLinearColor(1.0f, 1.0f, 1.0f), 60.0f, 0.35f, 0.0f, 1.0f, 0.3f },
            { TEXT("T_Metal_Gold_D"), TEXT("T_Metal_Gold_N"), FLinearColor(0.8f, 0.55f, 0.35f), 60.0f, 0.4f, 0.0f, 1.0f, 0.4f },
            { TEXT("T_Metal_Aluminum_D"), TEXT("T_Metal_Steel_N"), FLinearColor(0.85f, 0.85f, 0.85f), 60.0f, 0.4f, 0.0f, 1.0f, 0.3f },
            { TEXT("T_Metal_Aluminum_D"), TEXT("T_Metal_Steel_N"), FLinearColor(0.45f, 0.47f, 0.5f), 60.0f, 0.6f, 0.0f, 1.0f, 0.3f },
            { TEXT("T_Metal_Aluminum_D"), TEXT("T_Metal_Steel_N"), FLinearColor(1.0f, 1.0f, 1.0f), 60.0f, 0.2f, 0.0f, 1.0f, 0.2f },
            { TEXT("T_Metal_Gold_D"), TEXT("T_Metal_Gold_N"), FLinearColor(1.0f, 1.0f, 1.0f), 60.0f, 0.25f, 0.0f, 1.0f, 0.3f },
            { TEXT("T_Metal_Rust_D"), TEXT("T_Metal_Rust_N"), FLinearColor(1.0f, 1.0f, 1.0f), 50.0f, 0.9f, 0.2f, 0.0f, 1.0f },
            { TEXT("T_Concrete_Poured_D"), TEXT("T_Water_N"), FLinearColor(0.03f, 0.07f, 0.08f), 300.0f, 0.03f, 0.0f, 0.0f, 0.35f },
            { TEXT("T_Concrete_Poured_D"), TEXT("T_Water_N"), FLinearColor(0.6f, 0.75f, 0.85f), 150.0f, 0.1f, 0.0f, 0.0f, 0.3f },
            { TEXT("T_Concrete_Poured_D"), TEXT("T_Concrete_Poured_N"), FLinearColor(1.4f, 1.45f, 1.5f), 100.0f, 0.6f, 0.3f, 0.0f, 0.2f },
            { TEXT("T_Concrete_Poured_D"), TEXT("T_Concrete_Poured_N"), FLinearColor(1.2f, 1.15f, 1.0f), 40.0f, 0.6f, 0.3f, 0.0f, 0.3f },
            { TEXT("T_Concrete_Grime_D"), TEXT("T_Concrete_Poured_N"), FLinearColor(0.55f, 0.35f, 0.22f), 30.0f, 0.6f, 0.5f, 0.0f, 0.3f },
            { TEXT("T_Concrete_Poured_D"), TEXT("T_Concrete_Poured_N"), FLinearColor(0.85f, 0.82f, 0.75f), 20.0f, 0.95f, 0.9f, 0.0f, 0.3f },
            { TEXT("T_Wood_Pine_D"), TEXT("T_Wood_Pine_N"), FLinearColor(0.75f, 0.6f, 0.4f), 15.0f, 0.9f, 0.8f, 0.0f, 0.8f },
            { TEXT("T_Concrete_Poured_D"), TEXT("T_Concrete_Poured_N"), FLinearColor(1.1f, 0.85f, 0.35f), 40.0f, 0.35f, 0.0f, 0.0f, 0.1f },
            { TEXT("T_Concrete_Grime_D"), TEXT("T_Concrete_Poured_N"), FLinearColor(1.0f, 0.75f, 0.45f), 30.0f, 0.8f, 0.5f, 0.0f, 0.3f },
            { TEXT("T_Ground_Grass_D"), TEXT("T_Ground_Grass_N"), FLinearColor(1.0f, 1.0f, 1.0f), 30.0f, 0.7f, 0.3f, 0.0f, 0.5f },
            { TEXT("T_Concrete_Poured_D"), TEXT("T_Concrete_Poured_N"), FLinearColor(1.3f, 1.28f, 1.2f), 40.0f, 0.9f, 0.9f, 0.0f, 0.05f },
            { TEXT("T_Rock_Marble_Polished_D"), TEXT("T_Rock_Sandstone_N"), FLinearColor(1.1f, 1.1f, 1.1f), 40.0f, 0.5f, 0.1f, 0.0f, 0.2f },
            { TEXT("T_Concrete_Poured_D"), TEXT("T_Concrete_Poured_N"), FLinearColor(1.25f, 1.22f, 1.15f), 80.0f, 0.9f, 0.7f, 0.0f, 0.3f },
            { TEXT("T_Ground_Grass_D"), TEXT("T_Ground_Grass_N"), FLinearColor(0.95f, 1.0f, 0.9f), 400.0f, 0.9f, 0.5f, 0.0f, 0.6f },
            { TEXT("T_Ground_Gravel_D"), TEXT("T_Ground_Gravel_N"), FLinearColor(1.0f, 1.0f, 1.0f), 200.0f, 0.9f, 0.5f, 0.0f, 0.8f },
            { TEXT("T_Concrete_Poured_D"), TEXT("T_Concrete_Poured_N"), FLinearColor(0.28f, 0.28f, 0.29f), 300.0f, 0.85f, 0.3f, 0.0f, 0.5f },
            { TEXT("T_CobbleStone_Smooth_D"), TEXT("T_CobbleStone_Smooth_N"), FLinearColor(1.0f, 1.0f, 1.0f), 200.0f, 0.8f, 0.3f, 0.0f, 1.0f },
            { TEXT("T_Brick_Clay_Old_D"), TEXT("T_Brick_Clay_Old_N"), FLinearColor(1.0f, 1.0f, 1.0f), 150.0f, 0.85f, 0.6f, 0.0f, 1.0f },
            { TEXT("T_Brick_Cut_Stone_D"), TEXT("T_Brick_Cut_Stone_N"), FLinearColor(1.0f, 1.0f, 1.0f), 200.0f, 0.8f, 0.4f, 0.0f, 1.0f },
            { TEXT("T_Concrete_Panels_D"), TEXT("T_Concrete_Panels_N"), FLinearColor(1.0f, 1.0f, 1.0f), 300.0f, 0.8f, 0.4f, 0.0f, 0.8f },
            { TEXT("T_Concrete_Tiles_D"), TEXT("T_Concrete_Tiles_N"), FLinearColor(1.0f, 1.0f, 1.0f), 150.0f, 0.6f, 0.2f, 0.0f, 0.8f },
            { TEXT("T_Concrete_Grime_D"), TEXT("T_Concrete_Poured_N"), FLinearColor(1.0f, 1.0f, 1.0f), 250.0f, 0.9f, 0.5f, 0.0f, 0.5f },
            { TEXT("T_Rock_Slate_D"), TEXT("T_Rock_Slate_N"), FLinearColor(1.0f, 1.0f, 1.0f), 150.0f, 0.75f, 0.2f, 0.0f, 1.0f },
            { TEXT("T_ground_Moss_D"), TEXT("T_Ground_Moss_N"), FLinearColor(1.0f, 1.0f, 1.0f), 150.0f, 0.9f, 0.6f, 0.0f, 0.8f }
        };
        static_assert(UE_ARRAY_COUNT(Presets) == static_cast<int32>(EMatterLook::Count), "each look needs a preset");
        const int32 Index = FMath::Clamp(static_cast<int32>(Look), 0, static_cast<int32>(UE_ARRAY_COUNT(Presets)) - 1);
        return Presets[Index];
    }

    UTexture* TextureNamed(const TCHAR* Name)
    {
        return LoadObject<UTexture>(nullptr, *FString::Printf(TEXT("/Game/StarterContent/Textures/%s.%s"), Name, Name));
    }

    float Gauss(FRandomStream& Stream)
    {
        const float U1 = FMath::Max(Stream.FRand(), 1.0e-6f);
        const float U2 = Stream.FRand();
        return FMath::Sqrt(-2.0f * FMath::Loge(U1)) * FMath::Cos(UE_TWO_PI * U2);
    }

    void Wander(float& Value, float Mean, float TauHours, float Sigma, float Hours, FRandomStream& Stream)
    {
        const float Keep = FMath::Exp(-Hours / FMath::Max(0.01f, TauHours));
        Value = Mean + (Value - Mean) * Keep + Sigma * FMath::Sqrt(FMath::Max(0.0f, 1.0f - Keep * Keep)) * Gauss(Stream);
    }

    void DateOf(int32 DayOfYear, int32& OutMonth, int32& OutDay)
    {
        static const int32 Lengths[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
        int32 Left = FMath::Clamp(DayOfYear, 0, 364);
        int32 Month = 0;
        while (Month < 11 && Left >= Lengths[Month])
        {
            Left -= Lengths[Month];
            ++Month;
        }
        OutMonth = Month;
        OutDay = Left + 1;
    }

    const TCHAR* MonthName(int32 Month)
    {
        static const TCHAR* Names[] = { TEXT("января"), TEXT("февраля"), TEXT("марта"), TEXT("апреля"), TEXT("мая"),
            TEXT("июня"), TEXT("июля"), TEXT("августа"), TEXT("сентября"), TEXT("октября"), TEXT("ноября"), TEXT("декабря") };
        return Names[FMath::Clamp(Month, 0, 11)];
    }

    constexpr float Latitude = 55.75f;
}

void UMatterSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Collection.InitializeDependency<UHumanWorldSubsystem>();
    Super::Initialize(Collection);

    Chance.Initialize(static_cast<int32>(FDateTime::Now().GetTicks() % 2147483629));
    int32 Season = FParse::Param(FCommandLine::Get(), TEXT("Village")) ? 215 : 130;
    FParse::Value(FCommandLine::Get(), TEXT("Season="), Season);
    StartDayOfYear = FMath::Clamp(Season, 0, 364);
    CloudDrift = 0.45f + Chance.FRandRange(-0.25f, 0.25f);
    Anomaly = Gauss(Chance) * 2.0f;
    WindDrift = 2.5f + Chance.FRandRange(0.0f, 2.0f);

    MatterBase = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Matter/M_Matter.M_Matter"));
    GroundBase = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Matter/M_Ground.M_Ground"));
    if (!MatterBase || !GroundBase)
    {
        UE_LOG(LogHumanCity, Error, TEXT("Вещество: нет материалов в /Game/Matter. Их строит Tools/MakeMatterMaterials.py"));
    }

    AdvanceClimate(0.0f);
}

void UMatterSubsystem::Deinitialize()
{
    Items.Reset();
    Heat.Reset();
    HeatBuilding.Reset();
    Looks.Reset();
    Surfaces.Reset();
    Ground = nullptr;
    Super::Deinitialize();
}

bool UMatterSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
    return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UMatterSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UMatterSubsystem, STATGROUP_Tickables);
}

UMatterSubsystem* UMatterSubsystem::Get(const UObject* WorldContext)
{
    const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
    return World ? World->GetSubsystem<UMatterSubsystem>() : nullptr;
}

float UMatterSubsystem::GameSeconds(float RealDelta) const
{
    const UWorld* World = GetWorld();
    const UHumanWorldSubsystem* WorldMind = World ? World->GetSubsystem<UHumanWorldSubsystem>() : nullptr;
    return WorldMind ? WorldMind->RealToGameSeconds(RealDelta) : RealDelta * 60.0f;
}

float UMatterSubsystem::WorldTime() const
{
    const UWorld* World = GetWorld();
    const UHumanWorldSubsystem* WorldMind = World ? World->GetSubsystem<UHumanWorldSubsystem>() : nullptr;
    return WorldMind ? WorldMind->WorldSeconds : 0.0f;
}

void UMatterSubsystem::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    const float Seconds = GameSeconds(DeltaTime);

    ClimateTimer += DeltaTime;
    ClimateSeconds += Seconds;
    if (ClimateTimer >= 0.25f)
    {
        AdvanceClimate(ClimateSeconds);
        ClimateTimer = 0.0f;
        ClimateSeconds = 0.0f;
        PushWeatherToPeople();
        if (Ground)
        {
            Ground->SetScalarParameterValue(TEXT("Frost"), FMath::Clamp(-Weather.Celsius / 6.0f, 0.0f, 1.0f) * (1.0f - Weather.Cloud * 0.5f));
        }
    }

    MatterTimer += DeltaTime;
    MatterSeconds += Seconds;
    if (MatterTimer >= 0.5f)
    {
        AdvanceMatter(MatterSeconds);
        MatterTimer = 0.0f;
        MatterSeconds = 0.0f;
    }
}

void UMatterSubsystem::AdvanceClimate(float Seconds)
{
    const UWorld* World = GetWorld();
    const UHumanWorldSubsystem* WorldMind = World ? World->GetSubsystem<UHumanWorldSubsystem>() : nullptr;
    const float Now = WorldMind ? WorldMind->WorldSeconds : 0.0f;
    const int32 Day = WorldMind ? WorldMind->Now.Day : 1;
    const float Hour = WorldMind ? WorldMind->Now.HourFloat : 12.0f;
    const float Hours = FMath::Max(0.0f, Seconds) / 3600.0f;

    Weather.Hour = Hour;
    Weather.DayOfYear = (StartDayOfYear + Day - 1) % 365;
    const float Year = UE_TWO_PI / 365.0f;

    Wander(CloudDrift, 0.55f, 10.0f, 0.33f, Hours, Chance);
    Wander(Anomaly, 0.0f, 96.0f, 3.5f, Hours, Chance);
    Wander(WindDrift, 3.0f, 6.0f, 2.0f, Hours, Chance);

    Weather.Cloud = FMath::Clamp(CloudDrift, 0.0f, 1.0f);
    float Precipitation = Weather.Cloud > 0.72f ? 12.0f * FMath::Square((Weather.Cloud - 0.72f) / 0.28f) : 0.0f;
    if (Now < ForcedRainUntil)
    {
        Precipitation = ForcedRain;
        Weather.Cloud = FMath::Max(Weather.Cloud, ForcedRain > 0.0f ? 0.85f : 0.0f);
        if (ForcedRain <= 0.0f)
        {
            Weather.Cloud = FMath::Min(Weather.Cloud, 0.3f);
        }
    }

    Weather.SeasonMean = 5.8f + 12.9f * FMath::Sin(Year * (Weather.DayOfYear - 110));
    const float Swing = (3.5f + 1.5f * FMath::Sin(Year * (Weather.DayOfYear - 80))) * (1.0f - 0.6f * Weather.Cloud);
    float Celsius = Weather.SeasonMean + Swing * FMath::Cos(UE_TWO_PI * (Hour - 15.0f) / 24.0f) + Anomaly;
    if (Now < ForcedCelsiusUntil)
    {
        Celsius = ForcedCelsius;
    }
    Weather.Celsius = Celsius;

    const float SnowShare = FMath::Clamp((2.0f - Celsius) / 1.5f, 0.0f, 1.0f);
    Weather.Snow = Precipitation * SnowShare;
    Weather.Rain = Precipitation * (1.0f - SnowShare);

    const float WantHumidity = FMath::Clamp(0.5f + 0.35f * Weather.Cloud + (Precipitation > 0.0f ? 0.12f : 0.0f)
        - 0.015f * FMath::Max(0.0f, Celsius - Weather.SeasonMean), 0.25f, 1.0f);
    Weather.Humidity = FMath::Lerp(WantHumidity, Weather.Humidity, FMath::Exp(-Hours));
    Weather.Wind = FMath::Clamp(WindDrift + (Precipitation > 0.0f ? 2.0f : 0.0f), 0.0f, 20.0f);

    const float Declination = FMath::DegreesToRadians(-23.44f * FMath::Cos(Year * (Weather.DayOfYear + 10)));
    const float Lat = FMath::DegreesToRadians(Latitude);
    const float HourAngle = FMath::DegreesToRadians(15.0f * (Hour - 12.0f));
    const float SinAltitude = FMath::Sin(Lat) * FMath::Sin(Declination) + FMath::Cos(Lat) * FMath::Cos(Declination) * FMath::Cos(HourAngle);
    const float Altitude = FMath::Asin(FMath::Clamp(SinAltitude, -1.0f, 1.0f));
    const float CosAzimuth = (FMath::Sin(Declination) - SinAltitude * FMath::Sin(Lat))
        / FMath::Max(0.0001f, FMath::Cos(Altitude) * FMath::Cos(Lat));
    float Azimuth = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(CosAzimuth, -1.0f, 1.0f)));
    if (HourAngle > 0.0f)
    {
        Azimuth = 360.0f - Azimuth;
    }
    Weather.SunElevation = FMath::RadiansToDegrees(Altitude);
    Weather.SunAzimuth = Azimuth;
    Weather.Sun = SinAltitude > 0.01f
        ? 1098.0f * SinAltitude * FMath::Exp(-0.059f / SinAltitude) * (1.0f - 0.75f * FMath::Pow(Weather.Cloud, 3.4f))
        : 0.0f;

    Weather.SnowOnGround += Weather.Snow * Hours;
    if (Celsius > 0.0f && Weather.SnowOnGround > 0.0f)
    {
        const float Melt = 4.0f * Celsius * Hours / 24.0f + Weather.Rain * 0.12f * Hours + Weather.Sun * 3600.0f * Hours * 0.2f / FMatter::LatentMelt;
        Weather.SnowOnGround = FMath::Max(0.0f, Weather.SnowOnGround - Melt);
    }

    const float Drying = FMatter::Evaporation(Celsius, Celsius, Weather.Humidity, Weather.Wind, Weather.Sun) * 3600.0f;
    Weather.Wetness = FMath::Clamp(Weather.Wetness + (Weather.Rain * 0.35f - Drying) * Hours, 0.0f, 1.0f);
}

void UMatterSubsystem::PushWeatherToPeople() const
{
    UWorld* World = GetWorld();
    UHumanWorldSubsystem* WorldMind = World ? World->GetSubsystem<UHumanWorldSubsystem>() : nullptr;
    if (!WorldMind)
    {
        return;
    }
    const float Cold = FMath::Clamp((8.0f - Weather.Celsius) / 25.0f, 0.0f, 1.0f);
    const float Hot = FMath::Clamp((Weather.Celsius - 27.0f) / 12.0f, 0.0f, 1.0f);
    const float Wet = FMath::Clamp((Weather.Rain + Weather.Snow) / 6.0f, 0.0f, 1.0f);
    WorldMind->Weather = FMath::Clamp(0.15f * Weather.Cloud + 0.4f * Wet + 0.35f * Cold + 0.3f * Hot + Weather.Wind / 40.0f, 0.0f, 1.0f);
}

FMatterAir UMatterSubsystem::AirAt(const FVector& Where, bool bSheltered) const
{
    FMatterAir Air;
    Air.Celsius = Weather.Celsius - FMath::Max(0.0f, Where.Z) / 100.0f * 0.0065f;
    Air.Humidity = Weather.Humidity;
    Air.Wind = Weather.Wind;
    Air.Rain = Weather.Rain;
    Air.Snow = Weather.Snow;
    Air.Sun = Weather.Sun;
    if (bSheltered)
    {
        Air.Celsius = Weather.SeasonMean + (Air.Celsius - Weather.SeasonMean) * 0.4f + 2.0f;
        Air.Wind *= 0.15f;
        Air.Rain = 0.0f;
        Air.Snow = 0.0f;
        Air.Sun *= 0.05f;
        Air.Humidity = FMath::Min(Air.Humidity, 0.8f);
    }
    Air.Radiant = RadiantAt(Where);
    if (const ATerrainGrid* Ground0 = Terrain.Get())
    {
        Air.bInWater = Ground0->IsWaterAt(Where) && Where.Z < Ground0->HeightAt(Where) + 40.0f;
    }
    return Air;
}

bool UMatterSubsystem::IsSheltered(const FVector& Where, const AActor* Ignore) const
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return false;
    }
    FCollisionQueryParams Params(SCENE_QUERY_STAT(MatterShelter), false, Ignore);
    FHitResult Hit;
    const FVector From = Where + FVector(0.0f, 0.0f, 40.0f);
    return World->LineTraceSingleByChannel(Hit, From, From + FVector(0.0f, 0.0f, 3000.0f), ECC_Visibility, Params);
}

void UMatterSubsystem::ForceRain(float MillimetresPerHour, float GameHours)
{
    ForcedRain = FMath::Max(0.0f, MillimetresPerHour);
    ForcedRainUntil = WorldTime() + FMath::Max(0.0f, GameHours) * 3600.0f;
    AdvanceClimate(0.0f);
}

void UMatterSubsystem::ForceTemperature(float Celsius, float GameHours)
{
    ForcedCelsius = Celsius;
    ForcedCelsiusUntil = WorldTime() + FMath::Max(0.0f, GameHours) * 3600.0f;
    AdvanceClimate(0.0f);
}

void UMatterSubsystem::ForceSeason(int32 DayOfYear)
{
    const UWorld* World = GetWorld();
    const UHumanWorldSubsystem* WorldMind = World ? World->GetSubsystem<UHumanWorldSubsystem>() : nullptr;
    const int32 Day = WorldMind ? WorldMind->Now.Day : 1;
    StartDayOfYear = ((DayOfYear - (Day - 1)) % 365 + 365) % 365;
    AdvanceClimate(0.0f);
}

void UMatterSubsystem::Register(UMatterComponent* Item)
{
    if (Item)
    {
        Items.AddUnique(Item);
    }
}

void UMatterSubsystem::Unregister(UMatterComponent* Item)
{
    for (int32 i = Items.Num() - 1; i >= 0; --i)
    {
        if (!Items[i].IsValid() || Items[i].Get() == Item)
        {
            Items.RemoveAtSwap(i);
        }
    }
}

void UMatterSubsystem::RegisterStructure(AMatterStructure* Structure)
{
    if (Structure)
    {
        Structures.AddUnique(Structure);
    }
}

void UMatterSubsystem::UnregisterStructure(AMatterStructure* Structure)
{
    for (int32 i = Structures.Num() - 1; i >= 0; --i)
    {
        if (!Structures[i].IsValid() || Structures[i].Get() == Structure)
        {
            Structures.RemoveAtSwap(i);
        }
    }
}

void UMatterSubsystem::SetTerrain(ATerrainGrid* InTerrain)
{
    Terrain = InTerrain;
}

void UMatterSubsystem::AdvanceMatter(float Seconds)
{
    HeatBuilding.Reset();

    const TArray<TWeakObjectPtr<UMatterComponent>> Snapshot = Items;
    for (const TWeakObjectPtr<UMatterComponent>& Weak : Snapshot)
    {
        UMatterComponent* Item = Weak.Get();
        if (!Item || Item->bVisualOnly)
        {
            continue;
        }
        const AActor* Owner = Item->GetOwner();
        if (!Owner || Owner->IsActorBeingDestroyed())
        {
            continue;
        }
        Item->Advance(Seconds, AirAt(Owner->GetActorLocation(), Item->bSheltered));
    }

    const int32 Count = Items.Num();
    for (int32 Step = 0; Step < FMath::Min(Count, 24); ++Step)
    {
        ShelterIndex = (ShelterIndex + 1) % Count;
        if (UMatterComponent* Item = Items[ShelterIndex].Get())
        {
            Item->CheckShelter(this);
        }
    }

    const TArray<TWeakObjectPtr<AMatterStructure>> Houses = Structures;
    for (const TWeakObjectPtr<AMatterStructure>& Weak : Houses)
    {
        if (AMatterStructure* House = Weak.Get())
        {
            House->AdvanceMatter(Seconds, this);
        }
    }

    if (ATerrainGrid* Ground0 = Terrain.Get())
    {
        Ground0->AdvanceSoil(Seconds, Weather);
    }

    Swap(Heat, HeatBuilding);
}

void UMatterSubsystem::AddHeat(const FVector& Where, float Watts)
{
    if (Watts > 1.0f)
    {
        FHeat Entry;
        Entry.Where = Where;
        Entry.Watts = Watts;
        HeatBuilding.Add(Entry);
    }
}

float UMatterSubsystem::RadiantAt(const FVector& Where) const
{
    float Total = 0.0f;
    for (const FHeat& Source : Heat)
    {
        const float Centimetres = FVector::Dist(Source.Where, Where);
        const float Metres = FMath::Max(0.3f, Centimetres / 100.0f);
        if (Centimetres > 1.0f && Metres < 25.0f)
        {
            Total += Source.Watts / (4.0f * UE_PI * Metres * Metres);
        }
    }
    return Total;
}

UMaterialInterface* UMatterSubsystem::LookOf(EResourceKind Kind)
{
    const FSubstance& S = FMatter::Of(Kind);
    return Look(S.Look, S.Tint);
}

UMaterialInterface* UMatterSubsystem::Look(EMatterLook Kind, const FLinearColor& Tint)
{
    const FName Key(*FString::Printf(TEXT("L%d_%d_%d_%d"), static_cast<int32>(Kind),
        FMath::RoundToInt(Tint.R * 100.0f), FMath::RoundToInt(Tint.G * 100.0f), FMath::RoundToInt(Tint.B * 100.0f)));
    if (const TObjectPtr<UMaterialInstanceDynamic>* Found = Looks.Find(Key))
    {
        return Found->Get();
    }
    if (!MatterBase)
    {
        return nullptr;
    }

    const FLookPreset& Preset = PresetOf(Kind);
    UMaterialInstanceDynamic* Made = UMaterialInstanceDynamic::Create(MatterBase, this);
    if (!Made)
    {
        return nullptr;
    }
    if (UTexture* Albedo = TextureNamed(Preset.Albedo))
    {
        Made->SetTextureParameterValue(TEXT("Albedo"), Albedo);
    }
    if (UTexture* Bumps = TextureNamed(Preset.Bumps))
    {
        Made->SetTextureParameterValue(TEXT("Bumps"), Bumps);
    }
    const FLinearColor Colour(Preset.Tint.R * Tint.R, Preset.Tint.G * Tint.G, Preset.Tint.B * Tint.B, 1.0f);
    Made->SetVectorParameterValue(TEXT("Tint"), Colour);
    Made->SetScalarParameterValue(TEXT("Tile"), Preset.Tile);
    Made->SetScalarParameterValue(TEXT("Rough"), Preset.Rough);
    Made->SetScalarParameterValue(TEXT("Porous"), Preset.Porous);
    Made->SetScalarParameterValue(TEXT("Metal"), Preset.Metal);
    Made->SetScalarParameterValue(TEXT("Bump"), Preset.Bump);
    Looks.Add(Key, Made);
    return Made;
}

UMaterialInterface* UMatterSubsystem::GroundMaterial()
{
    if (!Ground && GroundBase)
    {
        Ground = UMaterialInstanceDynamic::Create(GroundBase, this);
    }
    return Ground;
}

UPhysicalMaterial* UMatterSubsystem::SurfaceOf(EResourceKind Kind, float Moisture, float Celsius)
{
    const FSubstance& S = FMatter::Of(Kind);
    const float Full = FMath::Max(S.SaturatedMoisture(), S.LiquidLimit);
    const int32 Wet = Full > 0.0f
        ? FMath::Clamp(FMath::FloorToInt(Moisture / Full * 4.0f), 0, 5)
        : (Moisture > 0.005f ? 2 : 0);
    const bool bFrozen = Celsius < -0.5f && Moisture > 0.02f;
    const int32 Key = (static_cast<int32>(Kind) << 8) | (Wet << 1) | (bFrozen ? 1 : 0);

    if (const TObjectPtr<UPhysicalMaterial>* Found = Surfaces.Find(Key))
    {
        return Found->Get();
    }

    const float SampleMoisture = Full > 0.0f ? (Wet + 0.5f) / 4.0f * Full : (Wet > 0 ? 0.02f : 0.0f);
    const float SampleCelsius = bFrozen ? -5.0f : 10.0f;

    UPhysicalMaterial* Made = NewObject<UPhysicalMaterial>(this);
    Made->Friction = FMatter::FrictionOf(S, SampleMoisture, SampleCelsius, true);
    Made->StaticFriction = FMatter::FrictionOf(S, SampleMoisture, SampleCelsius, false);
    Made->Restitution = FMatter::BounceOf(S, SampleMoisture, SampleCelsius);
    Made->Density = S.Density / 1000.0f;
    Made->bOverrideFrictionCombineMode = true;
    Made->FrictionCombineMode = EFrictionCombineMode::Min;
    Made->bOverrideRestitutionCombineMode = true;
    Made->RestitutionCombineMode = EFrictionCombineMode::Min;
    Surfaces.Add(Key, Made);
    return Made;
}

FString UMatterSubsystem::DescribeClimate() const
{
    int32 Month = 0;
    int32 Date = 1;
    DateOf(Weather.DayOfYear, Month, Date);
    FString Sky = Weather.Cloud < 0.25f ? TEXT("ясно") : (Weather.Cloud < 0.6f ? TEXT("облачно") : TEXT("пасмурно"));
    FString Fall;
    if (Weather.Rain > 0.05f)
    {
        Fall = FString::Printf(TEXT(", дождь %.1f мм/ч"), Weather.Rain);
    }
    if (Weather.Snow > 0.05f)
    {
        Fall += FString::Printf(TEXT(", снег %.1f мм/ч"), Weather.Snow);
    }
    return FString::Printf(TEXT("%d %s, %+.1f °C, влажность %.0f%%, ветер %.1f м/с, %s%s, солнце %.0f° (%.0f Вт/м²)%s"),
        Date, MonthName(Month), Weather.Celsius, Weather.Humidity * 100.0f, Weather.Wind, *Sky, *Fall,
        Weather.SunElevation, Weather.Sun,
        Weather.SnowOnGround > 1.0f ? *FString::Printf(TEXT(", снег на земле %.0f мм"), Weather.SnowOnGround) : TEXT(""));
}

FString UMatterSubsystem::DescribeAt(const FVector& Where, UPrimitiveComponent* Component, int32 Item) const
{
    if (Component)
    {
        if (const AMatterStructure* House = Cast<AMatterStructure>(Component->GetOwner()))
        {
            return House->DescribePiece(House->FindPiece(Component, Item));
        }
        if (const ATerrainGrid* Ground0 = Cast<ATerrainGrid>(Component->GetOwner()))
        {
            return Ground0->DescribeAt(Where);
        }
        if (const AActor* Owner = Component->GetOwner())
        {
            if (const UMatterComponent* Matter = Owner->FindComponentByClass<UMatterComponent>())
            {
                return Matter->Describe();
            }
        }
    }
    return FString();
}
