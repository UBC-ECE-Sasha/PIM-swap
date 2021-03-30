#!/usr/bin/python

import pandas as pd
import argparse
import matplotlib.pyplot as plt

input_file = "utilization.csv"

parser = argparse.ArgumentParser()
parser.add_argument('--csv', '-f', help="input CSV file", required=True)
args = parser.parse_args()

# read raw data from CSV into a dataframe
df = pd.read_csv(args.csv)
#print(df)

# trim rows (indices) up to maximum 'used_bytes'
best = df.loc[:, 'used_bytes'].idxmax()
print("highest value in row ", best)
df = df.truncate(after=best, axis=0)

# resample to a more reasonable amount of data to graph
max_data_points = 30
resample = int(best / max_data_points)
df = df.iloc[::resample, :]

# create new column for 'allocated_bytes'
block_size = 1024
blocks = df['allocated_blocks'] 
df['allocated_bytes'] = blocks * 1024
#print(df)

# create new column 'percent used'
df['percent_used'] = df['used_bytes'] * 100 / df['allocated_bytes']

print(df)
print("Mean", df['percent_used'].mean())
print("STD", df['percent_used'].std())

#ax = df[['percent_used']].plot.bar(title='Utilization Efficiency', xlabel='number of insertions', rot=45).legend(bbox_to_anchor=(1,1))
ax = df[['percent_used']].plot.bar(title='Percent Utilization', xlabel='number of insertions', rot=45)

# get the legend to not block important information
plt.subplots_adjust(right=0.7)
plt.tight_layout()

# save the output file
plt.savefig("utilization.png")

# pyplot likes to cache things it probably shouldn't
plt.close('all')
