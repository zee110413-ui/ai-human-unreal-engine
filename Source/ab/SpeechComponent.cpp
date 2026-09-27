// SpeechComponent.cpp

#include "SpeechComponent.h"
#include "Improvise.h"
#include "Textbook.h"

USpeechComponent::USpeechComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

// ---------------------------------------------------------------------------
//  ЯЗЫК
//
//  Никто не рождается со словами, и взрослые здесь тоже начинают с нуля.
//  Новое слово берётся только из книги; услышанное лишь закрепляет то,
//  что уже встречалось на странице.
// ---------------------------------------------------------------------------

FName USpeechComponent::NormalizeWord(const FString& Raw)
{
    // Движок опускает в строчные только латиницу: «Привет» в начале фразы
    // и «привет» в середине считались разными словами. Кириллицу — вручную.
    const FString Junk = TEXT(".,!?;:«»\"()—-…");
    FString Result;
    for (TCHAR Ch : Raw)
    {
        if (Junk.Contains(FString(1, &Ch)))
        {
            continue;
        }
        if (Ch >= 0x0410 && Ch <= 0x042F)
        {
            Ch = TCHAR(Ch + 0x20);
        }
        else if (Ch == 0x0401 || Ch == 0x0451)
        {
            Ch = TCHAR(0x0435);   // ё читают и пишут как е
        }
        else if (Ch >= 'A' && Ch <= 'Z')
        {
            Ch = TCHAR(Ch + 32);
        }
        Result.AppendChar(Ch);
    }
    return Result.IsEmpty() ? NAME_None : FName(*Result);
}

int32 USpeechComponent::GetVocabularySize() const
{
    int32 Count = 0;
    for (const TPair<FName, float>& Pair : Vocabulary)
    {
        if (Pair.Value > 0.45f)
        {
            ++Count;
        }
    }
    return Count;
}

void USpeechComponent::SetupForAge(float Age)
{
    // Слов нет ни у кого: их ещё предстоит прочитать.
    Vocabulary.Reset();
    Letters.Reset();
    Grounded.Reset();
    bFluent = false;
    Fluency = 0.0f;
    bChildVoice = Age < 13.0f;
    EarAge = Age;
}

void USpeechComponent::LearnOralSpeech(float Age)
{
    EarAge = Age;
    if (Age < 2.5f)
    {
        return;
    }
    const int32 Passes = Age >= 6.0f ? 4 : (Age >= 4.0f ? 2 : 1);
    for (const TCHAR* Subject : { TEXT("Reader"), TEXT("Village") })
    {
        if (const FTextbook* Book = FLibrary::Find(Subject))
        {
            for (int32 Pass = 0; Pass < Passes; ++Pass)
            {
                for (const FTextbookPage& Page : Book->Pages)
                {
                    LearnWordsFromBook(Page.Text, 1.0f, Age < 13.0f);
                }
            }
        }
    }
    Letters.Reset();
}

FString USpeechComponent::Articulate(const FString& Intended, const TSet<FString>* Names,
                                    const FString& SelfName, const FString& ListenerName) const
{
    if (Intended.IsEmpty())
    {
        return Intended;
    }

    TArray<FString> Words;
    Intended.ParseIntoArray(Words, TEXT(" "), true);

    const FString Junk = TEXT(".,!?;:«»\"()—-…");
    auto Bare = [&Junk](const FString& Word)
    {
        FString Out;
        for (TCHAR Ch : Word)
        {
            if (!Junk.Contains(FString(1, &Ch)))
            {
                Out.AppendChar(Ch);
            }
        }
        return Out;
    };

    const FName Self = NormalizeWord(SelfName);
    const FName Listener = NormalizeWord(ListenerName);

    TArray<FString> WithMarks;   // слова со знаками — для связной речи
    TArray<FString> Fragments;   // голые слова — для обрывков
    int32 Meaningful = 0;
    int32 KnownWords = 0;
    bool bSaidOwnName = false;
    bool bSaidTheirName = false;

    for (const FString& Word : Words)
    {
        const FName Key = NormalizeWord(Word);
        if (Key.IsNone())
        {
            WithMarks.Add(Word);
            continue;
        }
        ++Meaningful;

        const bool bName = Names && Names->Contains(Key.ToString());
        const float* Mastery = Vocabulary.Find(Key);

        // Слово, которого человек не читал, он произнести не может.
        if (Mastery && *Mastery > 0.45f)
        {
            WithMarks.Add(Word);
            Fragments.Add(Bare(Word));
            ++KnownWords;
        }
        else if (bName)
        {
            WithMarks.Add(Word);
            // Имя без слов имеет смысл, только если это своё или того, к кому обращаешься.
            if (Key == Self || Key == Listener)
            {
                Fragments.Add(Bare(Word));
                bSaidOwnName |= (Key == Self);
                bSaidTheirName |= (Key == Listener);
            }
        }
    }

    // --- Слов нет: остаются жест и, может быть, имя --------------------------
    if (KnownWords == 0)
    {
        if (bSaidOwnName)
        {
            return FString::Printf(TEXT("%s. (показывает на себя)"), *SelfName);
        }
        if (bSaidTheirName)
        {
            return FString::Printf(TEXT("%s! (машет рукой)"), *ListenerName);
        }
        static const TCHAR* InfantBabble[] = { TEXT("Агу!"), TEXT("Уа-уа!"), TEXT("Ба-ба-ба"), TEXT("Гу-гу"), TEXT("Ма-ма-ма") };
        if (EarAge < 1.5f)
        {
            return FString(InfantBabble[FMath::RandRange(0, UE_ARRAY_COUNT(InfantBabble) - 1)]);
        }
        static const TCHAR* ChildBabble[] = { TEXT("а..."), TEXT("ы-ы"), TEXT("м-м") };
        static const TCHAR* AdultBabble[] = { TEXT("Э-э..."), TEXT("М-м..."), TEXT("(кивает)"), TEXT("(разводит руками)") };
        return bChildVoice
            ? FString(ChildBabble[FMath::RandRange(0, UE_ARRAY_COUNT(ChildBabble) - 1)])
            : FString(AdultBabble[FMath::RandRange(0, UE_ARRAY_COUNT(AdultBabble) - 1)]);
    }

    // --- Говорит обрывками ---------------------------------------------------
    const float Coverage = static_cast<float>(KnownWords) / FMath::Max(1, Meaningful);
    if (Coverage < 0.65f)
    {
        return FString::Join(Fragments, TEXT("... ")) + TEXT("...");
    }

    return FString::Join(WithMarks, TEXT(" "));
}

float USpeechComponent::Comprehension(const FString& Text) const
{
    return ComprehensionWithNames(Text, TSet<FString>());
}

float USpeechComponent::ComprehensionWithNames(const FString& Text, const TSet<FString>& Names) const
{
    TArray<FString> Words;
    Text.ParseIntoArray(Words, TEXT(" "), true);

    int32 Meaningful = 0;
    int32 Understood = 0;
    for (const FString& Word : Words)
    {
        const FName Key = NormalizeWord(Word);
        if (Key.IsNone())
        {
            continue;
        }
        ++Meaningful;
        const float* Mastery = Vocabulary.Find(Key);
        if ((Mastery && *Mastery > 0.4f) || Names.Contains(Key.ToString()))
        {
            ++Understood;
        }
    }
    return Meaningful > 0 ? static_cast<float>(Understood) / Meaningful : 0.0f;
}

void USpeechComponent::LearnWords(const FString& HeardText, float Attention, bool bChild)
{
    if (HeardText.IsEmpty())
    {
        return;
    }

    TArray<FString> Words;
    HeardText.ParseIntoArray(Words, TEXT(" "), true);

    // На слух слово только закрепляется: незнакомое так и остаётся звуком.
    const float Rate = (bChild ? 0.05f : 0.03f) * FMath::Clamp(Attention, 0.05f, 1.0f);
    const float Ripe = FMath::Clamp((EarAge - 0.55f) / 1.2f, 0.0f, 1.0f);
    const bool bAbsorbs = EarAge < 7.0f;
    const float Fresh = 0.07f * Ripe * FMath::Clamp(Attention, 0.05f, 1.0f);
    for (const FString& Word : Words)
    {
        const FName Key = NormalizeWord(Word);
        if (Key.IsNone())
        {
            continue;
        }
        if (float* Mastery = Vocabulary.Find(Key))
        {
            *Mastery = FMath::Clamp(*Mastery + FMath::Max(Rate, Fresh), 0.0f, 1.0f);
        }
        else if (bAbsorbs && EarAge >= 0.3f)
        {
            Vocabulary.Add(Key, Fresh);
        }
        ++WordsHeard;
    }
    if (bAbsorbs)
    {
        Fluency = FMath::Clamp(GetVocabularySize() / 400.0f, 0.0f, 1.0f);
        bFluent = Fluency > 0.9f;
    }
}

void USpeechComponent::LearnWordsFromBook(const FString& Text, float Attention, bool bChild)
{
    if (Text.IsEmpty())
    {
        return;
    }

    TArray<FString> Words;
    Text.ParseIntoArray(Words, TEXT(" "), true);

    // Встретил слово на странице — оно начинает оседать. Трёх-четырёх
    // встреч хватает, чтобы узнать его и произнести самому.
    const float Rate = (bChild ? 0.26f : 0.2f) * FMath::Clamp(Attention, 0.05f, 1.0f);
    for (const FString& Word : Words)
    {
        const FName Key = NormalizeWord(Word);
        if (Key.IsNone())
        {
            continue;
        }
        float& Mastery = Vocabulary.FindOrAdd(Key);
        Mastery = FMath::Clamp(Mastery + Rate, 0.0f, 1.0f);
    }

    // Свободной речь становится, когда слов набирается на обычный разговор.
    Fluency = FMath::Clamp(GetVocabularySize() / 400.0f, 0.0f, 1.0f);
    bFluent = Fluency > 0.9f;
}

namespace
{
    TCHAR LowerLetter(TCHAR Ch)
    {
        if (Ch >= 0x0410 && Ch <= 0x042F)
        {
            return TCHAR(Ch + 0x20);
        }
        if (Ch == 0x0401 || Ch == 0x0451)
        {
            return TCHAR(0x0435);
        }
        return Ch;
    }

    bool IsLetter(TCHAR Ch)
    {
        return Ch >= 0x0430 && Ch <= 0x044F;
    }
}

void USpeechComponent::NoticeThing(const FString& Thing)
{
    const FName Key = NormalizeWord(Thing);
    if (!Key.IsNone())
    {
        Grounded.Add(Key);
    }
}

bool USpeechComponent::HasSeen(const FString& Thing) const
{
    return Grounded.Contains(NormalizeWord(Thing));
}

float USpeechComponent::LetterFamiliarity(TCHAR Letter) const
{
    const float* Known = Letters.Find(FString::Chr(LowerLetter(Letter)));
    return Known ? *Known : 0.0f;
}

float USpeechComponent::Literacy() const
{
    int32 Known = 0;
    for (const TPair<FString, float>& Pair : Letters)
    {
        if (Pair.Value >= 0.5f)
        {
            ++Known;
        }
    }
    return FMath::Clamp(Known / 32.0f, 0.0f, 1.0f);
}

float USpeechComponent::Legibility(FName Word) const
{
    const FString Text = Word.ToString();
    int32 Total = 0;
    int32 Known = 0;
    for (TCHAR Ch : Text)
    {
        if (!IsLetter(Ch))
        {
            continue;
        }
        ++Total;
        if (LetterFamiliarity(Ch) >= 0.5f)
        {
            ++Known;
        }
    }
    return Total > 0 ? static_cast<float>(Known) / static_cast<float>(Total) : 0.0f;
}

int32 USpeechComponent::ReadText(const FString& Text, const TArray<FString>& Pictures, const FString& Heading, float Attention, bool bChild)
{
    const float Focus = FMath::Clamp(Attention, 0.05f, 1.0f);
    const float WordRate = (bChild ? 0.26f : 0.2f) * Focus;
    const float LetterRate = (bChild ? 0.2f : 0.14f) * Focus;

    auto Strengthen = [this](TCHAR Ch, float Amount)
    {
        if (IsLetter(Ch) && Amount > 0.0f)
        {
            float& Known = Letters.FindOrAdd(FString::Chr(Ch));
            Known = FMath::Clamp(Known + Amount, 0.0f, 1.0f);
        }
    };

    TMap<FName, float> Shown;
    for (const FString& Picture : Pictures)
    {
        const FName Key = NormalizeWord(Picture);
        if (Key.IsNone())
        {
            continue;
        }
        const float Link = Grounded.Contains(Key) ? 1.0f : 0.25f;
        Shown.Add(Key, Link);
        float& Mastery = Vocabulary.FindOrAdd(Key);
        Mastery = FMath::Clamp(Mastery + WordRate * Link, 0.0f, 1.0f);
        for (TCHAR Ch : Key.ToString())
        {
            Strengthen(Ch, LetterRate * 0.3f * Link);
        }
    }

    if (Shown.Num() > 0)
    {
        TCHAR Headline = 0;
        for (TCHAR Ch : Heading)
        {
            const TCHAR Low = LowerLetter(Ch);
            if (IsLetter(Low))
            {
                Headline = Low;
                break;
            }
        }
        if (Headline != 0)
        {
            float Link = 0.0f;
            for (const TPair<FName, float>& Pair : Shown)
            {
                if (Pair.Key.ToString().Contains(FString::Chr(Headline), ESearchCase::CaseSensitive))
                {
                    Link = FMath::Max(Link, Pair.Value);
                }
            }
            Strengthen(Headline, LetterRate * Link);
        }
    }

    TArray<FString> Raw;
    Text.ParseIntoArrayWS(Raw);
    TArray<FName> Words;
    for (const FString& Piece : Raw)
    {
        const FName Key = NormalizeWord(Piece);
        if (!Key.IsNone())
        {
            Words.Add(Key);
        }
    }

    int32 Legible = 0;
    int32 Understood = 0;
    for (const FName& Word : Words)
    {
        if (Shown.Contains(Word) || Legibility(Word) >= 1.0f)
        {
            ++Legible;
            const float* Mastery = Vocabulary.Find(Word);
            if (Mastery && *Mastery > 0.4f)
            {
                ++Understood;
            }
        }
    }
    if (Legible == 0)
    {
        return 0;
    }

    const float Context = static_cast<float>(Understood) / static_cast<float>(FMath::Max(1, Words.Num()));
    for (const FName& Word : Words)
    {
        if (Shown.Contains(Word) || Legibility(Word) < 1.0f)
        {
            continue;
        }
        float& Mastery = Vocabulary.FindOrAdd(Word);
        const float Gain = Mastery > 0.4f ? WordRate * 0.35f : WordRate * FMath::Max(0.1f, Context);
        Mastery = FMath::Clamp(Mastery + Gain, 0.0f, 1.0f);
        for (TCHAR Ch : Word.ToString())
        {
            Strengthen(Ch, LetterRate * 0.04f);
        }
    }

    Fluency = FMath::Clamp(GetVocabularySize() / 400.0f, 0.0f, 1.0f);
    bFluent = Fluency > 0.9f;
    return Legible;
}

void USpeechComponent::LearnLettersFrom(const USpeechComponent& Teacher, const FString& Shown, float Amount)
{
    TSet<TCHAR> Done;
    for (TCHAR Ch : Shown)
    {
        const TCHAR Low = LowerLetter(Ch);
        if (!IsLetter(Low) || Done.Contains(Low) || Teacher.LetterFamiliarity(Low) < 0.5f)
        {
            continue;
        }
        Done.Add(Low);
        float& Known = Letters.FindOrAdd(FString::Chr(Low));
        Known = FMath::Clamp(Known + Amount, 0.0f, 1.0f);
    }
}

void USpeechComponent::MasterLetters()
{
    for (TCHAR Ch = 0x0430; Ch <= 0x044F; ++Ch)
    {
        Letters.Add(FString::Chr(Ch), 1.0f);
    }
}

FString USpeechComponent::Pick(const TArray<FString>& Options)
{
    if (Options.Num() == 0)
    {
        return FString();
    }
    return Options[FMath::RandRange(0, Options.Num() - 1)];
}

FString USpeechComponent::Address(const FSpeechContext& Ctx)
{
    // Как человек обращается — уже половина сказанного.
    if (Ctx.Resentment > 0.5f)
    {
        return FString();  // обиженный не называет по имени
    }
    if (Ctx.Closeness > 0.55f && !Ctx.ListenerName.IsEmpty())
    {
        return Ctx.ListenerName + TEXT(", ");
    }
    if (Ctx.Kind == ERelationKind::Stranger)
    {
        return Pick({ TEXT("Извините, "), TEXT("Простите, "), FString() });
    }
    if (!Ctx.ListenerName.IsEmpty() && FMath::FRand() < 0.5f)
    {
        return Ctx.ListenerName + TEXT(", ");
    }
    return FString();
}

FString USpeechComponent::TimeGreeting(int32 Hour)
{
    if (Hour < 5)  return TEXT("Не спится?");
    if (Hour < 12) return TEXT("Доброе утро");
    if (Hour < 18) return TEXT("Добрый день");
    if (Hour < 23) return TEXT("Добрый вечер");
    return TEXT("Поздно уже");
}

FString USpeechComponent::ActLabel(ESpeechAct Act)
{
    switch (Act)
    {
    case ESpeechAct::Greet:      return TEXT("приветствие");
    case ESpeechAct::SmallTalk:  return TEXT("болтовня");
    case ESpeechAct::Question:   return TEXT("вопрос");
    case ESpeechAct::Answer:     return TEXT("ответ");
    case ESpeechAct::ShareNews:  return TEXT("новость");
    case ESpeechAct::Gossip:     return TEXT("сплетня");
    case ESpeechAct::Request:    return TEXT("просьба");
    case ESpeechAct::Offer:      return TEXT("предложение помощи");
    case ESpeechAct::Refuse:     return TEXT("отказ");
    case ESpeechAct::Agree:      return TEXT("согласие");
    case ESpeechAct::Compliment: return TEXT("похвала");
    case ESpeechAct::Insult:     return TEXT("оскорбление");
    case ESpeechAct::Joke:       return TEXT("шутка");
    case ESpeechAct::Complain:   return TEXT("жалоба");
    case ESpeechAct::Boast:      return TEXT("хвастовство");
    case ESpeechAct::Confess:    return TEXT("признание");
    case ESpeechAct::Lie:        return TEXT("ложь");
    case ESpeechAct::Apologize:  return TEXT("извинение");
    case ESpeechAct::Thank:      return TEXT("благодарность");
    case ESpeechAct::Console:    return TEXT("утешение");
    case ESpeechAct::Threaten:   return TEXT("угроза");
    case ESpeechAct::Advise:     return TEXT("совет");
    case ESpeechAct::Farewell:   return TEXT("прощание");
    default:                     return TEXT("молчание");
    }
}

// ---------------------------------------------------------------------------
//  Что сказать
// ---------------------------------------------------------------------------

ESpeechAct USpeechComponent::ChooseAct(const FSpeechContext& Ctx) const
{
    struct FOption { ESpeechAct Act; float Weight; };
    TArray<FOption> Options;

    // --- Сначала то, что перебивает всё остальное ---------------------------

    // Первая встреча требует представиться — иначе никак.
    if (Ctx.bFirstMeeting)
    {
        return ESpeechAct::Greet;
    }

    // Долг извиниться гложет, пока не отдашь.
    if (Ctx.bIOweApology)
    {
        Options.Add({ ESpeechAct::Apologize, 2.5f });
    }

    // Человеку напротив плохо — и это видно.
    if (Ctx.bTheyLookUpset)
    {
        Options.Add({ ESpeechAct::Console, 1.2f + Ctx.Agreeableness * 1.5f + Ctx.Closeness * 1.2f });
        Options.Add({ ESpeechAct::Question, 0.8f + Ctx.Closeness });
    }

    // Я ему должен — надо бы поблагодарить.
    if (Ctx.Debt > 0.4f)
    {
        Options.Add({ ESpeechAct::Thank, 1.0f + Ctx.Debt });
    }

    // --- Обида окрашивает всё -----------------------------------------------
    if (Ctx.Resentment > 0.45f)
    {
        Options.Add({ ESpeechAct::Insult, Ctx.Resentment * (1.5f - Ctx.Agreeableness) });
        Options.Add({ ESpeechAct::Silence, Ctx.Resentment * 1.5f });
        Options.Add({ ESpeechAct::Threaten, Ctx.Resentment * (1.0f - Ctx.Agreeableness) * 0.6f });
        if (Ctx.Closeness > 0.4f)
        {
            // На близкого обижаются вслух — это попытка починить, а не сломать.
            Options.Add({ ESpeechAct::Complain, Ctx.Resentment * 1.2f });
        }
    }

    // --- Он неправильно обо мне думает --------------------------------------
    // Самый человеческий из поводов заговорить: не сказать что-то, а
    // поправить чужое представление о себе. «Он решил, что я на него
    // злюсь» — и надо как-то показать, что нет.
    if (Ctx.bHeMisreadsMe > 0.3f)
    {
        Options.Add({ ESpeechAct::Compliment, Ctx.bHeMisreadsMe * 1.4f });
        Options.Add({ ESpeechAct::Offer, Ctx.bHeMisreadsMe * 0.9f });
        Options.Add({ ESpeechAct::Joke, Ctx.bHeMisreadsMe * 0.7f * Ctx.Extraversion });
    }

    // --- Обычный разговор ---------------------------------------------------
    Options.Add({ ESpeechAct::SmallTalk, 0.8f + Ctx.Extraversion * 0.7f });
    Options.Add({ ESpeechAct::Question, 0.5f + Ctx.Closeness * 0.8f + Ctx.Agreeableness * 0.4f });

    if (!Ctx.MemoryToShare.IsEmpty())
    {
        Options.Add({ ESpeechAct::ShareNews, 0.6f + Ctx.Extraversion * 0.6f });
    }
    if (!Ctx.OpinionToShare.IsEmpty())
    {
        Options.Add({ ESpeechAct::Advise, 0.4f + (1.0f - Ctx.Agreeableness) * 0.4f });
    }
    if (Ctx.GossipSubject)
    {
        // Сплетничают с теми, кому доверяют, — это парадокс, но так и есть.
        Options.Add({ ESpeechAct::Gossip, 0.3f + Ctx.Trust * 0.9f + Ctx.Extraversion * 0.4f });
    }

    // --- Эмоция ищет выхода -------------------------------------------------
    switch (Ctx.Emotion)
    {
    case EEmotionType::Joy:
    case EEmotionType::Pride:
        Options.Add({ ESpeechAct::Boast, Ctx.EmotionIntensity * (0.4f + Ctx.SelfEsteem * 0.8f) });
        Options.Add({ ESpeechAct::Joke, Ctx.EmotionIntensity * (0.5f + Ctx.Extraversion) });
        Options.Add({ ESpeechAct::Compliment, Ctx.EmotionIntensity * Ctx.Agreeableness });
        break;

    case EEmotionType::Sadness:
    case EEmotionType::Loneliness:
    case EEmotionType::Anxiety:
        // Жалуются только тем, кому доверяют. Остальным — молчат.
        Options.Add({ ESpeechAct::Complain, Ctx.EmotionIntensity * (Ctx.Trust * 1.4f) });
        Options.Add({ ESpeechAct::Silence, Ctx.EmotionIntensity * (1.0f - Ctx.Trust) });
        if (Ctx.Closeness > 0.5f)
        {
            Options.Add({ ESpeechAct::Confess, Ctx.EmotionIntensity * Ctx.Closeness });
        }
        break;

    case EEmotionType::Anger:
    case EEmotionType::Contempt:
        Options.Add({ ESpeechAct::Insult, Ctx.EmotionIntensity * (1.2f - Ctx.Agreeableness) });
        Options.Add({ ESpeechAct::Silence, Ctx.EmotionIntensity * 0.7f });
        break;

    case EEmotionType::Gratitude:
        Options.Add({ ESpeechAct::Thank, Ctx.EmotionIntensity * 1.6f });
        break;

    case EEmotionType::Shame:
    case EEmotionType::Guilt:
        Options.Add({ ESpeechAct::Apologize, Ctx.EmotionIntensity * 1.2f });
        Options.Add({ ESpeechAct::Silence, Ctx.EmotionIntensity * 0.9f });
        // Соврать, чтобы не выглядеть виноватым, — очень человеческий ход.
        Options.Add({ ESpeechAct::Lie, Ctx.EmotionIntensity * (1.0f - Ctx.Honesty) * 1.1f });
        break;

    case EEmotionType::Affection:
    case EEmotionType::Love:
        Options.Add({ ESpeechAct::Compliment, Ctx.EmotionIntensity * 1.4f });
        Options.Add({ ESpeechAct::Confess, Ctx.EmotionIntensity * Ctx.Closeness * 1.2f });
        break;

    case EEmotionType::Fear:
        Options.Add({ ESpeechAct::Silence, Ctx.EmotionIntensity * 1.3f });
        Options.Add({ ESpeechAct::Request, Ctx.EmotionIntensity * Ctx.Trust });
        break;

    case EEmotionType::Curiosity:
        Options.Add({ ESpeechAct::Question, Ctx.EmotionIntensity * 1.5f });
        break;

    case EEmotionType::Envy:
        Options.Add({ ESpeechAct::Gossip, Ctx.EmotionIntensity * 0.8f });
        Options.Add({ ESpeechAct::Silence, Ctx.EmotionIntensity * 0.6f });
        break;

    default:
        break;
    }

    // --- Нужда толкает просить ----------------------------------------------
    if (Ctx.Need == ENeedType::Hunger || Ctx.Need == ENeedType::Money || Ctx.Need == ENeedType::Shelter)
    {
        // Просить стыдно — тем стыднее, чем выше самооценка и меньше доверия.
        Options.Add({ ESpeechAct::Request, 0.8f * Ctx.Trust * (1.2f - Ctx.SelfEsteem * 0.5f) });
    }

    // --- Интроверт вообще предпочитает промолчать ---------------------------
    Options.Add({ ESpeechAct::Silence, 0.4f + (1.0f - Ctx.Extraversion) * 0.9f + Ctx.Stress * 0.5f });

    // --- Взвешенный выбор ---------------------------------------------------
    float Total = 0.0f;
    for (const FOption& O : Options)
    {
        Total += FMath::Max(0.0f, O.Weight);
    }
    if (Total <= 0.0f)
    {
        return ESpeechAct::SmallTalk;
    }

    float Roll = FMath::FRandRange(0.0f, Total);
    for (const FOption& O : Options)
    {
        Roll -= FMath::Max(0.0f, O.Weight);
        if (Roll <= 0.0f)
        {
            return O.Act;
        }
    }
    return ESpeechAct::SmallTalk;
}

// ---------------------------------------------------------------------------
//  Как это звучит
// ---------------------------------------------------------------------------

FString USpeechComponent::RenderText(ESpeechAct Act, const FSpeechContext& Ctx)
{
    const FString Addr = Address(Ctx);
    const bool bWarm = Ctx.Liking > 0.3f;
    const bool bCold = Ctx.Liking < -0.2f || Ctx.Resentment > 0.3f;

    switch (Act)
    {
    case ESpeechAct::Greet:
        if (Ctx.bFirstMeeting)
        {
            return Pick({
                FString::Printf(TEXT("Здравствуйте. Я %s."), *Ctx.SpeakerName),
                FString::Printf(TEXT("%s. А вас как?"), *Ctx.SpeakerName),
                FString::Printf(TEXT("Добрый день. Меня зовут %s."), *Ctx.SpeakerName)
            });
        }
        if (bWarm)
        {
            return Pick({
                FString::Printf(TEXT("%sО, рад(а) тебя видеть!"), *Addr),
                FString::Printf(TEXT("%sнаконец-то! Сколько не виделись."), *Addr),
                TimeGreeting(Ctx.Hour) + TEXT("! Как ты?")
            });
        }
        if (bCold)
        {
            return Pick({ TEXT("Здравствуй."), TEXT("А, это ты."), TEXT("Привет.") });
        }
        return Pick({ TimeGreeting(Ctx.Hour) + TEXT("."), Addr + TEXT("здравствуйте."), TEXT("Привет.") });

    case ESpeechAct::SmallTalk:
        if (Ctx.Hour < 8)
        {
            return Pick({ TEXT("Рано сегодня поднялся."), TEXT("Не спится что-то."), TEXT("Холодно с утра.") });
        }
        if (Ctx.Hour > 21)
        {
            return Pick({ TEXT("Поздно уже, а всё хожу."), TEXT("День какой-то длинный вышел."), TEXT("Пора бы домой.") });
        }
        return Pick({
            TEXT("Как день проходит?"),
            TEXT("Ничего нового?"),
            TEXT("Погода-то какая."),
            TEXT("Опять всё как всегда."),
            TEXT("Народу сегодня немного."),
            TEXT("Куда все спешат вечно...")
        });

    case ESpeechAct::Question:
        if (Ctx.bTheyLookUpset)
        {
            return Pick({
                Addr + TEXT("у тебя всё в порядке?"),
                TEXT("Что-то случилось?"),
                TEXT("Ты какой-то не такой сегодня. Что?")
            });
        }
        if (Ctx.Closeness > 0.5f)
        {
            return Pick({
                TEXT("А ты вообще как? По-настоящему?"),
                TEXT("Расскажи, что у тебя."),
                TEXT("Ты о чём сейчас думаешь?")
            });
        }
        return Pick({
            TEXT("А ты давно тут живёшь?"),
            TEXT("Не подскажешь, что тут вообще происходит?"),
            TEXT("Чем занимаешься?"),
            TEXT("Ты кого-то ждёшь?")
        });

    case ESpeechAct::Answer:
        if (Ctx.Mood < -0.3f)
        {
            return Pick({ TEXT("Да так... нормально."), TEXT("Бывало и лучше."), TEXT("Не спрашивай.") });
        }
        if (Ctx.Mood > 0.3f)
        {
            return Pick({ TEXT("Да всё хорошо, спасибо."), TEXT("Неплохо, если честно."), TEXT("Грех жаловаться.") });
        }
        return Pick({ TEXT("Да как обычно."), TEXT("Ничего особенного."), TEXT("Живу помаленьку.") });

    case ESpeechAct::ShareNews:
        return Ctx.MemoryToShare.IsEmpty()
            ? TEXT("Тут такое было недавно...")
            : Pick({
                FString::Printf(TEXT("Слушай, тут такое: %s"), *Ctx.MemoryToShare),
                FString::Printf(TEXT("Представляешь — %s"), *Ctx.MemoryToShare),
                FString::Printf(TEXT("А я ведь %s"), *Ctx.MemoryToShare)
              });

    case ESpeechAct::Gossip:
        if (Ctx.GossipValence < 0.0f)
        {
            return Pick({
                FString::Printf(TEXT("Ты только никому: %s — не тот человек, за кого себя выдаёт."), *Ctx.GossipSubjectName),
                FString::Printf(TEXT("С %s поосторожнее."), *Ctx.GossipSubjectName),
                FString::Printf(TEXT("Я про %s такое слышал(а)... ну да ладно."), *Ctx.GossipSubjectName)
            });
        }
        return Pick({
            FString::Printf(TEXT("%s — хороший человек, что бы там ни говорили."), *Ctx.GossipSubjectName),
            FString::Printf(TEXT("Если что — обращайся к %s, не откажет."), *Ctx.GossipSubjectName)
        });

    case ESpeechAct::Request:
        switch (Ctx.Need)
        {
        case ENeedType::Hunger:  return Pick({ Addr + TEXT("нет ли чего поесть?"), TEXT("Я со вчера ничего не ел(а). Не выручишь?") });
        case ENeedType::Money:   return Pick({ Addr + TEXT("не одолжишь немного?"), TEXT("Мне бы перехватить до получки.") });
        case ENeedType::Shelter: return Pick({ TEXT("Мне сегодня некуда идти."), TEXT("Можно я побуду рядом? Ненадолго.") });
        default:                 return Pick({ Addr + TEXT("помоги, а?"), TEXT("Мне нужна помощь.") });
        }

    case ESpeechAct::Offer:
        return Pick({
            TEXT("Тебе помочь?"),
            Addr + TEXT("давай я."),
            TEXT("Если что нужно — говори.")
        });

    case ESpeechAct::Refuse:
        if (Ctx.Agreeableness > 0.6f)
        {
            return Pick({ TEXT("Прости, сейчас никак."), TEXT("Рад(а) бы, но не могу."), TEXT("В другой раз, ладно?") });
        }
        return Pick({ TEXT("Нет."), TEXT("Не могу."), TEXT("Это не ко мне.") });

    case ESpeechAct::Agree:
        return Pick({ TEXT("Да, ты прав(а)."), TEXT("Согласен(на)."), TEXT("И я о том же.") });

    case ESpeechAct::Compliment:
        if (Ctx.Closeness > 0.6f)
        {
            return Pick({
                TEXT("Хорошо, что ты есть."),
                TEXT("С тобой как-то легче."),
                Addr + TEXT("я тебе правда рад(а).")
            });
        }
        return Pick({
            TEXT("Хорошо выглядишь."),
            TEXT("Ты молодец, честно."),
            TEXT("Мне нравится, как ты это делаешь.")
        });

    case ESpeechAct::Insult:
        if (Ctx.Resentment > 0.7f)
        {
            return Pick({
                TEXT("Я тебе этого не забуду."),
                TEXT("Знаешь что? Иди-ка ты."),
                TEXT("Ты вообще себя со стороны видел(а)?")
            });
        }
        return Pick({
            TEXT("Да что ты понимаешь."),
            TEXT("Отстань."),
            TEXT("Слушать тебя тошно.")
        });

    case ESpeechAct::Joke:
        return Pick({
            TEXT("Живём один раз — и то не факт."),
            TEXT("Всё будет хорошо. Потом."),
            TEXT("У меня два состояния: устал и очень устал."),
            TEXT("Говорят, счастье любит тишину. У меня тут прямо дворец.")
        });

    case ESpeechAct::Complain:
        if (!Ctx.ComplaintText.IsEmpty())
        {
            return FString::Printf(TEXT("Честно? %s"), *Ctx.ComplaintText);
        }
        return Pick({
            TEXT("Устал(а) я от всего этого."),
            TEXT("Ничего не выходит, за что ни возьмись."),
            TEXT("Иногда думаю — а зачем вообще стараться."),
            TEXT("Никому ведь не нужно, понимаешь?")
        });

    case ESpeechAct::Boast:
        return Pick({
            TEXT("А у меня, между прочим, получилось."),
            TEXT("Меня, кстати, сегодня хвалили."),
            TEXT("Я тут кое-что сделал(а). Сам(а) не ожидал(а).")
        });

    case ESpeechAct::Confess:
        if (!Ctx.DreamText.IsEmpty() && FMath::FRand() < 0.5f)
        {
            return FString::Printf(TEXT("Знаешь, о чём я мечтаю? %s"), *Ctx.DreamText);
        }
        return Pick({
            TEXT("Я тебе такого никогда не говорил(а)..."),
            TEXT("Мне страшно. Вот прямо сейчас страшно."),
            TEXT("Я не справляюсь. Просто делаю вид."),
            TEXT("Ты для меня важнее, чем я показываю.")
        });

    case ESpeechAct::Lie:
        return Pick({
            TEXT("Да нет, всё в порядке."),
            TEXT("Меня там вообще не было."),
            TEXT("Это не я."),
            TEXT("Я как раз собирался(ась) этим заняться.")
        });

    case ESpeechAct::Apologize:
        if (Ctx.Closeness > 0.5f)
        {
            return Pick({
                TEXT("Прости меня. Правда прости."),
                TEXT("Я был(а) неправ(а). Совсем."),
                TEXT("Не знаю, что на меня нашло. Прости.")
            });
        }
        return Pick({ TEXT("Извини."), TEXT("Прошу прощения."), TEXT("Виноват(а).") });

    case ESpeechAct::Thank:
        return Pick({
            TEXT("Спасибо тебе."),
            TEXT("Я этого не забуду."),
            TEXT("Спасибо. Правда.")
        });

    case ESpeechAct::Console:
        return Pick({
            TEXT("Эй. Всё пройдёт."),
            TEXT("Я рядом, если что."),
            TEXT("Не надо ничего говорить. Просто посиди."),
            TEXT("Тебе сейчас тяжело. Это нормально.")
        });

    case ESpeechAct::Threaten:
        return Pick({
            TEXT("Ещё раз — и пожалеешь."),
            TEXT("Не доводи."),
            TEXT("Я предупредил(а).")
        });

    case ESpeechAct::Advise:
        return Ctx.OpinionToShare.IsEmpty()
            ? Pick({ TEXT("Я бы на твоём месте не спешил(а)."), TEXT("Подумай ещё раз.") })
            : FString::Printf(TEXT("Вот что я тебе скажу: %s"), *Ctx.OpinionToShare);

    case ESpeechAct::Farewell:
        if (bWarm)
        {
            return Pick({ TEXT("Ну, бывай. Заходи."), TEXT("Береги себя."), TEXT("До встречи!") });
        }
        return Pick({ TEXT("Ладно, пойду."), TEXT("Всего доброго."), TEXT("Пока.") });

    default:
        return FString();
    }
}

// ---------------------------------------------------------------------------
//  Сборка реплики
// ---------------------------------------------------------------------------

FUtterance USpeechComponent::Compose(ESpeechAct Act, const FSpeechContext& Ctx, AActor* Speaker)
{
    FUtterance U;
    U.Speaker = Speaker;
    U.Listener = Ctx.Listener;
    U.Act = Act;
    U.bTruthful = (Act != ESpeechAct::Lie);

    // =======================================================================
    //  ОТКУДА БЕРЁТСЯ РЕПЛИКА
    //
    //  Приветствие и извинение у всех людей похожи — их и берём готовыми.
    //  А всё остальное человек сочиняет на ходу: берёт слова, которые сейчас
    //  на языке, и складывает фразу. Оттого два разговора никогда не выходят
    //  одинаковыми, даже если говорят об одном и том же.
    // =======================================================================
    const bool bRitual = (Act == ESpeechAct::Greet || Act == ESpeechAct::Farewell
                       || Act == ESpeechAct::Apologize || Act == ESpeechAct::Thank);

    FString Composed;
    if (!bRitual)
    {
        FImproviseContext Mind;
        Mind.Topic = Topic;
        Mind.Mood = Ctx.Mood;
        Mind.Closeness = Ctx.Closeness;
        Mind.Talkativeness = Talkativeness;
        Mind.Turn = TurnsInConversation;
        Mind.bFemale = bFemale;
        Mind.bChild = !bFluent;

        // --- Отвечаем на услышанное ----------------------------------------
        // Если собеседник только что о чём-то сказал, человек отвечает
        // именно на это, а не заводит речь заново. Из этого и складывается
        // разговор вместо двух не связанных между собой монологов.
        if (Heard.IsValid() && FMath::FRand() < 0.75f)
        {
            // Соглашаться или спорить — зависит от нрава и от того,
            // насколько сказанное расходится с собственным мнением.
            const bool bAgree = FMath::FRand() < (0.45f + Ctx.Agreeableness * 0.45f
                                                + Ctx.Liking * 0.2f);
            Composed = FPhrase::ReplyAbout(Heard, bAgree, Mind);

            // И, ответив, нередко спрашивают о том же в ответ.
            if (!Composed.IsEmpty() && Talkativeness > 0.45f && FMath::FRand() < 0.4f)
            {
                const FString Back = FPhrase::AskAbout(Heard, Mind);
                if (!Back.IsEmpty())
                {
                    // Между своей репликой и встречным вопросом нужна точка,
                    // иначе выходит «не сказал бы а ты где работаешь».
                    const TCHAR Last = Composed[Composed.Len() - 1];
                    if (Last != TEXT('.') && Last != TEXT('!') && Last != TEXT('?'))
                    {
                        Composed += TEXT(".");
                    }
                    Composed += TEXT(" ") + Back;
                }
            }
        }

        // --- Заводим своё ---------------------------------------------------
        if (Composed.IsEmpty() && Speaking.IsValid())
        {
            Composed = (Act == ESpeechAct::Question)
                ? FPhrase::AskAbout(Speaking, Mind)
                : FPhrase::SayAbout(Speaking, Mind);
        }

        // --- Ни того, ни другого --------------------------------------------
        if (Composed.IsEmpty())
        {
            Composed = Ctx.bIsResponse && RecentUtterances.Num() > 0
                ? FPhrase::Respond(Ctx.RespondingTo, RecentUtterances.Last().Text, Mind)
                : FPhrase::Say(Act, Mind);
        }
    }

    // Реплика несёт свой предмет: собеседник ответит именно на него.
    U.Point = Speaking;

    // Шаблон остаётся опорой: если сочинить не вышло, человек скажет
    // привычное — как и мы, когда не находим слов.
    U.Text = Composed.IsEmpty() ? RenderText(Act, Ctx) : Composed;
    ++TurnsInConversation;

    // Тон: от чего говорится, а не что говорится.
    float Tone = Ctx.Mood * 0.4f + Ctx.Liking * 0.4f - Ctx.Resentment * 0.6f;
    switch (Act)
    {
    case ESpeechAct::Compliment:
    case ESpeechAct::Thank:
    case ESpeechAct::Console:
    case ESpeechAct::Offer:      Tone += 0.5f; break;
    case ESpeechAct::Insult:
    case ESpeechAct::Threaten:   Tone -= 0.8f; break;
    case ESpeechAct::Complain:   Tone -= 0.3f; break;
    case ESpeechAct::Joke:       Tone += 0.3f; break;
    default: break;
    }
    U.Tone = FMath::Clamp(Tone, -1.0f, 1.0f);

    // Сплетня и совет несут с собой убеждение — так знание (и клевета)
    // расходится по городу.
    if (Act == ESpeechAct::Gossip && Ctx.GossipSubject)
    {
        FBelief Payload;
        Payload.Subject = FName(*Ctx.GossipSubjectName);
        Payload.Predicate = TEXT("IsGoodPerson");
        Payload.Value = FMath::Clamp(Ctx.GossipValence, -1.0f, 1.0f);
        Payload.Confidence = 0.5f;
        Payload.Source = Speaker;
        Payload.Text = U.Text;
        U.Payload = Payload;
        U.bHasPayload = true;
    }

    LastSaid = U.Text;
    return U;
}

void USpeechComponent::Remember(const FUtterance& Utterance)
{
    RecentUtterances.Add(Utterance);
    while (RecentUtterances.Num() > HistoryCapacity)
    {
        RecentUtterances.RemoveAt(0);
    }
}

// ---------------------------------------------------------------------------
//  Как это отзовётся
// ---------------------------------------------------------------------------

FSpeechEffect USpeechComponent::EvaluateEffect(const FUtterance& U, float ListenerTrust,
                                               float ListenerCloseness, float ListenerEmpathy)
{
    FSpeechEffect E;
    E.Pleasantness = U.Tone;
    E.SatisfiedNeed = ENeedType::SocialContact;
    E.SatisfactionAmount = 0.05f;

    switch (U.Act)
    {
    case ESpeechAct::Greet:
        E.SatisfactionAmount = 0.08f;
        E.bExpectsReply = true;
        break;

    case ESpeechAct::SmallTalk:
        E.SatisfactionAmount = 0.10f;
        E.bExpectsReply = true;
        break;

    case ESpeechAct::Question:
        E.SatisfactionAmount = 0.12f;
        E.bExpectsReply = true;
        // Когда о тебе спрашивают, это само по себе тепло.
        E.SatisfiedNeed = ENeedType::Belonging;
        E.EvokedEmotion = EEmotionType::Affection;
        E.EvokedIntensity = 0.12f * (0.5f + ListenerCloseness);
        break;

    case ESpeechAct::Compliment:
        E.SatisfiedNeed = ENeedType::Esteem;
        E.SatisfactionAmount = 0.22f;
        E.EvokedEmotion = EEmotionType::Joy;
        E.EvokedIntensity = 0.25f;
        E.TrustDelta = 0.02f;
        // От неискреннего человека похвала не греет.
        if (ListenerTrust < 0.25f)
        {
            E.SatisfactionAmount *= 0.4f;
            E.EvokedEmotion = EEmotionType::Surprise;
        }
        break;

    case ESpeechAct::Insult:
        E.SatisfiedNeed = ENeedType::Esteem;
        E.SatisfactionAmount = -0.30f;
        E.EvokedEmotion = EEmotionType::Anger;
        E.EvokedIntensity = 0.45f + ListenerCloseness * 0.35f; // от своего больнее
        E.TrustDelta = -0.12f;
        break;

    case ESpeechAct::Console:
        E.SatisfiedNeed = ENeedType::Intimacy;
        E.SatisfactionAmount = 0.30f;
        E.EvokedEmotion = EEmotionType::Gratitude;
        E.EvokedIntensity = 0.30f;
        E.TrustDelta = 0.06f;
        break;

    case ESpeechAct::Confess:
        // Откровенность — это подарок доверия, и она сближает обоих.
        E.SatisfiedNeed = ENeedType::Intimacy;
        E.SatisfactionAmount = 0.35f;
        E.EvokedEmotion = EEmotionType::Compassion;
        E.EvokedIntensity = 0.3f * (0.4f + ListenerEmpathy);
        E.TrustDelta = 0.10f;
        break;

    case ESpeechAct::Complain:
        E.SatisfiedNeed = ENeedType::SocialContact;
        E.SatisfactionAmount = 0.05f;
        E.EvokedEmotion = (ListenerEmpathy > 0.5f) ? EEmotionType::Compassion : EEmotionType::Boredom;
        E.EvokedIntensity = 0.2f;
        E.bExpectsReply = true;
        break;

    case ESpeechAct::Boast:
        E.EvokedEmotion = (ListenerCloseness > 0.5f) ? EEmotionType::Joy : EEmotionType::Envy;
        E.EvokedIntensity = 0.18f;
        E.SatisfactionAmount = 0.02f;
        break;

    case ESpeechAct::Thank:
        E.SatisfiedNeed = ENeedType::Esteem;
        E.SatisfactionAmount = 0.18f;
        E.EvokedEmotion = EEmotionType::Joy;
        E.EvokedIntensity = 0.2f;
        E.TrustDelta = 0.03f;
        break;

    case ESpeechAct::Apologize:
        E.SatisfactionAmount = 0.12f;
        E.EvokedEmotion = EEmotionType::Relief;
        E.EvokedIntensity = 0.25f;
        E.TrustDelta = 0.05f;
        break;

    case ESpeechAct::Threaten:
        E.SatisfiedNeed = ENeedType::Safety;
        E.SatisfactionAmount = -0.35f;
        E.EvokedEmotion = EEmotionType::Fear;
        E.EvokedIntensity = 0.5f;
        E.TrustDelta = -0.2f;
        break;

    case ESpeechAct::Joke:
        E.SatisfactionAmount = 0.14f;
        E.EvokedEmotion = EEmotionType::Joy;
        E.EvokedIntensity = 0.15f;
        break;

    case ESpeechAct::Gossip:
        E.SatisfactionAmount = 0.12f;
        E.EvokedEmotion = EEmotionType::Curiosity;
        E.EvokedIntensity = 0.2f;
        // Со сплетником делятся охотно, но доверяют ему чуть меньше.
        E.TrustDelta = -0.01f;
        break;

    case ESpeechAct::Request:
        E.bExpectsReply = true;
        E.SatisfactionAmount = 0.0f;
        E.EvokedEmotion = EEmotionType::Compassion;
        E.EvokedIntensity = 0.2f * ListenerEmpathy;
        break;

    case ESpeechAct::Lie:
        E.SatisfactionAmount = 0.04f;
        break;

    case ESpeechAct::Silence:
        // Молчание тоже действует — и часто хуже слов.
        E.SatisfactionAmount = -0.04f;
        if (ListenerCloseness > 0.5f)
        {
            E.EvokedEmotion = EEmotionType::Anxiety;
            E.EvokedIntensity = 0.15f;
        }
        break;

    default:
        break;
    }

    return E;
}

bool USpeechComponent::DetectLie(const FUtterance& U, float ListenerEmpathySkill,
                                 float SpeakerDeceptionSkill, float Familiarity)
{
    if (U.bTruthful)
    {
        return false;
    }

    // Ложь раскрывают не логикой, а тем, что человека хорошо знают.
    const float DetectionPower = ListenerEmpathySkill * 0.45f + Familiarity * 0.45f;
    const float Chance = FMath::Clamp(DetectionPower - SpeakerDeceptionSkill * 0.6f + 0.15f, 0.02f, 0.9f);
    return FMath::FRand() < Chance;
}
