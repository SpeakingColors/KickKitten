#!/usr/bin/env python3
"""Builds docs/tour/tour.json for the manual's visual tour from the approved mockup (docs/mockups/KickKitten-v1.html)
and assets/layout.json (control and panel rectangles read from the mockup DOM by tools/export_layout.js).
Panels come from <section class="panel" id=...> and their <h2>; displays are the <canvas> elements with their aria-label text.
Nothing is written by hand: run `python3 tools/build_tour_data.py` after the mockup or layout changes."""
import json, re, os, html
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
mock = open(os.path.join(root, "docs/mockups/KickKitten-v1.html"), encoding="utf-8").read()
lay = json.load(open(os.path.join(root, "assets/layout.json")))
rect = {it["id"]: it["r"] for it in lay["items"] if it.get("id")}
panels = []
for m in re.finditer(r'<section class="panel" id="(\w+)"', mock):
    h2 = re.search(r"<h2[^>]*>([^<]*)", mock[m.end():m.end() + 900])
    if m.group(1) in rect and h2:
        panels.append({"id": m.group(1), "title": html.unescape(h2.group(1)).strip(), "r": rect[m.group(1)]})
# the Render tab sits on top of the Glue panel (same rectangle): list the rectangle once
seen=set(); uniq=[]
for pn in panels:
    key=tuple(pn['r'])
    if key not in seen: seen.add(key); uniq.append(pn)
panels=uniq
panels[:] = [pn for pn in panels if pn['title']!='Render']
if "pGlue" in rect and "Glue &amp; limiter" in mock and not any(pn["id"] == "pGlue" for pn in panels):
    panels.append({"id": "pGlue", "title": "Glue & limiter", "r": rect["pGlue"]})   # title from the tab button text
panels.sort(key=lambda pn: (pn["r"][0], pn["r"][1]))
displays = []
for m in re.finditer(r'<canvas id="(cv\w+|lw\d)"[^>]*?aria-label="([^"]*)"', mock):
    if m.group(1) in rect:
        displays.append({"id": m.group(1), "text": html.unescape(m.group(2)), "r": rect[m.group(1)]})
# the three layer waveform strips are drawn by script, not in the HTML: take them from the layout
for i in range(3):
    k = "lw%d" % i
    if k in rect and not any(d["id"] == k for d in displays):
        displays.append({"id": k, "text": "Layer %s sound waveform" % "ABC"[i], "r": rect[k]})
displays.sort(key=lambda d: (round(d["r"][1] / 40), d["r"][0]))
for n, d in enumerate(displays): d["letter"] = chr(65 + n)
out = {"W": lay["W"], "H": lay["H"], "panels": panels, "displays": displays}
json.dump(out, open(os.path.join(root, "docs/tour/tour.json"), "w"), indent=1)
print("panels", [p["title"] for p in panels]); print("displays", [(d["letter"], d["id"], d["text"][:40]) for d in displays])
