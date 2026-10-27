"""Execute native pointers, layouts, C interop, and raw assembly on x86-64 Linux."""
import argparse
from pathlib import Path
import platform
import subprocess
import tempfile

BRIDGE = '''
struct Mixed { int value; int *target; };
int bump(int *value, int amount) { *value += amount; return *value; }
int mixed(struct Mixed *p) { *p->target += p->value; return sizeof(*p); }
void *identity(void *p) { return p; }
int negative(int n) { return n - 10; }
const int *readonly_identity(const int *p) { return p; }
int order_index(int *p) { *p = 1; return 0; }
int *order_pointer(int *p) { if (*p == 1) *p = 2; else *p = 99; return p; }
'''
CASES = [
 ('assembly_bool_output', 'start: void main(void) { bool ready=false; {{ mov eax, 2 }} -> ready; print ready; }', 'true\n'),
 ('wide_pointer_condition', 'start: void main(void) { void* p=0; {{ movabs rax, 0x100000000 }} -> p; if p { print 1; } else { print 0; } while p { print 2; p=0; } }', '1\n2\n'),
 ('const_mixed_equality', 'start: void main(void) { int n=2; int* p=&n; const int* q=p; print p==q; }', 'true\n'),
 ('const_return', 'const int* same(const int* p) { return p; } extern const int* readonly_identity(const int* p); start: void main(void) { const int n=5; const int* p=same(&n); const int* q=readonly_identity(p); print *q; print q==p; }', '5\ntrue\n'),
 ('const_member_address', 'struct One { int n; }; start: void main(void) { const struct One p={4}; const int* q=&p.n+0; print *q; }', '4\n'),
 ('pointer_evaluation_order', 'extern int order_index(int* p); extern int* order_pointer(int* p); start: void main(void) { int n=0; int* p=order_index(&n)+order_pointer(&n); print n; print *p; }', '2\n2\n'),
 ('pointer', 'start: void main(void) { int n=5; int* p=&n; *p=9; print n; p[0]=12; print *p; }', '9\n12\n'),
 ('pointer_arithmetic', 'start: void main(void) { int n=5; int* p=&n; int* q=p+1; q=q-1; print q==p; print q[0]; }', 'true\n5\n'),
 ('pointer_function', 'int* same(int* p) { return p; } start: void main(void) { int n=7; int* p=same(&n); print *p; }', '7\n'),
 ('let_pointer', 'start: void main(void) { int n=1; let int* p=&n; *p=2; print n; }', '2\n'),
 ('const_pointer', 'start: void main(void) { int a=1; int b=2; const int* p=&a; p=&b; print *p; }', '2\n'),
 ('pair', 'struct Pair { int x; int y; }; start: void main(void) { struct Pair p={7,9}; p.x=10; struct Pair* q=&p; q->y=11; print p.x+p.y; print sizeof(struct Pair); }', '21\n8\n'),
 ('nested', 'struct Inner { int n; }; struct Outer { struct Inner inner; int tail; }; start: void main(void) { struct Outer p={ {3}, 4}; p.inner.n=5; print p.inner.n+p.tail; print sizeof(struct Outer); }', '9\n8\n'),
 ('self_pointer', 'struct Node { int n; struct Node* next; }; start: void main(void) { struct Node p={7,0}; print p.next==0; print sizeof(struct Node); }', 'true\n16\n'),
 ('external', 'extern int bump(int* p,int n); start: void main(void) { int n=4; print bump(&n,3); print n; }', '7\n7\n'),
 ('external_pointer', 'extern void* identity(void* p); extern int negative(int n); start: void main(void) { int n=4; int* p=identity(&n); print *p; print negative(-3); }', '4\n-13\n'),
 ('layout', 'struct Mixed { int value; int* target; }; extern int mixed(struct Mixed* p); start: void main(void) { int n=10; struct Mixed p={5,&n}; print sizeof(struct Mixed); print mixed(&p); print n; }', '16\n16\n15\n'),
 ('asm_output', 'start: void main(void) { int result=0; {{ mov eax, 42 }} -> result; print result; }', '42\n'),
 ('asm_input', 'start: void main(void) { int n=41; int result=0; asm(edi=n) {{ mov eax, edi\n add eax, 1 }} -> result; print result; }', '42\n'),
 ('asm_memory', 'start: void main(void) { int n=5; int* p=&n; asm(rdi=p, esi=7) {{ add DWORD PTR [rdi], esi }} print n; }', '12\n'),
 ('asm_saved', 'start: int main(void) { {{ mov rbx, 0\n mov r12, 0\n mov r13, 0\n mov r14, 0\n mov r15, 0 }} return 0; }', ''),
 ('asm_delimiters', 'start: void main(void) { int n=0; {{ # }} inside an assembly comment\n jmp 1f\n .ascii "}}"\n1:\n mov eax, 3\n }} -> n; print n; }', '3\n'),
 ('asm_pointer_output', 'start: void main(void) { int n=8; int* p=0; asm(rdi=&n) {{ mov rax, rdi }} -> p; print *p; }', '8\n'),
]
REJECT = [
 ('oversized_pointer_literal', 'start: void main(void) { int* p=999999999999999999999999999; }'),
 ('fractional_pointer_literal', 'start: void main(void) { int* p=1.5; }'),
 ('const_write', 'start: void main(void) { int n=1; const int* p=&n; *p=2; }'),
 ('const_drop', 'start: void main(void) { const int n=1; int* p=&n; }'),
 ('const_struct', 'struct Pair { int n; }; start: void main(void) { const struct Pair p={1}; p.n=2; }'),
 ('unknown_field', 'struct Pair { int n; }; start: void main(void) { struct Pair p={1}; print p.bad; }'),
 ('recursive_layout', 'struct Bad { struct Bad value; }; start: void main(void) {}'),
 ('incomplete_type', 'start: void main(void) { print sizeof(struct Missing); }'),
 ('void_deref', 'start: void main(void) { void* p=0; print *p; }'),
 ('pointer_depth', 'start: void main(void) { int** p=0; }'),
 ('float_pointer', 'start: void main(void) { float* p=0; }'),
 ('asm_readonly_output', 'start: void main(void) { const int n=0; {{ mov eax,1 }} -> n; }'),
 ('asm_stack_binding', 'start: void main(void) { int n=1; asm(rsp=n) {{ nop }} }'),
 ('asm_alias_binding', 'start: void main(void) { asm(rax=1,eax=2) {{ nop }} }'),
 ('asm_truncated_pointer', 'start: void main(void) { int n=1; asm(edi=&n) {{ nop }} }'),
 ('bad_initializer', 'struct Pair { int a; int b; }; start: void main(void) { struct Pair p={1}; }'),
 ('extern_bool', 'extern bool flag(void); start: void main(void) {}'),
 ('struct_return', 'struct Pair { int a; }; struct Pair f(void) { struct Pair p={1}; return p; } start: void main(void) {}'),
]

def run(args):
 return subprocess.run([str(x) for x in args],capture_output=True,text=True,timeout=10)

def main():
 parser=argparse.ArgumentParser(description=__doc__)
 parser.add_argument('--newt',type=Path,default=Path(__file__).resolve().parent.parent/'newt')
 parser.add_argument('--cc',default='gcc');args=parser.parse_args();newt=args.newt.resolve()
 if platform.system()!='Linux' or platform.machine() not in ('x86_64','AMD64'):
  raise SystemExit('Native systems tests require x86-64 Linux.')
 with tempfile.TemporaryDirectory(prefix='newt-systems-') as directory:
  d=Path(directory);bridge=d/'bridge.c';bridge.write_text(BRIDGE)
  for name,source,stdout in CASES:
   nt,asm,exe=[d/(name+ext) for ext in ('.nt','.s','.exe')];nt.write_text(source+'\n')
   compiled=run([newt,'--emit-asm',nt,'-o',asm]);assert compiled.returncode==0,(name,compiled.stderr)
   extra=[]
   if name=='asm_saved':
    # A machine-code caller verifies all five SysV callee-saved registers.
    assembly=asm.read_text().replace('.globl main','.globl newt_entry').replace('.type main,','.type newt_entry,').replace('\nmain:', '\nnewt_entry:')
    asm.write_text(assembly);wrapper=d/'wrapper.s'
    lines=['.intel_syntax noprefix','.text','.globl main','main:','push rbp','mov rbp,rsp']
    regs=['rbx','r12','r13','r14','r15']
    lines += ['push '+reg for reg in regs]+['sub rsp,8']
    lines += [f'mov {reg},{1234+i}' for i,reg in enumerate(regs)]+['call newt_entry']
    for i,reg in enumerate(regs): lines += [f'cmp {reg},{1234+i}','jne 1f']
    lines += ['xor eax,eax','jmp 2f','1:','mov eax,99','2:','add rsp,8']+['pop '+reg for reg in reversed(regs)]+['pop rbp','ret','.section .note.GNU-stack,"",@progbits']
    wrapper.write_text('\n'.join(lines)+'\n');extra=[wrapper]
   linked=run([args.cc,'-no-pie',asm,bridge,*extra,'-o',exe]);assert linked.returncode==0,(name,linked.stderr)
   result=run([exe]);assert (result.returncode,result.stdout,result.stderr)==(0,stdout,''),(name,result)
  for name,source in REJECT:
   nt,asm=d/(name+'.nt'),d/(name+'.s');nt.write_text(source+'\n')
   result=run([newt,'--emit-asm',nt,'-o',asm]);assert result.returncode==1 and result.stderr and not asm.exists(),(name,result)
  nt=d/'unterminated.nt';nt.write_text('start: void main(void) { {{ nop\n')
  result=run([newt,'--emit-asm',nt,'-o',d/'bad.s']);assert result.returncode==1 and 'unterminated assembly block' in result.stderr
  # Preserve an existing assembly output when compilation fails.
  sentinel=d/'existing.s';sentinel.write_text('preserve this output\n')
  bad=d/'bad_const.nt';bad.write_text('start: void main(void) { const int n=1; n=2; }')
  result=run([newt,'--emit-asm',bad,'-o',sentinel]);assert result.returncode==1 and sentinel.read_text()=='preserve this output\n'
  # Source aliases must not be overwritten by assembly output.
  protected=d/'protected.nt';protected.write_text('start: void main(void) {}\n')
  aliases=[d/'.'/'protected.nt', d/'protected-link.nt']
  aliases[1].symlink_to(protected)
  for alias in aliases:
   result=run([newt,'--emit-asm',protected,'-o',alias])
   assert result.returncode==1 and 'overwrite' in result.stderr and protected.read_text()=='start: void main(void) {}\n'
  # Raw assembly must remain one token, with following source locations intact.
  tokens=d/'tokens.nt';tokens.write_text('start:\nvoid main(void) {\n{{\nnop\n}}\nprint 1;\n}\n')
  result=run([newt,'--tokens',tokens]);assert result.returncode==0 and '3:1 ASM_BLOCK {{' in result.stdout and '6:1 PRINT print' in result.stdout
  parsed=run([newt,'--parse',d/'const_pointer.nt']);assert parsed.returncode==0 and 'TYPE const int*' in parsed.stdout
  # The interpreter must reject native pointer operations instead of treating them as arithmetic.
  nt=d/'pointer.nt';result=run([newt,nt]);assert result.returncode==1 and '--emit-asm' in result.stderr
 print(f'{len(CASES)} native systems executions and {len(REJECT)+7} safety/rejection checks passed.')

if __name__=='__main__': main()
