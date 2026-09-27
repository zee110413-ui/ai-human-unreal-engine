#pragma once

#include "CoreMinimal.h"
#include "HumanTypes.h"

class ACompleteHumanNPC;
class AFurnitureActor;
class UWorld;
struct FCraft;

struct AB_API FVillageTrade
{
    FName Skill;
    const TCHAR* Title = TEXT("");
    const TCHAR* TitleFemale = TEXT("");
    const TCHAR* Deed = TEXT("");
};

struct AB_API FPersonPlan
{
    FString FirstName;
    FString LastName;
    float Age = 30.0f;
    bool bFemale = false;
    bool bMasterTeacher = false;
    EVillageRole Role = EVillageRole::Peasant;
    FName Trade;
    int32 Household = INDEX_NONE;
    int32 Spouse = INDEX_NONE;
    int32 Mother = INDEX_NONE;
    int32 Father = INDEX_NONE;
    float Money = 10.0f;
};

struct AB_API FVillageWork
{
    FName Id;
    FString Label;
    uint8 Station = 0;
    FName Skill;
    float Difficulty = 0.3f;
    float Duration = 1800.0f;
    float Effort = 0.3f;
    EResourceKind Input = EResourceKind::None;
    float InputAmount = 0.0f;
    EResourceKind Fuel = EResourceKind::None;
    float FuelAmount = 0.0f;
    EResourceKind Output = EResourceKind::None;
    float OutputAmount = 0.0f;
    EResourceKind Tool = EResourceKind::None;
    int32 FromDay = 0;
    int32 ToDay = 366;
    bool bHome = false;
};

class AB_API FVillage
{
public:
    static FString Kingdom() { return TEXT("Светлоград"); }
    static FString Coins(float Amount);
    static FString CoinWord(int32 Amount);
    static float PriceOf(EResourceKind Kind);
    static bool IsFood(EResourceKind Kind);
    static bool IsReadyFood(EResourceKind Kind);
    static bool IsSellable(EResourceKind Kind);
    static bool IsTool(EResourceKind Kind);
    static float Portions(EResourceKind Kind);

    static const TArray<FVillageTrade>& Trades();
    static const FVillageTrade* TradeOf(FName Skill);
    static FString TitleOf(EVillageRole Role, FName Trade, bool bFemale, float Age);

    static const TArray<FVillageWork>& Works();
    static const FVillageWork* FindWork(FName Id);
    static bool InSeason(const FVillageWork& Work, int32 DayOfYear);
    static void AppendWorks(TArray<FCraft>& Out);

    static void Plan(int32 Count, uint32 Seed, TArray<FPersonPlan>& Out, int32& OutHouseholds);
    static void Weave(UWorld* World, const TArray<ACompleteHumanNPC*>& People, const TArray<FPersonPlan>& Plans);
    static void Skills(ACompleteHumanNPC* Person, const FPersonPlan& Plan);
    static FName BookFor(FName Skill);
    static void HomeBooks(const TArray<FName>& Trades, TArray<FName>& Out);
    static bool IsLiterate(const FPersonPlan& Plan);
    static void RememberBook(ACompleteHumanNPC* Person, FName Subject, float Level, float T);
    static void Dress(ACompleteHumanNPC* Person, EVillageRole Role, FName Trade, float Age, bool bFemale, uint32 Seed);

    static bool IsMedieval(const UObject* Context);
    static void SetMedieval(bool bOn);
    static bool FastInfants();
    static ACompleteHumanNPC* Birth(UWorld* World, ACompleteHumanNPC* Mother, ACompleteHumanNPC* Father);
    static float HeightScaleForAge(float Age);

    static float HouseholdStores(const ACompleteHumanNPC* Person, EResourceKind Only = EResourceKind::None);
    static float HouseholdFood(const ACompleteHumanNPC* Person);
    static float HouseholdFirewood(const ACompleteHumanNPC* Person);
    static float ProvisionFrom(float FoodDays, float Firewood, int32 DayOfYear, float Age);
    static float ExpectedWork(float Age, EVillageRole Role, int32 DayOfYear, float Hour);
    static bool IsWorkLike(const FAffordance& A);
    static void JudgeDay(class UNeedComponent* Needs, float Worked, float Expected, float Hours);
    static float Provision(const ACompleteHumanNPC* Person);
    static float CarriedWares(const ACompleteHumanNPC* Person);
    static float WealthForNeeds(const ACompleteHumanNPC* Person, float Money);
    static AFurnitureActor* HomeStore(const ACompleteHumanNPC* Person, EResourceKind For);
    static bool HasTool(const ACompleteHumanNPC* Person, EResourceKind Tool);
    static void GatherHomeStores(const ACompleteHumanNPC* Person, TArray<AFurnitureActor*>& Out);
    static int32 DayOfYear(const UObject* Context);
};
