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
def _btn(key, cls):
    return svg_inline(button(key), 64, 64, cls, LABEL[key])


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
        out += '<div class="ri">%s<span>%s</span></div>' % (_btn(k, "ric"), LABEL[k])
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


MOCK_CSS = '''
.win{width:820px;border-radius:8px;overflow:hidden;box-shadow:0 18px 50px rgba(20,23,43,.3);font:12.500px/1.35 'Segoe UI Variable Text','Segoe UI',sans-serif;display:flex;flex-direction:column;background:%(window)s;color:%(text)s}
.tb{display:flex;align-items:center;gap:8px;height:34px;padding:0 12px;flex:none;background:%(primary)s;color:%(on-primary)s}
.tb b{font-weight:600}.tbi{width:18px;height:18px}.ctl{margin-left:auto;display:flex;opacity:.75}.tbm{opacity:.7;margin-left:6px}
.body{display:flex;min-height:430px}
.rail{width:168px;flex:none;background:%(rail)s;border-right:1px solid %(line-soft)s;padding:10px 8px;display:flex;flex-direction:column;gap:2px}
.ri{display:flex;align-items:center;gap:9px;height:40px;padding:0 8px;border-radius:6px;font-weight:500}
.ri.on{background:%(surface)s;box-shadow:inset 3px 0 0 %(primary)s,0 1px 2px rgba(30,42,120,.12);font-weight:700}
.rail.busy .ri:not(.on){opacity:.4}
.ric{width:28px;height:28px;flex:none}.rsp{flex:1}
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
''' % L

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
<h2>Interface: choose a task</h2>
<p>One window. The rail holds the six disc tasks and two tools; the pane holds the task. The original and the copy are always shown side by side before anything starts.</p>
<div class="scroll"><div class="stage" id="mock-read">{mock_read()}</div></div>
<h2>Interface: watch it run</h2>
<p>The surface map draws the disc as cells, one row block per layer. Every read, write, clone, verify and erase uses it.</p>
<div class="scroll"><div class="stage" id="mock-progress">{mock_progress()}</div></div>
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
        for name, html, h in (("main-read", mock_read(), 540), ("progress-surface-map", mock_progress(), 640)):
            body = '%s<style>%s body{background:#DCE1F2;display:flex;justify-content:center;padding:30px 0}</style>%s' % (fonts, MOCK_CSS, html)
            shoot(browser, tmp, body, 880, h, 2, os.path.join(mocks, name + ".png"))


if __name__ == "__main__":
    build_vectors()
    if "--no-raster" not in sys.argv[1:]:
        build_rasters()
    print("design assets built")
