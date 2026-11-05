// Standalone offline benchmark orchestrator; uses only the Go standard library.
package main

import (
	"archive/zip"
	"bufio"
	"bytes"
	"context"
	"crypto/sha256"
	"encoding/csv"
	"encoding/hex"
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"io"
	"math"
	"os"
	"os/signal"
	"path/filepath"
	"reflect"
	"runtime"
	"sort"
	"strconv"
	"strings"
	"time"
)

type Entry struct{ Name, SHA256 string }
type Manifest struct {
	Baseline, Candidate, Timer, Compiler, BuildRun, PackagingCommit, ScenariosSHA256 string
	Executables                                                                      []Entry
}
type Case struct {
	ID, Driver, Scene, Profile string
	Count, Size, Steps         int
}
type Scenario struct {
	Name                            string
	Steps, SmokeSize, ReferenceSize int
}
type Side struct{ Variant, Solver string }
type Comparison struct {
	Label string
	A, B  Side
}
type Sample struct {
	RunSeconds, InitSeconds float64
	Batch, Size, Invocation int
	StateHash               string
}
type Pair struct {
	Case                string
	Label               string
	Block, Round, Batch int
	Order               string
	A, B                Sample
	Ratio               float64
	Before, After       map[string]any
}
type Summary struct {
	Case, Label                     string
	Pairs, Faster, AB, BA           int
	Percent, MinPercent, MaxPercent float64
}
type App struct {
	ctx           context.Context
	root, out     string
	manifest      Manifest
	mode          string
	cpu, sequence int
	invocations   *os.File
	pairs         []Pair
	metadata      map[string]any
}

func check(err error) {
	if err != nil {
		panic(err)
	}
}
func writeJSON(path string, v any) {
	data, e := json.MarshalIndent(v, "", "  ")
	check(e)
	check(os.WriteFile(path, append(data, '\n'), 0644))
}
func sha(path string) string {
	b, e := os.ReadFile(path)
	check(e)
	h := sha256.Sum256(b)
	return hex.EncodeToString(h[:])
}
func lineJSON(w io.Writer, v any) { check(json.NewEncoder(w).Encode(v)) }
func utc() string                 { return time.Now().UTC().Format(time.RFC3339Nano) }
func finite(x float64) bool       { return !math.IsNaN(x) && !math.IsInf(x, 0) }
func require(ok bool, message string) {
	if !ok {
		panic(message)
	}
}
func median(x []float64) float64 {
	require(len(x) > 0, "empty median")
	x = append([]float64(nil), x...)
	sort.Float64s(x)
	n := len(x)
	if n%2 == 1 {
		return x[n/2]
	}
	return (x[n/2-1] + x[n/2]) / 2
}
func (a *App) verify() {
	require(len(a.manifest.Executables) == 5, "expected four benchmark drivers and one runner")
	expected := map[string]bool{}
	for _, n := range []string{"munkbench-baseline", "munkbench-candidate", "munkbench_mixed-baseline", "munkbench_mixed-candidate", "runner"} {
		expected[n+suffix()] = true
	}
	for _, v := range a.manifest.Executables {
		require(expected[v.Name], "unexpected or duplicate manifest executable")
		delete(expected, v.Name)
		require(filepath.Base(v.Name) == v.Name, "unsafe manifest path")
		require(sha(filepath.Join(a.root, v.Name)) == v.SHA256, "checksum mismatch: "+v.Name)
	}
	require(sha(filepath.Join(a.root, "scenarios.json")) == a.manifest.ScenariosSHA256, "scenario checksum mismatch")
}
func suffix() string {
	if runtime.GOOS == "windows" {
		return ".exe"
	}
	return ""
}
func (a *App) invoke(stage, driver, variant string, args []string) []byte {
	if e := a.ctx.Err(); e != nil {
		panic(e)
	}
	a.sequence++
	id := a.sequence
	exe := driver + "-" + variant + suffix()
	path := filepath.Join(a.root, exe)
	ctx, cancel := context.WithTimeout(a.ctx, 15*time.Minute)
	defer cancel()
	cmd := engineCommand(ctx, path, args, a.cpu)
	cmd.Dir = a.root
	var stdout, stderr bytes.Buffer
	cmd.Stdout = &stdout
	cmd.Stderr = &stderr
	before := telemetry(a.cpu)
	start := time.Now()
	err := cmd.Run()
	elapsed := time.Since(start).Seconds()
	stem := fmt.Sprintf("%05d-%s-%s", id, stage, exe)
	check(os.WriteFile(filepath.Join(a.out, "raw", stem+".stdout"), stdout.Bytes(), 0644))
	if stderr.Len() > 0 {
		check(os.WriteFile(filepath.Join(a.out, "raw", stem+".stderr"), stderr.Bytes(), 0644))
	}
	record := map[string]any{"ID": id, "Stage": stage, "Executable": exe, "Args": args, "StartedUtc": start.UTC().Format(time.RFC3339Nano), "EndedUtc": utc(), "InvocationWallSeconds": elapsed, "Before": before, "After": telemetry(a.cpu)}
	if cmd.ProcessState != nil {
		record["InvocationCPUSeconds"] = cmd.ProcessState.UserTime().Seconds() + cmd.ProcessState.SystemTime().Seconds()
		record["ExitCode"] = cmd.ProcessState.ExitCode()
	}
	if err != nil {
		record["Error"] = err.Error()
	}
	lineJSON(a.invocations, record)
	if a.ctx.Err() != nil {
		panic(a.ctx.Err())
	}
	if err != nil {
		panic(fmt.Sprintf("%s failed: %v; %s", exe, err, stderr.String()))
	}
	return stdout.Bytes()
}
func parseSample(data []byte, c Case, batch int) Sample {
	var s Sample
	if c.Driver == "munkbench_mixed" {
		var x struct {
			Scene, Solver, StateHash              string
			Bodies, Steps, Batch, Sample, Invalid int
			RunSeconds, InitSeconds               float64
		}
		check(json.Unmarshal(bytes.TrimSpace(data), &x))
		require(x.Scene == c.Scene && x.Bodies == c.Count && x.Steps == c.Steps && x.Sample == 0 && x.Invalid == 0 && len(x.StateHash) == 16, "bad mixed protocol")
		s = Sample{RunSeconds: x.RunSeconds, InitSeconds: x.InitSeconds, Batch: x.Batch, Size: x.Bodies, StateHash: x.StateHash}
	} else {
		r := csv.NewReader(bytes.NewReader(data))
		rows, e := r.ReadAll()
		check(e)
		require(len(rows) == 2 && reflect.DeepEqual(rows[0], []string{"version", "benchmark", "size", "sample", "batch", "init_time", "run_time"}), "unexpected timing CSV")
		f := rows[1]
		require(len(f) == 7 && f[0] == "2.1.0" && f[1] == c.Scene && f[3] == "1", "wrong timing row")
		s.Size, e = strconv.Atoi(f[2])
		check(e)
		s.Batch, e = strconv.Atoi(f[4])
		check(e)
		s.InitSeconds, e = strconv.ParseFloat(f[5], 64)
		check(e)
		s.RunSeconds, e = strconv.ParseFloat(f[6], 64)
		check(e)
		require(s.Size == c.Size, "unexpected scene size")
	}
	require(s.Batch == batch && finite(s.RunSeconds) && s.RunSeconds > 0 && finite(s.InitSeconds) && s.InitSeconds >= 0, "invalid timing or batch")
	return s
}
func (a *App) sample(stage string, c Case, side Side, batch int) Sample {
	var args []string
	if c.Driver == "munkbench_mixed" {
		args = []string{"--scene", c.Scene, "--count", strconv.Itoa(c.Count), "--steps", strconv.Itoa(c.Steps), "--solver", side.Solver}
	} else {
		args = []string{"-b", c.Scene, "--profile", c.Profile, "--contact-solver", side.Solver}
	}
	args = append(args, "--batch", strconv.Itoa(batch), "--samples", "1", "--warmup", "0")
	data := a.invoke(stage, c.Driver, side.Variant, args)
	s := parseSample(data, c, batch)
	s.Invocation = a.sequence
	return s
}
func decodeSummary(data []byte) map[string]any {
	var x map[string]any
	check(json.Unmarshal(data, &x))
	bs, ok := x["benchmarks"].([]any)
	require(ok && len(bs) == 26, "expected 26 validation scenes")
	seen := map[string]bool{}
	for _, v := range bs {
		b := v.(map[string]any)
		name := b["benchmark"].(string)
		require(!seen[name], "duplicate scene")
		seen[name] = true
		cs := b["checkpoints"].([]any)
		total := b["simulated_steps"].(float64)
		require(total > 0, "invalid simulated steps")
		expected := []float64{}
		for _, step := range []float64{0, 1, 10, 50, 180, 181} {
			if step < total {
				expected = append(expected, step)
			}
		}
		expected = append(expected, total)
		require(len(cs) == len(expected), "missing checkpoints")
		for i, p := range cs {
			checkpoint := p.(map[string]any)
			require(checkpoint["step"] == expected[i], "incorrect checkpoint sequence")
			require(checkpoint["invalid_values"] == float64(0), "nonfinite checkpoint")
		}
	}
	return x
}
func (a *App) validate(mixed []Case) {
	fmt.Println("Checking all 26 scenes at both sizes, both solvers, both builds, twice...")
	var records []any
	for _, profile := range []string{"smoke", "reference"} {
		for _, solver := range []string{"original", "avx2"} {
			args := []string{"--profile", profile, "--contact-solver", solver, "--summary-json", "--checkpoints", "0,1,10,50,180,181,final"}
			var baseline map[string]any
			for _, variant := range []string{"baseline", "candidate"} {
				x := decodeSummary(a.invoke("validate", "munkbench", variant, args))
				repeat := decodeSummary(a.invoke("validate", "munkbench", variant, args))
				require(reflect.DeepEqual(x, repeat), "nonrepeatable checkpoint state")
				if variant == "baseline" {
					baseline = x
				} else {
					require(reflect.DeepEqual(baseline, x), "old/new checkpoint mismatch: "+profile+"/"+solver)
				}
			}
			records = append(records, map[string]any{"Profile": profile, "Solver": solver, "OldNewExact": true, "BothRepeatedExact": true})
			writeJSON(filepath.Join(a.out, "validation.json"), records)
		}
	}
	fmt.Println("Checking mixed scene body/joint hashes at six checkpoints...")
	for _, c := range mixed {
		for _, steps := range []int{1, 10, 50, 180, 181, 1200} {
			c.Steps = steps
			hash := ""
			for _, v := range []string{"baseline", "candidate"} {
				for _, solver := range []string{"original", "avx2"} {
					s := a.sample("validate-mixed", c, Side{v, solver}, 1)
					if hash == "" {
						hash = s.StateHash
					}
					require(hash == s.StateHash, "mixed checkpoint mismatch: "+c.ID)
					// Independently repeat both new-build modes at every mixed checkpoint.
					if v == "candidate" {
						r := a.sample("validate-repeat", c, Side{v, solver}, 1)
						require(r.StateHash == hash, "mixed repeat mismatch")
					}
				}
			}
			records = append(records, map[string]any{"Case": c.ID, "Steps": steps, "Hash": hash, "AllFourModesExact": true, "CandidateRepeatedExact": true})
		}
	}
	writeJSON(filepath.Join(a.out, "validation.json"), records)
}
func cases(root, mode string) ([]Case, []Case) {
	var config struct{ Scenarios []Scenario }
	b, e := os.ReadFile(filepath.Join(root, "scenarios.json"))
	check(e)
	check(json.Unmarshal(b, &config))
	require(len(config.Scenarios) == 26, "bad scenario list")
	var all, mixed []Case
	quick := map[string]bool{"Tumbler": true, "ConstraintMix": true, "FallingSquares": true, "FrictionalPyramid": true}
	for _, p := range []string{"smoke", "reference"} {
		for _, s := range config.Scenarios {
			if mode == "quick" && !quick[s.Name] {
				continue
			}
			if mode == "ci" && (p != "smoke" || (s.Name != "Tumbler" && s.Name != "ConstraintMix")) {
				continue
			}
			size := s.SmokeSize
			if p == "reference" {
				size = s.ReferenceSize
			}
			all = append(all, Case{ID: p + "/" + s.Name, Driver: "munkbench", Scene: s.Name, Profile: p, Size: size, Steps: s.Steps})
		}
	}
	for _, n := range []int{64, 256} {
		for _, s := range []string{"light", "dense", "mechanisms"} {
			mixed = append(mixed, Case{ID: fmt.Sprintf("mixed%d/%s", n, s), Driver: "munkbench_mixed", Scene: s, Count: n, Size: n, Steps: 1200})
		}
	}
	return append(all, mixed...), mixed
}
func summarize(pairs []Pair) ([]Summary, []map[string]any, string) {
	groups := map[string][]Pair{}
	var keys []string
	for _, p := range pairs {
		key := p.Case + "|" + p.Label
		if _, ok := groups[key]; !ok {
			keys = append(keys, key)
		}
		groups[key] = append(groups[key], p)
	}
	sort.Strings(keys)
	var rows []Summary
	var text strings.Builder
	text.WriteString("Munk2D native AVX2 mixed-joint benchmark\nNegative = less whole-step time. No outliers removed or control subtraction.\nQuick is selected cases, not a full-suite conclusion.\nLabels: prior-avx2 = old vs new AVX2; primary = candidate original vs AVX2;\naa = candidate original vs itself; original-control = old vs new original.\n\n")
	suites := map[string][]float64{}
	for _, key := range keys {
		ps := groups[key]
		s := Summary{Case: ps[0].Case, Label: ps[0].Label, Pairs: len(ps)}
		var ratios []float64
		for _, p := range ps {
			require(p.Ratio > 0 && finite(p.Ratio), "invalid ratio")
			ratios = append(ratios, p.Ratio)
			if p.Ratio < 1 {
				s.Faster++
			}
			if p.Order == "AB" {
				s.AB++
			} else {
				s.BA++
			}
		}
		s.Percent = 100 * (median(ratios) - 1)
		sort.Float64s(ratios)
		s.MinPercent = 100 * (ratios[0] - 1)
		s.MaxPercent = 100 * (ratios[len(ratios)-1] - 1)
		rows = append(rows, s)
		group := strings.Split(s.Case, "/")[0] + "|" + s.Label
		suites[group] = append(suites[group], 1+s.Percent/100)
		fmt.Fprintf(&text, "%-32s %-17s %+7.2f%% [%+.2f, %+.2f] %d/%d faster, AB/BA %d/%d\n", s.Case, s.Label, s.Percent, s.MinPercent, s.MaxPercent, s.Faster, s.Pairs, s.AB, s.BA)
	}
	var totals []map[string]any
	keys = nil
	for k := range suites {
		keys = append(keys, k)
	}
	sort.Strings(keys)
	text.WriteString("\nEqual-case geometric means of per-case median paired ratios:\n")
	for _, k := range keys {
		x := suites[k]
		logs := 0.
		for _, r := range x {
			logs += math.Log(r)
		}
		pct := 100 * (math.Exp(logs/float64(len(x))) - 1)
		totals = append(totals, map[string]any{"GroupLabel": k, "Cases": len(x), "Percent": pct})
		fmt.Fprintf(&text, "%-40s %+7.2f%%\n", k, pct)
	}
	text.WriteString("\nNoise flags (diagnostic only; no causality or universal speedup claim):\n")
	for _, s := range rows {
		if s.Label == "aa" || s.Label == "original-control" {
			if math.Abs(s.Percent) > 2 || s.MinPercent < -5 || s.MaxPercent > 5 {
				fmt.Fprintf(&text, "CHECK %s %s: median %+.2f%%, range [%+.2f, %+.2f]\n", s.Case, s.Label, s.Percent, s.MinPercent, s.MaxPercent)
			}
		}
	}
	return rows, totals, text.String()
}
func zipResults(dir string) string {
	target := dir + ".zip"
	f, e := os.Create(target)
	check(e)
	w := zip.NewWriter(f)
	e = filepath.WalkDir(dir, func(path string, d os.DirEntry, err error) error {
		if err != nil {
			return err
		}
		if d.IsDir() {
			return nil
		}
		rel, err := filepath.Rel(dir, path)
		if err != nil {
			return err
		}
		z, err := w.Create(filepath.ToSlash(filepath.Join(filepath.Base(dir), rel)))
		if err != nil {
			return err
		}
		b, err := os.ReadFile(path)
		if err == nil {
			_, err = z.Write(b)
		}
		return err
	})
	check(e)
	check(w.Close())
	check(f.Close())
	return target
}
func (a *App) run() (err error) {
	a.metadata = map[string]any{"Status": "Running", "StartedUtc": utc(), "Mode": a.mode, "Platform": runtime.GOOS, "Arch": runtime.GOARCH, "Source": a.manifest, "Protocol": "native mixed ABBA v1", "Timer": a.manifest.Timer, "CPU": a.cpu, "Host": hostInfo(), "StartTelemetry": telemetry(a.cpu), "AutomaticUpload": false}
	writeJSON(filepath.Join(a.out, "metadata.json"), a.metadata)
	defer func() {
		if r := recover(); r != nil {
			err = fmt.Errorf("%v", r)
		}
		status := "PASS"
		if err != nil {
			status = "FAILED"
			if a.ctx.Err() != nil {
				status = "Interrupted"
			}
			a.metadata["Error"] = err.Error()
		}
		a.metadata["Status"] = status
		a.metadata["EndedUtc"] = utc()
		a.metadata["Pairs"] = len(a.pairs)
		a.metadata["EndTelemetry"] = telemetry(a.cpu)
		a.invocations.Close()
		writeJSON(filepath.Join(a.out, "metadata.json"), a.metadata)
		writeJSON(filepath.Join(a.out, "timing-pairs.json"), a.pairs)
		rows, totals, text := summarize(a.pairs)
		writeJSON(filepath.Join(a.out, "summary.json"), map[string]any{"Scenes": rows, "Totals": totals})
		check(os.WriteFile(filepath.Join(a.out, "SUMMARY.txt"), []byte(text), 0644))
		archive := zipResults(a.out)
		fmt.Println("\nStatus:", status, "—", len(a.pairs), "pairs")
		fmt.Println("Send this file back:", archive)
	}()
	a.verify()
	all, mixed := cases(a.root, a.mode)
	writeJSON(filepath.Join(a.out, "cases.json"), all)
	a.validate(mixed)
	comparisons := []Comparison{{"primary", Side{"candidate", "original"}, Side{"candidate", "avx2"}}, {"prior-avx2", Side{"baseline", "avx2"}, Side{"candidate", "avx2"}}, {"aa", Side{"candidate", "original"}, Side{"candidate", "original"}}, {"original-control", Side{"baseline", "original"}, Side{"candidate", "original"}}}
	warmup := 60 * time.Second
	target := 0.08
	blocks := 4
	if a.mode == "quick" {
		blocks = 2
	}
	if a.mode == "ci" {
		blocks = 1
		warmup = time.Second
		target = 0.02
	}
	a.metadata["Cases"] = len(all)
	a.metadata["Blocks"] = blocks
	a.metadata["TargetSeconds"] = target
	a.metadata["WarmupSeconds"] = warmup.Seconds()
	a.metadata["Comparisons"] = comparisons
	fmt.Println("Untimed sustained warmup:", warmup)
	until := time.Now().Add(warmup)
	for n := 0; time.Now().Before(until); n++ {
		c := mixed[n%len(mixed)]
		a.sample("warmup", c, Side{"candidate", "avx2"}, 1)
	}
	batchByID := map[string]int{}
	var calibrations []any
	for i, c := range all {
		fmt.Printf("Calibrating %d/%d %s\n", i+1, len(all), c.ID)
		batch := 1
		for pass := 0; pass < 6; pass++ {
			min := math.Inf(1)
			var trials []Sample
			for _, side := range []Side{{"candidate", "original"}, {"candidate", "avx2"}, {"baseline", "avx2"}} {
				s := a.sample("calibrate", c, side, batch)
				trials = append(trials, s)
				if s.RunSeconds < min {
					min = s.RunSeconds
				}
			}
			calibrations = append(calibrations, map[string]any{"Case": c.ID, "Batch": batch, "Runs": trials})
			if min >= target {
				break
			}
			require(batch < 4096 && pass < 5, "could not reach minimum calibrated sample")
			next := int(math.Ceil(float64(batch) * target / min * 1.1))
			if next <= batch {
				next = batch + 1
			}
			if next > 4096 {
				next = 4096
			}
			batch = next
		}
		batchByID[c.ID] = batch
		writeJSON(filepath.Join(a.out, "calibration.json"), calibrations)
	}
	raw, e := os.Create(filepath.Join(a.out, "timing-pairs.jsonl"))
	check(e)
	defer raw.Close()
	for block := 0; block < blocks; block++ {
		// Rotate, reverse every other block, and interleave comparisons locally.
		for i := 0; i < len(all); i++ {
			idx := (i + block) % len(all)
			if block%2 == 1 {
				idx = len(all) - 1 - idx
			}
			c := all[idx]
			batch := batchByID[c.ID]
			fmt.Printf("Block %d/%d, case %d/%d: %s\n", block+1, blocks, i+1, len(all), c.ID)
			for ci := 0; ci < len(comparisons); ci++ {
				cmp := comparisons[(ci+block)%len(comparisons)]
				order := []string{"A", "B", "B", "A"}
				if (block+idx+ci)%2 == 1 {
					order = []string{"B", "A", "A", "B"}
				}
				results := make([]Sample, 4)
				before := make([]map[string]any, 4)
				after := make([]map[string]any, 4)
				for k, label := range order {
					side := cmp.A
					if label == "B" {
						side = cmp.B
					}
					before[k] = telemetry(a.cpu)
					results[k] = a.sample("timing", c, side, batch)
					after[k] = telemetry(a.cpu)
				}
				for k := 0; k < 4; k += 2 {
					x, y := results[k], results[k+1]
					ord := order[k] + order[k+1]
					if ord == "BA" {
						x, y = y, x
					}
					if c.Driver == "munkbench_mixed" {
						require(x.StateHash == y.StateHash, "mixed timing state mismatch")
					}
					p := Pair{Case: c.ID, Label: cmp.Label, Block: block, Round: 2*block + k/2, Batch: batch, Order: ord, A: x, B: y, Ratio: y.RunSeconds / x.RunSeconds, Before: before[k], After: after[k+1]}
					a.pairs = append(a.pairs, p)
					lineJSON(raw, p)
					check(raw.Sync())
				}
			}
		}
	}
	require(len(a.pairs) == len(all)*len(comparisons)*blocks*2, "incomplete results")
	for _, s := range func() []Summary { r, _, _ := summarize(a.pairs); return r }() {
		require(s.AB == blocks && s.BA == blocks, "unbalanced order")
	}
	a.verify()
	a.metadata["ChecksumsVerifiedAfter"] = true
	return nil
}
func main() {
	mode := flag.String("mode", "", "quick, full, or ci (verification only)")
	out := flag.String("output", "", "new results directory")
	cpu := flag.Int("cpu", -1, "logical CPU (default: CPU2 if allowed)")
	flag.Parse()
	if *mode == "" {
		fmt.Print("Munk2D AVX2 mixed-joint benchmark\nAC power, close heavy apps, keep power settings unchanged.\nF = Full (recommended, longer); Q = Quick (selected cases). [F]: ")
		line, _ := bufio.NewReader(os.Stdin).ReadString('\n')
		if strings.EqualFold(strings.TrimSpace(line), "q") {
			*mode = "quick"
		} else {
			*mode = "full"
		}
	}
	*mode = strings.ToLower(*mode)
	if *mode != "quick" && *mode != "full" && *mode != "ci" {
		fmt.Fprintln(os.Stderr, "Invalid mode")
		os.Exit(2)
	}
	exe, e := os.Executable()
	check(e)
	root := filepath.Dir(exe)
	if *out == "" {
		*out = filepath.Join(root, "results", runtime.GOOS+"-mixed-"+*mode+"-"+time.Now().UTC().Format("20060102T150405Z")+fmt.Sprint("-", os.Getpid()))
	}
	if _, e = os.Stat(*out); !errors.Is(e, os.ErrNotExist) {
		fmt.Fprintln(os.Stderr, "Refusing existing output directory")
		os.Exit(2)
	}
	check(os.MkdirAll(filepath.Join(*out, "raw"), 0755))
	*out, e = filepath.Abs(*out)
	check(e)
	ctx, cancel := signal.NotifyContext(context.Background(), os.Interrupt)
	defer cancel()
	f, e := os.Create(filepath.Join(*out, "invocations.jsonl"))
	check(e)
	a := &App{ctx: ctx, root: root, out: *out, mode: *mode, cpu: *cpu, invocations: f}
	data, e := os.ReadFile(filepath.Join(root, "manifest.json"))
	check(e)
	check(json.Unmarshal(data, &a.manifest))
	a.cpu, e = setupCPU(*cpu)
	check(e)
	fmt.Println("No install, network, admin rights, or system-setting changes. Logical CPU:", a.cpu)
	fmt.Println("Results:", *out)
	if e = a.run(); e != nil {
		fmt.Fprintln(os.Stderr, e)
		os.Exit(1)
	}
}
