#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Matter.h"
#include "MatterComponent.h"
#include "MatterStructure.generated.h"

class UInstancedStaticMeshComponent;
class UAffordanceComponent;
class UMatterSubsystem;
class AResourceActor;

UENUM()
enum class EJointKind : uint8
{
    Ground,
    Rest,
    Notch,
    Peg,
    Nail,
    Mortar,
    Lashing
};

USTRUCT()
struct FStructurePiece
{
    GENERATED_BODY()

    UPROPERTY() EResourceKind Substance = EResourceKind::Wood;
    UPROPERTY() EMatterShape Shape = EMatterShape::Box;
    UPROPERTY() FTransform Frame;
    UPROPERTY() FVector Size = FVector(10.0f);
    UPROPERTY() FName Role;
    UPROPERTY() float Exposure = 1.0f;
    UPROPERTY() float Load = 0.0f;
    UPROPERTY() float Moisture = 0.1f;
    UPROPERTY() float Temperature = 12.0f;
    UPROPERTY() float Ice = 0.0f;
    UPROPERTY() float Damage = 0.0f;
    UPROPERTY() float Char = 0.0f;
    UPROPERTY() float Rot = 0.0f;
    UPROPERTY() float Snow = 0.0f;
    UPROPERTY() float DryMass = 1.0f;
    UPROPERTY() float OriginalMass = 1.0f;
    UPROPERTY() bool bBurning = false;
    UPROPERTY() bool bRemoved = false;
    UPROPERTY() bool bSpan = false;
    UPROPERTY() int32 Renderer = INDEX_NONE;
    UPROPERTY() int32 Instance = INDEX_NONE;
    UPROPERTY() FName Model;
};

USTRUCT()
struct FStructureJoint
{
    GENERATED_BODY()

    UPROPERTY() EJointKind Kind = EJointKind::Rest;
    UPROPERTY() int32 Upper = INDEX_NONE;
    UPROPERTY() int32 Lower = INDEX_NONE;
    UPROPERTY() float Load = 0.0f;
    UPROPERTY() float Damage = 0.0f;
    UPROPERTY() bool bBroken = false;
};

USTRUCT()
struct FStructureRenderer
{
    GENERATED_BODY()

    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Mesh;
    UPROPERTY() EResourceKind Substance = EResourceKind::Wood;
    UPROPERTY() EMatterShape Shape = EMatterShape::Box;
    UPROPERTY() TArray<int32> Pieces;
    UPROPERTY() bool bDirty = false;
    UPROPERTY() FName Model;
    UPROPERTY() FVector ModelMin = FVector::ZeroVector;
    UPROPERTY() FVector ModelSize = FVector(100.0f);
};

UCLASS()
class AB_API AMatterStructure : public AActor
{
    GENERATED_BODY()

public:
    AMatterStructure();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Structure")
    FString Title = TEXT("изба");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Structure")
    bool bPrivate = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Structure")
    FVector OwnerAnchor = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Structure")
    TObjectPtr<UAffordanceComponent> Affordances;

    static AMatterStructure* BuildIzba(UWorld* World, const FVector& Centre, float Yaw, const FVector& Footprint,
                                       EResourceKind Roofing, const FString& Title, bool bPrivate);

    int32 AddLog(const FVector& From, const FVector& To, float Diameter, EResourceKind Substance, FName PieceRole, bool bSpan, float Exposure);
    int32 AddBlock(const FVector& Centre, const FVector& Size, const FQuat& Turn, EResourceKind Substance, FName PieceRole, bool bSpan, float Exposure);
    int32 AddModelled(FName Model, const FVector& Centre, const FVector& Size, const FQuat& Turn, EResourceKind Substance, FName PieceRole, bool bSpan, float Exposure);
    void Join(int32 Upper, int32 Lower, EJointKind Kind);
    void Finish();

    void AdvanceMatter(float Seconds, UMatterSubsystem* Matter);
    int32 FindPiece(const UPrimitiveComponent* Component, int32 Item) const;
    void ImpactPiece(int32 Piece, float Joules, const FVector& Where, const FVector& Direction, AActor* Culprit, bool bEdge);
    bool IgnitePiece(int32 Piece);
    void SoakPiece(int32 Piece, float Kilograms);
    FString DescribePiece(int32 Piece) const;
    float PieceCompliance(int32 Piece) const;

    int32 PieceCount() const { return Pieces.Num(); }
    int32 MissingCount() const;
    float Condition() const;
    int32 NeedsRepair(EResourceKind& OutMaterial, float& OutAmount) const;
    bool RepairPiece(int32 Piece, float Quality);
    bool RepairFromOffer(float Quality);
    void RefreshOffers();

    const TArray<FStructurePiece>& GetPieces() const { return Pieces; }

private:
    int32 AddShape(EMatterShape Shape, const FVector& Centre, const FVector& Size, const FQuat& Turn, EResourceKind Substance, FName PieceRole, bool bSpan, float Exposure,
        FName Model = NAME_None);
    int32 RendererFor(EResourceKind Substance, EMatterShape Shape, FName Model = NAME_None);
    void Show(int32 Piece);
    void Hide(int32 Piece);
    void BreakPiece(int32 Piece, const FVector& Direction, float Energy, bool bEdge, AActor* Culprit);
    AResourceActor* DropPiece(int32 Piece, const FVector& Kick);
    void CutJoints(int32 Piece);
    void CheckStability();
    float JointCapacity(const FStructureJoint& Joint) const;
    float PieceStrength(int32 Piece) const;
    void PushLook(int32 Piece, bool bForce);
    void FlushLooks();
    void Announce(const FString& What, float Valence, float Radius) const;

    UPROPERTY()
    TObjectPtr<USceneComponent> Root;

    UPROPERTY()
    TArray<FStructurePiece> Pieces;

    UPROPERTY()
    TArray<FStructureJoint> Joints;

    UPROPERTY()
    TArray<FStructureRenderer> Renderers;

    TArray<float> Shown;
    bool bFinished = false;
    bool bStabilityDirty = false;
    float StabilityTimer = 0.0f;
    float OfferTimer = 0.0f;
    float SnowLoad = 0.0f;
    int32 LastMissing = -1;
    double AnnouncedAt = -1000.0;
    int32 Reported = 0;
};
