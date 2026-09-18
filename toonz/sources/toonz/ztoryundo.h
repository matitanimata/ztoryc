#pragma once

#include "tundo.h"
#include "ztorymodel.h"
#include "toonz/txshlevel.h"
#include "toonz/txshsoundcolumn.h"  // ColumnLevel

#include <QString>
#include <vector>

class StoryboardPanel;

// The first Board panel alive, or nullptr.  Needed by panels that mutate the
// storyboard from outside it (Thumbnail room export, Animatic) and therefore
// have to register their undo against a Board.
//
// NOTE: ztoryanimatic.cpp and ztorymonitorpanel.cpp each carry their own static
// copy of this, written before there was a shared home for it.  They are
// identical; new callers should use this one rather than making a fourth.
StoryboardPanel *ztoryFindBoardPanel();

// Snapshot of a single shot's state for undo/redo.
// TXshLevelP keeps the child level alive even after the xsheet column is deleted.
struct ZtoryShotSnap {
    ShotData   data;
    TXshLevelP level;
    int        duration;
};

// Posizione di UN livello audio nella timeline. Sono i tre soli numeri che la
// descrivono: il resto (quale suono, a che frame rate) non cambia mai per
// effetto di un'operazione del Board.
struct ZtoryAudioLevelSnap {
    ColumnLevel *level = nullptr;  // identita', non indice: uno shift RIORDINA
    int startFrame  = 0;
    int startOffset = 0;
    int endOffset   = 0;
};

// Lo stato di una colonna sonora.
struct ZtoryAudioColSnap {
    int col = -1;
    std::vector<ZtoryAudioLevelSnap> levels;
};

// Lo stato del Board per undo/redo: gli shot, e le posizioni dell'audio.
//
// ⚠️ L'audio serve perche' con il **link audio-video** acceso un'operazione sul
// video sposta anche le colonne sonore (ZtoryAnimaticPanel::resequenceXsheet),
// e lo snapshot non lo sapeva: l'undo rimetteva a posto gli shot e lasciava
// l'audio dove l'operazione l'aveva portato. Segnalato da Franco il 2026-09-18
// facendo «Match Subscene Duration».
//
// ⚠️ E NON basta rifare lo spostamento al contrario: shiftLevelFromFrame()
// **taglia** (setEndOffset) il livello precedente quando lo spostamento a
// sinistra lo farebbe sovrapporre. Quel taglio uno shift inverso non lo
// ripristina. Per questo qui si salva lo STATO, non il movimento.
//
// I metodi sotto esistono perche' quasi tutto il codice tratta lo snapshot come
// la sola lista di shot, ed e' giusto: l'audio riguarda solo l'undo.
struct ZtoryBoardSnap {
    std::vector<ZtoryShotSnap>     shots;
    std::vector<ZtoryAudioColSnap> audio;

    bool   empty() const { return shots.empty(); }
    void   clear()       { shots.clear(); audio.clear(); }
    size_t size()  const { return shots.size(); }
    const ZtoryShotSnap &operator[](size_t i) const { return shots[i]; }
};

// Legge dallo xsheet principale la posizione di ogni livello audio.
std::vector<ZtoryAudioColSnap> ztoryCaptureAudioSnap();
// Rimette quelle posizioni. Salta una colonna se la sua struttura e' cambiata
// (livelli creati o distrutti nel frattempo): li' un ripristino posizionale
// sarebbe un danno nuovo, non un undo.
void ztoryRestoreAudioSnap(const std::vector<ZtoryAudioColSnap> &snap);
// Vero se le due fotografie dell'audio differiscono, cioe' se l'operazione ha
// davvero mosso il suono. Undo e redo toccano l'audio SOLO in quel caso: cosi'
// un'operazione che non c'entra niente con l'audio non se lo trascina dietro.
bool ztoryAudioSnapDiffers(const std::vector<ZtoryAudioColSnap> &a,
                           const std::vector<ZtoryAudioColSnap> &b);

// Generic undo item for Board CRUD operations.
// Stores full before/after snapshots and calls restoreFromSnapshot on undo/redo.
class UndoBoardState final : public TUndo {
    StoryboardPanel           *m_panel;
    QString                    m_label;
    ZtoryBoardSnap             m_before;
    ZtoryBoardSnap             m_after;
    // Levels pulled out of the scene cast because the deleted shots were their
    // last users.  The smart pointers keep them alive while this undo lives, so
    // undo() can put them back and the snapshots' level pointers stay valid.
    std::vector<TXshLevelP>    m_removedLevels;
public:
    UndoBoardState(StoryboardPanel *panel, const QString &label,
                   ZtoryBoardSnap before,
                   ZtoryBoardSnap after,
                   std::vector<TXshLevelP> removedLevels = {})
        : m_panel(panel), m_label(label)
        , m_before(std::move(before)), m_after(std::move(after))
        , m_removedLevels(std::move(removedLevels)) {}

    void    undo() const override;
    void    redo() const override;
    int     getSize() const override { return sizeof(*this); }
    QString getHistoryString() override { return m_label; }
};
