"""Build every CarbonCD design asset from design/tokens.json.

    python design/build.py            # SVG masters, the design sheet, PNG renders and the app resources
    python design/build.py --no-raster  # SVG masters and the design sheet only (no browser needed)

Writes, under design/: brand/*.svg, icons/*.svg, sheet.html, and with rasters brand/*.png and
mockups/*.png. With rasters it also writes the resources the app compiles in: src/res/cdm.ico,
src/res/idr_main.ico, src/res/AboutImage.bmp and the eight main button bitmaps.

Needs fontTools and uharfbuzz (lettering is outlined, so no SVG depends on an installed font) and,
for rasters, Pillow plus Chrome or Edge. Colours come from tokens.json and nowhere else.
"""
import io
import json
import math
import os
import shutil
import struct
import subprocess
import sys
import tempfile

import uharfbuzz as hb
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen
from fontTools.ttLib import TTFont
from fontTools.varLib import instancer

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
RES = os.path.join(ROOT, "src", "res")
with open(os.path.join(HERE, "tokens.json"), encoding="utf-8") as _f:
    T = json.load(_f)

B = T["color"]["brand"]
L = T["color"]["light"]
D = T["color"]["dark"]
INK, CARBON, CANARY, CANARY_DEEP = B["ink"], B["carbon"], B["canary"], B["canary-deep"]
TAGLINE = "Read, write, clone and master CDs and DVDs."


# ---------------------------------------------------------------- lettering, outlined
def _num(v):
    return ("%.2f" % v).rstrip("0").rstrip(".")


class Face:
    def __init__(self, rel, **axes):
        font = TTFont(os.path.join(HERE, rel))
        if "fvar" in font:
            font = instancer.instantiateVariableFont(font, axes)
        buf = io.BytesIO()
        font.save(buf)
        data = buf.getvalue()
        self.tt = TTFont(io.BytesIO(data))
        self.hb = hb.Font(hb.Face(data))
        self.upm = self.tt["head"].unitsPerEm
        self.glyphs = self.tt.getGlyphSet()

    def run(self, text, x, y, size, tracking=0.0):
        """Shape text with kerning. Returns ([(path data, source index)], advance in px)."""
        buf = hb.Buffer()
        buf.add_str(text)
        buf.guess_segment_properties()
        hb.shape(self.hb, buf)
        s = size / self.upm
        pen = 0.0
        out = []
        for info, pos in zip(buf.glyph_infos, buf.glyph_positions):
            sp = SVGPathPen(self.glyphs, ntos=_num)
            tp = TransformPen(sp, (s, 0, 0, -s, x + pen + pos.x_offset * s, y - pos.y_offset * s))
            self.glyphs[self.tt.getGlyphName(info.codepoint)].draw(tp)
            out.append((sp.getCommands(), info.cluster))
            pen += pos.x_advance * s + tracking
        return out, pen - tracking

    def text(self, text, x, y, size, fill, tracking=0.0, anchor="start"):
        if anchor != "start":
            adv = self.run(text, 0, y, size, tracking)[1]
            x -= adv / 2 if anchor == "middle" else adv
        glyphs, _ = self.run(text, x, y, size, tracking)
        return '<path d="%s" fill="%s"/>' % ("".join(g for g, _ in glyphs), fill)


_faces = {}


def face(key):
    if key not in _faces:
        bric = T["type"]["display"]["file"]
        mono = T["type"]["data"]["files"]
        _faces[key] = {
            "display": lambda: Face(bric, wght=800, opsz=96, wdth=100),
            "text": lambda: Face(bric, wght=500, opsz=24, wdth=100),
            "mono": lambda: Face(mono[0]),
            "mono-medium": lambda: Face(mono[1]),
        }[key]()
    return _faces[key]


# ---------------------------------------------------------------- geometry
def circ(cx, cy, r):
    return "M%s %sa%s %s 0 1 0 %s 0a%s %s 0 1 0 %s 0Z" % (_num(cx - r), _num(cy), _num(r), _num(r), _num(2 * r), _num(r), _num(r), _num(-2 * r))


def svg_doc(inner, w, h):
    return '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 %s %s" width="%s" height="%s">%s</svg>\n' % (w, h, w, h, inner)


def svg_inline(inner, w, h, cls, label):
    return '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 %s %s" class="%s" role="img" aria-label="%s">%s</svg>' % (w, h, cls, label, inner)


# ---------------------------------------------------------------- button graphics, 64 grid
GLYPH = {
    "read": '<path d="M0-8v11M-5-1l5 5 5-5M-7 8h14"/>',
    "write": '<path d="M0-9c4 4 7 7 7 11a7 7 0 0 1-14 0c0-2.500 1.500-4.500 3-6 .5 2 1.500 3 2.500 3C-1.500-3-1-6 0-9z" FILL/>',
    "tracks": '<path d="M-4 6v-12l9-2.500v11"/><circle cx="-6.500" cy="6" r="2.400" FILL/><circle cx="2.500" cy="4" r="2.400" FILL/>',
    "master": '<path d="M-7.500 8l1.500-5.500L4-7.500l4 4-10 10zM1-4.500l4 4"/>',
    "subcode": '<circle cx="-2" cy="-2" r="5.500"/><path d="M2.500 2.500L8 8"/>',
    "recognize": '<path d="M-7 0a7 7 0 0 1 12-5M7 0a7 7 0 0 1-12 5"/><path d="M5.500-9.500v5h-5M-5.500 9.500v-5h5"/>',
    "erase": '<path d="M-8.500 2.500l7-8.500 8.500 6.500-5 6.500h-6.500zM-4.500-2l8 6.500M-2 7.500h10"/>',
}
LABEL = {
    "read": "Read to image", "write": "Write image", "clone": "Clone disc", "tracks": "Read tracks",
    "master": "Master disc", "subcode": "Subcode", "recognize": "Recognise disc", "erase": "Erase disc",
    "drive": "Drive info", "exit": "Exit",
}
KEYS = list(LABEL)
# Resource bitmap of the current main dialog for each graphic (src/cdm.rc, IDB_B_*).
RES_BMP = {
    "read": "CreateImage.bmp", "write": "WriteImage.bmp", "clone": "Duplicate.bmp", "tracks": "ReadTrack.bmp",
    "master": "Mastering.bmp", "subcode": "SubQ.bmp", "recognize": "Recognize.bmp", "exit": "Exit.bmp",
}


def button(key, dark=False):
    ink = D["primary"] if dark else INK
    face_fill = D["surface"] if dark else L["surface"]
    ground = D["window"] if dark else L["window"]
    if key == "exit":
        return ('<path d="M31 13h19v40H31" fill="%s" stroke="%s" stroke-width="3" stroke-linejoin="round"/>'
                '<path d="M40 33H12M20 25l-8 8 8 8" fill="none" stroke="%s" stroke-width="3.400" stroke-linecap="round" '
                'stroke-linejoin="round"/><circle cx="44" cy="33" r="2" fill="%s"/>') % (CANARY, ink, ink, ink)
    if key == "drive":
        return ('<rect x="11" y="25" width="46" height="22" rx="5" fill="%s"/>'
                '<rect x="7" y="20" width="46" height="22" rx="5" fill="%s" stroke="%s" stroke-width="3"/>'
                '<path d="M14 34h22" stroke="%s" stroke-width="3" stroke-linecap="round"/>'
                '<circle cx="45" cy="31" r="2.600" fill="%s"/>') % (CANARY, face_fill, ink, ink, ink)
    out = '<circle cx="30" cy="30" r="20" fill="%s"/>' % CANARY
    out += '<circle cx="26" cy="26" r="20" fill="%s" stroke="%s" stroke-width="3"/>' % (face_fill, ink)
    out += '<circle cx="26" cy="26" r="6" fill="none" stroke="%s" stroke-width="3"/>' % ink
    if key == "clone":
        out += '<circle cx="42" cy="42" r="17" fill="%s" stroke="%s" stroke-width="3"/>' % (ink, ground)
        return out + '<circle cx="42" cy="42" r="5" fill="%s"/>' % ground
    glyph_colour = D["on-primary"] if dark else L["on-primary"]
    out += '<circle cx="46" cy="46" r="13.500" fill="%s" stroke="%s" stroke-width="3"/>' % (ink, ground)
    g = GLYPH[key].replace("FILL", 'fill="%s"' % glyph_colour)
    return out + ('<g transform="translate(46 46)" fill="none" stroke="%s" stroke-width="2.300" stroke-linecap="round" '
                  'stroke-linejoin="round">%s</g>') % (glyph_colour, g)


# ---------------------------------------------------------------- app icon, 256 grid
def icon(simple=False):
    out = '<path d="%s" fill="%s" fill-rule="evenodd"/>' % (circ(108, 108, 96) + circ(108, 108, 13), CANARY)
    if not simple:
        out += '<circle cx="108" cy="108" r="34" fill="none" stroke="%s" stroke-width="5"/>' % CANARY_DEEP
    out += '<circle cx="148" cy="148" r="103" fill="#fff"/>'
    out += '<path d="%s" fill="%s" fill-rule="evenodd"/>' % (circ(148, 148, 96) + circ(148, 148, 13), INK)
    out += '<path d="%s" fill="%s" fill-rule="evenodd"/>' % (circ(148, 148, 36) + circ(148, 148, 13), CARBON)
    if not simple:
        for r in (52, 66, 80):
            out += '<circle cx="148" cy="148" r="%d" fill="none" stroke="#fff" stroke-opacity=".13" stroke-width="2"/>' % r
        out += '<path d="M148 62a86 86 0 0 1 86 86" fill="none" stroke="%s" stroke-width="7" stroke-linecap="round" opacity=".9"/>' % CANARY
    return out


# ---------------------------------------------------------------- wordmark
def wordmark(dark=False, tagline=True):
    """Returns (inner svg, width, height). The canary copy sits 6 px down and right of 'CD'."""
    ink = D["primary"] if dark else INK
    glyphs, adv = face("display").run("CarbonCD", 2, 86, 92, -2.5)
    copy = "".join(g for g, c in glyphs if c >= 6)
    main = "".join(g for g, _ in glyphs)
    out = '<path d="%s" fill="%s" transform="translate(6 6)"/><path d="%s" fill="%s"/>' % (copy, CANARY, main, ink)
    if tagline:
        out += face("mono").text("ORIGINAL IN, COPY OUT", 5, 122, 15, ink, 3.5)
    return out, int(math.ceil(adv + 12)), 130 if tagline else 104


# ---------------------------------------------------------------- banners
def banner(W, H):
    big = H > 400
    cx, cy, r = (1080, 330, 265) if big else (1010, 110, 215)
    out = '<defs><clipPath id="clip"><rect width="%d" height="%d"/></clipPath></defs><g clip-path="url(#clip)">' % (W, H)
    out += '<rect width="%d" height="%d" fill="%s"/>' % (W, H, L["window"])
    out += '<path d="%s" stroke="%s" stroke-opacity=".05"/>' % ("".join("M%d 0v%d" % (i, H) for i in range(0, W, 32)), INK)
    out += '<path d="%s" fill="%s" fill-rule="evenodd"/>' % (circ(cx, cy, r) + circ(cx, cy, r * .13), CANARY)
    fx, fy = cx + r * .42, cy + r * .42
    out += '<circle cx="%s" cy="%s" r="%s" fill="%s"/>' % (_num(fx), _num(fy), _num(r * 1.07), L["window"])
    out += '<path d="%s" fill="%s" fill-rule="evenodd"/>' % (circ(fx, fy, r) + circ(fx, fy, r * .13), INK)
    out += '<path d="%s" fill="%s" fill-rule="evenodd"/>' % (circ(fx, fy, r * .37) + circ(fx, fy, r * .13), CARBON)
    for k in (.52, .66, .8, .93):
        out += '<circle cx="%s" cy="%s" r="%s" fill="none" stroke="#fff" stroke-opacity=".12" stroke-width="2"/>' % (_num(fx), _num(fy), _num(r * k))
    wy, sc = (150, 1.35) if big else (62, 1.0)
    out += '<g transform="translate(72 %d) scale(%s)">%s</g>' % (wy, sc, wordmark()[0])
    ty = wy + 130 * sc + (58 if big else 40)
    out += face("text").text(TAGLINE, 76, ty, 34 if big else 25, L["text"])
    if big:
        y = ty + 62
        mono = face("mono")
        out += '<rect x="76" y="%d" width="150" height="40" fill="none" stroke="%s" stroke-width="2"/>' % (y, INK)
        out += mono.text("ORIGINAL", 151, y + 26, 17, INK, 2, "middle")
        out += '<path d="M240 %dh46m-10-8l10 8-10 8" fill="none" stroke="%s" stroke-width="2.500"/>' % (y + 20, INK)
        out += '<rect x="300" y="%d" width="110" height="40" fill="%s"/>' % (y, CANARY)
        out += mono.text("COPY", 355, y + 26, 17, INK, 2, "middle")
        out += mono.text("2 295 104 sectors, bit for bit", 440, y + 26, 17, INK, 1)
    return out + "</g>"


def about_banner():
    wm = wordmark(tagline=False)[0]
    return ('<rect width="600" height="150" fill="%s"/><rect y="138" width="600" height="12" fill="%s"/>'
            '<svg x="24" y="13" width="112" height="112" viewBox="0 0 256 256">%s</svg>'
            '<g transform="translate(160 30) scale(.72)">%s</g>') % (L["window"], CANARY, icon(), wm)


# ---------------------------------------------------------------- interface mockups (HTML)
DARK = [False]  # set while a dark screen is being built


def _btn(key, cls):
    return svg_inline(button(key, DARK[0]), 64, 64, cls, LABEL[key])


def _chrome(title_extra=""):
    ctl = ('<span class="ctl"><svg viewBox="0 0 46 12" width="46" height="12" aria-hidden="true" fill="none" '
           'stroke="currentColor" stroke-width="1.200"><path d="M1 6h8M19 2h8v8h-8zM37 2l8 8M45 2l-8 8"/></svg></span>')
    return '<div class="tb">%s<b>CarbonCD</b>%s%s</div>' % (svg_inline(icon(True), 256, 256, "tbi", "CarbonCD"), title_extra, ctl)


def _rail(active, busy=False):
    out = ""
    for k in ("read", "write", "clone", "master", "tracks", "subcode"):
        out += '<div class="ri%s">%s<span>%s</span></div>' % (" on" if k == active else "", _btn(k, "ric"), LABEL[k])
    out += '<div class="rsp"></div>'
    for k in ("erase", "drive"):
        out += '<div class="ri%s">%s<span>%s</span></div>' % (" on" if k == active else "", _btn(k, "ric"), LABEL[k])
    out += '<div class="rf"><span>Settings</span><span>About</span></div>'
    return '<div class="rail%s">%s</div>' % (" busy" if busy else "", out)


DRIVE = ('<div class="drv"><div class="sel">E: &nbsp; HL-DT-ST BD-RE BH16NS40 <i>&#9662;</i></div><span class="chip y">DVD+R DL</span>'
         '<span class="chip">2 layers</span><span class="chip">6.31 GB used</span>%s</div>')


def mock_read():
    return f'''<div class="win">{_chrome()}
<div class="body">{_rail("read")}
<div class="pane">
 {DRIVE % '<span class="lnk">Recognise disc</span>'}
 <h4>Read disc to image</h4>
 <div class="row"><label>Save as</label><div class="inp">D:\\Images\\Backup-2026.iso</div><div class="bt">Browse</div></div>
 <div class="row"><label>Format</label><div class="seg"><span class="on">ISO</span><span>CUE / BIN</span><span>CCD</span></div><small>DVDs read to ISO. CUE and CCD are for CDs.</small></div>
 <div class="row"><label>Speed</label><div class="inp short">Maximum (8x) <i>&#9662;</i></div><span class="ck"><span class="bx on"></span>Verify after reading</span><span class="ck"><span class="bx"></span>Skip unreadable sectors</span></div>
 <div class="cc"><div class="cc1"><em>ORIGINAL</em><b>DVD+R DL in drive E:</b><span>3 080 112 sectors &#183; UDF 2.50 &#183; layer break at 1 540 056</span></div><div class="ccar">&#8594;</div><div class="cc2"><em>COPY</em><b>Backup-2026.iso</b><span>6.31 GB &#183; 412 GB free on D:</span></div></div>
 <div class="acts"><div class="bt pri">Read disc</div><div class="bt">Options</div></div>
</div></div>
<div class="sb"><span>Ready. Drive E: holds a DVD+R DL with 1 session.</span><span>Log &#9652;</span></div></div>'''


def mock_progress():
    cols, rows = T["size_px"]["map-columns"], T["size_px"]["map-rows-per-layer"]
    per = cols * rows
    done = int(per * 2 * 0.62)
    layers = ""
    for layer in range(2):
        cells = ""
        for i in range(per):
            n = layer * per + i
            cls = "bad" if n in (71, 72) else ("d" if n < done else ("n" if n == done else ""))
            cells += '<i class="%s"></i>' % cls
        layers += '<div class="maprow"><em>L%d</em><div class="map">%s</div></div>' % (layer, cells)
    return f'''<div class="win">{_chrome('<span class="tbm">Reading to Backup-2026.iso</span>')}
<div class="body">{_rail("read", True)}
<div class="pane">
 {DRIVE % ""}
 <h4>Reading disc</h4>
 <div class="cc"><div class="cc1"><em>ORIGINAL</em><b>DVD+R DL in drive E:</b><span>3 080 112 sectors &#183; UDF 2.50</span></div><div class="ccar">&#8594;</div><div class="cc2"><em>COPY</em><b>Backup-2026.iso</b><span>3.91 of 6.31 GB written</span></div></div>
 <div class="maphd"><span>DISC SURFACE &#183; 1 CELL = 7 000 SECTORS</span><span>read <u class="k1"></u> reading <u class="k2"></u> pending <u class="k3"></u> unreadable <u class="k4"></u></span></div>
 {layers}
 <div class="bar"><div style="width:62%"></div></div>
 <div class="stats"><div><em>LBA</em><b>1 909 669</b></div><div><em>RATE</em><b>11.08 MB/s</b></div><div><em>ELAPSED</em><b>05:53</b></div><div><em>REMAINING</em><b>03:36</b></div><div><em>UNREADABLE</em><b class="err">2 cells</b></div></div>
 <div class="acts"><div class="bt pri">Stop</div><div class="bt">Show log</div></div>
</div></div>
<div class="sb"><span>READ(12) LBA 1 909 664 length 32 ok</span><span>Log &#9652;</span></div></div>'''


MOCK_CSS_T = '''
.win{width:820px;border-radius:8px;overflow:hidden;box-shadow:0 18px 50px rgba(20,23,43,.3);font:12.500px/1.35 'Segoe UI Variable Text','Segoe UI',sans-serif;display:flex;flex-direction:column;background:%(window)s;color:%(text)s}
.tb{display:flex;align-items:center;gap:8px;height:34px;padding:0 12px;flex:none;background:%(primary)s;color:%(on-primary)s}
.tb b{font-weight:600}.tbi{width:18px;height:18px}.ctl{margin-left:auto;display:flex;opacity:.75}.tbm{opacity:.7;margin-left:6px}
.body{display:flex;min-height:430px}
.rail{width:168px;flex:none;background:%(rail)s;border-right:1px solid %(line-soft)s;padding:10px 8px;display:flex;flex-direction:column;gap:2px}
.ri{display:flex;align-items:center;gap:9px;height:40px;padding:0 8px;border-radius:6px;font-weight:500}
.ri.on{background:%(surface)s;box-shadow:inset 3px 0 0 %(primary)s,0 1px 2px rgba(30,42,120,.12);font-weight:700}
.rail.busy .ri:not(.on){opacity:.4}
.ric{width:28px;height:28px;flex:none}.rsp{flex:1}
.rf{display:flex;gap:16px;padding:10px 8px 2px;margin-top:6px;border-top:1px solid %(line-soft)s;font-weight:600;font-size:12px;color:%(link)s}
.pane{flex:1;padding:16px 22px 20px;display:flex;flex-direction:column;gap:13px;min-width:0}
.drv{display:flex;align-items:center;gap:8px;padding-bottom:13px;border-bottom:1px dashed %(line)s}
.sel,.inp{display:flex;align-items:center;justify-content:space-between;gap:8px;height:30px;padding:0 10px;border-radius:5px;white-space:nowrap;min-width:0;flex:1;background:%(surface)s;border:1px solid %(line)s}
.sel i,.inp i{font-style:normal;opacity:.6}.inp.short{flex:none;width:150px}
.chip{font:500 11px 'IBM Plex Mono',Consolas,monospace;padding:4px 8px;border:1px solid %(primary)s;border-radius:3px;white-space:nowrap}
.chip.y{background:%(accent)s;color:%(on-accent)s}
.lnk{color:%(link)s;font-weight:600;white-space:nowrap;text-decoration:underline}
.win h4{margin:2px 0 0;font:800 23px 'Bricolage Grotesque','Segoe UI',sans-serif;letter-spacing:-.01em;color:%(primary)s}
.row{display:flex;align-items:center;gap:10px}
.row>label{width:62px;flex:none;font:500 11px 'IBM Plex Mono',Consolas,monospace;letter-spacing:.06em;text-transform:uppercase;color:%(text-muted)s}
.row small{color:%(text-muted)s;font-size:11.500px}
.seg{display:flex;border:1px solid %(primary)s;border-radius:5px;overflow:hidden;flex:none}
.seg span{padding:6px 12px;font-weight:600;border-right:1px solid %(primary)s}.seg span:last-child{border:0}
.seg .on{background:%(primary)s;color:%(on-primary)s}
.ck{display:flex;align-items:center;gap:7px;white-space:nowrap}
.bx{width:15px;height:15px;border-radius:3px;display:inline-block;flex:none;border:1.500px solid %(primary)s;background:%(surface)s}
.bx.on{background:%(primary)s;box-shadow:inset 0 0 0 2.500px %(surface)s}
.bt{height:30px;padding:0 16px;border-radius:5px;display:flex;align-items:center;justify-content:center;font-weight:600;white-space:nowrap;border:1px solid %(primary)s;background:%(surface)s;color:%(primary)s}
.bt.pri{background:%(primary)s;color:%(on-primary)s;box-shadow:4px 4px 0 %(accent)s;padding:0 26px}
.cc{display:flex;align-items:stretch}
.cc1,.cc2{flex:1;padding:10px 12px;display:flex;flex-direction:column;gap:2px;min-width:0;border:1.500px solid %(primary)s}
.cc1{background:%(surface)s}.cc2{background:%(accent)s;color:%(on-accent)s}
.cc em{font:500 10px 'IBM Plex Mono',Consolas,monospace;letter-spacing:.14em;font-style:normal}
.cc span{font:11px 'IBM Plex Mono',Consolas,monospace}
.ccar{display:flex;align-items:center;padding:0 10px;font-size:22px;color:%(primary)s}
.acts{display:flex;gap:8px;margin-top:auto;padding-top:4px}
.sb{display:flex;justify-content:space-between;padding:6px 12px;font-size:11.500px;flex:none;background:%(rail)s;border-top:1px solid %(line-soft)s;color:%(text-muted)s}
.maphd{display:flex;justify-content:space-between;gap:12px;font:500 10px 'IBM Plex Mono',Consolas,monospace;letter-spacing:.08em;color:%(text-muted)s}
.maphd u{display:inline-block;width:9px;height:9px;margin:0 8px 0 3px;text-decoration:none;vertical-align:-1px;border-radius:1.500px}
.k1{background:%(map-done)s}.k2{background:%(map-active)s;outline:1px solid %(primary)s}.k3{background:%(map-pending)s}.k4{background:%(map-bad)s}
.maprow{display:flex;align-items:center;gap:8px;margin-top:-6px}
.maprow em{font:500 10px 'IBM Plex Mono',Consolas,monospace;font-style:normal;color:%(text-muted)s;width:16px;flex:none}
.map{flex:1;display:grid;grid-template-columns:repeat(44,1fr);gap:2px}
.map i{aspect-ratio:1;background:%(map-pending)s;border-radius:1.500px}
.map i.d{background:%(map-done)s}.map i.n{background:%(map-active)s;outline:1.500px solid %(primary)s}.map i.bad{background:%(map-bad)s}
.bar{height:6px;background:%(map-pending)s;border-radius:3px;overflow:hidden}.bar div{height:100%%;background:%(primary)s}
.stats{display:flex;gap:26px}
.stats em{display:block;font:500 9.500px 'IBM Plex Mono',Consolas,monospace;letter-spacing:.12em;font-style:normal;color:%(text-muted)s}
.stats b{font:500 16px 'IBM Plex Mono',Consolas,monospace}.stats .err{color:%(error)s}
'''

MOCK_CSS_MORE = '''
.tabs{display:flex;gap:18px;border-bottom:1px solid %(line)s}
.tabs span{padding:0 0 8px;font-weight:600;color:%(text-muted)s}
.tabs .on{color:%(primary)s;box-shadow:inset 0 -3px 0 %(primary)s}
.kv{display:grid;grid-template-columns:150px 1fr;gap:7px 12px;align-items:baseline}
.kv em,.lbl{font:500 11px 'IBM Plex Mono',Consolas,monospace;letter-spacing:.06em;text-transform:uppercase;font-style:normal;color:%(text-muted)s}
.kv b{font:500 12.500px 'IBM Plex Mono',Consolas,monospace}
.tbl{border:1px solid %(line)s;border-radius:5px;background:%(surface)s;overflow:hidden}
.tbl>div{display:grid;gap:8px;padding:6px 10px;border-bottom:1px solid %(line-soft)s;align-items:center;font:12px 'IBM Plex Mono',Consolas,monospace}
.tbl>div:last-child{border-bottom:0}
.tbl .hd{font:500 10px 'IBM Plex Mono',Consolas,monospace;letter-spacing:.08em;text-transform:uppercase;color:%(text-muted)s;background:%(rail)s}
.c5{grid-template-columns:20px 30px 70px 1fr 1fr 90px}
.c4{grid-template-columns:96px 54px 54px 1fr}
.c3{grid-template-columns:70px 100px 1fr}
.capbar{height:10px;background:%(map-pending)s;border-radius:5px;overflow:hidden}.capbar div{height:100%%;background:%(primary)s}
.caprow{display:flex;justify-content:space-between;font:11px 'IBM Plex Mono',Consolas,monospace;color:%(text-muted)s;margin-top:-8px}
.files{display:grid;grid-template-columns:170px 1fr;gap:8px;height:132px}
.files>div{border:1px solid %(line)s;border-radius:5px;background:%(surface)s;padding:7px 9px;font-size:12px;display:flex;flex-direction:column;gap:5px;overflow:hidden}
.frow{display:flex;justify-content:space-between;gap:8px}.frow span:last-child{font:11.500px 'IBM Plex Mono',Consolas,monospace;color:%(text-muted)s}
.frow.on{font-weight:700;color:%(primary)s}
.lose{border:1.500px solid %(error)s;background:%(surface)s;padding:10px 12px;display:flex;flex-direction:column;gap:2px}
.lose em{font:500 10px 'IBM Plex Mono',Consolas,monospace;letter-spacing:.14em;font-style:normal;color:%(error)s}
.lose span{font:11px 'IBM Plex Mono',Consolas,monospace}
.note{color:%(text-muted)s;margin:0;max-width:none}
.okc{color:%(ok)s}.warnc{color:%(warn)s}.errc{color:%(error)s}
.reason{color:%(text-muted)s;font-size:11.500px;align-self:center}
.dis{opacity:.4}
.drawer{background:%(surface)s;border-top:1px solid %(line)s;padding:8px 12px;font:11px/1.7 'IBM Plex Mono',Consolas,monospace;flex:none}
.drawer .hdr{display:flex;gap:14px;align-items:center;font:500 10px 'IBM Plex Mono',Consolas,monospace;letter-spacing:.08em;text-transform:uppercase;color:%(text-muted)s;margin-bottom:4px}
.drawer .hdr b{color:%(primary)s}.drawer .hdr i{margin-left:auto;font-style:normal;color:%(link)s;text-decoration:underline}
.drawer p{margin:0;max-width:none;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.drawer s{text-decoration:none;color:%(text-muted)s;margin-right:8px}
.dbody{padding:18px 20px 20px;display:flex;flex-direction:column;gap:13px}
.dacts{display:flex;justify-content:flex-end;gap:8px;margin-top:6px}
.dbody h5{margin:0;font:800 19px 'Bricolage Grotesque','Segoe UI',sans-serif;color:%(primary)s}
.msf{display:grid;grid-template-columns:90px 1fr;gap:9px 12px;align-items:center}
.msf .three{display:flex;gap:6px}.msf .inp{font:12.500px 'IBM Plex Mono',Consolas,monospace;justify-content:flex-end}
.abt{display:flex;flex-direction:column;gap:6px}
'''


def mock_css(palette):
    return (MOCK_CSS_T + MOCK_CSS_MORE) % palette


MOCK_CSS = mock_css(L)

CHIPS_DVD = (("DVD+R DL", 1), ("2 layers", 0), ("6.31 GB used", 0))
CHIPS_BLANK = (("DVD+R DL", 1), ("Blank", 0), ("8.55 GB free", 0))
CHIPS_CD = (("CD-ROM", 1), ("1 session, 6 tracks", 0), ("52:14:33", 0))
CHIPS_RW = (("DVD-RW", 1), ("1 layer", 0), ("3.10 GB used", 0))


def _drive(chips, link=True):
    out = '<div class="drv"><div class="sel">E: &nbsp; HL-DT-ST BD-RE BH16NS40 <i>&#9662;</i></div>'
    out += "".join('<span class="chip%s">%s</span>' % (" y" if y else "", t) for t, y in chips)
    return out + ('<span class="lnk">Recognise disc</span>' if link else "") + "</div>"


def _pane(active, inner, status, extra="", busy=False, drawer=""):
    return ('<div class="win">%s<div class="body">%s<div class="pane">%s</div></div>%s'
            '<div class="sb"><span>%s</span><span>Log &#9652;</span></div></div>') % (
        _chrome(extra), _rail(active, busy), inner, drawer, status)


def _dialog(title, width, inner):
    ctl = ('<span class="ctl"><svg viewBox="0 0 12 12" width="12" height="12" aria-hidden="true" fill="none" '
           'stroke="currentColor" stroke-width="1.200"><path d="M2 2l8 8M10 2l-8 8"/></svg></span>')
    return '<div class="win" style="width:%dpx"><div class="tb"><b>%s</b>%s</div><div class="dbody">%s</div></div>' % (width, title, ctl, inner)


def _row(label, html):
    return '<div class="row"><label>%s</label>%s</div>' % (label, html)


def _ck(text, on=False, dis=False):
    return '<span class="ck%s"><span class="bx%s"></span>%s</span>' % (" dis" if dis else "", " on" if on else "", text)


def _cards(o_title, o_detail, c_title, c_detail):
    return ('<div class="cc"><div class="cc1"><em>ORIGINAL</em><b>%s</b><span>%s</span></div><div class="ccar">&#8594;</div>'
            '<div class="cc2"><em>COPY</em><b>%s</b><span>%s</span></div></div>') % (o_title, o_detail, c_title, c_detail)


def _sel(text, cls=""):
    return '<div class="inp %s">%s <i>&#9662;</i></div>' % (cls, text)


def _map(done_fraction, bad=(), layers=2, all_done=False):
    cols, rows = T["size_px"]["map-columns"], T["size_px"]["map-rows-per-layer"]
    per = cols * rows
    done = per * layers if all_done else int(per * layers * done_fraction)
    out = ""
    for layer in range(layers):
        cells = ""
        for i in range(per):
            n = layer * per + i
            cells += '<i class="%s"></i>' % ("bad" if n in bad else ("d" if n < done else ("n" if n == done else "")))
        out += '<div class="maprow"><em>L%d</em><div class="map">%s</div></div>' % (layer, cells)
    return out


def mock_write():
    return _pane("write", _drive(CHIPS_BLANK) + '<h4>Write image</h4>'
                 + _row("Image", '<div class="inp">D:\\Images\\Backup-2026.iso</div><div class="bt">Browse</div>')
                 + _row("Speed", _sel("8x (11.08 MB/s)", "short") + '<label style="width:auto">Method</label>' + _sel("Automatic", "short"))
                 + _row("Layer", _sel("Automatic", "short") + '<small>Layer break. Dual-layer discs only.</small>')
                 + _row("Options", _ck("Buffer underrun protection", True) + _ck("Verify after writing", True) + _ck("Eject when done", True))
                 + _row("", _ck("Test mode", dis=True) + _ck("Power calibration", dis=True) + '<small>DVD+R has no test write or calibration.</small>')
                 + _cards("Backup-2026.iso", "6.31 GB &#183; 3 080 112 sectors &#183; UDF 2.50", "DVD+R DL in drive E:", "Holds 8.55 GB &#183; the image needs 6.31 GB")
                 + '<div class="acts"><div class="bt pri">Write image</div></div>',
                 "Ready. Drive E: holds a blank DVD+R DL.")


def mock_clone():
    return _pane("clone", _drive(CHIPS_DVD) + '<h4>Clone disc</h4>'
                 + _row("Target", _sel("F: &nbsp; ASUS DRW-24D5MT"))
                 + _row("Temp", '<div class="inp">D:\\Temp</div><div class="bt">Browse</div>')
                 + _row("", '<small>Needs 6.31 GB &#183; 412 GB free on D:</small>')
                 + _row("Speed", '<label style="width:auto">Read</label>' + _sel("Maximum", "short") + '<label style="width:auto">Write</label>' + _sel("8x", "short"))
                 + _row("Options", _ck("Verify", True) + _ck("Eject when done", True) + _ck("Skip unreadable sectors"))
                 + _cards("DVD+R DL in drive E:", "3 080 112 sectors &#183; UDF 2.50", "Blank DVD+R DL in drive F:", "Holds 8.55 GB &#183; through a temporary image")
                 + '<div class="acts"><div class="bt pri">Clone disc</div></div>',
                 "Ready. Source in E:, blank disc in F:.")


def mock_master():
    tree = ('<div><div class="frow on"><span>BACKUP_2026</span></div><div class="frow"><span>&nbsp; Documents</span></div>'
            '<div class="frow"><span>&nbsp; Photos</span></div><div class="frow"><span>&nbsp; &nbsp; 2026</span></div><div class="frow"><span>&nbsp; Video</span></div></div>')
    files = ('<div><div class="frow"><span>Documents</span><span>412 files</span></div><div class="frow"><span>Photos</span><span>788 files</span></div>'
             '<div class="frow"><span>Video</span><span>3 files</span></div><div class="frow"><span>archive-2025.zip</span><span>1.20 GB</span></div>'
             '<div class="frow"><span>readme.txt</span><span>2.10 KB</span></div></div>')
    return _pane("master", _drive(CHIPS_BLANK) + '<h4>Master disc</h4>'
                 + _row("Type", '<div class="seg"><span>Data CD</span><span>Audio CD</span><span>Mixed CD</span><span class="on">Data DVD</span></div>'
                        + '<label style="width:auto">Label</label><div class="inp">BACKUP_2026</div>')
                 + _row("Files", '<div class="bt">Add files</div><div class="bt">Add folder</div><div class="bt">New folder</div><div class="bt">Rename</div><div class="bt">Remove</div>')
                 + '<div class="files">%s%s</div>' % (tree, files)
                 + '<div class="capbar"><div style="width:45%"></div></div>'
                 + '<div class="caprow"><span>3.82 GB of 8.55 GB &#183; 1 865 234 of 4 173 824 sectors</span><span>ISO 9660 + Joliet + UDF 1.02</span></div>'
                 + _cards("1 204 files", "3.82 GB &#183; largest file 1.20 GB", "DVD+R DL in drive E:", "4.73 GB will stay free")
                 + '<div class="acts"><div class="bt pri">Write to disc</div><div class="bt">Create image</div><div class="bt">Options</div></div>',
                 "Ready. 1 204 files in the project.")


def mock_tracks():
    rows = '<div class="hd c5"><span></span><span>No</span><span>Type</span><span>Start</span><span>LBA</span><span>Length</span></div>'
    data = (("01", "Data", "00:02:00", "0", "21:40:12", 1), ("02", "Audio", "21:42:12", "97 512", "04:12:33", 1), ("03", "Audio", "25:54:45", "116 445", "03:58:10", 1),
            ("04", "Audio", "29:52:55", "134 305", "05:01:62", 0), ("05", "Audio", "34:54:42", "156 942", "04:40:08", 0), ("06", "Audio", "39:34:50", "177 950", "12:39:58", 0))
    for no, typ, start, lba, length, on in data:
        rows += '<div class="c5"><span class="bx%s"></span><span>%s</span><span>%s</span><span>%s</span><span>%s</span><span>%s</span></div>' % (
            " on" if on else "", no, typ, start, lba, length)
    return _pane("tracks", _drive(CHIPS_CD) + '<h4>Read tracks</h4><div class="tbl">%s</div>' % rows
                 + _row("Folder", '<div class="inp">D:\\Music\\Album</div><div class="bt">Browse</div>')
                 + '<div class="acts"><div class="bt pri">Read tracks</div><div class="bt">Options</div><span class="reason">3 tracks: audio as 02.wav, 03.wav; data as 01.iso</span></div>',
                 "Ready. Drive E: holds a CD with 6 tracks.")


def mock_subcode():
    rows = '<div class="hd c3"><span>Track</span><span>Index</span><span>Start</span></div>'
    for t, i, s in (("01", "01", "00:02:00"), ("02", "00", "21:40:12"), ("02", "01", "21:42:12"), ("03", "00", "25:52:45"), ("03", "01", "25:54:45")):
        rows += '<div class="c3"><span>%s</span><span>%s</span><span>%s</span></div>' % (t, i, s)
    return _pane("subcode", _drive(CHIPS_CD) + '<h4>Subcode</h4>'
                 + _row("Method", '<b style="font:500 12.500px \'IBM Plex Mono\',Consolas,monospace">Raw 96 &#183; 8x</b><span class="lnk">Change</span>')
                 + '<p class="note">The accurate table of contents, read from the Q channel:</p><div class="tbl">%s</div>' % rows
                 + '<div class="acts"><div class="bt pri">Analyse subcode</div><div class="bt">Save as cue sheet</div><span class="lnk" style="align-self:center">MSF calculator</span></div>',
                 "Subcode analysed. 2 pregaps found.")


def mock_erase():
    return _pane("erase", _drive(CHIPS_RW) + '<h4>Erase disc</h4>'
                 + _row("Method", '<div class="seg"><span class="on">Quick</span><span>Full</span></div><small>Quick takes about a minute. Full writes the whole disc and cannot be stopped.</small>')
                 + '<div class="lose"><em>WILL BE LOST</em><b>PHOTOS</b><span>3.10 GB in use &#183; 1 session</span></div>'
                 + '<div class="acts"><div class="bt pri">Erase disc</div></div>',
                 "Ready. Drive E: holds a DVD-RW.")


def mock_drive():
    rows = '<div class="hd c4"><span>Media</span><span>Reads</span><span>Writes</span><span>Write speeds</span></div>'
    for m, r, w, s in (("CD-R", "Yes", "Yes", "16x, 24x, 32x, 48x"), ("CD-RW", "Yes", "Yes", "10x, 16x, 24x"), ("DVD-R", "Yes", "Yes", "4x, 6x, 8x, 16x"),
                       ("DVD+R DL", "Yes", "Yes", "2.4x, 4x, 6x, 8x"), ("DVD-RAM", "Yes", "Yes", "3x, 5x"), ("BD", "Yes", "Not supported", "")):
        rows += '<div class="c4"><span>%s</span><span>%s</span><span>%s</span><span>%s</span></div>' % (m, r, w, s)
    return _pane("drive", _drive(CHIPS_DVD) + '<h4>Drive info</h4><div class="tabs"><span class="on">Drive</span><span>Disc</span></div>'
                 + '<div class="kv"><em>Vendor</em><b>HL-DT-ST</b><em>Model</em><b>BD-RE BH16NS40</b><em>Firmware</em><b>1.05</b><em>Buffer</em><b>4 096 KB</b>'
                   '<em>CD write modes</em><b>SAO, RAW+16, RAW+96 (each with test write)</b></div>'
                 + '<div class="tbl">%s</div>' % rows,
                 "Ready.")


def mock_disc():
    return _pane("drive", _drive(CHIPS_DVD) + '<h4>Drive info</h4><div class="tabs"><span>Drive</span><span class="on">Disc</span></div>'
                 + '<div class="kv"><em>Type</em><b>DVD+R DL</b><em>Book type</em><b>DVD+R DL</b><em>Layers</em><b>2</b><em>Layer break</em><b>1 540 056</b>'
                   '<em>Sessions</em><b>1, closed</b><em>Capacity</em><b>4 173 824 sectors &#183; 8.55 GB</b><em>In use</em><b>3 080 112 sectors &#183; 6.31 GB</b>'
                   '<em>File system</em><b>UDF 2.50</b><em>Label</em><b>BACKUP_2026</b></div>'
                 + '<div class="acts"><div class="bt">Export</div><span class="reason">Text, HTML or CSV. A cue sheet for CDs.</span></div>',
                 "Ready.")


def mock_result():
    return _pane("read", _drive(CHIPS_DVD, False) + '<h4 class="warnc">Done with unreadable sectors</h4>'
                 + '<p class="note">The disc was read. 2 sectors could not be read and were written as zeros. The log lists them.</p>'
                 + _cards("DVD+R DL in drive E:", "3 080 112 sectors &#183; 2 unreadable", "Backup-2026.iso", "6.31 GB &#183; verified against the disc")
                 + '<div class="maphd"><span>DISC SURFACE &#183; 1 CELL = 7 000 SECTORS</span><span>read <u class="k1"></u> unreadable <u class="k4"></u></span></div>'
                 + _map(1, (71, 72), all_done=True)
                 + '<div class="stats"><div><em>READ</em><b>6.31 GB</b></div><div><em>TIME</em><b>09:29</b></div><div><em>AVERAGE</em><b>11.08 MB/s</b></div><div><em>UNREADABLE</em><b class="err">2 sectors</b></div></div>'
                 + '<div class="acts"><div class="bt pri">Close</div><div class="bt">Open folder</div><div class="bt">Eject</div><div class="bt">Save log</div><div class="bt">Do it again</div></div>',
                 "Done. 6.31 GB read, 2 sectors unreadable.", ' <span class="tbm">Done</span>')


def mock_log():
    lines = (("14:02:07", "", "GET CONFIGURATION: current profile 002Bh DVD+R DL"), ("14:02:08", "", "READ CAPACITY: 3 080 112 sectors of 2048 bytes"),
             ("14:05:31", "warnc", "READ(12) LBA 497 104 length 32: medium error 03 11 05, retrying sector by sector"),
             ("14:05:36", "errc", "Sector 497 112 unreadable after 3 tries, written as zeros"),
             ("14:05:41", "errc", "Sector 497 113 unreadable after 3 tries, written as zeros"), ("14:08:00", "", "READ(12) LBA 1 909 664 length 32 ok"))
    body = "".join('<p class="%s"><s>%s</s>%s</p>' % (c, t, x) for t, c, x in lines)
    drawer = '<div class="drawer"><div class="hdr"><b>All</b><span>Warnings and errors</span><i>Save log</i><span>Clear</span></div>%s</div>' % body
    inner = (_drive(CHIPS_DVD, False) + '<h4>Reading disc</h4>'
             + _cards("DVD+R DL in drive E:", "3 080 112 sectors &#183; UDF 2.50", "Backup-2026.iso", "3.91 of 6.31 GB written")
             + '<div class="maphd"><span>DISC SURFACE &#183; 1 CELL = 7 000 SECTORS</span><span>read <u class="k1"></u> reading <u class="k2"></u> pending <u class="k3"></u> unreadable <u class="k4"></u></span></div>'
             + _map(0.62, (71, 72)) + '<div class="bar"><div style="width:62%"></div></div>'
             + '<div class="acts"><div class="bt pri">Stop</div><div class="bt">Hide log</div></div>')
    return _pane("read", inner, "READ(12) LBA 1 909 664 length 32 ok", ' <span class="tbm">Reading to Backup-2026.iso</span>', True, drawer)


def mock_settings():
    return _dialog("Settings", 520, '<div class="tabs"><span class="on">General</span><span>Sounds</span><span>Drive</span><span>Subcode</span></div>'
                   + _row("Language", _sel("English", "short") + '<span class="lnk">More languages</span>')
                   + _row("Theme", '<div class="seg"><span class="on">Follow Windows</span><span>Light</span><span>Dark</span></div>')
                   + _row("Temp", '<div class="inp">D:\\Temp</div><div class="bt">Browse</div>')
                   + _row("Log", _ck("Save the log to a file after each task", True))
                   + _row("", '<div class="inp">D:\\Logs\\CarbonCD.log</div><div class="bt">Browse</div>')
                   + _row("Priority", _sel("Normal", "short"))
                   + '<div class="dacts"><div class="bt">Cancel</div><div class="bt pri">Save</div></div>')


def mock_about():
    return _dialog("About CarbonCD", 480, '<div style="margin:-18px -20px 0;line-height:0">%s</div>' % svg_inline(about_banner(), 600, 150, "banner", "About banner")
                   + '<div class="abt"><b>CarbonCD 2.0.0</b><span class="lbl">64-bit &#183; build 2026.11.16</span>'
                     '<span>Copyright 2017 to 2026 Rizonesoft. MIT licence.</span>'
                     '<p class="note">Please do not use this program to make illegal copies of copyrighted work.</p></div>'
                   + '<div class="dacts"><span class="lnk" style="margin-right:auto;align-self:center">Rizonesoft Online</span><div class="bt">Help</div><div class="bt">Copy version</div><div class="bt pri">Close</div></div>')


def mock_language():
    rows = '<div class="hd c3" style="grid-template-columns:1fr 1fr"><span>Language</span><span>Author</span></div>'
    for lang, who, on in (("English", "Rizonesoft", 1), ("Fran&#231;ais", "Rizonesoft", 0), ("&#54620;&#44397;&#50612;", "VenusGirl", 0)):
        rows += '<div style="grid-template-columns:1fr 1fr;%s"><span>%s</span><span>%s</span></div>' % ("font-weight:700" if on else "", lang, who)
    return _dialog("Language", 420, '<div class="tbl">%s</div><div class="dacts"><div class="bt">Cancel</div><div class="bt pri">Use this language</div></div>' % rows)


def mock_message():
    return _dialog("Erase disc", 420, '<h5>Erase this disc?</h5><p class="note">Everything on it will be lost.</p>'
                   '<div class="dacts"><div class="bt pri">Cancel</div><div class="bt">Erase disc</div></div>')


def mock_msf():
    three = '<div class="three"><div class="inp">%s</div><div class="inp">%s</div><div class="inp">%s</div></div>'
    return _dialog("MSF calculator", 360, '<div class="msf"><span class="lbl">MSF</span>' + three % ("00", "02", "00")
                   + '<span class="lbl">MSF + 150</span>' + three % ("00", "04", "00")
                   + '<span class="lbl">LBA</span><div class="inp">0</div><span class="lbl">LBA hex</span><div class="inp">0</div></div>'
                   + '<div class="dacts"><div class="bt">Zero</div><div class="bt pri">Close</div></div>')


# name, builder, canvas height, the SPEC section it is the reference for
SCREENS = [
    ("main-read", mock_read, 540, "S1, S2, S3"), ("write", mock_write, 580, "S4"), ("clone", mock_clone, 580, "S5"),
    ("master", mock_master, 660, "S6"), ("tracks", mock_tracks, 580, "S7"), ("subcode", mock_subcode, 580, "S8"),
    ("erase", mock_erase, 540, "S9"), ("drive-info", mock_drive, 620, "S10.1"), ("drive-info-disc", mock_disc, 560, "S10.2"),
    ("progress-surface-map", mock_progress, 640, "S11"), ("result", mock_result, 640, "S12"), ("log-drawer", mock_log, 700, "S17"),
    ("settings", mock_settings, 440, "S14.1"), ("about", mock_about, 420, "S16"), ("language", mock_language, 300, "S15"),
    ("message", mock_message, 240, "S13"), ("msf-calculator", mock_msf, 320, "S10.3"),
]


def screen_html(builder, dark):
    DARK[0] = dark
    try:
        return builder()
    finally:
        DARK[0] = False


FONT_LINK = ("https://fonts.googleapis.com/css2?family=Bricolage+Grotesque:wght@800&amp;family=IBM+Plex+Mono:wght@400;500"
             "&amp;family=Instrument+Sans:wght@400;500;600;700&amp;display=swap")

SHEET_CSS = '''
/* Layout: one reading column of specimens; brand pieces sit on fixed light and dark swatches. */
:root{--bg:#F1F3F6;--card:#FFFFFF;--fg:#171A21;--muted:#586170;--line:#D8DDE5;--mark:#1E2A78;
 --sans:'Instrument Sans','Segoe UI',system-ui,sans-serif;--mono:'IBM Plex Mono',Consolas,monospace}
@media (prefers-color-scheme:dark){:root:not([data-theme="light"]){--bg:#0E1022;--card:#181C3A;--fg:#E8EAFB;--muted:#A4ABD6;--line:#2E3566;--mark:#AAB6FF;color-scheme:dark}}
:root[data-theme="dark"]{--bg:#0E1022;--card:#181C3A;--fg:#E8EAFB;--muted:#A4ABD6;--line:#2E3566;--mark:#AAB6FF;color-scheme:dark}
body{background:var(--bg);color:var(--fg);font:15px/1.55 var(--sans);padding-block:40px 80px;padding-left:max(16px,env(safe-area-inset-left,0px));padding-right:max(16px,env(safe-area-inset-right,0px))}
.wrap{max-width:1040px;margin:0 auto;display:flex;flex-direction:column}
h1{font-size:clamp(28px,5vw,42px);line-height:1.08;letter-spacing:-.02em;margin:0 0 10px;text-wrap:balance}
h2{font-size:13px;letter-spacing:.09em;text-transform:uppercase;color:var(--muted);margin:36px 0 10px;font-weight:600;border-top:2px solid var(--mark);padding-top:14px}
h2 small{letter-spacing:0;text-transform:none;font-weight:400}
p{margin:0;max-width:66ch;color:var(--muted)}
.eyebrow{font:500 12px var(--mono);letter-spacing:.14em;text-transform:uppercase;margin-bottom:8px}
.pair{display:grid;grid-template-columns:1fr 1fr;gap:12px}
.sp{border-radius:10px;padding:26px;display:flex;align-items:center;justify-content:center;min-width:0;border:1px solid var(--line)}
.sp.l{background:%(lw)s;color:#30343B}.sp.dk{background:%(dw)s;color:#C9CEEE}
.wm{width:100%%;max-width:400px;height:auto}
.icons{gap:18px;align-items:flex-end;flex-wrap:wrap}.icons span{display:block;line-height:0}.icons svg{width:100%%;height:auto}
.btns{display:grid;grid-template-columns:repeat(5,1fr);gap:16px 6px;padding:20px 12px}
.btns figure{margin:0;display:flex;flex-direction:column;align-items:center;gap:6px;min-width:0}
.btns figcaption{font-size:11px;text-align:center;line-height:1.2}.btn{width:56px;height:56px}
.frame{border:1px solid var(--line);border-radius:10px;overflow:hidden;line-height:0}
.frame.narrow{max-width:760px}.frame.about{max-width:600px}.banner{width:100%%;height:auto;display:block}
.scroll{overflow-x:auto;border:1px solid var(--line);border-radius:10px;margin-top:12px}
.stage{min-width:890px;padding:34px;display:flex;justify-content:center;background:#DCE1F2}
.tok{display:grid;grid-template-columns:repeat(auto-fill,minmax(150px,1fr));gap:8px}
.tok div{display:flex;align-items:center;gap:8px;font-size:12.500px;min-width:0}
.tok i{width:22px;height:22px;border-radius:50%%;border:1px solid var(--line);flex:none}
.tok code{font:11px var(--mono);color:var(--muted);display:block}
h3{font-size:14px;margin:16px 0 8px}
@media (max-width:720px){.pair{grid-template-columns:1fr}.btn{width:44px;height:44px}}
''' % {"lw": L["window"], "dw": D["window"]}


def sheet():
    wl, ww, wh = wordmark()
    wd = wordmark(True)[0]
    sizes = "".join('<span style="width:%dpx">%s</span>' % (s, svg_inline(icon(s < 48), 256, 256, "", "CarbonCD app icon"))
                    for s in (128, 64, 48, 32, 16))
    bl = "".join('<figure>%s<figcaption>%s</figcaption></figure>' % (svg_inline(button(k), 64, 64, "btn", LABEL[k]), LABEL[k]) for k in KEYS)
    bd = "".join('<figure>%s<figcaption>%s</figcaption></figure>' % (svg_inline(button(k, True), 64, 64, "btn", LABEL[k]), LABEL[k]) for k in KEYS)

    def toks(group):
        return "".join('<div><i style="background:%s"></i><span>%s<code>%s</code></span></div>' % (v, k, v) for k, v in group.items())

    screens = "".join('<h3 id="screen-%s">%s <small style="font-weight:400">SPEC %s</small></h3><div class="scroll"><div class="stage">%s</div></div>'
                      % (name, name.replace("-", " ").capitalize(), spec, builder()) for name, builder, _, spec in SCREENS)
    return f'''<title>CarbonCD Design Sheet</title>
<meta name="claude:viewport-fit-sides" content="cover">
<link rel="stylesheet" href="{FONT_LINK}">
<style>{SHEET_CSS}{MOCK_CSS}</style>
<div class="wrap">
<p class="eyebrow">CarbonCD 2 &#183; approved identity &#183; Carbon Copy</p>
<h1>An original disc and its canary copy</h1>
<p>The approved direction, with the disc surface map added to every long operation. This sheet is generated by design/build.py from design/tokens.json; the binding rules are in design/DESIGN.md and the screens in design/SPEC.md.</p>
<h2>Wordmark</h2>
<div class="pair"><div class="sp l">{svg_inline(wl, ww, wh, "wm", "CarbonCD wordmark")}</div><div class="sp dk">{svg_inline(wd, ww, wh, "wm", "CarbonCD wordmark")}</div></div>
<h2>App icon</h2>
<div class="pair"><div class="sp l icons">{sizes}</div><div class="sp dk icons">{sizes}</div></div>
<h2>Button graphics</h2>
<div class="pair"><div class="sp l btns">{bl}</div><div class="sp dk btns">{bd}</div></div>
<h2>GitHub banner <small>1280 by 320</small></h2>
<div class="frame">{svg_inline(banner(1280, 320), 1280, 320, "banner", "GitHub banner")}</div>
<h2>Social share banner <small>1280 by 640</small></h2>
<div class="frame narrow">{svg_inline(banner(1280, 640), 1280, 640, "banner", "Social share banner")}</div>
<h2>About banner <small>600 by 150</small></h2>
<div class="frame about">{svg_inline(about_banner(), 600, 150, "banner", "About banner")}</div>
<h2>Interface</h2>
<p>One window. The rail holds the six disc tasks and two tools; the pane holds the task. The original and the copy are always shown side by side before anything starts, and every long task shows the disc surface map. Each screen below is the reference for the SPEC section named above it; the same screens are rendered in light and dark in design/mockups.</p>
{screens}
<h2>Colour tokens</h2>
<h3>Brand</h3><div class="tok">{toks(B)}</div>
<h3>Light</h3><div class="tok">{toks(L)}</div>
<h3>Dark</h3><div class="tok">{toks(D)}</div>
</div>
'''


# ---------------------------------------------------------------- output
def write(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="ascii", newline="\n") as f:
        f.write(text)


def build_vectors():
    brand = os.path.join(HERE, "brand")
    write(os.path.join(brand, "icon.svg"), svg_doc(icon(), 256, 256))
    write(os.path.join(brand, "icon-small.svg"), svg_doc(icon(True), 256, 256))
    for dark in (False, True):
        inner, w, h = wordmark(dark)
        write(os.path.join(brand, "wordmark-%s.svg" % ("dark" if dark else "light")), svg_doc(inner, w, h))
    write(os.path.join(brand, "github-banner.svg"), svg_doc(banner(1280, 320), 1280, 320))
    write(os.path.join(brand, "social-preview.svg"), svg_doc(banner(1280, 640), 1280, 640))
    write(os.path.join(brand, "about-banner.svg"), svg_doc(about_banner(), 600, 150))
    for k in KEYS:
        write(os.path.join(HERE, "icons", k + ".svg"), svg_doc(button(k), 64, 64))
        write(os.path.join(HERE, "icons", k + "-dark.svg"), svg_doc(button(k, True), 64, 64))
    write(os.path.join(HERE, "sheet.html"), sheet())


def find_browser():
    for env in ("ProgramFiles", "ProgramFiles(x86)", "LOCALAPPDATA"):
        base = os.environ.get(env)
        for rel in (r"Google\Chrome\Application\chrome.exe", r"Microsoft\Edge\Application\msedge.exe"):
            if base and os.path.isfile(os.path.join(base, rel)):
                return os.path.join(base, rel)
    for name in ("chrome", "chromium", "msedge", "google-chrome"):
        found = shutil.which(name)
        if found:
            return found
    sys.exit("build.py: no Chrome or Edge found for rasters; run with --no-raster")


def shoot(browser, tmp, html, w, h, scale, out):
    """Render an HTML body to a PNG with a transparent ground. The browser's whole process tree is ended on timeout."""
    page = os.path.join(tmp, "page.html")
    with open(page, "w", encoding="ascii") as f:
        f.write('<!doctype html><html><head><meta charset="utf-8"><style>html,body{margin:0;background:transparent}'
                '</style></head><body>%s</body></html>' % html)
    args = [browser, "--headless=new", "--disable-gpu", "--hide-scrollbars", "--no-first-run",
            "--default-background-color=00000000", "--force-device-scale-factor=%s" % scale,
            "--virtual-time-budget=6000", "--user-data-dir=" + os.path.join(tmp, "profile"),
            "--window-size=%d,%d" % (w, h), "--screenshot=" + out, "file:///" + page.replace("\\", "/")]
    flags = getattr(subprocess, "CREATE_NO_WINDOW", 0)
    proc = subprocess.Popen(args, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, creationflags=flags)
    try:
        proc.wait(timeout=90)
    except subprocess.TimeoutExpired:
        if os.name == "nt":
            subprocess.run(["taskkill", "/PID", str(proc.pid), "/T", "/F"], capture_output=True, creationflags=flags)
        else:
            proc.kill()
        sys.exit("build.py: the browser did not finish rendering " + out)
    if not os.path.isfile(out):
        sys.exit("build.py: the browser wrote no " + out)


def write_ico(path, images):
    """Icons up to 64 px as 32-bit bitmaps, larger ones as PNG, the form every Windows loader and rc.exe reads."""
    blobs = []
    for im in images:
        w = im.width
        if w >= 128:
            b = io.BytesIO()
            im.save(b, "PNG")
            blobs.append((w, b.getvalue()))
            continue
        raw = im.tobytes("raw", "BGRA")
        xor = b"".join(raw[i * w * 4:(i + 1) * w * 4] for i in range(w - 1, -1, -1))
        mask = b"\x00" * (((w + 31) // 32) * 4 * w)
        head = struct.pack("<IiiHHIIiiII", 40, w, w * 2, 1, 32, 0, len(xor) + len(mask), 0, 0, 0, 0)
        blobs.append((w, head + xor + mask))
    offset = 6 + 16 * len(blobs)
    directory = b""
    for w, data in blobs:
        directory += struct.pack("<BBBBHHII", w % 256, w % 256, 0, 0, 1, 32, len(data), offset)
        offset += len(data)
    with open(path, "wb") as f:
        f.write(struct.pack("<HHH", 0, 1, len(blobs)) + directory + b"".join(d for _, d in blobs))


def build_rasters():
    from PIL import Image

    browser = find_browser()
    brand = os.path.join(HERE, "brand")
    mocks = os.path.join(HERE, "mockups")
    os.makedirs(mocks, exist_ok=True)
    fonts = '<link rel="stylesheet" href="%s">' % FONT_LINK
    with tempfile.TemporaryDirectory(prefix="carboncd-design-") as tmp:
        def square(inner, grid, name):
            out = os.path.join(tmp, name + ".png")
            shoot(browser, tmp, '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 %d %d" width="1024" height="1024">%s</svg>' % (grid, grid, inner), 1024, 1024, 1, out)
            return Image.open(out).convert("RGBA")

        full, small = square(icon(), 256, "icon"), square(icon(True), 256, "icon-small")

        def sized(px):
            return (small if px < 48 else full).resize((px, px), Image.Resampling.LANCZOS)

        sized(512).save(os.path.join(brand, "icon-512.png"))
        write_ico(os.path.join(RES, "cdm.ico"), [sized(s) for s in T["icon"]["sizes_px"]])
        write_ico(os.path.join(RES, "idr_main.ico"), [sized(s) for s in (16, 24, 32, 48)])

        def on_white(im, px):
            base = Image.new("RGBA", (px, px), (255, 255, 255, 255))
            base.alpha_composite(im.resize((px, px), Image.Resampling.LANCZOS))
            return base.convert("RGB")

        on_white(full, 128).save(os.path.join(RES, "AboutImage.bmp"))
        for key, name in RES_BMP.items():
            art = square('<g transform="translate(6 6) scale(.8125)">%s</g>' % button(key), 64, key)
            on_white(art, 128).save(os.path.join(RES, name))

        for name, w, h, scale in (("github-banner", 1280, 320, 2), ("social-preview", 1280, 640, 1), ("about-banner", 600, 150, 2)):
            with open(os.path.join(brand, name + ".svg"), encoding="ascii") as f:
                shoot(browser, tmp, f.read(), w, h, scale, os.path.join(brand, name + ".png"))
        for name, builder, h, _ in SCREENS:
            for dark in (False, True):
                body = '%s<style>%s body{background:%s;display:flex;justify-content:center;align-items:flex-start;padding:30px 0}</style>%s' % (
                    fonts, mock_css(D if dark else L), "#07081A" if dark else "#DCE1F2", screen_html(builder, dark))
                shoot(browser, tmp, body, 880, h, 2, os.path.join(mocks, name + ("-dark" if dark else "") + ".png"))
        with open(os.path.join(mocks, "README.md"), "w", encoding="ascii", newline="\n") as f:
            f.write("# Reference renders\n\nGenerated by `python design/build.py`. Each screen is rendered at 200 percent in light (`<name>.png`) and dark (`<name>-dark.png`). The HTML and CSS they are drawn from are in `design/sheet.html`, where every size can be measured.\n\n| Screen | SPEC | Light | Dark |\n| ------ | ---- | ----- | ---- |\n")
            for name, _, _, spec in SCREENS:
                f.write("| %s | %s | [%s.png](./%s.png) | [%s-dark.png](./%s-dark.png) |\n" % (name.replace("-", " ").capitalize(), spec, name, name, name, name))


if __name__ == "__main__":
    build_vectors()
    if "--no-raster" not in sys.argv[1:]:
        build_rasters()
    print("design assets built")
