
struct GUI {
  struct Object {
    virtual operator hiro::mObject*() = 0;
    virtual auto ref_add() -> void = 0;
    virtual auto ref_release() -> void = 0;
  };

  struct Sizable : Object {
    virtual operator hiro::mSizable*() = 0;
    virtual void setPosition(float x, float y) {
      auto sizable = (operator hiro::mSizable*)();
      sizable->setGeometry(hiro::Geometry(
        hiro::Position(x, y),
        sizable->geometry().size()
      ));
    }
  };

#define BindShared(Name) \
    hiro::s##Name self; \
    ~Name() { \
      self->setVisible(false); \
      self->reset(); \
      /*self->destruct();*/ \
    } \
    auto ref_add() -> void override { \
      self.manager->strong++; \
      printf("%p ref_add() -> %d\n", self.manager, self.manager->strong); \
    } \
    auto ref_release() -> void override { \
      if (--self.manager->strong == 0) { \
        printf("%p ref_release() => 0; delete this!\n", self.manager); \
        delete this; \
      } \
      printf("%p ref_release() -> %d\n", self.manager, self.manager->strong); \
    }

#define BindObject(Name) \
    operator hiro::mObject*() { return (hiro::mObject*)self.data(); } \
    auto setVisible(bool visible = true) -> void { \
      self->setVisible(visible); \
    }

#define BindConstructor(Name) \
    Name() { \
      self = new hiro::m##Name(); \
      self->construct(); \
    } \

#define BindSizable(Name) \
    operator hiro::mSizable*() override { \
      printf("operator mSizable() \n"); \
      return (hiro::mSizable*)self.data(); \
    }

  struct Window : Object {
    BindShared(Window) \
      BindObject(Window)

    Window() {
      self = new hiro::mWindow();
      self->construct();
    }

    // create a Window relative to main presentation window
    Window(float x, float y, bool relative = false) : Window() {
      if (relative) {
        self->setPosition(platform->presentationWindow(), hiro::Position{x, y});
      } else {
        self->setPosition(hiro::Position{x, y});
      }
    }

    auto appendSizable(Sizable *child) -> void {
      self->append((hiro::mSizable*)*child);
    }

    auto setSize(hiro::Size *size) -> void {
      self->setSize(*size);
    }

    auto setTitle(const string *title) -> void {
      self->setTitle(*title);
    }
  };

  struct VerticalLayout : Sizable {
    BindShared(VerticalLayout)
    BindObject(VerticalLayout)
    BindSizable(VerticalLayout)

    VerticalLayout() {
      self = new hiro::mVerticalLayout();
      self->construct();
    }

    auto resize() -> void {
      self->resize();
    }

    auto appendSizable(Sizable *child, hiro::Size *size) -> void {
      self->append((hiro::mSizable*)*child, *size);
    }
  };

  struct HorizontalLayout : Sizable {
    BindShared(HorizontalLayout)
    BindObject(HorizontalLayout)
    BindSizable(HorizontalLayout)

    HorizontalLayout() {
      self = new hiro::mHorizontalLayout();
      self->construct();
    }

    auto resize() -> void {
      self->resize();
    }

    auto appendSizable(Sizable *child, hiro::Size *size) -> void {
      self->append((hiro::mSizable*)*child, *size);
    }
  };

  struct LineEdit : Sizable {
    BindShared(LineEdit)
    BindObject(LineEdit)
    BindSizable(LineEdit)

    LineEdit() {
      self = new hiro::mLineEdit();
      self->construct();
      self->setFocusable();
    }

    auto getText() -> string* {
      return new string(self->text());
    }

    auto setText(const string *text) -> void {
      self->setText(*text);
    }

    asIScriptFunction *onChange = nullptr;
    auto setOnChange(asIScriptFunction *func) -> void {
      if (onChange != nullptr) {
        onChange->Release();
      }

      onChange = func;

      // register onChange callback with LineEdit:
      auto me = this;
      auto ctx = asGetActiveContext();
      self->onChange([=]() -> void {
        ctx->Prepare(onChange);
        ctx->SetArgObject(0, me);
        ctx->Execute();
      });
    }
  };

  struct Label : Sizable {
    BindShared(Label)
    BindObject(Label)
    BindSizable(Label)

    Label() {
      self = new hiro::mLabel();
      self->construct();
    }

    auto getText() -> string * {
      return new string(self->text());
    }

    auto setText(const string *text) -> void {
      self->setText(*text);
    }
  };

  struct Button : Sizable {
    BindShared(Button)
    BindObject(Button)
    BindSizable(Button)

    Button() {
      self = new hiro::mButton();
      self->construct();
      self->setFocusable();
    }

    auto getText() -> string* {
      return new string(self->text());
    }

    auto setText(const string *text) -> void {
      self->setText(*text);
    }

    asIScriptFunction *onActivate = nullptr;
    auto setOnActivate(asIScriptFunction *func) -> void {
      if (onActivate != nullptr) {
        onActivate->Release();
      }

      onActivate = func;

      // register onActivate callback with Button:
      auto me = this;
      auto ctx = asGetActiveContext();
      self->onActivate([=]() -> void {
        ctx->Prepare(onActivate);
        ctx->SetArgObject(0, me);
        ctx->Execute();
      });
    }
  };

  struct Canvas : Sizable {
    BindShared(Canvas)
    BindObject(Canvas)
    BindSizable(Canvas)

    Canvas() {
      self = new hiro::mCanvas();
      self->construct();
    }

    auto setSize(hiro::Size *size) -> void {
      // Set 15-bit BGR format for PPU-compatible images:
      image img(0, 16, 0x8000u, 0x001Fu, 0x03E0u, 0x7C00u);
      img.allocate(size->width(), size->height());
      img.fill(0x0000u);
      self->setIcon(img);
    }

    auto update() -> void {
      self->update();
    }

    auto fill(uint16 color) -> void {
      auto& img = self->iconRef();
      img.fill(color);
    }

    auto pixel(int x, int y, uint16 color) -> void {
      auto& img = self->iconRef();
      // bounds check:
      if (x < 0 || y < 0 || x >= img.width() || y >= img.height()) return;
      // set pixel with full alpha (1-bit on/off):
      img.write(img.data() + (y * img.pitch()) + (x * img.stride()), color | 0x8000u);
    }

    auto draw_sprite_4bpp(int x, int y, uint c, uint width, uint height, const CScriptArray *tile_data, const CScriptArray *palette_data) -> void {
      // Check validity of array inputs:
      if (tile_data == nullptr) {
        asGetActiveContext()->SetException("tile_data array cannot be null", true);
        return;
      }
      if (tile_data->GetElementTypeId() != asTYPEID_UINT16) {
        asGetActiveContext()->SetException("tile_data array must be uint16[]", true);
        return;
      }
      if (tile_data->GetSize() < 16) {
        asGetActiveContext()->SetException("tile_data array must have at least 16 elements", true);
        return;
      }

      if (palette_data == nullptr) {
        asGetActiveContext()->SetException("palette_data array cannot be null", true);
        return;
      }
      if (palette_data->GetElementTypeId() != asTYPEID_UINT16) {
        asGetActiveContext()->SetException("palette_data array must be uint16[]", true);
        return;
      }
      if (palette_data->GetSize() < 16) {
        asGetActiveContext()->SetException("palette_data array must have 16 elements", true);
        return;
      }

      auto tile_data_p = static_cast<const uint16 *>(tile_data->At(0));
      if (tile_data_p == nullptr) {
        asGetActiveContext()->SetException("tile_data array value pointer must not be null", true);
        return;
      }
      auto palette_p = static_cast<const b5g5r5 *>(palette_data->At(0));
      if (palette_p == nullptr) {
        asGetActiveContext()->SetException("palette_data array value pointer must not be null", true);
        return;
      }

      auto tileWidth = width >> 3u;
      auto tileHeight = height >> 3u;
      for (int ty = 0; ty < tileHeight; ty++) {
        for (int tx = 0; tx < tileWidth; tx++) {
          // data is stored in runs of 8x8 pixel tiles:
          auto p = tile_data_p + (((((c >> 4u) + ty) << 4u) + (tx + c & 15)) << 4u);

          for (int py = 0; py < 8; py++) {
            // consume 8 pixel columns:
            uint32 tile = uint32(p[py]) | (uint32(p[py+8]) << 16u);
            for (int px = 0; px < 8; px++) {
              uint32 t = 0u, shift = 7u - px;
              t += tile >> (shift + 0u) & 1u;
              t += tile >> (shift + 7u) & 2u;
              t += tile >> (shift + 14u) & 4u;
              t += tile >> (shift + 21u) & 8u;
              if (t) {
                auto color = palette_p[t];
                pixel(x + (tx << 3) + px, y + (ty << 3) + py, color);
              }
            }
          }
        }
      }
    }
  };


#define Constructor(Name) \
    static auto create##Name() -> Name* { return new Name(); }

  // Size is a value type:
  static auto createSize(void *memory) -> void { new(memory) hiro::Size(); }
  static auto createSizeWH(float width, float height, void *memory) -> void { new(memory) hiro::Size(width, height); }
  static auto destroySize(void *memory) -> void { ((hiro::Size*)memory)->~Size(); }

  Constructor(Window)
  static auto createWindowAtPosition(float x, float y, bool relative) -> Window* { return new Window(x, y, relative); }
  Constructor(VerticalLayout)
  Constructor(HorizontalLayout)
  Constructor(LineEdit)
  Constructor(Label)
  Constructor(Button)
  Constructor(Canvas)

#undef Constructor
};
