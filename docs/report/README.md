# Interactive report

Open `docs/report/index.html` directly in a browser, or serve the repository for
local development and open `/docs/report/`. The report works offline without map
tiles or external libraries. `tools/route_map.py` remains the separate HTTP-served
street-map viewer and follows the OSM tile policy.

## Files and ownership

- `index.html`: hierarchy, contracts and explanation text.
- `report.js`: interactions and stage/failure descriptions; educational SVGs are not
  captured engine traces. User-imported JSON stays in the browser and is never sent.
- `report.css`: responsive presentation and accessible controls.
- `fixtures/`: small recorded engine outputs. `example-route.json` is the original
  fixed-start demo from the quality integration; `area-route.json` is the new
  three-candidate integration run (21 eligible starts within 500 m). Both selected
  node 281756094, scenic, target 5 km, 100 iterations, seed 777, four workers, using
  the simplified Karlsruhe regional graph with no elevation overlay. Wall times
  are observations from individual runs, not comparative benchmarks.
- `data.js`: generated CLI inventory, benchmark summaries and recorded route data.
- `source-lock.json`: reviewed file hashes, review date/note and combined fingerprint.

## Maintenance contract

Run `python3 tools/build_report.py --check` before merging. CTest and GitHub Actions
run this check too. Relevant additions, edits and deletions in source, tests, tools,
docs, CMake and CI invalidate the review. Generated `data.js` is compared against
regeneration from source fixtures/CSVs; do not edit it by hand.

When the check fails:

1. Read the changed code. Update affected stages, inputs, output contracts, failure
   cases, structure guidance and tests. Add a focused regression for new behavior.
2. If a performance claim changes, collect matched evidence first. Existing archived
   measurements must retain their original provenance, not be relabeled as current.
3. Update fixtures when their meaning changes; label new samples and their commands.
4. Run `python3 tools/build_report.py --refresh --note "Describe the behavioral change and evidence reviewed"`.
5. Commit the explanations, evidence, generated bundle and manifest together.

The hook detects drift; it cannot prove the accuracy of a human/agent explanation.
Refreshing hashes alone is not a review. Code links target main so they remain
navigable after merges; exact reviewed content is identified by the manifest hashes.

## Validation and future work

`ctest --test-dir build/quality --output-on-failure` includes source-drift/tampering
checks plus coordinate/area end-to-end tests on generated OSM fixtures. Browser QA
covers step switching, command generation, route point selection and benchmark modes.
See [issue #4](https://github.com/SpeziMM/VeloGraph/issues/4) for missing profiling and
[issue #5](https://github.com/SpeziMM/VeloGraph/issues/5) for feasibility classification.
