#pragma once

#ifndef PLASTICVISUALSETTINGS_H

#include "tcommon.h"
#define PLASTICVISUALSETTINGS_H

//===========================================================

//    Forward declarations

class TXshColumn;

//===========================================================

//*********************************************************************************************
//    PlasticVisualSettings  definition
//*********************************************************************************************

#undef DVAPI
#undef DVVAR
#ifdef TNZEXT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//! The PlasticVisualSettings class stores the fundamental visualization options
//! that need
//! to be implemented in any painter supporting the plastic framework.

struct PlasticVisualSettings {
  bool m_applyPlasticDeformation;  //!< Whether the deformation must be applied.
                                   //! If false,
  //!< the original image should be displayed instead.
  TXshColumn *m_showOriginalColumn;  //!< As an exception to the above control,
                                     //! one specific
  //!< mesh column can be dispensed from deforming.
  //!< This is typically used in PlasticTool's 'build mode'.
  bool m_drawMeshesWireframe;  //!< Whether any mesh wireframe should be
                               //! displayed
  bool m_drawRigidity;         //!< Whether mesh rigidities should be displayed
  bool m_drawSO;               //!< Whether mesh vertices' stacking order should
                               //!< be displayed
public:
  //! Global, persistent mesh-wireframe visibility.
  /*! m_drawMeshesWireframe above is per-painter and, in practice, was only
      ever set while the Plastic tool was active: the setting could not be
      reached from anywhere else and did not survive a restart. This is the
      single source of truth -- the viewer reads it on every draw, the Show
      Mesh command writes it (and persists it), and the Plastic tool's own
      menu entry writes it too, so the two are views of one setting. */
  static DVVAR bool s_showMeshWireframe;

  PlasticVisualSettings()
      : m_applyPlasticDeformation(true)
      , m_showOriginalColumn()
      , m_drawMeshesWireframe(true)
      , m_drawRigidity(false)
      , m_drawSO(false) {}
};

#endif  // PLASTICVISUALSETTINGS_H
