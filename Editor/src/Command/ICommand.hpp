#pragma once

namespace Editor {

    class ICommand {
    public:
        virtual ~ICommand() = default;
        virtual void Execute() = 0;
        virtual void Undo() = 0;

        virtual const char* GetName() const = 0;
    };

}
