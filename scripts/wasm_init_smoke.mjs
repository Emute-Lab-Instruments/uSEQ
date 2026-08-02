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

const evalOk = (source) => {
  const value = mod.ccall("useq_eval", "string", ["string"], [source]);
  if (value.startsWith("Error")) {
    throw new Error(`generated WASM eval failed for ${source}: ${value}`);
  }
};
const activeDiagnostics = () =>
  JSON.parse(mod.ccall("useq_active_diagnostics", "string", [], []));

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

const exactBoundaryNumber = mod.ccall(
  "useq_eval",
  "string",
  ["string"],
  ["12"],
);
if (exactBoundaryNumber !== "12") {
  throw new Error("generated WASM did not parse a numeric token at source end");
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

// Exercise the actual generated linear-memory ABI for retained synth-control
// programs, not merely the native wrapper compiled on a 64-bit host.
evalOk("(useq-clear)");
evalOk("(define wasm-synth-dep 110)");
evalOk(
  '(synth "osc/sine" :name "wasm-a" :freq wasm-synth-dep :amp wasm-synth-dep)',
);
evalOk(
  '(synth "osc/sine" :name "wasm-b" :freq wasm-synth-dep :amp wasm-synth-dep)',
);
const controlPtr = mod.ccall("malloc", "number", ["number"], [8 * 8]);
if (!controlPtr) throw new Error("generated WASM control buffer allocation failed");
const tickControls = (time) =>
  mod.ccall(
    "useq_tick_synth_controls",
    "number",
    ["number", "number", "number"],
    [time, controlPtr, 8],
  );
const controlValues = (count) =>
  Array.from(mod.HEAPF64.subarray(controlPtr / 8, controlPtr / 8 + count));

try {
  if (
    tickControls(4) !== 4 ||
    controlValues(4).some((value) => value !== 110)
  ) {
    throw new Error("generated WASM did not seed synth-control LKG values");
  }

  evalOk("(defn wasm-synth-dep [x] x)");
  const rejected = activeDiagnostics();
  const rejectedOrder = rejected.map(
    ({ subject, identity, control }) => `${subject}:${identity}:${control}`,
  );
  if (
    JSON.stringify(rejectedOrder) !==
      JSON.stringify([
        "synth-control:wasm-a:freq",
        "synth-control:wasm-a:amp",
        "synth-control:wasm-b:freq",
        "synth-control:wasm-b:amp",
      ]) ||
    rejected.some(
      (diagnostic) =>
        diagnostic.triggered_by !== "wasm-synth-dep" ||
        diagnostic.status !== "retained",
    )
  ) {
    throw new Error(
      "generated WASM did not serialize synth diagnostics in artifact order",
    );
  }
  if (
    tickControls(5) !== 4 ||
    controlValues(4).some((value) => value !== 110)
  ) {
    throw new Error("generated WASM did not retain synth-control LKG on reject");
  }

  evalOk('(synth "osc/sine" :name "wasm-a" :freq 220 :amp 0.2)');
  const afterReplacement = activeDiagnostics();
  if (
    afterReplacement.length !== 2 ||
    afterReplacement.some((diagnostic) => diagnostic.identity !== "wasm-b")
  ) {
    throw new Error("generated WASM did not replace synth diagnostic subjects");
  }
  evalOk("(define wasm-synth-dep 120)");
  if (activeDiagnostics().length !== 0) {
    throw new Error("generated WASM did not clear repaired synth diagnostics");
  }

  evalOk("(define wasm-synth-amp 0.5)");
  evalOk(
    '(synth "osc/sine" :name "wasm-b" :freq 330 :amp wasm-synth-amp)',
  );
  evalOk("(defn wasm-synth-amp [x] x)");
  const optionalFailure = activeDiagnostics();
  if (
    optionalFailure.length !== 1 ||
    optionalFailure[0].identity !== "wasm-b" ||
    optionalFailure[0].control !== "amp"
  ) {
    throw new Error("generated WASM did not attribute optional-control reject");
  }
  evalOk('(synth "osc/sine" :name "wasm-b" :freq 330)');
  if (activeDiagnostics().length !== 0) {
    throw new Error("generated WASM retained a removed synth-control slot");
  }

  evalOk("(define wasm-synth-clear 440)");
  evalOk(
    '(synth "osc/sine" :name "wasm-clear" :freq wasm-synth-clear)',
  );
  evalOk("(defn wasm-synth-clear [x] x)");
  if (activeDiagnostics()[0]?.identity !== "wasm-clear") {
    throw new Error("generated WASM did not expose clear-test diagnostic");
  }
  evalOk("(useq-clear)");
  if (activeDiagnostics().length !== 0) {
    throw new Error("generated WASM did not clear synth diagnostic subjects");
  }
  evalOk("(defstate wasm-clear-dt 0 (+ wasm-clear-dt dt))");
  if (tickControls(0) !== 0) {
    throw new Error("generated WASM retained its authoritative frontier after clear");
  }
  const postClearState = mod.ccall(
    "useq_eval",
    "string",
    ["string"],
    ["wasm-clear-dt"],
  );
  if (postClearState !== "0") {
    throw new Error("generated WASM retained its dt origin after clear");
  }
} finally {
  mod.ccall("free", null, ["number"], [controlPtr]);
}

console.log(
  "generated WASM init/eval, live-edit metadata, and synth diagnostics passed",
);
