#pragma once
#include "EngineAPI.hpp"
#include "Puzzle_.hpp"
#include <map>
#include <vector>
#include "Misc_WireChild.hpp"

/**
 * Template - Auto-generated script template
 * Implement your game logic in the lifecycle methods below.
 */

enum WirePuzzleIndex
{
    _01,
    _02,
    _03
};
class Puzzle_Wire : public Puzzle_ {
public:
    Puzzle_Wire() {
        // Register any editable fields here
        // Example: SCRIPT_FIELD(speed, float);
        // Example: SCRIPT_FIELD_VECTOR(blingstring, String);;
        SCRIPT_GAMEOBJECT_REF(wireHolderObject);
        SCRIPT_GAMEOBJECT_REF(correctHolderObject);
        SCRIPT_GAMEOBJECT_REF(connectedHolderObject);
        SCRIPT_FIELD_VECTOR(wireColours, Int);
        SCRIPT_FIELD_VECTOR(correctColours, Int);
        SCRIPT_FIELD(wirePuzzleIndex, Int);
        SCRIPT_COMPONENT_REF(finishedWireColour, MaterialRef);
        SCRIPT_FIELD(changeTimer, Float);

    }

    ~Puzzle_Wire() override = default;

    // === Lifecycle Methods ===

    void Awake() override {
        // Called when the script component is first created
    }

    void Initialize(Entity entity) override {
        // Called to initialize the script with its entity
        // Grab all the children from the entity

    }

    void Start() override {
        // Called when the script is enabled and play mode starts
        // Message = "WireButtonPressed" + wire puzzle index
        // the pressed button sends the wire child index to swap colours
        // Tell all the children to change to their respective colour
        InitWireColours();
    }

    void Update(double deltaTime) override {
        // Called every frame while the script is enabled
        if (buttonPressed)
        {
            buttonPressed = false;
            SwapWireColours(indexToSwap);
        }
    }

    void OnDestroy() override {
        // Called when the script is about to be destroyed
    }

    // === Optional Callbacks ===

    void OnEnable() override {
        // Called when the script is enabled
        std::string listenToMessage = "WireButtonPressed" + std::to_string(wirePuzzleIndex);
        Events::Listen(listenToMessage.c_str(), [this](void* data) {
            this->RecieveIndexData(data);
            });

        LOG_DEBUG(listenToMessage.c_str());
    }

    void OnDisable() override {
        // Called when the script is disabled
    }

    void OnValidate() override {
        // Called when a field value is changed in the editor
    }

    const char* GetTypeName() const override {
        return "Puzzle_Wire";
    }

    // === Collision Callbacks ===

    void OnCollisionEnter(Entity other) override { (void)other; }
    void OnCollisionExit(Entity other) override { (void)other; }
    void OnCollisionStay(Entity other) override { (void)other; }
    void OnTriggerEnter(Entity other) override { (void)other; }
    void OnTriggerExit(Entity other) override { (void)other; }
    void OnTriggerStay(Entity other) override { (void)other; }

    void InitWireColours()
    {
        numWires = static_cast<int>(GetChildCount(wireHolderObject.GetEntity()));
        LOG_DEBUG("NUM CHILDREN: " + std::to_string(numWires));
        // Message is UpdateWireColour + WirePuzzleIndex + WireChildIndex
        wireChildren = GetChildren(wireHolderObject.GetEntity());
        correctChildren = GetChildren(correctHolderObject.GetEntity());
        connectedWires = GetChildren(connectedHolderObject.GetEntity());
        //std::string message = "UpdateWireColour" + std::to_string(wirePuzzleIndex);
        for (int i = 0; i < numWires; ++i)
        {
            //Events::Send((message + std::to_string(i)).c_str(), &wireColours[i]);
            GameObject child(wireChildren[i]);
            if (child)
            {
                child.GetComponent<Misc_WireChild>()->UpdateWireColour(wireColours[i]);
            }
            GameObject child2(correctChildren[i]);
            if (child2)
            {
                child2.GetComponent<Misc_WireChild>()->UpdateWireColour(correctColours[i]); 
            }
            GameObject child3(connectedWires[i]);
            if (child3)
            {
                child3.GetComponent<Misc_WireChild>()->UpdateWireColour(correctColours[i]);
                SetActive(false, connectedWires[i]);
            }
        }



    }

    void RecieveIndexData(void* indexData)
    {
        indexToSwap = *reinterpret_cast<int*>(indexData);
        buttonPressed = true;
    }

    void SwapWireColours(int lIndex)
    {
        int leftIndex = lIndex;
        int rightIndex = leftIndex + 1;
        // Update the colours in the vector
        std::swap(wireColours[leftIndex], wireColours[rightIndex]);

        // update the wire gameobjects to their new colour
        //std::string message = "UpdateWireColour" + std::to_string(wirePuzzleIndex);
        //LOG_DEBUG(message.c_str());
        //Events::Send((message + std::to_string(leftIndex)).c_str(), &wireColours[leftIndex]);
        //Events::Send((message + std::to_string(rightIndex)).c_str(), &wireColours[rightIndex]);
        //LOG_DEBUG(message.c_str());

        GameObject child1(wireChildren[leftIndex]);
        child1.GetComponent<Misc_WireChild>()->UpdateWireColour(wireColours[leftIndex]);
        GameObject child2(wireChildren[rightIndex]);
        child2.GetComponent<Misc_WireChild>()->UpdateWireColour(wireColours[rightIndex]);

        // Check if the puzzle is solved
        if (CheckWireColours())
        {
            // Send message that puzzle is solved
            //Solve();

            // Current Fix until solve is able to be used
            LOG_DEBUG("PUZZLE SOLVED!");
            std::string message = "PuzzleSolved1";
            Events::Send(message.c_str());

            for (int i = 0; i < numWires; ++i)
            {
                SetActive(true, connectedWires[i]);
            }
        }
    }

    bool CheckWireColours()
    {
        for (int i = 0; i < wireColours.size(); ++i)
        {
            if (wireColours[i] != correctColours[i])
            {
                return false;
            }
        }
        return true;
    }

private:
    // Add your private member variables here
    // Example: float speed = 5.0f;
    GameObjectRef wireHolderObject;
    GameObjectRef correctHolderObject;
    GameObjectRef connectedHolderObject;
    int numWires = 3;
    std::vector<int> wireColours;
    std::vector<int> correctColours;
    int wirePuzzleIndex;
    bool buttonPressed = false;
    int indexToSwap = 0;
    MaterialRef finishedWireColour;
    float changeTimer = 0.5f;

    // Testing
    std::vector<Entity> wireChildren;
    std::vector<Entity> correctChildren;
    std::vector<Entity> connectedWires;
};