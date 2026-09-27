// DialogueWork.cpp
// Умения, беды, работа, распоряжения — и проверка, что все слова речи есть в книгах.

#include "Dialogue.h"
#include "SpeechComponent.h"

// ---------------------------------------------------------------------------
//  Умения
// ---------------------------------------------------------------------------

FString FDialogueLines::TellSkill(const FLineContext& C, FName Skill)
{
    if (Skill == TEXT("Cooking"))    return TEXT("Я неплохо готовлю.");
    if (Skill == TEXT("Reading"))    return My(C, TEXT("Я читать научился."), TEXT("Я читать научилась."));
    if (Skill == TEXT("Writing"))    return TEXT("Я писать умею.");
    if (Skill == TEXT("Counting"))   return TEXT("Я считать умею.");
    if (Skill == TEXT("Arithmetic")) return TEXT("Я таблицу умножения знаю.");
    if (Skill == TEXT("Repair"))     return TEXT("Руки у меня на месте — могу починить что угодно.");
    if (Skill == TEXT("Craft"))      return TEXT("Я мастерить умею.");
    if (Skill == TEXT("Medicine"))   return TEXT("Я немного лечить умею.");
    if (Skill == TEXT("Music"))      return TEXT("Я на пианино играю.");
    if (Skill == TEXT("Teaching"))   return TEXT("Я объяснять умею.");
    if (Skill == TEXT("Trade"))      return TEXT("Я торговать умею.");
    if (Skill == TEXT("Empathy"))    return TEXT("Я людей хорошо понимаю.");
    if (Skill == TEXT("Navigation")) return TEXT("Я город хорошо знаю.");
    if (Skill == TEXT("Story"))      return TEXT("Я истории рассказывать люблю.");
    if (Skill == TEXT("Work"))       return TEXT("Я работы не боюсь.");
    if (Skill == TEXT("Fitness"))    return TEXT("Я каждый день бегаю.");
    return FString();
}

FString FDialogueLines::AskTeach(const FLineContext& C)
{
    return Ty(C, TEXT("Научишь меня?"), TEXT("Научите меня?"));
}

FString FDialogueLines::AgreeTeach(const FLineContext& C)
{
    return Ty(C, TEXT("Давай покажу, это просто."), TEXT("Давайте покажу, это просто."));
}

FString FDialogueLines::RefuseTeach(const FLineContext& C)
{
    return Ty(C, TEXT("Некогда мне, извини."), TEXT("Некогда мне, извините."));
}

// ---------------------------------------------------------------------------
//  Беды и работа
// ---------------------------------------------------------------------------

FString FDialogueLines::TellNoJob(const FLineContext& C)
{
    return TEXT("Работу найти не могу.");
}

FString FDialogueLines::TellNoMoney(const FLineContext& C)
{
    return TEXT("Денег совсем нет.");
}

FString FDialogueLines::Encourage(const FLineContext& C)
{
    return Pick({ Ty(C, TEXT("Ничего, найдёшь."), TEXT("Ничего, найдёте.")),
                  Ty(C, TEXT("Держись."), TEXT("Держитесь.")) });
}

FString FDialogueLines::OfferMoney(const FLineContext& C)
{
    return Ty(C, TEXT("Держи, вот немного денег. Отдашь, когда сможешь."),
                 TEXT("Возьмите, вот немного денег. Отдадите, когда сможете."));
}

FString FDialogueLines::AcceptMoney(const FLineContext& C)
{
    return TEXT("Спасибо! Отдам, честное слово.");
}

FString FDialogueLines::TellWork(const FLineContext& C, EPlaceKind K)
{
    if (K == EPlaceKind::Work || K == EPlaceKind::Unknown || K == EPlaceKind::Home)
    {
        return TEXT("Я работаю, на жизнь хватает.");
    }
    return FString::Printf(TEXT("Я %s работаю."), *PlaceAt(K));
}

FString FDialogueLines::AskPay(const FLineContext& C)
{
    return Pick({ TEXT("И как, платят нормально?"), TEXT("И как там, нравится?") });
}

FString FDialogueLines::AnswerPay(const FLineContext& C, bool bGood)
{
    return bGood
        ? Pick({ TEXT("Да, жаловаться не на что."), TEXT("Нравится.") })
        : Pick({ TEXT("Мало платят."), TEXT("Тяжело, но что делать.") });
}

// ---------------------------------------------------------------------------
//  Распоряжения
// ---------------------------------------------------------------------------

FString FDialogueLines::Instruct(const FLineContext& C, EActionType Action, EPlaceKind K, const FString& Way)
{
    const FString Go = Ty(C, TEXT("Иди"), TEXT("Идите"));
    const FString There = (K == EPlaceKind::Home || Way.IsEmpty())
        ? FString() : FString::Printf(TEXT(" — это %s"), *Way);
    const FString Dest = (K == EPlaceKind::Work) ? FString(TEXT("к нам на работу")) : PlaceTo(K);

    switch (Action)
    {
    case EActionType::Work:
        return FString::Printf(TEXT("%s. %s %s%s. Там люди нужны."),
            *Ty(C, TEXT("Тебе работа нужна"), TEXT("Вам работа нужна")), *Go, *Dest, *There);

    case EActionType::Read:
    case EActionType::Study:
        return FString::Printf(TEXT("%s. %s %s%s. %s"),
            *Ty(C, TEXT("Тебе учиться надо"), TEXT("Вам учиться надо")), *Go, *Dest, *There,
            *Ty(C, TEXT("Возьми азбуку и читай."), TEXT("Возьмите азбуку и читайте.")));

    case EActionType::Eat:
        return FString::Printf(TEXT("%s %s %s%s."),
            *Go, *Ty(C, TEXT("поешь"), TEXT("поешьте")), *PlaceAt(K), *There);

    case EActionType::Drink:
        return FString::Printf(TEXT("%s %s %s%s."),
            *Go, *Ty(C, TEXT("попей"), TEXT("попейте")), *PlaceAt(K), *There);

    case EActionType::Sleep:
    case EActionType::Rest:
    case EActionType::GoHome:
        return FString::Printf(TEXT("%s домой, %s."), *Go, *Ty(C, TEXT("отдохни"), TEXT("отдохните")));

    default:
        return FString::Printf(TEXT("%s %s%s."), *Ty(C, TEXT("Сходи"), TEXT("Сходите")), *Dest, *There);
    }
}

FString FDialogueLines::Obey(const FLineContext& C)
{
    return Pick({ TEXT("Хорошо, схожу."), TEXT("Ладно, так и сделаю.") });
}

FString FDialogueLines::AskForAdvice(const FLineContext& C)
{
    return Ty(C, TEXT("Что посоветуешь?"), TEXT("Что посоветуете?"));
}

FString FDialogueLines::RefuseOrder(const FLineContext& C, const FString& Reason)
{
    return Reason.IsEmpty()
        ? FString(TEXT("Не сейчас."))
        : FString::Printf(TEXT("Не могу сейчас — %s."), *Reason);
}

// ---------------------------------------------------------------------------
//  Проверка: откуда жителю взять каждое слово
// ---------------------------------------------------------------------------

void FDialogueLines::CollectSpeechWords(TSet<FString>& Out)
{
    auto Add = [&Out](const FString& Line)
    {
        TArray<FString> Words;
        Line.ParseIntoArray(Words, TEXT(" "), true);
        for (const FString& W : Words)
        {
            const FName Key = USpeechComponent::NormalizeWord(W);
            if (!Key.IsNone() && Key != FName(TEXT("икс")))
            {
                Out.Add(Key.ToString().ToLower());
            }
        }
    };

    static const ENeedType Needs[] = {
        ENeedType::Hunger, ENeedType::Thirst, ENeedType::Sleep, ENeedType::Bladder, ENeedType::Hygiene,
        ENeedType::Comfort, ENeedType::Safety, ENeedType::Health, ENeedType::Shelter, ENeedType::Money,
        ENeedType::Order, ENeedType::SocialContact, ENeedType::Belonging, ENeedType::Intimacy,
        ENeedType::Esteem, ENeedType::Achievement, ENeedType::Autonomy, ENeedType::Competence,
        ENeedType::Novelty, ENeedType::Beauty, ENeedType::Meaning };

    static const EActionType Actions[] = {
        EActionType::Work, EActionType::Eat, EActionType::Drink, EActionType::Cook, EActionType::Sleep,
        EActionType::GoHome, EActionType::Rest, EActionType::Wash, EActionType::Read, EActionType::Study,
        EActionType::Practice, EActionType::Entertain, EActionType::Exercise, EActionType::Wander,
        EActionType::Explore };

    static const EPlaceKind Places[] = {
        EPlaceKind::Food, EPlaceKind::Shop, EPlaceKind::Social, EPlaceKind::Rest, EPlaceKind::Beautiful,
        EPlaceKind::Study, EPlaceKind::Hospital, EPlaceKind::Workshop, EPlaceKind::Library,
        EPlaceKind::Market, EPlaceKind::Bathhouse, EPlaceKind::Bakery, EPlaceKind::TownHall,
        EPlaceKind::Work, EPlaceKind::Home };

    static const EEmotionType Feelings[] = {
        EEmotionType::Sadness, EEmotionType::Loneliness, EEmotionType::Anxiety, EEmotionType::Fear,
        EEmotionType::Anger, EEmotionType::Joy, EEmotionType::Shame, EEmotionType::Guilt,
        EEmotionType::Boredom, EEmotionType::Pride, EEmotionType::Love, EEmotionType::Nostalgia,
        EEmotionType::Curiosity, EEmotionType::Gratitude };

    static const TCHAR* Skills[] = {
        TEXT("Cooking"), TEXT("Reading"), TEXT("Writing"), TEXT("Counting"), TEXT("Arithmetic"),
        TEXT("Repair"), TEXT("Craft"), TEXT("Medicine"), TEXT("Music"), TEXT("Teaching"), TEXT("Trade"),
        TEXT("Empathy"), TEXT("Navigation"), TEXT("Story"), TEXT("Work"), TEXT("Fitness") };

    for (int32 Variant = 0; Variant < 48; ++Variant)
    {
        FLineContext C;
        C.MyName = TEXT("Икс");
        C.TheirName = (Variant % 3 == 0) ? FString() : FString(TEXT("Икс"));
        C.bMeFemale = (Variant & 1) != 0;
        C.bThemFemale = (Variant & 2) != 0;
        C.bFormal = (Variant & 4) != 0;
        C.Hour = 3 + (Variant * 5) % 22;
        C.Weather = (Variant % 4) * 0.25f;
        C.Mood = -0.8f + (Variant % 5) * 0.4f;

        const FVector To(FMath::Cos(Variant * 0.8f) * 9000.0f * (1 + Variant % 5),
                         FMath::Sin(Variant * 0.8f) * 9000.0f * (1 + Variant % 5), 0.0f);
        const FString Way = WayTo(FVector::ZeroVector, To * (Variant % 7 == 0 ? 0.01f : 1.0f));
        Add(Way);

        Add(Greet(C)); Add(GreetBack(C)); Add(Introduce(C)); Add(IntroduceBack(C));
        Add(HowAreYou(C)); Add(AndYou(C)); Add(Thanks(C)); Add(ThanksForKindness(C)); Add(NoProblem(C)); Add(Acknowledge(C));
        Add(Farewell(C, FString())); Add(FarewellBack(C)); Add(Busy(C, FString()));
        Add(MoodWord(C, C.Mood)); Add(DontKnowWhere(C));
        Add(AskWhatWritten(C)); Add(Retell(C, TEXT("Икс"))); Add(KnowThat(C)); Add(DidntKnow(C)); Add(CantRead(C));
        Add(TellReading(C, TEXT("Икс"), TEXT("§1. Икс")));
        Add(AskWhere(C)); Add(AskWhatHappened(C)); Add(AnswerWhatHappened(C, FString()));
        Add(AnswerWhatHappened(C, TEXT("Икс"))); Add(Console(C)); Add(GladForYou(C));
        Add(TellWeather(C)); Add(AgreeWeather(C));
        Add(TellDream(C, TEXT("Икс"))); Add(ReactDream(C)); Add(AskWhatStops(C));
        Add(AnswerWhatStops(C, true, false)); Add(AnswerWhatStops(C, false, true)); Add(AnswerWhatStops(C, false, false));
        Add(AskTeach(C)); Add(AgreeTeach(C)); Add(RefuseTeach(C));
        Add(TellNoJob(C)); Add(TellNoMoney(C)); Add(Encourage(C)); Add(OfferMoney(C)); Add(AcceptMoney(C));
        Add(AskPay(C)); Add(AnswerPay(C, true)); Add(AnswerPay(C, false));
        Add(Obey(C)); Add(RefuseOrder(C, FString())); Add(AskForAdvice(C));
        Add(DontKnowPerson(C, true)); Add(DontKnowPerson(C, false));
        Add(DisagreePerson(C, C.bThemFemale, true)); Add(DisagreePerson(C, C.bThemFemale, false));
        Add(AgreePerson(C, TEXT("Икс"), C.bThemFemale, true)); Add(AgreePerson(C, TEXT("Икс"), C.bThemFemale, false));
        Add(LeaveReason(C, ENeedType::Hunger, 0.0f, EActionType::Idle, true));

        for (uint8 R = 1; R <= uint8(EPersonReason::Unpleasant); ++R)
        {
            Add(TellPerson(C, TEXT("Икс"), C.bThemFemale, EPersonReason(R), Variant % 2 == 0));
        }
        for (ENeedType N : Needs)
        {
            Add(Feel(C, N)); Add(AskWhereFor(C, N)); Add(SameHere(C, N));
            Add(LeaveReason(C, N, 0.9f, EActionType::Idle, false));
        }
        for (EActionType A : Actions)
        {
            Add(Doing(C, A, true)); Add(Doing(C, A, false)); Add(ReactDoing(C, A));
            Add(LeaveReason(C, ENeedType::Hunger, 0.0f, A, false));
            for (EPlaceKind K : Places)
            {
                Add(Instruct(C, A, K, Way));
            }
        }
        for (EPlaceKind K : Places)
        {
            Add(PlaceName(K)); Add(AdvisePlace(C, K, Way)); Add(AnswerWhere(C, K, Way)); Add(TellWork(C, K));
            Add(TellPlace(C, K, 0.5f)); Add(TellPlace(C, K, -0.5f)); Add(TellPlace(C, K, 0.0f));
            Add(AgreePlace(C, K, 0.5f)); Add(AgreePlace(C, K, -0.5f));
            Add(DisagreePlace(C, K, 0.5f)); Add(DisagreePlace(C, K, -0.5f));
        }
        for (EEmotionType E : Feelings)
        {
            Add(TellFeeling(C, E));
        }
        for (const TCHAR* S : Skills)
        {
            Add(TellSkill(C, S));
        }
    }
}
