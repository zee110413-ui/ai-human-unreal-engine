#include "Textbook.h"
#include "Elements.h"

namespace
{
    FString StateWord(EMatterState State)
    {
        switch (State)
        {
        case EMatterState::Liquid: return TEXT("жидкое");
        case EMatterState::Gas:    return TEXT("воздушное");
        default:                   return TEXT("твёрдое");
        }
    }

    FString KindWord(EElementKind Kind)
    {
        switch (Kind)
        {
        case EElementKind::Metal:      return TEXT("металл");
        case EElementKind::Semimetal:  return TEXT("полуметалл");
        case EElementKind::Nonmetal:   return TEXT("неметалл");
        case EElementKind::Halogen:    return TEXT("едкое вещество");
        case EElementKind::NobleGas:   return TEXT("нелюдимый газ");
        case EElementKind::Lanthanide: return TEXT("редкая земля");
        case EElementKind::Actinide:   return TEXT("тяжёлая земля");
        default:                       return TEXT("вещество");
        }
    }

    FTextbookPage ElementPage(const FElement& Element)
    {
        FTextbookPage Page;
        Page.Title = Element.Name;
        Page.Difficulty = FMath::Clamp(0.3f + Element.Number / 260.0f, 0.3f, 0.85f);
        Page.Text = FString::Printf(
            TEXT("%s. Знак %s, номер %d. Это %s, %s. Вес доли %.1f. Плотность %.2f раза против воды. Плавится при %.0f градусах, кипит при %.0f. Находят: %s."),
            *Element.Name, *Element.Symbol, Element.Number, *KindWord(Element.Kind),
            *StateWord(Element.State), Element.Mass, Element.Density,
            Element.Melting, Element.Boiling, *Element.FoundIn);
        return Page;
    }

    FTextbookPage LawPage(const FWorldLaw& Law)
    {
        FTextbookPage Page;
        Page.Title = Law.Name;
        Page.Difficulty = 0.4f;
        Page.Text = Law.Unit.IsEmpty()
            ? Law.Statement
            : FString::Printf(TEXT("%s Мера: %.2f %s."), *Law.Statement, Law.Value, *Law.Unit);
        return Page;
    }

    FTextbook Make(const TCHAR* Subject, const TCHAR* Title, const TCHAR* Course, const TCHAR* Skill,
                   float Difficulty, float RequiredReading, TArray<FTextbookPage> Pages)
    {
        FTextbook B;
        B.Subject = Subject;
        B.Title = Title;
        B.Course = Course;
        B.Skill = Skill;
        B.SecondSkill = TEXT("Crafting");
        B.Topic = EWordTopic::Thought;
        B.Difficulty = Difficulty;
        B.RequiredReading = RequiredReading;
        B.Pages = MoveTemp(Pages);
        return B;
    }
}

void AddScienceBooks(TArray<FTextbook>& Books)
{
    TArray<FTextbookPage> Common;
    TArray<FTextbookPage> Rare;
    TArray<FTextbookPage> Made;

    Common.Add([]()
    {
        FTextbookPage P;
        P.Title = TEXT("из чего всё");
        P.Difficulty = 0.35f;
        P.Text = TEXT("Всякая вещь сложена из веществ, а вещества — из немногих начал. Их сто восемнадцать, и больше нет. Одни лежат под ногами, другие редки, а иные и вовсе не встречаются в земле.");
        return P;
    }());

    for (const FElement& Element : FElements::All())
    {
        if (Element.Crust >= 100.0f)
        {
            Common.Add(ElementPage(Element));
        }
        else if (Element.Crust > 0.0f)
        {
            Rare.Add(ElementPage(Element));
        }
        else
        {
            Made.Add(ElementPage(Element));
        }
    }

    Books.Add(Make(TEXT("ElementsCommon"), TEXT("вещества земли"), TEXT("природа"),
        TEXT("Masonry"), 0.45f, 0.4f, MoveTemp(Common)));
    Books.Add(Make(TEXT("ElementsRare"), TEXT("редкие вещества"), TEXT("природа"),
        TEXT("Smithing"), 0.6f, 0.5f, MoveTemp(Rare)));
    Books.Add(Make(TEXT("ElementsMade"), TEXT("вещества, которых нет в земле"), TEXT("природа"),
        TEXT("Smithing"), 0.75f, 0.6f, MoveTemp(Made)));

    TArray<FTextbookPage> Rocks;
    Rocks.Add([]()
    {
        FTextbookPage P;
        P.Title = TEXT("порода и вещество");
        P.Difficulty = 0.4f;
        P.Text = TEXT("Камень редко бывает из одного начала. Известняк сложен из кальция, углерода и кислорода, песок — из кремния и кислорода.");
        return P;
    }());

    for (const FMineral& Mineral : FElements::Minerals())
    {
        FString Of;
        for (const FString& Symbol : Mineral.Elements)
        {
            const FElement* Element = FElements::BySymbol(Symbol);
            Of += (Of.IsEmpty() ? TEXT("") : TEXT(", "));
            Of += Element ? Element->Name : Symbol;
        }

        FTextbookPage P;
        P.Title = Mineral.Name;
        P.Difficulty = 0.4f;
        P.Text = FString::Printf(
            TEXT("%s. Сложен из: %s. Тяжелее воды в %.1f раза, твёрдость %.0f из десяти. %s Лежит там, где %s."),
            *Mineral.Name, *Of, Mineral.Density, Mineral.Hardness,
            Mineral.bBurns ? TEXT("Горит.") : TEXT("Не горит."),
            *Mineral.Ground.ToString());
        Rocks.Add(P);
    }

    Books.Add(Make(TEXT("Rocks"), TEXT("из чего сложены камни"), TEXT("природа"),
        TEXT("Masonry"), 0.5f, 0.4f, MoveTemp(Rocks)));

    TArray<FTextbookPage> Laws;
    for (const FWorldLaw& Law : FElements::Physics())
    {
        Laws.Add(LawPage(Law));
    }
    Books.Add(Make(TEXT("Physics"), TEXT("начала физики"), TEXT("природа"),
        TEXT("Building"), 0.5f, 0.4f, MoveTemp(Laws)));

    TArray<FTextbookPage> Numbers;
    for (const FWorldLaw& Truth : FElements::Mathematics())
    {
        Numbers.Add(LawPage(Truth));
    }
    Books.Add(Make(TEXT("Geometry"), TEXT("начала математики"), TEXT("счёт"),
        TEXT("Counting"), 0.45f, 0.35f, MoveTemp(Numbers)));
}
