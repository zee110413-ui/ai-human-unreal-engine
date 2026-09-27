#pragma once

#include "CoreMinimal.h"

class UStaticMesh;
class USceneComponent;
class UStaticMeshComponent;
class AActor;

class AB_API FImportedModels
{
public:
    static void Gather(const FString& Folder, const FString& Filter, TArray<UStaticMesh*>& Out);
    static FBox BoundsOf(const TArray<UStaticMesh*>& Meshes);
    static FTransform Fit(const FBox& Box, const FVector& Offset, const FVector& Size, float YawDegrees, bool bStretch = false);
    static int32 Place(AActor* Owner, USceneComponent* Parent, const FString& Folder, const FString& Filter,
        const FVector& Offset, const FVector& Size, float YawDegrees, bool bCollide, TArray<UStaticMeshComponent*>* OutParts,
        bool bStretch = false);
};
