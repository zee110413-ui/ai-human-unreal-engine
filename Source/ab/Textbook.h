// Textbook.h
// ---------------------------------------------------------------------------
// Учебники.
//
// До сих пор человек учился только на собственном опыте: обжёгся — узнал,
// что плита горячая. Так учится и животное. Человека от животного отличает
// то, что он может узнать нечто, чего сам не переживал, — из книги.
//
// Поэтому в книге здесь лежит настоящий текст, а не пометка «+0.1 к навыку».
// Читая страницу, человек усваивает то, что на ней написано: слова, числа,
// правила. Потом он может это применить, пересказать другому и ошибиться,
// если запомнил плохо.
// ---------------------------------------------------------------------------

#pragma once

#include "CoreMinimal.h"
#include "Lexicon.h"
#include "Textbook.generated.h"

/** Одна страница: то, что на ней написано, и то, что с неё усваивается. */
USTRUCT(BlueprintType)
struct FTextbookPage
{
    GENERATED_BODY()

    /** Название параграфа. */
    UPROPERTY(BlueprintReadOnly, Category = "Textbook") FString Title;

    /** Текст страницы — его человек и читает. */
    UPROPERTY(BlueprintReadOnly, Category = "Textbook") FString Text;

    /** Насколько трудна для понимания 0..1. */
    UPROPERTY(BlueprintReadOnly, Category = "Textbook") float Difficulty = 0.3f;

    UPROPERTY(BlueprintReadOnly, Category = "Textbook") TArray<FString> Pictures;

    /** Дело, которое описано на этой странице. Понял страницу — знаешь, как это делается. */
    UPROPERTY(BlueprintReadOnly, Category = "Textbook") FName Craft;
};

/**
 * Закладка.
 *
 * Человек не начинает книгу заново каждый раз. Он помнит, на каком месте
 * остановился, и продолжает оттуда — даже если прошла неделя.
 */
USTRUCT(BlueprintType)
struct FReadingBookmark
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Textbook") FName Subject;

    /** Какой параграф читает. */
    UPROPERTY(BlueprintReadOnly, Category = "Textbook") int32 Page = 0;

    /** Сколько слов этого параграфа уже прочитано. */
    UPROPERTY(BlueprintReadOnly, Category = "Textbook") int32 Word = 0;

    /** Сколько параграфов понято. */
    UPROPERTY(BlueprintReadOnly, Category = "Textbook") int32 Understood = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Textbook") bool bFinished = false;
};

/** Учебник. */
USTRUCT(BlueprintType)
struct FTextbook
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Textbook") FName Subject;
    UPROPERTY(BlueprintReadOnly, Category = "Textbook") FString Title;

    /** Класс, для которого книга. 0 — не школьная. */
    UPROPERTY(BlueprintReadOnly, Category = "Textbook") int32 Grade = 0;

    /** Предмет по-русски: «русский язык», «математика». */
    UPROPERTY(BlueprintReadOnly, Category = "Textbook") FString Course;

    /** Какому умению учит. */
    UPROPERTY(BlueprintReadOnly, Category = "Textbook") FName Skill;

    /** Второе умение, которое книга задевает попутно. */
    UPROPERTY(BlueprintReadOnly, Category = "Textbook") FName SecondSkill;

    /** Слова какой области встречаются в книге. */
    UPROPERTY(BlueprintReadOnly, Category = "Textbook") EWordTopic Topic = EWordTopic::Thought;

    /** Насколько трудна книга целиком. */
    UPROPERTY(BlueprintReadOnly, Category = "Textbook") float Difficulty = 0.3f;

    /** Какой навык чтения нужен, чтобы её вообще осилить. */
    UPROPERTY(BlueprintReadOnly, Category = "Textbook") float RequiredReading = 0.2f;

    UPROPERTY(BlueprintReadOnly, Category = "Textbook") TArray<FTextbookPage> Pages;
};

/** Все книги, какие есть в городе. */
class AB_API FLibrary
{
public:
    static const TArray<FTextbook>& All();

    /** Найти книгу по предмету. */
    static const FTextbook* Find(FName Subject);

    /** Книги, которые ставят в такой кабинет. */
    static void ForRoom(FName RoomKind, TArray<const FTextbook*>& Out);

    /** Сколько слов на странице — от этого зависит, сколько её читать. */
    static int32 WordCount(const FTextbookPage& Page);

    /** Сколько слов во всей книге. */
    static int32 TotalWords(const FTextbook& Book);

    /**
     * Кусок текста, который человек прочтёт за это время.
     * FromWord — с какого слова, HowMany — сколько успеет.
     */
    static FString Excerpt(const FTextbookPage& Page, int32 FromWord, int32 HowMany);

    /**
     * Учебник, до которого человек дорос: по классу, а не по возрасту.
     * Взрослый неуч начнёт с первого класса — как и должно быть.
     */
    static const FTextbook* NextFor(const FString& Course, int32 FinishedGrade);
};
