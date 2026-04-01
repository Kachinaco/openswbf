#pragma once

#include "core/types.h"

namespace swbf {

// ---------------------------------------------------------------------------
// Known UCFB chunk FourCC identifiers
//
// Every chunk in a .lvl file is tagged with a 4-byte ASCII identifier.
// This header collects the ones we know about so call-sites can compare
// against named constants rather than raw integers.
// ---------------------------------------------------------------------------

namespace chunk_id {

// Container / root
constexpr FourCC ucfb = make_fourcc('u', 'c', 'f', 'b');
constexpr FourCC lvl_ = make_fourcc('l', 'v', 'l', '_');

// Textures
constexpr FourCC tex_ = make_fourcc('t', 'e', 'x', '_');

// Models & skeletons
constexpr FourCC modl = make_fourcc('m', 'o', 'd', 'l');
constexpr FourCC skel = make_fourcc('s', 'k', 'e', 'l');
constexpr FourCC coll = make_fourcc('c', 'o', 'l', 'l');

// Entity / object classes
constexpr FourCC entc = make_fourcc('e', 'n', 't', 'c');
constexpr FourCC ordc = make_fourcc('o', 'r', 'd', 'c');
constexpr FourCC wpnc = make_fourcc('w', 'p', 'n', 'c');
constexpr FourCC expc = make_fourcc('e', 'x', 'p', 'c');

// World / terrain
constexpr FourCC wrld = make_fourcc('w', 'r', 'l', 'd');
constexpr FourCC tern = make_fourcc('t', 'e', 'r', 'n');

// AI planning
constexpr FourCC plan = make_fourcc('p', 'l', 'a', 'n');
constexpr FourCC PATH = make_fourcc('P', 'A', 'T', 'H');

// Scripts
constexpr FourCC scr_ = make_fourcc('s', 'c', 'r', '_');
constexpr FourCC Locl = make_fourcc('L', 'o', 'c', 'l');

// Animation (top-level munged chunks in .lvl)
constexpr FourCC zaa_ = make_fourcc('z', 'a', 'a', '_');
constexpr FourCC zaf_ = make_fourcc('z', 'a', 'f', '_');

// Animation sub-chunks (ANM2 hierarchy inside .msh / munged .lvl)
constexpr FourCC ANM2 = make_fourcc('A', 'N', 'M', '2');
constexpr FourCC CYCL = make_fourcc('C', 'Y', 'C', 'L');
constexpr FourCC KFR3 = make_fourcc('K', 'F', 'R', '3');
constexpr FourCC SKL2 = make_fourcc('S', 'K', 'L', '2');
constexpr FourCC BLN2 = make_fourcc('B', 'L', 'N', '2');
constexpr FourCC ENVL = make_fourcc('E', 'N', 'V', 'L');
constexpr FourCC FRAM = make_fourcc('F', 'R', 'A', 'M');

// Effects / sky / lighting
constexpr FourCC fx__ = make_fourcc('f', 'x', '_', '_');
constexpr FourCC sky_ = make_fourcc('s', 'k', 'y', '_');
constexpr FourCC lght = make_fourcc('l', 'g', 'h', 't');

// Audio
constexpr FourCC snd_ = make_fourcc('s', 'n', 'd', '_');
constexpr FourCC mus_ = make_fourcc('m', 'u', 's', '_');

// UI
constexpr FourCC hud_ = make_fourcc('h', 'u', 'd', '_');
constexpr FourCC font = make_fourcc('f', 'o', 'n', 't');

// Shaders
constexpr FourCC SHDR = make_fourcc('S', 'H', 'D', 'R');

// World instance sub-chunks
constexpr FourCC inst = make_fourcc('i', 'n', 's', 't');

// Skeleton sub-chunks
constexpr FourCC BONE = make_fourcc('B', 'O', 'N', 'E');
constexpr FourCC XFRM = make_fourcc('X', 'F', 'R', 'M');

// Entity class sub-chunks
constexpr FourCC BASE = make_fourcc('B', 'A', 'S', 'E');
constexpr FourCC TYPE = make_fourcc('T', 'Y', 'P', 'E');
constexpr FourCC PROP = make_fourcc('P', 'R', 'O', 'P');
constexpr FourCC VALU = make_fourcc('V', 'A', 'L', 'U');

// PATH / shared sub-chunks
constexpr FourCC INFO = make_fourcc('I', 'N', 'F', 'O');
constexpr FourCC NODE = make_fourcc('N', 'O', 'D', 'E');

// ---------------------------------------------------------------------------
// MSH / MODL sub-chunks (model geometry, materials, skeleton data inside .lvl)
// ---------------------------------------------------------------------------

// Top-level mesh container
constexpr FourCC HEDR = make_fourcc('H', 'E', 'D', 'R');
constexpr FourCC MSH2 = make_fourcc('M', 'S', 'H', '2');

// Model info
constexpr FourCC SINF = make_fourcc('S', 'I', 'N', 'F');
constexpr FourCC NAME = make_fourcc('N', 'A', 'M', 'E');
constexpr FourCC BBOX = make_fourcc('B', 'B', 'O', 'X');

// Materials
constexpr FourCC MATL = make_fourcc('M', 'A', 'T', 'L');
constexpr FourCC MATD = make_fourcc('M', 'A', 'T', 'D');
constexpr FourCC ATRB = make_fourcc('A', 'T', 'R', 'B');
constexpr FourCC TX0D = make_fourcc('T', 'X', '0', 'D');
constexpr FourCC TX1D = make_fourcc('T', 'X', '1', 'D');
constexpr FourCC TX2D = make_fourcc('T', 'X', '2', 'D');
constexpr FourCC TX3D = make_fourcc('T', 'X', '3', 'D');
constexpr FourCC DATA = make_fourcc('D', 'A', 'T', 'A');

// Geometry hierarchy
constexpr FourCC MODL_sub = make_fourcc('M', 'O', 'D', 'L');  // MODL inside MSH2 (sub-model)
constexpr FourCC MTYP = make_fourcc('M', 'T', 'Y', 'P');
constexpr FourCC MNDX = make_fourcc('M', 'N', 'D', 'X');
constexpr FourCC PRNT = make_fourcc('P', 'R', 'N', 'T');
constexpr FourCC GEOM = make_fourcc('G', 'E', 'O', 'M');
constexpr FourCC SEGM = make_fourcc('S', 'E', 'G', 'M');
constexpr FourCC SHDW = make_fourcc('S', 'H', 'D', 'W');
constexpr FourCC TRAN = make_fourcc('T', 'R', 'A', 'N');
constexpr FourCC FLGS = make_fourcc('F', 'L', 'G', 'S');

// Segment data
constexpr FourCC MTRL = make_fourcc('M', 'T', 'R', 'L');  // material index in segment
constexpr FourCC POSL = make_fourcc('P', 'O', 'S', 'L');  // positions
constexpr FourCC NRML = make_fourcc('N', 'R', 'M', 'L');  // normals
constexpr FourCC UV0L = make_fourcc('U', 'V', '0', 'L');  // UV set 0
constexpr FourCC UV1L = make_fourcc('U', 'V', '1', 'L');  // UV set 1
constexpr FourCC CLRL = make_fourcc('C', 'L', 'R', 'L');  // vertex colors (RGBA packed u32)
constexpr FourCC CLRB = make_fourcc('C', 'L', 'R', 'B');  // vertex colors (packed u32, alt format)
constexpr FourCC WGHT = make_fourcc('W', 'G', 'H', 'T');  // bone weights
constexpr FourCC STRP = make_fourcc('S', 'T', 'R', 'P');  // triangle strips
constexpr FourCC NDXL = make_fourcc('N', 'D', 'X', 'L');  // index list (optional)
constexpr FourCC NDXT = make_fourcc('N', 'D', 'X', 'T');  // triangle index list

// ---------------------------------------------------------------------------
// Munged model format sub-chunks (soldier/vehicle models in side .lvl files)
//
// These use lowercase tags for container chunks (segm, shdw) and uppercase
// for data chunks (IBUF, VBUF, TNAM, etc.).
// ---------------------------------------------------------------------------

constexpr FourCC segm = make_fourcc('s', 'e', 'g', 'm');  // geometry segment (munged)
constexpr FourCC shdw = make_fourcc('s', 'h', 'd', 'w');  // shadow mesh (munged)
constexpr FourCC IBUF = make_fourcc('I', 'B', 'U', 'F');  // index buffer
constexpr FourCC VBUF = make_fourcc('V', 'B', 'U', 'F');  // vertex buffer
constexpr FourCC TNAM = make_fourcc('T', 'N', 'A', 'M');  // texture name
constexpr FourCC MNAM = make_fourcc('M', 'N', 'A', 'M');  // material name
constexpr FourCC RTYP = make_fourcc('R', 'T', 'Y', 'P');  // render type
constexpr FourCC SPHR = make_fourcc('S', 'P', 'H', 'R');  // bounding sphere
constexpr FourCC SKIN = make_fourcc('S', 'K', 'I', 'N');  // bone weights (munged)
constexpr FourCC BMAP = make_fourcc('B', 'M', 'A', 'P');  // bone mapping
constexpr FourCC VDAT = make_fourcc('V', 'D', 'A', 'T');  // extra vertex data

} // namespace chunk_id

} // namespace swbf
