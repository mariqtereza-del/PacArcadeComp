# X46: runs a command and writes its CPU seconds (user, system) to the file named first. Usage: cpu_run.py OUTFILE command...
import sys, subprocess, resource
r = subprocess.call(sys.argv[2:], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL); u = resource.getrusage(resource.RUSAGE_CHILDREN)
open(sys.argv[1], "w").write("%.2f %.2f\n" % (u.ru_utime, u.ru_stime)); sys.exit(r)
