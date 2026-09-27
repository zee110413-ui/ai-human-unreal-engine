#include "TongueSubsystem.h"
#include "HumanTypes.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"

namespace
{
    TCHAR Lower(TCHAR C)
    {
        if (C >= 0x0410 && C <= 0x042F)
        {
            return TCHAR(C + 0x20);
        }
        if (C == 0x0401)
        {
            return TCHAR(0x0451);
        }
        if (C >= 'A' && C <= 'Z')
        {
            return TCHAR(C + 32);
        }
        return C;
    }

    bool IsLetter(TCHAR C)
    {
        return (C >= 0x0430 && C <= 0x044F) || C == 0x0451 || (C >= 'a' && C <= 'z') || C == '-';
    }

    FString Clean(const FString& Raw)
    {
        FString Word;
        for (const TCHAR C : Raw)
        {
            const TCHAR L = Lower(C);
            if (IsLetter(L))
            {
                Word.AppendChar(L == 0x0451 ? TCHAR(0x0435) : L);
            }
        }
        return Word;
    }

    struct FLetterNode
    {
        TMap<TCHAR, int32> Next;
        bool bEnd = false;
    };

    FString Branches(const TArray<FLetterNode>& Nodes, int32 At, TArray<FString>& Rules)
    {
        TArray<TCHAR> Keys;
        Nodes[At].Next.GetKeys(Keys);
        Keys.Sort();
        TArray<FString> Choices;
        for (const TCHAR Key : Keys)
        {
            FString Run;
            Run.AppendChar(Key);
            int32 Node = Nodes[At].Next[Key];
            while (!Nodes[Node].bEnd && Nodes[Node].Next.Num() == 1)
            {
                const TPair<TCHAR, int32>& Only = *Nodes[Node].Next.CreateConstIterator();
                Run.AppendChar(Only.Key);
                Node = Only.Value;
            }
            if (Nodes[Node].Next.Num() == 0)
            {
                Choices.Add(TEXT("\"") + Run + TEXT("\""));
                continue;
            }
            const FString Body = Branches(Nodes, Node, Rules);
            const FString Name = FString::Printf(TEXT("t%d"), Rules.Num());
            Rules.Add(Name + TEXT(" ::= ") + Body);
            Choices.Add(TEXT("\"") + Run + TEXT("\" ") + Name);
        }
        const FString Body = FString::Join(Choices, TEXT(" | "));
        return Nodes[At].bEnd ? TEXT("(") + Body + TEXT(")?") : Body;
    }

    FString Fitting(const FString& Meaning, const TArray<FString>& Words, int32 Limit)
    {
        TArray<FString> Parts;
        Meaning.ParseIntoArray(Parts, TEXT(" "), true);
        TArray<FString> Stems;
        for (const FString& Part : Parts)
        {
            const FString Word = Clean(Part);
            if (Word.Len() >= 5)
            {
                Stems.Add(Word.LeftChop(2));
            }
            else if (Word.Len() == 4)
            {
                Stems.Add(Word.LeftChop(1));
            }
            else if (!Word.IsEmpty())
            {
                Stems.Add(Word);
            }
        }
        TArray<FString> Picked;
        TSet<FString> Seen;
        for (const FString& Raw : Words)
        {
            const FString Word = Clean(Raw);
            for (const FString& Stem : Stems)
            {
                if (Word == Stem || (Stem.Len() >= 3 && Word.StartsWith(Stem)))
                {
                    if (!Seen.Contains(Word))
                    {
                        Seen.Add(Word);
                        Picked.Add(Word);
                    }
                    break;
                }
            }
        }
        for (const FString& Raw : Words)
        {
            if (Picked.Num() >= Limit)
            {
                break;
            }
            const FString Word = Clean(Raw);
            if (!Word.IsEmpty() && !Seen.Contains(Word))
            {
                Seen.Add(Word);
                Picked.Add(Word);
            }
        }
        return FString::Join(Picked, TEXT(", "));
    }

    struct FAttempts
    {
        int32 Left = 0;
        double Best = -1.0e9;
        FString Said;
        TFunction<void(const FString&)> Done;
    };

    double Naturalness(const FJsonObject& Answer)
    {
        const TArray<TSharedPtr<FJsonValue>>* Steps = nullptr;
        if (!Answer.TryGetArrayField(TEXT("completion_probabilities"), Steps) || !Steps || Steps->Num() == 0)
        {
            return -50.0;
        }
        double Sum = 0.0;
        double Worst = 0.0;
        int32 Count = 0;
        for (const TSharedPtr<FJsonValue>& Step : *Steps)
        {
            const TSharedPtr<FJsonObject>* Item = nullptr;
            if (!Step.IsValid() || !Step->TryGetObject(Item) || !Item)
            {
                continue;
            }
            double LogProb = 0.0;
            double Prob = 0.0;
            if (!(*Item)->TryGetNumberField(TEXT("logprob"), LogProb))
            {
                LogProb = (*Item)->TryGetNumberField(TEXT("prob"), Prob) ? FMath::Loge(FMath::Max(Prob, 1.0e-9)) : -10.0;
            }
            Sum += LogProb;
            Worst = FMath::Min(Worst, LogProb);
            ++Count;
        }
        return Count > 0 ? Sum / Count + 0.3 * Worst : -50.0;
    }
}

TCHAR UTongueSubsystem::Upper(TCHAR C)
{
    if (C >= 0x0430 && C <= 0x044F)
    {
        return TCHAR(C - 0x20);
    }
    if (C == 0x0451)
    {
        return TCHAR(0x0401);
    }
    if (C >= 'a' && C <= 'z')
    {
        return TCHAR(C - 32);
    }
    return C;
}

void UTongueSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    if (IsRunningCommandlet() || FParse::Param(FCommandLine::Get(), TEXT("NoTongue")))
    {
        return;
    }
    const FString Exe = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Tools") / TEXT("llama") / TEXT("llama-server.exe"));
    const FString Model = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Models") / TEXT("Qwen3-8B-Q4_K_M.gguf"));
    if (!FPaths::FileExists(Exe) || !FPaths::FileExists(Model))
    {
        UE_LOG(LogHumanCity, Display, TEXT("TONGUE нет модели или сервера: жители говорят без языковой модели"));
        return;
    }
    const int32 Port = 8089;
    Address = FString::Printf(TEXT("http://127.0.0.1:%d"), Port);
    const FString Journal = FPaths::ConvertRelativePathToFull(FPaths::ProjectLogDir() / TEXT("tongue.log"));
    const FString Args = FString::Printf(TEXT("-m \"%s\" --host 127.0.0.1 --port %d -ngl 99 -c 8192 -np 4 --no-webui --log-file \"%s\""),
        *Model, Port, *Journal);
    Server = FPlatformProcess::CreateProc(*Exe, *Args, false, true, true, nullptr, 0, *FPaths::GetPath(Exe), nullptr);
    bStarted = true;
    StartedAt = FPlatformTime::Seconds();
    NextProbe = StartedAt + 2.0;
    Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UTongueSubsystem::Beat), 0.5f);
    UE_LOG(LogHumanCity, Display, TEXT("TONGUE запускаю языковую модель: %s"), *Model);
}

void UTongueSubsystem::Deinitialize()
{
    if (Ticker.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(Ticker);
        Ticker.Reset();
    }
    if (Server.IsValid())
    {
        FPlatformProcess::TerminateProc(Server, true);
        FPlatformProcess::CloseProc(Server);
    }
    bReady = false;
    Super::Deinitialize();
}

bool UTongueSubsystem::Beat(float DeltaTime)
{
    if (!bStarted || bReady || bProbing)
    {
        return !bReady;
    }
    const double Clock = FPlatformTime::Seconds();
    if (Clock < NextProbe)
    {
        return true;
    }
    if (Clock - StartedAt > 240.0)
    {
        UE_LOG(LogHumanCity, Warning, TEXT("TONGUE модель так и не поднялась"));
        return false;
    }
    bProbing = true;
    NextProbe = Clock + 1.5;
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(Address + TEXT("/health"));
    Request->SetVerb(TEXT("GET"));
    Request->SetTimeout(3.0f);
    TWeakObjectPtr<UTongueSubsystem> Self(this);
    Request->OnProcessRequestComplete().BindLambda([Self](FHttpRequestPtr, FHttpResponsePtr Response, bool bOk)
    {
        if (!Self.IsValid())
        {
            return;
        }
        if (!bOk || !Response.IsValid() || Response->GetResponseCode() != 200)
        {
            Self->bProbing = false;
            return;
        }
        Self->Warm();
    });
    Request->ProcessRequest();
    return true;
}

void UTongueSubsystem::Warm()
{
    TArray<FString> Words = { TEXT("день"), TEXT("добрый"), TEXT("здравствуй"), TEXT("пора"), TEXT("домой") };
    TWeakObjectPtr<UTongueSubsystem> Self(this);
    Send(TEXT("Здравствуй."), FString(), FString(), Words, 1, [Self](const FString& Said)
    {
        if (!Self.IsValid())
        {
            return;
        }
        Self->bProbing = false;
        Self->bReady = true;
        UE_LOG(LogHumanCity, Display, TEXT("TONGUE язык готов (%.0f с), проба: «%s»"), FPlatformTime::Seconds() - Self->StartedAt, *Said);
    }, 60.0f);
}

FString UTongueSubsystem::Grammar(const TArray<FString>& Words)
{
    TArray<FLetterNode> Nodes;
    Nodes.AddDefaulted();
    int32 Count = 0;
    for (const FString& Raw : Words)
    {
        const FString Word = Clean(Raw);
        if (Word.IsEmpty() || Word.Len() > 24 || Word.StartsWith(TEXT("-")) || Word.EndsWith(TEXT("-")))
        {
            continue;
        }
        int32 At = 0;
        for (const TCHAR C : Word)
        {
            const int32* Found = Nodes[At].Next.Find(C);
            if (Found)
            {
                At = *Found;
                continue;
            }
            const int32 Made = Nodes.AddDefaulted();
            Nodes[At].Next.Add(C, Made);
            At = Made;
        }
        if (!Nodes[At].bEnd)
        {
            Nodes[At].bEnd = true;
            if (++Count >= 3000)
            {
                break;
            }
        }
    }
    if (Count < 3)
    {
        return FString();
    }
    TArray<FString> Rules;
    const FString Word = Branches(Nodes, 0, Rules);
    FString Text = TEXT("root ::= w ((\", \" | \" \") w){0,11} [.!?]\nw ::= ") + Word + TEXT("\n");
    for (const FString& Rule : Rules)
    {
        Text += Rule;
        Text += TEXT("\n");
    }
    return Text;
}

void UTongueSubsystem::Phrase(const FString& Meaning, const FString& Speaker, const FString& Listener, const TArray<FString>& Words,
    TFunction<void(const FString&)> Done)
{
    if (++Asked <= 6)
    {
        UE_LOG(LogHumanCity, Display, TEXT("TONGUE просят сказать «%s», слов у говорящего %d, готов %d, в очереди %d"),
            *Meaning, Words.Num(), bReady ? 1 : 0, Pending);
    }
    if (!bReady || Pending >= 4)
    {
        Done(FString());
        return;
    }
    Send(Meaning, Speaker, Listener, Words, Pending < 2 ? 2 : 1, MoveTemp(Done), 30.0f);
}

void UTongueSubsystem::Send(const FString& Meaning, const FString& Speaker, const FString& Listener, const TArray<FString>& Words,
    int32 Tries, TFunction<void(const FString&)> Done, float Patience)
{
    const FString Rules = Grammar(Words);
    if (Rules.IsEmpty())
    {
        Done(FString());
        return;
    }
    const FString Who = Speaker.IsEmpty() ? FString() : FString::Printf(TEXT("Тебя зовут %s. "), *Speaker);
    const FString Whom = Listener.IsEmpty() ? FString(TEXT("Перед тобой человек, имени которого ты не знаешь. "))
                                            : FString::Printf(TEXT("Перед тобой %s. "), *Listener);
    const FString Prompt = FString::Printf(
        TEXT("<|im_start|>system\nТы простой житель старой русской деревни. Ты знаешь только те слова, что прочёл в книгах, и говоришь только ими: одной короткой фразой, просто и по-живому, от первого лица. Если нужного слова не знаешь, скажи иначе.<|im_end|>\n")
        TEXT("<|im_start|>user\n%s%sТы хочешь сказать вот что: «%s». Из твоих слов сюда годятся: %s. Скажи это своими словами.<|im_end|>\n<|im_start|>assistant\n<think>\n\n</think>\n\n"),
        *Who, *Whom, *Meaning, *Fitting(Meaning, Words, 110));

    TSharedRef<FAttempts> Attempts = MakeShared<FAttempts>();
    Attempts->Left = Tries;
    Attempts->Done = MoveTemp(Done);
    TWeakObjectPtr<UTongueSubsystem> Self(this);
    for (int32 Try = 0; Try < Tries; ++Try)
    {
        TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
        Body->SetStringField(TEXT("prompt"), Prompt);
        Body->SetStringField(TEXT("grammar"), Rules);
        Body->SetNumberField(TEXT("n_predict"), 64);
        Body->SetNumberField(TEXT("temperature"), 0.6 + 0.2 * Try);
        Body->SetNumberField(TEXT("top_p"), 0.9);
        Body->SetNumberField(TEXT("repeat_penalty"), 1.1);
        Body->SetNumberField(TEXT("n_probs"), 1);
        Body->SetNumberField(TEXT("seed"), FMath::Rand());
        Body->SetBoolField(TEXT("cache_prompt"), true);
        FString Json;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
        FJsonSerializer::Serialize(Body, Writer);

        TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
        Request->SetURL(Address + TEXT("/completion"));
        Request->SetVerb(TEXT("POST"));
        Request->SetHeader(TEXT("Content-Type"), TEXT("application/json; charset=utf-8"));
        Request->SetContentAsString(Json);
        Request->SetTimeout(Patience);
        ++Pending;
        Request->OnProcessRequestComplete().BindLambda([Self, Attempts](FHttpRequestPtr, FHttpResponsePtr Response, bool bOk)
        {
            if (Self.IsValid())
            {
                Self->Pending = FMath::Max(0, Self->Pending - 1);
            }
            if (bOk && Response.IsValid() && Response->GetResponseCode() == 200)
            {
                TSharedPtr<FJsonObject> Answer;
                const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
                if (FJsonSerializer::Deserialize(Reader, Answer) && Answer.IsValid())
                {
                    const FString Said = Answer->GetStringField(TEXT("content")).TrimStartAndEnd();
                    const double Score = Naturalness(*Answer);
                    if (!Said.IsEmpty() && Score > Attempts->Best)
                    {
                        Attempts->Best = Score;
                        Attempts->Said = Said;
                    }
                }
            }
            else if (Self.IsValid() && Self->Failures++ < 20)
            {
                UE_LOG(LogHumanCity, Warning, TEXT("TONGUE не ответил: связь %s, код %d, %s"),
                    bOk ? TEXT("есть") : TEXT("нет"), Response.IsValid() ? Response->GetResponseCode() : 0,
                    Response.IsValid() ? *Response->GetContentAsString().Left(300) : TEXT("пусто"));
            }
            if (--Attempts->Left > 0)
            {
                return;
            }
            FString Said = Attempts->Said;
            if (!Said.IsEmpty())
            {
                Said[0] = UTongueSubsystem::Upper(Said[0]);
                const TCHAR Last = Said[Said.Len() - 1];
                if (Last != '.' && Last != '!' && Last != '?')
                {
                    Said.AppendChar('.');
                }
            }
            Attempts->Done(Said);
        });
        Request->ProcessRequest();
    }
}
