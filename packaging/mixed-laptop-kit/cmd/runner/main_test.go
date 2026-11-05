package main

import (
	"archive/zip"
	"math"
	"os"
	"path/filepath"
	"testing"
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
