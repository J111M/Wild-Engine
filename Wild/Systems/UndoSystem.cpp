#include "Systems/UndoSystem.hpp"

#include "Core/Engine.hpp"

namespace Wild
{
    void UndoSystem::BeginEdit()
    {
        if (m_editDepth++ == 0) m_pendingSnapshot = engine.GetSceneSerializer()->SerializeToString();
    }

    void UndoSystem::CommitEdit()
    {
        if (m_editDepth == 0) return;
        if (--m_editDepth != 0) return;
        if (!m_pendingSnapshot.has_value()) return;

        std::string before = std::move(*m_pendingSnapshot);
        m_pendingSnapshot.reset();

        if (before == engine.GetSceneSerializer()->SerializeToString()) return;

        m_undoStack.push_back(std::move(before));
        if (m_undoStack.size() > kMaxHistory) m_undoStack.erase(m_undoStack.begin());

        m_redoStack.clear();
    }

    void UndoSystem::RequestUndo()
    {
        if (CanUndo()) m_pendingAction = PendingAction::Undo;
    }

    void UndoSystem::RequestRedo()
    {
        if (CanRedo()) m_pendingAction = PendingAction::Redo;
    }

    bool UndoSystem::ProcessPending()
    {
        PendingAction action = m_pendingAction;
        m_pendingAction = PendingAction::None;

        if (action == PendingAction::Undo) return Undo();
        if (action == PendingAction::Redo) return Redo();
        return false;
    }

    bool UndoSystem::Undo()
    {
        if (m_undoStack.empty()) return false;

        m_redoStack.push_back(engine.GetSceneSerializer()->SerializeToString());

        std::string previous = std::move(m_undoStack.back());
        m_undoStack.pop_back();

        engine.GetSceneSerializer()->DeserializeFromString(previous);
        return true;
    }

    bool UndoSystem::Redo()
    {
        if (m_redoStack.empty()) return false;

        m_undoStack.push_back(engine.GetSceneSerializer()->SerializeToString());

        std::string next = std::move(m_redoStack.back());
        m_redoStack.pop_back();

        engine.GetSceneSerializer()->DeserializeFromString(next);
        return true;
    }

    void UndoSystem::ClearHistory()
    {
        m_undoStack.clear();
        m_redoStack.clear();
        m_pendingSnapshot.reset();
        m_editDepth = 0;
        m_pendingAction = PendingAction::None;
    }
} // namespace Wild
