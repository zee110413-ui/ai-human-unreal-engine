#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LearningAgentsManager.h"
#include "LearningAgentsInteractor.h"
#include "LearningAgentsTrainingEnvironment.h"
#include "LearningAgentsPolicy.h"
#include "LearningAgentsCritic.h"
#include "LearningAgentsPPOTrainer.h"
#include "LearningAgentsCommunicator.h"
#include "HumanLearning.generated.h"

class ACompleteHumanNPC;

UCLASS()
class AB_API UHumanAgentsManager : public ULearningAgentsManager
{
    GENERATED_BODY()

public:
    UHumanAgentsManager();
};

UCLASS()
class AB_API UHumanInteractor : public ULearningAgentsInteractor
{
    GENERATED_BODY()

public:
    virtual void SpecifyAgentObservation_Implementation(FLearningAgentsObservationSchemaElement& OutObservationSchemaElement, ULearningAgentsObservationSchema* InObservationSchema) override;
    virtual void GatherAgentObservation_Implementation(FLearningAgentsObservationObjectElement& OutObservationObjectElement, ULearningAgentsObservationObject* InObservationObject, const int32 AgentId) override;
    virtual void SpecifyAgentAction_Implementation(FLearningAgentsActionSchemaElement& OutActionSchemaElement, ULearningAgentsActionSchema* InActionSchema) override;
    virtual void PerformAgentAction_Implementation(const ULearningAgentsActionObject* InActionObject, const FLearningAgentsActionObjectElement& InActionObjectElement, const int32 AgentId) override;

    UPROPERTY()
    TObjectPtr<UHumanAgentsManager> HumanManager;
};

UCLASS()
class AB_API UHumanTrainingEnvironment : public ULearningAgentsTrainingEnvironment
{
    GENERATED_BODY()

public:
    virtual void GatherAgentReward_Implementation(float& OutReward, const int32 AgentId) override;
    virtual void GatherAgentCompletion_Implementation(ELearningAgentsCompletion& OutCompletion, const int32 AgentId) override;
    virtual void ResetAgentEpisode_Implementation(const int32 AgentId) override;

    UPROPERTY()
    TObjectPtr<UHumanAgentsManager> HumanManager;

    static float Feeling(const ACompleteHumanNPC* Human);

    double FeelingSum = 0.0;
    int64 FeelingCount = 0;
    int32 Rebirths = 0;

    double RecentSum = 0.0;
    int64 RecentCount = 0;
    float FirstWindow = 0.0f;
    bool bHaveFirst = false;

private:
    TMap<int32, float> PreviousFeeling;
};

UCLASS()
class AB_API AMindLearningDirector : public AActor
{
    GENERATED_BODY()

public:
    AMindLearningDirector();

    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHumanAgentsManager> Manager;

    UPROPERTY(EditAnywhere)
    float DecisionInterval = 0.2f;

    UPROPERTY(EditAnywhere)
    FLearningAgentsPPOTrainerSettings TrainerSettings;

    UPROPERTY(EditAnywhere)
    FLearningAgentsPPOTrainingSettings TrainingSettings;

    UPROPERTY(EditAnywhere)
    FLearningAgentsTrainingGameSettings GameSettings;

private:
    bool ReadyToSetup() const;
    void SetupLearning();
    void RegisterNewHumans();
    void SaveBrain();
    void LoadBrain();
    FString BrainFile(const TCHAR* Part) const;
    FString BrainFolder() const;

    UPROPERTY()
    TObjectPtr<ULearningAgentsInteractor> Interactor;

    UPROPERTY()
    TObjectPtr<ULearningAgentsPolicy> Policy;

    UPROPERTY()
    TObjectPtr<ULearningAgentsCritic> Critic;

    UPROPERTY()
    TObjectPtr<ULearningAgentsTrainingEnvironment> Environment;

    UPROPERTY()
    TObjectPtr<ULearningAgentsPPOTrainer> Trainer;

    UPROPERTY()
    TArray<TObjectPtr<ACompleteHumanNPC>> Registered;

    FLearningAgentsTrainerProcess TrainerProcess;

    bool bSetup = false;
    bool bTraining = false;
    float StartDelay = 3.0f;
    float DecisionTimer = 0.0f;
    float SaveTimer = 0.0f;
    float ReportTimer = 0.0f;
};
