# Renders the navigator (views/sidebar.json) from nav.blend into the skin.
# In Blender, with nav.blend open: exec(open(bpy.path.abspath("//render_nav.py")).read())
#
#   panels/nav_frame     176 x 473  the pot bezel's well as a rounded rectangle round the whole navigator
#   panels/nav_column    155 x 448  a second, inner well round the lower buttons
#   buttons/nav_big      143 x 35   x3 tiles: rest, pressed, hover (the seven page buttons)
#   buttons/nav_small    26 x 26    x4 tiles: off, on, off pressed/hovered, on pressed/hovered
#   icons/nav_pointer    8 x 14     the current page's LED
#   icons/nav_md_*       34 x 35    the page icons' glyphs, lifted off their old dark squares
#                                   (nav_icons/, the originals) to sit in the big buttons' sockets
import bpy, os, glob
import numpy as np

PX = 1 / 21
sc = bpy.context.scene
D = bpy.data
here = bpy.path.abspath("//")
skin = os.path.normpath(os.path.join(here, "..", "plugin", "skin", "images"))
tmp = os.path.join(bpy.app.tempdir, "nav_part.png")
sc.render.image_settings.file_format = 'PNG'
sc.render.image_settings.color_mode = 'RGBA'
sc.render.filepath = tmp

def load(path):
    img = D.images.load(path)
    w, h = img.size
    a = np.array(img.pixels[:], dtype=np.float32).reshape(h, w, 4)[::-1]   # top row first
    D.images.remove(img)
    return a

def render(coll, w, h):
    for c in sc.collection.children:
        c.hide_render = c.name != coll
    sc.render.resolution_x, sc.render.resolution_y = w, h
    sc.camera.data.ortho_scale = max(w, h) * PX
    bpy.ops.render.render(write_still=True)
    return load(tmp)

def save(a, name):
    h, w = a.shape[:2]
    out = D.images.new(os.path.basename(name), w, h, alpha=True)
    out.pixels[:] = a[::-1].ravel()
    path = os.path.join(skin, name + ".png")
    out.filepath_raw = path
    out.file_format = 'PNG'
    out.save()
    D.images.remove(out)
    print("wrote", path, w, h)

def tiles(coll, obj, mats, w, h):
    o = D.objects[obj]
    base = o.data.materials[0]
    out = []
    for m in mats:
        o.data.materials[0] = D.materials[m]
        out.append(render(coll, w, h))
    o.data.materials[0] = base
    return np.concatenate(out, axis=0)

save(render("NavFrame", 176, 473), "panels/nav_frame")
save(render("NavColumn", 155, 448), "panels/nav_column")
save(tiles("NavBig", "BigBody", ["Slate", "SlateDown", "SlateHover"], 143, 35), "buttons/nav_big")
save(tiles("NavSmall", "SmallBody", ["Slate", "SlateOn", "SlateHover", "SlateOnHover"], 26, 26), "buttons/nav_small")
save(render("NavLed", 8, 14), "icons/nav_pointer")

# icon glyphs: the old icons are light line art on a dark square; keep the art, drop the square
for src in sorted(glob.glob(os.path.join(here, "nav_icons", "nav_md_*.png"))):
    a = load(src)
    lum = a[..., :3] @ np.array([0.299, 0.587, 0.114], dtype=np.float32)
    g = np.zeros_like(a)
    g[..., :3] = np.array([0.90, 0.93, 0.95], dtype=np.float32)
    g[..., 3] = np.clip((lum - 0.30) / (0.85 - 0.30), 0, 1) * a[..., 3]
    save(g, "icons/" + os.path.splitext(os.path.basename(src))[0])

for c in sc.collection.children:
    c.hide_render = False
