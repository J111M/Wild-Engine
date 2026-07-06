#pragma once

#include <optional>
#include <string>
#include <vector>

namespace Wild
{
    class UndoSystem : public NonCopyable
    {
      public:
        UndoSystem() = default;

        void BeginEdit();

        // Call once the edit ends. Pushes the BeginEdit snapshot onto the
        // undo stack.
        void CommitEdit();

        bool CanUndo() const { return !m_undoStack.empty(); }
        bool CanRedo() const { return !m_redoStack.empty(); }

        void RequestUndo();
        void RequestRedo();

        bool ProcessPending();

        void ClearHistory();

      private:
        bool Undo();
        bool Redo();

        enum class PendingAction
        {
            None,
            Undo,
            Redo
        };

        int m_editDepth = 0;
        std::optional<std::string> m_pendingSnapshot;
        PendingAction m_pendingAction = PendingAction::None;

        std::vector<std::string> m_undoStack;
        std::vector<std::string> m_redoStack;

        static constexpr size_t kMaxHistory = 50;
    };
} // namespace Wild
