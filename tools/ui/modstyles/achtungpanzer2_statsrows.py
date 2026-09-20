#!/usr/bin/env python3
# Generates the Achtung Panzer 2 styles of the four report-table row templates.
#
# The report's nation rows are written in cream on a band cut from back-stats.
# In the base game that band is dark and the cream is what makes the row read;
# the mod's back-stats paints it a light brown, and the mod never restyled these
# templates, so its nation rows come out cream on light brown. Same rows, same
# ids, same pictures - only the colour the nation line is written in, which on
# the mod's band is the black the rest of the paper uses.
#
#   python3 tools/ui/modstyles/achtungpanzer2_statsrows.py \
#       Data/UI/common/StatsPlayerName.xml \
#       Data/UI/ModStyles/achtungpanzer2/common/StatsPlayerName.xml
import re
import sys

src, dst = sys.argv[1], sys.argv[2]
raw = open(src, 'rb').read().decode('utf-8')

CREAM = re.compile(r'(TextColor=")0xFFFDF2DB\s*(")', re.IGNORECASE)
ON_THE_BAND = '0xff000000'
styled, count = CREAM.subn(r'\g<1>' + ON_THE_BAND + r'\g<2>', raw)
if count == 0:
    raise SystemExit('%s: no nation-row colour to restyle' % src)

name = src.replace('\\', '/').rsplit('/', 1)[-1][:-4]
note = ('<!-- The %s row restyled for the Achtung Panzer 2 mod: its report paper\r\n'
        '     bands the nation rows in a light brown, which the base game\'s cream\r\n'
        '     lettering does not read on. Generated from ui\\common\\%s.xml.\r\n'
        '     Used instead of it while that mod is loaded; see OpenLayoutStream. -->\r\n')
head, rest = styled.split('\r\n', 1)
open(dst, 'wb').write((head + '\r\n' + (note % (name, name)) + rest).encode('utf-8'))
