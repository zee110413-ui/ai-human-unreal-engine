// DialogueLines.cpp
// Грамматика, места, ритуалы и речь о себе.

#include "Dialogue.h"

// ---------------------------------------------------------------------------
//  Грамматика
// ---------------------------------------------------------------------------

FString FDialogueLines::G(bool bFemale, const TCHAR* M, const TCHAR* F)
{
    return FString(bFemale ? F : M);
}

FString FDialogueLines::My(const FLineContext& C, const TCHAR* M, const TCHAR* F)
{
    return G(C.bMeFemale, M, F);
}

FString FDialogueLines::Ty(const FLineContext& C, const TCHAR* Informal, const TCHAR* Formal)
{
    return FString(C.bFormal ? Formal : Informal);
}

FString FDialogueLines::Capital(const FString& S)
{
    // ToUpper движка кириллицу не трогает — поднимаем букву сами.
    if (S.IsEmpty())
    {
        return S;
    }
    FString Out = S;
    TCHAR& First = Out[0];
    if (First >= 0x0430 && First <= 0x044F)
    {
        First = TCHAR(First - 0x20);
    }
    else if (First == 0x0451)
    {
        First = TCHAR(0x0401);
    }
    else if (First >= 'a' && First <= 'z')
    {
        First = TCHAR(First - 32);
    }
    return Out;
}

FString FDialogueLines::Pick(const TArray<FString>& Options)
{
    return Options.Num() > 0 ? Options[FMath::RandRange(0, Options.Num() - 1)] : FString();
}

FString FDialogueLines::FirstSentence(const FString& Text)
{
    for (int32 i = 12; i < Text.Len(); ++i)
    {
        const TCHAR Ch = Text[i];
        if (Ch == TEXT('.') || Ch == TEXT('!') || Ch == TEXT('?'))
        {
            return Text.Left(i + 1).TrimStartAndEnd();
        }
    }
    return Text.Left(140).TrimStartAndEnd();
}

// ---------------------------------------------------------------------------
//  Места
// ---------------------------------------------------------------------------

FString FDialogueLines::PlaceName(EPlaceKind K)
{
    switch (K)
    {
    case EPlaceKind::Food:      return TEXT("кафе");
    case EPlaceKind::Shop:      return TEXT("магазин");
    case EPlaceKind::Social:    return TEXT("площадь");
    case EPlaceKind::Rest:      return TEXT("парк");
    case EPlaceKind::Beautiful: return TEXT("сквер");
    case EPlaceKind::Study:     return TEXT("школа");
    case EPlaceKind::Hospital:  return TEXT("больница");
    case EPlaceKind::Workshop:  return TEXT("мастерская");
    case EPlaceKind::Library:   return TEXT("библиотека");
    case EPlaceKind::Market:    return TEXT("рынок");
    case EPlaceKind::Bathhouse: return TEXT("баня");
    case EPlaceKind::Bakery:    return TEXT("пекарня");
    case EPlaceKind::TownHall:  return TEXT("управа");
    case EPlaceKind::Work:      return TEXT("работа");
    case EPlaceKind::Home:      return TEXT("дом");
    default:                    return TEXT("место");
    }
}

FString FDialogueLines::PlaceAt(EPlaceKind K)
{
    switch (K)
    {
    case EPlaceKind::Food:      return TEXT("в кафе");
    case EPlaceKind::Shop:      return TEXT("в магазине");
    case EPlaceKind::Social:    return TEXT("на площади");
    case EPlaceKind::Rest:      return TEXT("в парке");
    case EPlaceKind::Beautiful: return TEXT("в сквере");
    case EPlaceKind::Study:     return TEXT("в школе");
    case EPlaceKind::Hospital:  return TEXT("в больнице");
    case EPlaceKind::Workshop:  return TEXT("в мастерской");
    case EPlaceKind::Library:   return TEXT("в библиотеке");
    case EPlaceKind::Market:    return TEXT("на рынке");
    case EPlaceKind::Bathhouse: return TEXT("в бане");
    case EPlaceKind::Bakery:    return TEXT("в пекарне");
    case EPlaceKind::TownHall:  return TEXT("в управе");
    case EPlaceKind::Work:      return TEXT("на работе");
    case EPlaceKind::Home:      return TEXT("дома");
    default:                    return TEXT("там");
    }
}

FString FDialogueLines::PlaceTo(EPlaceKind K)
{
    switch (K)
    {
    case EPlaceKind::Food:      return TEXT("в кафе");
    case EPlaceKind::Shop:      return TEXT("в магазин");
    case EPlaceKind::Social:    return TEXT("на площадь");
    case EPlaceKind::Rest:      return TEXT("в парк");
    case EPlaceKind::Beautiful: return TEXT("в сквер");
    case EPlaceKind::Study:     return TEXT("в школу");
    case EPlaceKind::Hospital:  return TEXT("в больницу");
    case EPlaceKind::Workshop:  return TEXT("в мастерскую");
    case EPlaceKind::Library:   return TEXT("в библиотеку");
    case EPlaceKind::Market:    return TEXT("на рынок");
    case EPlaceKind::Bathhouse: return TEXT("в баню");
    case EPlaceKind::Bakery:    return TEXT("в пекарню");
    case EPlaceKind::TownHall:  return TEXT("в управу");
    case EPlaceKind::Work:      return TEXT("на работу");
    case EPlaceKind::Home:      return TEXT("домой");
    default:                    return TEXT("туда");
    }
}

FString FDialogueLines::WayTo(const FVector& From, const FVector& To)
{
    const FVector2D D(To.X - From.X, To.Y - From.Y);
    const float Meters = D.Size() / 100.0f;
    if (Meters < 25.0f)
    {
        return TEXT("совсем рядом");
    }

    // Север — ось X города, восток — ось Y.
    static const TCHAR* Sides[] = {
        TEXT("на север"), TEXT("на северо-восток"), TEXT("на восток"), TEXT("на юго-восток"),
        TEXT("на юг"), TEXT("на юго-запад"), TEXT("на запад"), TEXT("на северо-запад") };

    const float Angle = FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X));
    const int32 Sector = (FMath::RoundToInt(Angle / 45.0f) + 8) % 8;

    const TCHAR* Far = Meters < 120.0f ? TEXT("недалеко")
                     : Meters < 400.0f ? TEXT("подальше")
                     : TEXT("далеко");

    return FString::Printf(TEXT("%s, %s"), Sides[Sector], Far);
}

// ---------------------------------------------------------------------------
//  Ритуалы
// ---------------------------------------------------------------------------

FString FDialogueLines::Greet(const FLineContext& C)
{
    FString Hello;
    if (C.bFormal)
    {
        Hello = (C.Hour >= 5 && C.Hour < 11) ? TEXT("Доброе утро")
              : (C.Hour >= 11 && C.Hour < 18) ? TEXT("Добрый день")
              : (C.Hour >= 18 && C.Hour < 23) ? TEXT("Добрый вечер")
              : TEXT("Здравствуйте");
    }
    else
    {
        Hello = Pick({ TEXT("Привет"), TEXT("Привет"),
                       (C.Hour >= 5 && C.Hour < 11) ? TEXT("Доброе утро") : TEXT("Здравствуй") });
    }

    return C.TheirName.IsEmpty()
        ? Hello + TEXT("!")
        : FString::Printf(TEXT("%s, %s!"), *Hello, *C.TheirName);
}

FString FDialogueLines::GreetBack(const FLineContext& C)
{
    const FString Hello = C.bFormal
        ? FString(TEXT("Здравствуйте"))
        : Pick({ TEXT("Привет"), TEXT("И тебе привет") });

    return C.TheirName.IsEmpty()
        ? Hello + TEXT(".")
        : FString::Printf(TEXT("%s, %s."), *Hello, *C.TheirName);
}

FString FDialogueLines::Introduce(const FLineContext& C)
{
    if (!C.bFormal)
    {
        return FString::Printf(TEXT("Привет! Я %s."), *C.MyName);
    }
    return Pick({
        FString::Printf(TEXT("Здравствуйте. Меня зовут %s."), *C.MyName),
        FString::Printf(TEXT("Добрый день. Я %s, живу тут недалеко."), *C.MyName) });
}

FString FDialogueLines::IntroduceBack(const FLineContext& C)
{
    if (!C.bFormal)
    {
        return FString::Printf(TEXT("А я %s."), *C.MyName);
    }
    return Pick({
        FString::Printf(TEXT("Очень приятно. А я %s."), *C.MyName),
        FString::Printf(TEXT("Приятно познакомиться. Меня зовут %s."), *C.MyName) });
}

FString FDialogueLines::HowAreYou(const FLineContext& C)
{
    return C.bFormal
        ? Pick({ TEXT("Как ваши дела?"), TEXT("Как поживаете?") })
        : Pick({ TEXT("Как дела?"), TEXT("Как ты?"), TEXT("Как жизнь?") });
}

FString FDialogueLines::AndYou(const FLineContext& C)
{
    return C.bFormal
        ? Pick({ TEXT("А у вас как?"), TEXT("А вы как?") })
        : Pick({ TEXT("А у тебя как?"), TEXT("А ты как?") });
}

FString FDialogueLines::Thanks(const FLineContext& C)
{
    return C.bFormal
        ? Pick({ TEXT("Спасибо вам!"), TEXT("Спасибо, запомню.") })
        : Pick({ TEXT("Спасибо!"), TEXT("Спасибо, запомню.") });
}

FString FDialogueLines::ThanksForKindness(const FLineContext& C)
{
    return Pick({ TEXT("Спасибо на добром слове."), TEXT("Спасибо.") });
}

FString FDialogueLines::NoProblem(const FLineContext& C)
{
    return Pick({ TEXT("Не за что."), Ty(C, TEXT("Обращайся."), TEXT("Обращайтесь.")) });
}

FString FDialogueLines::Acknowledge(const FLineContext& C)
{
    return Pick({ TEXT("Понятно."), TEXT("Ясно."), TEXT("Вот оно что.") });
}

FString FDialogueLines::Farewell(const FLineContext& C, const FString& Reason)
{
    const FString Bye = C.bFormal
        ? Pick({ TEXT("До свидания."), TEXT("Всего доброго.") })
        : Pick({ TEXT("Пока!"), TEXT("До встречи!") });

    if (!Reason.IsEmpty())
    {
        return FString::Printf(TEXT("Ладно, пойду я — %s. %s"), *Reason, *Bye);
    }
    return Pick({
        FString::Printf(TEXT("Ладно, пойду. %s"), *Bye),
        FString::Printf(TEXT("%s поговорить. %s"), *My(C, TEXT("Рад был"), TEXT("Рада была")), *Bye) });
}

FString FDialogueLines::FarewellBack(const FLineContext& C)
{
    return C.bFormal
        ? Pick({ TEXT("До свидания."), TEXT("Всего доброго.") })
        : Pick({ TEXT("Пока!"), TEXT("Давай, до встречи."), TEXT("Счастливо!") });
}

FString FDialogueLines::Busy(const FLineContext& C, const FString& Doing)
{
    return FString::Printf(TEXT("%s, спешу — %s."),
        *Ty(C, TEXT("Извини"), TEXT("Извините")), *Doing);
}

// ---------------------------------------------------------------------------
//  О себе
// ---------------------------------------------------------------------------

FString FDialogueLines::Doing(const FLineContext& C, EActionType Action, bool bOnTheWay)
{
    switch (Action)
    {
    case EActionType::Work:     return bOnTheWay ? TEXT("на работу иду") : TEXT("работаю");
    case EActionType::Eat:
        if (bOnTheWay) return TEXT("иду поесть");
        return C.Hour < 11 ? TEXT("завтракаю") : C.Hour < 16 ? TEXT("обедаю") : TEXT("ужинаю");
    case EActionType::Drink:    return bOnTheWay ? TEXT("иду попить") : TEXT("воду пью");
    case EActionType::Cook:     return bOnTheWay ? TEXT("иду готовить") : TEXT("готовлю");
    case EActionType::Sleep:
    case EActionType::GoHome:   return TEXT("домой иду");
    case EActionType::Rest:     return bOnTheWay ? TEXT("иду отдохнуть") : TEXT("отдыхаю");
    case EActionType::Wash:     return bOnTheWay ? TEXT("иду помыться") : TEXT("моюсь");
    case EActionType::Read:
    case EActionType::Study:    return bOnTheWay ? TEXT("иду почитать") : TEXT("читаю");
    case EActionType::Practice: return bOnTheWay ? TEXT("иду упражняться") : TEXT("упражняюсь");
    case EActionType::Entertain:return bOnTheWay ? TEXT("иду развеяться") : TEXT("отдыхаю");
    case EActionType::Exercise: return bOnTheWay ? TEXT("иду размяться") : TEXT("разминаюсь");
    case EActionType::Wander:   return TEXT("гуляю");
    case EActionType::Explore:  return TEXT("город смотрю");
    default:                    return FString();
    }
}

FString FDialogueLines::Feel(const FLineContext& C, ENeedType Need)
{
    switch (Need)
    {
    case ENeedType::Hunger:        return Pick({ TEXT("есть хочется"), My(C, TEXT("проголодался"), TEXT("проголодалась")) });
    case ENeedType::Thirst:        return TEXT("пить хочется");
    case ENeedType::Sleep:         return My(C, TEXT("устал, спать хочется"), TEXT("устала, спать хочется"));
    case ENeedType::Hygiene:       return TEXT("помыться бы");
    case ENeedType::Comfort:       return TEXT("ноги гудят, присесть бы");
    case ENeedType::Safety:        return TEXT("неспокойно мне как-то");
    case ENeedType::Health:        return TEXT("нездоровится");
    case ENeedType::Shelter:       return TEXT("жить негде");
    case ENeedType::Money:         return TEXT("денег совсем нет");
    case ENeedType::Order:         return TEXT("всё кувырком");
    case ENeedType::SocialContact: return My(C, TEXT("скучно одному"), TEXT("скучно одной"));
    case ENeedType::Belonging:     return TEXT("одиноко как-то");
    case ENeedType::Intimacy:      return TEXT("поговорить по душам не с кем");
    case ENeedType::Esteem:        return TEXT("никто меня не ценит");
    case ENeedType::Achievement:   return TEXT("ничего не выходит");
    case ENeedType::Autonomy:      return TEXT("всё не по-моему");
    case ENeedType::Competence:    return TEXT("ничего толком не умею");
    case ENeedType::Novelty:       return TEXT("скучно, всё одно и то же");
    case ENeedType::Beauty:        return TEXT("серо всё вокруг");
    case ENeedType::Meaning:       return TEXT("не знаю, зачем всё это");
    default:                       return FString();
    }
}

FString FDialogueLines::MoodWord(const FLineContext& C, float Mood)
{
    if (Mood > 0.4f)  return Pick({ TEXT("Хорошо!"), TEXT("Отлично!") });
    if (Mood > 0.1f)  return Pick({ TEXT("Хорошо, спасибо."), TEXT("Нормально.") });
    if (Mood > -0.2f) return Pick({ TEXT("Потихоньку."), TEXT("Ничего.") });
    if (Mood > -0.5f) return Pick({ TEXT("Так себе."), TEXT("Неважно.") });
    return TEXT("Плохо, если честно.");
}

FString FDialogueLines::LeaveReason(const FLineContext& C, ENeedType Need, float Urgency, EActionType Next, bool bLate)
{
    if (bLate)
    {
        return TEXT("поздно уже, домой пора");
    }
    if (Urgency > 0.5f)
    {
        switch (Need)
        {
        case ENeedType::Hunger:  return TEXT("есть хочется");
        case ENeedType::Thirst:  return TEXT("пить хочется");
        case ENeedType::Sleep:   return TEXT("спать пора");
        case ENeedType::Bladder: return TEXT("мне бежать надо");
        case ENeedType::Hygiene: return TEXT("помыться надо");
        default: break;
        }
    }
    switch (Next)
    {
    case EActionType::Work:  return TEXT("на работу пора");
    case EActionType::Read:
    case EActionType::Study: return TEXT("почитать хочу");
    case EActionType::Cook:  return TEXT("готовить пора");
    case EActionType::Eat:   return TEXT("поесть надо");
    case EActionType::Sleep: return TEXT("спать пора");
    default:                 return FString();
    }
}

// ---------------------------------------------------------------------------
//  Нужды и дела
// ---------------------------------------------------------------------------

FString FDialogueLines::AskWhereFor(const FLineContext& C, ENeedType Need)
{
    const FString Know = Ty(C, TEXT("Не знаешь"), TEXT("Не знаете"));
    const TCHAR* What = nullptr;

    switch (Need)
    {
    case ENeedType::Hunger:     What = TEXT("где тут можно поесть"); break;
    case ENeedType::Thirst:     What = TEXT("где тут можно попить"); break;
    case ENeedType::Sleep:
    case ENeedType::Shelter:    What = TEXT("где тут можно переночевать"); break;
    case ENeedType::Hygiene:    What = TEXT("где тут можно помыться"); break;
    case ENeedType::Health:     What = TEXT("где тут больница"); break;
    case ENeedType::Money:      What = TEXT("где тут можно заработать"); break;
    case ENeedType::Competence:
    case ENeedType::Novelty:    What = TEXT("где тут можно почитать"); break;
    case ENeedType::Comfort:    What = TEXT("где тут можно посидеть"); break;
    default:                    return FString();
    }
    return FString::Printf(TEXT("%s, %s?"), *Know, What);
}

FString FDialogueLines::AdvisePlace(const FLineContext& C, EPlaceKind K, const FString& Way)
{
    if (K == EPlaceKind::Home)
    {
        return Ty(C, TEXT("Иди домой, там всё есть."), TEXT("Идите домой, там всё есть."));
    }
    if (K == EPlaceKind::Work)
    {
        return FString::Printf(TEXT("%s у нас на работе — это %s."),
            *Ty(C, TEXT("Спроси"), TEXT("Спросите")), *Way);
    }
    return FString::Printf(TEXT("%s %s — это %s."),
        *Ty(C, TEXT("Сходи"), TEXT("Сходите")), *PlaceTo(K), *Way);
}

FString FDialogueLines::DontKnowWhere(const FLineContext& C)
{
    return Pick({
        My(C, TEXT("Не знаю, сам ищу."), TEXT("Не знаю, сама ищу.")),
        My(C, TEXT("Не знаю, я тут мало где был."), TEXT("Не знаю, я тут мало где была.")) });
}

FString FDialogueLines::SameHere(const FLineContext& C, ENeedType Need)
{
    switch (Need)
    {
    case ENeedType::Hunger: return My(C, TEXT("Я тоже проголодался."), TEXT("Я тоже проголодалась."));
    case ENeedType::Thirst: return TEXT("Мне тоже пить хочется.");
    case ENeedType::Sleep:  return My(C, TEXT("Я тоже устал."), TEXT("Я тоже устала."));
    default:                return TEXT("У меня то же самое.");
    }
}

FString FDialogueLines::ReactDoing(const FLineContext& C, EActionType Action)
{
    switch (Action)
    {
    case EActionType::Work:     return Pick({ TEXT("Работа — дело нужное."), TEXT("Хорошо, что есть работа.") });
    case EActionType::Eat:      return TEXT("Приятного аппетита!");
    case EActionType::Read:
    case EActionType::Study:    return Pick({ TEXT("Читать полезно."), TEXT("Правильно, читать полезно.") });
    case EActionType::Cook:     return TEXT("Вкусно, наверное, будет.");
    case EActionType::Rest:
    case EActionType::Entertain:return TEXT("Отдыхать тоже надо.");
    case EActionType::Wander:
    case EActionType::Explore:
        return C.Weather < 0.4f
            ? FString(TEXT("Погода как раз для прогулки."))
            : Ty(C, TEXT("Смотри не промокни."), TEXT("Смотрите не промокните."));
    case EActionType::Exercise: return TEXT("Правильно, это полезно.");
    default:                    return Acknowledge(C);
    }
}
