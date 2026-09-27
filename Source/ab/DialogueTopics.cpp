// DialogueTopics.cpp
// О чём говорят: книги, места, люди, чувства, погода, мечты.

#include "Dialogue.h"

// ---------------------------------------------------------------------------
//  Книги
// ---------------------------------------------------------------------------

FString FDialogueLines::TellReading(const FLineContext& C, const FString& Title, const FString& PageTitle)
{
    const FString Base = FString::Printf(TEXT("Я книгу читаю — «%s»."), *Title);

    // Параграф называют как он есть, в кавычках: склонять заголовок нельзя.
    int32 Dot = INDEX_NONE;
    if (PageTitle.StartsWith(TEXT("§")) && PageTitle.FindChar(TEXT('.'), Dot))
    {
        const FString Topic = PageTitle.Mid(Dot + 1).TrimStartAndEnd();
        if (!Topic.IsEmpty())
        {
            return FString::Printf(TEXT("%s Сейчас там параграф «%s»."), *Base, *Topic);
        }
    }
    return Base;
}

FString FDialogueLines::AskWhatWritten(const FLineContext& C)
{
    return Pick({ TEXT("И что там пишут?"), TEXT("Интересно. И о чём там?") });
}

FString FDialogueLines::Retell(const FLineContext& C, const FString& Sentence)
{
    return Pick({
        FString::Printf(TEXT("Там написано: «%s»"), *Sentence),
        FString::Printf(TEXT("Вот, например: «%s»"), *Sentence) });
}

FString FDialogueLines::KnowThat(const FLineContext& C)
{
    return My(C, TEXT("Знаю, я тоже это читал."), TEXT("Знаю, я тоже это читала."));
}

FString FDialogueLines::DidntKnow(const FLineContext& C)
{
    return Pick({
        My(C, TEXT("Надо же, не знал."), TEXT("Надо же, не знала.")),
        TEXT("Интересно. Запомню.") });
}

FString FDialogueLines::CantRead(const FLineContext& C)
{
    return TEXT("А я читать пока не умею.");
}

// ---------------------------------------------------------------------------
//  Места
// ---------------------------------------------------------------------------

FString FDialogueLines::TellPlace(const FLineContext& C, EPlaceKind K, float Affect)
{
    const FString Was = My(C, TEXT("Был"), TEXT("Была"));
    if (Affect > 0.25f)
    {
        return FString::Printf(TEXT("%s сегодня %s — хорошо там."), *Was, *PlaceAt(K));
    }
    if (Affect < -0.25f)
    {
        return FString::Printf(TEXT("%s %s — не понравилось мне там."), *Was, *PlaceAt(K));
    }
    return FString::Printf(TEXT("%s сегодня %s."), *Was, *PlaceAt(K));
}

FString FDialogueLines::AgreePlace(const FLineContext& C, EPlaceKind K, float Affect)
{
    return Affect >= 0.0f
        ? FString::Printf(TEXT("Да, %s хорошо."), *PlaceAt(K))
        : FString::Printf(TEXT("Да, мне %s тоже не понравилось."), *PlaceAt(K));
}

FString FDialogueLines::DisagreePlace(const FLineContext& C, EPlaceKind K, float MyAffect)
{
    return MyAffect >= 0.0f
        ? FString::Printf(TEXT("А мне %s нравится."), *PlaceAt(K))
        : FString::Printf(TEXT("А мне %s не понравилось."), *PlaceAt(K));
}

FString FDialogueLines::AskWhere(const FLineContext& C)
{
    return Pick({ TEXT("А где это?"), TEXT("А это где?") });
}

FString FDialogueLines::AnswerWhere(const FLineContext& C, EPlaceKind K, const FString& Way)
{
    return FString::Printf(TEXT("%s — %s."), *Capital(PlaceName(K)), *Way);
}

// ---------------------------------------------------------------------------
//  Люди
// ---------------------------------------------------------------------------

FString FDialogueLines::TellPerson(const FLineContext& C, const FString& Name, bool bFemale,
                                   EPersonReason Why, bool bWantLeader)
{
    FString Line;
    switch (Why)
    {
    case EPersonReason::Helped:
        Line = FString::Printf(TEXT("%s — хороший человек. %s меня однажды."),
            *Name, *G(bFemale, TEXT("Выручил"), TEXT("Выручила")));
        break;
    case EPersonReason::Respected:
        Line = FString::Printf(TEXT("%s — %s. С %s все считаются."),
            *Name, *G(bFemale, TEXT("толковый"), TEXT("толковая")), *G(bFemale, TEXT("ним"), TEXT("ней")));
        break;
    case EPersonReason::Pleasant:
        Line = FString::Printf(TEXT("%s — приятный человек, с %s легко."),
            *Name, *G(bFemale, TEXT("ним"), TEXT("ней")));
        break;
    case EPersonReason::Offended:
        Line = FString::Printf(TEXT("%s меня %s."), *Name, *G(bFemale, TEXT("обидел"), TEXT("обидела")));
        break;
    case EPersonReason::Liar:
        Line = FString::Printf(TEXT("%s врёт много, %s %s."),
            *Name, *Ty(C, TEXT("не верь"), TEXT("не верьте")), *G(bFemale, TEXT("ему"), TEXT("ей")));
        break;
    case EPersonReason::Unpleasant:
        Line = FString::Printf(TEXT("%s — неприятный человек."), *Name);
        break;
    default:
        return FString();
    }

    if (bWantLeader)
    {
        Line += TEXT(" Вот кого бы нам в старшие.");
    }
    return Line;
}

FString FDialogueLines::AgreePerson(const FLineContext& C, const FString& Name, bool bFemale, bool bGood)
{
    return bGood
        ? FString::Printf(TEXT("Да, %s — хороший человек."), *Name)
        : FString::Printf(TEXT("Да, с %s лучше не связываться."), *G(bFemale, TEXT("ним"), TEXT("ней")));
}

FString FDialogueLines::DisagreePerson(const FLineContext& C, bool bFemale, bool bTheySaidGood)
{
    return bTheySaidGood
        ? FString::Printf(TEXT("Не знаю, мне %s не нравится."), *G(bFemale, TEXT("он"), TEXT("она")))
        : FString::Printf(TEXT("%s, %s хороший человек."),
            *Ty(C, TEXT("Зря ты так"), TEXT("Зря вы так")), *G(bFemale, TEXT("он"), TEXT("она")));
}

FString FDialogueLines::DontKnowPerson(const FLineContext& C, bool bFemale)
{
    return FString::Printf(TEXT("Не знаю %s."), *G(bFemale, TEXT("такого"), TEXT("такую")));
}

// ---------------------------------------------------------------------------
//  Чувства
// ---------------------------------------------------------------------------

FString FDialogueLines::TellFeeling(const FLineContext& C, EEmotionType E)
{
    switch (E)
    {
    case EEmotionType::Sadness:    return TEXT("Грустно мне что-то.");
    case EEmotionType::Loneliness: return TEXT("Одиноко мне.");
    case EEmotionType::Anxiety:    return TEXT("Тревожно мне.");
    case EEmotionType::Fear:       return TEXT("Страшно мне.");
    case EEmotionType::Anger:      return My(C, TEXT("Злой я сегодня."), TEXT("Злая я сегодня."));
    case EEmotionType::Joy:        return TEXT("Хорошо мне сегодня!");
    case EEmotionType::Shame:      return TEXT("Стыдно мне.");
    case EEmotionType::Guilt:      return My(C, TEXT("Виноват я кое в чём."), TEXT("Виновата я кое в чём."));
    case EEmotionType::Boredom:    return TEXT("Скучно.");
    case EEmotionType::Pride:      return TEXT("Горжусь собой сегодня.");
    case EEmotionType::Love:
    case EEmotionType::Affection:  return My(C, TEXT("Влюбился я, кажется."), TEXT("Влюбилась я, кажется."));
    case EEmotionType::Nostalgia:  return TEXT("Всё прошлое вспоминаю.");
    case EEmotionType::Curiosity:  return TEXT("Интересно мне всё вокруг.");
    case EEmotionType::Gratitude:  return TEXT("Хорошие у нас люди.");
    default:                       return FString();
    }
}

FString FDialogueLines::AskWhatHappened(const FLineContext& C)
{
    return Pick({ TEXT("Что случилось?"), TEXT("А что такое?") });
}

FString FDialogueLines::AnswerWhatHappened(const FLineContext& C, const FString& Reason)
{
    return Reason.IsEmpty()
        ? FString(TEXT("Да ничего, пройдёт."))
        : FString::Printf(TEXT("Да вот — %s."), *Reason);
}

FString FDialogueLines::Console(const FLineContext& C)
{
    return C.bFormal
        ? Pick({ TEXT("Не грустите, всё наладится."), TEXT("Держитесь.") })
        : Pick({ TEXT("Не грусти, всё наладится."), TEXT("Держись.") });
}

FString FDialogueLines::GladForYou(const FLineContext& C)
{
    return FString::Printf(TEXT("%s за %s!"),
        *My(C, TEXT("Рад"), TEXT("Рада")), *Ty(C, TEXT("тебя"), TEXT("вас")));
}

// ---------------------------------------------------------------------------
//  Погода
// ---------------------------------------------------------------------------

FString FDialogueLines::TellWeather(const FLineContext& C)
{
    if (C.Hour >= 21 || C.Hour < 5)  return TEXT("Темно уже.");
    if (C.Weather < 0.2f)            return TEXT("Хорошая сегодня погода.");
    if (C.Weather < 0.45f)           return TEXT("Облачно сегодня.");
    if (C.Weather < 0.7f)            return TEXT("Сыро сегодня, пасмурно.");
    return TEXT("Ну и погода сегодня!");
}

FString FDialogueLines::AgreeWeather(const FLineContext& C)
{
    return C.Weather < 0.3f
        ? Pick({ TEXT("Да, тепло."), TEXT("Да, хорошо сегодня.") })
        : Pick({ TEXT("Да, погода не очень."), TEXT("Да уж.") });
}

// ---------------------------------------------------------------------------
//  Мечта
// ---------------------------------------------------------------------------

FString FDialogueLines::TellDream(const FLineContext& C, const FString& Dream)
{
    const FString Know = Ty(C, TEXT("Знаешь"), TEXT("Знаете"));
    return Dream.StartsWith(TEXT("чтобы"))
        ? FString::Printf(TEXT("%s, я мечтаю, %s."), *Know, *Dream)
        : FString::Printf(TEXT("%s, я мечтаю %s."), *Know, *Dream);
}

FString FDialogueLines::ReactDream(const FLineContext& C)
{
    return Pick({ TEXT("Хорошая мечта."), TEXT("Это хорошее дело.") });
}

FString FDialogueLines::AskWhatStops(const FLineContext& C)
{
    return TEXT("А что мешает?");
}

FString FDialogueLines::AnswerWhatStops(const FLineContext& C, bool bNoMoney, bool bAfraid)
{
    if (bNoMoney) return TEXT("Денег нет.");
    if (bAfraid)  return TEXT("Боюсь, если честно.");
    return TEXT("Времени не хватает.");
}
