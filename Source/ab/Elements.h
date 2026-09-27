#pragma once

#include "CoreMinimal.h"
#include "HumanTypes.h"
#include "Elements.generated.h"

UENUM(BlueprintType)
enum class EMatterState : uint8
{
    Solid, Liquid, Gas
};

UENUM(BlueprintType)
enum class EElementKind : uint8
{
    Metal, Semimetal, Nonmetal, Halogen, NobleGas, Lanthanide, Actinide, Unknown
};

USTRUCT(BlueprintType)
struct FElement
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Element") int32 Number = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Element") FString Symbol;
    UPROPERTY(BlueprintReadOnly, Category = "Element") FString Name;
    UPROPERTY(BlueprintReadOnly, Category = "Element") float Mass = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category = "Element") float Density = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category = "Element") float Melting = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category = "Element") float Boiling = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category = "Element") EMatterState State = EMatterState::Solid;
    UPROPERTY(BlueprintReadOnly, Category = "Element") EElementKind Kind = EElementKind::Metal;

    /** Сколько его в земной коре, долей на миллион. */
    UPROPERTY(BlueprintReadOnly, Category = "Element") float Crust = 0.0f;

    /** Где его находят: порода, соль, руда, воздух, вода. */
    UPROPERTY(BlueprintReadOnly, Category = "Element") FString FoundIn;
};

USTRUCT(BlueprintType)
struct FMineral
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Mineral") EResourceKind Kind = EResourceKind::None;
    UPROPERTY(BlueprintReadOnly, Category = "Mineral") FString Name;

    /** Из чего состоит: символы элементов. */
    UPROPERTY(BlueprintReadOnly, Category = "Mineral") TArray<FString> Elements;

    UPROPERTY(BlueprintReadOnly, Category = "Mineral") float Density = 2.5f;
    UPROPERTY(BlueprintReadOnly, Category = "Mineral") float Hardness = 3.0f;
    UPROPERTY(BlueprintReadOnly, Category = "Mineral") float Melting = 1200.0f;
    UPROPERTY(BlueprintReadOnly, Category = "Mineral") bool bBurns = false;

    /** На какой земле лежит: низина, холм, равнина, берег, болото. */
    UPROPERTY(BlueprintReadOnly, Category = "Mineral") FName Ground;

    /** Насколько часто встречается 0..1. */
    UPROPERTY(BlueprintReadOnly, Category = "Mineral") float Common = 0.5f;
};

USTRUCT(BlueprintType)
struct FWorldLaw
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Law") FString Name;
    UPROPERTY(BlueprintReadOnly, Category = "Law") FString Statement;
    UPROPERTY(BlueprintReadOnly, Category = "Law") float Value = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category = "Law") FString Unit;
};

class AB_API FElements
{
public:
    static const TArray<FElement>& All();
    static const FElement* BySymbol(const FString& Symbol);
    static const FElement* ByName(const FString& Name);

    static const TArray<FMineral>& Minerals();
    static const FMineral* MineralOf(EResourceKind Kind);
    static void MineralsOfGround(FName Ground, TArray<const FMineral*>& Out);

    static const TArray<FWorldLaw>& Physics();
    static const TArray<FWorldLaw>& Mathematics();
};
