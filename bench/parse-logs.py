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
import numpy as np

def main():
    args = parse_args()
    for logdir in args.logDirs:
        if not isinstance(logdir, Path):
            logdir = Path(logdir)
        df = read_log(logdir)
        df.to_csv(logdir / "output.csv")
        sum_dict = summarize_log(df)
        with open(logdir / "summary.json", "w") as f:
            json.dump(sum_dict, f, indent=4)

def read_log(logdir):
    """
    Reads the log files from a PIM-swap benchmark run and outputs a dataframe of the data.

    Parameters
    ----------
    logdir : str
        The directory containing the log files.
        
    Returns
    -------
    df : pandas.DataFrame
        A dataframe containing the data from the log files.
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
        app_log, load_start = read_wtperf_log(app_log_path)
        app_log.index = app_log.index.round("1s")

    pidstat_path = logdir / "pidstat.log"
    pidstat_df = read_pidstat(pidstat_path)

    vmstat_path = logdir / "vmstat.log"
    mem_df = read_vmstat(vmstat_path)

    df = mem_df.join(pidstat_df, how="inner")

    zswap_path = logdir / "zswap.csv"
    if zswap_path.exists():
        zswap_df = read_zswapmon(zswap_path)
        df = df.join(zswap_df, how="inner")

    pages_path = logdir / "pages.csv"
    if pages_path.exists():
        pages_df = read_pages(pages_path)
        df = df.join(pages_df, how="inner")

    # resample application df to 1 second
    freq = pd.infer_freq(df.index)
    if freq == None:
        print("Undiscernible frequency, setting to 1s")
        freq = "1s"
    
    app_log = app_log.resample(freq).bfill()
    df = df.join(app_log, how="inner")

    if load_start is None:
        load_start = df.index[0]
    else:
        before_start = df.loc[:load_start].index
        if len(before_start) > 0:
            load_start = before_start[-1]
        else:
            load_start = df.index[0]
    df.index = df.index - load_start
    df.index.name = "time"
    add_composite_cols(df)
    return df

def summarize_log(df):
    """
    Summarizes the data in a log dataframe into a dictionary.
    
    Parameters
    ----------
    df : pandas.DataFrame
        A dataframe containing the data from the log files.

    Returns
    -------
    sum_dict : dict
        A dictionary containing the summarized data.
    """
    test_start = pd.Timedelta("0s")
    test_df = df.loc[test_start:]
    test_desc = test_df.describe(include=[np.number]).to_dict()

    if test_start != df.index[0]:
        setup_df = df.loc[:test_start]
        setup_desc = setup_df.describe().to_dict()
        ret_dict = {}
        for k, v in setup_desc.items():
            ret_dict["setup_" + k] = v
        for k, v in test_desc.items():
            ret_dict[k] = v
    else:
        ret_dict = test_desc
    for k, v in ret_dict.items():
        if isinstance(v, pd.Timestamp):
            ret_dict[k] = v.strftime('%Y-%m-%d %X')
    return ret_dict

def read_wtperf_log(fname):
    f = open(fname, "r")
    f.readline()
    json_obj = [json.loads(line) for line in f]
    monitor_df = pd.json_normalize(json_obj)
    monitor_df["localTime"] = pd.to_datetime(monitor_df["localTime"], utc=True)
    monitor_df.set_index("localTime", inplace=True)
    start = find_wtperf_start(monitor_df)
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
    
    intcols = ["UID", "PID", "CPU", "VSZ", "RSS"]
    floatcols = ["%%usr", "%%system", "%%guest", "%%wait", "%CPU", "%%MEM", "minflt/s", "majflt/s"]
    for col in pidstat_df.columns:
        if col in intcols:
            pidstat_df[col] = pidstat_df[col].astype(int)
        elif col in floatcols:
            pidstat_df[col] = pidstat_df[col].astype(float)
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
            "swpd": "virt mem used",
            "free": "free mem",
            "buff": "buffers memory",
            "cache": "cache memory",
            "si": "memory swapped in /s",
            "so": "memory swapped out /s",
            "bi": "blocks received /s",
            "bo": "blocks sent /s",
            "in": "interrupts received /s",
            "cs": "context switches /s",
            "us": "CPU time spent in user mode",
            "sy": "CPU time spent in kernel",
            "id": "CPU time spent idle",
            "wa": "CPU time spent waiting for IO",
            "st": "CPU time stolen"
        }
        vm_df.rename(columns=renameDict, inplace=True)
    return vm_df

def read_zswapmon(fname):
    df = pd.read_csv(fname, index_col=0, parse_dates=True)
    df.index = df.index.tz_localize("UTC")
    return df

def read_pages(fname):
    df = pd.read_csv(fname, index_col=0, parse_dates=True)
    df.index = df.index.tz_localize("UTC")
    return df

def add_composite_cols(df):
    # calculate compression ratio of zswap
    if "zswap/stored_pages" in df.columns and "zswap/pool_total_size" in df.columns:
        df["zswap_total_compression_ratio"] = df["zswap/stored_pages"]*4096 / df["zswap/pool_total_size"]
        df.loc[~np.isfinite(df["zswap_total_compression_ratio"]), "zswap_total_compression_ratio"] = 0

        if "zswap/same_filled_pages" in df.columns:
            df["zswap_pure_compression_ratio"] = (df["zswap/stored_pages"] - df["zswap/same_filled_pages"])*4096 / df["zswap/pool_total_size"]
            df.loc[~np.isfinite(df["zswap_pure_compression_ratio"]), "zswap_pure_compression_ratio"] = 0
    
    # calculate total physical memory used
    if "Active(anon)" in df.columns:
        if "Active(file)" in df.columns:
            df["total_phy_mem_needed"] = df["Active(anon)"] + df["Active(file)"]
        if "RSS" in df.columns:
            df["total_phy_mem_used"] = df["RSS"] + df["Active(file)"]

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