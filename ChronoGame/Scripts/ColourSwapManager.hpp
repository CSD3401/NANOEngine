#pragma once
#include "EngineAPI.hpp"




/**
 * ColourSwapManager - Auto-generated script template
 * Implement your game logic in the lifecycle methods below.
 */
class ColourSwapManager : public IScript {
public:
	ColourSwapManager() {
		// Register any editable fields here
		// Example: SCRIPT_FIELD(speed, Float);
		SCRIPT_FIELD(isActive, Bool);
		SCRIPT_FIELD(numColourChildren, Int);
		SCRIPT_FIELD(puzzleIndex, Int);
		SCRIPT_FIELD(turnTimer, Float);
		//SCRIPT_FIELD_VECTOR(colours, Material);  // Now uncommented - will work!
		SCRIPT_FIELD(correctColour, MaterialRef);
		SCRIPT_FIELD_VECTOR(correctSol, MaterialRef);
		SCRIPT_FIELD_VECTOR(startingColours, MaterialRef);
		SCRIPT_FIELD_VECTOR(swappableChildren, Entity);
		SCRIPT_FIELD_VECTOR(childSolution, Entity);

		std::cout << "[ColourSwapManager] Created with fields registered" << std::endl;

	}

	~ColourSwapManager() override = default;

	// === Lifecycle Methods ===

	void Awake() override {
		// Called when the script component is first created
	}

	void Initialize(Entity entity) override {
		// Called to initialize the script with its entity
	}

	void Start() override {
		// Called when the script is enabled and play mode starts
		InitPuzzle();
		LOG_WARNING("ColourSwapManager Start :" << eventMessage.c_str());

	}

	void Update(double deltaTime) override {
		// Called every frame while the script is enabled
		if (!isActive) return;

		if (!isSolved) {

		}
		else
		{
			if (!changedToSolved)
			{
				turnTimer -= (float)deltaTime;
				if (turnTimer <= 0.0f)
				{
					changedToSolved = true;
					PuzzleSolved();
				}
			}
		}
	}

	void OnDestroy() override {
		// Called when the script is about to be destroyed
	}

	// === Optional Callbacks ===

	void OnEnable() override {
		// Called when the script is enabled
	}

	void OnDisable() override {
		// Called when the script is disabled
	}

	void OnValidate() override {
		// Called when a field value is changed in the editor
	}

	const char* GetTypeName() const override {
		return "ColourSwapManager";
	}

	// === Collision Callbacks ===

	void OnCollisionEnter(Entity other) override {
		// Called when this entity starts colliding with another
	}

	void OnCollisionExit(Entity other) override {
		// Called when this entity stops colliding with another
	}

	void OnTriggerEnter(Entity other) override {
		// Called when this entity enters a trigger
	}

	void OnTriggerExit(Entity other) override {
		// Called when this entity exits a trigger
	}

	

private:
	// Add your private member variables here
	// Example: float speed = 5.0f;
	bool isActive = true;
	int numColourChildren = 0;

	bool isSolved = false;
	bool changedToSolved = false;
	float turnTimer = 1.0f;
	int puzzleIndex = 0;
	// green, red, blue
	MaterialRef correctColour;
	// Make sure the size of each of the vectors are the SAME
	std::vector<MaterialRef> correctSol; // Set the materials in here for the solution the player has to get
	std::vector<MaterialRef> startingColours; // Set the materials in here for the starting colours of the 2nd row
	std::vector<Entity> swappableChildren; // set the 2nd row in here
	std::vector<Entity> childSolution; // set the 1st row in here

	std::unordered_map<int, MaterialRef> currentPuzzle;

	std::string eventMessage = "colourPuzzle0";

	void InitPuzzle()
	{
		if (swappableChildren.size() != startingColours.size())
		{
			LOG_ERROR("Vector sizes not equal!");
			return;
		}
		int childIndex = 0;
		// for each swappable child
		// set it in the unordered map alongside its starting colour
		// set the childs material to that colour
		//LOG_DEBUG("STARTING CHILDS COLOURS:");
		for (Entity child : swappableChildren)
		{
			currentPuzzle[childIndex] = startingColours[childIndex];
			Entity childDome = GetChildOf(child, 1);
			Entity childMesh = GetChildOf(childDome, 0);
			NE::Renderer::Command::AssignMaterial(childMesh, startingColours[childIndex]);
			//LOG_DEBUG("CHILD[" << childIndex << "]: " << startingColours[childIndex].GetEntity());
			childIndex++;
		}

		// Set the children in the vector to the materials according to the correct solution vector
		if (childSolution.size() != correctSol.size())
		{
			LOG_ERROR("Vector sizes not equal!");
			return;
		}
		childIndex = 0;
		//LOG_DEBUG("SOLUTION CHILDS COLOURS:");
		for (Entity c : childSolution)
		{

			Entity childDome = GetChildOf(c, 1);
			Entity childMesh = GetChildOf(childDome, 0);
			NE::Renderer::Command::AssignMaterial(childMesh, correctSol[childIndex]);
			//LOG_DEBUG("SOLUTION[" << childIndex << "]: " << correctSol[childIndex].GetEntity());
			childIndex++;
		}

		std::string listenEvent = "ColourSwapPuzzle" + std::to_string(puzzleIndex);
		Events::Listen(listenEvent.c_str(), [this](void* data) {
			this->SwapColours(data);
		});
		LOG_DEBUG("LISTENING TO EVENT: " + listenEvent);

		//eventMessage += std::to_string(puzzleIndex);
	}

	void SwapColours(void* indexData)
	{
		if (isSolved)
			return;

		int leftIndex = *reinterpret_cast<int*>(indexData);
		int rightIndex = leftIndex + 1;
		if (leftIndex > swappableChildren.size())
		{
			LOG_ERROR("IndexData out of range!");
			return;
		}
		//LOG_DEBUG("SWAPPING COLOURS");
		// Get the children to swap
		Entity leftChild = swappableChildren[leftIndex];
		Entity rightChild = swappableChildren[rightIndex];

		//LOG_DEBUG("LEFT CHILD MATERIAL: " << currentPuzzle[leftIndex].GetEntity());
		//LOG_DEBUG("RIGHT CHILD MATERIAL: " << currentPuzzle[rightIndex].GetEntity());
		// Swap the colours assigned to those entities
		std::swap(currentPuzzle[leftIndex], currentPuzzle[rightIndex]);

		Entity leftDome = GetChildOf(leftChild, 1);
		Entity leftMesh = GetChildOf(leftDome, 0);

		Entity rightDome = GetChildOf(rightChild, 1);
		Entity rightMesh = GetChildOf(rightDome, 0);

		//LOG_DEBUG("AFTER SWAPPING:");
		//LOG_DEBUG("LEFT CHILD MATERIAL: " << currentPuzzle[leftIndex].GetEntity());
		//LOG_DEBUG("RIGHT CHILD MATERIAL: " << currentPuzzle[rightIndex].GetEntity());
		// Apply new materials to the actual scene objects
		NE::Renderer::Command::AssignMaterial(leftMesh, currentPuzzle[leftIndex]);
		NE::Renderer::Command::AssignMaterial(rightMesh, currentPuzzle[rightIndex]);

		// Check if puzzle solved
		if (CheckPuzzle())
		{
			LOG_DEBUG("Puzzle is correct!");
			isSolved = true;
		}
	}

	


	bool CheckPuzzle()
	{
		for (int i = 0; i < swappableChildren.size(); ++i)
		{
			//LOG_DEBUG("SWAPCHILD[" << i << "]: " << currentPuzzle[i].GetEntity());
			//LOG_DEBUG("SOLUTION[" << i << "]: " << correctSol[i].GetEntity());
			if (currentPuzzle[i].GetEntity() != correctSol[i].GetEntity())
				return false;
		}

		return true;
	}

	void PuzzleSolved()
	{
		for (Entity child : swappableChildren)
		{
			Entity childDome = GetChildOf(child, 1);
			Entity childMesh = GetChildOf(childDome, 0);
			NE::Renderer::Command::AssignMaterial(childMesh, correctColour);
		}
		for (Entity child : childSolution)
		{
			Entity childDome = GetChildOf(child, 1);
			Entity childMesh = GetChildOf(childDome, 0);
			NE::Renderer::Command::AssignMaterial(childMesh, correctColour);

		}

		LOG_WARNING("PuzzleSolved :" << eventMessage.c_str());
		Events::Send(eventMessage.c_str());
	}
};
