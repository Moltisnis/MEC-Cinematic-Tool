#include "WorldImages.h"
#include "GreenPlane.h"
#include "D3DStateBackup.h"
#include "Panini.h"
#include "../Main.h"
#include "../Util/Util.h"
#include "../imgui/imgui.h"
#include "../Util/ImGuiEXT.h"
#include "WorldImagesVs.h"
#include "WorldImagesPs.h"

#include <DirectXMath.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include <shellapi.h>
#include <wincodec.h>
#include <wrl/client.h>

#pragma comment(lib, "windowscodecs.lib")

using Microsoft::WRL::ComPtr;
using namespace DirectX;

namespace
{
  // The files read from the folder
  const wchar_t* const Extensions[] = { L".jpg", L".jpeg", L".png" };
  // Bigger pictures are scaled down to this many pixels on their longer side
  const UINT MaxSide = 4096;
  // A picture's width in the world (metres), and how far from the world's
  // middle it can be put
  const float MinWidth = 0.05f, MaxWidth = 2000.f;
  const float MaxPlace = 100000.f;
  // A new picture is this wide for each metre it is from the camera (about a
  // third of the screen's width at the game's usual field of view)
  const float WidthPerMetre = 0.4f;
  // Put on a clicked surface: this far out from it (metres)
  const float SurfaceGap = 0.02f;
  // How far behind the game's surface a picture's pixel may be and still be
  // drawn (metres, plus this much for each metre away), so one put flat on a
  // wall doesn't flicker with it
  const float DepthSlack = 0.02f, DepthSlackPerMetre = 0.0005f;
  // A click farther than this (metres) hit the sky
  const float NothingThere = 20000.f;
  // The game's depth read round a click: this many texels each side, and
  // how far apart the texels for the surface's direction are
  const int PickReach = 3;
  const int PickSide = PickReach * 2 + 1;
  const int PickStep = 2;
  // Frames a click's depth is waited for before it's given up
  const int PickPatience = 60;
  // How far a drag with the wheel can take a picture from the camera, and
  // one wheel step (as the lights' dots)
  const float DragNearest = 0.25f, DragFarthest = 2000.f;
  const float WheelStep = 1.15f;
  // The menu drew the dots this recently: the camera is still wanted (ms)
  const ULONGLONG DotsWantView = 500;
  const float Degrees = 0.01745329252f;

  struct Constants
  {
    XMFLOAT4X4 ViewProjection;
    XMFLOAT4 Corners[4];
    XMFLOAT4 Look;
    XMFLOAT4 Depth;
    XMFLOAT4 Projection;
  };

  // A file's picture, shared by every image showing it
  struct Picture
  {
    enum Status { Loading, Ready, Failed };
    Status State = Loading;
    UINT Width = 0, Height = 0;           // as drawn (turned upright, scaled down to MaxSide)
    UINT FileWidth = 0, FileHeight = 0;   // as in the file
    std::vector<std::vector<BYTE>> Levels;   // premultiplied RGBA, the full size first; until made into the texture
    std::string Error;
    ComPtr<ID3D11ShaderResourceView> View;   // render thread
  };

  // One picture put in the world
  struct WorldImage
  {
    int Id = 0;
    std::string File;   // UTF-8, in the folder
    bool Shown = true;
    float Position[3]{};   // its middle
    // Degrees: its front (the side it's seen from) round the upright (0
    // towards +z), up or down from level, and the picture turned round its front
    float Yaw = 0.f, Pitch = 0.f, Roll = 0.f;
    float Width = 4.f;     // metres; its height follows the picture's shape
    float Opacity = 1.f, Brightness = 1.f;
    bool FaceCamera = false, BothSides = true, Mirror = false;
    float Pushed = 0.f;    // the menu's "Out / in" drag so far (not saved)
  };

  // Under g_Mutex: the menu, the overlay and Present on the render thread,
  // the config on the main thread, the end of each loading thread
  std::mutex g_Mutex;
  std::vector<WorldImage> g_Images;
  int g_NextId = 1;
  std::map<std::string, std::shared_ptr<Picture>> g_Pictures;
  std::vector<std::string> g_Files;     // the folder's pictures, UTF-8
  bool g_Scanned = false;
  int g_SelectedFile = -1;
  float g_Distance = 10.f;              // in front of the camera (and a click on the sky)
  std::string g_Status;                 // what the last placing did, for the menu
  std::vector<int> g_OpenIds;           // open in the menu: their outlines are drawn
  int g_OpenIdsFrame = -10;             // the menu frame they were listed in (not shown: not listed)
  // Put in front of the camera at the next frame: a new picture of the file
  // (target 0) or the image with that id moved
  bool g_FrontPending = false;
  int g_FrontTarget = 0;
  std::string g_FrontFile;
  // Placing by click: armed (-1 not, else the target as above), then the
  // click's place (a fraction of the screen's width and height) waiting for
  // Present
  int g_Armed = -1;
  std::string g_ArmedFile;
  bool g_ClickPending = false;
  float g_ClickU = 0.f, g_ClickV = 0.f;
  int g_ClickTarget = 0;
  std::string g_ClickFile;
  // The dots (render thread): one at each picture's middle while the menu
  // is open (always: there's no tick for them), dragged with the mouse as
  // the lights' are (LightManager)
  bool g_DragAlongGround = false;
  int g_Selected = 0;              // the picture whose dot was clicked last: ringed
  bool g_OpenRequest = false;      // the Images header opened at the menu's next frame (TakeOpenRequest)
  bool g_OpenSelected = false;     // and that picture opened in it
  int g_DragId = 0;                // the picture held with the mouse, 0: none
  bool g_DragUpDown = false;       // Shift held: only up and down
  float g_DragBase[3]{};           // where the drag measures from (moved by the wheel)
  float g_DragOffsetX = 0.f, g_DragOffsetY = 0.f;   // the dot from the cursor when grabbed
  bool g_DragMoved = false;
  ULONGLONG g_OverlayTick = 0;     // when the menu last drew the overlay: the camera is kept for the dots
  // The last frame's camera, for the outlines and the dots
  bool g_ViewOk = false;
  float g_ViewTransform[16]{}, g_ViewProjection[16]{};
  bool g_RightHanded = true;   // the camera's right x up is its backward

  // The click's depth copied, read back a frame or more later (render thread, under g_Mutex)
  struct Pick
  {
    bool Copied = false;
    int Waited = 0;
    int Target = 0;
    std::string File;
    float Transform[16]{}, Projection[16]{};
    float ScaleX = 1.f, ScaleY = 1.f;
    UINT Width = 0, Height = 0;
    int BoxX = 0, BoxY = 0;        // the block's corner in the depth copy
    int ClickX = 0, ClickY = 0;    // the clicked texel in the block
    ComPtr<ID3D11Texture2D> Staging;
    D3D11_TEXTURE2D_DESC Desc{};
  } g_Pick;

  // Render thread only
  ComPtr<ID3D11VertexShader> g_Vs;
  ComPtr<ID3D11PixelShader> g_Ps;
  ComPtr<ID3D11Buffer> g_Constants;
  ComPtr<ID3D11RasterizerState> g_Rasterizer;
  ComPtr<ID3D11BlendState> g_Blend;
  ComPtr<ID3D11DepthStencilState> g_Depth;
  ComPtr<ID3D11SamplerState> g_Sampler;
  bool g_Made = false, g_Failed = false;
  D3DStateBackup g_Backup;   // big: its class instance lists

  std::string Utf8(std::wstring const& text)
  {
    if (text.empty())
      return std::string();
    int length = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), nullptr, 0, nullptr, nullptr);
    std::string out(length > 0 ? length : 0, '\0');
    if (length > 0)
      WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), &out[0], length, nullptr, nullptr);
    return out;
  }

  std::wstring Wide(std::string const& text)
  {
    if (text.empty())
      return std::wstring();
    int length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), (int)text.size(), nullptr, 0);
    std::wstring out(length > 0 ? length : 0, L'\0');
    if (length > 0)
      MultiByteToWideChar(CP_UTF8, 0, text.c_str(), (int)text.size(), &out[0], length);
    return out;
  }

  // "Cinematic Tools\Images" next to the game's exe
  std::wstring Folder()
  {
    wchar_t path[MAX_PATH * 2];
    DWORD length = GetModuleFileNameW(NULL, path, MAX_PATH * 2);
    std::wstring exe(path, length < MAX_PATH * 2 ? length : 0);
    size_t slash = exe.find_last_of(L"\\/");
    return (slash == std::wstring::npos ? std::wstring(L".") : exe.substr(0, slash)) + L"\\Cinematic Tools\\Images";
  }

  bool IsPictureName(wchar_t const* name)
  {
    wchar_t const* dot = wcsrchr(name, L'.');
    if (!dot)
      return false;
    for (wchar_t const* extension : Extensions)
      if (_wcsicmp(dot, extension) == 0)
        return true;
    return false;
  }

  // Under g_Mutex: the folder's pictures listed again (it's made if missing)
  void Scan()
  {
    g_Scanned = true;
    std::string selected = g_SelectedFile >= 0 && g_SelectedFile < (int)g_Files.size() ? g_Files[g_SelectedFile] : "";
    g_Files.clear();
    std::wstring folder = Folder();
    CreateDirectoryW(folder.c_str(), nullptr);
    WIN32_FIND_DATAW data;
    HANDLE find = FindFirstFileW((folder + L"\\*").c_str(), &data);
    if (find != INVALID_HANDLE_VALUE)
    {
      do
      {
        if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && IsPictureName(data.cFileName))
          g_Files.push_back(Utf8(data.cFileName));
      } while (FindNextFileW(find, &data));
      FindClose(find);
    }
    std::sort(g_Files.begin(), g_Files.end(),
      [](std::string const& a, std::string const& b) { return _stricmp(a.c_str(), b.c_str()) < 0; });
    g_SelectedFile = g_Files.empty() ? -1 : 0;
    for (size_t i = 0; i < g_Files.size(); ++i)
      if (g_Files[i] == selected)
        g_SelectedFile = (int)i;
  }

  // A JPG's EXIF orientation as a turn (the plain turns only: 90, 180, 270
  // degrees; a mirrored orientation is left as it is)
  WICBitmapTransformOptions ExifTurn(IWICBitmapFrameDecode* pFrame)
  {
    ComPtr<IWICMetadataQueryReader> pReader;
    if (FAILED(pFrame->GetMetadataQueryReader(pReader.GetAddressOf())) || !pReader)
      return WICBitmapTransformRotate0;
    PROPVARIANT value;
    PropVariantInit(&value);
    WICBitmapTransformOptions turn = WICBitmapTransformRotate0;
    if (SUCCEEDED(pReader->GetMetadataByName(L"/app1/ifd/{ushort=274}", &value)) && value.vt == VT_UI2)
    {
      if (value.uiVal == 3) turn = WICBitmapTransformRotate180;
      else if (value.uiVal == 6) turn = WICBitmapTransformRotate90;
      else if (value.uiVal == 8) turn = WICBitmapTransformRotate270;
    }
    PropVariantClear(&value);
    return turn;
  }

  std::string LoadError(HRESULT hr)
  {
    char text[160];
    if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) || hr == HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND))
      sprintf_s(text, "the file isn't in the folder");
    else if (hr == WINCODEC_ERR_COMPONENTNOTFOUND || hr == WINCODEC_ERR_UNKNOWNIMAGEFORMAT)
      sprintf_s(text, "Windows can't read it as a picture");
    else if (hr == E_OUTOFMEMORY)
      sprintf_s(text, "not enough memory");
    else
      sprintf_s(text, "Windows' picture reader failed (HRESULT 0x%08X)", (unsigned)hr);
    return text;
  }

  // The next smaller size (a mipmap): each pixel the average of the 2 x 2 above it
  std::vector<BYTE> HalfSize(std::vector<BYTE> const& level, UINT width, UINT height, UINT& halfWidth, UINT& halfHeight)
  {
    halfWidth = width > 1 ? width / 2 : 1;
    halfHeight = height > 1 ? height / 2 : 1;
    std::vector<BYTE> half((size_t)halfWidth * halfHeight * 4);
    for (UINT y = 0; y < halfHeight; ++y)
    {
      UINT y0 = y * 2 < height ? y * 2 : height - 1, y1 = y * 2 + 1 < height ? y * 2 + 1 : height - 1;
      for (UINT x = 0; x < halfWidth; ++x)
      {
        UINT x0 = x * 2 < width ? x * 2 : width - 1, x1 = x * 2 + 1 < width ? x * 2 + 1 : width - 1;
        BYTE const* a = &level[((size_t)y0 * width + x0) * 4];
        BYTE const* b = &level[((size_t)y0 * width + x1) * 4];
        BYTE const* c = &level[((size_t)y1 * width + x0) * 4];
        BYTE const* d = &level[((size_t)y1 * width + x1) * 4];
        BYTE* out = &half[((size_t)y * halfWidth + x) * 4];
        for (int i = 0; i < 4; ++i)
          out[i] = (BYTE)((a[i] + b[i] + c[i] + d[i] + 2) / 4);
      }
    }
    return half;
  }

  // A loading thread: the file read with Windows' own decoders (WIC), turned
  // as its EXIF orientation says, scaled down to MaxSide, premultiplied, and
  // its smaller sizes made; handed to the picture at the end
  void Load(std::shared_ptr<Picture> picture, std::wstring path, std::string name)
  {
    std::vector<std::vector<BYTE>> levels;
    UINT width = 0, height = 0, fileWidth = 0, fileHeight = 0;
    HRESULT hr = S_OK;
    try
    {
      HRESULT co = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
      {
        ComPtr<IWICImagingFactory> pFactory;
        ComPtr<IWICBitmapDecoder> pDecoder;
        ComPtr<IWICBitmapFrameDecode> pFrame;
        ComPtr<IWICBitmapSource> pSource;
        hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(pFactory.GetAddressOf()));
        if (SUCCEEDED(hr))
          hr = pFactory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand,
            pDecoder.GetAddressOf());
        if (SUCCEEDED(hr))
          hr = pDecoder->GetFrame(0, pFrame.GetAddressOf());
        if (SUCCEEDED(hr))
        {
          pSource = pFrame;
          hr = pFrame->GetSize(&fileWidth, &fileHeight);
        }
        if (SUCCEEDED(hr))
        {
          WICBitmapTransformOptions turn = ExifTurn(pFrame.Get());
          ComPtr<IWICBitmapFlipRotator> pRotator;
          if (turn != WICBitmapTransformRotate0 && SUCCEEDED(pFactory->CreateBitmapFlipRotator(pRotator.GetAddressOf())) &&
              SUCCEEDED(pRotator->Initialize(pSource.Get(), turn)))
            pSource = pRotator;
          hr = pSource->GetSize(&width, &height);
        }
        if (SUCCEEDED(hr) && (!width || !height))
          hr = WINCODEC_ERR_UNKNOWNIMAGEFORMAT;
        if (SUCCEEDED(hr) && (width > MaxSide || height > MaxSide))
        {
          double scale = (double)MaxSide / (double)(width > height ? width : height);
          UINT w = (UINT)(width * scale + 0.5), h = (UINT)(height * scale + 0.5);
          w = w ? w : 1;
          h = h ? h : 1;
          ComPtr<IWICBitmapScaler> pScaler;
          hr = pFactory->CreateBitmapScaler(pScaler.GetAddressOf());
          if (SUCCEEDED(hr))
            hr = pScaler->Initialize(pSource.Get(), w, h, WICBitmapInterpolationModeFant);
          if (SUCCEEDED(hr))
          {
            pSource = pScaler;
            width = w;
            height = h;
          }
        }
        ComPtr<IWICFormatConverter> pConverter;
        if (SUCCEEDED(hr))
          hr = pFactory->CreateFormatConverter(pConverter.GetAddressOf());
        if (SUCCEEDED(hr))
          hr = pConverter->Initialize(pSource.Get(), GUID_WICPixelFormat32bppPRGBA, WICBitmapDitherTypeNone, nullptr, 0.0,
            WICBitmapPaletteTypeCustom);
        if (SUCCEEDED(hr))
        {
          levels.emplace_back((size_t)width * height * 4);
          hr = pConverter->CopyPixels(nullptr, width * 4, (UINT)levels[0].size(), levels[0].data());
        }
      }
      if (SUCCEEDED(co))
        CoUninitialize();
      if (SUCCEEDED(hr))
      {
        UINT w = width, h = height;
        while (w > 1 || h > 1)
        {
          UINT halfWidth, halfHeight;
          std::vector<BYTE> half = HalfSize(levels.back(), w, h, halfWidth, halfHeight);
          levels.push_back(std::move(half));
          w = halfWidth;
          h = halfHeight;
        }
      }
    }
    catch (...)
    {
      hr = E_OUTOFMEMORY;
      levels.clear();
    }

    std::string error = SUCCEEDED(hr) ? std::string() : LoadError(hr);
    {
      std::lock_guard<std::mutex> lock(g_Mutex);
      if (SUCCEEDED(hr))
      {
        picture->Width = width;
        picture->Height = height;
        picture->FileWidth = fileWidth;
        picture->FileHeight = fileHeight;
        picture->Levels = std::move(levels);
        picture->State = Picture::Ready;
      }
      else
      {
        picture->Error = error;
        picture->State = Picture::Failed;
      }
    }
    if (SUCCEEDED(hr))
      util::log::Write("Images: read \"%s\", %u x %u pixels%s", name.c_str(), fileWidth, fileHeight,
        width != fileWidth || height != fileHeight ? " (turned upright or scaled down for drawing)" : "");
    else
      util::log::Warning("Images: \"%s\" couldn't be read: %s", name.c_str(), error.c_str());
  }

  // Under g_Mutex: every image's picture read (once a file), and the
  // pictures no image shows any more let go
  void EnsurePictures()
  {
    for (WorldImage const& image : g_Images)
    {
      if (image.File.empty() || g_Pictures.count(image.File))
        continue;
      std::shared_ptr<Picture> picture = std::make_shared<Picture>();
      g_Pictures[image.File] = picture;
      try
      {
        std::thread(Load, picture, Folder() + L"\\" + Wide(image.File), image.File).detach();
      }
      catch (...)
      {
        picture->State = Picture::Failed;
        picture->Error = "no thread could be started to read it";
      }
    }
    for (auto it = g_Pictures.begin(); it != g_Pictures.end();)
    {
      bool used = std::any_of(g_Images.begin(), g_Images.end(), [&](WorldImage const& image) { return image.File == it->first; });
      it = used ? std::next(it) : g_Pictures.erase(it);
    }
  }

  // Under g_Mutex, render thread: a read picture made into a texture (all its sizes)
  void Upload(ID3D11Device* pDevice, Picture& picture, std::string const& name)
  {
    std::vector<D3D11_SUBRESOURCE_DATA> data(picture.Levels.size());
    UINT w = picture.Width;
    for (size_t i = 0; i < data.size(); ++i)
    {
      data[i].pSysMem = picture.Levels[i].data();
      data[i].SysMemPitch = w * 4;
      w = w > 1 ? w / 2 : 1;
    }
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = picture.Width;
    desc.Height = picture.Height;
    desc.MipLevels = (UINT)data.size();
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    ComPtr<ID3D11Texture2D> pTexture;
    HRESULT hr = pDevice->CreateTexture2D(&desc, data.data(), pTexture.GetAddressOf());
    if (SUCCEEDED(hr))
      hr = pDevice->CreateShaderResourceView(pTexture.Get(), nullptr, picture.View.GetAddressOf());
    picture.Levels.clear();
    picture.Levels.shrink_to_fit();
    if (FAILED(hr))
    {
      char text[128];
      sprintf_s(text, "the graphics card's copy couldn't be made (HRESULT 0x%08X)", (unsigned)hr);
      picture.Error = text;
      picture.State = Picture::Failed;
      picture.View.Reset();
      util::log::Warning("Images: \"%s\": %s", name.c_str(), text);
    }
  }

  // Under g_Mutex
  Picture const* FindPicture(std::string const& file)
  {
    auto it = g_Pictures.find(file);
    return it != g_Pictures.end() ? it->second.get() : nullptr;
  }

  WorldImage* FindImage(int id)
  {
    for (WorldImage& image : g_Images)
      if (image.Id == id)
        return &image;
    return nullptr;
  }

  // Height over width; 0 while it isn't read
  float Aspect(Picture const* pPicture)
  {
    return pPicture && pPicture->State == Picture::Ready && pPicture->Width && pPicture->Height ?
      (float)pPicture->Height / (float)pPicture->Width : 0.f;
  }

  XMVECTOR Row(float const* t, int row)
  {
    return XMVectorSet(t[row * 4], t[row * 4 + 1], t[row * 4 + 2], 0.f);
  }

  // Its directions in the world: right and up along the picture as seen
  // from its front, and its front. Facing the camera, the camera's own right,
  // up and backward (t: its transform); else from its angles (see WorldImage).
  void Basis(WorldImage const& image, bool rightHanded, float const* t, XMVECTOR& right, XMVECTOR& up, XMVECTOR& front)
  {
    if (image.FaceCamera && t)
    {
      right = XMVector3Normalize(Row(t, 0));
      up = XMVector3Normalize(Row(t, 1));
      front = XMVector3Normalize(Row(t, 2));
    }
    else
    {
      float yaw = image.Yaw * Degrees, pitch = image.Pitch * Degrees;
      front = XMVectorSet(sinf(yaw) * cosf(pitch), sinf(pitch), cosf(yaw) * cosf(pitch), 0.f);
      up = XMVectorSet(-sinf(yaw) * sinf(pitch), cosf(pitch), -cosf(yaw) * sinf(pitch), 0.f);
      // As the camera's: seen from the front, right is the viewer's right
      right = rightHanded ? XMVector3Cross(up, front) : XMVector3Cross(front, up);
    }
    float roll = image.Roll * Degrees, c = cosf(roll), s = sinf(roll);
    XMVECTOR rolledRight = right * c + up * s;
    up = up * c - right * s;
    right = rolledRight;
  }

  // Its corners: bottom left, bottom right, top left, top right
  void Corners(WorldImage const& image, float aspect, bool rightHanded, float const* t, XMVECTOR* corners, XMVECTOR& front)
  {
    XMVECTOR right, up;
    Basis(image, rightHanded, t, right, up, front);
    XMVECTOR middle = XMVectorSet(image.Position[0], image.Position[1], image.Position[2], 0.f);
    XMVECTOR across = right * (image.Width * 0.5f), along = up * (image.Width * aspect * 0.5f);
    corners[0] = middle - across - along;
    corners[1] = middle + across - along;
    corners[2] = middle - across + along;
    corners[3] = middle + across + along;
  }

  // Turned to face along a direction (made of length 1), upright
  void FaceAlong(WorldImage& image, XMVECTOR direction)
  {
    XMFLOAT3 d;
    XMStoreFloat3(&d, direction);
    float y = d.y < -1.f ? -1.f : (d.y > 1.f ? 1.f : d.y);
    image.Pitch = asinf(y) / Degrees;
    image.Yaw = d.x * d.x + d.z * d.z > 1e-10f ? atan2f(d.x, d.z) / Degrees : 0.f;
    image.Roll = 0.f;
  }

  float WidthFor(float distance)
  {
    float width = distance * WidthPerMetre;
    return width < 0.2f ? 0.2f : (width > 500.f ? 500.f : width);
  }

  // A depth's distance along the view (as Distance in WorldImages.hlsl):
  // huge for none (the sky)
  float DepthDistance(float depth, float const* projection)
  {
    float below = projection[10] - depth * projection[11];
    float above = projection[14] - depth * projection[15];
    return below > 1e-12f ? above / below : 1e30f;
  }

  // The world point at a place on the frame (x, y in pixels) at a distance
  // along the view (the view space looks down -z; row vectors)
  XMVECTOR Unproject(float const* t, float const* projection, float x, float y, UINT width, UINT height, float distance)
  {
    float nx = x / (float)width * 2.f - 1.f, ny = 1.f - y / (float)height * 2.f;
    float w = -distance * projection[11] + projection[15];
    float vx = (nx * w + distance * projection[8] - projection[12]) / projection[0];
    float vy = (ny * w + distance * projection[9] - projection[13]) / projection[5];
    return Row(t, 3) + Row(t, 0) * vx + Row(t, 1) * vy - Row(t, 2) * distance;
  }

  float DepthTexel(BYTE const* p, DXGI_FORMAT format)
  {
    switch (format)
    {
    case DXGI_FORMAT_R32G8X24_TYPELESS: case DXGI_FORMAT_R32_TYPELESS: return *(float const*)p;
    case DXGI_FORMAT_R24G8_TYPELESS: return (float)(*(UINT32 const*)p & 0xFFFFFF) / 16777215.f;
    case DXGI_FORMAT_R16_TYPELESS: return (float)*(UINT16 const*)p / 65535.f;
    default: return -1.f;
    }
  }

  UINT TexelBytes(DXGI_FORMAT format)
  {
    switch (format)
    {
    case DXGI_FORMAT_R32G8X24_TYPELESS: return 8;
    case DXGI_FORMAT_R32_TYPELESS: case DXGI_FORMAT_R24G8_TYPELESS: return 4;
    case DXGI_FORMAT_R16_TYPELESS: return 2;
    default: return 0;
    }
  }

  // A view format for a typeless swapchain one (as the green screen's)
  DXGI_FORMAT TargetFormat(DXGI_FORMAT format)
  {
    switch (format)
    {
    case DXGI_FORMAT_R8G8B8A8_TYPELESS: return DXGI_FORMAT_R8G8B8A8_UNORM;
    case DXGI_FORMAT_B8G8R8A8_TYPELESS: return DXGI_FORMAT_B8G8R8A8_UNORM;
    case DXGI_FORMAT_B8G8R8X8_TYPELESS: return DXGI_FORMAT_B8G8R8X8_UNORM;
    case DXGI_FORMAT_R10G10B10A2_TYPELESS: return DXGI_FORMAT_R10G10B10A2_UNORM;
    case DXGI_FORMAT_R16G16B16A16_TYPELESS: return DXGI_FORMAT_R16G16B16A16_FLOAT;
    default: return format;
    }
  }

  // Under g_Mutex: what's being placed: a new picture of the file (target
  // 0, created set), or the image with that id (null if it's gone)
  WorldImage* Target(int target, std::string const& file, bool& created)
  {
    created = false;
    if (target)
      return FindImage(target);
    if (file.empty())
      return nullptr;
    WorldImage image;
    image.Id = g_NextId++;
    image.File = file;
    g_Images.push_back(image);
    created = true;
    return &g_Images.back();
  }

  // Under g_Mutex, render thread: put in front of the camera
  void PlaceInFront(float const* t)
  {
    g_FrontPending = false;
    bool created;
    WorldImage* pImage = Target(g_FrontTarget, g_FrontFile, created);
    if (!pImage)
      return;
    XMVECTOR look = -XMVector3Normalize(Row(t, 2));
    XMStoreFloat3((XMFLOAT3*)pImage->Position, Row(t, 3) + look * g_Distance);
    FaceAlong(*pImage, -look);
    if (created)
      pImage->Width = WidthFor(g_Distance);
    char text[160];
    sprintf_s(text, "Put %.1f m in front of the camera.", g_Distance);
    g_Status = text;
    util::log::Write("Images: \"%s\" put %.1f m in front of the camera at %.2f %.2f %.2f", pImage->File.c_str(), g_Distance,
      pImage->Position[0], pImage->Position[1], pImage->Position[2]);
    g_mainHandle->OnConfigChanged();
  }

  // Under g_Mutex, render thread: the click's block of the game's depth
  // copied from this frame's depth
  void StartPick(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, greenplane::FrameDepth const& frame)
  {
    g_ClickPending = false;
    ComPtr<ID3D11Resource> pResource;
    frame.View->GetResource(pResource.GetAddressOf());
    ComPtr<ID3D11Texture2D> pDepth;
    if (!pResource || FAILED(pResource.As(&pDepth)))
      return;
    D3D11_TEXTURE2D_DESC desc{};
    pDepth->GetDesc(&desc);
    if (!TexelBytes(desc.Format) || desc.Width < (UINT)PickSide || desc.Height < (UINT)PickSide)
    {
      g_Status = "The game's depth can't be read here: the picture couldn't be placed by click.";
      return;
    }
    if (!g_Pick.Staging || g_Pick.Desc.Format != desc.Format)
    {
      g_Pick.Staging.Reset();
      D3D11_TEXTURE2D_DESC staging{};
      staging.Width = PickSide;
      staging.Height = PickSide;
      staging.MipLevels = 1;
      staging.ArraySize = 1;
      staging.Format = desc.Format;
      staging.SampleDesc.Count = 1;
      staging.Usage = D3D11_USAGE_STAGING;
      staging.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
      if (FAILED(pDevice->CreateTexture2D(&staging, nullptr, g_Pick.Staging.GetAddressOf())))
      {
        g_Status = "The game's depth couldn't be read: the picture couldn't be placed by click.";
        return;
      }
      g_Pick.Desc = staging;
    }
    int x = (int)(g_ClickU * frame.Width * frame.Scale[0]), y = (int)(g_ClickV * frame.Height * frame.Scale[1]);
    x = x < 0 ? 0 : (x >= (int)desc.Width ? (int)desc.Width - 1 : x);
    y = y < 0 ? 0 : (y >= (int)desc.Height ? (int)desc.Height - 1 : y);
    int boxX = x - PickReach, boxY = y - PickReach;
    boxX = boxX < 0 ? 0 : (boxX > (int)desc.Width - PickSide ? (int)desc.Width - PickSide : boxX);
    boxY = boxY < 0 ? 0 : (boxY > (int)desc.Height - PickSide ? (int)desc.Height - PickSide : boxY);
    D3D11_BOX box{ (UINT)boxX, (UINT)boxY, 0, (UINT)(boxX + PickSide), (UINT)(boxY + PickSide), 1 };
    pContext->CopySubresourceRegion(g_Pick.Staging.Get(), 0, 0, 0, 0, pDepth.Get(), 0, &box);
    g_Pick.Copied = true;
    g_Pick.Waited = 0;
    g_Pick.Target = g_ClickTarget;
    g_Pick.File = g_ClickFile;
    memcpy(g_Pick.Transform, frame.Transform, sizeof(g_Pick.Transform));
    memcpy(g_Pick.Projection, frame.Projection, sizeof(g_Pick.Projection));
    g_Pick.ScaleX = frame.Scale[0];
    g_Pick.ScaleY = frame.Scale[1];
    g_Pick.Width = frame.Width;
    g_Pick.Height = frame.Height;
    g_Pick.BoxX = boxX;
    g_Pick.BoxY = boxY;
    g_Pick.ClickX = x - boxX;
    g_Pick.ClickY = y - boxY;
  }

  // Under g_Mutex, render thread: the click's depth read back once it's
  // there, and the picture put on what was clicked (flat on it, facing out
  // of it towards the camera), or Distance away on the sky
  void FinishPick(ID3D11DeviceContext* pContext)
  {
    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = pContext->Map(g_Pick.Staging.Get(), 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &mapped);
    if (hr == DXGI_ERROR_WAS_STILL_DRAWING)
    {
      if (++g_Pick.Waited > PickPatience)
      {
        g_Pick.Copied = false;
        g_Status = "The game's depth didn't come back in time: click again.";
      }
      return;
    }
    g_Pick.Copied = false;
    if (FAILED(hr))
    {
      g_Status = "The game's depth couldn't be read: click again.";
      return;
    }
    float depths[PickSide][PickSide];
    UINT bytes = TexelBytes(g_Pick.Desc.Format);
    for (int y = 0; y < PickSide; ++y)
      for (int x = 0; x < PickSide; ++x)
        depths[y][x] = DepthTexel((BYTE const*)mapped.pData + (size_t)y * mapped.RowPitch + (size_t)x * bytes, g_Pick.Desc.Format);
    pContext->Unmap(g_Pick.Staging.Get(), 0);

    float const* t = g_Pick.Transform;
    float const* projection = g_Pick.Projection;
    auto at = [&](int x, int y, XMVECTOR& point, float& distance)
    {
      if (x < 0 || y < 0 || x >= PickSide || y >= PickSide)
        return false;
      distance = DepthDistance(depths[y][x], projection);
      if (!(distance > 0.f) || distance > NothingThere)
        return false;
      point = Unproject(t, projection, (g_Pick.BoxX + x + 0.5f) / g_Pick.ScaleX, (g_Pick.BoxY + y + 0.5f) / g_Pick.ScaleY,
        g_Pick.Width, g_Pick.Height, distance);
      return true;
    };

    bool created;
    WorldImage* pImage = Target(g_Pick.Target, g_Pick.File, created);
    if (!pImage)
      return;
    XMVECTOR camera = Row(t, 3);
    XMVECTOR look = -XMVector3Normalize(Row(t, 2));
    int cx = g_Pick.ClickX, cy = g_Pick.ClickY;
    XMVECTOR middle;
    float distance;
    char text[160];
    if (!at(cx, cy, middle, distance))
    {
      // The sky: Distance away, facing the camera
      float u = (g_Pick.BoxX + cx + 0.5f) / g_Pick.ScaleX, v = (g_Pick.BoxY + cy + 0.5f) / g_Pick.ScaleY;
      middle = Unproject(t, projection, u, v, g_Pick.Width, g_Pick.Height, g_Distance);
      XMStoreFloat3((XMFLOAT3*)pImage->Position, middle);
      FaceAlong(*pImage, XMVector3Normalize(camera - middle));
      if (created)
        pImage->Width = WidthFor(XMVectorGetX(XMVector3Length(middle - camera)));
      sprintf_s(text, "Nothing there (the sky): put %.1f m away, facing the camera.", g_Distance);
      g_Status = text;
    }
    else
    {
      // The surface's direction from the texels beside it, on the side
      // nearer in distance (not across an edge)
      XMVECTOR a, b, across = XMVectorZero(), down = XMVectorZero();
      float da, db;
      bool okA = at(cx - PickStep, cy, a, da), okB = at(cx + PickStep, cy, b, db);
      bool hasAcross = okA || okB, hasDown;
      if (okA && (!okB || fabsf(da - distance) <= fabsf(db - distance)))
        across = middle - a;
      else if (okB)
        across = b - middle;
      okA = at(cx, cy - PickStep, a, da);
      okB = at(cx, cy + PickStep, b, db);
      hasDown = okA || okB;
      if (okA && (!okB || fabsf(da - distance) <= fabsf(db - distance)))
        down = middle - a;
      else if (okB)
        down = b - middle;
      XMVECTOR toCamera = XMVector3Normalize(camera - middle);
      XMVECTOR normal = XMVector3Cross(across, down);
      if (!hasAcross || !hasDown || XMVectorGetX(XMVector3Length(normal)) < 1e-8f)
        normal = toCamera;
      normal = XMVector3Normalize(normal);
      if (XMVectorGetX(XMVector3Dot(normal, toCamera)) < 0.f)
        normal = -normal;
      XMStoreFloat3((XMFLOAT3*)pImage->Position, middle + normal * SurfaceGap);
      float ny = XMVectorGetY(normal);
      float lx = XMVectorGetX(look), lz = XMVectorGetZ(look);
      if (fabsf(ny) > 0.97f && lx * lx + lz * lz > 1e-8f)
      {
        // Flat on the ground (or a ceiling): its top away from the camera,
        // so it reads the right way up from where it's seen
        pImage->Pitch = ny > 0.f ? 90.f : -90.f;
        pImage->Yaw = (ny > 0.f ? atan2f(-lx, -lz) : atan2f(lx, lz)) / Degrees;
        pImage->Roll = 0.f;
      }
      else
        FaceAlong(*pImage, normal);
      if (created)
        pImage->Width = WidthFor(distance);
      sprintf_s(text, "Put on what was clicked, %.1f m away.", distance);
      g_Status = text;
    }
    util::log::Write("Images: \"%s\" put by click at %.2f %.2f %.2f, turned %.1f, tilted %.1f", pImage->File.c_str(),
      pImage->Position[0], pImage->Position[1], pImage->Position[2], pImage->Yaw, pImage->Pitch);
    g_mainHandle->OnConfigChanged();
  }

  bool Make(ID3D11Device* pDevice)
  {
    D3D11_BUFFER_DESC constants{};
    constants.ByteWidth = sizeof(Constants);
    constants.Usage = D3D11_USAGE_DYNAMIC;
    constants.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    constants.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    D3D11_RASTERIZER_DESC rasterizer{};
    rasterizer.FillMode = D3D11_FILL_SOLID;
    rasterizer.CullMode = D3D11_CULL_NONE;
    rasterizer.DepthClipEnable = TRUE;

    // Premultiplied (the pictures are read so); the frame's alpha left alone
    D3D11_BLEND_DESC blend{};
    blend.RenderTarget[0].BlendEnable = TRUE;
    blend.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
    blend.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blend.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blend.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
    blend.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
    blend.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blend.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN |
      D3D11_COLOR_WRITE_ENABLE_BLUE;

    D3D11_DEPTH_STENCIL_DESC depth{};
    depth.DepthEnable = FALSE;
    depth.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    depth.DepthFunc = D3D11_COMPARISON_ALWAYS;
    depth.StencilEnable = FALSE;

    D3D11_SAMPLER_DESC sampler{};
    sampler.Filter = D3D11_FILTER_ANISOTROPIC;
    sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler.MaxAnisotropy = 8;
    sampler.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampler.MinLOD = 0.f;
    sampler.MaxLOD = D3D11_FLOAT32_MAX;

    HRESULT hr = pDevice->CreateVertexShader(g_WorldImagesVs, sizeof(g_WorldImagesVs), nullptr, g_Vs.GetAddressOf());
    if (SUCCEEDED(hr)) hr = pDevice->CreatePixelShader(g_WorldImagesPs, sizeof(g_WorldImagesPs), nullptr, g_Ps.GetAddressOf());
    if (SUCCEEDED(hr)) hr = pDevice->CreateBuffer(&constants, nullptr, g_Constants.GetAddressOf());
    if (SUCCEEDED(hr)) hr = pDevice->CreateRasterizerState(&rasterizer, g_Rasterizer.GetAddressOf());
    if (SUCCEEDED(hr)) hr = pDevice->CreateBlendState(&blend, g_Blend.GetAddressOf());
    if (SUCCEEDED(hr)) hr = pDevice->CreateDepthStencilState(&depth, g_Depth.GetAddressOf());
    if (SUCCEEDED(hr)) hr = pDevice->CreateSamplerState(&sampler, g_Sampler.GetAddressOf());
    if (FAILED(hr))
    {
      g_Failed = true;
      util::log::Error("Images: their shaders or render states couldn't be made (HRESULT 0x%X), they're not drawn", hr);
      return false;
    }
    g_Made = true;
    return true;
  }

  // Where a point is on the screen, in pixels, with the Panini projection
  // when it's on (as the lights' dots have it); false behind the camera
  bool ToScreen(float const* t, float const* projection, XMVECTOR point, ImVec2& out)
  {
    XMVECTOR offset = point - Row(t, 3);
    float right = XMVectorGetX(XMVector3Dot(offset, Row(t, 0)));
    float up = XMVectorGetX(XMVector3Dot(offset, Row(t, 1)));
    float depth = -XMVectorGetX(XMVector3Dot(offset, Row(t, 2)));
    if (depth < 0.1f)
      return false;
    float px = right / depth, py = up / depth;
    panini::ToPanini(px, py);
    float scale = panini::ScreenScale();
    ImVec2 size = ImGui::GetIO().DisplaySize;
    out.x = (px * projection[0] / scale * 0.5f + 0.5f) * size.x;
    out.y = (0.5f - py * projection[5] / scale * 0.5f) * size.y;
    return true;
  }

  // Where the held picture goes for the cursor at x, y: from base, across
  // the picture (the camera's right and up), along the ground (level), or
  // only up and down. The lights' DragPlace (LightManager.cpp) on this
  // file's ToScreen: Newton steps on the screen place itself, so it ends up
  // under the cursor where its dot is drawn (the Panini view too), a step
  // only taken when it brings it nearer.
  bool DragPlace(float const* t, float const* projection, float const* base, bool upDown, bool ground,
    float x, float y, float* place)
  {
    XMVECTOR origin = XMVectorSet(base[0], base[1], base[2], 0.f);
    XMVECTOR back = XMVector3Normalize(Row(t, 2));
    float depth = -XMVectorGetX(XMVector3Dot(origin - Row(t, 3), back));
    if (depth < 0.1f)
      return false;
    XMVECTOR axisA, axisB;
    if (upDown)
      axisA = axisB = XMVectorSet(0.f, 1.f, 0.f, 0.f);
    else if (ground)
    {
      axisA = XMVectorSet(1.f, 0.f, 0.f, 0.f);
      axisB = XMVectorSet(0.f, 0.f, 1.f, 0.f);
    }
    else
    {
      axisA = XMVector3Normalize(Row(t, 0));
      axisB = XMVector3Normalize(Row(t, 1));
    }
    float step = depth * 0.001f;
    float a = 0.f, b = 0.f;
    ImVec2 s;
    if (!ToScreen(t, projection, origin, s))
      return false;
    float error = (x - s.x) * (x - s.x) + (y - s.y) * (y - s.y);
    for (int i = 0; i < 16 && error > 0.0025f; ++i)
    {
      XMVECTOR v = origin + axisA * a + axisB * b;
      ImVec2 pa, pb;
      if (!ToScreen(t, projection, v + axisA * step, pa) || (!upDown && !ToScreen(t, projection, v + axisB * step, pb)))
        break;
      float ex = x - s.x, ey = y - s.y;
      float j11 = (pa.x - s.x) / step, j21 = (pa.y - s.y) / step;   // pixels a metre
      float da, db = 0.f;
      if (upDown)
      {
        // As near the cursor as the line up and down comes
        float jj = j11 * j11 + j21 * j21;
        if (jj < 1e-8f)
          break;
        da = (j11 * ex + j21 * ey) / jj;
      }
      else
      {
        float j12 = (pb.x - s.x) / step, j22 = (pb.y - s.y) / step;
        float det = j11 * j22 - j12 * j21;
        if (fabsf(det) < 1e-8f)
          break;
        da = (j22 * ex - j12 * ey) / det;
        db = (j11 * ey - j21 * ex) / det;
      }
      // No more than its distance from the camera at a time
      float length = sqrtf(da * da + db * db);
      if (length > depth)
      {
        da *= depth / length;
        db *= depth / length;
      }
      bool nearer = false;
      for (int halve = 0; halve < 6 && !nearer; ++halve, da *= 0.5f, db *= 0.5f)
      {
        ImVec2 n;
        if (!ToScreen(t, projection, v + axisA * da + axisB * db, n))
          continue;
        float e = (x - n.x) * (x - n.x) + (y - n.y) * (y - n.y);
        if (e < error)
        {
          a += da;
          b += db;
          s = n;
          error = e;
          nearer = true;
        }
      }
      if (!nearer)
        break;
    }
    XMStoreFloat3((XMFLOAT3*)place, origin + axisA * a + axisB * b);
    return true;
  }

  // Under g_Mutex: the drag let go (noted, and the config saved, if it moved)
  void EndDrag()
  {
    if (!g_DragId)
      return;
    WorldImage const* pImage = FindImage(g_DragId);
    if (pImage && g_DragMoved)
    {
      util::log::Write("Images: \"%s\" dragged to %.2f %.2f %.2f", pImage->File.c_str(), pImage->Position[0],
        pImage->Position[1], pImage->Position[2]);
      g_mainHandle->OnConfigChanged();
    }
    g_DragId = 0;
    g_DragMoved = false;
  }

  // A file name as one ini value: % and what the ini reader would cut or
  // split (; # = [ ] and spaces) written as %XX
  std::string Encode(std::string const& name)
  {
    std::string out;
    for (char ch : name)
    {
      unsigned char c = (unsigned char)ch;
      if (c == '%' || c == ';' || c == '#' || c == '=' || c == '[' || c == ']' || c <= ' ')
      {
        char hex[4];
        sprintf_s(hex, "%%%02X", c);
        out += hex;
      }
      else
        out += ch;
    }
    return out;
  }

  std::string Decode(std::string const& text)
  {
    std::string out;
    for (size_t i = 0; i < text.size(); ++i)
    {
      unsigned int c;
      if (text[i] == '%' && i + 2 < text.size() && isxdigit((unsigned char)text[i + 1]) &&
          isxdigit((unsigned char)text[i + 2]) && sscanf_s(text.substr(i + 1, 2).c_str(), "%2X", &c) == 1)
      {
        out += (char)c;
        i += 2;
      }
      else
        out += text[i];
    }
    return out;
  }

  float Clamp(float value, float low, float high)
  {
    return value < low ? low : (value > high ? high : value);
  }
}

void worldimages::Apply(IDXGISwapChain* pSwapChain, ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
  if (!pSwapChain || !pDevice || !pContext)
    return;
  std::lock_guard<std::mutex> lock(g_Mutex);
  EnsurePictures();
  for (auto& entry : g_Pictures)
  {
    Picture& picture = *entry.second;
    if (picture.State == Picture::Ready && !picture.View && !picture.Levels.empty())
      Upload(pDevice, picture, entry.first);
  }

  // The game's depth and camera only while there's something to draw or place
  bool shown = false;
  for (WorldImage const& image : g_Images)
  {
    Picture const* pPicture = FindPicture(image.File);
    shown |= image.Shown && pPicture && pPicture->View;
  }
  bool placing = g_FrontPending || g_ClickPending || g_Pick.Copied;
  // and while the menu draws the dots (hidden pictures have dots too)
  bool dots = !g_Images.empty() && GetTickCount64() - g_OverlayTick < DotsWantView;
  bool wanted = shown || placing || dots;
  greenplane::WantDepth(wanted, greenplane::WantImages);
  greenplane::FrameDepth frame;
  if (!wanted || !greenplane::TakeFrameDepth(pSwapChain, pDevice, pContext, frame))
  {
    g_ViewOk = false;
    return;
  }
  float const* t = frame.Transform;
  float const* projection = frame.Projection;
  memcpy(g_ViewTransform, t, sizeof(g_ViewTransform));
  memcpy(g_ViewProjection, projection, sizeof(g_ViewProjection));
  g_ViewOk = true;
  g_RightHanded = XMVectorGetX(XMVector3Dot(XMVector3Cross(Row(t, 0), Row(t, 1)), Row(t, 2))) > 0.f;

  // Placing: in front of the camera now; a click's depth copied now, read
  // back once it's there (a frame or more later)
  if (g_FrontPending)
    PlaceInFront(t);
  if (g_Pick.Copied)
    FinishPick(pContext);
  if (g_ClickPending && !g_Pick.Copied)
    StartPick(pDevice, pContext, frame);
  if (!shown || g_Failed || (!g_Made && !Make(pDevice)))
    return;

  ComPtr<ID3D11Texture2D> pBack;
  if (FAILED(pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)pBack.GetAddressOf())) || !pBack)
    return;
  D3D11_TEXTURE2D_DESC back{};
  pBack->GetDesc(&back);
  if (back.SampleDesc.Count != 1 || !back.Width || !back.Height)
    return;

  // Farthest first, so nearer ones blend over them
  struct Draw
  {
    WorldImage const* pImage;
    Picture const* pPicture;
    float Distance;
  };
  std::vector<Draw> draws;
  XMVECTOR camera = Row(t, 3);
  for (WorldImage const& image : g_Images)
  {
    Picture const* pPicture = FindPicture(image.File);
    if (!image.Shown || !pPicture || !pPicture->View || image.Opacity <= 0.f)
      continue;
    XMVECTOR middle = XMVectorSet(image.Position[0], image.Position[1], image.Position[2], 0.f);
    draws.push_back({ &image, pPicture, XMVectorGetX(XMVector3Length(middle - camera)) });
  }
  if (draws.empty())
    return;
  std::sort(draws.begin(), draws.end(), [](Draw const& a, Draw const& b) { return a.Distance > b.Distance; });

  ComPtr<ID3D11RenderTargetView> pTarget;
  D3D11_RENDER_TARGET_VIEW_DESC targetDesc{};
  targetDesc.Format = TargetFormat(back.Format);
  targetDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
  if (FAILED(pDevice->CreateRenderTargetView(pBack.Get(), targetDesc.Format != back.Format ? &targetDesc : nullptr,
      pTarget.GetAddressOf())))
    return;

  XMMATRIX cameraMatrix = XMLoadFloat4x4((XMFLOAT4X4 const*)t);
  XMMATRIX viewProjection = XMMatrixInverse(nullptr, cameraMatrix) * XMLoadFloat4x4((XMFLOAT4X4 const*)projection);

  g_Backup.Save(pContext);

  D3D11_VIEWPORT viewport{ 0.f, 0.f, (float)back.Width, (float)back.Height, 0.f, 1.f };
  ID3D11Buffer* noBuffer = nullptr;
  UINT zero = 0;
  const float blendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
  pContext->OMSetRenderTargets(1, pTarget.GetAddressOf(), nullptr);
  pContext->RSSetViewports(1, &viewport);
  pContext->RSSetState(g_Rasterizer.Get());
  pContext->OMSetBlendState(g_Blend.Get(), blendFactor, 0xFFFFFFFF);
  pContext->OMSetDepthStencilState(g_Depth.Get(), 0);
  pContext->IASetInputLayout(nullptr);
  pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  pContext->IASetVertexBuffers(0, 1, &noBuffer, &zero, &zero);
  pContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_R16_UINT, 0);
  pContext->VSSetShader(g_Vs.Get(), nullptr, 0);
  pContext->VSSetConstantBuffers(0, 1, g_Constants.GetAddressOf());
  pContext->GSSetShader(nullptr, nullptr, 0);
  pContext->HSSetShader(nullptr, nullptr, 0);
  pContext->DSSetShader(nullptr, nullptr, 0);
  pContext->PSSetShader(g_Ps.Get(), nullptr, 0);
  pContext->PSSetConstantBuffers(0, 1, g_Constants.GetAddressOf());
  pContext->PSSetSamplers(0, 1, g_Sampler.GetAddressOf());

  for (Draw const& draw : draws)
  {
    WorldImage const& image = *draw.pImage;
    XMVECTOR corners[4], front;
    Corners(image, Aspect(draw.pPicture), g_RightHanded, t, corners, front);
    // Seen from behind: drawn only if it's wanted from both sides
    XMVECTOR middle = XMVectorSet(image.Position[0], image.Position[1], image.Position[2], 0.f);
    if (!image.BothSides && !image.FaceCamera && XMVectorGetX(XMVector3Dot(front, camera - middle)) < 0.f)
      continue;
    D3D11_MAPPED_SUBRESOURCE mapped;
    if (FAILED(pContext->Map(g_Constants.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
      continue;
    Constants* c = (Constants*)mapped.pData;
    XMStoreFloat4x4(&c->ViewProjection, viewProjection);
    for (int i = 0; i < 4; ++i)
    {
      XMStoreFloat4(&c->Corners[i], corners[i]);
      c->Corners[i].w = 1.f;
    }
    c->Look = XMFLOAT4(Clamp(image.Opacity, 0.f, 1.f), image.Brightness, image.Mirror ? 1.f : 0.f, 0.f);
    c->Depth = XMFLOAT4(frame.Scale[0], frame.Scale[1], 0.f, DepthSlack + DepthSlackPerMetre * draw.Distance);
    c->Projection = XMFLOAT4(projection[10], projection[11], projection[14], projection[15]);
    pContext->Unmap(g_Constants.Get(), 0);
    ID3D11ShaderResourceView* views[2] = { frame.View, draw.pPicture->View.Get() };
    pContext->PSSetShaderResources(0, 2, views);
    pContext->Draw(6, 0);
  }

  // The picture's slot isn't in the backup: emptied
  ID3D11ShaderResourceView* none[2] = { nullptr, nullptr };
  pContext->PSSetShaderResources(0, 2, none);
  g_Backup.Restore(pContext);
}

void worldimages::DrawOverlay()
{
  std::lock_guard<std::mutex> lock(g_Mutex);
  ImGuiIO& io = ImGui::GetIO();
  ImDrawList* pDrawList = ImGui::GetOverlayDrawList();
  float fontSize = ImGui::GetFontSize();
  const ImU32 shadow = IM_COL32(0, 0, 0, 170);
  const ImU32 outline = IM_COL32(255, 210, 60, 255);

  // The open pictures' outlines, with the last frame's camera (only while
  // the menu shows them: listed in its last frame)
  if (g_ViewOk && g_OpenIdsFrame >= ImGui::GetFrameCount() - 1)
  {
    for (int id : g_OpenIds)
    {
      WorldImage const* pImage = FindImage(id);
      float aspect = pImage ? Aspect(FindPicture(pImage->File)) : 0.f;
      if (!pImage || aspect <= 0.f)
        continue;
      XMVECTOR corners[4], front;
      Corners(*pImage, aspect, g_RightHanded, g_ViewTransform, corners, front);
      ImVec2 points[4];
      bool onScreen = true;
      // Round the outside: bottom left, bottom right, top right, top left
      const int order[4] = { 0, 1, 3, 2 };
      for (int i = 0; i < 4 && onScreen; ++i)
        onScreen = ToScreen(g_ViewTransform, g_ViewProjection, corners[order[i]], points[i]);
      if (!onScreen)
        continue;
      pDrawList->AddPolyline(points, 4, shadow, true, 4.f);
      pDrawList->AddPolyline(points, 4, outline, true, 2.f);
      char label[16];
      sprintf_s(label, "%d", (int)(pImage - g_Images.data()) + 1);
      pDrawList->AddText(ImVec2(points[3].x + 5.f, points[3].y + 4.f), shadow, label);
      pDrawList->AddText(ImVec2(points[3].x + 4.f, points[3].y + 3.f), outline, label);
    }
  }

  // Placing by click: the click on the picture (off the menu's windows) is
  // the picture's, taken off ImGui's input; no dots meanwhile
  const ImU32 white = IM_COL32(255, 255, 255, 255);
  if (g_Armed >= 0)
  {
    EndDrag();
    if (io.WantCaptureMouse)
      return;
    ImVec2 at = io.MousePos;
    pDrawList->AddCircle(at, fontSize * 0.6f, shadow, 20, 4.f);
    pDrawList->AddCircle(at, fontSize * 0.6f, outline, 20, 2.f);
    const char* hint = "Click: the picture goes here";
    pDrawList->AddText(ImVec2(at.x + fontSize + 1.f, at.y + 1.f), shadow, hint);
    pDrawList->AddText(ImVec2(at.x + fontSize, at.y), white, hint);
    if (io.MouseClicked[0] && !io.MouseDownOwned[0] && io.DisplaySize.x > 0.f && io.DisplaySize.y > 0.f)
    {
      g_ClickPending = true;
      g_ClickU = at.x / io.DisplaySize.x;
      g_ClickV = at.y / io.DisplaySize.y;
      g_ClickTarget = g_Armed;
      g_ClickFile = g_ArmedFile;
      g_Armed = -1;
      g_Status = "Placing...";
      io.MouseClicked[0] = false;
    }
    return;
  }

  // The dots, as the lights' (LightManager::DrawOverlay): the camera is kept
  // for them while the menu draws them (see Apply)
  g_OverlayTick = GetTickCount64();
  if (!g_ViewOk || g_Images.empty())
  {
    EndDrag();
    return;
  }
  float const* t = g_ViewTransform;
  float const* projection = g_ViewProjection;
  XMVECTOR camera = Row(t, 3);

  // The picture held with the mouse follows the cursor (before the dots are
  // drawn, so its dot is drawn where it went); let go, the drag ends
  WorldImage* pHeld = g_DragId ? FindImage(g_DragId) : nullptr;
  if (g_DragId && (!pHeld || !io.MouseDown[0]))
  {
    EndDrag();
    pHeld = nullptr;
  }
  if (pHeld)
  {
    XMVECTOR now = XMLoadFloat3((XMFLOAT3 const*)pHeld->Position);
    // Shift held or let go: from where it is now, the other way
    bool upDown = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    ImVec2 at;
    if (upDown != g_DragUpDown && ToScreen(t, projection, now, at))
    {
      g_DragUpDown = upDown;
      XMStoreFloat3((XMFLOAT3*)g_DragBase, now);
      g_DragOffsetX = at.x - io.MousePos.x;
      g_DragOffsetY = at.y - io.MousePos.y;
    }
    // The wheel: nearer or farther, along the line from the camera
    if (io.MouseWheel != 0.f)
    {
      XMVECTOR base = XMLoadFloat3((XMFLOAT3 const*)g_DragBase);
      XMVECTOR away = base - camera;
      float distance = XMVectorGetX(XMVector3Length(away));
      if (distance > 1e-3f)
      {
        float wanted = distance * powf(WheelStep, io.MouseWheel);
        wanted = wanted < DragNearest ? DragNearest : (wanted > DragFarthest ? DragFarthest : wanted);
        XMStoreFloat3((XMFLOAT3*)g_DragBase, camera + away * (wanted / distance));
      }
      io.MouseWheel = 0.f;
    }
    // Only once it moves: a click that doesn't move it changes nothing
    float place[3];
    if (DragPlace(t, projection, g_DragBase, g_DragUpDown, g_DragAlongGround, io.MousePos.x + g_DragOffsetX,
        io.MousePos.y + g_DragOffsetY, place) &&
        XMVectorGetX(XMVector3Length(XMLoadFloat3((XMFLOAT3 const*)place) - now)) > 1e-4f)
    {
      for (int i = 0; i < 3; ++i)
        pHeld->Position[i] = Clamp(place[i], -MaxPlace, MaxPlace);
      g_DragMoved = true;
    }
  }

  // Where each picture's middle is on the screen
  struct Dot
  {
    ImVec2 At;
    bool Shown;
  };
  std::vector<Dot> dots(g_Images.size());
  for (size_t i = 0; i < g_Images.size(); ++i)
    dots[i].Shown = ToScreen(t, projection, XMLoadFloat3((XMFLOAT3 const*)g_Images[i].Position), dots[i].At);

  // What the cursor is on, off the menu's windows: the nearest dot
  int hover = -1;
  if (!g_DragId && !io.WantCaptureMouse)
  {
    float reach = fontSize * 0.7f;
    float best = reach * reach;
    for (size_t i = 0; i < dots.size(); ++i)
    {
      if (!dots[i].Shown)
        continue;
      float dx = dots[i].At.x - io.MousePos.x, dy = dots[i].At.y - io.MousePos.y;
      if (dx * dx + dy * dy <= best)
      {
        best = dx * dx + dy * dy;
        hover = (int)i;
      }
    }
  }

  // A click on a dot (not on the menu): its picture is opened in the menu and
  // held. The click is the picture's: taken off ImGui's input, so the lights'
  // dots and the camera's drawn track path (drawn after) don't take it too.
  if (io.MouseClicked[0] && !io.MouseDownOwned[0] && !g_DragId && hover >= 0)
  {
    WorldImage& image = g_Images[hover];
    g_Selected = image.Id;
    g_OpenRequest = true;
    g_OpenSelected = true;
    g_DragId = image.Id;
    g_DragMoved = false;
    g_DragUpDown = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    memcpy(g_DragBase, image.Position, sizeof(g_DragBase));
    g_DragOffsetX = dots[hover].At.x - io.MousePos.x;
    g_DragOffsetY = dots[hover].At.y - io.MousePos.y;
    io.MouseClicked[0] = false;
  }

  // The dots: amber, grey while the picture is hidden; its number beside it
  const ImU32 hiddenColour = IM_COL32(140, 140, 140, 220);
  for (size_t i = 0; i < g_Images.size(); ++i)
  {
    if (!dots[i].Shown)
      continue;
    WorldImage const& image = g_Images[i];
    ImVec2 c = dots[i].At;
    bool held = g_DragId == image.Id;
    float size = (held || (int)i == hover) ? 1.3f : 1.f;
    pDrawList->AddRectFilled(ImVec2(c.x - fontSize * 0.36f * size, c.y - fontSize * 0.36f * size),
      ImVec2(c.x + fontSize * 0.36f * size, c.y + fontSize * 0.36f * size), shadow, fontSize * 0.1f);
    pDrawList->AddRectFilled(ImVec2(c.x - fontSize * 0.26f * size, c.y - fontSize * 0.26f * size),
      ImVec2(c.x + fontSize * 0.26f * size, c.y + fontSize * 0.26f * size), image.Shown ? outline : hiddenColour,
      fontSize * 0.06f);
    if (image.Id == g_Selected)
      pDrawList->AddCircle(c, fontSize * 0.6f * size, white, 20, 2.f);
    char number[16];
    sprintf_s(number, "%d", (int)i + 1);
    ImVec2 at(c.x + fontSize * 0.5f * size, c.y - fontSize * 1.1f * size);
    pDrawList->AddText(ImVec2(at.x + 1.f, at.y + 1.f), shadow, number);
    pDrawList->AddText(at, white, number);
  }

  // Its name next to the cursor, and what a drag does; while held, how far
  // away it is
  int named = hover;
  if (pHeld)
    named = (int)(pHeld - g_Images.data());
  if (named >= 0)
  {
    WorldImage const& image = g_Images[named];
    std::string text = "Picture " + std::to_string(named + 1) + ": " + image.File;
    if (pHeld)
    {
      char away[64];
      sprintf_s(away, ", %.1f m away%s", XMVectorGetX(XMVector3Length(XMLoadFloat3((XMFLOAT3 const*)image.Position) -
        camera)), g_DragUpDown ? " (up and down)" : "");
      text += away;
    }
    else
      text += "\nDrag to move it (Shift: up and down, wheel: nearer or farther)";
    ImVec2 at(io.MousePos.x + fontSize * 1.2f, io.MousePos.y + fontSize * 0.6f);
    pDrawList->AddText(ImVec2(at.x + 1, at.y + 1), shadow, text.c_str());
    pDrawList->AddText(at, white, text.c_str());
  }
}

bool worldimages::TakeOpenRequest()
{
  std::lock_guard<std::mutex> lock(g_Mutex);
  bool open = g_OpenRequest;
  g_OpenRequest = false;
  return open;
}

void worldimages::DrawUI()
{
  std::lock_guard<std::mutex> lock(g_Mutex);
  float fontSize = ImGui::GetFontSize();
  bool changed = false;
  if (!g_Scanned)
    Scan();
  g_OpenIds.clear();
  g_OpenIdsFrame = ImGui::GetFrameCount();

  ImGui::PushTextWrapPos(0.f);
  ImGui::TextDisabled("Your own pictures standing in the world, like the game's video screens in the city. Put JPG or "
    "PNG files in the folder below, pick one, then put it in front of the camera or click where it goes. While this "
    "menu is open each picture has a square dot at its middle: drag it to move the picture, click it to pick the "
    "picture here. They're shown with the free camera on or off.");
  ImGui::PopTextWrapPos();
  changed |= ImGui::Checkbox("Drag along the ground##Images", &g_DragAlongGround);
  ImGui::HelpMarker("Drag a picture's dot to move it across the screen, at the same distance. Hold Shift to move it "
    "only up and down, and turn the mouse wheel while dragging to take it nearer or farther. Only away from this "
    "menu's window. The number beside a dot is the picture's in the list below.\n"
    "Ticked, a drag keeps the picture at its height, moving it over the ground instead of across the screen. Shift "
    "still moves it up and down.");
  std::wstring folder = Folder();
  ImGui::TextDisabled("%s", Utf8(folder).c_str());
  if (ImGui::Button("Open the folder##Images"))
  {
    CreateDirectoryW(folder.c_str(), nullptr);
    ShellExecuteW(NULL, L"open", folder.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
  }
  ImGui::SameLine();
  if (ImGui::Button("Refresh folder##Images"))
  {
    Scan();
    // Pictures already placed read again from their files
    g_Pictures.clear();
  }
  ImGui::ItemTooltip("Lists the folder's pictures again, and reads the placed pictures again from their files (after "
    "you changed or replaced a file).");

  if (g_Files.empty())
    ImGui::TextDisabled("No JPG or PNG files in the folder yet.");
  else
  {
    int rows = (int)g_Files.size() < 6 ? (int)g_Files.size() : 6;
    if (ImGui::ListBoxHeader("##ImageFiles", ImVec2(-1.f, (rows + 0.5f) * ImGui::GetTextLineHeightWithSpacing())))
    {
      for (size_t i = 0; i < g_Files.size(); ++i)
      {
        ImGui::PushID((int)i);
        if (ImGui::Selectable(g_Files[i].c_str(), (int)i == g_SelectedFile))
          g_SelectedFile = (int)i;
        ImGui::PopID();
      }
      ImGui::ListBoxFooter();
    }
  }

  bool hasFile = g_SelectedFile >= 0 && g_SelectedFile < (int)g_Files.size();
  std::string file = hasFile ? g_Files[g_SelectedFile] : "";
  ImGui::PushItemWidth(fontSize * 12.f);
  changed |= ImGui::SliderFloat("Distance##Images", &g_Distance, 0.5f, 500.f, "%.1f m", 3.f);
  ImGui::PopItemWidth();
  ImGui::HelpMarker("How far in front of the camera \"Put it in front of the camera\" puts a picture, and how far "
    "away a click on the sky puts it.");
  if (g_Distance < 0.5f) g_Distance = 0.5f;
  if (ImGui::Button("Put it in front of the camera##Images") && hasFile)
  {
    g_FrontPending = true;
    g_FrontTarget = 0;
    g_FrontFile = file;
  }
  ImGui::ItemTooltip("Adds the picture picked above, Distance in front of the camera, facing it.");
  ImGui::SameLine();
  bool armedNew = g_Armed == 0;
  if (ImGui::Button(armedNew ? "Click on the picture... (or here to stop)###ImagesClickNew" : "Click where it goes###ImagesClickNew"))
  {
    if (armedNew)
      g_Armed = -1;
    else if (hasFile)
    {
      g_Armed = 0;
      g_ArmedFile = file;
    }
  }
  ImGui::HelpMarker("Then click anywhere on the picture outside the menu: the picture picked above goes there, flat "
    "on what you clicked (a wall, the ground), 2 cm out from it. On the sky it's put Distance away, facing the "
    "camera. With the Panini projection or the lens distortion on, the click lands a little off.");
  if (!hasFile)
    ImGui::TextDisabled("Pick a picture in the list first.");
  if (g_FrontPending || g_ClickPending || g_Pick.Copied)
    ImGui::TextDisabled("Waiting for the game's camera and depth (a level has to be loaded)...");
  else if (!g_Status.empty())
    ImGui::TextDisabled("%s", g_Status.c_str());

  // The placed pictures
  if (!g_Images.empty())
  {
    ImGui::SubHeading("Placed pictures");
    if (ImGui::ConfirmButton("Remove all##Images"))
    {
      g_Images.clear();
      g_DragId = 0;
      g_Armed = g_Armed > 0 ? -1 : g_Armed;
      changed = true;
    }
  }
  int removeId = 0, copyId = 0;
  for (size_t i = 0; i < g_Images.size(); ++i)
  {
    WorldImage& image = g_Images[i];
    Picture const* pPicture = FindPicture(image.File);
    ImGui::PushID(image.Id);
    std::string label = std::to_string(i + 1) + ". " + image.File + (image.Shown ? "" : "    [hidden]") + "###Image";
    // Its dot was clicked: opened
    if (g_OpenSelected && image.Id == g_Selected)
      ImGui::SetNextTreeNodeOpen(true);
    if (ImGui::TreeNode(label.c_str()))
    {
      g_OpenIds.push_back(image.Id);
      if (!pPicture || pPicture->State == Picture::Loading)
        ImGui::TextDisabled("Reading the file...");
      else if (pPicture->State == Picture::Failed)
        ImGui::TextColored(ImVec4(1.f, 0.6f, 0.3f, 1.f), "Couldn't be read: %s", pPicture->Error.c_str());
      else if (pPicture->Width != pPicture->FileWidth || pPicture->Height != pPicture->FileHeight)
        ImGui::TextDisabled("%u x %u pixels (drawn at %u x %u)", pPicture->FileWidth, pPicture->FileHeight, pPicture->Width,
          pPicture->Height);
      else
        ImGui::TextDisabled("%u x %u pixels", pPicture->Width, pPicture->Height);

      changed |= ImGui::Checkbox("Show it", &image.Shown);
      if (ImGui::Button("Move it in front of the camera"))
      {
        g_FrontPending = true;
        g_FrontTarget = image.Id;
      }
      ImGui::SameLine();
      bool armed = g_Armed == image.Id;
      if (ImGui::Button(armed ? "Click on the picture... (or here to stop)###Click" : "Move it: click where it goes###Click"))
        g_Armed = armed ? -1 : image.Id;

      ImGui::PushItemWidth(fontSize * 12.f);
      changed |= ImGui::DragFloat3("Place", image.Position, 0.05f, 0.f, 0.f, "%.2f");
      ImGui::HelpMarker("Its middle in the world, in metres. Drag a value left or right, or Ctrl+click it to type one.");
      // ImGui's drag gives the whole way dragged since the click: what it
      // moved since the last frame is moved, and it's back to 0 when let go
      float pushedBefore = image.Pushed;
      if (ImGui::DragFloat("Out / in", &image.Pushed, 0.01f, 0.f, 0.f, "<  drag  >") && image.Pushed != pushedBefore)
      {
        XMVECTOR right, up, front;
        Basis(image, g_RightHanded, g_ViewOk ? g_ViewTransform : nullptr, right, up, front);
        XMVECTOR middle = XMLoadFloat3((XMFLOAT3 const*)image.Position) + front * (image.Pushed - pushedBefore);
        XMStoreFloat3((XMFLOAT3*)image.Position, middle);
        changed = true;
      }
      if (!ImGui::IsItemActive())
        image.Pushed = 0.f;
      ImGui::HelpMarker("Drag right to move it out of its front (towards whoever sees it), left to move it back: "
        "away from a wall it's put on, or into it.");
      bool turned = false;
      ImGui::PushItemWidth(fontSize * 12.f);
      turned |= ImGui::SliderFloat("Turn", &image.Yaw, -180.f, 180.f, "%.1f degrees");
      turned |= ImGui::SliderFloat("Tilt", &image.Pitch, -90.f, 90.f, "%.1f degrees");
      turned |= ImGui::SliderFloat("Roll", &image.Roll, -180.f, 180.f, "%.1f degrees");
      ImGui::PopItemWidth();
      ImGui::HelpMarker("Turn: round the upright. Tilt: its front up (towards the sky) or down. Roll: the picture "
        "turned round its middle. Not used while it always faces the camera (only Roll is).");
      if (ImGui::Button("Stand it upright"))
      {
        image.Pitch = 0.f;
        image.Roll = 0.f;
        turned = true;
      }
      changed |= turned;
      changed |= ImGui::SliderFloat("Width", &image.Width, MinWidth, MaxWidth, "%.2f m", 4.f);
      float aspect = Aspect(pPicture);
      if (aspect > 0.f)
        ImGui::TextDisabled("Height: %.2f m (the picture's own shape)", image.Width * aspect);
      changed |= ImGui::SliderFloat("Opacity", &image.Opacity, 0.f, 1.f, "%.2f");
      changed |= ImGui::SliderFloat("Brightness", &image.Brightness, 0.f, 4.f, "%.2f", 2.f);
      ImGui::HelpMarker("1 shows the picture's own colors. It isn't lit by the game: lower it for a dark scene, "
        "raise it for a glowing screen.");
      ImGui::PopItemWidth();
      changed |= ImGui::Checkbox("Always face the camera", &image.FaceCamera);
      changed |= ImGui::Checkbox("Seen from behind too", &image.BothSides);
      ImGui::HelpMarker("From behind it shows the picture mirrored, as through a window. Off, it's only seen from its front.");
      changed |= ImGui::Checkbox("Mirror it", &image.Mirror);
      if (ImGui::Button("Make a copy"))
        copyId = image.Id;
      ImGui::ItemTooltip("Another one of the same picture, beside this one.");
      ImGui::SameLine();
      if (ImGui::Button("Remove"))
        removeId = image.Id;
      ImGui::TreePop();
    }
    ImGui::PopID();

    image.Width = Clamp(image.Width, MinWidth, MaxWidth);
    image.Opacity = Clamp(image.Opacity, 0.f, 1.f);
    image.Brightness = Clamp(image.Brightness, 0.f, 4.f);
    for (float& value : image.Position)
      value = Clamp(value, -MaxPlace, MaxPlace);
  }
  if (copyId)
  {
    WorldImage const* pImage = FindImage(copyId);
    if (pImage)
    {
      WorldImage copy = *pImage;
      copy.Id = g_NextId++;
      copy.Pushed = 0.f;
      XMVECTOR right, up, front;
      Basis(copy, g_RightHanded, g_ViewOk ? g_ViewTransform : nullptr, right, up, front);
      XMStoreFloat3((XMFLOAT3*)copy.Position, XMLoadFloat3((XMFLOAT3 const*)copy.Position) + right * (copy.Width * 1.05f));
      g_Images.push_back(copy);
      changed = true;
    }
  }
  if (removeId)
  {
    g_Images.erase(std::remove_if(g_Images.begin(), g_Images.end(), [&](WorldImage const& image) { return image.Id == removeId; }),
      g_Images.end());
    if (g_Armed == removeId)
      g_Armed = -1;
    if (g_DragId == removeId)
      g_DragId = 0;
    changed = true;
  }
  g_OpenSelected = false;
  if (changed)
    g_mainHandle->OnConfigChanged();
}

void worldimages::ReadConfig(INIReader* pReader)
{
  std::lock_guard<std::mutex> lock(g_Mutex);
  g_Distance = Clamp((float)pReader->GetReal("Images", "Distance", 10.0), 0.5f, 500.f);
  g_DragAlongGround = pReader->GetBoolean("Images", "DragAlongGround", false);
  long count = pReader->GetInteger("Images", "Count", 0);
  count = count < 0 ? 0 : (count > 256 ? 256 : count);
  g_Images.clear();
  for (long n = 1; n <= count; ++n)
  {
    std::string key = "Image" + std::to_string(n);
    WorldImage image;
    image.File = Decode(pReader->Get("Images", key + "File", ""));
    std::string place = pReader->Get("Images", key + "Place", "");
    std::string look = pReader->Get("Images", key + "Look", "");
    float* p = image.Position;
    if (image.File.empty() || sscanf_s(place.c_str(), "%f %f %f %f %f %f %f", &p[0], &p[1], &p[2], &image.Yaw, &image.Pitch,
        &image.Roll, &image.Width) != 7)
      continue;
    int shown = 1, faceCamera = 0, bothSides = 1, mirror = 0;
    sscanf_s(look.c_str(), "%f %f %d %d %d %d", &image.Opacity, &image.Brightness, &shown, &faceCamera, &bothSides, &mirror);
    image.Shown = shown != 0;
    image.FaceCamera = faceCamera != 0;
    image.BothSides = bothSides != 0;
    image.Mirror = mirror != 0;
    image.Width = Clamp(image.Width, MinWidth, MaxWidth);
    image.Opacity = Clamp(image.Opacity, 0.f, 1.f);
    image.Brightness = Clamp(image.Brightness, 0.f, 4.f);
    image.Yaw = Clamp(image.Yaw, -180.f, 180.f);
    image.Pitch = Clamp(image.Pitch, -90.f, 90.f);
    image.Roll = Clamp(image.Roll, -180.f, 180.f);
    for (float& value : image.Position)
      value = Clamp(value, -MaxPlace, MaxPlace);
    image.Id = g_NextId++;
    g_Images.push_back(image);
  }
}

std::string worldimages::GetConfig()
{
  std::lock_guard<std::mutex> lock(g_Mutex);
  std::string config = "[Images]\n";
  // snprintf: cut rather than the invalid parameter handler (sprintf_s ends
  // the game when a line doesn't fit). Every value is clamped (see DrawUI).
  char line[256];
  snprintf(line, sizeof(line), "Distance = %.3f\n", g_Distance);
  config += line;
  config += std::string("DragAlongGround = ") + (g_DragAlongGround ? "true" : "false") + "\n";
  snprintf(line, sizeof(line), "Count = %d\n", (int)g_Images.size());
  config += line;
  for (size_t i = 0; i < g_Images.size(); ++i)
  {
    WorldImage const& image = g_Images[i];
    int n = (int)i + 1;
    config += "Image" + std::to_string(n) + "File = " + Encode(image.File) + "\n";
    snprintf(line, sizeof(line), "Image%dPlace = %.3f %.3f %.3f %.2f %.2f %.2f %.3f\n", n, image.Position[0],
      image.Position[1], image.Position[2], image.Yaw, image.Pitch, image.Roll, image.Width);
    config += line;
    snprintf(line, sizeof(line), "Image%dLook = %.3f %.3f %d %d %d %d\n", n, image.Opacity, image.Brightness,
      image.Shown ? 1 : 0, image.FaceCamera ? 1 : 0, image.BothSides ? 1 : 0, image.Mirror ? 1 : 0);
    config += line;
  }
  config += "\n";
  return config;
}
