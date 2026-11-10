#include "PackedResource.h"
#include <bcrypt.h>
#include <stdio.h>
#include <string.h>

#pragma comment(lib, "bcrypt.lib")

namespace
{
  const size_t HeaderSize = 56;
  // The Compression API's algorithm numbers (compressapi.h)
  const DWORD AlgorithmFirst = 2, AlgorithmLast = 5;

  // compressapi.h's functions, taken from cabinet.dll when needed (Windows 8 and later)
  typedef BOOL(WINAPI* CreateDecompressorFn)(DWORD algorithm, void* routines, HANDLE* handle);
  typedef BOOL(WINAPI* DecompressFn)(HANDLE handle, void const* in, SIZE_T inSize, void* out, SIZE_T outSize,
    SIZE_T* outDataSize);
  typedef BOOL(WINAPI* CloseDecompressorFn)(HANDLE handle);

  bool Fail(char* whyNot, size_t whyNotSize, char const* what, DWORD error)
  {
    if (whyNot && whyNotSize)
    {
      if (error)
        sprintf_s(whyNot, whyNotSize, "%s (error %lu)", what, error);
      else
        sprintf_s(whyNot, whyNotSize, "%s", what);
    }
    return false;
  }

  bool Sha256(BYTE const* data, size_t size, BYTE(&hash)[32])
  {
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE h = nullptr;
    bool ok = BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0))
      && BCRYPT_SUCCESS(BCryptCreateHash(alg, &h, nullptr, 0, nullptr, 0, 0));
    // BCryptHashData takes a ULONG size: in pieces of 1 GB at most
    for (size_t at = 0; ok && at < size; )
    {
      size_t left = size - at;
      ULONG n = (ULONG)(left < ((size_t)1 << 30) ? left : ((size_t)1 << 30));
      ok = BCRYPT_SUCCESS(BCryptHashData(h, (PUCHAR)(data + at), n, 0));
      at += n;
    }
    ok = ok && BCRYPT_SUCCESS(BCryptFinishHash(h, hash, sizeof(hash), 0));
    if (h)
      BCryptDestroyHash(h);
    if (alg)
      BCryptCloseAlgorithmProvider(alg, 0);
    return ok;
  }
}

util::UnpackedResource::~UnpackedResource()
{
  if (Data)
    VirtualFree(Data, 0, MEM_RELEASE);
}

bool util::UnpackResource(BYTE const* p, size_t n, UnpackedResource& out, char* whyNot, size_t whyNotSize)
{
  if (out.Data)
    return Fail(whyNot, whyNotSize, "already unpacked", 0);
  if (!p || n <= HeaderSize || memcmp(p, "CTPK", 4) != 0)
    return Fail(whyNot, whyNotSize, "it isn't a packed resource", 0);
  DWORD version, algorithm, zero;
  unsigned long long rawSize;
  memcpy(&version, p + 4, 4);
  memcpy(&algorithm, p + 8, 4);
  memcpy(&zero, p + 12, 4);
  memcpy(&rawSize, p + 16, 8);
  if (version != 1 || algorithm < AlgorithmFirst || algorithm > AlgorithmLast || zero != 0 || rawSize == 0
    || rawSize > 0xFFFFFFFFull)
    return Fail(whyNot, whyNotSize, "its header isn't one this build reads", 0);

  // From the system folder only, never from the game's own
  wchar_t path[MAX_PATH];
  UINT dir = GetSystemDirectoryW(path, MAX_PATH);
  if (dir == 0 || dir + 13 >= MAX_PATH)
    return Fail(whyNot, whyNotSize, "the system folder isn't known", GetLastError());
  wcscat_s(path, L"\\cabinet.dll");
  HMODULE cabinet = LoadLibraryW(path);
  if (!cabinet)
    return Fail(whyNot, whyNotSize, "cabinet.dll (Windows 8 or later) can't be loaded", GetLastError());
  auto createDecompressor = (CreateDecompressorFn)GetProcAddress(cabinet, "CreateDecompressor");
  auto decompress = (DecompressFn)GetProcAddress(cabinet, "Decompress");
  auto closeDecompressor = (CloseDecompressorFn)GetProcAddress(cabinet, "CloseDecompressor");
  HANDLE h = nullptr;
  if (!createDecompressor || !decompress || !closeDecompressor || !createDecompressor(algorithm, nullptr, &h))
  {
    DWORD error = GetLastError();
    FreeLibrary(cabinet);
    return Fail(whyNot, whyNotSize, "Windows' decompressor can't be made", error);
  }

  BYTE* data = nullptr;
  char const* failed = nullptr;
  DWORD error = 0;
  // A call with no buffer fails with ERROR_INSUFFICIENT_BUFFER and gives the stream's own unpacked size
  SIZE_T need = 0;
  BOOL sized = decompress(h, p + HeaderSize, n - HeaderSize, nullptr, 0, &need);
  DWORD sizeError = GetLastError();
  if (sized || sizeError != ERROR_INSUFFICIENT_BUFFER || need != rawSize)
  {
    failed = "the stream's size isn't the header's";
    error = sized ? 0 : sizeError;
  }
  else if (!(data = (BYTE*)VirtualAlloc(nullptr, (SIZE_T)rawSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE)))
  {
    failed = "there's no memory for it";
    error = GetLastError();
  }
  else
  {
    SIZE_T got = 0;
    if (!decompress(h, p + HeaderSize, n - HeaderSize, data, (SIZE_T)rawSize, &got))
    {
      failed = "it can't be unpacked";
      error = GetLastError();
    }
    else if (got != rawSize)
      failed = "it unpacked to another size";
  }
  closeDecompressor(h);
  FreeLibrary(cabinet);

  BYTE hash[32];
  if (!failed && (!Sha256(data, (size_t)rawSize, hash) || memcmp(hash, p + 24, sizeof(hash)) != 0))
    failed = "the unpacked data's SHA-256 isn't the header's";
  if (failed)
  {
    if (data)
      VirtualFree(data, 0, MEM_RELEASE);
    return Fail(whyNot, whyNotSize, failed, error);
  }
  out.Data = data;
  out.Size = (size_t)rawSize;
  return true;
}
