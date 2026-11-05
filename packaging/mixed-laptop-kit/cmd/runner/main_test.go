package main

import (
	"archive/zip"
	"context"
	"encoding/json"
	"fmt"
	"math"
	"os"
	"path/filepath"
	"testing"
	"time"
)

func mustPanic(t *testing.T, f func()) {
	t.Helper()
	defer func() {
		if recover() == nil {
			t.Error("expected rejection")
		}
	}()
	f()
}
func TestParse(t *testing.T) {
	c := Case{Driver: "munkbench", Scene: "Test", Size: 10}
	good := "version,benchmark,size,sample,batch,init_time,run_time\n2.1.0,Test,10,1,2,0.1,0.2\n"
	s := parseSample([]byte(good), c, 2)
	if s.RunSeconds != .2 {
		t.Fatal(s)
	}
	for _, bad := range []string{"NaN", "Inf", "0", "-1"} {
		mustPanic(t, func() {
			parseSample([]byte("version,benchmark,size,sample,batch,init_time,run_time\n2.1.0,Test,10,1,2,0.1,"+bad+"\n"), c, 2)
		})
	}
	mustPanic(t, func() { parseSample([]byte(good), c, 3) })
	mustPanic(t, func() { parseSample([]byte(good+"2.1.0,Test,10,1,2,0.1,0.2\n"), c, 2) })
}
func TestStats(t *testing.T) {
	if median([]float64{3, 1, 2, 4}) != 2.5 {
		t.Fatal("median")
	}
	rows, totals, _ := summarize([]Pair{{Case: "g/s", Label: "prior-avx2", Order: "AB", Ratio: .8}, {Case: "g/s", Label: "prior-avx2", Order: "BA", Ratio: 1.0}})
	if math.Abs(rows[0].Percent+10) > 1e-10 || rows[0].AB != 1 || rows[0].BA != 1 || len(totals) != 1 {
		t.Fatal(rows, totals)
	}
}
func TestChecksumAndZip(t *testing.T) {
	dir := t.TempDir()
	f := filepath.Join(dir, "a")
	os.WriteFile(f, []byte("abc"), 0644)
	if sha(f) != "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" {
		t.Fatal("sha")
	}
	z := zipResults(dir)
	defer os.Remove(z)
	r, e := zip.OpenReader(z)
	if e != nil {
		t.Fatal(e)
	}
	defer r.Close()
	if len(r.File) != 1 {
		t.Fatal("archive")
	}
	a := App{root: dir, manifest: Manifest{Executables: []Entry{{"a", "wrong"}, {"a", "wrong"}, {"a", "wrong"}, {"a", "wrong"}, {"a", "wrong"}}}}
	mustPanic(t, a.verify)
}

func TestShortAndFullCheckpoints(t *testing.T) {
	for _, total := range []int{100, 600} {
		var scenes []any
		for i := 0; i < 26; i++ {
			var checkpoints []any
			for _, n := range []int{0, 1, 10, 50, 180, 181, total} {
				if n <= total {
					checkpoints = append(checkpoints, map[string]any{"step": n, "invalid_values": 0})
				}
			}
			scenes = append(scenes, map[string]any{"benchmark": fmt.Sprint("s", i), "simulated_steps": total, "checkpoints": checkpoints})
		}
		data, e := json.Marshal(map[string]any{"benchmarks": scenes})
		if e != nil {
			t.Fatal(e)
		}
		decodeSummary(data)
		scenes[0].(map[string]any)["checkpoints"] = []any{}
		bad, _ := json.Marshal(map[string]any{"benchmarks": scenes})
		mustPanic(t, func() { decodeSummary(bad) })
	}
}
func TestMixedParser(t *testing.T) {
	c := Case{Driver: "munkbench_mixed", Scene: "light", Count: 64, Steps: 1200}
	good := `{"scene":"light","solver":"avx2","bodies":64,"steps":1200,"batch":2,"sample":0,"invalid":0,"stateHash":"0123456789abcdef","runSeconds":0.1,"initSeconds":0}`
	s := parseSample([]byte(good), c, 2)
	if s.StateHash != "0123456789abcdef" {
		t.Fatal(s)
	}
	c.Count = 256
	mustPanic(t, func() { parseSample([]byte(good), c, 2) })
}
func TestEngineHelper(t *testing.T) {
	if os.Getenv("MUNK2D_KIT_HELPER") == "1" {
		time.Sleep(30 * time.Second)
		os.Exit(0)
	}
}
func TestEngineCancellation(t *testing.T) {
	cpu, e := setupCPU(-1)
	if e != nil {
		t.Fatal(e)
	}
	exe, e := os.Executable()
	if e != nil {
		t.Fatal(e)
	}
	ctx, cancel := context.WithTimeout(context.Background(), 200*time.Millisecond)
	defer cancel()
	cmd := engineCommand(ctx, exe, []string{"-test.run=^TestEngineHelper$"}, cpu)
	cmd.Env = append(os.Environ(), "MUNK2D_KIT_HELPER=1")
	start := time.Now()
	if cmd.Run() == nil || ctx.Err() == nil || time.Since(start) > 4*time.Second {
		t.Fatal("engine was not promptly terminated")
	}
}
