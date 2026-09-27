#include "ImportedModels.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Modules/ModuleManager.h"

namespace
{
    TMap<FString, TArray<TWeakObjectPtr<UStaticMesh>>>& Cache()
    {
        static TMap<FString, TArray<TWeakObjectPtr<UStaticMesh>>> Found;
        return Found;
    }

    bool Matches(const FString& Name, const FString& Filter)
    {
        if (Filter.IsEmpty())
        {
            return true;
        }
        if (Filter.EndsWith(TEXT("*")))
        {
            return Name.StartsWith(Filter.LeftChop(1), ESearchCase::IgnoreCase);
        }
        if (Filter.StartsWith(TEXT("*")))
        {
            return Name.Contains(Filter.RightChop(1), ESearchCase::IgnoreCase);
        }
        return Name.Equals(Filter, ESearchCase::IgnoreCase);
    }
}

void FImportedModels::Gather(const FString& Folder, const FString& Filter, TArray<UStaticMesh*>& Out)
{
    Out.Reset();
    const FString Key = Folder + TEXT("|") + Filter;
    if (const TArray<TWeakObjectPtr<UStaticMesh>>* Known = Cache().Find(Key))
    {
        for (const TWeakObjectPtr<UStaticMesh>& Mesh : *Known)
        {
            if (Mesh.IsValid())
            {
                Out.Add(Mesh.Get());
            }
        }
        if (Out.Num() == Known->Num())
        {
            return;
        }
        Out.Reset();
    }
    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    TArray<FString> Paths = { Folder };
    Registry.ScanPathsSynchronous(Paths, false);
    TArray<FAssetData> Assets;
    Registry.GetAssetsByPath(FName(*Folder), Assets, false);
    Assets.Sort([](const FAssetData& A, const FAssetData& B) { return A.AssetName.LexicalLess(B.AssetName); });
    TArray<TWeakObjectPtr<UStaticMesh>> Remembered;
    for (const FAssetData& Asset : Assets)
    {
        if (Asset.AssetClassPath != UStaticMesh::StaticClass()->GetClassPathName() || !Matches(Asset.AssetName.ToString(), Filter))
        {
            continue;
        }
        if (UStaticMesh* Mesh = Cast<UStaticMesh>(Asset.GetAsset()))
        {
            Out.Add(Mesh);
            Remembered.Add(Mesh);
        }
    }
    Cache().Add(Key, Remembered);
}

FBox FImportedModels::BoundsOf(const TArray<UStaticMesh*>& Meshes)
{
    FBox Box(ForceInit);
    for (const UStaticMesh* Mesh : Meshes)
    {
        if (Mesh)
        {
            Box += Mesh->GetBoundingBox();
        }
    }
    return Box;
}

FTransform FImportedModels::Fit(const FBox& Box, const FVector& Offset, const FVector& Size, float YawDegrees, bool bStretch)
{
    if (!Box.IsValid)
    {
        return FTransform(FRotator(0.0f, YawDegrees, 0.0f), Offset);
    }
    const FVector Extent = Box.GetSize().ComponentMax(FVector(0.1f));
    const float Uniform = FMath::Min3(Size.X / Extent.X, Size.Y / Extent.Y, Size.Z / Extent.Z);
    const FVector Scale = bStretch ? FVector(Size.X / Extent.X, Size.Y / Extent.Y, Size.Z / Extent.Z) : FVector(Uniform);
    const FRotator Turn(0.0f, YawDegrees, 0.0f);
    const FVector Centre = Box.GetCenter();
    const FVector Pivot(Centre.X * Scale.X, Centre.Y * Scale.Y, Box.Min.Z * Scale.Z);
    return FTransform(Turn, Offset - Turn.RotateVector(Pivot), Scale);
}

int32 FImportedModels::Place(AActor* Owner, USceneComponent* Parent, const FString& Folder, const FString& Filter,
    const FVector& Offset, const FVector& Size, float YawDegrees, bool bCollide, TArray<UStaticMeshComponent*>* OutParts,
    bool bStretch)
{
    if (!Owner || !Parent)
    {
        return 0;
    }
    TArray<UStaticMesh*> Meshes;
    Gather(Folder.StartsWith(TEXT("/")) ? Folder : TEXT("/Game/Imported/") + Folder, Filter, Meshes);
    if (Meshes.Num() == 0)
    {
        return 0;
    }
    const FTransform Where = Fit(BoundsOf(Meshes), Offset, Size, YawDegrees, bStretch);
    for (UStaticMesh* Mesh : Meshes)
    {
        UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(Owner);
        Part->SetStaticMesh(Mesh);
        Part->SetMobility(EComponentMobility::Movable);
        Part->SetupAttachment(Parent);
        Part->SetRelativeTransform(Where);
        if (bCollide)
        {
            Part->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            Part->SetCollisionProfileName(TEXT("BlockAll"));
        }
        else
        {
            Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        }
        Part->RegisterComponent();
        if (OutParts)
        {
            OutParts->Add(Part);
        }
    }
    return Meshes.Num();
}
