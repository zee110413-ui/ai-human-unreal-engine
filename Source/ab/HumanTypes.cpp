// HumanTypes.cpp
// ---------------------------------------------------------------------------
// Словарь: перевод внутренних состояний в человеческие слова.
// Всё, что NPC «думает» и «говорит», собирается из этих кирпичиков.
// ---------------------------------------------------------------------------

#include "HumanTypes.h"

DEFINE_LOG_CATEGORY(LogHumanCity);

namespace HumanText
{

FString Emotion(EEmotionType Type)
{
    switch (Type)
    {
    case EEmotionType::Joy:            return TEXT("радость");
    case EEmotionType::Sadness:        return TEXT("печаль");
    case EEmotionType::Fear:           return TEXT("страх");
    case EEmotionType::Anger:          return TEXT("гнев");
    case EEmotionType::Disgust:        return TEXT("отвращение");
    case EEmotionType::Surprise:       return TEXT("удивление");
    case EEmotionType::Hope:           return TEXT("надежда");
    case EEmotionType::Anxiety:        return TEXT("тревога");
    case EEmotionType::Relief:         return TEXT("облегчение");
    case EEmotionType::Disappointment: return TEXT("разочарование");
    case EEmotionType::Pride:          return TEXT("гордость");
    case EEmotionType::Shame:          return TEXT("стыд");
    case EEmotionType::Guilt:          return TEXT("вина");
    case EEmotionType::Remorse:        return TEXT("раскаяние");
    case EEmotionType::Satisfaction:   return TEXT("удовлетворение");
    case EEmotionType::Love:           return TEXT("любовь");
    case EEmotionType::Affection:      return TEXT("нежность");
    case EEmotionType::Gratitude:      return TEXT("благодарность");
    case EEmotionType::Admiration:     return TEXT("восхищение");
    case EEmotionType::Compassion:     return TEXT("сострадание");
    case EEmotionType::Envy:           return TEXT("зависть");
    case EEmotionType::Jealousy:       return TEXT("ревность");
    case EEmotionType::Contempt:       return TEXT("презрение");
    case EEmotionType::Resentment:     return TEXT("обида");
    case EEmotionType::Embarrassment:  return TEXT("смущение");
    case EEmotionType::Loneliness:     return TEXT("одиночество");
    case EEmotionType::Trust:          return TEXT("доверие");
    case EEmotionType::Curiosity:      return TEXT("любопытство");
    case EEmotionType::Boredom:        return TEXT("скука");
    case EEmotionType::Frustration:    return TEXT("досада");
    case EEmotionType::Nostalgia:      return TEXT("ностальгия");
    case EEmotionType::Serenity:       return TEXT("умиротворение");
    case EEmotionType::Awe:            return TEXT("благоговение");
    default:                           return TEXT("ничего");
    }
}

FString EmotionFirstPerson(EEmotionType Type)
{
    switch (Type)
    {
    case EEmotionType::Joy:            return TEXT("мне хорошо");
    case EEmotionType::Sadness:        return TEXT("мне грустно");
    case EEmotionType::Fear:           return TEXT("мне страшно");
    case EEmotionType::Anger:          return TEXT("я злюсь");
    case EEmotionType::Disgust:        return TEXT("меня мутит от этого");
    case EEmotionType::Surprise:       return TEXT("я не ожидал");
    case EEmotionType::Hope:           return TEXT("я надеюсь");
    case EEmotionType::Anxiety:        return TEXT("мне тревожно");
    case EEmotionType::Relief:         return TEXT("отлегло");
    case EEmotionType::Disappointment: return TEXT("я разочарован");
    case EEmotionType::Pride:          return TEXT("я горжусь собой");
    case EEmotionType::Shame:          return TEXT("мне стыдно");
    case EEmotionType::Guilt:          return TEXT("я виноват");
    case EEmotionType::Remorse:        return TEXT("зря я так");
    case EEmotionType::Satisfaction:   return TEXT("я доволен");
    case EEmotionType::Love:           return TEXT("я люблю");
    case EEmotionType::Affection:      return TEXT("мне тепло рядом");
    case EEmotionType::Gratitude:      return TEXT("я благодарен");
    case EEmotionType::Admiration:     return TEXT("я восхищён");
    case EEmotionType::Compassion:     return TEXT("мне его жаль");
    case EEmotionType::Envy:           return TEXT("я завидую");
    case EEmotionType::Jealousy:       return TEXT("я ревную");
    case EEmotionType::Contempt:       return TEXT("я его презираю");
    case EEmotionType::Resentment:     return TEXT("я обижен");
    case EEmotionType::Embarrassment:  return TEXT("мне неловко");
    case EEmotionType::Loneliness:     return TEXT("мне одиноко");
    case EEmotionType::Trust:          return TEXT("я доверяю");
    case EEmotionType::Curiosity:      return TEXT("мне интересно");
    case EEmotionType::Boredom:        return TEXT("мне скучно");
    case EEmotionType::Frustration:    return TEXT("меня это бесит");
    case EEmotionType::Nostalgia:      return TEXT("я вспоминаю былое");
    case EEmotionType::Serenity:       return TEXT("мне спокойно");
    case EEmotionType::Awe:            return TEXT("я замер");
    default:                           return TEXT("я ничего не чувствую");
    }
}

FString Need(ENeedType Type)
{
    switch (Type)
    {
    case ENeedType::Hunger:        return TEXT("голод");
    case ENeedType::Thirst:        return TEXT("жажда");
    case ENeedType::Sleep:         return TEXT("сон");
    case ENeedType::Bladder:       return TEXT("нужда");
    case ENeedType::Hygiene:       return TEXT("чистота");
    case ENeedType::Comfort:       return TEXT("комфорт");
    case ENeedType::Safety:        return TEXT("безопасность");
    case ENeedType::Health:        return TEXT("здоровье");
    case ENeedType::Shelter:       return TEXT("кров");
    case ENeedType::Money:         return TEXT("деньги");
    case ENeedType::Order:         return TEXT("порядок");
    case ENeedType::SocialContact: return TEXT("общение");
    case ENeedType::Belonging:     return TEXT("принадлежность");
    case ENeedType::Intimacy:      return TEXT("близость");
    case ENeedType::Esteem:        return TEXT("уважение");
    case ENeedType::Achievement:   return TEXT("достижения");
    case ENeedType::Autonomy:      return TEXT("свобода");
    case ENeedType::Competence:    return TEXT("мастерство");
    case ENeedType::Novelty:       return TEXT("новизна");
    case ENeedType::Beauty:        return TEXT("красота");
    case ENeedType::Meaning:       return TEXT("смысл");
    default:                       return TEXT("нечто");
    }
}

FString NeedDesire(ENeedType Type)
{
    switch (Type)
    {
    case ENeedType::Hunger:        return TEXT("хочу есть");
    case ENeedType::Thirst:        return TEXT("хочу пить");
    case ENeedType::Sleep:         return TEXT("хочу спать");
    case ENeedType::Bladder:       return TEXT("надо в туалет");
    case ENeedType::Hygiene:       return TEXT("надо бы помыться");
    case ENeedType::Comfort:       return TEXT("хочу, чтобы перестало болеть");
    case ENeedType::Safety:        return TEXT("хочу оказаться в безопасности");
    case ENeedType::Health:        return TEXT("хочу поправиться");
    case ENeedType::Shelter:       return TEXT("хочу домой");
    case ENeedType::Money:         return TEXT("нужны деньги");
    case ENeedType::Order:         return TEXT("хочу, чтобы всё было понятно");
    case ENeedType::SocialContact: return TEXT("хочу с кем-нибудь поговорить");
    case ENeedType::Belonging:     return TEXT("хочу быть среди своих");
    case ENeedType::Intimacy:      return TEXT("хочу, чтобы меня кто-то понимал");
    case ENeedType::Esteem:        return TEXT("хочу, чтобы меня ценили");
    case ENeedType::Achievement:   return TEXT("хочу чего-то добиться");
    case ENeedType::Autonomy:      return TEXT("хочу решать сам");
    case ENeedType::Competence:    return TEXT("хочу научиться делать это хорошо");
    case ENeedType::Novelty:       return TEXT("хочу чего-то нового");
    case ENeedType::Beauty:        return TEXT("хочу увидеть что-нибудь красивое");
    case ENeedType::Meaning:       return TEXT("хочу понять, зачем всё это");
    default:                       return TEXT("чего-то хочу");
    }
}

FString Action(EActionType Type)
{
    switch (Type)
    {
    case EActionType::Idle:      return TEXT("стоять");
    case EActionType::Wander:    return TEXT("бродить");
    case EActionType::MoveTo:    return TEXT("идти");
    case EActionType::GoHome:    return TEXT("идти домой");
    case EActionType::Wait:      return TEXT("ждать");
    case EActionType::Eat:       return TEXT("есть");
    case EActionType::Drink:     return TEXT("пить");
    case EActionType::Cook:      return TEXT("готовить");
    case EActionType::Sleep:     return TEXT("спать");
    case EActionType::Rest:      return TEXT("отдыхать");
    case EActionType::UseToilet: return TEXT("в туалет");
    case EActionType::Wash:      return TEXT("мыться");
    case EActionType::Work:      return TEXT("работать");
    case EActionType::Study:     return TEXT("учиться");
    case EActionType::Read:      return TEXT("читать");
    case EActionType::Practice:  return TEXT("тренироваться");
    case EActionType::Entertain: return TEXT("развлекаться");
    case EActionType::Exercise:  return TEXT("заниматься спортом");
    case EActionType::Approach:  return TEXT("подойти");
    case EActionType::Talk:      return TEXT("говорить");
    case EActionType::Listen:    return TEXT("слушать");
    case EActionType::Help:      return TEXT("помогать");
    case EActionType::Comfort:   return TEXT("утешать");
    case EActionType::Apologize: return TEXT("извиняться");
    case EActionType::Confront:  return TEXT("выяснять отношения");
    case EActionType::Avoid:     return TEXT("избегать");
    case EActionType::Follow:    return TEXT("идти следом");
    case EActionType::Observe:   return TEXT("наблюдать");
    case EActionType::Flee:      return TEXT("убегать");
    case EActionType::Hide:      return TEXT("прятаться");
    case EActionType::Fight:     return TEXT("драться");
    case EActionType::Freeze:    return TEXT("оцепенеть");
    case EActionType::Reflect:   return TEXT("размышлять");
    case EActionType::Reminisce: return TEXT("вспоминать");
    case EActionType::Mourn:     return TEXT("горевать");
    case EActionType::Celebrate: return TEXT("радоваться");
    case EActionType::Explore:   return TEXT("исследовать");
    default:                     return TEXT("ничего");
    }
}

FString Relation(ERelationKind Kind)
{
    switch (Kind)
    {
    case ERelationKind::Stranger:     return TEXT("незнакомец");
    case ERelationKind::Acquaintance: return TEXT("знакомый");
    case ERelationKind::Colleague:    return TEXT("коллега");
    case ERelationKind::Friend:       return TEXT("друг");
    case ERelationKind::CloseFriend:  return TEXT("близкий друг");
    case ERelationKind::Partner:      return TEXT("любимый человек");
    case ERelationKind::Family:       return TEXT("родня");
    case ERelationKind::Mentor:       return TEXT("наставник");
    case ERelationKind::Rival:        return TEXT("соперник");
    case ERelationKind::Enemy:        return TEXT("враг");
    default:                          return TEXT("кто-то");
    }
}

FString LifeStage(ELifeStage Stage)
{
    switch (Stage)
    {
    case ELifeStage::Child:      return TEXT("ребёнок");
    case ELifeStage::Adolescent: return TEXT("подросток");
    case ELifeStage::YoungAdult: return TEXT("молодой");
    case ELifeStage::Adult:      return TEXT("взрослый");
    case ELifeStage::MiddleAge:  return TEXT("в зрелых годах");
    case ELifeStage::Senior:     return TEXT("пожилой");
    case ELifeStage::Elder:      return TEXT("старик");
    default:                     return TEXT("человек");
    }
}

FString Ailment(EAilment Type)
{
    switch (Type)
    {
    case EAilment::Cold:            return TEXT("простуда");
    case EAilment::Fever:           return TEXT("жар");
    case EAilment::Injury:          return TEXT("травма");
    case EAilment::Headache:        return TEXT("головная боль");
    case EAilment::Exhaustion:      return TEXT("истощение");
    case EAilment::Burnout:         return TEXT("выгорание");
    case EAilment::Depression:      return TEXT("подавленность");
    case EAilment::AnxietyDisorder: return TEXT("тревожность");
    default:                        return TEXT("здоров");
    }
}

FString Place(EPlaceKind Kind)
{
    switch (Kind)
    {
    case EPlaceKind::Home:      return TEXT("дом");
    case EPlaceKind::Work:      return TEXT("работа");
    case EPlaceKind::Food:      return TEXT("где поесть");
    case EPlaceKind::Shop:      return TEXT("магазин");
    case EPlaceKind::Social:    return TEXT("людное место");
    case EPlaceKind::Rest:      return TEXT("место отдыха");
    case EPlaceKind::Danger:    return TEXT("опасное место");
    case EPlaceKind::Beautiful: return TEXT("красивое место");
    case EPlaceKind::Landmark:  return TEXT("приметное место");
    case EPlaceKind::Study:     return TEXT("школа");
    case EPlaceKind::Hospital:  return TEXT("больница");
    case EPlaceKind::Workshop:  return TEXT("мастерская");
    case EPlaceKind::Library:   return TEXT("библиотека");
    case EPlaceKind::Market:    return TEXT("торг");
    case EPlaceKind::Bathhouse: return TEXT("баня");
    case EPlaceKind::Bakery:    return TEXT("пекарня");
    case EPlaceKind::TownHall:  return TEXT("управа");
    case EPlaceKind::Field:     return TEXT("поле");
    case EPlaceKind::Forest:    return TEXT("лес");
    case EPlaceKind::River:     return TEXT("река");
    case EPlaceKind::Church:    return TEXT("церковь");
    case EPlaceKind::Castle:    return TEXT("замок");
    case EPlaceKind::Forge:     return TEXT("кузница");
    case EPlaceKind::Mill:      return TEXT("мельница");
    default:                    return TEXT("незнакомое место");
    }
}

// --- Имена -----------------------------------------------------------------

static const TCHAR* MaleNames[] = {
    TEXT("Игорь"), TEXT("Андрей"), TEXT("Сергей"), TEXT("Дмитрий"), TEXT("Алексей"),
    TEXT("Михаил"), TEXT("Николай"), TEXT("Павел"), TEXT("Артём"), TEXT("Виктор"),
    TEXT("Роман"), TEXT("Егор"), TEXT("Кирилл"), TEXT("Максим"), TEXT("Тимофей"),
    TEXT("Борис"), TEXT("Лев"), TEXT("Глеб"), TEXT("Степан"), TEXT("Юрий")
};

static const TCHAR* FemaleNames[] = {
    TEXT("Анна"), TEXT("Мария"), TEXT("Ольга"), TEXT("Елена"), TEXT("Ирина"),
    TEXT("Наталья"), TEXT("Светлана"), TEXT("Вера"), TEXT("Дарья"), TEXT("Полина"),
    TEXT("Ксения"), TEXT("Алиса"), TEXT("Татьяна"), TEXT("Людмила"), TEXT("Софья"),
    TEXT("Нина"), TEXT("Валентина"), TEXT("Юлия"), TEXT("Марина"), TEXT("Зоя")
};

// Фамилии в мужской форме; женская получается добавлением «а».
static const TCHAR* LastNameStems[] = {
    TEXT("Соколов"), TEXT("Морозов"), TEXT("Волков"), TEXT("Зайцев"), TEXT("Лебедев"),
    TEXT("Кузнецов"), TEXT("Смирнов"), TEXT("Попов"), TEXT("Новиков"), TEXT("Фёдоров"),
    TEXT("Медведев"), TEXT("Орлов"), TEXT("Журавлёв"), TEXT("Беляев"), TEXT("Гусев"),
    TEXT("Тихонов"), TEXT("Крылов"), TEXT("Ершов"), TEXT("Мельников"), TEXT("Северов")
};

FString RandomFirstName(bool bFemale)
{
    if (bFemale)
    {
        const int32 Count = UE_ARRAY_COUNT(FemaleNames);
        return FString(FemaleNames[FMath::RandRange(0, Count - 1)]);
    }
    const int32 Count = UE_ARRAY_COUNT(MaleNames);
    return FString(MaleNames[FMath::RandRange(0, Count - 1)]);
}

FString RandomLastName(bool bFemale)
{
    const int32 Count = UE_ARRAY_COUNT(LastNameStems);
    FString Stem = FString(LastNameStems[FMath::RandRange(0, Count - 1)]);
    return bFemale ? Stem + TEXT("а") : Stem;
}

ELifeStage StageForAge(float Age)
{
    if (Age < 13.0f) return ELifeStage::Child;
    if (Age < 20.0f) return ELifeStage::Adolescent;
    if (Age < 35.0f) return ELifeStage::YoungAdult;
    if (Age < 50.0f) return ELifeStage::Adult;
    if (Age < 65.0f) return ELifeStage::MiddleAge;
    if (Age < 80.0f) return ELifeStage::Senior;
    return ELifeStage::Elder;
}

} // namespace HumanText
