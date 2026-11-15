#!/usr/bin/env python3
"""Builds the PDF manual of Kick Kitten: text from docs/manual.txt, controls from assets/spec.json (same data as the plugin),
a numbered tour on a screenshot of the real editor (docs/manual-shot.png, made by the KickKittenSnap tool at width 1640) and the
factory presets from assets/presets.json.
Usage: python3 tools/make_manual.py <mockup.html (unused, kept for the CI call)> <docs/manual.txt> <shot.png> <out.pdf>
Needs: pip install reportlab pillow"""
import json, os, sys, io
from PIL import Image, ImageDraw, ImageFont
from reportlab.lib import colors
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.units import mm
from reportlab.lib.utils import ImageReader
from reportlab.platypus import SimpleDocTemplate, Paragraph, Spacer, Table, TableStyle, KeepTogether, Image as RLImage, PageBreak, CondPageBreak

_, _mock, txt_path, shot_path, out = sys.argv[:5]
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
spec = json.load(open(os.path.join(root, "assets/spec.json")))
lay = json.load(open(os.path.join(root, "assets/layout.json")))
presets = json.load(open(os.path.join(root, "assets/presets.json")))
byid = {s["id"]: s for s in spec}
name = "Kick Kitten"

GROUPS = [
 ("Pitch envelope", ["tailHz", "startHz", "sweep", "curve", "swell"]),
 ("Click", ["clickLvl", "clickFreq", "clickLen"]),
 ("Layers and amp", ["body", "sub", "hold", "decay", "ampShape"]),
 ("Purr Trio (colour)", ["drive", "trans", "fuzz", "clip"]),
 ("Sauce", ["sauce", "sPunch", "sThump", "sBloom", "sCrack", "sGlue", "sLoud"]),
 ("Glue and limiter", ["gThr", "gRat", "gAtk", "gRel", "lim", "limG"]),
 ("Tone EQ", ["eMud", "eChest", "eShine"]),
 ("Out", ["ceil", "palign", "out"]),
] + [("Layer %s" % "ABC"[i], ["l%d%s" % (i, k) for k in ("lvl", "pitch", "start", "len", "delay", "hp", "lp")]) for i in range(3)]
LAYER_LBL = {"lvl": "Level", "pitch": "Pitch", "start": "Start", "len": "Length", "delay": "Delay", "hp": "High-pass", "lp": "Low-pass"}

# number every control in on-screen order (top to bottom, left to right) that exists in the spec
items = [it for it in lay["items"] if it.get("kid") in byid]
items.sort(key=lambda it: (round(it["r"][1] / 40), it["r"][0]))
num = {}
for it in items:
    num.setdefault(it["kid"], len(num) + 1)
pos = {it["kid"]: it["r"] for it in items}

def fmt(v, s):
    d = s.get("dec", 0)
    t = ("%.*f" % (d, v)).rstrip("0").rstrip(".") if d else "%d" % round(v)
    return t + s.get("unit", "")
def rng(s):
    if s["min"] == 0 and s["max"] == 1 and s.get("dec", 0) == 0 and not s.get("unit"):
        return "switch, default " + ("on" if s["def"] else "off")
    return "%s to %s, default %s%s" % (fmt(s["min"], s), fmt(s["max"], s), fmt(s["def"], s), ", log scale" if s.get("log") else "")

# ---- tour image with numbered badges
im = Image.open(shot_path).convert("RGB"); W, H = im.size; k = W / lay["W"]
dr = ImageDraw.Draw(im)
try: f = ImageFont.truetype(os.path.join(root, "assets/fonts/JetBrainsMono-SemiBold.ttf"), 13)
except Exception: f = ImageFont.load_default()
for kid, n in num.items():
    x, y, w, h = [v * k for v in pos[kid]]; cx, cy = x + 8, y + 6
    dr.ellipse([cx - 10, cy - 10, cx + 10, cy + 10], fill=(255, 123, 177), outline=(22, 22, 56))
    t = str(n); tw = dr.textlength(t, font=f)
    dr.text((cx - tw / 2, cy - 8), t, fill=(22, 22, 56), font=f)
tour_path = os.path.join(root, "docs/tour/tour.json")
tour = json.load(open(tour_path)) if os.path.exists(tour_path) else None
if tour:
    for d in tour["displays"]:
        x, y, w, h = [v * k for v in d["r"]]; cx, cy = x + w - 12, y + 12
        dr.ellipse([cx - 10, cy - 10, cx + 10, cy + 10], fill=(2, 235, 186), outline=(22, 22, 56))
        tw = dr.textlength(d["letter"], font=f); dr.text((cx - tw / 2, cy - 8), d["letter"], fill=(22, 22, 56), font=f)
buf = io.BytesIO(); im.save(buf, "PNG"); buf.seek(0)
def crop(r, pad=10):
    x0, y0, x1, y1 = r[0] - pad, r[1] - pad, r[0] + r[2] + pad, r[1] + r[3] + pad
    c = im.crop((max(0, int(x0 * k)), max(0, int(y0 * k)), min(W, int(x1 * k)), min(H, int(y1 * k))))
    b = io.BytesIO(); c.save(b, "PNG"); b.seek(0); return b, c.size

sec = {}; cur = None
for line in open(txt_path, encoding="utf-8").read().splitlines():
    if line.startswith("# "): cur = line[2:].strip(); sec[cur] = []
    elif cur is not None: sec[cur].append(line)
para = lambda k: [p.strip() for p in "\n".join(sec.get(k, [])).split("\n\n") if p.strip()]
bullets = lambda k: [l[2:].strip() for l in sec.get(k, []) if l.startswith("- ")]

BG, PANEL = colors.HexColor("#161638"), colors.HexColor("#2c2c62")
PINK, LILAC, INK, DIM = colors.HexColor("#ff7bb1"), colors.HexColor("#e1cbf4"), colors.HexColor("#dcdaf5"), colors.HexColor("#9b98c8")
st = lambda n, **kw: ParagraphStyle(n, fontName=kw.pop("font", "Helvetica"), **kw)
H1 = st("h1", fontSize=26, leading=30, textColor=PINK, font="Helvetica-Bold")
H2 = st("h2", fontSize=14, leading=18, textColor=LILAC, font="Helvetica-Bold", spaceBefore=10, spaceAfter=4)
B = st("b", fontSize=10, leading=14, textColor=INK)
SM = st("s", fontSize=8.5, leading=11.5, textColor=DIM)
CELL = st("c", fontSize=8.5, leading=11, textColor=INK)
CELLB = st("cb", fontSize=8.5, leading=11, textColor=PINK, font="Helvetica-Bold")
esc = lambda t: t.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")

story = [Paragraph(name, H1), Paragraph("Manual . Studio 87 . free and open source (AGPL-3.0-or-later) . github.com/SpeakingColors/KickKitten", SM), Spacer(1, 6)]
for p in para("Intro"): story += [Paragraph(esc(p), B), Spacer(1, 4)]
story.append(Paragraph("Quick start", H2))
for t in bullets("Quick start"): story.append(Paragraph("&bull; " + esc(t), B))
story.append(PageBreak())
story.append(Paragraph("Interface tour", H1))
story.append(Paragraph("Pink numbers match the control tables on the next pages. This is a picture of the plugin editor, rendered offscreen.", SM))
story.append(Spacer(1, 4))
iw = 170 * mm
story.append(RLImage(buf, width=iw, height=iw * H / W))
story.append(Paragraph("Signal flow: pitch envelope drives body and sub; plus click; three layers; Purr Trio colour; Sauce; glue; tone EQ; limiter; clip and ceiling; output.", SM))
def ctl_rows(ids, layer_prefix=False):
    rows = []
    for i in ids:
        sp = byid.get(i)
        if not sp: continue
        label = sp["label"]
        if layer_prefix and i[0] == "l" and i[1] in "012": label = "Layer %s %s" % ("ABC"[int(i[1])], LAYER_LBL.get(i[2:], label))
        rows.append([Paragraph(str(num.get(i, "")), CELLB), Paragraph(esc(label), CELLB), Paragraph(esc(rng(sp)), CELL), Paragraph(esc(sp.get("tip", "")), CELL)])
    return rows
def ctl_table(rows):
    t = Table(rows, colWidths=[8 * mm, 22 * mm, 42 * mm, 98 * mm])
    t.setStyle(TableStyle([("VALIGN", (0, 0), (-1, -1), "TOP"), ("BACKGROUND", (0, 0), (-1, -1), PANEL), ("LINEBELOW", (0, 0), (-1, -1), 0.5, BG),
                           ("TOPPADDING", (0, 0), (-1, -1), 4), ("BOTTOMPADDING", (0, 0), (-1, -1), 4), ("LEFTPADDING", (0, 0), (-1, -1), 5)]))
    return t
PH = st("ph", fontSize=12.5, leading=16, textColor=LILAC, font="Helvetica-Bold", spaceBefore=6, spaceAfter=4)
if tour:
    story.append(Paragraph("Teal letters mark the displays and meters, pink numbers mark the controls. The next pages go panel by panel.", SM))
    story.append(Paragraph("Displays", H2))
    drows = [[Paragraph(d["letter"], st("dl", fontSize=8.5, leading=11, textColor=colors.HexColor("#02ebba"), font="Helvetica-Bold")), Paragraph(esc(d["text"]), CELL)] for d in tour["displays"]]
    dt = Table(drows, colWidths=[8 * mm, 162 * mm])
    dt.setStyle(TableStyle([("VALIGN", (0, 0), (-1, -1), "TOP"), ("BACKGROUND", (0, 0), (-1, -1), PANEL), ("LINEBELOW", (0, 0), (-1, -1), 0.5, BG), ("TOPPADDING", (0, 0), (-1, -1), 3), ("BOTTOMPADDING", (0, 0), (-1, -1), 3)]))
    story.append(dt)
    used = set()
    def inside(kid, r):
        x, y, w, h = pos[kid]; cx, cy = x + w / 2, y + h / 2
        return r[0] <= cx <= r[0] + r[2] and r[1] <= cy <= r[1] + r[3]
    for pn in tour["panels"]:
        ids = sorted([kid for kid in num if kid not in used and inside(kid, pn["r"])], key=lambda kid: num[kid])
        used.update(ids)
        if not ids: continue
        cb, (cw, ch) = crop(pn["r"]); wmm = min(170.0, 120.0 * cw / ch if ch > cw else 170.0)
        ds = [d for d in tour["displays"] if pn["r"][0] <= d["r"][0] + 1 and d["r"][0] + d["r"][2] <= pn["r"][0] + pn["r"][2] + 1 and pn["r"][1] <= d["r"][1] and d["r"][1] + d["r"][3] <= pn["r"][1] + pn["r"][3] + 1]
        blk = [Paragraph(esc(pn["title"]), PH), RLImage(cb, width=wmm * mm, height=wmm * mm * ch / cw)]
        if ds: blk.append(Paragraph("Display " + "; ".join("%s: %s" % (d["letter"], esc(d["text"])) for d in ds), SM))
        story.append(PageBreak()); story += blk + [Spacer(1, 4), ctl_table(ctl_rows(ids))]
    rest = [kid for kid in sorted(num, key=lambda kid: num[kid]) if kid not in used]
    if rest:
        xs = [pos[kid] for kid in rest if kid[0] == "l" and kid[1] in "012"]
        story.append(PageBreak()); story.append(Paragraph("Layers A, B and C", PH))
        if xs:
            x0 = min(r[0] for r in xs); y0 = min(r[1] for r in xs); x1 = max(r[0] + r[2] for r in xs); y1 = max(r[1] + r[3] for r in xs)
            cb, (cw, ch) = crop([x0, y0 - 48, x1 - x0, y1 - y0 + 52]); story.append(RLImage(cb, width=170 * mm, height=170 * mm * ch / cw))
        story += [Spacer(1, 4), ctl_table(ctl_rows(rest, True))]
else:
    story.append(Paragraph("Controls", H2))
    for title, ids in GROUPS:
        rows = ctl_rows(ids, False)
        if rows: story.append(KeepTogether([Paragraph(esc(title), PH), ctl_table(rows)]))
story.append(CondPageBreak(70 * mm))
story.append(Paragraph("Under the hood", H2))
for t in ["The kick is rendered at 4x oversampling and decimated with a sinc filter; the finished kick is soft-clipped above 0.8 and held under the true-peak Ceiling.",
          "Rendering runs on a background thread. The audio thread only plays a finished render, so a knob turn is heard when the new render is ready, not on the next buffer.",
          "Same mode plays one rendered kick on every key. Pitched mode re-renders the kick for each note (tail and pitch envelope scale with the note); until that note is ready, a resampled copy plays.",
          "Velocity is ignored. The plugin reports no latency.",
          "Your presets are plain .kkpreset files in the Studio 87 / Kick Kitten / presets folder."]:
    story.append(Paragraph("&bull; " + esc(t), B))
tips = bullets("Recommendations")
if tips:
    story.append(Paragraph("Recommendations", H2))
    for t in tips: story.append(Paragraph("&bull; " + esc(t), B))
story.append(Paragraph("Factory presets", H2))
story.append(Paragraph(esc(", ".join(p["name"] for p in presets)), B))
story.append(Paragraph("Install, remove and your saved presets", H2))
for t in bullets("Install"): story.append(Paragraph("&bull; " + esc(t), B))
story.append(Paragraph("See INSTALL.txt in the zip for the exact steps and folders on each system.", B))

def page(c, d):
    c.saveState(); c.setFillColor(BG); c.rect(0, 0, A4[0], A4[1], stroke=0, fill=1)
    c.setFillColor(PANEL); c.rect(0, A4[1] - 5 * mm, A4[0], 5 * mm, stroke=0, fill=1)
    c.setFillColor(PINK); c.rect(0, A4[1] - 5.8 * mm, A4[0], 0.8 * mm, stroke=0, fill=1)
    c.setFont("Helvetica", 8); c.setFillColor(DIM)
    c.drawString(20 * mm, 10 * mm, name + " manual"); c.drawRightString(190 * mm, 10 * mm, "page %d" % d.page); c.restoreState()
SimpleDocTemplate(out, pagesize=A4, leftMargin=20 * mm, rightMargin=20 * mm, topMargin=16 * mm, bottomMargin=18 * mm,
                  title=name + " manual", author="Studio 87").build(story, onFirstPage=page, onLaterPages=page)
print("wrote", out, "controls", len(num), "of", len(spec), "presets", len(presets))
