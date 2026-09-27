#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Async/Future.h"
#include "HumanTypes.h"
#include "MotorLearning.h"
#include "BodyMotorComponent.generated.h"

class UPoseableMeshComponent;
class ACompleteHumanNPC;
class ATerrainGrid;
class UHumanMovementComponent;

UENUM()
enum class EHandStage : uint8
{
    Idle,
    Approach,
    Taking,
    Putting
};

USTRUCT()
struct FWalkerTick : public FTickFunction
{
    GENERATED_BODY()

    class UBodyMotorComponent* Motor = nullptr;

    virtual void ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent) override;
    virtual FString DiagnosticMessage() override { return TEXT("WalkerTick"); }
};

template<>
struct TStructOpsTypeTraits<FWalkerTick> : public TStructOpsTypeTraitsBase2<FWalkerTick>
{
    enum
    {
        WithCopy = false
    };
};

UCLASS(ClassGroup = (Human), meta = (BlueprintSpawnableComponent))
class AB_API UBodyMotorComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UBodyMotorComponent();

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

    void WalkerStep(float DeltaTime);

    void Bind(UPoseableMeshComponent* InBody);

    void MarkGreeting() { GreetTime = 1.8f; }
    void MarkSpeaking(float Seconds) { SpeakTime = FMath::Max(SpeakTime, Seconds); }

    bool IsReady() const { return bReady; }

    float GetWalkSkill() const { return WalkSkill; }
    float GetReachSkill() const { return bLearningStarted ? ReachSkill : 1.0f; }
    bool IsWalkerActive() const { return bWalkerActive; }
    bool IsDown() const { return bFallenDown || bToppling || bRising || bRagdoll; }
    bool IsInfant() const { return bInfant; }
    bool CanTryWalking() const { return !bInfant || InfantCycles >= 60; }
    int32 GetInfantPractice() const { return InfantCycles; }
    FString DescribeWalking() const;

    float GestureLevel(FName Gesture) const;
    void ObserveGesture(FName Gesture, float Attention);
    void PracticeGesture(FName Gesture, float Amount);
    FString DescribeBody() const;

    bool BeginTake(AActor* Item);
    bool BeginPut(bool bReturnToPlace);
    bool IsHandling() const { return Stage != EHandStage::Idle || QueuedTake.IsValid(); }
    bool IsTaking(const AActor* Item) const;
    bool TakeFailed(const AActor* Item) const;
    bool WantsApproach(FVector& OutPoint, float& OutSpeed) const;
    bool WantsFacing(FVector& OutPoint) const;
    void ForgetHeld();
    void CancelTake(bool bReturnIfHeld);
    EHandStage GetHandStage() const { return Stage; }
    FString DescribeHands() const;

    void SetGround(float SinkCm, float Grip, float Effort);
    float GetGroundEffort() const { return GroundEffort; }
    float GetFootSink() const { return FootSink; }

    UPROPERTY(Transient)
    TObjectPtr<UPoseableMeshComponent> Body;

private:
    struct FLimb
    {
        int32 Upper = INDEX_NONE;
        int32 Lower = INDEX_NONE;
        int32 End = INDEX_NONE;
        int32 Tip = INDEX_NONE;
    };

    void BuildCache();
    void BeginLearning();
    float GraspChance(const AActor* Item) const;

    void BeginWalker(ACompleteHumanNPC* Human);
    void AdvanceWalker(float Dt, ACompleteHumanNPC* Human);
    void RefreshGrounds(ACompleteHumanNPC* Human, UHumanMovementComponent* Move);
    FStepGround GroundAt(const ATerrainGrid* Terrain, const FVector2D& At, float Kilograms, float FloorZ, float& OutZ) const;
    ATerrainGrid* TerrainUnder(ACompleteHumanNPC* Human, UHumanMovementComponent* Move) const;
    void FootDown(int32 Index, ACompleteHumanNPC* Human, UHumanMovementComponent* Move);
    void FallOver(ACompleteHumanNPC* Human, UHumanMovementComponent* Move);
    void AdvanceTopple(float Dt, ACompleteHumanNPC* Human);
    bool StartRagdoll(ACompleteHumanNPC* Human);
    void AdvanceRagdoll(float Dt, ACompleteHumanNPC* Human);
    void EndRagdoll(ACompleteHumanNPC* Human);
    void TiltBody(ACompleteHumanNPC* Human, float Angle, const FVector& Toward, float Ahead);
    float BodyHeightMetres(const ACompleteHumanNPC* Human) const;
    void CloseDecision(bool bFailed);
    void MaybeStudy(ACompleteHumanNPC* Human);
    bool FacingWish(ACompleteHumanNPC* Human, FVector& OutPoint) const;
    FVector FootWorld(int32 Index, const ATerrainGrid* Terrain, float FloorZ) const;
    float CarriedKilograms(const ACompleteHumanNPC* Human) const;
    FString OwnWalkPath(const ACompleteHumanNPC* Human) const;

    void FK(int32 From);
    void RotateBone(int32 Bone, const FQuat& Delta);
    void AimBone(int32 Bone, int32 Child, const FVector& Direction, float Weight = 1.0f);
    void SolveLimb(const FLimb& Limb, float L1, float L2, const FVector& Target, const FVector& Pole, const FVector& EndDirection);
    static FVector SolveMiddle(const FVector& Root, const FVector& Target, float L1, float L2, const FVector& Pole);

    void AdvanceHands(float Dt, ACompleteHumanNPC* Human, const FTransform& ComponentToWorld);
    void BeginHands(ACompleteHumanNPC* Human);
    void EnterStage(EHandStage NewStage);
    void FailHands(const TCHAR* Why);
    bool MeasureItem(const AActor* Item, FVector& OutCentre, FVector& OutExtent, float& OutMass) const;
    FVector PlacePointFor(ACompleteHumanNPC* Human, bool bReturnToPlace, bool& bOutShelf) const;
    void FeelItem(const AActor* Item, int32 Hand, float Thickness);
    FVector HandGoalWorld(int32 Hand, ACompleteHumanNPC* Human, const FVector& Centre, const FVector& Extent) const;
    FVector ToBodyMetres(const ACompleteHumanNPC* Human, const FVector& World) const;
    FVector ModelHandInComponent(int32 Hand, const FVector& Fwd, const FVector& Side, float Scale) const;
    FVector PalmNormal(const FLimb& Arm, float Side) const;
    void OrientHand(const FLimb& Arm, float Side, const FVector& FingerDir, const FVector& PalmDir, float Weight);
    void PoseFingers(int32 HandIndex, const FLimb& Arm, float Side, float Curl, float Spread);
    FTransform HandWorld(const FLimb& Arm, const FTransform& ComponentToWorld) const;
    FTransform HoldFrame(const FTransform& ComponentToWorld) const;
    FVector IdealHold(const AActor* Held, const FTransform& Frame, const FTransform& ComponentToWorld) const;

    bool bReady = false;
    bool bHasSmoothed = false;

    TArray<FTransform> RefLocal;
    TArray<FTransform> RefCS;
    TArray<int32> Parents;
    TArray<FTransform> Local;
    TArray<FTransform> CS;
    TArray<FTransform> Smoothed;

    int32 Pelvis = INDEX_NONE;
    int32 Spine1 = INDEX_NONE;
    int32 Spine2 = INDEX_NONE;
    int32 Spine3 = INDEX_NONE;
    int32 Neck = INDEX_NONE;
    int32 Head = INDEX_NONE;
    FLimb LegL;
    FLimb LegR;
    FLimb ArmL;
    FLimb ArmR;
    int32 FingerBones[2][5][3];
    bool bHasFingers = false;

    float ThighLen = 0.0f;
    float CalfLen = 0.0f;
    float UpperArmLen = 0.0f;
    float ForeArmLen = 0.0f;
    float AnkleHeight = 8.0f;
    float FootPitch = -0.4f;
    float HipHalfWidth = 10.0f;
    float ShoulderHeight = 145.0f;
    FVector RefPelvisPos = FVector::ZeroVector;

    float Clock = 0.0f;
    float GreetTime = 0.0f;
    float SpeakTime = 0.0f;
    float StumbleTime = 0.0f;
    float HiddenAccumulator = 0.0f;
    EPosture LastPosture = EPosture::Standing;
    float SupportHeight = 0.0f;

    FWalkBody Walker;
    FMotorPolicy WalkBrain;
    FWalkGrounds WalkGrounds;
    FMotorBuffer Lived;
    TFuture<FMotorPolicy> Studying;
    TArray<float> WalkObservation;
    TArray<float> WalkNormalized;
    TArray<float> WalkAction;
    TArray<float> WalkNoise;
    TArray<float> StepObservation;
    TArray<float> StepNormalized;
    FVector2D StepVel = FVector2D::ZeroVector;
    FVector2D StepWant = FVector2D::ZeroVector;
    FVector2D StepFace = FVector2D::ZeroVector;
    float StepEnergy = 0.0f;
    float StepSlip = 0.0f;
    float StepTime = 0.0f;
    float StepLogProb = 0.0f;
    float StepValue = 0.0f;
    int32 StepTrips = 0;
    int32 NoiseAge = 0;
    int32 Studies = 0;
    bool bStepOpen = false;
    FWalkerTick WalkerTick;
    bool bInfant = false;
    int32 InfantCycles = 0;
    float PracticeClock = 0.0f;
    TFuture<FMotorPolicy> Practicing;
    void PracticeInfant(float DeltaTime, ACompleteHumanNPC* Human);
    void ReadyToWalk(ACompleteHumanNPC* Human);
    FVector2D DrivenVelocity = FVector2D::ZeroVector;
    float BumpRecent = 0.0f;
    float BlockedFor = 0.0f;
    float WalkingFor = 0.0f;
    float LastFrame = 0.0f;
    FVector2D LastWant = FVector2D::ZeroVector;
    FVector2D LastFace = FVector2D::ZeroVector;
    float WalkDecision = 0.0f;
    float FallClock = 0.0f;
    float FloorZ = 0.0f;
    float PelvisDrop = 0.0f;
    float TorsoTwist = 0.0f;
    float LastTorsoTwist = 0.0f;
    float ArmAngle[2] = { 0.0f, 0.0f };
    float ArmRate[2] = { 0.0f, 0.0f };
    FVector2D LastWalkVel = FVector2D::ZeroVector;
    int32 WalkKind = 4;
    int32 WalkSteps = 0;
    int32 WalkFalls = 0;
    int32 WalkCatches = 0;
    float CatchClock = 0.0f;
    mutable float LastSlope = 0.0f;
    int32 WalkTrips = 0;
    double WalkSeconds = 0.0;
    bool bWalkerReady = false;
    bool bWalkerUsable = false;
    bool bWalkerActive = false;
    bool bFallenDown = false;
    bool bToppling = false;
    bool bRising = false;
    bool bTilted = false;
    float ToppleAngle = 0.0f;
    float ToppleRate = 0.0f;
    float ToppleTime = 0.0f;
    float ToppleSpeed = 0.0f;
    float RiseTime = 0.0f;
    float FallCrouch = 0.0f;
    float PivotAhead = 0.0f;
    FVector ToppleDir = FVector::ForwardVector;
    FVector CatchWorld[2] = { FVector::ZeroVector, FVector::ZeroVector };
    FTransform UprightRelative;
    bool bRagdoll = false;
    bool bDollLying = false;
    bool bRiseFromDoll = false;
    float DollTime = 0.0f;
    float DollCalm = 0.0f;
    float DollImpact = 0.0f;

    bool bLearningStarted = false;
    float WalkSkill = 0.0f;
    float ReachSkill = 0.0f;
    float SitSkill = 0.0f;
    float WorkSkill = 0.0f;
    TMap<FName, float> Gestures;
    FRandomStream Noise;

    EHandStage Stage = EHandStage::Idle;
    TWeakObjectPtr<AActor> HandItem;
    TWeakObjectPtr<AActor> QueuedTake;
    TWeakObjectPtr<AActor> FailedItem;
    double FailedAt = -100.0;
    bool bPutting = false;
    bool bReturnPut = false;
    bool bShelfPut = false;
    bool bTwoHands = false;
    bool bFumbled = false;
    bool bPutAfterLift = false;
    bool bPutAfterLiftReturn = false;
    float HandSide = 1.0f;
    FVector GoalWorld = FVector::ZeroVector;
    FVector ItemExtent = FVector(5.0f);
    float ItemMass = 1.0f;
    float StageClock = 0.0f;
    int32 GraspTries = 0;
    FVector LastHandL = FVector::ZeroVector;
    FVector LastHandR = FVector::ZeroVector;
    float Bend = 0.0f;
    float Squat = 0.0f;
    float Grip[2] = { 0.15f, 0.15f };
    float HoldCurl = 0.6f;

    FReachBody Arms[2];
    FGripState Grips[2];
    FMotorPolicy ReachBrain;
    FMotorPolicy GripBrain;
    TArray<float> ReachSense;
    TArray<float> ReachNorm;
    TArray<float> ReachAct;
    TArray<float> ReachNoise;
    TArray<float> GripSense;
    TArray<float> GripNorm;
    TArray<float> GripAct;
    TArray<float> GripNoise;
    int32 ReachNoiseAge = 0;
    int32 GripNoiseAge = 0;
    FVector HandGoal[2] = { FVector::ZeroVector, FVector::ZeroVector };
    FVector HandInComponent[2] = { FVector::ZeroVector, FVector::ZeroVector };
    float HandLift[2] = { 0.0f, 0.0f };
    float HandRise[2] = { 0.0f, 0.0f };
    float LastHandHeight[2] = { 0.0f, 0.0f };
    bool bHandBusy[2] = { false, false };
    float ReachClock = 0.0f;
    float GripClock = 0.0f;
    bool bHandsReady = false;
    bool bHandsUsable = false;
    FTransform HeldOffset;
    FVector HeldFrom = FVector::ZeroVector;
    float SettleClock = 1.0f;
    TWeakObjectPtr<AActor> OffsetFor;
    FVector PlaceWorld = FVector::ZeroVector;
    FString LastHandNote;

    float FootSink = 0.0f;
    float GroundGrip = 0.7f;
    float GroundEffort = 1.0f;
};
