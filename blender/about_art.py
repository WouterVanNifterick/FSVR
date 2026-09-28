# The About dialog's art and the topbar's musica.studio wordmark, in the installer's style: Analog
# Whispers (its free edition, kept beside the Installer, ../../Installer, not in this repo) rasterized
# into Hollow glyph strips and button tiles, black and the installer's grey on white. The free edition
# has letters, digits, space and .,-!? only; the few marks the About text needs (BORROWED) come from the
# licensed edition, and every other code in the strips is blank.
#   python blender/about_art.py            (from the repo root, needs Pillow)
#
#   fonts/aw_<ink>_<px>          Latin-1 glyph strips with a marker row (hollow/docs/skin-format.md, Fonts)
#   branding/fsvr_logo_black     the FSVR logo, recoloured black, one tile
#   branding/musica_studio       "musica." black, "studio" grey, x2 tiles: rest, hover (all grey)
#   branding/musica_studio_small the same at the topbar's size
#   buttons/about_button         196 x 45, x2 tiles: the installer's pale button, and its hover
#   buttons/about_close          32 x 32, x2 tiles: clear, and the installer's pale hover behind its x
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
SKIN = ROOT / 'plugin' / 'skin'
TTF = ROOT.parent / 'Installer' / 'Analog Whispers FREE.ttf'
LICENSED = ROOT.parent / 'Installer' / 'Analog Whispers.ttf'
BORROWED = "'+;:/()"
# the installer's colours (Installer/ui.go)
BLACK, GREY, PALE, HOVER = (0, 0, 0), (0x78, 0x74, 0x78), (0xe8, 0xe8, 0xe8), (0xd8, 0xd8, 0xd8)


def face(px):
    return ImageFont.truetype(str(TTF), px)


def glyph_strip(px, ink, out):
    free, lic = face(px), ImageFont.truetype(str(LICENSED), px)
    asc, desc = free.getmetrics()
    h = asc + desc
    cells = []
    for c in range(256):
        ch = chr(c)
        f = lic if ch in BORROWED else free
        printable = 32 <= c < 127 or c >= 160
        w = max(1, round(f.getlength(ch))) if printable else 1
        im = Image.new('L', (w, h), 0)
        if printable and c != 32:
            ImageDraw.Draw(im).text((0, asc - f.getmetrics()[0]), ch, font=f, fill=255)   # on the free edition's baseline
        cells.append(im)
    W = sum(i.width for i in cells)
    strip = Image.new('RGBA', (W, h + 1), (0, 0, 0, 0))
    x = 0
    for im in cells:
        strip.putpixel((x, 0), (255, 255, 255, 255))   # the marker row: a glyph starts here
        solid = Image.new('RGBA', im.size, ink + (255,))
        strip.paste(solid, (x, 1), im)
        x += im.width
    strip.save(SKIN / 'fonts' / f'{out}.png')


def words(parts, px, pad=(0, 0)):
    """parts: [(text, ink)] set on one baseline, trimmed to the ink, then padded."""
    f = face(px)
    W = round(sum(f.getlength(t) for t, _ in parts)) + 4
    asc, desc = f.getmetrics()
    im = Image.new('RGBA', (W, asc + desc + 4), (0, 0, 0, 0))
    d, x = ImageDraw.Draw(im), 0
    for t, ink in parts:
        d.text((x, 0), t, font=f, fill=ink + (255,))
        x += f.getlength(t)
    im = im.crop(im.getchannel('A').getbbox())
    out = Image.new('RGBA', (im.width + 2 * pad[0], im.height + 2 * pad[1]), (0, 0, 0, 0))
    out.paste(im, pad)
    return out


def tiles(*ims):   # stacked top to bottom, the skin's default axis
    w, h = max(i.width for i in ims), max(i.height for i in ims)
    out = Image.new('RGBA', (w, h * len(ims)), (0, 0, 0, 0))
    for n, i in enumerate(ims):
        out.paste(i, (0, n * h))
    return out


def flat(w, h, rgb):
    return Image.new('RGBA', (w, h), rgb + (255,))


def wordmark(px, out):
    rest = words([('musica.', BLACK), ('studio', GREY)], px, (2, 2))
    hover = words([('musica.', GREY), ('studio', GREY)], px, (2, 2))
    tiles(rest, hover).save(SKIN / 'images' / 'branding' / f'{out}.png')
    return rest.size


if __name__ == '__main__':
    for ink, rgb in (('black', BLACK), ('grey', GREY)):
        for px in (13, 16, 24):
            glyph_strip(px, rgb, f'aw_{ink}_{px}')
    glyph_strip(32, BLACK, 'aw_black_32')

    logo = Image.open(SKIN / 'images' / 'branding' / 'fsvr_logo.png').convert('RGBA')
    logo = logo.crop((0, 0, logo.width, logo.height // 2))   # the rest tile
    black = Image.new('RGBA', logo.size, (0, 0, 0, 255))
    black.putalpha(logo.getchannel('A'))
    black.save(SKIN / 'images' / 'branding' / 'fsvr_logo_black.png')

    print('musica_studio', wordmark(24, 'musica_studio'))
    print('musica_studio_small', wordmark(20, 'musica_studio_small'))
    tiles(flat(196, 45, PALE), flat(196, 45, HOVER)).save(SKIN / 'images' / 'buttons' / 'about_button.png')
    tiles(Image.new('RGBA', (32, 32), (0, 0, 0, 0)), flat(32, 32, PALE)).save(SKIN / 'images' / 'buttons' / 'about_close.png')
