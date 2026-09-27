#include "MindComponent.h"
#include "CompleteHumanAI.h"
#include "SpeechComponent.h"
#include "IdentityComponent.h"
#include "NeedComponent.h"
#include "Textbook.h"
#include "MemoryComponent.h"
#include "Crafts.h"

// ---------------------------------------------------------------------------
//  PreloadKnowledge
//
//  Человек, которому дали знание сразу, — не всезнающий бог. Он читал все
//  книги, которые есть в городе, и запомнил всё, что в них написано.
//  Теперь он может взять топор и попробовать, может рассказать другому,
//  может ошибиться, если запомнил плохо. Но он ЗНАЕТ, как это делается.
//
//  Так в городе появляется первый мастер: тот, кто прочитал всё.
//  Остальные учатся у него — глядя на его руки и спрашивая.
// ---------------------------------------------------------------------------
void UMindComponent::PreloadKnowledge()
{
    if (!Memory)
    {
        return;
    }

    const float T = Now();
    int32 Learned = 0;

    for (const FTextbook& Book : FLibrary::All())
    {
        // Закладка на конец: этот человек дочитал все книги.
        FReadingBookmark& Mark = BookmarkFor(Book.Subject);
        Mark.Page = 0;
        Mark.Word = 0;
        Mark.Understood = Book.Pages.Num();
        Mark.bFinished = true;

        for (const FTextbookPage& Page : Book.Pages)
        {
            // Усваиваем то, что на странице написано: слова.
            if (Speech)
            {
                Speech->LearnWords(Page.Text, 1.0f, false);
            }

            // И дело, которое описано на странице.
            if (!Page.Craft.IsNone())
            {
                FBelief HowTo;
                HowTo.Subject = Page.Craft;
                HowTo.Predicate = TEXT("HowTo");
                HowTo.Value = 1.0f;
                HowTo.Confidence = FMath::Clamp(1.0f - Page.Difficulty * 0.15f, 0.5f, 1.0f);
                HowTo.LearnedAt = T;
                HowTo.bVerified = true;
                HowTo.Text = FString::Printf(TEXT("%s: %s"), *Book.Title, *Page.Title);
                Memory->Learn(HowTo, 1.0f, 1.0f, T);
                ++Learned;
            }
        }

        // Навык книги тоже прокачивается: кто прочёл всё, тот и умел.
        if (!Book.Skill.IsNone())
        {
            const float Level = FMath::Clamp(0.75f - Book.Difficulty * 0.2f, 0.35f, 0.85f);
            if (Mastery(Book.Skill) < Level)
            {
                FBelief Skill;
                Skill.Subject = Book.Skill;
                Skill.Predicate = TEXT("HowTo");
                Skill.Value = 1.0f;
                Skill.Confidence = Level;
                Skill.LearnedAt = T;
                Skill.bVerified = true;
                Skill.Text = MasteryLabel(Book.Skill);
                Memory->Learn(Skill, 1.0f, 1.0f, T);
            }
        }
    }

    if (ACompleteHumanNPC* Human = GetHuman())
    {
        UE_LOG(LogHumanCity, Warning, TEXT("%s прочёл все книги: %d дел усвоено."),
            *Human->GetName(), Learned);
    }
}

void UMindComponent::TeachReading(AActor* PupilActor, float GameDelta)
{
    ACompleteHumanNPC* Me = GetHuman();
    ACompleteHumanNPC* Pupil = Cast<ACompleteHumanNPC>(PupilActor);
    if (!Me || !Pupil || !Pupil->Mind || !Pupil->SpeechComponent || !Speech || !Pupil->IsAlive())
    {
        return;
    }
    if (Speech->Literacy() < 0.5f)
    {
        return;
    }
    if (FVector::Dist2D(Me->GetActorLocation(), Pupil->GetActorLocation()) > 350.0f)
    {
        return;
    }

    if (!Pupil->Mind->IsTalkingWith(Me) && !Pupil->Mind->bAsleep)
    {
        Pupil->Mind->StayToTalk(Me);
    }

    TeachAccumulator += GameDelta;
    if (TeachAccumulator < 50.0f)
    {
        return;
    }
    TeachAccumulator = 0.0f;

    FName Subject = TEXT("Alphabet");
    if (Pupil->SpeechComponent->Literacy() > 0.9f)
    {
        Subject = TEXT("Reader");
    }
    const FTextbook* Book = FLibrary::Find(Subject);
    if (!Book || Book->Pages.Num() == 0)
    {
        return;
    }

    FReadingBookmark& Mark = Pupil->Mind->BookmarkFor(Subject);
    Mark.Page = FMath::Clamp(Mark.Page, 0, Book->Pages.Num() - 1);
    const FTextbookPage& Page = Book->Pages[Mark.Page];
    const int32 Words = FLibrary::WordCount(Page);
    Mark.Word = FMath::Clamp(Mark.Word, 0, FMath::Max(0, Words - 1));

    const int32 Take = FMath::Clamp(Words - Mark.Word, 1, 12);
    const bool bPageStart = Mark.Word == 0;
    const FString Chunk = FLibrary::Excerpt(Page, Mark.Word, Take);

    TArray<FString> Pictures;
    if (bPageStart)
    {
        Pictures = Page.Pictures;
    }

    Mark.Word += Take;
    if (Mark.Word >= Words)
    {
        Mark.Word = 0;
        ++Mark.Understood;
        ++Mark.Page;
        if (Mark.Page >= Book->Pages.Num())
        {
            Mark.Page = 0;
            Mark.bFinished = true;
        }
    }

    const float T = Now();
    const bool bChild = Pupil->IdentityComponent && Pupil->IdentityComponent->Age < 13.0f;
    const float Skill = Mastery(TEXT("Teaching"));

    const float Attention = FMath::Clamp(0.55f + Skill * 0.35f, 0.1f, 1.0f);

    Pupil->SpeechComponent->LearnLettersFrom(*Speech, Page.Title + TEXT(" ") + Chunk, 0.10f + Skill * 0.12f);
    const int32 Legible = Pupil->SpeechComponent->ReadText(Chunk, Pictures, Page.Title, Attention, bChild);

    if (Legible > 0)
    {
        Pupil->Mind->Practised(TEXT("Reading"), 0.02f + Skill * 0.02f);
    }
    Practised(TEXT("Teaching"), 0.015f);

    Me->ShowSpeech(FString::Printf(TEXT("«%s»"), *Chunk));
    Report(FString::Printf(TEXT("учит грамоте %s: «%s»"), *NameOf(Pupil), *Chunk.Left(80)));

    Pupil->Mind->JudgeByDeeds(Me, 0.02f, TEXT("учит меня грамоте"));
    if (Pupil->Mind->Needs)
    {
        Pupil->Mind->Needs->Satisfy(ENeedType::Competence, 0.03f);
    }
    if (Needs)
    {
        Needs->Satisfy(ENeedType::Meaning, 0.02f);
    }
}

void UMindComponent::TeachProduction(AActor* PupilActor, float GameDelta)
{
    ACompleteHumanNPC* Me = GetHuman();
    ACompleteHumanNPC* Pupil = Cast<ACompleteHumanNPC>(PupilActor);
    if (!Me || !Pupil || !Identity || !Identity->bMasterTeacher || !Pupil->Mind
        || !Pupil->MemoryComponent || !Pupil->IsAlive() || !Pupil->IdentityComponent)
    {
        return;
    }
    if (FVector::Dist2D(Me->GetActorLocation(), Pupil->GetActorLocation()) > 350.0f)
    {
        return;
    }

    TeachAccumulator += GameDelta;
    if (TeachAccumulator < 50.0f)
    {
        return;
    }
    TeachAccumulator = 0.0f;

    const FTextbook* Book = FLibrary::Find(TEXT("VillageProduction"));
    if (!Book)
    {
        return;
    }

    const FTextbookPage* Lesson = nullptr;
    float LowestRecall = 1.1f;
    for (const FTextbookPage& Page : Book->Pages)
    {
        if (Page.Craft.IsNone())
        {
            continue;
        }
        const float Recall = Pupil->Mind->RecallOfDeed(Page.Craft);
        if (Recall < LowestRecall)
        {
            LowestRecall = Recall;
            Lesson = &Page;
        }
    }
    if (!Lesson)
    {
        return;
    }

    const float TeacherRecall = RecallOfDeed(Lesson->Craft);
    if (TeacherRecall < 0.55f)
    {
        return;
    }

    FBelief HowTo;
    HowTo.Subject = Lesson->Craft;
    HowTo.Predicate = TEXT("HowTo");
    HowTo.Value = 1.0f;
    HowTo.Confidence = FMath::Clamp(FMath::Max(0.25f, LowestRecall + 0.16f), 0.0f, FMath::Min(0.85f, TeacherRecall));
    HowTo.LearnedAt = Now();
    HowTo.bVerified = false;
    HowTo.Source = Me;
    HowTo.Text = Lesson->Text;
    Pupil->MemoryComponent->Learn(HowTo, 0.9f, 0.8f, HowTo.LearnedAt);

    const bool bChild = Pupil->IdentityComponent->Age < 13.0f;
    const FString SpokenLesson = Speech
        ? Speech->Articulate(Lesson->Text, nullptr, NameOf(Me), NameOf(Pupil))
        : Lesson->Text;
    Pupil->SpeechComponent->LearnWords(SpokenLesson, 0.75f, bChild);

    if (const FCraft* Deed = FCraftBook::Find(Lesson->Craft))
    {
        Me->ShowSpeech(FString::Printf(TEXT("%s: %s"), *Deed->Label, *SpokenLesson));
        Report(FString::Printf(TEXT("объясняет %s: %s"), *NameOf(Pupil), *Deed->Label));
    }
    Pupil->Mind->Practised(Lesson->Craft, 0.01f);
    Practised(TEXT("Teaching"), 0.01f);
    Pupil->Mind->JudgeByDeeds(Me, 0.03f, TEXT("передал знание ремесла"));
}

void UMindComponent::PolicyUse(const FAffordance& Affordance)
{
    FIntention Intention;
    Intention.bValid = true;
    Intention.Affordance = Affordance;
    Intention.DecidedAt = Now();
    Intention.Reason = Affordance.Label;
    BeginIntention(Intention);
}

void UMindComponent::PolicyStop()
{
    if (!bActionActive)
    {
        return;
    }
    if (ACompleteHumanNPC* Human = GetHuman())
    {
        Human->StopMoving();
    }
    ReleaseOccupied();
    FinishAction();
    bActionActive = false;
    bMoving = false;
    CurrentAction = EActionType::Idle;
    CurrentActionLabel = TEXT("стою");
}

bool UMindComponent::PolicyTalk(AActor* Other)
{
    return StartConversation(Other);
}

void UMindComponent::Reborn()
{
    if (IsInConversation())
    {
        EndConversation();
    }
    ReleaseOccupied();
    FinishAction();
    bActionActive = false;
    bMoving = false;
    bAsleep = false;
    SleepPhase = ESleepPhase::Awake;
    CurrentAction = EActionType::Idle;
    CurrentActionLabel = TEXT("стою");
    ActiveIntention = FIntention();
    Suggestions.Reset();
    PolicyMove = FVector2D::ZeroVector;
}
