
#include <sfc/sfc.hpp>

#if !defined(PLATFORM_WINDOWS)
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <sys/ioctl.h>
  #include <netinet/in.h>
  #include <netinet/tcp.h>
  #include <netdb.h>
  #define SEND_BUF_CAST(t) ((const void *)(t))
  #define RECV_BUF_CAST(t) ((void *)(t))

int sock_capture_error() {
  return errno;
}

char* sock_error_string(int err) {
  return strerror(err);
}

bool sock_has_error(int err) {
  return err != 0;
}
#else
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #define SEND_BUF_CAST(t) ((const char *)(t))
  #define RECV_BUF_CAST(t) ((char *)(t))

/* gcc doesn't know _Thread_local from C11 yet */
#ifdef __GNUC__
# define thread_local __thread
#elif __STDC_VERSION__ >= 201112L
# define thread_local _Thread_local
#elif defined(_MSC_VER)
# define thread_local __declspec( thread )
#else
# error Cannot define thread_local
#endif

thread_local char errmsg[256];

char* sock_error_string(int errcode) {
  DWORD len = FormatMessageA(FORMAT_MESSAGE_ARGUMENT_ARRAY | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, errcode, 0, errmsg, 255, NULL);
  if (len != 0)
    errmsg[len] = 0;
  else
    sprintf(errmsg, "error %d", errcode);
  return errmsg;
}

int sock_capture_error() {
  return WSAGetLastError();
}

bool sock_has_error(int err) {
  return FAILED(err);
}
#endif

#define S1(x) #x
#define S2(x) S1(x)
#define LOCATION __FILE__ ":" S2(__LINE__)

#include "vga-charset.cpp"
#include "script-string.cpp"

// R5G5B5 is what ends up on the final PPU frame buffer (R and B are swapped from SNES)
typedef uint16 r5g5b5;
// B5G5R5 is used by the SNES system
typedef uint16 b5g5r5;

namespace ScriptInterface {
  struct ExceptionHandler {
    void exceptionCallback(asIScriptContext *ctx) {
      asIScriptEngine *engine = ctx->GetEngine();

      // Determine the exception that occurred
      const asIScriptFunction *function = ctx->GetExceptionFunction();
      platform->scriptMessage(
        string("EXCEPTION `{0}` occurred in `{1}` (line {2})").format({
          ctx->GetExceptionString(),
          function->GetDeclaration(),
          ctx->GetExceptionLineNumber()
        }),
        true
      );

      // Determine the function where the exception occurred
      //printf("func: %s\n", function->GetDeclaration());
      //printf("modl: %s\n", function->GetModuleName());
      //printf("sect: %s\n", function->GetScriptSectionName());
      // Determine the line number where the exception occurred
      //printf("line: %d\n", ctx->GetExceptionLineNumber());
    }
  } exceptionHandler;

  static auto message(const string *msg) -> void {
    platform->scriptMessage(*msg);
  }

  static auto array_to_string(CScriptArray *array, uint offs, uint size) -> string* {
    // avoid index out of range exception:
    if (array->GetSize() <= offs) return new string();
    if (array->GetSize() < offs + size) return new string();

    // append bytes from array:
    auto appended = new string();
    appended->resize(size);
    for (auto i : range(size)) {
      appended->get()[i] = *(const char *)(array->At(offs + i));
    }
    return appended;
  }

  static void uint8_array_append_uint16(CScriptArray *array, const uint16 *value) {
    if (array->GetElementTypeId() != asTYPEID_UINT8) {
      array->InsertLast((void *)value);
      return;
    }

    array->InsertLast((void *)&((const uint8 *)value)[0]);
    array->InsertLast((void *)&((const uint8 *)value)[1]);
  }

  static void uint8_array_append_uint32(CScriptArray *array, const uint32 *value) {
    if (array->GetElementTypeId() != asTYPEID_UINT8) {
      array->InsertLast((void *)value);
      return;
    }

    array->InsertLast((void *)&((const uint8 *)value)[0]);
    array->InsertLast((void *)&((const uint8 *)value)[1]);
    array->InsertLast((void *)&((const uint8 *)value)[2]);
    array->InsertLast((void *)&((const uint8 *)value)[3]);
  }

  static void uint8_array_append_string(CScriptArray *array, string *other) {
    if (array->GetElementTypeId() != asTYPEID_UINT8) {
      return;
    }
    if (other == nullptr) {
      return;
    }

    for (auto& c : *other) {
      array->InsertLast((void *)&c);
    }
  }

  static void uint8_array_append_array(CScriptArray *array, CScriptArray *other) {
    if (array->GetElementTypeId() != asTYPEID_UINT8) {
      array->InsertAt(array->GetSize(), *other);
      return;
    }

    if (other->GetElementTypeId() == asTYPEID_UINT8) {
      array->InsertAt(array->GetSize(), *other);
      return;
    } else if (other->GetElementTypeId() == asTYPEID_UINT16) {
      for (uint i = 0; i < other->GetSize(); i++) {
        auto value = (const uint16 *) other->At(i);
        array->InsertLast((void *) &((const uint8 *) value)[0]);
        array->InsertLast((void *) &((const uint8 *) value)[1]);
      }
    } else if (other->GetElementTypeId() == asTYPEID_UINT32) {
      for (uint i = 0; i < other->GetSize(); i++) {
        auto value = (const uint32 *) other->At(i);
        array->InsertLast((void *) &((const uint8 *) value)[0]);
        array->InsertLast((void *) &((const uint8 *) value)[1]);
        array->InsertLast((void *) &((const uint8 *) value)[2]);
        array->InsertLast((void *) &((const uint8 *) value)[3]);
      }
    }
  }

  #include "script-bus.cpp"
  #include "script-ppu.cpp"
  #include "script-frame.cpp"
  #include "script-extra.cpp"
  #include "script-net.cpp"
  #include "script-gui.cpp"

};

template<class A, class B>
B* refCast(A* a) {
  if (a == nullptr) {
    printf("refCast A is null!\n");
    return nullptr;
  }

  B* b = dynamic_cast<B*>(a);
  if (b != nullptr) {
    //printf("refCast A to B succeeded %p -> %p\n", a, b);
    b->ref_add();
  } else {
    printf("refCast to B failed!\n");
  }

  return b;
}

template< class T >
auto RegisterSizableMembers(asIScriptEngine *e, const char *type) -> void {
  int r;

  // casts back and forth:
  r = e->RegisterObjectMethod("Sizable", (std::string(type) + "@ opCast()").c_str(), asFUNCTION((refCast<ScriptInterface::GUI::Sizable, T>)), asCALL_CDECL_OBJLAST); assert( r >= 0 );
  r = e->RegisterObjectMethod(type, "Sizable@ opImplCast()", asFUNCTION((refCast<T, ScriptInterface::GUI::Sizable>)), asCALL_CDECL_OBJLAST); assert( r >= 0 );
  r = e->RegisterObjectMethod("Sizable", (std::string("const ") + std::string(type) + "@ opCast() const").c_str(), asFUNCTION((refCast<ScriptInterface::GUI::Sizable, T>)), asCALL_CDECL_OBJLAST); assert( r >= 0 );
  r = e->RegisterObjectMethod(type, "const Sizable@ opImplCast() const", asFUNCTION((refCast<T, ScriptInterface::GUI::Sizable>)), asCALL_CDECL_OBJLAST); assert( r >= 0 );

  // Sizable members:
  r = e->RegisterObjectMethod(type, "void setPosition(float x, float y)", asMETHOD(T, setPosition), asCALL_THISCALL); assert( r >= 0 );
}

auto Interface::registerScriptDefs() -> void {
  int r;

  script.engine = platform->scriptEngine();

  // register global functions for the script to use:
  auto defaultNamespace = script.engine->GetDefaultNamespace();

  // register array type:
  RegisterScriptArray(script.engine, true /* bool defaultArrayType */);

  // register string type:
  registerScriptString();

  // additional array functions for serialization purposes:
  r = script.engine->RegisterObjectMethod("array<T>", "void insertLast(const uint16 &in value)", asFUNCTION(ScriptInterface::uint8_array_append_uint16), asCALL_CDECL_OBJFIRST); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("array<T>", "void insertLast(const uint32 &in value)", asFUNCTION(ScriptInterface::uint8_array_append_uint32), asCALL_CDECL_OBJFIRST); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("array<T>", "void insertLast(const string &in other)", asFUNCTION(ScriptInterface::uint8_array_append_string), asCALL_CDECL_OBJFIRST); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("array<T>", "void insertLast(const ? &in other)", asFUNCTION(ScriptInterface::uint8_array_append_array), asCALL_CDECL_OBJFIRST); assert(r >= 0);

  r = script.engine->RegisterObjectMethod("array<T>", "string &toString(uint offs, uint size) const", asFUNCTION(ScriptInterface::array_to_string), asCALL_CDECL_OBJFIRST); assert(r >= 0);

  // global function to write debug messages:
  r = script.engine->RegisterGlobalFunction("void message(const string &in msg)", asFUNCTION(ScriptInterface::message), asCALL_CDECL); assert(r >= 0);

  // default bus memory functions:
  {
    r = script.engine->SetDefaultNamespace("bus"); assert(r >= 0);

    // read functions:
    r = script.engine->RegisterGlobalFunction("uint8 read_u8(uint32 addr)",  asFUNCTION(ScriptInterface::Bus::read_u8), asCALL_CDECL); assert(r >= 0);
    r = script.engine->RegisterGlobalFunction("uint16 read_u16(uint32 addr0, uint32 addr1)", asFUNCTION(ScriptInterface::Bus::read_u16), asCALL_CDECL); assert(r >= 0);
    r = script.engine->RegisterGlobalFunction("void read_block_u8(uint32 addr, uint offs, uint16 size, const array<uint8> &in output)",  asFUNCTION(ScriptInterface::Bus::read_block_u8), asCALL_CDECL); assert(r >= 0);
    r = script.engine->RegisterGlobalFunction("void read_block_u16(uint32 addr, uint offs, uint16 size, const array<uint16> &in output)",  asFUNCTION(ScriptInterface::Bus::read_block_u16), asCALL_CDECL); assert(r >= 0);

    // write functions:
    r = script.engine->RegisterGlobalFunction("void write_u8(uint32 addr, uint8 data)", asFUNCTION(ScriptInterface::Bus::write_u8), asCALL_CDECL); assert(r >= 0);
    r = script.engine->RegisterGlobalFunction("void write_u16(uint32 addr0, uint32 addr1, uint16 data)", asFUNCTION(ScriptInterface::Bus::write_u16), asCALL_CDECL); assert(r >= 0);
    r = script.engine->RegisterGlobalFunction("void write_block_u8(uint32 addr, uint offs, uint16 size, const array<uint8> &in output)", asFUNCTION(ScriptInterface::Bus::write_block_u8), asCALL_CDECL); assert(r >= 0);
    r = script.engine->RegisterGlobalFunction("void write_block_u16(uint32 addr, uint offs, uint16 size, const array<uint16> &in output)", asFUNCTION(ScriptInterface::Bus::write_block_u16), asCALL_CDECL); assert(r >= 0);

    r = script.engine->RegisterFuncdef("void WriteInterceptCallback(uint32 addr, uint8 value)"); assert(r >= 0);
    r = script.engine->RegisterGlobalFunction("void add_write_interceptor(const string &in addr, uint32 size, WriteInterceptCallback @cb)", asFUNCTION(ScriptInterface::Bus::add_write_interceptor), asCALL_CDECL); assert(r >= 0);
  }

  {
    r = script.engine->SetDefaultNamespace("cpu"); assert(r >= 0);
    r = script.engine->RegisterObjectType    ("DMAIntercept", sizeof(CPU::DMAIntercept), asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = script.engine->RegisterObjectProperty("DMAIntercept", "uint8  channel", asOFFSET(CPU::DMAIntercept, channel)); assert(r >= 0);
    r = script.engine->RegisterObjectProperty("DMAIntercept", "uint8  transferMode", asOFFSET(CPU::DMAIntercept, transferMode)); assert(r >= 0);
    r = script.engine->RegisterObjectProperty("DMAIntercept", "uint8  fixedTransfer", asOFFSET(CPU::DMAIntercept, fixedTransfer)); assert(r >= 0);
    r = script.engine->RegisterObjectProperty("DMAIntercept", "uint8  reverseTransfer", asOFFSET(CPU::DMAIntercept, reverseTransfer)); assert(r >= 0);
    r = script.engine->RegisterObjectProperty("DMAIntercept", "uint8  unused", asOFFSET(CPU::DMAIntercept, unused)); assert(r >= 0);
    r = script.engine->RegisterObjectProperty("DMAIntercept", "uint8  indirect", asOFFSET(CPU::DMAIntercept, indirect)); assert(r >= 0);
    r = script.engine->RegisterObjectProperty("DMAIntercept", "uint8  direction", asOFFSET(CPU::DMAIntercept, direction)); assert(r >= 0);
    r = script.engine->RegisterObjectProperty("DMAIntercept", "uint8  targetAddress", asOFFSET(CPU::DMAIntercept, targetAddress)); assert(r >= 0);
    r = script.engine->RegisterObjectProperty("DMAIntercept", "uint16 sourceAddress", asOFFSET(CPU::DMAIntercept, sourceAddress)); assert(r >= 0);
    r = script.engine->RegisterObjectProperty("DMAIntercept", "uint8  sourceBank", asOFFSET(CPU::DMAIntercept, sourceBank)); assert(r >= 0);
    r = script.engine->RegisterObjectProperty("DMAIntercept", "uint16 transferSize", asOFFSET(CPU::DMAIntercept, transferSize)); assert(r >= 0);
    r = script.engine->RegisterObjectProperty("DMAIntercept", "uint16 indirectAddress", asOFFSET(CPU::DMAIntercept, indirectAddress)); assert(r >= 0);
    r = script.engine->RegisterObjectProperty("DMAIntercept", "uint8  indirectBank", asOFFSET(CPU::DMAIntercept, indirectBank)); assert(r >= 0);

    r = script.engine->RegisterFuncdef("void DMAInterceptCallback(DMAIntercept @dma)"); assert(r >= 0);
    r = script.engine->RegisterGlobalFunction("void register_dma_interceptor(DMAInterceptCallback @cb)", asFUNCTION(ScriptInterface::Bus::register_dma_interceptor), asCALL_CDECL); assert(r >= 0);
  }

  // create ppu namespace
  r = script.engine->SetDefaultNamespace("ppu"); assert(r >= 0);
  r = script.engine->RegisterGlobalFunction("uint16 rgb(uint8 r, uint8 g, uint8 b)", asFUNCTION(ScriptInterface::PPUAccess::ppu_rgb), asCALL_CDECL); assert(r >= 0);
  r = script.engine->RegisterGlobalFunction("uint8 get_luma()", asFUNCTION(ScriptInterface::PPUAccess::ppu_get_luma), asCALL_CDECL); assert(r >= 0);
  r = script.engine->RegisterGlobalFunction("uint8 sprite_width(uint8 baseSize, uint8 size)", asFUNCTION(ScriptInterface::PPUAccess::ppu_sprite_width), asCALL_CDECL); assert(r >= 0);
  r = script.engine->RegisterGlobalFunction("uint8 sprite_height(uint8 baseSize, uint8 size)", asFUNCTION(ScriptInterface::PPUAccess::ppu_sprite_height), asCALL_CDECL); assert(r >= 0);
  r = script.engine->RegisterGlobalFunction("uint8 sprite_base_size()", asFUNCTION(ScriptInterface::PPUAccess::ppu_sprite_base_size), asCALL_CDECL); assert(r >= 0);

  // define ppu::VRAM object type for opIndex convenience:
  r = script.engine->RegisterObjectType("VRAM", 0, asOBJ_REF | asOBJ_NOHANDLE); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("VRAM", "uint16 opIndex(uint16 addr)", asMETHOD(ScriptInterface::PPUAccess, vram_read), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("VRAM", "uint16 chr_address(uint16 chr)", asMETHOD(ScriptInterface::PPUAccess, vram_chr_address), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("VRAM", "void read_block(uint16 addr, uint offs, uint16 size, array<uint16> &inout output)", asMETHOD(ScriptInterface::PPUAccess, vram_read_block), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("VRAM", "void write_block(uint16 addr, uint offs, uint16 size, const array<uint16> &in data)", asMETHOD(ScriptInterface::PPUAccess, vram_write_block), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterGlobalProperty("VRAM vram", &ScriptInterface::ppuAccess); assert(r >= 0);

  // define ppu::CGRAM object type for opIndex convenience:
  r = script.engine->RegisterObjectType("CGRAM", 0, asOBJ_REF | asOBJ_NOHANDLE); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("CGRAM", "uint16 opIndex(uint16 addr)", asMETHOD(ScriptInterface::PPUAccess, cgram_read), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterGlobalProperty("CGRAM cgram", &ScriptInterface::ppuAccess); assert(r >= 0);

  r = script.engine->RegisterObjectType    ("OAMSprite", sizeof(ScriptInterface::PPUAccess::OAMObject), asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
  r = script.engine->RegisterObjectMethod  ("OAMSprite", "bool   get_is_enabled()", asMETHOD(ScriptInterface::PPUAccess::OAMObject, get_is_enabled), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterObjectProperty("OAMSprite", "uint16 x", asOFFSET(ScriptInterface::PPUAccess::OAMObject, x)); assert(r >= 0);
  r = script.engine->RegisterObjectProperty("OAMSprite", "uint8  y", asOFFSET(ScriptInterface::PPUAccess::OAMObject, y)); assert(r >= 0);
  r = script.engine->RegisterObjectProperty("OAMSprite", "uint16 character", asOFFSET(ScriptInterface::PPUAccess::OAMObject, character)); assert(r >= 0);
  r = script.engine->RegisterObjectProperty("OAMSprite", "bool   vflip", asOFFSET(ScriptInterface::PPUAccess::OAMObject, vflip)); assert(r >= 0);
  r = script.engine->RegisterObjectProperty("OAMSprite", "bool   hflip", asOFFSET(ScriptInterface::PPUAccess::OAMObject, hflip)); assert(r >= 0);
  r = script.engine->RegisterObjectProperty("OAMSprite", "uint8  priority", asOFFSET(ScriptInterface::PPUAccess::OAMObject, priority)); assert(r >= 0);
  r = script.engine->RegisterObjectProperty("OAMSprite", "uint8  palette", asOFFSET(ScriptInterface::PPUAccess::OAMObject, palette)); assert(r >= 0);
  r = script.engine->RegisterObjectProperty("OAMSprite", "uint8  size", asOFFSET(ScriptInterface::PPUAccess::OAMObject, size)); assert(r >= 0);
  r = script.engine->RegisterObjectMethod  ("OAMSprite", "uint8  get_width()", asMETHOD(ScriptInterface::PPUAccess::OAMObject, get_width), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterObjectMethod  ("OAMSprite", "uint8  get_height()", asMETHOD(ScriptInterface::PPUAccess::OAMObject, get_height), asCALL_THISCALL); assert(r >= 0);

  r = script.engine->RegisterObjectType  ("OAM", 0, asOBJ_REF | asOBJ_NOHANDLE); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("OAM", "OAMSprite @get_opIndex(uint8 chr)", asFUNCTION(ScriptInterface::PPUAccess::oam_get_object), asCALL_CDECL_OBJFIRST); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("OAM", "void set_opIndex(uint8 chr, OAMSprite @sprite)", asFUNCTION(ScriptInterface::PPUAccess::oam_set_object), asCALL_CDECL_OBJFIRST); assert(r >= 0);

  r = script.engine->RegisterGlobalProperty("OAM oam", &ScriptInterface::ppuAccess); assert(r >= 0);

  {
    // define ppu::Frame object type:
    r = script.engine->RegisterObjectType("Frame", 0, asOBJ_REF | asOBJ_NOHANDLE); assert(r >= 0);

    // define width and height properties:
    r = script.engine->RegisterObjectMethod("Frame", "uint get_width()", asMETHOD(ScriptInterface::PostFrame, get_width), asCALL_THISCALL); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("Frame", "uint get_height()", asMETHOD(ScriptInterface::PostFrame, get_height), asCALL_THISCALL); assert(r >= 0);

    // define x_scale and y_scale properties:
    r = script.engine->RegisterObjectMethod("Frame", "int get_x_scale()", asMETHOD(ScriptInterface::PostFrame, get_x_scale), asCALL_THISCALL); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("Frame", "void set_x_scale(int x_scale)", asMETHOD(ScriptInterface::PostFrame, set_x_scale), asCALL_THISCALL); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("Frame", "int get_y_scale()", asMETHOD(ScriptInterface::PostFrame, get_y_scale), asCALL_THISCALL); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("Frame", "void set_y_scale(int y_scale)", asMETHOD(ScriptInterface::PostFrame, set_y_scale), asCALL_THISCALL); assert(r >= 0);

    // adjust y_offset of drawing functions (default 8):
    r = script.engine->RegisterObjectProperty("Frame", "int y_offset", asOFFSET(ScriptInterface::PostFrame, y_offset)); assert(r >= 0);

    // set color to use for drawing functions (15-bit RGB):
    r = script.engine->RegisterObjectMethod("Frame", "uint16 get_color()", asMETHOD(ScriptInterface::PostFrame, get_color), asCALL_THISCALL); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("Frame", "void set_color(uint16 color)", asMETHOD(ScriptInterface::PostFrame, set_color), asCALL_THISCALL); assert(r >= 0);

    // set luma (paired with color) to use for drawing functions (0..15):
    r = script.engine->RegisterObjectMethod("Frame", "uint8 get_luma()", asMETHOD(ScriptInterface::PostFrame, get_luma), asCALL_THISCALL); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("Frame", "void set_luma(uint8 luma)", asMETHOD(ScriptInterface::PostFrame, set_luma), asCALL_THISCALL); assert(r >= 0);

    // set alpha to use for drawing functions (0..31):
    r = script.engine->RegisterObjectMethod("Frame", "uint8 get_alpha()", asMETHOD(ScriptInterface::PostFrame, get_alpha), asCALL_THISCALL); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("Frame", "void set_alpha(uint8 alpha)", asMETHOD(ScriptInterface::PostFrame, set_alpha), asCALL_THISCALL); assert(r >= 0);

    // set font_height to use for text (8 or 16):
    r = script.engine->RegisterObjectProperty("Frame", "int font_height", asOFFSET(ScriptInterface::PostFrame, font_height)); assert(r >= 0);

    // set text_shadow to draw shadow behind text:
    r = script.engine->RegisterObjectProperty("Frame", "bool text_shadow", asOFFSET(ScriptInterface::PostFrame, text_shadow)); assert(r >= 0);

    // register the DrawOp enum:
    r = script.engine->RegisterEnum("draw_op"); assert(r >= 0);
    r = script.engine->RegisterEnumValue("draw_op", "op_solid", ScriptInterface::PostFrame::draw_op_t::op_solid); assert(r >= 0);
    r = script.engine->RegisterEnumValue("draw_op", "op_alpha", ScriptInterface::PostFrame::draw_op_t::op_alpha); assert(r >= 0);
    r = script.engine->RegisterEnumValue("draw_op", "op_xor", ScriptInterface::PostFrame::draw_op_t::op_xor); assert(r >= 0);

    // set draw_op:
    r = script.engine->RegisterObjectProperty("Frame", "draw_op draw_op", asOFFSET(ScriptInterface::PostFrame, draw_op)); assert(r >= 0);

    // pixel access functions:
    r = script.engine->RegisterObjectMethod("Frame", "uint16 read_pixel(int x, int y)", asMETHOD(ScriptInterface::PostFrame, read_pixel), asCALL_THISCALL); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("Frame", "void pixel(int x, int y)", asMETHOD(ScriptInterface::PostFrame, pixel), asCALL_THISCALL); assert(r >= 0);

    // primitive drawing functions:
    r = script.engine->RegisterObjectMethod("Frame", "void hline(int lx, int ty, int w)", asMETHOD(ScriptInterface::PostFrame, hline), asCALL_THISCALL); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("Frame", "void vline(int lx, int ty, int h)", asMETHOD(ScriptInterface::PostFrame, vline), asCALL_THISCALL); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("Frame", "void rect(int x, int y, int w, int h)", asMETHOD(ScriptInterface::PostFrame, rect), asCALL_THISCALL); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("Frame", "void fill(int x, int y, int w, int h)", asMETHOD(ScriptInterface::PostFrame, fill), asCALL_THISCALL); assert(r >= 0);

    // text drawing function:
    r = script.engine->RegisterObjectMethod("Frame", "int text(int x, int y, const string &in text)", asMETHOD(ScriptInterface::PostFrame, text), asCALL_THISCALL); assert(r >= 0);

    // draw 4bpp paletted 8x1 row:
    r = script.engine->RegisterObjectMethod("Frame", "int draw_4bpp_8x8(int x, int y, const array<uint32> &in tiledata, const array<uint16> &in palette)", asMETHOD(ScriptInterface::PostFrame, draw_4bpp_8x8), asCALL_THISCALL); assert(r >= 0);

    // global property to access current frame:
    r = script.engine->RegisterGlobalProperty("Frame frame", &ScriptInterface::postFrame); assert(r >= 0);
  }

  {
    // extra-tiles access:
    r = script.engine->RegisterObjectType("Extra", 0, asOBJ_REF | asOBJ_NOHANDLE); assert(r >= 0);

    r = script.engine->RegisterObjectType("ExtraTile", sizeof(PPUfast::ExtraTile), asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);

    r = script.engine->RegisterObjectProperty("ExtraTile", "int x", asOFFSET(PPUfast::ExtraTile, x)); assert(r >= 0);
    r = script.engine->RegisterObjectProperty("ExtraTile", "int y", asOFFSET(PPUfast::ExtraTile, y)); assert(r >= 0);
    r = script.engine->RegisterObjectProperty("ExtraTile", "uint source", asOFFSET(PPUfast::ExtraTile, source)); assert(r >= 0);
    r = script.engine->RegisterObjectProperty("ExtraTile", "bool hflip", asOFFSET(PPUfast::ExtraTile, hflip)); assert(r >= 0);
    r = script.engine->RegisterObjectProperty("ExtraTile", "bool vflip", asOFFSET(PPUfast::ExtraTile, vflip)); assert(r >= 0);
    r = script.engine->RegisterObjectProperty("ExtraTile", "uint priority", asOFFSET(PPUfast::ExtraTile, priority)); assert(r >= 0);
    r = script.engine->RegisterObjectProperty("ExtraTile", "uint width", asOFFSET(PPUfast::ExtraTile, width)); assert(r >= 0);
    r = script.engine->RegisterObjectProperty("ExtraTile", "uint height", asOFFSET(PPUfast::ExtraTile, height)); assert(r >= 0);
    r = script.engine->RegisterObjectProperty("ExtraTile", "uint index", asOFFSET(PPUfast::ExtraTile, index)); assert(r >= 0);

    r = script.engine->RegisterObjectMethod("ExtraTile", "void reset()", asFUNCTION(ScriptInterface::ExtraLayer::tile_reset), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("ExtraTile", "void pixels_clear()", asFUNCTION(ScriptInterface::ExtraLayer::tile_pixels_clear), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = script.engine->RegisterObjectMethod("ExtraTile", "void pixel_set(int x, int y, uint16 color)", asFUNCTION(ScriptInterface::ExtraLayer::tile_pixel_set), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("ExtraTile", "void pixel_off(int x, int y)", asFUNCTION(ScriptInterface::ExtraLayer::tile_pixel_off), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("ExtraTile", "void pixel_on(int x, int y)", asFUNCTION(ScriptInterface::ExtraLayer::tile_pixel_on), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("ExtraTile", "void pixel(int x, int y)", asFUNCTION(ScriptInterface::ExtraLayer::tile_pixel_set), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("ExtraTile", "void draw_sprite(int x, int y, int width, int height, const array<uint32> &in tiledata, const array<uint16> &in palette)", asFUNCTION(ScriptInterface::ExtraLayer::tile_draw_sprite), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    // primitive drawing functions:
    r = script.engine->RegisterObjectMethod("ExtraTile", "void hline(int lx, int ty, int w)", asFUNCTION(ScriptInterface::ExtraLayer::tile_hline), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("ExtraTile", "void vline(int lx, int ty, int h)", asFUNCTION(ScriptInterface::ExtraLayer::tile_vline), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("ExtraTile", "void rect(int x, int y, int w, int h)", asFUNCTION(ScriptInterface::ExtraLayer::tile_rect), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("ExtraTile", "void fill(int x, int y, int w, int h)", asFUNCTION(ScriptInterface::ExtraLayer::tile_fill), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    // text drawing function:
    r = script.engine->RegisterObjectMethod("ExtraTile", "int text(int x, int y, const string &in text)", asFUNCTION(ScriptInterface::ExtraLayer::tile_text), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = script.engine->RegisterObjectMethod("Extra", "uint16 get_color()", asMETHOD(ScriptInterface::ExtraLayer, get_color), asCALL_THISCALL); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("Extra", "void set_color(uint16 color)", asMETHOD(ScriptInterface::ExtraLayer, set_color), asCALL_THISCALL); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("Extra", "bool get_text_shadow()", asMETHOD(ScriptInterface::ExtraLayer, get_text_shadow), asCALL_THISCALL); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("Extra", "void set_text_shadow(bool color)", asMETHOD(ScriptInterface::ExtraLayer, set_text_shadow), asCALL_THISCALL); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("Extra", "int measure_text(const string &in text)", asMETHOD(ScriptInterface::ExtraLayer, measure_text), asCALL_THISCALL); assert(r >= 0);

    r = script.engine->RegisterObjectMethod("Extra", "void reset()", asMETHOD(ScriptInterface::ExtraLayer, reset), asCALL_THISCALL); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("Extra", "uint get_count()", asMETHOD(ScriptInterface::ExtraLayer, get_tile_count), asCALL_THISCALL); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("Extra", "void set_count(uint count)", asMETHOD(ScriptInterface::ExtraLayer, set_tile_count), asCALL_THISCALL); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("Extra", "ExtraTile @get_opIndex(uint i)", asFUNCTION(ScriptInterface::ExtraLayer::get_tile), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = script.engine->RegisterGlobalProperty("Extra extra", &ScriptInterface::extraLayer); assert(r >= 0);
  }

  {
    r = script.engine->SetDefaultNamespace("net"); assert(r >= 0);

    // error reporting functions:
    r = script.engine->RegisterGlobalFunction("string get_is_error()", asFUNCTION(ScriptInterface::Net::get_is_error), asCALL_CDECL); assert( r >= 0 );
    r = script.engine->RegisterGlobalFunction("string get_error_code()", asFUNCTION(ScriptInterface::Net::get_error_code), asCALL_CDECL); assert( r >= 0 );
    r = script.engine->RegisterGlobalFunction("string get_error_text()", asFUNCTION(ScriptInterface::Net::get_error_text), asCALL_CDECL); assert( r >= 0 );
    r = script.engine->RegisterGlobalFunction("bool throw_if_error()",
                                              asFUNCTION(ScriptInterface::Net::exception_thrown), asCALL_CDECL); assert(r >= 0 );

    // Address type:
    r = script.engine->RegisterObjectType("Address", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = script.engine->RegisterGlobalFunction("Address@ resolve_tcp(const string &in host, const int port)", asFUNCTION(ScriptInterface::Net::resolve_tcp), asCALL_CDECL); assert(r >= 0);
    r = script.engine->RegisterGlobalFunction("Address@ resolve_udp(const string &in host, const int port)", asFUNCTION(ScriptInterface::Net::resolve_udp), asCALL_CDECL); assert(r >= 0);
    // TODO: try opImplCast
    r = script.engine->RegisterObjectMethod("Address", "bool get_is_valid()", asMETHOD(ScriptInterface::Net::Address, operator bool), asCALL_THISCALL); assert( r >= 0 );

    r = script.engine->RegisterObjectType("Socket", 0, asOBJ_REF); assert(r >= 0);
    r = script.engine->RegisterObjectBehaviour("Socket", asBEHAVE_FACTORY, "Socket@ f(Address @addr)", asFUNCTION(ScriptInterface::Net::create_socket), asCALL_CDECL); assert(r >= 0);
    r = script.engine->RegisterObjectBehaviour("Socket", asBEHAVE_ADDREF, "void f()", asMETHOD(ScriptInterface::Net::Socket, addRef), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectBehaviour("Socket", asBEHAVE_RELEASE, "void f()", asMETHOD(ScriptInterface::Net::Socket, release), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("Socket", "bool get_is_valid()", asMETHOD(ScriptInterface::Net::Socket, operator bool), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("Socket", "void connect(Address@ addr)", asMETHOD(ScriptInterface::Net::Socket, connect), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("Socket", "void bind(Address@ addr)", asMETHOD(ScriptInterface::Net::Socket, bind), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("Socket", "void listen(int backlog)", asMETHOD(ScriptInterface::Net::Socket, listen), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("Socket", "Socket@ accept()", asMETHOD(ScriptInterface::Net::Socket, accept), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("Socket", "int recv(int offs, int size, array<uint8> &inout buffer)", asMETHOD(ScriptInterface::Net::Socket, recv), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("Socket", "int send(int offs, int size, array<uint8> &inout buffer)", asMETHOD(ScriptInterface::Net::Socket, send), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("Socket", "int recvfrom(int offs, int size, array<uint8> &inout buffer)", asMETHOD(ScriptInterface::Net::Socket, recvfrom), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("Socket", "int sendto(int offs, int size, array<uint8> &inout buffer, const Address@ addr)", asMETHOD(ScriptInterface::Net::Socket, sendto), asCALL_THISCALL); assert( r >= 0 );

    r = script.engine->RegisterObjectType("WebSocketMessage", 0, asOBJ_REF); assert(r >= 0);
    r = script.engine->RegisterObjectBehaviour("WebSocketMessage", asBEHAVE_FACTORY, "WebSocketMessage@ f(uint8 opcode)", asFUNCTION(ScriptInterface::Net::create_web_socket_message), asCALL_CDECL); assert(r >= 0);
    r = script.engine->RegisterObjectBehaviour("WebSocketMessage", asBEHAVE_ADDREF, "void f()", asMETHOD(ScriptInterface::Net::WebSocketMessage, addRef), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectBehaviour("WebSocketMessage", asBEHAVE_RELEASE, "void f()", asMETHOD(ScriptInterface::Net::WebSocketMessage, release), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("WebSocketMessage", "uint8 get_opcode()", asMETHOD(ScriptInterface::Net::WebSocketMessage, get_opcode), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("WebSocketMessage", "void set_opcode(uint8 value)", asMETHOD(ScriptInterface::Net::WebSocketMessage, set_opcode), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("WebSocketMessage", "string &as_string()", asMETHOD(ScriptInterface::Net::WebSocketMessage, as_string), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("WebSocketMessage", "array<uint8> &as_array()", asMETHOD(ScriptInterface::Net::WebSocketMessage, as_array), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("WebSocketMessage", "void set_payload_as_string(string &in value)", asMETHOD(ScriptInterface::Net::WebSocketMessage, set_payload_as_string), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("WebSocketMessage", "void set_payload_as_array(array<uint8> &in value)", asMETHOD(ScriptInterface::Net::WebSocketMessage, set_payload_as_array), asCALL_THISCALL); assert( r >= 0 );

    r = script.engine->RegisterObjectType("WebSocket", 0, asOBJ_REF); assert(r >= 0);
    r = script.engine->RegisterObjectBehaviour("WebSocket", asBEHAVE_ADDREF, "void f()", asMETHOD(ScriptInterface::Net::WebSocket, addRef), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectBehaviour("WebSocket", asBEHAVE_RELEASE, "void f()", asMETHOD(ScriptInterface::Net::WebSocket, release), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("WebSocket", "WebSocketMessage@ process()", asMETHOD(ScriptInterface::Net::WebSocket, process), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("WebSocket", "void send(WebSocketMessage@ msg)", asMETHOD(ScriptInterface::Net::WebSocket, send), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("WebSocket", "bool get_is_valid()", asMETHOD(ScriptInterface::Net::WebSocket, operator bool), asCALL_THISCALL); assert( r >= 0 );

    r = script.engine->RegisterObjectType("WebSocketHandshaker", 0, asOBJ_REF); assert(r >= 0);
    r = script.engine->RegisterObjectBehaviour("WebSocketHandshaker", asBEHAVE_FACTORY, "WebSocketHandshaker@ f(Socket@ socket)", asFUNCTION(ScriptInterface::Net::create_web_socket_handshaker), asCALL_CDECL); assert(r >= 0);
    r = script.engine->RegisterObjectBehaviour("WebSocketHandshaker", asBEHAVE_ADDREF, "void f()", asMETHOD(ScriptInterface::Net::WebSocketHandshaker, addRef), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectBehaviour("WebSocketHandshaker", asBEHAVE_RELEASE, "void f()", asMETHOD(ScriptInterface::Net::WebSocketHandshaker, release), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("WebSocketHandshaker", "Socket@ get_socket()", asMETHOD(ScriptInterface::Net::WebSocketHandshaker, get_socket), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("WebSocketHandshaker", "WebSocket@ handshake()", asMETHOD(ScriptInterface::Net::WebSocketHandshaker, handshake), asCALL_THISCALL); assert( r >= 0 );    r = script.engine->RegisterObjectType("WebSocketHandshaker", 0, asOBJ_REF); assert(r >= 0);

    r = script.engine->RegisterObjectType("WebSocketServer", 0, asOBJ_REF); assert(r >= 0);
    r = script.engine->RegisterObjectBehaviour("WebSocketServer", asBEHAVE_FACTORY, "WebSocketServer@ f(string &in uri)", asFUNCTION(ScriptInterface::Net::create_web_socket_server), asCALL_CDECL); assert(r >= 0);
    r = script.engine->RegisterObjectBehaviour("WebSocketServer", asBEHAVE_ADDREF, "void f()", asMETHOD(ScriptInterface::Net::WebSocketServer, addRef), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectBehaviour("WebSocketServer", asBEHAVE_RELEASE, "void f()", asMETHOD(ScriptInterface::Net::WebSocketServer, release), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("WebSocketServer", "array<WebSocket@> &get_clients()", asMETHOD(ScriptInterface::Net::WebSocketServer, get_clients), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("WebSocketServer", "int process()", asMETHOD(ScriptInterface::Net::WebSocketServer, process), asCALL_THISCALL); assert( r >= 0 );
  }

  // UI
  {
    r = script.engine->SetDefaultNamespace("gui"); assert(r >= 0);

    // Register base types:
    r = script.engine->RegisterObjectType("Sizable", 0, asOBJ_REF); assert(r >= 0);
    r = script.engine->RegisterObjectBehaviour("Sizable", asBEHAVE_ADDREF, "void f()", asMETHOD(ScriptInterface::GUI::Sizable, ref_add), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectBehaviour("Sizable", asBEHAVE_RELEASE, "void f()", asMETHOD(ScriptInterface::GUI::Sizable, ref_release), asCALL_THISCALL); assert( r >= 0 );
    RegisterSizableMembers<ScriptInterface::GUI::Sizable>(script.engine, "Sizable");

    // Register types first:
    r = script.engine->RegisterObjectType("Window", 0, asOBJ_REF); assert(r >= 0);
    r = script.engine->RegisterObjectType("VerticalLayout", 0, asOBJ_REF); assert(r >= 0);
    r = script.engine->RegisterObjectType("HorizontalLayout", 0, asOBJ_REF); assert(r >= 0);
    r = script.engine->RegisterObjectType("LineEdit", 0, asOBJ_REF); assert(r >= 0);
    r = script.engine->RegisterObjectType("Label", 0, asOBJ_REF); assert(r >= 0);
    r = script.engine->RegisterObjectType("Button", 0, asOBJ_REF); assert(r >= 0);
    r = script.engine->RegisterObjectType("Canvas", 0, asOBJ_REF); assert(r >= 0);
    r = script.engine->RegisterObjectType("Size", sizeof(hiro::Size), asOBJ_VALUE); assert(r >= 0);

    // Size value type:
    r = script.engine->RegisterObjectBehaviour("Size", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(ScriptInterface::GUI::createSize), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = script.engine->RegisterObjectBehaviour("Size", asBEHAVE_CONSTRUCT, "void f(float width, float height)", asFUNCTION(ScriptInterface::GUI::createSizeWH), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = script.engine->RegisterObjectBehaviour("Size", asBEHAVE_DESTRUCT, "void f()", asFUNCTION(ScriptInterface::GUI::destroySize), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("Size", "float get_width()", asMETHOD(hiro::Size, width), asCALL_THISCALL); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("Size", "void set_width(float width)", asMETHOD(hiro::Size, setWidth), asCALL_THISCALL); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("Size", "float get_height()", asMETHOD(hiro::Size, height), asCALL_THISCALL); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("Size", "void set_height(float height)", asMETHOD(hiro::Size, setHeight), asCALL_THISCALL); assert(r >= 0);

    // Window
    r = script.engine->RegisterObjectBehaviour("Window", asBEHAVE_FACTORY, "Window@ f()", asFUNCTION(ScriptInterface::GUI::createWindow), asCALL_CDECL); assert(r >= 0);
    r = script.engine->RegisterObjectBehaviour("Window", asBEHAVE_FACTORY, "Window@ f(float rx, float ry, bool relative)", asFUNCTION(ScriptInterface::GUI::createWindowAtPosition), asCALL_CDECL); assert(r >= 0);
    r = script.engine->RegisterObjectBehaviour("Window", asBEHAVE_ADDREF, "void f()", asMETHOD(ScriptInterface::GUI::Window, ref_add), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectBehaviour("Window", asBEHAVE_RELEASE, "void f()", asMETHOD(ScriptInterface::GUI::Window, ref_release), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("Window", "void set_visible(bool visible)", asMETHOD(ScriptInterface::GUI::Window, setVisible), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("Window", "void set_title(const string &in title)", asMETHOD(ScriptInterface::GUI::Window, setTitle), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("Window", "void set_size(Size &in size)", asMETHOD(ScriptInterface::GUI::Window, setSize), asCALL_THISCALL); assert( r >= 0 );

    r = script.engine->RegisterObjectMethod("Window", "void append(Sizable @sizable)", asMETHOD(ScriptInterface::GUI::HorizontalLayout, appendSizable), asCALL_THISCALL); assert( r >= 0 );
    //r = script.engine->RegisterObjectMethod("Window", "void append(VerticalLayout @sizable)", asMETHOD(ScriptInterface::GUI::Window, appendSizable), asCALL_THISCALL); assert( r >= 0 );
    //r = script.engine->RegisterObjectMethod("Window", "void append(HorizontalLayout @sizable)", asMETHOD(ScriptInterface::GUI::Window, appendSizable), asCALL_THISCALL); assert( r >= 0 );
    //r = script.engine->RegisterObjectMethod("Window", "void append(LineEdit @sizable)", asMETHOD(ScriptInterface::GUI::Window, appendSizable), asCALL_THISCALL); assert( r >= 0 );
    //r = script.engine->RegisterObjectMethod("Window", "void append(Label @sizable)", asMETHOD(ScriptInterface::GUI::Window, appendSizable), asCALL_THISCALL); assert( r >= 0 );
    //r = script.engine->RegisterObjectMethod("Window", "void append(Button @sizable)", asMETHOD(ScriptInterface::GUI::Window, appendSizable), asCALL_THISCALL); assert( r >= 0 );
    //r = script.engine->RegisterObjectMethod("Window", "void append(Canvas @sizable)", asMETHOD(ScriptInterface::GUI::Window, appendSizable), asCALL_THISCALL); assert( r >= 0 );

    // VerticalLayout
    r = script.engine->RegisterObjectBehaviour("VerticalLayout", asBEHAVE_FACTORY, "VerticalLayout@ f()", asFUNCTION(ScriptInterface::GUI::createVerticalLayout), asCALL_CDECL); assert(r >= 0);
    r = script.engine->RegisterObjectBehaviour("VerticalLayout", asBEHAVE_ADDREF, "void f()", asMETHOD(ScriptInterface::GUI::VerticalLayout, ref_add), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectBehaviour("VerticalLayout", asBEHAVE_RELEASE, "void f()", asMETHOD(ScriptInterface::GUI::VerticalLayout, ref_release), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("VerticalLayout", "void resize()", asMETHOD(ScriptInterface::GUI::VerticalLayout, resize), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("VerticalLayout", "void set_visible(bool visible)", asMETHOD(ScriptInterface::GUI::VerticalLayout, setVisible), asCALL_THISCALL); assert( r >= 0 );

    RegisterSizableMembers<ScriptInterface::GUI::VerticalLayout>(script.engine, "VerticalLayout");
    r = script.engine->RegisterObjectMethod("VerticalLayout", "void append(Sizable @sizable, Size &in size)", asMETHOD(ScriptInterface::GUI::VerticalLayout, appendSizable), asCALL_THISCALL); assert( r >= 0 );
    //r = script.engine->RegisterObjectMethod("VerticalLayout", "void append(VerticalLayout @sizable, Size &in size)", asMETHOD(ScriptInterface::GUI::VerticalLayout, appendSizable), asCALL_THISCALL); assert( r >= 0 );
    //r = script.engine->RegisterObjectMethod("VerticalLayout", "void append(HorizontalLayout @sizable, Size &in size)", asMETHOD(ScriptInterface::GUI::VerticalLayout, appendSizable), asCALL_THISCALL); assert( r >= 0 );
    //r = script.engine->RegisterObjectMethod("VerticalLayout", "void append(LineEdit @sizable, Size &in size)", asMETHOD(ScriptInterface::GUI::VerticalLayout, appendSizable), asCALL_THISCALL); assert( r >= 0 );
    //r = script.engine->RegisterObjectMethod("VerticalLayout", "void append(Label @sizable, Size &in size)", asMETHOD(ScriptInterface::GUI::VerticalLayout, appendSizable), asCALL_THISCALL); assert( r >= 0 );
    //r = script.engine->RegisterObjectMethod("VerticalLayout", "void append(Button @sizable, Size &in size)", asMETHOD(ScriptInterface::GUI::VerticalLayout, appendSizable), asCALL_THISCALL); assert( r >= 0 );
    //r = script.engine->RegisterObjectMethod("VerticalLayout", "void append(Canvas @sizable, Size &in size)", asMETHOD(ScriptInterface::GUI::VerticalLayout, appendSizable), asCALL_THISCALL); assert( r >= 0 );

    // HorizontalLayout
    r = script.engine->RegisterObjectBehaviour("HorizontalLayout", asBEHAVE_FACTORY, "HorizontalLayout@ f()", asFUNCTION(ScriptInterface::GUI::createHorizontalLayout), asCALL_CDECL); assert(r >= 0);
    r = script.engine->RegisterObjectBehaviour("HorizontalLayout", asBEHAVE_ADDREF, "void f()", asMETHOD(ScriptInterface::GUI::HorizontalLayout, ref_add), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectBehaviour("HorizontalLayout", asBEHAVE_RELEASE, "void f()", asMETHOD(ScriptInterface::GUI::HorizontalLayout, ref_release), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("HorizontalLayout", "void resize()", asMETHOD(ScriptInterface::GUI::HorizontalLayout, resize), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("HorizontalLayout", "void set_visible(bool visible)", asMETHOD(ScriptInterface::GUI::HorizontalLayout, setVisible), asCALL_THISCALL); assert( r >= 0 );

    RegisterSizableMembers<ScriptInterface::GUI::HorizontalLayout>(script.engine, "HorizontalLayout");
    r = script.engine->RegisterObjectMethod("HorizontalLayout", "void append(Sizable @sizable, Size &in size)", asMETHOD(ScriptInterface::GUI::HorizontalLayout, appendSizable), asCALL_THISCALL); assert( r >= 0 );
    //r = script.engine->RegisterObjectMethod("HorizontalLayout", "void append(VerticalLayout @sizable, Size &in size)", asMETHOD(ScriptInterface::GUI::HorizontalLayout, appendSizable), asCALL_THISCALL); assert( r >= 0 );
    //r = script.engine->RegisterObjectMethod("HorizontalLayout", "void append(HorizontalLayout @sizable, Size &in size)", asMETHOD(ScriptInterface::GUI::HorizontalLayout, appendSizable), asCALL_THISCALL); assert( r >= 0 );
    //r = script.engine->RegisterObjectMethod("HorizontalLayout", "void append(LineEdit @sizable, Size &in size)", asMETHOD(ScriptInterface::GUI::HorizontalLayout, appendSizable), asCALL_THISCALL); assert( r >= 0 );
    //r = script.engine->RegisterObjectMethod("HorizontalLayout", "void append(Label @sizable, Size &in size)", asMETHOD(ScriptInterface::GUI::HorizontalLayout, appendSizable), asCALL_THISCALL); assert( r >= 0 );
    //r = script.engine->RegisterObjectMethod("HorizontalLayout", "void append(Button @sizable, Size &in size)", asMETHOD(ScriptInterface::GUI::HorizontalLayout, appendSizable), asCALL_THISCALL); assert( r >= 0 );
    //r = script.engine->RegisterObjectMethod("HorizontalLayout", "void append(Canvas @sizable, Size &in size)", asMETHOD(ScriptInterface::GUI::HorizontalLayout, appendSizable), asCALL_THISCALL); assert( r >= 0 );

    // LineEdit
    // Register a simple funcdef for the callback
    r = script.engine->RegisterFuncdef("void LineEditCallback(LineEdit @self)"); assert(r >= 0);
    r = script.engine->RegisterObjectBehaviour("LineEdit", asBEHAVE_FACTORY, "LineEdit@ f()", asFUNCTION(ScriptInterface::GUI::createLineEdit), asCALL_CDECL); assert(r >= 0);
    r = script.engine->RegisterObjectBehaviour("LineEdit", asBEHAVE_ADDREF, "void f()", asMETHOD(ScriptInterface::GUI::LineEdit, ref_add), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectBehaviour("LineEdit", asBEHAVE_RELEASE, "void f()", asMETHOD(ScriptInterface::GUI::LineEdit, ref_release), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("LineEdit", "void set_visible(bool visible)", asMETHOD(ScriptInterface::GUI::LineEdit, setVisible), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("LineEdit", "string &get_text()", asMETHOD(ScriptInterface::GUI::LineEdit, getText), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("LineEdit", "void set_text(const string &in text)", asMETHOD(ScriptInterface::GUI::LineEdit, setText), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("LineEdit", "void set_on_change(LineEditCallback @cb)", asMETHOD(ScriptInterface::GUI::LineEdit, setOnChange), asCALL_THISCALL); assert( r >= 0 );
    RegisterSizableMembers<ScriptInterface::GUI::LineEdit>(script.engine, "LineEdit");

    // Label
    r = script.engine->RegisterObjectBehaviour("Label", asBEHAVE_FACTORY, "Label@ f()", asFUNCTION(ScriptInterface::GUI::createLabel), asCALL_CDECL); assert(r >= 0);
    r = script.engine->RegisterObjectBehaviour("Label", asBEHAVE_ADDREF, "void f()", asMETHOD(ScriptInterface::GUI::Label, ref_add), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectBehaviour("Label", asBEHAVE_RELEASE, "void f()", asMETHOD(ScriptInterface::GUI::Label, ref_release), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("Label", "void set_visible(bool visible)", asMETHOD(ScriptInterface::GUI::Label, setVisible), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("Label", "string &get_text()", asMETHOD(ScriptInterface::GUI::Label, getText), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("Label", "void set_text(const string &in text)", asMETHOD(ScriptInterface::GUI::Label, setText), asCALL_THISCALL); assert( r >= 0 );
    RegisterSizableMembers<ScriptInterface::GUI::Label>(script.engine, "Label");

    // Button
    // Register a simple funcdef for the callback
    r = script.engine->RegisterFuncdef("void ButtonCallback(Button @self)"); assert(r >= 0);
    r = script.engine->RegisterObjectBehaviour("Button", asBEHAVE_FACTORY, "Button@ f()", asFUNCTION(ScriptInterface::GUI::createButton), asCALL_CDECL); assert(r >= 0);
    r = script.engine->RegisterObjectBehaviour("Button", asBEHAVE_ADDREF, "void f()", asMETHOD(ScriptInterface::GUI::Button, ref_add), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectBehaviour("Button", asBEHAVE_RELEASE, "void f()", asMETHOD(ScriptInterface::GUI::Button, ref_release), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("Button", "void set_visible(bool visible)", asMETHOD(ScriptInterface::GUI::Button, setVisible), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("Button", "string &get_text()", asMETHOD(ScriptInterface::GUI::Button, getText), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("Button", "void set_text(const string &in text)", asMETHOD(ScriptInterface::GUI::Button, setText), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("Button", "void set_on_activate(ButtonCallback @cb)", asMETHOD(ScriptInterface::GUI::Button, setOnActivate), asCALL_THISCALL); assert( r >= 0 );
    RegisterSizableMembers<ScriptInterface::GUI::Button>(script.engine, "Button");

    // Canvas
    r = script.engine->RegisterObjectBehaviour("Canvas", asBEHAVE_FACTORY, "Canvas@ f()", asFUNCTION(ScriptInterface::GUI::createCanvas), asCALL_CDECL); assert(r >= 0);
    r = script.engine->RegisterObjectBehaviour("Canvas", asBEHAVE_ADDREF, "void f()", asMETHOD(ScriptInterface::GUI::Canvas, ref_add), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectBehaviour("Canvas", asBEHAVE_RELEASE, "void f()", asMETHOD(ScriptInterface::GUI::Canvas, ref_release), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("Canvas", "void set_visible(bool visible)", asMETHOD(ScriptInterface::GUI::Canvas, setVisible), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("Canvas", "void set_size(Size &in size)", asMETHOD(ScriptInterface::GUI::Canvas, setSize), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("Canvas", "void update()", asMETHOD(ScriptInterface::GUI::Canvas, update), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("Canvas", "void fill(uint16 color)", asMETHOD(ScriptInterface::GUI::Canvas, fill), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("Canvas", "void pixel(int x, int y, uint16 color)", asMETHOD(ScriptInterface::GUI::Canvas, pixel), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("Canvas", "void draw_sprite_4bpp(int x, int y, uint c, uint width, uint height, const array<uint16> &in tiledata, const array<uint16> &in palette)", asMETHOD(ScriptInterface::GUI::Canvas, draw_sprite_4bpp), asCALL_THISCALL); assert( r >= 0 );
    RegisterSizableMembers<ScriptInterface::GUI::Canvas>(script.engine, "Canvas");
  }

  r = script.engine->SetDefaultNamespace(defaultNamespace); assert(r >= 0);

  // create context:
  script.context = script.engine->CreateContext();

  script.context->SetExceptionCallback(asMETHOD(ScriptInterface::ExceptionHandler, exceptionCallback), &ScriptInterface::exceptionHandler, asCALL_THISCALL);
}

auto Interface::loadScript(string location) -> void {
  int r;

  if (!inode::exists(location)) return;

  if (script.module) {
    unloadScript();
  }

  // load script from file:
  string scriptSource = string::read(location);

  // add script into module:
  script.module = script.engine->GetModule(nullptr, asGM_ALWAYS_CREATE);
  r = script.module->AddScriptSection("script", scriptSource.begin(), scriptSource.length());
  assert(r >= 0);

  // compile module:
  r = script.module->Build();
  assert(r >= 0);

  // bind to functions in the script:
  script.funcs.init = script.module->GetFunctionByDecl("void init()");
  script.funcs.pre_nmi = script.module->GetFunctionByDecl("void pre_nmi()");
  script.funcs.pre_frame = script.module->GetFunctionByDecl("void pre_frame()");
  script.funcs.post_frame = script.module->GetFunctionByDecl("void post_frame()");

  if (script.funcs.init) {
    script.context->Prepare(script.funcs.init);
    script.context->Execute();
  }
}

auto Interface::unloadScript() -> void {
  // free any references to script callbacks:
  ::SuperFamicom::bus.reset_interceptors();
  ::SuperFamicom::cpu.reset_dma_interceptor();
  // TODO: GUI callbacks

  if (script.module) {
    script.module->Discard();
    script.module = nullptr;
  }

  script.funcs.init = nullptr;
  script.funcs.pre_frame = nullptr;
  script.funcs.post_frame = nullptr;

  // reset extra-tile data:
  ScriptInterface::extraLayer.reset();
}
