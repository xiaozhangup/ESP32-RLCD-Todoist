struct FatFsFile {
  File file;
};

struct DisplayFontSlot {
  const char *name;
  const char *path;
  lv_font_t *font = nullptr;
};

static DisplayFontSlot display_fonts[] = {
  {"latin", "F:/fonts/fusion_pixel_10_latin.bin"},
  {"zh_hans", "F:/fonts/fusion_pixel_10_zh_hans.bin"},
  {"zh_hant", "F:/fonts/fusion_pixel_10_zh_hant.bin"},
  {"ja", "F:/fonts/fusion_pixel_10_ja.bin"},
  {"ko", "F:/fonts/fusion_pixel_10_ko.bin"},
};

static lv_fs_drv_t fatfs_drv;

static int displayFontCount();

static void *openFontFile(lv_fs_drv_t *, const char *path, lv_fs_mode_t mode) {
  if (mode != LV_FS_MODE_RD) {
    return nullptr;
  }
  String full_path = path && path[0] == '/' ? String(path) : String("/") + path;
  File file = FFat.open(full_path, FILE_READ);
  if (!file) {
    return nullptr;
  }
  return new FatFsFile{file};
}

static lv_fs_res_t closeFontFile(lv_fs_drv_t *, void *file_p) {
  FatFsFile *font_file = static_cast<FatFsFile *>(file_p);
  font_file->file.close();
  delete font_file;
  return LV_FS_RES_OK;
}

static lv_fs_res_t readFontFile(lv_fs_drv_t *, void *file_p, void *buf, uint32_t btr, uint32_t *br) {
  FatFsFile *font_file = static_cast<FatFsFile *>(file_p);
  const int read_len = font_file->file.read(static_cast<uint8_t *>(buf), btr);
  if (read_len < 0) {
    return LV_FS_RES_UNKNOWN;
  }
  *br = static_cast<uint32_t>(read_len);
  return LV_FS_RES_OK;
}

static lv_fs_res_t seekFontFile(lv_fs_drv_t *, void *file_p, uint32_t pos, lv_fs_whence_t whence) {
  FatFsFile *font_file = static_cast<FatFsFile *>(file_p);
  SeekMode mode = SeekSet;
  if (whence == LV_FS_SEEK_CUR) {
    mode = SeekCur;
  } else if (whence == LV_FS_SEEK_END) {
    mode = SeekEnd;
  }
  return font_file->file.seek(pos, mode) ? LV_FS_RES_OK : LV_FS_RES_UNKNOWN;
}

static lv_fs_res_t tellFontFile(lv_fs_drv_t *, void *file_p, uint32_t *pos_p) {
  FatFsFile *font_file = static_cast<FatFsFile *>(file_p);
  *pos_p = font_file->file.position();
  return LV_FS_RES_OK;
}

static bool registerFontFs() {
  if (!FFat.begin(false)) {
    Serial.println("[Font] FFat mount failed; upload data/fonts to FATFS");
    return false;
  }
  lv_fs_drv_init(&fatfs_drv);
  fatfs_drv.letter = 'F';
  fatfs_drv.open_cb = openFontFile;
  fatfs_drv.close_cb = closeFontFile;
  fatfs_drv.read_cb = readFontFile;
  fatfs_drv.seek_cb = seekFontFile;
  fatfs_drv.tell_cb = tellFontFile;
  lv_fs_drv_register(&fatfs_drv);
  Serial.printf("[Font] FFat mounted, total=%u used=%u\n",
                static_cast<unsigned>(FFat.totalBytes()),
                static_cast<unsigned>(FFat.usedBytes()));
  return true;
}

static bool initFontStorage() {
  if (!registerFontFs()) {
    return false;
  }
  bool loaded_any = false;
  int loaded_count = 0;
  for (DisplayFontSlot &slot : display_fonts) {
    slot.font = lv_binfont_create(slot.path);
    if (slot.font) {
      loaded_any = true;
      Serial.printf("[Font] Loaded %s from %s\n", slot.name, slot.path);
    } else {
      Serial.printf("[Font] Failed to load %s from %s\n", slot.name, slot.path);
    }
    loaded_count++;
    showBootLoading(loaded_count, displayFontCount());
  }
  return loaded_any;
}

static int displayFontCount() {
  return sizeof(display_fonts) / sizeof(display_fonts[0]);
}

static bool glyphFromFont(const lv_font_t *font, uint32_t cp, lv_font_glyph_dsc_t &glyph) {
  return font && lv_font_get_glyph_dsc(font, &glyph, cp, 0);
}

static const lv_font_t *findGlyphFont(uint32_t cp, lv_font_glyph_dsc_t &glyph) {
  const int latin = 0;
  const int zh_hans = 1;
  const int zh_hant = 2;
  const int ja = 3;
  const int ko = 4;

  const bool hangul = (cp >= 0x3130 && cp <= 0x318f) || (cp >= 0xac00 && cp <= 0xd7af);
  const bool kana = cp >= 0x3040 && cp <= 0x30ff;
  const bool cjk = (cp >= 0x3000 && cp <= 0x303f) || (cp >= 0x4e00 && cp <= 0x9fff) ||
                   (cp >= 0xff00 && cp <= 0xffef);
  const int *order = nullptr;
  int count = 0;

  static constexpr int LATIN_ORDER[] = {latin, zh_hans, zh_hant, ja, ko};
  static constexpr int CJK_ORDER[] = {zh_hans, zh_hant, ja, ko, latin};
  static constexpr int JA_ORDER[] = {ja, zh_hans, zh_hant, latin, ko};
  static constexpr int KO_ORDER[] = {ko, latin, zh_hans, zh_hant, ja};

  if (hangul) {
    order = KO_ORDER;
    count = sizeof(KO_ORDER) / sizeof(KO_ORDER[0]);
  } else if (kana) {
    order = JA_ORDER;
    count = sizeof(JA_ORDER) / sizeof(JA_ORDER[0]);
  } else if (cjk) {
    order = CJK_ORDER;
    count = sizeof(CJK_ORDER) / sizeof(CJK_ORDER[0]);
  } else {
    order = LATIN_ORDER;
    count = sizeof(LATIN_ORDER) / sizeof(LATIN_ORDER[0]);
  }

  for (int i = 0; i < count; i++) {
    const lv_font_t *font = display_fonts[order[i]].font;
    if (glyphFromFont(font, cp, glyph)) {
      return glyph.resolved_font ? glyph.resolved_font : font;
    }
  }
  return nullptr;
}

static int displayFontLineHeight() {
  int line_height = 10;
  for (const DisplayFontSlot &slot : display_fonts) {
    if (slot.font) {
      line_height = max(line_height, static_cast<int>(slot.font->line_height));
    }
  }
  return line_height;
}

static int displayFontLineOffset(const lv_font_t *font) {
  return font ? max(0, displayFontLineHeight() - static_cast<int>(font->line_height)) / 2 : 0;
}
