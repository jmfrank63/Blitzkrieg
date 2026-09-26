#!/usr/bin/env bash
# Plays one random mission in the worktree's staged debug game, wins it through
# the mission script, and checks the chapter screen: before the win it offers no
# historical mission, after it the chapter's historical mission.
#
# usage: tools/missions/run_random_mission.sh <campaign index> <campaign> \
#          <first chapter> <chapter> <template mission> <historical mission>
#   campaign index: 0 German, 1 USSR, 2 Allies (GameTT/UIState.cpp)
#
# Uses the throw-away profile MissionRun; never the player's profiles.
# Output: zig-out/local-test/mission-run/<template>.log and .png
set -euo pipefail
[ $# -eq 6 ] || { sed -n 2,12p "$0"; exit 2; }
index=$1 campaign=$2 first=$3 chapter=$4 template=$5 historical=$6
root=$(cd "$(dirname "$0")/../.." && pwd)
game="$root/zig-out/game/macos/arm64/debug"
out="$root/zig-out/local-test/mission-run"
name=$(echo "$template" | tr '\\/' '__')
log="$out/$name.log"
mkdir -p "$out"
rm -rf "$game/profiles/MissionRun" "$HOME/.local/share/Nival/Blitzkrieg/cache/generated/MissionRun"

# Frames: tuned with the frame lines of a BK_UI_TRACE log (Task 6 Step 5), and
# with screenshots (`shot`) measured by hand - the win dialog's own "Finish
# Mission" button (BUTTON_WIN_WIN_MISSION, Data/UI/mission.xml ElementID
# 10018) is what actually leaves the mission; the harness's generic `ok`
# verb sends IMC_OK, which this dialog does not listen for, so lua=Win(0)
# would otherwise strand the run on the win screen. The post-win clicks
# below were located by screenshot, at the render's native 1440x900 (see
# the `shot`-written .rgba's own name):
#   718x495  "Finish Mission" on the win dialog (ESCAPE_WIN_MISSION, 99995)
#   705x765  the checkmark on a rank/medal popup (ui\Popup\PlayerRank) that
#            a first randomly-won mission can raise on top of the personal
#            card - found while driving the France chapter (Task 8), where
#            this mission's first win awards the "Panzer Badge 4th Class"
#            and the popup blocks every click until it is dismissed; the
#            popup is modal, so this click is a no-op on chapters/runs
#            where no rank/medal was earned (confirmed by screenshot against
#            the Stalingrad run, which never shows this popup)
#   1200x825 the checkmark on the personal-card/stats screen (ui\PlayerStats)
#   1180x825 "X" (Cancel Upgrade) on the depot-upgrades popup (ui\upgrades)
# The briefing's own OK (frame 900) does answer to IMC_OK, so `ok` still
# works there.
#
# 240:var=Mission.Last.FinishStatus=-1 sits between the two chapter= actions
# because opening a chapter screen sets Mission.Last.FinishStatus=LOSE
# (Chapter.cpp, IncrementChapterVisited) - a real player only ever reaches a
# new chapter after winning the previous one, so a fresh session never carries
# a stale LOSE into the chapter it is about to visit. The harness drives two
# chapter= in one session, so it has to reset the var by hand in between.
schedule="60:campaign=$index=$campaign,120:chapter=$first,240:var=Mission.Last.FinishStatus=-1,300:chapter=$chapter"
schedule="$schedule,600:mission=$template,900:ok,1500:key=SPACE,1800:lua=Win(0)"
schedule="$schedule,2100:click=718x495,2150:click=705x765,2200:click=1200x825,2300:click=1180x825,2500:shot,2600:exit"

cd "$game"
# The schedule above spans ~2600 engine frames; at this machine's real-time
# rate that is on the order of 100s, so give the run generous wall-clock room
# - a `timeout` shorter than the schedule kills the process mid-run and its
# graceful-shutdown path looks, in the log, just like a normal `exit`.
BK_UI_TRACE=1 BK_NO_HELP=1 BK_AUTO_UI="$schedule" timeout 240 ./Game -profile=MissionRun -windowed 2> "$log" || true
cd "$root"

win_line=$(grep -n "action lua=Win(0)" "$log" | head -1 | cut -d: -f1)
[ -n "$win_line" ] || { echo "FAIL: the run never reached lua=Win(0); see $log"; exit 1; }
before=$(head -n "$win_line" "$log" | grep -F "chapter \"$chapter\" offers" || true)
after=$(tail -n "+$win_line" "$log" | grep -F "chapter \"$chapter\" offers" || true)
status=0
if echo "$before" | grep -q "offers historical"; then
  echo "FAIL: before the random win the chapter offered a historical mission:"; echo "$before"; status=1
fi
echo "$before" | grep -q "offers random" || { echo "FAIL: before the win the chapter offered no random mission"; status=1; }
echo "$after" | grep -qF "offers historical mission \"$historical\"" || {
  echo "FAIL: after the random win the chapter did not offer $historical:"; echo "$after"; status=1; }
[ $status -eq 0 ] && echo "PASS: $template in $chapter"
exit $status
