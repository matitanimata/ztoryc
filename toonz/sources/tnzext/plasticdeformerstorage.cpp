#include <memory>

// TnzExt includes
#include "ext/plasticskeleton.h"
#include "ext/plasticskeletondeformation.h"

// STD includes
#include <limits>
#include <map>
#include <algorithm>

// Boost includes
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/member.hpp>

// Qt includes
#include <QMutex>
#include <QMutexLocker>

#include "ext/plasticdeformerstorage.h"
#include "ext/plasticvisualsettings.h"

// Flag globale di visibilita' della mesh. DEVE stare a livello di file: la
// prima stesura era finita dentro il namespace anonimo qui sotto, quindi con
// collegamento interno, e il link da tnztools falliva.
bool PlasticVisualSettings::s_showMeshWireframe = true;

//***********************************************************************************************
//    Storage multi-index map  definition
//***********************************************************************************************

namespace {

typedef PlasticDeformerDataGroup DataGroup;

//----------------------------------------------------------------------------------

typedef std::pair<const SkD *, int> DeformedSkeleton;

//----------------------------------------------------------------------------------

struct Key {
  const TMeshImage *m_mi;
  DeformedSkeleton m_ds;

  std::shared_ptr<DataGroup> m_dataGroup;

public:
  Key(const TMeshImage *mi, const SkD *sd, int skelId)
      : m_mi(mi), m_ds(sd, skelId), m_dataGroup() {}

  bool operator<(const Key &other) const {
    return (m_mi < other.m_mi) ||
           ((!(other.m_mi < m_mi)) && (m_ds < other.m_ds));
  }
};

//----------------------------------------------------------------------------------

using namespace boost::multi_index;

typedef boost::multi_index_container<
    Key, indexed_by<

             ordered_unique<identity<Key>>,
             ordered_non_unique<tag<TMeshImage>,
                                member<Key, const TMeshImage *, &Key::m_mi>>,
             ordered_non_unique<tag<DeformedSkeleton>,
                                member<Key, DeformedSkeleton, &Key::m_ds>>

             >>
    DeformersSet;

typedef DeformersSet::nth_index<0>::type DeformersByKey;
typedef DeformersSet::index<TMeshImage>::type DeformersByMeshImage;
typedef DeformersSet::index<DeformedSkeleton>::type DeformersByDeformedSkeleton;

}  // namespace

//***********************************************************************************************
//    Initialization stage  functions
//***********************************************************************************************

namespace {

void initializeSO(PlasticDeformerData &data, const TTextureMeshP &mesh) {
  data.m_so.reset(new double[mesh->facesCount()]);
}

//----------------------------------------------------------------------------------

void initializeDeformerData(PlasticDeformerData &data,
                            const TTextureMeshP &mesh) {
  initializeSO(data, mesh);  // Allocates SO data

  // Also, allocate suitable input-output arrays for the deformation
  data.m_output.reset(new double[2 * mesh->verticesCount()]);
}

//----------------------------------------------------------------------------------

void initializeDeformersData(DataGroup *group, const TMeshImage *meshImage) {
  group->m_datas.reset(new PlasticDeformerData[meshImage->meshes().size()]);

  // Push a PlasticDeformer for each mesh in the image
  const std::vector<TTextureMeshP> &meshes = meshImage->meshes();
  int fTotal                               = 0;  // Also count total # of faces

  int m, mCount = meshes.size();
  for (m = 0; m != mCount; ++m) {
    fTotal += meshes[m]->facesCount();
    initializeDeformerData(group->m_datas[m], meshes[m]);
  }

  // Initialize the vector of sorted faces
  std::vector<std::pair<int, int>> &sortedFaces = group->m_sortedFaces;

  sortedFaces.reserve(fTotal);
  for (m = 0; m != mCount; ++m) {
    const TTextureMesh &mesh = *meshes[m];

    int f, fCount = mesh.facesCount();
    for (f = 0; f != fCount; ++f) sortedFaces.push_back(std::make_pair(f, m));
  }
}

}  // namespace

//***********************************************************************************************
//    Handle processing  functions
//***********************************************************************************************

namespace {

void transformHandles(std::vector<PlasticHandle> &handles, const TAffine &aff) {
  // Transforms handles through deformAff AND applies mi's dpi scale inverse
  std::vector<PlasticHandle>::size_type h, hCount = handles.size();
  for (h = 0; h != hCount; ++h) handles[h].m_pos = aff * handles[h].m_pos;
}

//----------------------------------------------------------------------------------

void transformHandles(std::vector<TPointD> &handles, const TAffine &aff) {
  // Transforms handles through deformAff AND applies mi's dpi scale inverse
  std::vector<PlasticHandle>::size_type h, hCount = handles.size();
  for (h = 0; h != hCount; ++h) handles[h] = aff * handles[h];
}

//----------------------------------------------------------------------------------

bool l_rigidJointDiscs = false;

//! ZtoRig — quanti punti compongono la corona di un disco. Otto bastano a
//! inchiodare l'interno: sono vincoli, non geometria da vedere, e ognuno costa
//! nel solver.
const int l_jointDiscPoints = 8;

//! Distanza dal punto \p p allo spigolo di CONTORNO piu' vicino, cioe' al bordo
//! del disegno. Uno spigolo e' di contorno quando tocca una faccia sola.
//! Torna -1 se la mesh non ne ha (non dovrebbe succedere).
double distanceToMeshBoundary(const TMeshImage *meshImage, const TPointD &p,
                              const TAffine &toMeshSpace) {
  double best = -1.0;

  for (const TTextureMeshP &mesh : meshImage->meshes()) {
    for (int e = 0; e < mesh->edgesCount(); ++e) {
      if (!mesh->edge(e).facesCount()) continue;
      if (mesh->edge(e).facesCount() != 1) continue;  // interno: non e' bordo

      const TPointD a = toMeshSpace * mesh->vertex(mesh->edge(e).vertex(0)).P();
      const TPointD b = toMeshSpace * mesh->vertex(mesh->edge(e).vertex(1)).P();

      const TPointD ab = b - a, ap = p - a;
      const double len2 = norm2(ab);
      double t = (len2 > 1e-12) ? ((ap * ab) / len2) : 0.0;
      t        = std::max(0.0, std::min(1.0, t));
      const double d = norm(ap - t * ab);

      if (best < 0.0 || d < best) best = d;
    }
  }
  return best;
}

//! Direzione della bisettrice del giunto \p v: la media delle direzioni verso il
//! padre e verso i figli. E' l'orientamento naturale del disco — i due ossi lo
//! incontrano a meta' strada ciascuno invece che uno solo portarselo dietro.
//! Torna false quando il giunto non e' un'articolazione (radice o estremita').
bool jointBisector(const PlasticSkeleton &skel, int v, TPointD &dir) {
  const int parent = skel.vertex(v).parent();
  if (parent < 0) return false;  // la radice non e' un'articolazione

  const TPointD c = skel.vertex(v).P();
  TPointD toParent = skel.vertex(parent).P() - c;
  if (norm2(toParent) < 1e-12) return false;
  toParent = normalize(toParent);

  TPointD toChild;
  int children = 0;
  for (int w = 0; w < skel.verticesCount(); ++w) {
    if (skel.vertex(w).parent() != v) continue;
    TPointD d = skel.vertex(w).P() - c;
    if (norm2(d) < 1e-12) continue;
    toChild = toChild + normalize(d);
    ++children;
  }
  if (!children) return false;  // estremita': non c'e' niente da articolare

  // Il disco guarda dove guarda l'angolo fra i due ossi. Con piu' figli si usa
  // la loro media, che degrada in modo sensato su una biforcazione.
  TPointD b = toChild - toParent;
  if (norm2(b) < 1e-12) b = toChild;   // ossi allineati: basta un riferimento
  if (norm2(b) < 1e-12) return false;

  dir = normalize(b);
  return true;
}

void processHandles(DataGroup *group, double frame, const TMeshImage *meshImage,
                    const SkD *sd, int skelId,
                    const TAffine &deformationAffine) {
  assert(sd);

  const PlasticSkeletonP &skeleton = sd->skeleton(skelId);

  if (!skeleton || skeleton->verticesCount() == 0) {
    group->m_handles.clear();
    group->m_dstHandles.clear();

    group->m_compiled |= PlasticDeformerStorage::HANDLES;
    group->m_upToDate |= PlasticDeformerStorage::HANDLES;

    return;
  }

  int mCount = meshImage->meshes().size();

  if (!(group->m_upToDate & PlasticDeformerStorage::HANDLES)) {
    // Compile handles if necessary
    if (!(group->m_compiled & PlasticDeformerStorage::HANDLES)) {
      // Build and transform handles
      group->m_handles = skeleton->verticesToHandles();
      ::transformHandles(group->m_handles, deformationAffine);

      // ZtoRig — le corone dei dischi di articolazione, IN CODA ai punti dello
      // scheletro: cosi' gli indici dei vertici restano quelli di sempre e
      // nessun altro codice se ne accorge.
      //
      // Si lavora nello spazio della mesh, cioe' DOPO deformationAffine, per
      // non dover riportare avanti e indietro il raggio, che si misura sulla
      // geometria.
      group->m_jointDiscs.clear();
      for (int v = 0; l_rigidJointDiscs && v != skeleton->verticesCount(); ++v) {
        TPointD dir;
        if (!::jointBisector(*skeleton, v, dir)) continue;

        const TPointD c = deformationAffine * skeleton->vertex(v).P();
        const double r  = ::distanceToMeshBoundary(meshImage, c, deformationAffine);
        if (r <= 0.0) continue;

        DataGroup::JointDisc disc;
        disc.m_vIdx   = v;
        disc.m_radius = r;
        disc.m_first  = (int)group->m_handles.size();
        disc.m_count  = l_jointDiscPoints;
        // La bisettrice va misurata nello stesso spazio in cui si piazza la
        // corona, altrimenti una scala non uniforme falsa la rotazione.
        const TPointD dirM = deformationAffine * (skeleton->vertex(v).P() + dir) - c;
        disc.m_restAngle   = atan2(dirM.y, dirM.x);

        for (int i = 0; i != l_jointDiscPoints; ++i) {
          const double a = disc.m_restAngle + 2.0 * M_PI * i / l_jointDiscPoints;
          group->m_handles.push_back(
              PlasticHandle(c + TPointD(r * cos(a), r * sin(a))));
        }
        group->m_jointDiscs.push_back(disc);
      }

      // Prepare a vector for handles' face hints
      for (int m = 0; m != mCount; ++m)
        group->m_datas[m].m_faceHints.resize(group->m_handles.size(), -1);

      group->m_compiled |= PlasticDeformerStorage::HANDLES;
    }

    // Then, build destination handles
    PlasticSkeleton
        deformedSkeleton;  // NOTE: Could this be moved to the group as well?
    sd->storeDeformedSkeleton(skelId, frame, deformedSkeleton);

    // Copy deformed skeleton data into input deformation parameters
    group->m_dstHandles = std::vector<TPointD>(
        deformedSkeleton.vertices().begin(), deformedSkeleton.vertices().end());
    ::transformHandles(group->m_dstHandles, deformationAffine);

    // ZtoRig — le corone, nello stesso ordine in cui sono state compilate.
    // Ogni disco si piazza RIGIDAMENTE: si sposta col giunto e ruota di quanto
    // e' ruotata la sua bisettrice. Nessun punto della corona ha liberta', ed
    // e' questo che impedisce alla massa attorno al giunto di deformarsi.
    for (const DataGroup::JointDisc &disc : group->m_jointDiscs) {
      const int v = disc.m_vIdx;

      TPointD dir;
      const TPointD cD = deformationAffine * deformedSkeleton.vertex(v).P();
      double angle     = disc.m_restAngle;  // giunto degenere: nessuna rotazione
      if (::jointBisector(deformedSkeleton, v, dir)) {
        const TPointD dirM =
            deformationAffine * (deformedSkeleton.vertex(v).P() + dir) - cD;
        angle = atan2(dirM.y, dirM.x);
      }

      for (int i = 0; i != disc.m_count; ++i) {
        const double a = angle + 2.0 * M_PI * i / disc.m_count;
        group->m_dstHandles.push_back(
            cD + TPointD(disc.m_radius * cos(a), disc.m_radius * sin(a)));
      }
    }

    group->m_upToDate |= PlasticDeformerStorage::HANDLES;
  }
}

}  // namespace

//***********************************************************************************************
//    Stacking Order processing  functions
//***********************************************************************************************

namespace {

bool updateHandlesSO(DataGroup *group, const SkD *sd, int skelId,
                     double frame) {
  assert(sd);

  const PlasticSkeletonP &skeleton = sd->skeleton(skelId);

  if (!skeleton || skeleton->verticesCount() == 0) {
    group->m_soMin = group->m_soMax = 0.0;
    return false;
  }

  // Copy SO values to data's handles
  // Return whether values changed with respect to previous ones
  bool changed = false;

  assert(group->m_handles.size() == skeleton->verticesCount());

  int h, hCount = group->m_handles.size();
  {
    tcg::list<PlasticSkeletonVertex>::iterator vt =
        skeleton->vertices().begin();

    for (h = 0; h != hCount; ++h, ++vt) {
      const SkVD *vd = sd->vertexDeformation(vt->name());
      if (!vd) continue;

      double so = vd->m_params[SkVD::SO]->getValue(frame);

      PlasticHandle &handle = group->m_handles[h];
      if (handle.m_so != so) {
        group->m_handles[h].m_so = so;
        changed                  = true;
      }
    }
  }

  if (changed) {
    // Rebuild SO minmax
    group->m_soMax = -(group->m_soMin = (std::numeric_limits<double>::max)());

    for (h = 0; h != hCount; ++h) {
      const double &so = group->m_handles[h].m_so;

      group->m_soMin = std::min(group->m_soMin, so);
      group->m_soMax = std::max(group->m_soMax, so);
    }
  }

  return changed;
}

//----------------------------------------------------------------------------------

// \p sd and \p skelId are here for the ZtoRig SO ownership: a mesh vertex can
// be declared to belong to a joint, and then takes that joint's SO exactly
// instead of the distance-blended value. That is what makes a clean cut at a
// bend possible at all — the blend is smooth by construction, so near a joint
// the two limbs' values always mix.
void interpolateSO(DataGroup *group, const TMeshImage *meshImage, const SkD *sd,
                   int skelId) {
  int m, mCount = meshImage->meshes().size();

  if (group->m_handles.size() == 0) {
    // No handles case, fill in with 0s

    for (m = 0; m != mCount; ++m) {
      const TTextureMesh &mesh  = *meshImage->meshes()[m];
      PlasticDeformerData &data = group->m_datas[m];

      std::fill(data.m_so.get(), data.m_so.get() + mesh.facesCount(), 0.0);
    }

    return;
  }

  // Apply handles' SO values to each mesh
  for (m = 0; m != mCount; ++m) {
    const TTextureMesh &mesh  = *meshImage->meshes()[m];
    PlasticDeformerData &data = group->m_datas[m];

    // Interpolate so values
    std::unique_ptr<double[]> verticesSO(new double[mesh.verticesCount()]);

    ::buildSO(verticesSO.get(), mesh, group->m_handles,
              &data.m_faceHints.front());

    // Ownership wins over the blend, applied BEFORE faces average their
    // vertices so the edge stays as hard as the mesh allows.
    if (sd && sd->hasSOOwners()) {
      const PlasticSkeletonP &skel = sd->skeleton(skelId);
      if (skel) {
        std::map<QString, int> handleOfName;
        {
          int h = 0;
          for (tcg::list<PlasticSkeletonVertex>::iterator vt =
                   skel->vertices().begin();
               vt != skel->vertices().end(); ++vt, ++h)
            handleOfName[vt->name()] = h;
        }
        const int vCount = mesh.verticesCount();
        for (int v = 0; v != vCount; ++v) {
          QString owner;
          if (!sd->soOwner(m, v, owner)) continue;
          std::map<QString, int>::const_iterator ht = handleOfName.find(owner);
          if (ht == handleOfName.end()) continue;  // joint gone: keep the blend
          if (ht->second < (int)group->m_handles.size())
            verticesSO[v] = group->m_handles[ht->second].m_so;
        }
      }
    }

    // Make the mean of each face's vertex values and store that
    int f, fCount = mesh.facesCount();
    for (f = 0; f != fCount; ++f) {
      int v0, v1, v2;
      mesh.faceVertices(f, v0, v1, v2);

      data.m_so[f] = (verticesSO[v0] + verticesSO[v1] + verticesSO[v2]) / 3.0;
    }
  }
}

//----------------------------------------------------------------------------------

struct FaceLess {
  const PlasticDeformerDataGroup *m_group;

public:
  FaceLess(const PlasticDeformerDataGroup *group) : m_group(group) {}

  bool operator()(const std::pair<int, int> &a, const std::pair<int, int> &b) {
    return (m_group->m_datas[a.second].m_so[a.first] <
            m_group->m_datas[b.second].m_so[b.first]);
  }
};

// Must be invoked after updateSO
void updateSortedFaces(PlasticDeformerDataGroup *group) {
  FaceLess comp(group);
  std::sort(group->m_sortedFaces.begin(), group->m_sortedFaces.end(), comp);
}

//----------------------------------------------------------------------------------

void processSO(DataGroup *group, double frame, const TMeshImage *meshImage,
               const SkD *sd, int skelId, const TAffine &deformationAffine) {
  // SO re-interpolate values along the mesh if either:
  //  1. Recompilation was requested (ie some vertex may have been
  //  added/removed)
  //  2. OR the value of one of the handle has changed

  bool interpolate = !(group->m_compiled & PlasticDeformerStorage::SO);

  if (!(group->m_upToDate &
        PlasticDeformerStorage::SO))  // implied by (interpolate == true)
  {
    interpolate = updateHandlesSO(group, sd, skelId, frame) ||
                  interpolate;  // Order is IMPORTANT

    if (interpolate) {
      interpolateSO(group, meshImage, sd, skelId);
      updateSortedFaces(group);
    }

    group->m_compiled |= PlasticDeformerStorage::SO;
    group->m_upToDate |= PlasticDeformerStorage::SO;
  }
}

}  // namespace

//***********************************************************************************************
//    Mesh Deform processing  functions
//***********************************************************************************************

namespace {

void processMesh(DataGroup *group, double frame, const TMeshImage *meshImage,
                 const SkD *sd, int skelId, const TAffine &deformationAffine) {
  if (!(group->m_upToDate & PlasticDeformerStorage::MESH)) {
    int m, mCount = meshImage->meshes().size();

    if (!(group->m_compiled & PlasticDeformerStorage::MESH)) {
      for (m = 0; m != mCount; ++m) {
        const TTextureMeshP &mesh = meshImage->meshes()[m];
        PlasticDeformerData &data = group->m_datas[m];

        data.m_deformer.initialize(mesh);
        data.m_deformer.compile(
            group->m_handles,
            data.m_faceHints.empty() ? 0 : &data.m_faceHints.front());
        data.m_deformer.releaseInitializedData();
      }

      group->m_compiled |= PlasticDeformerStorage::MESH;
    }

    const TPointD *dstHandlePos =
        group->m_dstHandles.empty() ? 0 : &group->m_dstHandles.front();

    for (m = 0; m != mCount; ++m) {
      PlasticDeformerData &data = group->m_datas[m];
      data.m_deformer.deform(dstHandlePos, data.m_output.get());

      // ZtoRig joint correctives: pose-space deformation on top of the ARAP
      // result. Reads the driving joint's base angle at this frame, so it
      // recomputes whenever the deform does (both keyed off `frame`). No-op
      // and cheap when the deformation has no correctives.
      if (sd->meshCorrectivesCount() > 0) {
        const TTextureMeshP &mesh = meshImage->meshes()[m];
        const int vCount          = mesh->verticesCount();
        double *out               = data.m_output.get();
        for (int v = 0; v < vCount; ++v) {
          const TPointD off = sd->meshCorrectiveOffset(m, v, frame);
          out[2 * v]     += off.x;
          out[2 * v + 1] += off.y;
        }
      }
    }

    group->m_upToDate |= PlasticDeformerStorage::MESH;
  }
}

}  // namespace

//***********************************************************************************************
//    PlasticDeformerData  implementation
//***********************************************************************************************

PlasticDeformerData::PlasticDeformerData() {}

//----------------------------------------------------------------------------------

PlasticDeformerData::~PlasticDeformerData() {}

//***********************************************************************************************
//    PlasticDeformerDataGroup  implementation
//***********************************************************************************************

PlasticDeformerDataGroup::PlasticDeformerDataGroup()
    : m_datas()
    , m_compiled(PlasticDeformerStorage::NONE)
    , m_upToDate(PlasticDeformerStorage::NONE)
    , m_outputFrame((std::numeric_limits<double>::max)())
    , m_soMin()
    , m_soMax() {}

//----------------------------------------------------------------------------------

PlasticDeformerDataGroup::~PlasticDeformerDataGroup() {}

//***********************************************************************************************
//    PlasticDeformerStorage::Imp  definition
//***********************************************************************************************

class PlasticDeformerStorage::Imp {
public:
  QMutex m_mutex;            //!< Access mutex - needed for thread-safety
  DeformersSet m_deformers;  //!< Set of deformers, ordered by mesh image,
                             //! deformation, and affine.

public:
  Imp() : m_mutex(QMutex::Recursive) {}
};

//***********************************************************************************************
//    PlasticDeformerStorage  implementation
//***********************************************************************************************

PlasticDeformerStorage::PlasticDeformerStorage() : m_imp(new Imp) {}

//----------------------------------------------------------------------------------

PlasticDeformerStorage::~PlasticDeformerStorage() {}

//----------------------------------------------------------------------------------

PlasticDeformerStorage *PlasticDeformerStorage::instance() {
  static PlasticDeformerStorage theInstance;
  return &theInstance;
}

//----------------------------------------------------------------------------------

PlasticDeformerDataGroup *PlasticDeformerStorage::deformerData(
    const TMeshImage *meshImage, const PlasticSkeletonDeformation *deformation,
    int skelId) {
  QMutexLocker locker(&m_imp->m_mutex);

  // Search for the corresponding deformation in the storage
  Key key(meshImage, deformation, skelId);

  DeformersByKey::iterator dt = m_imp->m_deformers.find(key);
  if (dt == m_imp->m_deformers.end()) {
    // No deformer was found. Allocate it.
    key.m_dataGroup = std::make_shared<PlasticDeformerDataGroup>();
    initializeDeformersData(key.m_dataGroup.get(), meshImage);

    dt = m_imp->m_deformers.insert(key).first;
  }

  return dt->m_dataGroup.get();
}

//----------------------------------------------------------------------------------

const PlasticDeformerDataGroup *PlasticDeformerStorage::process(
    double frame, const TMeshImage *meshImage,
    const PlasticSkeletonDeformation *deformation, int skelId,
    const TAffine &skeletonAffine, DataType dataType) {
  QMutexLocker locker(&m_imp->m_mutex);

  PlasticDeformerDataGroup *group =
      deformerData(meshImage, deformation, skelId);

  // On-the-fly checks for data invalidation
  if (group->m_skeletonAffine != skeletonAffine) {
    group->m_upToDate       = NONE;
    group->m_compiled       = NONE;
    group->m_skeletonAffine = skeletonAffine;
  }

  if (group->m_compiledWithDiscs != l_rigidJointDiscs) {
    group->m_upToDate            = NONE;
    group->m_compiled            = NONE;
    group->m_compiledWithDiscs   = l_rigidJointDiscs;
  }

  if (group->m_outputFrame != frame) {
    group->m_upToDate    = NONE;
    group->m_outputFrame = frame;
  }

  bool doMesh    = (dataType & MESH);
  bool doSO      = (dataType & SO) || doMesh;
  bool doHandles = (bool)dataType;

  // Process data
  if (doHandles)
    processHandles(group, frame, meshImage, deformation, skelId,
                   skeletonAffine);

  if (doSO)
    processSO(group, frame, meshImage, deformation, skelId, skeletonAffine);

  if (doMesh)
    processMesh(group, frame, meshImage, deformation, skelId, skeletonAffine);

  return group;
}

//----------------------------------------------------------------------------------

const PlasticDeformerDataGroup *PlasticDeformerStorage::processOnce(
    double frame, const TMeshImage *meshImage,
    const PlasticSkeletonDeformation *deformation, int skelId,
    const TAffine &skeletonAffine, DataType dataType) {
  PlasticDeformerDataGroup *group = new PlasticDeformerDataGroup;
  initializeDeformersData(group, meshImage);

  bool doMesh    = (dataType & MESH);
  bool doSO      = (dataType & SO) || doMesh;
  bool doHandles = (bool)dataType;

  // Process data
  if (doHandles)
    processHandles(group, frame, meshImage, deformation, skelId,
                   skeletonAffine);

  if (doSO)
    processSO(group, frame, meshImage, deformation, skelId, skeletonAffine);

  if (doMesh)
    processMesh(group, frame, meshImage, deformation, skelId, skeletonAffine);

  return group;
}

//----------------------------------------------------------------------------------

void PlasticDeformerStorage::invalidateMeshImage(const TMeshImage *meshImage,
                                                 int recompiledData) {
  QMutexLocker locker(&m_imp->m_mutex);

  DeformersByMeshImage &deformers = m_imp->m_deformers.get<TMeshImage>();

  DeformersByMeshImage::iterator dBegin(deformers.lower_bound(meshImage));
  if (dBegin == deformers.end()) return;

  DeformersByMeshImage::iterator dt, dEnd(deformers.upper_bound(meshImage));
  for (dt = dBegin; dt != dEnd; ++dt) {
    dt->m_dataGroup->m_outputFrame =
        (std::numeric_limits<double>::max)();  // Schedule for redeformation
    if (recompiledData)
      dt->m_dataGroup->m_compiled &=
          ~recompiledData;  // Schedule for recompilation, too
  }
}

//----------------------------------------------------------------------------------

void PlasticDeformerStorage::invalidateSkeleton(
    const PlasticSkeletonDeformation *deformation, int skelId,
    int recompiledData) {
  QMutexLocker locker(&m_imp->m_mutex);

  DeformedSkeleton ds(deformation, skelId);

  DeformersByDeformedSkeleton &deformers =
      m_imp->m_deformers.get<DeformedSkeleton>();

  DeformersByDeformedSkeleton::iterator dBegin(deformers.lower_bound(ds));
  if (dBegin == deformers.end()) return;

  DeformersByDeformedSkeleton::iterator dt, dEnd(deformers.upper_bound(ds));
  for (dt = dBegin; dt != dEnd; ++dt) {
    dt->m_dataGroup->m_outputFrame =
        (std::numeric_limits<double>::max)();  // Schedule for redeformation
    if (recompiledData)
      dt->m_dataGroup->m_compiled &=
          ~recompiledData;  // Schedule for recompilation, too
  }
}

//----------------------------------------------------------------------------------

void PlasticDeformerStorage::invalidateDeformation(
    const PlasticSkeletonDeformation *deformation, int recompiledData) {
  QMutexLocker locker(&m_imp->m_mutex);

  DeformersByDeformedSkeleton &deformers =
      m_imp->m_deformers.get<DeformedSkeleton>();

  DeformedSkeleton dsBegin(deformation, -(std::numeric_limits<int>::max)()),
      dsEnd(deformation, (std::numeric_limits<int>::max)());

  DeformersByDeformedSkeleton::iterator dBegin(deformers.lower_bound(dsBegin));
  DeformersByDeformedSkeleton::iterator dEnd(deformers.upper_bound(dsEnd));

  if (dBegin == dEnd) return;

  for (DeformersByDeformedSkeleton::iterator dt = dBegin; dt != dEnd; ++dt) {
    dt->m_dataGroup->m_outputFrame =
        (std::numeric_limits<double>::max)();  // Schedule for redeformation
    if (recompiledData)
      dt->m_dataGroup->m_compiled &=
          ~recompiledData;  // Schedule for recompilation, too
  }
}

//----------------------------------------------------------------------------------

void PlasticDeformerStorage::releaseMeshData(const TMeshImage *meshImage) {
  QMutexLocker locker(&m_imp->m_mutex);

  DeformersByMeshImage &deformers = m_imp->m_deformers.get<TMeshImage>();

  DeformersByMeshImage::iterator dBegin(deformers.lower_bound(meshImage));
  if (dBegin == deformers.end()) return;

  deformers.erase(dBegin, deformers.upper_bound(meshImage));
}

//----------------------------------------------------------------------------------

void PlasticDeformerStorage::releaseSkeletonData(const SkD *deformation,
                                                 int skelId) {
  QMutexLocker locker(&m_imp->m_mutex);

  DeformedSkeleton ds(deformation, skelId);

  DeformersByDeformedSkeleton &deformers =
      m_imp->m_deformers.get<DeformedSkeleton>();

  DeformersByDeformedSkeleton::iterator dBegin(deformers.lower_bound(ds));
  if (dBegin == deformers.end()) return;

  deformers.erase(dBegin, deformers.upper_bound(ds));
}

//----------------------------------------------------------------------------------

void PlasticDeformerStorage::releaseDeformationData(const SkD *deformation) {
  QMutexLocker locker(&m_imp->m_mutex);

  DeformersByDeformedSkeleton &deformers =
      m_imp->m_deformers.get<DeformedSkeleton>();

  DeformedSkeleton dsBegin(deformation, -(std::numeric_limits<int>::max)()),
      dsEnd(deformation, (std::numeric_limits<int>::max)());

  DeformersByDeformedSkeleton::iterator dBegin(deformers.lower_bound(dsBegin));
  DeformersByDeformedSkeleton::iterator dEnd(deformers.upper_bound(dsEnd));

  if (dBegin == dEnd) return;

  deformers.erase(dBegin, dEnd);
}

//----------------------------------------------------------------------------------

void PlasticDeformerStorage::clear() {
  QMutexLocker locker(&m_imp->m_mutex);

  m_imp->m_deformers.clear();
}

//====================================================================================

void PlasticDeformerStorage::setRigidJointDiscsEnabled(bool on) {
  l_rigidJointDiscs = on;
}

bool PlasticDeformerStorage::isRigidJointDiscsEnabled() {
  return l_rigidJointDiscs;
}
