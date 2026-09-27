// Improvise.h
// ---------------------------------------------------------------------------
// Сочинение фраз.
//
// Раньше на каждый случай был готовый список из трёх-четырёх строк, и люди
// повторяли их изо дня в день. Здесь фраза не выбирается, а собирается:
// берётся каркас («без чего-то как-то»), в него подставляются слова из
// словаря — подходящие по теме разговора и по настроению говорящего, — и
// ставятся в нужные падежи.
//
// Каркасов около сотни, слов — за двадцать тысяч словоформ. Двух одинаковых
// разговоров больше не будет.
// ---------------------------------------------------------------------------

#pragma once

#include "CoreMinimal.h"
#include "HumanTypes.h"
#include "Lexicon.h"

/** Обстоятельства, от которых зависит, что человек сочинит. */
struct FImproviseContext
{
    /** О чём речь. */
    EWordTopic Topic = EWordTopic::Everyday;

    /** Настроение говорящего -1..+1. Определяет окраску слов. */
    float Mood = 0.0f;

    /** Насколько близки — от этого зависит, насколько вольно он говорит. */
    float Closeness = 0.0f;

    /** Разговорчивость 0..1. Молчун скажет короче. */
    float Talkativeness = 0.5f;

    /** Сколько реплик уже прозвучало: чем дальше, тем глубже разговор. */
    int32 Turn = 0;

    /** Женщина ли говорит — для согласования прошедшего времени. */
    bool bFemale = false;

    /** Ребёнок говорит проще. */
    bool bChild = false;
};

/** Сочинитель фраз. */
class AB_API FPhrase
{
public:
    /** Сочинить реплику под речевой акт. */
    static FString Say(ESpeechAct Act, const FImproviseContext& Ctx);

    /** Подхватить чужую реплику: согласиться, возразить, уточнить. */
    static FString Respond(ESpeechAct ToAct, const FString& ToText, const FImproviseContext& Ctx);

    // =======================================================================
    //  РЕЧЬ О ЧЁМ-ТО
    //
    //  Главный путь. Человек говорит не «слова подходящей области», а о том,
    //  что у него в голове: о голоде, о рынке, о прочитанном, о соседе.
    //  Отсюда и берётся связный разговор — собеседнику есть что ответить.
    // =======================================================================

    /** Рассказать о чём-то своём. */
    static FString SayAbout(const FTalkingPoint& Point, const FImproviseContext& Ctx);

    /** Спросить собеседника об этом же. */
    static FString AskAbout(const FTalkingPoint& Point, const FImproviseContext& Ctx);

    /** Ответить по существу на то, что сказали. */
    static FString ReplyAbout(const FTalkingPoint& Theirs, bool bAgree,
                              const FImproviseContext& Ctx);

    /** О чём человеку естественно заговорить в таком положении. */
    static EWordTopic PickTopic(ENeedType PressingNeed, EEmotionType Feeling, float Hour);

    /** Тема по-русски: «о работе», «о погоде». */
    static FString TopicName(EWordTopic Topic);

private:
    /** Существительное темы, подходящее по настроению. */
    static const FLexeme* Noun(const FImproviseContext& Ctx, float MoodBias = 0.0f);
    static const FLexeme* Verb(const FImproviseContext& Ctx);
    static const FLexeme* Adj(const FImproviseContext& Ctx, float MoodBias = 0.0f);
    static const FLexeme* Adv(const FImproviseContext& Ctx, float MoodBias = 0.0f);

    /** Существительное с согласованным прилагательным в нужном падеже. */
    static FString Phrase(const FLexeme& N, const FLexeme* A, EGramCase Case, bool bPlural = false);

    /** Вводное слово: «слушай», «знаешь», «честно говоря». */
    static FString Opener(const FImproviseContext& Ctx);

    /** Хвост: «да?», «вот так», «вот и всё». */
    static FString Tail(const FImproviseContext& Ctx);

    static FString Pick(const TArray<FString>& Options);
    static FString Capitalise(const FString& Text);
};
