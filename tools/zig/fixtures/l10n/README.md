# l10n-check fixtures

Small passing inputs of the `zig build test-l10n-check` seeded-fault cases (tools/zig/l10n_check_test.zig).
The test copies them to `zig-out/local-test/l10n-check/<case>/`, breaks the copy in code and asserts the
exact finding line. Nothing here is read by the game or the editors.

- `tables/en.lang`, `tables/de.lang`: a string table pair (UTF-8, CRLF). The test copies `de.lang` to the
  other eight language codes.
- `docs/README.md`, `docs/README_German.md`: a document set with a language bar, headings and one fenced
  code block. The test copies the German file to the other eight language names.
- `literals/ui.zig`: one English literal in `igButton` (the finding) and one wrapped text (no finding).
