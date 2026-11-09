#include "Util.h"
#include "../Main.h"

#include <fstream>
#include <unordered_map>
#include <Psapi.h>

namespace
{
  bool m_UseScannedResults = false;
  std::unordered_map<std::string, util::offsets::Signature> m_Signatures;

  // Module-relative offsets taken from the original CT_MirrorsEdge.dll (2018-02-10).
  // They only match the exact MirrorsEdgeCatalyst.exe build that tool targeted
  // (PE timestamp 0x5762E3B7, 2016-06-16, also the current Steam / EA app build),
  // and a wrong address here means a crash, so they're only used as a fallback
  // when the game's timestamp matches.
  const DWORD g_supportedTimeStamp = 0x5762E3B7;
  bool m_UseHardcodedFallback = false;
  std::unordered_map<std::string, __int64> m_HardcodedOffsets =
  {
    { "OFFSET_DXRENDERER",          0x2563300 },
    { "OFFSET_GAMERENDERER",        0x2563A10 },
    { "OFFSET_SYSTEM",              0x2413A00 },
    { "OFFSET_GAMERENDERSETTINGS",  0x2142A58 },
    { "OFFSET_WORLDRENDERSETTINGS", 0x240C2B8 },
    { "OFFSET_GAMETIMESETTINGS",    0x2142A68 },
    { "OFFSET_UISETTINGS",          0x2142B08 },
    { "OFFSET_CLIENTGAMECONTEXT",   0x24A9F80 },
    { "OFFSET_MAIN",                0x2142A48 },
    { "OFFSET_3PTOGGLE",            0x1B18E58 },
    { "OFFSET_CAMERAUPDATE",        0x352D810 },
    { "OFFSET_ENTRYINPUT",          0x32FD3B0 },
    { "OFFSET_CONTROLLERINPUT",     0x37BABA0 },
    { "OFFSET_MESHSETTINGS",        0x23E3A70 },
    { "OFFSET_VISUALENVUPDATE",     0x36ED460 },
    { "OFFSET_UPDATELIGHT",         0x376A030 },
    { "OFFSET_RESOURCEMANAGER",     0x237DE70 },
    { "OFFSET_DOFFIX",              0x35C72EE },
    // Not from the original tools: the static pointer MEC-TC (Mirror's Edge Catalyst
    // Time Changer) reads the time of day through, [[[p] + 0x8] + 0x28] + 0x30. No signature yet.
    { "OFFSET_TIMEOFDAY",           0x255C2F8 },
    // Pointer to the mesh streaming manager
    { "OFFSET_MESHSTREAMING",       0x2563410 },
    // Not from the original tools: the city's zone streamer
    { "OFFSET_ZONESTREAMERUPDATE",  0x3C81900 },
    { "OFFSET_ZONEFOCUSADD",        0x3C7E7C0 },
    { "OFFSET_ZONEFOCUSREMOVE",     0x3C81390 },
    { "OFFSET_ZONELOADORDER",       0x3C87880 },
    { "OFFSET_VISTASHOWZONE",       0x3C87F60 },
    { "OFFSET_MESHGATHER",          0x32968E0 },
    { "OFFSET_ZONESTEP",            0x3C81CD0 },
    { "OFFSET_LISTENERTRANSFORM",   0x301DBC0 },
    { "OFFSET_LISTENERVELOCITY",    0x301DCA0 },
    { "OFFSET_LISTENERSPACE",       0x2409C08 },
  };

  bool DataCompare(BYTE* pData, BYTE* bSig, const char* szMask)
  {
    for (; *szMask; ++szMask, ++pData, ++bSig)
    {
      if (*szMask == 'x' && *pData != *bSig)
        return false;
    }
    return (*szMask) == 0;
  }

  BYTE* FindPattern(BYTE* dwAddress, __int64 dwSize, BYTE* pbSig, const char* szMask)
  {
    BYTE bFirstByte = *(BYTE*)pbSig;

    __int64 length = (__int64)dwAddress + dwSize - strlen(szMask);

    for (__int64 i = (__int64)dwAddress; i < length; i += 4) // might run faster with 8 bytes but I am too lazy
    {
      unsigned x = *(unsigned*)(i);

      if ((x & 0xFF) == bFirstByte)
        if (DataCompare(reinterpret_cast<BYTE*>(i), pbSig, szMask))
          return reinterpret_cast<BYTE*>(i);

      if ((x & 0xFF00) >> 8 == bFirstByte)
        if (DataCompare(reinterpret_cast<BYTE*>(i + 1), pbSig, szMask))
          return reinterpret_cast<BYTE*>(i + 1);

      if ((x & 0xFF0000) >> 16 == bFirstByte)
        if (DataCompare(reinterpret_cast<BYTE*>(i + 2), pbSig, szMask))
          return reinterpret_cast<BYTE*>(i + 2);

      if ((x & 0xFF000000) >> 24 == bFirstByte)
        if (DataCompare(reinterpret_cast<BYTE*>(i + 3), pbSig, szMask))
          return reinterpret_cast<BYTE*>(i + 3);
    }
    return 0;
  }

  // Page protections can change while the game decrypts itself, so an
  // access violation just counts as "not found" instead of crashing the game.
  BYTE* SafeFindPattern(BYTE* pStart, __int64 size, BYTE* pbSig, const char* szMask)
  {
    __try
    {
      return FindPattern(pStart, size, pbSig, szMask);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return nullptr;
    }
  }

  // Searches the committed, readable parts of the game module
  __int64 ScanModule(util::offsets::Signature const& sig)
  {
    MODULEINFO info;
    if (!GetModuleInformation(GetCurrentProcess(), g_gameHandle, &info, sizeof(MODULEINFO)))
      return 0;

    BYTE* pEnd = (BYTE*)info.lpBaseOfDll + info.SizeOfImage;
    BYTE* pRunStart = nullptr;
    BYTE* pCurrent = (BYTE*)info.lpBaseOfDll;

    while (pCurrent < pEnd)
    {
      MEMORY_BASIC_INFORMATION mbi;
      if (!VirtualQuery(pCurrent, &mbi, sizeof(mbi)))
        break;

      BYTE* pRegionEnd = (BYTE*)mbi.BaseAddress + mbi.RegionSize;
      if (pRegionEnd > pEnd)
        pRegionEnd = pEnd;

      const DWORD readableFlags = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
        PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
      bool readable = mbi.State == MEM_COMMIT && (mbi.Protect & readableFlags) && !(mbi.Protect & PAGE_GUARD);

      // Neighbouring readable regions are scanned as one run so that
      // patterns crossing a region boundary are still found
      if (readable && !pRunStart)
        pRunStart = pCurrent;
      else if (!readable && pRunStart)
      {
        BYTE* pResult = SafeFindPattern(pRunStart, pCurrent - pRunStart, sig.Pattern, sig.Mask.c_str());
        if (pResult)
          return (__int64)pResult;
        pRunStart = nullptr;
      }

      pCurrent = pRegionEnd;
    }

    if (pRunStart)
      return (__int64)SafeFindPattern(pRunStart, pCurrent - pRunStart, sig.Pattern, sig.Mask.c_str());

    return 0;
  }

  DWORD GetGameTimeStamp()
  {
    IMAGE_DOS_HEADER* pDosHeader = (IMAGE_DOS_HEADER*)g_gameHandle;
    if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE)
      return 0;

    IMAGE_NT_HEADERS* pNtHeaders = (IMAGE_NT_HEADERS*)((BYTE*)g_gameHandle + pDosHeader->e_lfanew);
    if (pNtHeaders->Signature != IMAGE_NT_SIGNATURE)
      return 0;

    return pNtHeaders->FileHeader.TimeDateStamp;
  }

  // The renderer's instance arena (CameraManager.cpp, RenderArenaRva) is set
  // aside once while the game starts, 16 MB, and streaming the city around the
  // camera fills it (the level alone takes a third). Its size is two constants
  // in the code that sets it aside (0x1436D4CE0), so it's made 128 MB here,
  // before the game gets there (about a second after its code is decrypted).
  // Only one byte of each changes: the size asked for, 0xE6FFA20F + disp32,
  // from 0x01000010 to 0x08000010, and the size it records, from 16 MB to 128
  // (64 until the whole city: 191325 instances took 36.4 MB).
  // The first goes first: if the game runs it in between, it gets 128 MB but
  // records 16, and the tools hold back too early rather than too late.
  enum class ArenaPatch { Done, TooLate, Unexpected, Failed };

  ArenaPatch TryEnlargeRenderArena()
  {
    BYTE* pSizeLea = (BYTE*)g_gameHandle + 0x36D4D00;   // lea edx, [rdx + 0x1A005E01]
    BYTE* pSizeStore = (BYTE*)g_gameHandle + 0x36D4D12; // mov dword ptr [rip + x], 0x01000000
    __int64* pArenaStart = (__int64*)((BYTE*)g_gameHandle + 0x2568C08);
    __try
    {
      if (*pArenaStart)
        return ArenaPatch::TooLate;
      bool leaOk = pSizeLea[0] == 0x8D && pSizeLea[1] == 0x92 && pSizeLea[2] == 0x01 && pSizeLea[3] == 0x5E &&
        pSizeLea[4] == 0x00 && pSizeLea[5] == 0x1A;
      bool storeOk = pSizeStore[0] == 0xC7 && pSizeStore[1] == 0x05 && pSizeStore[6] == 0x00 &&
        pSizeStore[7] == 0x00 && pSizeStore[8] == 0x00 && pSizeStore[9] == 0x01;
      if (!leaOk || !storeOk)
        return ArenaPatch::Unexpected;

      DWORD oldProtect;
      if (!VirtualProtect(pSizeLea, 0x20, PAGE_EXECUTE_READWRITE, &oldProtect))
        return ArenaPatch::Failed;
      pSizeLea[5] = 0x21;   // 0x08000010 bytes
      pSizeStore[9] = 0x08; // records 0x08000000
      VirtualProtect(pSizeLea, 0x20, oldProtect, &oldProtect);
      FlushInstructionCache(GetCurrentProcess(), pSizeLea, 0x20);
      return ArenaPatch::Done;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return ArenaPatch::Failed;
    }
  }

  bool RenderArenaStarted()
  {
    __try
    {
      return *(__int64*)((BYTE*)g_gameHandle + 0x2568C08) != 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return false;
    }
  }

  void RecordRenderArenaSize(DWORD bytes)
  {
    __try
    {
      *(DWORD*)((BYTE*)g_gameHandle + 0x2568C10) = bytes;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
  }

  void EnlargeRenderArena()
  {
    // The code may still be decrypting for a moment: a few tries
    ArenaPatch result = ArenaPatch::Unexpected;
    for (int attempt = 0; attempt < 300 && !g_shutdown; ++attempt)
    {
      result = TryEnlargeRenderArena();
      if (result != ArenaPatch::Unexpected)
        break;
      Sleep(10);
    }
    if (result == ArenaPatch::Done)
    {
      // Set aside right as it was patched, the game may have been in the
      // middle: asked for 16 MB (the old size) and then recorded 64. The
      // record is only read by the tools (the game writes it there and clears
      // it when it frees the memory), so it's set back to 16, the safe side
      // (the tools then hold back as if it were 16). The game records the
      // size before the start (0x1436D4D12, then 0x1436D4D23), so once the
      // start is there the game's own write is done.
      ULONGLONG patched = GetTickCount64();
      bool soon = false;
      while (!soon && GetTickCount64() - patched < 100)
      {
        soon = RenderArenaStarted();
        if (!soon)
          Sleep(1);
      }
      if (soon)
      {
        RecordRenderArenaSize(0x1000000);
        util::log::Warning("The renderer's instance memory was set aside just as it was made bigger, the tools treat it as 16 MB");
      }
      else
        util::log::Ok("The renderer's instance memory will be 128 MB instead of 16 MB");
    }
    else if (result == ArenaPatch::TooLate)
      util::log::Warning("The renderer's instance memory was already set aside (16 MB), too late to make it bigger");
    else
      util::log::Warning("The renderer's instance memory couldn't be made bigger (%s), it stays 16 MB",
        result == ArenaPatch::Unexpected ? "its code isn't what was expected" : "the code couldn't be changed");
  }

  // Every frame the renderer gathers what each view draws (the main view and
  // the sun shadow's layers), 0x18 bytes a record, into one scratch buffer
  // (0x1435CDB10) of a fixed 0xC0000 bytes: room for 32768. Nothing checks
  // that: with the whole city loaded (a mod) and a long shadow distance it ran
  // far past it and the game crashed reading them back (exe + 0x35CDEAB). Its
  // size is one constant, 0xC34EA20F + disp32 (0x1435CDB58, 0x1435CDB6F), made
  // 32 MB here (about 1.4 million records). The scratch allocator gives a
  // request that big a block of its own (0x142A3A010), only touched as far
  // as it's used.
  enum class RecordsPatch { Done, Unexpected, Failed };

  RecordsPatch TryEnlargeViewRecords()
  {
    BYTE* pSizeMov = (BYTE*)g_gameHandle + 0x35CDB58; // mov edx, 0xC34EA20F
    BYTE* pSizeLea = (BYTE*)g_gameHandle + 0x35CDB6F; // lea edx, [rdx + 0x3CBD5DF1]
    __try
    {
      bool movOk = pSizeMov[0] == 0xBA && *(DWORD*)(pSizeMov + 1) == 0xC34EA20F;
      bool leaOk = pSizeLea[0] == 0x8D && pSizeLea[1] == 0x92 && *(DWORD*)(pSizeLea + 2) == 0x3CBD5DF1;
      if (!movOk || !leaOk)
        return RecordsPatch::Unexpected;
      DWORD oldProtect;
      if (!VirtualProtect(pSizeLea, 8, PAGE_EXECUTE_READWRITE, &oldProtect))
        return RecordsPatch::Failed;
      *(DWORD*)(pSizeLea + 2) = 0x3EB15DF1; // 0xC34EA20F + 0x3EB15DF1 = 0x02000000
      VirtualProtect(pSizeLea, 8, oldProtect, &oldProtect);
      FlushInstructionCache(GetCurrentProcess(), pSizeLea, 8);
      return RecordsPatch::Done;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return RecordsPatch::Failed;
    }
  }

  void EnlargeViewRecords()
  {
    // Like the arena: the code may still be decrypting for a moment
    RecordsPatch result = RecordsPatch::Unexpected;
    for (int attempt = 0; attempt < 300 && !g_shutdown; ++attempt)
    {
      result = TryEnlargeViewRecords();
      if (result != RecordsPatch::Unexpected)
        break;
      Sleep(10);
    }
    if (result == RecordsPatch::Done)
      util::log::Ok("The renderer's list of what each view draws has room for 1.4 million instead of 32768");
    else
      util::log::Warning("The renderer's list of what each view draws couldn't be made bigger (%s), it stays at 32768",
        result == RecordsPatch::Unexpected ? "its code isn't what was expected" : "the code couldn't be changed");
  }

  // The renderer's draw batches (CameraManager.cpp, BatchChunkPointers) keep
  // where an instance is as a handle: category << 28 | batch << 16 | chunk.
  // The add (0x1436C3AC8) writes the chunk whole, but the update
  // (0x1436C8CF3) and the take-out (0x1436D27E8) read it with
  // and r8d, 0x3FF: an instance in chunk 1024 or later is looked for in the
  // wrong chunk and never found, so it's never taken out and stays in the
  // batch after its zone's memory is freed. A batch has room for 2048 chunk
  // pointers and bits 10 - 15 are unused, so both read 11 bits here (one byte
  // each, 0x3FF -> 0x7FF). They're the only reads of a chunk from a handle
  // with that mask in the game code; the jobs that go through a batch go by
  // its chunk count.
  enum class BatchPatch { Done, Unexpected, Failed };
  bool g_BatchChunkIndexWidened = false;

  // Each renderer object keeps its instances' handles in a list with room for
  // 200000 (made by 0x1436BFC90 for the object at renderer + 0x142768: a u32
  // array that is also the free list, next[i] = i + 1 and the last -1, and a
  // u64 array, both sized by one constant, 0x90C3050F + 0x6F400831). The add
  // (0x1436C40B0) takes the head without a check: with the list used up it
  // takes -1, writes 16 GB past the array and crashes on the next one (seen
  // with the whole city: 191325 instances in one category). Made 1000000
  // here, before the game makes a renderer (each level load); the two sizes
  // for an array that's already big enough (0x1436BFD62, 0x1436BFD99) go with
  // it. The tools read each renderer's room from its array.
  const unsigned int RendererInstanceRoom = 1000000;

  RecordsPatch TryEnlargeRendererInstances()
  {
    static const BYTE roomCode[] = { 0xBF, 0x0F, 0x05, 0xC3, 0x90, 0x8D, 0xBF, 0x31, 0x08, 0x40, 0x6F };
    static const BYTE arrayCode[] = { 0x48, 0x8D, 0x81, 0x00, 0x35, 0x0C, 0x00 };
    static const BYTE recordsCode[] = { 0x48, 0x8D, 0x81, 0x00, 0x6A, 0x18, 0x00 };
    BYTE* pRoom = (BYTE*)g_gameHandle + 0x36BFD3C;   // mov edi, 0x90C3050F; lea edi, [rdi + disp]
    BYTE* pSmall = (BYTE*)g_gameHandle + 0x36BFD62;  // lea rax, [rcx + 200000 * 4]
    BYTE* pBig = (BYTE*)g_gameHandle + 0x36BFD99;    // lea rax, [rcx + 200000 * 8]
    __try
    {
      if (memcmp(pRoom, roomCode, sizeof(roomCode)) || memcmp(pSmall, arrayCode, sizeof(arrayCode)) || memcmp(pBig, recordsCode, sizeof(recordsCode)))
        return RecordsPatch::Unexpected;
      BYTE* pPage = (BYTE*)((ULONG_PTR)pRoom & ~(ULONG_PTR)0xFFF);
      if ((BYTE*)((ULONG_PTR)(pBig + sizeof(recordsCode)) & ~(ULONG_PTR)0xFFF) != pPage)
        return RecordsPatch::Unexpected;
      DWORD oldProtect;
      if (!VirtualProtect(pPage, 0x1000, PAGE_EXECUTE_READWRITE, &oldProtect))
        return RecordsPatch::Failed;
      const unsigned int disp = RendererInstanceRoom - 0x90C3050Fu;
      const unsigned int smallSize = RendererInstanceRoom * 4;
      const unsigned int bigSize = RendererInstanceRoom * 8;
      memcpy(pRoom + 7, &disp, 4);
      memcpy(pSmall + 3, &smallSize, 4);
      memcpy(pBig + 3, &bigSize, 4);
      VirtualProtect(pPage, 0x1000, oldProtect, &oldProtect);
      FlushInstructionCache(GetCurrentProcess(), pPage, 0x1000);
      return RecordsPatch::Done;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return RecordsPatch::Failed;
    }
  }

  void EnlargeRendererInstances()
  {
    RecordsPatch result = RecordsPatch::Unexpected;
    for (int attempt = 0; attempt < 300 && !g_shutdown; ++attempt)
    {
      result = TryEnlargeRendererInstances();
      if (result != RecordsPatch::Unexpected)
        break;
      Sleep(10);
    }
    if (result == RecordsPatch::Done)
      util::log::Ok("The renderer's instance list has room for %u instead of 200000", RendererInstanceRoom);
    else
      util::log::Warning("The renderer's instance list couldn't be made bigger (%s), it stays at 200000",
        result == RecordsPatch::Unexpected ? "its code isn't what was expected" : "the code couldn't be changed");
  }

  BatchPatch TryWidenBatchChunkIndex()
  {
    // and r8d, 0x3FF, then the category and batch shifts after it
    static const BYTE update[] = { 0x41, 0x81, 0xE0, 0xFF, 0x03, 0x00, 0x00, 0xC1, 0xEA, 0x1C, 0xC1, 0xE8, 0x10 };
    static const BYTE takeOut[] = { 0x41, 0x81, 0xE0, 0xFF, 0x03, 0x00, 0x00, 0x48, 0xC1, 0xE9, 0x10, 0x48, 0xC1, 0xE8, 0x1C };
    BYTE* pUpdate = (BYTE*)g_gameHandle + 0x36C8CF3;
    BYTE* pTakeOut = (BYTE*)g_gameHandle + 0x36D27E8;
    __try
    {
      if (memcmp(pUpdate, update, sizeof(update)) || memcmp(pTakeOut, takeOut, sizeof(takeOut)))
        return BatchPatch::Unexpected;

      // Both writable first, so it's both or neither
      DWORD updateProtect = 0, takeOutProtect = 0;
      if (!VirtualProtect(pUpdate + 4, 1, PAGE_EXECUTE_READWRITE, &updateProtect))
        return BatchPatch::Failed;
      if (!VirtualProtect(pTakeOut + 4, 1, PAGE_EXECUTE_READWRITE, &takeOutProtect))
      {
        VirtualProtect(pUpdate + 4, 1, updateProtect, &updateProtect);
        return BatchPatch::Failed;
      }
      pUpdate[4] = 0x07;
      pTakeOut[4] = 0x07;
      VirtualProtect(pUpdate + 4, 1, updateProtect, &updateProtect);
      VirtualProtect(pTakeOut + 4, 1, takeOutProtect, &takeOutProtect);
      FlushInstructionCache(GetCurrentProcess(), pUpdate, sizeof(update));
      FlushInstructionCache(GetCurrentProcess(), pTakeOut, sizeof(takeOut));
      return BatchPatch::Done;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      return BatchPatch::Failed;
    }
  }

  void WidenBatchChunkIndex()
  {
    // Like the arena: the code may still be decrypting for a moment
    BatchPatch result = BatchPatch::Unexpected;
    for (int attempt = 0; attempt < 300 && !g_shutdown; ++attempt)
    {
      result = TryWidenBatchChunkIndex();
      if (result != BatchPatch::Unexpected)
        break;
      Sleep(10);
    }
    g_BatchChunkIndexWidened = result == BatchPatch::Done;
    if (g_BatchChunkIndexWidened)
      util::log::Ok("The renderer's draw batches can use all 2048 of their chunks (the game lost track of instances past the first 1024)");
    else
      util::log::Warning("The renderer's draw batches couldn't be fixed (%s): past 1024 chunks the game loses track of instances, the tools hold back before that",
        result == BatchPatch::Unexpected ? "its code isn't what was expected" : "the code couldn't be changed");
  }

  bool ResolveSignature(std::string const& name, util::offsets::Signature& sig)
  {
    __int64 result = ScanModule(sig);
    if (!result)
      return false;

    if (sig.HasReference)
    {
      // Get the assembly reference
      int* pReference = (int*)(result + sig.ReferenceOffset);
      // Assembly reference is relative to the address after the reference
      sig.Result = ((__int64)pReference + sig.ReferenceSize) + *pReference;
      sig.Result += sig.AddOffset;
    }
    else
      sig.Result = result + sig.AddOffset;

    util::log::Write("%-28s 0x%I64X", name.c_str(), sig.Result);
    return true;
  }
}

util::offsets::Signature::Signature(std::string const& sig, int offset /* = 0 */)
{
  Pattern = new BYTE[sig.size()]();
  AddOffset = offset;

  unsigned int patternOffset = 0;
  for (unsigned int i = 0; i < sig.size(); ++i)
  {
    switch (sig[i])
    {
      case ' ':
      {
        break;
      }
      case '[':
      {
        HasReference = true;
        ReferenceOffset = patternOffset;
        break;
      }
      case ']':
      {
        ReferenceSize = patternOffset - ReferenceOffset;
        break;
      }
      case '?':
      {
        Mask += '?';
        // In signature it's clearer to mark one wildcard byte as ??
        // so skip the next character.
        Pattern[patternOffset++] = 0;
        i += 1;
        break;
      }
      default:
      {
        Mask += 'x';
        // Process 2 characters into a single byte
        Pattern[patternOffset++] = (util::CharToByte(sig[i]) << 4) + util::CharToByte(sig[i+1]);
        i += 1;
      }
    }
  }
}

void util::offsets::Scan()
{
  // Signature example
  // Scan memory and find this pattern. Question marks are wildcard bytes.
  // Brackets mean that the offset is extracted from the assembly reference
  // For example:
  // mov rax,[123456]
  // We want 123456, its place should be marked with wildcard bytes surrounded
  // by brackets in the signature.
  //
  // The last argument is the offset to be added to the result, useful when
  // you need a code offset for byte patches.
  //
  // Signatures are copied from the original CT_MirrorsEdge.dll.
  // Bracketed ones resolve to a static pointer (or a call target for
  // CAMERAUPDATE), the rest are the address of the match itself.

  m_Signatures.emplace("OFFSET_DXRENDERER",          Signature("48 8B 0D [ ?? ?? ?? ?? ] 48 8B 53 10 43 8D 04 76"));
  m_Signatures.emplace("OFFSET_GAMERENDERER",        Signature("48 8B 0D [ ?? ?? ?? ?? ] 48 8B 01 31 D2 FF 50 60"));
  m_Signatures.emplace("OFFSET_SYSTEM",              Signature("74 09 48 89 C1 E8 ?? ?? ?? ?? 90 48 89 05 [ ?? ?? ?? ?? ] E8"));
  m_Signatures.emplace("OFFSET_GAMERENDERSETTINGS",  Signature("48 8B 05 [ ?? ?? ?? ?? ] 80 B8 ?? ?? ?? ?? ?? 74 52"));
  m_Signatures.emplace("OFFSET_WORLDRENDERSETTINGS", Signature("48 89 05 [ ?? ?? ?? ?? ] 0F B6 88 ?? ?? ?? ?? 8B 80"));
  m_Signatures.emplace("OFFSET_GAMETIMESETTINGS",    Signature("74 1A 48 8B 15 [ ?? ?? ?? ?? ]"));
  m_Signatures.emplace("OFFSET_UISETTINGS",          Signature("48 89 05 [ ?? ?? ?? ?? ] 83 78 20 00 75 0D"));
  m_Signatures.emplace("OFFSET_CLIENTGAMECONTEXT",   Signature("48 8D 05 [ ?? ?? ?? ?? ] 48 8B 04 D0 48 85 C0"));
  m_Signatures.emplace("OFFSET_MAIN",                Signature("48 89 3D [ ?? ?? ?? ?? ] C7 87"));
  m_Signatures.emplace("OFFSET_3PTOGGLE",            Signature("48 8D 05 [ ?? ?? ?? ?? ] 48 89 03 31 F6 48 89 B3 ?? ?? ?? ?? 48 89 B3 ?? ?? ?? ?? 0F 57 C0"));
  m_Signatures.emplace("OFFSET_CAMERAUPDATE",        Signature("48 89 D9 E8 [ ?? ?? ?? ?? ] 0F B6 45 07"));
  m_Signatures.emplace("OFFSET_ENTRYINPUT",          Signature("3B 15 ?? ?? ?? ?? 73 2C"));
  m_Signatures.emplace("OFFSET_CONTROLLERINPUT",     Signature("0F 57 DB 30 C9"));
  m_Signatures.emplace("OFFSET_MESHSETTINGS",        Signature("48 89 05 [ ?? ?? ?? ?? ] 8B 40 44"));
  m_Signatures.emplace("OFFSET_VISUALENVUPDATE",     Signature("48 89 C2 E8 [ ?? ?? ?? ?? ] 45 31 C0"));
  m_Signatures.emplace("OFFSET_UPDATELIGHT",         Signature("48 83 EC 48 80 B9 ?? ?? ?? ?? ?? 75 44"));
  m_Signatures.emplace("OFFSET_RESOURCEMANAGER",     Signature("48 8B 14 D8 48 8B 0D [ ?? ?? ?? ?? ]"));
  m_Signatures.emplace("OFFSET_DOFFIX",              Signature("48 8B 48 08 48 8B 01 FF 50 58"));

  // Not from the original tools. ZONESTREAMERUPDATE is the start of ZoneStreamerEntity's
  // update, the other two are the calls ZoneStreamerFocusEntity makes when it's enabled
  // (add) and disabled (remove).
  m_Signatures.emplace("OFFSET_ZONESTREAMERUPDATE",  Signature("57 48 83 EC 50 48 C7 44 24 20 FE FF FF FF 48 89 5C 24 60 48 89 6C 24 68 48 89 74 24 70 0F 29 74 24 40 48 89 CF F3 0F 10 32"));
  m_Signatures.emplace("OFFSET_ZONEFOCUSADD",        Signature("74 0B 4C 8D 43 40 31 D2 E8 [ ?? ?? ?? ?? ]"));
  m_Signatures.emplace("OFFSET_ZONEFOCUSREMOVE",     Signature("48 8B 49 48 48 85 C9 74 2B 4C 8D 43 40 31 D2 E8 [ ?? ?? ?? ?? ]"));
  // The zone streamer's sorter, ordering zones to load (it passes 1; the one
  // for unloading, right after it, passes -1)
  m_Signatures.emplace("OFFSET_ZONELOADORDER",       Signature("48 83 EC 68 48 8B 41 08 F3 0F 10 02 F3 0F 10 4A 08 48 89 44 24 28 48 8D 44 24 70 C7 44 24 20 01 00 00 00"));
  // A streamer's zone step: release the zones asked to unload, load the ones
  // asked to load, clear the requests (0x143C81E60, 0x143C81D20; the game
  // runs it at a level load)
  m_Signatures.emplace("OFFSET_ZONESTEP",            Signature("53 48 83 EC 20 48 89 CB E8 ?? ?? ?? ?? 48 89 D9 E8 ?? ?? ?? ?? 48 8B 43 28 48 8B 90 80 00 00 00 8B 4A FC 85 C9 74 19"));
  // ZoneStreamerVistaEntity showing or hiding the objects of one of its zones
  m_Signatures.emplace("OFFSET_VISTASHOWZONE",       Signature("48 89 5C 24 18 56 41 54 41 55 41 56 41 57 48 83 EC 20 89 D0 45 31 ED 45 0F B6 E0 4C 8D 3C C5 00 00 00 00 48 8B 41 58"));
  // A model group's delayed job gathering the mesh of one entity (see hMeshGather)
  m_Signatures.emplace("OFFSET_MESHGATHER",          Signature("48 89 E0 48 89 58 18 48 89 70 20 55 57 41 56 48 8D A8 58 FF FF FF 48 81 EC 90 01 00 00 0F 29 70 D8 0F 29 78 C8 44 0F 29 40 B8 44 0F 29 48 A8 44 0F 29 50 98 48 8B 05 ?? ?? ?? ?? 48 31 E0 48 89 45 E0 44 0F B7 4A 08 0F B7 05 ?? ?? ?? ??"));
  // The sound listener's setters (vtable 0x141ACFA78 slots 0 and 2, nothing
  // else uses them): its transform, 4 rows copied to +0x10, and its velocity
  // (+0x50). The game sets them once a frame from its camera (0x1432C1350).
  m_Signatures.emplace("OFFSET_LISTENERTRANSFORM",   Signature("0F 28 51 40 0F 28 41 30 0F 28 49 20 49 89 C8 0F C2 42 20 00 0F C2 4A 10 00 0F C2 52 30 00 0F 54 D0 0F 28 41 10 31 C9 0F C2 02 00"));
  m_Signatures.emplace("OFFSET_LISTENERVELOCITY",    Signature("48 83 EC 18 0F 28 41 50 31 C0 0F 5C 02 83 61 60 FD 0F 59 C0 66 0F 70 C8 00 66 0F 70 D0 55 66 0F 70 C0 AA"));
  // What that listener update multiplies the camera's transform by when it's
  // set (the listener is then in another space than the world's)
  m_Signatures.emplace("OFFSET_LISTENERSPACE",       Signature("48 8B 0D [ ?? ?? ?? ?? ] 48 85 C9 0F 84 ?? ?? ?? ?? 48 8B 01 48 8D 54 24 70 FF 50 40"));

  m_UseScannedResults = true;

  MODULEINFO info;
  if (!GetModuleInformation(GetCurrentProcess(), g_gameHandle, &info, sizeof(MODULEINFO)))
  {
    util::log::Error("GetModuleInformation failed, GetLastError 0x%X", GetLastError());
    util::log::Error("Offset scanning unavailable");
    return;
  }

  // The Steam / EA app exe is wrapped by EA's DRM: its code is encrypted on
  // disk and only decrypted while the game starts. When injected early, wait
  // until a known piece of code shows up before scanning for the rest.
  util::log::Write("Waiting for the game code to be decrypted...");
  Signature& probe = m_Signatures.at("OFFSET_CAMERAUPDATE");
  ULONGLONG waitStart = GetTickCount64();
  bool codeReady = false;
  while (!g_shutdown && GetTickCount64() - waitStart < 5 * 60 * 1000)
  {
    if (ResolveSignature("OFFSET_CAMERAUPDATE", probe))
    {
      codeReady = true;
      break;
    }
    // Often: the renderer's instance memory is set aside about a second after
    // the code is decrypted, and it's made bigger before that (EnlargeRenderArena)
    Sleep(50);
  }

  if (!codeReady)
  {
    util::log::Error("The game code never became readable, no game hooks will be installed");
    return;
  }
  util::log::Ok("Game code found after %.1f seconds", (GetTickCount64() - waitStart) / 1000.0);

  DWORD timeStamp = GetGameTimeStamp();
  m_UseHardcodedFallback = (timeStamp == g_supportedTimeStamp);
  util::log::Write("Game build timestamp 0x%X (%s)", timeStamp,
    m_UseHardcodedFallback ? "same build as the original tools" : "unknown build, no hardcoded fallback");

  // First, before the game sets its renderer's instance memory aside
  if (m_UseHardcodedFallback)
    EnlargeRenderArena();
  if (m_UseHardcodedFallback)
    EnlargeViewRecords();
  if (m_UseHardcodedFallback)
    WidenBatchChunkIndex();
  if (m_UseHardcodedFallback)
    EnlargeRendererInstances();
  // Before the game makes its mesh manager
  if (m_UseHardcodedFallback)
    util::hooks::EnlargeMeshTable();
  // Before the game makes any zone streamer
  if (m_UseHardcodedFallback)
    util::hooks::EnlargeZoneSlots();
  // Before the game makes the renderer's instance record pool
  if (m_UseHardcodedFallback)
    util::hooks::EnlargeRecordPool();

  util::log::Write("Scanning for offsets...");

  // Retry for a few seconds in case some code is decrypted a little later
  bool allFound = false;
  for (int attempt = 0; attempt < 20 && !allFound && !g_shutdown; ++attempt)
  {
    if (attempt > 0)
      Sleep(250);

    allFound = true;
    for (auto& entry : m_Signatures)
    {
      if (entry.second.Result) continue;
      if (!ResolveSignature(entry.first, entry.second))
        allFound = false;
    }
  }

  if (allFound)
  {
    util::log::Ok("All offsets found");
    return;
  }

  for (auto& entry : m_Signatures)
  {
    if (entry.second.Result) continue;
    if (m_UseHardcodedFallback && m_HardcodedOffsets.count(entry.first))
      util::log::Warning("Could not find pattern for %s, using the hardcoded offset", entry.first.c_str());
    else
      util::log::Error("Could not find pattern for %s", entry.first.c_str());
  }
}

__int64 util::offsets::GetOffset(std::string const& name)
{
  // If a scan was done, prefer those results.
  // If something couldn't be found or there was no scan,
  // use the hardcoded offsets.

  if (m_UseScannedResults)
  {
    auto result = m_Signatures.find(name);
    if (result != m_Signatures.end())
    {
      if (result->second.Result)
        return result->second.Result;
    }
  }

  // If the offsets were scanned, their absolute position is known.
  // With hardcoded offsets, use relative offset because it's not
  // 100% guaranteed the game module will load at the same address space.

  auto hardcodedResult = m_HardcodedOffsets.find(name);
  if (m_UseHardcodedFallback && hardcodedResult != m_HardcodedOffsets.end())
    return hardcodedResult->second + (__int64)g_gameHandle;

  util::log::Error("Offset %s does not exist", name.c_str());
  return 0;
}
bool util::offsets::BatchChunkIndexWidened()
{
  return g_BatchChunkIndexWidened;
}
