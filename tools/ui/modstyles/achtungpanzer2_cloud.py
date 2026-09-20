#!/usr/bin/env python3
# Generates the Achtung Panzer 2 styles of the two cloud screens from the base
# layouts: same elements, ids and script, drawn with the mod's own pictures
# (the menu paper, its buttons and the paper strips), referenced by name only.
#
#   python3 tools/ui/modstyles/achtungpanzer2_cloud.py \
#       Data/UI/CloudCredentials.xml Data/UI/ModStyles/achtungpanzer2/CloudCredentials.xml
#   python3 tools/ui/modstyles/achtungpanzer2_cloud.py \
#       Data/UI/CloudBackups.xml Data/UI/ModStyles/achtungpanzer2/CloudBackups.xml
import sys
import xml.etree.ElementTree as ET

src, dst = sys.argv[1], sys.argv[2]
raw = open(src, 'rb').read().decode('utf-8-sig')
root = ET.fromstring(raw[raw.index('<base'):])

SHEET = ('ui\\IntermissionTextures\\menu-popups', (0, 0, 497, 516))   # the menu paper, tabs cropped off
STRIP = ('UI\\Textures\\dtf-ui-misc', (5, 6, 432, 30))                # a paper strip, as the settings tab buttons use
CLEAR = ('ui\\intermissiontextures\\back-mainmenu', (439, 788, 324, 50))   # a transparent patch of the desk
CHEVRON_UP = ('ui\\IntermissionTextures\\back-chapter', (636, 769, 45, 58))
CHEVRON_DOWN = ('ui\\IntermissionTextures\\back-chapter', (591, 769, 45, 58))
THUMB = ('ui\\slider', (108, 0, 40, 33))
BUTTONS = 'UI\\IntermissionTextures\\buttons-ap2'
HEADER, BLACK, RED_HI, RED_PUSH, GREY = '0xff815335', '0xff000000', '0xff841a17', '0xffb72420', '0xff7a6a55'
# The icons in buttons-ap2 come in a silver, a gold and a red cut of each shape;
# an appearance's Color modulates them, so the mod's own field green - the green
# of the leather tab its popups accept on - can be had from the silver one.
CHECK, CROSS = (0, 40), (120, 40)
GREEN, GREEN_HI = '0xff5a7a3c', '0xff8cb45e'
GOLD_CROSS, RED_CROSS = (160, 40), (200, 40)
# The paper prints its own frame. Everything the dialog writes has to start
# inside that line, not on the sheet's edge where the base layout's panel had
# its border. Measured off the sheet: the line is at x=41 of the 736 the
# dialog is wide, and the frame's other side at 655.
MARGIN, CONTENT_WIDTH = 56, 600


def kids(e):
    return e.find('Children')


def find(parent, eid):
    for it in parent.iter('item'):
        if it.get('ElementID') == str(eid):
            return it
    return None


def set_pos(it, x, y, w=None, h=None):
    p = it.find('WindowPos')
    if p is None:
        p = ET.Element('WindowPos')
        it.insert(0, p)
    p.set('x', str(x)); p.set('y', str(y))
    if w is not None:
        s = it.find('WindowSize')
        if s is None:
            s = ET.SubElement(it, 'WindowSize')
        s.set('x', str(w)); s.set('y', str(h))


def place(it, x=None, y=None, w=None, h=None):
    """Moves or resizes one element, leaving the coordinates not given alone."""
    if it is None:
        return
    p = it.find('WindowPos'); size = it.find('WindowSize')
    if x is not None:
        p.set('x', str(x))
    if y is not None:
        p.set('y', str(y))
    if w is not None:
        size.set('x', str(w))
    if h is not None:
        size.set('y', str(h))


def hide(it):
    set_pos(it, 0, 0, 0, 0)
    it.set('VisibleFlag', '0')


def appearance(tag, tex, maps, text_color=None, tiles=None, tint=None):
    a = ET.Element(tag, {'Color': tint or '0xffffffff', 'Specular': '0xff000000'})
    if text_color:
        a.set('TextColor', text_color)
    if tiles:
        tr = ET.SubElement(a, 'TileRects')
        for rect, size, m in tiles:
            t = ET.SubElement(tr, 'item')
            ET.SubElement(t, 'rect', dict(zip(('x1', 'y1', 'x2', 'y2'), map(str, rect))))
            ET.SubElement(t, 'size', dict(zip(('x', 'y'), map(str, size))))
            ET.SubElement(t, 'map', dict(zip(('x1', 'y1', 'x2', 'y2'), map(str, m))))
    elif maps is not None:
        ET.SubElement(a, 'Maps', dict(zip(('x1', 'y1', 'x2', 'y2'), map(str, maps))))
    if tex is not None:
        ET.SubElement(a, 'Texture').text = tex
    return a


def restyle(it, looks):
    """Replaces every state's appearances; keeps sounds, text keys and tooltips."""
    states = it.find('States')
    if states is None:
        return
    for st in states.findall('item'):
        for child in list(st):
            if child.tag.startswith('Appearance'):
                st.remove(child)
        for i, (name, spec) in enumerate(looks.items()):
            tex, maps, color = spec[:3]
            st.insert(i, appearance('Appearance' + name, tex, maps, color,
                                    spec[3] if len(spec) > 3 else None,
                                    spec[4] if len(spec) > 4 else None))


def plain(it, color=BLACK):
    """Text only, no picture: labels and captions on the paper."""
    restyle(it, {n: (None, None, color) for n in ('Normal', 'Highlighted', 'Pushed', 'Disabled')})


def strip(it, color=BLACK):
    """A paper strip, as the settings screen's Config.../Backups... buttons."""
    restyle(it, {'Normal': (*STRIP, color), 'Highlighted': (*STRIP, RED_HI),
                 'Pushed': (*STRIP, RED_PUSH), 'Disabled': (*STRIP, GREY)})


def tab_button(it, icon_normal, icon_hi, tint=None, tint_hi=None):
    """A check or a cross on the paper. The sheet's green and red tabs belong
    to the menu paper's lower edge, which these dialogs crop off, so the icon
    stands on the paper itself and carries the colour the tab would have."""
    def icon(m):
        return [((24, 8, 64, 48), (40, 40), (m[0], m[1], 40, 40))]
    size = it.find('WindowSize')
    size.set('x', '88'); size.set('y', '56')
    restyle(it, {'Normal': (BUTTONS, None, None, icon(icon_normal), tint),
                 'Highlighted': (BUTTONS, None, None, icon(icon_hi), tint_hi or tint),
                 'Pushed': (BUTTONS, None, None, icon(icon_hi), tint_hi or tint)})


# The dialog's frame layers: the mod's paper instead of the metal plate, and
# the inner plate, the engraved frame and the bottom bar taken out from under
# it - the paper carries its own frame.
for eid in (20021, 31420, 20030):
    it = find(root, eid)
    if it is not None:
        hide(it)
sheet = find(root, 20020)
if sheet is not None:
    restyle(sheet, {n: (SHEET[0], SHEET[1], None) for n in ('Normal', 'Highlighted', 'Pushed')})

# The dialog caption in the menu paper's brown.
title = find(root, 20000)
if title is not None:
    title.set('FontSize', '1')
    title.set('TextColor', HEADER)
    set_pos(title, MARGIN, 24, 500, 30)
    plain(title, HEADER)

if 'CloudCredentials' in src:
    for eid in (3001, 3002, 3003, 3004, 3005, 3006, 3007, 3101, 3102):     # field labels and the two notes
        plain(find(root, eid))
    for eid in (3001, 3002, 3003, 3004, 3005, 3006, 3007):                 # clear of the printed frame,
        place(find(root, eid), x=MARGIN, w=247)                            # and still short of the edits
    for eid in (3101, 3102):                                               # rclone's path and the status line
        place(find(root, eid), x=MARGIN, w=CONTENT_WIDTH)
    for eid in (2001, 2002, 2003, 2004, 2005, 2006, 2007):                 # the edit fields
        strip(find(root, eid))
    for eid in (4001, 4002, 4003, 4004, 4005, 4006, 4007):                 # the per-field buttons
        strip(find(root, eid))
    restyle(find(root, 4100), {n: (*CHEVRON_UP, None) for n in ('Normal', 'Highlighted', 'Pushed', 'Disabled')})
    restyle(find(root, 4101), {n: (*CHEVRON_DOWN, None) for n in ('Normal', 'Highlighted', 'Pushed', 'Disabled')})
    for eid in (10020, 10021, 10022, 10023):                               # service, advanced, test, forget
        strip(find(root, eid))
    tab_button(find(root, 10002), CHECK, CHECK, GREEN, GREEN_HI)           # accept: the tab's green
    tab_button(find(root, 10001), RED_CROSS, GOLD_CROSS)                   # cancel: red, then yellow
else:
    for eid in (3102, 3103):                                               # the warning and the hint
        plain(find(root, eid))
    # Both notes sat on the paper's left edge, the first of them across the
    # caption and the rule printed under it. Below that rule, and inside the
    # frame, they need two lines each, so the list gives up the room and
    # keeps its own bottom.
    place(find(root, 3102), x=MARGIN, y=128, w=CONTENT_WIDTH, h=44)
    place(find(root, 2100), y=168, h=216)
    place(find(root, 3103), x=MARGIN, y=392, w=CONTENT_WIDTH, h=52)
    place(find(root, 10030), x=MARGIN, w=280)
    place(find(root, 10031), x=376, w=280)
    lst = find(root, 2100)                                                 # the snapshot list
    if lst is not None:
        restyle(lst, {n: (*CLEAR, None) for n in ('Normal', 'Highlighted')})
        sb = kids(lst).find('item') if kids(lst) is not None else None
        if sb is not None and kids(sb) is not None:
            parts = kids(sb).findall('item')
            for part, art in zip(parts, (CHEVRON_UP, CHEVRON_DOWN, THUMB)):
                restyle(part, {n: (*art, None) for n in ('Normal', 'Highlighted', 'Pushed')})
        for eid in (10, 11, 12):                                           # the list's column headers
            head = find(lst, eid)
            if head is not None:
                plain(head)
    for eid in (10030, 10031):                                             # the restore and delete buttons
        strip(find(root, eid))
    tab_button(find(root, 10001), RED_CROSS, GOLD_CROSS)                   # cancel: red, then yellow

header = ('<!-- The %s screen restyled for the Achtung Panzer 2 mod from its own\n'
          '     pictures (the menu paper, its tab buttons and the paper strips), referenced\n'
          '     by name only. Generated from ui\\%s.xml: same elements, ids and script.\n'
          '     Used instead of it while that mod is loaded; see OpenLayoutStream in UIScreen.cpp. -->\n')
name = src.replace('\\', '/').rsplit('/', 1)[-1][:-4]
ET.indent(root, '\t')
open(dst, 'w', encoding='utf-8', newline='\r\n').write(
    (header % (name, name)) + ET.tostring(root, encoding='unicode') + '\n')
