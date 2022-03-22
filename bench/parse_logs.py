import argparse
from pathlib import Path

def main():
    args = parse_args()
    for logdir in args.logDirs:
        read_log(Path(logdir))

def read_log(logdir):
    if "WT" in logdir.stem:
        return read_wtperf_log(logdir)

def read_wtperf_log(logdir):
    pass

def wtperf_read_monitor(fname):
    pass

def read_pidstat(fname):
    pass

def read_memlog(fname):
    pass

def parse_args():
    parser = argparse.ArgumentParser(
        description="Decode PIM-swap logs."
        )
    parser.add_argument("logDirs", metavar='D', type=str,
        nargs='+',
        help="Log directories to read from."
        )
    args = parser.parse_args()
    return args

if __name__ == "__main__":
    main()