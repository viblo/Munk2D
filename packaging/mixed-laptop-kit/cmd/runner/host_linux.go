package main

import (
	"context"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
	"strings"
	"syscall"
	"unsafe"
)

func setupCPU(request int) (int, error) {
	var mask [128]byte
	_, _, e := syscall.RawSyscall(syscall.SYS_SCHED_GETAFFINITY, 0, uintptr(len(mask)), uintptr(unsafe.Pointer(&mask[0])))
	if e != 0 {
		return -1, e
	}
	allowed := func(n int) bool { return n >= 0 && n < len(mask)*8 && mask[n/8]&(1<<uint(n%8)) != 0 }
	if request < 0 {
		if allowed(2) {
			request = 2
		} else {
			for n := 0; n < len(mask)*8; n++ {
				if allowed(n) {
					request = n
					break
				}
			}
		}
	}
	if !allowed(request) {
		return -1, fmt.Errorf("CPU %d is not allowed", request)
	}
	_, e2 := exec.LookPath("taskset")
	return request, e2
}
func engineCommand(ctx context.Context, path string, args []string, cpu int) *exec.Cmd {
	return exec.CommandContext(ctx, "taskset", append([]string{"-c", strconv.Itoa(cpu), path}, args...)...)
}
func readText(path string) string { b, _ := os.ReadFile(path); return strings.TrimSpace(string(b)) }
func hostInfo() map[string]any {
	cpu := ""
	for _, line := range strings.Split(readText("/proc/cpuinfo"), "\n") {
		if strings.HasPrefix(line, "model name") {
			cpu = strings.TrimSpace(strings.SplitN(line, ":", 2)[1])
			break
		}
	}
	return map[string]any{"CPUModel": cpu, "OS": readText("/etc/os-release"), "Kernel": readText("/proc/sys/kernel/osrelease"), "Memory": strings.Split(readText("/proc/meminfo"), "\n")[0], "AffinityMethod": "taskset before engine exec; no core reservation or fixed clock"}
}
func telemetry(cpu int) map[string]any {
	p := fmt.Sprintf("/sys/devices/system/cpu/cpu%d/cpufreq/", cpu)
	freq := map[string]string{}
	for _, s := range []string{"scaling_cur_freq", "scaling_min_freq", "scaling_max_freq", "scaling_governor", "energy_performance_preference"} {
		freq[s] = readText(p + s)
	}
	load := strings.Fields(readText("/proc/loadavg"))
	if len(load) > 3 {
		load = load[:3]
	}
	t := map[string]any{"Utc": utc(), "LoadAverage": strings.Join(load, " "), "FrequencyDriverInstantaneous": freq}
	for _, line := range strings.Split(readText("/proc/stat"), "\n") {
		if strings.HasPrefix(line, fmt.Sprintf("cpu%d ", cpu)) {
			t["CPUStat"] = line
		}
	}
	temps := map[string]string{}
	paths, _ := filepath.Glob("/sys/class/thermal/thermal_zone*/temp")
	for _, path := range paths {
		temps[readText(filepath.Join(filepath.Dir(path), "type"))] = readText(path)
	}
	t["TemperatureMilliC"] = temps
	power := map[string]any{}
	paths, _ = filepath.Glob("/sys/class/power_supply/*")
	for _, path := range paths {
		v := map[string]string{}
		for _, key := range []string{"type", "online", "status", "capacity"} {
			v[key] = readText(filepath.Join(path, key))
		}
		power[filepath.Base(path)] = v
	}
	t["Power"] = power
	return t
}
