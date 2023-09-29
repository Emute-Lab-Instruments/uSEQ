import re

class Lispy:
    @staticmethod
    def tokenize_lisp(expression):
        # Regular expression pattern for tokens in LISP syntax
        pattern = r'\'\(|\(|\)|\'[^\']*\'|"[^"]*"|[^\s()]+|\n|.'

        tokens = []
        row, col = 1, 1  # Initialize row and column positions
        for match in re.finditer(pattern, expression):
            token = match.group()
            # print(list(token))
            if token:
                if token == '\n':
                    row += 1
                    col = 1
                elif token == ' ':
                    col += 1
                else:
                    tokens.append([str(token), row, col])
                    col += len(token)

        return tokens


    @staticmethod
    def get_ast(tokens, level=0, prev=None):
        def makeNode(tok, lev, prev):
            return {'sym': tok[0], 'row': tok[1], 'col': tok[2], 'level': lev, 'prev': prev, 'next': None}
        openSyms = ["(", "'("]
        sym = tokens.pop(0)
        # print(sym)
        if level == 0 and sym[0] not in openSyms:
            raise ValueError("Opening parenthesis missing")
        firstNode = makeNode(sym, level, prev)
        leaf = [firstNode]
        prev = firstNode
        while sym[0] != ")":
            # print(tokens)
            if len(tokens) == 0:
                raise ValueError("Closing parenthesis missing")
            if tokens[0][0] in ["(", "'("]:
                node = Lispy.get_ast(tokens, level + 1, prev)
                leaf.append(node)
                prev['next'] = node[0]
                prev = node[-1]
            else:
                # print(len(tokens))
                sym = tokens.pop(0)
                # print(sym)
                newNode = makeNode(sym, level, prev)
                leaf.append(newNode)
                prev['next'] = newNode
                prev = newNode
        return leaf

    @staticmethod
    def traverseAST(ast, function):
        for i, v in enumerate(ast):
            if isinstance(v, list):
                Lispy.traverseAST(v, function)
            else:
                function(v)

    @staticmethod
    def collectAST(ast, function, obj):
        for i, v in enumerate(ast):
            if isinstance(v, list):
                obj = Lispy.collectAST(v, function, obj)
            else:
                obj = function(obj, v)
        return obj


    @staticmethod
    def astToFormattedCode(ast):
        def renderToFormattedCode(code, v):
            if v['sym'] in ['(', '('] and v['level'] > 0:
                code = code + '\n' + ('\t' * v['level'])
            code = code + v['sym']
            if v['sym'] not in ['(', '(']:
                code = code + ' '
            return code

        return Lispy.collectAST(ast, renderToFormattedCode, "")

    # @staticmethod
    # def astToString(ast, prevSym=''):
    #     str = ""
    #     for i,v in enumerate(ast):
    #         # print(v)
    #         if isinstance(v[0], list):
    #             str = str + Lispy.astToString(v)
    #         else:
    #             if v[0] in ['(', '('] and v[3] > 0:
    #                 str = str + '\n' + ('\t' * v[3])
    #             str = str + v[0]
    #             if v[0] not in  ['(', '(']:
    #                 str = str + ' '
    #             # if v[0] == ')':
    #             #     str = str + '\n' + ('\t' * v[3])
    #             prevSym = v[0]
    #     return str

