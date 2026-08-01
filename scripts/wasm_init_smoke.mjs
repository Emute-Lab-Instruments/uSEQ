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

mod.ccall("useq_init", null, [], []);
const result = mod.ccall("useq_eval", "string", ["string"], ["(a1 1)"]);
if (result.startsWith("Error")) {
  throw new Error(`generated WASM initialization/eval failed: ${result}`);
}
const artifacts = JSON.parse(
  mod.ccall("useq_synth_artifacts", "string", [], []),
);
if (artifacts.abi !== 2 || !Array.isArray(artifacts.connections)) {
  throw new Error("generated WASM did not expose the synth ABI-2 envelope");
}

console.log("generated WASM init/eval smoke passed");
