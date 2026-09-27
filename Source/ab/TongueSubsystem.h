#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "TongueSubsystem.generated.h"

UCLASS()
class AB_API UTongueSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    bool IsReady() const { return bReady; }
    int32 Waiting() const { return Pending; }

    void Phrase(const FString& Meaning, const FString& Speaker, const FString& Listener, const TArray<FString>& Words,
        TFunction<void(const FString&)> Done);

    static FString Grammar(const TArray<FString>& Words);
    static TCHAR Upper(TCHAR C);

private:
    bool Beat(float DeltaTime);
    void Warm();
    void Send(const FString& Meaning, const FString& Speaker, const FString& Listener, const TArray<FString>& Words,
        int32 Tries, TFunction<void(const FString&)> Done, float Patience);

    FProcHandle Server;
    FTSTicker::FDelegateHandle Ticker;
    FString Address;
    double StartedAt = 0.0;
    double NextProbe = 0.0;
    int32 Pending = 0;
    int32 Failures = 0;
    int32 Asked = 0;
    bool bStarted = false;
    bool bReady = false;
    bool bProbing = false;
};
