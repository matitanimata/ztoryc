#pragma once

//=============================================================================
// CellKeyframeMode — modalità globale celle/chiavi stile Toon Boom Harmony.
//
// Tre stati che filtrano cosa fanno selezione e spostamento nell'xsheet/timeline:
//   Drawings — solo exposure (celle)          → TCellSelection      / LevelMoverTool
//   Keys     — solo keyframe (TStageObject)    → TKeyframeSelection  / KeyframeMoverTool(grid)
//   Both     — celle + keyframe insieme        → TCellKeyframeSelection / CellKeyframeMoverTool
//
// I tre mover esistono già (xsheetdragtool.cpp): questa modalità si limita a
// instradare selezione e drag verso quello giusto invece dell'euristica spaziale
// attuale (xshcellviewer.cpp ~4245, flag isKeySelection da Ctrl/Alt).
//
// FOUNDATION ONLY: per ora nessuno legge questo stato — il ricablaggio del
// routing è un passo successivo e separato.  Singleton header-only, niente Qt/
// moc finché non serve un segnale per la toolbar (allora si promuove a QObject).
//
// Vedi KEYS_CELS_MODES_DESIGN.md per il piano completo.
//=============================================================================

namespace XsheetGUI {

enum class CellKeyframeMode { Drawings, Keys, Both };

class CellKeyframeModeManager {
public:
  static CellKeyframeModeManager *instance() {
    static CellKeyframeModeManager theInstance;
    return &theInstance;
  }

  CellKeyframeMode getMode() const { return m_mode; }
  void setMode(CellKeyframeMode mode) { m_mode = mode; }

  void cycle() {
    m_mode = (m_mode == CellKeyframeMode::Drawings) ? CellKeyframeMode::Keys
             : (m_mode == CellKeyframeMode::Keys)   ? CellKeyframeMode::Both
                                                    : CellKeyframeMode::Drawings;
  }

private:
  CellKeyframeModeManager()  = default;
  ~CellKeyframeModeManager() = default;
  CellKeyframeModeManager(const CellKeyframeModeManager &)            = delete;
  CellKeyframeModeManager &operator=(const CellKeyframeModeManager &) = delete;

  // Default = Drawings → comportamento corrente preservato finché l'utente non
  // cambia modalità.
  CellKeyframeMode m_mode = CellKeyframeMode::Drawings;
};

}  // namespace XsheetGUI
