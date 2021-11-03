import os
import pandas as pd
import datetime

def convertKB(val):
    if type(val) is str:
        if val[-1] == 'm':
            return float(val[:-1])*1024
        if val[-1] == 'g':
            return float(val[:-1])*1024*1024
        if val[-1] == 't':
            return float(val[:-1])*1024*1024*1024
    return float(val)

def removeN(val):
    if type(val) is str:
        if val[0] == 'N':
            return float(val[1:])
    return float(val)

localLogDir = "local_logs"

localLogList = os.listdir(localLogDir)

datetimeFormat = "%Y-%m-%d--%H-%M-%S"
sysDateFormat = "%Y/%m/%d-%H:%M:%S"
monitorDateFormat = "%b %d %H:%M:%S"
opList = ["insert", "modify", "read", "truncate", "update", "backup", "checkpoint", "scan"]

dictlist = []

for dirname in localLogList:
    fulldir = os.path.join(localLogDir, dirname)
    if os.path.isdir(fulldir):
        sdict = {}
        sdict["mem (MB)"] = int(dirname.split('_')[-2])
        sdict["program"] = '_'.join(dirname.split('_')[:-2])
        sdict["datetime"] = datetime.datetime.strptime(dirname.split('_')[-1], datetimeFormat)
        sdict["runtime"] = 0
        sdict["mean cpu usage"] = 0
        sdict["mean memavailable"] = 0
        sdict["mean memfree"] = 0
        sdict["mean physical mem (KB)"] = 0
        sdict["mean virtual mem (KB)"] = 0

        for op in opList:
            sdict[op + " ops/sec"] = 0
            sdict["mean " + op + " latency (uS)"] = 0

        statName = os.path.join(fulldir, "test.stat")
        statName = os.path.join(fulldir, "test.stat")
        if os.path.isfile(statName):
            with open(statName, "r") as statFile:
                statLines = statFile.readlines()
                for line in statLines:
                    if "Run completed" in line and line.split()[-1] == "seconds":
                        sdict["runtime"] = float(line.split()[-2])
                    for op in opList:
                        if (op + " operations") in line:
                            words = line.split()
                            if words[-1] == "ops/sec":
                                sdict[op + " ops/sec"] = int(words[-2])

        monitorName = os.path.join(fulldir, "monitor")
        if os.path.isfile(monitorName):
            monitorDF = pd.read_csv(monitorName, index_col=False)
            monitorDF["#time"] = pd.to_datetime(monitorDF["#time"], format=monitorDateFormat)
            endTime = monitorDF.iloc[-1]["#time"]
            startTime = endTime - datetime.timedelta(seconds=sdict["runtime"])
            runMonitorDF = monitorDF[monitorDF["#time"] >= startTime].copy()

            for col in runMonitorDF.columns:
                for op in opList:
                    if (op + " average latency(uS)") in col:
                        runMonitorDF[col] = runMonitorDF[col].apply(removeN)
                        sdict["mean " + op + " latency (uS)"] = runMonitorDF[col].mean()

        syslogName = os.path.join(fulldir, "sys.log")
        if os.path.isfile(syslogName):
            syslogDF = pd.read_csv(syslogName, index_col=False)
            syslogDF["date"] =  pd.to_datetime(syslogDF["date"], format=sysDateFormat)
            endTime = syslogDF.iloc[-1]["date"]
            startTime = endTime - datetime.timedelta(seconds=sdict["runtime"])
            runlogDF = syslogDF[syslogDF["date"] >= startTime].copy()

            for col in runlogDF.columns:
                if "phymem(kB)" in col:
                    runlogDF[col] = runlogDF[col].apply(convertKB)
                    sdict["mean physical mem (KB)"] = runlogDF[col].mean()
                if "virtmem(kB)" in col:
                    runlogDF[col] = runlogDF[col].apply(convertKB)
                    sdict["mean virtual mem (KB)"] = runlogDF[col].mean()
                if "CPU(%)" in col:
                    sdict["mean cpu usage"] = runlogDF[col].mean()
                if "free mem(kB)" in col:
                    sdict["mean memfree"] = runlogDF[col].mean()
                if "available mem(kB)" in col:
                    sdict["mean memavailable"] = runlogDF[col].mean()

        dictlist.append(sdict)
        
runDF = pd.DataFrame(dictlist)
runDF.to_csv("test.csv")
