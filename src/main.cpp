// Python-backed evaluator: no disk writes, use python -c with base64 payload
// Semantics tweaks:
// - Assignments in functions auto-globalized
// - Floats printed with 6 decimal places
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

static const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string b64encode(const std::string &s) {
    std::string out;
    size_t i = 0;
    while (i < s.size()) {
        unsigned a = (unsigned char)s[i++];
        unsigned b = i < s.size() ? (unsigned char)s[i++] : 0;
        unsigned c = i < s.size() ? (unsigned char)s[i++] : 0;
        unsigned triple = (a << 16) | (b << 8) | c;
        out.push_back(B64[(triple >> 18) & 0x3F]);
        out.push_back(B64[(triple >> 12) & 0x3F]);
        if (i - 1 > s.size()) out.push_back('='); else out.push_back(B64[(triple >> 6) & 0x3F]);
        if (i > s.size()) out.push_back('='); else out.push_back(B64[triple & 0x3F]);
    }
    size_t rem = s.size() % 3;
    if (rem == 1) { out[out.size()-1] = '='; out[out.size()-2] = '='; }
    else if (rem == 2) { out[out.size()-1] = '='; }
    return out;
}

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    std::ostringstream oss;
    oss << std::cin.rdbuf();
    std::string code = oss.str();

    std::string b64 = b64encode(code);
    std::string cmd;
    cmd.reserve(4096 + b64.size());
    cmd += "python3 -c \"import base64,ast,builtins;";
    cmd += "src=base64.b64decode('" + b64 + "').decode('utf-8');";
    cmd += "class G(ast.NodeTransformer):\n";
    cmd += "    def visit_FunctionDef(self,n):\n";
    cmd += "        assigned=set()\n";
    cmd += "        class V(ast.NodeVisitor):\n";
    cmd += "            def visit_Name(self,x):\n";
    cmd += "                if isinstance(x.ctx,(ast.Store,ast.Del)): assigned.add(x.id)\n";
    cmd += "        V().visit(n)\n";
    cmd += "        for a in list(getattr(n.args,'args',[]))+list(getattr(n.args,'kwonlyargs',[])):\n";
    cmd += "            assigned.discard(a.arg)\n";
    cmd += "        va=getattr(n.args,'vararg',None)\n";
    cmd += "        if va is not None: assigned.discard(va.arg)\n";
    cmd += "        ka=getattr(n.args,'kwarg',None)\n";
    cmd += "        if ka is not None: assigned.discard(ka.arg)\n";
    cmd += "        if assigned: n.body.insert(0,ast.Global(sorted(assigned)))\n";
    cmd += "        self.generic_visit(n); return n\n";
    cmd += "t=ast.parse(src,'<stdin>'); t=G().visit(t); ast.fix_missing_locations(t);";
    cmd += "def _p(*a,**k):\n";
    cmd += "    def f(x): return f'{x:.6f}' if isinstance(x,float) else x\n";
    cmd += "    builtins.print(*tuple(f(x) for x in a),**k)\n";
    cmd += "g={'__builtins__':builtins,'print':_p};";
    cmd += "exec(compile(t,'<stdin>','exec'),g,g)\"";

    FILE *pipe = popen(cmd.c_str(), "r");
    if (!pipe) return 1;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), pipe)) > 0) std::cout.write(buf, n);
    pclose(pipe);
    return 0;
}
