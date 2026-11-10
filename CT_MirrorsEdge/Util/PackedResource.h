#pragma once
#include <Windows.h>
#include <cstddef>

// A resource the build stores compressed (tools/pack_resource.py): a 56-byte header, then a stream of Windows' own
// Compression API (cabinet.dll, buffer mode). The header: "CTPK", u32 version 1, u32 the algorithm
// (COMPRESS_ALGORITHM_*: 2 MSZIP, 3 XPRESS, 4 XPRESS_HUFF, 5 LZMS), u32 0, u64 the unpacked size, then the unpacked
// data's SHA-256 (32 bytes).
namespace util
{
  // The unpacked data, in memory of its own, freed when this goes out of scope
  struct UnpackedResource
  {
    BYTE* Data = nullptr;
    size_t Size = 0;

    UnpackedResource() = default;
    UnpackedResource(UnpackedResource const&) = delete;
    UnpackedResource& operator=(UnpackedResource const&) = delete;
    ~UnpackedResource();
  };

  // Unpacks a packed blob after checking its header, the stream's own size and the data's SHA-256. cabinet.dll is
  // taken from the system folder (the DLL imports it anyway, for ReplayStore's saved recordings). On failure, whyNot
  // says what failed.
  bool UnpackResource(BYTE const* packed, size_t size, UnpackedResource& out, char* whyNot, size_t whyNotSize);
}
