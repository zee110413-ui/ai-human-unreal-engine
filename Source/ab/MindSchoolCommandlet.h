#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "MindSchoolCommandlet.generated.h"

UCLASS()
class AB_API UMindSchoolCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    UMindSchoolCommandlet();
    virtual int32 Main(const FString& Params) override;
};
