#!/usr/bin/env python3
"""Section drawings of the case round the board, with the gaps between them measured and labelled.

Cuts the printed parts (stl/housing.stl, stl/cover.stl) and a dummy of the board (parts and cell, from case.scad) with
planes, draws the cut faces, and for each cut finds where the board is closest to the case and writes the distance on the
drawing. Writes renders/sections_front.png (vertical cuts along the long side, seen from the front of the device),
renders/sections_side.png (vertical cuts across it) and renders/sections_plan.png (horizontal cuts, seen from the front),
plus renders/gaps.txt with the numbers.

Run from case/ with a venv that has trimesh, networkx, shapely and matplotlib:  .venv/bin/python sections.py
Case frame: X long side (USB-C at +X), Y short side, Z thickness (front +Z), Z = 0 is the back face of the PCB.
"""
import os, subprocess, sys, tempfile
import numpy as np
import trimesh
from shapely.geometry import LineString, MultiLineString, Point, Polygon
from shapely.ops import unary_union, polygonize, nearest_points
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "renders")
TMP = tempfile.mkdtemp(prefix="focustype_sec_")
LIMIT = 3.0      # label gaps smaller than this (mm)
CONTACT = 0.05   # closer than this is a deliberate contact (the standoff ends rest on the cover)
TIGHT = 0.25     # closer than this, and not a contact, is a problem
SNUG = 0.55      # up to this is a designed small gap (glass 0.3 mm under the front frame, pocket 0.5 mm round the cell)


def scad(part):
    f = os.path.join(TMP, part + ".stl")
    subprocess.run(["openscad", "-o", f, "-D", 'part="%s"' % part, os.path.join(HERE, "case.scad")],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return trimesh.load(f)


def load_all():
    case = [trimesh.load(os.path.join(HERE, "stl", n + ".stl")) for n in ("housing", "cover")]
    return case, scad("board_parts"), scad("battery")


AXES = {"X": ([1, 0, 0], (1, 2)), "Y": ([0, 1, 0], (0, 2)), "Z": ([0, 0, 1], (0, 1))}


def cut(meshes, axis, value):
    """Cross-section of the meshes at axis = value, in the plane's two coordinates. Each mesh is filled even-odd (a loop
    inside a loop is a hole); the meshes are then merged."""
    normal, (a, b) = AXES[axis]
    origin = np.zeros(3); origin["XYZ".index(axis)] = value
    parts = []
    for m in meshes:
        sec = m.section(plane_normal=normal, plane_origin=origin)
        if sec is None:
            continue
        geom = None
        for loop in sec.discrete:                       # closed loops, vertices merged by trimesh
            if len(loop) < 4:
                continue
            shell = Polygon(loop[:, [a, b]]).buffer(0)
            if shell.is_empty:
                continue
            geom = shell if geom is None else geom.symmetric_difference(shell)
        if geom is not None:
            parts.append(geom)
    return unary_union(parts) if parts else None


def gaps(board, case):
    """Local minima of the distance from the board outline to the case, below LIMIT, at least 7 mm apart."""
    if board is None or case is None or board.is_empty or case.is_empty:
        return []
    pts = []
    for poly in getattr(board, "geoms", [board]):
        for ring in [poly.exterior] + list(poly.interiors):
            n = max(int(ring.length / 0.4), 2)
            pts += [ring.interpolate(i * ring.length / n) for i in range(n)]
    found = []
    for p in pts:
        d = case.distance(p)
        if d < LIMIT:
            found.append((d, p))
    found.sort(key=lambda t: t[0])
    picked = []
    for d, p in found:
        if all(p.distance(q) > 7.0 for _, q, _ in picked):
            picked.append((d, p, nearest_points(p, case)[1]))
        if len(picked) >= 14:
            break
    return picked


def draw(ax, geom, **kw):
    if geom is None:
        return
    for poly in getattr(geom, "geoms", [geom]):
        if poly.geom_type != "Polygon":
            continue
        x, y = poly.exterior.xy
        ax.fill(x, y, **kw)
        for h in poly.interiors:
            hx, hy = h.xy
            ax.fill(hx, hy, color="white", zorder=kw.get("zorder", 1) + 0.1)


def prepare(case, board, cell, axis, value):
    c = cut(case, axis, value)
    b = cut([board], axis, value)
    k = cut([cell], axis, value)
    both = unary_union([g for g in (b, k) if g is not None]) if (b is not None or k is not None) else None
    allg = unary_union([g for g in (c, b, k) if g is not None])
    return c, b, k, gaps(both, c), allg.bounds


def panel(ax, title, data, labels, report):
    c, b, k, found, bounds = data
    draw(ax, c, facecolor="#c9ccd1", edgecolor="#3b3f45", linewidth=0.6, zorder=1)
    draw(ax, b, facecolor="#4c9a6a", edgecolor="#1d4d30", linewidth=0.6, zorder=2)
    draw(ax, k, facecolor="#f0a23c", edgecolor="#8a5a14", linewidth=0.6, zorder=3)
    rows = []
    for d, p, q in found:
        col = "#777777" if d < CONTACT else ("#c0392b" if d < TIGHT else ("#d98200" if d < SNUG else "#1f5fbf"))
        ax.plot([p.x, q.x], [p.y, q.y], color=col, linewidth=1.2, zorder=5)
        ax.annotate("%.2f" % d, ((p.x + q.x) / 2, (p.y + q.y) / 2), fontsize=8, color=col, zorder=6,
                    xytext=(3, 3), textcoords="offset points", fontweight="bold")
        rows.append((d, p.x, p.y))
    x0, y0, x1, y1 = bounds
    ax.set_xlim(x0 - 3, x1 + 3)
    ax.set_ylim(y0 - 3, y1 + 3)
    ax.set_title(title, fontsize=9.5, loc="left")
    ax.set_aspect("equal", adjustable="box")
    ax.grid(True, linewidth=0.25)
    ax.tick_params(labelsize=7)
    ax.set_xlabel(labels[0], fontsize=7.5)
    ax.set_ylabel(labels[1], fontsize=7.5)
    report.append(title)
    for d, x, y in rows:
        note = "   contact (by design)" if d < CONTACT else ("   <-- TIGHT" if d < TIGHT else ("   designed small gap" if d < SNUG else ""))
        report.append("    %.2f mm at (%.1f, %.1f)%s" % (d, x, y, note))


def sheet(name, cuts, ncols, case, board, cell, labels, report, mm_per_inch):
    """One figure, panels sized to their data (1 mm = the same length everywhere), so nothing is wasted."""
    data = [prepare(case, board, cell, axis, value) for axis, value, _ in cuts]
    nrows = (len(cuts) + ncols - 1) // ncols
    w = [0.0] * ncols
    h = [0.0] * nrows
    for n, d in enumerate(data):
        x0, y0, x1, y1 = d[4]
        r, c = divmod(n, ncols)
        w[c] = max(w[c], x1 - x0 + 6)
        h[r] = max(h[r], y1 - y0 + 6)
    pad_w, pad_h = 14.0, 17.0            # mm of room for tick labels and titles between panels
    fig_w = (sum(w) + pad_w * ncols) / mm_per_inch
    fig_h = (sum(h) + pad_h * nrows) / mm_per_inch + 0.5
    fig = plt.figure(figsize=(fig_w, fig_h))
    gs = fig.add_gridspec(nrows, ncols, width_ratios=[x + pad_w for x in w], height_ratios=[x + pad_h for x in h],
                          left=0.05, right=0.99, top=1 - 0.5 / fig_h, bottom=0.03, wspace=0.12, hspace=0.12)
    for n, ((axis, value, title), d) in enumerate(zip(cuts, data)):
        panel(fig.add_subplot(gs[divmod(n, ncols)]), title, d, labels[axis], report)
    fig.suptitle("%s   grey = case, green = board, orange = 18650 cell. Numbers = gap in mm: grey contact, red under %.2f, "
                 "orange under %.2f (designed small gap), blue otherwise" % (name, TIGHT, SNUG), fontsize=9, y=1 - 0.1 / fig_h)
    fig.savefig(os.path.join(OUT, name + ".png"), dpi=130)
    plt.close(fig)


def main():
    os.makedirs(OUT, exist_ok=True)
    case, board, cell = load_all()
    labels = {"Y": ("X (mm), USB-C at +X", "Z (mm), front up"), "X": ("Y (mm)", "Z (mm), front up"),
              "Z": ("X (mm), USB-C at +X", "Y (mm)")}
    rep = []
    sheet("sections_front", [("Y", 0.0, "Cut at Y = 0: USB-C, header, centre"),
                             ("Y", -19.4, "Cut at Y = -19.4: through the cell axis and the battery pocket"),
                             ("Y", 19.15, "Cut at Y = 19.15: microSD slot"),
                             ("Y", 31.05, "Cut at Y = 31.05: through the top standoffs"),
                             ("Y", 37.9, "Cut at Y = 37.9: through the top screw bosses and the button tunnels"),
                             ("Y", -45.7, "Cut at Y = -45.7: through the bottom screw bosses (and the engraved text)")],
          2, case, board, cell, labels, rep, 14.0)
    sheet("sections_side", [("X", 0.0, "Cut at X = 0: buttons, header, cell, from the side"),
                            ("X", 42.75, "Cut at X = 42.75: through the standoffs"),
                            ("X", -20.0, "Cut at X = -20: through the top and bottom screw bosses"),
                            ("X", 44.0, "Cut at X = 44: top and bottom screw bosses, right side")],
          2, case, board, cell, labels, rep, 12.0)
    sheet("sections_plan", [("Z", 3.0, "Plan at Z = 3.0: glass level"),
                            ("Z", 0.8, "Plan at Z = 0.8: PCB level"),
                            ("Z", -1.0, "Plan at Z = -1.0: components on the back"),
                            ("Z", -5.0, "Plan at Z = -5.0: under the components"),
                            ("Z", -8.0, "Plan at Z = -8.0: inside the cover plate"),
                            ("Z", -12.0, "Plan at Z = -12.0: battery pocket")],
          3, case, board, cell, labels, rep, 18.0)
    with open(os.path.join(OUT, "gaps.txt"), "w") as f:
        f.write("Smallest gaps between the board (and cell) and the case, per cut. Values in mm, position (a, b) in the cut's axes.\n")
        f.write("\n".join(rep) + "\n")
    print("\n".join(rep))


if __name__ == "__main__":
    sys.exit(main())
