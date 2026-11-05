package main

import (
	"context"
	"encoding/json"
	"fmt"
	"os/exec"
	"syscall"
	"time"
	"unsafe"
)

var kernel = syscall.NewLazyDLL("kernel32.dll")

func proc(name string) *syscall.LazyProc { return kernel.NewProc(name) }
func setupCPU(n int) (int, error) {
	h, _, _ := proc("GetCurrentProcess").Call()
	var allowed, system uintptr
	r, _, e := proc("GetProcessAffinityMask").Call(h, uintptr(unsafe.Pointer(&allowed)), uintptr(unsafe.Pointer(&system)))
	if r == 0 {
		return -1, e
	}
	if n < 0 {
		if allowed&4 != 0 {
			n = 2
		} else {
			for i := 0; i < 64; i++ {
				if allowed&(uintptr(1)<<uint(i)) != 0 {
					n = i
					break
				}
			}
		}
	}
	if n < 0 || n >= 64 || allowed&(uintptr(1)<<uint(n)) == 0 {
		return -1, fmt.Errorf("CPU %d is not allowed", n)
	}
	r, _, e = proc("SetProcessAffinityMask").Call(h, uintptr(1)<<uint(n))
	if r == 0 {
		return -1, e
	}
	return n, nil
}
func engineCommand(ctx context.Context, path string, args []string, cpu int) *exec.Cmd {
	return exec.CommandContext(ctx, path, args...)
}
func hostInfo() map[string]any {
	ctx, cancel := context.WithTimeout(context.Background(), 20*time.Second)
	defer cancel()
	script := `$ErrorActionPreference='Stop';[Console]::OutputEncoding=[Text.UTF8Encoding]::new($false); @{CPU=@(Get-CimInstance Win32_Processor|Select-Object Name,NumberOfCores,NumberOfLogicalProcessors,MaxClockSpeed);OS=Get-CimInstance Win32_OperatingSystem|Select-Object Caption,Version,BuildNumber,OSArchitecture;System=Get-CimInstance Win32_ComputerSystem|Select-Object Manufacturer,Model,HypervisorPresent;PowerPlan=(& powercfg.exe /GETACTIVESCHEME|Out-String).Trim()}|ConvertTo-Json -Depth 8 -Compress`
	b, e := exec.CommandContext(ctx, "powershell.exe", "-NoLogo", "-NoProfile", "-NonInteractive", "-Command", script).Output()
	m := map[string]any{"AffinityMethod": "parent process affinity mask inherited before engine start; no core reservation or fixed clock"}
	if e != nil {
		m["CollectionError"] = e.Error()
	} else {
		var v any
		if json.Unmarshal(b, &v) == nil {
			m["Details"] = v
		} else {
			m["CollectionError"] = "invalid JSON"
		}
	}
	return m
}

type powerStatus struct {
	ACLine, BatteryFlag, BatteryPercent, SystemStatus byte
	BatteryLife, BatteryFullLife                      uint32
}

func telemetry(cpu int) map[string]any {
	m := map[string]any{"Utc": utc()}
	var p powerStatus
	r, _, _ := proc("GetSystemPowerStatus").Call(uintptr(unsafe.Pointer(&p)))
	if r != 0 {
		m["Power"] = p
	}
	var idle, kern, user syscall.Filetime
	r, _, _ = proc("GetSystemTimes").Call(uintptr(unsafe.Pointer(&idle)), uintptr(unsafe.Pointer(&kern)), uintptr(unsafe.Pointer(&user)))
	if r != 0 {
		ticks := func(t syscall.Filetime) uint64 { return uint64(t.HighDateTime)<<32 | uint64(t.LowDateTime) }
		m["SystemCPU100ns"] = map[string]uint64{"Idle": ticks(idle), "KernelIncludingIdle": ticks(kern), "User": ticks(user)}
	}
	return m
}
