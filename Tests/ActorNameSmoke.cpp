#include "Engine/World.h"
#include "Engine/Components/NameComponent.h"
#include "Engine/EngineStatics.h"
#include "ThirdParty/Json/json.hpp"
#include <iostream>
#include <stdexcept>

static void Check(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}

static AActor* Add(UWorld& world, const char* name)
{
    auto* actor = FObjectFactory::ConstructObjectWithName<AActor>(FName(name));
    world.AddActor(actor);
    return actor;
}

static void ExpectName(const AActor* actor, const char* name)
{
    Check(actor->GetName().ToString().Equals(std::string_view(name)), name);
}

int main()
{
    try
    {
        std::unique_ptr<UWorld> worldOwner(FObjectFactory::ConstructObject<UWorld>());
        UWorld& world = *worldOwner;
        auto* first = Add(world, "Particle");
        auto* second = Add(world, "particle");
        ExpectName(first, "Particle");
        ExpectName(second, "particle0"); // FName Number=1 displays suffix 0.
        Check(world.RemoveActor(second->UUID), "Remove actor");
        delete second;
        ExpectName(Add(world, "Particle"), "Particle1");

        first->SetName(FName("Particle"));
        ExpectName(first, "Particle"); // Reapplying the name must not consume a number.
        auto* renamed = Add(world, "Cube");
        auto* label = FObjectFactory::ConstructObject<UNameComponent>(FString(""), FVector{}, nullptr);
        renamed->AddComponent(label);
        renamed->SetName(FName("PARTICLE"));
        ExpectName(renamed, "PARTICLE2");
        json::JSON labelJson;
        label->SerializeClass(labelJson);
        Check(labelJson.at("Properties").at("mNameText").ToString().find("PARTICLE2") != std::string::npos,
              "Label uses resolved actor name");
        renamed->SetName(FName("Particle8"));
        ExpectName(Add(world, "Particle"), "Particle9");

        json::JSON copyJson;
        first->SerializeClass(copyJson);
        copyJson["Properties"]["UUID"] = UEngineStatics::GenerateUUID();
        auto* copy = FObjectFactory::LoadObject<AActor>(copyJson);
        world.AddActor(copy);
        ExpectName(copy, "Particle10");

        std::unique_ptr<UWorld> otherWorldOwner(FObjectFactory::ConstructObject<UWorld>());
        UWorld& otherWorld = *otherWorldOwner;
        ExpectName(Add(otherWorld, "Particle"), "Particle");
        Check(world.RemoveActor(copy->UUID), "Detach actor");
        copy->SetName(FName("Detached"));
        otherWorld.AddActor(copy);
        ExpectName(copy, "Detached");

        // Unsorted saved names, including a duplicate before a later numbered name.
        std::unique_ptr<UWorld> sourceOwner(FObjectFactory::ConstructObject<UWorld>());
        UWorld& source = *sourceOwner;
        Add(source, "Cube"); Add(source, "Sphere"); Add(source, "Box"); Add(source, "Cone");
        json::JSON saved;
        source.SerializeClass(saved);
        auto& actors = saved["Properties"]["mActors"];
        actors[0]["Properties"]["Name"] = "Cube8";
        actors[1]["Properties"]["Name"] = "Cube";
        actors[2]["Properties"]["Name"] = "cube";
        actors[3]["Properties"]["Name"] = "Cube9";
        std::unique_ptr<UWorld> loadedOwner(FObjectFactory::ConstructObject<UWorld>());
        UWorld& loaded = *loadedOwner;
        loaded.DeserializeClass(saved);
        ExpectName(loaded.GetActors()[0], "Cube8");
        ExpectName(loaded.GetActors()[1], "Cube");
        ExpectName(loaded.GetActors()[2], "cube10");
        ExpectName(loaded.GetActors()[3], "Cube9");
        ExpectName(Add(loaded, "Cube"), "Cube11");

        json::JSON roundTrip;
        loaded.SerializeClass(roundTrip);
        std::unique_ptr<UWorld> restoredOwner(FObjectFactory::ConstructObject<UWorld>());
        UWorld& restored = *restoredOwner;
        restored.DeserializeClass(roundTrip);
        ExpectName(Add(restored, "Cube"), "Cube12");
        std::cout << "Actor naming smoke passed: create, rename, copy, delete, world scope, load and round-trip.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
