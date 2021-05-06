#!/usr/bin/python

# graph the usage of blocks over time
# uses the same csv input file as 'utilization'

import pandas as pd
import argparse
import matplotlib.pyplot as plt

parser = argparse.ArgumentParser()
parser.add_argument('--csv', '-f', help="input CSV file", required=True)
parser.add_argument('--totalblocks', '-t', help="total blocks", required=True)
parser.add_argument('--blocksize', '-b', help="block size (in bytes)", required=True)
args = parser.parse_args()

# read raw data from CSV into a dataframe
df = pd.read_csv(args.csv)
#print(df)

# resample to a more reasonable amount of data to graph
max_data_points = 300
resample = int(max_data_points)
df = df.iloc[::resample, :]

df['blocks'] = df['allocated_blocks'].astype(int) * 100 / int(args.totalblocks)
df['blocks'] = df['blocks'].round(2)
print(df)

# find where ramp-up ends and steady state begins
#steady = df.loc[:, 'blocks'].idxmax()
steady = 26000
ramp_down = 155000

# find the lowest point in the graph, not including ramp-up and ramp-down
lowest = df.loc[steady:ramp_down, 'blocks'].idxmin()
lowest_val = df.at[lowest, 'blocks']

ax = df[['blocks']].plot.line(title='Block Allocation over Time', xlabel='# of cache operations', ylabel='% allocated blocks', rot=45)

ax.text(lowest, lowest_val - 4, str(lowest_val))

# get the legend to not block important information
plt.subplots_adjust(right=0.7)
plt.tight_layout()

# save the output file
outname = "allocation-" + args.blocksize + ".png";
plt.savefig(outname)

# pyplot likes to cache things it probably shouldn't
plt.close('all')
