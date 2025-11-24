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
		//SCRIPT_FIELD_VECTOR(colours, Material);  // Now uncommented - will work!
		SCRIPT_FIELD_VECTOR(correctSol, MaterialRef);
		std::cout << "[ColourSwapManager] Created with fields registered" << std::endl;
	}

	~ColourSwapManager() override = default;

	// === Lifecycle Methods ===

	void Awake() override {
		// Called when the script component is first created
	}

	void Initialize(Entity entity) override {
		// Called to initialize the script with its entity
		GrabChildren();
	}

	void Start() override {
		// Called when the script is enabled and play mode starts
		GrabChildren();
		LOG_DEBUG("CORRECT SEQUENCE FOR PUZZLE: BLUE - GREEN - RED");
	}

	void Update(double deltaTime) override {
		// Called every frame while the script is enabled
		if (!isActive) return;

		if (!isSolved)
		{
			// need to change this
			if (Input::WasKeyPressed('N')) {
				SwapColours(1, 2);
			}
			if (Input::WasKeyPressed('M')) {
				SwapColours(2, 4);
			}
		}
		else
		{
			if (!changedToSolved)
			{
				turnTimer -= deltaTime;
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
	// green, red, blue
	std::vector<std::string> colours = { "c57f74a5-e22a-40fe-bc56-f02aaaa494c8", "c427718b-41d1-465f-a21f-99ac1981e4e9", "ea32e122-a8ba-4672-ab60-705fd79b9086"};
	// Child Index - Child Material
	std::unordered_map<int, std::string> childColours;
	// blue, green, red 
	std::vector<std::string> correctSolution = { "ea32e122-a8ba-4672-ab60-705fd79b9086" ,"c57f74a5-e22a-40fe-bc56-f02aaaa494c8",  "c427718b-41d1-465f-a21f-99ac1981e4e9" };
	
	// Make sure the size of each of the vectors are the SAME
	std::vector<MaterialRef> correctSol;
	std::vector<MaterialRef> startingColours;
	std::vector<Entity> swappableChildren;
	std::vector<Entity> childSolution;

	std::unordered_map<Entity, MaterialRef> currentPuzzle;

	void GrabChildren() {
		size_t childCount = GetChildCount();
		int currColour = 0;
		for (int i = 0; i < childCount; ++i)
		{
			Entity child = GetChild(i);

			if (!Command::HasComponent<Component::NativeScript>(child)) {
				LOG_INFO("  Child " << i << " has entity ID: " << child);
				LOG_INFO("  Child " << i << " does not have Script component");
				childColours[i] = colours[currColour++];
			}
		}
	}

	void InitPuzzle()
	{
		int childIndex = 0;
		// for each swappable child
		// set it in the unordered map alongside its starting colour
		// set the childs material to that colour
		for (Entity child : swappableChildren)
		{
			childColours[childIndex] = startingColours[childIndex];
			currentPuzzle[child] = startingColours[childIndex];
			NE::Renderer::Command::AssignMaterial(child, startingColours[childIndex++]);
		}

		// Set the children in the vector to the materials according to the correct solution vector
		childIndex = 0;
		for (Entity child : childSolution)
		{
			NE::Renderer::Command::AssignMaterial(child, correctSol[childIndex++]);
		}
	}

	void SwapColours(size_t leftIndex, size_t rightIndex)
	{
		// Get the children to swap
		Entity leftChild = swappableChildren[leftIndex];
		Entity rightChild = swappableChildren[rightIndex];

		// Swap the colours assigned to those entities
		std::swap(currentPuzzle[leftChild], currentPuzzle[rightChild]);

		// Apply new materials to the actual scene objects
		NE::Renderer::Command::AssignMaterial(leftChild, currentPuzzle[leftChild]);
		NE::Renderer::Command::AssignMaterial(rightChild, currentPuzzle[rightChild]);

		// Check if puzzle solved
		if (CheckPuzzle())
		{
			LOG_DEBUG("Puzzle is correct!");
			isSolved = true;
		}
	}


	bool CheckPuzzle()
	{
		for (size_t i = 0; i < swappableChildren.size(); ++i)
		{
			Entity child = swappableChildren[i];

			if (currentPuzzle[child] != correctSol[i])
				return false;
		}

		//for (std::pair<int, std::string> pair : childColours)
		//{
		//	if (pair.second != correctSolution[currIndex++])
		//	{
		//		return false;
		//	}

		//}

		return true;
	}

	void PuzzleSolved()
	{
		//for (std::pair<int, std::string> pair : childColours)
		//{
		//	Entity child = GetChild(pair.first);
		//	// change the materials to white
		//	NE::Renderer::Command::AssignMaterial(child, colours[0]);
		//}

		for (Entity child : swappableChildren)
		{
			NE::Renderer::Command::AssignMaterial(child, colours[0]);
		}
	}
};
