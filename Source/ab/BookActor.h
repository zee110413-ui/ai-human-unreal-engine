// BookActor.h
// ---------------------------------------------------------------------------
// Книга.
//
// Отдельная вещь, а не значок на полке. Она лежит там, где её оставили,
// падает, если столкнуть, и её нельзя читать, не взяв в руки.
//
// Человек подходит, берёт книгу, садится — и только тогда читает. Пока
// книга не у него в руках, никакого знания из неё не возьмётся.
// ---------------------------------------------------------------------------

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BookActor.generated.h"

class UAffordanceComponent;
class UStaticMeshComponent;

UCLASS()
class AB_API ABookActor : public AActor
{
    GENERATED_BODY()

public:
    ABookActor();

    virtual void BeginPlay() override;

    /** Какой учебник. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Book")
    FName Subject;

    /** Обложка и страницы — три доски, чтобы издали было видно книгу. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Book")
    TObjectPtr<UStaticMeshComponent> Cover;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Book")
    TObjectPtr<UStaticMeshComponent> Pages;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Book")
    TObjectPtr<UAffordanceComponent> Affordances;

    /** У кого книга сейчас в руках. */
    UPROPERTY(BlueprintReadOnly, Category = "Book")
    TObjectPtr<AActor> HeldBy;

    /** Где книга лежала: туда её и возвращают. */
    UPROPERTY(BlueprintReadOnly, Category = "Book")
    FVector ShelfLocation = FVector::ZeroVector;

    /** Собрать вид и предложение. */
    void Setup(FName InSubject);

    /** Взять в руки. Возвращает false, если книга уже у кого-то. */
    UFUNCTION(BlueprintCallable, Category = "Book")
    bool PickUp(AActor* Who);

    /** Положить — там, где стоишь. */
    UFUNCTION(BlueprintCallable, Category = "Book")
    void PutDown();

    /** Вернуть на полку. Так делает тот, кто уважает чужое. */
    UFUNCTION(BlueprintCallable, Category = "Book")
    void ReturnToShelf();

    /** Свободна ли. */
    UFUNCTION(BlueprintCallable, Category = "Book")
    bool IsAvailable() const { return HeldBy == nullptr; }
};
