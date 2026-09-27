// HumanTypes.h
// ---------------------------------------------------------------------------
// Единый словарь понятий человеческой психики.
// Здесь живут ТОЛЬКО данные (enum'ы и структуры) — никакой логики.
// Все компоненты разума зависят от этого файла и не зависят друг от друга,
// что позволяет добавлять новые подсистемы, ничего не ломая.
// ---------------------------------------------------------------------------

#pragma once

#include "CoreMinimal.h"

/** Летопись города: сюда пишется всё, что люди делают и говорят. */
DECLARE_LOG_CATEGORY_EXTERN(LogHumanCity, Log, All);
#include "HumanTypes.generated.h"

class AActor;

// ===========================================================================
//  ПЕРЕЧИСЛЕНИЯ
// ===========================================================================

/** Эмоции. Базовые (Плутчик) + социальные + проспективные (модель OCC). */
UENUM(BlueprintType)
enum class EEmotionType : uint8
{
    None            UMETA(DisplayName = "None"),

    // --- базовые ---
    Joy             UMETA(DisplayName = "Joy"),            // радость
    Sadness         UMETA(DisplayName = "Sadness"),        // печаль
    Fear            UMETA(DisplayName = "Fear"),           // страх
    Anger           UMETA(DisplayName = "Anger"),          // гнев
    Disgust         UMETA(DisplayName = "Disgust"),        // отвращение
    Surprise        UMETA(DisplayName = "Surprise"),       // удивление

    // --- проспективные (о будущем) ---
    Hope            UMETA(DisplayName = "Hope"),           // надежда
    Anxiety         UMETA(DisplayName = "Anxiety"),        // тревога
    Relief          UMETA(DisplayName = "Relief"),         // облегчение
    Disappointment  UMETA(DisplayName = "Disappointment"), // разочарование

    // --- самооценочные ---
    Pride           UMETA(DisplayName = "Pride"),          // гордость
    Shame           UMETA(DisplayName = "Shame"),          // стыд
    Guilt           UMETA(DisplayName = "Guilt"),          // вина
    Remorse         UMETA(DisplayName = "Remorse"),        // раскаяние
    Satisfaction    UMETA(DisplayName = "Satisfaction"),   // удовлетворение

    // --- социальные ---
    Love            UMETA(DisplayName = "Love"),           // любовь
    Affection       UMETA(DisplayName = "Affection"),      // тёплая привязанность
    Gratitude       UMETA(DisplayName = "Gratitude"),      // благодарность
    Admiration      UMETA(DisplayName = "Admiration"),     // восхищение
    Compassion      UMETA(DisplayName = "Compassion"),     // сострадание
    Envy            UMETA(DisplayName = "Envy"),           // зависть
    Jealousy        UMETA(DisplayName = "Jealousy"),       // ревность
    Contempt        UMETA(DisplayName = "Contempt"),       // презрение
    Resentment      UMETA(DisplayName = "Resentment"),     // обида
    Embarrassment   UMETA(DisplayName = "Embarrassment"),  // смущение
    Loneliness      UMETA(DisplayName = "Loneliness"),     // одиночество
    Trust           UMETA(DisplayName = "Trust"),          // доверие

    // --- тонические состояния ---
    Curiosity       UMETA(DisplayName = "Curiosity"),      // любопытство
    Boredom         UMETA(DisplayName = "Boredom"),        // скука
    Frustration     UMETA(DisplayName = "Frustration"),    // фрустрация
    Nostalgia       UMETA(DisplayName = "Nostalgia"),      // ностальгия
    Serenity        UMETA(DisplayName = "Serenity"),       // умиротворение
    Awe             UMETA(DisplayName = "Awe"),            // благоговение

    MAX             UMETA(Hidden)
};

/** Потребности — от телесных до экзистенциальных (пирамида, но без жёсткой иерархии). */
UENUM(BlueprintType)
enum class ENeedType : uint8
{
    // --- телесные ---
    Hunger          UMETA(DisplayName = "Hunger"),         // голод
    Thirst          UMETA(DisplayName = "Thirst"),         // жажда
    Sleep           UMETA(DisplayName = "Sleep"),          // сон
    Bladder         UMETA(DisplayName = "Bladder"),        // туалет
    Hygiene         UMETA(DisplayName = "Hygiene"),        // гигиена
    Comfort         UMETA(DisplayName = "Comfort"),        // телесный комфорт (тепло, отсутствие боли)

    // --- безопасность ---
    Safety          UMETA(DisplayName = "Safety"),         // физическая безопасность
    Health          UMETA(DisplayName = "Health"),         // здоровье
    Shelter         UMETA(DisplayName = "Shelter"),        // крыша над головой
    Money           UMETA(DisplayName = "Money"),          // материальная опора
    Order           UMETA(DisplayName = "Order"),          // предсказуемость мира

    // --- принадлежность ---
    SocialContact   UMETA(DisplayName = "SocialContact"),  // просто общение
    Belonging       UMETA(DisplayName = "Belonging"),      // принадлежность к «своим»
    Intimacy        UMETA(DisplayName = "Intimacy"),       // близость, быть понятым

    // --- уважение ---
    Esteem          UMETA(DisplayName = "Esteem"),         // уважение других
    Achievement     UMETA(DisplayName = "Achievement"),    // достижения

    // --- рост ---
    Autonomy        UMETA(DisplayName = "Autonomy"),       // самостоятельность, решать самому
    Competence      UMETA(DisplayName = "Competence"),     // быть умелым
    Novelty         UMETA(DisplayName = "Novelty"),        // новизна, впечатления
    Beauty          UMETA(DisplayName = "Beauty"),         // красота
    Meaning         UMETA(DisplayName = "Meaning"),        // смысл жизни

    MAX             UMETA(Hidden)
};

/** Элементарные действия, из которых складываются планы. */
UENUM(BlueprintType)
enum class EActionType : uint8
{
    Idle            UMETA(DisplayName = "Idle"),
    Wander          UMETA(DisplayName = "Wander"),
    MoveTo          UMETA(DisplayName = "MoveTo"),
    GoHome          UMETA(DisplayName = "GoHome"),
    Wait            UMETA(DisplayName = "Wait"),

    Eat             UMETA(DisplayName = "Eat"),
    Drink           UMETA(DisplayName = "Drink"),
    Cook            UMETA(DisplayName = "Cook"),
    Sleep           UMETA(DisplayName = "Sleep"),
    Rest            UMETA(DisplayName = "Rest"),
    UseToilet       UMETA(DisplayName = "UseToilet"),
    Wash            UMETA(DisplayName = "Wash"),

    Work            UMETA(DisplayName = "Work"),
    Study           UMETA(DisplayName = "Study"),
    Read            UMETA(DisplayName = "Read"),
    Practice        UMETA(DisplayName = "Practice"),
    Entertain       UMETA(DisplayName = "Entertain"),
    Exercise        UMETA(DisplayName = "Exercise"),

    Approach        UMETA(DisplayName = "Approach"),
    Talk            UMETA(DisplayName = "Talk"),
    Listen          UMETA(DisplayName = "Listen"),
    Help            UMETA(DisplayName = "Help"),
    Comfort         UMETA(DisplayName = "Comfort"),
    Apologize       UMETA(DisplayName = "Apologize"),
    Confront        UMETA(DisplayName = "Confront"),
    Avoid           UMETA(DisplayName = "Avoid"),
    Follow          UMETA(DisplayName = "Follow"),
    Observe         UMETA(DisplayName = "Observe"),

    Flee            UMETA(DisplayName = "Flee"),
    Hide            UMETA(DisplayName = "Hide"),
    Fight           UMETA(DisplayName = "Fight"),
    Freeze          UMETA(DisplayName = "Freeze"),

    Reflect         UMETA(DisplayName = "Reflect"),
    Reminisce       UMETA(DisplayName = "Reminisce"),
    Mourn           UMETA(DisplayName = "Mourn"),
    Celebrate       UMETA(DisplayName = "Celebrate"),
    Explore         UMETA(DisplayName = "Explore"),

    MAX             UMETA(Hidden)
};

/** Состояние цели. */
UENUM(BlueprintType)
enum class EGoalStatus : uint8
{
    Pending         UMETA(DisplayName = "Pending"),     // задумана
    Active          UMETA(DisplayName = "Active"),      // выполняется
    Suspended       UMETA(DisplayName = "Suspended"),   // отложена
    Achieved        UMETA(DisplayName = "Achieved"),    // достигнута
    Failed          UMETA(DisplayName = "Failed"),      // провалена
    Abandoned       UMETA(DisplayName = "Abandoned")    // брошена
};

/** Горизонт цели — насколько она далека. */
UENUM(BlueprintType)
enum class EGoalHorizon : uint8
{
    Immediate       UMETA(DisplayName = "Immediate"),   // прямо сейчас (поесть)
    Short           UMETA(DisplayName = "Short"),       // сегодня (сходить на работу)
    Medium          UMETA(DisplayName = "Medium"),      // недели (подружиться, накопить)
    Life            UMETA(DisplayName = "Life")         // жизненная (стать кем-то, семья)
};

/** Тип отношений. */
UENUM(BlueprintType)
enum class ERelationKind : uint8
{
    Stranger        UMETA(DisplayName = "Stranger"),
    Acquaintance    UMETA(DisplayName = "Acquaintance"),
    Colleague       UMETA(DisplayName = "Colleague"),
    Friend          UMETA(DisplayName = "Friend"),
    CloseFriend     UMETA(DisplayName = "CloseFriend"),
    Partner         UMETA(DisplayName = "Partner"),
    Family          UMETA(DisplayName = "Family"),
    Mentor          UMETA(DisplayName = "Mentor"),
    Rival           UMETA(DisplayName = "Rival"),
    Enemy           UMETA(DisplayName = "Enemy")
};

/** Речевые акты — что именно человек делает словами. */
UENUM(BlueprintType)
enum class ESpeechAct : uint8
{
    Greet           UMETA(DisplayName = "Greet"),       // здороваться
    SmallTalk       UMETA(DisplayName = "SmallTalk"),   // болтать ни о чём
    Question        UMETA(DisplayName = "Question"),    // спрашивать
    Answer          UMETA(DisplayName = "Answer"),      // отвечать
    ShareNews       UMETA(DisplayName = "ShareNews"),   // делиться новостью
    Gossip          UMETA(DisplayName = "Gossip"),      // сплетничать
    Request         UMETA(DisplayName = "Request"),     // просить
    Offer           UMETA(DisplayName = "Offer"),       // предлагать помощь
    Refuse          UMETA(DisplayName = "Refuse"),      // отказывать
    Agree           UMETA(DisplayName = "Agree"),       // соглашаться
    Compliment      UMETA(DisplayName = "Compliment"),  // хвалить
    Insult          UMETA(DisplayName = "Insult"),      // оскорблять
    Joke            UMETA(DisplayName = "Joke"),        // шутить
    Complain        UMETA(DisplayName = "Complain"),    // жаловаться
    Boast           UMETA(DisplayName = "Boast"),       // хвастаться
    Confess         UMETA(DisplayName = "Confess"),     // признаваться
    Lie             UMETA(DisplayName = "Lie"),         // врать
    Apologize       UMETA(DisplayName = "Apologize"),   // извиняться
    Thank           UMETA(DisplayName = "Thank"),       // благодарить
    Console         UMETA(DisplayName = "Console"),     // утешать
    Threaten        UMETA(DisplayName = "Threaten"),    // угрожать
    Advise          UMETA(DisplayName = "Advise"),      // советовать
    Farewell        UMETA(DisplayName = "Farewell"),    // прощаться
    Silence         UMETA(DisplayName = "Silence"),     // молчать (тоже акт)

    MAX             UMETA(Hidden)
};

/** Этап жизни — влияет на всё: тело, цели, эмоции, память. */
UENUM(BlueprintType)
enum class ELifeStage : uint8
{
    Child           UMETA(DisplayName = "Child"),        // 0-12
    Adolescent      UMETA(DisplayName = "Adolescent"),   // 13-19
    YoungAdult      UMETA(DisplayName = "YoungAdult"),   // 20-34
    Adult           UMETA(DisplayName = "Adult"),        // 35-49
    MiddleAge       UMETA(DisplayName = "MiddleAge"),    // 50-64
    Senior          UMETA(DisplayName = "Senior"),       // 65-79
    Elder           UMETA(DisplayName = "Elder")         // 80+
};

/** Фаза сна. */
UENUM(BlueprintType)
enum class ESleepPhase : uint8
{
    Awake           UMETA(DisplayName = "Awake"),
    Drowsy          UMETA(DisplayName = "Drowsy"),   // дремота
    Light           UMETA(DisplayName = "Light"),    // поверхностный
    Deep            UMETA(DisplayName = "Deep"),     // глубокий — восстановление тела
    REM             UMETA(DisplayName = "REM")       // быстрый — сны и консолидация памяти
};

/** Вид памяти. */
UENUM(BlueprintType)
enum class EMemoryKind : uint8
{
    Episodic        UMETA(DisplayName = "Episodic"),        // «что со мной случилось»
    Autobiographic  UMETA(DisplayName = "Autobiographic"),  // веха жизни
    Flashbulb       UMETA(DisplayName = "Flashbulb"),       // яркое потрясение
    Traumatic       UMETA(DisplayName = "Traumatic")        // травма (вытесняется, но возвращается)
};

/** Вид мысли в потоке сознания. */
UENUM(BlueprintType)
enum class EThoughtKind : uint8
{
    Observation     UMETA(DisplayName = "Observation"),   // «вон идёт кто-то»
    Feeling         UMETA(DisplayName = "Feeling"),       // «мне тревожно»
    Need            UMETA(DisplayName = "Need"),          // «хочу есть»
    Intention       UMETA(DisplayName = "Intention"),     // «пойду домой»
    Judgement       UMETA(DisplayName = "Judgement"),     // «он мне не нравится»
    Recall          UMETA(DisplayName = "Recall"),        // «а помню, как...»
    Rumination      UMETA(DisplayName = "Rumination"),    // навязчивое пережёвывание
    Worry           UMETA(DisplayName = "Worry"),         // тревога о будущем
    Fantasy         UMETA(DisplayName = "Fantasy"),       // мечта
    SelfTalk        UMETA(DisplayName = "SelfTalk"),      // «соберись»
    Existential     UMETA(DisplayName = "Existential"),   // «зачем это всё»
    Dream           UMETA(DisplayName = "Dream")          // сон
};

/** Стиль совладания со стрессом. */
UENUM(BlueprintType)
enum class ECopingStyle : uint8
{
    ProblemFocused  UMETA(DisplayName = "ProblemFocused"), // решать проблему
    Reappraisal     UMETA(DisplayName = "Reappraisal"),    // переосмыслить
    Suppression     UMETA(DisplayName = "Suppression"),    // задавить в себе
    SeekSupport     UMETA(DisplayName = "SeekSupport"),    // пойти к людям
    Avoidance       UMETA(DisplayName = "Avoidance"),      // избегать
    Aggression      UMETA(DisplayName = "Aggression"),     // сорваться
    Rumination      UMETA(DisplayName = "Rumination")      // пережёвывать
};

/** Типы мест на когнитивной карте. */
UENUM(BlueprintType)
enum class EPlaceKind : uint8
{
    Unknown         UMETA(DisplayName = "Unknown"),
    Home            UMETA(DisplayName = "Home"),
    Work            UMETA(DisplayName = "Work"),
    Food            UMETA(DisplayName = "Food"),
    Shop            UMETA(DisplayName = "Shop"),
    Social          UMETA(DisplayName = "Social"),
    Rest            UMETA(DisplayName = "Rest"),
    Danger          UMETA(DisplayName = "Danger"),
    Beautiful       UMETA(DisplayName = "Beautiful"),
    Landmark        UMETA(DisplayName = "Landmark"),
    Study           UMETA(DisplayName = "Study"),      // школа
    Hospital        UMETA(DisplayName = "Hospital"),   // больница
    Workshop        UMETA(DisplayName = "Workshop"),   // мастерская
    Library         UMETA(DisplayName = "Library"),    // библиотека
    Market          UMETA(DisplayName = "Market"),     // рынок
    Bathhouse       UMETA(DisplayName = "Bathhouse"),  // баня
    Bakery          UMETA(DisplayName = "Bakery"),     // пекарня
    TownHall        UMETA(DisplayName = "TownHall"),   // управа
    Field           UMETA(DisplayName = "Field"),
    Forest          UMETA(DisplayName = "Forest"),
    River           UMETA(DisplayName = "River"),
    Church          UMETA(DisplayName = "Church"),
    Castle          UMETA(DisplayName = "Castle"),
    Forge           UMETA(DisplayName = "Forge"),
    Mill            UMETA(DisplayName = "Mill")
};

/**
 * О чём идёт речь.
 *
 * Человек говорит не словами вообще, а о чём-то: о том, что ему нужно,
 * где он был, кого встретил, что прочитал, чего боится. Раньше реплика
 * собиралась из случайных слов подходящей области, и выходила бессмыслица
 * вроде «ты толкаешь вес?». Теперь у каждой реплики есть предмет, взятый
 * из головы говорящего, — и собеседнику есть на что отвечать.
 */
UENUM(BlueprintType)
enum class ETalkKind : uint8
{
    None        UMETA(DisplayName = "None"),
    Need        UMETA(DisplayName = "Need"),      // «есть хочу»
    Deed        UMETA(DisplayName = "Deed"),      // «был на рынке»
    Place       UMETA(DisplayName = "Place"),     // «у реки хорошо»
    Person      UMETA(DisplayName = "Person"),    // «Андрей вчера помог»
    Book        UMETA(DisplayName = "Book"),      // «в книге написано...»
    Feeling     UMETA(DisplayName = "Feeling"),   // «тревожно мне»
    Weather     UMETA(DisplayName = "Weather"),   // «холодает»
    Dream       UMETA(DisplayName = "Dream"),     // «хочу увидеть мир»
    Skill       UMETA(DisplayName = "Skill"),     // «научился готовить»
    Opinion     UMETA(DisplayName = "Opinion"),   // вывод из жизни
    Trouble     UMETA(DisplayName = "Trouble"),   // «денег нет»
    Work        UMETA(DisplayName = "Work")       // о работе
};

/** Ход в разговоре: что сказано и чего это ждёт в ответ. */
UENUM(BlueprintType)
enum class EDialogueMove : uint8
{
    None        UMETA(DisplayName = "None"),
    Greet       UMETA(DisplayName = "Greet"),       // «привет»
    Introduce   UMETA(DisplayName = "Introduce"),   // «я Алиса»
    HowAreYou   UMETA(DisplayName = "HowAreYou"),   // «как дела?»
    StateOfSelf UMETA(DisplayName = "StateOfSelf"), // «да ничего, на работу иду»
    Tell        UMETA(DisplayName = "Tell"),        // рассказ о своём
    Ask         UMETA(DisplayName = "Ask"),         // вопрос по теме
    Answer      UMETA(DisplayName = "Answer"),      // ответ по существу
    React       UMETA(DisplayName = "React"),       // отклик без вопроса
    Advise      UMETA(DisplayName = "Advise"),      // совет
    AskAdvice   UMETA(DisplayName = "AskAdvice"),   // «не знаешь, где тут поесть?»
    Thank       UMETA(DisplayName = "Thank"),
    Offer       UMETA(DisplayName = "Offer"),       // «держи», «покажу»
    Accept      UMETA(DisplayName = "Accept"),
    Decline     UMETA(DisplayName = "Decline"),
    Console     UMETA(DisplayName = "Console"),     // «не грусти, всё наладится»
    Instruct    UMETA(DisplayName = "Instruct"),    // «иди в мастерскую, там люди нужны»
    Obey        UMETA(DisplayName = "Obey"),        // «хорошо, схожу»
    Farewell    UMETA(DisplayName = "Farewell")
};

/** Предмет разговора — то, о чём человеку есть что сказать. */
USTRUCT(BlueprintType)
struct FTalkingPoint
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Talk") ETalkKind Kind = ETalkKind::None;

    /** Какая нужда, если речь о нужде. */
    UPROPERTY(BlueprintReadWrite, Category = "Talk") ENeedType Need = ENeedType::Hunger;

    /** Какое дело, если речь о деле. */
    UPROPERTY(BlueprintReadWrite, Category = "Talk") EActionType Action = EActionType::Idle;

    /** Какое место и где оно стоит. */
    UPROPERTY(BlueprintReadWrite, Category = "Talk") EPlaceKind PlaceKind = EPlaceKind::Unknown;
    UPROPERTY(BlueprintReadWrite, Category = "Talk") FVector Where = FVector::ZeroVector;
    UPROPERTY(BlueprintReadWrite, Category = "Talk") bool bHasWhere = false;

    /** Предмет учебника или навык. */
    UPROPERTY(BlueprintReadWrite, Category = "Talk") FName Key;

    /** Параграф учебника. */
    UPROPERTY(BlueprintReadWrite, Category = "Talk") int32 Page = 0;

    /** О ком речь и какого он рода — иначе «хороший человек, помог(ла)». */
    UPROPERTY(BlueprintReadWrite, Category = "Talk") TObjectPtr<AActor> Person = nullptr;
    UPROPERTY(BlueprintReadWrite, Category = "Talk") bool bFemale = false;

    /** О чём именно: «рынок», «Андрей», «математика, 2 класс». */
    UPROPERTY(BlueprintReadWrite, Category = "Talk") FString About;

    /** Что об этом сказать: вывод, правило, подробность. */
    UPROPERTY(BlueprintReadWrite, Category = "Talk") FString Detail;

    /** Хорошее это или дурное: -1..+1. */
    UPROPERTY(BlueprintReadWrite, Category = "Talk") float Valence = 0.0f;

    /** Насколько хочется об этом заговорить. */
    UPROPERTY(BlueprintReadWrite, Category = "Talk") float Weight = 0.5f;

    bool IsValid() const { return Kind != ETalkKind::None && !About.IsEmpty(); }
};

/**
 * Припасы.
 *
 * Всё, что человек тратит, должно откуда-то взяться. Еда не появляется
 * у плиты сама: продукты покупают или выращивают, приносят, готовят.
 */
UENUM(BlueprintType)
enum class EResourceKind : uint8
{
    None        UMETA(DisplayName = "None"),
    RawFood     UMETA(DisplayName = "RawFood"),     // сырые продукты
    CookedFood  UMETA(DisplayName = "CookedFood"),  // готовая еда
    Water       UMETA(DisplayName = "Water"),
    Coin        UMETA(DisplayName = "Coin"),
    Firewood    UMETA(DisplayName = "Firewood"),
    Herb        UMETA(DisplayName = "Herb"),
    Seed        UMETA(DisplayName = "Seed"),
    Cloth       UMETA(DisplayName = "Cloth"),

    Wood        UMETA(DisplayName = "Wood"),
    Plank       UMETA(DisplayName = "Plank"),
    Stone       UMETA(DisplayName = "Stone"),
    Clay        UMETA(DisplayName = "Clay"),
    Brick       UMETA(DisplayName = "Brick"),
    Grain       UMETA(DisplayName = "Grain"),
    Flour       UMETA(DisplayName = "Flour"),
    Tool        UMETA(DisplayName = "Tool"),
    Rope        UMETA(DisplayName = "Rope"),
    Thread      UMETA(DisplayName = "Thread"),
    Leather     UMETA(DisplayName = "Leather"),
    Iron        UMETA(DisplayName = "Iron"),
    Charcoal    UMETA(DisplayName = "Charcoal"),
    Pot         UMETA(DisplayName = "Pot"),
    Glass       UMETA(DisplayName = "Glass"),
    Paper       UMETA(DisplayName = "Paper"),
    Ink         UMETA(DisplayName = "Ink"),
    Soap        UMETA(DisplayName = "Soap"),
    Honey       UMETA(DisplayName = "Honey"),
    Milk        UMETA(DisplayName = "Milk"),
    Fish        UMETA(DisplayName = "Fish"),
    Medicine    UMETA(DisplayName = "Medicine"),

    Axe         UMETA(DisplayName = "Axe"),
    Shovel      UMETA(DisplayName = "Shovel"),
    Saw         UMETA(DisplayName = "Saw"),
    Hammer      UMETA(DisplayName = "Hammer"),
    Needle      UMETA(DisplayName = "Needle"),
    Rod         UMETA(DisplayName = "Rod"),

    Limestone   UMETA(DisplayName = "Limestone"),
    Granite     UMETA(DisplayName = "Granite"),
    Sandstone   UMETA(DisplayName = "Sandstone"),
    Flint       UMETA(DisplayName = "Flint"),
    Chalk       UMETA(DisplayName = "Chalk"),
    Quartz      UMETA(DisplayName = "Quartz"),
    Sand        UMETA(DisplayName = "Sand"),
    Peat        UMETA(DisplayName = "Peat"),
    Coal        UMETA(DisplayName = "Coal"),
    Lime        UMETA(DisplayName = "Lime"),
    Ash         UMETA(DisplayName = "Ash"),
    Salt        UMETA(DisplayName = "Salt"),
    Sulphur     UMETA(DisplayName = "Sulphur"),
    Saltpetre   UMETA(DisplayName = "Saltpetre"),
    IronOre     UMETA(DisplayName = "IronOre"),
    CopperOre   UMETA(DisplayName = "CopperOre"),
    TinOre      UMETA(DisplayName = "TinOre"),
    LeadOre     UMETA(DisplayName = "LeadOre"),
    SilverOre   UMETA(DisplayName = "SilverOre"),
    GoldOre     UMETA(DisplayName = "GoldOre"),
    Copper      UMETA(DisplayName = "Copper"),
    Tin         UMETA(DisplayName = "Tin"),
    Bronze      UMETA(DisplayName = "Bronze"),
    Lead        UMETA(DisplayName = "Lead"),
    Silver      UMETA(DisplayName = "Silver"),
    Gold        UMETA(DisplayName = "Gold"),
    Steel       UMETA(DisplayName = "Steel"),
    Flax        UMETA(DisplayName = "Flax"),
    Wool        UMETA(DisplayName = "Wool"),
    Hide        UMETA(DisplayName = "Hide"),
    Wax         UMETA(DisplayName = "Wax"),
    Resin       UMETA(DisplayName = "Resin"),
    Tar         UMETA(DisplayName = "Tar"),
    Oil         UMETA(DisplayName = "Oil"),
    Bark        UMETA(DisplayName = "Bark"),
    Reed        UMETA(DisplayName = "Reed"),
    Mushroom    UMETA(DisplayName = "Mushroom"),
    Berry       UMETA(DisplayName = "Berry"),
    Egg         UMETA(DisplayName = "Egg"),
    Meat        UMETA(DisplayName = "Meat"),
    Bone        UMETA(DisplayName = "Bone"),
    Snow        UMETA(DisplayName = "Snow"),
    Ice         UMETA(DisplayName = "Ice"),
    Straw       UMETA(DisplayName = "Straw"),
    Soil        UMETA(DisplayName = "Soil"),
    Maize       UMETA(DisplayName = "Maize"),
    Bread       UMETA(DisplayName = "Bread"),
    Sickle      UMETA(DisplayName = "Sickle"),
    Shirt       UMETA(DisplayName = "Shirt"),
    // Расширение мира: огород, сад, напитки, изделия
    BirchWood   UMETA(DisplayName = "BirchWood"),
    PineWood    UMETA(DisplayName = "PineWood"),
    CharcoalDust UMETA(DisplayName = "CharcoalDust"),
    Acorn       UMETA(DisplayName = "Acorn"),
    Cone        UMETA(DisplayName = "Cone"),
    BirchBark   UMETA(DisplayName = "BirchBark"),
    Stump       UMETA(DisplayName = "Stump"),
    Brushwood   UMETA(DisplayName = "Brushwood"),
    Marble      UMETA(DisplayName = "Marble"),
    Basalt      UMETA(DisplayName = "Basalt"),
    Slate       UMETA(DisplayName = "Slate"),
    Obsidian    UMETA(DisplayName = "Obsidian"),
    Mica        UMETA(DisplayName = "Mica"),
    Graphite    UMETA(DisplayName = "Graphite"),
    Gravel      UMETA(DisplayName = "Gravel"),
    Mud         UMETA(DisplayName = "Mud"),
    Compost     UMETA(DisplayName = "Compost"),
    ZincOre     UMETA(DisplayName = "ZincOre"),
    Zinc        UMETA(DisplayName = "Zinc"),
    Brass       UMETA(DisplayName = "Brass"),
    Electrum    UMETA(DisplayName = "Electrum"),
    Gem         UMETA(DisplayName = "Gem"),
    Diamond     UMETA(DisplayName = "Diamond"),
    Ruby        UMETA(DisplayName = "Ruby"),
    Emerald     UMETA(DisplayName = "Emerald"),
    Sapphire    UMETA(DisplayName = "Sapphire"),
    Pearl       UMETA(DisplayName = "Pearl"),
    Wheat       UMETA(DisplayName = "Wheat"),
    Rye         UMETA(DisplayName = "Rye"),
    Barley      UMETA(DisplayName = "Barley"),
    Oats        UMETA(DisplayName = "Oats"),
    Turnip      UMETA(DisplayName = "Turnip"),
    Cabbage     UMETA(DisplayName = "Cabbage"),
    Carrot      UMETA(DisplayName = "Carrot"),
    Onion       UMETA(DisplayName = "Onion"),
    Garlic      UMETA(DisplayName = "Garlic"),
    Beet        UMETA(DisplayName = "Beet"),
    Cucumber    UMETA(DisplayName = "Cucumber"),
    Pumpkin     UMETA(DisplayName = "Pumpkin"),
    Melon       UMETA(DisplayName = "Melon"),
    Watermelon  UMETA(DisplayName = "Watermelon"),
    Potato      UMETA(DisplayName = "Potato"),
    Sunflower   UMETA(DisplayName = "Sunflower"),
    Hemp        UMETA(DisplayName = "Hemp"),
    Hay         UMETA(DisplayName = "Hay"),
    Apple       UMETA(DisplayName = "Apple"),
    Pear        UMETA(DisplayName = "Pear"),
    Plum        UMETA(DisplayName = "Plum"),
    Cherry      UMETA(DisplayName = "Cherry"),
    Grape       UMETA(DisplayName = "Grape"),
    Nut         UMETA(DisplayName = "Nut"),
    Feather     UMETA(DisplayName = "Feather"),
    Fur         UMETA(DisplayName = "Fur"),
    Antler      UMETA(DisplayName = "Antler"),
    Tallow      UMETA(DisplayName = "Tallow"),
    CurdledMilk UMETA(DisplayName = "CurdledMilk"),
    Butter      UMETA(DisplayName = "Butter"),
    Cheese      UMETA(DisplayName = "Cheese"),
    CottageCheese UMETA(DisplayName = "CottageCheese"),
    SmokedFish  UMETA(DisplayName = "SmokedFish"),
    SmokedMeat  UMETA(DisplayName = "SmokedMeat"),
    DriedFish   UMETA(DisplayName = "DriedFish"),
    Hops        UMETA(DisplayName = "Hops"),
    GrapeMust   UMETA(DisplayName = "GrapeMust"),
    Kvass       UMETA(DisplayName = "Kvass"),
    Beer        UMETA(DisplayName = "Beer"),
    Wine        UMETA(DisplayName = "Wine"),
    Mead        UMETA(DisplayName = "Mead"),
    Vinegar     UMETA(DisplayName = "Vinegar"),
    Madder      UMETA(DisplayName = "Madder"),
    Woad        UMETA(DisplayName = "Woad"),
    Dye         UMETA(DisplayName = "Dye"),
    Candle      UMETA(DisplayName = "Candle"),
    OliveOil    UMETA(DisplayName = "OliveOil"),
    Soda        UMETA(DisplayName = "Soda"),
    Vine        UMETA(DisplayName = "Vine"),
    Basket      UMETA(DisplayName = "Basket"),
    Cork        UMETA(DisplayName = "Cork"),
    Gunpowder   UMETA(DisplayName = "Gunpowder"),
    Mortar      UMETA(DisplayName = "Mortar"),
    CharcoalPencil UMETA(DisplayName = "CharcoalPencil"),
};

UENUM(BlueprintType)
enum class EVillageRole : uint8
{
    Peasant     UMETA(DisplayName = "Peasant"),
    King        UMETA(DisplayName = "King"),
    Queen       UMETA(DisplayName = "Queen"),
    Prince      UMETA(DisplayName = "Prince"),
    Princess    UMETA(DisplayName = "Princess"),
    QueenMother UMETA(DisplayName = "QueenMother"),
    Steward     UMETA(DisplayName = "Steward"),
    Cook        UMETA(DisplayName = "Cook"),
    Guard       UMETA(DisplayName = "Guard"),
    Maid        UMETA(DisplayName = "Maid"),
    Groom       UMETA(DisplayName = "Groom"),
    Child       UMETA(DisplayName = "Child")
};

/**
 * Положение тела.
 *
 * Человек не делает всё стоя. Он садится за стол, ложится в кровать,
 * встаёт, когда уходит. Поза — не украшение: сидя не походишь, а чтобы
 * сесть, нужно сперва оказаться у того, на что садятся.
 */
UENUM(BlueprintType)
enum class EPosture : uint8
{
    Standing    UMETA(DisplayName = "Standing"),
    Sitting     UMETA(DisplayName = "Sitting"),
    Lying       UMETA(DisplayName = "Lying")
};

/**
 * Что человек делает прямо сейчас внутри одного дела.
 *
 * Дело не совершается мгновенно. Сперва туда надо дойти, потом взять что
 * нужно и приладиться, и только потом начинается само занятие. Раньше всё
 * это происходило в один кадр — оттого и выглядело, будто люди берут
 * книги из воздуха.
 */
UENUM(BlueprintType)
enum class EActionPhase : uint8
{
    None        UMETA(DisplayName = "None"),
    Travelling  UMETA(DisplayName = "Travelling"),   // иду
    Preparing   UMETA(DisplayName = "Preparing"),    // беру, сажусь, приноравливаюсь
    Doing       UMETA(DisplayName = "Doing"),        // делаю
    Finishing   UMETA(DisplayName = "Finishing")     // кладу на место, встаю
};

/** Недомогания и расстройства. */
UENUM(BlueprintType)
enum class EAilment : uint8
{
    None            UMETA(DisplayName = "None"),
    Cold            UMETA(DisplayName = "Cold"),          // простуда
    Fever           UMETA(DisplayName = "Fever"),         // лихорадка
    Injury          UMETA(DisplayName = "Injury"),        // травма
    Headache        UMETA(DisplayName = "Headache"),      // головная боль
    Exhaustion      UMETA(DisplayName = "Exhaustion"),    // истощение
    Burnout         UMETA(DisplayName = "Burnout"),       // выгорание
    Depression      UMETA(DisplayName = "Depression"),    // депрессия
    AnxietyDisorder UMETA(DisplayName = "AnxietyDisorder")// тревожное расстройство
};

// ===========================================================================
//  ЛИЧНОСТЬ И ЦЕННОСТИ
// ===========================================================================

/** Большая пятёрка. Значения 0..1. Медленно меняется под влиянием жизни. */
USTRUCT(BlueprintType)
struct FBigFive
{
    GENERATED_BODY()

    /** Открытость опыту: любопытство, тяга к новому, воображение. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BigFive", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Openness = 0.5f;

    /** Добросовестность: дисциплина, планирование, доведение до конца. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BigFive", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Conscientiousness = 0.5f;

    /** Экстраверсия: тяга к людям, энергия вовне. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BigFive", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Extraversion = 0.5f;

    /** Доброжелательность: эмпатия, уступчивость, доверие. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BigFive", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Agreeableness = 0.5f;

    /** Нейротизм: эмоциональная нестабильность, склонность к тревоге. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BigFive", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Neuroticism = 0.5f;
};

/** Фасеты — то, чем различаются два человека с одинаковой «пятёркой». */
USTRUCT(BlueprintType)
struct FPersonalityFacets
{
    GENERATED_BODY()

    /** Импульсивность — действует раньше, чем думает. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facets") float Impulsivity = 0.5f;
    /** Склонность к риску. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facets") float RiskTaking = 0.5f;
    /** Злопамятность — как долго держит обиду. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facets") float Vengefulness = 0.4f;
    /** Честность — насколько тяжело даётся ложь. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facets") float Honesty = 0.6f;
    /** Эмпатия — способность чувствовать чужое. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facets") float Empathy = 0.5f;
    /** Упрямство — сопротивление смене мнения. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facets") float Stubbornness = 0.5f;
    /** Оптимизм — ожидание хорошего исхода. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facets") float Optimism = 0.5f;
    /** Любознательность. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facets") float Curiosity = 0.5f;
    /** Самоконтроль — база силы воли. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facets") float SelfControl = 0.5f;
    /** Тревожность как черта (не состояние). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facets") float TraitAnxiety = 0.4f;
    /** Общительность — сколько общения нужно, чтобы наесться. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facets") float Sociability = 0.5f;
    /** Амбициозность. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facets") float Ambition = 0.5f;
};

/** Базовые жизненные ценности (по мотивам круга Шварца). Сумма важностей 0..1 каждая. */
USTRUCT(BlueprintType)
struct FValueSystem
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Values") float SelfDirection = 0.5f; // свобода, самостоятельность
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Values") float Stimulation   = 0.4f; // новизна, острота
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Values") float Hedonism      = 0.4f; // удовольствие
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Values") float Achievement   = 0.5f; // успех
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Values") float Power         = 0.3f; // влияние, статус
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Values") float Security      = 0.5f; // стабильность
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Values") float Conformity    = 0.4f; // «как принято»
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Values") float Tradition     = 0.3f; // корни, обычай
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Values") float Benevolence   = 0.5f; // забота о близких
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Values") float Universalism  = 0.4f; // справедливость для всех
};

/** Моральные основания — по ним человек судит поступки (свои и чужие). */
USTRUCT(BlueprintType)
struct FMoralFoundations
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Morals") float Care      = 0.6f; // не навреди
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Morals") float Fairness  = 0.6f; // честно/нечестно
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Morals") float Loyalty   = 0.5f; // свои/чужие
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Morals") float Authority = 0.4f; // уважение к старшим/порядку
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Morals") float Sanctity  = 0.3f; // чистота, отвращение
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Morals") float Liberty   = 0.5f; // свобода от принуждения
};

// ===========================================================================
//  АФФЕКТ
// ===========================================================================

/** Трёхмерное аффективное пространство PAD — «сырое» самочувствие. */
USTRUCT(BlueprintType)
struct FAffectPAD
{
    GENERATED_BODY()

    /** Приятность: -1 (мучительно) .. +1 (прекрасно). */
    UPROPERTY(BlueprintReadWrite, Category = "Affect") float Pleasure = 0.0f;
    /** Возбуждение: -1 (вялость) .. +1 (на взводе). */
    UPROPERTY(BlueprintReadWrite, Category = "Affect") float Arousal = 0.0f;
    /** Доминирование: -1 (я беспомощен) .. +1 (я контролирую). */
    UPROPERTY(BlueprintReadWrite, Category = "Affect") float Dominance = 0.0f;
};

/** Одна вспышка эмоции. Эмоция — событие, а не число: она рождается, живёт, гаснет. */
USTRUCT(BlueprintType)
struct FEmotionInstance
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Emotion") EEmotionType Type = EEmotionType::None;
    /** Текущая сила 0..1. */
    UPROPERTY(BlueprintReadWrite, Category = "Emotion") float Intensity = 0.0f;
    /** Скорость затухания в секунду. Стыд тлеет дольше, чем испуг. */
    UPROPERTY(BlueprintReadWrite, Category = "Emotion") float DecayRate = 0.05f;
    /** Кто/что вызвало. */
    UPROPERTY(BlueprintReadWrite, Category = "Emotion") TObjectPtr<AActor> Cause = nullptr;
    /** Короткое словесное объяснение — попадёт в мысли. */
    UPROPERTY(BlueprintReadWrite, Category = "Emotion") FString Reason;
    /** Момент рождения (секунды мира). */
    UPROPERTY(BlueprintReadWrite, Category = "Emotion") float BornAt = 0.0f;
    /** Сколько раз эту эмоцию подавляли — подавление копит цену. */
    UPROPERTY(BlueprintReadWrite, Category = "Emotion") float Suppressed = 0.0f;
};

/** Настроение — медленный фон, на котором вспыхивают эмоции. */
USTRUCT(BlueprintType)
struct FMoodState
{
    GENERATED_BODY()

    /** Текущее настроение. */
    UPROPERTY(BlueprintReadWrite, Category = "Mood") FAffectPAD Mood;
    /** Врождённая «точка равновесия», к которой настроение всегда возвращается. */
    UPROPERTY(BlueprintReadWrite, Category = "Mood") FAffectPAD Baseline;
    /** Накопленный стресс (аллостатическая нагрузка) 0..1. */
    UPROPERTY(BlueprintReadWrite, Category = "Mood") float StressLoad = 0.0f;
    /** Эмоциональное истощение 0..1 — когда чувствовать уже нечем. */
    UPROPERTY(BlueprintReadWrite, Category = "Mood") float Numbness = 0.0f;
};

/** Событие, прошедшее когнитивную оценку. Именно оно порождает эмоции. */
USTRUCT(BlueprintType)
struct FAppraisedEvent
{
    GENERATED_BODY()

    /** Машинный тег события: "Insulted", "Helped", "SawFriend"... */
    UPROPERTY(BlueprintReadWrite, Category = "Appraisal") FName Tag;
    /** Человеческое описание для памяти и мыслей. */
    UPROPERTY(BlueprintReadWrite, Category = "Appraisal") FString Description;
    /** Насколько это хорошо ДЛЯ МЕНЯ: -1..+1. */
    UPROPERTY(BlueprintReadWrite, Category = "Appraisal") float Desirability = 0.0f;
    /** Неожиданность 0..1 — основа удивления. */
    UPROPERTY(BlueprintReadWrite, Category = "Appraisal") float Unexpectedness = 0.0f;
    /** Насколько я это контролирую 0..1 — отличает гнев от страха. */
    UPROPERTY(BlueprintReadWrite, Category = "Appraisal") float Controllability = 0.5f;
    /** Моя ли это вина 0..1 — основа вины и гордости. */
    UPROPERTY(BlueprintReadWrite, Category = "Appraisal") float SelfAgency = 0.0f;
    /** Вина другого 0..1 — основа гнева и благодарности. */
    UPROPERTY(BlueprintReadWrite, Category = "Appraisal") float OtherAgency = 0.0f;
    /** Нарушение моральных норм: -1 (мерзость) .. +1 (благородно). */
    UPROPERTY(BlueprintReadWrite, Category = "Appraisal") float NormAlignment = 0.0f;
    /** Уверенность, что это правда/случилось 0..1. <1 — это прогноз, а не факт. */
    UPROPERTY(BlueprintReadWrite, Category = "Appraisal") float Certainty = 1.0f;
    /** Общая значимость 0..1 — попадёт ли вообще в сознание. */
    UPROPERTY(BlueprintReadWrite, Category = "Appraisal") float Significance = 0.5f;
    /** Кто участвовал. */
    UPROPERTY(BlueprintReadWrite, Category = "Appraisal") TObjectPtr<AActor> Subject = nullptr;
    /** Где случилось. */
    UPROPERTY(BlueprintReadWrite, Category = "Appraisal") FVector Location = FVector::ZeroVector;
};

// ===========================================================================
//  ТЕЛО
// ===========================================================================

/** Гормональный фон — химия, которая правит настроением и телом. */
USTRUCT(BlueprintType)
struct FHormones
{
    GENERATED_BODY()

    /** Адреналин — мгновенная мобилизация. */
    UPROPERTY(BlueprintReadWrite, Category = "Hormones") float Adrenaline = 0.05f;
    /** Кортизол — медленный стресс, разрушает при хроническом уровне. */
    UPROPERTY(BlueprintReadWrite, Category = "Hormones") float Cortisol = 0.15f;
    /** Дофамин — предвкушение и мотивация. */
    UPROPERTY(BlueprintReadWrite, Category = "Hormones") float Dopamine = 0.4f;
    /** Серотонин — устойчивость настроения, ощущение статуса. */
    UPROPERTY(BlueprintReadWrite, Category = "Hormones") float Serotonin = 0.5f;
    /** Окситоцин — привязанность и доверие. */
    UPROPERTY(BlueprintReadWrite, Category = "Hormones") float Oxytocin = 0.3f;
    /** Эндорфин — естественное обезболивание. */
    UPROPERTY(BlueprintReadWrite, Category = "Hormones") float Endorphin = 0.2f;
    /** Тестостерон — напор, доминирование, риск. */
    UPROPERTY(BlueprintReadWrite, Category = "Hormones") float Testosterone = 0.4f;
    /** Мелатонин — сонливость по часам. */
    UPROPERTY(BlueprintReadWrite, Category = "Hormones") float Melatonin = 0.1f;
};

/**
 * Органы.
 *
 * Шкалы жизни у человека нет. Есть сердце, лёгкие, мозг, печень, почки,
 * желудок, кишечник, кожа, мышцы и кости — и каждое из них работает
 * настолько, насколько цело. Смерть наступает не когда «кончилось здоровье»,
 * а когда отказал орган, без которого не живут.
 */
USTRUCT(BlueprintType)
struct FOrgans
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Organs") float Heart = 1.0f;
    UPROPERTY(BlueprintReadOnly, Category = "Organs") float Lungs = 1.0f;
    UPROPERTY(BlueprintReadOnly, Category = "Organs") float Brain = 1.0f;
    UPROPERTY(BlueprintReadOnly, Category = "Organs") float Liver = 1.0f;
    UPROPERTY(BlueprintReadOnly, Category = "Organs") float Kidneys = 1.0f;
    UPROPERTY(BlueprintReadOnly, Category = "Organs") float Stomach = 1.0f;
    UPROPERTY(BlueprintReadOnly, Category = "Organs") float Gut = 1.0f;
    UPROPERTY(BlueprintReadOnly, Category = "Organs") float Skin = 1.0f;
    UPROPERTY(BlueprintReadOnly, Category = "Organs") float Muscle = 1.0f;
    UPROPERTY(BlueprintReadOnly, Category = "Organs") float Bones = 1.0f;
};

/** Показатели организма. */
USTRUCT(BlueprintType)
struct FBodyState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Body") float HeartRate = 70.0f;              // уд/мин
    UPROPERTY(BlueprintReadWrite, Category = "Body") float BloodPressureSystolic = 120.0f;
    UPROPERTY(BlueprintReadWrite, Category = "Body") float BloodPressureDiastolic = 80.0f;
    UPROPERTY(BlueprintReadWrite, Category = "Body") float RespiratoryRate = 16.0f;        // вдохов/мин
    UPROPERTY(BlueprintReadWrite, Category = "Body") float OxygenSaturation = 98.0f;       // %
    UPROPERTY(BlueprintReadWrite, Category = "Body") float BodyTemperature = 36.6f;        // °C
    UPROPERTY(BlueprintReadWrite, Category = "Body") float GlucoseLevel = 5.0f;            // ммоль/л
    UPROPERTY(BlueprintReadWrite, Category = "Body") float Hydration = 1.0f;               // 0..1
    UPROPERTY(BlueprintReadWrite, Category = "Body") float StomachFullness = 0.6f;         // 0..1
    UPROPERTY(BlueprintReadWrite, Category = "Body") float BladderFullness = 0.2f;         // 0..1
    UPROPERTY(BlueprintReadWrite, Category = "Body") float Cleanliness = 0.9f;             // 0..1

    /** Физическая энергия 0..1 — «сколько во мне сил прямо сейчас». */
    UPROPERTY(BlueprintReadWrite, Category = "Body") float Stamina = 1.0f;
    /** Долг сна в часах — копится, если не спать. */
    UPROPERTY(BlueprintReadWrite, Category = "Body") float SleepDebt = 0.0f;
    /** Боль 0..1. */
    UPROPERTY(BlueprintReadWrite, Category = "Body") float Pain = 0.0f;
    /** Общее здоровье 0..1. */
    /** Состояние тела 0..1 — не запас жизни, а сводка по органам и мере лишений. */
    UPROPERTY(BlueprintReadWrite, Category = "Body") float Health = 1.0f;

    /** Запас сил тела 0..1: полный — примерно на неделю без еды. */
    UPROPERTY(BlueprintReadWrite, Category = "Body") float Reserve = 1.0f;

    /** Крови в теле, литры. У взрослого около пяти. */
    UPROPERTY(BlueprintReadWrite, Category = "Body") float BloodLitres = 5.0f;
    /** Сила иммунитета 0..1 — падает от стресса и недосыпа. */
    UPROPERTY(BlueprintReadWrite, Category = "Body") float Immunity = 0.9f;
    /** Физическая форма 0..1 — растёт от нагрузки, падает от лежания. */
    UPROPERTY(BlueprintReadWrite, Category = "Body") float Fitness = 0.5f;
};

/** Активное недомогание. */
USTRUCT(BlueprintType)
struct FAilmentInstance
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Health") EAilment Type = EAilment::None;
    /** Тяжесть 0..1. */
    UPROPERTY(BlueprintReadWrite, Category = "Health") float Severity = 0.0f;
    /** Сколько осталось (секунды мира); отрицательное — хроническое. */
    UPROPERTY(BlueprintReadWrite, Category = "Health") float RemainingTime = 0.0f;
    UPROPERTY(BlueprintReadWrite, Category = "Health") float StartedAt = 0.0f;
};

// ===========================================================================
//  ПОТРЕБНОСТИ
// ===========================================================================

/** Одна потребность. */
USTRUCT(BlueprintType)
struct FNeedState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Need") ENeedType Type = ENeedType::Hunger;
    /** Удовлетворённость 0 (мучительно не хватает) .. 1 (полностью). */
    UPROPERTY(BlueprintReadWrite, Category = "Need") float Satisfaction = 1.0f;
    /** Скорость падения в секунду. */
    UPROPERTY(BlueprintReadWrite, Category = "Need") float DecayPerSecond = 0.001f;
    /** Личная важность 0..2 — у кого-то смысл важнее еды. */
    UPROPERTY(BlueprintReadWrite, Category = "Need") float Weight = 1.0f;
    /** Порог, ниже которого потребность начинает кричать. */
    UPROPERTY(BlueprintReadWrite, Category = "Need") float UrgentThreshold = 0.35f;
    /** Как давно её удовлетворяли (секунды мира). */
    UPROPERTY(BlueprintReadWrite, Category = "Need") float LastSatisfiedAt = 0.0f;
};

// ===========================================================================
//  ПАМЯТЬ И ЗНАНИЯ
// ===========================================================================

/** Эпизод — то, что со мной случилось. Помнится нечётко и со временем искажается. */
USTRUCT(BlueprintType)
struct FEpisodicMemory
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Memory") int32 Id = 0;
    UPROPERTY(BlueprintReadWrite, Category = "Memory") EMemoryKind Kind = EMemoryKind::Episodic;
    /** Как я это помню словами (может измениться при пересказе!). */
    UPROPERTY(BlueprintReadWrite, Category = "Memory") FString Summary;
    /** Тег для ассоциативного поиска. */
    UPROPERTY(BlueprintReadWrite, Category = "Memory") FName Tag;
    /** Кто там был. */
    UPROPERTY(BlueprintReadWrite, Category = "Memory") TArray<TObjectPtr<AActor>> Participants;
    UPROPERTY(BlueprintReadWrite, Category = "Memory") FVector Location = FVector::ZeroVector;
    /** Когда случилось (секунды мира). */
    UPROPERTY(BlueprintReadWrite, Category = "Memory") float Timestamp = 0.0f;
    /** Прочность следа 0..1 — падает по кривой забывания, растёт при повторении. */
    UPROPERTY(BlueprintReadWrite, Category = "Memory") float Strength = 1.0f;
    /** Эмоциональная окраска -1..+1. */
    UPROPERTY(BlueprintReadWrite, Category = "Memory") float Valence = 0.0f;
    /** Эмоциональный накал 0..1 — яркие воспоминания не стираются. */
    UPROPERTY(BlueprintReadWrite, Category = "Memory") float Arousal = 0.0f;
    /** Сколько раз вспоминал. */
    UPROPERTY(BlueprintReadWrite, Category = "Memory") int32 RecallCount = 0;
    UPROPERTY(BlueprintReadWrite, Category = "Memory") float LastRecalledAt = 0.0f;
    /** Накопленное искажение 0..1 — память врёт всё сильнее. */
    UPROPERTY(BlueprintReadWrite, Category = "Memory") float Distortion = 0.0f;
    /** Вытеснено ли (травма, к которой сознание не подпускает). */
    UPROPERTY(BlueprintReadWrite, Category = "Memory") bool bRepressed = false;
    /** Консолидировано во сне — такое уже почти не забывается. */
    UPROPERTY(BlueprintReadWrite, Category = "Memory") bool bConsolidated = false;
};

/** Убеждение о мире. Может быть ЛОЖНЫМ — и человек будет по нему действовать. */
USTRUCT(BlueprintType)
struct FBelief
{
    GENERATED_BODY()

    /** О ком/чём: имя, объект, место. */
    UPROPERTY(BlueprintReadWrite, Category = "Belief") FName Subject;
    /** Что именно: "IsDangerous", "HasFood", "Likes", "IsLiar"... */
    UPROPERTY(BlueprintReadWrite, Category = "Belief") FName Predicate;
    /** Значение убеждения -1..+1 (или 0/1 для фактов). */
    UPROPERTY(BlueprintReadWrite, Category = "Belief") float Value = 0.0f;
    /** Уверенность 0..1. */
    UPROPERTY(BlueprintReadWrite, Category = "Belief") float Confidence = 0.5f;
    /** Кто сказал (nullptr = увидел сам). */
    UPROPERTY(BlueprintReadWrite, Category = "Belief") TObjectPtr<AActor> Source = nullptr;
    UPROPERTY(BlueprintReadWrite, Category = "Belief") float LearnedAt = 0.0f;
    /** Проверено личным опытом. */
    UPROPERTY(BlueprintReadWrite, Category = "Belief") bool bVerified = false;
    /** Человеческая формулировка. */
    UPROPERTY(BlueprintReadWrite, Category = "Belief") FString Text;
};

// ===========================================================================
//  АФФОРДАНСЫ — «что с этим можно сделать»
// ===========================================================================
//
//  Ключевая идея всей архитектуры принятия решений.
//
//  Мир не говорит человеку, что ему делать. Мир говорит, что он ПРЕДЛАГАЕТ:
//  «во мне есть еда», «здесь можно лечь», «с этим человеком можно поговорить».
//  Это свойство вещей, а не правило поведения.
//
//  Внутри человека НЕТ таблицы «голоден → иди есть». Есть голод, есть
//  предложение «+0.8 сытости за 20 минут в 300 метрах отсюда» — и есть
//  оценка, которая рождается из его состояния, опыта и характера.
//  Поэтому голодный может пойти не есть, а разговаривать. Как человек.
//
// ===========================================================================

/** Что предложение обещает дать (или отнять). */
USTRUCT(BlueprintType)
struct FNeedPromise
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Affordance")
    ENeedType Need = ENeedType::Hunger;

    /** Сколько даст: -1..+1. Отрицательное — отнимет (работа отнимает силы). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Affordance")
    float Amount = 0.5f;
};

/** Откуда взялось предложение. */
UENUM(BlueprintType)
enum class EAffordanceSource : uint8
{
    Object   UMETA(DisplayName = "Object"),   // вещь: кровать, плита
    Place    UMETA(DisplayName = "Place"),    // место: кафе, сквер
    Person   UMETA(DisplayName = "Person"),   // человек: поговорить, помочь
    Self     UMETA(DisplayName = "Self")      // сам с собой: подумать, отдохнуть
};

/** Одно предложение мира. */
USTRUCT(BlueprintType)
struct FAffordance
{
    GENERATED_BODY()

    /** Ключ опыта: «поесть@кафе №3». По нему помнится, чем кончилось ЗДЕСЬ. */
    UPROPERTY(BlueprintReadWrite, Category = "Affordance") FName Key;

    /**
     * Ключ вида: «поесть@кафе».
     *
     * За вещами стоят виды вещей. Обжёгшись у одной плиты, живое существо
     * делается осторожнее со всеми плитами, а не только с этой. Без этого
     * человек подходил к каждому новому кафе с нулевым опытом, будто
     * никогда в жизни не ел, — и был в этом смысле глупее собаки.
     */
    UPROPERTY(BlueprintReadWrite, Category = "Affordance") FName CategoryKey;

    /** Ремесло, которому это дело принадлежит. Пусто — делать может всякий. */
    UPROPERTY(BlueprintReadWrite, Category = "Affordance") FName Craft;

    UPROPERTY(BlueprintReadWrite, Category = "Affordance") FName Deal;
    UPROPERTY(BlueprintReadWrite, Category = "Affordance") EResourceKind DealKind = EResourceKind::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Affordance") EActionType Action = EActionType::Idle;
    UPROPERTY(BlueprintReadWrite, Category = "Affordance") EAffordanceSource Source = EAffordanceSource::Object;

    UPROPERTY(BlueprintReadWrite, Category = "Affordance") TObjectPtr<AActor> Target = nullptr;
    UPROPERTY(BlueprintReadWrite, Category = "Affordance") FVector Location = FVector::ZeroVector;
    UPROPERTY(BlueprintReadWrite, Category = "Affordance") bool bHasLocation = false;

    /** Что это даст. Объективное свойство вещи, а не мнение человека. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Affordance")
    TArray<FNeedPromise> Promises;

    /** Сколько игровых секунд занимает. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Affordance") float Duration = 600.0f;
    /** Объективная опасность 0..1. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Affordance") float Risk = 0.0f;
    /** Сколько стоит денег. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Affordance") float MoneyCost = 0.0f;
    /** Сколько приносит денег за игровой час. Для работы и приработков. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Affordance") float MoneyGainPerHour = 0.0f;
    /** Сколько требует усилия воли 0..1. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Affordance") float EffortCost = 0.05f;
    /** Нужное умение и сложность. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Affordance") FName RequiredSkill;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Affordance") float Difficulty = 0.0f;

    /** Как человек назвал бы это действие. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Affordance") FString Label;

    /**
     * Это чужое.
     *
     * Человек не заходит в незнакомый дом, чтобы поспать в чужой кровати
     * или воспользоваться чужой уборной, — даже если очень надо и дверь
     * открыта. Это не запрет в его голове, это свойство самой вещи:
     * она принадлежит кому-то.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Affordance") bool bPrivate = false;

    /** Чьё именно: точка, по которой хозяин узнаёт своё. */
    UPROPERTY(BlueprintReadWrite, Category = "Affordance") FVector OwnerAnchor = FVector::ZeroVector;

    /** Требуется ли подойти вплотную. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Affordance") bool bRequiresProximity = true;

    // --- УСЛОВИЯ -----------------------------------------------------------
    //
    // Дело не делается по щелчку. Чтобы читать, книгу надо держать в руках;
    // чтобы читать — нужен свет; чтобы есть за столом — надо сесть.
    // Условие, которое не выполнено, не отменяет замысла: человек сперва
    // сделает то, что нужно, и только потом возьмётся за само дело.

    /** Вещь надо взять в руки. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Affordance") bool bNeedsInHand = false;

    /** Нужен свет: в темноте не читают и не шьют. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Affordance") bool bNeedsLight = false;

    /** Надо сесть. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Affordance") bool bNeedsSeat = false;

    /** Надо лечь. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Affordance") bool bNeedsLying = false;

    /** Сколько секунд занимает подготовка: взять, сесть, приладиться. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Affordance") float SetupSeconds = 0.0f;

    /** Что для этого нужно израсходовать: продукты, дрова, деньги. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Affordance") EResourceKind Requires = EResourceKind::None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Affordance") float RequiresAmount = 0.0f;

    /** Что после этого появится: готовая еда, урожай, купленное. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Affordance") EResourceKind Produces = EResourceKind::None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Affordance") float ProducesAmount = 0.0f;

    /**
     * Часы, когда это вообще доступно (0..24). Равные значения — всегда.
     * Контора ночью закрыта — и человеку не надо об этом «знать правило»:
     * такой возможности просто не существует, пока она закрыта.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Affordance") float AvailableFromHour = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Affordance") float AvailableToHour = 0.0f;
};

/** Выученная связь «поступок → чем он для меня обернулся». */
USTRUCT(BlueprintType)
struct FOutcomeAssociation
{
    GENERATED_BODY()

    /** Ключ аффорданса. */
    UPROPERTY(BlueprintReadWrite, Category = "Learning") FName Key;
    /** Насколько хорошо это обычно кончается: -1..+1. Ниоткуда не задано — выучено. */
    UPROPERTY(BlueprintReadWrite, Category = "Learning") float ExpectedValue = 0.0f;
    /** Насколько я в этом уверен 0..1 — растёт с числом проб. */
    UPROPERTY(BlueprintReadWrite, Category = "Learning") float Confidence = 0.0f;
    UPROPERTY(BlueprintReadWrite, Category = "Learning") int32 Samples = 0;
    UPROPERTY(BlueprintReadWrite, Category = "Learning") float LastAt = 0.0f;

    /**
     * Пресыщение 0..1. Растёт с каждым повтором и тает со временем.
     *
     * Без него человек, нашедший одно приятное занятие, застревал бы в нём
     * навсегда: лежал бы на диване до самой смерти, потому что диван
     * каждый раз честно приносит комфорт. Живому надоедает — и именно
     * поэтому он встаёт и идёт делать что-то другое.
     */
    UPROPERTY(BlueprintReadWrite, Category = "Learning") float Satiation = 0.0f;
    /** Человеческая формулировка вывода: «там вкусно», «с ним тяжело». */
    UPROPERTY(BlueprintReadWrite, Category = "Learning") FString Conclusion;
};

/**
 * Выученная дорога.
 *
 * Маршрут не выдаётся сверху — он остаётся в памяти после того, как
 * человек прошёл дорогу сам. В следующий раз память подсказывает, куда
 * держать, но идёт он всё равно глазами: подсказка — не рельсы.
 *
 * Именно этого не хватало больше всего: каждый поход в знакомое место
 * начинался так, будто человек там никогда не был.
 */
USTRUCT(BlueprintType)
struct FRouteMemory
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Map") FVector From = FVector::ZeroVector;
    UPROPERTY(BlueprintReadWrite, Category = "Map") FVector To = FVector::ZeroVector;

    /** Повороты, которые запомнились по дороге. */
    UPROPERTY(BlueprintReadWrite, Category = "Map") TArray<FVector> Waypoints;

    /** Насколько твёрдо помнится 0..1. Забывается без хождения. */
    UPROPERTY(BlueprintReadWrite, Category = "Map") float Strength = 0.4f;

    /** Сколько игровых секунд занял путь — по этому и судят, далеко ли. */
    UPROPERTY(BlueprintReadWrite, Category = "Map") float TravelTime = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Map") int32 UseCount = 1;
    UPROPERTY(BlueprintReadWrite, Category = "Map") float LastUsedAt = 0.0f;
};

/** Известное место на когнитивной карте. */
USTRUCT(BlueprintType)
struct FKnownLocation
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Map") EPlaceKind Kind = EPlaceKind::Unknown;
    UPROPERTY(BlueprintReadWrite, Category = "Map") FString Label;
    UPROPERTY(BlueprintReadWrite, Category = "Map") FVector Location = FVector::ZeroVector;

    /**
     * Что, по моему мнению, здесь можно получить.
     * Именно МОЕМУ: представление может расходиться с действительностью
     * и уточняется только опытом.
     */
    UPROPERTY(BlueprintReadWrite, Category = "Map") TArray<FAffordance> Offers;
    /** Насколько хорошо знаю это место 0..1. */
    UPROPERTY(BlueprintReadWrite, Category = "Map") float Familiarity = 0.1f;
    /** Насколько мне тут хорошо -1..+1 (место может пугать). */
    UPROPERTY(BlueprintReadWrite, Category = "Map") float Affect = 0.0f;
    /** Откуда узнал: сам видел / рассказали. */
    UPROPERTY(BlueprintReadWrite, Category = "Map") bool bFirsthand = true;
    UPROPERTY(BlueprintReadWrite, Category = "Map") TObjectPtr<AActor> ToldBy = nullptr;
    UPROPERTY(BlueprintReadWrite, Category = "Map") float LastVisitedAt = 0.0f;
};

/** Навык. */
USTRUCT(BlueprintType)
struct FSkillEntry
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Skill") FName Name;
    /** Уровень 0..1. */
    UPROPERTY(BlueprintReadWrite, Category = "Skill") float Level = 0.0f;
    /** Врождённая предрасположенность 0.5..1.5 — кому-то даётся легче. */
    UPROPERTY(BlueprintReadWrite, Category = "Skill") float Talent = 1.0f;
    /** Всего часов практики. */
    UPROPERTY(BlueprintReadWrite, Category = "Skill") float PracticeHours = 0.0f;
    UPROPERTY(BlueprintReadWrite, Category = "Skill") float LastPracticedAt = 0.0f;
    /** Уверенность в себе по этому навыку — может быть выше или ниже реального уровня. */
    UPROPERTY(BlueprintReadWrite, Category = "Skill") float SelfEfficacy = 0.3f;
    /** Сколько раз ошибался — из ошибок и растёт мастерство. */
    UPROPERTY(BlueprintReadWrite, Category = "Skill") int32 Failures = 0;
    UPROPERTY(BlueprintReadWrite, Category = "Skill") int32 Successes = 0;
};

/** Привычка: в контексте X почти автоматически делаю Y. */
USTRUCT(BlueprintType)
struct FHabit
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Habit") FName Context;
    UPROPERTY(BlueprintReadWrite, Category = "Habit") EActionType Action = EActionType::Idle;
    /** Сила 0..1 — при высокой человек делает это, не думая. */
    UPROPERTY(BlueprintReadWrite, Category = "Habit") float Strength = 0.0f;
    UPROPERTY(BlueprintReadWrite, Category = "Habit") int32 Repetitions = 0;
    UPROPERTY(BlueprintReadWrite, Category = "Habit") float LastFiredAt = 0.0f;
};

// ===========================================================================
//  СОЦИАЛЬНОЕ
// ===========================================================================

/** Теория разума: моя модель того, что происходит в голове у ДРУГОГО. */
USTRUCT(BlueprintType)
struct FMindModel
{
    GENERATED_BODY()

    /** Как я думаю, он ко мне относится -1..+1. */
    UPROPERTY(BlueprintReadWrite, Category = "ToM") float BelievedLikingOfMe = 0.0f;
    /** Насколько он мне доверяет (по-моему) 0..1. */
    UPROPERTY(BlueprintReadWrite, Category = "ToM") float BelievedTrustInMe = 0.3f;
    /** Как я оцениваю его настроение -1..+1. */
    UPROPERTY(BlueprintReadWrite, Category = "ToM") float BelievedMood = 0.0f;
    /** Что он, по-моему, сейчас хочет. */
    UPROPERTY(BlueprintReadWrite, Category = "ToM") ENeedType BelievedDominantNeed = ENeedType::SocialContact;
    /** Что он, по-моему, знает обо мне (0..1) — база для лжи. */
    UPROPERTY(BlueprintReadWrite, Category = "ToM") float BelievedKnowledgeOfMe = 0.1f;
    /**
     * Второй уровень: как я думаю, ОН представляет себе МОЁ отношение к нему.
     *
     * Это уже не «что он чувствует», а «что он думает, что я чувствую».
     * Отсюда берётся почти всё неловкое в человеческих отношениях:
     * молчание, принятое за обиду; попытка показать, что не сердишься;
     * стыд не от поступка, а от того, каким тебя теперь считают.
     */
    UPROPERTY(BlueprintReadWrite, Category = "ToM") float BelievedViewOfMyOpinion = 0.0f;

    /** Насколько моя модель вообще точна 0..1 (я могу глубоко ошибаться). */
    UPROPERTY(BlueprintReadWrite, Category = "ToM") float ModelAccuracy = 0.3f;
    /** Когда последний раз обновлял модель. */
    UPROPERTY(BlueprintReadWrite, Category = "ToM") float UpdatedAt = 0.0f;
};

/** Отношение к конкретному человеку. */
USTRUCT(BlueprintType)
struct FRelationship
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Social") TObjectPtr<AActor> Other = nullptr;
    UPROPERTY(BlueprintReadWrite, Category = "Social") FString KnownName;
    UPROPERTY(BlueprintReadWrite, Category = "Social") ERelationKind Kind = ERelationKind::Stranger;

    /** Насколько хорошо знаком 0..1. */
    UPROPERTY(BlueprintReadWrite, Category = "Social") float Familiarity = 0.0f;
    /** Симпатия -1..+1. */
    UPROPERTY(BlueprintReadWrite, Category = "Social") float Liking = 0.0f;
    /** Доверие 0..1. */
    UPROPERTY(BlueprintReadWrite, Category = "Social") float Trust = 0.3f;
    /** Уважение 0..1. */
    UPROPERTY(BlueprintReadWrite, Category = "Social") float Respect = 0.3f;
    /** Привязанность 0..1 — по ней измеряется боль потери. */
    UPROPERTY(BlueprintReadWrite, Category = "Social") float Attachment = 0.0f;
    /** Романтический интерес 0..1. */
    UPROPERTY(BlueprintReadWrite, Category = "Social") float Romantic = 0.0f;
    /** Накопленная обида 0..1 — тает медленно. */
    UPROPERTY(BlueprintReadWrite, Category = "Social") float Resentment = 0.0f;
    /** Долг: >0 — я ему должен, <0 — он мне. */
    UPROPERTY(BlueprintReadWrite, Category = "Social") float Debt = 0.0f;
    /** Насколько считаю его «своим» 0..1. */
    UPROPERTY(BlueprintReadWrite, Category = "Social") float InGroup = 0.0f;
    /** Страх перед ним 0..1. */
    UPROPERTY(BlueprintReadWrite, Category = "Social") float Fear = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Social") int32 InteractionCount = 0;
    UPROPERTY(BlueprintReadWrite, Category = "Social") float LastInteractionAt = 0.0f;
    UPROPERTY(BlueprintReadWrite, Category = "Social") float FirstMetAt = 0.0f;
    /** Сколько раз он меня обманул (из тех, что я заметил). */
    UPROPERTY(BlueprintReadWrite, Category = "Social") int32 CaughtLying = 0;

    /** Моя модель его сознания. */
    UPROPERTY(BlueprintReadWrite, Category = "Social") FMindModel Theory;
};

/** Реплика — то, что один человек сказал другому. */
USTRUCT(BlueprintType)
struct FUtterance
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Speech") TObjectPtr<AActor> Speaker = nullptr;
    UPROPERTY(BlueprintReadWrite, Category = "Speech") TObjectPtr<AActor> Listener = nullptr;
    UPROPERTY(BlueprintReadWrite, Category = "Speech") ESpeechAct Act = ESpeechAct::SmallTalk;
    UPROPERTY(BlueprintReadWrite, Category = "Speech") FString Text;
    /** Эмоциональный тон -1..+1. */
    UPROPERTY(BlueprintReadWrite, Category = "Speech") float Tone = 0.0f;
    /** Это правда? (слушатель не знает, но мир — знает) */
    UPROPERTY(BlueprintReadWrite, Category = "Speech") bool bTruthful = true;
    /** Передаваемое убеждение — так распространяются знания и слухи. */
    UPROPERTY(BlueprintReadWrite, Category = "Speech") FBelief Payload;
    UPROPERTY(BlueprintReadWrite, Category = "Speech") bool bHasPayload = false;

    /** Место, о котором рассказывают. Доедет только если слова поняты. */
    UPROPERTY(BlueprintReadWrite, Category = "Speech") FKnownLocation PlacePayload;
    UPROPERTY(BlueprintReadWrite, Category = "Speech") bool bHasPlacePayload = false;

    /** Умение, о котором говорят: «делается вот так». */
    UPROPERTY(BlueprintReadWrite, Category = "Speech") FName SkillPayload;
    UPROPERTY(BlueprintReadWrite, Category = "Speech") float SkillPayloadLevel = 0.0f;

    /** О чём эта реплика. Собеседник отвечает именно на это. */
    UPROPERTY(BlueprintReadWrite, Category = "Speech") FTalkingPoint Point;

    /** Какой это ход разговора — от него зависит, что ответят. */
    UPROPERTY(BlueprintReadWrite, Category = "Speech") EDialogueMove Move = EDialogueMove::None;

    /** Пересказ прочитанного: какая книга и какой параграф. */
    UPROPERTY(BlueprintReadWrite, Category = "Speech") FName BookSubject;
    UPROPERTY(BlueprintReadWrite, Category = "Speech") int32 BookPage = -1;

    /** Деньги, переданные из рук в руки. */
    UPROPERTY(BlueprintReadWrite, Category = "Speech") float MoneyGiven = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Speech") float SpokenAt = 0.0f;

    UPROPERTY() uint8 TalkReply = 255;
    UPROPERTY() uint8 TalkOwn = 255;
};

// ===========================================================================
//  МОТИВАЦИЯ
// ===========================================================================

/** Шаг плана. */
USTRUCT(BlueprintType)
struct FPlanStep
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Plan") EActionType Action = EActionType::Idle;
    UPROPERTY(BlueprintReadWrite, Category = "Plan") FString Label;
    UPROPERTY(BlueprintReadWrite, Category = "Plan") TObjectPtr<AActor> TargetActor = nullptr;
    UPROPERTY(BlueprintReadWrite, Category = "Plan") FVector TargetLocation = FVector::ZeroVector;
    UPROPERTY(BlueprintReadWrite, Category = "Plan") bool bHasLocation = false;
    /** Сколько секунд займёт (0 = мгновенно). */
    UPROPERTY(BlueprintReadWrite, Category = "Plan") float Duration = 0.0f;
    /** Требуемый навык (NAME_None — не нужен). */
    UPROPERTY(BlueprintReadWrite, Category = "Plan") FName RequiredSkill;
    /** Базовая вероятность успеха 0..1 (модифицируется навыком и состоянием). */
    UPROPERTY(BlueprintReadWrite, Category = "Plan") float BaseSuccessChance = 1.0f;
};

/** Цель. Цели вложены: жизненная → среднесрочная → сегодняшняя. */
USTRUCT(BlueprintType)
struct FGoal
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Goal") int32 Id = 0;
    UPROPERTY(BlueprintReadWrite, Category = "Goal") int32 ParentId = -1;
    UPROPERTY(BlueprintReadWrite, Category = "Goal") FString Name;
    UPROPERTY(BlueprintReadWrite, Category = "Goal") EGoalHorizon Horizon = EGoalHorizon::Immediate;
    UPROPERTY(BlueprintReadWrite, Category = "Goal") EGoalStatus Status = EGoalStatus::Pending;
    /** Какую потребность закрывает. */
    UPROPERTY(BlueprintReadWrite, Category = "Goal") ENeedType DrivingNeed = ENeedType::Hunger;
    /** Базовая важность 0..1. */
    UPROPERTY(BlueprintReadWrite, Category = "Goal") float Importance = 0.5f;
    /** Срочность 0..1 — пересчитывается каждый цикл. */
    UPROPERTY(BlueprintReadWrite, Category = "Goal") float Urgency = 0.5f;
    /** Прогресс 0..1. */
    UPROPERTY(BlueprintReadWrite, Category = "Goal") float Progress = 0.0f;
    /** Ожидаемая вероятность успеха 0..1 — влияет на надежду/отчаяние. */
    UPROPERTY(BlueprintReadWrite, Category = "Goal") float Expectancy = 0.5f;
    UPROPERTY(BlueprintReadWrite, Category = "Goal") TObjectPtr<AActor> TargetActor = nullptr;
    UPROPERTY(BlueprintReadWrite, Category = "Goal") FVector TargetLocation = FVector::ZeroVector;
    UPROPERTY(BlueprintReadWrite, Category = "Goal") float CreatedAt = 0.0f;
    /** Дедлайн в секундах мира; <0 — нет. */
    UPROPERTY(BlueprintReadWrite, Category = "Goal") float Deadline = -1.0f;
    /** Сколько раз проваливал — много провалов ведут к выученной беспомощности. */
    UPROPERTY(BlueprintReadWrite, Category = "Goal") int32 Attempts = 0;
    /** Шаги текущего плана. */
    UPROPERTY(BlueprintReadWrite, Category = "Goal") TArray<FPlanStep> Plan;
    UPROPERTY(BlueprintReadWrite, Category = "Goal") int32 CurrentStep = 0;
};

// ===========================================================================
//  СОЗНАНИЕ
// ===========================================================================

/** Мысль в потоке сознания. */
USTRUCT(BlueprintType)
struct FThought
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Mind") FString Text;
    UPROPERTY(BlueprintReadWrite, Category = "Mind") EThoughtKind Kind = EThoughtKind::Observation;
    /** Насколько мысль «громкая» 0..1 — вытесняет остальные. */
    UPROPERTY(BlueprintReadWrite, Category = "Mind") float Salience = 0.5f;
    UPROPERTY(BlueprintReadWrite, Category = "Mind") float Time = 0.0f;
    UPROPERTY(BlueprintReadWrite, Category = "Mind") TObjectPtr<AActor> About = nullptr;
};

/** То, что попало в поле восприятия. */
USTRUCT(BlueprintType)
struct FPercept
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Mind") TObjectPtr<AActor> Actor = nullptr;
    UPROPERTY(BlueprintReadWrite, Category = "Mind") FName Kind;              // "Sight", "Sound", "Touch"
    UPROPERTY(BlueprintReadWrite, Category = "Mind") FVector Location = FVector::ZeroVector;
    /** Насколько это привлекает внимание 0..1. */
    UPROPERTY(BlueprintReadWrite, Category = "Mind") float Salience = 0.5f;
    UPROPERTY(BlueprintReadWrite, Category = "Mind") float Time = 0.0f;
    UPROPERTY(BlueprintReadWrite, Category = "Mind") FString Description;
};

/** Веха биографии — то, что человек рассказал бы о своей жизни. */
USTRUCT(BlueprintType)
struct FLifeEvent
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Identity") FString Text;
    UPROPERTY(BlueprintReadWrite, Category = "Identity") float AtAge = 0.0f;
    UPROPERTY(BlueprintReadWrite, Category = "Identity") float Valence = 0.0f;
    /** Насколько изменило меня 0..1. */
    UPROPERTY(BlueprintReadWrite, Category = "Identity") float Impact = 0.5f;
    UPROPERTY(BlueprintReadWrite, Category = "Identity") float WorldTime = 0.0f;
};

// ===========================================================================
//  ВРЕМЯ МИРА
// ===========================================================================

/** Игровое время. */
USTRUCT(BlueprintType)
struct FHumanTime
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Time") int32 Day = 1;
    UPROPERTY(BlueprintReadWrite, Category = "Time") int32 Hour = 8;
    UPROPERTY(BlueprintReadWrite, Category = "Time") int32 Minute = 0;
    /** 0 = понедельник. */
    UPROPERTY(BlueprintReadWrite, Category = "Time") int32 DayOfWeek = 0;
    /** Дробные часы 0..24 — удобно для циркадных расчётов. */
    UPROPERTY(BlueprintReadWrite, Category = "Time") float HourFloat = 8.0f;
    UPROPERTY(BlueprintReadWrite, Category = "Time") bool bIsNight = false;
    UPROPERTY(BlueprintReadWrite, Category = "Time") bool bIsWeekend = false;
};

// ===========================================================================
//  СЛОВАРЬ: перевод машинных понятий в человеческую речь
// ===========================================================================

namespace HumanText
{
    /** Название эмоции по-русски (именительный падеж). */
    AB_API FString Emotion(EEmotionType Type);
    /** «Мне страшно», «Я злюсь» — от первого лица. */
    AB_API FString EmotionFirstPerson(EEmotionType Type);
    /** Название потребности. */
    AB_API FString Need(ENeedType Type);
    /** «Хочу есть», «Хочу, чтобы меня поняли». */
    AB_API FString NeedDesire(ENeedType Type);
    /** Название действия. */
    AB_API FString Action(EActionType Type);
    /** Название типа отношений. */
    AB_API FString Relation(ERelationKind Kind);
    /** Этап жизни. */
    AB_API FString LifeStage(ELifeStage Stage);
    /** Недомогание. */
    AB_API FString Ailment(EAilment Type);
    /** Тип места. */
    AB_API FString Place(EPlaceKind Kind);

    /** Случайное русское имя (мужское/женское по флагу). */
    AB_API FString RandomFirstName(bool bFemale);
    /** Случайная фамилия. */
    AB_API FString RandomLastName(bool bFemale);

    /** Этап жизни по возрасту. */
    AB_API ELifeStage StageForAge(float Age);
}
