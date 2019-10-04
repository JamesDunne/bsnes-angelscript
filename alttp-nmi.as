// Communicate with ROM hack via memory at $7F7667[0x6719]

class OAMSprite {
  uint8 b0; // xxxxxxxx
  uint8 b1; // yyyyyyyy
  uint8 b2; // cccccccc
  uint8 b3; // vhoopppc
  uint8 b4; // ------sx

  OAMSprite() {}

  int16 x { get const { return int16(b0) | (int16(b4 & 1) << 8); } };
  uint8 y { get const { return b1; }};
  uint16 chr { get const { return uint16(b2) | ((uint16(b3) & 1) << 8); } };
  uint8 palette { get const { return (b3 >> 1) & 7; } };
  uint8 priority { get const { return (b3 >> 4) & 3; } };
  uint8 hflip { get const { return (b3 >> 6) & 1; } };
  uint8 vflip { get const { return (b3 >> 7) & 1; } };
  uint8 size { get const { return (b4 & 2) >> 1; } };
};

class Packet {
  uint32 addr;

  uint32 location;
  uint16 x, y, z;
  uint16 xoffs, yoffs;

  uint16 oam_size;
  array<OAMSprite> oam_table;

  array<uint16> tiledata;

  Packet(uint32 addr) {
    this.addr = addr;
  }

  void readRAM() {
    auto a = addr;
    location = uint32(bus::read_u16(a+0, a+1)) | (uint32(bus::read_u8(a+2)) << 16); a += 3;
    x = bus::read_u16(a+0, a+1); a += 2;
    y = bus::read_u16(a+0, a+1); a += 2;
    z = bus::read_u16(a+0, a+1); a += 2;
    xoffs = bus::read_u16(a+0, a+1); a += 2;
    yoffs = bus::read_u16(a+0, a+1); a += 2;

    // read oam_table:
    oam_size = bus::read_u16(a+0, a+1); a += 2;
    auto len = oam_size / 5;
    oam_table.resize(len);
    for (uint i = 0; i < len; i++) {
      oam_table[i].b0 = bus::read_u8(a); a++;
      oam_table[i].b1 = bus::read_u8(a); a++;
      oam_table[i].b2 = bus::read_u8(a); a++;
      oam_table[i].b3 = bus::read_u8(a); a++;
      oam_table[i].b4 = bus::read_u8(a); a++;
    }

    // read tiledata:
    a = addr + 0x298;
    tiledata.resize(0x10);
    for (uint i = 0; i < 0x10; i++) {
      tiledata[i] = bus::read_u16(a+0, a+1);
      a += 2;
    }
  }
};

Packet local(0x7f7668);
Packet remote(0x7f7668); // TODO: update me to where remote packet is stored in RAM

void pre_frame() {
  local.readRAM();
}

void post_frame() {
  ppu::frame.text_shadow = true;
  ppu::frame.color = 0x7fff;

  // read local packet composed during NMI:
  ppu::frame.text(0, 0, fmtHex(local.location, 6));
  ppu::frame.text(52, 0, fmtHex(local.x, 4));
  ppu::frame.text(88, 0, fmtHex(local.y, 4));
  ppu::frame.text(124, 0, fmtHex(local.z, 4));
  ppu::frame.text(160, 0, fmtHex(local.xoffs, 4));
  ppu::frame.text(196, 0, fmtHex(local.yoffs, 4));

  auto len = local.oam_table.length();
  if (len > 26) len = 26;

  ppu::frame.text(0, 8, fmtHex(len, 2));
  for (uint i = 0; i < len; i++) {
    auto y = 224 - ((len - i) * 8);
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

  for (uint i = 0; i < 0x10; i++) {
    ppu::frame.text(i*16, 16, fmtHex(local.tiledata[i], 4));
  }

  if (false) {
    auto in_dark_world = bus::read_u8(0x7E0FFF);
    auto in_dungeon = bus::read_u8(0x7E001B);
    auto overworld_room = bus::read_u16(0x7E008A, 0x7E008B);
    auto dungeon_room = bus::read_u16(0x7E00A0, 0x7E00A1);

    // compute aggregated location for Link into a single 24-bit number:
    auto location =
      uint32(in_dark_world & 1) << 17 |
      uint32(in_dungeon & 1) << 16 |
      uint32(in_dungeon != 0 ? dungeon_room : overworld_room);

    ppu::frame.text(0, 8, fmtHex(location, 6));
  }
}
