// Build-host helper. Never included in the laptop package.
package main

import (
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"flag"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"runtime"
	"strings"
)

const baseline = "0c4b0ff81d1d47040c7043d487c9414058c30faa"
const candidate = "19dfdeacbf14b0b24a0d789e0330dab15c82a1d3"

func check(e error) {
	if e != nil {
		panic(e)
	}
}
func read(p string) []byte     { b, e := os.ReadFile(p); check(e); return b }
func write(p string, b []byte) { check(os.WriteFile(p, b, 0644)) }
func hash(p string) string     { h := sha256.Sum256(read(p)); return hex.EncodeToString(h[:]) }
func replace(p, old, new string) {
	b := string(read(p))
	if strings.Count(b, old) != 1 {
		panic("nonunique patch: " + p)
	}
	write(p, []byte(strings.Replace(b, old, new, 1)))
}
func command(args ...string) string {
	b, e := exec.Command(args[0], args[1:]...).CombinedOutput()
	if e != nil {
		panic(string(b) + e.Error())
	}
	return strings.TrimSpace(string(b))
}
func main() {
	stage := flag.String("stage", "sources", "sources or manifest")
	root := flag.String("root", ".", "repository root")
	pkg := flag.String("package", "", "package path")
	compiler := flag.String("compiler", "", "compiler version")
	flag.Parse()
	tools := filepath.Join(*root, "packaging", "mixed-laptop-kit")
	if *stage == "sources" {
		for name, ref := range map[string]string{"baseline": baseline, "candidate": candidate} {
			path := filepath.Join(*root, name)
			if command("git", "-C", path, "rev-parse", "HEAD") != ref || command("git", "-C", path, "status", "--porcelain") != "" {
				panic("source not clean/pinned: " + name)
			}
		}
		// Identical public-API mixed fixture and build flags on both engine versions.
		for _, f := range []string{"mixedbench.c", "CMakeLists.txt"} {
			write(filepath.Join(*root, "baseline", "benchmarks", f), read(filepath.Join(*root, "candidate", "benchmarks", f)))
		}
		if runtime.GOOS == "windows" {
			for _, name := range []string{"baseline", "candidate"} {
				bench := filepath.Join(*root, name, "benchmarks", "munkbench.c")
				replace(bench, "#include <time.h>", "#include <time.h>\n#define WIN32_LEAN_AND_MEAN\n#define NOMINMAX\n#include <windows.h>")
				patch := func(p, pattern, replacement string) {
					b := string(read(p))
					r := regexp.MustCompile(pattern)
					if len(r.FindAllStringIndex(b, -1)) != 1 {
						panic("timer patch not unique")
					}
					write(p, []byte(r.ReplaceAllString(b, replacement)))
				}
				body := "static double\nnow_seconds(void)\n{\n LARGE_INTEGER c,f;\n if(!QueryPerformanceCounter(&c)||!QueryPerformanceFrequency(&f)) exit(1);\n return (double)c.QuadPart/(double)f.QuadPart;\n}"
				patch(bench, `static double\s+now_seconds\(void\)\s*\{\s*return \(double\)clock\(\)/\(double\)CLOCKS_PER_SEC;\s*\}`, body)
				mixed := filepath.Join(*root, name, "benchmarks", "mixedbench.c")
				replace(mixed, "#include <windows.h>", "#define WIN32_LEAN_AND_MEAN\n#define NOMINMAX\n#include <windows.h>")
				patch(mixed, `(?s)static double seconds\(void\)\n\{.*?\n#endif\n\}`, strings.Replace(body, "now_seconds", "seconds", 1))
			}
		}
		for _, f := range []string{"munkbench.c", "mixedbench.c"} {
			if hash(filepath.Join(*root, "baseline", "benchmarks", f)) != hash(filepath.Join(*root, "candidate", "benchmarks", f)) {
				panic("harness mismatch: " + f)
			}
		}
		fmt.Println("PASS source revisions and identical harnesses; no engine source patched")
		return
	}
	if *stage != "manifest" || *pkg == "" {
		panic("invalid stage/package")
	}
	suffix := ""
	timer := "clock(): Linux PROCESS CPU seconds inside complete physics steps; setup/cleanup excluded"
	if runtime.GOOS == "windows" {
		suffix = ".exe"
		timer = "QueryPerformanceCounter: Windows WALL seconds inside complete physics steps; identical benchmark-only patch on both drivers/builds; setup/cleanup excluded"
	}
	files := []map[string]string{}
	for _, f := range []string{"munkbench-baseline", "munkbench-candidate", "munkbench_mixed-baseline", "munkbench_mixed-candidate", "runner"} {
		n := f + suffix
		files = append(files, map[string]string{"Name": n, "SHA256": hash(filepath.Join(*pkg, n))})
	}
	for _, f := range []string{"scenarios.json", "README.txt"} {
		write(filepath.Join(*pkg, f), read(filepath.Join(tools, f)))
	}
	write(filepath.Join(*pkg, "LICENSE.txt"), read(filepath.Join(*root, "candidate", "LICENSE.txt")))
	m := map[string]any{"Kit": "Munk2D PR17 native AVX2 mixed constraints", "Baseline": baseline, "Candidate": candidate, "Timer": timer, "Compiler": *compiler, "BuildRun": "https://github.com/" + os.Getenv("GITHUB_REPOSITORY") + "/actions/runs/" + os.Getenv("GITHUB_RUN_ID"), "PackagingCommit": os.Getenv("GITHUB_SHA"), "Configuration": "Release/O3/default LTO; generic x86-64 outside existing AVX2 TU; static chipmunk; Windows static CRT /MT; Linux glibc <=2.31", "BenchmarkSHA256": hash(filepath.Join(*root, "candidate", "benchmarks", "munkbench.c")), "MixedBenchmarkSHA256": hash(filepath.Join(*root, "candidate", "benchmarks", "mixedbench.c")), "ScenariosSHA256": hash(filepath.Join(*pkg, "scenarios.json")), "Executables": files}
	b, e := json.MarshalIndent(m, "", "  ")
	check(e)
	write(filepath.Join(*pkg, "manifest.json"), append(b, '\n'))
	fmt.Println("Manifest written")
}
