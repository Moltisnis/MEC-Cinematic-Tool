#pragma once
#include <d3d11.h>
#include <cstddef>
#include <cstring>
#include <memory>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>
#include <wrl/client.h>

// Saved recordings (2026-10-27; the user: "save every recording to Cinematic
// Tools\Recordings with all its parts, and load it in any session. also find a
// way to make them smaller"; Design_Mission_Capture_2026-10-27.md stage 1).
// A recording is written to Cinematic Tools\Recordings\<when> <where>\
// Recording.ctr: its plain data (Faith, the people, the models, the bundles,
// the sound track) in sections, and an index of the graphics card's parts its
// kept draws and shapes use (shader code, input layouts, states, buffers,
// textures). Each part's bytes are a file of their own in Recordings\Parts,
// named by their SHA-256, so a part two recordings use is there once. Every
// section and part is packed with Windows' own LZMS compression; frame data is
// put column by column, each value as its change from the frame before, first
// (lossless). Loading reads them back, makes the graphics card's objects again
// (at Present, a few each frame) and hands the sections to the replay. See the
// .cpp.
namespace replay_store
{
  // Plain data written into a section
  class Out
  {
  public:
    std::vector<unsigned char> Data;
    void Bytes(void const* p, size_t n)
    {
      unsigned char const* b = (unsigned char const*)p;
      if (n)
        Data.insert(Data.end(), b, b + n);
    }
    template <class T> void Put(T const& v)
    {
      static_assert(std::is_trivially_copyable<T>::value, "plain data only");
      Bytes(&v, sizeof(T));
    }
    void Text(std::string const& s)
    {
      Put((unsigned)s.size());
      Bytes(s.data(), s.size());
    }
    template <class T> void Vec(std::vector<T> const& v)
    {
      static_assert(std::is_trivially_copyable<T>::value, "plain data only");
      Put((unsigned long long)v.size());
      if (!v.empty())
        Bytes(v.data(), v.size() * sizeof(T));
    }
    // Frames of fixed size (rows of rowBytes), column by column, each
    // elementBytes-wide value as its change from the row before (smaller once
    // packed; lossless)
    void Frames(void const* data, size_t rows, size_t rowBytes, size_t elementBytes);
  };

  // Plain data read from a section (bounds checked: a short or bad one makes
  // Ok false, and every read after it fails)
  class In
  {
  public:
    In(void const* p, size_t n) : m_P((unsigned char const*)p), m_Left(n) {}
    bool Bytes(void* out, size_t n)
    {
      if (!m_Ok || n > m_Left)
      {
        m_Ok = false;
        return false;
      }
      if (n)
        memcpy(out, m_P, n);
      m_P += n;
      m_Left -= n;
      return true;
    }
    template <class T> bool Get(T& v)
    {
      static_assert(std::is_trivially_copyable<T>::value, "plain data only");
      return Bytes(&v, sizeof(T));
    }
    bool Text(std::string& s, size_t max = 1 << 16)
    {
      unsigned n = 0;
      if (!Get(n) || n > max || n > m_Left)
        return m_Ok = false;
      s.assign((char const*)m_P, n);
      m_P += n;
      m_Left -= n;
      return true;
    }
    template <class T> bool Vec(std::vector<T>& v, size_t max)
    {
      static_assert(std::is_trivially_copyable<T>::value, "plain data only");
      unsigned long long n = 0;
      if (!Get(n) || n > max || n * sizeof(T) > m_Left)
        return m_Ok = false;
      v.resize((size_t)n);
      return Bytes(v.data(), (size_t)n * sizeof(T));
    }
    // What Out::Frames wrote: rows x rowBytes bytes
    bool Frames(std::vector<unsigned char>& out, size_t maxBytes);
    bool Ok() const { return m_Ok; }
    size_t Left() const { return m_Left; }

  private:
    unsigned char const* m_P;
    size_t m_Left;
    bool m_Ok{ true };
  };

  // While saving: the game's addresses a recording keeps (entities, bodies,
  // buffers, as keys) written as numbers of its own, 1, 2, ... (0 stays 0)
  class Keys
  {
  public:
    long long Of(__int64 address);

  private:
    std::unordered_map<__int64, long long> m_Map;
  };
  // While loading: a key's address, in zeroed memory of the tools' (never one
  // of the game's: the replay finds nothing there, as for something gone)
  __int64 AddressOf(long long key);

  // The graphics card's objects a save takes (its parts): each added once
  // (held until it's written), an id for it; -1 for none, -2 for one that
  // can't be saved (a shader made before the tools were there, a texture of
  // a format the store doesn't know)
  struct SaveParts;
  std::shared_ptr<SaveParts> NewSaveParts();
  int AddPart(SaveParts& parts, IUnknown* object);
  // The objects a load made, by the ids they were saved with (null: not made)
  struct LoadParts;
  IUnknown* PartObject(LoadParts const& parts, int id);
  template <class T> Microsoft::WRL::ComPtr<T> PartAs(LoadParts const& parts, int id)
  {
    Microsoft::WRL::ComPtr<T> out;
    if (IUnknown* object = PartObject(parts, id))
      object->QueryInterface(__uuidof(T), (void**)out.GetAddressOf());
    return out;
  }

  // Sections (four-character tags)
  constexpr unsigned Tag(char a, char b, char c, char d)
  {
    return (unsigned)(unsigned char)a | (unsigned)(unsigned char)b << 8 | (unsigned)(unsigned char)c << 16 |
      (unsigned)(unsigned char)d << 24;
  }

  // Game thread: a recording to save, its sections written by the replay
  // (with the parts they use); the store writes it on its own threads
  struct SaveRequest
  {
    std::string Where;        // for its folder's name: the mission, or what's known
    double Seconds = 0.0;
    std::string Summary;      // one line for the list
    std::vector<std::pair<unsigned, Out>> Sections;
    std::shared_ptr<SaveParts> Parts;
  };
  void StartSave(SaveRequest&& request);

  // Game thread: a saved recording read back (false while another load or a
  // delete is going on)
  bool StartLoad(std::string const& folder);
  // Game thread: a load's sections and objects once all are made (once)
  struct Loaded
  {
    std::string Folder;
    std::vector<std::pair<unsigned, std::vector<unsigned char>>> Sections;
    std::shared_ptr<LoadParts> Parts;
    int PartsMissing = 0;     // in its index but not made (a part file gone, an object the device didn't make)
  };
  bool TakeLoaded(Loaded& out);
  bool Loading();

  // The menu: the saved recordings (newest first); refresh reads the folder
  // again (once a list is asked for, and after a save or a delete)
  struct Listed
  {
    std::string Folder;       // its folder's name (in Cinematic Tools\Recordings)
    std::string When;         // when it was saved (YYYY-MM-DD HH-MM-SS: the list's order)
    double Seconds = 0.0;
    std::string Summary;
    unsigned long long Bytes = 0;   // its file
  };
  void List(std::vector<Listed>& out);
  void Refresh();
  // A saved recording and the parts only it used, deleted (on the store's thread)
  void Delete(std::string const& folder);
  // A saved recording's folder renamed (on the store's thread; the name
  // without what a folder's name can't have, at most 60 characters; not over
  // another's). RenamedTo: a folder's new name once it's renamed in this
  // session ("" if it wasn't)
  void Rename(std::string const& folder, std::string const& name);
  std::string RenamedTo(std::string const& folder);
  // What it's doing, for the menu (empty when nothing)
  const char* Status(char* buffer, size_t size);
  // The parts folder's size (all recordings' parts), for the menu
  unsigned long long PartsBytes();

  // Render thread, from the draw log's device hooks: each shader's code as
  // the game makes it (for the parts a save takes)
  void NoteShader(void const* object, void const* code, size_t size);
  // Render thread, each Present: the device's CreateInputLayout hooked once;
  // a save's graphics card copies read back, a load's objects made
  void Present(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
}
