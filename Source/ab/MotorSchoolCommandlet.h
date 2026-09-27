#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "MotorSchoolCommandlet.generated.h"

UCLASS()
class AB_API UMotorSchoolCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    UMotorSchoolCommandlet();
    virtual int32 Main(const FString& Params) override;

    static void RaiseAll(const FString& Skills, int32 Decisions, const TArray<int32>& Kinds, bool bSave);
};
