#pragma once

#ifndef TVER_INCLUDED
#define TVER_INCLUDED

namespace TVER {

class ToonzVersion {
public:
  std::string getAppName(void);
  float getAppVersion(void);
  float getAppRevision(void);
  std::string getAppNote(void);
  bool hasAppNote(void);
  std::string getAppVersionString(void);
  std::string getAppRevisionString(void);
  /** Upstream engine lineage (keep in sync with TAHOMA2DVERSION in packaging scripts). */
  std::string getTahomaBaseVersionString(void);
  std::string getAppVersionInfo(std::string msg);

private:
  const char *applicationName     = "Ztoryc";
  /* The DISPLAYED version is a string, and it has to be: the float below cannot
     represent a two-digit minor at all — 0.10f IS 0.1f, the same number, so
     "10" was already lost before any formatting happened. That is how 0.10.0
     reached the splash screen as "0.1.0". The floats stay only because
     getAppVersion()/getAppRevision() are public API used elsewhere. */
  const char *applicationVersionStr  = "0.12";
  const char *applicationRevisionStr = "0";
  const float applicationVersion  = 0.12f;
  /* PATCH.0f — never use bare f (PATCH 0 becomes illegal token "0f"). */
  const float applicationRevision = 0.0f;
  const char *applicationNote     = "";
  const char *tahomaBaseVersion   = "1.6";
};

std::string ToonzVersion::getAppName(void) {
  std::string appname = applicationName;
  return appname;
}
float ToonzVersion::getAppVersion(void) {
  float appver = applicationVersion;
  return appver;
}
float ToonzVersion::getAppRevision(void) {
  float apprev = applicationRevision;
  return apprev;
}
std::string ToonzVersion::getAppNote(void) {
  std::string appnote = applicationNote;
  return appnote;
}
bool ToonzVersion::hasAppNote(void) { return *applicationNote != 0; }
std::string ToonzVersion::getAppVersionString(void) {
  /* Straight from the string: formatting the float printed 0.10 as "0.1". */
  return std::string(applicationVersionStr);
}
std::string ToonzVersion::getAppRevisionString(void) {
  return std::string(applicationRevisionStr);
}
std::string ToonzVersion::getTahomaBaseVersionString(void) {
  return std::string(tahomaBaseVersion);
}
std::string ToonzVersion::getAppVersionInfo(std::string msg) {
  std::string appinfo = std::string(applicationName);
  appinfo += " " + msg + " v";
  appinfo += getAppVersionString();
  appinfo += "." + getAppRevisionString();
  if (hasAppNote()) appinfo += " " + std::string(applicationNote);
  return appinfo;
}

}  // namespace TVER

#endif  // TVER_INCLUDED
