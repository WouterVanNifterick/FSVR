# Renders the slider parts from slider.blend into the skin:
#   faders/pot_cap.png (the grabber), faders/track_narrow.png and faders/track_long_narrow.png.
# In Blender, with slider.blend open: exec(open(bpy.path.abspath("//render_slider.py")).read())
import bpy, bmesh, os
import numpy as np

PX = 1 / 21        # one pixel in scene units, for every part
CAP = 42           # the skin lays the sliders out around a 42 px grabber
CAP_PX = 18.5      # grabber radius in px, leaving room for its shadow
TRACK_W = 20
# The skin draws the tracks 1:1 at their own size, so the sizes are the old art's, and so are the rows of
# the first, middle and last ticks (the heavier ones). The old art spaced ticks 4.5 px apart, which cannot
# land on whole pixels: half of them smeared across two rows. These are every 4 px on whole rows instead.
TRACKS = {"track_narrow": (173, 14, 37), "track_long_narrow": (188, 17, 39)}
TICK_X = (6, 14)        # tick span, px from the left edge
TICK_X_HEAVY = (4, 16)
WELL_R = 9.8            # half width of the slot, px (the WellMat shader's W_PX)
WELL_END = 0.2          # gap between the slot's round ends and the image ends, px

sc = bpy.context.scene
D = bpy.data
here = bpy.path.abspath("//")
out_dir = os.path.normpath(os.path.join(here, "..", "plugin", "skin", "images", "faders"))
tmp = os.path.join(bpy.app.tempdir, "slider_part.png")
sc.render.image_settings.file_format = 'PNG'
sc.render.image_settings.color_mode = 'RGBA'
sc.render.filepath = tmp
cam = sc.camera

def render(w, h):
    sc.render.resolution_x, sc.render.resolution_y = w, h
    cam.data.ortho_scale = max(w, h) * PX
    bpy.ops.render.render(write_still=True)
    img = D.images.load(tmp)
    a = np.array(img.pixels[:], dtype=np.float32).reshape(h, w, 4)
    D.images.remove(img)
    return a

def save(a, name):
    h, w = a.shape[:2]
    out = D.images.new(name, w, h, alpha=True)
    out.pixels[:] = a.ravel()
    path = os.path.join(out_dir, name + ".png")
    out.filepath_raw = path
    out.file_format = 'PNG'
    out.save()
    D.images.remove(out)
    print("wrote", path)

def show(grabber):
    D.collections["GrabberSet"].hide_render = not grabber
    D.collections["TrackSet"].hide_render = grabber

# grabber
show(True)
a = render(CAP, CAP)
# The drop shadow runs past the frame; fade it to nothing at the edge so it never shows a square clip.
yy, xx = np.mgrid[0:CAP, 0:CAP] + 0.5
r = np.hypot(xx - CAP / 2, yy - CAP / 2)
t = np.clip((CAP / 2 - r) / (CAP / 2 - CAP_PX - 0.5), 0, 1)
a[..., 3] *= np.where(r > CAP_PX + 0.5, t * t * (3 - 2 * t), 1)
save(a, "pot_cap")

# tracks: the pot bezel's well drawn as a slot (WellMat), with flat ticks in the pot's tick colour
show(False)
well = D.objects["TrackWell"]
ticks = D.objects["TrackTicks"]
for name, (h, first, n) in TRACKS.items():
    half = h / 2 * PX
    bm = bmesh.new()   # the well covers the whole image; its shader draws the slot inside
    bmesh.ops.create_grid(bm, x_segments=1, y_segments=1, size=1.0)
    bmesh.ops.scale(bm, vec=(TRACK_W / 2 * PX, half, 1), verts=bm.verts)
    bm.to_mesh(well.data)
    bm.free()
    # straight part of the slot: its round ends sit WELL_END px in from the image ends
    D.materials["WellMat"].node_tree.nodes["HalfLen"].outputs[0].default_value = (h / 2 - WELL_END - WELL_R) * PX
    bm = bmesh.new()
    for i in range(n):
        heavy = i in (0, n // 2, n - 1)
        x0, x1 = TICK_X_HEAVY if heavy else TICK_X
        y = h / 2 - (first + 4 * i)                   # top edge of the tick's pixel row
        vs = [bm.verts.new(((x - TRACK_W / 2) * PX, yy * PX, 0)) for x, yy in ((x0, y), (x1, y), (x1, y - 1), (x0, y - 1))]
        f = bm.faces.new(vs)
        f.material_index = 1 if heavy else 0
    bm.to_mesh(ticks.data)
    bm.free()
    save(render(TRACK_W, h), name)

show(True)
sc.render.resolution_x = sc.render.resolution_y = CAP
cam.data.ortho_scale = CAP * PX
