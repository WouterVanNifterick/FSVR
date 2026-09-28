# Renders the pitch and mod wheel filmstrip from wheel.blend into the skin as faders/wheel.png.
# In Blender, with wheel.blend open: exec(open(bpy.path.abspath("//render_wheel.py")).read())
# 128 tiles side by side (the skin's "axis": "x"); frame 0 has the groove low, frame 127 high,
# 33 px either side of the centre as in the old art.
import bpy, math, os
import numpy as np

FRAMES = 128
W, H = 17, 92
TRAVEL_PX = 33
DRUM_PX = 48       # drum radius; must match the Drum mesh in wheel.blend

sc = bpy.context.scene
drum = bpy.data.objects["Drum"]
here = bpy.path.abspath("//")
path = os.path.normpath(os.path.join(here, "..", "plugin", "skin", "images", "faders", "wheel.png"))
tmp = os.path.join(bpy.app.tempdir, "wheel_frame.png")
sc.render.resolution_x, sc.render.resolution_y = W, H
sc.render.image_settings.file_format = 'PNG'
sc.render.image_settings.color_mode = 'RGBA'
sc.render.filepath = tmp

theta = math.asin(TRAVEL_PX / DRUM_PX)
tiles = []
for k in range(FRAMES):
    # the drum turns evenly with the value, so the groove's height follows a sine, as a real wheel's does
    drum.rotation_euler.x = theta - 2 * theta * k / (FRAMES - 1)
    bpy.ops.render.render(write_still=True)
    img = bpy.data.images.load(tmp)
    tiles.append(np.array(img.pixels[:], dtype=np.float32).reshape(H, W, 4))
    bpy.data.images.remove(img)
drum.rotation_euler.x = 0

s = np.concatenate(tiles, axis=1)
out = bpy.data.images.new("wheel", W * FRAMES, H, alpha=True)
out.pixels[:] = s.ravel()
out.filepath_raw = path
out.file_format = 'PNG'
out.save()
bpy.data.images.remove(out)
print("wrote", path)
