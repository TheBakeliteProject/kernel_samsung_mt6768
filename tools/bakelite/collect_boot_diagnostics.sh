#!/system/bin/sh
# Collect post-boot state and persistent crash logs. Run as root.
set -eu
output=${1:-/sdcard/Download/bakelite-diagnostics-$(date +%Y%m%d-%H%M%S)}
[ ! -e "$output" ] || { echo "Output exists: $output" >&2; exit 1; }
mkdir -p "$output/cpufreq" "$output/thermal" "$output/pstore" "$output/gpu"
copy_if_readable() {
	if [ -r "$1" ]; then
		cat "$1" > "$2"
	fi
}
date +%s > "$output/epoch_seconds"
uname -a > "$output/uname"
getprop > "$output/getprop" 2>/dev/null || true
copy_if_readable /proc/version "$output/proc_version"
copy_if_readable /proc/cmdline "$output/proc_cmdline"
copy_if_readable /proc/last_kmsg "$output/last_kmsg"
copy_if_readable "${0%/*}/features.txt" "$output/features.txt"
for policy in /sys/devices/system/cpu/cpufreq/policy*; do
  [ -d "$policy" ] || continue; name=${policy##*/}
  for field in scaling_driver scaling_governor scaling_available_governors cpuinfo_min_freq cpuinfo_max_freq scaling_min_freq scaling_max_freq scaling_cur_freq related_cpus; do copy_if_readable "$policy/$field" "$output/cpufreq/$name.$field"; done
  copy_if_readable "$policy/stats/time_in_state" "$output/cpufreq/$name.time_in_state"
done
for zone in /sys/class/thermal/thermal_zone*; do
  [ -d "$zone" ] || continue; name=${zone##*/}
  copy_if_readable "$zone/type" "$output/thermal/$name.type"; copy_if_readable "$zone/temp" "$output/thermal/$name.temp"; copy_if_readable "$zone/policy" "$output/thermal/$name.policy"
done
for path in /sys/module/ged/parameters/simple_gpu_enabled /sys/module/ged/parameters/simple_gpu_up_threshold /sys/module/ged/parameters/simple_gpu_hold_ms /sys/module/ged/parameters/mali_boost_level /sys/module/mtk_bus_boost/parameters/enabled /sys/module/mtk_bus_boost/parameters/boost_opp /sys/module/mtk_bus_boost/parameters/duration_ms /sys/module/mtk_bus_boost/parameters/cooldown_ms; do
  [ -r "$path" ] || continue; name=$(printf '%s' "$path" | tr '/' '_'); cat "$path" > "$output/gpu/$name"
done
for pstore in /sys/fs/pstore/* /dev/pmsg0; do [ -r "$pstore" ] || continue; name=${pstore##*/}; cat "$pstore" > "$output/pstore/$name" 2>/dev/null || true; done
echo "Saved boot diagnostics to $output"
