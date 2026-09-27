#include "Elements.h"

namespace
{
    FElement E(int32 Number, const TCHAR* Symbol, const TCHAR* Name, float Mass, float Density,
               float Melting, float Boiling, EMatterState State, EElementKind Kind,
               float Crust, const TCHAR* FoundIn)
    {
        FElement Out;
        Out.Number = Number;
        Out.Symbol = Symbol;
        Out.Name = Name;
        Out.Mass = Mass;
        Out.Density = Density;
        Out.Melting = Melting;
        Out.Boiling = Boiling;
        Out.State = State;
        Out.Kind = Kind;
        Out.Crust = Crust;
        Out.FoundIn = FoundIn;
        return Out;
    }

    FMineral M(EResourceKind Kind, const TCHAR* Name, std::initializer_list<const TCHAR*> Of,
               float Density, float Hardness, float Melting, bool bBurns, const TCHAR* Ground, float Common)
    {
        FMineral Out;
        Out.Kind = Kind;
        Out.Name = Name;
        for (const TCHAR* Symbol : Of)
        {
            Out.Elements.Add(Symbol);
        }
        Out.Density = Density;
        Out.Hardness = Hardness;
        Out.Melting = Melting;
        Out.bBurns = bBurns;
        Out.Ground = Ground;
        Out.Common = Common;
        return Out;
    }

    FWorldLaw L(const TCHAR* Name, const TCHAR* Statement, float Value, const TCHAR* Unit)
    {
        FWorldLaw Out;
        Out.Name = Name;
        Out.Statement = Statement;
        Out.Value = Value;
        Out.Unit = Unit;
        return Out;
    }
}

const TArray<FElement>& FElements::All()
{
    static TArray<FElement> Table;
    if (Table.Num() > 0)
    {
        return Table;
    }

    Table.Add(E(1, TEXT("H"), TEXT("водород"), 1.008f, 0.00009f, -259.0f, -253.0f, EMatterState::Gas, EElementKind::Nonmetal, 1400.0f, TEXT("вода, воздух")));
    Table.Add(E(2, TEXT("He"), TEXT("гелий"), 4.003f, 0.00018f, -272.0f, -269.0f, EMatterState::Gas, EElementKind::NobleGas, 0.008f, TEXT("воздух, подземный газ")));
    Table.Add(E(3, TEXT("Li"), TEXT("литий"), 6.94f, 0.53f, 181.0f, 1342.0f, EMatterState::Solid, EElementKind::Metal, 20.0f, TEXT("камень")));
    Table.Add(E(4, TEXT("Be"), TEXT("бериллий"), 9.012f, 1.85f, 1287.0f, 2469.0f, EMatterState::Solid, EElementKind::Metal, 2.8f, TEXT("камень")));
    Table.Add(E(5, TEXT("B"), TEXT("бор"), 10.81f, 2.34f, 2076.0f, 3927.0f, EMatterState::Solid, EElementKind::Semimetal, 10.0f, TEXT("соль, камень")));
    Table.Add(E(6, TEXT("C"), TEXT("углерод"), 12.011f, 2.27f, 3550.0f, 4027.0f, EMatterState::Solid, EElementKind::Nonmetal, 200.0f, TEXT("уголь, дерево, всё живое")));
    Table.Add(E(7, TEXT("N"), TEXT("азот"), 14.007f, 0.00125f, -210.0f, -196.0f, EMatterState::Gas, EElementKind::Nonmetal, 19.0f, TEXT("воздух, селитра")));
    Table.Add(E(8, TEXT("O"), TEXT("кислород"), 15.999f, 0.00143f, -218.0f, -183.0f, EMatterState::Gas, EElementKind::Nonmetal, 461000.0f, TEXT("воздух, вода, камень")));
    Table.Add(E(9, TEXT("F"), TEXT("фтор"), 18.998f, 0.0017f, -220.0f, -188.0f, EMatterState::Gas, EElementKind::Halogen, 585.0f, TEXT("камень")));
    Table.Add(E(10, TEXT("Ne"), TEXT("неон"), 20.18f, 0.0009f, -249.0f, -246.0f, EMatterState::Gas, EElementKind::NobleGas, 0.005f, TEXT("воздух")));
    Table.Add(E(11, TEXT("Na"), TEXT("натрий"), 22.99f, 0.97f, 98.0f, 883.0f, EMatterState::Solid, EElementKind::Metal, 23600.0f, TEXT("соль")));
    Table.Add(E(12, TEXT("Mg"), TEXT("магний"), 24.305f, 1.74f, 650.0f, 1090.0f, EMatterState::Solid, EElementKind::Metal, 23300.0f, TEXT("камень, морская вода")));
    Table.Add(E(13, TEXT("Al"), TEXT("алюминий"), 26.982f, 2.7f, 660.0f, 2519.0f, EMatterState::Solid, EElementKind::Metal, 82300.0f, TEXT("глина")));
    Table.Add(E(14, TEXT("Si"), TEXT("кремний"), 28.085f, 2.33f, 1414.0f, 3265.0f, EMatterState::Solid, EElementKind::Semimetal, 282000.0f, TEXT("песок, кварц, кремень")));
    Table.Add(E(15, TEXT("P"), TEXT("фосфор"), 30.974f, 1.82f, 44.0f, 280.0f, EMatterState::Solid, EElementKind::Nonmetal, 1050.0f, TEXT("кость, камень")));
    Table.Add(E(16, TEXT("S"), TEXT("сера"), 32.06f, 2.07f, 115.0f, 445.0f, EMatterState::Solid, EElementKind::Nonmetal, 350.0f, TEXT("сера у горячих ключей")));
    Table.Add(E(17, TEXT("Cl"), TEXT("хлор"), 35.45f, 0.0032f, -102.0f, -34.0f, EMatterState::Gas, EElementKind::Halogen, 145.0f, TEXT("соль")));
    Table.Add(E(18, TEXT("Ar"), TEXT("аргон"), 39.948f, 0.0018f, -189.0f, -186.0f, EMatterState::Gas, EElementKind::NobleGas, 3.5f, TEXT("воздух")));
    Table.Add(E(19, TEXT("K"), TEXT("калий"), 39.098f, 0.86f, 64.0f, 759.0f, EMatterState::Solid, EElementKind::Metal, 20900.0f, TEXT("зола, камень")));
    Table.Add(E(20, TEXT("Ca"), TEXT("кальций"), 40.078f, 1.55f, 842.0f, 1484.0f, EMatterState::Solid, EElementKind::Metal, 41500.0f, TEXT("известняк, мел, кость")));
    Table.Add(E(21, TEXT("Sc"), TEXT("скандий"), 44.956f, 2.99f, 1541.0f, 2836.0f, EMatterState::Solid, EElementKind::Metal, 22.0f, TEXT("камень")));
    Table.Add(E(22, TEXT("Ti"), TEXT("титан"), 47.867f, 4.51f, 1668.0f, 3287.0f, EMatterState::Solid, EElementKind::Metal, 5650.0f, TEXT("песок, руда")));
    Table.Add(E(23, TEXT("V"), TEXT("ванадий"), 50.942f, 6.0f, 1910.0f, 3407.0f, EMatterState::Solid, EElementKind::Metal, 120.0f, TEXT("руда")));
    Table.Add(E(24, TEXT("Cr"), TEXT("хром"), 51.996f, 7.15f, 1907.0f, 2671.0f, EMatterState::Solid, EElementKind::Metal, 102.0f, TEXT("руда")));
    Table.Add(E(25, TEXT("Mn"), TEXT("марганец"), 54.938f, 7.3f, 1246.0f, 2061.0f, EMatterState::Solid, EElementKind::Metal, 950.0f, TEXT("руда")));
    Table.Add(E(26, TEXT("Fe"), TEXT("железо"), 55.845f, 7.87f, 1538.0f, 2862.0f, EMatterState::Solid, EElementKind::Metal, 56300.0f, TEXT("руда")));
    Table.Add(E(27, TEXT("Co"), TEXT("кобальт"), 58.933f, 8.9f, 1495.0f, 2927.0f, EMatterState::Solid, EElementKind::Metal, 25.0f, TEXT("руда")));
    Table.Add(E(28, TEXT("Ni"), TEXT("никель"), 58.693f, 8.91f, 1455.0f, 2913.0f, EMatterState::Solid, EElementKind::Metal, 84.0f, TEXT("руда")));
    Table.Add(E(29, TEXT("Cu"), TEXT("медь"), 63.546f, 8.96f, 1085.0f, 2562.0f, EMatterState::Solid, EElementKind::Metal, 60.0f, TEXT("медная руда")));
    Table.Add(E(30, TEXT("Zn"), TEXT("цинк"), 65.38f, 7.13f, 420.0f, 907.0f, EMatterState::Solid, EElementKind::Metal, 70.0f, TEXT("руда")));
    Table.Add(E(31, TEXT("Ga"), TEXT("галлий"), 69.723f, 5.91f, 30.0f, 2204.0f, EMatterState::Solid, EElementKind::Metal, 19.0f, TEXT("руда")));
    Table.Add(E(32, TEXT("Ge"), TEXT("германий"), 72.63f, 5.32f, 938.0f, 2833.0f, EMatterState::Solid, EElementKind::Semimetal, 1.5f, TEXT("руда")));
    Table.Add(E(33, TEXT("As"), TEXT("мышьяк"), 74.922f, 5.73f, 817.0f, 614.0f, EMatterState::Solid, EElementKind::Semimetal, 1.8f, TEXT("руда")));
    Table.Add(E(34, TEXT("Se"), TEXT("селен"), 78.971f, 4.81f, 221.0f, 685.0f, EMatterState::Solid, EElementKind::Nonmetal, 0.05f, TEXT("руда")));
    Table.Add(E(35, TEXT("Br"), TEXT("бром"), 79.904f, 3.12f, -7.0f, 59.0f, EMatterState::Liquid, EElementKind::Halogen, 2.4f, TEXT("соль, морская вода")));
    Table.Add(E(36, TEXT("Kr"), TEXT("криптон"), 83.798f, 0.0037f, -157.0f, -153.0f, EMatterState::Gas, EElementKind::NobleGas, 0.0001f, TEXT("воздух")));
    Table.Add(E(37, TEXT("Rb"), TEXT("рубидий"), 85.468f, 1.53f, 39.0f, 688.0f, EMatterState::Solid, EElementKind::Metal, 90.0f, TEXT("камень")));
    Table.Add(E(38, TEXT("Sr"), TEXT("стронций"), 87.62f, 2.64f, 777.0f, 1382.0f, EMatterState::Solid, EElementKind::Metal, 370.0f, TEXT("камень")));
    Table.Add(E(39, TEXT("Y"), TEXT("иттрий"), 88.906f, 4.47f, 1526.0f, 3336.0f, EMatterState::Solid, EElementKind::Metal, 33.0f, TEXT("камень")));
    Table.Add(E(40, TEXT("Zr"), TEXT("цирконий"), 91.224f, 6.51f, 1855.0f, 4409.0f, EMatterState::Solid, EElementKind::Metal, 165.0f, TEXT("песок")));
    Table.Add(E(41, TEXT("Nb"), TEXT("ниобий"), 92.906f, 8.57f, 2477.0f, 4744.0f, EMatterState::Solid, EElementKind::Metal, 20.0f, TEXT("руда")));
    Table.Add(E(42, TEXT("Mo"), TEXT("молибден"), 95.95f, 10.22f, 2623.0f, 4639.0f, EMatterState::Solid, EElementKind::Metal, 1.2f, TEXT("руда")));
    Table.Add(E(43, TEXT("Tc"), TEXT("технеций"), 98.0f, 11.5f, 2157.0f, 4265.0f, EMatterState::Solid, EElementKind::Metal, 0.0f, TEXT("не встречается в земле")));
    Table.Add(E(44, TEXT("Ru"), TEXT("рутений"), 101.07f, 12.37f, 2334.0f, 4150.0f, EMatterState::Solid, EElementKind::Metal, 0.001f, TEXT("руда")));
    Table.Add(E(45, TEXT("Rh"), TEXT("родий"), 102.906f, 12.41f, 1964.0f, 3695.0f, EMatterState::Solid, EElementKind::Metal, 0.001f, TEXT("руда")));
    Table.Add(E(46, TEXT("Pd"), TEXT("палладий"), 106.42f, 12.02f, 1555.0f, 2963.0f, EMatterState::Solid, EElementKind::Metal, 0.015f, TEXT("руда")));
    Table.Add(E(47, TEXT("Ag"), TEXT("серебро"), 107.868f, 10.49f, 962.0f, 2162.0f, EMatterState::Solid, EElementKind::Metal, 0.075f, TEXT("серебряная руда")));
    Table.Add(E(48, TEXT("Cd"), TEXT("кадмий"), 112.414f, 8.69f, 321.0f, 767.0f, EMatterState::Solid, EElementKind::Metal, 0.15f, TEXT("руда")));
    Table.Add(E(49, TEXT("In"), TEXT("индий"), 114.818f, 7.31f, 157.0f, 2072.0f, EMatterState::Solid, EElementKind::Metal, 0.25f, TEXT("руда")));
    Table.Add(E(50, TEXT("Sn"), TEXT("олово"), 118.71f, 7.29f, 232.0f, 2602.0f, EMatterState::Solid, EElementKind::Metal, 2.3f, TEXT("оловянная руда")));
    Table.Add(E(51, TEXT("Sb"), TEXT("сурьма"), 121.76f, 6.69f, 631.0f, 1587.0f, EMatterState::Solid, EElementKind::Semimetal, 0.2f, TEXT("руда")));
    Table.Add(E(52, TEXT("Te"), TEXT("теллур"), 127.6f, 6.23f, 450.0f, 988.0f, EMatterState::Solid, EElementKind::Semimetal, 0.001f, TEXT("руда")));
    Table.Add(E(53, TEXT("I"), TEXT("йод"), 126.904f, 4.93f, 114.0f, 184.0f, EMatterState::Solid, EElementKind::Halogen, 0.45f, TEXT("морская вода, водоросли")));
    Table.Add(E(54, TEXT("Xe"), TEXT("ксенон"), 131.293f, 0.0059f, -112.0f, -108.0f, EMatterState::Gas, EElementKind::NobleGas, 0.00003f, TEXT("воздух")));
    Table.Add(E(55, TEXT("Cs"), TEXT("цезий"), 132.905f, 1.93f, 28.0f, 671.0f, EMatterState::Solid, EElementKind::Metal, 3.0f, TEXT("камень")));
    Table.Add(E(56, TEXT("Ba"), TEXT("барий"), 137.327f, 3.59f, 727.0f, 1897.0f, EMatterState::Solid, EElementKind::Metal, 425.0f, TEXT("камень")));
    Table.Add(E(57, TEXT("La"), TEXT("лантан"), 138.905f, 6.15f, 920.0f, 3464.0f, EMatterState::Solid, EElementKind::Lanthanide, 39.0f, TEXT("редкая руда")));
    Table.Add(E(58, TEXT("Ce"), TEXT("церий"), 140.116f, 6.77f, 795.0f, 3443.0f, EMatterState::Solid, EElementKind::Lanthanide, 66.5f, TEXT("редкая руда")));
    Table.Add(E(59, TEXT("Pr"), TEXT("празеодим"), 140.908f, 6.77f, 935.0f, 3520.0f, EMatterState::Solid, EElementKind::Lanthanide, 9.2f, TEXT("редкая руда")));
    Table.Add(E(60, TEXT("Nd"), TEXT("неодим"), 144.242f, 7.01f, 1024.0f, 3074.0f, EMatterState::Solid, EElementKind::Lanthanide, 41.5f, TEXT("редкая руда")));
    Table.Add(E(61, TEXT("Pm"), TEXT("прометий"), 145.0f, 7.26f, 1042.0f, 3000.0f, EMatterState::Solid, EElementKind::Lanthanide, 0.0f, TEXT("не встречается в земле")));
    Table.Add(E(62, TEXT("Sm"), TEXT("самарий"), 150.36f, 7.52f, 1072.0f, 1794.0f, EMatterState::Solid, EElementKind::Lanthanide, 7.05f, TEXT("редкая руда")));
    Table.Add(E(63, TEXT("Eu"), TEXT("европий"), 151.964f, 5.24f, 826.0f, 1529.0f, EMatterState::Solid, EElementKind::Lanthanide, 2.0f, TEXT("редкая руда")));
    Table.Add(E(64, TEXT("Gd"), TEXT("гадолиний"), 157.25f, 7.9f, 1312.0f, 3273.0f, EMatterState::Solid, EElementKind::Lanthanide, 6.2f, TEXT("редкая руда")));
    Table.Add(E(65, TEXT("Tb"), TEXT("тербий"), 158.925f, 8.23f, 1356.0f, 3230.0f, EMatterState::Solid, EElementKind::Lanthanide, 1.2f, TEXT("редкая руда")));
    Table.Add(E(66, TEXT("Dy"), TEXT("диспрозий"), 162.5f, 8.55f, 1407.0f, 2567.0f, EMatterState::Solid, EElementKind::Lanthanide, 5.2f, TEXT("редкая руда")));
    Table.Add(E(67, TEXT("Ho"), TEXT("гольмий"), 164.93f, 8.8f, 1461.0f, 2700.0f, EMatterState::Solid, EElementKind::Lanthanide, 1.3f, TEXT("редкая руда")));
    Table.Add(E(68, TEXT("Er"), TEXT("эрбий"), 167.259f, 9.07f, 1529.0f, 2868.0f, EMatterState::Solid, EElementKind::Lanthanide, 3.5f, TEXT("редкая руда")));
    Table.Add(E(69, TEXT("Tm"), TEXT("тулий"), 168.934f, 9.32f, 1545.0f, 1950.0f, EMatterState::Solid, EElementKind::Lanthanide, 0.52f, TEXT("редкая руда")));
    Table.Add(E(70, TEXT("Yb"), TEXT("иттербий"), 173.045f, 6.9f, 824.0f, 1196.0f, EMatterState::Solid, EElementKind::Lanthanide, 3.2f, TEXT("редкая руда")));
    Table.Add(E(71, TEXT("Lu"), TEXT("лютеций"), 174.967f, 9.84f, 1652.0f, 3402.0f, EMatterState::Solid, EElementKind::Lanthanide, 0.8f, TEXT("редкая руда")));
    Table.Add(E(72, TEXT("Hf"), TEXT("гафний"), 178.49f, 13.31f, 2233.0f, 4603.0f, EMatterState::Solid, EElementKind::Metal, 3.0f, TEXT("песок")));
    Table.Add(E(73, TEXT("Ta"), TEXT("тантал"), 180.948f, 16.65f, 3017.0f, 5458.0f, EMatterState::Solid, EElementKind::Metal, 2.0f, TEXT("руда")));
    Table.Add(E(74, TEXT("W"), TEXT("вольфрам"), 183.84f, 19.25f, 3422.0f, 5555.0f, EMatterState::Solid, EElementKind::Metal, 1.25f, TEXT("руда")));
    Table.Add(E(75, TEXT("Re"), TEXT("рений"), 186.207f, 21.02f, 3186.0f, 5596.0f, EMatterState::Solid, EElementKind::Metal, 0.0007f, TEXT("руда")));
    Table.Add(E(76, TEXT("Os"), TEXT("осмий"), 190.23f, 22.59f, 3033.0f, 5012.0f, EMatterState::Solid, EElementKind::Metal, 0.0015f, TEXT("руда")));
    Table.Add(E(77, TEXT("Ir"), TEXT("иридий"), 192.217f, 22.56f, 2446.0f, 4428.0f, EMatterState::Solid, EElementKind::Metal, 0.001f, TEXT("руда")));
    Table.Add(E(78, TEXT("Pt"), TEXT("платина"), 195.084f, 21.45f, 1768.0f, 3825.0f, EMatterState::Solid, EElementKind::Metal, 0.005f, TEXT("руда, речной песок")));
    Table.Add(E(79, TEXT("Au"), TEXT("золото"), 196.967f, 19.32f, 1064.0f, 2856.0f, EMatterState::Solid, EElementKind::Metal, 0.004f, TEXT("золотая руда, речной песок")));
    Table.Add(E(80, TEXT("Hg"), TEXT("ртуть"), 200.592f, 13.53f, -39.0f, 357.0f, EMatterState::Liquid, EElementKind::Metal, 0.085f, TEXT("киноварь")));
    Table.Add(E(81, TEXT("Tl"), TEXT("таллий"), 204.38f, 11.85f, 304.0f, 1473.0f, EMatterState::Solid, EElementKind::Metal, 0.85f, TEXT("руда")));
    Table.Add(E(82, TEXT("Pb"), TEXT("свинец"), 207.2f, 11.34f, 327.0f, 1749.0f, EMatterState::Solid, EElementKind::Metal, 14.0f, TEXT("свинцовая руда")));
    Table.Add(E(83, TEXT("Bi"), TEXT("висмут"), 208.98f, 9.78f, 271.0f, 1564.0f, EMatterState::Solid, EElementKind::Metal, 0.009f, TEXT("руда")));
    Table.Add(E(84, TEXT("Po"), TEXT("полоний"), 209.0f, 9.2f, 254.0f, 962.0f, EMatterState::Solid, EElementKind::Semimetal, 0.0f, TEXT("следы в руде")));
    Table.Add(E(85, TEXT("At"), TEXT("астат"), 210.0f, 7.0f, 302.0f, 337.0f, EMatterState::Solid, EElementKind::Halogen, 0.0f, TEXT("следы в руде")));
    Table.Add(E(86, TEXT("Rn"), TEXT("радон"), 222.0f, 0.0097f, -71.0f, -62.0f, EMatterState::Gas, EElementKind::NobleGas, 0.0f, TEXT("подземный воздух")));
    Table.Add(E(87, TEXT("Fr"), TEXT("франций"), 223.0f, 1.87f, 27.0f, 677.0f, EMatterState::Solid, EElementKind::Metal, 0.0f, TEXT("следы в руде")));
    Table.Add(E(88, TEXT("Ra"), TEXT("радий"), 226.0f, 5.5f, 700.0f, 1737.0f, EMatterState::Solid, EElementKind::Metal, 0.0f, TEXT("следы в руде")));
    Table.Add(E(89, TEXT("Ac"), TEXT("актиний"), 227.0f, 10.07f, 1050.0f, 3200.0f, EMatterState::Solid, EElementKind::Actinide, 0.0f, TEXT("следы в руде")));
    Table.Add(E(90, TEXT("Th"), TEXT("торий"), 232.038f, 11.72f, 1750.0f, 4788.0f, EMatterState::Solid, EElementKind::Actinide, 9.6f, TEXT("песок, руда")));
    Table.Add(E(91, TEXT("Pa"), TEXT("протактиний"), 231.036f, 15.37f, 1572.0f, 4000.0f, EMatterState::Solid, EElementKind::Actinide, 0.0014f, TEXT("руда")));
    Table.Add(E(92, TEXT("U"), TEXT("уран"), 238.029f, 19.1f, 1135.0f, 4131.0f, EMatterState::Solid, EElementKind::Actinide, 2.7f, TEXT("руда")));
    Table.Add(E(93, TEXT("Np"), TEXT("нептуний"), 237.0f, 20.45f, 644.0f, 3902.0f, EMatterState::Solid, EElementKind::Actinide, 0.0f, TEXT("не встречается в земле")));
    Table.Add(E(94, TEXT("Pu"), TEXT("плутоний"), 244.0f, 19.82f, 640.0f, 3228.0f, EMatterState::Solid, EElementKind::Actinide, 0.0f, TEXT("не встречается в земле")));
    Table.Add(E(95, TEXT("Am"), TEXT("америций"), 243.0f, 13.69f, 1176.0f, 2011.0f, EMatterState::Solid, EElementKind::Actinide, 0.0f, TEXT("рукотворный")));
    Table.Add(E(96, TEXT("Cm"), TEXT("кюрий"), 247.0f, 13.51f, 1345.0f, 3110.0f, EMatterState::Solid, EElementKind::Actinide, 0.0f, TEXT("рукотворный")));
    Table.Add(E(97, TEXT("Bk"), TEXT("берклий"), 247.0f, 14.78f, 1050.0f, 2627.0f, EMatterState::Solid, EElementKind::Actinide, 0.0f, TEXT("рукотворный")));
    Table.Add(E(98, TEXT("Cf"), TEXT("калифорний"), 251.0f, 15.1f, 900.0f, 1470.0f, EMatterState::Solid, EElementKind::Actinide, 0.0f, TEXT("рукотворный")));
    Table.Add(E(99, TEXT("Es"), TEXT("эйнштейний"), 252.0f, 8.84f, 860.0f, 996.0f, EMatterState::Solid, EElementKind::Actinide, 0.0f, TEXT("рукотворный")));
    Table.Add(E(100, TEXT("Fm"), TEXT("фермий"), 257.0f, 9.7f, 1527.0f, 1800.0f, EMatterState::Solid, EElementKind::Actinide, 0.0f, TEXT("рукотворный")));
    Table.Add(E(101, TEXT("Md"), TEXT("менделевий"), 258.0f, 10.3f, 827.0f, 1100.0f, EMatterState::Solid, EElementKind::Actinide, 0.0f, TEXT("рукотворный")));
    Table.Add(E(102, TEXT("No"), TEXT("нобелий"), 259.0f, 9.9f, 827.0f, 1100.0f, EMatterState::Solid, EElementKind::Actinide, 0.0f, TEXT("рукотворный")));
    Table.Add(E(103, TEXT("Lr"), TEXT("лоуренсий"), 266.0f, 15.6f, 1627.0f, 1900.0f, EMatterState::Solid, EElementKind::Actinide, 0.0f, TEXT("рукотворный")));
    Table.Add(E(104, TEXT("Rf"), TEXT("резерфордий"), 267.0f, 23.2f, 2100.0f, 5500.0f, EMatterState::Solid, EElementKind::Metal, 0.0f, TEXT("рукотворный")));
    Table.Add(E(105, TEXT("Db"), TEXT("дубний"), 268.0f, 29.3f, 2400.0f, 5800.0f, EMatterState::Solid, EElementKind::Metal, 0.0f, TEXT("рукотворный")));
    Table.Add(E(106, TEXT("Sg"), TEXT("сиборгий"), 269.0f, 35.0f, 2600.0f, 6000.0f, EMatterState::Solid, EElementKind::Metal, 0.0f, TEXT("рукотворный")));
    Table.Add(E(107, TEXT("Bh"), TEXT("борий"), 270.0f, 37.1f, 2700.0f, 6200.0f, EMatterState::Solid, EElementKind::Metal, 0.0f, TEXT("рукотворный")));
    Table.Add(E(108, TEXT("Hs"), TEXT("хассий"), 269.0f, 40.7f, 2800.0f, 6400.0f, EMatterState::Solid, EElementKind::Metal, 0.0f, TEXT("рукотворный")));
    Table.Add(E(109, TEXT("Mt"), TEXT("мейтнерий"), 278.0f, 37.4f, 2900.0f, 6500.0f, EMatterState::Solid, EElementKind::Metal, 0.0f, TEXT("рукотворный")));
    Table.Add(E(110, TEXT("Ds"), TEXT("дармштадтий"), 281.0f, 34.8f, 2900.0f, 6600.0f, EMatterState::Solid, EElementKind::Metal, 0.0f, TEXT("рукотворный")));
    Table.Add(E(111, TEXT("Rg"), TEXT("рентгений"), 282.0f, 28.7f, 2900.0f, 6700.0f, EMatterState::Solid, EElementKind::Metal, 0.0f, TEXT("рукотворный")));
    Table.Add(E(112, TEXT("Cn"), TEXT("коперниций"), 285.0f, 23.7f, 10.0f, 67.0f, EMatterState::Liquid, EElementKind::Metal, 0.0f, TEXT("рукотворный")));
    Table.Add(E(113, TEXT("Nh"), TEXT("нихоний"), 286.0f, 16.0f, 430.0f, 1130.0f, EMatterState::Solid, EElementKind::Metal, 0.0f, TEXT("рукотворный")));
    Table.Add(E(114, TEXT("Fl"), TEXT("флеровий"), 289.0f, 14.0f, 70.0f, 150.0f, EMatterState::Solid, EElementKind::Metal, 0.0f, TEXT("рукотворный")));
    Table.Add(E(115, TEXT("Mc"), TEXT("московий"), 290.0f, 13.5f, 400.0f, 1100.0f, EMatterState::Solid, EElementKind::Metal, 0.0f, TEXT("рукотворный")));
    Table.Add(E(116, TEXT("Lv"), TEXT("ливерморий"), 293.0f, 12.9f, 360.0f, 760.0f, EMatterState::Solid, EElementKind::Metal, 0.0f, TEXT("рукотворный")));
    Table.Add(E(117, TEXT("Ts"), TEXT("теннессин"), 294.0f, 7.2f, 400.0f, 550.0f, EMatterState::Solid, EElementKind::Halogen, 0.0f, TEXT("рукотворный")));
    Table.Add(E(118, TEXT("Og"), TEXT("оганесон"), 294.0f, 5.0f, -10.0f, 80.0f, EMatterState::Gas, EElementKind::NobleGas, 0.0f, TEXT("рукотворный")));

    return Table;
}

const FElement* FElements::BySymbol(const FString& Symbol)
{
    for (const FElement& Entry : All())
    {
        if (Entry.Symbol.Equals(Symbol, ESearchCase::CaseSensitive))
        {
            return &Entry;
        }
    }
    return nullptr;
}

const FElement* FElements::ByName(const FString& Name)
{
    for (const FElement& Entry : All())
    {
        if (Entry.Name.Equals(Name, ESearchCase::IgnoreCase))
        {
            return &Entry;
        }
    }
    return nullptr;
}

const TArray<FMineral>& FElements::Minerals()
{
    static TArray<FMineral> Table;
    if (Table.Num() > 0)
    {
        return Table;
    }

    Table.Add(M(EResourceKind::Limestone, TEXT("известняк"), { TEXT("Ca"), TEXT("C"), TEXT("O") }, 2.7f, 3.0f, 825.0f, false, TEXT("холм"), 0.9f));
    Table.Add(M(EResourceKind::Chalk, TEXT("мел"), { TEXT("Ca"), TEXT("C"), TEXT("O") }, 2.5f, 1.5f, 825.0f, false, TEXT("холм"), 0.6f));
    Table.Add(M(EResourceKind::Granite, TEXT("гранит"), { TEXT("Si"), TEXT("O"), TEXT("Al"), TEXT("K") }, 2.75f, 6.5f, 1215.0f, false, TEXT("скала"), 0.85f));
    Table.Add(M(EResourceKind::Sandstone, TEXT("песчаник"), { TEXT("Si"), TEXT("O") }, 2.4f, 4.0f, 1650.0f, false, TEXT("холм"), 0.8f));
    Table.Add(M(EResourceKind::Flint, TEXT("кремень"), { TEXT("Si"), TEXT("O") }, 2.6f, 7.0f, 1700.0f, false, TEXT("равнина"), 0.7f));
    Table.Add(M(EResourceKind::Quartz, TEXT("кварц"), { TEXT("Si"), TEXT("O") }, 2.65f, 7.0f, 1670.0f, false, TEXT("скала"), 0.5f));
    Table.Add(M(EResourceKind::Sand, TEXT("песок"), { TEXT("Si"), TEXT("O") }, 1.6f, 7.0f, 1700.0f, false, TEXT("берег"), 0.9f));
    Table.Add(M(EResourceKind::Clay, TEXT("глина"), { TEXT("Al"), TEXT("Si"), TEXT("O"), TEXT("H") }, 1.8f, 2.0f, 1300.0f, false, TEXT("низина"), 0.9f));
    Table.Add(M(EResourceKind::Peat, TEXT("торф"), { TEXT("C"), TEXT("H"), TEXT("O") }, 0.4f, 1.0f, 0.0f, true, TEXT("болото"), 0.7f));
    Table.Add(M(EResourceKind::Coal, TEXT("каменный уголь"), { TEXT("C") }, 1.35f, 2.5f, 0.0f, true, TEXT("скала"), 0.45f));
    Table.Add(M(EResourceKind::Salt, TEXT("соль"), { TEXT("Na"), TEXT("Cl") }, 2.17f, 2.5f, 801.0f, false, TEXT("равнина"), 0.4f));
    Table.Add(M(EResourceKind::Sulphur, TEXT("сера"), { TEXT("S") }, 2.07f, 2.0f, 115.0f, true, TEXT("болото"), 0.25f));
    Table.Add(M(EResourceKind::IronOre, TEXT("железная руда"), { TEXT("Fe"), TEXT("O") }, 5.2f, 6.0f, 1538.0f, false, TEXT("скала"), 0.6f));
    Table.Add(M(EResourceKind::CopperOre, TEXT("медная руда"), { TEXT("Cu"), TEXT("S") }, 4.2f, 4.0f, 1085.0f, false, TEXT("скала"), 0.35f));
    Table.Add(M(EResourceKind::TinOre, TEXT("оловянная руда"), { TEXT("Sn"), TEXT("O") }, 6.9f, 6.5f, 1127.0f, false, TEXT("берег"), 0.25f));
    Table.Add(M(EResourceKind::LeadOre, TEXT("свинцовая руда"), { TEXT("Pb"), TEXT("S") }, 7.6f, 2.5f, 1114.0f, false, TEXT("скала"), 0.2f));
    Table.Add(M(EResourceKind::SilverOre, TEXT("серебряная руда"), { TEXT("Ag"), TEXT("S") }, 7.2f, 2.5f, 962.0f, false, TEXT("скала"), 0.08f));
    Table.Add(M(EResourceKind::GoldOre, TEXT("золотая руда"), { TEXT("Au") }, 15.0f, 2.5f, 1064.0f, false, TEXT("берег"), 0.04f));

    return Table;
}

const FMineral* FElements::MineralOf(EResourceKind Kind)
{
    for (const FMineral& Entry : Minerals())
    {
        if (Entry.Kind == Kind)
        {
            return &Entry;
        }
    }
    return nullptr;
}

void FElements::MineralsOfGround(FName Ground, TArray<const FMineral*>& Out)
{
    for (const FMineral& Entry : Minerals())
    {
        if (Entry.Ground == Ground)
        {
            Out.Add(&Entry);
        }
    }
}

const TArray<FWorldLaw>& FElements::Physics()
{
    static TArray<FWorldLaw> Laws;
    if (Laws.Num() > 0)
    {
        return Laws;
    }

    Laws.Add(L(TEXT("тяжесть"), TEXT("Всё падает вниз и разгоняется на девять целых восемь десятых метра в секунду за секунду."), 9.81f, TEXT("м/с2")));
    Laws.Add(L(TEXT("вес"), TEXT("Вес есть масса, умноженная на тяжесть. Вдвое больший камень вдвое тяжелее нести."), 9.81f, TEXT("Н/кг")));
    Laws.Add(L(TEXT("опора"), TEXT("Тело стоит, пока опора под ним. Сдвинь опору за край — тело упадёт."), 0.0f, TEXT("")));
    Laws.Add(L(TEXT("рычаг"), TEXT("Длинное плечо выигрывает в силе во столько раз, во сколько оно длиннее короткого."), 0.0f, TEXT("")));
    Laws.Add(L(TEXT("трение"), TEXT("Дерево по дереву держит примерно четыре десятых веса, лёд — пять сотых."), 0.4f, TEXT("")));
    Laws.Add(L(TEXT("вода"), TEXT("Вода замерзает при нуле и кипит при ста градусах."), 100.0f, TEXT("°C")));
    Laws.Add(L(TEXT("плотность воды"), TEXT("Ведро воды в десять литров весит десять килограммов."), 1000.0f, TEXT("кг/м3")));
    Laws.Add(L(TEXT("плавание"), TEXT("Что легче воды, то плавает: дерево плавает, камень тонет."), 1000.0f, TEXT("кг/м3")));
    Laws.Add(L(TEXT("тепло"), TEXT("Тепло идёт от горячего к холодному, пока оба не сравняются."), 0.0f, TEXT("")));
    Laws.Add(L(TEXT("огонь"), TEXT("Огню нужны три вещи: топливо, воздух и жар. Отними одно — огонь погаснет."), 0.0f, TEXT("")));
    Laws.Add(L(TEXT("дым"), TEXT("Горячий воздух легче холодного и потому идёт вверх."), 0.0f, TEXT("")));
    Laws.Add(L(TEXT("звук"), TEXT("Звук идёт по воздуху триста сорок метров в секунду и огибает препятствия."), 340.0f, TEXT("м/с")));
    Laws.Add(L(TEXT("свет"), TEXT("Свет идёт прямо и потому даёт тень."), 300000000.0f, TEXT("м/с")));
    Laws.Add(L(TEXT("день"), TEXT("Солнце встаёт с одной стороны и садится с противоположной, и так каждые двадцать четыре часа."), 24.0f, TEXT("ч")));
    Laws.Add(L(TEXT("сохранение"), TEXT("Ничто не берётся из ничего: чтобы что-то появилось, надо взять вещество и силу."), 0.0f, TEXT("")));
    Laws.Add(L(TEXT("усталость"), TEXT("Всякая работа берёт силы, и силы возвращаются едой и сном."), 0.0f, TEXT("")));

    return Laws;
}

const TArray<FWorldLaw>& FElements::Mathematics()
{
    static TArray<FWorldLaw> Truths;
    if (Truths.Num() > 0)
    {
        return Truths;
    }

    Truths.Add(L(TEXT("счёт"), TEXT("Считать — значит ставить вещам числа по порядку, не пропуская и не повторяя."), 0.0f, TEXT("")));
    Truths.Add(L(TEXT("сложение"), TEXT("От перемены мест слагаемых сумма не меняется."), 0.0f, TEXT("")));
    Truths.Add(L(TEXT("умножение"), TEXT("Умножить — сложить одно и то же число несколько раз."), 0.0f, TEXT("")));
    Truths.Add(L(TEXT("деление"), TEXT("Разделить — узнать, сколько раз одно помещается в другом."), 0.0f, TEXT("")));
    Truths.Add(L(TEXT("ноль"), TEXT("Ноль ничего не прибавляет и ничего не отнимает, а умноженное на ноль исчезает."), 0.0f, TEXT("")));
    Truths.Add(L(TEXT("дробь"), TEXT("Половина есть одно, разделённое на два; четверть — на четыре."), 0.5f, TEXT("")));
    Truths.Add(L(TEXT("площадь"), TEXT("Площадь прямоугольника есть длина, умноженная на ширину."), 0.0f, TEXT("")));
    Truths.Add(L(TEXT("объём"), TEXT("Объём ящика есть длина на ширину и на высоту."), 0.0f, TEXT("")));
    Truths.Add(L(TEXT("треугольник"), TEXT("В треугольнике с прямым углом квадрат длинной стороны равен сумме квадратов двух других."), 0.0f, TEXT("")));
    Truths.Add(L(TEXT("круг"), TEXT("Длина круга больше его поперечника примерно в три целых четырнадцать сотых раза."), 3.14159f, TEXT("")));
    Truths.Add(L(TEXT("равенство"), TEXT("Если к равным прибавить равное, останется равное."), 0.0f, TEXT("")));
    Truths.Add(L(TEXT("доля"), TEXT("Если из десяти вещей взять три, то взята треть без малого."), 0.3f, TEXT("")));
    Truths.Add(L(TEXT("мера"), TEXT("Измерить — значит сравнить с тем, что принято за единицу."), 0.0f, TEXT("")));
    Truths.Add(L(TEXT("больше и меньше"), TEXT("Из двух чисел больше то, которое при счёте называют позже."), 0.0f, TEXT("")));

    return Truths;
}
