// Textbook.cpp
//
// Содержание книг. Написано так, как пишут в учебниках: коротко, по одному
// правилу на страницу. Это не украшение — человек читает именно эти строки
// и запоминает именно их, а потом повторяет своими словами.

#include "Textbook.h"
#include "Crafts.h"

namespace
{
    FTextbookPage P(const TCHAR* Text, float Difficulty)
    {
        FTextbookPage Page;
        Page.Text = Text;
        Page.Difficulty = Difficulty;

        // Заголовок для коротких статей — первые слова: так человек называет
        // прочитанное, когда пересказывает его другому.
        TArray<FString> Words;
        Page.Text.ParseIntoArrayWS(Words);
        for (int32 i = 0; i < FMath::Min(4, Words.Num()); ++i)
        {
            Page.Title += (i > 0 ? TEXT(" ") : TEXT("")) + Words[i];
        }
        Page.Title += TEXT("...");
        return Page;
    }

    FTextbook MakeBook(const TCHAR* Subject, const TCHAR* Title, const TCHAR* Skill,
                       const TCHAR* Second, EWordTopic Topic, float Difficulty,
                       float RequiredReading, TArray<FTextbookPage> Pages)
    {
        FTextbook Book;
        Book.Subject = Subject;
        Book.Title = Title;
        Book.Skill = Skill;
        Book.SecondSkill = Second;
        Book.Topic = Topic;
        Book.Difficulty = Difficulty;
        Book.RequiredReading = RequiredReading;
        Book.Pages = MoveTemp(Pages);
        return Book;
    }
}

/** Школьные учебники лежат отдельно — их много. */
extern void AddRussianBooks(TArray<FTextbook>& Books);
extern void AddMathBooks(TArray<FTextbook>& Books);
extern void AddReaderBooks(TArray<FTextbook>& Books);
extern void AddCraftBooks(TArray<FTextbook>& Books);
extern void AddVillageBook(TArray<FTextbook>& Books);
extern void AddVillageCraftBooks(TArray<FTextbook>& Books);
extern void AddScienceBooks(TArray<FTextbook>& Books);
extern void AddBooksFromFiles(TArray<FTextbook>& Books);

const TArray<FTextbook>& FLibrary::All()
{
    static TArray<FTextbook> Books;
    if (Books.Num() > 0)
    {
        return Books;
    }

    AddReaderBooks(Books);
    AddRussianBooks(Books);
    AddMathBooks(Books);

    auto Picture = [](const TCHAR* Letter, const TCHAR* Text, std::initializer_list<const TCHAR*> Shown)
    {
        FTextbookPage Page;
        Page.Title = Letter;
        Page.Text = Text;
        Page.Difficulty = 0.05f;
        for (const TCHAR* Thing : Shown)
        {
            Page.Pictures.Add(Thing);
        }
        return Page;
    };

    Books.Add(MakeBook(TEXT("Alphabet"), TEXT("азбука"), TEXT("Reading"), TEXT("Writing"),
        EWordTopic::Child, 0.05f, 0.0f, {
        Picture(TEXT("А а"), TEXT("А а. Вода. Лампа. Парта."), { TEXT("вода"), TEXT("лампа"), TEXT("парта") }),
        Picture(TEXT("Б б"), TEXT("Б б. Бочка. Баня. Библиотека."), { TEXT("бочка"), TEXT("баня"), TEXT("библиотека") }),
        Picture(TEXT("В в"), TEXT("В в. Вода. Диван. Ванна. Кровать."), { TEXT("вода"), TEXT("диван"), TEXT("ванна"), TEXT("кровать") }),
        Picture(TEXT("Г г"), TEXT("Г г. Грядка. Глобус."), { TEXT("грядка"), TEXT("глобус") }),
        Picture(TEXT("Д д"), TEXT("Д д. Дом. Диван. Доска. Дерево."), { TEXT("дом"), TEXT("диван"), TEXT("доска"), TEXT("дерево") }),
        Picture(TEXT("Е е"), TEXT("Е е. Ё ё. Еда. Небо. Человек. Ковёр."), { TEXT("еда"), TEXT("небо"), TEXT("человек"), TEXT("ковёр") }),
        Picture(TEXT("Ж ж"), TEXT("Ж ж. Жажда. Вода."), { TEXT("жажда"), TEXT("вода") }),
        Picture(TEXT("З з"), TEXT("З з. Зеркало."), { TEXT("зеркало") }),
        Picture(TEXT("И и"), TEXT("И и. Книга. Пианино. Растение."), { TEXT("книга"), TEXT("пианино"), TEXT("растение") }),
        Picture(TEXT("Й й"), TEXT("Й й. Чай."), { TEXT("чай") }),
        Picture(TEXT("К к"), TEXT("К к. Книга. Ковёр. Кровать. Кафе."), { TEXT("книга"), TEXT("ковёр"), TEXT("кровать"), TEXT("кафе") }),
        Picture(TEXT("Л л"), TEXT("Л л. Лампа. Стол. Стул. Глобус."), { TEXT("лампа"), TEXT("стол"), TEXT("стул"), TEXT("глобус") }),
        Picture(TEXT("М м"), TEXT("М м. Дом. Мыло. Лампа."), { TEXT("дом"), TEXT("мыло"), TEXT("лампа") }),
        Picture(TEXT("Н н"), TEXT("Н н. Небо. Солнце. Ванна."), { TEXT("небо"), TEXT("солнце"), TEXT("ванна") }),
        Picture(TEXT("О о"), TEXT("О о. Дом. Стол. Вода."), { TEXT("дом"), TEXT("стол"), TEXT("вода") }),
        Picture(TEXT("П п"), TEXT("П п. Плита. Парта. Пианино."), { TEXT("плита"), TEXT("парта"), TEXT("пианино") }),
        Picture(TEXT("Р р"), TEXT("Р р. Рынок. Трава. Дерево."), { TEXT("рынок"), TEXT("трава"), TEXT("дерево") }),
        Picture(TEXT("С с"), TEXT("С с. Стол. Стул. Солнце. Сквер."), { TEXT("стол"), TEXT("стул"), TEXT("солнце"), TEXT("сквер") }),
        Picture(TEXT("Т т"), TEXT("Т т. Трава. Тумбочка. Стол."), { TEXT("трава"), TEXT("тумбочка"), TEXT("стол") }),
        Picture(TEXT("У у"), TEXT("У у. Улица. Стул."), { TEXT("улица"), TEXT("стул") }),
        Picture(TEXT("Ф ф"), TEXT("Ф ф. Кафе. Шкаф."), { TEXT("кафе"), TEXT("шкаф") }),
        Picture(TEXT("Х х"), TEXT("Х х. Холодильник."), { TEXT("холодильник") }),
        Picture(TEXT("Ц ц"), TEXT("Ц ц. Улица. Солнце."), { TEXT("улица"), TEXT("солнце") }),
        Picture(TEXT("Ч ч"), TEXT("Ч ч. Чай. Человек. Бочка."), { TEXT("чай"), TEXT("человек"), TEXT("бочка") }),
        Picture(TEXT("Ш ш"), TEXT("Ш ш. Шкаф. Школа."), { TEXT("шкаф"), TEXT("школа") }),
        Picture(TEXT("Щ щ"), TEXT("Щ щ. Овощи."), { TEXT("овощи") }),
        Picture(TEXT("Ъ ъ"), TEXT("Ъ ъ. Подъезд."), { TEXT("подъезд") }),
        Picture(TEXT("Ы ы"), TEXT("Ы ы. Мыло. Рынок. Счёты."), { TEXT("мыло"), TEXT("рынок"), TEXT("счёты") }),
        Picture(TEXT("Ь ь"), TEXT("Ь ь. Кровать. Дверь."), { TEXT("кровать"), TEXT("дверь") }),
        Picture(TEXT("Э э"), TEXT("Э э. Экран."), { TEXT("экран") }),
        Picture(TEXT("Ю ю"), TEXT("Ю ю. Компьютер."), { TEXT("компьютер") }),
        Picture(TEXT("Я я"), TEXT("Я я. Баня. Грядка. Пекарня."), { TEXT("баня"), TEXT("грядка"), TEXT("пекарня") }),
        P(TEXT("Буква — это знак для звука. Звуков немного, а слов из них выходит без счёта."), 0.1f),
        P(TEXT("А, Б, В, Г, Д — так начинается ряд букв. Его помнят наизусть."), 0.1f),
        P(TEXT("Из двух букв выходит слог: ма, па, ба. Из двух слогов — слово: ма-ма, па-па."), 0.15f),
        P(TEXT("Слова стоят в ряд и получается речь. Между словами оставляют промежуток."), 0.2f),
        P(TEXT("В конце мысли ставят точку. Если спрашивают — знак вопроса."), 0.2f),
        P(TEXT("Кто знает буквы, тот может узнать то, чего сам не видел."), 0.25f)
    }));

    // --- Счёт ---------------------------------------------------------------
    Books.Add(MakeBook(TEXT("Counting"), TEXT("счёт"), TEXT("Counting"), TEXT("Arithmetic"),
        EWordTopic::Thought, 0.2f, 0.1f, {
        P(TEXT("Один, два, три, четыре, пять, шесть, семь, восемь, девять, десять."), 0.1f),
        P(TEXT("Считать — значит ставить вещам числа по порядку, не пропуская и не повторяя."), 0.2f),
        P(TEXT("Сколько бы раз ни пересчитал одно и то же — выйдет столько же. Это и есть число."), 0.35f),
        P(TEXT("Два да три — пять. Прибавить значит взять сперва одно, потом другое, и сосчитать всё вместе."), 0.25f),
        P(TEXT("Пять без двух — три. Отнять значит убрать и сосчитать оставшееся."), 0.3f),
        P(TEXT("Десять десятков — сто. Десять сотен — тысяча."), 0.35f),
        P(TEXT("Числа не кончаются: к любому можно прибавить один."), 0.5f)
    }));

    // --- Таблица счёта ------------------------------------------------------
    Books.Add(MakeBook(TEXT("Arithmetic"), TEXT("таблица счёта"), TEXT("Arithmetic"), TEXT("Counting"),
        EWordTopic::Thought, 0.45f, 0.3f, {
        P(TEXT("Умножить — значит сложить одно и то же число несколько раз. Трижды четыре — это четыре да четыре да четыре."), 0.4f),
        P(TEXT("Дважды два — четыре. Дважды три — шесть. Дважды четыре — восемь. Дважды пять — десять."), 0.35f),
        P(TEXT("Трижды три — девять. Трижды четыре — двенадцать. Трижды пять — пятнадцать."), 0.4f),
        P(TEXT("Пятью пять — двадцать пять. Шестью шесть — тридцать шесть. Семью семь — сорок девять."), 0.45f),
        P(TEXT("Восемью восемь — шестьдесят четыре. Девятью девять — восемьдесят один."), 0.5f),
        P(TEXT("От перемены мест сомножителей произведение не меняется: трижды четыре и четырежды три — одно и то же."), 0.55f),
        P(TEXT("Разделить — значит узнать, сколько раз одно помещается в другом."), 0.5f)
    }));

    // --- Письмо -------------------------------------------------------------
    Books.Add(MakeBook(TEXT("Writing"), TEXT("письмо"), TEXT("Writing"), TEXT("Reading"),
        EWordTopic::Speech, 0.4f, 0.35f, {
        P(TEXT("Сказанное пропадает, записанное остаётся. В этом вся польза письма."), 0.3f),
        P(TEXT("Пишут слева направо, сверху вниз, ровными строками."), 0.25f),
        P(TEXT("Имя человека пишут с большой буквы. И начало каждой мысли — тоже."), 0.3f),
        P(TEXT("Записанное можно перечитать через год и узнать то, что сам давно забыл."), 0.4f),
        P(TEXT("Кто ведёт записи, тот не путается в счёте и не спорит о том, что было обещано."), 0.45f)
    }));

    // --- Природа ------------------------------------------------------------
    Books.Add(MakeBook(TEXT("Nature"), TEXT("книга о природе"), TEXT("Medicine"), TEXT("Reading"),
        EWordTopic::Nature, 0.35f, 0.25f, {
        P(TEXT("Дерево растёт корнем вниз, ветвями вверх. Корнем оно пьёт, листом дышит."), 0.25f),
        P(TEXT("Вода течёт сверху вниз и собирается в низинах. Оттого река всегда в ложбине."), 0.3f),
        P(TEXT("Перед дождём воздух тяжелеет и птицы летают низко."), 0.35f),
        P(TEXT("Зимой холодно оттого, что солнце стоит низко и светит вкось."), 0.5f),
        P(TEXT("Кипячёная вода не в пример безопаснее сырой. От сырой бывает животом маяться."), 0.4f),
        P(TEXT("Рана заживает чище, если её промыть. Грязь в ране — вот отчего бывает жар."), 0.45f),
        P(TEXT("Всякое живое требует воды чаще, чем еды. Без питья человек слабеет за день."), 0.3f)
    }));

    // --- Врачевание ---------------------------------------------------------
    Books.Add(MakeBook(TEXT("Medicine"), TEXT("врачевание"), TEXT("Medicine"), TEXT("Empathy"),
        EWordTopic::Body, 0.55f, 0.4f, {
        P(TEXT("Жар — не болезнь, а её признак. Тело греется, когда борется."), 0.45f),
        P(TEXT("Больному нужны покой, питьё и тепло. Этим одним спасают чаще, чем травами."), 0.4f),
        P(TEXT("Кто не спит, тот болеет чаще. Сон лечит незаметно, но вернее многого."), 0.35f),
        P(TEXT("Руки моют перед тем, как трогать рану. Иначе своими же руками занесёшь беду."), 0.4f),
        P(TEXT("Боль указывает, где неладно. Заглушить боль — не то же, что вылечить."), 0.5f),
        P(TEXT("Старому человеку всё даётся тяжелее: он и устаёт скорее, и заживает дольше."), 0.45f)
    }));

    // --- Ремесло ------------------------------------------------------------
    Books.Add(MakeBook(TEXT("Craft"), TEXT("ремесло"), TEXT("Craft"), TEXT("Repair"),
        EWordTopic::Work, 0.4f, 0.3f, {
        P(TEXT("Всякая работа начинается с меры. Отмерь дважды, отрежь однажды."), 0.3f),
        P(TEXT("Дерево пилят вдоль волокна легче, чем поперёк."), 0.35f),
        P(TEXT("Тупым работать тяжелее и опаснее, чем острым: тупое соскальзывает."), 0.35f),
        P(TEXT("Вещь держится не на гвозде, а на том, как пригнаны части."), 0.45f),
        P(TEXT("Сделанное на совесть переживает того, кто сделал."), 0.4f)
    }));

    // --- Домоводство --------------------------------------------------------
    Books.Add(MakeBook(TEXT("Cooking"), TEXT("домоводство"), TEXT("Cooking"), TEXT("Cleaning"),
        EWordTopic::Food, 0.25f, 0.15f, {
        P(TEXT("Крупу перед варкой промывают, иначе каша выйдет с песком."), 0.2f),
        P(TEXT("Мясо варят долго и на малом огне: на большом оно снаружи готово, а внутри сыро."), 0.3f),
        P(TEXT("Соль кладут в конце. Положишь в начале — не разберёшь, сколько вышло."), 0.25f),
        P(TEXT("Что осталось с вечера, на другой день едят с осторожностью."), 0.3f),
        P(TEXT("Сытная еда — не та, которой много, а та, после которой долго не хочется есть."), 0.35f)
    }));

    // --- Торговля -----------------------------------------------------------
    Books.Add(MakeBook(TEXT("Trade"), TEXT("торговое дело"), TEXT("Trade"), TEXT("Arithmetic"),
        EWordTopic::Money, 0.5f, 0.35f, {
        P(TEXT("Цена — это не то, чего вещь стоит, а то, за сколько на неё находится охотник."), 0.5f),
        P(TEXT("Кто считает выручку, но не считает расходов, тот разоряется незаметно."), 0.45f),
        P(TEXT("Запас держат на чёрный день, а не на всякий случай: всякий случай съест любой запас."), 0.5f),
        P(TEXT("Уговор дороже денег: с кем однажды поступили нечестно, тот не вернётся."), 0.4f),
        P(TEXT("Дважды продать одну вещь нельзя, а доброе имя продаётся только раз."), 0.55f)
    }));

    // --- Музыка -------------------------------------------------------------
    Books.Add(MakeBook(TEXT("Music"), TEXT("музыкальная грамота"), TEXT("Music"), TEXT("Story"),
        EWordTopic::Feeling, 0.5f, 0.3f, {
        P(TEXT("Звуки бывают выше и ниже. Ряд звуков по высоте называют строем."), 0.4f),
        P(TEXT("Долгие и короткие звуки, поставленные в порядок, дают ход — его слышно как шаг."), 0.5f),
        P(TEXT("Два звука вместе звучат согласно или несогласно. Это слышит всякий, и учить тут нечему."), 0.45f),
        P(TEXT("Одна и та же песня в разном ходе то весела, то печальна."), 0.5f)
    }));

    // --- Рассказ ------------------------------------------------------------
    Books.Add(MakeBook(TEXT("Story"), TEXT("книга рассказов"), TEXT("Story"), TEXT("Conversation"),
        EWordTopic::Speech, 0.3f, 0.25f, {
        P(TEXT("Во всяком рассказе есть тот, с кем случилось, и то, что случилось."), 0.25f),
        P(TEXT("Рассказ держится не на словах, а на том, что было поставлено на кон."), 0.45f),
        P(TEXT("Кто рассказывает всё подряд, того перестают слушать на середине."), 0.3f),
        P(TEXT("Слушают не того, кто громче, а того, кто говорит о том, что задевает."), 0.4f)
    }));

    // --- Обучение -----------------------------------------------------------
    Books.Add(MakeBook(TEXT("Teaching"), TEXT("как учить"), TEXT("Teaching"), TEXT("Empathy"),
        EWordTopic::Thought, 0.55f, 0.4f, {
        P(TEXT("Объясняют не так, как знают сами, а так, как поймёт тот, кто слушает."), 0.5f),
        P(TEXT("Кто не умеет спрашивать, тот не научится. Вопрос — половина знания."), 0.45f),
        P(TEXT("Повторение — не пустая трата времени: без него знание выветривается за месяц."), 0.35f),
        P(TEXT("Ученик, которому дали ошибиться и подумать, помнит дольше того, кому сказали ответ."), 0.55f)
    }));

    // --- Землеописание ------------------------------------------------------
    Books.Add(MakeBook(TEXT("Geography"), TEXT("землеописание"), TEXT("Reading"), TEXT("Counting"),
        EWordTopic::City, 0.4f, 0.3f, {
        P(TEXT("Солнце встаёт с одной стороны и садится с противоположной. По ним и держат путь."), 0.35f),
        P(TEXT("Город стоит на улицах, улицы сходятся на площадях. Заблудиться можно только между домами."), 0.3f),
        P(TEXT("Кто запомнил приметное здание, тот найдёт дорогу и в темноте."), 0.35f),
        P(TEXT("За городом есть другие города, и живут там такие же люди."), 0.4f)
    }));

    AddCraftBooks(Books);
    AddVillageBook(Books);
    AddScienceBooks(Books);
    AddBooksFromFiles(Books);
    AddVillageCraftBooks(Books);
    FCraftBook::BuildFrom(Books);

    return Books;
}

const FTextbook* FLibrary::Find(FName Subject)
{
    for (const FTextbook& Book : All())
    {
        if (Book.Subject == Subject)
        {
            return &Book;
        }
    }
    return nullptr;
}

int32 FLibrary::WordCount(const FTextbookPage& Page)
{
    TArray<FString> Words;
    Page.Text.ParseIntoArrayWS(Words);
    return FMath::Max(1, Words.Num());
}

int32 FLibrary::TotalWords(const FTextbook& Book)
{
    int32 Sum = 0;
    for (const FTextbookPage& Page : Book.Pages)
    {
        Sum += WordCount(Page);
    }
    return Sum;
}

FString FLibrary::Excerpt(const FTextbookPage& Page, int32 FromWord, int32 HowMany)
{
    TArray<FString> Words;
    Page.Text.ParseIntoArrayWS(Words);

    const int32 Start = FMath::Clamp(FromWord, 0, Words.Num());
    const int32 End = FMath::Clamp(Start + HowMany, Start, Words.Num());

    FString Out;
    for (int32 i = Start; i < End; ++i)
    {
        if (!Out.IsEmpty())
        {
            Out += TEXT(" ");
        }
        Out += Words[i];
    }
    return Out;
}

const FTextbook* FLibrary::NextFor(const FString& Course, int32 FinishedGrade)
{
    const FTextbook* Best = nullptr;
    for (const FTextbook& Book : All())
    {
        if (Book.Course != Course || Book.Grade <= FinishedGrade)
        {
            continue;
        }
        if (!Best || Book.Grade < Best->Grade)
        {
            Best = &Book;
        }
    }
    return Best;
}

void FLibrary::ForRoom(FName RoomKind, TArray<const FTextbook*>& Out)
{
    auto Take = [&Out](const TCHAR* Subject)
    {
        if (const FTextbook* Book = Find(Subject))
        {
            Out.Add(Book);
        }
    };

    auto TakeCourse = [&Out](const TCHAR* Course)
    {
        // Весь курс по порядку классов: на полке стоят все учебники,
        // а какой человеку по силам — он выяснит, открыв его.
        for (int32 Grade = 1; Grade <= 11; ++Grade)
        {
            for (const FTextbook& Book : All())
            {
                if (Book.Course == Course && Book.Grade == Grade)
                {
                    Out.Add(&Book);
                }
            }
        }
    };

    auto TakeAny = [&Out](const TCHAR* Course)
    {
        for (const FTextbook& Book : All())
        {
            if (Book.Course == Course)
            {
                Out.Add(&Book);
            }
        }
    };

    if (RoomKind == TEXT("Language"))
    {
        Take(TEXT("Reader"));
        Take(TEXT("Alphabet"));
        TakeCourse(TEXT("русский язык"));
        TakeAny(TEXT("чтение"));
        Take(TEXT("Story"));
    }
    else if (RoomKind == TEXT("Numbers"))
    {
        Take(TEXT("Counting"));
        TakeCourse(TEXT("математика"));
        Take(TEXT("Trade"));
    }
    else if (RoomKind == TEXT("Nature"))
    {
        Take(TEXT("Nature")); Take(TEXT("Medicine")); Take(TEXT("Geography"));
        TakeAny(TEXT("лечение"));
    }
    else if (RoomKind == TEXT("Craft"))
    {
        Take(TEXT("Craft")); Take(TEXT("Cooking")); Take(TEXT("Music"));
        TakeAny(TEXT("ремесло"));
        TakeAny(TEXT("хозяйство"));
    }
    else
    {
        // Библиотека: всё сразу.
        for (const FTextbook& Book : All())
        {
            Out.Add(&Book);
        }
    }
}
