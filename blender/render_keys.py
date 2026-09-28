# Renders the keyboard from keys.blend into the skin: keyboard/key_{c,d,e,f,g,a,b,c_top,black}.png,
# each two tiles side by side, at rest and pressed.
# In Blender, with keys.blend open: exec(open(bpy.path.abspath("//render_keys.py")).read())
#
# The piano kind in hollow/src/core/kinds.cpp places the keys itself: a 130 px octave, white keys at
# 0 19 38 56 75 94 112 and black keys at 11 33 67 87 107, each times the skin's density and rounded.
# At density 1.5 that makes the white slots 29 28 27 29 28 27 27 px wide. The old art drew every white
# key 27 px wide (26 for A), so the leftover 0 to 2 px showed as uneven gaps. Here each key is modelled
# at its slot and rendered at the slot's width, with the same 1 px gap inside every key.
import bpy, math, os
import numpy as np

DENSITY = 1.5
WHITE_AT = [0, 19, 38, 56, 75, 94, 112, 130]
BLACK_AT = [11, 33, 67, 87, 107]
WHITE_H, BLACK_W, BLACK_H = 161, 20, 99
PRESS_DEG = 3.0
PRESS_DROP = 12.0   # px a pressed key sinks, so its neighbours shade both its sides
PX = 1 / 21
NAMES = ["C", "D", "E", "F", "G", "A", "B"]

def dp(v):   # std::lround, as Skin::dp
    return int(math.floor(v * DENSITY + 0.5))

wx = [dp(a) for a in WHITE_AT]
sc = bpy.context.scene
D = bpy.data
here = bpy.path.abspath("//")
out_dir = os.path.normpath(os.path.join(here, "..", "plugin", "skin", "images", "keyboard"))
tmp = os.path.join(bpy.app.tempdir, "keys_pass.png")
sc.render.image_settings.file_format = 'PNG'
sc.render.image_settings.color_mode = 'RGBA'
sc.render.filepath = tmp

whites = NAMES + ["prevB", "nextC"]
blacks = ["Cs", "Ds", "Fs", "Gs", "As", "prevAs"]
bed = ["Keybed"]

def render(white_cam, pressed):
    for n in whites + bed:
        D.objects[n].visible_camera = white_cam
    for n in blacks:
        D.objects[n].visible_camera = not white_cam
    for n in whites + blacks:
        o = D.objects[n]
        o.rotation_euler.x = math.radians(PRESS_DEG) if n in pressed else 0
        o.location.z = o["z0"] - (PRESS_DROP * PX if n in pressed else 0)
        # sinking shows mostly as the shadows at its sides, so a pressed key also takes a slightly shaded finish
        base = "WhiteKey" if n in whites else "BlackKey"
        o.data.materials[0] = D.materials[base + ("Down" if n in pressed else "")]
    bpy.ops.render.render(write_still=True)
    img = D.images.load(tmp)
    w, h = img.size
    a = np.array(img.pixels[:], dtype=np.float32).reshape(h, w, 4)[::-1]   # top row first
    D.images.remove(img)
    return a

def save(name, rest, pressed):
    s = np.concatenate([rest, pressed], axis=1)[::-1]
    h, w = s.shape[:2]
    out = D.images.new(name, w, h, alpha=True)
    out.pixels[:] = s.ravel()
    path = os.path.join(out_dir, name + ".png")
    out.filepath_raw = path
    out.file_format = 'PNG'
    out.save()
    D.images.remove(out)
    print("wrote", path, w, h)

# white keys: at rest, then two pressed sets with no two neighbours down together
rest = render(True, set())
down = render(True, {"C", "E", "G", "B", "nextC"})
down2 = render(True, {"D", "F", "A"})
for i, n in enumerate(NAMES):
    x0, x1 = wx[i], wx[i + 1]
    p = down if n in ("C", "E", "G", "B") else down2
    save("key_" + n.lower(), rest[:WHITE_H, x0:x1], p[:WHITE_H, x0:x1])
# the last key of the keyboard: a C with no black key to its right (the next octave's C here)
x0 = wx[7]
save("key_c_top", rest[:WHITE_H, x0:x0 + wx[1]], down[:WHITE_H, x0:x0 + wx[1]])

# black keys: one picture serves all five; C sharp's
rest = render(False, set())
down = render(False, set(blacks))
bx = dp(BLACK_AT[0])
save("key_black", rest[:BLACK_H, bx:bx + BLACK_W], down[:BLACK_H, bx:bx + BLACK_W])

for n in whites + blacks + bed:
    D.objects[n].visible_camera = True
    D.objects[n].rotation_euler.x = 0
for n in whites + blacks:
    D.objects[n].location.z = D.objects[n]["z0"]
    D.objects[n].data.materials[0] = D.materials["WhiteKey" if n in whites else "BlackKey"]

# the keyboard's end cheeks: shadows for the skin to lay over the keys at each end, so the keyboard
# sits in the case rather than on it. Only CheekL/CheekR's shadow on the KeyShadow catcher is rendered,
# under an even overhead CheekSky; each column is averaged down the key length, where it does not
# change, to lose the sampling noise
SHADOW_W = 48
cheek = ["CheekL", "CheekR", "KeyShadow", "CheekSky"]
lit = [o for o in D.objects if o.name not in cheek + ["Camera"] and not o.hide_render]
for o in lit:
    o.hide_render = True
for n in cheek:
    D.objects[n].hide_render = False
bgn = next(n for n in sc.world.node_tree.nodes if n.type == 'BACKGROUND')
strength, samples = bgn.inputs['Strength'].default_value, sc.cycles.samples
bgn.inputs['Strength'].default_value, sc.cycles.samples = 0, 2048
sh = render(True, set())
bgn.inputs['Strength'].default_value, sc.cycles.samples = strength, samples
for o in lit:
    o.hide_render = False
for n in cheek:
    D.objects[n].hide_render = True
alpha = sh[10:WHITE_H - 10, :, 3].mean(axis=0)
# the shadow runs further than SHADOW_W; take its last 12 px to nothing so the plate's edge never shows
taper = np.clip(np.arange(SHADOW_W)[::-1] / 12.0, 0, 1)
taper = taper * taper * (3 - 2 * taper)
for name, cols in (("shadow_left", alpha[:SHADOW_W] * taper), ("shadow_right", alpha[wx[7] + wx[1] - SHADOW_W:wx[7] + wx[1]] * taper[::-1])):
    a = np.zeros((WHITE_H, SHADOW_W, 4), dtype=np.float32)
    a[..., 3] = cols
    out = D.images.new(name, SHADOW_W, WHITE_H, alpha=True)
    out.pixels[:] = a[::-1].ravel()
    path = os.path.join(out_dir, name + ".png")
    out.filepath_raw = path
    out.file_format = 'PNG'
    out.save()
    D.images.remove(out)
    print("wrote", path)
for n in whites + blacks + bed:   # render() above hid the black keys from the camera
    D.objects[n].visible_camera = True
