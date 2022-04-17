"""
parse-logs.py

2022-03-28 J. Dagger (JacksonDDagger at gmail)

    This script reads the log files from a PIM-swap benchmark run and
    outputs a dataframe of the data.
"""

import argparse
from pathlib import Path
import pandas as pd
import json
import re

def main():
    args = parse_args()
    for logdir in args.logDirs:
        if not isinstance(logdir, Path):
            logdir = Path(logdir)
        df, start = read_log(logdir, fullRun=args.fullRun)
        df.to_csv(logdir / "output.csv")
        sum_dict = summarize_log(df, start)
        with open(logdir / "summary.json", "w") as f:
            json.dump(sum_dict, f, indent=4)

def read_log(logdir, fullRun=False):
    """
    Reads the log files from a PIM-swap benchmark run and outputs a dataframe of the data.

    Parameters
    ----------
    logdir : str
        The directory containing the log files.
    cropSetup : bool
        If True, the setup phase is cropped from the data.
    
    Returns
    -------
    df : pandas.DataFrame
        A dataframe containing the data from the log files.
    load_start : pandas.Timestamp
    """
    if not isinstance(logdir, Path):
        logdir = Path(logdir)

    # ADD APPLICATION HERE (STEP 6)
    if "WT" in logdir.stem:
        application = "wiredtiger"
    else:
        print("Unknown log type: {}".format(logdir.stem))
        return None
    load_start = None
    # ADD APPLICATION HERE (STEP 6)
    if application == "wiredtiger":
        app_log_path = logdir / "monitor.json"
        app_log, load_start = read_wtperf_log(app_log_path, cropSetup=not fullRun)
        app_log.index = app_log.index.round("1s")

    pidstat_path = logdir / "pidstat.log"
    pidstat_df = read_pidstat(pidstat_path)

    vmstat_path = logdir / "vmstat.log"
    mem_df = read_vmstat(vmstat_path)

    df = mem_df.join(pidstat_df, how="outer")

    # resample application df to 1 second
    freq = pd.infer_freq(df.index)
    if freq == None:
        print("Undiscernible frequency, setting to 1s")
        freq = "1s"
    
    app_log = app_log.resample(freq).bfill()
    if not fullRun:
        df = df.loc[app_log.index[0]:]
    df = df.join(app_log, how="outer")

    if load_start is None:
        load_start = df.index[0]
    else:
        before_start = df.loc[:load_start].index
        if len(before_start) > 0:
            load_start = before_start[-1]
        else:
            load_start = df.index[0]
        
    return df, load_start

def summarize_log(df, test_start=0):
    test_df = df.loc[test_start:]
    test_desc = test_df.describe().to_dict()

    if test_start != df.index[0]:
        setup_df = df.loc[:test_start]
        setup_desc = setup_df.describe().to_dict()
        ret_dict = {}
        for k, v in setup_desc.items():
            ret_dict["setup_" + k] = v
        for k, v in test_desc.items():
            ret_dict["test_" + k] = v
    else:
        ret_dict = test_desc
    ret_dict["test_start"] = test_start
    for k, v in ret_dict.items():
        if isinstance(v, pd.Timestamp):
            ret_dict[k] = v.strftime('%Y-%m-%d %X')
    return ret_dict

def read_wtperf_log(fname, cropSetup=True):
    f = open(fname, "r")
    f.readline()
    json_obj = [json.loads(line) for line in f]
    monitor_df = pd.json_normalize(json_obj)
    monitor_df["localTime"] = pd.to_datetime(monitor_df["localTime"], utc=True)
    monitor_df.set_index("localTime", inplace=True)
    start = find_wtperf_start(monitor_df)
    if cropSetup:
        monitor_df = monitor_df.loc[start:]
    return monitor_df, start

# ADD APPLICATION HERE (STEP 5)

def find_wtperf_start(monitor_df):
    ops_cols = [col for col in monitor_df.columns if "ops per sec" in col]
    non_se_ops_cols = [col for col in ops_cols if "insert" not in col]
    first_appears = [monitor_df[col].ne(0).idxmax() for col in non_se_ops_cols if monitor_df[col].ne(0).nunique()>1]
    start = min(first_appears)
    return start

def read_pidstat(fname):
    f = open(fname, "r")
    lines = f.readlines()
    f.close()
    lines = [line.strip() for line in lines]

    # read line 0 and get date
    l0 = lines[0].split()
    date_pattern = re.compile(r"\d{2}/\d{2}/\d{4}")
    date_strings = list(filter(date_pattern.match, l0))
    if len(date_strings) == 0:
        print("Could not find date in pidstat.log")
        date = "01/01/1970"
    else:
        date = date_strings[0]

    line_pairs = []
    curr_pair = []
    for line in lines[2:]:
        if line == "":
            if len(curr_pair) == 2:
                line_pairs.append(curr_pair)
            curr_pair = []
        else:
            curr_pair.append(line.split())
    if len(curr_pair) == 2:
        line_pairs.append(curr_pair)
    
    data_dict = {}
    for pair in line_pairs:
        if len(pair) != 2:
            print("Warning: weird pidstat.log format {}".format(len(pair)))
            continue
        col_dict = {}
        for col, val in zip(pair[0][2:], pair[1][2:]):
            col_dict[col] = val
        time = pair[1][0] + " " + pair[1][1]
        datetime = date + " " + time
        if datetime in data_dict:
            data_dict[datetime] = {**data_dict[datetime], **col_dict}
        else:
            data_dict[datetime] = col_dict
    
    pidstat_df = pd.DataFrame.from_dict(data_dict, orient="index")
    pidstat_df.index = pd.to_datetime(pidstat_df.index, utc=True)
    return pidstat_df

def read_vmstat(fname, renameCols=True):
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
    vm_df["datetime"] = pd.to_datetime(vm_df["date"] + " " + vm_df["time"], utc=True)
    vm_df.set_index("datetime", inplace=True)
    vm_df.drop(columns=["date", "time"], inplace=True)
    
    if renameCols:
        renameDict = {
            "swpd": "virt_mem_used",
            "free": "idle_mem",
            "si": "memory swapped in /s",
            "so": "memory swapped out /s",
            "bi": "blocks received /s",
            "bo": "blocks sent /s",
        }
        vm_df.rename(columns=renameDict, inplace=True)
    return vm_df

def parse_args():
    parser = argparse.ArgumentParser(
        description="Decode PIM-swap logs."
        )
    parser.add_argument("logDirs", metavar='D', type=str,
        nargs='+',
        help="Log directories to read from."
        )
    parser.add_argument("--fullRun", "-f", action="store_true",
        help="Don't crop the data to start when the real workload starts.")
    args = parser.parse_args()
    return args

if __name__ == "__main__":
    main()