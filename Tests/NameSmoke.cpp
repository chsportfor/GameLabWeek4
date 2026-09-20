#include "Core/Name.h"
#include "Core/Container/TMap.h"
#include "Engine/Serialization/PropertyJson.h"
#include <iostream>
#include <stdexcept>

static void Check(bool Value, const char* Message)
{
    if (!Value) throw std::runtime_error(Message);
}

int main()
{
    Check(FName().ToString() == FString("None"), "Default name display");
    const FName First("Particle1"), Second("Particle2"), Lower("particle1");
    Check(First.ComparisonIndex == Second.ComparisonIndex, "Numbered names share base entry");
    Check(First.Number == 2 && Second.Number == 3, "WEEK3 number convention");
    Check(First == Lower && !(First == Second), "Case-insensitive, number-aware equality");
    Check(First.ToString() == FString("Particle1") && Lower.ToString() == FString("particle1"),
        "Case-preserving display");
    Check(First.Compare(Second) < 0 && Second.Compare(First) > 0, "Number comparison");
    const FName Z("ZebraNameSmoke"), A("AppleNameSmoke");
    Check(Z.Compare(A) < 0, "WEEK3 entry-order comparison");

    TMap<FName, int> Values;
    Values.Add(First, 11);
    Values.Add(Second, 22);
    Check(Values.Num() == 2 && Values.Find(Lower) && *Values.Find(Lower) == 11,
        "Default TMap hasher integration");
    Check(Values.Find(Second) && *Values.Find(Second) == 22, "Numbered map lookup");
    Check(std::hash<FName>{}(First) == FNameHasher{}(First), "Hasher delegation");

    for (const char* Text : {"Particle", "Particle0", "Particle12", "Particle_12",
                            "Particle01", "Particle4294967294", "Particle4294967295"})
    {
        const FName Original(Text);
        Check(Original.ToString() == FString(Text), "Suffix round trip");
        json::JSON Json;
        TPropertyJsonSerializer<FName>::Serialize(Json, "Name", Original);
        FName Restored;
        TPropertyJsonSerializer<FName>::Deserialize(Json, "Name", Restored);
        Check(Restored == Original, "Property serialization round trip");
    }
    std::cout << "FName smoke passed\n";
}
