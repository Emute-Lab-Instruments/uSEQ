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

(def preds->words
  {;; NUMBERS
   ".is_number()" "a number"
   ".is_negative_number()" "a negative number"
   "!.is_negative_number()" "a non-negative number"
   ".is_positive_number()" "a positive number"
   "!.is_positive_number()" "a non-positive number"
   ".is_non_zero_number()" "a non-zero number"
   ;; LISTS
   ".is_list()" "a list"
   ".is_empty()" "an empty list"
   "!.is_empty()" "a non-empty list"
   ;; VECTORS
   ".is_vector()" "a vector"
   ;; SEQUENTIAL
   ".is_sequential()" "a sequential structure (e.g. a list or a vector)"
   ;; SYMBOLS
   ".is_symbol()" "a symbol"
   ;; STRINGS
   ".is_string()" "a string"
   "!.is_string()" "not a string"
   ".is_non_empty_string()" "a non-empty string"
   ;; MISC
   ".is_error()" "an error"})

(def internal->user-names
  {"let_block" "let"
   "b_to_u" "b->u"
   "u_to_b" "u->b"
   "get_expr" "get-expr"
   "do_block" "do"
   "if_then_else" "if"
   "for_loop" "for"
   "while_loop" "while"
   "head" "first"
   "pop" "last"
   "map_list" "map"
   "filter_list" "filter"
   "reduce_list" "reduce"
   "eq" "="
   "neq" "!="
   "greater" ">"
   "less" "<"
   "greater_eq" ">="
   "less_eq" "<="

   "sum" "+"
   "subtract" "-"
   "product" "*"
   "divide" "/"
   "remainder" "%"
   "ard_floor" "floor"
   "ard_ceil" "ceil"
   ;;
   ;; "useq_sqr" "square"
   "ard_sin" "sin"
   "ard_cos" "cos"
   "ard_tan" "tan"
   "ard_abs" "abs"
   "ard_usin" "usin"
   "ard_ucos" "ucos"
   "ard_min" "min"
   "ard_max" "max"
   "ard_pow" "pow"
   "ard_sqrt" "sqrt"
   "ard_map" "scale"
   "get_type_name" "type"
   "useq_perf" "perf"
   "gen_random" "random"
   "cast_to_int" "int"
   "cast_to_float" "float"
   "ard_digitalWrite" "dw"
   "ard_digitalRead" "dr"})

(def orders {0 "first"
             1 "second"
             2 "third"})

(def useq-form-setters
  (->
   (for [[type how-many] {"a" 6
                          "d" 6
                          "s" 8}]
     (for [i (map (partial + 1) (range how-many))]
       (let [ast-type (case type
                        "a" "continuous"
                        "d" "binary"
                        "s" "serial")
             method-name (str type i)
             internal-name (str "useq_" method-name)
             user-name method-name]
         [method-name {:useq-builtin? true
                       :internal-name internal-name
                       :user-name method-name
                       :args {:num "== 1"}
                       :eval-args? false
                       :body [(format "set(\"%s\", args[0]);"
                                      (str type i "-expr"))
                              (format
                               "m_%s_ASTs[0] = { args[0] };"
                               ast-type)]
                       :docstring (format "Sets the expression for output `%s`." method-name)}])))

   (conj ["useq_q0" {:useq-builtin? true
                     :internal-name "useq_q0"
                     :user-name "q0"
                     :args {:num "== 1"}
                     :eval-args? false
                     :body ["set(\"q-expr\", args[0]);"
                            "m_q0AST = { args[0] };"]
                     :docstring "Sets the expression for the quantising phasor."}

          "useq_in1" {:useq-builtin? true
                      :internal-name "useq_q0"
                      :user-name "q0"
                      :args {:num "== 1"}
                      :eval-args? false
                      :body ["set(\"q-expr\", args[0]);"
                             "m_q0AST = { args[0] };"]
                      :docstring "Sets the expression for the quantising phasor."}])

   flatten
   (->> (apply hash-map))))

(def useq-builtins
  (merge useq-form-setters))

(defn read-builtin-specs [path]
  (->>
   (with-open [r (clojure.java.io/reader path)]
     (clojure.edn/read (java.io.PushbackReader. r)))
   ;; Add the name as a key
   (reduce (fn [acc [k v]]
             (assoc acc
                    k
                    (-> v
                        (assoc :internal-name k)
                        (assoc :user-name (get internal->user-names k k)))))
           {})
   #_(merge useq-builtins)))

(def builtins (read-builtin-specs specs-file))

;; UTILS

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
    "error_arg_is_error(user_facing_name, i + 1, pre_eval.display());"
    "return Value::error();")
   "}"])

(defn eval-args-expr [spec]
  ["// Evaluating & checking args"
   (apply args-for-loop (eval-args-body spec))])

(defn type-signature [spec]
  (str "Value " (:internal-name spec) "(std::vector<Value>& args, Environment& env)"))

(defn parse-comparison [s]
  (let [[_ op num] (re-matches #"([<>]=?|==)\s*(-?\d+)" s)]
    {:op op
     :num (Integer/parseInt num)}))

(def comparison-ops->cpp-enum
  {"==" "NumArgsComparison::EqualTo"
   ">=" "NumArgsComparison::AtLeast"
   "<=" "NumArgsComparison::AtMost"
   ;; TODO between
   ;; "==" "NumArgsComparison::EqualTo"
   })
(defn num-args-comparison-op-str [spec]
  (-> spec
      (get-in [:args :num])
      parse-comparison
      :op))

(defn num-args-comparison-op-enum [spec]
  (-> spec
      (get-in [:args :num])
      parse-comparison
      :op
      comparison-ops->cpp-enum))

(defn num-args-ranged? [spec]
  ;; FIXME
  false)

(defn num-args-range-low [spec]
  ;; FIXME
  (-> spec
      (get-in [:args :num])
      parse-comparison
      :num))

(defn num-args-range-high [spec]
  ;; FIXME
  nil)

(defn if-else-num-args [spec]
  (let [comparison (-> spec :args :num)]
    (if-not comparison
      nil
      ["// Checking number of args"
       (format "if (!(args.size() %s %d))"
               (num-args-comparison-op-str spec)
               (num-args-range-low spec))
       "{"
       (tab
        (format "error_wrong_num_args(user_facing_name, args.size(), %s, %d%s);"
                (num-args-comparison-op-enum spec)
                (num-args-range-low spec)
                ;; FIXME varargs
                ", -1"
                #_(if (num-args-ranged? spec)
                    (str ", " (num-args-range-high spec))
                    ""))
        "return Value::error();")
       "}"
       ""])))

(defn if-else-individual-args [spec]
  (let [type-constraints (-> spec :args :type (dissoc :all))
        gen-check (fn [arg-num pred]
                    (let [negated? (is-negated-pred? pred)
                          ;; _ (println  (str "pred: " pred))
                          ]
                      [(format "if (!(%sargs[%d]%s))"
                               (if negated? "!" "")
                               arg-num
                               (if negated? (subs pred 1) pred))
                       "{"
                       (tab (format "error_wrong_specific_pred(user_facing_name, %d, \"%s\", %s);"
                                    (inc arg-num)
                                    (preds->words pred)
                                    (format "args[%d].display()" arg-num))
                            "return Value::error();")
                       "}"]))]
    (if (empty? type-constraints)
      nil
      ["// Checking individual args"
       (into [] (for [[arg-num preds] type-constraints]
                  (if (sequential? preds)
                    (mapv (partial gen-check arg-num) preds)
                    (gen-check arg-num preds))))
       ""])))

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
           (eval-args-body spec)]
          nil)
        (if all-preds
          ["// Check all-pred(s)"
           (into []
                 (for [pred all-preds]
                   (let [negate? (is-negated-pred? pred)]
                     [""
                      (format "if (!(%sargs[i]%s))"
                              (if negate? "!" "")
                              (if negate? (subs pred 1)
                                  pred))
                      "{"
                      (tab (format "error_wrong_all_pred(user_facing_name, %s, \"%s\", %s);"
                                   "i + 1"
                                   (preds->words pred)
                                   "args[i].display()")
                           "return Value::error();")
                      "}"])))]
          nil))
       ""])))

(defn static-user-name-str [spec]
  (format "constexpr const char* user_facing_name = \"%s\";\n"
          (:user-name spec)))

;; /COMPONENTS
(defn gen-builtin-def [spec]
  ;; eval-args defaults to true
  (->> [(type-signature spec)
        "{"
        (tab
         (static-user-name-str spec)
         ;; ""
         (if-else-num-args spec)
         ;; ""
         (iterate-over-args-and-eval-and-or-check-all-pred spec)
         ;; ""
         (if-else-individual-args spec)
         ;; ""
         (builtin-body spec))
        "}"]
       flatten
       (filter identity)
       ((partial clojure.string/join "\n"))))

(defn gen-builtin-header-decl [spec]
  (str (type-signature spec) ";"))

(defn gen-h-and-cpp [spec-file]
  (let [builtins (vals (read-builtin-specs spec-file))
        ;; builtins-no-methods (filter #(not (get % :useq-builtin? nil))
        ;;                             builtins)
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
    (->> [(clojure.string/join "\n"
                               ["#include \"generated_builtins.h\""
                                "#include \"value.h\""
                                "#include \"environment.h\""
                                "#include \"interpreter.h\""])
          (clojure.string/join "\n"
                               ["namespace builtin"
                                "{"])
          cpp-file-contents
          "} // namespace builtin"]
         flatten
         ;;
         (clojure.string/join "\n\n") (spit target-file-cpp))))

(gen-h-and-cpp specs-file)
