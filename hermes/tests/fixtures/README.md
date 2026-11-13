# HBC parser fixtures

`v96.hbc` through `v99.hbc` come from `../fixture.js`. `large96.hbc` through
`large99.hbc` come from `large.js`, whose 10,000 explicit additions force
40,008-byte function bodies and overflow headers. Both JavaScript inputs are
checked in; tests do not construct source programs.

The main fixture includes `/hello/g` to exercise both string-ID operands of
`CreateRegExp`. Its 35-parameter generator overflows the small header in HBC98
and HBC99; its 128-parameter generator overflows it in all four versions. These
check function kinds and rejection of generator return hooks in HBC97–99
(HBC96 does not encode function kinds).

To regenerate with the official compilers pinned in `src/formats.lock.json`:

```sh
uv run --locked --group build python hermes/tools/build_toolchains.py
uv run --locked python hermes/tools/generate_fixtures.py --output build/fixtures
```

Use `--versions 99` on either command to select a subset. The generator copies
the fixed inputs into a temporary directory using their historical relative
filenames, because Hermes embeds source paths in HBC. This keeps the binary
fixtures reproducible across checkout locations. Ordinary host tests only read
the checked-in HBC files.
