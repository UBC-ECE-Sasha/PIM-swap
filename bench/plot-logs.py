"""
plot-logs.py

2022-03-31 J. Dagger (JacksonDDagger at gmail)

    Plot log file summaries from parse-logs.py.
    Includes a command line tool.
    Can also be used by other code by calling `from plot-logs import plot_outputs`.
"""

import argparse
from pathlib import Path
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import re

def main():
    args = parse_args()
    logFiles = args.logFiles
    
    savePath = Path(args.saveFile)
    changeDir = savePath.name and not ("/" in savePath.name or "\\" in savePath.name)

    sumFiles = []
    for i in range(len(logFiles)):
        if not isinstance(logFiles[i], Path):
            logFiles[i] = Path(logFiles[i])
        if logFiles[i].is_dir():
            sumFiles.append(logFiles[i] / "summary.json")
            logFiles[i] = logFiles[i] / "output.csv"
        elif args.compare:
            sumFiles.append(logFiles[i])
    if args.compare:
        dfs = [pd.read_json(sumFile) for sumFile in sumFiles]
        names = [plain_text_log_name(sumFile.parent.name) for sumFile in sumFiles]
        if args.comparisonCols == "AUTO":
            cols = ["wtperf.read.ops per sec"]
        else:
            cols = args.comparisonCols.split(",")
        row=args.comparisonRow
        plotTitle = args.plotTitle
        if plotTitle == "AUTO":
            if len(cols) >= 1:
                plotTitle = "Comparison of " + row + " of " + ", ".join(cols)
            else:
                plotTitle = "Comparison of " + row + " of " + cols[0]
        comparison_plot(dfs,
                names,
                cols,
                row=row,
                savePath=savePath
        )
    else:
        for logFile in logFiles:
            df = pd.read_csv(logFile, index_col=0, parse_dates=True)
            if changeDir:
                plotSavePath = Path(logFile.parent) / savePath
            else:
                plotSavePath = savePath
            plotTitle = args.plotTitle
            if plotTitle == "AUTO":
                plotTitle = logFile.parent.name + " performance plot"
            plot_outputs(df,
                    savePath=plotSavePath,
                    title=plotTitle,
                    show=args.show,
                    mem=args.memory,
                    swap=args.swap,
                    appPerf=args.appPerf,
                    fault=args.fault,
                    resamplingPeriod=args.resamplingPeriod
            )

def plot_outputs(df, savePath="output.png", title="", show=False, appPerf=True, mem=True, swap=False, fault=False, resamplingPeriod=0, includeSetup=False):
    """
    Plot the data in the passed in dataframe from parse-logs.py.
    
    Parameters
    ----------
    df : pandas.DataFrame
        Dataframe containing the data to plot. (may be modified)
    savePath : Path
        Path to save the plot to.
    title : str
        Title of the plot. Empty string for no title
    show : bool
        Whether or not to show the plot.
    appPerf : bool
        Plot app performance data.
    mem : bool
        Plot memory system statistics.
    swap : bool
        Plot swap-in and swap-out/s.
    fault : bool
        Plot major and minor page-faults/s.
    resamplingPeriod : int
        Resample data to this period. (default is 0 for no resampling)
    includeSetup : bool
        Include the setup time in the plot.
    """
    if not isinstance(savePath, Path):
        savePath = Path(savePath)

    df.index = df.index - df.index[0]
    if resamplingPeriod > 0:
        df = df.resample('{}S'.format(resamplingPeriod)).mean()
    
    # minflt is very large so it's converted to thousand per/s
    if "minflt/s" in df.columns:
        df["minflt k/s"] = df["minflt/s"]/1000
    
    # various columns that may be in the dataframe
    appPerfCols = ["wtperf.read.ops per sec"]
    # ADD APPLICATION HERE (STEP 7)
    memCols = ["virt_mem_used", "VSZ", "RSS"]
    swapCols = ["memory swapped in /s", "memory swapped out /s"]
    faultCols = ["minflt k/s", "majflt/s"]

    if isinstance(appPerf, list):
        appPerfCols = appPerf
    if isinstance(mem, list):
        memCols = mem
    if isinstance(swap, list):
        swapCols = swap
    if isinstance(fault, list):
        faultCols = fault

    # only use columns actually in df
    appPerfCols = [col for col in appPerfCols if col in df.columns]
    memCols = [col for col in memCols if col in df.columns]
    swapCols = [col for col in swapCols if col in df.columns]
    faultCols = [col for col in faultCols if col in df.columns]
    
    axCols = []
    if appPerf:
        axCols.append((appPerfCols, "App Performance (ops/s)"))
    if mem:
        axCols.append((memCols, "Memory (kB)"))
    if swap:
        axCols.append((swapCols, "Swap (ops/sec)"))
    if fault:
        axCols.append((faultCols, "Faults (ops/sec)"))
    
    fig, ax = plt.subplots()
    if title:
        ax.set_title(title)
    # ax.set_yscale('log')

    if len(axCols) > 0:
        cols, ylabel = axCols[0]
        df[cols].plot(ax=ax, style='-', xlabel="Time elapsed", ylabel=ylabel)
    if len(axCols) > 1:
        cols, ylabel = axCols[1]
        ax2 = df[cols].plot(ax=ax, style='--', secondary_y=True)
        ax2.set_ylabel(ylabel)
    if len(axCols) > 2:
        cols, ylabel = axCols[2]
        ax3 = ax.twinx()
        rspine = ax3.spines['right']
        rspine.set_position(('axes', 1.15))
        ax3.set_frame_on(True)
        ax3.patch.set_visible(False)
        fig.subplots_adjust(right=0.7)
        df[cols].plot(ax=ax3, style='-.', ylabel=ylabel)

    if savePath.name:
        plt.savefig(savePath)
    if show:
        plt.show()

def comparison_plot(dfs, names, cols, row="mean", savePath="output.png", title="", show=False):
    """
    Plot the data in the passed in dataframes from parse-logs.py.
    
    Parameters
    ----------
    dfs : list of (pandas.DataFrame, str)
        Dataframes containing the data to plot. (may be modified)
    cols : list of str
        Columns to plot.
    savePath : Path
        Path to save the plot to.
    title : str
        Title of the plot. Empty string for no title
    show : bool
        Whether or not to show the plot.
    """
    if not isinstance(savePath, Path):
        savePath = Path(savePath)
    n = len(dfs)
    if len(names) != n:
        raise ValueError("Number of names must match number of dataframes")
    
    x = np.arange(len(cols)) 
    plt_vals = []
    for df in dfs:
        plt_vals.append([df[col].loc[row] for col in cols])
    
    width = 0.35  # the width of the bars
    fig, ax = plt.subplots()
    rects = []
    for i in range(n):
        vals = plt_vals[i]
        name = names[i]
        if n == 1:
            loc = width
        else:
            loc = x - width/2 + width/(n-1)*i
        rects.append(ax.bar(loc, vals, width, label=name))

    ax.set_ymargin(0.25)
    ax.set_ylabel(row)
    ax.set_title(title)
    ax.set_xticks(x, cols)
    ax.legend()

    for rect in rects:
        ax.bar_label(rect, padding=3)

    #fig.tight_layout()

    if savePath.name:
        plt.savefig(savePath)
    if show:
        plt.show()

def parse_args():
    parser = argparse.ArgumentParser(
        description="Plot PIM-swap test logs."
        )
    parser.add_argument("logFiles", metavar='D', type=str,
        nargs='+',
        help="CSVs to read from (or directory in which they can be found)."
        )
    parser.add_argument("--appPerf", "-a", action="store_true",
        help="Show app performance data.")
    parser.add_argument("--memory", "-m", action="store_true",
        help="Show memory system statistics.")
    parser.add_argument("--swap", "-s", action="store_true",
        help="Show swap-in and swap-out/s.")
    parser.add_argument("--fault", "-f", action="store_true",
        help="Show major and minor page-faults/s.")
    parser.add_argument("--compare", "-c", action="store_true",
        help="Compare passed in outputs.")
    parser.add_argument("--comparisonCols", "-C", type=str,
        default="AUTO", nargs='?', const='',
        help="Columns to compare. If not specified, app performance will be used.")
    parser.add_argument("--comparisonRow", "-R", type=str,
        default="mean",
        help="Row to compare. If not specified, mean will be used.")
    parser.add_argument("--show", "-w", action="store_true",
        help="Show generated plots.")
    parser.add_argument("--plotTitle", "-t", type=str,
        default="AUTO", nargs='?', const='',
        help="Title of the plot. Leave blank for no title and use 'AUTO' to use the log file name.")
    parser.add_argument("--saveFile", "-S", type=str,
        default="plot.png", nargs='?', const='',
        help="File to save plot to. Will be put in same directory as csv unless directory is specified. Leave empty to not save.")
    parser.add_argument("--resamplingPeriod", "-r", type=int,
        default=0, help="Resample data to this period. (default is 0 for no resampling)")
    args = parser.parse_args()
    return args

def plain_text_log_name(logFile):
    """
    Return the name of the log file in human readable english
    """
    if isinstance(logFile, Path):
        logFile = logFile.name
    
    nameParts = []
    logFileParts = logFile.split("_")

    if logFileParts[0] == "WT":
        nameParts.append("wiredtiger")
    
    zswap = False
    cacheApp = False # the next part will be app cache size
    for part in logFileParts[1:]:
        if cacheApp:
            nameParts.append("(" + part + " cache)")
            cacheApp = False
            continue
        if "ycsb" in part:
            nameParts.append(part.upper())
            cacheApp = True
            continue
        if re.fullmatch("([0-9]+)M", part):
            nameParts.append("with " + part[:-1] + " MB")
            continue
        if "ZSWAP" in part:
            zswap = True
            continue

    if zswap:
        nameParts.append("with zswap")
    return " ".join(nameParts)

if __name__ == "__main__":
    main()