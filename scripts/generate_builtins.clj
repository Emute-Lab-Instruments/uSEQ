;; RULES
;; TODO 1. evaluate args, unless told not to
;; TODO 2. Create a return value that defaults to Value::nil();
;; TODO 3. Check the number of args - early exit of not
;; TODO 4. Check the types.
;;   TODO 4.1. First, if there is an :all, check those. Early exit with error if any doesn't satisfy.
;;   TODO 4.2. Then check each individual arg, in ascending order. Early exit with error if any fail.
;; TODO 5. Insert body
;; TODO 6. Return `result` value

(def specs-file "builtins.edn")
(def target-file-h "../uSEQ/src/lisp/generated_builtins.h")
(def target-file-cpp "../uSEQ/src/lisp/generated_builtins.cpp")

(defn read-builtin-specs [path]
  (->>
   (with-open [r (clojure.java.io/reader path)]
     (clojure.edn/read (java.io.PushbackReader. r)))
   ;; Add the name as a key
   (reduce (fn [acc [k v]]
             (assoc acc
                    k
                    (-> v
                        (assoc :name k))))
           {})))

(def builtins (read-builtin-specs specs-file))

;; UTILS
(def orders {0 "first"
             1 "second"
             2 "third"})

(def preds->words
  {".is_number()" "number"
   ".is_list()" "list"
   ".is_empty()" "empty list"
   "!.is_empty()" "non-empty list"
   ".is_error()" "error"})

(defn ensure-vector [arg]
  (if (sequential? arg)
    (into [] arg)
    [arg]))

(defn tab
  ([& args]
   (if (not (= args [nil]))
     (->> args
          flatten
          (filter identity)
          (map (fn [s]
                 (case s
                   "\n" s
                   (str "    " s))))
          (into []))
     nil)))

(defn is-negated-pred? [str]
  (= \! (nth str 0)))

(defn builtin-body [spec]
  ["// BODY"
   "Value result = Value::nil();"
   (:body spec)
   "return result;"])
;; /UTILS

;; COMPONENTS
(defn args-for-loop [& body]
  ["for (size_t i = 0; i < args.size(); i++)"
   "{"
   (tab body)
   "}"])

(defn eval-args-body [spec]
  ["Value pre_eval = args[i];"
   "args[i] = args[i].eval(env);"
   "if (args[i].is_error())"
   "{"
   (tab
    (format
     "error(\"(%s) Argument #\" + String(i + 1) + \" evaluates to an error:\\n\" + pre_eval.display());"
     (:name spec))
    "return Value::error();")
   "}"])

(defn eval-args-expr [spec]
  ["// Evaluating & checking args"
   (apply args-for-loop (eval-args-body spec))])

(defn type-signature [spec]
  (str "Value " (:name spec) "(std::vector<Value>& args, Environment& env)"))

(defn if-else-num-args [spec]
  (let [comparison (-> spec :args :num)]
    (if-not comparison
      nil
      ["// Checking number of args"
       (format "if (!(args.size() %s))" comparison)
       "{"
       (tab
        (format "error(\"(%s) Expected %s args, received \" + String(args.size()) + \" instead.\");"
                (:name spec)
                comparison)
        "return Value::error();")
       "}"])))

(defn if-else-individual-args [spec]
  (let [name (:name spec)
        type-constraints (-> spec :args :type (dissoc :all))
        gen-check (fn [arg-num pred]
                    (let [negated? (is-negated-pred? pred)
                          _ (println  (str "pred: " pred))]
                      [(format "if (!(%sargs[%d]%s))"
                               (if negated? "!" "")
                               arg-num
                               (if negated? (subs pred 1) pred))
                       "{"
                       (tab (format "error(\"(%s) Argument #%d should evaluate to a %s, instead it evaluates to a \" + args[%d].get_type_name() + \":\\n\" + args[%d].display());"
                                    name
                                    arg-num
                                    (preds->words pred)
                                    arg-num
                                    arg-num)
                            "return Value::error();")
                       "}"]))]
    (if (empty? type-constraints)
      nil
      ["// Checking individual args"
       (into [] (for [[arg-num preds] type-constraints]
                  (if (sequential? preds)
                    (mapv (partial gen-check arg-num) preds)
                    (gen-check arg-num preds))))])))

(comment
  (def spec (builtins "sum"))

  (and (get spec :eval-args? true)
       (get-in spec [:args :type :all] nil))
  ;;
  )

(defn get-preds-for-all-args [spec]
  (let [preds (filter identity
                      (ensure-vector
                       (get-in spec [:args :type :all] nil)))]
    (if (not-empty preds)
      preds
      nil)))

(comment
  (get-preds-for-all-args (builtins "sum")))

(defn iterate-over-args-and-eval-and-or-check-all-pred [spec]
  (let [eval-args? (get spec :eval-args? true)
        all-preds (get-preds-for-all-args spec)
        combine-iterations? (and eval-args? all-preds)]
    (if (and (not eval-args?)
             (not all-preds))
      nil
      [;; Comment
       (cond
         combine-iterations? "// Evaluating args, checking for errors & all-arg constraints"
         all-preds "// Checking all-arg constraints"
         eval-args? "// Evaluating & checking args for errors"
         :else "")
       (args-for-loop
        (if eval-args?
          ["// Eval"
           (eval-args-body spec)
           ""]
          nil)
        (if all-preds
          ["// Check all-pred(s)"
           (into []
                 (for [pred all-preds]
                   (let [negate? (is-negated-pred? pred)]
                     [(format "if (!(%sargs[i]%s))"
                              (if negate? "!" "")
                              (if negate? (subs pred 1)
                                  pred))
                      "{"
                      (tab (format "error(\"(%s) All arguments should evaluate to a %s, but argument #\" + String(i + 1) + \" is a \" + args[i].get_type_name() + \" instead:\\n\" + args[i].display() + \"\\n\");"
                                   (:name spec)
                                   (preds->words pred))
                           "return Value::error();")
                      "}"])))]
          nil))])))

;; /COMPONENTS
(defn gen-builtin-def [spec]
  ;; eval-args defaults to true
  (->> [(type-signature spec)
        "{"
        (tab
         (if-else-num-args spec)
         ""
         (iterate-over-args-and-eval-and-or-check-all-pred spec)
         ""
         (if-else-individual-args spec)
         ""
         (builtin-body spec))
        "}"]
       flatten
       (filter identity)
       ((partial clojure.string/join "\n"))))

(defn gen-builtin-header-decl [spec]
  (str (type-signature spec) ";"))

(defn gen-h-and-cpp [spec-file]
  (let [builtins (vals (read-builtin-specs spec-file))
        h-file-contents  (map gen-builtin-header-decl builtins)
        cpp-file-contents (map gen-builtin-def builtins)]
    ;; .h
    (->> [;; "#include \" \""
          "#ifndef GENERATED_BUILTINS_H_"
          "#define GENERATED_BUILTINS_H_"
          ""
          "#include <vector>"
          ""
          "class Value;"
          "class Environment;"
          ""
          "namespace builtin"
          "{"
          h-file-contents
          "} // namespace builtin"
          ""
          "#endif // GENERATED_BUILTINS_H_"]
         ;;
         flatten
         (clojure.string/join "\n")
         (spit target-file-h))
    ;; .cpp
    (->> ["#include \"generated_builtins.h\""
          "#include \"value.h\""
          "#include \"environment.h\""
          "#include \"interpreter.h\""
          "namespace builtin"
          "{"
          cpp-file-contents
          "} // namespace builtin"]
         flatten
         ;;
         (clojure.string/join "\n\n") (spit target-file-cpp))))

(gen-h-and-cpp specs-file)
