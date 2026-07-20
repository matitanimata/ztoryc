#pragma once

#include "tundo.h"
#include "ztorymodel.h"
#include "toonz/txshlevel.h"

#include <QString>
#include <vector>

class StoryboardPanel;

// Snapshot of a single shot's state for undo/redo.
// TXshLevelP keeps the child level alive even after the xsheet column is deleted.
struct ZtoryShotSnap {
    ShotData   data;
    TXshLevelP level;
    int        duration;
};

// Generic undo item for Board CRUD operations.
// Stores full before/after snapshots and calls restoreFromSnapshot on undo/redo.
class UndoBoardState final : public TUndo {
    StoryboardPanel           *m_panel;
    QString                    m_label;
    std::vector<ZtoryShotSnap> m_before;
    std::vector<ZtoryShotSnap> m_after;
    // Levels pulled out of the scene cast because the deleted shots were their
    // last users.  The smart pointers keep them alive while this undo lives, so
    // undo() can put them back and the snapshots' level pointers stay valid.
    std::vector<TXshLevelP>    m_removedLevels;
public:
    UndoBoardState(StoryboardPanel *panel, const QString &label,
                   std::vector<ZtoryShotSnap> before,
                   std::vector<ZtoryShotSnap> after,
                   std::vector<TXshLevelP> removedLevels = {})
        : m_panel(panel), m_label(label)
        , m_before(std::move(before)), m_after(std::move(after))
        , m_removedLevels(std::move(removedLevels)) {}

    void    undo() const override;
    void    redo() const override;
    int     getSize() const override { return sizeof(*this); }
    QString getHistoryString() override { return m_label; }
};
