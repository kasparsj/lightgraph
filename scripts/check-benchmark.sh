#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BENCHMARK_BIN="${1:-$ROOT_DIR/build/preset-static-analysis/lightgraph_core_benchmark}"
MIN_FPS="${2:-${LIGHTGRAPH_MIN_BENCHMARK_FPS:-10000}}"

if [[ ! -x "$BENCHMARK_BIN" ]]; then
  echo "Benchmark binary not found or not executable: $BENCHMARK_BIN" >&2
  exit 1
fi

output="$("$BENCHMARK_BIN")"
echo "$output"

fps="$(echo "$output" | awk -F': ' '/Benchmark minimum frames\/sec/ {print $2}' | tail -n 1)"
if [[ -z "$fps" ]]; then
  fps="$(echo "$output" | awk -F': ' '/Benchmark approx frames\/sec/ {print $2}' | tail -n 1)"
fi
if [[ -z "$fps" ]]; then
  echo "Unable to parse benchmark frames/sec from output" >&2
  exit 1
fi

python3 - "$fps" "$MIN_FPS" <<'PY'
import sys

measured = float(sys.argv[1])
threshold = float(sys.argv[2])
if measured < threshold:
    print(
        f"Benchmark regression: measured {measured:.2f} fps, "
        f"minimum required {threshold:.2f} fps",
        file=sys.stderr,
    )
    raise SystemExit(1)
print(f"Benchmark guardrail passed: {measured:.2f} fps >= {threshold:.2f} fps")
PY
