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
        SCRIPT_FIELD_VECTOR(wirePath, Int);
        SCRIPT_FIELD(wirePuzzleIndex, Int);
        SCRIPT_COMPONENT_REF(finishedWireColour, MaterialRef);
        SCRIPT_FIELD(changeTimer, Float);

        SCRIPT_FIELD(eventName, String);
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
            //SwapWireColours(indexToSwap);
            UpdatePuzzleVars();
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
        //indexToSwap = *reinterpret_cast<int*>(indexData);
        wireDataRecieved = *reinterpret_cast<std::string*>(indexData);
        buttonPressed = true;
    }

    //void SwapWireColours(int lIndex)
    //{
    //    int leftIndex = lIndex;
    //    int rightIndex = leftIndex + 1;
    //    // Update the colours in the vector
    //    std::swap(wireColours[leftIndex], wireColours[rightIndex]);

    //    // update the wire gameobjects to their new colour
    //    //std::string message = "UpdateWireColour" + std::to_string(wirePuzzleIndex);
    //    //LOG_DEBUG(message.c_str());
    //    //Events::Send((message + std::to_string(leftIndex)).c_str(), &wireColours[leftIndex]);
    //    //Events::Send((message + std::to_string(rightIndex)).c_str(), &wireColours[rightIndex]);
    //    //LOG_DEBUG(message.c_str());

    //    GameObject child1(wireChildren[leftIndex]);
    //    child1.GetComponent<Misc_WireChild>()->UpdateWireColour(wireColours[leftIndex]);
    //    GameObject child2(wireChildren[rightIndex]);
    //    child2.GetComponent<Misc_WireChild>()->UpdateWireColour(wireColours[rightIndex]);

    //    // Check if the puzzle is solved
    //    if (CheckWireColours())
    //    {
    //        // Send message that puzzle is solved
    //        //Solve();

    //        // Current Fix until solve is able to be used
    //        LOG_DEBUG("PUZZLE SOLVED!");
    //        std::string message = "PuzzleSolved1";
    //        Events::Send(message.c_str());

    //        //for (int i = 0; i < numWires; ++i)
    //        //{
    //        //    SetActive(true, connectedWires[i]);
    //        //}
    //    }
    //}

    //bool CheckWireColours()
    //{
    //    for (int i = 0; i < wireColours.size(); ++i)
    //    {
    //        //if (wireColours[i] != correctColours[i])
    //        if (wireColours[i] != correctColours[wirePath[i]])
    //        {
    //            return false;
    //        }
    //    }
    //    return true;
    //}

    void UpdatePuzzleVars()
    {
        int side = std::stoi(wireDataRecieved.substr(0,1));
        int index = std::stoi(wireDataRecieved.substr(1));
        if (side == 0)
        {
            currentSelectedLeftIndex = index;
        }
        else
        {
            currentSelectedRightIndex = index;
        }

        std::string message = "SIDE: ";
        message += side == 0 ? "TOP" : "BOTTOM";
        message += ", INDEX: ";
        message += std::to_string(index);
        LOG_DEBUG(message);

        if (currentSelectedLeftIndex != 9999 && currentSelectedRightIndex != 9999)
        {
            if (CheckWirePair()) // if all 4 are correct
            {
                LOG_DEBUG("PUZZLE SOLVED!");
                std::string message = "PuzzleSolved1";
                Events::Send(eventName.c_str());
            }

        }
    }

    bool CheckWirePair()
    {
        if (wireColours[currentSelectedLeftIndex] == correctColours[currentSelectedRightIndex])
        {
            // if true turn on the wire connecting them
            SetActive(true, connectedWires[currentSelectedRightIndex]);
            // then reset
            currentSelectedLeftIndex = 9999;
            currentSelectedRightIndex = 9999;

            // Increment number of correct pairs
            ++correctPairs;
            if (correctPairs == numWires)
            {
                return true;
            }
        }
        // fail then reset pair
        currentSelectedLeftIndex = 9999;
        currentSelectedRightIndex = 9999;
        return false;
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
    std::vector<int> wirePath;
    int wirePuzzleIndex;
    bool buttonPressed = false;
    int indexToSwap = 0;
    MaterialRef finishedWireColour;
    float changeTimer = 0.5f;

    // Testing
    std::vector<Entity> wireChildren;
    std::vector<Entity> correctChildren;
    std::vector<Entity> connectedWires;

    int currentSelectedLeftIndex = 9999; // left -> top row
    int currentSelectedRightIndex = 9999; // right -> bottom row
    int correctPairs = 0;
    std::string wireDataRecieved;

    // RF Added
    // PuzzleSolved1 - at the left and right
    // PuzzleSolved2 - right b4 catwalk

    std::string eventName = "PuzzleSolved1"; 

    // Connected wires and correct children are teh colours the player needs to line up
    // correct[0, 2, 1] -> blue red green
    // connected are coloured the same as correct
    // wireChildren [1, 0, 2] -> red green blue 
    // wirepaths -> map where [x][y]
    // x: the index of the wirechild
    // y: what colour it connects to
    // must match wirechildren to correct wire paths
    // the value inside wirechild[x] must be == to correctWire[wirepath[x]]
};