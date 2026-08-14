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
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QStandardPaths>

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

//! Diagnostica del disco, accesa con ZTORYC_DISC_DIAG=1. Scrive su file perche'
//! l'app si lancia dal Finder e uno standard output non c'e'. Gira solo alla
//! COMPILAZIONE dei punti di comando, non a ogni fotogramma.
void jointDiscDiag(const QString &joint, int meshIdx, double sideA,
                   double sideB, double radius) {
  static const bool on = (::getenv("ZTORYC_DISC_DIAG") != nullptr);
  if (!on) return;

  QFile f(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) +
          "/ztoryc_disc.log");
  if (!f.open(QIODevice::Append | QIODevice::Text)) return;
  QTextStream(&f) << QDateTime::currentDateTime().toString("HH:mm:ss")
                  << "  giunto=" << joint << " pezzo=" << meshIdx
                  << "  latoA=" << sideA << " latoB=" << sideB
                  << "  RAGGIO=" << radius << "\n";
}

//! ZtoRig — quanti punti compongono la corona di un disco. Otto bastano a
//! inchiodare l'interno: sono vincoli, non geometria da vedere, e ognuno costa
//! nel solver.
const int l_jointDiscPoints = 8;

//! Fin dove arriva la fascia di raccordo, in multipli del raggio del disco.
//! Fra il bordo del disco e questo limite i vertici passano gradualmente dal
//! disco all'osso: e' l'unica cosa che impedisce all'arto di strapparsi al
//! bordo, dove disco e osso ruotano di angoli diversi.
double l_discBlendOuter = 1.6;

//! Distanza dal punto \p p allo spigolo di CONTORNO piu' vicino, cioe' al bordo
//! del disegno. Uno spigolo e' di contorno quando tocca una faccia sola.
//! Torna -1 se la mesh non ne ha (non dovrebbe succedere).
//! Indice del pezzo che CONTIENE \p p, o -1. Un giunto appartiene al pezzo in
//! cui sta: e' quello, e solo quello, che il suo disco deve irrigidire.
//! ⚠️ \p p e' GIA' in spazio mesh: i punti di comando vengono trasformati con
//! deformationAffine e poi confrontati direttamente con la mesh in compile(),
//! quindi dopo quella trasformata sono nello spazio della geometria. Convertirli
//! ancora (l'errore del 2026-08-14) faceva uscire un raggio scalato per il DPI:
//! il disco inghiottiva mezzo braccio e lo ruotava rigido.
int meshContaining(const TMeshImage *meshImage, const TPointD &p) {
  const std::vector<TTextureMeshP> &meshes = meshImage->meshes();
  for (int m = 0; m != (int)meshes.size(); ++m)
    if (meshes[m]->faceContaining(p) >= 0) return m;
  return -1;
}

//----------------------------------------------------------------------------------

//! Distanza dal punto \p p al primo spigolo di CONTORNO incontrato andando
//! nella direzione \p dir. -1 se non se ne incontra nessuno.
double rayToMeshBoundary(const TTextureMesh &mesh, const TPointD &p,
                         const TPointD &dir) {
  double best = -1.0;

  for (int e = 0; e < mesh.edgesCount(); ++e) {
    if (mesh.edge(e).facesCount() != 1) continue;  // interno: non e' bordo

    const TPointD a = mesh.vertex(mesh.edge(e).vertex(0)).P();
    const TPointD b = mesh.vertex(mesh.edge(e).vertex(1)).P();

    // Raggio p+t*dir contro il segmento a+u*(b-a), con t>=0 e u in [0,1].
    const TPointD ab = b - a;
    const double den = cross(dir, ab);
    if (fabs(den) < 1e-12) continue;  // paralleli

    const TPointD ap = a - p;
    const double t   = cross(ap, ab) / den;
    const double u   = cross(ap, dir) / den;
    if (t < 0.0 || u < 0.0 || u > 1.0) continue;

    if (best < 0.0 || t < best) best = t;
  }
  return best;
}

//! Raggio del disco: **meta' della larghezza del braccio all'altezza del
//! giunto** (Franco, 2026-08-14 — «il cerchio deve essere grande quanto la
//! larghezza del braccio all'altezza del gomito»).
//!
//! Si misura attraversando l'arto, cioe' lungo la PERPENDICOLARE alla
//! bisettrice, contando i due lati. La distanza dal bordo piu' vicino — la
//! prima versione — al gomito prende l'angolo INTERNO della piega, che e'
//! molto meno di meta' larghezza: usciva un disco troppo piccolo per servire.
double jointDiscRadius(const TMeshImage *meshImage, int meshIdx,
                       const TPointD &p, const TPointD &bisector,
                       double &outSideA, double &outSideB) {
  const TTextureMesh &mesh = *meshImage->meshes()[meshIdx];
  const TPointD perp(-bisector.y, bisector.x);

  outSideA = rayToMeshBoundary(mesh, p, perp);
  outSideB = rayToMeshBoundary(mesh, p, -perp);

  if (outSideA < 0.0 && outSideB < 0.0) return -1.0;
  if (outSideA < 0.0) return outSideB;
  if (outSideB < 0.0) return outSideA;

  return 0.5 * (outSideA + outSideB);
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

//! Distanza dal punto \p p al segmento a-b, e posizione lungo di esso.
double distToSegment(const TPointD &p, const TPointD &a, const TPointD &b) {
  const TPointD ab = b - a, ap = p - a;
  const double len2 = norm2(ab);
  double t = (len2 > 1e-12) ? ((ap * ab) / len2) : 0.0;
  t        = std::max(0.0, std::min(1.0, t));
  return norm(ap - t * ab);
}

//! Assegna ogni vertice della maglia a un corpo rigido: il disco se ci sta
//! dentro, altrimenti l'osso piu' vicino. Fra il bordo del disco e l'inizio
//! della zona rigida dell'osso c'e' una FASCIA DI RACCORDO in cui i due si
//! mescolano — senza, al bordo del disco si aprirebbe una discontinuita' pari a
//! meta' dell'angolo di piega, e l'arto si strapperebbe.
void buildRigidBodies(DataGroup *group, const PlasticSkeleton &skeleton,
                      const TMeshImage *meshImage,
                      const TAffine &deformationAffine) {
  group->m_rigidBodies.clear();
  group->m_rigidOwner.clear();
  group->m_rigidWeight.clear();
  group->m_rigidOther.clear();
  if (!l_rigidJointDiscs) return;

  // Un corpo per disco...
  std::map<int, int> discBody;  // vertice del giunto -> indice del corpo
  for (const DataGroup::JointDisc &disc : group->m_jointDiscs) {
    DataGroup::RigidBody b;
    b.m_isDisc    = true;
    b.m_vIdx      = disc.m_vIdx;
    b.m_restOrigin = deformationAffine * skeleton.vertex(disc.m_vIdx).P();
    b.m_restAngle = disc.m_restAngle;
    discBody[disc.m_vIdx] = (int)group->m_rigidBodies.size();
    group->m_rigidBodies.push_back(b);
  }
  // ...e uno per osso (identificato dal vertice FIGLIO).
  std::map<int, int> boneBody;
  for (int v = 0; v != skeleton.verticesCount(); ++v) {
    const int p = skeleton.vertex(v).parent();
    if (p < 0) continue;
    DataGroup::RigidBody b;
    b.m_isDisc     = false;
    b.m_vIdx       = v;
    const TPointD a = deformationAffine * skeleton.vertex(p).P();
    const TPointD c = deformationAffine * skeleton.vertex(v).P();
    b.m_restOrigin = a;
    b.m_restAngle  = atan2(c.y - a.y, c.x - a.x);
    boneBody[v]    = (int)group->m_rigidBodies.size();
    group->m_rigidBodies.push_back(b);
  }
  if (group->m_rigidBodies.empty()) return;

  const int mCount = (int)meshImage->meshes().size();
  group->m_rigidOwner.resize(mCount);
  group->m_rigidWeight.resize(mCount);
  group->m_rigidOther.resize(mCount);

  for (int m = 0; m != mCount; ++m) {
    const TTextureMesh &mesh = *meshImage->meshes()[m];
    const int vCount         = mesh.verticesCount();
    group->m_rigidOwner[m].assign(vCount, -1);
    group->m_rigidWeight[m].assign(vCount, 1.0);
    group->m_rigidOther[m].assign(vCount, -1);

    for (int mv = 0; mv != vCount; ++mv) {
      const TPointD p = mesh.vertex(mv).P();

      // L'osso piu' vicino.
      int bestBone = -1;
      double bestD = 0.0;
      for (const auto &kv : boneBody) {
        const int v = kv.first, par = skeleton.vertex(v).parent();
        const double d =
            ::distToSegment(p, deformationAffine * skeleton.vertex(par).P(),
                            deformationAffine * skeleton.vertex(v).P());
        if (bestBone < 0 || d < bestD) bestD = d, bestBone = kv.second;
      }
      if (bestBone < 0) continue;

      // Il disco piu' vicino, se ci si e' dentro o nella sua fascia.
      int bestDisc     = -1;
      double bestDiscD = 0.0, bestDiscR = 0.0;
      for (const DataGroup::JointDisc &disc : group->m_jointDiscs) {
        const TPointD c = deformationAffine * skeleton.vertex(disc.m_vIdx).P();
        const double d  = norm(p - c);
        if (d > disc.m_radius * l_discBlendOuter) continue;
        if (bestDisc < 0 || d < bestDiscD)
          bestDiscD = d, bestDiscR = disc.m_radius,
          bestDisc  = discBody[disc.m_vIdx];
      }

      if (bestDisc < 0) {                       // fuori da ogni disco: osso puro
        group->m_rigidOwner[m][mv]  = bestBone;
        group->m_rigidWeight[m][mv] = 1.0;
        continue;
      }
      if (bestDiscD <= bestDiscR) {             // dentro il disco: disco puro
        group->m_rigidOwner[m][mv]  = bestDisc;
        group->m_rigidWeight[m][mv] = 1.0;
        continue;
      }
      // Nella fascia: si sfuma dal disco all'osso.
      const double t = (bestDiscD - bestDiscR) /
                       std::max(1e-9, bestDiscR * (l_discBlendOuter - 1.0));
      group->m_rigidOwner[m][mv]  = bestDisc;
      group->m_rigidOther[m][mv]  = bestBone;
      group->m_rigidWeight[m][mv] = 1.0 - std::min(1.0, std::max(0.0, t));
    }
  }
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

        // Il pezzo a cui il giunto appartiene: quello che lo CONTIENE. Senza
        // questo, la corona finiva dentro ogni pezzo sovrapposto — il braccio
        // sopra il corpo — e inchiodava anche quello alla rotazione del gomito.
        const int ownerMesh = ::meshContaining(meshImage, c);
        if (ownerMesh < 0) continue;  // giunto fuori dal disegno: niente disco

        // La perpendicolare alla bisettrice, nello stesso spazio del punto.
        const TPointD dirM =
            deformationAffine * (skeleton->vertex(v).P() + dir) - c;
        const double dirLen = norm(dirM);
        if (dirLen < 1e-9) continue;

        // Il raggio deciso a mano vince sulla misura; 0 = misuralo, negativo =
        // disco spento su questo giunto.
        const double chosen = skeleton->vertex(v).m_discRadius;
        if (chosen < 0.0) continue;

        double sideA = 0.0, sideB = 0.0;
        double r = (chosen > 0.0)
                       ? chosen
                       : ::jointDiscRadius(meshImage, ownerMesh, c,
                                           dirM * (1.0 / dirLen), sideA, sideB);
        ::jointDiscDiag(skeleton->vertex(v).name(), ownerMesh, sideA, sideB, r);
        if (r <= 0.0) continue;

        DataGroup::JointDisc disc;
        disc.m_vIdx   = v;
        disc.m_radius = r;
        disc.m_first  = (int)group->m_handles.size();
        disc.m_count  = l_jointDiscPoints;
        // Stessa bisettrice usata per misurare: nello spazio in cui si piazza la
        // corona, altrimenti una scala non uniforme falsa la rotazione.
        disc.m_restAngle = atan2(dirM.y, dirM.x);

        for (int i = 0; i != l_jointDiscPoints; ++i) {
          const double a = disc.m_restAngle + 2.0 * M_PI * i / l_jointDiscPoints;
          PlasticHandle handle(c + TPointD(r * cos(a), r * sin(a)));
          handle.m_meshIdx = ownerMesh;  // vale SOLO sul pezzo del giunto
          group->m_handles.push_back(handle);
        }
        group->m_jointDiscs.push_back(disc);
      }

      ::buildRigidBodies(group, *skeleton, meshImage, deformationAffine);

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
      // I dischi sono compilati una volta, lo scheletro deformato si ricostruisce
      // ad ogni fotogramma: se per qualunque motivo non combaciano, si salta
      // invece di leggere fuori. Ma allora anche m_dstHandles resterebbe piu'
      // corto di m_handles, quindi si riempie comunque, col centro.
      if (v < 0 || v >= deformedSkeleton.verticesCount()) {
        for (int i = 0; i != disc.m_count; ++i)
          group->m_dstHandles.push_back(TPointD());
        continue;
      }

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

  // ⚠️ Questo ciclo cammina sui punti di comando IN LOCKSTEP con la lista dei
  // vertici, quindi si ferma dove finiscono i vertici. Le corone dei dischi di
  // articolazione stanno IN CODA e non hanno un vertice a cui corrispondere:
  // proseguire porterebbe l'iteratore oltre la fine (crash del 2026-08-14,
  // `vertexDeformation()` su una stringa spazzatura — l'assert qui sotto c'era
  // ma in release non esiste).
  const int skelCount = skeleton->verticesCount();
  assert((int)group->m_handles.size() >= skelCount);

  int h, hCount = std::min((int)group->m_handles.size(), skelCount);
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

  // La corona di un disco prende l'ordine di sovrapposizione DEL SUO GIUNTO: il
  // disco appartiene all'articolazione, quindi si impila come lei. Lasciarlo a
  // zero (il default) tirava l'SO verso lo zero attorno a ogni giunto, ed e' il
  // motivo per cui il disegno si sfasciava ancora prima del crash.
  for (const DataGroup::JointDisc &disc : group->m_jointDiscs) {
    if (disc.m_vIdx < 0 || disc.m_vIdx >= skelCount) continue;
    const SkVD *vd = sd->vertexDeformation(skeleton->vertex(disc.m_vIdx).name());
    if (!vd || !vd->m_params[SkVD::SO]) continue;

    const double so = vd->m_params[SkVD::SO]->getValue(frame);
    for (int i = 0; i != disc.m_count; ++i) {
      const int idx = disc.m_first + i;
      if (idx < 0 || idx >= (int)group->m_handles.size()) continue;
      if (group->m_handles[idx].m_so != so) {
        group->m_handles[idx].m_so = so;
        changed                    = true;
      }
    }
  }

  if (changed) {
    // Rebuild SO minmax — QUI su tutti, corone comprese: i loro valori sono
    // legittimi e devono entrare nella scala.
    group->m_soMax = -(group->m_soMin = (std::numeric_limits<double>::max)());

    hCount = (int)group->m_handles.size();
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
            data.m_faceHints.empty() ? 0 : &data.m_faceHints.front(), m);
        data.m_deformer.releaseInitializedData();
      }

      group->m_compiled |= PlasticDeformerStorage::MESH;
    }

    const TPointD *dstHandlePos =
        group->m_dstHandles.empty() ? 0 : &group->m_dstHandles.front();

    for (m = 0; m != mCount; ++m) {
      PlasticDeformerData &data = group->m_datas[m];
      data.m_deformer.deform(dstHandlePos, data.m_output.get());

      // ZtoRig — i corpi rigidi. I vertici assegnati a un osso o a un disco
      // vengono PIAZZATI con la trasformazione rigida di quel corpo invece che
      // lasciati dove l'ARAP li ha messi: non si deformano, si spostano. E'
      // questo che fa di braccio e avambraccio due segmenti che ruotano attorno
      // al gomito, invece di un arco morbido.
      if (!group->m_rigidBodies.empty() && m < (int)group->m_rigidOwner.size()) {
        const PlasticSkeletonP &restSkel = sd->skeleton(skelId);
        PlasticSkeleton defSkel;
        sd->storeDeformedSkeleton(skelId, frame, defSkel);

        // Trasformazione corrente di ogni corpo: origine deformata e rotazione
        // rispetto al riposo.
        std::vector<TPointD> orig(group->m_rigidBodies.size());
        std::vector<double> rot(group->m_rigidBodies.size(), 0.0);
        for (size_t b = 0; b != group->m_rigidBodies.size(); ++b) {
          const DataGroup::RigidBody &rb = group->m_rigidBodies[b];
          if (rb.m_vIdx < 0 || rb.m_vIdx >= defSkel.verticesCount()) continue;

          if (rb.m_isDisc) {
            orig[b] = deformationAffine * defSkel.vertex(rb.m_vIdx).P();
            TPointD dir;
            if (::jointBisector(defSkel, rb.m_vIdx, dir)) {
              const TPointD d =
                  deformationAffine * (defSkel.vertex(rb.m_vIdx).P() + dir) - orig[b];
              rot[b] = atan2(d.y, d.x) - rb.m_restAngle;
            }
          } else {
            const int par = defSkel.vertex(rb.m_vIdx).parent();
            if (par < 0) continue;
            const TPointD a = deformationAffine * defSkel.vertex(par).P();
            const TPointD c = deformationAffine * defSkel.vertex(rb.m_vIdx).P();
            orig[b] = a;
            rot[b]  = atan2(c.y - a.y, c.x - a.x) - rb.m_restAngle;
          }
        }

        // ⚠️ Nella fascia si fonde la ROTAZIONE, non il risultato.
        //
        // Mediare due posizioni gia' ruotate (w*P1 + (1-w)*P2) taglia la corda
        // dell'arco: la stessa cosa che nell'esperimento sul giro di testa
        // faceva sbagliare la traiettoria del 13% a meta' rotazione. Qui si
        // vedeva come un MORSO all'interno del gomito — massa persa proprio
        // dove il disco doveva conservarla.
        //
        // Fondendo origine e angolo e applicando UNA trasformazione, le
        // distanze si conservano e il raccordo non accorcia niente.
        auto placeBlend = [&](int b1, int b2, double w, const TPointD &p) {
          const DataGroup::RigidBody &r1 = group->m_rigidBodies[b1];

          TPointD o = orig[b1];
          double a  = rot[b1];
          TPointD restO = r1.m_restOrigin;

          if (b2 >= 0 && w < 1.0) {
            const DataGroup::RigidBody &r2 = group->m_rigidBodies[b2];
            // Differenza d'angolo riportata in [-pi, pi]: senza, un raccordo a
            // cavallo del giro si avvolgerebbe dalla parte lunga.
            double da = rot[b2] - rot[b1];
            while (da > M_PI) da -= 2.0 * M_PI;
            while (da < -M_PI) da += 2.0 * M_PI;

            a     = rot[b1] + (1.0 - w) * da;
            o     = w * orig[b1] + (1.0 - w) * orig[b2];
            restO = w * r1.m_restOrigin + (1.0 - w) * r2.m_restOrigin;
          }

          const TPointD d = p - restO;
          const double c = cos(a), s2 = sin(a);
          return o + TPointD(c * d.x - s2 * d.y, s2 * d.x + c * d.y);
        };

        const TTextureMeshP &mesh = meshImage->meshes()[m];
        const int vCount          = mesh->verticesCount();
        double *out               = data.m_output.get();
        const std::vector<int> &owner  = group->m_rigidOwner[m];
        const std::vector<int> &other  = group->m_rigidOther[m];
        const std::vector<double> &wgt = group->m_rigidWeight[m];

        for (int v = 0; v < vCount && v < (int)owner.size(); ++v) {
          if (owner[v] < 0) continue;
          const TPointD rest = mesh->vertex(v).P();

          const TPointD p = placeBlend(owner[v], other[v], wgt[v], rest);
          out[2 * v]     = p.x;
          out[2 * v + 1] = p.y;
        }
      }

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

void PlasticDeformerStorage::setJointBlendPercent(double percent) {
  l_discBlendOuter = 1.0 + 0.01 * std::max(0.0, percent);
}
