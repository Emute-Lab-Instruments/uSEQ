#!/usr/bin/env node

/**
 * JSONL session probe for the generated uSEQ WASM artifact.
 *
 * This implements the same black-box protocol as
 * test/signal_engine/signal_engine_probe.cpp so one YAML corpus can execute
 * against native and actual generated WASM without a second set of oracles.
 */

import { readFileSync } from "node:fs";
import { createInterface } from "node:readline";
import { fileURLToPath } from "node:url";
import { dirname, resolve } from "node:path";

const scriptDir = dirname(fileURLToPath(import.meta.url));
const artifactDir = resolve(
  process.env.USEQ_WASM_ARTIFACT_DIR ?? resolve(scriptDir, "..", "wasm"),
);

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

const ccall = (name, returnType, argTypes = [], args = []) =>
  mod.ccall(name, returnType, argTypes, args);

ccall("useq_init", null);

const healthNames = new Map([
  [0, "idle"],
  [1, "running"],
  [2, "fallback"],
  [3, "error"],
]);

function respond(value) {
  process.stdout.write(`${JSON.stringify(value)}\n`);
}

function lastError() {
  return ccall("useq_last_error", "string") || "WASM operation failed";
}

function lastDiagnostics() {
  const raw = ccall("useq_last_diagnostics", "string");
  let parsed;
  try {
    parsed = JSON.parse(raw || "[]");
  } catch {
    return [];
  }
  if (!Array.isArray(parsed)) return [];
  return parsed.map((diagnostic) => ({
    severity: diagnostic.severity,
    category: diagnostic.category,
    span: [diagnostic.start, diagnostic.end],
    msg: diagnostic.message,
    ...(diagnostic.suggestion === undefined
      ? {}
      : { suggestion: diagnostic.suggestion }),
  }));
}

function evalCode(code) {
  const result = ccall("useq_eval", "string", ["string"], [code]);
  const diagnostics = lastDiagnostics();
  if (result.startsWith("Error:")) {
    return {
      ok: false,
      diagnostics,
      ...(diagnostics.length === 0 ? { error: result } : {}),
    };
  }

  const response = { ok: true };
  // useq_eval formats numeric EvalResult values as a complete decimal token.
  // Text/data/ok results remain non-numeric and deliberately omit `value`.
  if (/^[+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?$/.test(result)) {
    const value = Number(result);
    if (Number.isFinite(value)) response.value = value;
  }
  if (diagnostics.length > 0) response.diagnostics = diagnostics;
  return response;
}

function sampleOutput(output, times) {
  const health = ccall("useq_output_health", "number", ["string"], [output]);
  if (health < 0 || health === 0) {
    return { ok: false, error: `Output ${output} not assigned` };
  }
  const values = [];
  for (const time of times) {
    const value = ccall(
      "useq_eval_output",
      "number",
      ["string", "number"],
      [output, time],
    );
    if (Number.isNaN(value)) {
      return { ok: false, error: lastError() };
    }
    values.push(value);
  }
  return { ok: true, values };
}

function tick(time) {
  const result = ccall(
    "useq_tick_and_project",
    "number",
    ["string", "number", "number", "number", "number", "number", "number"],
    ["[]", time, 0, 0, 0, 0, 0],
  );
  return result < 0 ? { ok: false, error: lastError() } : { ok: true };
}

function configure(request) {
  if (request.failure_mode !== undefined) {
    const mode = request.failure_mode === "lkg"
      ? 0
      : request.failure_mode === "zero"
        ? 1
        : -1;
    if (mode < 0) {
      return {
        ok: false,
        error: 'config: failure_mode must be "lkg" or "zero"',
      };
    }
    return ccall("useq_set_failure_mode", "number", ["number"], [mode]) === mode
      ? { ok: true }
      : { ok: false, error: "failure-mode update rejected" };
  }
  if (request.opt_level === undefined) {
    return { ok: false, error: 'config: missing "opt_level" or "failure_mode"' };
  }
  return request.opt_level === 0
    ? { ok: false, error: "unsupported" }
    : { ok: true };
}

function handle(request) {
  if (!request || typeof request !== "object" || Array.isArray(request)) {
    return { ok: false, error: "request must be an object" };
  }
  switch (request.op) {
    case "eval":
      return typeof request.code === "string"
        ? evalCode(request.code)
        : { ok: false, error: 'eval: missing "code"' };
    case "sample":
      return typeof request.output === "string" && Array.isArray(request.times)
        ? sampleOutput(request.output, request.times)
        : { ok: false, error: "sample: malformed output/times" };
    case "tick":
      return typeof request.t === "number"
        ? tick(request.t)
        : { ok: false, error: 'tick: missing "t"' };
    case "clear":
      return evalCode("(useq-clear)").ok
        ? { ok: true }
        : { ok: false, error: "clear failed" };
    case "health": {
      if (typeof request.output !== "string") {
        return { ok: false, error: 'health: missing "output"' };
      }
      const code = ccall(
        "useq_output_health",
        "number",
        ["string"],
        [request.output],
      );
      return healthNames.has(code)
        ? { ok: true, health: healthNames.get(code) }
        : { ok: false, error: `Unknown output ${request.output}` };
    }
    case "config":
      return configure(request);
    default:
      return { ok: false, error: `unknown op: ${request.op ?? "<missing>"}` };
  }
}

const input = createInterface({ input: process.stdin, crlfDelay: Infinity });
for await (const line of input) {
  if (line.trim() === "") continue;
  try {
    respond(handle(JSON.parse(line)));
  } catch (error) {
    respond({ ok: false, error: error instanceof Error ? error.message : String(error) });
  }
}
