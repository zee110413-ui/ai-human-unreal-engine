#include "Textbook.h"
#include "HumanTypes.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"

namespace
{
    EWordTopic TopicOf(const FString& Name)
    {
        if (Name == TEXT("Work"))
        {
            return EWordTopic::Work;
        }
        if (Name == TEXT("Body"))
        {
            return EWordTopic::Body;
        }
        if (Name == TEXT("Child"))
        {
            return EWordTopic::Child;
        }
        if (Name == TEXT("City"))
        {
            return EWordTopic::City;
        }
        if (Name == TEXT("Feeling"))
        {
            return EWordTopic::Feeling;
        }
        return EWordTopic::Thought;
    }

    bool Header(const FString& Line, const TCHAR* Key, FString& Out)
    {
        const FString Tag = FString(Key) + TEXT(":");
        if (!Line.StartsWith(Tag))
        {
            return false;
        }
        Out = Line.RightChop(Tag.Len()).TrimStartAndEnd();
        return true;
    }

    bool ReadBook(const FString& Path, FTextbook& Out)
    {
        TArray<FString> Lines;
        if (!FFileHelper::LoadFileToStringArray(Lines, *Path))
        {
            return false;
        }
        FString Value;
        FString PageTitle;
        FString PageText;
        float PageDifficulty = 0.3f;
        Out = FTextbook();
        Out.Difficulty = 0.4f;
        Out.RequiredReading = 0.3f;

        auto Flush = [&Out, &PageTitle, &PageText, &PageDifficulty]()
        {
            const FString Body = PageText.TrimStartAndEnd();
            if (Body.Len() < 20)
            {
                PageText.Reset();
                return;
            }
            FTextbookPage Page;
            Page.Title = PageTitle;
            Page.Text = Body;
            Page.Difficulty = PageDifficulty;
            Out.Pages.Add(MoveTemp(Page));
            PageText.Reset();
        };

        for (const FString& Line : Lines)
        {
            if (Header(Line, TEXT("SUBJECT"), Value))
            {
                Out.Subject = FName(*Value);
            }
            else if (Header(Line, TEXT("TITLE"), Value))
            {
                Out.Title = Value;
            }
            else if (Header(Line, TEXT("COURSE"), Value))
            {
                Out.Course = Value;
            }
            else if (Header(Line, TEXT("GRADE"), Value))
            {
                Out.Grade = FCString::Atoi(*Value);
            }
            else if (Header(Line, TEXT("SKILL"), Value))
            {
                Out.Skill = FName(*Value);
            }
            else if (Header(Line, TEXT("TOPIC"), Value))
            {
                Out.Topic = TopicOf(Value);
            }
            else if (Header(Line, TEXT("DIFFICULTY"), Value))
            {
                Out.Difficulty = FCString::Atof(*Value);
                PageDifficulty = Out.Difficulty;
            }
            else if (Header(Line, TEXT("READING"), Value))
            {
                Out.RequiredReading = FCString::Atof(*Value);
            }
            else if (Header(Line, TEXT("SOURCE"), Value))
            {
                continue;
            }
            else if (Header(Line, TEXT("PAGE"), Value))
            {
                Flush();
                PageTitle = Value;
            }
            else
            {
                PageText += Line + TEXT(" ");
            }
        }
        Flush();
        return Out.Subject != NAME_None && Out.Pages.Num() > 0;
    }
}

void AddBooksFromFiles(TArray<FTextbook>& Books)
{
    const FString Folder = FPaths::ProjectDir() / TEXT("Books");
    TArray<FString> Files;
    IFileManager::Get().FindFiles(Files, *(Folder / TEXT("*.txt")), true, false);
    int32 Added = 0;
    int32 Pages = 0;
    for (const FString& File : Files)
    {
        FTextbook Book;
        if (!ReadBook(Folder / File, Book))
        {
            UE_LOG(LogHumanCity, Warning, TEXT("Книга не прочиталась: %s"), *File);
            continue;
        }
        bool bKnown = false;
        for (const FTextbook& Have : Books)
        {
            bKnown |= Have.Subject == Book.Subject;
        }
        if (bKnown)
        {
            continue;
        }
        Pages += Book.Pages.Num();
        ++Added;
        Books.Add(MoveTemp(Book));
    }
    if (Added > 0)
    {
        UE_LOG(LogHumanCity, Display, TEXT("В городе появились настоящие книги: %d, страниц %d"), Added, Pages);
    }
}
