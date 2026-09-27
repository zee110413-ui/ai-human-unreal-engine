// Lexicon.cpp

#include "Lexicon.h"

// ---------------------------------------------------------------------------
//  Буквы
// ---------------------------------------------------------------------------

namespace Rus
{
    /** Гласная ли. */
    static bool Vowel(TCHAR C)
    {
        const FString V = TEXT("аеёиоуыэюяАЕЁИОУЫЭЮЯ");
        return V.Contains(FString::Chr(C));
    }

    /** Шипящая или ц: после них пишется «е», а не «о». */
    static bool Hissing(TCHAR C)
    {
        const FString H = TEXT("жшщчц");
        return H.Contains(FString::Chr(C));
    }

    /** Заднеязычная или шипящая: после них «и», а не «ы». */
    static bool NeedsI(TCHAR C)
    {
        const FString K = TEXT("кгхжшщч");
        return K.Contains(FString::Chr(C));
    }

    /** Мягкая основа. */
    static bool Soft(TCHAR C)
    {
        const FString S = TEXT("ьйчщ");
        return S.Contains(FString::Chr(C));
    }

    static TCHAR Last(const FString& S)
    {
        return S.Len() > 0 ? S[S.Len() - 1] : TEXT(' ');
    }

    static TCHAR At(const FString& S, int32 FromEnd)
    {
        const int32 Index = S.Len() - 1 - FromEnd;
        return S.IsValidIndex(Index) ? S[Index] : TEXT(' ');
    }

    static FString Cut(const FString& S, int32 Count)
    {
        return S.Left(FMath::Max(0, S.Len() - Count));
    }

    /**
     * Вставить беглую гласную: «сестра» → «сестёр», «окно» → «окон».
     * Правило приблизительное — оно покрывает частые случаи и не портит
     * остальные; полный разбор русской морфологии здесь не нужен.
     */
    static FString Fleeting(const FString& Stem)
    {
        if (Stem.Len() < 2)
        {
            return Stem;
        }

        const TCHAR A = Last(Stem);
        const TCHAR B = At(Stem, 1);

        if (!Vowel(A) && !Vowel(B) && A != TEXT('ь') && B != TEXT('ь'))
        {
            const FString Insert = (Hissing(B) || A == TEXT('й')) ? TEXT("е")
                                 : (A == TEXT('к') || A == TEXT('г') || A == TEXT('х')) ? TEXT("о")
                                 : TEXT("е");
            return Cut(Stem, 1) + Insert + FString::Chr(A);
        }
        return Stem;
    }
}

// ---------------------------------------------------------------------------
//  Склонение существительных
//
//  Русский язык здесь описан не полностью — полное описание потребовало бы
//  словаря исключений на десятки тысяч строк. Описаны три склонения и
//  правила правописания после шипящих; этого хватает, чтобы речь звучала
//  по-русски, а редкие огрехи в косвенных падежах слышны не больше, чем
//  оговорки у живого человека.
// ---------------------------------------------------------------------------

EGramGender ULexicon::GuessGender(const FString& Base)
{
    const TCHAR L = Rus::Last(Base);

    if (L == TEXT('а') || L == TEXT('я'))
    {
        return EGramGender::Feminine;
    }
    if (L == TEXT('о') || L == TEXT('е') || L == TEXT('ё'))
    {
        return EGramGender::Neuter;
    }
    if (L == TEXT('ь'))
    {
        // «ночь», «радость» — женский; «день», «конь» — мужской.
        // Общего правила тут нет, есть привычка: после шипящих и у слов
        // на -ость почти всегда женский, остальное приходится знать.
        if (Base.EndsWith(TEXT("ость")) || Base.EndsWith(TEXT("есть")) || Base.EndsWith(TEXT("чь"))
            || Base.EndsWith(TEXT("шь")) || Base.EndsWith(TEXT("щь")) || Base.EndsWith(TEXT("жь"))
            || Base.EndsWith(TEXT("знь")) || Base.EndsWith(TEXT("бь")) || Base.EndsWith(TEXT("пь")))
        {
            return EGramGender::Feminine;
        }

        static const TCHAR* Feminines[] = {
            TEXT("вещь"), TEXT("соль"), TEXT("боль"), TEXT("даль"), TEXT("мать"), TEXT("дочь"),
            TEXT("ночь"), TEXT("тень"), TEXT("дверь"), TEXT("постель"), TEXT("кровать"),
            TEXT("тетрадь"), TEXT("площадь"), TEXT("часть"), TEXT("жизнь"), TEXT("смерть"),
            TEXT("любовь"), TEXT("речь"), TEXT("помощь"), TEXT("память"), TEXT("осень"),
            TEXT("сеть"), TEXT("цепь"), TEXT("степь"), TEXT("грудь"), TEXT("кость"),
            TEXT("кровь"), TEXT("морковь"), TEXT("печь"), TEXT("мышь"), TEXT("рожь"),
            TEXT("суть"), TEXT("нить"), TEXT("роль"), TEXT("боязнь"), TEXT("метель")
        };
        for (const TCHAR* Word : Feminines)
        {
            if (Base == Word)
            {
                return EGramGender::Feminine;
            }
        }
        return EGramGender::Masculine;
    }
    if (L == TEXT('и') || L == TEXT('ы'))
    {
        return EGramGender::Plural;
    }
    return EGramGender::Masculine;
}

FString ULexicon::Decline(const FLexeme& Word, EGramCase Case, bool bPlural)
{
    const FString& B = Word.Base;
    if (B.Len() < 2)
    {
        return B;
    }

    const TCHAR L = Rus::Last(B);
    const TCHAR P = Rus::At(B, 1);

    // --- 1-е склонение: -а / -я --------------------------------------------
    if (L == TEXT('а') || L == TEXT('я'))
    {
        const FString S = Rus::Cut(B, 1);
        const bool bSoft = (L == TEXT('я'));
        const TCHAR SL = Rus::Last(S);

        if (!bPlural)
        {
            switch (Case)
            {
            case EGramCase::Nom: return B;
            case EGramCase::Gen: return S + (bSoft ? TEXT("и") : (Rus::NeedsI(SL) ? TEXT("и") : TEXT("ы")));
            case EGramCase::Dat: return S + TEXT("е");
            case EGramCase::Acc: return S + (bSoft ? TEXT("ю") : TEXT("у"));
            case EGramCase::Ins: return S + (bSoft ? TEXT("ей") : (Rus::Hissing(SL) ? TEXT("ей") : TEXT("ой")));
            case EGramCase::Pre: return S + TEXT("е");
            }
        }
        else
        {
            const FString Bare = bSoft ? Rus::Fleeting(S) + TEXT("ь") : Rus::Fleeting(S);
            switch (Case)
            {
            case EGramCase::Nom: return S + (bSoft ? TEXT("и") : (Rus::NeedsI(SL) ? TEXT("и") : TEXT("ы")));
            case EGramCase::Gen: return Bare;
            case EGramCase::Dat: return S + (bSoft ? TEXT("ям") : TEXT("ам"));
            case EGramCase::Acc: return Word.bAnimate ? Bare : S + (bSoft ? TEXT("и") : (Rus::NeedsI(SL) ? TEXT("и") : TEXT("ы")));
            case EGramCase::Ins: return S + (bSoft ? TEXT("ями") : TEXT("ами"));
            case EGramCase::Pre: return S + (bSoft ? TEXT("ях") : TEXT("ах"));
            }
        }
    }

    // --- Средний род: -о / -е ----------------------------------------------
    if (L == TEXT('о') || L == TEXT('е') || L == TEXT('ё'))
    {
        const FString S = Rus::Cut(B, 1);
        const bool bSoft = (L != TEXT('о'));
        const bool bIe = B.EndsWith(TEXT("ие")) || B.EndsWith(TEXT("ье"));

        if (!bPlural)
        {
            switch (Case)
            {
            case EGramCase::Nom:
            case EGramCase::Acc: return B;
            case EGramCase::Gen: return S + (bSoft ? TEXT("я") : TEXT("а"));
            case EGramCase::Dat: return S + (bSoft ? TEXT("ю") : TEXT("у"));
            case EGramCase::Ins: return S + (bSoft ? TEXT("ем") : TEXT("ом"));
            case EGramCase::Pre: return S + (bIe ? TEXT("и") : (bSoft ? TEXT("е") : TEXT("е")));
            }
        }
        else
        {
            switch (Case)
            {
            case EGramCase::Nom:
            case EGramCase::Acc: return S + (bSoft ? TEXT("я") : TEXT("а"));
            case EGramCase::Gen: return bIe ? S + TEXT("й") : Rus::Fleeting(S);
            case EGramCase::Dat: return S + (bSoft ? TEXT("ям") : TEXT("ам"));
            case EGramCase::Ins: return S + (bSoft ? TEXT("ями") : TEXT("ами"));
            case EGramCase::Pre: return S + (bSoft ? TEXT("ях") : TEXT("ах"));
            }
        }
    }

    // --- 3-е склонение: женский на -ь --------------------------------------
    if (L == TEXT('ь') && GuessGender(B) == EGramGender::Feminine)
    {
        const FString S = Rus::Cut(B, 1);
        if (!bPlural)
        {
            switch (Case)
            {
            case EGramCase::Nom:
            case EGramCase::Acc: return B;
            case EGramCase::Gen:
            case EGramCase::Dat:
            case EGramCase::Pre: return S + TEXT("и");
            case EGramCase::Ins: return S + TEXT("ью");
            }
        }
        else
        {
            switch (Case)
            {
            case EGramCase::Nom:
            case EGramCase::Acc: return S + TEXT("и");
            case EGramCase::Gen: return S + TEXT("ей");
            case EGramCase::Dat: return S + TEXT("ям");
            case EGramCase::Ins: return S + TEXT("ями");
            case EGramCase::Pre: return S + TEXT("ях");
            }
        }
    }

    // --- 2-е склонение: мужской на согласный, -ь, -й ------------------------
    {
        const bool bSoftEnd = (L == TEXT('ь') || L == TEXT('й'));
        const FString S = bSoftEnd ? Rus::Cut(B, 1) : B;
        const TCHAR SL = Rus::Last(S);
        const bool bSoft = bSoftEnd;

        if (!bPlural)
        {
            switch (Case)
            {
            case EGramCase::Nom: return B;
            case EGramCase::Gen: return S + (bSoft ? TEXT("я") : TEXT("а"));
            case EGramCase::Dat: return S + (bSoft ? TEXT("ю") : TEXT("у"));
            case EGramCase::Acc: return Word.bAnimate ? S + (bSoft ? TEXT("я") : TEXT("а")) : B;
            case EGramCase::Ins: return S + (bSoft ? TEXT("ем") : (Rus::Hissing(SL) ? TEXT("ем") : TEXT("ом")));
            case EGramCase::Pre: return S + TEXT("е");
            }
        }
        else
        {
            const FString NomPl = S + (bSoft ? TEXT("и") : (Rus::NeedsI(SL) ? TEXT("и") : TEXT("ы")));
            switch (Case)
            {
            case EGramCase::Nom: return NomPl;
            case EGramCase::Gen: return S + (bSoft ? TEXT("ей") : (Rus::Hissing(SL) ? TEXT("ей") : TEXT("ов")));
            case EGramCase::Dat: return S + (bSoft ? TEXT("ям") : TEXT("ам"));
            case EGramCase::Acc: return Word.bAnimate
                    ? S + (bSoft ? TEXT("ей") : (Rus::Hissing(SL) ? TEXT("ей") : TEXT("ов")))
                    : NomPl;
            case EGramCase::Ins: return S + (bSoft ? TEXT("ями") : TEXT("ами"));
            case EGramCase::Pre: return S + (bSoft ? TEXT("ях") : TEXT("ах"));
            }
        }
    }

    return B;
}

// ---------------------------------------------------------------------------
//  Прилагательные
// ---------------------------------------------------------------------------

FString ULexicon::Agree(const FLexeme& Adjective, EGramGender Gender, EGramCase Case)
{
    const FString& B = Adjective.Base;
    if (B.Len() < 3)
    {
        return B;
    }

    const FString S = Rus::Cut(B, 2);                       // основа без -ый/-ий/-ой
    const TCHAR SL = Rus::Last(S);
    const bool bSoft = B.EndsWith(TEXT("ий")) && !Rus::NeedsI(SL);
    const bool bStressed = B.EndsWith(TEXT("ой"));

    // После к, г, х, ж, ш, щ, ч пишется «и», а не «ы».
    const FString Y = Rus::NeedsI(SL) ? TEXT("и") : (bSoft ? TEXT("и") : TEXT("ы"));
    const FString O = (Rus::Hissing(SL) || bSoft) ? TEXT("е") : TEXT("о");

    switch (Gender)
    {
    case EGramGender::Masculine:
        switch (Case)
        {
        case EGramCase::Nom:
        case EGramCase::Acc: return B;
        case EGramCase::Gen: return S + O + TEXT("го");
        case EGramCase::Dat: return S + O + TEXT("му");
        case EGramCase::Ins: return S + Y + TEXT("м");
        case EGramCase::Pre: return S + O + TEXT("м");
        }
        break;

    case EGramGender::Feminine:
        switch (Case)
        {
        case EGramCase::Nom: return S + (bSoft ? TEXT("яя") : TEXT("ая"));
        case EGramCase::Acc: return S + (bSoft ? TEXT("юю") : TEXT("ую"));
        default:             return S + O + TEXT("й");
        }
        break;

    case EGramGender::Neuter:
        switch (Case)
        {
        case EGramCase::Nom:
        case EGramCase::Acc: return S + (bSoft ? TEXT("ее") : (Rus::Hissing(SL) && !bStressed ? TEXT("ее") : TEXT("ое")));
        case EGramCase::Gen: return S + O + TEXT("го");
        case EGramCase::Dat: return S + O + TEXT("му");
        case EGramCase::Ins: return S + Y + TEXT("м");
        case EGramCase::Pre: return S + O + TEXT("м");
        }
        break;

    case EGramGender::Plural:
        switch (Case)
        {
        case EGramCase::Nom:
        case EGramCase::Acc: return S + Y + TEXT("е");
        case EGramCase::Gen:
        case EGramCase::Pre: return S + Y + TEXT("х");
        case EGramCase::Dat: return S + Y + TEXT("м");
        case EGramCase::Ins: return S + Y + TEXT("ми");
        }
        break;
    }

    return B;
}

FString ULexicon::ShortForm(const FLexeme& Adjective, EGramGender Gender)
{
    const FString& B = Adjective.Base;
    if (B.Len() < 3)
    {
        return B;
    }

    FString S = Rus::Cut(B, 2);

    switch (Gender)
    {
    case EGramGender::Feminine: return S + TEXT("а");
    case EGramGender::Neuter:   return S + (Rus::Hissing(Rus::Last(S)) ? TEXT("е") : TEXT("о"));
    case EGramGender::Plural:   return S + (Rus::NeedsI(Rus::Last(S)) ? TEXT("и") : TEXT("ы"));
    default:
        // «умный» → «умён»: в мужском роде часто всплывает беглая гласная.
        return Rus::Fleeting(S);
    }
}

// ---------------------------------------------------------------------------
//  Глаголы
// ---------------------------------------------------------------------------

FString ULexicon::Conjugate(const FLexeme& Verb, EGramPerson Person, bool bPlural)
{
    const FString& B = Verb.Base;
    if (B.Len() < 3)
    {
        return B;
    }

    // Второе спряжение — глаголы на -ить.
    if (B.EndsWith(TEXT("ить")))
    {
        const FString S = Rus::Cut(B, 3);
        const TCHAR SL = Rus::Last(S);

        if (Person == EGramPerson::First && !bPlural)
        {
            // «просить» → «прошу»: чередование. Берём общий случай.
            return S + (Rus::Hissing(SL) || Rus::Soft(SL) ? TEXT("у") : TEXT("ю"));
        }
        if (bPlural)
        {
            switch (Person)
            {
            case EGramPerson::First:  return S + TEXT("им");
            case EGramPerson::Second: return S + TEXT("ите");
            default:                  return S + (Rus::Hissing(SL) ? TEXT("ат") : TEXT("ят"));
            }
        }
        return S + (Person == EGramPerson::Second ? TEXT("ишь") : TEXT("ит"));
    }

    // Первое спряжение: -ать, -ять, -еть, -ыть.
    if (B.EndsWith(TEXT("ть")))
    {
        const FString S = Rus::Cut(B, 2);            // «думать» → «дума»
        const TCHAR SL = Rus::Last(S);
        const bool bSoftStem = (SL == TEXT('я') || SL == TEXT('е') || SL == TEXT('и'));
        const FString Yu = bSoftStem ? TEXT("ю") : (Rus::Hissing(SL) ? TEXT("у") : TEXT("ю"));

        if (bPlural)
        {
            switch (Person)
            {
            case EGramPerson::First:  return S + TEXT("ем");
            case EGramPerson::Second: return S + TEXT("ете");
            default:                  return S + (bSoftStem ? TEXT("ют") : TEXT("ют"));
            }
        }
        switch (Person)
        {
        case EGramPerson::First:  return S + Yu;
        case EGramPerson::Second: return S + TEXT("ешь");
        default:                  return S + TEXT("ет");
        }
    }

    return B;
}

FString ULexicon::Past(const FLexeme& Verb, EGramGender Gender)
{
    const FString& B = Verb.Base;
    if (!B.EndsWith(TEXT("ть")) || B.Len() < 4)
    {
        return B;
    }

    const FString S = Rus::Cut(B, 2);

    switch (Gender)
    {
    case EGramGender::Feminine: return S + TEXT("ла");
    case EGramGender::Neuter:   return S + TEXT("ло");
    case EGramGender::Plural:   return S + TEXT("ли");
    default:                    return S + TEXT("л");
    }
}

FString ULexicon::Imperative(const FLexeme& Verb, bool bPolite)
{
    const FString& B = Verb.Base;
    if (B.Len() < 4)
    {
        return B;
    }

    FString Stem;
    if (B.EndsWith(TEXT("ить")))
    {
        Stem = Rus::Cut(B, 3) + TEXT("и");
    }
    else if (B.EndsWith(TEXT("ать")) || B.EndsWith(TEXT("ять")))
    {
        Stem = Rus::Cut(B, 2) + TEXT("й");
    }
    else if (B.EndsWith(TEXT("ть")))
    {
        Stem = Rus::Cut(B, 2) + TEXT("й");
    }
    else
    {
        Stem = B;
    }

    return bPolite ? Stem + TEXT("те") : Stem;
}

void ULexicon::AllForms(const FLexeme& Word, TArray<FString>& Out)
{
    switch (Word.Class)
    {
    case EWordClass::Noun:
        for (int32 C = 0; C < 6; ++C)
        {
            Out.Add(Decline(Word, static_cast<EGramCase>(C), false));
            Out.Add(Decline(Word, static_cast<EGramCase>(C), true));
        }
        break;

    case EWordClass::Adjective:
        for (int32 G = 0; G < 4; ++G)
        {
            for (int32 C = 0; C < 6; ++C)
            {
                Out.Add(Agree(Word, static_cast<EGramGender>(G), static_cast<EGramCase>(C)));
            }
            Out.Add(ShortForm(Word, static_cast<EGramGender>(G)));
        }
        break;

    case EWordClass::Verb:
        Out.Add(Word.Base);
        for (int32 P = 0; P < 3; ++P)
        {
            Out.Add(Conjugate(Word, static_cast<EGramPerson>(P), false));
            Out.Add(Conjugate(Word, static_cast<EGramPerson>(P), true));
        }
        for (int32 G = 0; G < 4; ++G)
        {
            Out.Add(Past(Word, static_cast<EGramGender>(G)));
        }
        Out.Add(Imperative(Word, false));
        Out.Add(Imperative(Word, true));
        break;

    default:
        Out.Add(Word.Base);
        break;
    }
}

FString ULexicon::Stem(const FString& Word)
{
    FString W = Word.ToLower().TrimStartAndEnd();

    // Отбрасываем самые ходовые окончания — грубо, но одинаково для всех,
    // а значит сравнение слов остаётся честным.
    static const TCHAR* Endings[] = {
        TEXT("ями"), TEXT("ами"), TEXT("ого"), TEXT("ему"), TEXT("ому"), TEXT("ыми"), TEXT("ими"),
        TEXT("ешь"), TEXT("ишь"), TEXT("ете"), TEXT("ите"), TEXT("ают"), TEXT("яют"), TEXT("уют"),
        TEXT("ей"), TEXT("ов"), TEXT("ям"), TEXT("ам"), TEXT("ях"), TEXT("ах"), TEXT("ые"), TEXT("ие"),
        TEXT("ая"), TEXT("ое"), TEXT("ую"), TEXT("ых"), TEXT("их"), TEXT("ым"), TEXT("им"),
        TEXT("ла"), TEXT("ло"), TEXT("ли"), TEXT("ть"), TEXT("ет"), TEXT("ит"), TEXT("ют"), TEXT("ят"),
        TEXT("ем"), TEXT("им"), TEXT("ой"), TEXT("ей"), TEXT("ью"),
        TEXT("а"), TEXT("я"), TEXT("о"), TEXT("е"), TEXT("у"), TEXT("ю"), TEXT("ы"), TEXT("и"),
        TEXT("ь"), TEXT("й"), TEXT("л")
    };

    for (const TCHAR* End : Endings)
    {
        const int32 Len = FCString::Strlen(End);
        if (W.Len() > Len + 2 && W.EndsWith(End))
        {
            return W.Left(W.Len() - Len);
        }
    }
    return W;
}

// ---------------------------------------------------------------------------
//  Сам словарь
// ---------------------------------------------------------------------------

ULexicon& ULexicon::Get()
{
    static ULexicon* Instance = nullptr;
    if (!Instance)
    {
        Instance = NewObject<ULexicon>();
        Instance->AddToRoot();
        Instance->Build();
    }
    return *Instance;
}

void ULexicon::Add(const TCHAR* Base, EWordClass Class, EWordTopic Topic, float Valence,
                   float Frequency, bool bAnimate)
{
    FLexeme L;
    L.Base = Base;
    L.Class = Class;
    L.Topic = Topic;
    L.Valence = Valence;
    L.Frequency = Frequency;
    L.bAnimate = bAnimate;
    L.Gender = (Class == EWordClass::Noun) ? GuessGender(L.Base) : EGramGender::Masculine;

    const int32 Index = Words.Add(L);

    ByBase.Add(L.Base, Index);
    ByClass.FindOrAdd(static_cast<uint8>(Class)).Add(Index);
    const uint16 Key = static_cast<uint16>(static_cast<uint8>(Class)) * 256 + static_cast<uint8>(Topic);
    ByClassTopic.FindOrAdd(Key).Add(Index);
}

const FLexeme* ULexicon::Find(const FString& Base) const
{
    if (const int32* Index = ByBase.Find(Base))
    {
        return &Words[*Index];
    }
    return nullptr;
}

const FLexeme* ULexicon::Random(EWordClass Class, EWordTopic Topic) const
{
    const uint16 Key = static_cast<uint16>(static_cast<uint8>(Class)) * 256 + static_cast<uint8>(Topic);
    if (const TArray<int32>* Pool = ByClassTopic.Find(Key))
    {
        if (Pool->Num() > 0)
        {
            return &Words[(*Pool)[FMath::RandRange(0, Pool->Num() - 1)]];
        }
    }
    return Random(Class);
}

const FLexeme* ULexicon::Random(EWordClass Class) const
{
    if (const TArray<int32>* Pool = ByClass.Find(static_cast<uint8>(Class)))
    {
        if (Pool->Num() > 0)
        {
            return &Words[(*Pool)[FMath::RandRange(0, Pool->Num() - 1)]];
        }
    }
    return nullptr;
}

const FLexeme* ULexicon::RandomByMood(EWordClass Class, EWordTopic Topic, float Mood, float Tolerance) const
{
    const uint16 Key = static_cast<uint16>(static_cast<uint8>(Class)) * 256 + static_cast<uint8>(Topic);
    const TArray<int32>* Pool = ByClassTopic.Find(Key);
    if (!Pool || Pool->Num() == 0)
    {
        Pool = ByClass.Find(static_cast<uint8>(Class));
    }
    if (!Pool || Pool->Num() == 0)
    {
        return nullptr;
    }

    // Человек в горе тянется к горьким словам, в радости — к светлым.
    // Ищем не идеальное совпадение, а подходящее по настроению.
    const float Want = FMath::Clamp(Mood, -1.0f, 1.0f);

    const int32 Tries = FMath::Min(12, Pool->Num());
    const FLexeme* Best = nullptr;
    float BestGap = BIG_NUMBER;

    for (int32 i = 0; i < Tries; ++i)
    {
        const FLexeme& Candidate = Words[(*Pool)[FMath::RandRange(0, Pool->Num() - 1)]];
        const float Gap = FMath::Abs(Candidate.Valence - Want);
        if (Gap < BestGap)
        {
            BestGap = Gap;
            Best = &Candidate;
        }
        if (Gap <= Tolerance)
        {
            return &Candidate;
        }
    }
    return Best;
}

void ULexicon::CollectTopic(EWordTopic Topic, EWordClass Class, TArray<const FLexeme*>& Out) const
{
    const uint16 Key = static_cast<uint16>(static_cast<uint8>(Class)) * 256 + static_cast<uint8>(Topic);
    if (const TArray<int32>* Pool = ByClassTopic.Find(Key))
    {
        for (int32 Index : *Pool)
        {
            Out.Add(&Words[Index]);
        }
    }
}
