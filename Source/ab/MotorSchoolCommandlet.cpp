#include "MotorSchoolCommandlet.h"
#include "MotorLearning.h"
#include "HumanTypes.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformTime.h"

namespace
{
    float AgeOfKind(int32 Kind, bool& bOutFemale)
    {
        bOutFemale = Kind == 3 || Kind == 5;
        switch (Kind)
        {
        case 0:  return 2.0f;
        case 1:  return 6.0f;
        case 2:  return 12.0f;
        case 3:
        case 4:  return 30.0f;
        default: return 72.0f;
        }
    }

    void Print(const FString& Report)
    {
        TArray<FString> Lines;
        Report.ParseIntoArrayLines(Lines);
        for (const FString& Line : Lines)
        {
            UE_LOG(LogHumanCity, Display, TEXT("MOTOR %s"), *Line);
        }
    }
}

UMotorSchoolCommandlet::UMotorSchoolCommandlet()
{
    IsClient = false;
    IsServer = false;
    IsEditor = true;
    LogToConsole = true;
}

void UMotorSchoolCommandlet::RaiseAll(const FString& Skills, int32 Budget, const TArray<int32>& Kinds, bool bSave)
{
    const double Start = FPlatformTime::Seconds();
    TMap<int32, FMotorPolicy> Walkers;

    for (int32 Kind : Kinds)
    {
        bool bFemale = false;
        const float Age = AgeOfKind(Kind, bFemale);
        for (const TCHAR* Skill : { TEXT("Walk"), TEXT("Reach"), TEXT("Grip") })
        {
            if (!Skills.Contains(Skill))
            {
                continue;
            }
            const double JobStart = FPlatformTime::Seconds();
            const uint32 Seed = 1000u + Kind * 17u + GetTypeHash(FString(Skill));
            FString Report;
            if (FCString::Strcmp(Skill, TEXT("Walk")) == 0)
            {
                FWalkBody Body;
                FMotorSchool::MakeWalkBody(Age, bFemale, Body);
                FMotorPolicy Policy;
                const int32 Parent = FMotorSchool::ParentKind(Kind);
                bool bGrown = FMotorSchool::Childhood(TEXT("Walk"), Kind, Policy);
                if (!bGrown && Parent >= 0)
                {
                    if (const FMotorPolicy* Found = Walkers.Find(Parent))
                    {
                        Policy = *Found;
                        bGrown = true;
                    }
                    else
                    {
                        bGrown = FMotorSchool::Childhood(TEXT("Walk"), Parent, Policy);
                    }
                }
                const int32 Iterations = bGrown ? FMath::Max(1, Budget / 2) : Budget;
                UE_LOG(LogHumanCity, Display, TEXT("MOTOR == Walk, тело %d (возраст %.0f, нога %.2f м, масса %.0f кг, сила %.2f), %s, циклов %d"),
                    Kind, Age, Body.Leg, Body.Mass, Body.Strength,
                    bGrown ? *FString::Printf(TEXT("вырос из тела %d"), Parent) : TEXT("с нуля"), Iterations);
                FMotorSchool::RaiseWalker(Policy, Body, Iterations, Seed, &Report);
                TArray<FString> Trace;
                const FString Easy = FMotorSchool::ExamineWalker(Policy, Body, 0.0f, 4242u, nullptr);
                const FString Hard = FMotorSchool::ExamineWalker(Policy, Body, Policy.Difficulty, 4343u, &Trace);
                UE_LOG(LogHumanCity, Display, TEXT("MOTOR проверка без случайности, лёгкая земля: %s"), *Easy);
                UE_LOG(LogHumanCity, Display, TEXT("MOTOR проверка без случайности, трудность %.2f: %s"), Policy.Difficulty, *Hard);
                FFileHelper::SaveStringArrayToFile(Trace, *(FMotorSchool::Folder() / FString::Printf(TEXT("WalkTrace_%d.txt"), Kind)),
                    FFileHelper::EEncodingOptions::ForceUTF8);
                Walkers.Add(Kind, Policy);
                if (bSave)
                {
                    Policy.Save(FMotorSchool::Folder() / FString::Printf(TEXT("Walk_%d.pol"), Kind));
                }
            }
            else if (FCString::Strcmp(Skill, TEXT("Reach")) == 0)
            {
                FReachBody Body;
                FMotorSchool::MakeReachBody(Age, bFemale, 1.0f, Body);
                FMotorPolicy Policy;
                const int32 Parent = FMotorSchool::ParentKind(Kind);
                bool bGrown = FMotorSchool::Childhood(TEXT("Reach"), Kind, Policy);
                if (!bGrown && Parent >= 0)
                {
                    bGrown = FMotorSchool::Childhood(TEXT("Reach"), Parent, Policy);
                }
                const int32 Iterations = bGrown ? FMath::Max(1, Budget / 2) : Budget;
                UE_LOG(LogHumanCity, Display, TEXT("MOTOR == Reach, тело %d (возраст %.0f, рука %.2f м, сила %.2f), %s, циклов %d"),
                    Kind, Age, Body.UpperArm + Body.ForeArm, Body.Strength, bGrown ? TEXT("продолжает") : TEXT("с нуля"), Iterations);
                FMotorSchool::RaiseReacher(Policy, Body, Iterations, Seed, &Report);
                if (bSave)
                {
                    Policy.Save(FMotorSchool::Folder() / FString::Printf(TEXT("Reach_%d.pol"), Kind));
                }
            }
            else
            {
                float Leg = 0.9f;
                float Mass = 70.0f;
                float Strength = 1.0f;
                FMotorSchool::Grow(Age, bFemale, Leg, Mass, Strength);
                FMotorPolicy Policy;
                const int32 Parent = FMotorSchool::ParentKind(Kind);
                bool bGrown = FMotorSchool::Childhood(TEXT("Grip"), Kind, Policy);
                if (!bGrown && Parent >= 0)
                {
                    bGrown = FMotorSchool::Childhood(TEXT("Grip"), Parent, Policy);
                }
                const int32 Iterations = bGrown ? FMath::Max(1, Budget / 2) : Budget;
                UE_LOG(LogHumanCity, Display, TEXT("MOTOR == Grip, тело %d (сила %.2f), %s, циклов %d"),
                    Kind, Strength, bGrown ? TEXT("продолжает") : TEXT("с нуля"), Iterations);
                FMotorSchool::RaiseGripper(Policy, Strength, Iterations, Seed, &Report);
                if (bSave)
                {
                    Policy.Save(FMotorSchool::Folder() / FString::Printf(TEXT("Grip_%d.pol"), Kind));
                }
            }
            UE_LOG(LogHumanCity, Display, TEXT("MOTOR == %s, тело %d готово за %.1f с"), Skill, Kind, FPlatformTime::Seconds() - JobStart);
        }
    }
    UE_LOG(LogHumanCity, Display, TEXT("MOTOR школа заняла %.1f с"), FPlatformTime::Seconds() - Start);
}

int32 UMotorSchoolCommandlet::Main(const FString& Params)
{
    FString Skills = TEXT("Walk+Reach+Grip");
    FParse::Value(*Params, TEXT("Skill="), Skills, false);
    int32 Budget = 600;
    FParse::Value(*Params, TEXT("Generations="), Budget);
    FString KindList = TEXT("0+1+2+3+4+5+6");
    FParse::Value(*Params, TEXT("Kinds="), KindList, false);
    const bool bSave = !FParse::Param(*Params, TEXT("NoSave"));

    TArray<FString> Parts;
    KindList.ParseIntoArray(Parts, TEXT("+"));
    TArray<int32> Kinds;
    for (const FString& Part : Parts)
    {
        Kinds.Add(FCString::Atoi(*Part));
    }
    RaiseAll(Skills, Budget, Kinds, bSave);
    return 0;
}
