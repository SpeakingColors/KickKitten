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
from reportlab.platypus import SimpleDocTemplate, Paragraph, Spacer, Table, TableStyle, KeepTogether, Image as RLImage, PageBreak

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
buf = io.BytesIO(); im.save(buf, "PNG"); buf.seek(0)

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
story.append(Paragraph("Pink numbers match the control tables on the next pages. This is a picture of the real plugin window.", SM))
story.append(Spacer(1, 4))
iw = 170 * mm
story.append(RLImage(buf, width=iw, height=iw * H / W))
story.append(Paragraph("Signal flow: pitch envelope drives body and sub; plus click; three layers; Purr Trio colour; Sauce; glue; tone EQ; limiter; clip and ceiling; output.", SM))
story.append(Paragraph("Controls", H2))
for title, ids in GROUPS:
    rows = []
    for i in ids:
        s = byid.get(i)
        if not s: continue
        label = s["label"]
        if i[0] == "l" and i[1] in "012" and i[2:] in LAYER_LBL: label = LAYER_LBL[i[2:]]
        rows.append([Paragraph(str(num.get(i, "")), CELLB), Paragraph(esc(label), CELLB), Paragraph(esc(rng(s)), CELL), Paragraph(esc(s.get("tip", "")), CELL)])
    if not rows: continue
    t = Table(rows, colWidths=[8 * mm, 20 * mm, 42 * mm, 100 * mm])
    t.setStyle(TableStyle([("VALIGN", (0, 0), (-1, -1), "TOP"), ("BACKGROUND", (0, 0), (-1, -1), PANEL), ("LINEBELOW", (0, 0), (-1, -1), 0.5, BG),
                           ("TOPPADDING", (0, 0), (-1, -1), 4), ("BOTTOMPADDING", (0, 0), (-1, -1), 4), ("LEFTPADDING", (0, 0), (-1, -1), 5)]))
    story.append(KeepTogether([Paragraph(esc(title), st("ph", fontSize=11.5, leading=15, textColor=LILAC, font="Helvetica-Bold", spaceBefore=8, spaceAfter=4)), t]))
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
