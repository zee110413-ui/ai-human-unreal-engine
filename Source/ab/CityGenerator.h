// CityGenerator.h
// ---------------------------------------------------------------------------
// Город: дома, улицы, места и люди.
//
// Задача генератора — не красота, а СОДЕРЖАНИЕ: чтобы у каждого человека был
// дом, работа, было где поесть и куда сходить просто так. Без этого вся
// психика окажется без применения: человеку некуда будет хотеть.
// ---------------------------------------------------------------------------

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FurnitureActor.h"
#include "HumanTypes.h"
#include "Village.h"
#include "CityGenerator.generated.h"

class ACompleteHumanNPC;
class UInstancedStaticMeshComponent;

USTRUCT(BlueprintType)
struct FBuildingInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite) FString Type;
    UPROPERTY(BlueprintReadWrite) FVector Location = FVector::ZeroVector;
    UPROPERTY(BlueprintReadWrite) FVector Scale = FVector::OneVector;
    UPROPERTY(BlueprintReadWrite) int32 Capacity = 0;
    UPROPERTY(BlueprintReadWrite) TArray<ACompleteHumanNPC*> Occupants;
    UPROPERTY(BlueprintReadWrite) TArray<AFurnitureActor*> Furniture;
    UPROPERTY(BlueprintReadWrite) FVector EntranceLocation = FVector::ZeroVector;

    /** Из чего сложено: бревно, камень, глина, солома — и сколько чего ушло. */
    UPROPERTY(BlueprintReadWrite) TMap<EResourceKind, float> Made;

    UPROPERTY(BlueprintReadWrite) TObjectPtr<class AMatterStructure> Structure = nullptr;
};

UENUM(BlueprintType)
enum class ESettlement : uint8
{
    Village  UMETA(DisplayName = "Деревня"),
    Town     UMETA(DisplayName = "Средневековый город"),
    Modern   UMETA(DisplayName = "Прежний город")
};

UCLASS()
class AB_API ACityGenerator : public AActor
{
    GENERATED_BODY()

public:
    ACityGenerator();

    /** Какое поселение строить. Ключи: -Village, -Town, -Modern. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City")
    ESettlement Settlement = ESettlement::Village;

    // --- Размеры города -----------------------------------------------------

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City") int32 BlocksX = 4;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City") int32 BlocksY = 4;
    /**
     * Шаг сетки кварталов в сантиметрах.
     * Город намеренно компактный: путь через него должен занимать минуты,
     * а не полдня, иначе вся жизнь человека уйдёт на дорогу.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City") float BlockSize = 2200.0f;
    /** Сколько людей поселить. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City") int32 NPCsCount = 16;

    /** Создать землю под городом — чтобы люди не падали в пустоту. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City") bool bSpawnGround = true;

    /** Генерировать всё при старте. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City") bool bGenerateOnBeginPlay = true;

    UPROPERTY(BlueprintReadOnly, Category = "City") TArray<FBuildingInfo> Buildings;

    /** Места, которые город расставил: кафе, конторы, скверы, подъезды. */
    UPROPERTY(BlueprintReadOnly, Category = "City") TArray<TObjectPtr<class APlaceActor>> SpawnedPlaces;

    UFUNCTION(BlueprintCallable, Category = "City") void GenerateCity();
    UFUNCTION(BlueprintCallable, Category = "City") void SpawnNPCs();

    FBuildingInfo* FindBuildingByType(const FString& Type);
    FBuildingInfo* GetRandomResidentialBuilding();
    FBuildingInfo* GetRandomBuildingByType(const FString& Type);

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    // --- Облик города -------------------------------------------------------

    /** Ставить уличные фонари (с настоящим светом ночью). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City|Облик") bool bStreetLamps = true;

    /** Сколько фонарей могут светить по-настоящему (остальные — просто столбы). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City|Облик") int32 MaxLitLamps = 28;

    /** Ставить деревья и кусты. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City|Облик") bool bGreenery = true;

    /** Высота одного этажа, см. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City|Облик") float FloorHeight = 300.0f;

    /** Ширина проезжей части и тротуара, см. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City|Облик") float RoadWidth = 700.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "City|Облик") float SidewalkWidth = 260.0f;

private:
    UPROPERTY() TObjectPtr<UStaticMesh> BuildingMesh;
    UPROPERTY() TObjectPtr<UStaticMesh> RoadMesh;
    UPROPERTY() TObjectPtr<UStaticMesh> FurnitureMesh;
    UPROPERTY() TObjectPtr<UStaticMesh> CylinderMesh;
    UPROPERTY() TObjectPtr<UStaticMesh> SphereMesh;
    UPROPERTY() TObjectPtr<UStaticMesh> ConeMesh;

    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> BuildingInstances;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> RoadInstances;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Ground;

    /** Наборы одинаковых деталей: по одному на пару «форма + материал». */
    UPROPERTY() TMap<FString, TObjectPtr<UInstancedStaticMeshComponent>> Instancers;

    /** Материалы города, подобранные из стартового набора. */
    UPROPERTY() TMap<FName, TObjectPtr<UMaterialInterface>> Palette;

    int32 LitLampsPlaced = 0;
    float ShownWetness = -1.0f;
    float ShownFrost = -1.0f;
    /** Чтобы у каждой вещи было своё имя, отличное от вида. */
    int32 FurnitureCounter = 0;

    /** Загрузить формы и материалы. */
    void LoadPalette();
    UMaterialInterface* Mat(FName Key) const;

    /** Получить (или создать) набор для пары «форма + материал». */
    UInstancedStaticMeshComponent* Instancer(UStaticMesh* Mesh, UMaterialInterface* Material, const FString& Key);
    /** Поставить одну деталь. Размер задаётся в САНТИМЕТРАХ, не в масштабе. */
    void Piece(UStaticMesh* Mesh, FName MaterialKey, const FVector& Centre, const FVector& SizeCm,
               const FRotator& Rotation = FRotator::ZeroRotator, bool bCollision = false);

    /**
     * Поставить деталь ИЗ ВЕЩЕСТВА: бревно, камень, глина, солома, стекло, железо.
     * Вид берётся от вещества, а объём записывается в то, из чего сложен дом.
     */
    void PieceOf(EResourceKind Substance, UStaticMesh* Mesh, const FVector& Centre, const FVector& SizeCm,
                 const FRotator& Rotation = FRotator::ZeroRotator, bool bCollision = true);

    static FName LookOf(EResourceKind Substance);

    /** Земля под поселением: блоки почвы, глины, камня. */
    UPROPERTY() TObjectPtr<class ATerrainGrid> Terrain;

    /** Высота земли в точке — по блокам, а не по плоскости. */
    float GroundAt(const FVector& At) const;

    /** Дом, который сейчас строится: в него и пишется израсходованное. */
    FBuildingInfo* UnderConstruction = nullptr;

    /** Улицы, тротуары, разметка. */
    void BuildStreets(const FVector& Origin, float Width, float Depth);
    /** Один дом: цоколь, этажи с окнами, крыша. */
    void BuildHouse(const FString& Type, const FVector& Centre, const FVector& Footprint, float Height);
    /** Сквер: трава, деревья, кусты, камни. */
    void BuildPark(const FVector& Centre, float Size);
    /** Дерево на заданном месте. */
    void PlantTree(const FVector& Base, float Scale);
    void Decor(const TCHAR* Folder, const TCHAR* Filter, const FVector& At, const FVector& Size, float Yaw, bool bCollide, bool bStretch = false);
    /** Фонарь. */
    void PlaceLamp(const FVector& Base);

    /**
     * Обставить помещение вещами.
     * Именно вещи, а не здание, дают человеку возможность что-то сделать:
     * пьют из раковины, спят в кровати, готовят на плите. Без них
     * действия висят в воздухе.
     */
    void FurnishRoom(FBuildingInfo& Building, const FString& Type);

    /** Поставить одну вещь и внести её в список дома. */
    class AFurnitureActor* PlaceFurniture(FBuildingInfo& Building, EFurnitureType Kind,
                                          const FVector& At, float YawDegrees);

    /** Положить книгу на полку. Книга — предмет, её берут в руки. */
    class ABookActor* SpawnBook(FName Subject, const FVector& At);

    /** Сколько книг разложено по городу. */
    int32 BooksPlaced = 0;

    // --- Средневековые поселения -------------------------------------------

    void BuildVillage();
    void BuildTown();

    /**
     * Разложить по земле то, что выросло: ягоды у кустов, яблоки под деревьями,
     * овощи на грядках. Мир должен быть осязаемым с первого взгляда.
     */
    void SeedVisibleHarvest();

    /** Изба: сруб, сени, очаг, соломенная крыша. */
    void BuildHut(const FString& Type, const FVector& Centre, const FVector& Footprint, float Yaw);
    void BuildHutBlocks(const FString& Type, const FVector& Centre, const FVector& Footprint, float Yaw);
    /** Городской дом: каменный низ, фахверковый верх, свес над улицей. */
    void BuildTimberHouse(const FString& Type, const FVector& Centre, const FVector& Footprint, int32 Floors, float Yaw);
    /** Отрезок городской стены с зубцами. */
    void BuildWallRun(const FVector& From, const FVector& To, float Height);
    /** Башня. */
    void BuildTower(const FVector& At, float Radius, float Height);
    /** Поле: борозды, межа. */
    void BuildField(const FVector& Centre, const FVector& Size, FName Crop);
    /** Лес: деревья, кусты, грибные места. */
    void BuildWood(const FVector& Centre, float Radius, int32 Trees);
    /** Ручей или река: русло, вода, брод. */
    void BuildStream(const FVector& From, const FVector& To, float Width);
    /** Обставить средневековое жильё. */
    void FurnishMedieval(FBuildingInfo& Building, const FString& Type);

    void SpawnBuilding(const FString& Type, const FVector& Location, const FVector& Scale);
    void RegisterPlacesInWorld();
    /** Дать человеку стартовое знание о своём районе. */
    void SeedKnowledge(ACompleteHumanNPC* NPC, const FBuildingInfo* HomeBuilding);
    /** Точка на улице, свободная от домов. */
    FVector GetStreetPoint() const;
    /** Тип места по типу здания. */
    EPlaceKind PlaceKindForBuilding(const FString& Type) const;
    void SpawnVillagers();
    void SetUpTrades(const TArray<class ACompleteHumanNPC*>& People);
    void FurnishCastle(FBuildingInfo& Building);
    TArray<TPair<EPlaceKind, FVector>> VillagePlaces;
    TArray<FPersonPlan> VillagePlans;
    TArray<int32> HouseholdBuilding;
    TArray<TObjectPtr<class AFurnitureActor>> VillageStalls;
    int32 VillageHouseholds = 0;
    FVector TorgAt = FVector::ZeroVector;
    FVector PierAt = FVector::ZeroVector;
};
