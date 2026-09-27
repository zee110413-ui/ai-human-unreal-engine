// HumanSandboxGameMode.h
// ---------------------------------------------------------------------------
// Песочница: режим, в котором можно просто нажать Play и смотреть на людей.
//
// Сам поднимает город, если его нет, и выводит на экран:
//   - часы мира и погоду;
//   - сводку по человеку, на которого вы смотрите (или ближайшему);
//   - летопись города: что за events произошло и с кем.
// ---------------------------------------------------------------------------

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "HumanTypes.h"
#include "HumanSandboxGameMode.generated.h"

class ACompleteHumanNPC;
class ACityGenerator;

UCLASS()
class AB_API AHumanSandboxGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AHumanSandboxGameMode();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    /** Создать город, если на уровне его нет. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sandbox")
    bool bAutoSpawnCity = true;

    /** Поставить солнце и небо, если на уровне нет света. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sandbox")
    bool bEnsureLighting = true;

    /** Вести солнце по небу вслед за часами города. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sandbox")
    bool bDriveSunByClock = true;

    /** Как часто (реальные секунды) разбирать решение одного из жителей. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sandbox")
    float PortraitInterval = 20.0f;

    /** Сколько людей поселить в созданном городе. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sandbox")
    int32 Population = 26;

    /**
     * Размер города в кварталах.
     *
     * Было четыре на четыре, и когда появились больница, баня, рынок,
     * управа, библиотека и пекарня, они съели половину жилья: домов осталось
     * четыре на весь город. Шесть на шесть — чтобы места хватило и людям.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sandbox")
    int32 CityBlocks = 6;

    /** Выводить сводку на экран. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sandbox")
    bool bShowOverlay = true;

    /** Писать мысли и события в лог (удобно для разбора без экрана). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sandbox")
    bool bLogChronicle = false;

    /** За кем следим. Если пусто — берётся ближайший к камере. */
    UPROPERTY(BlueprintReadWrite, Category = "Sandbox")
    TObjectPtr<ACompleteHumanNPC> Watched = nullptr;

    /** Следить за следующим человеком. */
    UFUNCTION(BlueprintCallable, Category = "Sandbox")
    void WatchNext();

    /** Кратко обо всех: одна строка на человека. */
    UFUNCTION(BlueprintCallable, Category = "Sandbox")
    FString GetCityDigest() const;

private:
    void SpawnCityIfNeeded();
    /** Подробный разбор одного человека: почему он выбрал именно это. */
    void LogPortrait();
    void EnsureLighting();
    void UpdateSun();
    void UpdateOverlay();
    void LogChronicle();

public:
    // =======================================================================
    //  КОМАНДЫ ДЛЯ ХОЗЯИНА ГОРОДА
    //
    //  Вызываются из консоли (клавиша ~ во время игры).
    //  Всё, что создаётся, — настоящие вещи: их видно, их берут в руки,
    //  их едят, и они кончаются.
    // =======================================================================

    /** Положить еду перед камерой: SpawnFood 3 */
    UFUNCTION(Exec, BlueprintCallable, Category = "Город")
    void SpawnFood(int32 Portions = 3);

    /** Воду: SpawnWater 2 */
    UFUNCTION(Exec, BlueprintCallable, Category = "Город")
    void SpawnWater(int32 Portions = 2);

    /** Деньги: SpawnCoins 50 */
    UFUNCTION(Exec, BlueprintCallable, Category = "Город")
    void SpawnCoins(int32 Amount = 50);

    /** Книгу: SpawnBook Rus1 (Rus1..Rus7, Mat1..Mat6, Alphabet, Medicine…) */
    UFUNCTION(Exec, BlueprintCallable, Category = "Город")
    void SpawnBook(const FString& Subject);

    /** Любой припас: SpawnStuff RawFood 5 */
    UFUNCTION(Exec, BlueprintCallable, Category = "Город")
    void SpawnStuff(const FString& Kind, int32 Amount = 1);

    /** Что вообще можно создать — список в лог. */
    UFUNCTION(Exec, BlueprintCallable, Category = "Город")
    void ListStuff();

    /** Показать или скрыть таблички над людьми: ShowTags 0 */
    UFUNCTION(Exec, BlueprintCallable, Category = "Город")
    void ShowTags(int32 bOn = 1);

    /**
     * Дать все знания одному человеку — сделать из него учителя.
     * Он прочтёт все книги разом и сможет показать остальным, как что делается.
     */
    UFUNCTION(Exec, BlueprintCallable, Category = "Город")
    void MakeScholar();

    UFUNCTION(Exec, BlueprintCallable, Category = "Город")
    void Speed(float Factor = 1.0f);

    UFUNCTION(Exec, BlueprintCallable, Category = "Вещество")
    void Rain(float MillimetresPerHour = 6.0f, float Hours = 2.0f);

    UFUNCTION(Exec, BlueprintCallable, Category = "Вещество")
    void Frost(float Celsius = -8.0f, float Hours = 6.0f);

    UFUNCTION(Exec, BlueprintCallable, Category = "Вещество")
    void Season(int32 DayOfYear = 130);

    UFUNCTION(Exec, BlueprintCallable, Category = "Вещество")
    void WeatherNow();

    UFUNCTION(Exec, BlueprintCallable, Category = "Вещество")
    void WhatIsThis();

    UFUNCTION(Exec, BlueprintCallable, Category = "Вещество")
    void Strike(float Joules = 500.0f, int32 Edge = 0);

    UFUNCTION(Exec, BlueprintCallable, Category = "Вещество")
    void Throw(const FString& Kind, float MetresPerSecond = 8.0f, float Wetness = -1.0f);

    UFUNCTION(Exec, BlueprintCallable, Category = "Вещество")
    void Ignite();

    UFUNCTION(Exec, BlueprintCallable, Category = "Вещество")
    void Soak(float Kilograms = 2.0f);

    UFUNCTION(Exec, BlueprintCallable, Category = "Вещество")
    void Repair(int32 Times = 1);

    UFUNCTION(Exec, BlueprintCallable, Category = "Вещество")
    void Dig(int32 Times = 1);

private:
    void SpeedUp();
    void SlowDown();
    void SpeedNormal();
    void BindHotkeys();
    void UpdateWatchTest(float RealDelta);
    ACompleteHumanNPC* PickInteresting() const;

    bool TraceView(FHitResult& OutHit, FVector& OutFrom, FVector& OutDirection) const;
    void UpdateMatterTest(float RealDelta);
    void UpdateGraspTest(float RealDelta);
    void UpdateTourTest(float RealDelta);
    void UpdateWalkTest(float RealDelta);

    bool bWalkTest = false;
    int32 WalkLeg = 0;
    TArray<FVector> WalkRoute;
    void UpdateRecording(float RealDelta);
    FVector GroundPoint(const FVector& Near) const;

    bool bGraspTest = false;
    bool bTourTest = false;
    int32 GraspStage = 0;
    float GraspClock = 0.0f;
    float GraspStageClock = 0.0f;
    float GraspLogTimer = 0.0f;
    int32 GraspItem = 0;
    FVector GraspSite = FVector::ZeroVector;
    FVector GraspForward = FVector::ForwardVector;
    FVector CameraSmooth = FVector::ZeroVector;
    bool bCameraSmoothValid = false;

    UPROPERTY()
    TArray<TObjectPtr<AActor>> GraspItems;

    UPROPERTY()
    TObjectPtr<ACompleteHumanNPC> GraspSubject;

    bool bRecording = false;
    float RecordFps = 10.0f;
    float RecordSeconds = 60.0f;
    float RecordClock = 0.0f;
    float RecordTimer = 0.0f;
    float RecordStep = 0.0f;
    int32 RecordFrame = 0;
    FString RecordFolder;
    TSharedPtr<class FClipRecorder> Recorder;
    AActor* SpawnSample(EResourceKind Kind, float WetShare, const FVector& At, const FVector& Size, const FRotator& Turn);

    bool bMatterTest = false;
    float MatterTestClock = 0.0f;
    int32 MatterTestStage = 0;
    int32 MatterTestShots = 0;
    float MatterShotTimer = 0.0f;
    FVector MatterSite = FVector::ZeroVector;

    UPROPERTY()
    TArray<TObjectPtr<AActor>> MatterSamples;

    UPROPERTY()
    TObjectPtr<class AMatterStructure> TestHouse;

    bool bHotkeysBound = false;
    bool bStartupDone = false;
    float GameSpeed = 1.0f;

    bool bWatchTest = false;
    int32 WatchShotsTotal = 30;
    int32 WatchShotsTaken = 0;
    float WatchEvery = 5.0f;
    float WatchShotTimer = 2.0f;
    float WatchSubjectTimer = 0.0f;
    float WatchExitTimer = 0.0f;
    FString WatchFolder;
    FVector WatchCamera = FVector::ZeroVector;
    bool bWatchCameraPlaced = false;

    UPROPERTY()
    TObjectPtr<ACompleteHumanNPC> WatchSubject = nullptr;

    UPROPERTY()
    TObjectPtr<ACompleteHumanNPC> LastWatchSubject = nullptr;

public:

private:
    /** Точка перед камерой игрока — туда и кладём. */
    FVector PointInFront(float Distance = 260.0f) const;

    UPROPERTY() TObjectPtr<ACityGenerator> City = nullptr;
    UPROPERTY() TObjectPtr<class ADirectionalLight> Sun = nullptr;

    float OverlayAccumulator = 0.0f;
    float ChronicleAccumulator = 0.0f;
    float PortraitAccumulator = 0.0f;
    int32 WatchIndex = 0;
    /** Сколько событий мира уже записано в летопись. */
    int32 LastEventCount = 0;
};
