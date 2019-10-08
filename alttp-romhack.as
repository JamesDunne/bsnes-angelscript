// Communicate with ROM hack via memory at $7F7667[0x6719]
class OAMSprite {
  uint8 b0; // xxxxxxxx
  uint8 b1; // yyyyyyyy
  uint8 b2; // cccccccc
  uint8 b3; // vhoopppc
  uint8 b4; // ------sx

  OAMSprite() {}

  int16 x{get const { return int16(b0) | (int16(b4 & 1) << 8); }};
  uint8 y{get const { return b1; }};
  uint16 chr{get const { return uint16(b2) | ((uint16(b3) & 1) << 8); }};
  uint8 palette{get const { return (b3 >> 1) & 7; }};
  uint8 priority{get const { return (b3 >> 4) & 3; }};
  uint8 hflip{get const { return (b3 >> 6) & 1; }};
  uint8 vflip{get const { return (b3 >> 7) & 1; }};
  uint8 size{get const { return (b4 & 2) >> 1; }};
};

const uint16 tile_count = 0x40;
const uint16 tiledata_size = tile_count * 0x10;

class Packet {
  uint32 addr;

  uint32 location;
  uint16 x, y, z;
  uint16 xoffs, yoffs;

  // visual aspects of player taken from SRAM at $7EFxxx:
  uint8 sword;  // $359
  uint8 shield; // $35A
  uint8 armor;  // $35B

  // WRAM locations for source addresses of animated sprite tile data:
  // bank:[addr] where bank is direct and [addr] is indirect

  // $10:[$0ACE] -> $4100 (0x40 bytes) (bottom of head)
  // $10:[$0AD2] -> $4120 (0x40 bytes) (bottom of body)
  // $10:[$0AD6] -> $4140 (0x20 bytes) (bottom sweat/arm/hand)

  // $10:[$0ACC] -> $4000 (0x40 bytes) (top of head)
  // $10:[$0AD0] -> $4020 (0x40 bytes) (top of body)
  // $10:[$0AD4] -> $4040 (0x20 bytes) (top sweat/arm/hand)

  // bank $7E (WRAM) is used to store decompressed 3bpp->4bpp tile data

  // $7E:[$0AC0] -> $4050 (0x40 bytes) (top of sword slash)
  // $7E:[$0AC4] -> $4070 (0x40 bytes) (top of shield)
  // $7E:[$0AC8] -> $4090 (0x40 bytes) (Zz sprites or bugnet top)
  // $7E:[$0AE0] -> $40B0 (0x20 bytes) (top of rupee)
  // $7E:[$0AD8] -> $40C0 (0x40 bytes) (top of movable block)

  // only if bird is active
  // $7E:[$0AF6] -> $40E0 (0x40 bytes) (top of hammer sprites)

  // $7E:[$0AC2] -> $4150 (0x40 bytes) (bottom of sword slash)
  // $7E:[$0AC6] -> $4170 (0x40 bytes) (bottom of shield)
  // $7E:[$0ACA] -> $4190 (0x40 bytes) (music note sprites or bugnet bottom)
  // $7E:[$0AE2] -> $41B0 (0x20 bytes) (bottom of rupee)
  // $7E:[$0ADA] -> $41C0 (0x40 bytes) (bottom of movable block)

  // only if bird is active
  // $7E:[$0AF8] -> $41E0 (0x40 bytes) (bottom of hammer sprites)

  // words from $0AC0..$0ACA:
  array<uint16> dma7E_addr(6);
  // words from $0ACC..$0AD6:
  array<uint16> dma10_addr(6);

  uint8 oam_count;
  array<OAMSprite> oam_table(12);

  array<uint16> tiledata;

  Packet(uint32 addr) {
    this.addr = addr;
  }

  void readRAM() {
    auto a = addr;

    // Read Link location data:
    location = uint32(bus::read_u16(a + 0, a + 1)) | (uint32(bus::read_u8(a + 2)) << 16); a += 3;
    x = bus::read_u16(a + 0, a + 1); a += 2;
    y = bus::read_u16(a + 0, a + 1); a += 2;
    z = bus::read_u16(a + 0, a + 1); a += 2;

    // Read current screen scroll offset:
    xoffs = bus::read_u16(a + 0, a + 1); a += 2;
    yoffs = bus::read_u16(a + 0, a + 1); a += 2;

    // Read visual state:
    sword  = bus::read_u8(a); a++;
    shield = bus::read_u8(a); a++;
    armor  = bus::read_u8(a); a++;

    // read DMA source address for Link sprite tiledata:
    for (uint i = 0; i < 6; i++) {
      dma7E_addr[i] = bus::read_u16(a + 0, a + 1); a += 2;
    }
    for (uint i = 0; i < 6; i++) {
      dma10_addr[i] = bus::read_u16(a + 0, a + 1); a += 2;
    }

    // number of used slots in oam_table:
    oam_count = bus::read_u8(a); a++;
    // read oam_table (always 12 OAM sprites):
    for (uint8 i = 0; i < 12; i++) {
      oam_table[i].b0 = bus::read_u8(a); a++;
      oam_table[i].b1 = bus::read_u8(a); a++;
      oam_table[i].b2 = bus::read_u8(a); a++;
      oam_table[i].b3 = bus::read_u8(a); a++;
      oam_table[i].b4 = bus::read_u8(a); a++;
    }
  }
};

Packet  local(0x7F7700);
Packet remote(0x7F8200);

uint8 module, sub_module;

void pre_frame() {
  local.readRAM();
}

void post_frame() {
  module     = bus::read_u8(0x7E0010);
  sub_module = bus::read_u8(0x7E0011);

  if (true) {
    ppu::frame.text_shadow = true;
    ppu::frame.color = 0x7fff;
    ppu::frame.alpha = 28;

    // module/sub_module:
    ppu::frame.text(  0, 0, fmtHex(module, 2));
    ppu::frame.text( 20, 0, fmtHex(sub_module, 2));

    // read local packet composed during NMI:
    ppu::frame.text(  0, 8, fmtHex(local.location, 6));
    ppu::frame.text( 52, 8, fmtHex(local.x, 4));
    ppu::frame.text( 88, 8, fmtHex(local.y, 4));
    ppu::frame.text(124, 8, fmtHex(local.z, 4));
    ppu::frame.text(160, 8, fmtHex(local.xoffs, 4));
    ppu::frame.text(196, 8, fmtHex(local.yoffs, 4));

    // draw DMA source addresses:
    for (uint i = 0; i < 3; i++) {
      ppu::frame.text((i+3) * (4 * 8 + 4), 224 -  8, fmtHex(local.dma10_addr[i*2+0], 4));
      ppu::frame.text((i+3) * (4 * 8 + 4), 224 - 16, fmtHex(local.dma10_addr[i*2+1], 4));

      ppu::frame.text((i+0) * (4 * 8 + 4), 224 -  8, fmtHex(local.dma7E_addr[i*2+0], 4));
      ppu::frame.text((i+0) * (4 * 8 + 4), 224 - 16, fmtHex(local.dma7E_addr[i*2+1], 4));
    }

    // limited to 12
    auto len = local.oam_count;

    ppu::frame.text(0, 16, fmtHex(len, 2));
    for (uint i = 0; i < len; i++) {
      auto y = 224 - 16 - ((len - i) * 8);
      //ppu::frame.text( 0, y, fmtHex(local.oam_table[i].b0, 2));
      //ppu::frame.text(20, y, fmtHex(local.oam_table[i].b1, 2));
      //ppu::frame.text(40, y, fmtHex(local.oam_table[i].b2, 2));
      //ppu::frame.text(60, y, fmtHex(local.oam_table[i].b3, 2));
      //ppu::frame.text(80, y, fmtHex(local.oam_table[i].b4, 1));

      ppu::frame.text(100, y, fmtHex(local.oam_table[i].x, 3));
      ppu::frame.text(130, y, fmtHex(local.oam_table[i].y, 2));
      ppu::frame.text(150, y, fmtHex(local.oam_table[i].chr, 3));
      ppu::frame.text(180, y, fmtHex(local.oam_table[i].palette, 1));
      ppu::frame.text(190, y, fmtHex(local.oam_table[i].priority, 1));
      ppu::frame.text(200, y, fmtBinary(local.oam_table[i].hflip, 1));
      ppu::frame.text(210, y, fmtBinary(local.oam_table[i].vflip, 1));
    }
  }
}