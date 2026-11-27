"""Generate flash-resident translations, phone pages and accent glyphs."""
from html import escape
from io import BytesIO
import json
import re
from PIL import ImageFont

LANGUAGES = ("en", "it", "es")


def read_catalog(path):
    entries = json.loads(path.read_text(encoding="utf-8"))
    for key, row in entries.items():
        if not key or set(row) != {"it", "es"} or not all(row.values()):
            raise ValueError(f"Incomplete translation for {key!r} in {path}")
        row["en"] = key
    return entries


def prepare_languages(root, sketch, write):
    entries = read_catalog(root / "locales/dial.json")
    def literal(s):
        return json.dumps(s, ensure_ascii=False)
    rows = (", ".join(literal(row[lang]) for lang in LANGUAGES)
            for _, row in sorted(entries.items()))
    write(sketch / "messages.inc", "".join("    {" + row + "},\n" for row in rows))

    phone = read_catalog(root / "locales/phone.json")
    template = (root / "firmware/phone/setup.html").read_text()
    pages = []
    for lang in LANGUAGES:
        def insert(match):
            context, key = match.groups()
            value = phone[key][lang]
            return literal(value) if context == "js" else escape(value)
        html = re.sub(r"\{\{(js|html):([^{}]+)\}\}", insert, template.replace("{{lang}}", lang))
        pages.append(f'static const char phone_{lang}[] PROGMEM = R"PAGE({html})PAGE";\n')
    write(sketch / "phone_pages.inc", "".join(pages))


def with_accents(source, font_bytes, size):
    """Attach a same-size Montserrat fallback to the vendor LVGL font."""
    # Latin-1 covers accented names as well as the Spanish and Italian interface.
    chars = list(range(0xA1, 0x100))
    font = ImageFont.truetype(BytesIO(font_bytes), size)
    bitmap = bytearray()
    metrics = ["{0}"]
    for cp in chars:
        ch = chr(cp)
        mask, (x, y) = font.getmask2(ch, mode="L", anchor="ls")
        w, h = mask.size
        start = len(bitmap)
        pixels = bytes(mask)
        if len(pixels) % 2:
            pixels += b"\0"
        bitmap.extend((a & 240) | (b >> 4) for a, b in zip(pixels[::2], pixels[1::2]))
        advance = round(16 * font.getlength(ch))
        metrics.append(f"{{.bitmap_index={start},.adv_w={advance},.box_w={w},.box_h={h},.ofs_x={x},.ofs_y={-y-h}}}")
    font_data = """
static const uint8_t latin_bitmap[] = {%s};
static const lv_font_fmt_txt_glyph_dsc_t latin_metrics[] = {%s};
static const lv_font_fmt_txt_cmap_t latin_map = {
    .range_start = 161, .range_length = 95, .glyph_id_start = 1, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
};
static const lv_font_fmt_txt_dsc_t latin_dsc = {
    .glyph_bitmap = latin_bitmap, .glyph_dsc = latin_metrics, .cmaps = &latin_map,
    .cmap_num = 1, .bpp = 4
};
static const lv_font_t latin_font = {
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt, .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,
    .dsc = &latin_dsc
};
""" % (",".join(map(str, bitmap)), ",\n".join(metrics))
    anchor = "/*Initialize a public general font descriptor*/"
    if anchor not in source or ".dsc = &font_dsc" not in source:
        raise ValueError("Unexpected LVGL font source")
    return source.replace(anchor, font_data + anchor).replace(".dsc = &font_dsc", ".fallback = &latin_font, .dsc = &font_dsc")
