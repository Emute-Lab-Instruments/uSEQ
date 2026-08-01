import { readFileSync } from "node:fs";
import { resolve } from "node:path";

const artifactDir = resolve(process.argv[2] ?? "wasm");
const wasmBinary = readFileSync(resolve(artifactDir, "useq.wasm"));
const glueSource = readFileSync(resolve(artifactDir, "useq.js"), "utf8");
const createModule = new Function(`${glueSource}; return createModule;`)();
const mod = await createModule({
  instantiateWasm(imports, receiveInstance) {
    WebAssembly.instantiate(wasmBinary, imports).then(({ instance }) =>
      receiveInstance(instance),
    );
    return {};
  },
});

if (mod.ccall("useq_output_health", "number", ["string"], ["a1"]) !== -1) {
  throw new Error("generated WASM health query did not fail closed before init");
}
mod.ccall("useq_init", null, [], []);
if (
  mod.ccall("useq_output_health", "number", ["string"], ["a2"]) !== 0 ||
  mod.ccall("useq_output_health", "number", ["string"], ["bad"]) !== -1
) {
  throw new Error("generated WASM health query did not distinguish idle/invalid");
}

const healthProgram = mod.ccall(
  "useq_eval",
  "string",
  ["string"],
  ["(a1 (* (* (- t 2) 1e308) 1e308))"],
);
if (healthProgram.startsWith("Error")) {
  throw new Error(`generated WASM health program failed: ${healthProgram}`);
}
const tick = (time) => mod.ccall(
  "useq_tick_and_project",
  "number",
  ["string", "number", "number", "number", "number", "number", "number"],
  ["[]", time, 0, 0, 0, 0, 0],
);
const health = () =>
  mod.ccall("useq_output_health", "number", ["string"], ["a1"]);
if (tick(1) !== 0 || health() !== 3) {
  throw new Error("generated WASM did not expose first-sample Error health");
}
if (tick(2) !== 0 || health() !== 1) {
  throw new Error("generated WASM did not expose recovered Running health");
}
if (tick(3) !== 0 || health() !== 2) {
  throw new Error("generated WASM did not expose LKG Fallback health");
}

const result = mod.ccall("useq_eval", "string", ["string"], ["(a1 1)"]);
if (result.startsWith("Error")) {
  throw new Error(`generated WASM initialization/eval failed: ${result}`);
}
if (health() !== 1) {
  throw new Error("generated WASM assignment did not clear stale failure health");
}
const synthResult = mod.ccall(
  "useq_eval",
  "string",
  ["string"],
  ['(synth "osc/sine" :name "smoke" :freq 440 :amp 0.25)'],
);
if (synthResult.startsWith("Error")) {
  throw new Error(`generated WASM synth eval failed: ${synthResult}`);
}
const artifacts = JSON.parse(
  mod.ccall("useq_synth_artifacts", "string", [], []),
);
if (
  artifacts.abi !== 2 ||
  artifacts.declarations?.[0]?.identity !== "smoke" ||
  artifacts.controls?.length !== 2 ||
  !Array.isArray(artifacts.connections)
) {
  throw new Error("generated WASM did not expose the synth ABI-2 envelope");
}

const liveEditResult = mod.ccall(
  "useq_eval",
  "string",
  ["string"],
  [
    '(a2 (live-edit true :id "smoke-bool")) ' +
      '(a3 (live-edit :up :id "smoke-keyword" :options [:left :up :right])) ' +
      '(a4 (live-edit 0.5 :id "smoke-number" :min 0 :max 1 :step 0.01 :precision 2))',
  ],
);
if (liveEditResult.startsWith("Error")) {
  throw new Error(`generated WASM live-edit eval failed: ${liveEditResult}`);
}
const liveSlots = JSON.parse(
  mod.ccall("useq_get_live_slots", "string", [], []),
);
const boolSlot = liveSlots.find((slot) => slot.id === "smoke-bool");
const keywordSlot = liveSlots.find((slot) => slot.id === "smoke-keyword");
const numberSlot = liveSlots.find((slot) => slot.id === "smoke-number");
if (
  boolSlot?.variant !== "boolean" ||
  boolSlot.seed !== 1 ||
  keywordSlot?.variant !== "keyword" ||
  keywordSlot.seed !== 1 ||
  JSON.stringify(keywordSlot.options) !==
    JSON.stringify([":left", ":up", ":right"]) ||
  numberSlot?.variant !== "numeric" ||
  numberSlot.step !== 0.01 ||
  numberSlot.precision !== 2
) {
  throw new Error("generated WASM did not preserve live-edit slot metadata");
}

console.log("generated WASM init/eval and live-edit metadata smoke passed");
