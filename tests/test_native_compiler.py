"""Assemble, link, and execute Newt's explicitly supported native subset."""
import argparse
from pathlib import Path
import platform
import subprocess
import tempfile

CASES = [
    ('hello', 'start: void main(void) { print "hello world"; }'),
    ('status', 'start: int main(void) { return 7; }'),
    ('arithmetic', 'start: int main(void) { int n = -5 / 2; print n; n = n * 4 + 3; print n; return 0; }'),
    ('calls', 'int add(int a, const int b) { a = a + b; return a; } start: void main(void) { print add(20, 22); }'),
    ('recursion', 'int fib(int n) { if n <= 1 { return n; } return fib(n-1) + fib(n-2); } start: void main(void) { print fib(10); }'),
    ('loops', 'start: void main(void) { int n = 0; while n < 8 { n = n + 1; if n == 2 { continue; } if n == 6 { break; } print n; } }'),
    ('shadow', 'start: void main(void) { int n = 4; if true { const int n = 8; print n; } else { print 0; } print n; }'),
    ('logic', 'start: void main(void) { bool a = false && bad(); bool b = true || bad(); print a; print b; } bool bad(void) { print "not reached"; return true; }'),
    ('strings', 'start: void main(void) { print "line\\n"; print "a\\tb"; print "quote: \\""; }'.replace('\\""','\\""')),
    ('negative_cast', 'start: void main(void) { print int(-5); }'),
    ('limits', 'start: void main(void) { int low = -2147483648; int high = 2147483647; print low; print high; }'),
    ('six_args', 'int sum(int a,int b,int c,int d,int e,int f) { return a+b+c+d+e+f; } start: void main(void) { print sum(1,2,3,4,5,6); }'),
    ('overflow', 'start: void main(void) { int n = 2147483647; print n + 1; }'),
    ('divzero', 'start: void main(void) { int n = 3; print n / 0; }'),
    ('divide_overflow', 'start: void main(void) { int n = -2147483648; print n / -1; }'),
    ('missing_return', 'start: int main(void) {}'),
]
REJECT = [
    ('float', 'start: void main(void) { float n = 2.5; print n; }'),
    ('untyped', 'start: void main(void) { let n = 2; print n; }'),
    ('global', 'int n=2; start: void main(void) { print n; }'),
    ('readonly', 'start: void main(void) { const int n=2; n=3; }'),
    ('type_mismatch', 'start: void main(void) { bool n=2; }'),
    ('bool_arithmetic', 'start: void main(void) { bool b=true; print b+b; }'),
    ('void_condition', 'void f(void) {} start: void main(void) { if f() {} }'),
    ('no_entry', 'print 1;'),
    ('dynamic_comparison_division', 'start: void main(void) { print 5 / 2 == 2; }'),
    ('dynamic_comparison_overflow', 'start: void main(void) { print 2147483647 + 1 > 0; }'),
    ('dynamic_arithmetic', 'start: void main(void) { print 5 / 2; }'),
]

def run(args, **kwargs):
    return subprocess.run([str(a) for a in args], capture_output=True, text=True, timeout=10, **kwargs)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    root = Path(__file__).resolve().parent.parent
    parser.add_argument('--newt', type=Path, default=root / 'newt')
    parser.add_argument('--cc', default='gcc')
    args = parser.parse_args()
    if platform.system() != 'Linux' or platform.machine() not in ('x86_64','AMD64'):
        raise SystemExit('Native execution tests require an x86-64 Linux host.')
    newt = args.newt.resolve()
    with tempfile.TemporaryDirectory(prefix='newt-native-') as directory:
        directory = Path(directory)
        for name, source in CASES:
            path, assembly, binary = (directory / (name + ext) for ext in ('.nt','.s','.exe'))
            path.write_text(source + '\n')
            interpreted = run([newt, path])
            compiled = run([newt, '--emit-asm', path, '-o', assembly])
            assert compiled.returncode == 0, (name, compiled.stderr)
            linked = run([args.cc, '-no-pie', assembly, '-o', binary])
            assert linked.returncode == 0, (name, linked.stderr)
            native = run([binary])
            assert (native.returncode,native.stdout) == (interpreted.returncode,interpreted.stdout), (name, native, interpreted)
            if interpreted.stderr:
                assert native.stderr and interpreted.stderr.split('runtime error: ')[-1].strip() in native.stderr, (name,native.stderr)
            else:
                assert not native.stderr, (name,native.stderr)
        for name, source in REJECT:
            path, assembly = directory / (name+'.nt'), directory / (name+'.s')
            path.write_text(source+'\n')
            result = run([newt,'--emit-asm',path,'-o',assembly])
            assert result.returncode == 1 and 'compile error:' in result.stderr and not assembly.exists(), (name,result)
        protected=directory/'source.nt';protected.write_text('start: void main(void) {}\n')
        alias=directory/'source-alias.nt';alias.symlink_to(protected)
        for output in [str(directory)+'/./source.nt',str(alias)]:
            result=run([newt,'--emit-asm',protected,'-o',output])
            assert result.returncode==1 and 'overwrite' in result.stderr and protected.read_text()=='start: void main(void) {}\n'
        path = directory/'hello.nt'
        result = run([newt,'--emit-asm',path,'-o',directory/'arm.s','--target','aarch64-linux'])
        assert result.returncode == 1 and 'unsupported target' in result.stderr
    print(f'{len(CASES)} native/interpreter comparisons and {len(REJECT)+3} compiler rejection checks passed.')

if __name__ == '__main__':
    main()
