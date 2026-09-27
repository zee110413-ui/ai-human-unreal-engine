#pragma once

#include "CoreMinimal.h"
#include "HumanTypes.h"
#include "FurnitureActor.h"
#include "ClothingActor.h"
#include "Crafts.generated.h"

USTRUCT(BlueprintType)
struct FCraftPart
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Craft") EResourceKind Kind = EResourceKind::None;
    UPROPERTY(BlueprintReadOnly, Category = "Craft") float Amount = 1.0f;
};

USTRUCT(BlueprintType)
struct FCraft
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Craft") FName Id;
    UPROPERTY(BlueprintReadOnly, Category = "Craft") FString Label;
    UPROPERTY(BlueprintReadOnly, Category = "Craft") FName Skill;
    UPROPERTY(BlueprintReadOnly, Category = "Craft") FName SecondSkill;
    UPROPERTY(BlueprintReadOnly, Category = "Craft") float Difficulty = 0.3f;
    UPROPERTY(BlueprintReadOnly, Category = "Craft") float Duration = 900.0f;
    UPROPERTY(BlueprintReadOnly, Category = "Craft") float Effort = 0.35f;

    UPROPERTY(BlueprintReadOnly, Category = "Craft") EFurnitureType Station = EFurnitureType::Workbench;
    UPROPERTY(BlueprintReadOnly, Category = "Craft") TArray<FCraftPart> Inputs;

    UPROPERTY(BlueprintReadOnly, Category = "Craft") EResourceKind Output = EResourceKind::None;
    UPROPERTY(BlueprintReadOnly, Category = "Craft") float OutputAmount = 1.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Craft") EFurnitureType Builds = EFurnitureType::Chair;
    UPROPERTY(BlueprintReadOnly, Category = "Craft") bool bBuilds = false;

    /** Чем это делают. Голыми руками выходит вдвое дольше и почти всегда плохо. */
    UPROPERTY(BlueprintReadOnly, Category = "Craft") EResourceKind Tool = EResourceKind::None;

    /** Делается где угодно, а не у станка: было бы чем. */
    UPROPERTY(BlueprintReadOnly, Category = "Craft") bool bAnywhere = false;

    /** Вышла одежда, а не припас. */
    UPROPERTY(BlueprintReadOnly, Category = "Craft") EClothingKind Wears = EClothingKind::None;
};

struct FTextbook;

/**
 * Дела, вычитанные из книг.
 *
 * В движке нет списка того, что можно смастерить. Есть только книги: что в них
 * написано словами, то в городе и умеют. Здесь текст страницы разбирается на
 * «из чего», «чем», «где» и «что выйдет».
 */
class AB_API FCraftBook
{
public:
    static const TArray<FCraft>& All();
    static const FCraft* Find(FName Id);
    static void AtStation(EFurnitureType Station, TArray<const FCraft*>& Out);
    static void Anywhere(TArray<const FCraft*>& Out);
    static FString NameOfKind(EResourceKind Kind);
    static FString NameOfBuilt(EFurnitureType Type);

    static void BuildFrom(TArray<FTextbook>& Books);
    static bool ReadDeed(const FString& Text, FName Skill, FName SecondSkill, float Difficulty, FCraft& Out);
};
