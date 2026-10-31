"""Build docs/PRESENTATION.pptx for StressLab — 10 slides.

Site-aligned version (2026-10-17): matches the Team 7 project site
(yoshik08.github.io/OS_Hackathon/) — a fully browser-native stress suite.
No backend: Web Workers + TypedArray memory + Canvas telemetry.
Copy is paraphrased from the site's facts; no measured results are invented
(scenario profiles are labelled as designed/expected).
"""
from pptx import Presentation
from pptx.util import Inches, Pt
from pptx.dml.color import RGBColor
from pptx.enum.text import PP_ALIGN

OUT = "/home/hatch/workspace/os-project/docs/PRESENTATION.pptx"
AMBER = RGBColor(0xB4, 0x5A, 0x00)
AMBER_BRIGHT = RGBColor(0xFF, 0x9E, 0x2C)
DARK = RGBColor(0x0E, 0x11, 0x16)
MUTED = RGBColor(0x6B, 0x74, 0x84)
WHITE = RGBColor(0xFF, 0xFF, 0xFF)
LIGHT = RGBColor(0xC9, 0xD1, 0xDC)

prs = Presentation()
prs.slide_width = Inches(13.333)
prs.slide_height = Inches(7.5)
BLANK = prs.slide_layouts[6]


def add_bg(slide, color):
    fill = slide.background.fill
    fill.solid()
    fill.fore_color.rgb = color


def textbox(slide, left, top, width, height, text, size=18, bold=False, color=DARK, align=PP_ALIGN.LEFT):
    tx = slide.shapes.add_textbox(Inches(left), Inches(top), Inches(width), Inches(height))
    tf = tx.text_frame
    tf.word_wrap = True
    p = tf.paragraphs[0]
    p.text = text
    p.font.size = Pt(size)
    p.font.bold = bold
    p.font.color.rgb = color
    p.font.name = "Calibri"
    p.alignment = align
    return tx


def bullets(slide, left, top, width, height, items, size=17):
    tx = slide.shapes.add_textbox(Inches(left), Inches(top), Inches(width), Inches(height))
    tf = tx.text_frame
    tf.word_wrap = True
    for i, (head, body) in enumerate(items):
        p = tf.paragraphs[0] if i == 0 else tf.add_paragraph()
        p.space_after = Pt(10)
        p.font.name = "Calibri"
        p.font.size = Pt(size)
        run_h = p.add_run()
        run_h.text = "•  " + head + (" — " if body else "")
        run_h.font.bold = True
        run_h.font.color.rgb = DARK
        if body:
            run_b = p.add_run()
            run_b.text = body
            run_b.font.color.rgb = DARK
    return tx


def slide_title(slide, kicker, title, subtitle=None):
    shape = slide.shapes.add_shape(1, Inches(0), Inches(0), prs.slide_width, Inches(0.08))
    shape.fill.solid()
    shape.fill.fore_color.rgb = AMBER
    shape.line.fill.background()
    textbox(slide, 0.7, 0.35, 11.9, 0.4, kicker.upper(), size=12, bold=True, color=AMBER)
    textbox(slide, 0.7, 0.7, 11.9, 0.9, title, size=32, bold=True, color=DARK)
    if subtitle:
        textbox(slide, 0.7, 1.55, 11.9, 0.7, subtitle, size=16, color=MUTED)


def footer(slide, n):
    textbox(slide, 0.7, 7.05, 11.9, 0.3, f"StressLab  •  CPU and Memory Stress Testing Suite  •  {n} / 10", size=10, color=MUTED)


# 1 — Title
s = prs.slides.add_slide(BLANK)
add_bg(s, DARK)
textbox(s, 0.9, 1.0, 11.5, 0.5, "OSSP S9 HACKATHON  •  TEAM 7  •  OCTOBER 17, 2026", size=14, bold=True, color=AMBER_BRIGHT)
textbox(s, 0.9, 1.6, 11.5, 1.6, "StressLab", size=64, bold=True, color=WHITE)
textbox(s, 0.9, 2.9, 11.5, 0.7, "CPU & Memory Stress Testing Suite", size=26, color=WHITE)
textbox(s, 0.9, 3.7, 11.5, 0.9, "Stress your machine on purpose.\nA controlled, browser-native workload generator and memory saturation suite.", size=17, color=LIGHT)
textbox(s, 0.9, 5.3, 11.5, 0.5, "Runs entirely in the browser  •  No installs, no backend", size=15, color=AMBER_BRIGHT)
textbox(s, 0.9, 6.7, 11.5, 0.4, "1 / 10", size=10, color=MUTED)

# 2 — Problem
s = prs.slides.add_slide(BLANK)
slide_title(s, "Problem statement", "Terminal stress tools are powerful — and hard to use safely",
            "Developers need answers: how does a machine behave at 50% vs 95% CPU? When does it start swapping?")
bullets(s, 0.7, 2.5, 11.9, 4.2, [
    ("Hard to control", "no precise intensity %, no workload choice, no duration cap"),
    ("Hard to observe", "scrolling terminal numbers — no live graphs, no history, no comparison"),
    ("Risky for beginners", "a runaway memory allocation can freeze the machine or crash the tab"),
    ("The gap", "no simple, safe, web-based tool combining control + live visibility + comparison + guardrails"),
])
footer(s, 2)

# 3 — Solution
s = prs.slides.add_slide(BLANK)
slide_title(s, "Solution", "StressLab: pick a workload, watch it live, compare runs")
bullets(s, 0.7, 2.5, 11.9, 4.2, [
    ("Control deck", "threads, duty cycle (10–100%), duration, one of four compute algorithms, target RAM"),
    ("Watch live", "throughput (ops/sec), stability score, allocated + peak RAM, latency — sampled every 500 ms"),
    ("Stay safe", "Emergency Stop kills workers instantly; memory auto-releases to 0 MB on stop or completion"),
    ("History & compare", "every run recorded with a verdict; compare runs side by side; export JSON / CSV"),
    ("Zero setup", "open the page and run — host specs are read from standard browser APIs"),
])
footer(s, 3)

# 4 — Architecture
s = prs.slides.add_slide(BLANK)
slide_title(s, "System design", "One page, three cooperating parts — no backend")
bullets(s, 0.7, 2.5, 11.9, 4.2, [
    ("Main UI thread", "controls, state, Canvas oscilloscopes, safety watchdog — never runs the workload itself"),
    ("Worker pool (1..N)", "dedicated Web Workers, one compute algorithm each, talking to the UI via messages"),
    ("Memory engine", "chunked TypedArray allocation, 4 KB page-touching, Hold mode, deterministic release"),
    ("Stability scoring", "500 ms ticks feed a scoring engine that turns throughput variance + latency into a 0–100 rating"),
    ("Why this split?", "compute lives on isolated threads, so the UI stays responsive while the machine is saturated"),
])
footer(s, 4)

# 5 — Implementation: CPU
s = prs.slides.add_slide(BLANK)
slide_title(s, "Implementation — CPU", "Four algorithms, four different ways to stress a core")
bullets(s, 0.7, 2.5, 11.9, 4.2, [
    ("01 · Prime Search", "segmented sieve + trial division over a 20M range — heavy integer ALU work"),
    ("02 · Matrix Multiply", "Float64 36×36 tensor dot products — saturates caches and the FPU"),
    ("03 · SHA-256 Hash Loop", "bitwise rotation and logical operations — exercises 32-bit logic paths"),
    ("04 · Sort + Search", "quicksort + binary search probes — branch mispredictions and cache misses"),
    ("Duty cycle", "intensity throttles each worker from 10% (time-sliced) to 100% (tight loop)"),
])
footer(s, 5)

# 6 — Implementation: memory + telemetry
s = prs.slides.add_slide(BLANK)
slide_title(s, "Implementation — memory & telemetry", "Real physical RAM, measured honestly")
bullets(s, 0.7, 2.5, 11.9, 4.2, [
    ("Demand-paged allocation", "memory is allocated in chunks as TypedArrays / ArrayBuffers"),
    ("4 KB page-touching", "writes at 4,096-byte strides force the OS to commit real physical frames — lazy zero pages don't count"),
    ("Auto-release", "on stop or completion every reference is dropped, so memory returns to 0 MB"),
    ("500 ms telemetry", "throughput sampled with high-resolution timers; variance and latency spikes feed the stability score"),
    ("Host diagnostics", "CPU concurrency, device memory, platform, viewport, network, timer resolution — via standard browser APIs"),
])
footer(s, 6)

# 7 — Technologies
s = prs.slides.add_slide(BLANK)
slide_title(s, "Technologies used", "Web platform only — nothing to install")
bullets(s, 0.7, 2.5, 5.8, 4.2, [
    ("Concurrency", "Web Workers, loaded from dynamic Blob URLs"),
    ("Memory", "TypedArray / ArrayBuffer"),
    ("Visualisation", "Canvas 2D oscilloscopes"),
    ("Timing", "performance.now() sampling"),
])
bullets(s, 6.9, 2.5, 5.7, 4.2, [
    ("Host info", "standard navigator APIs"),
    ("Export", "JSON / CSV from run history"),
    ("Hosting", "GitHub Pages"),
    ("Run it", "open the site — no server, no install"),
])
footer(s, 7)

# 8 — Test scenarios
s = prs.slides.add_slide(BLANK)
slide_title(s, "Standardised test scenarios", "Four presets, four expected behaviour profiles",
            "Profiles below are the suite's designed expectations per scenario — each run reports its actual numbers.")
bullets(s, 0.7, 2.5, 11.9, 4.2, [
    ("Light — 2 threads, 256 MB, 30 s", "Prime / ALU workload — smooth linear throughput, minimal thermal footprint"),
    ("Medium — 4 threads, 512 MB, 60 s", "Matrix / FPU workload — moderate core saturation, steady throughput"),
    ("Heavy — 8 threads, 1024 MB, 60 s", "Hashing / scatter workload — near-total core exhaustion, throttling visible on laptops"),
    ("Extreme — max cores, 2048 MB, 120 s", "Sort probes + heavy RAM — cache exhaustion, OS memory-compression guards, crash-immunity check"),
])
footer(s, 8)

# 9 — Testing & results
s = prs.slides.add_slide(BLANK)
slide_title(s, "Testing & results", "Every run produces a verdict — and the evidence behind it")
bullets(s, 0.7, 2.5, 11.9, 4.2, [
    ("What a run reports", "verdict, duration, total operations, average + peak ops/sec, peak memory, stability score"),
    ("Stability score", "0–100 from throughput variance and latency spikes — high score means smooth, consistent output"),
    ("Safety checks by construction", "Stop terminates workers immediately; memory counter returns to 0 MB; compute never blocks the UI thread"),
    ("Compare in history", "runs sit side by side with workload, threads / RAM, peak throughput, peak RAM, and stability"),
    ("Honest note", "throughput numbers are relative to the host — compare runs on the same machine, don't quote absolutes"),
])
footer(s, 9)

# 10 — Impact + team
s = prs.slides.add_slide(BLANK)
slide_title(s, "Impact & team", "Who this is for — and who built it")
bullets(s, 0.7, 2.5, 11.9, 3.0, [
    ("Students", "see scheduling, parallelism, paging, and saturation live instead of in a textbook"),
    ("Developers / QA", "check how an app behaves when the machine is busy"),
    ("DevOps", "validate a small server before deploying; compare configurations"),
    ("Future work", "disk / network / GPU stress, more algorithms, saved benchmark suites"),
])
textbox(s, 0.7, 5.5, 11.9, 0.4, "TEAM 7  •  Kairav Vashi (2520030582) — Architecture & Worker Concurrency  •  Sai Harshith Reddy (2520030375) — Memory & Safety Watchdogs  •  A. Yoshik (2520030574) — Telemetry, Canvas Instruments & UX", size=12, bold=True, color=AMBER)
textbox(s, 0.7, 6.1, 11.9, 0.6, "Thank you — live demo next: Light preset → Heavy preset → compare runs.", size=16, bold=True, color=AMBER)
footer(s, 10)

prs.save(OUT)
print(f"Saved {OUT} with {len(prs.slides._sldIdLst)} slides")
