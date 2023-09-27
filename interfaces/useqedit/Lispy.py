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
    def get_ast(tokens, level=0):
        openSyms = ["(", "'("]
        sym = tokens.pop(0)
        # print(sym)
        if level == 0 and sym[0] not in openSyms:
            raise ValueError("Opening parenthesis missing")
        leaf = [sym + [level]]
        while sym[0] != ")":
            # print(tokens)
            if len(tokens) == 0:
                raise ValueError("Closing parenthesis missing")
            if tokens[0][0] in ["(", "'("]:
                node = Lispy.get_ast(tokens, level + 1)
                leaf.append(node)
            else:
                # print(len(tokens))
                sym = tokens.pop(0)
                # print(sym)
                leaf.append(sym + [level])
        return leaf

    @staticmethod
    def astToString(ast, prevSym=''):
        str = ""
        for i,v in enumerate(ast):
            # print(v)
            if isinstance(v[0], list):
                str = str + Lispy.astToString(v)
            else:
                if v[0] in ['(', '('] and v[3] > 0:
                    str = str + '\n' + ('\t' * v[3])
                str = str + v[0]
                if v[0] not in  ['(', '(']:
                    str = str + ' '
                # if v[0] == ')':
                #     str = str + '\n' + ('\t' * v[3])
                prevSym = v[0]
        return str

