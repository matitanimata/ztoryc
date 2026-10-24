
#include <iostream>

#include <tsystem.h>
#include <tconvert.h>
#include <tiio.h>

#include <tmetaimage.h>
#include <toonz/txshsimplelevel.h>

#include "tiio_tzm.h"



//===========================================================================


// Tahoma's TFrameId has no constructor from text: «0001» or «0001a» (the
// form TFrameId::expand() writes) read back here; -1 when not a number.
static TFrameId tzmFrameId(const std::string &text) {
  size_t k = 0;
  while (k < text.size() && isdigit((unsigned char)text[k])) ++k;
  if (k == 0 || k > 9) return TFrameId(-1);  // std::stoi would throw
  return TFrameId(std::stoi(text.substr(0, k)),
                  QString::fromStdString(text.substr(k)));
}

namespace {
  class TLevelWriterTzm;

  class TImageWriterTzm final : public TImageWriter {
  private:
    TLevelWriterTzm &m_writer;
    TFrameId m_frameId;
  public:
    TImageWriterTzm(const TFilePath &path, TLevelWriterTzm &writer, const TFrameId &frameId):
      TImageWriter(path), m_writer(writer), m_frameId(frameId) { }
    void save(const TImageP &image) override;
  };

  class TLevelWriterTzm final : public TLevelWriter {
  private:
    TVariant m_data;
    Tofstream *m_stream;

  public:
    TLevelWriterTzm(const TFilePath &path, TPropertyGroup *winfo):
      TLevelWriter(path, winfo)
    {
      // lock file for whole time of saving process to avoid collisions
      try {
        m_stream = new Tofstream(path);
      } catch (const std::exception &e) {
        throw TImageException(path, e.what());
      } catch (const TException &e) {
        throw TImageException(path, to_string(e.getMessage()));
      }

      m_data["type"].setString("tzm");
      m_data["version"].setDouble( 0.0 );
    }

    TImageWriterP getFrameWriter(TFrameId frameId) override
      { return TImageWriterP(new TImageWriterTzm(m_path, *this, frameId)); }

    void saveFrame(const TFrameId &frameId, const TImageP &image) {
      if (const TMetaImage *metaImage = dynamic_cast<const TMetaImage*>(image.getPointer())) {
        TMetaImage::Reader reader(*metaImage);
        TVariant &frameData = m_data["frames"][frameId.expand()];
        TVariant &objectsData = frameData["objects"];
        objectsData.setType(TVariant::List);
        for(TMetaObjectListCW::iterator i = reader->begin(); i != reader->end(); ++i) {
          if (*i) {
            TVariant &objectData = objectsData[ objectsData.size() ];
            objectData["type"].setString( (*i)->getTypeName() );
            objectData["data"] = (*i)->data();
          }
        }
      }
    }

    // Ztoryc: a destructor must never throw. It used to rethrow a failed write
    // as TImageException, and a throw out of a destructor is std::terminate:
    // the autosave of an Assistants level to a Google Drive folder that
    // refused the write closed the app (Messina, 2026-10-24). Now the partial
    // file is removed instead: LevelUpdater::close, writing to a temporary,
    // then fails with «cant find!» and the original stays as it was.
    ~TLevelWriterTzm() {
      bool written = false;
      try {
        m_data["creator"].setString( m_creator.toStdString() );
        m_data.toStream(*m_stream, true);
        written = true;
      } catch (const std::exception &e) {
        std::cerr << to_string(m_path.getWideString()) << ": " << e.what() << std::endl;
      } catch (const TException &e) {
        std::cerr << to_string(m_path.getWideString()) << ": "
                  << to_string(e.getMessage()) << std::endl;
      } catch (...) {
      }
      try {
        delete m_stream;
        if (!written) TSystem::deleteFile(m_path);
      } catch (...) {
      }
    }
  };

  void TImageWriterTzm::save(const TImageP &image)
    { m_writer.saveFrame(m_frameId, image); }
} // end of anonymous namespace for TLevelWriterTzm

//===========================================================================

namespace {
  class TLevelReaderTzm;

  class TImageReaderTzm final : public TImageReader {
  private:
    TLevelReaderTzm &m_reader;
    TFrameId m_frameId;
  public:
    TImageReaderTzm(const TFilePath &path, TLevelReaderTzm &reader, const TFrameId &frameId):
      TImageReader(path), m_reader(reader), m_frameId(frameId) { }
    TImageP load() override;
  };


  class TLevelReaderTzm final : public TLevelReader {
  private:
    TVariant m_data;
    TLevelP m_level;

    void warning(const std::string &msg)
      { std::cerr << to_string(m_path.getWideString()) << ": " << msg << std::endl; }

  public:
    TLevelReaderTzm(const TFilePath &path):
      TLevelReader(path) { }

    TImageReaderP getFrameReader(TFrameId frameId) override
      { return TImageReaderP(new TImageReaderTzm(m_path, *this, frameId)); }

    TLevelP loadInfo() override {
      try {
        Tifstream stream(m_path);
        m_data.fromStream(stream);
      } catch (const std::exception &e) {
        throw TImageException(m_path, e.what());
      } catch (const TException &e) {
        throw TImageException(m_path, to_string(e.getMessage()));
      }

      if (m_data["type"].getString() != "tzm")
        warning("seems it's not TZM");
      if (m_data["version"].getDouble() > 0.0 + TConsts::epsilon)
        warning( "version ("
               + std::to_string(m_data["version"].getDouble())
               + ") is higher than supported (0.0)");

      const TVariantMap &map = m_data["frames"].getMap();
      for(TVariantMap::const_iterator i = map.begin(); i != map.end(); ++i) {
        TFrameId frameId = tzmFrameId(i->first.str());
        if (frameId.getNumber() < 0)
          warning("wrong frame number: " + i->first.str());
        else
        if (m_level->getTable()->count(frameId))
          warning(frameId.expand());
        else
          m_level->setFrame(frameId, TImageP());
      }

      return m_level;
    }

    QString getCreator() override
      { return QString::fromStdString( m_data["creator"].getString() ); }

    TImageP loadFrame(const TFrameId &frameId) {
      const TVariantMap &map = m_data["frames"].getMap();
      for(TVariantMap::const_iterator i = map.begin(); i != map.end(); ++i) {
        // Saved under frameId.expand(); Tahoma's TFrameId has no string
        // constructor, so the key is compared as written.
        if (i->first.str() == frameId.expand()) {
          TMetaImage *image = new TMetaImage();
          TMetaImage::Writer writer(*image);
          const TVariant &objectsData = i->second["objects"];
          if (objectsData.getType() == TVariant::List) {
            for(int j = 0; j < objectsData.size(); ++j) {
              const TVariant &objectData = objectsData[j];
              if (!objectData["type"].getString().empty()) {
                TMetaObjectP obj( new TMetaObject(objectData["type"].getString()) );
                obj->data() = objectData["data"];
                writer->push_back(obj);
              }
            }
          }
          return image;
        }
      }
      return TImageP();
    }
  };

  TImageP TImageReaderTzm::load()
    { return m_reader.loadFrame(m_frameId); }
} // end of anonymous namespace for TLevelReaderTzm

//===========================================================================

namespace tzm {
  TLevelWriter* createWriter(const TFilePath &path, TPropertyGroup *winfo)
    { return new TLevelWriterTzm(path, winfo); }
  TLevelReader* createReader(const TFilePath &path)
    { return new TLevelReaderTzm(path); }
}

//=============================================================================
