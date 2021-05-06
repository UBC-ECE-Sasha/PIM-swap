#!/usr/bin/python

# graph the utilization of multiple block sizes

import pandas as pd
import argparse
import matplotlib.pyplot as plt

parser = argparse.ArgumentParser()
parser.add_argument('--csv', '-f', help="input CSV file", required=True)
args = parser.parse_args()

# read raw data from CSV into a dataframe
df = pd.read_csv(args.csv)
print(df)

ax = df.plot.bar(legend=False, title='Percent Utilization', y='percent_used', x='blocks_allocated', xlabel='Block size', rot=0, ylim=[0,100])

for p in ax.patches:
	ax.text(p.get_x(), p.get_y() + p.get_height(), str(p.get_height())+ '%')

# save the output file
outname = "utilization-cmp.png";
plt.savefig(outname)

# pyplot likes to cache things it probably shouldn't
plt.close('all')
