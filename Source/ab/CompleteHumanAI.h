// CompleteHumanAI.h
// ---------------------------------------------------------------------------
// Человек.
//
// Сам класс намеренно тонкий: это тело и связь с движком. Всё, что делает
// его живым, живёт в компонентах и собирается воедино в UMindComponent.
//
//   Personality  — какой он
//   Physiology   — его тело
//   Need         — чего ему не хватает
//   Emotion      — что он чувствует
//   Memory       — что он помнит
//   Skill        — что он умеет
//   Identity     — кто он, сколько ему лет и зачем он живёт
//   Social       — кого он знает и что о них думает
//   Motivation   — чего он хочет и как собирается это получить
//   Speech       — что он говорит
//   Mind         — то, что связывает всё это в одного человека
//
// Старый интерфейс (Emotions, Hunger, Energy, Memories и прочее) сохранён
// как зеркало нового состояния, чтобы ничего существующее не сломалось.
// ---------------------------------------------------------------------------

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AIController.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "HumanTypes.h"
#include "HumanWorldSubsystem.h"
#include "PhysiologyComponent.h"
#include "BodyMotorComponent.h"
#include "CompleteHumanAI.generated.h"

class AFurnitureActor;
class UPersonalityComponent;
class UNeedComponent;
class UEmotionComponent;
class UMemoryComponent;
class UIdentityComponent;
class USocialComponent;
class UMotivationComponent;
class USpeechComponent;
class UMindComponent;

// ===========================================================================
//  Совместимость: старые структуры сохранены как есть
// ===========================================================================

/** Устаревшее. Зеркало UEmotionComponent — только для чтения снаружи. */
USTRUCT(BlueprintType)
struct FEmotionState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite) float Joy = 0.5f;
    UPROPERTY(BlueprintReadWrite) float Fear = 0.2f;
    UPROPERTY(BlueprintReadWrite) float Sadness = 0.3f;
    UPROPERTY(BlueprintReadWrite) float Anger = 0.1f;
    UPROPERTY(BlueprintReadWrite) float Boredom = 0.4f;
    UPROPERTY(BlueprintReadWrite) float Tiredness = 0.3f;
};

/** Устаревшее. Зеркало UPersonalityComponent. */
USTRUCT(BlueprintType)
struct FPersonality
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite) float Openness = 0.5f;
    UPROPERTY(BlueprintReadWrite) float Conscientiousness = 0.5f;
    UPROPERTY(BlueprintReadWrite) float Extraversion = 0.5f;
    UPROPERTY(BlueprintReadWrite) float Agreeableness = 0.5f;
    UPROPERTY(BlueprintReadWrite) float Neuroticism = 0.5f;
};

/** Устаревшее. Зеркало последних эпизодов памяти. */
USTRUCT(BlueprintType)
struct FMemoryEntry
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite) FString Event;
    UPROPERTY(BlueprintReadWrite) float EmotionalImpact = 0.0f;
    UPROPERTY(BlueprintReadWrite) FDateTime Timestamp;
};

/** Устаревшее. Упрощённое зеркало отношений. */
USTRUCT(BlueprintType)
struct FSocialRelation
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite) AActor* Other = nullptr;
    UPROPERTY(BlueprintReadWrite) float Opinion = 0.0f; // -100..100
    UPROPERTY(BlueprintReadWrite) float Trust = 0.5f;
};

/** Устаревшее. Субъективное переживание. */
USTRUCT(BlueprintType)
struct FQualia
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite) FString Type;
    UPROPERTY(BlueprintReadWrite) float Intensity = 0.0f;
    UPROPERTY(BlueprintReadWrite) FString Description;
};

/** Устаревшее. Зеркало когнитивной карты. */
USTRUCT(BlueprintType)
struct FKnownPlace
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite) FString Type;
    UPROPERTY(BlueprintReadWrite) FVector Location = FVector::ZeroVector;
    UPROPERTY(BlueprintReadWrite) float Familiarity = 0.0f;
    UPROPERTY(BlueprintReadWrite) FString Source;
};

/** Дом NPC. */
USTRUCT(BlueprintType)
struct FHomeState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite) FVector Location = FVector::ZeroVector;
    UPROPERTY(BlueprintReadWrite) int32 ComfortLevel = 0;
    UPROPERTY(BlueprintReadWrite) bool bOwned = false;
    UPROPERTY(BlueprintReadWrite) float Rent = 0.0f;
    UPROPERTY(BlueprintReadWrite) TArray<FString> Furniture;
};

// ===========================================================================
//  Человек
// ===========================================================================

UCLASS()
class AB_API ACompleteHumanNPC : public ACharacter
{
    GENERATED_BODY()

public:
    ACompleteHumanNPC(const FObjectInitializer& ObjectInitializer);

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void Tick(float DeltaSeconds) override;

    // --- ПОДСИСТЕМЫ РАЗУМА --------------------------------------------------

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Human") TObjectPtr<UPersonalityComponent> PersonalityComponent;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Human") TObjectPtr<UPhysiologyComponent> PhysiologyComponent;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Human") TObjectPtr<UNeedComponent> NeedComponent;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Human") TObjectPtr<UEmotionComponent> EmotionComponent;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Human") TObjectPtr<UMemoryComponent> MemoryComponent;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Human") TObjectPtr<UIdentityComponent> IdentityComponent;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Human") TObjectPtr<USocialComponent> SocialComponent;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Human") TObjectPtr<UMotivationComponent> MotivationComponent;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Human") TObjectPtr<USpeechComponent> SpeechComponent;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Human") TObjectPtr<class UDeliberationComponent> DeliberationComponent;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Human") TObjectPtr<UMindComponent> Mind;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Human")
    TObjectPtr<UAIPerceptionComponent> PerceptionComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Human")
    TObjectPtr<class UPoseableMeshComponent> PoseBody;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Human")
    TObjectPtr<class UBodyMotorComponent> Motor;

    /**
     * Простое видимое тело — чтобы человека было видно без MetaHuman.
     * Его цвет меняется вместе с настроением: так внутреннее состояние
     * видно снаружи, даже когда человек молчит.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Human")
    TObjectPtr<class UStaticMeshComponent> PlaceholderBody;

    /** Красить тело по настроению. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Human")
    bool bTintBodyByMood = true;

    // --- ВНЕШНОСТЬ И ДВИЖЕНИЕ ТЕЛА ------------------------------------------
    //
    // Модель и анимации НЕ зашиты в код. По умолчанию берётся гуманоид,
    // который идёт в комплекте с движком, — чтобы всё работало сразу.
    // Когда появятся свои (MetaHuman, Mixamo, что угодно) — достаточно
    // подставить их в этих полях, в редакторе или в блюпринте-наследнике.
    // Ни одной строчки кода менять не придётся.

    /** Скелетная модель. Пусто — берётся встроенная в движок. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Human|Внешность")
    TObjectPtr<class USkeletalMesh> CharacterMesh;

    /** Смещение и поворот модели относительно капсулы. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Human|Внешность")
    FVector MeshOffset = FVector(0.0f, 0.0f, -88.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Human|Внешность")
    FRotator MeshRotation = FRotator(0.0f, -90.0f, 0.0f);

    /**
     * Схема анимации. Если задать её, она возьмёт управление на себя,
     * а одиночные анимации ниже использоваться не будут.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Human|Анимация")
    TSubclassOf<class UAnimInstance> AnimationBlueprint;

    /** Анимации по состояниям. Используются, если схема не задана. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Human|Анимация")
    TObjectPtr<class UAnimSequence> IdleAnimation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Human|Анимация")
    TObjectPtr<class UAnimSequence> WalkAnimation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Human|Анимация")
    TObjectPtr<class UAnimSequence> RunAnimation;

    /** Сон, сидение, разговор — если есть подходящие анимации. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Human|Анимация")
    TObjectPtr<class UAnimSequence> SleepAnimation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Human|Анимация")
    TObjectPtr<class UAnimSequence> SitAnimation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Human|Анимация")
    TObjectPtr<class UAnimSequence> TalkAnimation;

    /** Скорость (см/с), выше которой включается бег. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Human|Анимация")
    float RunThreshold = 280.0f;

    /** Подобрать и запустить анимацию под текущее состояние. */
    void UpdateAnimation();

    // --- ТЕЛО И ДВИЖЕНИЕ ----------------------------------------------------

    /** Пойти в точку. Работает и без навигационной сетки (тогда — напрямую). */
    void RequestMoveTo(const FVector& Destination);
    void StopMoving();
    bool IsMoving() const { return bMoveRequested; }
    bool BodyLeadsFeet() const;
    void FallDown(const FVector& Direction, float Speed, const FVector* LandAt = nullptr);
    void BeginFalling();
    FVector PostureGoal() const { return bPostureMoving ? PostureTarget : GetActorLocation(); }
    void SettleAt(const FVector& Where);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Human|Movement")
    float BaseWalkSpeed = 135.0f;

    // --- ДОМ ----------------------------------------------------------------

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Human|Home")
    FHomeState Home;

    UFUNCTION(BlueprintCallable, Category = "Human|Home") bool HasHome() const;
    UFUNCTION(BlueprintCallable, Category = "Human|Home") bool IsAtHome() const;
    UFUNCTION(BlueprintCallable, Category = "Human|Home") FVector GetHomeLocation() const { return Home.Location; }
    UFUNCTION(BlueprintCallable, Category = "Human|Home") void SetHome(const FVector& Location);

    // --- СОБЫТИЯ ------------------------------------------------------------

    /** Воспринять событие мира. */
    void PerceiveWorldEvent(const FWorldEvent& Event);

    /** Причинить боль. */
    UFUNCTION(BlueprintCallable, Category = "Human")
    void Hurt(AActor* By, float Severity, bool bIntentional);

    /** Помочь. */
    UFUNCTION(BlueprintCallable, Category = "Human")
    void Help(AActor* By, float Magnitude);

    /** Человек умер. */
    void OnDied();

    void Revive(const FVector& At);

    UFUNCTION(BlueprintCallable, Category = "Human")
    bool IsAlive() const;

    /** Имя. */
    UFUNCTION(BlueprintCallable, Category = "Human")
    FString GetPersonName() const;

    /** Полная сводка состояния — удобно вывести на экран. */
    UFUNCTION(BlueprintCallable, Category = "Human")
    FString GetStatusReport() const;

    /** Что человек думает прямо сейчас. */
    UFUNCTION(BlueprintCallable, Category = "Human")
    FString GetCurrentThought() const;

    // --- ВОСПРИЯТИЕ ---------------------------------------------------------

    UFUNCTION()
    void OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

    // =======================================================================
    //  СОВМЕСТИМОСТЬ СО СТАРЫМ КОДОМ
    //  Эти поля обновляются автоматически и отражают настоящее состояние.
    // =======================================================================

    UPROPERTY(BlueprintReadOnly, Category = "Legacy") FEmotionState Emotions;
    UPROPERTY(BlueprintReadOnly, Category = "Legacy") FPersonality Personality;
    UPROPERTY(BlueprintReadOnly, Category = "Legacy") float Hunger = 50.0f;
    UPROPERTY(BlueprintReadOnly, Category = "Legacy") float Energy = 80.0f;
    UPROPERTY(BlueprintReadOnly, Category = "Legacy") bool bIsSleeping = false;
    UPROPERTY(BlueprintReadOnly, Category = "Legacy") TArray<FMemoryEntry> Memories;
    UPROPERTY(BlueprintReadOnly, Category = "Legacy") TArray<FSocialRelation> SocialRelationList;
    UPROPERTY(BlueprintReadOnly, Category = "Legacy") TArray<FKnownPlace> KnownPlaces;
    UPROPERTY(BlueprintReadOnly, Category = "Legacy") TArray<FQualia> QualiaList;
    UPROPERTY(BlueprintReadWrite, Category = "Legacy") TArray<FString> UnconsciousDesires;
    UPROPERTY(BlueprintReadOnly, Category = "Legacy") FString CurrentJob = TEXT("Безработный");

    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> SkinMaterial;

    /** Устаревшее: выбрать профессию. */
    UFUNCTION(BlueprintCallable, Category = "Legacy") void ChooseJob();
    /** Устаревшее: записать воспоминание. */
    UFUNCTION(BlueprintCallable, Category = "Legacy") void AddMemory(const FString& Event, float Impact);
    /** Устаревшее: изменить отношение. */
    UFUNCTION(BlueprintCallable, Category = "Legacy") void UpdateSocialRelation(AActor* Other, float DeltaOpinion);
    /** Устаревшее: добавить место на карту. */
    UFUNCTION(BlueprintCallable, Category = "Legacy") void AddKnownPlace(const FString& Type, const FVector& Location, const FString& Source);
    /** Устаревшее: найти ближайшее известное место. */
    UFUNCTION(BlueprintCallable, Category = "Legacy") FVector FindNearestKnownPlace(const FString& Type);
    /** Устаревшее: породить квалиа. */
    UFUNCTION(BlueprintCallable, Category = "Legacy") void GenerateQualia(const FString& Type, float Intensity, const FString& Context);
    /** Устаревшее: внешность. */
    UFUNCTION(BlueprintCallable, Category = "Legacy") void GenerateRandomAppearance();
    /** Устаревшее: заснуть. */
    UFUNCTION(BlueprintCallable, Category = "Legacy") void Sleep();
    /** Устаревшее: проснуться. */
    UFUNCTION(BlueprintCallable, Category = "Legacy") void WakeUp();
    /** Устаревшее: пойти домой. */
    UFUNCTION(BlueprintCallable, Category = "Legacy") void GoHome();

protected:
    void SetupPerception();
    /** Поставить модель и анимации: свои, если заданы, иначе встроенные. */
    void SetupAppearance();

    /**
     * Посмотреть в сторону и узнать, насколько далеко там свободно.
     * Это и есть всё «знание о дороге», которое у человека есть.
     */
    float LookAhead(const FVector& Direction, float Distance, FVector& OutNormal) const;

    /** Куда человек сейчас повёрнут по ходу движения. */
    FVector Heading = FVector::ForwardVector;

    /** С какой стороны обходит препятствие: 0 — не обходит. */
    int32 WallSide = 0;
    float WallFollowTime = 0.0f;

    /** Отслеживание заторов: где был и сколько стоит на месте. */
    FVector LastStuckCheckPos = FVector::ZeroVector;
    float StuckTime = 0.0f;

    /** Сколько прошёл с прошлой отметки пути. */
    float TrailSinceLastCrumb = 0.0f;

    // --- Память дороги ------------------------------------------------------
    /** Откуда и когда вышел — чтобы потом запомнить дорогу целиком. */
    FVector TravelStart = FVector::ZeroVector;
    float TravelBeganAt = 0.0f;
    /** Крошки, которые остаются по дороге и станут выученным маршрутом. */
    TArray<FVector> Trail;
    /** Вспомненная дорога: подсказка, а не рельсы. */
    TArray<FVector> RecalledPath;
    int32 PathIndex = 0;

    /** Что сейчас проигрывается — чтобы не перезапускать одно и то же. */
    UPROPERTY(Transient)
    TObjectPtr<class UAnimSequence> CurrentAnimation = nullptr;
    virtual void MoveBlockedBy(const FHitResult& Impact) override;

    /** Сесть и встать — движением, а не скачком. */
    void MovePosture(float DeltaSeconds);
    void TakeSeat();
    void LeaveSeat();

    FVector PostureTarget = FVector::ZeroVector;
    bool bPostureMoving = false;
    TWeakObjectPtr<AFurnitureActor> SeatedOn;

    /** Периодический осмотр окрестностей — работает и без системы восприятия. */
    void ScanSurroundings();
    void NoticeThingsAround();
    /** Заметить места, мимо которых проходишь, и занести их на свою карту. */
    void DiscoverPlacesNearby();
    /** Обновить цвет тела по настроению. */
    void UpdateBodyTint();
    /** Обновить старые поля из нового состояния. */
    void SyncLegacyMirror();
    /** Ручное движение, если навигационной сетки нет. */
    void UpdateDirectSteering(float DeltaSeconds);

private:
    /** Куда идём. */
    FVector MoveTarget = FVector::ZeroVector;
    bool bMoveRequested = false;
    /** Навигация не сработала — идём напрямую. */
    bool bDirectSteering = false;

    float ScanAccumulator = 0.0f;
    float StatureFactor = 1.0f;
    float GrownScale = -1.0f;
    void UpdateGrowth();
    float GroundAccumulator = 0.0f;
    float TrampleTimer = 0.0f;
    float MirrorAccumulator = 0.0f;
    float StrideTravel = 0.0f;
    float FootingFeelTimer = 0.0f;
    float GroundEffort = 1.0f;
    bool bLeftFootNext = true;
    bool bWasInMud = false;

    void UpdateFooting(float DeltaSeconds);
    void UpdateHandsMovement(float DeltaSeconds);

    /** Сколько ещё висеть реплике над головой. */
    float SpeechTagTimeLeft = 0.0f;

    /** Материал видимого тела. */
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> BodyMaterial;

    bool bDeathHandled = false;

public:
    // --- ВОСПРИЯТИЕ ---------------------------------------------------------
    //
    // Человек видит только то, что перед ним, и только если между ними нет
    // стены. Раньше хватало оказаться в радиусе — и люди замечали друг
    // друга сквозь дом, затылком, из соседнего квартала.

    /** Радиус, в котором человек замечает других (см). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Human|Восприятие")
    float SightRadius = 2600.0f;

    /** Половина угла обзора в градусах. У человека около 100. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Human|Восприятие")
    float SightHalfAngle = 100.0f;

    /** Насколько далеко слышно то, чего не видно (см). Звук огибает стены. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Human|Восприятие")
    float HearingRadius = 750.0f;

    /** Не загораживает ли что-нибудь. */
    UFUNCTION(BlueprintCallable, Category = "Human|Восприятие")
    bool HasLineOfSight(const AActor* Other) const;

    /**
     * Могу ли я дотянуться до этого места, или между нами стена.
     * Стоять в двух метрах от плиты, но снаружи дома, — не значит быть у плиты.
     */
    UFUNCTION(BlueprintCallable, Category = "Human|Восприятие")
    bool CanReachPoint(const FVector& Point) const;

    // --- ЧТО ВИДНО СО СТОРОНЫ -----------------------------------------------

    /**
     * Последняя сказанная вслух реплика — её видно над головой.
     *
     * Без этого со стороны город был немым: люди ходили молча, а вся речь
     * оставалась в логах.
     */
    UPROPERTY(BlueprintReadOnly, Category = "Human|Вид")
    FString LastSpokenLine;

    /** Показывать ли надписи над людьми. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Human|Вид")
    bool bShowTags = true;

    /** Сказать вслух — реплика повисит над головой несколько секунд. */
    UFUNCTION(BlueprintCallable, Category = "Human|Вид")
    void ShowSpeech(const FString& Text);

    /** Обновить надписи. */
    void UpdateTags(float DeltaSeconds);

    // --- ТЕЛО ---------------------------------------------------------------

    /** Что человек несёт в руках. Пусто — руки свободны. */
    UPROPERTY(BlueprintReadOnly, Category = "Human|Тело")
    TObjectPtr<AActor> CarriedItem;

    /** Стоит, сидит или лежит. */
    UPROPERTY(BlueprintReadOnly, Category = "Human|Тело")
    EPosture Posture = EPosture::Standing;

    /** Где он сидел или лежал — чтобы встать обратно. */
    UPROPERTY(BlueprintReadOnly, Category = "Human|Тело")
    FVector PostureOrigin = FVector::ZeroVector;

    /** Сесть, лечь, встать. Меняет и положение тела, и его возможности. */
    UFUNCTION(BlueprintCallable, Category = "Human|Тело")
    void SetPosture(EPosture NewPosture, const FVector& At);

    /** Взять вещь в руки. */
    UFUNCTION(BlueprintCallable, Category = "Human|Тело")
    bool TakeIntoHands(AActor* Item);

    /** Выпустить из рук. */
    UFUNCTION(BlueprintCallable, Category = "Human|Тело")
    void ReleaseFromHands(bool bReturnToPlace);

    bool BeginTake(AActor* Item);
    void LetGo(bool bReturnToPlace);
    float GetGroundEffort() const { return GroundEffort; }
    float BodyMassKg() const;
    bool AreHandsBusy() const;

    /** Держу ли я именно эту вещь. */
    UFUNCTION(BlueprintCallable, Category = "Human|Тело")
    bool IsHolding(const AActor* Item) const { return CarriedItem == Item; }

    /** Что на нём надето. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Human|Одежда")
    TArray<TObjectPtr<class AClothingActor>> Clothes;

    bool Wear(class AClothingActor* Garment);
    void Undress(class AClothingActor* Garment);
    float ClothingWarmth() const;
    float ClothingDirt() const;
    void AdvanceClothes(float GameDelta, bool bWorking);

    /**
     * Замечает ли человек этого другого вообще.
     * Учитывает расстояние, поле зрения, стены и слух.
     * OutSense — чем именно заметил: «Sight» или «Sound».
     */
    bool CanPerceive(const AActor* Other, float& OutSalience, FName& OutSense) const;
};
