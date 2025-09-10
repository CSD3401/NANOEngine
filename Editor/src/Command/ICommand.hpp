#pragma once

namespace Editor {

    class ICommand {
    public:
        virtual ~ICommand() = default;
        virtual void Execute() = 0;
        virtual void Undo() = 0;

        virtual const char* GetName() const = 0;

        // Coalescing API (optional)
        virtual bool CanCoalesceWith(const ICommand&) const { return false; }
        virtual void CoalesceFrom(const ICommand&) { /* default: none */ }
        // (Optional) timestamp for freshness
        virtual double TimeIssuedSeconds() const { return 0.0; }
    };

}
