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
import matplotlib.pyplot as plt
import re

def main():
    args = parse_args()
    logFiles = args.logFiles
    
    savePath = Path(args.saveFile)
    changeDir = savePath.name and not ("/" in savePath.name or "\\" in savePath.name)

    for i in range(len(logFiles)):
        if not isinstance(logFiles[i], Path):
            logFiles[i] = Path(logFiles[i])
        if logFiles[i].is_dir():
            logFiles[i] = logFiles[i] / "output.csv"
        df = pd.read_csv(logFiles[i], index_col=0, parse_dates=True)
        if changeDir:
            plotSavePath = Path(logFiles[i].parent) / savePath
        else:
            plotSavePath = savePath
        plotTitle = args.plotTitle
        if plotTitle == "AUTO":
            plotTitle = logFiles[i].parent.name + " performance plot"
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

def plot_outputs(df, savePath="output.png", title="", show=False, appPerf=True, mem=True, swap=False, fault=False, resamplingPeriod=0):
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
    """
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
    faultCols = ["minflt/s", "majflt/s"]

    if isinstance(appPerf, list):
        appPerfCols = appPerf
    if isinstance(mem, list):
        memCols = mem
    if isinstance(swap, list):
        swapCols = swap
    if isinstance(fault, list):
        faultCols = appPerf

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

if __name__ == "__main__":
    main()