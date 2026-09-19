#!/usr/bin/env python3
# Generates Data/UI/ModStyles/achtungpanzer2/OptionsSettings.xml from the base
# OptionsSettings.xml: same elements, ids and script, restyled with the
# Achtung Panzer 2 mod's own pictures (referenced by name only). Rerun it after
# changing the base layout:
#   python3 tools/ui/modstyles/achtungpanzer2_settings.py \
#       Data/UI/OptionsSettings.xml Data/UI/ModStyles/achtungpanzer2/OptionsSettings.xml
import sys
import xml.etree.ElementTree as ET

src, dst = sys.argv[1], sys.argv[2]
raw = open(src, 'rb').read().decode('utf-8')
root = ET.fromstring(raw[raw.index('<base'):])

X0, Y0 = 40, 66                                  # the paper sheet on the 1024x768 canvas
TABS_X, TABS_Y, TAB_STEP = 650, 274, 65          # the menu paper's ruled lines
HEADER = '0xff815335'                            # the mod's menu header brown
RED_HI, RED_PUSH, BLACK = '0xff841a17', '0xffb72420', '0xff000000'
CLEAR = ('ui\\intermissiontextures\\back-mainmenu', (439, 788, 324, 50))  # transparent, as the mod's menu buttons use
BUTTONS = 'UI\\IntermissionTextures\\buttons-ap2'


def kids(e):
    return e.find('Children')


def by_id(parent, eid):
    for it in kids(parent).findall('item'):
        if it.get('ElementID') == str(eid):
            return it
    raise KeyError(eid)


def set_pos(it, x, y, w=None, h=None):
    p = it.find('WindowPos')
    if p is None:
        p = ET.Element('WindowPos')
        it.insert(0, p)
    p.set('x', str(x)); p.set('y', str(y))
    if w is not None:
        s = it.find('WindowSize')
        s.set('x', str(w)); s.set('y', str(h))


def appearance(tag, tex, maps, text_color=None, tiles=None):
    a = ET.Element(tag, {'Color': '0xffffffff', 'Specular': '0xff000000'})
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
    for st in it.find('States').findall('item'):
        for child in list(st):
            if child.tag.startswith('Appearance'):
                st.remove(child)
        for i, (name, spec) in enumerate(looks.items()):
            tex, maps, color = spec[:3]
            st.insert(i, appearance('Appearance' + name, tex, maps, color, spec[3] if len(spec) > 3 else None))


# the desk with the mod's menu paper, as the mod's main menu draws it
for a in root.find('States').find('item'):
    if a.tag.startswith('Appearance'):
        a.find('Texture').text = 'ui\\intermissiontextures\\back-mainmenu'

children = kids(root)

# the paper sheet the options are listed on, drawn under the lists
sheet = ET.Element('item', {'ClassTypeID': '0x10001105', 'ElementID': '31500', 'PositionFlag': '0x0011', 'Background': '1'})
ET.SubElement(sheet, 'WindowPos', {'x': str(X0), 'y': str(Y0)})
ET.SubElement(sheet, 'WindowSize', {'x': '512', 'y': '630'})
st = ET.SubElement(ET.SubElement(sheet, 'States'), 'item')
for n in ('Normal', 'Highlighted', 'Pushed'):
    st.append(appearance('Appearance' + n, 'ui\\IntermissionTextures\\menu-popups', (0, 0, 512, 630)))
ET.SubElement(st, 'PushSound'); ET.SubElement(st, 'ClickSound'); ET.SubElement(st, 'TextKey')
children.append(sheet)

# "Settings of <profile>" as the menu paper's header
cap = by_id(root, 20000)
set_pos(cap, 610, 180, 400, 50)
cap.set('TextAlign', '0x0022'); cap.set('FontSize', '2'); cap.set('TextColor', HEADER)
restyle(cap, {n: (None, None, HEADER) for n in ('Normal', 'Highlighted', 'Pushed')})

# the profile name is in the header already
prof = by_id(root, 21000)
set_pos(prof, 0, 0, 0, 0)

# division tabs on the menu paper's lines, in the mod's menu style
for i in range(6):
    tab = by_id(root, 10007 + i)
    set_pos(tab, TABS_X, TABS_Y + TAB_STEP * i, 320, 50)
    tab.set('FontSize', '2'); tab.set('TextAlign', '0x0022')
    tex, m = CLEAR
    restyle(tab, {'Normal': (tex, m, BLACK), 'Highlighted': (tex, m, RED_HI),
                  'Pushed': (tex, m, RED_PUSH), 'Disabled': (tex, m, RED_PUSH)})


# OK and Cancel on the sheet's green and red tabs, as the mod's dialogs have them
def tab_button(eid, x, icon_normal, icon_hi):
    b = by_id(root, eid)
    b.set('PositionFlag', '0x0011')
    set_pos(b, X0 + x, Y0 + 537, 88, 56)
    bg = ((0, 0, 88, 56), (88, 56), (0, 200, 88, 56))

    def icon(m):
        return [bg, ((24, 8, 64, 48), (40, 40), (m[0], m[1], 40, 40))]
    restyle(b, {'Normal': (BUTTONS, None, None, icon(icon_normal)),
                'Highlighted': (BUTTONS, None, None, icon(icon_hi)),
                'Pushed': (BUTTONS, None, None, icon(icon_hi))})


tab_button(10002, 112, (0, 40), (40, 40))     # check
tab_button(10001, 312, (120, 40), (160, 40))  # cross


# Defaults as a small icon between the sheet's two tabs: anything over the
# list's rectangle loses its clicks to the list. The mod's dialogs have no help
# button, so neither does this one.
def icon_button(eid, x, y, n, h, p):
    b = by_id(root, eid)
    b.set('PositionFlag', '0x0011')
    set_pos(b, x, y, 40, 40)
    restyle(b, {'Normal': (BUTTONS, (n[0], n[1], 40, 40), None),
                'Highlighted': (BUTTONS, (h[0], h[1], 40, 40), None),
                'Pushed': (BUTTONS, (p[0], p[1], 40, 40), None)})


icon_button(10003, X0 + 236, Y0 + 548, (120, 0), (160, 0), (200, 0))   # "<": back to defaults
help_button = by_id(root, 31416)
set_pos(help_button, 0, 0, 0, 0)
help_button.set('VisibleFlag', '0')

# the option lists inside the sheet's frame, with the mod's list furniture
for i in range(6):
    lst = by_id(root, 1000 + i)
    set_pos(lst, X0 + 22, Y0 + 20, 469, 500)
    tex, m = CLEAR
    restyle(lst, {n: (tex, m, None) for n in ('Normal', 'Highlighted')})
    sb = kids(lst).find('item')
    up, down, slider = kids(sb).findall('item')
    restyle(up, {n: ('ui\\IntermissionTextures\\back-chapter', (636, 769, 45, 58), None) for n in ('Normal', 'Highlighted', 'Pushed')})
    restyle(down, {n: ('ui\\IntermissionTextures\\back-chapter', (591, 769, 45, 58), None) for n in ('Normal', 'Highlighted', 'Pushed')})
    restyle(slider, {n: ('ui\\slider', (108, 0, 40, 33), None) for n in ('Normal', 'Highlighted', 'Pushed')})
    # column headers: the division name as the sheet's title, the second one unused
    h1 = by_id(lst, 10)
    set_pos(h1, 10, 0, 410, 85)
    h1.set('FontSize', '2'); h1.set('TextAlign', '0x0021'); h1.set('TextColor', BLACK)
    restyle(h1, {n: (None, None, BLACK) for n in ('Normal', 'Highlighted', 'Pushed')})
    h2 = by_id(lst, 11)
    set_pos(h2, 420, 0, 0, 0)
    restyle(h2, {n: (None, None, BLACK) for n in ('Normal', 'Highlighted', 'Pushed')})
    # the Cloud tab's Config... and Backups... buttons: paper strips at the foot of the frame
    for eid, x in ((10013, 20), (10014, 225)):
        try:
            b = by_id(lst, eid)
        except KeyError:
            continue
        set_pos(b, x, 440, 190, 36)
        b.set('FontSize', '1'); b.set('TextAlign', '0x0022')
        strip = ('UI\\Textures\\dtf-ui-misc', (5, 6, 432, 30))
        restyle(b, {'Normal': (*strip, BLACK), 'Highlighted': (*strip, RED_HI),
                    'Pushed': (*strip, RED_PUSH), 'Disabled': (*strip, '0xff7a6a55')})

header = ('<!-- The settings screen restyled for the Achtung Panzer 2 mod from its own pictures\n'
          '     (the desk and menu paper, the paper sheet and its buttons), referenced by name\n'
          '     only. Generated from ui\\OptionsSettings.xml by tools/ui/modstyles/achtungpanzer2_settings.py:\n'
          '     same elements, ids and script.\n'
          '     Used instead of it while that mod is loaded; see OpenLayoutStream in UIScreen.cpp. -->\n')
ET.indent(root, '\t')
open(dst, 'w', encoding='utf-8', newline='\r\n').write(header + ET.tostring(root, encoding='unicode') + '\n')
