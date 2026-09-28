# Renders the pot filmstrips from pot.blend into the skin.
# In Blender, with pot.blend open: exec(open(bpy.path.abspath("//render_pot.py")).read())
# Frame 0 points at 7:30, frame 127 at 4:30, as the skin's knob kind expects.
import bpy, bmesh, math, os
import numpy as np

FRAMES = 128
# px square (ortho_scale 2.0 fills the frame either way), then the groove's inner and outer end
# as a fraction of the half-width. The old small strip drew a shorter pointer than a straight
# scale of the large one, and the same ~2.4 px width at both sizes; the groove keeps both.
SIZES = {"rotary_large": (56, 0.09, 0.52), "rotary_small": (44, 0.14, 0.50)}
GROOVE_PX = 2.6     # width across the top of the cap
GROOVE_DEPTH = 0.12 # scene units
CAP_TOP = 0.30

sc = bpy.context.scene
cap = bpy.data.objects["Cap"]
cut = bpy.data.objects["PointerGroove"]

def shape_groove(px, r0, r1):
    # a capsule lying along the pointer, sunk so its chord at the cap top is GROOVE_PX wide;
    # built in the cap's local units since it is parented to the cap
    hw = GROOVE_PX / px                        # half width in scene units (half-width = px/2)
    rho = (hw ** 2 + GROOVE_DEPTH ** 2) / (2 * GROOVE_DEPTH)
    a, b = r0 + hw, r1 - hw
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=48, v_segments=24, radius=rho)
    s = cap.scale.x
    for v in bm.verts:
        v.co.y += b if v.co.y > 0 else a
        v.co.x /= s
        v.co.y /= s
        v.co.z += CAP_TOP + rho - GROOVE_DEPTH
    bm.to_mesh(cut.data)
    bm.free()

here = bpy.path.abspath("//")
out_dir = globals().get("OUT_DIR") or os.path.normpath(os.path.join(here, "..", "plugin", "skin", "images", "knobs"))
tmp = os.path.join(bpy.app.tempdir, "pot_frame.png")
sc.render.image_settings.file_format = 'PNG'
sc.render.image_settings.color_mode = 'RGBA'
sc.render.filepath = tmp

for name, (px, r0, r1) in SIZES.items():
    sc.render.resolution_x = sc.render.resolution_y = px
    shape_groove(px, r0, r1)
    strip = []
    for k in range(FRAMES):
        cap.rotation_euler.z = math.radians(135 - 270 * k / (FRAMES - 1))
        bpy.ops.render.render(write_still=True)
        img = bpy.data.images.load(tmp)
        a = np.array(img.pixels[:], dtype=np.float32).reshape(px, px, 4)[::-1]
        bpy.data.images.remove(img)
        strip.append(a)
    s = np.concatenate(strip)[::-1]
    out = bpy.data.images.new(name, px, px * FRAMES, alpha=True)
    out.pixels[:] = s.ravel()
    path = os.path.join(out_dir, name + ".png")
    out.filepath_raw = path
    out.file_format = 'PNG'
    out.save()
    bpy.data.images.remove(out)
    print("wrote", path)

cap.rotation_euler.z = 0
sc.render.resolution_x = sc.render.resolution_y = 56
