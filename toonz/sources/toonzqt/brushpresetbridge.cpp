#include "toonzqt/brushpresetbridge.h"

BrushPresetBridge *BrushPresetBridge::instance() {
  static BrushPresetBridge theInstance;
  return &theInstance;
}
