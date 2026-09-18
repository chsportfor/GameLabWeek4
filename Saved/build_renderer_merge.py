import os, subprocess, sys
from pathlib import Path
root=Path(__file__).resolve().parents[1]
env={k.upper():v for k,v in os.environ.items()}
args=[r"C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe",str(root/'JungleEngine.sln'),'/p:Configuration='+ (sys.argv[1] if len(sys.argv)>1 else 'Debug'),'/p:Platform=x64','/m:1','/v:minimal','/nologo','/p:PreferredToolArchitecture=x64','/p:MultiProcessorCompilation=true','/p:CL_MPCount=4']
with (root/'Saved/renderer-merge-build.log').open('w',encoding='utf-8') as log:
 result=subprocess.run(args,env=env,stdout=log,stderr=subprocess.STDOUT)
print('Build exit:',result.returncode)
lines=(root/'Saved/renderer-merge-build.log').read_text(encoding='utf-8').splitlines()
errors=list(dict.fromkeys(l for l in lines if 'error ' in l))
print('\n'.join(errors[:65]))
sys.exit(result.returncode)
