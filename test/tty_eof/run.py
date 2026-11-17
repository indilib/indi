#!/usr/bin/env python3
from pathlib import Path
import sys,subprocess,re
T=Path(__file__).resolve().parent;R=Path(sys.argv[1]).resolve();B=Path(sys.argv[2]).resolve();B.mkdir(parents=True,exist_ok=True)
(B/'config.h').write_text('#define HAVE_CLOCK_GETTIME 1\n')
# Only EOF/TTY code is linked. Version macros are header metadata, not exercised.
s=(R/'CMakeLists.txt').read_text();versions=[re.search(r'set\(CMAKE_INDI_VERSION_'+k+r'\s+(\d+)',s,re.I).group(1) for k in ['MAJOR','MINOR','RELEASE']]
(B/'indiversion.h').write_text(''.join('#define INDI_VERSION_'+k+' '+v+'\n' for k,v in zip(['MAJOR','MINOR','RELEASE'],versions))+'#define INDI_VERSION "'+'.'.join(versions)+'"\n#define DATA_INSTALL_DIR "."\n')
subprocess.run(['cc','-std=gnu11','-O0','-Wall','-Wextra','-ffunction-sections','-fdata-sections','-I'+str(B),'-I'+str(R/'libs/indicore'),'-I'+str(R/'libs'),str(T/'eof-regression.c'),str(R/'libs/indicore/indicom.c'),'-Wl,--gc-sections','-lm','-o',str(B/'eof-regression')],check=True)
r=subprocess.run([str(B/'eof-regression')]);raise SystemExit(r.returncode)
