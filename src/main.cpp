#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int main(int argc, const char *argv[]) {
        std::ios::sync_with_stdio(false);
        std::cin.tie(nullptr);

        std::ostringstream oss;
        oss << std::cin.rdbuf();
        std::string code = oss.str();

        char tmpl[] = "/tmp/pywrapXXXXXX"; // mkstemp will replace XXXXXX
        int fd = mkstemp(tmpl);
        if (fd == -1) {
                return 1;
        }
        std::string path = std::string(tmpl) + ".py";
        // Rename to add .py extension
        rename(tmpl, path.c_str());

        FILE *f = fopen(path.c_str(), "wb");
        if (!f) {
                unlink(path.c_str());
                return 1;
        }
        const char *prelude =
                "import ast, builtins\n"
                "src = r'''\n";
        fwrite(prelude, 1, strlen(prelude), f);
        if (!code.empty()) fwrite(code.data(), 1, code.size(), f);
        const char *middle =
                "\n'''\n"
                "class _Globalize(ast.NodeTransformer):\n"
                "    def visit_FunctionDef(self, node):\n"
                "        assigned = set()\n"
                "        class _AV(ast.NodeVisitor):\n"
                "            def visit_Name(self, n):\n"
                "                if isinstance(n.ctx, (ast.Store, ast.Del)):\n"
                "                    assigned.add(n.id)\n"
                "        _AV().visit(node)\n"
                "        for a in list(getattr(node.args, 'args', [])) + list(getattr(node.args, 'kwonlyargs', [])):\n"
                "            if a.arg in assigned: assigned.discard(a.arg)\n"
                "        if getattr(node.args, 'vararg', None): assigned.discard(node.args.vararg.arg)\n"
                "        if getattr(node.args, 'kwarg', None): assigned.discard(node.args.kwarg.arg)\n"
                "        if assigned:\n"
                "            node.body.insert(0, ast.Global(sorted(assigned)))\n"
                "        self.generic_visit(node)\n"
                "        return node\n"
                "tree = ast.parse(src, '<stdin>')\n"
                "tree = _Globalize().visit(tree)\n"
                "ast.fix_missing_locations(tree)\n"
                "def _print(*args, **kwargs):\n"
                "    def _fmt(x):\n"
                "        return f'{x:.6f}' if isinstance(x, float) else x\n"
                "    builtins.print(*tuple(_fmt(x) for x in args), **kwargs)\n"
                "g = {'__builtins__': builtins, 'print': _print}\n"
                "exec(compile(tree, '<stdin>', 'exec'), g, g)\n";
        fwrite(middle, 1, strlen(middle), f);
        fclose(f);

        std::string cmd = std::string("python3 ") + path + " 2>&1";
        FILE *pipe = popen(cmd.c_str(), "r");
        if (!pipe) {
                unlink(path.c_str());
                return 1;
        }
        char outbuf[4096];
        size_t n;
        while ((n = fread(outbuf, 1, sizeof(outbuf), pipe)) > 0) {
                std::cout.write(outbuf, n);
        }
        int rc = pclose(pipe);
        unlink(path.c_str());
        (void)rc;
        return 0;
}
