#ifndef EDITOR_IPANEL
#define EDITOR_IPANEL

namespace Editor {

	class IPanel {
	public:
		virtual ~IPanel() = default;
		virtual void OnImGuiRender() = 0;
	};
}

#endif // !EDITOR_IPANEL
