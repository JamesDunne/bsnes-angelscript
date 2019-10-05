#pragma once

#include <nall/platform.hpp>
#include <nall/adaptive-array.hpp>
#include <nall/any.hpp>
#include <nall/chrono.hpp>
#include <nall/dl.hpp>
#include <nall/endian.hpp>
#include <nall/image.hpp>
#include <nall/literals.hpp>
#include <nall/random.hpp>
#include <nall/serializer.hpp>
#include <nall/shared-pointer.hpp>
#include <nall/string.hpp>
#include <nall/traits.hpp>
#include <nall/unique-pointer.hpp>
#include <nall/vector.hpp>
#include <nall/vfs.hpp>
#include <nall/hash/crc32.hpp>
#include <nall/hash/sha256.hpp>
using namespace nall;

#include <libco/libco.h>

#ifdef __SSE2__
  #include <emmintrin.h>
#endif

#ifdef __AVX2__
  #include <immintrin.h>
#endif

#include <emulator/types.hpp>
#include <emulator/memory/readable.hpp>
#include <emulator/memory/writable.hpp>
#include <emulator/audio/audio.hpp>

// [jsd] add support for AngelScript
#include <angelscript.h>
#include <scriptarray.h>

namespace Emulator {
  static const string Name    = "bsnes-angelscript";
  static const string Version = "110.5";
  static const string Author  = "byuu";
  static const string License = "GPLv3";
  static const string Website = "https://byuu.org";

  //incremented only when serialization format changes
  static const string SerializerVersion = "110";

  namespace Constants {
    namespace Colorburst {
      static constexpr double NTSC = 315.0 / 88.0 * 1'000'000.0;
      static constexpr double PAL  = 283.75 * 15'625.0 + 25.0;
    }
  }

  //nall/vfs shorthand constants for open(), load()
  namespace File {
    static const auto Read  = vfs::file::mode::read;
    static const auto Write = vfs::file::mode::write;
    static const auto Optional = false;
    static const auto Required = true;
  };
}

#include "platform.hpp"
#include "interface.hpp"
#include "game.hpp"
