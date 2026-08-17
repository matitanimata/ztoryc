# =============================================================================
# Single source of truth for Ztoryc semver (CMake, CI DMG names, generated headers).
# Bump only ZTORYC_VERSION_MAJOR / MINOR / PATCH here.
# =============================================================================
set(ZTORYC_VERSION_MAJOR 0)
set(ZTORYC_VERSION_MINOR 13)
set(ZTORYC_VERSION_PATCH 1)
set(ZTORYC_VERSION "${ZTORYC_VERSION_MAJOR}.${ZTORYC_VERSION_MINOR}.${ZTORYC_VERSION_PATCH}")

# Legacy in-app float: major.minor as one decimal (see getAppVersionString %.1f).
set(ZTORYC_APP_VERSION_FLOAT "${ZTORYC_VERSION_MAJOR}.${ZTORYC_VERSION_MINOR}f")
set(ZTORYC_APP_REVISION "${ZTORYC_VERSION_PATCH}")
