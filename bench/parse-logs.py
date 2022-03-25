import argparse
from pathlib import Path
import pandas as pd
import json

def main():
    args = parse_args()
    for logdir in args.logDirs:
        df = read_log(Path(logdir))
        print(df.head())

def read_log(logdir, cropSetup=True):
    if not isinstance(logdir, Path):
        logdir = Path(logdir)

    if "WT" in logdir.stem:
        app_log_path = logdir / "monitor.json"
        app_log = read_wtperf_log(app_log_path, cropSetup=cropSetup)
    else:
        print("Unknown log type: {}".format(logdir.stem))

    pidstat_path = logdir / "pidstat.log"
    pidstat_df = read_pidstat(pidstat_path)

    vmstat_path = logdir / "vmstat.log"
    mem_df = read_vmstat(vmstat_path)

    return app_log

def read_wtperf_log(fname, cropSetup=True):
    f = open(fname, "r")
    f.readline()
    json_obj = [json.loads(line) for line in f]
    monitor_df = pd.json_normalize(json_obj)
    monitor_df["localTime"] = pd.to_datetime(monitor_df["localTime"])
    monitor_df.set_index("localTime", inplace=True)
    return monitor_df

def read_pidstat(fname):
    pass

def read_vmstat(fname):
    # the csv is whitespace delimited, but the dates have a space so deal with it manually
    f = open(fname, "r")
    f.readline()
    l1 = f.readline().strip()
    colnames = l1.split()
    # tz = colnames[-1]
    colnames[-1] = "date"
    colnames.append("time")
    f.close()

    vm_df = pd.read_csv(fname, delim_whitespace=True, skiprows=[0, 1], names=colnames)
    vm_df["datetime"] = pd.to_datetime(vm_df["date"] + " " + vm_df["time"])
    vm_df.set_index("datetime", inplace=True)
    vm_df.drop(columns=["date", "time"], inplace=True)
    
    return vm_df

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