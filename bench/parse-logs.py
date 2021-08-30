# 2021-08-26 J. Dagger
#
# This script will process log files from qemu-test.sh and create relevant csv files

# TODO integrate memcached logs
# TODO allow for command line args

import os
from datetime import datetime
import re
import pandas as pd

logDirectory = "logs"
logSuffix = ".log"
logMemID = "QEMU guest memory (mb):"
logTestConfigID = "Test execution file:"

csvDirectory = "csvs"
csvHeader = "MONITOR" # CSV START"
csvFooter = "CSV DONE"

logList = os.listdir(logDirectory)
logList = list(filter(lambda x :x[-4:] == logSuffix, logList))

datetimeFormat = "%Y-%m-%d--%H-%M-%S"

memory = 0
testConfig = "unknown"

rowsList = []

for fileName in logList:
    logFile = open(os.path.join(logDirectory,fileName), "r")
    lines = logFile.readlines()

    # get amount of memory from log file or filename if unavailable in log
    memLines = list(filter(lambda x : logMemID in x, lines))
    if len(memLines) > 0:
        memory = int(memLines[0].split()[-1])
    else:
        fileWords = fileName.split("_")
        memWords = list(filter(lambda x : len(x) > 2 and x[-2:] == "MB", fileWords))
        if len(memWords) > 0:
            memory = int(memWords[0][:-2])

    # get test config file from log file or filename if unavailable in log
    testConfigLines = list(filter(lambda x : logTestConfigID in x, lines))
    if len(testConfigLines) > 0:
        testConfig = testConfigLines[0].split()[-1]
    else:
        testConfig = fileName.split("_")[0]

    # find start of monitor file 'cat'ed into log file
    csvStart = len(lines)
    for i, line in enumerate(lines):
        if csvHeader in line:
            csvStart = i + 1

    # find end of monitor file in log file
    csvEnd = csvStart
    if csvStart != len(lines):
        csvEnd = len(lines[csvStart:])
        for i, line in enumerate(lines[csvStart:]):
            if csvFooter in line:
                csvEnd = i + csvStart
    
    if not os.path.isdir(csvDirectory):
        os.mkdir(csvDirectory)

    # save monitor csv seperately
    csvFileName = fileName[:-4] + ".csv"
    if csvEnd > csvStart and not os.path.isfile(os.path.join(csvDirectory, csvFileName)):
        csvFile = open(os.path.join(csvDirectory, csvFileName), "w")
        csvFile.writelines(lines[csvStart:csvEnd])
        csvFile.close()

    if os.path.isfile(os.path.join(csvDirectory, csvFileName)):
        dfMonitor = pd.read_csv(os.path.join(csvDirectory, csvFileName), index_col=False)
        dfMonitor["insert average latency(uS)"] = pd.to_numeric(dfMonitor["insert average latency(uS)"].str.slice(start=1))

        elapsed = dfMonitor["totalsec"].iloc[-1] # get elapsed time
        mean_insert_ps = dfMonitor["insert ops per second"].mean()
        mean_insert_latency = dfMonitor["insert average latency(uS)"].mean()
    else:
        dfMonitor = pd.DataFrame()
        elapsed = 0
        mean_insert_ps = 0
        mean_insert_latency = 0

    fileNameParts = re.split("[_ .]", fileName)
    startDatetime = datetime.strptime(fileNameParts[-2], datetimeFormat)

    rowDict = {
        "datetime": startDatetime,
        "config": testConfig,
        "memory (MB)": memory,
        "elapsed (s)": elapsed,
        "mean insert ops /s": mean_insert_ps,
        "mean insert latency (mcs)": mean_insert_latency
    }
    rowsList.append(rowDict)

    logFile.close()

dfTests = pd.DataFrame(rowsList)
csvTestsName = "all_tests_" + datetime.now().strftime(datetimeFormat) + ".csv"

dfTests = dfTests.sort_values("datetime")

dfTests.to_csv(path_or_buf = os.path.join(csvDirectory, csvTestsName))
