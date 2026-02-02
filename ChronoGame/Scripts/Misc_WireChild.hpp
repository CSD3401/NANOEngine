#pragma once
#include "EngineAPI.hpp"
#include <map>
#include <vector>

/**
 * Template - Auto-generated script template
 * Implement your game logic in the lifecycle methods below.
 */

class Misc_WireChild : public IScript {
public:
    Misc_WireChild() {
        // Register any editable fields here
        // Example: SCRIPT_FIELD(speed, float);
        // Example: SCRIPT_FIELD_VECTOR(blingstring, String);
        SCRIPT_FIELD(wireChildIndex, Int);
        SCRIPT_FIELD(wirePuzzleIndex, Int);
        SCRIPT_COMPONENT_REF(blue, MaterialRef);
        SCRIPT_COMPONENT_REF(red, MaterialRef);
        SCRIPT_COMPONENT_REF(green, MaterialRef);
        SCRIPT_COMPONENT_REF(orange, MaterialRef);
        SCRIPT_COMPONENT_REF(yellow, MaterialRef);
        SCRIPT_COMPONENT_REF(purple, MaterialRef);
        SCRIPT_COMPONENT_REF(pink, MaterialRef);
        SCRIPT_COMPONENT_REF(white, MaterialRef);
        SCRIPT_FIELD(correctWire, Bool);
    }

    ~Misc_WireChild() override = default;

    // === Lifecycle Methods ===

    void Awake() override {
        // Called when the script component is first created
    }

    void Initialize(Entity entity) override {
        // Called to initialize the script with its entity
    }

    void Start() override {
        // Called when the script is enabled and play mode starts
        //std::string log = "EntityID: " + GetEntityName() + "WIRE CHILD INDEX: " + std::to_string(wireChildIndex);
        //LOG_DEBUG(log);
    }

    void Update(double deltaTime) override {
        // Called every frame while the script is enabled
        //if (Input::WasKeyReleased('L'))
        //{
        //    SetMaterialRef(GetRendererRef(GetEntity()), blue);
        //    LOG_DEBUG("Pressed L");

        //}
        //if (Input::WasKeyReleased('K'))
        //{
        //    SetMaterialRef(GetRendererRef(GetEntity()), red);
        //    LOG_DEBUG("Pressed K");

        //}

        if (puzzleSolved)
        {
            if (changeTimer < 0.0f)
            {
                SetMaterialRef(GetRendererRef(GetEntity()), white);
            }
            else
            {
                changeTimer -= static_cast<float>(deltaTime);
            }
        }
    }

    void OnDestroy() override {
        // Called when the script is about to be destroyed
    }

    // === Optional Callbacks ===

    void OnEnable() override {
        // Called when the script is enabled
        //std::string message = "UpdateWireColour" + std::to_string(wirePuzzleIndex) + std::to_string(wireChildIndex);
        //LOG_DEBUG(message.c_str());
        // listen to event for updating wire colours
        //Events::Listen(message.c_str(), UpdateWireColour);

        std::string message2 = "PuzzleSolved1";
        Events::Listen(message2.c_str(), [this](void* data) {
            this->PuzzleSolved();
            });
    }

    void OnDisable() override {
        // Called when the script is disabled
    }

    void OnValidate() override {
        // Called when a field value is changed in the editor
    }

    const char* GetTypeName() const override {
        return "Misc_WireChild";
    }

    // === Collision Callbacks ===

    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override { (void)other; }
    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

    bool GetIsCorrectWire()
    {
        return correctWire;
    }

    void UpdateWireColour(int colourIndex)
    {
        WIRE_COLOUR c = (WIRE_COLOUR)(colourIndex);
        switch (c)
        {
        case WIRE_COLOUR::BLUE:
        {
            SetMaterialRef(GetRendererRef(GetEntity()), blue);
            break;
        }
        case WIRE_COLOUR::GREEN:
        {
            SetMaterialRef(GetRendererRef(GetEntity()), green);
            break;
        }
        case WIRE_COLOUR::ORANGE:
        {
            SetMaterialRef(GetRendererRef(GetEntity()), orange);

            break;
        }
        case WIRE_COLOUR::PINK:
        {
            SetMaterialRef(GetRendererRef(GetEntity()), pink);
            break;
        }
        case WIRE_COLOUR::PURPLE:
        {
            SetMaterialRef(GetRendererRef(GetEntity()), purple);
            break;
        }
        case WIRE_COLOUR::RED:
        {
            SetMaterialRef(GetRendererRef(GetEntity()), red);

            break;
        }
        case WIRE_COLOUR::YELLOW:
        {
            SetMaterialRef(GetRendererRef(GetEntity()), yellow);
            break;
        }
        case WIRE_COLOUR::WHITE:
        {
            SetMaterialRef(GetRendererRef(GetEntity()), white);
            break;
        }
        default:
            LOG_ERROR("COLOUR OUT OF RANGE!");
        }
    }

    void PuzzleSolved()
    {
        // play sound effect
        // start countdown
        //puzzleSolved = true;
        //changeTimer = *reinterpret_cast<float*>(data);
    }

    enum WIRE_COLOUR
    {
        BLUE = 0,
        RED,
        GREEN,
        YELLOW,
        ORANGE,
        PURPLE,
        PINK,
        WHITE // Finished
    };

private:
    // Add your private member variables here
    // Example: float speed = 5.0f;
    int wireChildIndex;
    int wirePuzzleIndex;
    MaterialRef colourMat;

    // Colours
    MaterialRef blue;
    MaterialRef green;
    MaterialRef orange;
    MaterialRef pink;
    MaterialRef purple;
    MaterialRef red;
    MaterialRef yellow;
    MaterialRef white;

    bool puzzleSolved = false;
    float changeTimer = 0.5f;

    bool correctWire = false;

};