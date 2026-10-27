"""Deterministic differential, growth, layout, and malformed-source checks."""
import argparse
from pathlib import Path
import platform
import random
import subprocess
import tempfile


def run(command, timeout=12):
    return subprocess.run([str(x) for x in command], capture_output=True, text=True, timeout=timeout)


def main():
    root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--newt', type=Path, default=root/'newt')
    parser.add_argument('--cc', default='gcc')
    parser.add_argument('--fuzz-cases', type=int, default=400)
    args = parser.parse_args()
    if platform.system() != 'Linux' or platform.machine() not in ('x86_64', 'AMD64'):
        raise SystemExit('Native stress checks require x86-64 Linux.')
    newt = args.newt.resolve()
    rng = random.Random(60127)
    comparisons = 0
    with tempfile.TemporaryDirectory(prefix='newt-project-') as directory:
        d = Path(directory)
        source, assembly, binary = (d/('case'+extension) for extension in ('.nt','.s','.exe'))

        def differential(text):
            nonlocal comparisons
            source.write_text(text+'\n')
            interpreted = run([newt, source])
            compiled = run([newt, '--emit-asm', source, '-o', assembly])
            assert compiled.returncode == 0, (text, compiled.stderr)
            linked = run([args.cc, '-no-pie', assembly, '-o', binary])
            assert linked.returncode == 0, linked.stderr
            native = run([binary])
            assert (native.returncode, native.stdout) == (interpreted.returncode, interpreted.stdout), (text, native, interpreted)
            if interpreted.stderr:
                message = interpreted.stderr.split('runtime error: ')[-1].strip()
                assert message in native.stderr, (text, message, native.stderr)
            else:
                assert not native.stderr, (text,native.stderr)
            comparisons += 1

        def expression(depth):
            if depth == 0 or rng.random() < .25:
                return rng.choice(['a','b','c',str(rng.randint(-8,8))])
            if rng.random() < .15:
                return '-('+expression(depth-1)+')'
            op = rng.choice(['+','-','*','/'])
            right = str(rng.choice([-3,-2,-1,1,2,3])) if op=='/' else expression(depth-1)
            return '('+expression(depth-1)+op+right+')'

        for _ in range(80):
            e = expression(4)
            values = ','.join(str(rng.randint(-8,8)) for _ in range(3))
            differential(f'int evaluate(int a,int b,int c) {{ return {e}; }} start: void main(void) {{ print "sample"; print evaluate({values}); }}')
        for _ in range(25):
            limit, stop = rng.randint(4,15), rng.randint(2,4)
            differential('start: void main(void) { int n=0; int sum=0; while n<'+str(limit)+
                         ' { n=n+1; if n=='+str(stop)+' { continue; } if n>'+str(limit-2)+
                         ' { break; } if n<5 { sum=sum+n; } else { sum=sum-n; } } print sum; print n; }')
        for _ in range(15):
            values=[rng.randint(-20,20) for _ in range(6)]
            text='int id(int n) { return n; } int mix(int a,int b,int c,int d,int e,int f) { return a*3+b*5+c*7+d*11+e*13+f*17; } start: void main(void) { print mix('
            text+=','.join(f'id({n})' for n in values)+'); }'
            differential(text)
        differential('start: void main(void) {\n'+'print 1;\n'*1500+'}\n')
        differential('start: void main(void) { print last(); }\n'+
                     '\n'.join(f'int f{i}(void) {{ return {i}; }}' for i in range(150))+
                     '\nint last(void) { return 42; }')

        # Compare generated nested layouts and pointer fields against real C.
        records=[]
        c_lines=[]
        nt_declarations=[]
        expected_checksums=[]
        nt_main=['start: void main(void) { int seed=10;']
        for index in range(20):
            fields=[]
            for field_index in range(rng.randint(2,5)):
                kind=rng.choice(['int','pointer','nested'] if index else ['int','pointer'])
                child=rng.randrange(min(index,3)) if kind=='nested' else None
                fields.append((kind,child,f'f{field_index}'))
            records.append(fields)
            definition='struct R'+str(index)+' { '+ ' '.join(
                ('int' if kind=='int' else 'int*' if kind=='pointer' else 'struct R'+str(child))+' '+name+';'
                for kind,child,name in fields)+' };'
            c_lines.append(definition); nt_declarations.append(definition)
            def initialize(record_index):
                values=[];total=0
                for kind,child,name in records[record_index]:
                    if kind=='int':
                        value=rng.randint(-20,20);values.append(str(value));total+=value
                    elif kind=='pointer': values.append('&seed');total+=10
                    else:
                        value,subtotal=initialize(child);values.append(value);total+=subtotal
                return '{ '+', '.join(values)+' }',total
            def paths(record_index, prefix):
                result=[]
                for kind,child,name in records[record_index]:
                    path=prefix+name
                    result.extend(paths(child,path+'.') if kind=='nested' else ['*('+path+')' if kind=='pointer' else path])
                return result
            initializer,checksum=initialize(index);expected_checksums.append(checksum)
            c_lines.append(f'int bytes{index}(void) {{ return sizeof(struct R{index}); }}')
            c_lines.append(f'int check{index}(struct R{index}* p) {{ return '+ '+'.join(paths(index,'p->'))+'; }')
            nt_declarations.extend([f'extern int bytes{index}(void);',f'extern int check{index}(struct R{index}* p);'])
            nt_main.extend([f'struct R{index} v{index}={initializer};',f'print sizeof(struct R{index});',f'print bytes{index}();',f'print check{index}(&v{index});'])
        source.write_text('\n'.join(nt_declarations+nt_main+['}']))
        c_file=d/'layouts.c';c_file.write_text('\n'.join(c_lines))
        a=run([newt,'--emit-asm',source,'-o',assembly]);assert a.returncode==0,a.stderr
        a=run([args.cc,'-no-pie',assembly,c_file,'-o',binary]);assert a.returncode==0,a.stderr
        a=run([binary]);assert a.returncode==0 and not a.stderr,a
        results=[int(n) for n in a.stdout.splitlines()];assert len(results)==60
        for i,total in enumerate(expected_checksums):
            assert results[3*i]==results[3*i+1] and results[3*i+2]==total,(i,results[3*i:3*i+3],total)

        # Bounds must fail cleanly rather than overflow internal tables or frames.
        oversized='start: void main(void) { '+''.join(f'int v{i}=0;' for i in range(4097))+' }'
        huge_layout='struct Big0 { int n; };\n'+ '\n'.join(f'struct Big{i} {{ struct Big{i-1} a; struct Big{i-1} b; }};' for i in range(1,21))+ '\nstart: void main(void) {}'
        for text in [oversized,huge_layout]:
            source.write_text(text);assembly.unlink(missing_ok=True)
            a=run([newt,'--emit-asm',source,'-o',assembly]);assert a.returncode==1 and a.stderr and not assembly.exists(),a

        seeds=[
            'start: void main(void) { int n = 2; print n + 1; }',
            'struct Pair { int a; int b; }; start: void main(void) { struct Pair p = { 1, 2 }; print p.a; }',
            'extern int f(int* p); start: void main(void) { int n=1; print f(&n); }',
            'start: void main(void) { int n=0; asm(eax=1) {{ mov eax, 2 }} -> n; }',
        ]
        mutations=['{','}','{{','}}',';',':','*','&','extern','start','(',')','[',']','"','999999999999999999999999999','const','\n','#','->']
        for _ in range(args.fuzz_cases):
            tokens=rng.choice(seeds).split()
            for _ in range(rng.randint(1,4)):
                index=rng.randrange(len(tokens)+1)
                if index<len(tokens) and rng.random()<.4: tokens.pop(index)
                else: tokens.insert(index,rng.choice(mutations))
            text=' '.join(tokens)+'\n';source.write_text(text)
            for mode in ['--parse','--emit-asm']:
                assembly.unlink(missing_ok=True)
                command=[newt,mode,source]+(['-o',assembly] if mode=='--emit-asm' else [])
                a=run(command)
                assert a.returncode in (0,1) and 'AddressSanitizer' not in a.stderr and 'runtime error: signed integer overflow' not in a.stderr,(mode,text,a)
                if mode=='--emit-asm': assert assembly.exists()==(a.returncode==0),(text,a)
    print(f'{comparisons} generated/growth differential programs, 20 C layout checks, 2 capacity checks, and {args.fuzz_cases*2} malformed-source checks passed.')


if __name__=='__main__':
    main()
