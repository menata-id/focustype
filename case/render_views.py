#!/usr/bin/env python3
"""PNG views of a case type without a display: <type>/renders/{assembly_front,assembly_back,housing_front,housing_back,cover_back,exploded,cutaway}.png

OpenSCAD's own PNG export needs a display (render.sh uses it when there is one). This draws the same kind of views offscreen with VTK
(EGL), from <type>/stl/ and from the board, cell and foam parts exported by openscad. Colours as in case.scad assembly(): housing dark grey,
cover silver, buttons red, foam pink, board green, cell orange. Cameras are close to the ones in render.sh but not identical.

Run from case/ with the venv (cadquery brings VTK):  .venv/bin/python render_views.py [classic|equal]   (no argument: both)
"""
import math, os, subprocess, sys, tempfile
import vtk

HERE = os.path.dirname(os.path.abspath(__file__))
SIZE = (1200, 900)
COLORS = {"housing": (0.41, 0.41, 0.41), "cover": (0.75, 0.75, 0.75), "buttons": (1.0, 0.39, 0.28), "foam": (1.0, 0.41, 0.71),
          "pads": (1.0, 0.41, 0.71), "board": (0.18, 0.55, 0.34), "battery": (1.0, 0.65, 0.0)}


def scad_stl(t, part, tmp):
    f = os.path.join(tmp, part + ".stl")
    subprocess.run(["openscad", "-o", f, "-D", 'part="%s"' % part, os.path.join(HERE, t, t + ".scad")],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return f


def actor(path, colour, dz=0.0, dy=0.0, clip_x=None):
    r = vtk.vtkSTLReader(); r.SetFileName(path); r.Update()
    src = r.GetOutputPort()
    if clip_x is not None:                                  # keep X <= clip_x, to look inside
        pl = vtk.vtkPlane(); pl.SetOrigin(clip_x, 0, 0); pl.SetNormal(-1, 0, 0)
        c = vtk.vtkClipClosedSurface(); c.SetInputConnection(src); c.SetClippingPlanes(vtk.vtkPlaneCollection()); c.GetClippingPlanes().AddItem(pl)
        c.SetGenerateFaces(1); c.Update(); src = c.GetOutputPort()
    n = vtk.vtkPolyDataNormals(); n.SetInputConnection(src); n.SetFeatureAngle(35); n.SplittingOn(); n.Update()
    m = vtk.vtkPolyDataMapper(); m.SetInputConnection(n.GetOutputPort())
    a = vtk.vtkActor(); a.SetMapper(m)
    p = a.GetProperty(); p.SetColor(*colour); p.SetAmbient(0.25); p.SetDiffuse(0.75); p.SetSpecular(0.15)
    a.SetPosition(0, dy, dz)
    return a


def shoot(actors, name, out, target, rx, rz, dist):
    ren = vtk.vtkRenderer(); ren.SetBackground(1, 1, 1)
    for a in actors:
        ren.AddActor(a)
    w = vtk.vtkRenderWindow(); w.SetOffScreenRendering(1); w.SetMultiSamples(8); w.AddRenderer(ren); w.SetSize(*SIZE)
    rx_, rz_ = math.radians(rx), math.radians(rz)
    d = (math.sin(rx_) * math.sin(rz_), -math.sin(rx_) * math.cos(rz_), math.cos(rx_))        # from the target towards the camera
    cam = ren.GetActiveCamera()
    cam.SetFocalPoint(*target); cam.SetPosition(*[target[i] + dist * d[i] for i in range(3)])
    cam.SetViewUp(-math.sin(rz_), math.cos(rz_), 0) if rx < 90 else cam.SetViewUp(math.sin(rz_), -math.cos(rz_), 0)
    cam.SetViewAngle(25)
    ren.ResetCameraClippingRange()
    light = vtk.vtkLightKit(); light.AddLightsToRenderer(ren)
    w.Render()
    f = vtk.vtkWindowToImageFilter(); f.SetInput(w); f.Update()
    p = vtk.vtkPNGWriter(); p.SetFileName(os.path.join(out, name + ".png")); p.SetInputConnection(f.GetOutputPort()); p.Write()


def main():
    for t in sys.argv[1:] or ["classic", "equal"]:
        out = os.path.join(HERE, t, "renders"); os.makedirs(out, exist_ok=True)
        with tempfile.TemporaryDirectory() as tmp:
            st = lambda n: os.path.join(HERE, t, "stl", n + ".stl")
            parts = {"housing": st("housing"), "cover": st("cover"), "buttons": st("buttons"),
                     "foam": scad_stl(t, "foam_gasket", tmp), "pads": scad_stl(t, "cell_pads", tmp), "board": scad_stl(t, "board_parts", tmp), "battery": scad_stl(t, "battery", tmp)}
            def asm(**kw): return [actor(parts[k], COLORS[k], **kw.get(k, {})) for k in parts]
            shoot(asm(), "assembly_front", out, (0, 0, 0), 55, 25, 330)
            shoot(asm(), "assembly_back", out, (0, 0, -5), 125, 205, 330)
            shoot([actor(parts["housing"], COLORS["housing"])], "housing_front", out, (0, 0, 0), 50, 20, 330)
            shoot([actor(parts["housing"], COLORS["housing"])], "housing_back", out, (0, 0, 0), 125, 205, 330)
            shoot([actor(parts["cover"], COLORS["cover"])], "cover_back", out, (0, 0, -5), 125, 205, 330)
            shoot(asm(**{"housing": {"dz": 40}, "foam": {"dz": 22}, "pads": {"dz": -26}, "buttons": {"dz": 40, "dy": 22}, "battery": {"dz": -14}, "cover": {"dz": -38}}),
                  "exploded", out, (0, 0, 8), 70, 25, 450)
            shoot(asm(**{k: {"clip_x": 2.0} for k in parts}), "cutaway", out, (-24, 0, -4), 62, 70, 280)
        print("%s: wrote %s" % (t, ", ".join(sorted(f for f in os.listdir(out) if f.endswith(".png") and f.split(".")[0] in
              ("assembly_front", "assembly_back", "housing_front", "housing_back", "cover_back", "exploded", "cutaway")))))


if __name__ == "__main__":
    main()
