#!/bin/sh
# Flame graph of the running Game's main thread on macOS - no Xcode needed.
#
# Samples the live process with /usr/bin/sample (1 ms, all threads) and folds
# the main thread's stacks with Brendan Gregg's FlameGraph scripts
# (brew install flamegraph). Writes under zig-out/profile/:
#   <stamp>.sample.txt   raw sample(1) report, all threads
#   <stamp>.main.folded  folded stacks of the main thread
#   <stamp>.main.svg     flame graph of the main thread
#   <stamp>.work.folded / .work.svg
#                        the same minus the vsync wait (semaphore_timedwait_trap),
#                        so the CPU work per frame fills the whole width
# and prints the top self-time and inclusive frames of the work part.
#
# usage: tools/profile/flamegraph.sh [seconds] [pid]
#   seconds  sampling duration, default 20
#   pid      defaults to the running Game (pgrep -x Game)
#
# Start the game first (a busy battle gives the most useful profile), then run
# this. For an interactive view (call tree, per-thread timeline, source-line
# attribution) record with samply instead - the ad-hoc signed Game accepts its
# preload:   cd zig-out/game/macos/arm64/release && samply record ./Game -windowed
# (brew install samply). Both need the symbols the release build already keeps.

set -eu

secs=${1:-20}
pid=${2:-$(pgrep -x Game | head -1 || true)}
[ -n "$pid" ] || { echo "no Game process running (start it first, or pass a pid)" >&2; exit 1; }
for t in sample stackcollapse-sample.awk flamegraph.pl; do
	command -v "$t" >/dev/null || { echo "$t not found (brew install flamegraph)" >&2; exit 1; }
done

root=$(cd "$(dirname "$0")/../.." && pwd)
out=$root/zig-out/profile
mkdir -p "$out"
base=$out/$(date +%Y%m%d-%H%M%S)

echo "sampling pid $pid for ${secs}s ..."
sample "$pid" "$secs" -mayDie -file "$base.sample.txt" >/dev/null

# The main thread's subtree of the call graph, re-wrapped in the two marker
# lines stackcollapse-sample.awk keys on.
{
	echo "Call graph:"
	awk '/^Call graph:/{f=1;next} /^Total number in stack/{f=0} f' "$base.sample.txt" \
		| awk '/^    [0-9]+ Thread_[0-9]+: Main Thread/{p=1;print;next} /^    [0-9]+ Thread_/{p=0} p'
	echo
	echo "Total number in stack (recursive counted multiple, when >=5):"
} | stackcollapse-sample.awk > "$base.main.folded"
grep -v semaphore_timedwait_trap "$base.main.folded" > "$base.work.folded" || true

flamegraph.pl --title "Game main thread, ${secs}s" --width 1600 "$base.main.folded" > "$base.main.svg"
flamegraph.pl --title "Game main thread, work only (vsync wait removed), ${secs}s" --width 1600 "$base.work.folded" > "$base.work.svg"

# Summary: how much of the thread was the vsync wait, then the hottest frames
# of the remaining work by self time and by inclusive time.
awk '{n=$NF; all+=n; if ($0 ~ /semaphore_timedwait_trap/) wait+=n}
	END{printf "main thread: %d samples, %.1f%% in the vsync wait, %.1f%% work\n", all, 100*wait/all, 100*(all-wait)/all}' "$base.main.folded"
awk '{n=$NF; $NF=""; s=$0; sub(/ $/,"",s); k=split(s,a,";"); tot+=n; self[a[k]]+=n
		delete seen; for(i=1;i<=k;i++) if(!(a[i] in seen)){incl[a[i]]+=n; seen[a[i]]=1}}
	END{print "\ntop self time (work only):"
		for(f in self) printf "%6d %5.1f%%  %s\n", self[f], 100*self[f]/tot, substr(f,1,110) | "sort -rn | head -15"
		close("sort -rn | head -15")
		print "\ntop inclusive (work only):"
		for(f in incl) printf "%6d %5.1f%%  %s\n", incl[f], 100*incl[f]/tot, substr(f,1,110) | "sort -rn | head -40"}' "$base.work.folded"

echo
echo "wrote $base.main.svg"
echo "      $base.work.svg"
