# otlp — generated OTLP metrics messages

`common.pb.h`/`resource.pb.h`/`metrics.pb.h`/`metrics_service.pb.h` (here) and
their `.pb.c` counterparts (`../../src/otlp/`) are generated, not
hand-written. They are checked in so the firmware build needs no
`protoc`/nanopb toolchain — only `nanopb/Nanopb`'s runtime (`pb.h`,
`pb_encode.c`, `pb_common.c`) from `platformio.ini`'s `lib_deps`. The `.c`
files live under `src/` rather than here because PlatformIO only compiles
sources under `src_dir` — `include/` is header search path only.

The `.proto` files are a hand-trimmed subset of OpenTelemetry's own
`opentelemetry/proto/{common,resource,metrics,collector/metrics}/v1/*.proto`:
only `ExportMetricsServiceRequest`, `ResourceMetrics`, `ScopeMetrics`,
`Metric`/`Gauge`/`NumberDataPoint`, `Resource`, `KeyValue` and the one
`AnyValue` variant (`string_value`) this firmware ever sends — Sum,
Histogram, Summary, exemplars, InstrumentationScope and every other
`AnyValue` variant are left out entirely, since this firmware only ever
writes instantaneous readings tagged with a couple of string attributes.
Field numbers match the real schema for every field kept, so a real OTLP
receiver decodes what this firmware sends exactly as if the untrimmed
message had been sent with everything else left at its default.

`*.options` bounds every repeated/string field to a fixed `max_count`/
`max_size` so nanopb generates plain static C structs — no heap, no
callbacks. `metrics.options`' bound on `ScopeMetrics.metrics` must be kept
equal to `GRAFANA_QUEUE_CAPACITY` in `include/config.h` —
`test_otlp_metrics_builder` asserts the two haven't drifted apart.

## Regenerating

```sh
pip3 install --user nanopb   # provides nanopb_generator / protoc-gen-nanopb
export PATH="$(python3 -m site --user-base)/bin:$PATH"
cd include/otlp
protoc -I. \
  -I"$(python3 -c 'import nanopb,os;print(os.path.dirname(nanopb.__file__))')/generator/proto/.." \
  --plugin=protoc-gen-nanopb=protoc-gen-nanopb \
  --nanopb_out=. \
  common.proto resource.proto metrics.proto metrics_service.proto
mv common.pb.c resource.pb.c metrics.pb.c metrics_service.pb.c ../../src/otlp/
```

Generated with `protoc` (Homebrew, protobuf compiler) and `nanopb-0.4.9.1`.
Re-run after editing any `.proto` or `.options` file in this directory, and
diff the result before committing — a bound that silently widens (or a
struct layout that silently changes) is exactly the kind of drift the
checked-in `.proto`/`.options` sources exist to make visible.
