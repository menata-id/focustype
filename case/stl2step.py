#!/usr/bin/env python3
"""Convert the printed STL files to STEP (.stp): <type>/stl/{housing,cover,buttons_print}.stl -> <type>/step/*.stp.

An STL is a triangle mesh, so the STEP is a faceted solid, not a true CAD model: every triangle is one face (a housing has about 6000 of them), and round things (screw
holes, corner radii, the pins) stay polygons with the same facets as the STL. Good for a slicer or for importing as a body into a CAD
tool, but you cannot select a flat face or a hole in it; to change the design, edit case.scad instead.

Run from case/ with the venv that has cadquery:  .venv/bin/python stl2step.py [classic|equal]   (no argument: both types)
"""
import os, sys
import trimesh
import cadquery as cq
from OCP.BRepBuilderAPI import BRepBuilderAPI_MakePolygon, BRepBuilderAPI_MakeFace, BRepBuilderAPI_Sewing, BRepBuilderAPI_MakeSolid
from OCP.BRepCheck import BRepCheck_Analyzer
from OCP.TopoDS import TopoDS
from OCP.TopAbs import TopAbs_SHELL
from OCP.TopExp import TopExp_Explorer
from OCP.gp import gp_Pnt
from OCP.TopoDS import TopoDS_Compound
from OCP.BRep import BRep_Builder
from OCP.ShapeFix import ShapeFix_Solid, ShapeFix_Shape
from OCP.BRepGProp import BRepGProp
from OCP.GProp import GProp_GProps

HERE = os.path.dirname(os.path.abspath(__file__))
PARTS = ("housing", "cover", "buttons_print")      # the files that are printed


def stl_to_solid(path):
    m = trimesh.load(path)
    m.merge_vertices()
    # zero-area triangles (three collinear points, left by OpenSCAD's CGAL export) cannot be faces: drop them, the sewing below
    # closes the gap they leave (it is on a straight edge, so the solid does not change)
    m.update_faces(m.area_faces > 1e-9)
    sew = BRepBuilderAPI_Sewing(1e-3)
    for tri in m.faces:
        poly = BRepBuilderAPI_MakePolygon()
        for i in tri:
            poly.Add(gp_Pnt(*[float(c) for c in m.vertices[i]]))
        poly.Close()
        sew.Add(BRepBuilderAPI_MakeFace(poly.Wire()).Face())
    sew.Perform()
    # one STL can hold several separate bodies (buttons_print has three pins): one solid per shell, in a compound
    comp = TopoDS_Compound()
    BRep_Builder().MakeCompound(comp)
    exp = TopExp_Explorer(sew.SewedShape(), TopAbs_SHELL)
    while exp.More():
        solid = BRepBuilderAPI_MakeSolid(TopoDS.Shell_s(exp.Current())).Solid()
        fix = ShapeFix_Solid(solid)            # make sure the shell is oriented outwards
        fix.Perform()
        heal = ShapeFix_Shape(fix.Solid())      # and close small gaps
        heal.Perform()
        BRep_Builder().Add(comp, heal.Shape())
        exp.Next()
    shape = comp
    # Merging coplanar triangles (ShapeUpgrade_UnifySameDomain) was tried: it crashes OpenCASCADE 7.9 on these meshes, so every
    # triangle stays one face.
    return shape, m


def main():
    types = sys.argv[1:] or ["classic", "equal"]
    for t in types:
        os.makedirs(os.path.join(HERE, t, "step"), exist_ok=True)
        for p in PARTS:
            src = os.path.join(HERE, t, "stl", p + ".stl")
            dst = os.path.join(HERE, t, "step", p + ".stp")
            shape, m = stl_to_solid(src)
            props = GProp_GProps(); BRepGProp.VolumeProperties_s(shape, props)
            ok = BRepCheck_Analyzer(shape).IsValid()
            nfaces = len(cq.Shape.cast(shape).Faces())
            cq.exporters.export(cq.Workplane(obj=cq.Shape.cast(shape)), dst, exportType="STEP")
            print("%s/%s: %d triangles -> %d faces, valid=%s, volume %.1f mm3 (STL %.1f), %d bytes"
                  % (t, p, len(m.faces), nfaces, ok, props.Mass(), m.volume, os.path.getsize(dst)))


if __name__ == "__main__":
    main()
