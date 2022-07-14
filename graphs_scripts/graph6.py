#!/usr/bin/python
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
from cycler import cycler
import sys

fig, ax = plt.subplots(1,1)
bar_cycle = cycler(color=list('bgrymkwc')) + cycler(hatch=['--', '++', 'xX', '\\\\', '**', '..', 'OO', '--'])
styles = bar_cycle()
ax.set_ylabel('Throughput [MPPS]', fontsize=20)
ax.yaxis.grid(True)

hash = "xxh"
st = ""
df_c = pd.read_csv('../results/hh_caida.'+hash+'.raw_res', skipinitialspace=True)

df_c18 = pd.read_csv('../results/hh_caida18.'+hash+'.raw_res', skipinitialspace=True)

df_univ = pd.read_csv('../results/hh_univ1.'+hash+'.raw_res', skipinitialspace=True)

df_org = pd.concat([df_c, df_univ, df_c18])
df_org = df_org.replace(np.nan, 0)
df_org['dataset'] = df_org.apply(lambda row: 'caida16' if row['dataset'] == 'caida' else row['dataset'], axis=1)

if st=='qurey':
	ops=df_org['size']
else:
	ops=df_org['insertions']

df_org['throughput'] = ops / 1000000 / df_org['duration']
del df_org['insertions']
del df_org['duration']

sizes=map(lambda s : 10**int(s), sys.argv[1:])
for s in sizes:
        df_org = df_org[df_org['size'] == s]
	del df_org['size']
	
	#print(df_org['type'] )

	df_org['type'] = df_org.apply(
	    lambda row: r'$q$-MAX,$\gamma=$' + str(row['gamma']) if row['type'] == 'AmortizedQMax'+st else row['type'], axis=1)
    
    	df_org['type'] = df_org.apply(
	    lambda row: r'SQUID,$\gamma=$' + str(row['gamma']) if row['type'] == 'LVAmortizedQMax'+st else row['type'], axis=1)
    
	#print(df_org['type'] )
    
    
	df_org = df_org[df_org['type'] != '']

	j = df_org.groupby(['type', 'dataset'])['throughput'].mean().unstack(0)
	jstd = df_org.groupby(['type', 'dataset'])['throughput'].sem().unstack(0)
	traces = ["univ1", "caida16", "caida18"]

	w=-0.6
	i=0
	for c in [ "Heap" , "SkipList" , r'$q$-MAX,$\gamma=$0.05' , r'SQUID,$\gamma=$0.05' ,  r'$q$-MAX,$\gamma=$0.1' , r'SQUID,$\gamma=$0.1', r'$q$-MAX,$\gamma=$0.25'  , r'SQUID,$\gamma=$0.25']:
	    y = []
	    yerr = []
	    for t in traces:
	        y.append((j[c][t])
	        yerr.append(jstd[c][t])
        

	    ax.bar([x+w for x in [1,4.5,8]],[r-1 for r in y], error_kw=dict(ecolor='r', lw=3, capsize=7, capthick=1), yerr=yerr, width=0.2, alpha=1, edgecolor='k', linewidth=1, label=c, **next(styles))
	    w+=0.2
	    i+=1
	    print(i)
	    if(i%2 ==0):
	        w+=0.15
        if(i%5 ==0):
	        w+=0.4

	ax.set_xticks([1.5,5,8.5])
	ax.set_xticklabels([x.upper() for x in traces], fontsize=16)
	plt.tick_params(axis='y', which='major', labelsize=16)
#	ax.legend(prop={'size': 16}, loc='lower center', framealpha=1.0, bbox_to_anchor=(0.5,1), ncol=2)
	fig.tight_layout()
#	plt.show()
	plt.ylim(ymin=0, ymax=30)
	plt.xlim(xmin=0, xmax=10)
        plt.savefig('../figures/graph6-q='+str(sizes)+'.pdf', bbox_inches='tight')

