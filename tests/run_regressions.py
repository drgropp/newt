"""Run the literal cases shared with run_tests.ps1 without requiring PowerShell."""
import argparse
from pathlib import Path
import re
import shlex
import subprocess


def decode_literal(value):
    # Only literal strings are supported: do not evaluate PowerShell expressions.
    escapes = {'0':'\0','a':'\a','b':'\b','e':'\x1b','f':'\f','n':'\n','r':'\r','t':'\t','v':'\v'}
    result=[];index=0
    while index<len(value):
        character=value[index]
        if character=='`':
            index+=1
            if index==len(value): raise ValueError('Incomplete backtick escape in test data')
            character=escapes.get(value[index],value[index])
        elif character=='$':
            raise ValueError('PowerShell variable expansion is not supported in literal test data')
        result.append(character);index+=1
    return ''.join(result)


def cases(path):
    source=path.read_text(encoding='utf-8')
    data=source.split('$Tests = @(\n',1)[1].split('\n)\n',1)[0]
    blocks=list(re.finditer(r'\s*@\{\n(.*?)\n    \}\s*,?',data,re.S))
    position=0
    for match in blocks:
        if data[position:match.start()].strip(): raise ValueError('Unsupported test table syntax')
        position=match.end();block=match[1];case={}
        fields=list(re.finditer(r'^\s*(Name|Arguments|Stdout|Stderr) = "((?:`.|[^"\n])*)"\s*$',block,re.M))
        for field in fields:
            if field[1] in case: raise ValueError('Duplicate test field')
            case[field[1]]=decode_literal(field[2])
        status=re.search(r'^\s*ExitCode = (\d+)\s*$',block,re.M)
        if set(case)!={'Name','Arguments','Stdout','Stderr'} or status is None:
            raise ValueError('Unsupported or incomplete test case')
        remaining=block
        for field in reversed(fields): remaining=remaining[:field.start()]+remaining[field.end():]
        remaining=re.sub(r'^\s*ExitCode = \d+\s*$','',remaining,flags=re.M)
        if remaining.strip(): raise ValueError('Nonliteral test data is unsupported')
        case['ExitCode']=int(status[1]);yield case
    if data[position:].strip(): raise ValueError('Unparsed test data')


def main():
    root=Path(__file__).resolve().parent.parent
    default=root/'newt.exe'
    if not default.exists(): default=root/'newt'
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--newt',type=Path,default=default)
    parser.add_argument('--timeout',type=float,default=10)
    args=parser.parse_args();newt=args.newt.resolve()
    count=failures=0
    for case in cases(root/'tests/run_tests.ps1'):
        count+=1
        try:
            process=subprocess.run([str(newt),*shlex.split(case['Arguments'])],cwd=root,
                                   capture_output=True,timeout=args.timeout)
            actual=(process.returncode,process.stdout.decode('utf-8').replace('\r\n','\n'),
                    process.stderr.decode('utf-8').replace('\r\n','\n'))
            expected=(case['ExitCode'],case['Stdout'],case['Stderr'])
            if actual!=expected:
                failures+=1
                print(f"FAIL {case['Name']}: expected {expected!r}, got {actual!r}")
        except (OSError,subprocess.TimeoutExpired,UnicodeError) as error:
            failures+=1;print(f"FAIL {case['Name']}: {error}")
    print(f'{count-failures}/{count} regression cases passed.')
    raise SystemExit(bool(failures))


if __name__=='__main__': main()
