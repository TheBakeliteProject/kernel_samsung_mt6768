#!/system/bin/sh
# Collect one CPUFreq governor run on the phone. Run as root.
set -eu

usage() {
	echo "usage: sh measure_governor.sh GOVERNOR SECONDS [OUTPUT_DIR]" >&2
	echo "governors: schedutil bakeliteperf bakelitebalance bakelitebattery" >&2
	exit 2
}

[ "$#" -ge 2 ] && [ "$#" -le 3 ] || usage
governor=$1
duration=$2
case "$governor" in
	schedutil|bakeliteperf|bakelitebalance|bakelitebattery) ;;
	*) usage ;;
esac
case "$duration" in
	''|*[!0-9]*) usage ;;
esac
[ "$duration" -ge 10 ] || usage
[ "$(id -u)" -eq 0 ] || {
	echo 'Run this script in a root shell.' >&2
	exit 1
}

output=${3:-/sdcard/Download/bakelite-${governor}-$(date +%Y%m%d-%H%M%S)}
[ ! -e "$output" ] || {
	echo "Output directory already exists: $output" >&2
	exit 1
}

set -- /sys/devices/system/cpu/cpufreq/policy*
[ -d "$1" ] || {
	echo 'No CPUFreq policies found.' >&2
	exit 1
}
for policy; do
	available=$(cat "$policy/scaling_available_governors")
	case " $available " in
		*" $governor "*) ;;
		*) echo "$governor is unavailable for ${policy##*/}" >&2; exit 1 ;;
	esac
	[ -r "$policy/stats/time_in_state" ] || {
		echo "CPUFreq residency stats are unavailable for ${policy##*/}" >&2
		exit 1
	}
done

mkdir -p "$output/before" "$output/after"
originals=$output/original_governors.tsv
for policy; do
	printf '%s\t%s\n' "$policy" "$(cat "$policy/scaling_governor")" >> "$originals"
done

restore_governors() {
	trap - 0
	while read -r policy previous; do
		if [ -w "$policy/scaling_governor" ]; then
			printf '%s\n' "$previous" > "$policy/scaling_governor" ||
				echo "Could not restore ${policy##*/} to $previous" >&2
		else
			echo "Could not restore ${policy##*/} to $previous" >&2
		fi
	done < "$originals"
}
trap restore_governors 0
trap 'exit 130' 1 2 3 15

for policy; do
	printf '%s\n' "$governor" > "$policy/scaling_governor"
done

check_governors() {
	for policy in /sys/devices/system/cpu/cpufreq/policy*; do
		[ "$(cat "$policy/scaling_governor")" = "$governor" ] || {
			echo "Governor changed on ${policy##*/}; run is invalid." >&2
			exit 1
		}
	done
}
check_governors

read_value() {
	if [ -r "$1" ]; then cat "$1"; else printf 'NA\n'; fi
}

capture() {
	phase=$1
	printf '%s\n' "$(date +%s)" > "$output/$phase/epoch_seconds"
	for policy in /sys/devices/system/cpu/cpufreq/policy*; do
		name=${policy##*/}
		cat "$policy/stats/time_in_state" > "$output/$phase/$name.time_in_state"
		read_value "$policy/stats/total_trans" > "$output/$phase/$name.total_trans"
		read_value "$policy/scaling_governor" > "$output/$phase/$name.governor"
		read_value "$policy/scaling_min_freq" > "$output/$phase/$name.min_freq"
		read_value "$policy/scaling_max_freq" > "$output/$phase/$name.max_freq"
	done
	battery=/sys/class/power_supply/battery
	for field in charge_counter capacity current_now temp status; do
		read_value "$battery/$field" > "$output/$phase/battery.$field"
	done
}

printf 'governor\t%s\nduration_seconds\t%s\nkernel\t%s\n' \
	"$governor" "$duration" "$(uname -r)" > "$output/meta.tsv"
echo "Selected $governor; settling for 3 seconds."
sleep 3
capture before
echo "Repeat the same workload for $duration seconds now."
printf 'elapsed_seconds\tcurrent_now_uA\tbattery_temp_tenth_C\n' > "$output/samples.tsv"
elapsed=0
while [ "$elapsed" -lt "$duration" ]; do
	check_governors
	printf '%s\t%s\t%s\n' "$elapsed" \
		"$(read_value /sys/class/power_supply/battery/current_now)" \
		"$(read_value /sys/class/power_supply/battery/temp)" \
		>> "$output/samples.tsv"
	remaining=$((duration - elapsed))
	step=5
	[ "$remaining" -ge "$step" ] || step=$remaining
	sleep "$step"
	elapsed=$((elapsed + step))
done
check_governors
capture after
echo "Saved $output; restoring the previous governors."
