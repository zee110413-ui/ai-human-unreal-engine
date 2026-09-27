#include "MindSchoolCommandlet.h"
#include "MindLearning.h"
#include "TalkLearning.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Misc/FileHelper.h"
#include "Modules/ModuleManager.h"
#include "HumanTypes.h"
#include "Misc/Parse.h"
#include "HAL/PlatformTime.h"

UMindSchoolCommandlet::UMindSchoolCommandlet()
{
    IsClient = false;
    IsServer = false;
    IsEditor = true;
    LogToConsole = true;
}

int32 UMindSchoolCommandlet::Main(const FString& Params)
{
    int32 Days = 300;
    int32 People = 24;
    FParse::Value(*Params, TEXT("Days="), Days);
    FParse::Value(*Params, TEXT("People="), People);
    FString KindList = TEXT("4+3+2+1+0+5+6");
    FParse::Value(*Params, TEXT("Kinds="), KindList, false);
    const bool bSave = !FParse::Param(*Params, TEXT("NoSave"));
    const bool bFresh = FParse::Param(*Params, TEXT("Fresh"));

    if (FParse::Param(*Params, TEXT("FixCollision")))
    {
        IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
        Registry.SearchAllAssets(true);
        TArray<FAssetData> Assets;
        Registry.GetAssetsByPath(FName(TEXT("/Game/Imported")), Assets, true);
        int32 Fixed = 0;
        int32 Already = 0;
        for (const FAssetData& Asset : Assets)
        {
            if (Asset.AssetClassPath != UStaticMesh::StaticClass()->GetClassPathName())
            {
                continue;
            }
            UStaticMesh* Mesh = Cast<UStaticMesh>(Asset.GetAsset());
            if (!Mesh)
            {
                continue;
            }
            Mesh->CreateBodySetup();
            UBodySetup* Setup = Mesh->GetBodySetup();
            if (!Setup)
            {
                continue;
            }
            if (Setup->AggGeom.GetElementCount() > 0)
            {
                ++Already;
                continue;
            }
            const FBox Box = Mesh->GetBoundingBox();
            FKBoxElem Element;
            Element.Center = Box.GetCenter();
            const FVector Size = Box.GetSize().ComponentMax(FVector(1.0f));
            Element.X = Size.X;
            Element.Y = Size.Y;
            Element.Z = Size.Z;
            Setup->AggGeom.BoxElems.Add(Element);
            Setup->CollisionTraceFlag = CTF_UseDefault;
            Setup->InvalidatePhysicsData();
            Setup->CreatePhysicsMeshes();
            Mesh->MarkPackageDirty();
            UPackage* Package = Mesh->GetOutermost();
            const FString File = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
            FSavePackageArgs Save;
            Save.TopLevelFlags = RF_Public | RF_Standalone;
            if (UPackage::SavePackage(Package, Mesh, *File, Save))
            {
                ++Fixed;
            }
        }
        UE_LOG(LogHumanCity, Display, TEXT("MODELS коробка столкновения добавлена %d моделям, своя уже была у %d"), Fixed, Already);
        return 0;
    }

    FString ModelsFile;
    if (FParse::Value(*Params, TEXT("Models="), ModelsFile))
    {
        IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
        Registry.SearchAllAssets(true);
        TArray<FAssetData> Assets;
        Registry.GetAssetsByPath(FName(TEXT("/Game/Imported")), Assets, true);
        TArray<FString> Lines;
        for (const FAssetData& Asset : Assets)
        {
            if (Asset.AssetClassPath != UStaticMesh::StaticClass()->GetClassPathName())
            {
                continue;
            }
            const UStaticMesh* Mesh = Cast<UStaticMesh>(Asset.GetAsset());
            if (!Mesh)
            {
                continue;
            }
            const FBox Box = Mesh->GetBoundingBox();
            const FVector Size = Box.GetSize();
            Lines.Add(FString::Printf(TEXT("%s\t%.1f\t%.1f\t%.1f\t%.1f\t%.1f\t%.1f\t%d"), *Asset.PackageName.ToString(),
                Size.X, Size.Y, Size.Z, Box.Min.X, Box.Min.Y, Box.Min.Z, Mesh->GetStaticMaterials().Num()));
        }
        Lines.Sort();
        FFileHelper::SaveStringArrayToFile(Lines, *ModelsFile, FFileHelper::EEncodingOptions::ForceUTF8);
        UE_LOG(LogHumanCity, Display, TEXT("MODELS записано %d моделей в %s"), Lines.Num(), *ModelsFile);
        return 0;
    }

    int32 Talks = 0;
    if (FParse::Value(*Params, TEXT("Talk="), Talks) && Talks > 0)
    {
        FTalkCore Talk;
        const bool bKnown = !bFresh && Talk.Load(FTalkSchool::Path(4));
        UE_LOG(LogHumanCity, Display, TEXT("TALK == школа разговора, %s, разговоров %d"), bKnown ? TEXT("продолжает") : TEXT("с нуля"), Talks);
        const double TalkStart = FPlatformTime::Seconds();
        FString Report;
        FTalkSchool::Raise(Talk, 4, Talks, 777u, &Report);
        UE_LOG(LogHumanCity, Display, TEXT("TALK == выучено реплик %lld, уроков %lld, ошибка %.4f, за %.1f с"),
            Talk.Lived, Talk.Lessons, Talk.Error, FPlatformTime::Seconds() - TalkStart);
        if (bSave)
        {
            Talk.Save(FTalkSchool::Path(4));
        }
        return 0;
    }

    TArray<FString> Parts;
    KindList.ParseIntoArray(Parts, TEXT("+"));
    const double Start = FPlatformTime::Seconds();
    TSet<int32> Raised;
    for (const FString& Part : Parts)
    {
        const int32 Kind = FCString::Atoi(*Part);
        FMindCore Core;
        bool bKnown = !bFresh && Core.Load(FLifeSchool::CorePath(Kind));
        if (!bKnown)
        {
            const int32 Parent = FMotorSchool::ParentKind(Kind);
            bKnown = Parent >= 0 && (!bFresh || Raised.Contains(Parent)) && Core.Load(FLifeSchool::CorePath(Parent));
        }
        const int32 Budget = bKnown ? FMath::Max(1, Days / 2) : Days;
        UE_LOG(LogHumanCity, Display, TEXT("MIND == школа жизни, тело %d, %s, дней %d, людей %d"),
            Kind, bKnown ? TEXT("продолжает прожитое") : TEXT("с рождения"), Budget, People);
        const double JobStart = FPlatformTime::Seconds();
        FLifeSchool::Raise(Core, Kind, Budget, People, 5000u + Kind * 31u, nullptr);
        UE_LOG(LogHumanCity, Display, TEXT("MIND == тело %d: прожито решений %lld, уроков %lld, за %.1f с"),
            Kind, Core.Lived, Core.Lessons, FPlatformTime::Seconds() - JobStart);
        if (bSave)
        {
            Core.Save(FLifeSchool::CorePath(Kind));
            Raised.Add(Kind);
        }
    }
    UE_LOG(LogHumanCity, Display, TEXT("MIND школа заняла %.1f с"), FPlatformTime::Seconds() - Start);
    return 0;
}
