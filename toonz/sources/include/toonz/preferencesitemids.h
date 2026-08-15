#ifndef PREFERENCESITEMIDS_H
#define PREFERENCESITEMIDS_H

enum PreferencesItemId {
  // General
  defaultViewerEnabled,
  rasterOptimizedMemory,
  autosaveEnabled,
  autosavePeriod,
  autosaveSceneEnabled,
  autosaveOtherFilesEnabled,
  startupPopupEnabled,
  undoMemorySize,
  taskchunksize,
  replaceAfterSaveLevelAs,
  backupEnabled,
  backupKeepCount,
  sceneNumberingEnabled,
  watchFileSystemEnabled,
  projectRoot,
  customProjectRoot,
  pathAliasPriority,
  showAdvancedOptions,
  tipsPopupEnabled,

  //----------
  // Interface
  CurrentStyleSheetName,
  additionalStyleSheet,
  iconTheme,
  pixelsOnly,
  oldUnits,
  oldCameraUnits,
  linearUnits,
  cameraUnits,
  CurrentRoomChoice,
  functionEditorToggle,
  moveCurrentFrameByClickCellArea,
  actualPixelViewOnSceneEditingMode,
  showRasterImagesDarkenBlendedInViewer,
  iconSize,
  viewShrink,
  viewStep,
  viewerZoomCenter,
  CurrentLanguageName,
  interfaceFont,
  interfaceFontStyle,
  colorCalibrationEnabled,
  colorCalibrationLutPaths,
  showIconsInMenu,
  displayIn30bit,
  viewerIndicatorEnabled,
  highDpiScalingEnabled,
  iconSizePB,

  //----------
  // Visualization
  show0ThickLines,
  regionAntialias,
  rasterizeAntialias,

  //----------
  // Loading
  importPolicy,
  numberedFilesImportMode,
  autoExposeEnabled,
  subsceneFolderEnabled,
  removeSceneNumberFromLoadedLevelName,
  IgnoreImageDpi,
  rasterLevelCachingBehavior,
  // initialLoadTlvCachingBehavior, // deprecated
  columnIconLoadingPolicy,
  levelFormats,  // need to be handle separately
  autoRemoveUnusedLevels,

  //----------
  // Saving
  rasterBackgroundColor,
  resetUndoOnSavingLevel,
  defaultProjectPath,
  recordFileHistory,
  recordAsUsername,

  //----------
  // Import / Export
  ffmpegPath,
  ffmpegTimeout,
  fastRenderPath,
  ffmpegMultiThread,
  rhubarbPath,
  rhubarbTimeout,
  // Lip sync: true = Rhubarb's phonetic recognizer, false = PocketSphinx.
  // Remembered because the choice belongs to the production's language, not to
  // the single run: PocketSphinx only knows ENGLISH words, so on any other
  // language it was quietly trying to recognise English in Italian audio.
  lipSyncPhonetic,
  // Colora il nome del personaggio dentro il campo dialogo. Disattivabile
  // perche' su un progetto che non usa i personaggi ogni riga in maiuscolo
  // diventerebbe un avviso, cioe' rumore invece di informazione.
  dialogueSpeakerHighlight,
  // whisper.cpp: cartella dell'eseguibile e file del MODELLO.
  // Il modello e' separato e sta vuoto di default: pesa da 75 MB a qualche GB,
  // e Franco ha deciso (2026-08-15) che scaricarlo e' scelta dell'utente —
  // Ztoryc deve funzionare anche senza, col solo Rhubarb.
  whisperPath,
  whisperModel,
  // Lingua del lip sync. Sceglie anche il MOTORE: con una lingua per cui c'e'
  // un modello Vosk si usa l'allineatore forzato (tempi molto piu' precisi);
  // vuota = rilevamento automatico, cioe' Whisper. Non si deduce dalla lingua
  // dell'interfaccia: si lavora spesso su audio in una lingua diversa da
  // quella in cui si tiene il programma.
  lipSyncLanguage,
  // Anticipo in FOTOGRAMMI delle bocche rispetto al suono. Non e' una
  // correzione di un errore di misura: e' la convenzione dell'animazione —
  // l'occhio legge la bocca prima di sentire, e una bocca esattamente in
  // tempo sembra in ritardo. Regolabile perche' e' una scelta di stile e
  // dipende anche dalla cadenza del parlato.
  lipSyncLeadFrames,

  //----------
  // Drawing
  DefRasterFormat,
  // scanLevelType,// deprecated
  DefLevelType,
  newLevelSizeToCameraSizeEnabled,
  DefLevelWidth,
  DefLevelHeight,
  DefLevelDpi,
  // AutocreationType,// deprecated
  EnableAutocreation,
  KeyframesFollowExposure,  // i keyframe seguono le celle in insert/remove/extend
  GlobalKeyScope,  // Ztoryc: portata della chiave globale — 0 Stage, 1 Plastic, 2 All
  NumberingSystem,
  EnableAutoStretch,
  EnableImplicitHold,
  EnableCreationInHoldCells,
  EnableAutoRenumber,
  vectorSnappingTarget,
  saveUnpaintedInCleanup,
  minimizeSaveboxAfterEditing,
  useNumpadForSwitchingStyles,
  downArrowInLevelStripCreatesNewFrame,
  keepFillOnVectorSimplify,
  useHigherDpiOnVectorSimplify,

  //----------
  // Tools
  // dropdownShortcutsCycleOptions, // removed
  FillOnlysavebox,
  multiLayerStylePickerEnabled,
  cursorBrushType,
  cursorBrushStyle,
  cursorOutlineEnabled,
  levelBasedToolsDisplay,
  useCtrlAltToResizeBrush,
  temptoolswitchtimer,
  magnetNonLinearSliderEnabled,
  toolScale,

  //----------
  // Xsheet
  xsheetLayoutPreference,
  xsheetStep,
  xsheetAutopanEnabled,
  DragCellsBehaviour,
  pasteCellsBehavior,
  ignoreAlphaonColumn1Enabled,
  showKeyframesOnXsheetCellArea,
  showXsheetCameraColumn,
  useArrowKeyToShiftCellSelection,
  inputCellsWithoutDoubleClickingEnabled,
  shortcutCommandsWhileRenamingCellEnabled,
  showQuickToolbar,
  ztoryPerWorkflowQuickToolbar,  // Ztoryc: una Quick Toolbar per workflow
  // Ztoryc: quale «giro» di comandi nuovi e' gia' stato travasato nella Quick
  // Toolbar personale. Non compare nelle Preferenze: e' un segnaposto interno.
  ztoryQuickToolbarMigration,
  showXsheetBreadcrumbs,
  expandFunctionHeader,
  showColumnNumbers,
  unifyColumnVisibilityToggles,
  parentColorsInXsheetColumn,
  highlightLineEverySecond,
  syncLevelRenumberWithXsheet,
  currentTimelineEnabled,
  currentColumnColor,
  currentCellColor,
  levelNameDisplayType,
  showFrameNumberWithLetters,
  showDragBars,
  timelineLayoutPreference,
  showImagesInCellTooltip,
  showColumnParents,

  //----------
  // Animation
  keyframeType,
  autoBezierKeys,
  animationStep,
  modifyExpressionOnMovingReferences,

  //----------
  // Preview
  blanksCount,
  blankColor,
  rewindAfterPlayback,
  previewAlwaysOpenNewFlip,
  fitToFlipbook,
  generatedMovieViewEnabled,
  shortPlayFrameCount,
  inbetweenFlipDrawingCount,
  inbetweenFlipSpeed,

  //----------
  // Onion Skin
  onionSkinEnabled,
  onionPaperThickness,
  backOnionColor,
  frontOnionColor,
  onionInksOnly,
  onionSkinDuringPlayback,
  useOnionColorsForShiftAndTraceGhosts,
  animatedGuidedDrawing,

  //----------
  // Colors
  viewerBGColor,
  previewBGColor,
  useThemeViewerColors,
  levelEditorBoxColor,
  chessboardColor1,
  chessboardColor2,
  transpCheckInkOnWhite,
  transpCheckInkOnBlack,
  transpCheckPaint,

  //----------
  // Version Control
  SVNEnabled,
  automaticSVNFolderRefreshEnabled,
  latestVersionCheckEnabled,

  //----------
  // Touch / Tablet Settings
  // TounchGestureControl // Touch Gesture is a checkable command and not in
  // preferences.ini
  gestureUndoMethod,
  gestureRedoMethod,
  winInkEnabled,
  // This option will be shown & available only when WITH_WINTAB is defined
  useQtNativeWinInk,

  //----------
  // Others (not appeared in the popup)
  // Shortcut popup settings
  shortcutPreset,
  // Viewer context menu
  guidedDrawingType,
  guidedAutoInbetween,
  guidedInterpolationType,
  // OSX shared memory settings
  shmmax,
  shmseg,
  shmall,
  shmmni,
  //- obsoleted / unused members
  // interfaceFontWeight,
  // autoCreateEnabled,
  // animationSheetEnabled,
  // askForOverrideRender,
  // textureSize, // set to 0
  // LineTestFpsCapture,
  // guidedDrawingType,

  doNotShowPopupSaveScene,

  PreferencesItemCount
};

#endif
