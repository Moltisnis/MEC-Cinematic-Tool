"""Packs a resource the DLL embeds compressed (build.ps1 runs it for Resources\GroundProps.bin).

  python tools/pack_resource.py pack   <raw file> <packed file> [--algorithm lzms|xpress_huff|mszip|xpress]  (default lzms)
  python tools/pack_resource.py check  <packed file> [<raw file>]
  python tools/pack_resource.py unpack <packed file> <raw file>

The packed file: a 56-byte header, then the stream of Windows' own Compression API (cabinet.dll, buffer mode, the
same calls the DLL's util::GetPackedResource makes to undo it):
  0  "CTPK"
  4  u32 header version (1)
  8  u32 algorithm (COMPRESS_ALGORITHM_*: 2 MSZIP, 3 XPRESS, 4 XPRESS_HUFF, 5 LZMS)
  12 u32 0
  16 u64 the raw size
  24 the raw data's SHA-256 (32 bytes); the stream starts at 56
Only the Python standard library and Windows 8 or later. Packing checks the round trip before it writes.
"""
import ctypes, ctypes.wintypes as wt, hashlib, os, struct, sys

ALGORITHMS = {"mszip": 2, "xpress": 3, "xpress_huff": 4, "lzms": 5}
HEADER = struct.Struct("<4sIIIQ32s")      # 56 bytes
MAGIC, VERSION = b"CTPK", 1

_cab = ctypes.WinDLL("cabinet", use_last_error=True)
_H = ctypes.c_void_p
for _f in (_cab.CreateCompressor, _cab.CreateDecompressor):
    _f.argtypes = [wt.DWORD, ctypes.c_void_p, ctypes.POINTER(_H)]
    _f.restype = wt.BOOL
for _f in (_cab.Compress, _cab.Decompress):
    _f.argtypes = [_H, ctypes.c_void_p, ctypes.c_size_t, ctypes.c_void_p, ctypes.c_size_t,
                   ctypes.POINTER(ctypes.c_size_t)]
    _f.restype = wt.BOOL
_cab.CloseCompressor.argtypes = [_H]
_cab.CloseDecompressor.argtypes = [_H]


def compress(algorithm, data):
    h = _H()
    if not _cab.CreateCompressor(algorithm, None, ctypes.byref(h)):
        raise OSError("CreateCompressor(%d) failed: %d" % (algorithm, ctypes.get_last_error()))
    try:
        cap = len(data) + len(data) // 8 + (1 << 20)
        out = ctypes.create_string_buffer(cap)
        got = ctypes.c_size_t(0)
        if not _cab.Compress(h, data, len(data), out, cap, ctypes.byref(got)):
            raise OSError("Compress failed: %d" % ctypes.get_last_error())
        return out.raw[:got.value]
    finally:
        _cab.CloseCompressor(h)


def decompress(algorithm, stream):
    h = _H()
    if not _cab.CreateDecompressor(algorithm, None, ctypes.byref(h)):
        raise OSError("CreateDecompressor(%d) failed: %d" % (algorithm, ctypes.get_last_error()))
    try:
        size = ctypes.c_size_t(0)
        # A call with no buffer fails with ERROR_INSUFFICIENT_BUFFER (122) and gives the raw size
        if _cab.Decompress(h, stream, len(stream), None, 0, ctypes.byref(size)) or ctypes.get_last_error() != 122:
            raise OSError("the stream's size query didn't answer as expected")
        out = ctypes.create_string_buffer(size.value)
        got = ctypes.c_size_t(0)
        if not _cab.Decompress(h, stream, len(stream), out, size.value, ctypes.byref(got)) or got.value != size.value:
            raise OSError("Decompress failed: %d" % ctypes.get_last_error())
        return out.raw[:got.value]
    finally:
        _cab.CloseDecompressor(h)


def read_packed(path):
    blob = open(path, "rb").read()
    magic, version, algorithm, zero, size, sha = HEADER.unpack_from(blob)
    if magic != MAGIC or version != VERSION or algorithm not in ALGORITHMS.values() or zero:
        raise ValueError("%s: not a CTPK v1 file" % path)
    raw = decompress(algorithm, blob[HEADER.size:])
    if len(raw) != size or hashlib.sha256(raw).digest() != sha:
        raise ValueError("%s: the unpacked data isn't what the header says" % path)
    return raw, algorithm


def main(argv):
    if len(argv) >= 3 and argv[0] == "pack":
        algorithm = ALGORITHMS[argv[argv.index("--algorithm") + 1]] if "--algorithm" in argv else ALGORITHMS["lzms"]
        raw = open(argv[1], "rb").read()
        sha = hashlib.sha256(raw).digest()
        stream = compress(algorithm, raw)
        if decompress(algorithm, stream) != raw:
            raise SystemExit("the round trip isn't byte for byte: nothing written")
        tmp = argv[2] + ".tmp"
        with open(tmp, "wb") as f:
            f.write(HEADER.pack(MAGIC, VERSION, algorithm, 0, len(raw), sha))
            f.write(stream)
        os.replace(tmp, argv[2])
        print("%s: %d bytes -> %d (%.1f %%), sha256 %s" % (argv[2], len(raw), HEADER.size + len(stream),
                                                          100.0 * (HEADER.size + len(stream)) / len(raw), sha.hex()))
    elif len(argv) >= 2 and argv[0] == "check":
        raw, algorithm = read_packed(argv[1])
        if len(argv) >= 3 and open(argv[2], "rb").read() != raw:
            raise SystemExit("%s doesn't hold %s" % (argv[1], argv[2]))
        print("%s: OK, %d bytes, algorithm %d, sha256 %s" % (argv[1], len(raw), algorithm,
                                                             hashlib.sha256(raw).hexdigest()))
    elif len(argv) >= 3 and argv[0] == "unpack":
        raw, _ = read_packed(argv[1])
        open(argv[2], "wb").write(raw)
    else:
        raise SystemExit(__doc__)


if __name__ == "__main__":
    main(sys.argv[1:])
