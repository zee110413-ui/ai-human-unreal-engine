// FurnitureActor.h
// ---------------------------------------------------------------------------
// Вещь.
//
// Раньше здесь была таблица «кровать → NPC->UseBed()»: мебель командовала
// человеком. Теперь наоборот — вещь только объявляет, что она такое и что
// с ней можно сделать, а человек сам решает, нужно ему это или нет.
//
// Поставьте на уровень стул — и уставший сядет. Не потому что где-то
// написано «если устал, ищи стул», а потому что стул предлагает отдых,
// а отдых ему сейчас дороже всего остального.
// ---------------------------------------------------------------------------

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HumanTypes.h"
#include "FurnitureActor.generated.h"

class ACompleteHumanNPC;
class UAffordanceComponent;
class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;
class UBoxComponent;

UENUM(BlueprintType)
enum class EFurnitureType : uint8
{
    Bed, Chair, Table, Stove, TV, Sofa, Wardrobe, Fridge, Toilet, Shower, Sink, Desk,
    Computer, Bookshelf, Lamp, Plant,
    // Добавлено позже: без них дом был нежилым.
    Bath, Mirror, Nightstand, Rug, Counter, Washer, Crib, Piano,
    // Школа.
    Textbook, Blackboard, SchoolDesk, Globe, Abacus,
    // Хозяйство: то, откуда берётся еда.
    GardenBed, Barrel, MarketStall,
    // Промысел: откуда берётся всё остальное и где это делают руками.
    Tree, Workbench, Forge, Kiln, Loom, PotteryWheel, Beehive, ClayPit, StonePile,
    FishingSpot, Well, Shed, Bench, Cart, Fence, Sawhorse,
    // Дикое: то, что растёт и течёт само.
    Spring, Bush, Mushrooms, WildField
};

UCLASS()
class AB_API AFurnitureActor : public AActor
{
    GENERATED_BODY()

public:
    AFurnitureActor();

    virtual void BeginPlay() override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture")
    EFurnitureType FurnitureType = EFurnitureType::Chair;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture")
    FString Name;

    /** Для учебника — какой предмет. Пусто у прочих вещей. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture")
    FName Subject;

    /**
     * Что лежит внутри: у холодильника — продукты, у бочки — вода.
     * Кончается и пополняется, как и положено запасу.
     */
    UPROPERTY(BlueprintReadOnly, Category = "Furniture")
    TMap<EResourceKind, float> Stored;

    /** Что это за порода: у выхода камня — известняк или гранит, у ямы — глина или торф. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Furniture")
    EResourceKind Substance = EResourceKind::None;

    /** Спелость грядки 0..1. Урожай снимают, когда дозреет. */
    UPROPERTY(BlueprintReadOnly, Category = "Furniture")
    float Ripeness = 0.0f;

    /** Положить припас внутрь. */
    UFUNCTION(BlueprintCallable, Category = "Furniture")
    void Store(EResourceKind What, float HowMuch);

    /** Достать. Возвращает, сколько удалось. */
    UFUNCTION(BlueprintCallable, Category = "Furniture")
    float TakeOut(EResourceKind What, float HowMuch);

    UFUNCTION(BlueprintCallable, Category = "Furniture")
    float HowMuchInside(EResourceKind What) const;

    /** Шаг хозяйства: грядка зреет, запасы портятся. */
    void AdvanceHousehold(float GameDelta);

    /** Корень — пустышка: у вещи много частей, а не одна коробка. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Furniture")
    TObjectPtr<USceneComponent> Root;

    /** Совместимость: первая (главная) часть вещи. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Furniture")
    TObjectPtr<UStaticMeshComponent> Mesh;

    /** Из чего вещь сложена. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Furniture")
    TArray<TObjectPtr<UStaticMeshComponent>> Parts;

    /** То, что эта вещь предлагает. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Furniture")
    TObjectPtr<UAffordanceComponent> Affordances;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Furniture")
    TObjectPtr<UBoxComponent> Body;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Furniture")
    TObjectPtr<class UMatterComponent> Matter;

    static EResourceKind SubstanceOf(EFurnitureType Type);

    UPROPERTY(BlueprintReadOnly, Category = "Furniture")
    float MassKg = 20.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Furniture")
    bool bFixture = false;

    UPROPERTY(BlueprintReadOnly, Category = "Furniture")
    TWeakObjectPtr<AActor> HeldBy;

    void MakeSolid();

    /** Что здесь можно смастерить прямо сейчас — смотря что лежит рядом. */
    void RefreshCraftOffers();

    /** Сколько такого добра лежит вокруг: в этой вещи, в соседних и просто на земле. */
    float MaterialNearby(EResourceKind Kind, float Radius = 800.0f) const;

    /** Истратить материал вокруг. Возвращает, сколько удалось взять. */
    float SpendNearby(EResourceKind Kind, float HowMuch, float Radius = 800.0f);

    UFUNCTION(BlueprintCallable, Category = "Furniture")
    bool CanBeLifted() const;

    bool Shove(const FVector& Direction, float Effort);
    bool PickedUp(AActor* Who);
    void PutDown();

    static float MassOf(EFurnitureType Type);
    static bool FixtureOf(EFurnitureType Type);

    float CraftCheck = 0.0f;

    /** Собрать предложения, свойственные этому виду мебели. */
    void BuildOffers();
    bool BuildVillageOffers();
    void RefreshLook();

    struct FStallLot
    {
        EResourceKind Kind = EResourceKind::None;
        float Amount = 0.0f;
        float Price = 1.0f;
        TWeakObjectPtr<AActor> Owner;
    };
    TArray<FStallLot> Lots;
    void PutOnSale(AActor* Seller, EResourceKind Kind, float Amount, float Price);
    bool BuyFrom(AActor* Buyer, EResourceKind Kind, float Amount, float& OutPaid);
    float LotsOf(EResourceKind Kind) const;

    /** Сложить вещь из частей: ножки, столешница, спинка. */
    void BuildLook();

    /** Насколько вещь широка — чтобы не ставить её в стену. */
    UFUNCTION(BlueprintCallable, Category = "Furniture")
    FVector GetFootprint() const;

    /** Совместимость со старым кодом: подойти и воспользоваться. */
    UFUNCTION(BlueprintCallable, Category = "Furniture")
    void Interact(ACompleteHumanNPC* NPC);

private:
    /**
     * Приставить деталь.
     * Offset — от точки постановки (пол комнаты), Size — в сантиметрах.
     * Начало координат детали внизу: мебель стоит на полу, а не тонет в нём.
     */
    UStaticMeshComponent* Part(const TCHAR* Shape, const TCHAR* Material,
                               const FVector& Offset, const FVector& Size,
                               float YawDegrees = 0.0f);

    static UStaticMesh* Shape(const TCHAR* Name);
    static UMaterialInterface* Surface(const TCHAR* Name);

    bool Dress(const TCHAR* Folder, const TCHAR* Filter, const FVector& Offset, const FVector& Size, float YawDegrees = 0.0f, bool bStretch = false);
    bool DressFromModels();
};
