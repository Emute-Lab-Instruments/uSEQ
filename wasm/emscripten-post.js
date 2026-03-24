// Expose live heap views on the modularized Emscripten module.
//
// The editor's typed batch bridge needs HEAPF64 to read values written by
// useq_eval_outputs_time_window_into(). Emscripten keeps the heap views as
// closure locals in MODULARIZE output, so we attach getter-backed properties
// to Module here instead of copying a potentially stale view.

Object.defineProperty(Module, "HEAPF64", {
  configurable: true,
  enumerable: true,
  get() {
    return HEAPF64;
  },
});
