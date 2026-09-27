// Lexicon.h
// ---------------------------------------------------------------------------
// Язык города.
//
// Раньше речь собиралась из полутора десятков готовых строк, и разговоры
// выходили одинаковыми. Здесь лежит словарь: полторы тысячи слов с пометами
// — какая часть речи, о чём, какого рода, хорошее слово или плохое, —
// и правила, по которым из слова получаются его формы.
//
// Слово в словаре одно, а форм у него дюжина: работа, работы, работе,
// работу, работой, о работе, работы, работ, работам... Отсюда и берутся
// десятки тысяч словоформ, которыми жители распоряжаются.
//
// Человек знает не все слова. Что он знает — в его SpeechComponent;
// здесь только то, что вообще есть в языке.
// ---------------------------------------------------------------------------

#pragma once

#include "CoreMinimal.h"
#include "Lexicon.generated.h"

/** Часть речи. */
UENUM(BlueprintType)
enum class EWordClass : uint8
{
    Noun, Verb, Adjective, Adverb, Pronoun, Preposition, Conjunction, Particle, Interjection, Numeral
};

/** О чём слово. По темам подбираются слова для разговора. */
UENUM(BlueprintType)
enum class EWordTopic : uint8
{
    Everyday,   // быт
    Food,       // еда
    Home,       // дом и вещи
    Work,       // работа и ремесло
    City,       // город, улица, места
    Nature,     // природа, погода
    Body,       // тело, здоровье
    Feeling,    // чувства
    Thought,    // мысли, знание
    People,     // люди, родство
    Speech,     // разговор
    Time,       // время
    Money,      // деньги
    Movement,   // движение
    Quality,    // свойства
    Abstract,   // отвлечённое
    Child       // детское
};

/** Падеж. */
UENUM(BlueprintType)
enum class EGramCase : uint8
{
    Nom,    // кто, что
    Gen,    // кого, чего
    Dat,    // кому, чему
    Acc,    // кого, что
    Ins,    // кем, чем
    Pre     // о ком, о чём
};

/** Род и число. */
UENUM(BlueprintType)
enum class EGramGender : uint8
{
    Masculine, Feminine, Neuter, Plural
};

/** Лицо — для глаголов. */
UENUM(BlueprintType)
enum class EGramPerson : uint8
{
    First, Second, Third
};

/** Одно слово языка. */
USTRUCT(BlueprintType)
struct FLexeme
{
    GENERATED_BODY()

    /** Начальная форма. */
    UPROPERTY(BlueprintReadOnly, Category = "Lexicon") FString Base;

    UPROPERTY(BlueprintReadOnly, Category = "Lexicon") EWordClass Class = EWordClass::Noun;
    UPROPERTY(BlueprintReadOnly, Category = "Lexicon") EWordTopic Topic = EWordTopic::Everyday;
    UPROPERTY(BlueprintReadOnly, Category = "Lexicon") EGramGender Gender = EGramGender::Masculine;

    /** Хорошее слово или плохое: -1..+1. «беда» и «радость». */
    UPROPERTY(BlueprintReadOnly, Category = "Lexicon") float Valence = 0.0f;

    /** Насколько слово ходовое 0..1. Редкие усваиваются позже. */
    UPROPERTY(BlueprintReadOnly, Category = "Lexicon") float Frequency = 0.5f;

    /** Одушевлённое — влияет на винительный падеж. */
    UPROPERTY(BlueprintReadOnly, Category = "Lexicon") bool bAnimate = false;
};

/**
 * Словарь языка.
 *
 * Один на весь город: язык общий, знание языка — личное.
 */
UCLASS()
class AB_API ULexicon : public UObject
{
    GENERATED_BODY()

public:
    /** Единственный экземпляр. Строится при первом обращении. */
    static ULexicon& Get();

    /** Все слова. */
    const TArray<FLexeme>& All() const { return Words; }

    /** Сколько слов в начальной форме. */
    UFUNCTION(BlueprintCallable, Category = "Lexicon")
    int32 LemmaCount() const { return Words.Num(); }

    /** Сколько всего словоформ получается со всеми склонениями. */
    UFUNCTION(BlueprintCallable, Category = "Lexicon")
    int32 FormCount() const { return TotalForms; }

    // --- Выбор слова --------------------------------------------------------

    /** Случайное слово заданной части речи и темы. */
    const FLexeme* Random(EWordClass Class, EWordTopic Topic) const;

    /** Случайное слово части речи, любой темы. */
    const FLexeme* Random(EWordClass Class) const;

    /**
     * Слово, подходящее по настроению: при горе выберется горькое слово,
     * при радости — светлое. Tolerance — насколько строго.
     */
    const FLexeme* RandomByMood(EWordClass Class, EWordTopic Topic, float Mood, float Tolerance = 0.45f) const;

    /** Найти слово по начальной форме. */
    const FLexeme* Find(const FString& Base) const;

    /** Все слова темы. */
    void CollectTopic(EWordTopic Topic, EWordClass Class, TArray<const FLexeme*>& Out) const;

    // --- Формы слова --------------------------------------------------------

    /** Существительное в нужном падеже и числе. */
    static FString Decline(const FLexeme& Word, EGramCase Case, bool bPlural = false);

    /** Прилагательное, согласованное с существительным. */
    static FString Agree(const FLexeme& Adjective, EGramGender Gender, EGramCase Case);

    /** Краткая форма прилагательного: «рад», «весела», «грустно». */
    static FString ShortForm(const FLexeme& Adjective, EGramGender Gender);

    /** Глагол в настоящем времени. */
    static FString Conjugate(const FLexeme& Verb, EGramPerson Person, bool bPlural = false);

    /** Глагол в прошедшем времени. */
    static FString Past(const FLexeme& Verb, EGramGender Gender);

    /** Повелительное наклонение: «иди», «подожди». */
    static FString Imperative(const FLexeme& Verb, bool bPolite = false);

    /** Все формы слова — для подсчёта и для обучения. */
    static void AllForms(const FLexeme& Word, TArray<FString>& Out);

    // --- Служебное ----------------------------------------------------------

    /** Привести словоформу к начальной форме, насколько это возможно без разбора. */
    static FString Stem(const FString& Word);

    /** Род существительного по окончанию. */
    static EGramGender GuessGender(const FString& Base);

private:
    void Build();
    void Add(const TCHAR* Base, EWordClass Class, EWordTopic Topic, float Valence = 0.0f,
             float Frequency = 0.5f, bool bAnimate = false);

    UPROPERTY() TArray<FLexeme> Words;

    /** Указатели по классам и темам — чтобы не перебирать весь словарь. */
    TMap<uint16, TArray<int32>> ByClassTopic;
    TMap<uint8, TArray<int32>> ByClass;
    TMap<FString, int32> ByBase;

    int32 TotalForms = 0;
};
