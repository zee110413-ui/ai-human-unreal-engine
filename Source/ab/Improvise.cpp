// Improvise.cpp

#include "Improvise.h"

// ---------------------------------------------------------------------------
//  Мелочи
// ---------------------------------------------------------------------------

FString FPhrase::Pick(const TArray<FString>& Options)
{
    return Options.Num() > 0 ? Options[FMath::RandRange(0, Options.Num() - 1)] : FString();
}

FString FPhrase::Capitalise(const FString& Text)
{
    if (Text.IsEmpty())
    {
        return Text;
    }
    return Text.Left(1).ToUpper() + Text.Mid(1);
}

const FLexeme* FPhrase::Noun(const FImproviseContext& Ctx, float MoodBias)
{
    const EWordTopic T = Ctx.bChild ? EWordTopic::Child : Ctx.Topic;
    return ULexicon::Get().RandomByMood(EWordClass::Noun, T, Ctx.Mood + MoodBias, 0.4f);
}

const FLexeme* FPhrase::Verb(const FImproviseContext& Ctx)
{
    const EWordTopic T = Ctx.bChild ? EWordTopic::Child : Ctx.Topic;
    const FLexeme* V = ULexicon::Get().Random(EWordClass::Verb, T);
    return V ? V : ULexicon::Get().Random(EWordClass::Verb);
}

const FLexeme* FPhrase::Adj(const FImproviseContext& Ctx, float MoodBias)
{
    return ULexicon::Get().RandomByMood(EWordClass::Adjective, EWordTopic::Quality,
                                        Ctx.Mood + MoodBias, 0.35f);
}

const FLexeme* FPhrase::Adv(const FImproviseContext& Ctx, float MoodBias)
{
    return ULexicon::Get().RandomByMood(EWordClass::Adverb, EWordTopic::Quality,
                                        Ctx.Mood + MoodBias, 0.35f);
}

FString FPhrase::Phrase(const FLexeme& N, const FLexeme* A, EGramCase Case, bool bPlural)
{
    const FString Head = ULexicon::Decline(N, Case, bPlural);
    if (!A)
    {
        return Head;
    }

    const EGramGender G = bPlural ? EGramGender::Plural : N.Gender;
    return ULexicon::Agree(*A, G, Case) + TEXT(" ") + Head;
}

FString FPhrase::Opener(const FImproviseContext& Ctx)
{
    if (Ctx.bChild)
    {
        return Pick({ TEXT(""), TEXT("а вот"), TEXT("смотри"), TEXT("ой") });
    }

    // Чем ближе люди, тем меньше церемоний в начале.
    if (Ctx.Closeness > 0.55f)
    {
        return Pick({ TEXT(""), TEXT("слушай"), TEXT("знаешь"), TEXT("вот что"),
                      TEXT("да ладно"), TEXT("честно говоря"), TEXT("между нами") });
    }
    if (Ctx.Closeness < 0.15f)
    {
        return Pick({ TEXT(""), TEXT(""), TEXT("простите"), TEXT("извините"),
                      TEXT("вот"), TEXT("а"), TEXT("скажите") });
    }
    return Pick({ TEXT(""), TEXT(""), TEXT("знаете"), TEXT("вот"), TEXT("а ведь"),
                  TEXT("по-моему"), TEXT("вообще-то") });
}

FString FPhrase::Tail(const FImproviseContext& Ctx)
{
    // Хвост появляется не всегда: постоянные «да?» звучат навязчиво.
    if (FMath::FRand() > 0.34f)
    {
        return FString();
    }

    if (Ctx.Mood < -0.3f)
    {
        return Pick({ TEXT("вот так"), TEXT("и всё"), TEXT("ну да ладно"),
                      TEXT("что уж теперь"), TEXT("такие дела") });
    }
    if (Ctx.Mood > 0.35f)
    {
        return Pick({ TEXT("правда"), TEXT("вот честно"), TEXT("да?"),
                      TEXT("хорошо ведь"), TEXT("а?") });
    }
    return Pick({ TEXT("да?"), TEXT("вот"), TEXT("или как"), TEXT("наверное"),
                  TEXT("не знаю"), TEXT("как думаешь") });
}

// ---------------------------------------------------------------------------
//  Тема
// ---------------------------------------------------------------------------

EWordTopic FPhrase::PickTopic(ENeedType PressingNeed, EEmotionType Feeling, float Hour)
{
    // Человек говорит о том, что его сейчас занимает. Голодный — о еде,
    // уставший — об отдыхе, влюблённый — о людях.
    switch (PressingNeed)
    {
    case ENeedType::Hunger:
    case ENeedType::Thirst:       return EWordTopic::Food;
    case ENeedType::Sleep:
    case ENeedType::Comfort:      return EWordTopic::Home;
    case ENeedType::Money:
    case ENeedType::Achievement:  return EWordTopic::Work;
    case ENeedType::Health:
    case ENeedType::Hygiene:      return EWordTopic::Body;
    case ENeedType::SocialContact:
    case ENeedType::Belonging:
    case ENeedType::Intimacy:     return EWordTopic::People;
    case ENeedType::Novelty:      return EWordTopic::City;
    case ENeedType::Beauty:       return EWordTopic::Nature;
    case ENeedType::Competence:   return EWordTopic::Thought;
    default: break;
    }

    switch (Feeling)
    {
    case EEmotionType::Sadness:
    case EEmotionType::Nostalgia:
    case EEmotionType::Loneliness: return EWordTopic::Feeling;
    case EEmotionType::Curiosity:
    case EEmotionType::Awe:        return EWordTopic::Thought;
    case EEmotionType::Affection:
    case EEmotionType::Love:       return EWordTopic::People;
    default: break;
    }

    // Не о чем говорить — говорят о погоде и о времени. Так у всех.
    if (Hour < 9.0f || Hour > 21.0f)
    {
        return EWordTopic::Time;
    }
    static const EWordTopic Idle[] = {
        EWordTopic::Nature, EWordTopic::City, EWordTopic::Everyday,
        EWordTopic::Time, EWordTopic::People, EWordTopic::Home
    };
    return Idle[FMath::RandRange(0, UE_ARRAY_COUNT(Idle) - 1)];
}

FString FPhrase::TopicName(EWordTopic Topic)
{
    switch (Topic)
    {
    case EWordTopic::Food:     return TEXT("о еде");
    case EWordTopic::Home:     return TEXT("о доме");
    case EWordTopic::Work:     return TEXT("о работе");
    case EWordTopic::City:     return TEXT("о городе");
    case EWordTopic::Nature:   return TEXT("о погоде");
    case EWordTopic::Body:     return TEXT("о здоровье");
    case EWordTopic::Feeling:  return TEXT("о наболевшем");
    case EWordTopic::Thought:  return TEXT("о всяком");
    case EWordTopic::People:   return TEXT("о людях");
    case EWordTopic::Speech:   return TEXT("о разговорах");
    case EWordTopic::Time:     return TEXT("о времени");
    case EWordTopic::Money:    return TEXT("о деньгах");
    case EWordTopic::Movement: return TEXT("о дороге");
    case EWordTopic::Child:    return TEXT("о своём, детском");
    default:                   return TEXT("о жизни");
    }
}

// ---------------------------------------------------------------------------
//  СОЧИНЕНИЕ
//
//  Каркас + слова + падежи. Каркасы намеренно бытовые: люди в разговоре
//  не строят сложных предложений, они говорят короткими оборотами.
// ---------------------------------------------------------------------------

FString FPhrase::Say(ESpeechAct Act, const FImproviseContext& Ctx)
{
    const FLexeme* N1 = Noun(Ctx);
    const FLexeme* N2 = Noun(Ctx, FMath::FRandRange(-0.4f, 0.4f));
    const FLexeme* V1 = Verb(Ctx);
    const FLexeme* A1 = Adj(Ctx);
    const FLexeme* D1 = Adv(Ctx);

    if (!N1 || !V1 || !A1 || !D1)
    {
        return FString();
    }

    const EGramGender Me = Ctx.bFemale ? EGramGender::Feminine : EGramGender::Masculine;
    FString Body;

    // Ребёнок говорит из двух-трёх слов — не потому, что ему так положено,
    // а потому что длинную фразу он ещё не удержит.
    if (Ctx.bChild)
    {
        Body = Pick({
            FString::Printf(TEXT("%s %s"), *ULexicon::Decline(*N1, EGramCase::Nom), *ULexicon::Decline(*N2, EGramCase::Nom)),
            FString::Printf(TEXT("вот %s"), *ULexicon::Decline(*N1, EGramCase::Nom)),
            FString::Printf(TEXT("я %s"), *ULexicon::Conjugate(*V1, EGramPerson::First)),
            FString::Printf(TEXT("%s %s"), *ULexicon::Agree(*A1, N1->Gender, EGramCase::Nom), *ULexicon::Decline(*N1, EGramCase::Nom)),
            FString::Printf(TEXT("а где %s"), *ULexicon::Decline(*N1, EGramCase::Nom))
        });
        return Capitalise(Body);
    }

    switch (Act)
    {
    // --- Болтовня ни о чём ------------------------------------------------
    case ESpeechAct::SmallTalk:
    case ESpeechAct::ShareNews:
        Body = Pick({
            FString::Printf(TEXT("%s нынче %s"),
                *ULexicon::Decline(*N1, EGramCase::Nom), *ULexicon::ShortForm(*A1, N1->Gender)),
            FString::Printf(TEXT("в %s %s"),
                *ULexicon::Decline(*N1, EGramCase::Pre), *D1->Base),
            FString::Printf(TEXT("%s бывает %s"),
                *ULexicon::Decline(*N1, EGramCase::Nom), *ULexicon::Agree(*A1, N1->Gender, EGramCase::Nom)),
            FString::Printf(TEXT("что ни %s, то %s"),
                *ULexicon::Decline(*N1, EGramCase::Nom), *ULexicon::Decline(*N2, EGramCase::Nom)),
            FString::Printf(TEXT("говорят, %s %s"),
                *ULexicon::Decline(*N1, EGramCase::Nom), *ULexicon::Conjugate(*V1, EGramPerson::Third)),
            FString::Printf(TEXT("без %s %s"),
                *ULexicon::Decline(*N1, EGramCase::Gen), *D1->Base),
            FString::Printf(TEXT("после %s всегда %s"),
                *ULexicon::Decline(*N1, EGramCase::Gen), *D1->Base),
            FString::Printf(TEXT("у нас тут %s"), *Phrase(*N1, A1, EGramCase::Nom)),
            FString::Printf(TEXT("%s — дело такое"), *ULexicon::Decline(*N1, EGramCase::Nom)),
            FString::Printf(TEXT("с %s нынче %s"),
                *ULexicon::Decline(*N1, EGramCase::Ins), *D1->Base)
        });
        break;

    // --- Вопрос ------------------------------------------------------------
    case ESpeechAct::Question:
        Body = Pick({
            FString::Printf(TEXT("ты %s %s?"),
                *ULexicon::Conjugate(*V1, EGramPerson::Second), *ULexicon::Decline(*N1, EGramCase::Acc)),
            FString::Printf(TEXT("а как %s?"), *ULexicon::Decline(*N1, EGramCase::Nom)),
            FString::Printf(TEXT("у тебя %s как?"), *ULexicon::Decline(*N1, EGramCase::Nom)),
            FString::Printf(TEXT("почему %s такая %s?"),
                *ULexicon::Decline(*N1, EGramCase::Nom), *ULexicon::Decline(*N2, EGramCase::Nom)),
            FString::Printf(TEXT("не знаешь, где %s?"), *ULexicon::Decline(*N1, EGramCase::Nom)),
            FString::Printf(TEXT("тебе не %s без %s?"),
                *D1->Base, *ULexicon::Decline(*N1, EGramCase::Gen)),
            FString::Printf(TEXT("а %s тебе зачем?"), *ULexicon::Decline(*N1, EGramCase::Nom)),
            FString::Printf(TEXT("ты %s когда-нибудь %s?"),
                *ULexicon::Conjugate(*V1, EGramPerson::Second), *ULexicon::Decline(*N1, EGramCase::Acc))
        });
        break;

    // --- Жалоба ------------------------------------------------------------
    case ESpeechAct::Complain:
        Body = Pick({
            FString::Printf(TEXT("с %s совсем беда"), *ULexicon::Decline(*N1, EGramCase::Ins)),
            FString::Printf(TEXT("мне %s от %s"),
                *Adv(Ctx, -0.6f)->Base, *ULexicon::Decline(*N1, EGramCase::Gen)),
            FString::Printf(TEXT("%s меня вымотала"), *ULexicon::Decline(*N1, EGramCase::Nom)),
            FString::Printf(TEXT("сколько ни %s, всё одно"), *ULexicon::Conjugate(*V1, EGramPerson::First)),
            FString::Printf(TEXT("не хватает мне %s"), *ULexicon::Decline(*N1, EGramCase::Gen)),
            FString::Printf(TEXT("%s, а толку никакого"), *ULexicon::Past(*V1, Me)),
            FString::Printf(TEXT("%s нынче %s"),
                *ULexicon::Decline(*N1, EGramCase::Nom), *ULexicon::ShortForm(*Adj(Ctx, -0.7f), N1->Gender))
        });
        break;

    // --- Похвала -----------------------------------------------------------
    case ESpeechAct::Compliment:
        Body = Pick({
            FString::Printf(TEXT("у тебя %s"), *Phrase(*N1, Adj(Ctx, 0.7f), EGramCase::Nom)),
            FString::Printf(TEXT("с тобой %s"), *Adv(Ctx, 0.7f)->Base),
            FString::Printf(TEXT("ты %s лучше всех"), *ULexicon::Conjugate(*V1, EGramPerson::Second)),
            FString::Printf(TEXT("умеешь ты %s"), *V1->Base),
            FString::Printf(TEXT("хорошая у тебя %s"), *ULexicon::Decline(*N1, EGramCase::Nom))
        });
        break;

    // --- Хвастовство -------------------------------------------------------
    case ESpeechAct::Boast:
        Body = Pick({
            FString::Printf(TEXT("я %s лучше многих"), *ULexicon::Conjugate(*V1, EGramPerson::First)),
            FString::Printf(TEXT("у меня %s"), *Phrase(*N1, Adj(Ctx, 0.7f), EGramCase::Nom)),
            FString::Printf(TEXT("я это %s — и ничего"), *ULexicon::Past(*V1, Me)),
            FString::Printf(TEXT("мне %s не впервой"), *ULexicon::Decline(*N1, EGramCase::Nom))
        });
        break;

    // --- Совет -------------------------------------------------------------
    case ESpeechAct::Advise:
        Body = Pick({
            FString::Printf(TEXT("%s %s — оно вернее"),
                *ULexicon::Imperative(*V1), *ULexicon::Decline(*N1, EGramCase::Acc)),
            FString::Printf(TEXT("без %s не берись"), *ULexicon::Decline(*N1, EGramCase::Gen)),
            FString::Printf(TEXT("с %s не тяни"), *ULexicon::Decline(*N1, EGramCase::Ins)),
            FString::Printf(TEXT("на %s не надейся"), *ULexicon::Decline(*N1, EGramCase::Acc)),
            FString::Printf(TEXT("%s — и всё пойдёт"), *ULexicon::Imperative(*V1))
        });
        break;

    // --- Утешение ----------------------------------------------------------
    case ESpeechAct::Console:
        Body = Pick({
            FString::Printf(TEXT("%s пройдёт"), *ULexicon::Decline(*N1, EGramCase::Nom)),
            FString::Printf(TEXT("бывает и %s"), *Adv(Ctx, -0.5f)->Base),
            FString::Printf(TEXT("не одна ты с %s"), *ULexicon::Decline(*N1, EGramCase::Ins)),
            FString::Printf(TEXT("%s — не навсегда"), *ULexicon::Decline(*N1, EGramCase::Nom)),
            TEXT("это ещё не самое худшее")
        });
        break;

    // --- Шутка -------------------------------------------------------------
    case ESpeechAct::Joke:
        Body = Pick({
            FString::Printf(TEXT("%s у нас как %s — не поймёшь"),
                *ULexicon::Decline(*N1, EGramCase::Nom), *ULexicon::Decline(*N2, EGramCase::Nom)),
            FString::Printf(TEXT("я бы %s, да %s не пускает"),
                *V1->Base, *ULexicon::Decline(*N2, EGramCase::Nom)),
            FString::Printf(TEXT("%s — оно, конечно, %s"),
                *ULexicon::Decline(*N1, EGramCase::Nom), *D1->Base),
            FString::Printf(TEXT("хоть %s на %s меняй"),
                *ULexicon::Decline(*N1, EGramCase::Acc), *ULexicon::Decline(*N2, EGramCase::Acc))
        });
        break;

    // --- Просьба -----------------------------------------------------------
    case ESpeechAct::Request:
        Body = Pick({
            FString::Printf(TEXT("не дашь ли %s"), *ULexicon::Decline(*N1, EGramCase::Gen)),
            FString::Printf(TEXT("помоги мне с %s"), *ULexicon::Decline(*N1, EGramCase::Ins)),
            FString::Printf(TEXT("мне бы %s"), *ULexicon::Decline(*N1, EGramCase::Gen)),
            FString::Printf(TEXT("%s, будь добр"), *ULexicon::Imperative(*V1))
        });
        break;

    // --- Предложение помощи ------------------------------------------------
    case ESpeechAct::Offer:
        Body = Pick({
            FString::Printf(TEXT("давай я %s"), *ULexicon::Conjugate(*V1, EGramPerson::First)),
            FString::Printf(TEXT("могу помочь с %s"), *ULexicon::Decline(*N1, EGramCase::Ins)),
            FString::Printf(TEXT("у меня есть %s, бери"), *ULexicon::Decline(*N1, EGramCase::Nom)),
            TEXT("если что — говори, не стесняйся")
        });
        break;

    // --- Признание ---------------------------------------------------------
    case ESpeechAct::Confess:
        Body = Pick({
            FString::Printf(TEXT("я давно %s про %s"),
                *ULexicon::Past(*Verb(Ctx), Me), *ULexicon::Decline(*N1, EGramCase::Acc)),
            FString::Printf(TEXT("мне %s это говорить"), *Adv(Ctx, -0.5f)->Base),
            FString::Printf(TEXT("%s у меня на душе"), *Phrase(*N1, Adj(Ctx, -0.5f), EGramCase::Nom)),
            TEXT("я не всё тебе рассказал, если честно")
        });
        break;

    // --- Сплетня -----------------------------------------------------------
    case ESpeechAct::Gossip:
        Body = Pick({
            FString::Printf(TEXT("говорят про %s всякое"), *ULexicon::Decline(*N1, EGramCase::Acc)),
            FString::Printf(TEXT("будто %s там %s"),
                *ULexicon::Decline(*N1, EGramCase::Nom), *ULexicon::Past(*V1, EGramGender::Masculine)),
            FString::Printf(TEXT("слышал я про %s — не поверишь"), *ULexicon::Decline(*N1, EGramCase::Acc))
        });
        break;

    // --- Угроза ------------------------------------------------------------
    case ESpeechAct::Threaten:
        Body = Pick({
            FString::Printf(TEXT("не доводи до %s"), *ULexicon::Decline(*Noun(Ctx, -0.8f), EGramCase::Gen)),
            FString::Printf(TEXT("я тебе не %s"), *ULexicon::Decline(*N1, EGramCase::Nom)),
            TEXT("ещё раз — и разговор будет другой")
        });
        break;

    // --- Оскорбление -------------------------------------------------------
    case ESpeechAct::Insult:
        Body = Pick({
            FString::Printf(TEXT("%s ты, вот что"), *ULexicon::Agree(*Adj(Ctx, -0.9f), Me, EGramCase::Nom)),
            FString::Printf(TEXT("от тебя одна %s"), *ULexicon::Decline(*Noun(Ctx, -0.8f), EGramCase::Nom)),
            TEXT("с тобой говорить — только время терять")
        });
        break;

    // --- Отказ -------------------------------------------------------------
    case ESpeechAct::Refuse:
        Body = Pick({
            FString::Printf(TEXT("нет у меня %s"), *ULexicon::Decline(*N1, EGramCase::Gen)),
            FString::Printf(TEXT("не могу я %s"), *V1->Base),
            TEXT("нет, уволь"), TEXT("в другой раз, ладно?")
        });
        break;

    // --- Ответ и согласие --------------------------------------------------
    case ESpeechAct::Answer:
    case ESpeechAct::Agree:
        Body = Pick({
            FString::Printf(TEXT("и правда %s"), *D1->Base),
            FString::Printf(TEXT("вот и я про %s"), *ULexicon::Decline(*N1, EGramCase::Acc)),
            FString::Printf(TEXT("%s, что тут скажешь"), *D1->Base),
            TEXT("так и есть"), TEXT("твоя правда"), TEXT("вот-вот")
        });
        break;

    default:
        return FString();
    }

    // --- Собираем -----------------------------------------------------------
    FString Result = Body;

    const FString Head = Opener(Ctx);
    if (!Head.IsEmpty() && Ctx.Talkativeness > 0.3f)
    {
        Result = Head + TEXT(", ") + Result;
    }

    const FString End = Tail(Ctx);
    if (!End.IsEmpty() && Ctx.Talkativeness > 0.45f)
    {
        const bool bQuestion = Result.EndsWith(TEXT("?"));
        if (!bQuestion)
        {
            Result += TEXT(". ") + Capitalise(End);
        }
    }

    // Разговорчивый на третьей-четвёртой реплике добавляет вторую мысль:
    // разговор у него расходится, а не обрывается.
    if (Ctx.Turn >= 2 && Ctx.Talkativeness > 0.6f && FMath::FRand() < 0.4f)
    {
        const FLexeme* N3 = Noun(Ctx, FMath::FRandRange(-0.3f, 0.3f));
        const FLexeme* D3 = Adv(Ctx);
        if (N3 && D3)
        {
            Result += FString::Printf(TEXT(" И с %s то же самое, %s."),
                *ULexicon::Decline(*N3, EGramCase::Ins), *D3->Base);
        }
    }

    return Capitalise(Result);
}

// ---------------------------------------------------------------------------
//  РЕЧЬ О ЧЁМ-ТО
//
//  Каждая ветка знает, о чём идёт речь, и оттого фраза выходит осмысленной.
//  Слова из словаря тут нужны только для окраски — «совсем», «нынче», —
//  а не для того, чтобы набить предложение чем попало.
// ---------------------------------------------------------------------------

FString FPhrase::SayAbout(const FTalkingPoint& Point, const FImproviseContext& Ctx)
{
    if (!Point.IsValid())
    {
        return FString();
    }

    const FString& It = Point.About;
    const bool bBad = Point.Valence < -0.2f;
    const bool bGood = Point.Valence > 0.25f;
    FString Body;

    switch (Point.Kind)
    {
    case ETalkKind::Need:
        // Говорят не «голод», а «есть хочу»: готовая человеческая формулировка
        // лежит в Detail, и она всегда лучше названия потребности.
        Body = Point.Detail.IsEmpty()
            ? FString::Printf(TEXT("%s — вот что меня сейчас донимает"), *It)
            : Pick({
                Point.Detail,
                FString::Printf(TEXT("%s, если честно"), *Point.Detail),
                FString::Printf(TEXT("вот что: %s"), *Point.Detail),
                FString::Printf(TEXT("%s — с утра об этом думаю"), *Point.Detail)
            });
        break;

    case ETalkKind::Deed:
        Body = Pick({
            FString::Printf(TEXT("я сейчас вот: %s"), *It),
            FString::Printf(TEXT("занят(а) — %s"), *It),
            FString::Printf(TEXT("как раз собрался(лась) %s"), *It)
        });
        break;

    case ETalkKind::Place:
        Body = bGood
            ? Pick({
                FString::Printf(TEXT("хорошо у нас в месте, где %s"), *It),
                FString::Printf(TEXT("я часто хожу туда, где %s"), *It),
                FString::Printf(TEXT("%s — доброе место, я тебе скажу"), *It),
                FString::Printf(TEXT("люблю бывать там, где %s"), *It),
                FString::Printf(TEXT("если будет время — загляни, где %s"), *It) })
            : bBad
            ? Pick({
                FString::Printf(TEXT("не ходи туда, где %s"), *It),
                FString::Printf(TEXT("%s — гиблое место"), *It),
                FString::Printf(TEXT("зря я ходил(а) туда, где %s"), *It),
                FString::Printf(TEXT("ничего хорошего там, где %s"), *It) })
            : Pick({
                FString::Printf(TEXT("а знаешь, где %s? Я там был(а)"), *It),
                FString::Printf(TEXT("есть у нас %s, если вдруг понадобится"), *It),
                FString::Printf(TEXT("%s — бывал(а) там, место как место"), *It),
                FString::Printf(TEXT("недалеко отсюда %s"), *It),
                FString::Printf(TEXT("дорогу туда, где %s, я знаю"), *It) });
        break;

    case ETalkKind::Person:
        // Имя ставим только в именительном падеже: склонять чужие имена
        // наугад — вернейший способ сказать «на Софья можно положиться».
        Body = bGood
            ? Pick({
                FString::Printf(TEXT("%s — хороший человек"), *It),
                FString::Printf(TEXT("%s — с ним(ней) мы ладим"), *It),
                FString::Printf(TEXT("%s — вот на кого можно положиться"), *It) })
            : Pick({
                FString::Printf(TEXT("%s — с ним(ней) я стараюсь не связываться"), *It),
                FString::Printf(TEXT("%s меня однажды подвёл(а)"), *It),
                FString::Printf(TEXT("%s — не по душе мне этот человек"), *It) });
        break;

    case ETalkKind::Book:
        // Главное: человек передаёт именно то, что прочитал.
        Body = Pick({
            FString::Printf(TEXT("я читал(а) «%s». Там сказано: %s"), *It, *Point.Detail),
            FString::Printf(TEXT("вот что я вычитал(а): %s"), *Point.Detail),
            FString::Printf(TEXT("в «%s» написано — %s"), *It, *Point.Detail)
        });
        break;

    case ETalkKind::Feeling:
        Body = Pick({
            Point.Detail,
            FString::Printf(TEXT("%s — вот что со мной"), *It),
            FString::Printf(TEXT("на душе %s"), *It)
        });
        break;

    case ETalkKind::Weather:
        Body = Pick({
            FString::Printf(TEXT("%s сегодня"), *It),
            FString::Printf(TEXT("видишь, %s"), *It),
            FString::Printf(TEXT("с утра %s, и весь день так"), *It)
        });
        break;

    case ETalkKind::Dream:
        Body = Pick({
            FString::Printf(TEXT("я вот чего хочу: %s"), *It),
            FString::Printf(TEXT("мне бы %s — а там пусть будет что будет"), *It),
            FString::Printf(TEXT("всю жизнь думаю: %s"), *It)
        });
        break;

    case ETalkKind::Skill:
        // Навык называем отдельным словом, а не вставляем в падеж:
        // «вот бы и мне работа уметь» — так не говорят.
        Body = Pick({
            FString::Printf(TEXT("моё ремесло — %s. Научился(лась) со временем"), *It),
            FString::Printf(TEXT("если понадобится помощь, моё дело — %s"), *It),
            FString::Printf(TEXT("лучше всего у меня выходит вот что: %s"), *It),
            FString::Printf(TEXT("я по этой части: %s"), *It)
        });
        break;

    case ETalkKind::Opinion:
        Body = Pick({
            Point.Detail,
            FString::Printf(TEXT("я так скажу: %s"), *Point.Detail),
            FString::Printf(TEXT("по опыту знаю — %s"), *Point.Detail)
        });
        break;

    case ETalkKind::Trouble:
        Body = Pick({
            FString::Printf(TEXT("с %s у меня беда: %s"), *It, *Point.Detail),
            FString::Printf(TEXT("%s — %s"), *It, *Point.Detail),
            FString::Printf(TEXT("туго мне сейчас: %s"), *Point.Detail)
        });
        break;

    case ETalkKind::Work:
        Body = Pick({
            FString::Printf(TEXT("я по работе %s"), *It),
            FString::Printf(TEXT("хожу на работу, %s"), *It),
            FString::Printf(TEXT("моё дело — %s"), *It)
        });
        break;

    default:
        return FString();
    }

    if (Body.IsEmpty())
    {
        return FString();
    }

    // Вводное слово и окраска — вот здесь словарь и пригождается.
    FString Result = Body;
    const FString Head = Opener(Ctx);
    if (!Head.IsEmpty() && Ctx.Talkativeness > 0.35f)
    {
        Result = Head + TEXT(", ") + Result;
    }

    // Присказку в конце люди вставляют изредка, а не в каждой фразе.
    const FString End = Tail(Ctx);
    if (!End.IsEmpty() && Ctx.Talkativeness > 0.55f && !Result.EndsWith(TEXT("?"))
        && FMath::FRand() < 0.4f)
    {
        Result += TEXT(". ") + Capitalise(End);
    }

    return Capitalise(Result);
}

FString FPhrase::AskAbout(const FTalkingPoint& Point, const FImproviseContext& Ctx)
{
    if (!Point.IsValid())
    {
        return FString();
    }

    const FString& It = Point.About;
    FString Body;

    switch (Point.Kind)
    {
    case ETalkKind::Need:
        Body = Pick({
            TEXT("а у тебя как с этим?"),
            TEXT("тебе такое знакомо?"),
            TEXT("а тебя что сейчас донимает?") });
        break;
    case ETalkKind::Deed:
        Body = TEXT("а ты чем занят?");
        break;
    case ETalkKind::Place:
        Body = Pick({
            FString::Printf(TEXT("ты бывал(а) там, где %s?"), *It),
            FString::Printf(TEXT("не знаешь, где тут %s?"), *It) });
        break;
    case ETalkKind::Person:
        Body = FString::Printf(TEXT("а %s ты знаешь?"), *It);
        break;
    case ETalkKind::Book:
        Body = Pick({
            FString::Printf(TEXT("ты «%s» читал(а)?"), *It),
            TEXT("а ты читать умеешь?") });
        break;
    case ETalkKind::Feeling:
        Body = TEXT("а у тебя как на душе?");
        break;
    case ETalkKind::Weather:
        Body = FString::Printf(TEXT("как тебе %s?"), *It);
        break;
    case ETalkKind::Dream:
        Body = TEXT("а ты чего от жизни хочешь?");
        break;
    case ETalkKind::Skill:
        Body = Pick({
            FString::Printf(TEXT("а ты этому учился(лась) — %s?"), *It),
            TEXT("а твоё ремесло какое?"),
            FString::Printf(TEXT("%s — тебе это знакомо?"), *It) });
        break;
    case ETalkKind::Work:
        Body = TEXT("а ты где работаешь?");
        break;
    case ETalkKind::Trouble:
        Body = FString::Printf(TEXT("у тебя с %s как?"), *It);
        break;
    default:
        Body = TEXT("а у тебя как дела?");
        break;
    }

    const FString Head = Opener(Ctx);
    return Capitalise(Head.IsEmpty() ? Body : Head + TEXT(", ") + Body);
}

FString FPhrase::ReplyAbout(const FTalkingPoint& Theirs, bool bAgree, const FImproviseContext& Ctx)
{
    if (!Theirs.IsValid())
    {
        return FString();
    }

    const FString& It = Theirs.About;

    // Ответ по существу: человек показывает, что услышал именно это.
    if (!bAgree)
    {
        switch (Theirs.Kind)
        {
        case ETalkKind::Book:
            return Capitalise(Pick({
                TEXT("не верю я книгам"),
                FString::Printf(TEXT("в «%s» так пишут, а на деле бывает иначе"), *It),
                TEXT("мало ли что написано") }));
        case ETalkKind::Person:
            return Capitalise(Pick({
                FString::Printf(TEXT("%s? По-моему, не такой(ая)"), *It),
                TEXT("это ты зря о нём так"),
                TEXT("у меня другое впечатление") }));
        case ETalkKind::Place:
            return Capitalise(Pick({
                FString::Printf(TEXT("не сказал(а) бы, что там хорошо — %s"), *It),
                TEXT("я там был(а), ничего особенного") }));
        default:
            return Capitalise(Pick({
                TEXT("не сказал(а) бы"),
                TEXT("это как посмотреть"),
                TEXT("по-моему, всё наоборот"),
                FString::Printf(TEXT("про %s я думаю иначе"), *It) }));
        }
    }

    switch (Theirs.Kind)
    {
    case ETalkKind::Need:
        return Capitalise(Pick({
            TEXT("и у меня то же самое"),
            TEXT("понимаю, самому(ой) не сладко"),
            TEXT("это дело поправимое"),
            TEXT("сходи да реши, чего тянуть") }));

    case ETalkKind::Book:
        return Capitalise(Pick({
            FString::Printf(TEXT("надо будет и мне «%s» посмотреть"), *It),
            TEXT("вот это дельно. Запомню"),
            FString::Printf(TEXT("не знал(а) этого. «%s», говоришь?"), *It) }));

    case ETalkKind::Place:
        return Capitalise(Pick({
            FString::Printf(TEXT("да, %s — знаю это место"), *It),
            FString::Printf(TEXT("схожу туда, где %s"), *It),
            TEXT("спасибо, пригодится") }));

    case ETalkKind::Person:
        return Capitalise(Pick({
            FString::Printf(TEXT("да, %s такой(ая) и есть"), *It),
            FString::Printf(TEXT("%s — я тоже с ним(ней) знаком(а)"), *It) }));

    case ETalkKind::Feeling:
        return Capitalise(Pick({
            TEXT("бывает. Пройдёт"),
            TEXT("понимаю тебя"),
            FString::Printf(TEXT("%s — со всяким случается"), *It) }));

    case ETalkKind::Trouble:
        return Capitalise(Pick({
            TEXT("держись. Само не наладится, но наладится"),
            FString::Printf(TEXT("с %s сейчас у многих туго"), *It),
            TEXT("если чем помочь — скажи") }));

    case ETalkKind::Weather:
        return Capitalise(Pick({
            FString::Printf(TEXT("и правда %s"), *It),
            TEXT("да, погода нынче такая"),
            TEXT("к вечеру, глядишь, переменится") }));

    case ETalkKind::Skill:
        return Capitalise(Pick({
            FString::Printf(TEXT("научишь? Я бы взялся(лась) за это — %s"), *It),
            TEXT("вот бы и мне так уметь"),
            TEXT("хорошее дело, уважаю") }));

    case ETalkKind::Dream:
        return Capitalise(Pick({
            TEXT("доброе желание. Дай-то бог"),
            FString::Printf(TEXT("%s — это ты хорошо задумал(а)"), *It) }));

    case ETalkKind::Work:
        return Capitalise(Pick({
            FString::Printf(TEXT("%s — работа нелёгкая"), *It),
            TEXT("а платят-то хоть сносно?") }));

    default:
        return Capitalise(Pick({
            TEXT("так и есть"), TEXT("твоя правда"),
            FString::Printf(TEXT("про %s — согласен(на)"), *It) }));
    }
}

// ---------------------------------------------------------------------------
//  Ответ на чужую реплику
//
//  Человек не говорит в пустоту: он цепляется за то, что услышал.
//  Здесь ответ строится вокруг слова из чужой фразы — так разговор
//  держится темы, а не рассыпается на отдельные выкрики.
// ---------------------------------------------------------------------------

FString FPhrase::Respond(ESpeechAct ToAct, const FString& ToText, const FImproviseContext& Ctx)
{
    // Выцепляем из услышанного самое длинное слово: обычно оно и несёт смысл.
    TArray<FString> Heard;
    ToText.ParseIntoArrayWS(Heard);

    FString KeyWord;
    for (const FString& W : Heard)
    {
        const FString Clean = W.TrimStartAndEnd().Replace(TEXT(","), TEXT("")).Replace(TEXT("."), TEXT(""))
                               .Replace(TEXT("?"), TEXT("")).Replace(TEXT("!"), TEXT(""));
        if (Clean.Len() > KeyWord.Len() && Clean.Len() > 4)
        {
            KeyWord = Clean;
        }
    }

    const FLexeme* Echo = KeyWord.IsEmpty() ? nullptr : ULexicon::Get().Find(ULexicon::Stem(KeyWord));

    // На вопрос отвечают, на жалобу сочувствуют, на похвалу смущаются.
    switch (ToAct)
    {
    case ESpeechAct::Question:
        if (Echo && Echo->Class == EWordClass::Noun)
        {
            return Capitalise(Pick({
                FString::Printf(TEXT("%s? Да как сказать"), *Capitalise(ULexicon::Decline(*Echo, EGramCase::Nom))),
                FString::Printf(TEXT("про %s я мало знаю"), *ULexicon::Decline(*Echo, EGramCase::Acc)),
                FString::Printf(TEXT("с %s у меня по-разному"), *ULexicon::Decline(*Echo, EGramCase::Ins)),
                Say(ESpeechAct::Answer, Ctx)
            }));
        }
        return Say(ESpeechAct::Answer, Ctx);

    case ESpeechAct::Complain:
        return Capitalise(Pick({
            TEXT("да, невесело"),
            TEXT("понимаю тебя"),
            Echo ? FString::Printf(TEXT("с %s у всех так"), *ULexicon::Decline(*Echo, EGramCase::Ins))
                 : FString(TEXT("у всех так бывает")),
            Say(ESpeechAct::Console, Ctx)
        }));

    case ESpeechAct::Compliment:
        return Capitalise(Pick({
            TEXT("ну что ты"), TEXT("да брось"), TEXT("спасибо, приятно слышать"),
            TEXT("скажешь тоже")
        }));

    case ESpeechAct::Boast:
        return Capitalise(Pick({
            TEXT("ну-ну"), TEXT("да неужели"), TEXT("бывает"),
            Say(ESpeechAct::Agree, Ctx)
        }));

    case ESpeechAct::Joke:
        return Capitalise(Pick({
            TEXT("вот это верно"), TEXT("ха, и правда"), TEXT("смешно, да не очень"),
            Say(ESpeechAct::Joke, Ctx)
        }));

    case ESpeechAct::Greet:
        return Capitalise(Pick({
            TEXT("и тебе здравствуй"), TEXT("привет"), TEXT("здравствуй"),
            Say(ESpeechAct::SmallTalk, Ctx)
        }));

    case ESpeechAct::Insult:
    case ESpeechAct::Threaten:
        return Capitalise(Pick({
            TEXT("это ты зря"), TEXT("сам такой"), TEXT("ну и разговор у нас"),
            TEXT("я этого не заслужил")
        }));

    default:
        break;
    }

    // Обычный ход: подхватить слово и продолжить свою мысль.
    if (Echo && Echo->Class == EWordClass::Noun && FMath::FRand() < 0.5f)
    {
        FImproviseContext Next = Ctx;
        Next.Topic = Echo->Topic;
        return Capitalise(FString::Printf(TEXT("%s... %s"),
            *Capitalise(ULexicon::Decline(*Echo, EGramCase::Nom)),
            *Say(ESpeechAct::SmallTalk, Next)));
    }

    return Say(ESpeechAct::SmallTalk, Ctx);
}
